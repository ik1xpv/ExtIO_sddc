#include "license.txt"  
/*
The ADC input real stream of 16 bit samples (at Fs = 64 Msps in the example) is converted to:
- 32 Msps float Fs/2 complex stream, or
- 16 Msps float Fs/2 complex stream, or
-  8 Msps float Fs/2 complex stream, or
-  4 Msps float Fs/2 complex stream, or
-  2 Msps float Fs/2 complex stream.
The decimation factor is selectable from HDSDR GUI sampling rate selector

The name r2iq as Real 2 I+Q stream

*/

#include "fft_mt_r2iq.h"
#include "config.h"
#include "fftw3.h"
#include "RadioHandler.h"

#include "fir.h"

#include <assert.h>
#include <utility>


r2iqControlClass::r2iqControlClass()
{
	r2iqOn = false;
	randADC = false;
	sideband = false;
	mdecimation = 0;
	mratio[0] = 1;  // 1,2,4,8,16
	for (int i = 1; i < NDECIDX; i++)
	{
		mratio[i] = mratio[i - 1] * 2;
	}
}

fft_mt_r2iq::fft_mt_r2iq() :
	r2iqControlClass(),
	filterHw(nullptr)
{
	mtunebin = halfFft / 4;
	mfftdim[0] = halfFft;
	for (int i = 1; i < NDECIDX; i++)
	{
		mfftdim[i] = mfftdim[i - 1] / 2;
	}
	GainScale = 0.0f;

#if PRINT_INPUT_RANGE
		MinMaxBlockCount = 0;
		MinValue = 0;
		MaxValue = 0;
#endif

#ifndef NDEBUG
	int mratio = 1;  // 1,2,4,8,16,..
	const float Astop = 120.0f;
	const float relPass = 0.85f;  // 85% of Nyquist should be usable
	const float relStop = 1.1f;   // 'some' alias back into transition band is OK
	printf("\n***************************************************************************\n");
	printf("Filter tap estimation, Astop = %.1f dB, relPass = %.2f, relStop = %.2f\n", Astop, relPass, relStop);
	for (int d = 0; d < NDECIDX; d++)
	{
		float Bw = 64.0f / mratio;
		int ntaps = KaiserWindow(0, Astop, relPass * Bw / 128.0f, relStop * Bw / 128.0f, nullptr);
		printf("decimation %2d: KaiserWindow(Astop = %.1f dB, Fpass = %.3f,Fstop = %.3f, Bw %.3f @ %f ) => %d taps\n",
			d, Astop, relPass * Bw, relStop * Bw, Bw, 128.0f, ntaps);
		mratio = mratio * 2;
	}
	printf("***************************************************************************\n");
#endif

}

fft_mt_r2iq::~fft_mt_r2iq()
{
	if (filterHw == nullptr)
		return;

	fftwf_export_wisdom_to_filename("wisdom");

	for (int d = 0; d < NDECIDX; d++)
	{
		fftwf_free(filterHw[d]);     // 4096
	}
	fftwf_free(filterHw);

	fftwf_destroy_plan(plan_t2f_r2c);
}


float fft_mt_r2iq::setFreqOffset(float offset)
{
	// align to 1/4 of halfft
	this->mtunebin = int(offset * halfFft / 4) * 4;  // mtunebin step 4 bin  ?
	float delta = ((float)this->mtunebin  / halfFft) - offset;
	float ret = delta * getRatio(); // ret increases with higher decimation
	DbgPrintf("offset %f mtunebin %d delta %f (%f)\n", offset, this->mtunebin, delta, ret);
	return ret;
}

void fft_mt_r2iq::TurnOn(ringbuffer<int16_t> &input) {
	this->inputbuffer = &input;    // set to the global exported by main_loop
	this->r2iqOn = true;
	this->bufIdx = 0;

	this->outputbuffer->Start();
	this->freqdomain.Start();

	r2iq_thread = std::thread(
		[this]() {
			return this->r2iqThreadf();
		});

	freq2time_thread = std::thread(
		[this]() {
			return this->receiverf();
		}
	);
}

void fft_mt_r2iq::TurnOff(void) {
	this->r2iqOn = false;

	inputbuffer->Stop();
	freqdomain.WriteDone();
	freqdomain.Stop();
	outputbuffer->Stop();
	r2iq_thread.join();
	freq2time_thread.join();
}

bool fft_mt_r2iq::IsOn(void) { return(this->r2iqOn); }

void fft_mt_r2iq::Init(float gain, ringbuffer<float>* obuffers)
{
	this->outputbuffer = obuffers;  // set to the global exported by main_loop

	this->GainScale = gain;

	fftwf_import_wisdom_from_filename("wisdom");

	{
		fftwf_plan filterplan_t2f_c2c; // time to frequency fft

		DbgPrintf((char *) "r2iqCntrl initialization\n");


		//        DbgPrintf((char *) "RandTable generated\n");

		   // filters
		fftwf_complex *pfilterht;       // time filter ht
		pfilterht = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*halfFft);     // halfFft
		filterHw = (fftwf_complex**)fftwf_malloc(sizeof(fftwf_complex*)*NDECIDX);
		for (int d = 0; d < NDECIDX; d++)
		{
			filterHw[d] = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*halfFft);     // halfFft
		}

		filterplan_t2f_c2c = fftwf_plan_dft_1d(halfFft, pfilterht, filterHw[0], FFTW_FORWARD, FFTW_MEASURE);
		float *pht = new float[halfFft / 4 + 1];
		const float Astop = 120.0f;
		const float relPass = 0.85f;  // 85% of Nyquist should be usable
		const float relStop = 1.1f;   // 'some' alias back into transition band is OK

		fftwf_complex *inFreqTmp = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * (halfFft)); // 1024

		for (int d = 0; d < NDECIDX; d++)	// @todo when increasing NDECIDX
		{
			// @todo: have dynamic bandpass filter size - depending on decimation
			//   to allow same stopband-attenuation for all decimations
			float Bw = 64.0f / mratio[d];
			// Bw *= 0.8f;  // easily visualize Kaiser filter's response
			KaiserWindow(halfFft / 4 + 1, Astop, relPass * Bw / 128.0f, relStop * Bw / 128.0f, pht);

			float gainadj = gain * 2048.0f / (float)FFTN_R_ADC; // reference is FFTN_R_ADC == 2048

			for (int t = 0; t < halfFft; t++)
			{
				pfilterht[t][0] = pfilterht[t][1]= 0.0F;
			}

			for (int t = 0; t < (halfFft/4+1); t++)
			{
				pfilterht[halfFft-1-t][0] = gainadj * pht[t];
			}

			fftwf_execute_dft(filterplan_t2f_c2c, pfilterht, filterHw[d]);

			// Create iFFT
			plans_f2t_c2c[d] = fftwf_plan_dft_1d(mfftdim[d], inFreqTmp, inFreqTmp, FFTW_BACKWARD, FFTW_MEASURE);
		}
		delete[] pht;
		fftwf_free(inFreqTmp);
		fftwf_destroy_plan(filterplan_t2f_c2c);
		fftwf_free(pfilterht);

		ADCinTime = (float*)fftwf_malloc(sizeof(float) * (halfFft + transferSize / 2));                 // 2048

		freqdomain.setBlockSize(sizeof(fftwf_complex)*(halfFft + 1));

		plan_t2f_r2c = fftwf_plan_dft_r2c_1d(2 * halfFft, ADCinTime, freqdomain.peekWritePtr(0), FFTW_MEASURE);
	}
}

#ifdef _WIN32
	//  Windows
	#include <intrin.h>
	#define cpuid(info, x)    __cpuidex(info, x, 0)
#else
	//  GCC Intrinsics
	#include <cpuid.h>
	#define cpuid(info, x)  __cpuid_count(x, 0, info[0], info[1], info[2], info[3])
#endif

void * fft_mt_r2iq::r2iqThreadf()
{
#ifdef NO_SIMD_OPTIM
	DbgPrintf("Hardware Capability: all SIMD features (AVX, AVX2, AVX512) deactivated\n");
	return r2iqThreadf_def(th);
#else
	int info[4];
	bool HW_AVX = false;
	bool HW_AVX2 = false;
	bool HW_AVX512F = false;

	cpuid(info, 0);
	int nIds = info[0];

	if (nIds >= 0x00000001){
		cpuid(info,0x00000001);
		HW_AVX    = (info[2] & ((int)1 << 28)) != 0;
	}
	if (nIds >= 0x00000007){
		cpuid(info,0x00000007);
		HW_AVX2   = (info[1] & ((int)1 <<  5)) != 0;

		HW_AVX512F     = (info[1] & ((int)1 << 16)) != 0;
	}

	DbgPrintf("Hardware Capability: AVX:%d AVX2:%d AVX512:%d\n", HW_AVX, HW_AVX2, HW_AVX512F);

	if (HW_AVX512F)
		return r2iqThreadf_avx512();
	else if (HW_AVX2)
		return r2iqThreadf_avx2();
	else if (HW_AVX)
		return r2iqThreadf_avx();
	else
		return r2iqThreadf_def();
#endif
}

void* fft_mt_r2iq::receiverf()
{
	fftwf_complex *inFreqTmp; // tmp decimation output buffers (after tune shift)

	const int decimate = this->mdecimation;
	const int mfft = this->mfftdim[decimate]; // = halfFft / 2^mdecimation
	const fftwf_complex *filter = filterHw[decimate];
	const bool lsb = this->getSideband();
	const auto filter2 = &filter[halfFft - mfft / 2];

	inFreqTmp = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * (halfFft)); // 1024

	int decimate_count = 0;
	auto plan_f2t_c2c = plans_f2t_c2c[decimate];
	fftwf_complex *pout = nullptr;

	while (r2iqOn)
	{
		const int _mtunebin = this->mtunebin; // Update LO tune is possible during run

		if (decimate_count == 0)
			pout = (fftwf_complex *)outputbuffer->getWritePtr();

		decimate_count = (decimate_count + 1) & ((1 << decimate) - 1);
		const auto count = std::min(mfft / 2, halfFft - _mtunebin);
		const auto start = std::max(0, mfft / 2 - _mtunebin);
		const auto dest = &inFreqTmp[mfft / 2];

		for (int k = 0; k < fftPerBuf; k++)
		{
			const fftwf_complex *ADCinFreq;         // buffers in frequency

			ADCinFreq = freqdomain.getReadPtr();

			if (!r2iqOn)
				return 0;

			// Calculate the parameters for the first half
			const auto source = &ADCinFreq[_mtunebin];

			// Calculate the parameters for the second half
			const auto source2 = &ADCinFreq[_mtunebin - mfft / 2];
			// circular shift (mixing in full bins) and low/bandpass filtering (complex multiplication)
			{
				// circular shift tune fs/2 first half array into inFreqTmp[]
				shift_freq(inFreqTmp, source, filter, 0, count);
				if (mfft / 2 != count)
					memset(inFreqTmp[count], 0, sizeof(float) * 2 * (mfft / 2 - count));

				// circular shift tune fs/2 second half array
				shift_freq(dest, source2, filter2, start, mfft / 2);
				if (start != 0)
					memset(inFreqTmp[mfft / 2], 0, sizeof(float) * 2 * start);
			}
			// result now in inFreqTmp[]

			freqdomain.ReadDone();

			// 'shorter' inverse FFT transform (decimation); frequency (back) to COMPLEX time domain
			// transform size: mfft = mfftdim[k] = halfFft / 2^k with k = mdecimation
			fftwf_execute_dft(plan_f2t_c2c, inFreqTmp, inFreqTmp); //  c2c decimation
																	// result now in inFreqTmp[]

			// postprocessing
			// @todo: is it possible to ..
			//  1)
			//    let inverse FFT produce/save it's result directly
			//    in "this->obuffers[modx] + offset" (pout)
			//    ( obuffers[] would need to have additional space ..;
			//      need to move 'scrap' of 'ovelap-scrap'? )
			//    at least FFTW would allow so,
			//      see http://www.fftw.org/fftw3_doc/New_002darray-Execute-Functions.html
			//    attention: multithreading!
			//  2)
			//    could mirroring (lower sideband) get calculated together
			//    with fine mixer - modifying the mixer frequency? (fs - fc)/fs
			//    (this would reduce one memory pass)
			if (lsb) // lower sideband
			{
				// mirror just by negating the imaginary Q of complex I/Q
				if (k == 0)
				{
					copy<true>(pout, &inFreqTmp[mfft / 4], mfft / 2);
				}
				else
				{
					copy<true>(pout + mfft / 2 + (3 * mfft / 4) * (k - 1), &inFreqTmp[0], (3 * mfft / 4));
				}
			}
			else // upper sideband
			{
				if (k == 0)
				{
					copy<false>(pout, &inFreqTmp[mfft / 4], mfft / 2);
				}
				else
				{
					copy<false>(pout + mfft / 2 + (3 * mfft / 4) * (k - 1), &inFreqTmp[0], (3 * mfft / 4));
				}
			}
			// result now in this->obuffers[]
		}

		if (decimate_count == 0)
		{
			outputbuffer->WriteDone();
			pout = nullptr;
		}
		else
		{
			pout += mfft / 2 + (3 * mfft / 4) * (fftPerBuf - 1);
		}
	} // while(run)
	  //    DbgPrintf((char *) "r2iqThreadf idx %d pthread_exit %u\n",(int)this->t, pthread_self());

	fftwf_free(inFreqTmp);

	return 0;
}
