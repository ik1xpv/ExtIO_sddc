
/*
 *  StartUp.c - set up the CPU environment then start the RTOS
 *  Created on: 21/set/2020
 *
 *  HF103_FX3 project
 *  Author: Oscar Steila
 *  modified from: SuperSpeed Device Design By Example - John Hyde
 *
 *  https://sdr-prototypes.blogspot.com/
 */

#include "Application.h"

#define UART_CTS	(54)

void IndicateError(uint16_t ErrorCode)
{
	return; // not used
/*
	// Setup a PWM to blink the DVK's only user LED at an "error rate"
    CyU3PGpioComplexConfig_t gpioConfig;
	// LED is on UART_CTS which is currently been assigned to the UART driver, claim it back
    CyU3PDeviceGpioOverride(UART_CTS, CyFalse);
    // Configure UART_CTS as PWM output
    CyU3PMemSet((uint8_t *)&gpioConfig, 0, sizeof(gpioConfig));
	gpioConfig.outValue = CyTrue;
    gpioConfig.driveLowEn = CyTrue;
    gpioConfig.driveHighEn = CyTrue;
    gpioConfig.pinMode = (ErrorCode == 0) ? CY_U3P_GPIO_MODE_STATIC : CY_U3P_GPIO_MODE_PWM;
    gpioConfig.timerMode = CY_U3P_GPIO_TIMER_HIGH_FREQ;
    gpioConfig.period = PWM_PERIOD << ErrorCode;
    gpioConfig.threshold = PWM_THRESHOLD << ErrorCode;
    CyU3PGpioSetComplexConfig(UART_CTS, &gpioConfig);
    // Last ditch effort to tell the user, it may not get through
    if (ErrorCode) DebugPrint(1, "\r\nFatal Error = %d", ErrorCode);
*/
}



// Main sets up the CPU environment then starts the RTOS
int main (void)
{
    CyU3PIoMatrixConfig_t ioConfig;
    CyU3PReturnStatus_t Status;
    CyU3PSysClockConfig_t clkCfg;

    // The default clock runs at 384MHz, adjust it up to 403MHz so that GPIF can be "100MHz"
	clkCfg.setSysClk400 = CyTrue;   /* FX3 device's master clock is set to a frequency > 400 MHz */
	clkCfg.cpuClkDiv = 2;           /* CPU clock divider */
	clkCfg.dmaClkDiv = 2;           /* DMA clock divider */
	clkCfg.mmioClkDiv = 2;          /* MMIO clock divider */
	clkCfg.useStandbyClk = CyFalse; /* device has no 32KHz clock supplied */
	clkCfg.clkSrc = CY_U3P_SYS_CLK; /* Clock source for a peripheral block  */

	Status = CyU3PDeviceInit(&clkCfg); // Start with the clock at 403 MHz
	if (Status == CY_U3P_SUCCESS)
    {
		Status = CyU3PDeviceCacheControl(CyTrue, CyTrue, CyTrue);
		if (Status == CY_U3P_SUCCESS)
		{
			/* Configure the IO matrix for the device. On the FX3 DVK board, the COM port
			 * is connected to the IO(53:56). This means that either DQ32 mode should be
			 * selected or lppMode should be set to UART_ONLY. Here we are choosing
			 * UART_ONLY configuration for 16 bit slave FIFO configuration and setting
			 * isDQ32Bit for 32-bit slave FIFO configuration. */
			ioConfig.useUart   = CyTrue;
			ioConfig.useI2C    = CyTrue;
			ioConfig.useI2S    = CyFalse;
			ioConfig.useSpi    = CyFalse;
			ioConfig.isDQ32Bit = CyFalse; //CY_FX_SLFIFO_GPIF_16_32BIT_CONF_SELECT == 0 -> 16 bit
			ioConfig.lppMode   = CY_U3P_IO_MATRIX_LPP_UART_ONLY;
			/* No GPIOs are enabled. */
			ioConfig.gpioSimpleEn[0]  = 0;
			ioConfig.gpioSimpleEn[1]  = 0;
			ioConfig.gpioComplexEn[0] = 0x00000000;  //
			ioConfig.gpioComplexEn[1] = 0x00000000;  // 0

		//	if (CyU3PDeviceConfigureIOMatrix (&ioConfig) == CY_U3P_SUCCESS)   CyU3PKernelEntry ();
			Status = CyU3PDeviceConfigureIOMatrix(&ioConfig);
			if (Status == CY_U3P_SUCCESS)
			{
				CyU3PKernelEntry();			// Start RTOS, this does not return
			}
		}
	}
    while (1);			// Get here on a failure, can't recover, just hang here
}
