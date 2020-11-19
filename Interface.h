#pragma once

// HF103 commands !!!
enum FX3Command {
	STARTFX3 = 0xAA,
	STOPFX3 = 0xAB,
	TESTFX3 = 0xAC,
	GPIOFX3 = 0xAD,
	I2CWFX3 = 0xAE,
	I2CRFX3 = 0xAF,
	DAT31FX3 = 0xB0,
	RESETFX3 = 0xB1,
	SI5351A = 0xB2,
	SI5351ATUNE = 0xB3,
	R820T2INIT = 0xB4,
	R820T2TUNE = 0xB5,
	R820T2SETATT = 0xB6,
	R820T2GETATT = 0xB7,
	R820T2STDBY = 0xB8
};

#define OUTXIO0 (1U << 0) 	// ATT_LE
#define OUTXIO1 (1U << 1) 	// ATT_CLK
#define OUTXIO2 (1U << 2) 	// ATT_DATA
#define OUTXIO3 (1U << 3)  	// SEL0
#define OUTXIO4 (1U << 4) 	// SEL1
#define OUTXIO5 (1U << 5)  	// SHDWN
#define OUTXIO6 (1U << 6)  	// DITH
#define OUTXIO7 (1U << 7)  	// RAND

#define OUTXIO8 (1U << 8) 	// 256
#define OUTXIO9 (1U << 9) 	// 512
#define OUTXI10 (1U << 10) 	// 1024
#define OUTXI11 (1U << 11)  	// 2048
#define OUTXI12 (1U << 12) 	// 4096
#define OUTXI13 (1U << 13)  	// 8192
#define OUTXI14 (1U << 14)  	// 16384
#define OUTXI15 (1U << 15)  	// 32768

enum GPIOPin {
    SHDWN = OUTXIO5,
    DITH = OUTXIO6,
    RANDO = OUTXIO7,
    BIAS_HF = OUTXIO8,
    BIAS_VHF = OUTXIO9,
    LED_YELLOW = OUTXI10,
    LED_RED = OUTXI11,
    LED_BLUE = OUTXI12,
    ATT_SEL0 = OUTXI13,
    ATT_SEL1 = OUTXI14,
    VHF_EN = OUTXI15,
};

enum RadioModel {
    NORADIO = 0x00,
    BBRF103 = 0x01,
    HF103 = 0x02,
    RX888 = 0x03,
};