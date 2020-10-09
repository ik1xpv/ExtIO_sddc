#ifndef _INCLUDED_AUXUTIL_H_
#define _INCLUDED_AUXUTIL_H_

#include "cyu3externcstart.h"
//#include "cyu3types.h"
#include "cyu3usbconst.h"
#include "cyu3usb.h"

#define LED_OVERLOAD (21)
#define LED_MODEA    (22)
#define LED_MODEB    (23)
// redefine to error handler
#define GPIO_LED1   (21)
#define GPIO_LED2   (22)
#define GPIO_LED3   (23)
#define LED_ON (1)
#define LED_OFF (0)
#define ERR_DIGIT_ON	100
#define ERR_DIGIT_OFF	400
#define ERR_DIGIT_SPACE	250
#define ERR_DIGIT_END   750

#define OUTXIO0 (1) 	// LED_OVERLOAD  GPIO21  bit position
#define OUTXIO1 (2) 	// LED_MODEA  GPIO22
#define OUTXIO2 (4) 	// LED_MODEB  GPIO22

#define OUTXIO3 (8)  	// SEL0  GPIO26
#define OUTXIO4 (16) 	// SEL1  GPIO27

#define OUTXIO5 (32)  	// SHDWN  GPIO28
#define OUTXIO6 (64)  	// DITH   GPIO29
#define OUTXIO7 (128)  	// DITH   GPIO20

typedef struct outxio_t
{
    uint8_t  buffer[4];         /* The actual byte used is [0]  */
} outxio_t;

typedef struct i2cxio_t
{
    uint8_t  i2caddr;            /* The i2c device address  */
    uint8_t  regaddr;            /* The device register address  */
    uint8_t  isRead;           	 /* (0) False value WRITE, (1) True value Read */
    uint8_t  lencount;			 /*	 byte count  < 12 */
    uint8_t  databyte[12];
} i2cxio_t;


#define CY_FX_RQT_I2C_WRITE                     (0xBA)
#define CY_FX_RQT_I2C_READ                      (0xBE)
#define CY_FX_RQT_GPIO_WRITE 					(0xBC)
#define CY_FX_RQT_GPIO_PWM                      (0xBD)
#define CY_FX_RQT_RESET                      	(0xCC)
#define CY_FX_RQT_START                      	(0xAA)
#define CY_FX_RQT_STOP                      	(0xBB)

void CyFxAppErrorHandler (  CyU3PReturnStatus_t apiRetStatus );
CyBool_t CyFxApplnLPMRqtCB ( CyU3PUsbLinkPowerMode link_mode);
void CyFxSlFifoApplnDebugInit (void);
void setDescriptors(void);
void SDR_ErrorHandler( CyU3PReturnStatus_t apiRetStatus, int code );
#endif
