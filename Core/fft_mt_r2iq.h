#pragma once

#include "r2iq.h"
#include "fftw3.h"
#include "config.h"
#include <algorithm>
#include <string.h>

// use up to this many threads
#define N_MAX_R2IQ_THREADS 1
#define PRINT_INPUT_RANGE  0

static const int halfFft = FFTN_R_ADC / 2;    // half the size of the first fft at ADC 64Msps real rate (2048)
static const int fftPerBuf = transferSize / sizeof(short) / (3 * halfFft / 2) + 1; // number of ffts per buffer with 256|768 overlap

class fft_mt_r2iq : public r2iqControlClass
{
public:
    fft_mt_r2iq();
    virtual ~fft_mt_r2iq();

    float setFreqOffset(float offset);

    void Init(float gain, ringbuffer<int16_t>* buffers, ringbuffer<float>* obuffers);
    void TurnOn();
    void TurnOff(void);
    bool IsOn(void);

protected:

    template<bool rand> void convert_float(const int16_t *input, float* output, int size)
    {
        if (rand)
        {
            for(int m = 0; m < size; m++)
            {
                int16_t val;
                if (rand && (input[m] & 1))
                {
                    val = input[m] ^ (-2);
                }
                else
                {
                    val = input[m];
                }
                output[m] = float(val);
            }
        }
        else
        {
            int blkCnt = size / 4;
            for(int m = 0; m < blkCnt; m++)
            {
                *output++ = float(*input++);
                *output++ = float(*input++);
                *output++ = float(*input++);
                *output++ = float(*input++);
            }

            /* If the blockSize is not a multiple of 4, compute any remaining output samples here.
            ** No loop unrolling is used. */
            blkCnt = size % 0x4u;

            for(int m = 0; m < blkCnt; m++)
            {
                *output++ = float(*input++);
            }
        }
    }

    void shift_freq(fftwf_complex* dest, const fftwf_complex* source1, const fftwf_complex* source2, int start, int end)
    {
        fftwf_complex* d = &dest[start];
        const fftwf_complex* s1 = &source1[start];
        const fftwf_complex* s2 = &source2[start];

        int count = end - start;

        int blkCnt = count / 4;
        for (int m = 0; m < blkCnt; m++)
        {
            (*d)[0] = (*s1)[0] * (*s2)[0] - (*s1)[1] * (*s2)[1];
            (*d)[1] = (*s1)[1] * (*s2)[0] + (*s1)[0] * (*s2)[1];
            d++; s1++; s2++;

            (*d)[0] = (*s1)[0] * (*s2)[0] - (*s1)[1] * (*s2)[1];
            (*d)[1] = (*s1)[1] * (*s2)[0] + (*s1)[0] * (*s2)[1];
            d++; s1++; s2++;

            (*d)[0] = (*s1)[0] * (*s2)[0] - (*s1)[1] * (*s2)[1];
            (*d)[1] = (*s1)[1] * (*s2)[0] + (*s1)[0] * (*s2)[1];
            d++; s1++; s2++;

            (*d)[0] = (*s1)[0] * (*s2)[0] - (*s1)[1] * (*s2)[1];
            (*d)[1] = (*s1)[1] * (*s2)[0] + (*s1)[0] * (*s2)[1];
            d++; s1++; s2++;
        }

        blkCnt = count % 0x4u;
        for (int m = 0; m < blkCnt; m++)
        {
            (*d)[0] = (*s1)[0] * (*s2)[0] - (*s1)[1] * (*s2)[1];
            (*d)[1] = (*s1)[1] * (*s2)[0] + (*s1)[0] * (*s2)[1];
            d++; s1++; s2++;
        }
    }

    template<bool flip> void copy(fftwf_complex* dest, const fftwf_complex* source, int count)
    {
        if (flip)
        {
            int blkCnt = count / 4;
            for (int i = 0; i < count; i++)
            {
                (*dest)[0] = (*source)[0];
                (*dest++)[1] = -(*source++)[1];

                (*dest)[0] = (*source)[0];
                (*dest++)[1] = -(*source++)[1];

                (*dest)[0] = (*source)[0];
                (*dest++)[1] = -(*source++)[1];

                (*dest)[0] = (*source)[0];
                (*dest++)[1] = -(*source++)[1];
            }

            /* If the blockSize is not a multiple of 4, compute any remaining output samples here.
            ** No loop unrolling is used. */
            blkCnt = count % 0x4u;

            for (int i = 0; i < blkCnt; i++)
            {
                (*dest)[0] = (*source)[0];
                (*dest++)[1] = -(*source++)[1];
            }
        }
        else
        {
            memcpy(dest, source, sizeof(fftwf_complex) * count);
        }
    }

private:
    ringbuffer<int16_t>* inputbuffer;    // pointer to input buffers
    ringbuffer<float>* outputbuffer;    // pointer to ouput buffers
    int bufIdx;         // index to next buffer to be processed
    r2iqThreadArg* lastThread;

    float GainScale;
    int mfftdim [NDECIDX]; // FFT N dimensions: mfftdim[k] = halfFft / 2^k
    int mtunebin;

    void *r2iqThreadf(r2iqThreadArg *th);   // thread function

    void * r2iqThreadf_def(r2iqThreadArg *th);
    void * r2iqThreadf_avx(r2iqThreadArg *th);
    void * r2iqThreadf_avx2(r2iqThreadArg *th);
    void * r2iqThreadf_avx512(r2iqThreadArg *th);
    void * r2iqThreadf_neon(r2iqThreadArg *th);

    fftwf_complex **filterHw;       // Hw complex to each decimation ratio

	fftwf_plan plan_t2f_r2c;          // fftw plan buffers Freq to Time complex to complex per decimation ratio
	fftwf_plan *plan_f2t_c2c;          // fftw plan buffers Time to Freq real to complex per buffer
	fftwf_plan plans_f2t_c2c[NDECIDX];

    uint32_t processor_count;
    r2iqThreadArg* threadArgs[N_MAX_R2IQ_THREADS];
    std::mutex mutexR2iqControl;                   // r2iq control lock
    std::thread r2iq_thread[N_MAX_R2IQ_THREADS]; // thread pointers
};

// assure, that ADC is not oversteered?
struct r2iqThreadArg {

	r2iqThreadArg()
	{
#if PRINT_INPUT_RANGE
		MinMaxBlockCount = 0;
		MinValue = 0;
		MaxValue = 0;
#endif
	}

	float *ADCinTime;                // point to each threads input buffers [nftt][n]
	fftwf_complex *ADCinFreq;         // buffers in frequency
	fftwf_complex *inFreqTmp;         // tmp decimation output buffers (after tune shift)
#if PRINT_INPUT_RANGE
	int MinMaxBlockCount;
	int16_t MinValue;
	int16_t MaxValue;
#endif
};
