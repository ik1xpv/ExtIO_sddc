#include "RadioHandler.h"

BBRF103Radio::BBRF103Radio(fx3class* fx3)
    : RadioHardware(fx3)
{
    
}

void BBRF103Radio::Initialize()
{

}

void BBRF103Radio::getFrequencyRange(int64_t& low, int64_t& high)
{
    low = 10 * 1000;
    high = 1750 * 1000 * 1000; // 
}

bool BBRF103Radio::UpdatemodeRF(rf_mode mode)
{
    uint32_t data[2];
    int32_t ret;
	data[0] = (UINT32)CORRECT(ADC_FREQ);

    if (mode == VHFMODE)
    {
        // switch to VHF Attenna
        FX3UnsetGPIO(ATT_SEL0 | ATT_SEL1);

        // Enable Tuner reference clock
    	data[1] = (int32_t)CORRECT(R820T_FREQ);
        Fx3->Control(SI5351A, (uint8_t*)&data[0]);

        // Initialize Tuner
        Fx3->Control(R820T2INIT, (uint8_t*)&ret);
        
        return (ret == 0);
    }
    else if (mode == HFMODE || mode == VLFMODE)
    {
        // Stop Tuner
        Fx3->Control(R820T2STDBY, (uint8_t*)&ret);

        // Disable Tuner reference clock
    	data[1] = 0;
        Fx3->Control(SI5351A, (uint8_t*)&data[0]);

        // switch to HF Attenna
        return FX3SetGPIO(ATT_SEL0 | ATT_SEL1);
    }

    return false;
}

bool BBRF103Radio::UpdateattRF(int att)
{
    if (gpios & (ATT_SEL0 | ATT_SEL1))  {
        // this is in HF mode
        if (att > 2) att = 2;
        if (att < 0) att = 0;
        auto attRF = 20 - att * 10;
        switch (attRF)
        {
        case 10: //11
            gpios |= ATT_SEL0 | ATT_SEL1;
            break;
        case 20: //01
            gpios |=  ATT_SEL0;
            gpios &=  ~ATT_SEL1;
            break;
        case 0:   //10
        default:
            gpios |=  ATT_SEL1;
            gpios &=  ~ATT_SEL0;
            break;
        }
        return Fx3->Control(GPIOFX3, (UINT8*)&gpios);
    }
    else {
        int8_t index = att;
        // this is in VHF mode
        return Fx3->Control(R820T2SETATT, (UINT8*) &index);
    }
}

int64_t BBRF103Radio::TuneLo(int64_t freq)
{
   if (gpios & (ATT_SEL0 | ATT_SEL1))  {
        // this is in HF mode
        return ADC_FREQ / 2;
    }
    else {
        // this is in VHF mode
        Fx3->Control(R820T2TUNE, (UINT8*)&freq);
        return freq;
    }
}

