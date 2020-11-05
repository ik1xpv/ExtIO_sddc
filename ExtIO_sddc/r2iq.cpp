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
extern HWND h_dialog;           // GUI dialog

class r2iqControlClass r2iqCntrl;

struct r2iqThreadArg {
	class r2iqControlClass *r2iqCntrl;
	long t;
};

static INT16 RandTable[65536];  // ADC RANDomize table used to whitening EMI from ADC data bus.

static void *r2iqThreadf(void *arg);   // thread function
static std::thread* r2iq_thread[N_R2IQ_THREAD]; // thread pointers

static int halfFft = FFTN_R_ADC / 2;   // half the size of the first fft at ADC 64Msps real rate (2048)
static int fftPerBuf;                  // number of fft per input buffer
static float ***ADCinTime;             // point to each threads input buffers [thread][nftt][n]
static fftwf_complex **ADCinFreq;      // buffers in frequency to each thread
static fftwf_complex **inFreqTmp;      // tmp decimation output buffers to each thread (after tune shift)
static fftwf_complex **outTimeTmp;     // tmp decimation output buffers baseband time cplx to each thread
static fftwf_plan **plan_t2f_r2c;      // fftw plan buffers Time to Freq real to complex  per thread per buffer
static fftwf_plan **plan_f2t_c2c;      // fftw plan buffers Freq to Time complex to complex per thread per decimation ratio
static int blockIdx;                   // index tuning buffer
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

void r2iqControlClass::Updt_SR_LO_TUNE(int srate_idx, int64_t* fLO, int64_t* fTune)
{
	int64_t LOfreq = *fLO;
	if (LOfreq < (ADC_FREQ / 2))
	{
		Setdecimate(4 - srate_idx); // reverse order 32,16,8,4,2 MHz
		rf_mode rfm = RadioHandler.GetmodeRF();
		if (mdecimation == 0) // no decimation
		{
			rfm = VLFMODE;
			RadioHandler.UpdatemodeRF(rfm);
			LOfreq = (int64_t)(((double)ADC_FREQ / 4.0) - 1000000.0);
			LOfreq = (int)(((double)LOfreq * (adcfixedfreq) / (double)ADC_FREQ)); // frequency correction
			*fLO = LOfreq;
		}
		else
		{
			if (*fTune < 500000)
				rfm = VLFMODE;
			else
				rfm = HFMODE;
			RadioHandler.UpdatemodeRF(rfm);
			LOfreq = (int64_t)(*fTune * (double)ADC_FREQ / (double)adcfixedfreq);
			// LO lower and upper limits
			if (LOfreq < ((double)ADC_FREQ / 4.0) / mratio[mdecimation])
				LOfreq = (int64_t)((double)ADC_FREQ / 4.0) / mratio[mdecimation];
			if (LOfreq > (ADC_FREQ / 2) - (ADC_FREQ / 4) / mratio[mdecimation])
				LOfreq = (ADC_FREQ / 2) - (ADC_FREQ / 4) / mratio[mdecimation];
			/*
				int64_t loprecision = (ADC_FREQ / 2) / 256;   // ie 32000000 / 256 = 125 kHz  bin even span
				LOfreq = LOfreq + loprecision / 2;
				LOfreq /= loprecision;
				LOfreq *= loprecision;
				*/
			mtunebin = (int)((LOfreq * halfFft) / (ADC_FREQ / 2));
			*fLO = (int)(((double)LOfreq * adcfixedfreq) / (double)ADC_FREQ); // frequency correction

		}
	}

}

int64_t r2iqControlClass::UptTuneFrq(int64_t LOfreq, int64_t tunefreq)
{
	int64_t loprecision;
	if (LOfreq < (ADC_FREQ / 2))
	{
		rf_mode rfm = RadioHandler.GetmodeRF();

		if (mdecimation == 0) // no decimation
		{
			rfm = VLFMODE;
			RadioHandler.UpdatemodeRF(rfm);
			//LOfreq = (int64_t)((adcfixedfreq / 4.0) - 1000000.0);
			LOfreq = (int64_t)((ADC_FREQ / 4.0) - 1000000.0);
			LOfreq = (int)(((double)LOfreq * adcfixedfreq) / (double)ADC_FREQ); // frequency correction
			loprecision = 1;
		}
		else
		{
			if (rfm != VHFMODE)
			{
				if (tunefreq < 1000000)
					rfm = VLFMODE;
				else
					rfm = HFMODE;
			}

			RadioHandler.UpdatemodeRF(rfm);

			if (LOfreq < (ADC_FREQ / 4) / mratio[mdecimation])
				LOfreq = (ADC_FREQ / 4) / mratio[mdecimation];
			if (LOfreq > (ADC_FREQ / 2) - (ADC_FREQ / 4) / mratio[mdecimation])
				LOfreq = (ADC_FREQ / 2) - (ADC_FREQ / 4) / mratio[mdecimation];

			if (rfm == VLFMODE)
			{
				LOfreq = (ADC_FREQ / 4) / mratio[mdecimation] - 500000;
			}

			loprecision = (ADC_FREQ / 2) / 256;   // ie 32000000 / 256 = 125 kHz  bin even span
			LOfreq = LOfreq + loprecision / 2;
			LOfreq /= loprecision;
			LOfreq *= loprecision;
			mtunebin = (int)((LOfreq * halfFft) / (ADC_FREQ / 2));
			LOfreq = (int)(((double)LOfreq * adcfixedfreq) / (double)ADC_FREQ); // frequency correction
		}
	} else {
		mtunebin = (int)(((int64_t)R820T2_IF_CARRIER * halfFft) / (ADC_FREQ / 2));
		loprecision = 1; // 1 Hz
	}
	// calculate nearest possible frequency
	// - emulate receiver which don't have 1 Hz resolution
	LOfreq += loprecision / 2;
	LOfreq /= loprecision;
	LOfreq *= loprecision;

	switch (global.radio) // update gains
	{
	case BBRF103:
		GainScale = BBRF103_GAINFACTOR;
		break;
	case HF103:
		GainScale = HF103_GAINFACTOR;
		break;
	case RX888:
		GainScale = RX888_GAINFACTOR;
		break;
	default:
		GainScale = BBRF103_GAINFACTOR;
		break;
	}

	return LOfreq;
}

static std::condition_variable cvADCbufferAvailable;  // unlock when a sample buffer is ready
static std::mutex mutexR2iqControl;                   // r2iq control lock

r2iqThreadArg *threadArg[N_R2IQ_THREAD];


void r2iqTurnOn(int idx) {
	r2iqCntrl.r2iqOn = true;
	mutexR2iqControl.lock();
	r2iqCntrl.bufIdx = idx - 1;
	if (r2iqCntrl.bufIdx < 0) r2iqCntrl.bufIdx += QUEUE_SIZE;
	r2iqCntrl.cntr = 1;
	cvADCbufferAvailable.notify_one();
	mutexR2iqControl.unlock();
}



void r2iqTurnOff(void) {
	r2iqCntrl.r2iqOn = false;
	r2iqCntrl.cntr = 1;
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

void initR2iq(int downsample) {
	r2iqCntrl.buffers = buffers;    // set to the global exported by main_loop
	r2iqCntrl.obuffers = obuffers;  // set to the global exported by main_loop
	r2iqCntrl.Setdecimate(downsample);  // save downsample index.
	fftPerBuf = global.transferSize / sizeof(short) / (3 * halfFft / 2) + 1; // number of ffts per buffer with 256|768 overlap
	blockIdx = 0;               // index to the tuning stage sample

	// Get the processor count
	auto processor_count = std::thread::hardware_concurrency();
	if (processor_count > N_R2IQ_THREAD)
		processor_count = N_R2IQ_THREAD;

	if (!r2iqCntrl.Initialized) // only at init
	{
		fftwf_plan* filterplan_t2f_c2c; // time to frequency fft

		DbgPrintf((char *) "r2iqCntrl initialization\n");
		plan_t2f_r2c = (fftwf_plan **)malloc(sizeof(fftwf_plan *) * processor_count);
		plan_f2t_c2c = (fftwf_plan **)malloc(sizeof(fftwf_plan *) * processor_count);
		ADCinTime = (float***)malloc(sizeof(float**) * processor_count);
		ADCinFreq = (fftwf_complex**)fftwf_malloc(sizeof(fftwf_complex **) * processor_count);
		inFreqTmp = (fftwf_complex**)fftwf_malloc(sizeof(fftwf_complex *) * processor_count);
		outTimeTmp = (fftwf_complex**)fftwf_malloc(sizeof(fftwf_complex *) * processor_count);
		for (unsigned int nthread = 0; nthread < processor_count; nthread++) {
			ADCinTime[nthread] = (float**)malloc(sizeof(float*) * fftPerBuf + 1  ); // +1 overlap
			ADCinFreq[nthread] = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*(halfFft + 1)); // 1024+1
			inFreqTmp[nthread] = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*(halfFft));    // 1024
			outTimeTmp[nthread] = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*(halfFft));
			plan_t2f_r2c[nthread] = (fftwf_plan *)malloc(sizeof(fftwf_plan)   * fftPerBuf);
		    plan_f2t_c2c[nthread] = (fftwf_plan *)malloc(sizeof(fftwf_plan)   * NDECIDX);
			for (int d = 0; d < NDECIDX; d++)
			{
				plan_f2t_c2c[nthread][d] = fftwf_plan_dft_1d(r2iqCntrl.getFftN(d), inFreqTmp[nthread], outTimeTmp[nthread], FFTW_BACKWARD, FFTW_MEASURE); // 0??
			}
			for (int n = 0; n < fftPerBuf; n++) {
				ADCinTime[nthread][n] = (float*)fftwf_malloc(sizeof(float) * 2 * halfFft);                 // 2048
				plan_t2f_r2c[nthread][n] = fftwf_plan_dft_r2c_1d(2 * halfFft, ADCinTime[nthread][n], ADCinFreq[nthread], FFTW_MEASURE);
			}
			threadArg[nthread] = (struct r2iqThreadArg *) malloc(sizeof(r2iqThreadArg));
		}

		// initialize RAND table for ADC

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

		   // filters
		pfilterht = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*halfFft);     // 1024
		filterHw = (fftwf_complex**)fftwf_malloc(sizeof(fftwf_complex*)*NDECIDX);
		filterplan_t2f_c2c = (fftwf_plan *)malloc(sizeof(fftwf_plan *) * NDECIDX);
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
			filterplan_t2f_c2c[d] = fftwf_plan_dft_1d(halfFft, pfilterht, filterHw[d], FFTW_FORWARD, FFTW_MEASURE);
		}

		for (int t = 0; t < halfFft; t++)
		{
			pfilterht[t][0] = 0.0f;
			pfilterht[t][1] = 0.0f;
		}

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

			fftwf_execute(filterplan_t2f_c2c[d]);

			fftwf_destroy_plan(filterplan_t2f_c2c[d]);

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

	for (unsigned int t = 0; t < processor_count; t++) {
		threadArg[t]->t = (long) t;
		threadArg[t]->r2iqCntrl = &r2iqCntrl;

		r2iq_thread[t] = new std::thread(r2iqThreadf, (void *)threadArg[t]);
	}
	r2iqCntrl.Initialized = true;
}

static void *r2iqThreadf(void *arg) {

	r2iqThreadArg *th = (r2iqThreadArg *)arg;
	r2iqControlClass *r2iqCntrl = th->r2iqCntrl;

	//    DbgPrintf((char *) "r2iqThreadf idx %d pthread_self is %u\n",(int)th->t, pthread_self());
	//    DbgPrintf((char *) "decimate idx %d  %d  %d \n",r2iqCntrl->getDecidx(),r2iqCntrl->getFftN(),r2iqCntrl->getRatio());

	long thisThread = th->t;
	long lastThread = 0;
	bool LWzero = r2iqCntrl->LWactive();
	char *buffer;

	float * pout;
	while (global.run) {
		int decimate = r2iqCntrl->getDecidx();
		int mfft = r2iqCntrl->getFftN();
		int mratio = r2iqCntrl->getRatio();

		{
			std::unique_lock<std::mutex> lk(mutexR2iqControl);
			cvADCbufferAvailable.wait(lk, [r2iqCntrl] {return r2iqCntrl->cntr > 0; });

			if (!global.run) 
				return 0;

			buffer = (char *)r2iqCntrl->buffers[r2iqCntrl->bufIdx];
			pout = (float *)r2iqCntrl->obuffers[r2iqCntrl->bufIdx];
			r2iqCntrl->bufIdx = ((r2iqCntrl->bufIdx + 1) % QUEUE_SIZE);
			lastThread = r2iqCntrl->lastThread;
			r2iqCntrl->lastThread = thisThread;
			r2iqCntrl->cntr--;
		}

		ADCSAMPLE *dataADC; // pointer to input data
		float *inloop;            // pointer to first fft input buffer
		float *endloop;           // pointer to end data to be copied to beginning
		int midx = r2iqCntrl->bufIdx;
		rf_mode  moderf = RadioHandler.GetmodeRF();
		inloop = ADCinTime[thisThread][0];
		endloop = ADCinTime[lastThread][fftPerBuf - 1] + halfFft;
		// first frame
		for (int m = 0; m < halfFft; m++) {
			*inloop++ = *endloop++;   // duplicate form last frame halfFft samples
		}
		dataADC = (ADCSAMPLE *)buffer;
		if (!r2iqCntrl->randADC)        // plain samples no ADC rand set
		{
			// completes first frame
			for (int m = 0; m < halfFft; m++) {
				*inloop++ = *dataADC++;
			}
			// all other frames
			dataADC = dataADC - halfFft / 2;    // halfFft/2 overlap
			for (int k = 1; k < fftPerBuf; k++) {
				inloop = ADCinTime[thisThread][k];
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
			dataADC = dataADC - halfFft / 2;  // halfFft/2 overlap
			for (int k = 1; k < fftPerBuf; k++) {
				inloop = ADCinTime[thisThread][k];
				for (int m = 0; m < 2 * halfFft; m++) {
					*inloop++ = (RandTable[(UINT16)*dataADC++]);
				}
				dataADC = dataADC - halfFft / 2;
			}
		}
		if (decimate == 0)     // plain 32Msps no downsampling and tuning
		{
			for (int k = 0; k < fftPerBuf; k++)
			{
				fftwf_execute(plan_t2f_r2c[thisThread][k]);   // FFT first stage time to frequency, real to complex
				// ADCinFreq[thisThread][k][0] = ADCinFreq[thisThread][k][0] /2.0f;   // DC component correction ? see
				// http://www.fftw.org/fftw3_doc/One_002dDimensional-DFTs-of-Real-Data.html

				if (LWzero)
				{
					inFreqTmp[thisThread][0][0] = inFreqTmp[thisThread][0][1] = 0;  // bin[0] = 0;

					for (int m = 1; m < halfFft / 2; m++) // circular shift tune fs/2 half array
					{
						inFreqTmp[thisThread][m][0] = ADCinFreq[thisThread][m][0] * filterHw[0][m][0] +
							ADCinFreq[thisThread][m][1] * filterHw[0][m][1]; ;
						inFreqTmp[thisThread][m][1] = ADCinFreq[thisThread][m][1] * filterHw[0][m][0] -
							ADCinFreq[thisThread][m][0] * filterHw[0][m][1];
					}
					for (int m = 0; m < halfFft / 2; m++) // circular shift tune fs/2 half array
					{
						inFreqTmp[thisThread][halfFft / 2 + m][0] = 0;
						inFreqTmp[thisThread][halfFft / 2 + m][1] = 0;
					}
				}
				
				 if (moderf == VLFMODE)
                {
                  int mtunebin = halfFft/2 - halfFft/32;
                  int mm;
                  for(int m = 0 ; m < halfFft/2; m++) // circular shift tune fs/2 half array
                    {
                        inFreqTmp[thisThread][m][0] =  ( ADCinFreq[thisThread][ mtunebin+m][0] * filterHw[decimate][m][0]  +
                                                         ADCinFreq[thisThread][ mtunebin+m][1] * filterHw[decimate][m][1]);
                        inFreqTmp[thisThread][m][1] =  ( ADCinFreq[thisThread][ mtunebin+m][1] * filterHw[decimate][m][0]  -
                                                         ADCinFreq[thisThread][ mtunebin+m][0] * filterHw[decimate][m][1]);
                    }

                  for(int m = 0 ; m < halfFft/2; m++) // circular shift tune fs/2 half array
                    {
                        mm = mtunebin - halfFft/2 + m;
                        if ( mm >0)
                        {

                        inFreqTmp[thisThread][halfFft/2+m][0] = ( ADCinFreq[thisThread][mm][0] * filterHw[decimate][halfFft - halfFft/2 + m][0]  +
                                                                  ADCinFreq[thisThread][mm][1] * filterHw[decimate][halfFft - halfFft/2 + m][1]);

                        inFreqTmp[thisThread][halfFft/2+m][1] = ( ADCinFreq[thisThread][mm][1] * filterHw[decimate][halfFft - halfFft/2 + m][0]  -
                                                                  ADCinFreq[thisThread][mm][0] * filterHw[decimate][halfFft - halfFft/2 + m][1]);
                        }else
                        {
                            inFreqTmp[thisThread][halfFft/2+m][0]= inFreqTmp[thisThread][halfFft/2+m][1] = 0;
                        }
                    }

                }
				else
				{
					for (int m = 0; m < halfFft / 2; m++) // circular shift tune fs/2 half array
					{
						inFreqTmp[thisThread][m][0] = ADCinFreq[thisThread][halfFft / 2 + m][0] * filterHw[0][m][0] +
							ADCinFreq[thisThread][halfFft / 2 + m][1] * filterHw[0][m][1]; ;
						inFreqTmp[thisThread][m][1] = ADCinFreq[thisThread][halfFft / 2 + m][1] * filterHw[0][m][0] -
							ADCinFreq[thisThread][halfFft / 2 + m][0] * filterHw[0][m][1];
					}
					for (int m = 0; m < halfFft / 2; m++) // circular shift tune fs/2 half array
					{
						inFreqTmp[thisThread][halfFft / 2 + m][0] = ADCinFreq[thisThread][m][0] * filterHw[0][halfFft / 2 + m][0] +
							ADCinFreq[thisThread][m][1] * filterHw[0][halfFft / 2 + m][1];

						inFreqTmp[thisThread][halfFft / 2 + m][1] = ADCinFreq[thisThread][m][1] * filterHw[0][halfFft / 2 + m][0] -
							ADCinFreq[thisThread][m][0] * filterHw[0][halfFft / 2 + m][1];

					}
				}
				fftwf_execute(plan_f2t_c2c[thisThread][decimate]);                  //  c2c + decimation
				float scale;
				float gainadj = (float) pow(10.0, gdGainCorr_dB / 20.0);
				scale = r2iqCntrl->GainScale * gainadj;
				float * pTimeTmp;
				if (k == 0)
				{ // first frame is 512 sample long
					pTimeTmp = &outTimeTmp[thisThread][halfFft / 4][0];
					for (int i = 0; i < halfFft / 2; i++)
					{
						*pout++ = scale * (*pTimeTmp++);
						*pout++ = scale * (*pTimeTmp++);
					}
				}
				else
				{ // (fftPerBuf-1) frames 768 sample long
					pTimeTmp = &outTimeTmp[thisThread][0][0];
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
			int offset = ((global.transferSize / 2) / mratio) *moff;
			pout = (float *)(r2iqCntrl->obuffers[modx] + offset);
			int mtunebin = r2iqCntrl->getTunebin();

			for (int k = 0; k < fftPerBuf; k++)
			{
				fftwf_execute(plan_t2f_r2c[thisThread][k]);                 // FFT first stage time to frequency, real to complex

				if (LWzero)
				{
					inFreqTmp[thisThread][0][0] = inFreqTmp[thisThread][0][1] = 0;  // bin[0] = 0;
					for (int m = 1; m < mfft / 2; m++) // circular shift tune fs/2 half array
					{
						inFreqTmp[thisThread][m][0] = (ADCinFreq[thisThread][m][0] * filterHw[decimate][m][0] +
							ADCinFreq[thisThread][m][1] * filterHw[decimate][m][1]);
						inFreqTmp[thisThread][m][1] = (ADCinFreq[thisThread][m][1] * filterHw[decimate][m][0] -
							ADCinFreq[thisThread][m][0] * filterHw[decimate][m][1]);
					}
					for (int m = 0; m < mfft / 2; m++) // circular shift tune fs/2 half array
					{
						inFreqTmp[thisThread][mfft / 2 + m][0] = 0;
						inFreqTmp[thisThread][mfft / 2 + m][1] = 0;
					}
				}
				else
				if (moderf == VLFMODE)
				{
				  int mm;
				  for(int m = 0 ; m < mfft/2; m++) // circular shift tune fs/2 half array
					{
						inFreqTmp[thisThread][m][0] =  ( ADCinFreq[thisThread][ mtunebin+m][0] * filterHw[decimate][m][0]  +
														 ADCinFreq[thisThread][ mtunebin+m][1] * filterHw[decimate][m][1]);
						inFreqTmp[thisThread][m][1] =  ( ADCinFreq[thisThread][ mtunebin+m][1] * filterHw[decimate][m][0]  -
														 ADCinFreq[thisThread][ mtunebin+m][0] * filterHw[decimate][m][1]);
					}
				  for(int m = 0 ; m < mfft/2; m++) // circular shift tune fs/2 half array
					{
						mm = mtunebin - mfft/2 + m;
						if ( mm > 0)
						{

						inFreqTmp[thisThread][mfft/2+m][0] = ( ADCinFreq[thisThread][mm][0] * filterHw[decimate][halfFft - mfft/2 + m][0]  +
																  ADCinFreq[thisThread][mm][1] * filterHw[decimate][halfFft - mfft/2 + m][1]);

						inFreqTmp[thisThread][mfft/2+m][1] = ( ADCinFreq[thisThread][mm][1] * filterHw[decimate][halfFft - mfft/2 + m][0]  -
																  ADCinFreq[thisThread][mm][0] * filterHw[decimate][halfFft - mfft/2 + m][1]);
						}else
						{
							inFreqTmp[thisThread][mfft/2+m][0]= inFreqTmp[thisThread][mfft/2+m][1] = 0;
						}
					}
				}
				else
				{
					for (int m = 0; m < mfft / 2; m++) // circular shift tune fs/2 half array
					{
						inFreqTmp[thisThread][m][0] = (ADCinFreq[thisThread][mtunebin + m][0] * filterHw[decimate][m][0] +
							ADCinFreq[thisThread][mtunebin + m][1] * filterHw[decimate][m][1]);
						inFreqTmp[thisThread][m][1] = (ADCinFreq[thisThread][mtunebin + m][1] * filterHw[decimate][m][0] -
							ADCinFreq[thisThread][mtunebin + m][0] * filterHw[decimate][m][1]);
					}

					for (int m = 0; m < mfft / 2; m++) // circular shift tune fs/2 half array
					{
						inFreqTmp[thisThread][mfft / 2 + m][0] = (ADCinFreq[thisThread][mtunebin - mfft / 2 + m][0] * filterHw[decimate][halfFft - mfft / 2 + m][0] +
							ADCinFreq[thisThread][mtunebin - mfft / 2 + m][1] * filterHw[decimate][halfFft - mfft / 2 + m][1]);

						inFreqTmp[thisThread][mfft / 2 + m][1] = (ADCinFreq[thisThread][mtunebin - mfft / 2 + m][1] * filterHw[decimate][halfFft - mfft / 2 + m][0] -
							ADCinFreq[thisThread][mtunebin - mfft / 2 + m][0] * filterHw[decimate][halfFft - mfft / 2 + m][1]);
					}
				}

				fftwf_execute(plan_f2t_c2c[thisThread][decimate]);     //  c2c decimation
			
				float scale;
				float gainadj = (float)pow(10.0, gdGainCorr_dB / 20.0);
				scale = r2iqCntrl->GainScale * gainadj;

				float * pTimeTmp;
				// here invert spectrum VHF
				if (moderf == VHFMODE) // invert spectrum
				{
					if (k == 0)
					{
						pTimeTmp = &outTimeTmp[thisThread][mfft / 4][0];
						for (int i = 0; i < mfft / 2; i++)
						{
							*pout++ = scale * (*pTimeTmp++);
							*pout++ = -scale * (*pTimeTmp++);
						}
					}
					else
					{
						pTimeTmp = &outTimeTmp[thisThread][0][0];
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
						pTimeTmp = &outTimeTmp[thisThread][mfft / 4][0];
						for (int i = 0; i < mfft / 2; i++)
						{
							*pout++ = scale * (*pTimeTmp++);
							*pout++ = scale * (*pTimeTmp++);
						}
					}
					else
					{  // other frames are 3*mfft/4 long
						pTimeTmp = &outTimeTmp[thisThread][0][0];
						for (int i = 0; i < 3 * mfft / 4; i++)
						{
							*pout++ = scale * (*pTimeTmp++);
							*pout++ = scale * (*pTimeTmp++);
						}
					}
				}
			}
		}

	} // while(global.run)
//    DbgPrintf((char *) "r2iqThreadf idx %d pthread_exit %u\n",(int)th->t, pthread_self());
	return 0;
}
