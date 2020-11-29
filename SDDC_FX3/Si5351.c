/*
 * Si5351.c
 *
 *  Created on: 28/ott/2020
 *      Author: ik1xp
 */

#include "Application.h"

#define SI_CLK0_CONTROL		16			// Registers
#define SI_CLK1_CONTROL		17
#define SI_CLK2_CONTROL		18
#define SI_SYNTH_PLL_A		26
#define SI_SYNTH_PLL_B		34
#define SI_SYNTH_MS_0		42
#define SI_SYNTH_MS_1		50
#define SI_SYNTH_MS_2		58
#define SI_PLL_RESET		177

#define SI_R_DIV_1		(0)     //  0b00000000		/
#define SI_R_DIV_2		(0x10)  //  0b00010000
#define SI_R_DIV_4		(0x20)  //  0b00100000
#define SI_R_DIV_8		(0x30)  //  0b00110000
#define SI_R_DIV_16		(0x40)  //  0b01000000
#define SI_R_DIV_32		(0x50)  //  0b01010000
#define SI_R_DIV_64		(0x60)  //  0b01100000
#define SI_R_DIV_128	(0x70)  //  0b01110000

#define SI_CLK_SRC_PLL_A	0b00000000
#define SI_CLK_SRC_PLL_B	0b00100000

#define SI5351_FREQ	27000000			// Crystal frequency
#define SI5351_PLL_FIXED                80000000000ULL

#define SI5351_ADDR        			    (0xC0) // (0x60 << 1 )

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

CyU3PReturnStatus_t Si5351init()
{
	CyU3PReturnStatus_t status;
	status = I2cTransferW1 ( SI5351_CRYSTAL_LOAD , SI5351_ADDR, 0x52);
	if (status != CY_U3P_SUCCESS)
		return status;

	status = I2cTransferW1 (     SI_CLK0_CONTROL , SI5351_ADDR, 0x80); // clocks off
	if (status != CY_U3P_SUCCESS)
		return status;

	status = I2cTransferW1 (     SI_CLK1_CONTROL , SI5351_ADDR, 0x80); // clocks off
	if (status != CY_U3P_SUCCESS)
		return status;

	status = I2cTransferW1 (     SI_CLK2_CONTROL , SI5351_ADDR, 0x80); // clocks off
	if (status != CY_U3P_SUCCESS)
		return status;

	return status;
}


//
// Set up specified PLL with mult, num and denom
// mult is 15..90
// num is 0..1,048,575 (0xFFFFF)
// denom is 0..1,048,575 (0xFFFFF)
//
void setupPLL(UINT8 pll, UINT8 mult, UINT32 num, UINT32 denom)
{
	UINT32 P1;					// PLL config register P1
	UINT32 P2;					// PLL config register P2
	UINT32 P3;					// PLL config register P3
	UINT8 data[8];

	// the actual multiplier is  mult + num / denom

	P1 = (UINT32)(128 * ((float)num / (float)denom));
	P1 = (UINT32)(128 * (UINT32)(mult)+P1 - 512);
	P2 = (UINT32)(128 * ((float)num / (float)denom));
	P2 = (UINT32)(128 * num - denom * P2);
	P3 = denom;

	data[0] = (P3 & 0x0000FF00) >> 8;
	data[1] = (P3 & 0x000000FF);
	data[2] = (P1 & 0x00030000) >> 16;
	data[3] = (P1 & 0x0000FF00) >> 8;
	data[4] = (P1 & 0x000000FF);
	data[5] = ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16);
	data[6] = (P2 & 0x0000FF00) >> 8;
	data[7] = (P2 & 0x000000FF);

	I2cTransfer ( pll , SI5351_ADDR, sizeof(data), data, false);
}

//
// Set up MultiSynth with integer divider and R divider
// R divider is the bit value which is OR'ed onto the appropriate register, it is a #define in si5351a.h
//
void setupMultisynth(UINT8 synth, UINT32 divider, UINT8 rDiv)
{
	UINT32 P1;	// Synth config register P1
	UINT32 P2;	// Synth config register P2
	UINT32 P3;	// Synth config register P3
	UINT8 data[8];


	P1 = 128 * divider - 512;
	P2 = 0;							// P2 = 0, P3 = 1 forces an integer value for the divider
	P3 = 1;

	data[0] = (P3 & 0x0000FF00) >> 8;
	data[1] = (P3 & 0x000000FF);
	data[2] = ((P1 & 0x00030000) >> 16) | rDiv;
	data[3] = (P1 & 0x0000FF00) >> 8;
	data[4] = (P1 & 0x000000FF);
	data[5] = ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16);
	data[6] = (P2 & 0x0000FF00) >> 8;
	data[7] = (P2 & 0x000000FF);

	I2cTransfer ( synth , SI5351_ADDR, sizeof(data), data, false);
}

void si5351aSetFrequencyA(UINT32 freq)
{
	UINT32 frequency;
	UINT32 pllFreq;
	UINT32 xtalFreq = SI5351_FREQ;
	UINT32 l;
	double f;
	UINT8 mult;
	UINT32 num;
	UINT32 denom;
	UINT32 divider;
	UINT32 rdiv;

	if (freq == 0)
	{
		I2cTransferW1 ( SI_CLK0_CONTROL, SI5351_ADDR, 0x80); // clk1 off
		return;
	}

	rdiv = (UINT32)SI_R_DIV_1;

	frequency = freq;

	while (frequency < 1000000)
	{
		frequency = frequency * 2;
		rdiv += SI_R_DIV_2;
	}
#ifdef _PLLDEBUG_
	DbgPrintf((char *) "\nCLK0 frequency %d \n", frequency);
#endif
	divider = 900000000UL / frequency;// Calculate the division ratio. 900,000,000 is the maximum internal
									// PLL frequency: 900MHz
	if (divider % 2) divider--;		// Ensure an even integer division ratio

	pllFreq = divider * frequency;	// Calculate the pllFrequency: the divider * desired output frequency
#ifdef _PLLDEBUG_
	DbgPrintf((char *) "pllA Freq  %d \n", pllFreq);
#endif
	mult = pllFreq / xtalFreq;		// Determine the multiplier to get to the required pllFrequency
	l = pllFreq % xtalFreq;			// It has three parts:
	f = (double)l;					// mult is an integer that must be in the range 15..90
	f *= 1048575;					// num and denom are the fractional parts, the numerator and denominator
	f /= xtalFreq;					// each is 20 bits (range 0..1048575)
	num = (UINT32)f;				// the actual multiplier is  mult + num / denom
	denom = 1048575;				// For simplicity we set the denominator to the maximum 1048575
									// Set up PLL A with the calculated multiplication ratio
	setupPLL(SI_SYNTH_PLL_A, mult, num, denom);
	// Set up MultiSynth divider 0, with the calculated divider.
	// The final R division stage can divide by a power of two, from 1..128.
	// represented by constants SI_R_DIV1 to SI_R_DIV128 (see si5351a.h header file)
	// If you want to output frequencies below 1MHz, you have to use the
	// final R division stage
	setupMultisynth(SI_SYNTH_MS_0, divider, rdiv);
	// Reset the PLL. This causes a glitch in the output. For small changes to
	// the parameters, you don't need to reset the PLL, and there is no glitch
	I2cTransferW1 (SI_PLL_RESET , SI5351_ADDR, 0x20);//pllA
	// Finally switch on the CLK0 output (0x4F)
	// and set the MultiSynth0 input to be PLL A
	I2cTransferW1 (SI_CLK0_CONTROL, SI5351_ADDR,  0x4F | SI_CLK_SRC_PLL_A);

}

void si5351aSetFrequencyB(UINT32 freq2)
{
	UINT32 frequency;
	UINT32 pllFreq;
	UINT32 xtalFreq = SI5351_FREQ;
	UINT32 l;
	double f;
	UINT8 mult;
	UINT32 num;
	UINT32 denom;
	UINT32 divider;
	UINT32 rdiv;

	if (freq2 == 0)
	{
		I2cTransferW1 ( SI_CLK2_CONTROL, SI5351_ADDR, 0x80); // clk2 off
		return;
	}

	// calculate clk2
	frequency = freq2 ;
	rdiv = SI_R_DIV_1;
	xtalFreq = SI5351_FREQ;
	while (frequency <= 1000000)
	{
		frequency = frequency * 2;
		rdiv += SI_R_DIV_2;
	}
#ifdef _PLLDEBUG_
	DbgPrintf((char *) "\nCLK2 frequency %d \n", frequency);
#endif
	divider = 900000000UL / frequency;// Calculate the division ratio. 900,000,000 is the maximum internal
										// PLL frequency: 900MHz
	if (divider % 2) divider--;		// Ensure an even integer division ratio

	pllFreq = divider * frequency;	// Calculate the pllFrequency: the divider * desired output frequency
#ifdef _PLLDEBUG_
	DbgPrintf((char *) "pllB Freq  %d \n", pllFreq);
#endif
	mult = pllFreq / xtalFreq;		// Determine the multiplier to get to the required pllFrequency
	l = pllFreq % xtalFreq;			// It has three parts:
	f = (double)l;							// mult is an integer that must be in the range 15..90
	f *= 1048575;					// num and denom are the fractional parts, the numerator and denominator
	f /= xtalFreq;					// each is 20 bits (range 0..1048575)
	num = (UINT32)f;				// the actual multiplier is  mult + num / denom
	denom = 1048575;				// For simplicity we set the denominator to the maximum 1048575

									// Set up PLL B with the calculated multiplication ratio
	setupPLL(SI_SYNTH_PLL_B, mult, num, denom);
	// Set up MultiSynth divider 0, with the calculated divider.
	// The final R division stage can divide by a power of two, from 1..128.
	// represented by constants SI_R_DIV1 to SI_R_DIV128 (see si5351a.h header file)
	// If you want to output frequencies below 1MHz, you have to use the
	// final R division stage

	setupMultisynth(SI_SYNTH_MS_2, divider, rdiv);
	// Reset the PLL. This causes a glitch in the output. For small changes to
	// the parameters, you don't need to reset the PLL, and there is no glitch
	I2cTransferW1 ( SI_PLL_RESET, SI5351_ADDR, 0x80) ; //pllB
	// Finally switch on the CLK2 output (0x4C)
	// and set the MultiSynth0 input to be PLL A
	I2cTransferW1 ( SI_CLK2_CONTROL, SI5351_ADDR,  0x4C | SI_CLK_SRC_PLL_B);  // select PLLB

}
