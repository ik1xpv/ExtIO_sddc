#include "Application.h"

#include "adf4351.h"
#include "pcal6408a.h"

//#define LUCY-BBRF103  // define if use BBRF103 to debug-emulate RXLUCy to be removed... remember to toggle RAND checkbutton
#ifdef LUCY-BBRF103
	#define GPIO_ATT_SEL0 26 
	#define GPIO_ATT_SEL1 27 
#endif


/*
 RX Lucy hardware - Wiktor Starzak 2020/21
 Oscar Steila 2020/21
*/

// #define GPIO_RANDO 		nu //  defined in hardware RXLUCY 
#define GPIO_SHDWN		28 // RXLUCY
#define GPIO_DITH		29 // RXLUCY
#define GPIO_4351_CLK  	22 // ADF4351 SV1 pin 9   RXLUCY 
#define GPIO_4351_DATA 	23 // ADF4351 SV1 pin 10  RXLUCY 
#define GPIO_4351_LE   	34 // ADF4351 SV1 pin 8   RXLUCY 
#define GPIO_6408_INT  	35 // PLL_LD  SV1 pin 6   RXLUCY 

extern adf4350_init_param adf4351_init_params;
extern uint32_t registers[6];
extern int32_t adf4350_write(uint32_t data);


void rxlucy_GpioSet(uint32_t mdata)
{
#ifdef LUCY-BBRF103 
    CyU3PGpioSetValue (GPIO_ATT_SEL0, 1 ); // ATT_SEL0
    CyU3PGpioSetValue (GPIO_ATT_SEL1, 1 ); // ATT_SEL1
#endif   
    CyU3PGpioSetValue (GPIO_SHDWN, (mdata & SHDWN) == SHDWN ); 		 // SHDN
    CyU3PGpioSetValue (GPIO_DITH, (mdata & DITH ) == DITH  ); 		 // DITH
//   CyU3PGpioSetValue (GPIO_RANDO, (mdata & RANDO) == RANDO ); 	 // RAND 

 
// VHF_EN 
	if ((mdata & VHF_EN) == VHF_EN)
	{
	    // Initialize adf4351
	    adf4350_setup(0, 0, adf4351_init_params);
	}
}

void rxlucy_GpioInitialize()
{
#ifdef LUCY-BBRF103
    ConfGPIOsimpleout (GPIO_ATT_SEL0);
    ConfGPIOsimpleout (GPIO_ATT_SEL1);
#endif   
	adf4351_init_params.clkin = 50000000;	// <- is it correct ??	
	ConfGPIOsimpleout (GPIO_SHDWN);
	ConfGPIOsimpleout (GPIO_DITH);
//	ConfGPIOsimpleout (GPIO_RANDO); // not used
	ConfGPIOsimpleinput (GPIO_6408_INT);
	ConfGPIOsimpleout (GPIO_4351_LE);
    ConfGPIOsimpleout (GPIO_4351_CLK);   
	ConfGPIOsimpleout (GPIO_4351_DATA);	
	
	PCAL6408A_Init();
}

void rxlucy_SetAttenuator(uint8_t value)
{
	// not used in RXLUCY  was used in HF103 ??
}

void rxlucy_VHFAttenuator(uint8_t value)
{
  	PCAL6408A_write(value);
}

