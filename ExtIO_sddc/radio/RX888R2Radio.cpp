#include "RadioHandler.h"

#define R828D_FREQ (16000000) // R820T reference frequency

#define HIGH_MODE 0x80
#define LOW_MODE 0x00

#define MODE HIGH_MODE

const float RX888R2Radio::vhf_rf_steps[RX888R2Radio::vhf_rf_step_size] = {
    0.0f, 0.9f, 1.4f, 2.7f, 3.7f, 7.7f, 8.7f, 12.5f, 14.4f, 15.7f,
    16.6f, 19.7f, 20.7f, 22.9f, 25.4f, 28.0f, 29.7f, 32.8f,
    33.8f, 36.4f, 37.2f, 38.6f, 40.2f, 42.1f, 43.4f, 43.9f,
    44.5f, 48.0f, 49.6f};

const float RX888R2Radio::vhf_if_steps[RX888R2Radio::vhf_if_step_size] = {
    -4.7f, -2.1f, 0.5f, 3.5f, 7.7f, 11.2f, 13.6f, 14.9f, 16.3f, 19.5f, 23.1f, 26.5f, 30.0f, 33.7f, 37.2f, 40.8f};

RX888R2Radio::RX888R2Radio(fx3class *fx3)
    : RadioHardware(fx3)
{
    for (uint8_t i = 0; i < hf_rf_step_size; i++)
    {
        this->hf_rf_steps[hf_rf_step_size - i - 1] = -(
            ((i & 0x01) != 0) * 0.5f +
            ((i & 0x02) != 0) * 1.0f +
            ((i & 0x04) != 0) * 2.0f +
            ((i & 0x08) != 0) * 4.0f +
            ((i & 0x016) != 0) * 8.0f +
            ((i & 0x032) != 0) * 16.0f);
    }

    // high mode gain = 0.409, start=-30
    // low mode gain = 0.059, start = -30
#if (MODE == HIGH_MODE)
    float ratio = 0.409f;
#else
    float ratio = 0.059f;
#endif
    for (uint8_t i = 0; i < hf_if_step_size; i++)
    {
        this->hf_if_steps[i] = -30.0f + ratio * i;
    }
}

void RX888R2Radio::Initialize()
{
    uint32_t data = (UINT32)CORRECT(ADC_FREQ);
    Fx3->Control(STARTADC, data);
}

void RX888R2Radio::getFrequencyRange(int64_t &low, int64_t &high)
{
    low = 10 * 1000;
    high = 1750 * 1000 * 1000; //
}

bool RX888R2Radio::UpdatemodeRF(rf_mode mode)
{
    if (mode == VHFMODE)
    {
        // switch to VHF Attenna
        FX3SetGPIO(VHF_EN);

        // high gain, 0db
        uint8_t gain = 75 | 0x80;
        Fx3->SetArgument(AD8340_VGA, gain);
        // Enable Tuner reference clock
        uint32_t ref = R828D_FREQ;
        return Fx3->Control(R82XXINIT, ref); // Initialize Tuner
    }
    else if (mode == HFMODE)
    {
        Fx3->Control(R82XXSTDBY); // Stop Tuner

        return FX3UnsetGPIO(VHF_EN);                // switch to HF Attenna
    }

    return false;
}

bool RX888R2Radio::UpdateattRF(int att)
{
    int att0 = att;
    if (!(gpios & VHF_EN))
    {
        // hf mode
        if (att > hf_rf_step_size - 1)
            att = hf_rf_step_size - 1;
        if (att < 0)
            att = 0;
        uint8_t d = hf_rf_step_size - att - 1;

        DbgPrintf("UpdateattRF %f \n", this->hf_rf_steps[att]);

        return Fx3->SetArgument(DAT31_ATT, d);
    }
    else
    {
        uint16_t index = att;
        // this is in VHF mode
        return Fx3->SetArgument(R82XX_ATTENUATOR, index);
    }
}

uint64_t RX888R2Radio::TuneLo(uint64_t freq)
{
    if (!(gpios & VHF_EN))
    {
        // this is in HF mode
        return ADC_FREQ / 2;
    }
    else
    {
        // this is in VHF mode
        Fx3->Control(R82XXTUNE, freq);
        return freq;
    }
}

int RX888R2Radio::getRFSteps(const float **steps)
{
    if (!(gpios & VHF_EN))
    {
        // hf mode
        *steps = this->hf_rf_steps;
        return hf_rf_step_size;
    }
    else
    {
        *steps = this->vhf_rf_steps;
        return vhf_rf_step_size;
    }
}

int RX888R2Radio::getIFSteps(const float **steps)
{
    if (!(gpios & VHF_EN))
    {
        *steps = this->hf_if_steps;
        return hf_if_step_size;
    }
    else
    {
        *steps = this->vhf_if_steps;
        return vhf_if_step_size;
    }
}

bool RX888R2Radio::UpdateGainIF(int gain_index)
{
    if (!(gpios & VHF_EN))
    {
        // this is in HF mode
        uint8_t gain = MODE | gain_index;

        DbgPrintf("UpdateGainIF %d \n", gain);

        return Fx3->SetArgument(AD8340_VGA, gain);
    }
    else
    {
        // this is in VHF mode
        return Fx3->SetArgument(R82XX_VGA, (uint16_t)gain_index);
    }
}