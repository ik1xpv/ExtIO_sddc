#include "RadioHandler.h"

#define ADC_FREQ (128u*1000*1000)
#define IF_FREQ (ADC_FREQ / 4)

#define HIGH_MODE 0x80
#define LOW_MODE 0x00

#define GAIN_SWEET_POINT 18
#define HIGH_GAIN_RATIO (0.409f)
#define LOW_GAIN_RATIO (0.059f)

RX999Radio::RX999Radio(fx3class *fx3)
    : RadioHardware(fx3)
{
    for (uint8_t i = 0; i < rf_step_size; i++)
    {
        this->rf_steps[rf_step_size - i - 1] = -(
            ((i & 0x01) != 0) * 0.5f +
            ((i & 0x02) != 0) * 1.0f +
            ((i & 0x04) != 0) * 2.0f +
            ((i & 0x08) != 0) * 4.0f +
            ((i & 0x010) != 0) * 8.0f +
            ((i & 0x020) != 0) * 16.0f);
    }

    for (uint8_t i = 0; i < if_step_size; i++)
    {
        if (i > GAIN_SWEET_POINT)
            this->if_steps[i] = 20.0f * log10f(HIGH_GAIN_RATIO * (i - GAIN_SWEET_POINT + 3));
        else
            this->if_steps[i] = 20.0f * log10f(LOW_GAIN_RATIO * (i + 1));
    }
}

void RX999Radio::Initialize(uint32_t adc_rate)
{
    uint32_t data = adc_rate;
    Fx3->Control(STARTADC, data);
}

void RX999Radio::getFrequencyRange(int64_t &low, int64_t &high)
{
    low = 10 * 1000;
    high = 6000ll * 1000 * 1000; //
}

bool RX999Radio::UpdatemodeRF(rf_mode mode)
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


bool RX999Radio::UpdateattRF(int att)
{
    // hf mode
    if (att > rf_step_size - 1)
        att = rf_step_size - 1;
    if (att < 0)
        att = 0;
    uint8_t d = rf_step_size - att - 1;

    DbgPrintf("UpdateattRF %f \n", this->rf_steps[att]);

    return Fx3->SetArgument(DAT31_ATT, d);
}

uint64_t RX999Radio::TuneLo(uint64_t freq)
{
    if (!(gpios & VHF_EN))
    {
        // this is in HF mode
        return 0;
    }
    else
    {
        int sel;
        // set preselector
        if (freq <= 120*1000*1000) sel = 0b111;
        else if (freq <= 250*1000*1000) sel = 0b101;
        else if (freq <= 300*1000*1000) sel = 0b110;
        else if (freq <= 380*1000*1000) sel = 0b100;
        else if (freq <= 500*1000*1000) sel = 0b000;
        else if (freq <= 1000ll*1000*1000) sel = 0b010;
        else if (freq <= 2000ll*1000*1000) sel = 0b001;
        else sel = 0b011;

        Fx3->Control(AD4351TUNE, freq + IF_FREQ);

        Fx3->SetArgument(PRESELECTOR, sel);
        // Set VCXO
        return freq - IF_FREQ;
    }
}

int RX999Radio::getRFSteps(const float **steps)
{
    *steps = this->rf_steps;
    return rf_step_size;
}

int RX999Radio::getIFSteps(const float **steps)
{
    *steps = this->if_steps;
    return if_step_size;
}

bool RX999Radio::UpdateGainIF(int gain_index)
{
    // this is in HF mode
    uint8_t gain;
    if (gain_index > GAIN_SWEET_POINT)
        gain = HIGH_MODE | (gain_index - GAIN_SWEET_POINT + 3);
    else
        gain = LOW_MODE | (gain_index + 1);

    DbgPrintf("UpdateGainIF %d \n", gain);

    return Fx3->SetArgument(AD8340_VGA, gain);
}
