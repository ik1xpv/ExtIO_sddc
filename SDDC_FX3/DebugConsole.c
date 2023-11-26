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
#include <stdarg.h>

// Declare external functions
extern void CheckStatus(char* StringPtr, CyU3PReturnStatus_t Status);

// Declare external data
extern CyU3PQueue EventAvailable;			  	// Used for threads communications
extern uint32_t glDMACount;
extern CyBool_t glIsApplnActive;				// Set true once device is enumerated
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
extern uint32_t Qevent __attribute__ ((aligned (32)));

CyBool_t flagdebug = false;
volatile uint16_t debtxtlen = 0;
uint8_t bufdebug[MAXLEN_D_USB];  // buffer debug string//

// For Debug and education display the name of the Event
const char* EventName[] = {
	    "CONNECT", "DISCONNECT", "SUSPEND", "RESUME", "RESET", "SET_CONFIGURATION", "SPEED",
	    "SET_INTERFACE", "SET_EXIT_LATENCY", "SOF_ITP", "USER_EP0_XFER_COMPLETE", "VBUS_VALID",
	    "VBUS_REMOVED", "HOSTMODE_CONNECT", "HOSTMODE_DISCONNECT", "OTG_CHANGE", "OTG_VBUS_CHG",
	    "OTG_SRP", "EP_UNDERRUN", "LINK_RECOVERY", "USB3_LINKFAIL", "SS_COMP_ENTRY", "SS_COMP_EXIT"
};

#ifdef TRACESERIAL 
// For Debug and display the name of the FX3Command
const char* FX3CommandName[] = {  // start 0xAA
"STARTFX3", "STOPFX3", "TESTFX3", "GPIOFX3", "I2CWFX3","I2CRFX3", "0xB0", "RESETFX3",
"STARTADC", "0xB3", "TUNERINIT","TUNERTUNE","SETARGFX3","0xB7", "TUNERSTDBY","0xB9","READINFODEBUG"
};

// For Debug and display the name of the FX3Command
const char* SETARGFX3List[] = { 
"0", "R82XX_ATTENUATOR","R82XX_VGA","R82XX_SIDEBAND","R82XX_HARMONIC","5","6","7","8","9",
"DAT31_ATT","AD8340_VGA","PRESELECTOR","VHF_ATTENUATOR"
};
#endif

void ParseCommand(void)
{
	// User has entered a command, process it
    CyU3PReturnStatus_t Status = CY_U3P_SUCCESS;

    if (!strcmp("", ConsoleInBuffer)||!strcmp("?", ConsoleInBuffer) )
    {
    	DebugPrint(4, "Enter commands:\r\n"
    			"threads, stack, gpif, reset;\r\n"
    			"DMAcnt = %x\r\n\r\n", glDMACount);
    }
    else if (!strcmp("threads", ConsoleInBuffer))
	{
    	DebugPrint(4, "threads:\r\n");
		CyU3PThread *ThisThread, *NextThread;
		char* ThreadName;
		// First find out who I am
		ThisThread = CyU3PThreadIdentify();
		tx_thread_info_get(ThisThread, &ThreadName, NULL, NULL, NULL, NULL, NULL, &NextThread, NULL);
		// Now, using the Thread linked list, look for other threads until I find myself again
		DebugPrint(4, "This : '%s'", ThreadName);
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
		DebugPrint(4, "stack:\r\n");
		// Note that StackSize is in bytes but RTOS fill pattern is a uint32
		uint32_t* StackStartPtr = StackPtr[0];
		uint32_t* DataPtr = StackStartPtr;
		while (*DataPtr++ != 0xEFEFEFEF) ;
		CyU3PThreadInfoGet(&ThreadHandle[0], &ThreadName, 0, 0, 0);
		ThreadName += 3;	// Skip numeric ID
		DebugPrint(4, "Stack free in %s is %d/%d\r\n\r\n", ThreadName,
				FIFO_THREAD_STACK - ((DataPtr - StackStartPtr)<<2), FIFO_THREAD_STACK);
    }
	else if (!strcmp("reset", ConsoleInBuffer))
	{
		DebugPrint(4, "reset:\r\n");
		DebugPrint(4, "RESETTING CPU\r\n");
		CyU3PThreadSleep(100);
		CyU3PDeviceReset(CyFalse);
	}
	else if (!strcmp("gpif", ConsoleInBuffer))
	{
		uint8_t State = 0xFF;
		Status = CyU3PGpifGetSMState(&State);
		CheckStatus("Get GPIF State ", Status);
		DebugPrint(4, "GPIF State = %d\r\n\r\n", State);
	}
	else DebugPrint(4, "Input: '%s'\r\n\r\n", &ConsoleInBuffer[0]);
	ConsoleInIndex = 0;
}

uint8_t * CyU3PDebugIntToStr(uint8_t *convertedString, uint32_t num, uint8_t base);

// from /FX3_SDK_1_3_1_SRC/sdk/firmware/src/system/cyu3debug.c  
// MyDebugSNPrint

static CyU3PReturnStatus_t MyDebugSNPrint (
        uint8_t  *debugMsg,
        uint16_t *length,
        char     *message,
        va_list   argp)
{
    uint8_t  *string_p;
    uint8_t  *argStr = NULL;
    CyBool_t  copyReqd = CyFalse;
    uint16_t  i = 0, j, maxLength = *length;
    int32_t   intArg;
    uint32_t  uintArg;
    uint8_t   convertedString[11];

    if (debugMsg == 0)
        return CY_U3P_ERROR_BAD_ARGUMENT;

    /* Parse the string and copy into the buffer for sending out. */
    for (string_p = (uint8_t *)message; (*string_p != '\0'); string_p++)
    {
        if (i >= (maxLength - 2))
            return CY_U3P_ERROR_BAD_ARGUMENT;

        if (*string_p != '%')
        {
            debugMsg[i++] = *string_p;
            continue;
        }

        string_p++;
        switch (*string_p)
        {
        case '%' :
            {
                debugMsg[i++] = '%';
            }
            break;

        case 'c' :
            {
                debugMsg[i++] = (uint8_t)va_arg (argp, int32_t);
            }
            break;

        case 'd' :
            {
                intArg = va_arg (argp, int32_t);
                if (intArg < 0)
                {
                    debugMsg[i++] = '-';
                    intArg = -intArg;
                }

                argStr =  CyU3PDebugIntToStr (convertedString, intArg, 10);
                copyReqd = CyTrue;
            }
            break;

        case 's':
            {
                argStr = va_arg (argp, uint8_t *);
                copyReqd = CyTrue;
            }
            break;

        case 'u':
            {
                uintArg = va_arg (argp, uint32_t);
                argStr = CyU3PDebugIntToStr (convertedString, uintArg, 10);
                copyReqd = CyTrue;
            }
            break;

        case 'X':
        case 'x':
            {
                uintArg = va_arg (argp, uint32_t);
                argStr = CyU3PDebugIntToStr (convertedString, uintArg, 16);
                copyReqd = CyTrue;
            }
            break;

        default:
            return CY_U3P_ERROR_BAD_ARGUMENT;
        }

        if (copyReqd)
        {
            j = (uint16_t)strlen ((char *)argStr);
            if (i >= (maxLength - j - 1))
                return CY_U3P_ERROR_BAD_ARGUMENT;
            strcpy ((char *)(debugMsg + i), (char *)argStr);
            i += j;
            copyReqd = CyFalse;
        }
    }

    /* NULL-terminate the string. There will always be space for this. */
    debugMsg[i] = '\0';
    *length     = i;

    return CY_U3P_SUCCESS;
}

void DebugPrint2USB ( uint8_t priority, char *msg, ...)
{
	if ((glIsApplnActive != CyTrue)||(flagdebug == false)) return;
	va_list argp;
	uint8_t buf[MAXLEN_D_USB];
//		memset(buf,0,sizeof(buf)); // not necessary
		CyU3PReturnStatus_t stat;
		uint16_t len = MAXLEN_D_USB;
		va_start (argp, msg);
		stat = MyDebugSNPrint (buf, &len, msg, argp);
		va_end (argp);
		if ( stat == CY_U3P_SUCCESS ) 
		{
			if (debtxtlen+len > MAXLEN_D_USB) CyU3PThreadSleep(100);
			if (debtxtlen+len < MAXLEN_D_USB) 
			{
				memcpy(&bufdebug[debtxtlen], buf, len);
				debtxtlen = debtxtlen+len;		
			}		
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

