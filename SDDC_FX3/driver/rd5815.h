#pragma once
#include <stdint.h>

#define RD5812_I2C_ADDR 0b0001100

void RDA5815Initial(void);
int32_t RDA5815Set(unsigned long fPLL, unsigned long fSym );