/*
 * USB_handler.c
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
 *  Date: Sat Nov 14 09:41:32 AM EST 2020
 */

#include "Application.h"

// Declare external functions
extern void CheckStatus(char* StringPtr, CyU3PReturnStatus_t Status);
extern void StartApplication(void);
extern void StopApplication(void);
extern CyU3PReturnStatus_t SetUSBdescriptors(void);
extern void WriteAttenuator(uint8_t value);
extern CyU3PReturnStatus_t Si5351init(UINT32 freqa, UINT32 freqb);



// Declare external data
extern CyU3PQueue EventAvailable;			  	// Used for threads communications
extern CyBool_t glIsApplnActive;				// Set true once device is enumerated
extern uint8_t  HWconfig;       			    // Hardware config
extern uint16_t  FWconfig;       			    // Firmware config hb.lb

extern ReturnStatus_t rt820init(void);
extern void set_all_gains(UINT8 gain_index);
extern void set_freq(UINT32 freq);
extern void rt820shutdown(void);
extern uint8_t m_gain_index;


#define CYFX_SDRAPP_MAX_EP0LEN  64      /* Max. data length supported for EP0 requests. */

/* CDC Class specific requests to be handled by this application. */
#define SET_LINE_CODING        0x20
#define GET_LINE_CODING        0x21
#define SET_CONTROL_LINE_STATE 0x22

extern CyU3PDmaMultiChannel glMultiChHandleSlFifoPtoU;
// Global data owned by this module
CyU3PDmaChannel glGPIF2USB_Handle;
uint8_t  *glEp0Buffer = 0;              /* Buffer used to handle vendor specific control requests. */
CyU3PUartConfig_t glUartConfig = {0};   /* Current UART configuration. */
uint8_t  vendorRqtCnt = 0;

/* Callback to handle the USB setup requests. */
CyBool_t
CyFxSlFifoApplnUSBSetupCB (
        uint32_t setupdat0,
        uint32_t setupdat1
    )
{
    /* Fast enumeration is used. Only requests addressed to the interface, class,
     * vendor and unknown control requests are received by this function.
     * This application only supports class requests for a CDC device. */

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
            {
                CyU3PUsbStall (0, CyTrue, CyFalse);
            }
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
                if (wIndex == CY_FX_EP_CONSUMER)
                {
                    CyU3PDmaMultiChannelReset (&glMultiChHandleSlFifoPtoU);
                    CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);
                    CyU3PUsbResetEp (CY_FX_EP_CONSUMER);
                    CyU3PDmaMultiChannelSetXfer (&glMultiChHandleSlFifoPtoU,FIFO_DMA_RX_SIZE,0);
                }
                CyU3PUsbStall (wIndex, CyFalse, CyTrue);
                isHandled = CyTrue;
            }
        }
    } else if (bType == CY_U3P_USB_VENDOR_RQT) {

    	outxio_t * pdata;
    	/*
   	    uint8_t * pd = (uint8_t *) &glEp0Buffer[0];
    	uint32_t event =( (VENDOR_RQT<<24) | ( bRequest<<16) | (pd[0]<<8) | pd[1] );
    	CyU3PQueueSend(&EventAvailable, &event, CYU3P_NO_WAIT);
  */
    	isHandled = CyFalse;

    	switch (bRequest)
    	 {
			case GPIOWFX3:
					if(CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL)== CY_U3P_SUCCESS)
					{
						uint16_t mdata = * (uint16_t *) &glEp0Buffer[0];
						switch(HWconfig)
						{
						case BBRF103:
							CyU3PGpioSetValue (26, (mdata & ATT_SEL0) == ATT_SEL0 ); // ATT_SEL0
							CyU3PGpioSetValue (27, (mdata & ATT_SEL1) == ATT_SEL1 ); // ATT_SEL1
							CyU3PGpioSetValue (28, (mdata & SHDWN) == SHDWN ); 		 // SHDN
							CyU3PGpioSetValue (29, (mdata & DITH ) == DITH  ); 		 // DITH
							CyU3PGpioSetValue (20, (mdata & RANDO) == RANDO ); 		 // RAND
							CyU3PGpioSetValue (19, ((mdata & BIAS_HF) == BIAS_HF) ^ 1);	  // negate
							CyU3PGpioSetValue (18, ((mdata & BIAS_VHF) == BIAS_VHF) ^ 1); // negate
							CyU3PGpioSetValue (21, (mdata & LED_RED) == LED_RED);
							CyU3PGpioSetValue (22, (mdata & LED_YELLOW) == LED_YELLOW);
							CyU3PGpioSetValue (23, (mdata & LED_BLUE) == LED_BLUE);
							isHandled = CyTrue;
						   break;

						case HF103:
							CyU3PGpioSetValue (21, (mdata & OUTXIO0) == OUTXIO0 ); // ATT_LE
							CyU3PGpioSetValue (22, (mdata & OUTXIO1) == OUTXIO1 ); // ATT_CLK
							CyU3PGpioSetValue (23, (mdata & OUTXIO2) == OUTXIO2 ); // ATT_DATA
							CyU3PGpioSetValue (26, (mdata & OUTXIO3) == OUTXIO3 ); // GPIO26
							CyU3PGpioSetValue (27, (mdata & OUTXIO4) == OUTXIO4 ); // GPIO27
							CyU3PGpioSetValue (18, (mdata & SHDWN) == SHDWN ); 	   // SHDN
							CyU3PGpioSetValue (17, (mdata & DITH ) == DITH ) ;     // DITH
							CyU3PGpioSetValue (29, (mdata & RANDO) == RANDO );     // RAND
							CyU3PGpioSetValue (19, (mdata & OUTXIO8) == OUTXIO8 ); // GPIO19
							CyU3PGpioSetValue (20, (mdata & OUTXIO9) == OUTXIO9 ); // GPIO20
							isHandled = CyTrue;
							break;

						case RX888:
							CyU3PGpioSetValue (26, (mdata & ATT_SEL0) == ATT_SEL0 ); // ATT_SEL0
							CyU3PGpioSetValue (27, (mdata & ATT_SEL1) == ATT_SEL1 ); // ATT_SEL1
							CyU3PGpioSetValue (28, (mdata & SHDWN) == SHDWN ); 		 // SHDN
							CyU3PGpioSetValue (29, (mdata & DITH ) == DITH  ); 		 // DITH
							CyU3PGpioSetValue (20, (mdata & RANDO) == RANDO ); 		 // RAND
							CyU3PGpioSetValue (19, (mdata & BIAS_HF) == BIAS_HF);
							CyU3PGpioSetValue (18, (mdata & BIAS_VHF) == BIAS_VHF);
							CyU3PGpioSetValue (21, (mdata & LED_RED) == LED_RED);
							CyU3PGpioSetValue (22, (mdata & LED_YELLOW) == LED_YELLOW);
							CyU3PGpioSetValue (23, (mdata & LED_BLUE) == LED_BLUE);
							isHandled = CyTrue;
						   break;

						}
					}
					break;

			case DAT31FX3:
					if(CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL)== CY_U3P_SUCCESS)
					{
						pdata = (outxio_t *) &glEp0Buffer[0];
						WriteAttenuator(pdata->buffer[0]);
						isHandled = CyTrue;
					}
					break;

			case SI5351A:
					if(CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL)== CY_U3P_SUCCESS)
					{
						uint32_t fa,fb;
						fa = *(uint32_t *) &glEp0Buffer[0];
						fb = *(uint32_t *) &glEp0Buffer[4];
						Si5351init( fa ,fb );
						isHandled = CyTrue;
					}
					break;


			case I2CWFX3:
					apiRetStatus  = CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL);
					if (apiRetStatus == CY_U3P_SUCCESS)
						{
							apiRetStatus = I2cTransfer ( wIndex, wValue, wLength, glEp0Buffer, CyFalse);
							if (apiRetStatus == CY_U3P_SUCCESS)
								isHandled = CyTrue;
							else
							{
								CyU3PDebugPrint (4, "I2cwrite Error %d\n", apiRetStatus);
								isHandled = CyTrue; // ?!?!?!
							}
						}
					break;

			case I2CRFX3:
					CyU3PMemSet (glEp0Buffer, 0, sizeof (glEp0Buffer));
					apiRetStatus = I2cTransfer (wIndex, wValue, wLength, glEp0Buffer, CyTrue);
					if (apiRetStatus == CY_U3P_SUCCESS)
					{
						apiRetStatus = CyU3PUsbSendEP0Data(wLength, glEp0Buffer);
						isHandled = CyTrue;
					}
					break;

			case R820T2INIT:
					{
						uint16_t dataw = rt820init();
						glEp0Buffer[0] = dataw;
						glEp0Buffer[1] = dataw >> 8;
						glEp0Buffer[2] = 0;
						glEp0Buffer[3] = 0;

						CyU3PUsbSendEP0Data (4, glEp0Buffer);
						vendorRqtCnt++;
						isHandled = CyTrue;
					}
					break;

			case R820T2STDBY:
					if(CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL)== CY_U3P_SUCCESS)
						{
							rt820shutdown();
							isHandled = CyTrue;
						}
					break;


			case R820T2TUNE:
					if(CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL)== CY_U3P_SUCCESS)
					{

						uint32_t freq;
						freq = *(uint32_t *) &glEp0Buffer[0];
						set_freq((uint32_t) freq);
						// R820T2 tune
						DebugPrint(4, "\r\n\r\nTune R820T2 %d \r\n",freq);
						isHandled = CyTrue;
					}
					break;

			case R820T2SETATT:
					if(CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL)== CY_U3P_SUCCESS)
					{
						uint8_t attIdx;
						attIdx = glEp0Buffer[0];
						set_all_gains(attIdx); // R820T2 set att
						isHandled = CyTrue;
					}
					break;

			case R820T2GETATT:
					glEp0Buffer[0] = m_gain_index;
					CyU3PUsbSendEP0Data (1, glEp0Buffer);
					isHandled = CyTrue;
					break;

    	 	case STARTFX3:
    	 		    CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL);
    	 		    CyU3PGpifControlSWInput ( CyFalse );
    	 		 	CyU3PDmaMultiChannelReset (&glMultiChHandleSlFifoPtoU);  // Reset existing channel
					apiRetStatus = CyU3PDmaMultiChannelSetXfer (&glMultiChHandleSlFifoPtoU, FIFO_DMA_RX_SIZE,0); //start
					if (apiRetStatus == CY_U3P_SUCCESS)
					{
						isHandled = CyTrue;
					}
					CyU3PGpifControlSWInput ( CyTrue );
					break;

			case STOPFX3:
				    CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL);
					CyU3PGpifControlSWInput ( CyFalse   );
					CyU3PThreadSleep(10);
					CyU3PDmaMultiChannelReset (&glMultiChHandleSlFifoPtoU);
					CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);
					isHandled = CyTrue;
					break;

			case RESETFX3:	// RESETTING CPU BY PC APPLICATION
				    CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL);
					DebugPrint(4, "\r\n\r\nHOST RESETTING CPU \r\n");
					CyU3PThreadSleep(100);
					CyU3PDeviceReset(CyFalse);
					break;


            case TESTFX3:
					glEp0Buffer[0] =  HWconfig;
					glEp0Buffer[1] = (uint8_t) (FWconfig >> 8);
					glEp0Buffer[2] = (uint8_t) FWconfig;
					glEp0Buffer[3] = vendorRqtCnt;
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
   	    uint8_t * pd = (uint8_t *) &glEp0Buffer[0];
    	uint32_t event =( (VENDOR_RQT<<24) | ( bRequest<<16) | (pd[1]<<8) | pd[0] );
    	CyU3PQueueSend(&EventAvailable, &event, CYU3P_NO_WAIT);
    } else if (bType == CY_U3P_USB_CLASS_RQT) {
	isHandled = CyFalse;
	if (bRequest == SET_LINE_CODING) {
	    if (CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL) == CY_U3P_SUCCESS) {
		CyU3PUartConfig_t uartConfig;
		CyBool_t isValid = CyTrue;
		CyU3PMemSet((uint8_t *) &uartConfig, 0, sizeof(uartConfig));
		uartConfig.baudRate = (CyU3PUartBaudrate_t) (glEp0Buffer[0] |
		    (glEp0Buffer[1] << 8) | (glEp0Buffer[2] << 16) | (glEp0Buffer[3] << 24));
		if (glEp0Buffer[4] == 0) {
		    uartConfig.stopBit = CY_U3P_UART_ONE_STOP_BIT;
		} else if (glEp0Buffer[4] == 2) {
		    uartConfig.stopBit = CY_U3P_UART_TWO_STOP_BIT;
		} else {
		    isValid = CyFalse;
		}
		if (glEp0Buffer[5] == 0) {
		    uartConfig.parity = CY_U3P_UART_NO_PARITY;
		} else if (glEp0Buffer[5] == 1) {
		    uartConfig.parity = CY_U3P_UART_ODD_PARITY;
		} else if (glEp0Buffer[5] == 2) {
		    uartConfig.parity = CY_U3P_UART_EVEN_PARITY;
		} else {
		    isValid = CyFalse;
		}
		uartConfig.txEnable = CyTrue;
		uartConfig.rxEnable = CyTrue;
		uartConfig.flowCtrl = CyFalse;
		uartConfig.isDma = CyTrue;
		if (isValid) {
		    apiRetStatus = CyU3PUartSetConfig(&uartConfig, NULL);
		    if (apiRetStatus == CY_U3P_SUCCESS) {
			CyU3PMemCopy((uint8_t *) &glUartConfig, (uint8_t *) &uartConfig,
			    sizeof (CyU3PUartConfig_t));
			isHandled = CyTrue;
		    }
		}
	    }
	} else if (bRequest == GET_LINE_CODING) {
	    /* get current UART config */
	    glEp0Buffer[0] = (glUartConfig.baudRate & 0x000000ff) >>  0;
	    glEp0Buffer[1] = (glUartConfig.baudRate & 0x0000ff00) >>  8;
	    glEp0Buffer[2] = (glUartConfig.baudRate & 0x00ff0000) >> 16;
	    glEp0Buffer[3] = (glUartConfig.baudRate & 0xff000000) >> 24;
	    if (glUartConfig.stopBit == CY_U3P_UART_ONE_STOP_BIT) {
		glEp0Buffer[4] = 0;
	    } else if (glUartConfig.stopBit == CY_U3P_UART_TWO_STOP_BIT) {
		glEp0Buffer[4] = 2;
	    } else {
		glEp0Buffer[4] = 2;
	    }
	    if (glUartConfig.parity == CY_U3P_UART_NO_PARITY) {
		glEp0Buffer[5] = 0;
	    } else if (glUartConfig.parity == CY_U3P_UART_ODD_PARITY) {
		glEp0Buffer[5] = 1;
	    } else if (glUartConfig.parity == CY_U3P_UART_EVEN_PARITY) {
		glEp0Buffer[5] = 2;
	    } else {
		glEp0Buffer[5] = 0;
	    }
	    glEp0Buffer[6] = 0;
	    CyU3PUsbSendEP0Data(7, glEp0Buffer);
	    isHandled = CyTrue;
	} else if (bRequest == SET_CONTROL_LINE_STATE) {
	    if (glIsApplnActive) {
		CyU3PUsbAckSetup();
		isHandled = CyTrue;
	    } else {
		CyU3PDebugPrint(4, "STALL CDC EP0 V.REQ %x\n", bRequest);
		CyU3PUsbStall(0, CyTrue, CyFalse);
	    }
	} else {
	    /* invalid request */
	    CyU3PDebugPrint(4, "STALL CDC EP0 V.REQ %x\n", bRequest);
	    CyU3PUsbStall(0, CyTrue, CyFalse);
	}
    }
    return isHandled;
}


/* This is the callback function to handle the USB events. */
void USBEvent_Callback ( CyU3PUsbEventType_t evtype, uint16_t evdata)
{
	uint32_t event = evtype;
	CyU3PQueueSend(&EventAvailable, &event, CYU3P_NO_WAIT);
    switch (evtype)
    {
        case CY_U3P_USB_EVENT_SETCONF:
            /* Stop the application before re-starting. */
            if (glIsApplnActive) StopApplication ();
            	StartApplication ();
            break;

        case CY_U3P_USB_EVENT_CONNECT:
        	break;
        case CY_U3P_USB_EVENT_RESET:
        case CY_U3P_USB_EVENT_DISCONNECT:
            if (glIsApplnActive)
            {
            	StopApplication ();
              	CyU3PUsbLPMEnable ();
            }
            break;

        case CY_U3P_USB_EVENT_EP_UNDERRUN:
        	DebugPrint (4, "\r\nEP Underrun on %d", evdata);
            break;

        case CY_U3P_USB_EVENT_EP0_STAT_CPLT:
               /* Make sure the bulk pipe is resumed once the control transfer is done. */
            break;

        default:
            break;
    }
}
/* Callback function to handle LPM requests from the USB 3.0 host. This function is invoked by the API
   whenever a state change from U0 -> U1 or U0 -> U2 happens. If we return CyTrue from this function, the
   FX3 device is retained in the low power state. If we return CyFalse, the FX3 device immediately tries
   to trigger an exit back to U0.
   This application does not have any state in which we should not allow U1/U2 transitions; and therefore
   the function always return CyTrue.
 */
CyBool_t  LPMRequest_Callback ( CyU3PUsbLinkPowerMode link_mode)
{
	DebugPrint (4, "\r\n LPMRequest_Callback link_mode %x \r\n", link_mode );
	return CyTrue;
}



// Spin up USB, let the USB driver handle enumeration
CyU3PReturnStatus_t InitializeUSB(void)
{
	CyU3PReturnStatus_t Status;
	CyBool_t NeedToRenumerate = CyTrue;
    /* Allocate a buffer for handling control requests. */
    glEp0Buffer = (uint8_t *)CyU3PDmaBufferAlloc (CYFX_SDRAPP_MAX_EP0LEN);

	Status = CyU3PUsbStart();

    if (Status == CY_U3P_ERROR_NO_REENUM_REQUIRED)
    {
    	NeedToRenumerate = CyFalse;
    	Status = CY_U3P_SUCCESS;
    	DebugPrint(4,"\r\nNeedToRenumerate = CyFalse");
    }
    CheckStatus("Start USB Driver", Status);

  // Setup callbacks to handle the setup requests, USB Events and LPM Requests (for USB 3.0)
    CyU3PUsbRegisterSetupCallback(CyFxSlFifoApplnUSBSetupCB, CyTrue);
    CyU3PUsbRegisterEventCallback(USBEvent_Callback);
    CyU3PUsbRegisterLPMRequestCallback( LPMRequest_Callback );

    // Driver needs all of the descriptors so it can supply them to the host when requested
    Status = SetUSBdescriptors();
    CheckStatus("Set USB Descriptors", Status);
    // Connect the USB Pins with SuperSpeed operation enabled
    if (NeedToRenumerate)
    {
		  Status = CyU3PConnectState(CyTrue, CyTrue);
		  CheckStatus("ConnectUSB", Status);

    }
    else	// USB connection already exists, restart the Application
    {
    	if (glIsApplnActive) StopApplication();
    	StartApplication();
    }

	return Status;
}
