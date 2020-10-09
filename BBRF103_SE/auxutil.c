#include "auxutil.h"

#include <cyu3types.h>
#include "cyu3system.h"
#include "cyu3os.h"
#include "cyu3dma.h"
#include "cyu3error.h"
#include "cyu3usb.h"
#include "cyu3uart.h"
#include "cyfxslfifosyncmulti.h"
#include "cyu3gpif.h"
#include "cyu3pib.h"
#include "pib_regs.h"
#include "auxutil.h"
#include <cyu3gpio.h>
#include <stdio.h>




void SDR_ErrorHandler( CyU3PReturnStatus_t apiRetStatus, int code );

uint32_t value;

CyU3PReturnStatus_t apiRetStatus = 0;

CyBool_t isHandled = CyFalse;

/* Application Error Handler */
void
CyFxAppErrorHandler (
        CyU3PReturnStatus_t apiRetStatus    /* API return status */
        )
{
    /* Application failed with the error code apiRetStatus */

    /* Add custom debug or recovery actions here */

    /* Loop Indefinitely */
	SDR_ErrorHandler(apiRetStatus,01);
    for (;;)
    {
        /* Thread sleep : 100 ms */
        CyU3PThreadSleep (100);
    }
}

/* Application Error Handler */
void
SDR_ErrorHandler(
        CyU3PReturnStatus_t apiRetStatus,    /* API return status */
        int code
        )
{
    /* Application failed with the error code apiRetStatus */
    CyU3PDebugPrint(4, " apiRetStatus = %d     err code=%d\n", apiRetStatus, code);


    /* Add custom debug or recovery actions here */
	if( code > 99 ) code = 99;

    /* Loop Indefinitely */
    for (;;)
    {
    	int digit1 = code / 10;
    	int digit0 = code % 10;
    	int cc;

    	// marker:
		CyU3PGpioSetValue(GPIO_LED3, LED_ON);
    	CyU3PThreadSleep(ERR_DIGIT_SPACE);
		CyU3PGpioSetValue(GPIO_LED2, LED_ON);
    	CyU3PThreadSleep(ERR_DIGIT_SPACE);
		CyU3PGpioSetValue(GPIO_LED1, LED_ON);
    	CyU3PThreadSleep(ERR_DIGIT_SPACE);
		CyU3PGpioSetValue(GPIO_LED3, LED_OFF);
		CyU3PGpioSetValue(GPIO_LED2, LED_OFF);
		CyU3PGpioSetValue(GPIO_LED1, LED_OFF);
    	CyU3PThreadSleep(ERR_DIGIT_SPACE);

    	// digit1:
    	for(cc=0;cc<digit1;cc++) {
    		CyU3PGpioSetValue(GPIO_LED3, LED_ON);
    		CyU3PGpioSetValue(GPIO_LED2, LED_ON);
    		CyU3PGpioSetValue(GPIO_LED1, LED_ON);
    		CyU3PThreadSleep(ERR_DIGIT_ON);
    		CyU3PGpioSetValue(GPIO_LED3, LED_OFF);
    		CyU3PGpioSetValue(GPIO_LED2, LED_OFF);
    		CyU3PGpioSetValue(GPIO_LED1, LED_OFF);
    		CyU3PThreadSleep(ERR_DIGIT_OFF);
    	}
        //for( ;cc<10;cc++) CyU3PThreadSleep(ERR_DIGIT_ON+ERR_DIGIT_OFF);

    	// space between digit1 and digit0:
        CyU3PThreadSleep(ERR_DIGIT_ON+ERR_DIGIT_OFF);
        CyU3PGpioSetValue(GPIO_LED2, LED_ON);
        CyU3PThreadSleep(ERR_DIGIT_ON);
        CyU3PGpioSetValue(GPIO_LED2, LED_OFF);
        CyU3PThreadSleep(ERR_DIGIT_OFF);

    	// digit0:
    	for(cc=0;cc<digit0;cc++) {
    		CyU3PGpioSetValue(GPIO_LED3, LED_ON);
    		CyU3PGpioSetValue(GPIO_LED2, LED_ON);
    		CyU3PGpioSetValue(GPIO_LED1, LED_ON);
    		CyU3PThreadSleep(ERR_DIGIT_ON);
    		CyU3PGpioSetValue(GPIO_LED3, LED_OFF);
    		CyU3PGpioSetValue(GPIO_LED2, LED_OFF);
    		CyU3PGpioSetValue(GPIO_LED1, LED_OFF);
    		CyU3PThreadSleep(ERR_DIGIT_OFF);
    	}
        //for( ;cc<10;cc++) CyU3PThreadSleep(ERR_DIGIT_ON+ERR_DIGIT_OFF);

    	// space after digit0:
    	CyU3PThreadSleep(ERR_DIGIT_END);
    }
}


/* Callback function to handle LPM requests from the USB 3.0 host. This function is invoked by the API
   whenever a state change from U0 -> U1 or U0 -> U2 happens. If we return CyTrue from this function, the
   FX3 device is retained in the low power state. If we return CyFalse, the FX3 device immediately tries
   to trigger an exit back to U0.

   This application does not have any state in which we should not allow U1/U2 transitions; and therefore
   the function always return CyTrue.
 */
CyBool_t CyFxApplnLPMRqtCB ( CyU3PUsbLinkPowerMode link_mode) {

    return CyTrue;
}

/*  The debug prints can be seen using a UART console
 * running at 115200 baud rate. */
void CyFxSlFifoApplnDebugInit (void) {
    CyU3PUartConfig_t uartConfig;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    /* Initialize the UART for printing debug messages */
    apiRetStatus = CyU3PUartInit();
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        /* Error handling */
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Set UART configuration */
    CyU3PMemSet ((uint8_t *)&uartConfig, 0, sizeof (uartConfig));
    uartConfig.baudRate = CY_U3P_UART_BAUDRATE_115200;
    uartConfig.stopBit = CY_U3P_UART_ONE_STOP_BIT;
    uartConfig.parity = CY_U3P_UART_NO_PARITY;
    uartConfig.txEnable = CyTrue;
    uartConfig.rxEnable = CyFalse;
    uartConfig.flowCtrl = CyFalse;
    uartConfig.isDma = CyTrue;

    apiRetStatus = CyU3PUartSetConfig (&uartConfig, NULL);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Set the UART transfer to a really large value. */
    apiRetStatus = CyU3PUartTxSetBlockXfer (0xFFFFFFFF);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Initialize the debug module. */
    apiRetStatus = CyU3PDebugInit (CY_U3P_LPP_SOCKET_UART_CONS, 8);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyFxAppErrorHandler(apiRetStatus);
    }
    CyU3PDebugPrint (4, "BBRF103_SE debug init \n");

}


void setDescriptors(void) {
/* Set the USB Enumeration descriptors */

CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

/* Super speed device descriptor. */
apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_DEVICE_DESCR, (int)NULL, (uint8_t *)CyFxUSB30DeviceDscr);
if (apiRetStatus != CY_U3P_SUCCESS)
{
    CyU3PDebugPrint (4, "USB set device descriptor failed, Error code = %d\n", apiRetStatus);
    CyFxAppErrorHandler(apiRetStatus);
}
/* High speed device descriptor. */
apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_HS_DEVICE_DESCR, (int)NULL, (uint8_t *)CyFxUSB20DeviceDscr);
if (apiRetStatus != CY_U3P_SUCCESS)
{
    CyU3PDebugPrint (4, "USB set device descriptor failed, Error code = %d\n", apiRetStatus);
    CyFxAppErrorHandler(apiRetStatus);
}

/* BOS descriptor */
apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_BOS_DESCR, (int)NULL, (uint8_t *)CyFxUSBBOSDscr);
if (apiRetStatus != CY_U3P_SUCCESS)
{
    CyU3PDebugPrint (4, "USB set configuration descriptor failed, Error code = %d\n", apiRetStatus);
    CyFxAppErrorHandler(apiRetStatus);
}

/* Device qualifier descriptor */
apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_DEVQUAL_DESCR, (int)NULL, (uint8_t *)CyFxUSBDeviceQualDscr);
if (apiRetStatus != CY_U3P_SUCCESS)
{
    CyU3PDebugPrint (4, "USB set device qualifier descriptor failed, Error code = %d\n", apiRetStatus);
    CyFxAppErrorHandler(apiRetStatus);
}

/* Super speed configuration descriptor */
apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_CONFIG_DESCR,(int) NULL, (uint8_t *)CyFxUSBSSConfigDscr);
if (apiRetStatus != CY_U3P_SUCCESS)
{
    CyU3PDebugPrint (4, "USB set configuration descriptor failed, Error code = %d\n", apiRetStatus);
    CyFxAppErrorHandler(apiRetStatus);
}

/* High speed configuration descriptor */
apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_HS_CONFIG_DESCR, (int)NULL, (uint8_t *)CyFxUSBHSConfigDscr);
if (apiRetStatus != CY_U3P_SUCCESS)
{
    CyU3PDebugPrint (4, "USB Set Other Speed Descriptor failed, Error Code = %d\n", apiRetStatus);
    CyFxAppErrorHandler(apiRetStatus);
}

/* Full speed configuration descriptor */
apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_FS_CONFIG_DESCR, (int)NULL, (uint8_t *)CyFxUSBFSConfigDscr);
if (apiRetStatus != CY_U3P_SUCCESS)
{
    CyU3PDebugPrint (4, "USB Set Configuration Descriptor failed, Error Code = %d\n", apiRetStatus);
    CyFxAppErrorHandler(apiRetStatus);
}

/* String descriptor 0 */
apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 0, (uint8_t *)CyFxUSBStringLangIDDscr);
if (apiRetStatus != CY_U3P_SUCCESS)
{
    CyU3PDebugPrint (4, "USB set string descriptor failed, Error code = %d\n", apiRetStatus);
    CyFxAppErrorHandler(apiRetStatus);
}

/* String descriptor 1 */
apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 1, (uint8_t *)CyFxUSBManufactureDscr);
if (apiRetStatus != CY_U3P_SUCCESS)
{
    CyU3PDebugPrint (4, "USB set string descriptor failed, Error code = %d\n", apiRetStatus);
    CyFxAppErrorHandler(apiRetStatus);
}

/* String descriptor 2 */
apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 2, (uint8_t *)CyFxUSBProductDscr);
if (apiRetStatus != CY_U3P_SUCCESS)
{
    CyU3PDebugPrint (4, "USB set string descriptor failed, Error code = %d\n", apiRetStatus);
    CyFxAppErrorHandler(apiRetStatus);
}

}
