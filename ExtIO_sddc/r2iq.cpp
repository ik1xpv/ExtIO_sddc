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

#include "r2iq.h"
#include "config.h"
#include "fftw3.h"
#include "RadioHandler.h"
#include "ht257_0_0.h"
#include "ht257_0_7M.h"
#include "ht257_1_7M.h"
#include "ht257_3_6M.h"
#include "ht257_7_5M.h"
#include "ht257_15_4M.h"

class r2iqControlClass r2iqCntrl;

struct r2iqThreadArg {

	fftwf_plan plan_t2f_r2c;          // fftw plan buffers Freq to Time complex to complex per decimation ratio
	fftwf_plan plan_f2t_c2c;          // fftw plan buffers Time to Freq real to complex per buffer
	fftwf_plan plans_f2t_c2c[NDECIDX];
	float **ADCinTime;                // point to each threads input buffers [nftt][n]
	fftwf_complex *ADCinFreq;         // buffers in frequency
	fftwf_complex *inFreqTmp;         // tmp decimation output buffers (after tune shift)
	fftwf_complex *outTimeTmp;        // tmp decimation output buffers baseband time cplx
};

r2iqControlClass::r2iqControlClass()
{
	r2iqOn = false;
	randADC = false;
	halfFft = FFTN_R_ADC / 2;    // half the size of the first fft at ADC 64Msps real rate (2048)
    fftPerBuf = transferSize / sizeof(short) / (3 * halfFft / 2) + 1; // number of ffts per buffer with 256|768 overlap

	mtunebin = halfFft / 4;
	mfftdim[0] = halfFft; 
	mratio[0] = 1;  // 1,2,4,8,16
	for (int i = 1; i < NDECIDX; i++)
	{
		mfftdim[i] = mfftdim[i - 1] / 2;
		mratio[i] = mratio[i - 1] * 2;
	}
	GainScale = BBRF103_GAINFACTOR;

}

r2iqControlClass::~r2iqControlClass()
{
}


float r2iqControlClass::setFreqOffset(float offset)
{
	// align to 1/4 of halfft
	this->mtunebin = int(offset * halfFft / 4) * 4;  // mtunebin step 4 bin  ?
	float delta = ((float)this->mtunebin  / halfFft) - offset;
	float ret = delta * mratio[mdecimation]; // ret increases with higher decimation
	DbgPrintf("offset %f mtunebin %d delta %f (%f)\n", offset, this->mtunebin, delta, ret);
	return ret;
}

void r2iqControlClass::TurnOn() {
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

void r2iqControlClass::TurnOff(void) {
	this->r2iqOn = false;
	this->cntr = 100;
	cvADCbufferAvailable.notify_all();
	for (unsigned t = 0; t < processor_count; t++) {
		r2iq_thread[t]->join();
	}
}

bool r2iqControlClass::IsOn(void) { return(this->r2iqOn); }

void r2iqControlClass::DataReady()
{ // signals new sample buffer arrived
	int pending;
	if (!this->r2iqOn)
		return;
	mutexR2iqControl.lock();
	pending = this->cntr;
	this->cntr++;
	mutexR2iqControl.unlock();
	if (pending == 0)
		cvADCbufferAvailable.notify_one(); // signal data available
	else
		cvADCbufferAvailable.notify_all(); // signal data available
}

void r2iqControlClass::Init(float gain, uint8_t **buffers, float** obuffers)
{
	this->buffers = buffers;    // set to the global exported by main_loop
	this->obuffers = obuffers;  // set to the global exported by main_loop

	this->GainScale = gain;

	// Get the processor count
	processor_count = std::thread::hardware_concurrency() - 1;
	if (processor_count == 0)
		processor_count = 1;
	if (processor_count > N_R2IQ_THREAD)
		processor_count = N_R2IQ_THREAD;

		fftwf_plan filterplan_t2f_c2c; // time to frequency fft

		DbgPrintf((char *) "r2iqCntrl initialization\n");

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
		//        DbgPrintf((char *) "RandTable generated\n");

		   // filters
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

		for (unsigned t = 0; t < processor_count; t++) {
			r2iqThreadArg *th = new r2iqThreadArg();
			threadArgs[t] = th;

			th->ADCinTime = (float**)malloc(sizeof(float*) * fftPerBuf + 1);
			for (int n = 0; n < fftPerBuf; n++) {
				th->ADCinTime[n] = (float*)fftwf_malloc(sizeof(float) * 2 * halfFft);                 // 2048
			}

			th->ADCinFreq = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*(halfFft + 1)); // 1024+1
			th->inFreqTmp = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*(halfFft));    // 1024
			th->outTimeTmp = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*(halfFft));
			th->plan_t2f_r2c = fftwf_plan_dft_r2c_1d(2 * halfFft, th->ADCinTime[0], th->ADCinFreq, FFTW_MEASURE);
			for (int d = 0; d < NDECIDX; d++)
			{
				th->plans_f2t_c2c[d] = fftwf_plan_dft_1d(mfftdim[d], th->inFreqTmp, th->outTimeTmp, FFTW_BACKWARD, FFTW_MEASURE);
			}

		}
	}

void * r2iqControlClass::r2iqThreadf(r2iqThreadArg *th) {

	char *buffer;
	int lastdecimate = -1;

	float * pout;
	int decimate = this->getDecimate();
	fftwf_complex* filter = filterHw[decimate];
	th->plan_f2t_c2c = th->plans_f2t_c2c[decimate];

	while (r2iqOn) {
		int mfft = this->getFftN();
		int mratio = this->getRatio();
		int idx;
		float iscale = this->GainScale * (float)powf(10.0f, gdGainCorr_dB / 20.0f);
		float qscale;
		int _mtunebin = this->mtunebin;  // Update LO tune is possible during run
		rf_mode  moderf = RadioHandler.GetmodeRF();
		float *endloop;           // pointer to end data to be copied to beginning

		// TODO: Change this to sideband check
		if (moderf == VHFMODE) {
			// Down converter
			qscale = -iscale;
		}
		else
		{
			// upconverter or Direct converter
			qscale = iscale;
		}

		{
			int wakecnt = 0;
			std::unique_lock<std::mutex> lk(mutexR2iqControl);
			while (this->cntr <= 0)
			{
				cvADCbufferAvailable.wait(lk, [&wakecnt, this]
				{
					wakecnt++;
					return this->cntr > 0;
				});
			}

			if (!r2iqOn)
				return 0;

			buffer = (char *)this->buffers[this->bufIdx];
			idx = this->bufIdx;
			this->cntr--;

			int modx = this->bufIdx / mratio;
			int moff = this->bufIdx - modx * mratio;
			int offset = ((transferSize / 2) / mratio) *moff;
			pout = (float *)(this->obuffers[modx] + offset);

			this->bufIdx = ((this->bufIdx + 1) % QUEUE_SIZE);
	
			endloop = lastThread->ADCinTime[fftPerBuf - 1] + halfFft;
			lastThread = th;
		}

		ADCSAMPLE *dataADC; // pointer to input data
		float *inloop;            // pointer to first fft input buffer
		dataADC = (ADCSAMPLE *)buffer;
		int blocks = fftPerBuf;

		// first frame
		inloop = th->ADCinTime[0];
		for (int m = 0; m < halfFft; m++) {
			*inloop++ = *endloop++;   // duplicate form last frame halfFft samples
		}

		if (!this->randADC)        // plain samples no ADC rand set
		{
			// completes first frame
			for (int m = 0; m < halfFft; m++) {
				*inloop++ = *dataADC++;
			}
 			// all other frames
			dataADC = dataADC - halfFft / 2;    // halfFft/2 overlap
			for (int k = 1; k < fftPerBuf; k++) {
				inloop = th->ADCinTime[k];
				for (int m = 0; m < 2 * halfFft; m++) {
					*inloop++ = *dataADC++;
				}
				dataADC = dataADC - halfFft / 2;
			}
		}
		else
		{
			// completes first frame
			for (int m = 0; m < halfFft; m++) {
				*inloop++ = (RandTable[(UINT16)*dataADC++]);
			}

 			// all other frames
			dataADC = dataADC - halfFft / 2;    // halfFft/2 overlap
			for (int k = 1; k < fftPerBuf; k++) {
				inloop = th->ADCinTime[k];
				for (int m = 0; m < 2 * halfFft; m++) {
					*inloop++ = (RandTable[(UINT16)*dataADC++]);
				}
				dataADC = dataADC - halfFft / 2;
			}
		}

		// decimate in frequency plus tuning
		for (int k = 0; k < fftPerBuf; k++)
		{
			// FFT first stage time to frequency, real to complex
			fftwf_execute_dft_r2c(th->plan_t2f_r2c, th->ADCinTime[k], th->ADCinFreq);

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
