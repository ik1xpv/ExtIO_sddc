#include "RadioHandler.h"

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
    }
}

uint64_t RadioHardware::GetSerialNumber()
{
    uint64_t value;
    Fx3->ReadSerialNumber(&value);

    return value;
}