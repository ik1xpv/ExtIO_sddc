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
#include "ht257_0_0.h"
#include "ht257_0_7M.h"
#include "ht257_1_7M.h"
#include "ht257_3_6M.h"
#include "ht257_7_5M.h"
#include "ht257_15_4M.h"

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
		pfilterht = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*halfFft);     // 1024
		filterHw = (fftwf_complex**)fftwf_malloc(sizeof(fftwf_complex*)*NDECIDX);
		for (int d = 0; d < NDECIDX; d++)
		{
			filterHw[d] = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*halfFft);     // 1024
		}

		for (int t = 0; t < halfFft; t++)
		{
			pfilterht[t][0] = 0.0f;
			pfilterht[t][1] = 0.0f;
		}

		filterplan_t2f_c2c = fftwf_plan_dft_1d(halfFft, pfilterht, filterHw[0], FFTW_FORWARD, FFTW_MEASURE);
		for (int d = 0; d < NDECIDX; d++)
		{
			float* pht;
			switch (d)
			{
			case 4:
				pht = FIR4;
				break;
			case 3:
				pht = FIR3;
				break;
			case 2:
				pht = FIR2;
				break;
			case 1:
				pht = FIR1;
				break;
			case 0:
			default:
				pht = FIR0;
				break;
			}
			for (int t = 0; t < 257; t++)
			{
				// pfilterht[t][0] = pht[t]/sqrt(2.0);
				pfilterht[t][0] = pfilterht[t][1] = pht[t] / sqrtf(2.0f);
			}

			fftwf_execute_dft(filterplan_t2f_c2c, pfilterht, filterHw[d]);
		}
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

		float iscale = this->GainScale;
		float qscale;
		int _mtunebin = this->mtunebin;  // Update LO tune is possible during run

		// TODO: Change this to sideband check
		if (lsb) {
			// Low sideband
			qscale = -iscale;
		}
		else
		{
			// Upper sideband
			// upconverter or Direct converter
			qscale = iscale;
		}

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
		for (int k = 0; k < fftPerBuf; k++)
		{
			// FFT first stage time to frequency, real to complex
			fftwf_execute_dft_r2c(th->plan_t2f_r2c, th->ADCinTime + (3 * halfFft / 2) * k, th->ADCinFreq);

			for (int m = 0; m < mfft / 2; m++) // circular shift tune fs/2 half array
			{
				int mm = _mtunebin + m;
				if (mm > halfFft - 1)
				{
					th->inFreqTmp[m][0] = th->inFreqTmp[m][1] = 0.0f;
				}
				else
				{
					th->inFreqTmp[m][0] = (th->ADCinFreq[mm][0] * filter[m][0] +
										   th->ADCinFreq[mm][1] * filter[m][1]);

					th->inFreqTmp[m][1] = (th->ADCinFreq[mm][1] * filter[m][0] -
										   th->ADCinFreq[mm][0] * filter[m][1]);
				}
			}

			for (int m = 0; m < mfft / 2; m++) // circular shift tune fs/2 half array
			{
				int fm = halfFft - mfft / 2 + m;
				int mm = _mtunebin - mfft / 2 + m;
				if (mm < 0)
				{
					th->inFreqTmp[mfft / 2 + m][0] = th->inFreqTmp[mfft / 2 + m][1] = 0.0f;
				}
				else
				{
					th->inFreqTmp[mfft / 2 + m][0] = (th->ADCinFreq[mm][0] * filter[fm][0] +
													  th->ADCinFreq[mm][1] * filter[fm][1]);

					th->inFreqTmp[mfft / 2 + m][1] = (th->ADCinFreq[mm][1] * filter[fm][0] -
													  th->ADCinFreq[mm][0] * filter[fm][1]);
				}
			}

			fftwf_execute(th->plan_f2t_c2c);     //  c2c decimation

			if (k == 0)
			{
				auto pTimeTmp = th->outTimeTmp[mfft / 4];
				for (int i = 0; i < mfft / 2; i++)
				{
					*pout++ = iscale * (*pTimeTmp++);
					*pout++ = qscale * (*pTimeTmp++);
				}
			}
			else
			{
				auto pTimeTmp = th->outTimeTmp[0];
				for (int i = 0; i < 3 * mfft / 4; i++)
				{
					*pout++ = iscale * (*pTimeTmp++);
					*pout++ = qscale * (*pTimeTmp++);
				}
			}
		}
	} // while(run)
//    DbgPrintf((char *) "r2iqThreadf idx %d pthread_exit %u\n",(int)th->t, pthread_self());
	return 0;
}
