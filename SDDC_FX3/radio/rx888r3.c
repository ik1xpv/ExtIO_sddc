#include "Application.h"
#include "radio.h"

#define GPIO_ATT_LE 17
#define GPIO_BIAS_VHF 18
#define GPIO_BIAS_HF 19
#define GPIO_RANDO 20
#define GPIO_LED_RED 21
#define GPIO_PGA 24
#define GPIO_ATT_DATA  26
#define GPIO_ATT_CLK   27
#define GPIO_SHDWN 28
#define GPIO_DITH 29
#define GPIO_VHF_EN 35
#define GPIO_VGA_LE 38

#define GPIO_DIY_0 22
#define GPIO_DIY_1 23
#define GPIO_DIY_2 34

#define GPIO_PRESEL_0 50
#define GPIO_PRESEL_1 54
#define GPIO_PRESEL_2 55
#define GPIO_FM_NOTCH 39

void rx888r3_GpioSet(uint32_t mdata)
{
    CyU3PGpioSetValue (GPIO_SHDWN, (mdata & SHDWN) == SHDWN ); 		 // SHDN
    CyU3PGpioSetValue (GPIO_DITH, (mdata & DITH ) == DITH  ); 		 // DITH
    CyU3PGpioSetValue (GPIO_RANDO, (mdata & RANDO) == RANDO ); 		 // RAND
    CyU3PGpioSetValue (GPIO_BIAS_HF, (mdata & BIAS_HF) == BIAS_HF);
    CyU3PGpioSetValue (GPIO_BIAS_VHF, (mdata & BIAS_VHF) == BIAS_VHF);
	CyU3PGpioSetValue (GPIO_LED_RED, (mdata & LED_RED) == LED_RED);
	CyU3PGpioSetValue (GPIO_PGA, (mdata & PGA_EN ) != PGA_EN  ); 		 // PGA_EN
    CyU3PGpioSetValue (GPIO_VHF_EN, (mdata & VHF_EN) == VHF_EN ); // VHF_EN
}

void rx888r3_GpioInitialize()
{
    ConfGPIOsimpleout (GPIO_SHDWN);
    ConfGPIOsimpleout (GPIO_DITH);
    ConfGPIOsimpleout (GPIO_RANDO);
    ConfGPIOsimpleout (GPIO_BIAS_HF);
    ConfGPIOsimpleout (GPIO_BIAS_VHF);
	ConfGPIOsimpleout (GPIO_LED_RED);
    ConfGPIOsimpleout (GPIO_VHF_EN);

	ConfGPIOsimpleout (GPIO_PGA);
    ConfGPIOsimpleout (GPIO_ATT_LE);
    ConfGPIOsimpleout (GPIO_ATT_DATA);
    ConfGPIOsimpleout (GPIO_ATT_CLK);
    ConfGPIOsimpleout (GPIO_VGA_LE);

    rx888r3_GpioSet(LED_RED | LED_YELLOW | LED_BLUE);

    CyU3PGpioSetValue (GPIO_ATT_LE, 0);  // ATT_LE latched
    CyU3PGpioSetValue (GPIO_ATT_CLK, 0);  // test version
    CyU3PGpioSetValue (GPIO_ATT_DATA, 0);  // ATT_DATA
    CyU3PGpioSetValue (GPIO_VGA_LE, 1);

	CyU3PGpioSetValue (GPIO_PGA, 1); // PGA =1 , 1.5v range

	// preselector
	ConfGPIOsimpleout (GPIO_PRESEL_0);
	ConfGPIOsimpleout (GPIO_PRESEL_1);
	ConfGPIOsimpleout (GPIO_PRESEL_2);
	ConfGPIOsimpleout (GPIO_FM_NOTCH);

	rx888r3_preselect (0b011); // default 64M

	// diy gpio
	ConfGPIOsimpleout (GPIO_DIY_0);
	ConfGPIOsimpleout (GPIO_DIY_1);
	ConfGPIOsimpleout (GPIO_DIY_2);

	CyU3PGpioSetValue (GPIO_DIY_0, 0);
	CyU3PGpioSetValue (GPIO_DIY_1, 0);
	CyU3PGpioSetValue (GPIO_DIY_2, 0);
}

/*
    PE4304
	direct GPIO output implementation  64 step 0.5 dB
*/
void rx888r3_SetAttenuator(uint8_t value)
{
	uint8_t bits = 6;
	const uint8_t mask = 0x20;
	uint8_t i,b;
	CyU3PGpioSetValue (GPIO_ATT_LE, 0);    // ATT_LE latched
	CyU3PGpioSetValue (GPIO_ATT_CLK, 0);   // ATT_CLK
	for( i = bits ; i >0; i--)
	{
		b = (value & mask) != 0;
		CyU3PGpioSetValue (GPIO_ATT_DATA, b); // ATT_DATA
		CyU3PGpioSetValue (GPIO_ATT_CLK, 1);
		value = value << 1;
		CyU3PGpioSetValue (GPIO_ATT_CLK, 0);
	}
	CyU3PGpioSetValue (GPIO_ATT_LE, 1);    // ATT_LE latched
	CyU3PGpioSetValue (GPIO_ATT_LE, 0);    // ATT_LE latched
}

/*
    AD8370
	direct GPIO output implementation 128 step 0.5 dB
*/
void rx888r3_SetGain(uint8_t value)
{
	uint8_t bits = 8;
	const uint8_t mask = 0x80;
	uint8_t i,b;
	CyU3PGpioSetValue (GPIO_VGA_LE, 0);    // ATT_LE latched
	CyU3PGpioSetValue (GPIO_ATT_CLK, 0);   // ATT_CLK
	for( i = bits ; i >0; i--)
	{
		b = (value & mask) != 0;
		CyU3PGpioSetValue (GPIO_ATT_DATA, b); // ATT_DATA
		CyU3PGpioSetValue (GPIO_ATT_CLK, 1);
		value = value << 1;
		CyU3PGpioSetValue (GPIO_ATT_CLK, 0);
	}
	CyU3PGpioSetValue (GPIO_VGA_LE, 1);    // ATT_LE latched
	CyU3PGpioSetValue (GPIO_ATT_DATA, 0); // ATT_DATA
}

int rx888r3_preselect(uint32_t data)
{
	CyU3PGpioSetValue (GPIO_PRESEL_0, ((data >> 0) & 0x01));
	CyU3PGpioSetValue (GPIO_PRESEL_1, ((data >> 1) & 0x01));
	CyU3PGpioSetValue (GPIO_PRESEL_2, ((data >> 2) & 0x01));
	CyU3PGpioSetValue (GPIO_FM_NOTCH, !((data >> 3) & 0x01));

	return 0;
}