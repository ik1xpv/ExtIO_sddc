// MIT License Copyright (c) 2017  ik1xpv@gmail.com, http://www.steila.com/blog

#ifndef SI5351_H
#define SI5351_H
#include "license.txt" // Oscar Steila ik1xpv
#include <basetsd.h>

#define SI_CLK0_CONTROL		16			// Registers
#define SI_CLK1_CONTROL		17
#define SI_CLK2_CONTROL		18
#define SI_SYNTH_PLL_A		26
#define SI_SYNTH_PLL_B		34
#define SI_SYNTH_MS_0		42
#define SI_SYNTH_MS_1		50
#define SI_SYNTH_MS_2		58
#define SI_PLL_RESET		(0x177)

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


void Si5351init();
void si5351aSetFrequency(UINT32 freq, UINT32 freq2);
void si5351aOutputOff(UINT8 clk);
void setupMultisynth( UINT8 synth, UINT32 divider, UINT8 rDiv);
void setupPLL(UINT8 pll, UINT8 mult, UINT32 num, UINT32 denom);


extern UINT32 pippo;

#endif
