#include "Application.h"
#include "radio.h"

#define LED_OVERLOAD  (21)

#define GPIO_ATT_LE    (21)
#define GPIO_ATT_CLK   (22)
#define GPIO_ATT_DATA  (23)
#define GPIO_LED_KIT   (54)

#define GPIO_RANDO 29
#define GPIO_SHDWN 18
#define GPIO_DITH 17

void hf103_GpioSet(uint32_t mdata)
{
    CyU3PGpioSetValue (GPIO_SHDWN, (mdata & SHDWN) == SHDWN ); 		 // SHDN
    CyU3PGpioSetValue (GPIO_DITH, (mdata & DITH ) == DITH  ); 		 // DITH
    CyU3PGpioSetValue (GPIO_RANDO, (mdata & RANDO) == RANDO ); 		 // RAND
}

void hf103_GpioInitialize()
{
    ConfGPIOsimpleout (GPIO_SHDWN);
    ConfGPIOsimpleout (GPIO_DITH);
    ConfGPIOsimpleout (GPIO_RANDO);
    ConfGPIOsimpleout (GPIO_LED_KIT);
    ConfGPIOsimpleout (GPIO_ATT_LE);
    ConfGPIOsimpleout (GPIO_ATT_CLK);
    ConfGPIOsimpleout (GPIO_ATT_DATA);

    hf103_GpioSet(0);

    CyU3PGpioSetValue (GPIO_LED_KIT, 0);
    CyU3PGpioSetValue (GPIO_ATT_LE, 0);  // ATT_LE latched
    CyU3PGpioSetValue (GPIO_ATT_CLK, 1);  // test version
    CyU3PGpioSetValue (GPIO_ATT_DATA, 1);  // ATT_DATA
}

/*
 	HF103
    DAT-31-SP+  control
	direct GPIO output implementation  31 step 1 dB
*/
void hf103_SetAttenuator(uint8_t value)
{
	uint8_t bits = 6;
	uint8_t mask = 0x20;
	uint8_t i,b;
	CyU3PGpioSetValue (GPIO_ATT_LE, 0);    // ATT_LE latched
	CyU3PGpioSetValue (GPIO_ATT_CLK, 0);   // ATT_CLK
	for( i = bits ; i >0; i--)
	{
		b = (value & mask) >> 5;
		CyU3PGpioSetValue (GPIO_ATT_DATA, b); // ATT_DATA
		CyU3PGpioSetValue (GPIO_ATT_CLK, 1);
		value = value << 1;
		CyU3PGpioSetValue (GPIO_ATT_CLK, 0);
	}
	CyU3PGpioSetValue (GPIO_ATT_LE, 1);    // ATT_LE latched
	CyU3PGpioSetValue (GPIO_ATT_LE, 0);    // ATT_LE latched
}

