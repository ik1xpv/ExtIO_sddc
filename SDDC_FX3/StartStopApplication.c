/*
 * StartStopApplication.c
 *
 *  Created on: 21/set/2020
 *
 *  HF103_FX3 project
 *  Author: Oscar Steila
 *  modified from: SuperSpeed Device Design By Example - John Hyde
 *
 *  https://sdr-prototypes.blogspot.com/
 *
 */

#include "SDDC_GPIF.h" // GPIFII include once
#include "Application.h"
extern uint32_t Qevent __attribute__ ((aligned (32)));
uint32_t glDMACount;

// Declare external functions
extern void CheckStatus(char* StringPtr, CyU3PReturnStatus_t Status);


// Declare external data
extern CyBool_t glIsApplnActive;				// Set true once device is enumerated

// Global data owned by this module


void StartApplication(void);
void StopApplication(void);

const char* BusSpeed[] = { "Not Connected", "Full ", "High ", "Super" };
char* CyFxGpifName = { "HF103.h" };

CyU3PDmaMultiChannel glMultiChHandleSlFifoPtoU;   /* DMA Channel handle for P2U transfer. */

/*
void Pib_error_cb(CyU3PPibIntrType cbType, uint16_t cbArg) {
	if (cbType == CYU3P_PIB_INTR_ERROR)
	{
	Qevent =( 1<<25 |  cbArg );
	CyU3PQueueSend(&EventAvailable, &Qevent, CYU3P_NO_WAIT);
	}
}
*/
/*
#define TH1_BUSY 7
#define TH1_WAIT 8
#define TH0_BUSY 5
*/
/* Callback funtion for the DMA event notification. */
void DmaCallback (
        CyU3PDmaChannel   *chHandle, /* Handle to the DMA channel. */
        CyU3PDmaCbType_t  type,      /* Callback type.             */
        CyU3PDmaCBInput_t *input)    /* Callback status.           */
{
    if (type == CY_U3P_DMA_CB_PROD_EVENT)
    {
        /* This is a produce event notification to the CPU. This notification is
         * received upon reception of every buffer. The DMA transfer will not wait
         * for the commit from CPU. Increment the counter. */
        glDMACount++;
    }
}


CyU3PReturnStatus_t StartGPIF(void)
{
	CyU3PReturnStatus_t Status;
	DebugPrint(4, "\r\nGPIF file %s", CyFxGpifName);
	Status = CyU3PGpifLoad(&CyFxGpifConfig);
	CheckStatus("GpifLoad", Status);
	Status = CyU3PGpifSMStart (0, 0); //START, ALPHA_START);
	return Status;
}

void StartApplication ( void ) {

    CyU3PEpConfig_t epCfg;
    CyU3PPibClock_t pibClock;
    CyU3PDmaMultiChannelConfig_t dmaMultiConfig;
    CyU3PReturnStatus_t Status = CY_U3P_SUCCESS;
    CyU3PUSBSpeed_t usbSpeed = CyU3PUsbGetSpeed();
    // Display the enumerated device bus speed
    DebugPrint(4, "\r\n@StartApplication, running at %sSpeed", BusSpeed[usbSpeed]);

    // Start GPIF clocks, they need to be running before we attach a DMA channel to GPIF
    pibClock.clkDiv = 2;
    pibClock.clkSrc = CY_U3P_SYS_CLK;
    pibClock.isHalfDiv = CyFalse;
    pibClock.isDllEnable = CyFalse;  // Disable Dll since this application is synchronous
    Status = CyU3PPibInit(CyTrue, &pibClock);
    CheckStatus("Start GPIF Clock", Status);

    CyU3PMemSet ((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable = CyTrue;
    epCfg.epType = CY_U3P_USB_EP_BULK;
    epCfg.burstLen = ENDPOINT_BURST_LENGTH;
    epCfg.streams = 0;
    epCfg.pcktSize = ENDPOINT_BURST_SIZE;
    epCfg.isoPkts = 1;

    glDMACount= 0;
    /* Consumer endpoint configuration */
    Status = CyU3PSetEpConfig(CY_FX_EP_CONSUMER, &epCfg);
    CheckStatus("CyU3PSetEpConfig Consumer", Status);
    CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);


	dmaMultiConfig.size           = DMA_BUFFER_SIZE;
	dmaMultiConfig.count          = DMA_BUFFER_COUNT;
	dmaMultiConfig.validSckCount  = 2;
	dmaMultiConfig.prodSckId [0]  = (CyU3PDmaSocketId_t)PING_PRODUCER_SOCKET;
	dmaMultiConfig.prodSckId [1]  = (CyU3PDmaSocketId_t)PONG_PRODUCER_SOCKET;
	dmaMultiConfig.consSckId [0]  = (CyU3PDmaSocketId_t)CONSUMER_USB_SOCKET;
	dmaMultiConfig.dmaMode        = CY_U3P_DMA_MODE_BYTE;
	//     dmaMultiConfig.notification   = CY_U3P_DMA_CB_CONS_EVENT;

    /* Create a DMA AUTO channel for P2U transfer. */
	dmaMultiConfig.notification   = CY_U3P_DMA_CB_PROD_EVENT;
	dmaMultiConfig.cb = (CyU3PDmaMultiCallback_t) DmaCallback;
	Status = CyU3PDmaMultiChannelCreate (&glMultiChHandleSlFifoPtoU,
		   CY_U3P_DMA_TYPE_AUTO_MANY_TO_ONE, &dmaMultiConfig);
	CheckStatus("CyU3PDmaMultiChannelCreate", Status);
    Status = CyU3PDmaMultiChannelSetXfer (&glMultiChHandleSlFifoPtoU,
    	   FIFO_DMA_RX_SIZE,0);  /* DMA transfer size is set to infinite */
    CheckStatus("CyU3PDmaMultiChannelSetXfer", Status);

    /* callback to see if there is any overflow of data on the GPIF II side*/
  //  CyU3PPibRegisterCallback(Pib_error_cb,CYU3P_PIB_INTR_ERROR);

	// Load, configure and start the GPIF state machine
    Status = StartGPIF();
	CheckStatus("GpifStart", Status);
    glIsApplnActive = CyTrue;
}



/* This function stops the slave FIFO loop application. This shall be called
 * whenever a RESET or DISCONNECT event is received from the USB host. The
 * endpoints are disabled and the DMA pipe is destroyed by this function. */
void StopApplication ( void )
{
    CyU3PEpConfig_t epConfig;
    CyU3PReturnStatus_t Status;

    // Disable GPIF, close the DMA channel, flush and disable the endpoint
    CyU3PGpifDisable(CyTrue);
    Status = CyU3PPibDeInit();
    CheckStatus("Stop GPIF Block", Status);
    Status = CyU3PDmaMultiChannelDestroy (&glMultiChHandleSlFifoPtoU);
    CheckStatus("DmaMultiChannelDestroy", Status);

    Status = CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);
    CheckStatus("FlushEndpoint", Status);
    CyU3PMemSet((uint8_t *)&epConfig, 0, sizeof(&epConfig));
    Status = CyU3PSetEpConfig(CY_FX_EP_CONSUMER, &epConfig);
	CheckStatus("SetEndpointConfig_Disable", Status);
    // OK, Application is now stopped
	glIsApplnActive  = CyFalse;
}

