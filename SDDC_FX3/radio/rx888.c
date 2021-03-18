#include "Application.h"
#include "radio.h"

#define GPIO_BIAS_VHF 18
#define GPIO_BIAS_HF 19
#define GPIO_RANDO 20
#define GPIO_LED_RED 21
#define GPIO_LED_YELLOW 22
#define GPIO_LED_BLUE 23
#define GPIO_ATT_SEL0 26
#define GPIO_ATT_SEL1 27
#define GPIO_SHDWN 28
#define GPIO_DITH 29

void rx888_GpioSet(uint32_t mdata)
{
    CyU3PGpioSetValue (GPIO_ATT_SEL0, (mdata & ATT_SEL0) == ATT_SEL0 ); // ATT_SEL0
    CyU3PGpioSetValue (GPIO_ATT_SEL1, (mdata & ATT_SEL1) == ATT_SEL1 ); // ATT_SEL1
    CyU3PGpioSetValue (GPIO_SHDWN, (mdata & SHDWN) == SHDWN ); 		 // SHDN
    CyU3PGpioSetValue (GPIO_DITH, (mdata & DITH ) == DITH  ); 		 // DITH
    CyU3PGpioSetValue (GPIO_RANDO, (mdata & RANDO) == RANDO ); 		 // RAND
    CyU3PGpioSetValue (GPIO_BIAS_HF, (mdata & BIAS_HF) == BIAS_HF);
    CyU3PGpioSetValue (GPIO_BIAS_VHF, (mdata & BIAS_VHF) == BIAS_VHF);
    CyU3PGpioSetValue (GPIO_LED_RED, (mdata & LED_RED) == LED_RED);
    CyU3PGpioSetValue (GPIO_LED_YELLOW, (mdata & LED_YELLOW) == LED_YELLOW);
    CyU3PGpioSetValue (GPIO_LED_BLUE, (mdata & LED_BLUE) == LED_BLUE);
}

void rx888_GpioInitialize()
{
    ConfGPIOsimpleout (GPIO_ATT_SEL0);
    ConfGPIOsimpleout (GPIO_ATT_SEL1);
    ConfGPIOsimpleout (GPIO_SHDWN);
    ConfGPIOsimpleout (GPIO_DITH);
    ConfGPIOsimpleout (GPIO_RANDO);
    ConfGPIOsimpleout (GPIO_BIAS_HF);
    ConfGPIOsimpleout (GPIO_BIAS_VHF);
    ConfGPIOsimpleout (GPIO_LED_RED);
    ConfGPIOsimpleout (GPIO_LED_YELLOW);
    ConfGPIOsimpleout (GPIO_LED_BLUE);

    rx888_GpioSet(LED_RED | LED_YELLOW | LED_BLUE);
}
