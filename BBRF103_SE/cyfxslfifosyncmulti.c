/* This file illustrates the Slave FIFO Synchronous mode example */

/*
   This example comprises of two USB bulk endpoints. A bulk OUT endpoint acts as the
   producer of data from the host. A bulk IN endpoint acts as the consumer of data to
   the host. Appropriate vendor class USB enumeration descriptors with these two bulk
   endpoints are implemented.

   The GPIF configuration data for the Synchronous Slave FIFO operation is loaded onto
   the appropriate GPIF registers. The p-port data transfers are done via the producer
   p-port socket and the consumer p-port socket.

   This example implements two DMA Channels either in MANUAL mode or AUTO mode (depending
   on #define MANUAL in cyfxslfifosync.h) one for P to U data transfer and one for U to P
   data transfer.

   The U to P DMA channel connects the USB producer (OUT) endpoint to the consumer p-port
   socket. And the P to U DMA channel connects the producer p-port socket to the USB 
   consumer (IN) endpoint.

   In case of MANUAL DMA channel, Upon every reception of data in the DMA buffer from the
   host or from the p-port, the CPU is signaled using DMA callbacks. There are two DMA callback
   functions implemented each for U to P and P to U data paths. The CPU then commits the DMA buffer received so
   that the data is transferred to the consumer.

   In case of AUTO DMA channel, the data will be transferred between U and P ports automatically
   without any intervention from the CPU.

   The DMA buffer size is configured differently based on the usage. #define LOOPBACK_SHRT_ZLP in cyfxslfifosync.h
   configures the DMA buffers for doing data loopback. Also, DMA buffer for each channel is defined based on the
   USB speed. 64 bytes for full speed, 512 bytes for high speed and 1024 bytes for super speed.
   #define STREAM_IN_OUT in cyfxslfifosync.h configures the DMA buffers for higher bandwidth data transfers.
   16*64 bytes for full speed, 16*512 bytes for high speed and 16*1024 bytes for super speed.

   CY_FX_SLFIFO_DMA_BUF_COUNT_P_2_U in cyfxslfifosync.h defines the number of DMA buffers allocated for P to U data path.
   CY_FX_SLFIFO_DMA_BUF_COUNT_U_2_P in cyfxslfifosync.h defines the number of DMA buffers allocated for U to P data path.

   The constant CY_FX_SLFIFO_GPIF_16_32BIT_CONF_SELECT in the header file is used to
   select 16bit or 32bit GPIF data bus configuration.
 */

#include "cyu3system.h"
#include "cyu3os.h"
#include "cyu3dma.h"
#include "cyu3error.h"
#include "cyu3uart.h"
#include "cyfxslfifosyncmulti.h"
#include "cyu3gpif.h"
#include "cyu3pib.h"
#include "pib_regs.h"
#include "auxutil.h"
#include "i2cmodule.h"
#include <cyu3gpio.h>
#include <stdio.h>

//	#include "cyfxgpif2configmulti.h"
#include "bbrf103.h"

// #define MANUAL

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;

CyU3PThread slFifoAppThread;	        /* Slave FIFO application thread structure */
CyU3PDmaMultiChannel glMultiChHandleSlFifoPtoU;   /* DMA Channel handle for P2U transfer. */

CyU3PEvent glAppEvent;                  /* Event group used to defer handling of vendor specific control requests. */
uint8_t  *glEp0Buffer = 0;              /* Buffer used to handle vendor specific control requests. */
#define CYFX_SDRAPP_MAX_EP0LEN  64      /* Max. data length supported for EP0 requests. */

#define ResetDevice() do { *(volatile uint32_t *)0xe0050000 &= ~(1<<31); } while(1)

uint32_t glDMARxCount = 0;               /* Counter to track the number of buffers received from USB. */
uint32_t glDMATxCount = 0;               /* Counter to track the number of buffers sent to USB. */
CyBool_t glIsApplnActive = CyFalse;      /* Whether the loopback application is active or not. */
uint8_t burstLength =0;
uint8_t  vendorRqtCnt = 0;
uint8_t  RT820T2alive = 0;

/* This function starts the slave FIFO loop application. This is called
 * when a SET_CONF event is received from the USB host. The endpoints
 * are configured and the DMA pipe is setup in this function. */
void CyFxSlFifoApplnStart (  void) {
    uint16_t size = 0;
    CyU3PEpConfig_t epCfg;

    CyU3PDmaMultiChannelConfig_t dmaMultiConfig;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    CyU3PUSBSpeed_t usbSpeed = CyU3PUsbGetSpeed();

    CyU3PDebugPrint (4, "Application Start\n");

     /* Based on the Bus Speed configure the endpoint packet size */
    switch (usbSpeed) {
        case CY_U3P_FULL_SPEED:
            size = 64;
            break;
        case CY_U3P_HIGH_SPEED:
            size = 512;
            burstLength=1;
            break;
        case  CY_U3P_SUPER_SPEED:
            size = 1024;
            burstLength=16;
            break;
        default:
            CyU3PDebugPrint (4, "Error! Invalid USB speed.\n");
            CyFxAppErrorHandler (CY_U3P_ERROR_FAILURE);
            break;
    }

    CyU3PMemSet ((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable = CyTrue;
    epCfg.epType = CY_U3P_USB_EP_BULK;
#ifdef STREAM_IN_OUT
    epCfg.burstLen = burstLength;
#else
    epCfg.burstLen = 1;
#endif
    epCfg.streams = 0;
    epCfg.pcktSize = size;

    /* Producer endpoint configuration */
    apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_PRODUCER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PSetEpConfig failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    epCfg.isoPkts = 1;

    /* Consumer endpoint configuration */
    apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_CONSUMER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PSetEpConfig failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }


    /* UVC Video Streaming Endpoint Packet Size */
    #define CY_FX_EP_BULK_VIDEO_PKT_SIZE    (0x400)         /* 1024 Bytes */
    /* UVC Video Streaming Endpoint Packet Count */
    #define CY_FX_EP_BULK_VIDEO_PKTS_COUNT  (0x10)          /* 16 packets (burst of 16) per DMA buffer. */
    /* DMA buffer size used for video streaming. */
    #define CY_FX_UVC_STREAM_BUF_SIZE      (CY_FX_EP_BULK_VIDEO_PKTS_COUNT * CY_FX_EP_BULK_VIDEO_PKT_SIZE)  /* 16 KB */
    /* Number of DMA buffers per GPIF DMA thread. */
    #define CY_FX_UVC_STREAM_BUF_COUNT     (4)

         dmaMultiConfig.size           = CY_FX_UVC_STREAM_BUF_SIZE;
         dmaMultiConfig.count          = CY_FX_UVC_STREAM_BUF_COUNT;
         dmaMultiConfig.validSckCount  = 2;
         dmaMultiConfig.prodSckId [0]  = (CyU3PDmaSocketId_t)CY_U3P_PIB_SOCKET_0;
         dmaMultiConfig.prodSckId [1]  = (CyU3PDmaSocketId_t)CY_U3P_PIB_SOCKET_1;
         dmaMultiConfig.consSckId [0]  = (CyU3PDmaSocketId_t)CY_FX_CONSUMER_USB_SOCKET;
		 dmaMultiConfig.prodAvailCount = 0;
         //     dmaMultiConfig.prodHeader     = 12;                 /* 12 byte UVC header to be added. */
         //     dmaMultiConfig.prodFooter     = 4;                  /* 4 byte footer to compensate for the 12 byte header. */
		 dmaMultiConfig.prodHeader     = 0;                 /* 12 byte UVC header to be added. */
		 dmaMultiConfig.prodFooter     = 0;                  /* 4 byte footer to compensate for the 12 byte header. */
         dmaMultiConfig.consHeader     = 0;
         dmaMultiConfig.dmaMode        = CY_U3P_DMA_MODE_BYTE;
         //     dmaMultiConfig.notification   = CY_U3P_DMA_CB_CONS_EVENT;

#ifdef MANUAL


	     dmaMultiConfig.notification   = CY_U3P_DMA_CB_PROD_EVENT;
         dmaMultiConfig.cb             = CyFxSlFifoPtoUDmaCallback;

         apiRetStatus = CyU3PDmaMultiChannelCreate (&glMultiChHandleSlFifoPtoU, CY_U3P_DMA_TYPE_MANUAL_MANY_TO_ONE,
                      &dmaMultiConfig);

     if (apiRetStatus != CY_U3P_SUCCESS)
     {
         /* Error handling */
         CyU3PDebugPrint (4, "DMA Channel Creation Failed, Error Code = %d\n", apiRetStatus);
         CyFxAppErrorHandler (apiRetStatus);
     }

#else

       /* Create a DMA AUTO channel for P2U transfer. */
//     dmaMultiConfig.size  = DMA_BUF_SIZE*size; //increase buffer size for higher performance
//     dmaMultiConfig.count = CY_FX_SLFIFO_DMA_BUF_COUNT_P_2_U; // increase buffer count for higher performance
 //    dmaMultiConfig.prodSckId = CY_FX_PRODUCER_PPORT_SOCKET;
 //    dmaMultiConfig.consSckId = CY_FX_CONSUMER_USB_SOCKET;

	dmaMultiConfig.notification   = CY_U3P_DMA_CB_PROD_EVENT;
	dmaMultiConfig.cb = NULL;
	apiRetStatus = CyU3PDmaMultiChannelCreate (&glMultiChHandleSlFifoPtoU,
		   CY_U3P_DMA_TYPE_AUTO_MANY_TO_ONE, &dmaMultiConfig);

    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PDmaMultiChannelCreate failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

#endif

    /* Flush the Endpoint memory */
    CyU3PUsbFlushEp(CY_FX_EP_PRODUCER);
    CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);

    CyU3PGpifControlSWInput ( CyFalse   );
    apiRetStatus = CyU3PGpifSMStart (RESET, ALPHA_RESET);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PGpifSMStart failed, Error Code = %d\n",apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }
 	CyU3PGpifControlSWInput ( CyFalse   );

    apiRetStatus = CyU3PDmaMultiChannelSetXfer (&glMultiChHandleSlFifoPtoU, CY_FX_SLFIFO_DMA_RX_SIZE,0);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PDmaMultiChannelSetXfer Failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Update the status flag. */
    glIsApplnActive = CyTrue;
    CyU3PGpioSetValue (21, CyFalse);
    CyU3PDebugPrint (4, "TRACE glIsApplnActive = CyTrue\n");

}

/* This function stops the slave FIFO loop application. This shall be called
 * whenever a RESET or DISCONNECT event is received from the USB host. The
 * endpoints are disabled and the DMA pipe is destroyed by this function. */
void
CyFxSlFifoApplnStop (
        void)
{
    CyU3PEpConfig_t epCfg;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

  //  CyU3PDebugPrint (4, "Application Stop\n");
    /* Update the flag. */
    glIsApplnActive = CyFalse;
    CyU3PGpioSetValue (21, CyTrue);
    CyU3PDebugPrint (4, "TRACE glIsApplnActive = CyFalse\n");

    /* Flush the endpoint memory */
    CyU3PUsbFlushEp(CY_FX_EP_PRODUCER);
    CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);

    /* Destroy the channel */
//    CyU3PDmaChannelDestroy (&glChHandleSlFifoUtoP);
    CyU3PDmaMultiChannelDestroy (&glMultiChHandleSlFifoPtoU);

    /* Disable endpoints. */
    CyU3PMemSet ((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable = CyFalse;


    /* Producer endpoint configuration. */
    apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_PRODUCER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PSetEpConfig failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Consumer endpoint configuration. */
    apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_CONSUMER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PSetEpConfig failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }
}

/* Callback to handle the USB setup requests. */
CyBool_t
CyFxSlFifoApplnUSBSetupCB (
        uint32_t setupdat0,
        uint32_t setupdat1
    )
{
//	CyU3PDebugPrint (4, "USBSetupCB\n");
    /* Fast enumeration is used. Only requests addressed to the interface, class,
     * vendor and unknown control requests are received by this function.
     * This application does not support any class or vendor requests. */

    uint8_t  bRequest, bReqType;
    uint8_t  bType, bTarget;
    uint16_t wValue, wIndex;
    uint16_t wLength;
    CyBool_t isHandled = CyFalse;
    CyU3PReturnStatus_t apiRetStatus;


    /* Decode the fields from the setup request. */
    bReqType = (setupdat0 & CY_U3P_USB_REQUEST_TYPE_MASK);
    bType    = (bReqType & CY_U3P_USB_TYPE_MASK);
    bTarget  = (bReqType & CY_U3P_USB_TARGET_MASK);
    bRequest = ((setupdat0 & CY_U3P_USB_REQUEST_MASK) >> CY_U3P_USB_REQUEST_POS);
    wValue   = ((setupdat0 & CY_U3P_USB_VALUE_MASK)   >> CY_U3P_USB_VALUE_POS);
    wIndex   = ((setupdat1 & CY_U3P_USB_INDEX_MASK)   >> CY_U3P_USB_INDEX_POS);
    wLength   = ((setupdat1 & CY_U3P_USB_LENGTH_MASK)   >> CY_U3P_USB_LENGTH_POS);

    if (bType == CY_U3P_USB_STANDARD_RQT)
    {
        /* Handle SET_FEATURE(FUNCTION_SUSPEND) and CLEAR_FEATURE(FUNCTION_SUSPEND)
         * requests here. It should be allowed to pass if the device is in configured
         * state and failed otherwise. */
        if ((bTarget == CY_U3P_USB_TARGET_INTF) && ((bRequest == CY_U3P_USB_SC_SET_FEATURE)
                    || (bRequest == CY_U3P_USB_SC_CLEAR_FEATURE)) && (wValue == 0))
        {
            if (glIsApplnActive)
                CyU3PUsbAckSetup ();
            else
                CyU3PUsbStall (0, CyTrue, CyFalse);

            isHandled = CyTrue;
        }

        /* CLEAR_FEATURE request for endpoint is always passed to the setup callback
         * regardless of the enumeration model used. When a clear feature is received,
         * the previous transfer has to be flushed and cleaned up. This is done at the
         * protocol level. So flush the EP memory and reset the DMA channel associated
         * with it. If there are more than one EP associated with the channel reset both
         * the EPs. The endpoint stall and toggle / sequence number is also expected to be
         * reset. Return CyFalse to make the library clear the stall and reset the endpoint
         * toggle. Or invoke the CyU3PUsbStall (ep, CyFalse, CyTrue) and return CyTrue.
         * Here we are clearing the stall. */
        if ((bTarget == CY_U3P_USB_TARGET_ENDPT) && (bRequest == CY_U3P_USB_SC_CLEAR_FEATURE)
                && (wValue == CY_U3P_USBX_FS_EP_HALT))
        {
            if (glIsApplnActive)
            {
                if (wIndex == CY_FX_EP_PRODUCER)
                {
//                    CyU3PDmaChannelReset (&glChHandleSlFifoUtoP);
//                    CyU3PUsbFlushEp(CY_FX_EP_PRODUCER);
//                    CyU3PUsbResetEp (CY_FX_EP_PRODUCER);
//                    CyU3PDmaChannelSetXfer (&glChHandleSlFifoUtoP, CY_FX_SLFIFO_DMA_TX_SIZE);
                }

                if (wIndex == CY_FX_EP_CONSUMER)
                {
                    CyU3PDmaMultiChannelReset (&glMultiChHandleSlFifoPtoU);
                    CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);
                    CyU3PUsbResetEp (CY_FX_EP_CONSUMER);
                    CyU3PDmaMultiChannelSetXfer (&glMultiChHandleSlFifoPtoU, CY_FX_SLFIFO_DMA_RX_SIZE,0);
                    CyU3PDebugPrint (4, "CyU3PDmaMultiChannelSetXfer \n" );
                }
                CyU3PUsbStall (wIndex, CyFalse, CyTrue);
                isHandled = CyTrue;
            }
        }
    } else if (bType == CY_U3P_USB_VENDOR_RQT) {
   // 	uint16_t u16 = (uint16_t) bRequest;
    	outxio_t * pdata;

  //  	uint8_t * pd = (uint8_t *) &glEp0Buffer[0];
 //   	CyU3PDebugPrint (4, "V.REQ. %x:%x:%x:%x - %x %x %x %x %x %x %x %x \n",u16, wValue, wIndex, wLength, pd[0], pd[1], pd[2], pd[3], pd[4], pd[5], pd[6], pd[7]);
  //  	CyU3PThreadSleep(10);
    	isHandled = CyFalse;

    	switch (bRequest)
    	 {
			case CY_FX_RQT_GPIO_WRITE:
					if(CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL)== CY_U3P_SUCCESS)
					{
						pdata = (outxio_t *) &glEp0Buffer[0];
						CyU3PGpioSetValue (21, (pdata->buffer[0] & OUTXIO0) == OUTXIO0 );
						CyU3PGpioSetValue (22, (pdata->buffer[0] & OUTXIO1) == OUTXIO1 );
						CyU3PGpioSetValue (23, (pdata->buffer[0] & OUTXIO2) == OUTXIO2 );
						CyU3PGpioSetValue (26, (pdata->buffer[0] & OUTXIO3) == OUTXIO3 );
						CyU3PGpioSetValue (27, (pdata->buffer[0] & OUTXIO4) == OUTXIO4 );
						CyU3PGpioSetValue (28, (pdata->buffer[0] & OUTXIO5) == OUTXIO5 );
						CyU3PGpioSetValue (29, (pdata->buffer[0] & OUTXIO6) == OUTXIO6 );
						CyU3PGpioSetValue (20, (pdata->buffer[0] & OUTXIO7) == OUTXIO7 );
						isHandled = CyTrue;
					}
					break;
			case CY_FX_RQT_I2C_WRITE:
					apiRetStatus  = CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL);
					if (apiRetStatus == CY_U3P_SUCCESS)
						{
							apiRetStatus = CyFxUsbI2cTransfer ( wIndex, wValue, wLength, glEp0Buffer, CyFalse);
							if (apiRetStatus == CY_U3P_SUCCESS)
								isHandled = CyTrue;
							else
							{
								CyU3PDebugPrint (4, "I2cwrite Error %d\n", apiRetStatus);
								isHandled = CyTrue; // ?!?!?!
							}
						}
					break;

			case CY_FX_RQT_I2C_READ:
					CyU3PMemSet (glEp0Buffer, 0, sizeof (glEp0Buffer));
					apiRetStatus = CyFxUsbI2cTransfer (wIndex, wValue, wLength,glEp0Buffer, CyTrue);
					if (apiRetStatus == CY_U3P_SUCCESS)
					{
						apiRetStatus = CyU3PUsbSendEP0Data(wLength, glEp0Buffer);
						isHandled = CyTrue;
					}
					break;

    	 	case 0xAA:
    	 		    CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL);
    	 		    CyU3PGpifControlSWInput ( CyFalse );
    	 		 	CyU3PDmaMultiChannelReset (&glMultiChHandleSlFifoPtoU);  // Reset existing channel
					apiRetStatus = CyU3PDmaMultiChannelSetXfer (&glMultiChHandleSlFifoPtoU, CY_FX_SLFIFO_DMA_RX_SIZE,0); //start
					if (apiRetStatus == CY_U3P_SUCCESS)
					{
						isHandled = CyTrue;

					}
					CyU3PGpifControlSWInput ( CyTrue );
					break;

			case 0xAB:
				    CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL);
					CyU3PGpifControlSWInput ( CyFalse   );
					CyU3PThreadSleep(10);
					CyU3PDmaMultiChannelReset (&glMultiChHandleSlFifoPtoU);
					CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);
					isHandled = CyTrue;
					break;

			case 0xCC:
				    CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL);
					CyU3PDebugPrint (4, "RESET DEVICE\n");
					CyU3PGpifControlSWInput ( CyFalse   );
					CyU3PDmaMultiChannelReset (&glMultiChHandleSlFifoPtoU);
					CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);
					CyU3PUsbResetEp (CY_FX_EP_CONSUMER);
					CyU3PConnectState (CyFalse,CyTrue);
					CyU3PDeviceReset(CyFalse);
				//	ResetDevice();
					break;

			case 0xDD:
				    CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL);
					CyU3PDebugPrint (4, "PAUSE GPIF\n");
					CyU3PGpifSMControl (CyTrue); //  pause
					isHandled = CyTrue;
					break;

            case 0xAC:		//TESTFX3 RT820T2 check, loop count
					glEp0Buffer[0] = RT820T2alive; // 0 = ok, 0x4a NOT present
					glEp0Buffer[1] = 0;
					glEp0Buffer[2] = vendorRqtCnt;
					glEp0Buffer[3] = ~vendorRqtCnt;
					CyU3PUsbSendEP0Data (4, glEp0Buffer);
					vendorRqtCnt++;
					isHandled = CyTrue;
					break;


            default: /* unknown request, stall the endpoint. */

             	isHandled = CyFalse;
             	CyU3PDebugPrint (4, "STALL EP0 V.REQ %x\n",bRequest);
                CyU3PUsbStall (0, CyTrue, CyFalse);
                break;
    	}
    }
    return isHandled;
}

/* This is the callback function to handle the USB events. */
void CyFxSlFifoApplnUSBEventCB (
    CyU3PUsbEventType_t evtype,
    uint16_t            evdata
    ) {
//	CyU3PDebugPrint (4, "USB EVENT CB %x\n",evtype);
    switch (evtype)
    {
        case CY_U3P_USB_EVENT_SETCONF:
            /* Stop the application before re-starting. */
            if (glIsApplnActive) CyFxSlFifoApplnStop ();
            CyU3PUsbLPMDisable();
            /* Start the loop back function. */
            CyFxSlFifoApplnStart ();
            break;
        case CY_U3P_USB_EVENT_RESET:
        case CY_U3P_USB_EVENT_DISCONNECT:
            /* Stop the loop back function. */
//        	CyU3PDeviceReset(CyFalse);
            if (glIsApplnActive) CyFxSlFifoApplnStop ();
            CyU3PDebugPrint (4, "USB EVENT CB DISCONNECT %x\n",evtype);

            break;
        default:
            break;
    }
}

unsigned int count = 0;
void gpif_error_cb(CyU3PPibIntrType cbType, uint16_t cbArg) {
count++;
if( count%256 != 0) return;

if(cbType==CYU3P_PIB_INTR_ERROR)
{
    switch (CYU3P_GET_PIB_ERROR_TYPE(cbArg))
    {
        case CYU3P_PIB_ERR_THR0_WR_OVERRUN:
        CyU3PDebugPrint (4, "CYU3P_PIB_ERR_THR0_WR_OVERRUN %d\n",count);
        break;
        case CYU3P_PIB_ERR_THR1_WR_OVERRUN:
        CyU3PDebugPrint (4, "CYU3P_PIB_ERR_THR1_WR_OVERRUN %d\n",count);
        break;
        case CYU3P_PIB_ERR_THR2_WR_OVERRUN:
        CyU3PDebugPrint (4, "CYU3P_PIB_ERR_THR2_WR_OVERRUN\n");
        break;
        case CYU3P_PIB_ERR_THR3_WR_OVERRUN:
        CyU3PDebugPrint (4, "CYU3P_PIB_ERR_THR3_WR_OVERRUN\n");
        break;

        case CYU3P_PIB_ERR_THR0_RD_UNDERRUN:
        CyU3PDebugPrint (4, "CYU3P_PIB_ERR_THR0_RD_UNDERRUN\n");
        break;
        case CYU3P_PIB_ERR_THR1_RD_UNDERRUN:
        CyU3PDebugPrint (4, "CYU3P_PIB_ERR_THR1_RD_UNDERRUN\n");
        break;
        case CYU3P_PIB_ERR_THR2_RD_UNDERRUN:
        CyU3PDebugPrint (4, "CYU3P_PIB_ERR_THR2_RD_UNDERRUN\n");
        break;
        case CYU3P_PIB_ERR_THR3_RD_UNDERRUN:
        CyU3PDebugPrint (4, "CYU3P_PIB_ERR_THR3_RD_UNDERRUN\n");
        break;

        default:
        CyU3PDebugPrint (4, "PIB No Error :%d\n  %d\n",CYU3P_GET_PIB_ERROR_TYPE(cbArg),count);
            break;
    }
    CyU3PThreadSleep(10);
}
}


CyU3PReturnStatus_t
ConfGPIOsimpleout( uint8_t gpioid)
{
	 CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
	 CyU3PGpioSimpleConfig_t gpioConfig;

	  apiRetStatus = CyU3PDeviceGpioOverride (gpioid, CyTrue);
	    if (apiRetStatus != 0)
	    {
	        /* Error Handling */
	        SDR_ErrorHandler (apiRetStatus,65);
	    }
	    /* Configure GPIO gpioid as output */
	      gpioConfig.outValue = CyFalse;
	      gpioConfig.driveLowEn = CyTrue;
	      gpioConfig.driveHighEn = CyTrue;
	      gpioConfig.inputEn = CyFalse;
	      gpioConfig.intrMode = CY_U3P_GPIO_NO_INTR;
	      apiRetStatus = CyU3PGpioSetSimpleConfig(gpioid , &gpioConfig);
	      if (apiRetStatus != CY_U3P_SUCCESS)
	      {
	          /* Error handling */
	          SDR_ErrorHandler (apiRetStatus,66);
	      }
	 return apiRetStatus;
}
/* Init GPIOs  */
void
SDR_GpioInit (void)
{
    CyU3PGpioClock_t gpioClock;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    /* Init the GPIO module */
    gpioClock.fastClkDiv = 2;
    gpioClock.slowClkDiv = 0;
    gpioClock.simpleDiv = CY_U3P_GPIO_SIMPLE_DIV_BY_2;
    gpioClock.clkSrc = CY_U3P_SYS_CLK;
    gpioClock.halfDiv = 0;

    apiRetStatus = CyU3PGpioInit(&gpioClock, NULL);
    if (apiRetStatus != 0)
    {
        /* Error Handling */
        //CyU3PDebugPrint (4, "CyU3PGpioInit failed, error code = %d\n", apiRetStatus);
        SDR_ErrorHandler (apiRetStatus,67);
    }

    // Configure GPIO  as PWM output
#ifdef _PWMACTIVE_
    ConfGPIOPWM(24,CY_FX_PWM_25P_THRESHOLD);
#endif
    ConfGPIOsimpleout(21);
    ConfGPIOsimpleout(22);
    ConfGPIOsimpleout(23);
    ConfGPIOsimpleout(26);
    ConfGPIOsimpleout(27);
    ConfGPIOsimpleout(28);
    ConfGPIOsimpleout(29);
    ConfGPIOsimpleout(20);

    CyU3PGpioSetValue (21, 0);
    CyU3PGpioSetValue (22, 1);
    CyU3PGpioSetValue (28, 1);
}

/* This function initializes the GPIF interface and initializes
 * the USB interface. */
void CyFxSlFifoApplnInit (void) {

    CyU3PPibClock_t pibClock;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

	CyU3PDebugPrint (4, "CyFxSlFifoApplnInit()\n");
    /* Initialize the p-port block. */
    pibClock.clkDiv = 2;
    pibClock.clkSrc = CY_U3P_SYS_CLK;
    pibClock.isHalfDiv = CyFalse;
    /* Disable DLL for sync GPIF */
    pibClock.isDllEnable = CyFalse;
    apiRetStatus = CyU3PPibInit(CyTrue, &pibClock);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "P-port Initialization failed, Error Code = %d\n",apiRetStatus);
        SDR_ErrorHandler(apiRetStatus,73);
    }

    /* Load the GPIF configuration for Slave FIFO sync mode. */
    apiRetStatus = CyU3PGpifLoad (&CyFxGpifConfig);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PGpifLoad failed, Error Code = %d\n",apiRetStatus);
        SDR_ErrorHandler(apiRetStatus,74);
    }

    CyU3PGpifSocketConfigure (0,CY_U3P_PIB_SOCKET_0,6,CyFalse,1);
	CyU3PGpifSocketConfigure (1,CY_U3P_PIB_SOCKET_1,6,CyFalse,1);

	SDR_GpioInit();


    apiRetStatus = CyFxI2cInit ();
	if (apiRetStatus != CY_U3P_SUCCESS)
		 {
		   // Error handling
		   CyU3PDebugPrint (4, "\n\r Error CyFxI2cInit() = 0x%X\n",apiRetStatus);
		 }

    /* Start the USB functionality. */
    apiRetStatus = CyU3PUsbStart();
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PUsbStart failed to Start, Error code = %d\n", apiRetStatus);
        SDR_ErrorHandler(apiRetStatus,75);
    }
    /* callback to see if there is any overflow of data on the GPIF II side*/
    CyU3PPibRegisterCallback(gpif_error_cb,0xffff);

    /* The fast enumeration is the easiest way to setup a USB connection,
     * where all enumeration phase is handled by the library. Only the
     * class / vendor requests need to be handled by the application. */
    CyU3PUsbRegisterSetupCallback(CyFxSlFifoApplnUSBSetupCB, CyTrue);

    /* Setup the callback to handle the USB events. */
    CyU3PUsbRegisterEventCallback(CyFxSlFifoApplnUSBEventCB);

    /* Register a callback to handle LPM requests from the USB 3.0 host. */
    CyU3PUsbRegisterLPMRequestCallback(CyFxApplnLPMRqtCB);    

    setDescriptors();

    /* Connect the USB Pins with super speed operation enabled. */
    apiRetStatus = CyU3PConnectState(CyTrue, CyTrue);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB Connect failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }
	apiRetStatus = CyU3PUsbLPMDisable();
	if (apiRetStatus != CY_U3P_SUCCESS)
	{
		CyU3PDebugPrint (4, "CyU3PUsbLPMDisable, Error code = %d\n", apiRetStatus);
		SDR_ErrorHandler(apiRetStatus,61);
	}

// init I2c staff

// prog xtal 64 M
	uint8_t tmp = 0x52;
	apiRetStatus = CyFxUsbI2cTransfer ( 0xB7 , 0xC0, 1, &tmp, CyFalse);
	tmp = 0x80;
	apiRetStatus += CyFxUsbI2cTransfer ( 0x10 , 0xC0, 1, &tmp, CyFalse);
	apiRetStatus += CyFxUsbI2cTransfer ( 0x11 , 0xC0, 1, &tmp, CyFalse);
	apiRetStatus += CyFxUsbI2cTransfer ( 0x12 , 0xC0, 1, &tmp, CyFalse);

	uint8_t dat[8]= {0xFF, 0xFF,  0x00,  0x0E,  0x97,  0xF6,  0x98,  0x97  };
	apiRetStatus += CyFxUsbI2cTransfer ( 0x1A , 0xC0, 8, dat, CyFalse);
	uint8_t dat2[8]= {0x00, 0x01,  0x00,  0x05,  0x00,  0x00,  0x00,  0x00  };
	apiRetStatus += CyFxUsbI2cTransfer ( 0x2A , 0xC0, 8, dat2, CyFalse);
	tmp = 0x20;
	apiRetStatus += CyFxUsbI2cTransfer ( 0x77 , 0xC0, 1, &tmp, CyFalse);
	tmp = 0x4F;
	apiRetStatus += CyFxUsbI2cTransfer ( 0x10 , 0xC0, 1, &tmp, CyFalse);
	if (apiRetStatus != CY_U3P_SUCCESS)
	{
		CyU3PDebugPrint (4, "\n\rCY_FX_RQT_I2C_WRITE failed ");
		SDR_ErrorHandler(apiRetStatus,41);
	}
	tmp = 0;
	RT820T2alive  = CyFxUsbI2cTransfer ( 0x05 , 0x34, 1, &tmp, CyFalse);  // 0 = present , 4a NOT present

	uint8_t dout = 0x16;
	CyU3PGpioSetValue (21, (dout & OUTXIO0) == OUTXIO0 );
	CyU3PGpioSetValue (22, (dout & OUTXIO1) == OUTXIO1 );
	CyU3PGpioSetValue (23, (dout & OUTXIO2) == OUTXIO2 );
	CyU3PGpioSetValue (26, (dout & OUTXIO3) == OUTXIO3 );
	CyU3PGpioSetValue (27, (dout & OUTXIO4) == OUTXIO4 );
	CyU3PGpioSetValue (28, (dout & OUTXIO5) == OUTXIO5 );
	CyU3PGpioSetValue (29, (dout & OUTXIO6) == OUTXIO6 );
	CyU3PGpioSetValue (20, (dout & OUTXIO7) == OUTXIO7 );
}

/* Entry function for the slFifoAppThread. */
unsigned long calls = 1;

void SlFifoAppThread_Entry ( uint32_t input)
{
    CyU3PDmaBuffer_t    buffer_p;
    CyU3PReturnStatus_t apiRetStatus;

    /* Initialize the slave FIFO application */
    CyFxSlFifoApplnInit();

	CyU3PDebugPrint (4, "ThreadEntry start loop\n");
//	SDR_ErrorHandler(NULL,11); //GPIO init
	CyU3PUsbLPMDisable ();

    for (;;) {
    	//        if (glIsApplnActive)
        if (CyFalse)
        {
            /* Print the number of buffers received so far from the USB host.
            CyU3PDebugPrint (6, "Data tracker: buffers received: %d, buffers sent: %d.\n",
                    glDMARxCount, glDMATxCount);*/

        	apiRetStatus = CyU3PDmaMultiChannelGetBuffer (&glMultiChHandleSlFifoPtoU, &buffer_p, CYU3P_NO_WAIT);
            if (apiRetStatus == CY_U3P_SUCCESS)
            {
				if (calls%4096 == 0)
				{
					CyU3PDebugPrint (4, "DMACallback, buffer %u count %u size %u status %u\n",
							buffer_p.buffer,buffer_p.count,buffer_p.size,buffer_p.status);
					CyU3PDebugPrint (4, "DMACallback, buffer %u count %u size %u status %u\n",
						buffer_p.buffer,buffer_p.count,buffer_p.size,buffer_p.status);
					apiRetStatus = CyU3PDmaMultiChannelCommitBuffer (&glMultiChHandleSlFifoPtoU, buffer_p.count, 0);
				}

            }
        }
        calls++;
        /* Allow other ready threads to run. */
        CyU3PThreadRelinquish ();

    }
}

/* Application define function which creates the threads. */
void CyFxApplicationDefine (void) {

	/* Initialize the debug module */
    CyFxSlFifoApplnDebugInit();

	CyU3PDebugPrint (4, "ApplicationDefine\n");
    /* Allocate a buffer for handling control requests. */
    glEp0Buffer = (uint8_t *)CyU3PDmaBufferAlloc (CYFX_SDRAPP_MAX_EP0LEN);

    void *ptr = NULL;
    uint32_t retThrdCreate = CY_U3P_SUCCESS;

    /* Create the event group required to defer vendor specific requests. */
    CyU3PEventCreate (&glAppEvent);

    /* Allocate the memory for the thread */
    ptr = CyU3PMemAlloc (CY_FX_SLFIFO_THREAD_STACK);

    /* Create the thread for the application */
    retThrdCreate = CyU3PThreadCreate (&slFifoAppThread,             /* Slave FIFO app thread structure */
                          "21:BBFR103_fifo_sync",                  /* Thread ID and thread name */
                          SlFifoAppThread_Entry,                   /* Slave FIFO app thread entry function */
                          0,                                       /* No input parameter to thread */
                              ptr,                                     /* Pointer to the allocated thread stack */
                          CY_FX_SLFIFO_THREAD_STACK,               /* App Thread stack size */
                          CY_FX_SLFIFO_THREAD_PRIORITY,            /* App Thread priority */
                          CY_FX_SLFIFO_THREAD_PRIORITY,            /* App Thread pre-emption threshold */
                          CYU3P_NO_TIME_SLICE,                     /* No time slice for the application thread */
                          CYU3P_AUTO_START                         /* Start the thread immediately */
                          );

    /* Check the return code */
    if (retThrdCreate != 0) {
//    if (CyTrue) {
        /* Thread Creation failed with the error code retThrdCreate */
        /* Add custom recovery or debug actions here */
        /* Application cannot continue */
        while(1);
    }
}

/* Main function */
int main (void) {
    CyU3PIoMatrixConfig_t io_cfg;
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;
    CyU3PSysClockConfig_t clkCfg;

	/* setSysClk400 clock configurations */
	clkCfg.setSysClk400 = CyTrue;   /* FX3 device's master clock is set to a frequency > 400 MHz */
	clkCfg.cpuClkDiv = 2;           /* CPU clock divider */
	clkCfg.dmaClkDiv = 2;           /* DMA clock divider */
	clkCfg.mmioClkDiv = 2;          /* MMIO clock divider */
	clkCfg.useStandbyClk = CyFalse; /* device has no 32KHz clock supplied */
	clkCfg.clkSrc = CY_U3P_SYS_CLK; /* Clock source for a peripheral block  */

    /* Initialize the device */
    status = CyU3PDeviceInit (&clkCfg);
    if (status != CY_U3P_SUCCESS)  goto handle_fatal_error;



    /* Initialize the caches. Enable instruction cache and keep data cache disabled. */
    status = CyU3PDeviceCacheControl (CyTrue, CyFalse, CyFalse);
    if (status != CY_U3P_SUCCESS)  goto handle_fatal_error;

    /* Configure the IO matrix for the device. On the FX3 DVK board, the COM port 
     * is connected to the IO(53:56). This means that either DQ32 mode should be
     * selected or lppMode should be set to UART_ONLY. Here we are choosing
     * UART_ONLY configuration for 16 bit slave FIFO configuration and setting
     * isDQ32Bit for 32-bit slave FIFO configuration. */
    io_cfg.useUart   = CyTrue;
    io_cfg.useI2C    = CyTrue;
    io_cfg.useI2S    = CyFalse;
    io_cfg.useSpi    = CyFalse;
#if (CY_FX_SLFIFO_GPIF_16_32BIT_CONF_SELECT == 0)
    io_cfg.isDQ32Bit = CyFalse;
 //   io_cfg.lppMode   = CY_U3P_IO_MATRIX_LPP_DEFAULT;
    io_cfg.lppMode   = CY_U3P_IO_MATRIX_LPP_UART_ONLY;
#else
    io_cfg.isDQ32Bit = CyTrue;
    io_cfg.lppMode   = CY_U3P_IO_MATRIX_LPP_DEFAULT;
#endif
    /* No GPIOs are enabled. */
    io_cfg.gpioSimpleEn[0]  = 0;
//    io_cfg.gpioSimpleEn[1]  = 0x08000000; /* GPIO 59 */
    io_cfg.gpioSimpleEn[1]  = 0;
    io_cfg.gpioComplexEn[0] = 0;
    io_cfg.gpioComplexEn[1] = 0;
    status = CyU3PDeviceConfigureIOMatrix (&io_cfg);
    if (status != CY_U3P_SUCCESS) goto handle_fatal_error;

    /* This is a non returnable call for initializing the RTOS kernel */
    CyU3PKernelEntry ();

    /* Dummy return to make the compiler happy */
    return 0;

handle_fatal_error:
	CyU3PGpioSetValue(GPIO_LED2, LED_OFF);
    /* Cannot recover from this error. */
    while (1);
}
