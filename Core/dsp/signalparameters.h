#pragma once

#define NDECIDX 10
#include "config.h"

class SignalParameters {
public:
    SignalParameters()
    {
        m_tunebin = halfFft / 4;
        m_fftdim[0] = halfFft;
        for (int i = 1; i < NDECIDX; i++)
        {
            m_fftdim[i] = m_fftdim[i - 1] / 2;
        }

        m_ratiodim[0] = 1;
	    for (int i = 1; i < NDECIDX; i++)
	    {
		    m_ratiodim[i] = m_ratiodim[i - 1] * 2;
	    }

        setDecimate(1);
    }

    virtual ~SignalParameters()
    {
    }

    virtual void setFreqOffset(float offset)
    {
        // align to 1/4 of halfft
        this->m_tunebin = int(offset * halfFft / 4) * 4; // m_tunebin step 4 bin  ?
        float delta = ((float)this->m_tunebin / halfFft) - offset;
        float ret = delta * m_ratio; // ret increases with higher decimation
        DbgPrintf("offset %f m_tunebin %d delta %f (%f)\n", offset, this->m_tunebin, delta, ret);
    }

    virtual void setDecimate(int val)
    {
        m_decimation = val;
        m_fft = m_fftdim[m_decimation];
        m_ratio = m_ratiodim[m_decimation];
    }

    int getDecimate() { return m_decimation; }

protected:
    int m_fft;
    int m_ratio;
    int m_decimation;
    int m_tunebin;

    const int halfFft = FFTN_R_ADC / 2;    // half the size of the first fft at ADC 64Msps real rate (2048)
    const int fftPerBuf = transferSize / sizeof(short) / (3 * halfFft / 2) + 1; // number of ffts per buffer with 256|768 overlap

private:

    int m_ratiodim[NDECIDX];  // ratio
    int m_fftdim[NDECIDX];

    // TODO
    float fc;
};
