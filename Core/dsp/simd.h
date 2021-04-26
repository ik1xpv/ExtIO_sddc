#pragma once

#include "fftw3.h"
#include <stdint.h>

template <bool rand>
static inline void convert_float(const int16_t *input, float *output, int size)
{
    for (int m = 0; m < size; m++)
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

static inline void shift_freq(fftwf_complex *dest, const fftwf_complex *source1, const fftwf_complex *source2, int start, int end)
{
    for (int m = start; m < end; m++)
    {
        // besides circular shift, do complex multiplication with the lowpass filter's spectrum
        dest[m][0] = source1[m][0] * source2[m][0] - source1[m][1] * source2[m][1];
        dest[m][1] = source1[m][1] * source2[m][0] + source1[m][0] * source2[m][1];
    }
}

template <bool flip>
static inline void copy(fftwf_complex *dest, const fftwf_complex *source, int count)
{
    if (flip)
    {
        for (int i = 0; i < count; i++)
        {
            dest[i][0] = source[i][0];
            dest[i][1] = -source[i][1];
        }
    }
    else
    {
        for (int i = 0; i < count; i++)
        {
            dest[i][0] = source[i][0];
            dest[i][1] = source[i][1];
        }
    }
}