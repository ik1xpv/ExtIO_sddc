#ifndef _INCLUDED_I2CMODULE_H_
#define _INCLUDED_I2CMODULE_H_

#include "cyu3types.h"
#include "cyu3usbconst.h"

/* I2C Data rate */
#define I2C_ACTIVE
#define CY_FX_USBI2C_I2C_BITRATE        (400000)

CyU3PReturnStatus_t
CyFxI2cInit();

CyU3PReturnStatus_t
CyFxUsbI2cTransfer (
        uint8_t   byteAddress,
        uint8_t   devAddr,
        uint8_t   byteCount,
        uint8_t   *buffer,
        CyBool_t  isRead);

#endif
