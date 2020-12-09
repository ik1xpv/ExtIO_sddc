#ifndef R2IQ_H
#define R2IQ_H
#include "license.txt" 
#define N_R2IQ_THREAD 64

#define NDECIDX 7  //number of srate
#include "LC_ExtIO_Types.h"
#include "fftw3.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

struct r2iqThreadArg;

class r2iqControlClass {
public:
    r2iqControlClass();
    ~r2iqControlClass();

    int getRatio()  {return mratio [mdecimation];}

    float setFreqOffset(float offset);
    void updateRand(bool v) {this->randADC = v; }

    void Init(float gain, uint8_t** buffers, float** obuffers);
    void TurnOn();
    void TurnOff(void);
    bool IsOn(void);
    void DataReady(void);
    void setDecimate(int dec) { mdecimation = dec; }

private:
    bool r2iqOn;        // r2iq on flag
    uint8_t** buffers;    // pointer to input buffers
    float** obuffers;   // pointer to output buffers
    int bufIdx;         // index to next buffer to be processed
    volatile std::atomic<int> cntr;           // counter of input buffer to be processed
    bool randADC;       // randomized ADC output
    r2iqThreadArg* lastThread;

    float GainScale;
    int mdecimation ;   // selected decimation ratio
                        // 0 => 32Msps, 1=> 16Msps, 2 = 8Msps, 3 = 4Msps, 5 = 2Msps
    int mfftdim [NDECIDX]; // FFT N dimensions
    int mratio [NDECIDX];  // ratio
    int mtunebin;

    int16_t RandTable[65536];  // ADC RANDomize table used to whitening EMI from ADC data bus.

    void *r2iqThreadf(r2iqThreadArg *th);   // thread function

    int halfFft;    // half the size of the first fft at ADC 64Msps real rate (2048)
    int fftPerBuf; // number of ffts per buffer with 256|768 overlap
    fftwf_complex *pfilterht;       // time filter ht
    fftwf_complex **filterHw;       // Hw complex to each decimation ratio
#ifdef _DEBUG
    fftwf_plan *filterplan_f2t_c2c; // frequency to time fft used for debug
    fftwf_complex **filterHt;       // Ht time vector used for debug
#endif

    uint32_t processor_count;
    r2iqThreadArg* threadArgs[N_R2IQ_THREAD];
    std::condition_variable cvADCbufferAvailable;  // unlock when a sample buffer is ready
    std::mutex mutexR2iqControl;                   // r2iq control lock
    std::thread* r2iq_thread[N_R2IQ_THREAD]; // thread pointers

protected:
    int getDecimate() {return mdecimation;}
    int getFftN()   {return mfftdim[mdecimation];}
};

extern class r2iqControlClass r2iqCntrl;

#endif
