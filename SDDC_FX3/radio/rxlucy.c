#include "Application.h"

#include "adf4351.h"
#include "pcal6408a.h"

/*
 RX Lucy hardware - Wiktor Starzak 2020/21

 FRONT END PROTOTYPE PCB 
1. GND   - GND
2. SCL   - GPIO_4351_CLK 
3. MOSI	 - GPIO_ATT_DATA	
4. CS    - GPIO_4351_LE
5. +3V3  - pull up 3v3 GPIO_6408_INT
6. +5V   - 5Volt
----
7. I2SCL
8. I2SDA
*/
//                        

#define GPIO_RANDO 		29 // HF103
#define GPIO_SHDWN 		18 // HF103
#define GPIO_DITH 		17 // HF103

#define GPIO_ATT_DATA          23 // HF103 // GPIO_4351_DATA
#define GPIO_ATT_CLK           22 // HF103 // GPIO_4351_CLK 
#define GPIO_ATT_LE 	        21 // HF103 

#define GPIO_4351_CLK  	22 // ADF4351 pin 1
#define GPIO_4351_DATA 	23 // ADF4351 pin 2
#define GPIO_4351_LE   	34 // ADF4351 pin 3
#define GPIO_6408_INT  	35 // PCAL6408  pin 3

extern uint32_t registers[6];
extern int32_t adf4350_write(uint32_t data);


void rxlucy_GpioSet(uint32_t mdata)
{
    CyU3PGpioSetValue (GPIO_SHDWN, (mdata & SHDWN) == SHDWN ); 		 // SHDN
    CyU3PGpioSetValue (GPIO_DITH, (mdata & DITH ) == DITH  ); 		 // DITH
    CyU3PGpioSetValue (GPIO_RANDO, (mdata & RANDO) == RANDO ); 		 // RAND
}

void rxlucy_GpioInitialize()
{
	adf4351_init_params.clkin = 50000000;
	ConfGPIOsimpleout (GPIO_SHDWN);
	ConfGPIOsimpleout (GPIO_DITH);
	ConfGPIOsimpleout (GPIO_RANDO);

 	ConfGPIOsimpleout (GPIO_ATT_LE);
	ConfGPIOsimpleout (GPIO_ATT_DATA);
	ConfGPIOsimpleout (GPIO_ATT_CLK);
	ConfGPIOsimpleinput (GPIO_6408_INT);

	// Set all nominated pins to outputs

	ConfGPIOsimpleout (GPIO_4351_LE);
	// ConfGPIOsimpleout (GPIO_4351_CLK);   // already done in GPIO_ATT_CLK
	// ConfGPIOsimpleout (GPIO_4351_DATA);	// already done in GPIO_ATT_DATA

    	CyU3PGpioSetValue (GPIO_ATT_LE, 0);    // ATT_LE latched
    	CyU3PGpioSetValue (GPIO_ATT_CLK, 0);   // test version
    	CyU3PGpioSetValue (GPIO_ATT_DATA, 0);  // ATT_DATA
    	
    	PCAL6408A_Init();
}

/*
	clone from HF103 (used for test of front end with HF103)
*/
void rxlucy_SetAttenuator(uint8_t value)
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

void rxlucy_VHFAttenuator(uint8_t value)
{
  	PCAL6408A_write(value);
}

