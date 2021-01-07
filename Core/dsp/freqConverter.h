#pragma once

#include "calculator.h"
#include <string.h>
#include <fftw3.h>

class FreqConverter : public calculator<float, fftwf_complex>
{
public:
    FreqConverter(ringbuffer<float> *input)
        : calculator(input)
    {
        auto ADCinTime = new float[FFTN_R_ADC];
        auto ADCinFreq = new fftwf_complex[FFTN_R_ADC + 1];
        plan_t2f_r2c = fftwf_plan_dft_r2c_1d(FFTN_R_ADC, ADCinTime, ADCinFreq, FFTW_MEASURE);
        delete[] ADCinFreq;
        delete[] ADCinTime;

        memset(firstBlock, 0, sizeof(firstBlock));
    }

    void setInputBlockSize(int input_size) override
    {
        InputBlockSize = input_size;

        output->setBlockSize(FFTN_R_ADC + 1);
    }

protected:
    void process() override
    {
        auto src0 = input->getReadPtr();
        int inputsize = input->getBlockSize();
        int offset = 0;

        // first block
        // copy src0
        memcpy(firstBlock + halfFft / 2, src0, 3 * halfFft / 2 * sizeof(float));

        auto dest = output->getWritePtr();
        fftwf_execute_dft_r2c(plan_t2f_r2c, firstBlock, dest);
        output->WriteDone();

        auto src = src0 + halfFft;
        auto count = (inputsize + halfFft / 2) / (halfFft * 2);
        for (int k = 0 ; k < count - 1; k++)
        {
            auto dest = output->getWritePtr();
            fftwf_execute_dft_r2c(plan_t2f_r2c, (float *)src, dest);
            output->WriteDone();
            src += 3 * halfFft / 2;
        }

        memcpy(firstBlock, src, halfFft / 2 * sizeof(float));
        input->ReadDone();
    }

private:
    fftwf_plan plan_t2f_r2c;

    float firstBlock[FFTN_R_ADC];
};

class FreqBackConverter : public calculator<fftwf_complex, fftwf_complex>
{
public:
    FreqBackConverter(ringbuffer<fftwf_complex> *input) : calculator(input)
    {
        auto ADCinTime = new fftwf_complex[FFTN_R_ADC];
        auto ADCinFreq = new fftwf_complex[FFTN_R_ADC + 1];
        int nfft = halfFft;
        for (int i = 0; i < NDECIDX; i++) {
            plans_f2t_c2c[i] = fftwf_plan_dft_1d(nfft, ADCinTime, ADCinFreq, FFTW_BACKWARD, FFTW_MEASURE);
            nfft /= 2;
        }
        delete[] ADCinFreq;
        delete[] ADCinTime;
    }

    ~FreqBackConverter()
    {
        for (int i = 0; i < NDECIDX; i++) {
            fftwf_destroy_plan(plans_f2t_c2c[i]);
        }
    }

    void setInputBlockSize(int input_size) override
    {
        InputBlockSize = input_size;

        output->setBlockSize(FFTN_R_ADC);
    }

protected:
    void process()
    {
        auto src = input->getReadPtr();
        auto dest = output->getWritePtr();
        fftwf_execute_dft(plans_f2t_c2c[m_decimation], (fftwf_complex*)src, dest);
        output->WriteDone();
        input->ReadDone();
    }

private:
    fftwf_plan plans_f2t_c2c[NDECIDX];
};
