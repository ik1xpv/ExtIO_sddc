#pragma once

#include "r2iq.h"
#include "fftw3.h"
#include "config.h"
#include <algorithm>
#include <string.h>

#include "dsp/simd.h"

#define PRINT_INPUT_RANGE  0

static const int halfFft = FFTN_R_ADC / 2;    // half the size of the first fft at ADC 64Msps real rate (2048)
static const int fftPerBuf = transferSize / sizeof(short) / (3 * halfFft / 2) + 1; // number of ffts per buffer with 256|768 overlap

class fft_mt_r2iq : public r2iqControlClass
{
public:
    fft_mt_r2iq();
    virtual ~fft_mt_r2iq();

    float setFreqOffset(float offset);

    void Init(float gain, ringbuffer<float>* obuffers);
    void TurnOn(ringbuffer<int16_t>& buffers);
    void TurnOff(void);
    bool IsOn(void);

private:
    ringbuffer<int16_t>* inputbuffer;    // pointer to input buffers
    ringbuffer<float>* outputbuffer;    // pointer to ouput buffers
    int bufIdx;         // index to next buffer to be processed

    float GainScale;
    int mfftdim [NDECIDX]; // FFT N dimensions: mfftdim[k] = halfFft / 2^k
    int mtunebin;

    void *r2iqThreadf();   // thread function
    void *receiverf();

    void *r2iqThreadf_def();
    void *r2iqThreadf_avx();
    void *r2iqThreadf_avx2();
    void *r2iqThreadf_avx512();

    fftwf_complex **filterHw;       // Hw complex to each decimation ratio

	fftwf_plan plan_t2f_r2c;          // fftw plan buffers Freq to Time complex to complex per decimation ratio

    std::thread r2iq_thread; // thread pointers
    std::thread freq2time_thread;

	float *ADCinTime;                // point to each threads input buffers [nftt][n]
    ringbuffer<fftwf_complex> freqdomain;
#if PRINT_INPUT_RANGE
	int MinMaxBlockCount;
	int16_t MinValue;
	int16_t MaxValue;
#endif
};