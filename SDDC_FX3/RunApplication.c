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
#include "Si5351.h"
#include "rd5815.h"

#include "radio.h"

// Declare external functions
extern void CheckStatus(char* StringPtr, CyU3PReturnStatus_t Status);
extern void CheckStatusSilent(char* StringPtr, CyU3PReturnStatus_t Status);
extern CyU3PReturnStatus_t InitializeDebugConsole(void);
extern void IndicateError(uint16_t ErrorCode);
extern CyU3PReturnStatus_t InitializeUSB(uint8_t hwconfig);
extern void ParseCommand(void);

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
uint16_t FWconfig = (FIRMWARE_VER_MAJOR << 8) | FIRMWARE_VER_MINOR;    // Firmware rc1 ver 1.02

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

CyU3PReturnStatus_t
ConfGPIOsimpleinputPU( uint8_t gpioid)
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
	      CyU3PGpioSetIoMode(gpioid, CY_U3P_GPIO_IO_MODE_WPU);
	 return apiRetStatus;
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
			DebugPrint(4, "\r\nEvent received = %s   \r\n", EventName[(uint8_t)qevent]);
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
#ifndef _DEBUG_USB_
	int32_t Seconds = 0;  // second count in serial debug
#endif
	uint32_t nline;
	CyBool_t measure;
	CyBool_t measure2;
    CyU3PReturnStatus_t Status;
    HWconfig = 0;

    GpioInitClock();

	DebugPrint(4, "Detect Hardware");
    Status = I2cInit (); // initialize i2c on FX3014 must be ok.
    if (Status != CY_U3P_SUCCESS)
    	DebugPrint(4, "I2cInit failed to initialize. Error code: %d.", Status);
	else
	{
		Status = Si5351init();
		if (Status != CY_U3P_SUCCESS)
		{
			ConfGPIOsimpleinputPU(GPIO52);
			ConfGPIOsimpleinputPU(GPIO53);
			CyU3PGpioSimpleGetValue ( GPIO52, &measure); //measure if external pull down
			if (measure) 
			{
				HWconfig = HF103;
				DebugPrint(4, "Si5351init failed to initialize. HF103 detected.");
			}
			else
			{
				HWconfig = RXLUCY;
				DebugPrint(4, "Si5351init failed to initialize. RXLUCY detected.");
			}
		}
		else
		{
			ConfGPIOsimpleinputPU(GPIO50);
			ConfGPIOsimpleinputPU(GPIO45);
			ConfGPIOsimpleinputPU(GPIO36);

			si5351aSetFrequencyB(16000000);
			uint8_t identity;
			if (I2cTransfer(0, R820T_I2C_ADDR, 1, &identity, true) == CY_U3P_SUCCESS)
			{
				// check if BBRF103 or RX888 (RX666 ?)
				CyU3PGpioSimpleGetValue ( GPIO50, &measure);
				CyU3PGpioSimpleGetValue ( GPIO45, &measure2);

				if(!measure) {
					HWconfig = BBRF103;
					DebugPrint(4, "R820T Detectedinitialize. BBRF103 detected.");
				}
				else if (!measure2)
				{
					HWconfig = RX888;
					DebugPrint(4, "R820T Detectedinitialize. RX888 detected.");
				}
				else
				{
					HWconfig = NORADIO;
				}
			}
			else if (I2cTransfer(0, R828D_I2C_ADDR, 1, &identity, true) == CY_U3P_SUCCESS)
			{
				CyU3PGpioSimpleGetValue ( GPIO36, &measure);

				if (!measure)
				{
					HWconfig = RX888r2;
					DebugPrint(4, "R828D Detectedinitialize. RX888r2 detected.");
				}
				else
				{
					HWconfig = NORADIO;
				}
			}
			else if (I2cTransfer(0, RD5812_I2C_ADDR, 1, &identity, true) == CY_U3P_SUCCESS)
			{
				HWconfig = RX888r3;
				DebugPrint(4, "RD5812 Detectedinitialize. RX888r3 detected.");
			}
			else
			{
				HWconfig = RX999;
				DebugPrint(4, "No Tuner Detectedinitialize. RX999 detected.");
			}
			si5351aSetFrequencyB(0);
		}
	}
    DebugPrint(4, "HWconfig: %d.", HWconfig);

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
		case RX888r3:
			rx888r3_GpioInitialize();
			break;
		case RX999:
			rx999_GpioInitialize();
			break;
		case RXLUCY:
			rxlucy_GpioInitialize();
			break;
	}

    // Spin up the USB Connection
	Status = InitializeUSB(HWconfig);
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
#ifndef _DEBUG_USB_  // #include "../Interface.h"  -> second count in serial debug
				if (glDMACount > 7812)
				{
					glDMACount -= 7812;
					DebugPrint(4, "%d, \n", Seconds++);
				}
#endif
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

