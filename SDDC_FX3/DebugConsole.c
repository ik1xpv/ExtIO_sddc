/*
 *  DebugConsole.c
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
 *                 see also FX3 SDK example 'serialif_examples/cyfxusbuart'
 *  Date: Sat Nov 14 12:15:07 PM EST 2020
 */

#include "Application.h"

// Declare external functions
extern void CheckStatus(char* StringPtr, CyU3PReturnStatus_t Status);

// Declare external data
extern CyU3PQueue EventAvailable;			  	// Used for threads communications
extern uint32_t glDMACount;
extern CyBool_t glIsApplnActive;				// Set true once device is enumerated
extern uint8_t  HWconfig;       			    // Hardware config
extern CyU3PUartConfig_t glUartConfig;
extern CyU3PDmaChannel glCDCtoCPU_Handle;        /* Handle needed for CDC Out Endpoint */
extern CyU3PDmaChannel glCPUtoCDC_Handle;        /* Handle needed for CDC In Endpoint */
extern CyU3PDmaChannel glCDCtoCDC_Handle;        /* Handle needed for CDC Loopback */

extern uint16_t EpSize[];

// Global data owned by this module

extern void CheckStatus(char* StringPtr, CyU3PReturnStatus_t Status);

extern CyU3PThread ThreadHandle[APP_THREADS];		// Handles to my Application Threads
extern void *StackPtr[APP_THREADS];				// Stack allocated to each thread

CyBool_t glDebugTxEnabled = CyFalse;	// Set true once I can output messages to the Console
CyU3PDmaChannel glUARTtoCPU_Handle;		// Handle needed by Uart Callback routine
static CyBool_t UsingUARTConsole;
char ConsoleInBuffer[32];				// Buffer for user Console Input
uint32_t ConsoleInIndex;				// Index into ConsoleIn buffer
uint32_t glCounter[20];					// Counters used to monitor GPIF
uint32_t MAXCLOCKVALUE = 10;
uint32_t ClockValue;	// Used to Set/Display GPIF clock
uint8_t Toggle;
uint32_t Qevent __attribute__ ((aligned (32)));

int glIsUartAttached = 0;

// For Debug and education display the name of the Event
const char* EventName[] = {
	    "CONNECT", "DISCONNECT", "SUSPEND", "RESUME", "RESET", "SET_CONFIGURATION", "SPEED",
	    "SET_INTERFACE", "SET_EXIT_LATENCY", "SOF_ITP", "USER_EP0_XFER_COMPLETE", "VBUS_VALID",
	    "VBUS_REMOVED", "HOSTMODE_CONNECT", "HOSTMODE_DISCONNECT", "OTG_CHANGE", "OTG_VBUS_CHG",
	    "OTG_SRP", "EP_UNDERRUN", "LINK_RECOVERY", "USB3_LINKFAIL", "SS_COMP_ENTRY", "SS_COMP_EXIT"
};


void SwitchConsoles(void);

void ParseCommand(void)
{
	// User has entered a command, process it
    CyU3PReturnStatus_t Status = CY_U3P_SUCCESS;

    if (!strcmp("", ConsoleInBuffer))
    {
	if (glIsUartAttached) {
	    DebugPrint(4, "\r\nEnter commands:\r\n"
			"threads, stack, gpif, reset, switch\r\n"
    			"DMAcnt = %x\r\n", glDMACount);
	} else {
	    DebugPrint(4, "\r\nEnter commands:\r\n"
    			"threads, stack, gpif, reset\r\n"
    			"DMAcnt = %x\r\n", glDMACount);
    	}
    }
    else if (!strcmp("threads", ConsoleInBuffer))
	{
    	DebugPrint(4, "\r\nthreads:");
		CyU3PThread *ThisThread, *NextThread;
		char* ThreadName;
		// First find out who I am
		ThisThread = CyU3PThreadIdentify();
		tx_thread_info_get(ThisThread, &ThreadName, NULL, NULL, NULL, NULL, NULL, &NextThread, NULL);
		// Now, using the Thread linked list, look for other threads until I find myself again
		DebugPrint(4, "\r\nThis : '%s'", ThreadName);
		while (NextThread != ThisThread)
		{
			tx_thread_info_get(NextThread, &ThreadName, NULL, NULL, NULL, NULL, NULL, &NextThread, NULL);
			DebugPrint(4, "\r\nFound: '%s'", ThreadName);
		}
		DebugPrint(4, "\r\n\r\n");
	}
    else if (!strcmp("stack", ConsoleInBuffer))
    {
		char* ThreadName;
		DebugPrint(4, "\r\nstack:");
		// Note that StackSize is in bytes but RTOS fill pattern is a uint32
		uint32_t* StackStartPtr = StackPtr[0];
		uint32_t* DataPtr = StackStartPtr;
		while (*DataPtr++ != 0xEFEFEFEF) ;
		CyU3PThreadInfoGet(&ThreadHandle[0], &ThreadName, 0, 0, 0);
		ThreadName += 3;	// Skip numeric ID
		DebugPrint(4, "\r\nStack free in %s is %d/%d\r\n", ThreadName,
				FIFO_THREAD_STACK - ((DataPtr - StackStartPtr)<<2), FIFO_THREAD_STACK);
    }
	else if (!strcmp("reset", ConsoleInBuffer))
	{
		DebugPrint(4, "\r\nreset:");
		DebugPrint(4, "\r\nRESETTING CPU\r\n");
		CyU3PThreadSleep(100);
		CyU3PDeviceReset(CyFalse);
	}
	else if (!strcmp("gpif", ConsoleInBuffer))
	{
		uint8_t State = 0xFF;
		Status = CyU3PGpifGetSMState(&State);
		CheckStatus("Get GPIF State", Status);
		DebugPrint(4, "\r\nGPIF State = %d\r\n", State);
	}
	else if (glIsUartAttached == 1 && !strcmp("switch", ConsoleInBuffer))
	{
		SwitchConsoles();
	}
	else DebugPrint(4, "Input: '%s'\r\n", &ConsoleInBuffer[0]);
	ConsoleInIndex = 0;
}

void ConsoleIn(char InputChar)
{
	DebugPrint(4, "%c", InputChar);			// Echo the character
	// On CR, signal Main loop to process the command entered by the user.  Should NOT do this in a CallBack routine
	if (InputChar == 0x0d)
	{
		Qevent = USER_COMMAND_AVAILABLE << 24;
		CyU3PQueueSend(&EventAvailable, &Qevent, CYU3P_NO_WAIT);
	}
	else
	{
		ConsoleInBuffer[ConsoleInIndex] = InputChar | 0x20;		// Save character as lower case (for compares)
		if (ConsoleInIndex++ < sizeof(ConsoleInBuffer)) ConsoleInBuffer[ConsoleInIndex] = 0;
		else ConsoleInIndex--;
	}
}

void UartCallback(CyU3PUartEvt_t Event, CyU3PUartError_t Error)
// Handle characters typed in by the developer
{
	CyU3PDmaBuffer_t ConsoleInDmaBuffer;
	char InputChar;
	if (Event == CY_U3P_UART_EVENT_RX_DONE)
    {
		CyU3PDmaChannelSetWrapUp(&glUARTtoCPU_Handle);
		CyU3PDmaChannelGetBuffer(&glUARTtoCPU_Handle, &ConsoleInDmaBuffer, CYU3P_NO_WAIT);
		InputChar = (char)*ConsoleInDmaBuffer.buffer;
		ConsoleIn(InputChar);
		CyU3PDmaChannelDiscardBuffer(&glUARTtoCPU_Handle);
		CyU3PUartRxSetBlockXfer(1);
    }
}

void CDC_CharsReceived(CyU3PDmaChannel *Handle, CyU3PDmaCbType_t Type, CyU3PDmaCBInput_t *Input)
{
	uint32_t i;
    if (Type == CY_U3P_DMA_CB_PROD_EVENT)
    {
    	for (i=0; i<Input->buffer_p.count; i++) ConsoleIn(*Input->buffer_p.buffer++);
		CyU3PDmaChannelDiscardBuffer(Handle);
    }
}

CyU3PReturnStatus_t InitializeDebugConsoleIn(CyBool_t UsingUART)
{
	CyU3PReturnStatus_t Status;
    CyU3PDmaChannelConfig_t dmaConfig;
    if (UsingUART)
    {
    	DebugPrint(4, "\nSetting up UART Console In");
    	// Now setup a DMA channel to receive characters from the Uart Rx
        Status = CyU3PUartRxSetBlockXfer(1);
        CheckStatus("CyU3PUartRxSetBlockXfer", Status);
    	CyU3PMemSet((uint8_t *)&dmaConfig, 0, sizeof(dmaConfig));
    	dmaConfig.size  		= 16;									// Minimum size allowed, I only need 1 byte
    	dmaConfig.count 		= 1;									// I can't type faster than the Uart Callback routine!
    	dmaConfig.prodSckId		= CY_U3P_LPP_SOCKET_UART_PROD;
    	dmaConfig.consSckId 	= CY_U3P_CPU_SOCKET_CONS;
    	dmaConfig.dmaMode 		= CY_U3P_DMA_MODE_BYTE;
    	dmaConfig.notification	= CY_U3P_DMA_CB_PROD_EVENT;
    	Status = CyU3PDmaChannelCreate(&glUARTtoCPU_Handle, CY_U3P_DMA_TYPE_MANUAL_IN, &dmaConfig);
        CheckStatus("CreateDebugRxDmaChannel", Status);
        if (Status != CY_U3P_SUCCESS) CyU3PDmaChannelDestroy(&glUARTtoCPU_Handle);
        else
        {
    		Status = CyU3PDmaChannelSetXfer(&glUARTtoCPU_Handle, 0);
    		CheckStatus("ConsoleInEnabled", Status);
        }
    }
    else
    {
    	DebugPrint(4, "\nSetting up USB_CDC Console In");
		CyU3PMemSet((uint8_t *)&dmaConfig, 0, sizeof(dmaConfig));
		dmaConfig.size  		= EpSize[CyU3PUsbGetSpeed()];
		dmaConfig.count 		= 2;
		dmaConfig.prodSckId		= CY_FX_EP_PRODUCER_CDC_SOCKET;
		dmaConfig.consSckId 	= CY_U3P_CPU_SOCKET_CONS;
		dmaConfig.dmaMode 		= CY_U3P_DMA_MODE_BYTE;
		dmaConfig.notification	= CY_U3P_DMA_CB_PROD_EVENT;
		dmaConfig.cb = CDC_CharsReceived;
		Status = CyU3PDmaChannelCreate(&glCDCtoCPU_Handle, CY_U3P_DMA_TYPE_MANUAL_IN, &dmaConfig);
		CheckStatus("CreateCDC_ConsoleInDmaChannel", Status);
		if (Status != CY_U3P_SUCCESS) CyU3PDmaChannelDestroy(&glCDCtoCPU_Handle);
		else
		{
			Status = CyU3PDmaChannelSetXfer(&glCDCtoCPU_Handle, 0);
			CheckStatus("ConsoleInEnabled", Status);
		}
    }
	return Status;
}

// Spin up the DEBUG Console, Out and In
CyU3PReturnStatus_t InitializeDebugConsole(void)
{
    CyU3PUartConfig_t uartConfig;
    CyU3PReturnStatus_t Status = CY_U3P_SUCCESS;

    if (glIsUartAttached == -1) {
	switch (HWconfig) {
	    case BBRF103: glIsUartAttached = 1; break;;
	    case HF103:   glIsUartAttached = 1; break;;
	    case RX888:   glIsUartAttached = 0; break;;
            default:      glIsUartAttached = 0; break;;
	}
    }
    Status = CyU3PUartInit();										// Start the UART driver
    CheckStatus("CyU3PUartInit", Status);							// This will not display since we're not connected yet

    CyU3PMemSet ((uint8_t *)&uartConfig, 0, sizeof (uartConfig));
	uartConfig.baudRate = CY_U3P_UART_BAUDRATE_115200;				// UART baudrate
	uartConfig.stopBit  = CY_U3P_UART_ONE_STOP_BIT;
	uartConfig.txEnable = CyTrue;
	uartConfig.rxEnable = CyTrue;
	uartConfig.isDma    = CyTrue;
	Status = CyU3PUartSetConfig(&uartConfig, UartCallback);			// Configure the UART hardware
    CheckStatus("CyU3PUartSetConfig", Status);

    Status = CyU3PUartTxSetBlockXfer(0xFFFFFFFF);					// Send as much data as I need to
    CheckStatus("CyU3PUartTxSetBlockXfer", Status);

	Status = CyU3PDebugInit(CY_U3P_LPP_SOCKET_UART_CONS, 8);		// Attach the Debug driver above the UART driver
	if (Status == CY_U3P_SUCCESS) glDebugTxEnabled = CyTrue;
    CheckStatus("ConsoleOutEnabled", Status);						// First console display message
	CyU3PDebugPreamble(CyFalse);									// Skip preamble, debug info is targeted for a person

	UsingUARTConsole = glIsUartAttached == 1 ? CyTrue : CyFalse;
	Status = InitializeDebugConsoleIn(UsingUARTConsole);
    return Status;
}

void SwitchConsoles(void)
{
	if (glIsUartAttached != 1) {
	    return;
	}

	CyU3PReturnStatus_t Status;
	// Only proceed if USB connection is up
	if (glIsApplnActive)
	{
		// Tear down DMA channels that need to be reassigned
		if (UsingUARTConsole)
		{
			CyU3PDmaChannelDestroy(&glCDCtoCDC_Handle);
			CyU3PDmaChannelDestroy(&glUARTtoCPU_Handle);
		}
		else CyU3PDmaChannelDestroy(&glCDCtoCPU_Handle);
		// Switch console
		UsingUARTConsole = !UsingUARTConsole;
		DebugPrint(4, "Switching console to %s", UsingUARTConsole ? "UART" : "USB");
		CyU3PThreadSleep(100);		// Delay to allow message to get to the user
		// Disconnect the current console
		CyU3PDebugDeInit();
		CyU3PThreadSleep(100);		// Delay to allow Debug thread to complete and all buffers returned
		// Connect up the new Console out - this is simpler than the I2C case since the USB socket is an infinite sink
		Status = CyU3PDebugInit(UsingUARTConsole ? CY_U3P_LPP_SOCKET_UART_CONS : CY_FX_EP_CONSUMER_CDC_SOCKET, 8);
		CheckStatus("DebugInit", Status);
		CyU3PDebugPreamble(CyFalse);							// Skip preamble, debug info is targeted for a person
		// Say hello on the new console
		DebugPrint(4, "Console is now %s", UsingUARTConsole ? "UART" : "USB" );
		// Now connect up Console In
		Status = InitializeDebugConsoleIn(UsingUARTConsole);
		CheckStatus("InitializeDebugConsoleIn", Status);
		// Connect CDC_Loopback if necessary
	}
	else DebugPrint(4, "USB not active, cannot switch consoles\n");
}
