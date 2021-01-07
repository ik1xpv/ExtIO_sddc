#pragma once

#include "calculator.h"

class converter : public calculator<int16_t, float>
{
public:
    converter(ringbuffer<int16_t> *adc) : calculator(adc), randADC(false)
    {
        // load RAND table for ADC
        for (int k = 0; k < 65536; k++)
        {
            if ((k & 1) == 1)
            {
                RandTable[k] = (k ^ 0xFFFE);
            }
            else
            {
                RandTable[k] = k;
            }
        }
    }

    void setRand(bool val) { this->randADC = val; }
    bool getRand() const { return this->randADC; }

protected:
    void process() override
    {
        auto *src = input->getReadPtr();
        auto *dest = output->getWritePtr();

        if (!randADC)
        {
            for (int i = 0; i < getInputBlockSize(); ++i)
                dest[i] = float(src[i]);
        }
        else
        {
            for (int i = 0; i < getInputBlockSize(); ++i)
                dest[i] = float(RandTable[(uint16_t)src[i]]);
        }

        output->WriteDone();
        input->ReadDone();
    }

private:
    bool randADC;       // randomized ADC output
    int16_t RandTable[65536];  // ADC RANDomize table used to whitening EMI from ADC data bus.
};
