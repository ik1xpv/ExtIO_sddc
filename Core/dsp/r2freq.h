#pragma once

#include "dspblock.h"
#include "fftw3.h"

/// This is a class to convert the int16 samples into a freq domain samples
///
///
class r2freq : public dsp::dsp_block<int16_t, fftwf_complex> {
public:
    r2freq();
    ~r2freq();

    void setRand(bool value) { this->randADC = value; }
    bool getRand() const { return this->randADC; }

    virtual void DataProcessor() override;

private:
    bool randADC;
    float *ADCinTime;
    fftwf_plan plan_t2f_r2c;

#if PRINT_INPUT_RANGE
	int MinMaxBlockCount;
	int16_t MinValue;
	int16_t MaxValue;
#endif
};