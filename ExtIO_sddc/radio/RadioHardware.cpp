#include "RadioHandler.h"

#define DEFAULT_ADC_FREQ  (64000000)	// ADC sampling frequency

bool RadioHardware::FX3SetGPIO(uint32_t mask)
{
    gpios |= mask;

    return Fx3->Control(GPIOFX3, gpios);
}

bool RadioHardware::FX3UnsetGPIO(uint32_t mask)
{
    gpios &= ~mask;

    return Fx3->Control(GPIOFX3, gpios);
}

RadioHardware::~RadioHardware()
{
    if (Fx3) {
        FX3SetGPIO(SHDWN);

        Fx3->Control(RESETFX3);

        delete Fx3;
    }
}

uint32_t RadioHardware::getSampleRate()
{
    return (uint32_t)CORRECT(DEFAULT_ADC_FREQ);
}