#ifndef _INCLUDED_I2CMODULE_H_
#define _INCLUDED_I2CMODULE_H_

#include "cyu3types.h"
#include "cyu3usbconst.h"
#include "Application.h"

/* I2C Data rate */
#define I2C_ACTIVE
#define I2C_BITRATE        (400000)

CyU3PReturnStatus_t I2cInit();

CyU3PReturnStatus_t I2cTransferW1 (  // Write one byte only
		uint8_t   byteAddress,
		uint8_t   devAddr,
        uint8_t   data);

CyU3PReturnStatus_t I2cTransfer (
        uint8_t   byteAddress,
        uint8_t   devAddr,
        uint8_t   byteCount,
        uint8_t   *buffer,
        CyBool_t  isRead);


//CyU3PReturnStatus_t SendI2cbytes(uint8_t i2caddr, uint8_t regaddr, uint8_t * pdata, uint8_t len);

#endif
