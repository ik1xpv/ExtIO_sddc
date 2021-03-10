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
 */

#include "Application.h"

#include "tuner_r82xx.h"
#include "adf4351.h"

// Declare external functions
extern void CheckStatus(char* StringPtr, CyU3PReturnStatus_t Status);
extern void StartApplication(void);
extern void StopApplication(void);
extern CyU3PReturnStatus_t SetUSBdescriptors(void);
extern void WriteAttenuator(uint8_t value);

void si5351aSetFrequencyA(UINT32 freq);
void si5351aSetFrequencyB(UINT32 freq2);

// Declare external data
extern CyU3PQueue EventAvailable;			  	// Used for threads communications
extern CyBool_t glIsApplnActive;				// Set true once device is enumerated
extern uint8_t  HWconfig;       			    // Hardware config
extern uint16_t  FWconfig;       			    // Firmware config hb.lb

// r820xx data
struct r82xx_priv tuner;
struct r82xx_config tuner_config;

extern int set_all_gains(struct r82xx_priv *priv, UINT8 gain_index);
extern int set_vga_gain(struct r82xx_priv *priv, UINT8 gain_index);
extern uint8_t m_gain_index;



#define CYFX_SDRAPP_MAX_EP0LEN  64      /* Max. data length supported for EP0 requests. */

extern CyU3PDmaMultiChannel glMultiChHandleSlFifoPtoU;
// Global data owned by this module
CyU3PDmaChannel glGPIF2USB_Handle;
uint8_t  *glEp0Buffer = 0;              /* Buffer used to handle vendor specific control requests. */
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
     * This application does not support any class or vendor requests. */

    uint8_t  bRequest, bReqType;
    uint8_t  bType, bTarget;
    uint16_t wValue, wIndex;
    uint16_t wLength;
    CyBool_t isHandled = CyFalse;
    CyU3PReturnStatus_t apiRetStatus;
	
	uint8_t  db[3];  // debug flags
   
	
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

		uint8_t * pd = (uint8_t *) &glEp0Buffer[0];
		db[3] = VENDOR_RQT;
		db[2] = bRequest;
		db[1] = pd[0];
		db[0] = pd[1];
		
    	isHandled = CyFalse;

    	switch (bRequest)
    	 {
			case GPIOFX3:
					if(CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL)== CY_U3P_SUCCESS)
					{
						uint32_t mdata = * (uint32_t *) &glEp0Buffer[0];
						switch(HWconfig)
						{
						case BBRF103:
							bbrf103_GpioSet(mdata);
							isHandled = CyTrue;
						   break;

						case HF103:
							hf103_GpioSet(mdata);
							isHandled = CyTrue;
							break;

						case RX888:
							rx888_GpioSet(mdata);
							isHandled = CyTrue;
							break;

						case RX888r2:
							rx888r2_GpioSet(mdata);
							isHandled = CyTrue;
							break;

						case RX999:
							rx999_GpioSet(mdata);
							isHandled = CyTrue;
							break;
							
						case RXLUCY:
							rxlucy_GpioSet(mdata);
							isHandled = CyTrue;
							break;	
							
						}
					}
					break;

			case STARTADC:
					if(CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL)== CY_U3P_SUCCESS)
					{
						uint32_t freq;
						freq = *(uint32_t *) &glEp0Buffer[0];
						si5351aSetFrequencyA(freq);
						CyU3PThreadSleep(1000);
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

			case R82XXINIT:
					{
						if(CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL)== CY_U3P_SUCCESS)
						{
							uint32_t freq;
							freq = *(uint32_t *) &glEp0Buffer[0];

							memset(&tuner_config, 0, sizeof(tuner_config));
							memset(&tuner, 0, sizeof(tuner));

							tuner_config.vco_curr_min = 0xff;
							tuner_config.vco_curr_max = 0xff;
							tuner_config.vco_algo = 0;

							// detect the hardware
							if (HWconfig == RX888 || HWconfig == BBRF103)
							{
								tuner_config.xtal = freq;
								tuner_config.i2c_addr = R820T_I2C_ADDR;
								tuner_config.rafael_chip = CHIP_R820T;
							}
							else if (HWconfig == RX888r2)
							{
								tuner_config.xtal = freq;
								tuner_config.i2c_addr = R828D_I2C_ADDR;
								tuner_config.rafael_chip = CHIP_R828D;
							}
							si5351aSetFrequencyB(tuner_config.xtal);

							tuner.cfg = &tuner_config;

							uint32_t bw;
							r82xx_init(&tuner);
							r82xx_set_bandwidth(&tuner, 8*1000*1000, 0, &bw, 1);

							vendorRqtCnt++;
							isHandled = CyTrue;
						}
					}
					break;

			case R82XXSTDBY:
					if(CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL)== CY_U3P_SUCCESS)
					{
						r82xx_standby(&tuner);
						si5351aSetFrequencyB(0);
						isHandled = CyTrue;
					}
					break;

			case R82XXTUNE:
					if(CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL)== CY_U3P_SUCCESS)
					{

						uint64_t freq;
						freq = *(uint64_t *) &glEp0Buffer[0];
						r82xx_set_freq64(&tuner, freq);
						// R820T2 tune
						DebugPrint(4, "\r\n\r\nTune R820T2 %d \r\n",freq);
						isHandled = CyTrue;
					}
					break;

			case AD4351TUNE:
					if(CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL)== CY_U3P_SUCCESS)
					{

						uint64_t freq;
						freq = *(uint64_t *) &glEp0Buffer[0];
						adf4350_out_altvoltage0_frequency(freq);
						// AD4351 tune
						DebugPrint(4, "\r\n\r\nTune AD4351 %d \r\n",freq);
						isHandled = CyTrue;
					}
					break;

			case SETARGFX3:
				{
					int rc = -1;
					CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL);
					db[3] = VR_ARG;
					db[1] = wIndex;
					db[0] = wValue;
					switch(wIndex) {
						case R82XX_ATTENUATOR:
							rc = set_all_gains(&tuner, wValue); // R820T2 set att
							break;
						case R82XX_VGA:
							rc = set_vga_gain(&tuner, wValue); // R820T2 set vga
							break;
						case R82XX_SIDEBAND:
							rc = r82xx_set_sideband(&tuner, wValue);
							break;
						case R82XX_HARMONIC:
							// todo
							break;
						case DAT31_ATT:
							switch(HWconfig)
							{
								case HF103:
									hf103_SetAttenuator(wValue);
									rc = 0;
									break;
								case RX888r2:
									rx888r2_SetAttenuator(wValue);
									rc = 0;
									break;
								case RX999:
									rx999_SetAttenuator(wValue);
									rc = 0;
									break;
								case RXLUCY:
									rxlucy_SetAttenuator(wValue);
									rc = 0;
								        break;
							}
							break;
						case AD8340_VGA:
							switch(HWconfig)
							{
								case RX888r2:
									rx888r2_SetGain(wValue);
									rc = 0;
									break;
								case RX999:
									rx999_SetGain(wValue);
									rc = 0;
									break;
							}
							break;
						case PRESELECTOR:
							switch(HWconfig)
							{
								case RX999:
									rx999_preselect(wValue);
									rc = 0;
									break;
							}
							break;
						case VHF_ATTENUATOR:
							switch(HWconfig)
							{
								case RXLUCY:
									rxlucy_VHFAttenuator(wValue);
									rc = 0;
									break;
							}
							break;	
					}
					vendorRqtCnt++;
					if (rc == 0)
						isHandled = CyTrue;
				}
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
    //	    uint8_t * pd = (uint8_t *) &glEp0Buffer[0];
    //	uint32_t event =( (VENDOR_RQT<<24) | ( bRequest<<16) | (pd[1]<<8) | pd[0] );
		uint32_t event = *(uint32_t *) db;
    	CyU3PQueueSend(&EventAvailable, &event, CYU3P_NO_WAIT);
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
