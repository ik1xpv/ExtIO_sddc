#include "fft_mt_r2iq.h"
#include "config.h"

#include "mipp.h"
#include <assert.h>

static_assert(mipp::N<int16_t>() == mipp::N<float>() * 2);

template<bool rand, bool aligned> void fft_mt_r2iq::simd_convert_float(const int16_t *input, float* output, int size)
{
	mipp::Reg<int16_t> rADC;
	mipp::Reg<int16_t> rOne = 1;
	mipp::Reg<int16_t> rNegativeTwo = -2;

	int m = 0;

	auto vecLoopSize1 = (size / mipp::N<float>()) * mipp::N<float>();
	auto vecLoopSize2 = (size / mipp::N<int16_t>()) * mipp::N<int16_t>();

	auto vecLoopSize = std::min(vecLoopSize1, vecLoopSize2);

	for (; m < vecLoopSize; m+=mipp::N<int16_t>()) {
			if (aligned)
				rADC.load(&input[m]);
			else
				rADC.loadu(&input[m]);

			if (rand)
			{
				// c = adc & 1
				auto rC = mipp::andb(rADC, rOne);

				// mask = (c == 1)?
				auto mask = mipp::cmpeq(rC, rOne);

				//d = adc ^ (0xfffe)
				auto rD = mipp::xorb(rADC, rNegativeTwo);

				// adc = mask? d : adc;
				rADC = mipp::blend(rD, rADC, mask);
			}

			{
				auto rExt = rADC.low().template cvt<int32_t>();
				auto rExtHihg = rADC.high().template cvt<int32_t>();

				auto rLow = rExt.cvt<float>();
				auto rHigh = rExtHihg.cvt<float>();

				if (aligned) {
					rLow.store(&output[m]);
					rHigh.store(&output[m + mipp::N<float>()]);
				}
				else {
					rLow.storeu(&output[m]);
					rHigh.storeu(&output[m + mipp::N<float>()]);
				}
			}
	}

	assert(m == vecLoopSize);

	if (size - vecLoopSize > 0)
	{
		// some left over, use standard version of converting
		norm_convert_float<rand, aligned>(input + m, output + m, size - vecLoopSize);
	}
}

template<bool rand, bool aligned> void fft_mt_r2iq::norm_convert_float(const int16_t *input, float* output, int size)
{
	for(int m = 0; m < size; m++)
	{
		int16_t val;
		if (rand && (input[m] & 1))
		{
			val = input[m] ^ (-2);
		}
		else
		{
			val = input[m];
		}
		output[m] = float(val);
	}
}

template<bool aligned> void fft_mt_r2iq::simd_shift_freq(fftwf_complex* dest, const fftwf_complex* source1, const fftwf_complex* source2, int start, int end)
{
	int m;
	auto size = end - start;
	auto vecLoopSize = (size / mipp::N<float>()) * mipp::N<float>();
	dest += start;
	source1 += start;
	source2 += start;
	for (m = 0; m < vecLoopSize; m += mipp::N<float>())
	{
		mipp::Regx2<float> rSrc1;
		mipp::Regx2<float> rSrc2;

		if (aligned)
		{
			rSrc1.load(&source1[m][0]);
			rSrc2.load(&source2[m][0]);
		}
		else
		{
			rSrc1.loadu(&source1[m][0]);
			rSrc2.loadu(&source2[m][0]);
		}

		auto s1 = mipp::deinterleave(rSrc1[0], rSrc1[1]);
		auto s2 = mipp::deinterleave(rSrc2[0], rSrc2[1]);

		auto result = mipp::cmul(s1, s2);

		auto r = mipp::interleave(result[0], result[1]);

		if (aligned)
			r.store((float*)&dest[m][0]);
		else
			r.storeu((float*)&dest[m][0]);
	}

	assert(m == vecLoopSize);

	if (size - vecLoopSize > 0)
	{
		norm_shift_freq<aligned>(&dest[m], &source1[m], &source2[m], 0, size - vecLoopSize);
	}
}

template<bool aligned> void fft_mt_r2iq::norm_shift_freq(fftwf_complex* dest, const fftwf_complex* source1, const fftwf_complex* source2, int start, int end)
{
	for (int m = start; m < end; m++)
	{
		// besides circular shift, do complex multiplication with the lowpass filter's spectrum
		dest[m][0] = source1[m][0] * source2[m][0] - source1[m][1] * source2[m][1];
		dest[m][1] = source1[m][1] * source2[m][0] + source1[m][0] * source2[m][1];
	}
}