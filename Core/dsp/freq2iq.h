#pragma once

#include <vector>
#include <tuple>

#include "config.h"
#include "dspblock.h"
#include "fftw3.h"

#include "pffft/pf_mixer.h"

#define NDECIDX 11

struct ChannelType
{
    bool enabled;
    int decimation;
    int tunebin;
    int sampleperblock;
    bool sideband;

    int decimate_count;
    fftwf_complex* pout;

    // finetune the frequence offset
    float fc;
    shift_limited_unroll_C_sse_data_t stateFineTune;

    ringbuffer<float> *output;
};

class freq2iq : public dsp::dsp_block<fftwf_complex, float>
{
public:
    freq2iq(float gain, int channel_num = 1);
    ~freq2iq();

    bool SetChannel(unsigned channel, int decimate, float offset, bool sideband, int sampleperblock);
    bool GetChannel(unsigned channel, int& decimate, float& offset, bool& sideband);

    ringbuffer<float> *getChannelOutput(unsigned channel);

    void DataProcessor() override;
    void Stop() override;

private:
    void InitFilter(float gain);

private:
    // tuple represent one channel
    // decimate: 0 - DECNIDX, -1 means the channel is not intialized
    // offsetbin: shift the freq domain before iFFT, from -halffft to +halfft
    // sideband: reverse the sign of Q channel
    std::vector<ChannelType> channels;

    int mfftdim[NDECIDX]; // FFT N dimensions: mfftdim[k] = halfFft / 2^k
    int mratio[NDECIDX];

    fftwf_complex **filterHw;       // Hw complex to each decimation ratio
    fftwf_plan plans_f2t_c2c[NDECIDX];
};
