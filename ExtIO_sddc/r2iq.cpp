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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "r2iq.h"
#include "config.h"
#include "fftw3.h"
#include "RadioHandler.h"
#include "resource.h"
#include "ht257_0_0.h"
#include "ht257_0_7M.h"
#include "ht257_1_7M.h"
#include "ht257_3_6M.h"
#include "ht257_7_5M.h"
#include "ht257_15_4M.h"


extern PUCHAR	*buffers;       // ExtIO_sddc defined
extern float**   obuffers;

class r2iqControlClass r2iqCntrl;

struct r2iqThreadArg {
	class r2iqControlClass *r2iqCntrl;

	fftwf_plan plan_t2f_r2c;          // fftw plan buffers Freq to Time complex to complex per decimation ratio
	fftwf_plan plan_f2t_c2c;          // fftw plan buffers Time to Freq real to complex per buffer
	fftwf_plan *plans_f2t_c2c;
	float **ADCinTime;                // point to each threads input buffers [nftt][n]
	fftwf_complex *ADCinFreq;         // buffers in frequency
	fftwf_complex *inFreqTmp;         // tmp decimation output buffers (after tune shift)
	fftwf_complex *outTimeTmp;        // tmp decimation output buffers baseband time cplx
};

static INT16 RandTable[65536];  // ADC RANDomize table used to whitening EMI from ADC data bus.

static void *r2iqThreadf(void *arg);   // thread function
static std::thread* r2iq_thread[N_R2IQ_THREAD]; // thread pointers

const int halfFft = FFTN_R_ADC / 2;    // half the size of the first fft at ADC 64Msps real rate (2048)
const int fftPerBuf = transferSize / sizeof(short) / (3 * halfFft / 2) + 1; // number of ffts per buffer with 256|768 overlap
static fftwf_complex *pfilterht;       // time filter ht
static fftwf_complex **filterHw;       // Hw complex to each decimation ratio
#ifndef NDEBUG
static fftwf_plan *filterplan_f2t_c2c; // frequency to time fft used for debug
static fftwf_complex **filterHt;       // Ht time vector used for debug
#endif

r2iqControlClass::r2iqControlClass()
	
{
	bufIdx = 0;
	cntr = 0;
	r2iqOn = false;
	Initialized = false;
	randADC = false;
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
	Initialized = false;
}

int r2iqControlClass::Setdecimate(int dec)
{
	mdecimation =  dec;  // 0 , 2 , 4, 8, 16 =>  32, 16, 8, 4, 2 MHz
	return mratio[mdecimation];
}

void r2iqControlClass::Updt_SR_LO_TUNE(int srate_idx, int64_t* pfLO, int64_t* pfTune)
{
	int64_t LOfreq = *pfLO;
	if (LOfreq < (ADC_FREQ / 2))
	{
		Setdecimate(4 - srate_idx); // reverse order 32,16,8,4,2 MHz
		rf_mode rfm = RadioHandler.GetmodeRF();
		if (mdecimation == 0) // no decimation
		{
			rfm = HFMODE;
			RadioHandler.UpdatemodeRF(rfm);
			LOfreq = (int64_t)(((double)ADC_FREQ / 4.0) - 1000000.0);
			LOfreq = (int)(((double)LOfreq * (adcfixedfreq) / (double)ADC_FREQ)); // frequency correction
			*pfLO = LOfreq;
		}
		else if (rfm != VHFMODE)
		{
			rfm = HFMODE;
			RadioHandler.UpdatemodeRF(rfm);
			LOfreq = (int64_t)(*pfTune * (double)ADC_FREQ / (double)adcfixedfreq);
			// LO lower and upper limits
			if (LOfreq < 0) LOfreq = 0;
			if (LOfreq > (ADC_FREQ / 2) - (ADC_FREQ / 4) / mratio[mdecimation])
				LOfreq = (ADC_FREQ / 2) - (ADC_FREQ / 4) / mratio[mdecimation];
			/*
				int64_t loprecision = (ADC_FREQ / 2) / 256;   // ie 32000000 / 256 = 125 kHz  bin even span
				LOfreq = LOfreq + loprecision / 2;
				LOfreq /= loprecision;
				LOfreq *= loprecision;
				*/
			mtunebin = (int)((LOfreq * halfFft) / (ADC_FREQ / 2));// -halfFft / 32;
			*pfLO = (int)(((double)LOfreq * adcfixedfreq) / (double)ADC_FREQ); // frequency correction
		}
	}

}

int64_t r2iqControlClass::UptTuneFrq(int64_t* pLOfreq, int64_t* ptunefreq, int64_t* pLOcorr)
{
	int64_t loprecision = 1;
	int64_t LOinput = *pLOfreq + *pLOcorr;
	
	if ( *pLOfreq < (ADC_FREQ / 2))
	{
		rf_mode rfm = RadioHandler.GetmodeRF();

		if (mdecimation == 0) // no decimation
		{
			rfm = HFMODE;
			RadioHandler.UpdatemodeRF(rfm);
			*pLOfreq = (int64_t)((ADC_FREQ / 4.0) - 1000000.0);
			*pLOfreq = (int)(((double) *pLOfreq * adcfixedfreq) / (double)ADC_FREQ); // frequency correction
			loprecision = 1;
		}
		else if (rfm != VHFMODE)
		{
			rfm = HFMODE;
			RadioHandler.UpdatemodeRF(rfm);
			if (*pLOfreq < 0)  
				*pLOfreq = 0;
			if (*pLOfreq > (ADC_FREQ / 2) - (ADC_FREQ / 4) / mratio[mdecimation])
				*pLOfreq = (ADC_FREQ / 2) - (ADC_FREQ / 4) / mratio[mdecimation];

			loprecision = (ADC_FREQ / 2) / 256;   // ie 32000000 / 256 = 125 kHz  bin even span
			*pLOfreq = *pLOfreq + loprecision / 2;
			*pLOfreq /= loprecision;
			*pLOfreq *= loprecision;
			mtunebin = (int)((*pLOfreq * halfFft) / (ADC_FREQ / 2));
			*pLOfreq = (int)(((double) *pLOfreq * adcfixedfreq) / (double)ADC_FREQ); // frequency correction
		}
		*pLOcorr = LOinput - *pLOfreq; // correction to the exact LO -75 kHz < corr < 75 kHz
	} else {
		mtunebin = (int)(((int64_t)R820T2_IF_CARRIER * halfFft) / (ADC_FREQ / 2));
	}
	return *pLOcorr;
}

static std::condition_variable cvADCbufferAvailable;  // unlock when a sample buffer is ready
static std::mutex mutexR2iqControl;                   // r2iq control lock

r2iqThreadArg* threadArgs[N_R2IQ_THREAD];

void r2iqTurnOn(int idx) {
	r2iqCntrl.r2iqOn = true;
}

void r2iqTurnOff(void) {
	r2iqCntrl.r2iqOn = false;
	r2iqCntrl.cntr = 100;
	cvADCbufferAvailable.notify_all();
}

bool r2iqIsOn(void) { return(r2iqCntrl.r2iqOn); }

void r2iqDataReady(void) { // signals new sample buffer arrived
	if (!r2iqCntrl.r2iqOn)
		return;
	mutexR2iqControl.lock();
	r2iqCntrl.cntr++;
	mutexR2iqControl.unlock();
	cvADCbufferAvailable.notify_one(); // signal data available
}

void initR2iq(int downsample, float gain) {
	r2iqCntrl.buffers = buffers;    // set to the global exported by main_loop
	r2iqCntrl.obuffers = obuffers;  // set to the global exported by main_loop
	r2iqCntrl.Setdecimate(downsample);  // save downsample index.

	r2iqCntrl.GainScale = gain;

	// Get the processor count
	auto processor_count = std::thread::hardware_concurrency();
	if (processor_count > N_R2IQ_THREAD)
		processor_count = N_R2IQ_THREAD;

	if (!r2iqCntrl.Initialized) // only at init
	{
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
#ifndef NDEBUG
		filterHt = (fftwf_complex**)fftwf_malloc(sizeof(fftwf_complex*)*NDECIDX);
		filterplan_f2t_c2c = (fftwf_plan *)malloc(sizeof(fftwf_plan) * NDECIDX);
#endif
		for (int d = 0; d < NDECIDX; d++)
		{
			filterHw[d] = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*halfFft);     // 1024
#ifndef NDEBUG
			filterHt[d] = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*halfFft);     // 1024
			filterplan_f2t_c2c[d] = fftwf_plan_dft_1d(halfFft, filterHw[d], filterHt[d], FFTW_BACKWARD, FFTW_MEASURE); // 0??
#endif
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

			th->r2iqCntrl = &r2iqCntrl;
			th->ADCinTime = (float**)malloc(sizeof(float*) * fftPerBuf + 1);
			for (int n = 0; n < fftPerBuf; n++) {
				th->ADCinTime[n] = (float*)fftwf_malloc(sizeof(float) * 2 * halfFft);                 // 2048
			}

			th->ADCinFreq = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*(halfFft + 1)); // 1024+1
			th->inFreqTmp = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*(halfFft));    // 1024
			th->outTimeTmp = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*(halfFft));
			th->plan_t2f_r2c = fftwf_plan_dft_r2c_1d(2 * halfFft, th->ADCinTime[0], th->ADCinFreq, FFTW_MEASURE);
			th->plans_f2t_c2c = (fftwf_plan *)malloc(sizeof(fftwf_plan)   * NDECIDX);
			for (int d = 0; d < NDECIDX; d++)
			{
				th->plans_f2t_c2c[d] = fftwf_plan_dft_1d(r2iqCntrl.getFftN(d), th->inFreqTmp, th->outTimeTmp, FFTW_BACKWARD, FFTW_MEASURE);
			}

		}
	}
	else
	{
		DbgPrintf((char *) "r2iqCntrl initialization skipped\n");
	}

#if 0
	{
		std::unique_lock<std::mutex> lk(mutexR2iqControl);
		cvADCbufferAvailable.wait(lk);
	}
#endif

	for (unsigned t = 0; t < processor_count; t++) {
		r2iq_thread[t] = new std::thread(r2iqThreadf, (void*)threadArgs[t]);
	}
	r2iqCntrl.Initialized = true;
}

static void *r2iqThreadf(void *arg) {

	r2iqThreadArg *th = (struct r2iqThreadArg *)arg;
	r2iqControlClass *r2iqCntrl = (r2iqControlClass*)th->r2iqCntrl;


	//    DbgPrintf((char *) "r2iqThreadf idx %d pthread_self is %u\n",(int)th->t, pthread_self());
	//    DbgPrintf((char *) "decimate idx %d  %d  %d \n",r2iqCntrl->getDecidx(),r2iqCntrl->getFftN(),r2iqCntrl->getRatio());

	bool LWzero = r2iqCntrl->LWactive();
	char *buffer;
	int lastdecimate = -1;

	float * pout;
	int decimate = r2iqCntrl->getDecidx();
	int _mtunebin = r2iqCntrl->getTunebin();
	th->plan_f2t_c2c = th->plans_f2t_c2c[decimate];

	while (run) {
		int mfft = r2iqCntrl->getFftN();
		int mratio = r2iqCntrl->getRatio();
		int idx;

	    _mtunebin = r2iqCntrl->getTunebin();  // Update LO tune is possible during run
		 
		if (lastdecimate != decimate) {
			th->plan_f2t_c2c = th->plans_f2t_c2c[decimate];
			lastdecimate = decimate;
		}


		{
			std::unique_lock<std::mutex> lk(mutexR2iqControl);
			while (r2iqCntrl->cntr <= 0)
			{
			cvADCbufferAvailable.wait(lk, [r2iqCntrl] {return r2iqCntrl->cntr > 0; });
			}

			if (!run) 
				return 0;

			buffer = (char *)r2iqCntrl->buffers[r2iqCntrl->bufIdx];
			pout = (float *)r2iqCntrl->obuffers[r2iqCntrl->bufIdx];
			idx = r2iqCntrl->bufIdx;
			r2iqCntrl->bufIdx = ((r2iqCntrl->bufIdx + 1) % QUEUE_SIZE);
			r2iqCntrl->cntr--;
		}


		ADCSAMPLE *dataADC; // pointer to input data
		float *inloop;            // pointer to first fft input buffer
		int midx = r2iqCntrl->bufIdx;
		rf_mode  moderf = RadioHandler.GetmodeRF();
		dataADC = (ADCSAMPLE *)buffer;
		int blocks = fftPerBuf;
		int k = 0;
		if (!r2iqCntrl->randADC)        // plain samples no ADC rand set
		{
			if (idx == 0) {
				inloop = th->ADCinTime[0];
				int16_t *out = (int16_t*)(r2iqCntrl->buffers[QUEUE_SIZE - 1] + transferSize - FFTN_R_ADC);
				for (int m = 0; m < halfFft; m++) {
					*inloop++ = *out++;
				}
				for (int m = 0; m < halfFft; m++) {
					*inloop++ = *dataADC++;
				}
				k++;
			} else {
				// all other frames
				dataADC = dataADC - halfFft / 2;    // halfFft/2 overlap
			}

			for (; k < fftPerBuf; k++) {
				inloop = th->ADCinTime[k];
				for (int m = 0; m < 2 * halfFft; m++) {
					*inloop++ = *dataADC++;
				}
				dataADC = dataADC - halfFft / 2;
			}
		}
		else
		{
			if (idx == 0) {
				inloop = th->ADCinTime[0];
				int16_t *out = (int16_t*)(r2iqCntrl->buffers[QUEUE_SIZE - 1] + transferSize - FFTN_R_ADC);
				for (int m = 0; m < halfFft; m++) {
					*inloop++ = (RandTable[(UINT16)*out++]);
				}
				for (int m = 0; m < halfFft; m++) {
					*inloop++ = (RandTable[(UINT16)*dataADC++]);
				}
				k++;
			} else {
				// all other frames
				dataADC = dataADC - halfFft / 2;    // halfFft/2 overlap	
			}

			// all other frames
			dataADC = dataADC - halfFft / 2;  // halfFft/2 overlap
			for (; k < fftPerBuf; k++) {
				inloop = th->ADCinTime[k];
				for (int m = 0; m < 2 * halfFft; m++) {
					*inloop++ = (RandTable[(UINT16)*dataADC++]);
				}
				dataADC = dataADC - halfFft / 2;
			}
		}

		fftwf_complex* filter;
		filter = filterHw[decimate];

		if (decimate == 0)     // plain 32Msps no downsampling and tuning
		{
			for (int k = 0; k < fftPerBuf; k++)
			{
				// FFT first stage time to frequency, real to complex
				fftwf_execute_dft_r2c(th->plan_t2f_r2c, th->ADCinTime[k], th->ADCinFreq);
				// th->ADCinFreq[k][0] = th->ADCinFreq[k][0] /2.0f;   // DC component correction ? see
				// http://www.fftw.org/fftw3_doc/One_002dDimensional-DFTs-of-Real-Data.html

				if (LWzero)
				{
					th->inFreqTmp[0][0] = th->inFreqTmp[0][1] = 0;  // bin[0] = 0;

					for (int m = 1; m < halfFft / 2; m++) // circular shift tune fs/2 half array
					{
						th->inFreqTmp[m][0] = th->ADCinFreq[m][0] * filter[m][0] +
							th->ADCinFreq[m][1] * filter[m][1]; ;
						th->inFreqTmp[m][1] = th->ADCinFreq[m][1] * filter[m][0] -
							th->ADCinFreq[m][0] * filter[m][1];
					}
					for (int m = 0; m < halfFft / 2; m++) // circular shift tune fs/2 half array
					{
						th->inFreqTmp[halfFft / 2 + m][0] = 0;
						th->inFreqTmp[halfFft / 2 + m][1] = 0;
                    }
                }
				else if (moderf == HFMODE)
                {

				  int _mtunebin = halfFft / 2 -halfFft / 32; // upshift 1 MHz to have LW band in bandpass filter
                  int mm;
                  for(int m = 0 ; m < (halfFft/2); m++) // circular shift tune fs/2 half array
                    {
                        th->inFreqTmp[m][0] =  ( th->ADCinFreq[ _mtunebin+m][0] * filter[m][0]  +
                                                         th->ADCinFreq[ _mtunebin+m][1] * filter[m][1]);
                        th->inFreqTmp[m][1] =  ( th->ADCinFreq[ _mtunebin+m][1] * filter[m][0]  -
                                                         th->ADCinFreq[ _mtunebin+m][0] * filter[m][1]);
                    }

                  for(int m = 0 ; m < halfFft/2; m++) // circular shift tune fs/2 half array
                    {
                        mm = _mtunebin - halfFft/2 + m;
                        if ( mm >0)
                        {

                        th->inFreqTmp[halfFft/2+m][0] = ( th->ADCinFreq[mm][0] * filter[halfFft - halfFft/2 + m][0]  +
                                                                  th->ADCinFreq[mm][1] * filter[halfFft - halfFft/2 + m][1]);

                        th->inFreqTmp[halfFft/2+m][1] = ( th->ADCinFreq[mm][1] * filter[halfFft - halfFft/2 + m][0]  -
                                                                  th->ADCinFreq[mm][0] * filter[halfFft - halfFft/2 + m][1]);
                        }else
                        {
                            th->inFreqTmp[halfFft/2+m][0]= th->inFreqTmp[halfFft/2+m][1] = 0;
                        }
                    }

                }
				else
				{
					for (int m = 0; m < halfFft / 2; m++) // circular shift tune fs/2 half array
					{
						th->inFreqTmp[m][0] = th->ADCinFreq[halfFft / 2 + m][0] * filter[m][0] +
							th->ADCinFreq[halfFft / 2 + m][1] * filter[m][1]; ;
						th->inFreqTmp[m][1] = th->ADCinFreq[halfFft / 2 + m][1] * filter[m][0] -
							th->ADCinFreq[halfFft / 2 + m][0] * filter[m][1];
					}
					for (int m = 0; m < halfFft / 2; m++) // circular shift tune fs/2 half array
					{
						th->inFreqTmp[halfFft / 2 + m][0] = th->ADCinFreq[m][0] * filter[halfFft / 2 + m][0] +
							th->ADCinFreq[m][1] * filter[halfFft / 2 + m][1];

						th->inFreqTmp[halfFft / 2 + m][1] = th->ADCinFreq[m][1] * filter[halfFft / 2 + m][0] -
							th->ADCinFreq[m][0] * filter[halfFft / 2 + m][1];

					}
				}
				fftwf_execute(th->plan_f2t_c2c);                  //  c2c + decimation
				float scale;
				float gainadj = (float) pow(10.0, gdGainCorr_dB / 20.0);
				scale = r2iqCntrl->GainScale * gainadj;
				float * pTimeTmp;
				if (k == 0)
				{ // first frame is 512 sample long
					pTimeTmp = &th->outTimeTmp[halfFft / 4][0];
					for (int i = 0; i < halfFft / 2; i++)
					{
						*pout++ = scale * (*pTimeTmp++);
						*pout++ = scale * (*pTimeTmp++);
					}
				}
				else
				{ // (fftPerBuf-1) frames 768 sample long
					pTimeTmp = &th->outTimeTmp[0][0];
					for (int i = 0; i < 3 * halfFft / 4; i++)
					{
						*pout++ = scale * (*pTimeTmp++);
						*pout++ = scale * (*pTimeTmp++);
					}
				}
				// 42 * 768 + 512 = 32.768  sample output
			}
			//     DbgPrintf("len %d \n",j);
		}
		else
		{      // decimate in frequency plus tuning
			int modx = midx / mratio;
			int moff = midx - modx * mratio;
			int offset = ((transferSize / 2) / mratio) *moff;
			pout = (float *)(r2iqCntrl->obuffers[modx] + offset);

			for (int k = 0; k < fftPerBuf; k++)
			{
				// FFT first stage time to frequency, real to complex
				fftwf_execute_dft_r2c(th->plan_t2f_r2c, th->ADCinTime[k], th->ADCinFreq);

				if (LWzero)
				{
					th->inFreqTmp[0][0] = th->inFreqTmp[0][1] = 0;  // bin[0] = 0;
					for (int m = 1; m < mfft / 2; m++) // circular shift tune fs/2 half array
					{
						th->inFreqTmp[m][0] = (th->ADCinFreq[m][0] * filter[m][0] +
							th->ADCinFreq[m][1] * filter[m][1]);
						th->inFreqTmp[m][1] = (th->ADCinFreq[m][1] * filter[m][0] -
							th->ADCinFreq[m][0] * filter[m][1]);
					}
					for (int m = 0; m < mfft / 2; m++) // circular shift tune fs/2 half array
					{
						th->inFreqTmp[mfft / 2 + m][0] = 0;
						th->inFreqTmp[mfft / 2 + m][1] = 0;
					}
				}
				else
				{
					for (int m = 0; m < mfft / 2; m++) // circular shift tune fs/2 half array
					{
						th->inFreqTmp[m][0] = (th->ADCinFreq[_mtunebin + m][0] * filter[m][0] +
							th->ADCinFreq[_mtunebin + m][1] * filter[m][1]);
						th->inFreqTmp[m][1] = (th->ADCinFreq[_mtunebin + m][1] * filter[m][0] -
							th->ADCinFreq[_mtunebin + m][0] * filter[m][1]);
					}

					for (int m = 0; m < mfft / 2; m++) // circular shift tune fs/2 half array
					{


						if ((_mtunebin - mfft / 2 + m) >= 0) // corrects off limits

						{
							th->inFreqTmp[mfft / 2 + m][0] = (th->ADCinFreq[_mtunebin - mfft / 2 + m][0] * filter[halfFft - mfft / 2 + m][0] +
								th->ADCinFreq[_mtunebin - mfft / 2 + m][1] * filter[halfFft - mfft / 2 + m][1]);

							th->inFreqTmp[mfft / 2 + m][1] = (th->ADCinFreq[_mtunebin - mfft / 2 + m][1] * filter[halfFft - mfft / 2 + m][0] -
								th->ADCinFreq[_mtunebin - mfft / 2 + m][0] * filter[halfFft - mfft / 2 + m][1]);
						}
						else
						{
							th->inFreqTmp[mfft / 2 + m][0] = 0;
							th->inFreqTmp[mfft / 2 + m][1] = 0;
						}
					}
				}

				fftwf_execute(th->plan_f2t_c2c);     //  c2c decimation
				float scale;
				float gainadj = (float)pow(10.0, gdGainCorr_dB / 20.0);
				scale = r2iqCntrl->GainScale * gainadj;

				float * pTimeTmp;
				// here invert spectrum VHF
				if (moderf == VHFMODE) // invert spectrum
				{
					if (k == 0)
					{
						pTimeTmp = &th->outTimeTmp[mfft / 4][0];
						for (int i = 0; i < mfft / 2; i++)
						{
							*pout++ = scale * (*pTimeTmp++);
							*pout++ = -scale * (*pTimeTmp++);
						}
					}
					else
					{
						pTimeTmp = &th->outTimeTmp[0][0];
						for (int i = 0; i < 3 * mfft / 4; i++)
						{
							*pout++ = scale * (*pTimeTmp++);
							*pout++ = -scale * (*pTimeTmp++);
						}
					}
				}
				else // normal
				{
					if (k == 0)
					{  // first frame is mfft/2 long
						pTimeTmp = &th->outTimeTmp[mfft / 4][0];
						for (int i = 0; i < mfft / 2; i++)
						{
							*pout++ = scale * (*pTimeTmp++);
							*pout++ = scale * (*pTimeTmp++);
						}
					}
					else
					{  // other frames are 3*mfft/4 long
						pTimeTmp = &th->outTimeTmp[0][0];
						for (int i = 0; i < 3 * mfft / 4; i++)
						{
							*pout++ = scale * (*pTimeTmp++);
							*pout++ = scale * (*pTimeTmp++);
						}
					}
				}
			}
		}

	} // while(run)
//    DbgPrintf((char *) "r2iqThreadf idx %d pthread_exit %u\n",(int)th->t, pthread_self());
	return 0;
}
