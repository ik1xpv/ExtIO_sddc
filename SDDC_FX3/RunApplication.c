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
 *
 *  add USB CDC debug port
 *  Author: Franco Venturi
 *  modified from: SuperSpeed Device Design By Example - John Hyde
 *  Date: Sat Nov 14 09:35:27 AM EST 2020
 *
 *  - If the device has a UART connected, then the debug console will work as follows:
 *      On power on the CDC interface is looped-back such that characters are
 *      just echoed and the UART is connected as the Debug Console
 *      Following a "switch" command the Debug Console is connected to the CDC
 *      interface. Another "switch" will switch it back to the UART
 *
 *  - If the device wdoes not have a UART connected, then the debug console
 *    will always be connected to the CDC interface
 */
#include "Application.h"

// Declare external functions
extern void CheckStatus(char* StringPtr, CyU3PReturnStatus_t Status);
extern void CheckStatusSilent(char* StringPtr, CyU3PReturnStatus_t Status);
extern CyU3PReturnStatus_t InitializeDebugConsole(void);
extern void IndicateError(uint16_t ErrorCode);
extern CyU3PReturnStatus_t InitializeUSB(void);
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

uint8_t HWconfig = 0x01;       // Hardware config type BBRF103
uint16_t FWconfig = 0x0101;    // Firmware rc1 ver 1.01

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
HF103_GpioInit ()
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

    if (HWconfig == HF103)
    {
		ConfGPIOsimpleout(17);   // DITH
		ConfGPIOsimpleout(18);   // SHDN
		ConfGPIOsimpleout(19);	 // free
		ConfGPIOsimpleout(20);	 // free
		ConfGPIOsimpleout(21);   // ATT_LE Latch Enable
		ConfGPIOsimpleout(22);   // ATT_CLK
		ConfGPIOsimpleout(23);   // ATT_DATA - set 23 as input to check HW version link 10K resistor
		ConfGPIOsimpleout(24);	 // free
		ConfGPIOsimpleout(25);	 // free
		ConfGPIOsimpleout(26);	 // free
		ConfGPIOsimpleout(27);	 // free
		ConfGPIOsimpleinput(28); // OFA overflow flag
		ConfGPIOsimpleout(29);   // RAND

    	ConfGPIOsimpleout(LED_KIT);  // H103 found
    	CyU3PGpioSetValue (LED_KIT, 0);

		CyU3PGpioSetValue (17, 0);
		CyU3PGpioSetValue (18, 0);
		CyU3PGpioSetValue (19, 0);
		CyU3PGpioSetValue (20, 0);
		CyU3PGpioSetValue (21, 0);  // ATT_LE latched
		CyU3PGpioSetValue (22, 1);  // test version
		CyU3PGpioSetValue (23, 1);  // ATT_DATA
		CyU3PGpioSetValue (24, 0);
		CyU3PGpioSetValue (25, 0);
		CyU3PGpioSetValue (26, 0);
		CyU3PGpioSetValue (27, 0);
		CyU3PGpioSetValue (29, 0);
    }
    else  //BBRF103, RX666, RX888
    {
        // check if BBRF103 or RX888 (RX666 ?)
        if(GPIOtestInputPulldown(LED_KIT)) {
        	HWconfig = BBRF103;
		}
        else {
        	HWconfig = RX888;
		}

		ConfGPIOsimpleout(17);   // free
		ConfGPIOsimpleout(18);   // VHF ANT Bias-T
		ConfGPIOsimpleout(19);	 // HF ANT Bias-T
		ConfGPIOsimpleout(20);	 // RAND
		ConfGPIOsimpleout(21);   // LED RED
		ConfGPIOsimpleout(22);   // LED YELLOW
		ConfGPIOsimpleout(23);   // LED BLUE
		ConfGPIOsimpleout(24);	 // IF PWM R820T2
		ConfGPIOsimpleinput(25); // IN OFA
		ConfGPIOsimpleout(26);	 // OUT SEL0
		ConfGPIOsimpleout(27);	 // OUT SEL1
		ConfGPIOsimpleout(28); 	 // SHDN
		ConfGPIOsimpleout(29);   // DITH

		CyU3PGpioSetValue (17, 0);
		CyU3PGpioSetValue (18, 0);
		CyU3PGpioSetValue (19, 0);
		CyU3PGpioSetValue (20, 0);
		CyU3PGpioSetValue (21, 1);
		CyU3PGpioSetValue (22, 1);
		CyU3PGpioSetValue (23, 1);
		CyU3PGpioSetValue (24, 0);
		CyU3PGpioSetValue (25, 0);
		CyU3PGpioSetValue (26, 0);
		CyU3PGpioSetValue (27, 0);
		CyU3PGpioSetValue (29, 0);
    }
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

unsigned long calls = 1;
void ApplicationThread ( uint32_t input)
{
	// input is passed to this routine from CyU3PThreadCreate, useful if the same code is used for multiple threads
	int32_t Seconds = 0;
	uint32_t nline;

    CyU3PReturnStatus_t Status;

    Status = I2cInit ();
    if (Status != CY_U3P_SUCCESS)
    	 HWconfig = HF103;

	HF103_GpioInit();

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

/*
 	HF103
    DAT-31-SP+  control
	direct GPIO output implementation  31 step 1 dB
*/
void WriteAttenuator(uint8_t value)
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


