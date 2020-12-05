/*
 * RunApplication.c
 *
 *  Created on: 21/set/2020
 *
 *  HF103_FX3 project
 *  Author: Oscar Steila
 *  modified from: SuperSpeed Device Design By Example - John Hyde
 *
 *  https://sdr-prototypes.blogspot.com/
 */
#include "Application.h"

#include "tuner_r82xx.h"

// Declare external functions
extern void CheckStatus(char* StringPtr, CyU3PReturnStatus_t Status);
extern void CheckStatusSilent(char* StringPtr, CyU3PReturnStatus_t Status);
extern CyU3PReturnStatus_t InitializeDebugConsole(void);
extern void IndicateError(uint16_t ErrorCode);
extern CyU3PReturnStatus_t InitializeUSB(void);
extern void ParseCommand(void);

extern CyU3PReturnStatus_t Si5351init();

extern void si5351aSetFrequencyA(UINT32 freq);
extern void si5351aSetFrequencyB(UINT32 freq2);

// Declare external data
extern const char* EventName[];
extern uint32_t glDMACount;

// Global data owned by this module
CyBool_t glIsApplnActive = CyFalse;     // Set true once device is enumerated
CyU3PQueue EventAvailable;			  	// Used for thread communications
uint32_t EventAvailableQueue[16] __attribute__ ((aligned (32)));// Queue for up to 16 uint32_t pointers
uint32_t Qevent __attribute__ ((aligned (32)));
CyU3PThread ThreadHandle[APP_THREADS];		// Handles to my Application Threads
void *StackPtr[APP_THREADS];				// Stack allocated to each thread

uint8_t HWconfig = NORADIO;       // Hardware config type BBRF103
uint16_t FWconfig = 0x0102;    // Firmware rc1 ver 1.02

CyU3PReturnStatus_t
ConfGPIOsimpleout( uint8_t gpioid)
{
	 CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
	 CyU3PGpioSimpleConfig_t gpioConfig;

	  apiRetStatus = CyU3PDeviceGpioOverride (gpioid, CyTrue);
	  CheckStatusSilent("CyU3PDeviceGpioOverride", apiRetStatus);
	    /* Configure GPIO gpioid as output */
	      gpioConfig.outValue = CyFalse;
	      gpioConfig.driveLowEn = CyTrue;
	      gpioConfig.driveHighEn = CyTrue;
	      gpioConfig.inputEn = CyFalse;
	      gpioConfig.intrMode = CY_U3P_GPIO_NO_INTR;
	      apiRetStatus = CyU3PGpioSetSimpleConfig(gpioid , &gpioConfig);
	      CheckStatusSilent("CyU3PGpioSetSimpleConfig", apiRetStatus);

	 return apiRetStatus;
}

CyU3PReturnStatus_t
ConfGPIOsimpleinput( uint8_t gpioid)
{
	 CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
	 CyU3PGpioSimpleConfig_t gpioConfig;

	  apiRetStatus = CyU3PDeviceGpioOverride (gpioid, CyTrue);
	  CheckStatusSilent("CyU3PDeviceGpioOverride", apiRetStatus);
	    /* Configure GPIO gpioid as output */
	      gpioConfig.outValue = CyFalse;
	      gpioConfig.driveLowEn = CyFalse;
	      gpioConfig.driveHighEn = CyFalse;
	      gpioConfig.inputEn = CyTrue;
	      gpioConfig.intrMode = CY_U3P_GPIO_NO_INTR;
	      apiRetStatus = CyU3PGpioSetSimpleConfig(gpioid , &gpioConfig);
	      CheckStatusSilent("CyU3PGpioSetSimpleConfig", apiRetStatus);
	      /* Adding internal pull-up resistor to GPIO 53 */
	      CyU3PGpioSetIoMode(gpioid, CY_U3P_GPIO_IO_MODE_NONE);
	 return apiRetStatus;
}

// tentative to recognize LED and pull up at pin gpioid
CyBool_t GPIOtestInputPulldown( uint8_t gpioid)
{
	CyU3PGpioSimpleConfig_t gpioConfig;
	CyBool_t measure;
	CyU3PDeviceGpioOverride (gpioid, CyTrue);
	/* Configure GPIO gpioid as output */
	gpioConfig.outValue = CyFalse;
	gpioConfig.driveLowEn = CyTrue;
	gpioConfig.driveHighEn = CyTrue;
	gpioConfig.inputEn = CyFalse;
	gpioConfig.intrMode = CY_U3P_GPIO_NO_INTR;
	CyU3PGpioSetSimpleConfig(gpioid , &gpioConfig); // GPIO is output
	CyU3PGpioSetValue (gpioid, 1); // up to discharge led capacitance
	gpioConfig.driveLowEn = CyFalse;
	gpioConfig.driveHighEn = CyFalse;
	gpioConfig.inputEn = CyTrue;
	CyU3PGpioSetSimpleConfig(gpioid , &gpioConfig); // GPIO is output
	/* Adding internal pull-down resistor to GPIO */
	CyU3PGpioSetIoMode(gpioid, CY_U3P_GPIO_IO_MODE_WPD); // activate pull down
	CyU3PGpioSimpleGetValue ( gpioid, &measure); //measure 1 if external pull up or LED to vdd
	CyU3PGpioSetIoMode(gpioid, CY_U3P_GPIO_IO_MODE_NONE);
	return measure;
}



void
GpioInitClock()
{
	CyU3PReturnStatus_t Status;
    CyU3PGpioClock_t gpioClock;

    /* Init the GPIO module */
    gpioClock.fastClkDiv = 2;
    gpioClock.slowClkDiv = 0;
    gpioClock.simpleDiv = CY_U3P_GPIO_SIMPLE_DIV_BY_2;
    gpioClock.clkSrc = CY_U3P_SYS_CLK;
    gpioClock.halfDiv = 0;
    Status = CyU3PGpioInit(&gpioClock,   NULL);
    CheckStatus("CyU3PGpioInit", Status);
}



void MsgParsing(uint32_t qevent)
{
	uint8_t label;
	label = qevent >> 24; //  label4bit|data24bit
	switch (label)
	{
		case 0:
			DebugPrint(4, "\r\nEvent received = %s   ", EventName[(uint8_t)qevent]);
			break;
		case 1:
			DebugPrint(4, "\r\nVendor request = %x  %x  %x\r\n", (uint8_t)( qevent >> 16), (uint8_t) (qevent >> 8) , (uint8_t) qevent );
			break;
		case 2:
			DebugPrint(4, "\r\nfree \r\n", (uint8_t) qevent );
			break;
		case USER_COMMAND_AVAILABLE:
			ParseCommand();
			break;
	}
}

void ApplicationThread ( uint32_t input)
{
	// input is passed to this routine from CyU3PThreadCreate, useful if the same code is used for multiple threads
	int32_t Seconds = 0;
	uint32_t nline;

    CyU3PReturnStatus_t Status;
    HWconfig = 0;

    GpioInitClock();

    Status = I2cInit (); // initialize i2c on FX3014 must be ok.
    if (Status != CY_U3P_SUCCESS)
    	DebugPrint(4, "\r\nI2cInit failed to initialize. Error code: %d.\r\n", Status);
	else
	{
		Status = Si5351init();
		if (Status != CY_U3P_SUCCESS)
		{
			HWconfig = HF103;
			DebugPrint(4, "\r\nSi5351init failed to initialize. HF103 detected \r\n");
		}
		else
		{
			si5351aSetFrequencyB(16000000);
			uint8_t identity;
			if (I2cTransfer(0, R820T_I2C_ADDR, 1, &identity, true) == CY_U3P_SUCCESS)
			{
				// check if BBRF103 or RX888 (RX666 ?)
				if(GPIOtestInputPulldown(LED_KIT)) {
					HWconfig = BBRF103;
				}
				else
				{
					HWconfig = RX888;
				}
			}
			else if (I2cTransfer(0, R828D_I2C_ADDR, 1, &identity, true) == CY_U3P_SUCCESS)
			{
				HWconfig = RX888r2;
			}
			else
			{
				HWconfig = RX999;
			}
			si5351aSetFrequencyB(0);
		}
	}
    DebugPrint(4, "\r\nHWconfig: %d.\r\n", HWconfig);

	switch(HWconfig) {
		case HF103:
			hf103_GpioInitialize();
			break;
		case BBRF103:
			bbrf103_GpioInitialize();
			break;
		case RX888:
			rx888_GpioInitialize();
			break;
		case RX888r2:
			rx888r2_GpioInitialize();
			break;
		case RX999:
			rx999_GpioInitialize();
			break;
	}

    // Spin up the USB Connection
	Status = InitializeUSB();
	CheckStatus("Initialize USB", Status);
	 if (Status == CY_U3P_SUCCESS)
	    {
	    	DebugPrint(4, "\r\nApplication started with %d", input);
			// Wait for the device to be enumerated
	    	while (!glIsApplnActive)
			{
				// Check for USB CallBack Events every 100msec
	    		CyU3PThreadSleep(100);
				while( CyU3PQueueReceive(&EventAvailable, &Qevent, CYU3P_NO_WAIT)== 0)
					{
						MsgParsing(Qevent);
					}
			}

	    	// Now run forever
			DebugPrint(4, "\r\nMAIN now running forever: ");
			while(1)
			{
				//for (Count = 0; Count<10; Count++)
				{
					// Check for User Commands (and other CallBack Events) every 100msec
					CyU3PThreadSleep(100);
					nline =0;
					while( CyU3PQueueReceive(&EventAvailable, &Qevent, CYU3P_NO_WAIT)== 0)
					{
						if (nline++ == 0) DebugPrint(4, "\r\n"); //first line
						MsgParsing(Qevent);
					}
				}
				if (glDMACount > 7812)
				{
					glDMACount -= 7812;
					DebugPrint(4, "%d, ", Seconds++);
				}
			}
	    }
	 DebugPrint(4, "\r\nApplication failed to initialize. Error code: %d.\r\n", Status);
	 	 // Returning here will stop the Application Thread running - it failed anyway so this is OK
}

/* Application define function which creates the threads. */
void CyFxApplicationDefine (void) {

    uint32_t Status;

    // If I get here then RTOS has started correctly, turn off ErrorIndicator
	IndicateError(0);

    // Create any needed resources then the Application thread
    Status = InitializeDebugConsole();
    CheckStatus("Initialize Debug Console", Status);

    // Create Queue used to transfer callback messages
        Status = CyU3PQueueCreate(&EventAvailable, 1, &EventAvailableQueue, sizeof(EventAvailableQueue));
        CheckStatus("Create EventAvailableQueue", Status);


    StackPtr[0] = CyU3PMemAlloc (FIFO_THREAD_STACK);
    Status = CyU3PThreadCreate (&ThreadHandle[0],                  /* Slave FIFO app thread structure */
                          "11:HF103_ADC2USB30",                     /* Thread ID and thread name */
                          ApplicationThread,                   /* Slave FIFO app thread entry function */
                          0,                                       /* No input parameter to thread */
                          StackPtr[0],                                     /* Pointer to the allocated thread stack */
                          FIFO_THREAD_STACK,               /* App Thread stack size */
                          FIFO_THREAD_PRIORITY,            /* App Thread priority */
                          FIFO_THREAD_PRIORITY,            /* App Thread pre-emption threshold */
                          CYU3P_NO_TIME_SLICE,                     /* No time slice for the application thread */
                          CYU3P_AUTO_START                         /* Start the thread immediately */
                          );

    /* Check the return code*/
    CheckStatus("CyFxApplicationDefine",Status);
    if (Status != 0)
    	while(1); // Application cannot continue. Loop indefinitely
}

