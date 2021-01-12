#pragma once

#include "r2iq.h"
#include "fftw3.h"

// use up to this many threads
#define N_MAX_R2IQ_THREADS 4

class fft_mt_r2iq : public r2iqControlClass
{
public:
    fft_mt_r2iq();
    virtual ~fft_mt_r2iq();

    float setFreqOffset(float offset);

    void Init(float gain, int16_t** buffers, float** obuffers);
    void TurnOn();
    void TurnOff(void);
    bool IsOn(void);
    void DataReady(void);

protected:
    template<bool rand, bool aligned> void simd_convert_float(const int16_t *input, float* output, int size);
    template<bool rand, bool aligned> void norm_convert_float(const int16_t *input, float* output, int size);
    template<bool aligned>            void simd_shift_freq(fftwf_complex* dest, const fftwf_complex* source1, const fftwf_complex* source2, int start, int end);
    template<bool aligned>            void norm_shift_freq(fftwf_complex* dest, const fftwf_complex* source1, const fftwf_complex* source2, int start, int end);

private:
    int16_t** buffers;    // pointer to input buffers
    float** obuffers;   // pointer to output buffers
    int bufIdx;         // index to next buffer to be processed
    volatile std::atomic<int> cntr;           // counter of input buffer to be processed

    float GainScale;
    int mfftdim [NDECIDX]; // FFT N dimensions: mfftdim[k] = halfFft / 2^k
    int mtunebin;

    void *r2iqThreadf(r2iqThreadArg *th);   // thread function

    fftwf_complex **filterHw;       // Hw complex to each decimation ratio

	fftwf_plan plan_t2f_r2c;          // fftw plan buffers Freq to Time complex to complex per decimation ratio
	fftwf_plan *plan_f2t_c2c;          // fftw plan buffers Time to Freq real to complex per buffer
	fftwf_plan plans_f2t_c2c[NDECIDX];

    uint32_t processor_count;
    r2iqThreadArg* threadArgs[N_MAX_R2IQ_THREADS];
    std::condition_variable cvADCbufferAvailable;  // unlock when a sample buffer is ready
    std::mutex mutexR2iqControl;                   // r2iq control lock
    std::thread* r2iq_thread[N_MAX_R2IQ_THREADS]; // thread pointers
};
