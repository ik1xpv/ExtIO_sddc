// Oscar Steila 2017
#pragma once

#include <stdio.h>
#include <math.h> 
#include <wtypes.h>	
#include "ExtIO_sddc.h"   // i2c Dbgprintf

#define R820T2_ADDRESS		 ((0x1A)<<1)			// 0011010+RW - 0x34 (w), 0x35 (r)
#define IFR820T				7000000



extern uint32_t r_freq;
extern uint32_t r_xtal;
extern uint32_t r_ifreq;
extern const int r820t2_lna_gain_steps[16];
extern const int r820t2_mixer_gain_steps[16];
extern const int r820t2_vga_gain_steps[16];

void R820T2_init(void);
void R820T2_set_stdby(void);
void R820T2_i2c_write_reg(uint8_t reg, uint8_t data);
void R820T2_i2c_read_raw(uint8_t *data, uint8_t sz);
uint8_t R820T2_i2c_read_reg_uncached(uint8_t reg);
uint8_t R820T2_i2c_read_reg_cached(uint8_t reg);
void R820T2_set_freq(uint32_t freq);
void R820T2_set_lna_gain(uint8_t gain_index);
void R820T2_set_mixer_gain(uint8_t gain_index);
void R820T2_set_vga_gain(uint8_t gain_index);
void R820T2_set_lna_agc(uint8_t value);
void R820T2_set_mixer_agc(uint8_t value);
void R820T2_set_if_bandwidth(uint8_t bw);


