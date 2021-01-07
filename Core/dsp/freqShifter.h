#pragma once

#include "calculator.h"
#include "fir.h"

#include <string.h>
#include <fftw3.h>
#include <algorithm>

class FreqShifter : public calculator<fftwf_complex, fftwf_complex>
{
public:
    FreqShifter(ringbuffer<fftwf_complex> *input)
        : calculator(input),
          gain(1.0f/32768.0f/1000.0f)
    {
        createFilters();
    }

    float getGain() const { return this->gain; }
    void setGain(float val) { this->gain = val; }

protected:
    void process() override
    {
        fftwf_complex* filter = filterHw[m_decimation];
        auto ADCinFreq = input->getReadPtr();
        auto inFreqTmp = output->getWritePtr();

		auto count = std::min(m_fft/2, halfFft - m_tunebin);
		auto source = &ADCinFreq[m_tunebin];

        auto source2 = &ADCinFreq[m_tunebin - m_fft / 2];
        auto dest = &inFreqTmp[m_fft / 2];
        auto start = std::max(0, m_fft / 2 - m_tunebin);
        auto filter2 = &filter[halfFft - m_fft / 2];

        for (int m = 0; m < count; m++) // circular shift tune fs/2 first half array
        {
            inFreqTmp[m][0] = (source[m][0] * filter[m][0] +
                                   source[m][1] * filter[m][1]);

            inFreqTmp[m][1] = (source[m][1] * filter[m][0] -
                                   source[m][0] * filter[m][1]);
        }
        if (m_fft / 2 != count)
            memset(inFreqTmp[count], 0, sizeof(float) * 2 * (m_fft / 2 - count));

        for (int m = start; m < m_fft / 2; m++) // circular shift tune fs/2 second half array
        {
            dest[m][0] = (source2[m][0] * filter2[m][0] +
                          source2[m][1] * filter2[m][1]);

            dest[m][1] = (source2[m][1] * filter2[m][0] -
                          source2[m][0] * filter2[m][1]);
        }
        if (start != 0)
            memset(inFreqTmp[m_fft / 2], 0, sizeof(float) * 2 * start);

        output->WriteDone();
        input->ReadDone();
    }

private:
    void createFilters()
    {
        // filters
		fftwf_complex *pfilterht;       // time filter ht
		pfilterht = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*halfFft);     // halfFft
		filterHw = (fftwf_complex**)fftwf_malloc(sizeof(fftwf_complex*)*NDECIDX);
		for (int d = 0; d < NDECIDX; d++)
		{
			filterHw[d] = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*halfFft);     // halfFft
		}

		for (int t = 0; t < halfFft; t++)
		{
			pfilterht[t][0] = 0.0f;
			pfilterht[t][1] = 0.0f;
		}

		auto filterplan_t2f_c2c = fftwf_plan_dft_1d(halfFft, pfilterht, filterHw[0], FFTW_FORWARD, FFTW_MEASURE);
		float *pht = new float[halfFft / 4 + 1];
		for (int d = 0; d < NDECIDX; d++)
		{
			switch (d)
			{
			case 5:
				KaiserWindow(halfFft / 4 + 1, 80.0f, 0.7f/64.0f, 1.0f/64.0f, pht);
				break;
			case 4:
				KaiserWindow(halfFft / 4 + 1, 90.0f, 1.6f/64.0f, 2.0f/64.0f, pht);
				break;
			case 3:
				KaiserWindow(halfFft / 4 + 1, 100.0f, 3.6f/64.0f, 4.0f/64.0f, pht);
				break;
			case 2:
				KaiserWindow(halfFft / 4 + 1, 110.0f, 7.6f/64.0f, 8.0f/64.0f, pht);
				break;
			case 1:
				KaiserWindow(halfFft / 4 + 1, 120.0f, 15.7f/64.0f, 16.0f/64.0f, pht);
				break;
			case 0:
			default:
				KaiserWindow(halfFft / 4 + 1, 120.0f, 30.5f/64.0f, 32.0f/64.0f, pht);
				break;
			}

			float gainadj = gain  / sqrtf(2.0f) * 2048.0f / (float)FFTN_R_ADC; // reference is FFTN_R_ADC == 2048

			for (int t = 0; t < (halfFft/4+1); t++)
			{
				pfilterht[t][0] = pfilterht[t][1] = gainadj * pht[t];
			}

			fftwf_execute_dft(filterplan_t2f_c2c, pfilterht, filterHw[d]);
		}
		delete[] pht;
		fftwf_destroy_plan(filterplan_t2f_c2c);
		fftwf_free(pfilterht);
    }

    fftwf_complex **filterHw;
    float gain;
};

