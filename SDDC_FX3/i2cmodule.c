/*
 * i2cmodule.c
 *
 *  Created on: Mar 14, 2017
 *      Author: Oscar
 */

#include "cyu3system.h"
#include "cyu3os.h"
#include "cyu3dma.h"
#include "cyu3error.h"
#include "cyu3usb.h"
#include "cyu3i2c.h"
#include "i2cmodule.h"

CyU3PReturnStatus_t
I2cInit ()
{
    CyU3PI2cConfig_t i2cConfig;
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;

    /* Initialize and configure the I2C master module. */
    status = CyU3PI2cInit ();
    if (status != CY_U3P_SUCCESS) return status;

    /* Start the I2C master block. The bit rate is set at 100KHz.
     * The data transfer is done via DMA. */
    CyU3PMemSet ((uint8_t *)&i2cConfig, 0, sizeof(i2cConfig));
    i2cConfig.bitRate    = I2C_BITRATE;
    i2cConfig.busTimeout = 0xFFFFFFFF;
    i2cConfig.dmaTimeout = 0xFFFF;
    i2cConfig.isDma      = CyFalse;
    status = CyU3PI2cSetConfig (&i2cConfig, NULL);

    return status;
}



/* I2C read / write for application. */
CyU3PReturnStatus_t
I2cTransfer (
        uint8_t   byteAddress,
        uint8_t   devAddr,
        uint8_t   byteCount,
        uint8_t   *buffer,
        CyBool_t  isRead)
{

//	DebugPrint(4, "\r\nI2cTransfer %d, %d, %d, %d, %d",
//			byteAddress, devAddr, byteCount, *buffer, isRead);

    CyU3PI2cPreamble_t preamble;
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;
    if (byteCount == 0)
    {
        return status;
    }
	if (isRead)
	{
		/* Update the preamble information. */
		preamble.length    = 3;
		preamble.buffer[0] = devAddr;
		preamble.buffer[1] = byteAddress;
		preamble.buffer[2] = (devAddr | 0x01);
		preamble.ctrlMask  = 0x0002; // stop position 2
		status = CyU3PI2cReceiveBytes (&preamble, buffer, byteCount, 0);
	}
	else /* Write */
	{
		/* Update the preamble information. */
		preamble.length    = 2;
		preamble.buffer[0] = devAddr;
		preamble.buffer[1] = byteAddress;
		preamble.ctrlMask  = 0x0000;
		status = CyU3PI2cTransmitBytes (&preamble, buffer, byteCount, 0);
	}
    return status;
}

CyU3PReturnStatus_t I2cTransferW1(  // Write one byte only
		uint8_t   byteAddress,
		uint8_t   devAddr,
        uint8_t   data)
{
	uint8_t   byteCount = 1;
	uint8_t   ldata = data;
	return I2cTransfer (byteAddress, devAddr, byteCount, &ldata, false);
}
