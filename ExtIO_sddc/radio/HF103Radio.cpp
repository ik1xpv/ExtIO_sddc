#include "RadioHandler.h"

void HF103Radio::Initialize()
{

}

void HF103Radio::getFrequencyRange(int64_t& low, int64_t& high)
{
    low = 0;
    high = 32 * 1000 * 1000;
}

bool HF103Radio::UpdatemodeRF(rf_mode mode)
{
    if (mode == VHFMODE)
        return false;

	UINT32 data[2];
	data[0] = (UINT32)CORRECT(ADC_FREQ);
	data[1] = 0;

	return Fx3->Control(SI5351A, (UINT8*)&data[0]);
}

bool HF103Radio::UpdateattRF(int att)
{
    if (att > 31) att = 31;
    if (att < 0) att = 0;
    uint8_t d = (31 - att) << 1; // bit0 = 0

    DbgPrintf("UpdateattRF  -%d \n", (31 - att));

    return Fx3->Control(DAT31FX3, &d);
}