#include "RadioHandler.h"

RXLucyRadio::RXLucyRadio(fx3class *fx3) 
    : RadioHardware(fx3)
{
    for (uint8_t i = 0; i < step_size; i++) 
    {
            this->steps[step_size - i - 1] = -1.0f * i;
    }
}

void RXLucyRadio::Initialize(uint32_t adc_rate)
{
    uint32_t data = adc_rate;
    Fx3->Control(STARTADC, data);
}

void RXLucyRadio::getFrequencyRange(int64_t &low, int64_t &high)
{
    low = 10 * 1000;
    high = 6000ll * 1000 * 1000; //
}

bool RXLucyRadio::UpdateattRF(int att) // VHF attenuator
{ 
    if (att > step_size - 1) att = step_size - 1;
    if (att < 0) att = 0;
    uint8_t d = step_size - att - 1;

    DbgPrintf("UpdateattRF %f \n", this->steps[att]);
    return Fx3->SetArgument(VHF_ATTENUATOR, d);
}

uint64_t RXLucyRadio::TuneLo(uint64_t freq)
{
    if (!(gpios & VHF_EN))
    {
        // this is in HF mode
        return 0;
    }
    else
    {
        uint64_t if_freq;
        if_freq = adcnominalfreq / 4;  // offset IF frequency

        Fx3->Control(AD4351TUNE, freq + if_freq); // set pll

        return freq - if_freq;
    }

}
bool RXLucyRadio::UpdatemodeRF(rf_mode mode)
{
    if (mode == VHFMODE)
    {
        // switch to VHF Attenna
        FX3SetGPIO(VHF_EN);

        // Initialize VCO

        // Initialize Mixer

        return true;
    }
    else if (mode == HFMODE)
    {
        return FX3UnsetGPIO(VHF_EN);                // switch to HF Attenna
    }
    return false;
}

int RXLucyRadio::getRFSteps(const float **steps)
{
    *steps = this->steps;
    return step_size;
}




