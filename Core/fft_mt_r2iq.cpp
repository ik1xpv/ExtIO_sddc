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

#include <algorithm>

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
	for (int k = 0; k < 65536; k++)
	{
		if ((k & 1) == 1)
		{
			RandTable[k] = (k ^ 0xFFFE);
		}
		else {
			RandTable[k] = k;
		}
	}
}

fft_mt_r2iq::fft_mt_r2iq() :
	r2iqControlClass()
{
	halfFft = FFTN_R_ADC / 2;    // half the size of the first fft at ADC 64Msps real rate (2048)
	fftPerBuf = transferSize / sizeof(short) / (3 * halfFft / 2) + 1; // number of ffts per buffer with 256|768 overlap

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
	this->lastThread = threadArgs[0];

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
	if (processor_count > N_R2IQ_THREAD)
		processor_count = N_R2IQ_THREAD;

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
		for (int d = 0; d < NDECIDX; d++)
		{
			switch (d)
			{
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
			case 0:
			default:
				KaiserWindow(halfFft / 4 + 1, 120.0f, 30.5f/64.0f, 32.0f/64.0f, pht);
				break;
			}

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

	int mfft = this->getFftN();
	int mratio = this->getRatio();
	int decimate = this->getDecimate();
	fftwf_complex* filter = filterHw[decimate];
	bool lsb = this->getSideband();
	th->plan_f2t_c2c = th->plans_f2t_c2c[decimate];

	while (r2iqOn) {
		int16_t *dataADC; // pointer to input data
		float *endloop;           // pointer to end data to be copied to beginning
		float * pout;

		int _mtunebin = this->mtunebin;  // Update LO tune is possible during run

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

			this->bufIdx = ((this->bufIdx + 1) % QUEUE_SIZE);

			endloop = lastThread->ADCinTime + transferSize / sizeof(int16_t);
			lastThread = th;
		}

		// first frame
		auto inloop = th->ADCinTime;
		for (int m = 0; m < halfFft; m++) {
			*inloop++ = *endloop++;   // duplicate form last frame halfFft samples
		}

		if (!this->getRand())        // plain samples no ADC rand set
		{
			for (int m = 0; m < transferSize / sizeof(int16_t); m++) {
				*inloop++ = *dataADC++;
			}
		}
		else
		{
			for (int m = 0; m < transferSize / sizeof(int16_t); m++) {
				*inloop++ = (RandTable[(uint16_t)*dataADC++]);
			}
		}

		// decimate in frequency plus tuning

		// Caculate the parameters for the first half
		auto count = std::min(mfft/2, halfFft - _mtunebin);
		auto source = &th->ADCinFreq[_mtunebin];

		// Caculate the parameters for the second half
		auto start = std::max(0, mfft / 2 - _mtunebin);
		auto source2 = &th->ADCinFreq[_mtunebin - mfft / 2];
		auto filter2 = &filter[halfFft - mfft / 2];
		auto dest = &th->inFreqTmp[mfft / 2];
		for (int k = 0; k < fftPerBuf; k++)
		{
			// FFT first stage time to frequency, real to complex
			fftwf_execute_dft_r2c(th->plan_t2f_r2c, th->ADCinTime + (3 * halfFft / 2) * k, th->ADCinFreq);

			for (int m = 0; m < count; m++) // circular shift tune fs/2 first half array
			{
				th->inFreqTmp[m][0] = (source[m][0] * filter[m][0] +
										source[m][1] * filter[m][1]);

				th->inFreqTmp[m][1] = (source[m][1] * filter[m][0] -
										source[m][0] * filter[m][1]);
				}
			if (mfft/2 != count)
					memset(th->inFreqTmp[count], 0, sizeof(float) * 2 * (mfft/2 - count));

			for (int m = start; m < mfft / 2; m++) // circular shift tune fs/2 second half array
			{
				dest[m][0] = (source2[m][0] * filter2[m][0] +
								source2[m][1] * filter2[m][1]);

				dest[m][1] = (source2[m][1] * filter2[m][0] -
								source2[m][0] * filter2[m][1]);
				}
			if (start != 0)
				memset(th->inFreqTmp[mfft / 2], 0, sizeof(float) * 2 * start);

			fftwf_execute(th->plan_f2t_c2c);     //  c2c decimation

			if (lsb) // lower sideband
			{
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
		}
	} // while(run)
//    DbgPrintf((char *) "r2iqThreadf idx %d pthread_exit %u\n",(int)th->t, pthread_self());
	return 0;
}
