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
 */

#include "Application.h"

// Declare external functions
extern void CheckStatus(char* StringPtr, CyU3PReturnStatus_t Status);

// Declare external data
extern CyU3PQueue EventAvailable;			  	// Used for threads communications
extern uint32_t glDMACount;

// Global data owned by this module


extern void CheckStatus(char* StringPtr, CyU3PReturnStatus_t Status);

extern CyU3PThread ThreadHandle[APP_THREADS];		// Handles to my Application Threads
extern void *StackPtr[APP_THREADS];				// Stack allocated to each thread

CyBool_t glDebugTxEnabled = CyFalse;	// Set true once I can output messages to the Console
CyU3PDmaChannel glUARTtoCPU_Handle;		// Handle needed by Uart Callback routine
char ConsoleInBuffer[32];				// Buffer for user Console Input
uint32_t ConsoleInIndex;				// Index into ConsoleIn buffer
uint32_t glCounter[20];					// Counters used to monitor GPIF
uint32_t MAXCLOCKVALUE = 10;
uint32_t ClockValue;	// Used to Set/Display GPIF clock
uint8_t Toggle;
uint32_t Qevent __attribute__ ((aligned (32)));

// For Debug and education display the name of the Event
const char* EventName[] = {
	    "CONNECT", "DISCONNECT", "SUSPEND", "RESUME", "RESET", "SET_CONFIGURATION", "SPEED",
	    "SET_INTERFACE", "SET_EXIT_LATENCY", "SOF_ITP", "USER_EP0_XFER_COMPLETE", "VBUS_VALID",
	    "VBUS_REMOVED", "HOSTMODE_CONNECT", "HOSTMODE_DISCONNECT", "OTG_CHANGE", "OTG_VBUS_CHG",
	    "OTG_SRP", "EP_UNDERRUN", "LINK_RECOVERY", "USB3_LINKFAIL", "SS_COMP_ENTRY", "SS_COMP_EXIT"
};



void ParseCommand(void)
{
	// User has entered a command, process it
    CyU3PReturnStatus_t Status = CY_U3P_SUCCESS;

    if (!strcmp("", ConsoleInBuffer))
    {
    	DebugPrint(4, "\r\nEnter commands:\r\n"
    			"threads, stack, gpif, reset\r\n"
    			"DMAcnt = %x\r\n", glDMACount);
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
	else DebugPrint(4, "Input: '%s'\r\n", &ConsoleInBuffer[0]);
	ConsoleInIndex = 0;
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
		CyU3PDmaChannelDiscardBuffer(&glUARTtoCPU_Handle);
		CyU3PUartRxSetBlockXfer(1);
    }
}

// Spin up the DEBUG Console, Out and In
CyU3PReturnStatus_t InitializeDebugConsole(void)
{
    CyU3PUartConfig_t uartConfig;
    CyU3PDmaChannelConfig_t dmaConfig;
    CyU3PReturnStatus_t Status = CY_U3P_SUCCESS;

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
    return Status;
}

