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

#include <string.h>
#include <algorithm>
#include <assert.h>

#include "mipp.h"

static const int halfFft = FFTN_R_ADC / 2;    // half the size of the first fft at ADC 64Msps real rate (2048)
static const int fftPerBuf = transferSize / sizeof(short) / (3 * halfFft / 2) + 1; // number of ffts per buffer with 256|768 overlap

struct r2iqThreadArg {

	fftwf_plan plan_t2f_r2c;          // fftw plan buffers Freq to Time complex to complex per decimation ratio
	fftwf_plan plan_f2t_c2c;          // fftw plan buffers Time to Freq real to complex per buffer
	fftwf_plan plans_f2t_c2c[NDECIDX];
	float *ADCinTime;                // point to each threads input buffers [nftt][n]
	fftwf_complex *ADCinFreq;         // buffers in frequency
	fftwf_complex *inFreqTmp;         // tmp decimation output buffers (after tune shift)
	fftwf_complex *outTimeTmp;        // tmp decimation output buffers baseband time cplx
};

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

	// load RAND table for ADC
	for (unsigned k = 0; k < 65536; k++)
	{
		if (k & 1)
		{
			RandTable[k] = (k ^ 0xFFFE);
		}
		else {
			RandTable[k] = k;
		}
	}

#ifndef NDEBUG
	for (unsigned k = 0; k < 65536; k++)
		assert(k == (uint16_t)RandTable[(uint16_t)RandTable[k]]);
#endif
}

fft_mt_r2iq::fft_mt_r2iq() :
	r2iqControlClass()
{
	mtunebin = halfFft / 4;
	mfftdim[0] = halfFft;
	for (int i = 1; i < NDECIDX; i++)
	{
		mfftdim[i] = mfftdim[i - 1] / 2;
	}
	GainScale = BBRF103_GAINFACTOR;

}

fft_mt_r2iq::~fft_mt_r2iq()
{
	fftwf_export_wisdom_to_filename("wisdom");

	for (int d = 0; d < NDECIDX; d++)
	{
		fftwf_free(filterHw[d]);     // 4096
	}
	fftwf_free(filterHw);

	for (unsigned t = 0; t < processor_count; t++) {
		auto th = threadArgs[t];
		fftwf_free(th->ADCinTime);
		fftwf_free(th->ADCinFreq);
		fftwf_free(th->inFreqTmp);
		fftwf_free(th->outTimeTmp);
		fftwf_destroy_plan(th->plan_t2f_r2c);
		for (int d = 0; d < NDECIDX; d++)
		{
			fftwf_destroy_plan(th->plans_f2t_c2c[d]);
		}

		delete threadArgs[t];
	}
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

void fft_mt_r2iq::TurnOn() {
	this->r2iqOn = true;
	this->cntr = 0;
	this->bufIdx = 0;

	for (unsigned t = 0; t < processor_count; t++) {
		r2iq_thread[t] = new std::thread(
			[this] (void* arg)
				{ return this->r2iqThreadf((r2iqThreadArg*)arg); }, (void*)threadArgs[t]);
	}
}

void fft_mt_r2iq::TurnOff(void) {
	this->r2iqOn = false;
	this->cntr = 100;
	cvADCbufferAvailable.notify_all();
	for (unsigned t = 0; t < processor_count; t++) {
		r2iq_thread[t]->join();
	}
}

bool fft_mt_r2iq::IsOn(void) { return(this->r2iqOn); }

void fft_mt_r2iq::DataReady()
{ // signals new sample buffer arrived
	if (!this->r2iqOn)
		return;

	auto pending = std::atomic_fetch_add(&this->cntr, 1);

	if (pending == 0)
		cvADCbufferAvailable.notify_one(); // signal data available
	else
		cvADCbufferAvailable.notify_all(); // signal data available
}

void fft_mt_r2iq::Init(float gain, int16_t **buffers, float** obuffers)
{
	this->buffers = buffers;    // set to the global exported by main_loop
	this->obuffers = obuffers;  // set to the global exported by main_loop

	this->GainScale = gain;

	fftwf_import_wisdom_from_filename("wisdom");

	// Get the processor count
	processor_count = std::thread::hardware_concurrency() - 1;
	if (processor_count == 0)
		processor_count = 1;
	if (processor_count > N_MAX_R2IQ_THREADS)
		processor_count = N_MAX_R2IQ_THREADS;

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

		for (int t = 0; t < halfFft; t++)
		{
			pfilterht[t][0] = 0.0f;
			pfilterht[t][1] = 0.0f;
		}

		filterplan_t2f_c2c = fftwf_plan_dft_1d(halfFft, pfilterht, filterHw[0], FFTW_FORWARD, FFTW_MEASURE);
		float *pht = new float[halfFft / 4 + 1];
		for (int d = 0; d < NDECIDX; d++)	// @todo when increasing NDECIDX
		{
			// @todo: have dynamic bandpass filter size - depending on decimation
			//   to allow same stopband-attenuation for all decimations
#if 0
			// switch below does same as this
			float Astop = std::min(120.0f, 130.0f - d * 10.0f);
			float normFpass = ???;
			float normFstop = (32.0f / (1 << d)) / 64.0f;
			KaiserWindow(halfFft / 4 + 1, Astop, 0.7f / 64.0f, normFstop, pht);
#else
			switch (d)
			{
			// @todo: case 6 == NDECIDX -1 is missing!?!
			case 5:
				KaiserWindow(halfFft / 4 + 1, 80.0f, 0.7f/64.0f, 1.0f/64.0f, pht);
				break;
			case 4:
				KaiserWindow(halfFft / 4 + 1, 90.0f, 1.6f/64.0f, 2.0f/64.0f, pht);
				break;
			case 3:
				KaiserWindow(halfFft / 4 + 1, 100.0f, 3.6f/64.0f, 4.0f/64.0f, pht);
				break;
			case 2:
				KaiserWindow(halfFft / 4 + 1, 110.0f, 7.6f/64.0f, 8.0f/64.0f, pht);
				break;
			case 1:
				KaiserWindow(halfFft / 4 + 1, 120.0f, 15.7f/64.0f, 16.0f/64.0f, pht);
				break;
			default:
				//assert(0);  --> case 6 == NDECIDX -1
				// no-break
			case 0:
				KaiserWindow(halfFft / 4 + 1, 120.0f, 30.5f/64.0f, 32.0f/64.0f, pht);
				break;
			}
#endif

			float gainadj = gain  / sqrtf(2.0f) * 2048.0f / (float)FFTN_R_ADC; // reference is FFTN_R_ADC == 2048

			for (int t = 0; t < (halfFft/4+1); t++)
			{
				pfilterht[t][0] = pfilterht[t][1] = gainadj * pht[t];
			}

			fftwf_execute_dft(filterplan_t2f_c2c, pfilterht, filterHw[d]);
		}
		delete[] pht;
		fftwf_destroy_plan(filterplan_t2f_c2c);
		fftwf_free(pfilterht);

		for (unsigned t = 0; t < processor_count; t++) {
			r2iqThreadArg *th = new r2iqThreadArg();
			threadArgs[t] = th;

			th->ADCinTime = (float*)fftwf_malloc(sizeof(float) * (halfFft + transferSize / 2));                 // 2048

			th->ADCinFreq = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*(halfFft + 1)); // 1024+1
			th->inFreqTmp = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*(halfFft));    // 1024
			th->outTimeTmp = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*(halfFft));
			th->plan_t2f_r2c = fftwf_plan_dft_r2c_1d(2 * halfFft, th->ADCinTime, th->ADCinFreq, FFTW_MEASURE);
			for (int d = 0; d < NDECIDX; d++)
			{
				th->plans_f2t_c2c[d] = fftwf_plan_dft_1d(mfftdim[d], th->inFreqTmp, th->outTimeTmp, FFTW_BACKWARD, FFTW_MEASURE);
			}

		}
	}

void * fft_mt_r2iq::r2iqThreadf(r2iqThreadArg *th) {

	const int decimate = this->mdecimation;
	const int mfft = this->mfftdim[decimate];	// = halfFft / 2^mdecimation
	const int mratio = this->getRatio();
	const fftwf_complex* filter = filterHw[decimate];
	const bool lsb = this->getSideband();
	th->plan_f2t_c2c = th->plans_f2t_c2c[decimate];

	mipp::Reg<float> rA;
	mipp::Reg<int16_t> rADC;
	mipp::Reg<int32_t> rExt;

	while (r2iqOn) {
		const int16_t *dataADC;  // pointer to input data
		const int16_t *lastDataADC;
		float * pout;

		const int _mtunebin = this->mtunebin;  // Update LO tune is possible during run

		{
			int wakecnt = 0;
			std::unique_lock<std::mutex> lk(mutexR2iqControl);
			while (this->cntr <= 0)
			{
				cvADCbufferAvailable.wait(lk, [&wakecnt, this] {
					wakecnt++;
					return this->cntr > 0;
				});
			}

			if (!r2iqOn)
				return 0;

			dataADC = this->buffers[this->bufIdx];
			std::atomic_fetch_sub(&this->cntr, 1);

			int modx = this->bufIdx / mratio;
			int moff = this->bufIdx - modx * mratio;
			int offset = ((transferSize / sizeof(int16_t)) / mratio) * moff;
			pout = this->obuffers[modx] + offset;

			lastDataADC = &this->buffers[(this->bufIdx + QUEUE_SIZE - 1) % QUEUE_SIZE][halfFft];
			this->bufIdx = ((this->bufIdx + 1) % QUEUE_SIZE);
		}

		// first frame
		auto inloop = th->ADCinTime;

		// duplicate  halfFft samples from the last frame
		static_assert(halfFft % mipp::N<int16_t>() == 0);
		for (int m = 0; m < halfFft; m+=mipp::N<int16_t>()) {
				rADC.loadu(&lastDataADC[m]);

				auto r_adc_low = rADC.low();
				rExt = r_adc_low.template cvt<int32_t>();
				// parse low part
				rA = rExt.cvt<float>();
				rA.store(&inloop[m]);

				// parse high part
				auto r_adc_high = rADC.high();
				rExt = r_adc_high.template cvt<int32_t>();
				// parse low part
				rA = rExt.cvt<float>();
				rA.store(&inloop[m + mipp::N<float>()]);
		}
		inloop += halfFft;

		// @todo: move the following int16_t conversion to (32-bit) float
		// directly inside the following loop (for "k < fftPerBuf")
		//   just before the forward fft "fftwf_execute_dft_r2c" is called
		// idea: this should improve cache/memory locality
		if (!this->getRand())        // plain samples no ADC rand set
		{
			static_assert((transferSize /sizeof(int16_t)) % (mipp::N<float>() * 2) == 0);
			for (int m = 0; m < transferSize / sizeof(int16_t); m+=mipp::N<int16_t>()) {
				rADC.load(&dataADC[m]);

				auto r_adc_low = rADC.low();
				rExt = r_adc_low.template cvt<int32_t>();
				// parse low part
				rA = rExt.cvt<float>();
				rA.store(&inloop[m]);

				// parse high part
				auto r_adc_high = rADC.high();
				rExt = r_adc_high.template cvt<int32_t>();
				// parse low part
				rA = rExt.cvt<float>();
				rA.store(&inloop[m + mipp::N<float>()]);
			}
		}
		else
		{
			// @todo: can this get implemented without the RandTable[]?
			//   for less cache trashing?
			//   ideally with some simd commands ..
			for (int m = 0; m < transferSize / sizeof(int16_t); m++) {
				*inloop++ = (RandTable[(uint16_t)*dataADC++]);
			}
		}

		// decimate in frequency plus tuning

		// Caculate the parameters for the first half
		const auto count = std::min(mfft/2, halfFft - _mtunebin);
		const auto source = &th->ADCinFreq[_mtunebin];

		// Caculate the parameters for the second half
		const auto start = std::max(0, mfft / 2 - _mtunebin);
		const auto source2 = &th->ADCinFreq[_mtunebin - mfft / 2];
		const auto filter2 = &filter[halfFft - mfft / 2];
		const auto dest = &th->inFreqTmp[mfft / 2];
		for (int k = 0; k < fftPerBuf; k++)
		{
			// core of fast convolution including filter and decimation
			//   main part is 'overlap-scrap' (IMHO better name for 'overlap-save'), see
			//   https://en.wikipedia.org/wiki/Overlap%E2%80%93save_method
			{
				// FFT first stage: time to frequency, real to complex
				// 'full' transformation size: 2 * halfFft
				fftwf_execute_dft_r2c(th->plan_t2f_r2c, th->ADCinTime + (3 * halfFft / 2) * k, th->ADCinFreq);
				// result now in th->ADCinFreq[]

				// circular shift (mixing in full bins) and low/bandpass filtering (complex multiplication)
				{
					// circular shift tune fs/2 first half array into th->inFreqTmp[]
					for (int m = 0; m < count; m++)
					{
						// besides circular shift, do complex multiplication with the lowpass filter's spectrum
						th->inFreqTmp[m][0] = (source[m][0] * filter[m][0] +
							source[m][1] * filter[m][1]);
						th->inFreqTmp[m][1] = (source[m][1] * filter[m][0] -
							source[m][0] * filter[m][1]);
					}
					if (mfft / 2 != count)
						memset(th->inFreqTmp[count], 0, sizeof(float) * 2 * (mfft / 2 - count));

					// circular shift tune fs/2 second half array
					for (int m = start; m < mfft / 2; m++)
					{
						// also do complex multiplication with the lowpass filter's spectrum
						dest[m][0] = (source2[m][0] * filter2[m][0] +
							source2[m][1] * filter2[m][1]);
						dest[m][1] = (source2[m][1] * filter2[m][0] -
							source2[m][0] * filter2[m][1]);
					}
					if (start != 0)
						memset(th->inFreqTmp[mfft / 2], 0, sizeof(float) * 2 * start);
				}
				// result now in th->inFreqTmp[]

				// 'shorter' inverse FFT transform (decimation); frequency (back) to COMPLEX time domain
				// transform size: mfft = mfftdim[k] = halfFft / 2^k with k = mdecimation
				fftwf_execute(th->plan_f2t_c2c);     //  c2c decimation
				// result now in th->outTimeTmp[]
			}

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
					auto pTimeTmp = th->outTimeTmp[mfft / 4];
					for (int i = 0; i < mfft / 2; i++)
					{
						*pout++ = *pTimeTmp++;
						*pout++ = -*pTimeTmp++;
					}
				}
				else
				{
					auto pTimeTmp = th->outTimeTmp[0];
					for (int i = 0; i < 3 * mfft / 4; i++)
					{
						*pout++ = *pTimeTmp++;
						*pout++ = -*pTimeTmp++;
					}
				}
			}
			else // upper sideband
			{
				// simple memcpy() calls are sufficient (possibly faster?)
				if (k == 0)
				{
					auto pTimeTmp = th->outTimeTmp[mfft / 4];
					for (int i = 0; i < mfft / 2; i++)
					{
						*pout++ = *pTimeTmp++;
						*pout++ = *pTimeTmp++;
					}
				}
				else
				{
					auto pTimeTmp = th->outTimeTmp[0];
					for (int i = 0; i < 3 * mfft / 4; i++)
					{
						*pout++ = *pTimeTmp++;
						*pout++ = *pTimeTmp++;
					}
				}
			}
			// result now in this->obuffers[]
		}
	} // while(run)
//    DbgPrintf((char *) "r2iqThreadf idx %d pthread_exit %u\n",(int)th->t, pthread_self());
	return 0;
}
