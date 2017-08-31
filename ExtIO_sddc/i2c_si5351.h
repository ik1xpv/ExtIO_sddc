/*
 * i2c_si5351.h
 * Oscar Steila 2017
 */
#pragma once


#include <stdio.h>
#include <math.h> 
#include <wtypes.h>	
#include "ExtIO_sddc.h"   // i2c  Dbgprintf

#define SI_CLK0_CONTROL		16			// Register definitions
#define SI_CLK1_CONTROL		17
#define SI_CLK2_CONTROL		18
#define SI_SYNTH_PLL_A		26
#define SI_SYNTH_PLL_B		34
#define SI_SYNTH_MS_0		42
#define SI_SYNTH_MS_1		50
#define SI_SYNTH_MS_2		58
#define SI_PLL_RESET		(0x177)

#define SI_R_DIV_1		0b00000000		// R-division ratio definitions
#define SI_R_DIV_2		0b00010000
#define SI_R_DIV_4		0b00100000
#define SI_R_DIV_8		0b00110000
#define SI_R_DIV_16		0b01000000
#define SI_R_DIV_32		0b01010000
#define SI_R_DIV_64		0b01100000
#define SI_R_DIV_128	0b01110000

#define SI_CLK_SRC_PLL_A	0b00000000
#define SI_CLK_SRC_PLL_B	0b00100000

#define SI5351_FREQ	27000000			// Crystal frequency
#define SI5351_PLL_FIXED                80000000000ULL

uint8_t si5351aOutputOff(uint8_t clk);
void si5351aSetFrequency(uint32_t frequencyA, uint32_t frequencyB);
uint8_t Si5351_init();

/* Define definitions */

#define SI5351_ADDR        			   (0x60 << 1 )
#define SI5351_XTAL_FREQ                25000000

#define SI5351_CRYSTAL_LOAD             183
#define SI5351_CRYSTAL_LOAD_MASK        (3<<6)
#define SI5351_CRYSTAL_LOAD_0PF         (0<<6)
#define SI5351_CRYSTAL_LOAD_6PF         (1<<6)
#define SI5351_CRYSTAL_LOAD_8PF         (2<<6)
#define SI5351_CRYSTAL_LOAD_10PF        (3<<6)

#define SI5351_PLL_INPUT_SOURCE         15
#define SI5351_CLKIN_DIV_MASK           (3<<6)
#define SI5351_CLKIN_DIV_1              (0<<6)
#define SI5351_CLKIN_DIV_2              (1<<6)
#define SI5351_CLKIN_DIV_4              (2<<6)
#define SI5351_CLKIN_DIV_8              (3<<6)
#define SI5351_PLLB_SOURCE              (1<<3)
#define SI5351_PLLA_SOURCE              (1<<2)

/*
low jtter noise freq

VCO = 864 MHz =  27 MHz * 32 
72 MHz = 864 MHz / 12
f/4 = 18MHz

VCO = 810 MHz =  27 MHz * 30
81 MHz = 810 MHz / 10

*/


