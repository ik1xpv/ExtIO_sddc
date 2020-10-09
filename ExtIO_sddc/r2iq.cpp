#include "license.txt" // Oscar Steila ik1xpv
/*
The ADC input real stream at Fs = 64 Msps  (short) is converted to:
- 32 Msps float Fs/2 complex stream, or
- 16 Msps float Fs/2 complex stream, or
-  8 Msps float Fs/2 complex stream, or
-  4 Msps float Fs/2 complex stream, or
-  2 Msps float Fs/2 complex stream.
The decimation factor is selectable from HDSDR GUI sampling rate selector

*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include "r2iq.h"
#include "config.h"
#include "fftw3.h"
#include "BBRF103.h"
#include "resource.h"
#include "ht257_0_0.h"
#include "ht257_0_7M.h"
#include "ht257_1_7M.h"
#include "ht257_3_6M.h"
#include "ht257_7_5M.h"
#include "ht257_15_4M.h"
#include "R820T2.h"

extern PUCHAR	*buffers;       // ExtIO_sddc defined
extern float**   obuffers;
extern HWND h_dialog;           // GUI dialog

class r2iqControlClass r2iqCntrl;

struct r2iqThreadArg {
    class r2iqControlClass *r2iqCntrl;
    long t;
};

static INT16 RandTable[65536];  // ADC RANDomize table used to whitening EMI from ADC data bus.

void *r2iqThreadf(void *arg);   // thread function
pthread_t r2iq_thread[N_R2IQ_THREAD]; // thread pointers

int halfFft = 1024;             // half the size of the first fft at ADC 64Msps real rate (2048)
int fftPerBuf;                  // number of fft per input buffer
float ***ADCinTime;             // point to each threads input buffers [thread][nftt][n]
fftwf_complex **ADCinFreq;      // buffers in frequency to each thread
fftwf_complex **inFreqTmp;      // tmp decimation output buffers to each thread (after tune shift)
fftwf_complex **outTimeTmp;     // tmp decimation output buffers baseband time cplx to each thread
fftwf_plan **plan_t2f_r2c;      // fftw plan buffers Time to Freq real to complex  per thread per buffer
fftwf_plan **plan_f2t_c2c;      // fftw plan buffers Freq to Time complex to complex per thread per decimation ratio
int blockIdx;                   // index tuning buffer
fftwf_complex *pfilterht;       // time filter ht
fftwf_plan *filterplan_t2f_c2c; // time to frequency fft
fftwf_complex **filterHw;       // Hw complex to each decimation ratio
#ifndef NDEBUG
fftwf_plan *filterplan_f2t_c2c; // frequency to time fft used for debug
fftwf_complex **filterHt;       // Ht time vector used for debug
#endif

r2iqControlClass::r2iqControlClass()
{
    bufIdx = 0;
    cntr = 0;
    r2iqOn = false;
    Initialized = false;
    randADC = false;
    mtunebin = halfFft/4;
    mfftdim [0] = halfFft ;
    mratio [0] = 1 ;
    for (int i = 1; i < NDECIDX; i++ )
    {
       mfftdim [i] = mfftdim [i-1]/2;
       mratio [i] = mratio [i-1]*2;
    }
}

int r2iqControlClass::Setdecimate(int dec)
{
     mdecimation = dec;
     return 0;
}

int64_t r2iqControlClass::UptTuneFrq(int64_t LOfreq)
{
    int64_t loprecision;
    if ( LOfreq < (ADC_FREQ/2) )
    {
        if (mdecimation == 0) // no decimation
            LOfreq = (ADC_FREQ/4);
        else
        {
            if (LOfreq < (ADC_FREQ/4)/mratio[mdecimation])
                LOfreq = (ADC_FREQ/4)/mratio[mdecimation];
            if (LOfreq > (ADC_FREQ/2) - (ADC_FREQ/4)/mratio[mdecimation])
                LOfreq = (ADC_FREQ/2) - (ADC_FREQ/4)/mratio[mdecimation];
            if (BBRF103.GetmodeRF() == VLFMODE)
                LOfreq = 0;
        }
        loprecision = (ADC_FREQ/2) / 256;   // ie 32000000 / 256 = 125 kHz  bin even span
        LOfreq = LOfreq + loprecision/2 ;
        LOfreq /= loprecision;
        LOfreq *= loprecision;
        mtunebin = (int)((LOfreq*halfFft) / (ADC_FREQ/2));
    }else{
        mtunebin = (int)((( int64_t)IFR820T*halfFft) / (ADC_FREQ/2));
        loprecision = 1; // 1 Hz
    }
	// calculate nearest possible frequency
	// - emulate receiver which don't have 1 Hz resolution
	LOfreq += loprecision/2 ;
	LOfreq /= loprecision;
	LOfreq *= loprecision;

    return LOfreq;
}

pthread_mutex_t mutexADCbufferAvailable;  // unlock when a sample buffer is ready
pthread_mutex_t mutexR2iqControl;         // r2iq control lock

r2iqThreadArg *threadArg[N_R2IQ_THREAD];


void r2iqTurnOn(int idx) {
    r2iqCntrl.r2iqOn = true;
    pthread_mutex_lock (&mutexR2iqControl);
    r2iqCntrl.bufIdx = idx-1;
    if(r2iqCntrl.bufIdx < 0) r2iqCntrl.bufIdx += QUEUE_SIZE;
    r2iqCntrl.cntr = 1;
    pthread_mutex_unlock (&mutexADCbufferAvailable);
    pthread_mutex_unlock (&mutexR2iqControl);
}



void r2iqTurnOff(void) {
    r2iqCntrl.r2iqOn = false;
    r2iqCntrl.cntr = 0;
}

bool r2iqIsOn(void){ return(r2iqCntrl.r2iqOn); }

void r2iqDataReady(void) { // signals new sample buffer arrived
    if(!r2iqCntrl.r2iqOn)
        return;
    pthread_mutex_lock (&mutexR2iqControl);
    r2iqCntrl.cntr++;
    pthread_mutex_unlock (&mutexADCbufferAvailable); // signal data available
    pthread_mutex_unlock (&mutexR2iqControl);
}

void initR2iq( int downsample ) {
    r2iqCntrl.buffers = buffers;    // set to the global exported by main_loop
    r2iqCntrl.obuffers = obuffers;  // set to the global exported by main_loop
    r2iqCntrl.Setdecimate(downsample);  // save downsample index.
    fftPerBuf = global.transferSize / sizeof(short) / (3*halfFft / 2) + 1; // number of ffts per buffer with 256|768 overlap
    blockIdx = 0;               // index to the tuning stage sample

    if (!r2iqCntrl.Initialized) // only at init
    {
        DbgPrintf((char *) "r2iqCntrl initialization\n");
        plan_t2f_r2c = (fftwf_plan **) malloc(sizeof(fftwf_plan *) * N_R2IQ_THREAD);
        plan_f2t_c2c = (fftwf_plan **) malloc(sizeof(fftwf_plan *) * N_R2IQ_THREAD);
        ADCinTime  = (float***) malloc(sizeof(float**) * N_R2IQ_THREAD);
        ADCinFreq = (fftwf_complex**) fftwf_malloc(sizeof(fftwf_complex **) * N_R2IQ_THREAD);
        inFreqTmp = (fftwf_complex**) fftwf_malloc(sizeof(fftwf_complex *) * N_R2IQ_THREAD);
        outTimeTmp = (fftwf_complex**) fftwf_malloc(sizeof(fftwf_complex *) * N_R2IQ_THREAD);
        for(int nthread = 0; nthread < N_R2IQ_THREAD; nthread++) {
            ADCinTime[nthread]  = (float**) malloc(sizeof(float*) * fftPerBuf + 1 ); // +1 overlap
            ADCinFreq[nthread] = (fftwf_complex*) fftwf_malloc (sizeof(fftwf_complex)*(halfFft +1)); // 1024+1
            inFreqTmp[nthread] = (fftwf_complex*) fftwf_malloc (sizeof(fftwf_complex)*(halfFft));    // 1024
            outTimeTmp[nthread] = (fftwf_complex*) fftwf_malloc (sizeof(fftwf_complex)*(halfFft));
            plan_t2f_r2c[nthread] = (fftwf_plan *)  malloc(sizeof(fftwf_plan)   * fftPerBuf);
            plan_f2t_c2c[nthread] = (fftwf_plan *)  malloc(sizeof(fftwf_plan)   * NDECIDX);
            for (int d = 0; d < NDECIDX; d++)
            {
                plan_f2t_c2c[nthread][d] = fftwf_plan_dft_1d(r2iqCntrl.getFftN(d), inFreqTmp[nthread], outTimeTmp[nthread], FFTW_BACKWARD, FFTW_MEASURE); // 0??
            }
            for(int n = 0; n < fftPerBuf; n++) {
                ADCinTime[nthread][n]  = (float*) fftwf_malloc(sizeof(float) * 2 * halfFft);                 // 2048
                plan_t2f_r2c[nthread][n] = fftwf_plan_dft_r2c_1d(2*halfFft, ADCinTime[nthread][n], ADCinFreq[nthread],FFTW_MEASURE);
            }
            threadArg[nthread] = (struct r2iqThreadArg *) malloc(sizeof(r2iqThreadArg));
        }

 // load RAND table for ADC

        for (int k= 0; k <= 65536; k++)
        {
            if ((k&1) == 1)
                {
                RandTable[k] = (k ^ 0xFFFE);}
            else{
                RandTable[k] =  k ;}
        }
 //        DbgPrintf((char *) "RandTable generated\n");

    // filters
    pfilterht  = (fftwf_complex*) fftwf_malloc (sizeof(fftwf_complex)*halfFft);     // 1024
    filterHw = (fftwf_complex**) fftwf_malloc (sizeof(fftwf_complex*)*NDECIDX);
    filterplan_t2f_c2c = (fftwf_plan *) malloc(sizeof(fftwf_plan *) * NDECIDX);
#ifndef NDEBUG
    filterHt = (fftwf_complex**) fftwf_malloc (sizeof(fftwf_complex*)*NDECIDX);
    filterplan_f2t_c2c = (fftwf_plan *) malloc(sizeof(fftwf_plan ) * NDECIDX);
#endif
    for (int d = 0; d < NDECIDX; d++)
    {
        filterHw[d] = (fftwf_complex*) fftwf_malloc (sizeof(fftwf_complex)*halfFft);     // 1024
#ifndef NDEBUG
        filterHt[d] = (fftwf_complex*) fftwf_malloc (sizeof(fftwf_complex)*halfFft);     // 1024
        filterplan_f2t_c2c[d] =  fftwf_plan_dft_1d(halfFft, filterHw[d], filterHt[d], FFTW_BACKWARD, FFTW_MEASURE); // 0??
#endif
        filterplan_t2f_c2c[d] =  fftwf_plan_dft_1d(halfFft, pfilterht,  filterHw[d], FFTW_FORWARD, FFTW_MEASURE);
    }
    for(int t=0; t <1024; t++)
    {
        pfilterht[t][0] = 0.0;
        pfilterht[t][1] = 0.0;
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
        for(int t=0; t <257; t++)
        {
              // pfilterht[t][0] = pht[t]/sqrt(2.0);
               pfilterht[t][0] = pfilterht[t][1] = pht[t]/sqrt(2.0);
        }

        fftwf_execute(filterplan_t2f_c2c[d]);
#ifndef NDEBUG
  if(0) // enable if debugging windowing
    {
        fftwf_execute(filterplan_f2t_c2c[d]);
        char s = 0x30+d;
        {
            FILE * hfiletrace; // save input real sequence  short
            char buf[] = " filterin.raw";
            buf[0]= s;
            hfiletrace = fopen( buf, "wb");
            fwrite((float *)pfilterht ,sizeof(float),2048, hfiletrace);
            fclose(hfiletrace);
        }
        {
            FILE * hfiletrace; //        save input float sequence
            char buf[] = " filterHw.raw";
            buf[0]= s;
            hfiletrace = fopen( buf, "wb");
            fwrite((float *)filterHw[d], sizeof(float), 2*1024, hfiletrace);
            fclose(hfiletrace);
        }
        {
            FILE * hfiletrace; //        save input float sequence
            char buf[] = " filterHt.raw";
            buf[0]= s;
            hfiletrace = fopen( buf, "wb");
            fwrite((float *)filterHt[d],sizeof(float), 2*1024, hfiletrace);
            fclose(hfiletrace);
        }
    }
 #endif
    }

    }
    else
    {
        DbgPrintf((char *) "r2iqCntrl initialization skipped\n");
    }
    int r;
    r = pthread_mutex_init(&mutexADCbufferAvailable, NULL);
    if (r) DbgPrintf("\nERROR: return code from pthread_mutex_init(&mutexSamplesAvailable) is %d\n", r);
    r = pthread_mutex_lock(&mutexADCbufferAvailable);
    if (r) DbgPrintf("\nERROR: return code from pthread_mutex_lock(&mutexSamplesAvailable) is %d\n", r);
    r = pthread_mutex_init(&mutexR2iqControl, NULL);
    if (r) DbgPrintf("\nERROR: return code from pthread_mutex_init(&mutexR2iqControl) is %d\n", r);
    r = pthread_mutex_unlock(&mutexR2iqControl);
    if (r) DbgPrintf("\nERROR: return code from pthread_mutex_unlock(&mutexR2iqControl) is %d\n", r);

    for(int t = 0; t < N_R2IQ_THREAD; t++) {
        threadArg[t]->t = t;
        threadArg[t]->r2iqCntrl = &r2iqCntrl;

        int rc = pthread_create(&r2iq_thread[t], NULL,
            (void* (__attribute__((__cdecl__)) *)(void*) )  r2iqThreadf, (void *) threadArg[t]);
        if (rc) DbgPrintf("\nERROR: return code from pthread_create() is %d\n", rc);
    }
    r2iqCntrl.Initialized = true;
}

void *r2iqThreadf(void *arg) {

    r2iqThreadArg *th = (r2iqThreadArg *) arg;
    r2iqControlClass *r2iqCntrl = th->r2iqCntrl;
    int decimate = r2iqCntrl->getDecidx();
    int mfft = r2iqCntrl->getFftN();
    int mratio = r2iqCntrl->getRatio();

//    DbgPrintf((char *) "r2iqThreadf idx %d pthread_self is %u\n",(int)th->t, pthread_self());
//    DbgPrintf((char *) "decimate idx %d  %d  %d \n",r2iqCntrl->getDecidx(),r2iqCntrl->getFftN(),r2iqCntrl->getRatio());

    long thisThread = th->t;
    long lastThread = 0;
    char *buffer;

    float * pout;
    while(global.run) {
        pthread_mutex_lock(&mutexADCbufferAvailable);
        pthread_mutex_lock(&mutexR2iqControl);
        if(r2iqCntrl->cntr > 0) { //sanity check

            buffer = (char *)r2iqCntrl->buffers[r2iqCntrl->bufIdx];
            pout = (float *)r2iqCntrl->obuffers[r2iqCntrl->bufIdx];
            r2iqCntrl->bufIdx = ((r2iqCntrl->bufIdx + 1)%QUEUE_SIZE);
            lastThread = r2iqCntrl->lastThread;
            r2iqCntrl->lastThread = thisThread;
            r2iqCntrl->cntr--;
            if(r2iqCntrl->cntr > 0)
                pthread_mutex_unlock(&mutexADCbufferAvailable);
            pthread_mutex_unlock(&mutexR2iqControl);
        } else {
            pthread_mutex_unlock(&mutexR2iqControl);
            continue;
        }

        ADCSAMPLE *dataADC; // pointer to input data
        float *inloop;            // pointer to first fft input buffer
        float *endloop;           // pointer to end data to be copied to beginning
        int midx = r2iqCntrl->bufIdx;
        rf_mode  moderf = BBRF103.GetmodeRF();
        inloop = ADCinTime[thisThread][0];
        endloop = ADCinTime[lastThread][fftPerBuf-1] + halfFft;
        // first frame
        for(int m = 0; m < halfFft; m++) {
            *inloop++ =  *endloop++ ;   // duplicate form last frame halfFft samples
        }
        dataADC = (ADCSAMPLE *) buffer;
        if (!r2iqCntrl->randADC)        // plain samples no ADC rand set
        {
            // completes first frame
            for(int m = 0; m < halfFft; m++) {
                *inloop++ = *dataADC++;
            }
            // all other frames
            dataADC = dataADC - halfFft/2;    // halfFft/2 overlap
            for(int k = 1;k < fftPerBuf ;k++) {
                inloop = ADCinTime[thisThread][k];
                for(int m = 0; m < 2*halfFft; m++) {
                     *inloop++ = *dataADC++;
                }
                dataADC = dataADC - halfFft/2;
            }
        }
        else
        {
           // completes first frame
            for(int m = 0; m < halfFft; m++) {
                *inloop++ = (RandTable[(UINT16)*dataADC++]);
            }
            // all other frames
            dataADC = dataADC - halfFft/2;  // halfFft/2 overlap
            for(int k = 1;k < fftPerBuf;k++) {
                inloop = ADCinTime[thisThread][k];
                for(int m = 0; m < 2*halfFft; m++) {
                     *inloop++ = (RandTable[(UINT16)*dataADC++]);
                }
                dataADC = dataADC - halfFft/2;
            }
        }
        if (decimate == 0)     // plain 32Msps no downsampling and tuning
        {
            for(int k = 0;k < fftPerBuf;k++)
            {
                fftwf_execute(plan_t2f_r2c[thisThread][k]);   // FFT first stage time to frequency, real to complex
                // ADCinFreq[thisThread][k][0][0] = ADCinFreq[thisThread][k][0][0] /2.0;   // DC component correction ? see
                // http://www.fftw.org/fftw3_doc/One_002dDimensional-DFTs-of-Real-Data.html

                if (moderf == VLFMODE)
                {
                    inFreqTmp[thisThread][0][0] =  inFreqTmp[thisThread][0][1] = 0;  // bin[0] = 0;

                for(int m = 1 ; m < halfFft/2; m++) // circular shift tune fs/2 half array
                    {
                        inFreqTmp[thisThread][m][0] = ADCinFreq[thisThread][ m][0] * filterHw[0][m][0]  +
                                                      ADCinFreq[thisThread][ m][1] * filterHw[0][m][1];                                                                      ;
                        inFreqTmp[thisThread][m][1] = ADCinFreq[thisThread][ m][1] * filterHw[0][m][0]  -
                                                      ADCinFreq[thisThread][ m][0] * filterHw[0][m][1];
                    }
                for(int m = 0 ; m < halfFft/2; m++) // circular shift tune fs/2 half array
                    {
                        inFreqTmp[thisThread][halfFft/2+m][0] = 0;
                        inFreqTmp[thisThread][halfFft/2+m][1] = 0;
                    }
                }
                else
                {
                    for(int m = 0 ; m < halfFft/2; m++) // circular shift tune fs/2 half array
                    {
                        inFreqTmp[thisThread][m][0] = ADCinFreq[thisThread][ halfFft/2+m][0] * filterHw[0][m][0]  +
                                                      ADCinFreq[thisThread][ halfFft/2+m][1] * filterHw[0][m][1];                                                                      ;
                        inFreqTmp[thisThread][m][1] = ADCinFreq[thisThread][ halfFft/2+m][1] * filterHw[0][m][0]  -
                                                      ADCinFreq[thisThread][ halfFft/2+m][0] * filterHw[0][m][1];
                    }
                    for(int m = 0 ; m < halfFft/2; m++) // circular shift tune fs/2 half array
                    {
                        inFreqTmp[thisThread][halfFft/2+m][0] = ADCinFreq[thisThread][m][0] * filterHw[0][halfFft/2+m][0]  +
                                                      ADCinFreq[thisThread][m][1]           * filterHw[0][halfFft/2+m][1];

                        inFreqTmp[thisThread][halfFft/2+m][1] = ADCinFreq[thisThread][m][1] * filterHw[0][halfFft/2+m][0]  -
                                                      ADCinFreq[thisThread][m][0]           * filterHw[0][halfFft/2+m][1];

                    }
                }
                fftwf_execute(plan_f2t_c2c[thisThread][decimate]);                  //  c2c + decimation
                float scale = GAINFACTOR;
                float * pTimeTmp;
                if (k == 0)
                { // first frame is 512 sample long
                    pTimeTmp = &outTimeTmp[thisThread][halfFft/4][0];
                    for(int i = 0; i < halfFft/2; i++)
                        {
                            *pout++ = scale *(*pTimeTmp++);
                            *pout++ = scale *(*pTimeTmp++);
                        }
                }
                else
                { // (fftPerBuf-1) frames 768 sample long
                    pTimeTmp = &outTimeTmp[thisThread][0][0];
                    for(int i = 0; i < 3*halfFft/4; i++)
                        {
                            *pout++ = scale *(*pTimeTmp++);
                            *pout++ = scale *(*pTimeTmp++);
                        }
                }
                // 42 * 768 + 512 = 32.768  sample output
            }
        //     DbgPrintf("len %d \n",j);
        }else
        {      // decimate in frequency plus tuning
            int modx = midx / mratio;
            int moff = midx - modx*mratio;
            int offset =((global.transferSize/2)/mratio) *moff;
            pout = (float *)(r2iqCntrl->obuffers[modx]+ offset );
            int mtunebin = r2iqCntrl->getTunebin();

            for(int k = 0;k < fftPerBuf;k++)
            {
              fftwf_execute(plan_t2f_r2c[thisThread][k]);                 // FFT first stage time to frequency, real to complex

                if (moderf == VLFMODE)
                {
                  inFreqTmp[thisThread][0][0] =  inFreqTmp[thisThread][0][1] = 0;  // bin[0] = 0;
                  for(int m = 1 ; m < mfft/2; m++) // circular shift tune fs/2 half array
                    {
                        inFreqTmp[thisThread][m][0] =  ( ADCinFreq[thisThread][m][0] * filterHw[decimate][m][0]  +
                                                         ADCinFreq[thisThread][m][1] * filterHw[decimate][m][1]);
                        inFreqTmp[thisThread][m][1] =  ( ADCinFreq[thisThread][m][1] * filterHw[decimate][m][0]  -
                                                         ADCinFreq[thisThread][m][0] * filterHw[decimate][m][1]);
                    }
                  for(int m = 0 ; m < mfft/2; m++) // circular shift tune fs/2 half array
                    {
                        inFreqTmp[thisThread][mfft/2+m][0] = 0;
                        inFreqTmp[thisThread][mfft/2+m][1] = 0;
                    }
                }
                else
                {
                  for(int m = 0 ; m < mfft/2; m++) // circular shift tune fs/2 half array
                    {
                        inFreqTmp[thisThread][m][0] =  ( ADCinFreq[thisThread][ mtunebin+m][0] * filterHw[decimate][m][0]  +
                                                         ADCinFreq[thisThread][ mtunebin+m][1] * filterHw[decimate][m][1]);
                        inFreqTmp[thisThread][m][1] =  ( ADCinFreq[thisThread][ mtunebin+m][1] * filterHw[decimate][m][0]  -
                                                         ADCinFreq[thisThread][ mtunebin+m][0] * filterHw[decimate][m][1]);
                    }

                  for(int m = 0 ; m < mfft/2; m++) // circular shift tune fs/2 half array
                    {
                        inFreqTmp[thisThread][mfft/2+m][0] = ( ADCinFreq[thisThread][mtunebin - mfft/2 + m][0] * filterHw[decimate][halfFft - mfft/2 + m][0]  +
                                                               ADCinFreq[thisThread][ mtunebin - mfft/2 + m][1] *       filterHw[decimate][halfFft - mfft/2 + m][1]);

                        inFreqTmp[thisThread][mfft/2+m][1] = ( ADCinFreq[thisThread][mtunebin - mfft/2 + m][1] * filterHw[decimate][halfFft - mfft/2 + m][0]  -
                                                               ADCinFreq[thisThread][mtunebin - mfft/2 + m][0]  *       filterHw[decimate][halfFft - mfft/2 + m][1]);
                    }
                }


              fftwf_execute(plan_f2t_c2c[thisThread][decimate]);     //  c2c decimation
              float scale =  GAINFACTOR;
              float * pTimeTmp;
              if (moderf == VHFMODE) // invert spectrum
              {
                if (k ==0)
                {
                    pTimeTmp = &outTimeTmp[thisThread][mfft/4][0];
                    for(int i = 0; i < mfft/2; i++)
                        {
                            *pout++ = scale *(*pTimeTmp++);
                            *pout++ = -scale *(*pTimeTmp++);
                        }
                }
                else
                {
                    pTimeTmp = &outTimeTmp[thisThread][0][0];
                    for(int i = 0; i < 3*mfft/4; i++)
                        {
                            *pout++ = scale *(*pTimeTmp++);
                            *pout++ = -scale *(*pTimeTmp++);
                        }
                }
              }else // normal
              {
                if (k ==0)
                {  // first frame is mfft/2 long
                    pTimeTmp = &outTimeTmp[thisThread][mfft/4][0];
                    for(int i = 0; i < mfft/2; i++)
                        {
                            *pout++ = scale *(*pTimeTmp++);
                            *pout++ = scale *(*pTimeTmp++);
                        }
                }
                else
                {  // other frames are 3*mfft/4 long
                    pTimeTmp = &outTimeTmp[thisThread][0][0];
                    for(int i = 0; i < 3*mfft/4; i++)
                        {
                            *pout++ = scale *(*pTimeTmp++);
                            *pout++ = scale *(*pTimeTmp++);
                        }
                }
              }
            }
        }

// TRACE
#ifndef NDEBUG
//    if(BBRF103.GetTrace())
    if(0)
    {
         BBRF103.UptTrace(false);
        {
            FILE * hfiletrace;
            hfiletrace = fopen( "bufferin.raw", "wb"); // save input real sequence  short
            fwrite((char *)buffer ,global.transferSize,1, hfiletrace); // buffers[idx][]  //65586 sample short
            fclose(hfiletrace);
        }
        {
            FILE * hfiletrace;
            hfiletrace = fopen( "bufferinR.raw", "wb");//  Input derandomized
            UINT16 * p =(UINT16 *) buffer;
            for (int i=0; i <global.transferSize/2;i++)
            {
                p[i]=RandTable[(UINT16)p[i]];
            }
            fwrite((char *)buffer ,global.transferSize,1, hfiletrace); // buffers[idx][] //65586 sample short
            fclose(hfiletrace);
        }
        {
            FILE * hfiletrace;
            hfiletrace = fopen( "ADCinTime.raw", "wb");  //  save input float buffers sequence with overlap (2048 = 1536+512)
            for(int k = 0;k < fftPerBuf;k++)
                  fwrite((float *)ADCinTime[thisThread][k],sizeof(float), 2*1024, hfiletrace); // 2048 real windowed
            fclose(hfiletrace);
        }
        {

            FILE * hfiletrace;
            hfiletrace = fopen( "ADCinFreq.raw", "wb"); //  save input float frequency
            fwrite((float *)ADCinFreq[thisThread],sizeof(float), (1024+1)*2, hfiletrace); // 2048+2 fftwf_r2c output
            fclose(hfiletrace);
        }
        {

            FILE * hfiletrace;
            hfiletrace = fopen( "inFreqTmp.raw", "wb");//  save input float frequency
            fwrite((float *)inFreqTmp[thisThread],sizeof(float), (1024*2), hfiletrace);   //  inFreqTmp after shift
             fclose(hfiletrace);
        }
        {
            FILE * hfiletrace;
             float scale =  GAINFACTOR;
            for(int k=0; k <1024; k++)
            {
                outTimeTmp[thisThread][k][0] *= scale;
                outTimeTmp[thisThread][k][1] *= scale;
            }
            hfiletrace = fopen( "outTimeTmp.raw", "wb");// save out  tmp
            fwrite((float *)outTimeTmp[thisThread],sizeof(float), (1024*2), hfiletrace);  //  outTimeTmp
            fclose(hfiletrace);
        }
        {

            FILE * hfiletrace;
            hfiletrace = fopen( "outTime.raw", "wb");// save output float complex
            int len = 1024*fftPerBuf;
            fwrite((float *)r2iqCntrl->obuffers[r2iqCntrl->bufIdx],sizeof(float), len, hfiletrace);  // 32768 complex 65586 float
            fclose(hfiletrace);
        }

        DbgPrintf("TRACE buffers pictures taken GL ! \n");
        RedrawWindow(h_dialog, NULL, NULL, RDW_INVALIDATE);
    }
#endif
    } // while(global.run)
//    DbgPrintf((char *) "r2iqThreadf idx %d pthread_exit %u\n",(int)th->t, pthread_self());
    pthread_exit(NULL);
    return 0;
}
