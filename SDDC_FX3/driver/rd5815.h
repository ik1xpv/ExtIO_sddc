#pragma once
#include <stdint.h>

#define RD5812_I2C_ADDR 0x18

void RDA5815Initial(uint32_t freq);
void RDA5815Shutdown();
int32_t RDA5815Set(unsigned long fPLL, unsigned long fSym );