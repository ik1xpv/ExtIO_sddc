/*
 * Application.h
 *
 *  Created on: 21/set/2020
 *  Author: ik1xp
 *  This file contains the constants and definitions used by the HF103 FIFO application
 *
 */


#ifndef _INCLUDED_APPLICATION_H_
#define _INCLUDED_APPLICATION_H_

#include "cyu3types.h"
#include "cyu3system.h"
#include "cyu3pib.h"
#include "cyu3gpif.h"
#include "cyu3os.h"
#include "cyu3dma.h"
#include "cyu3error.h"
#include "cyu3usbconst.h"
#include "cyu3lpp.h"
#include "cyu3gpio.h"
#include "cyu3usb.h"
#include "cyu3uart.h"
#include "cyu3externcstart.h"
#include "i2cmodule.h"

#define FIFO_DMA_RX_SIZE        (0)	                  /* DMA transfer size is set to infinite */
#define FIFO_THREAD_STACK       (0x400)               /* application thread stack size */
#define FIFO_THREAD_PRIORITY    (8)                   /* application thread priority */
#define CY_FX_EP_CONSUMER       (0x81)   				  /* EP 1 IN */
#define PING_PRODUCER_SOCKET	(CY_U3P_PIB_SOCKET_0)
#define PONG_PRODUCER_SOCKET	(CY_U3P_PIB_SOCKET_1)
#define CONSUMER_USB_SOCKET     (CY_U3P_UIB_SOCKET_CONS_1)    /* USB Socket 1 is consumer */
/* DMA buffers used by the application. */
#define BULK_PKT_SIZE        	(0x400)         /* 1024 Bytes */
#define BULK_PKTS_COUNT  	 	(0x10)          /* 16 packets (burst of 16) per DMA buffer. */
#define DMA_BUFFER_SIZE      	(BULK_PKTS_COUNT * BULK_PKT_SIZE)  /* 16 KB */
#define DMA_BUFFER_COUNT     	(4)


#define USER_COMMAND_AVAILABLE	(0xA)
#define VENDOR_RQT (0x1)

// Define constants for blinking Error LED used in StartUp.c
#define PWM_PERIOD 				(20000000)   // Approximately 10Hz
#define PWM_THRESHOLD			( 5000000)   // On for 25% of the time

// With these two defines you can use 'true' and 'false' as well as 'CyTrue' and 'CyFalse'
#define true	CyTrue
#define false	CyFalse
#define ReturnStatus_t CyU3PReturnStatus_t

#define UINT64  uint32_t
#define UINT32  uint32_t
#define UINT16  uint16_t
#define UINT8   uint8_t

/* Burst length in 1 KB packets. Only applicable to USB 3.0. */
#define ENDPOINT_BURST_LENGTH (16)
#define ENDPOINT_BURST_SIZE	(1024)
#define INFINITE_TRANSFER_SIZE			 (0)

#define APP_THREADS						 (1)
// void null_func(uint8_t, ...)  // redefine DebugPrint if required
#define DebugPrint (CyU3PDebugPrint)

// GPIO
#define LED_OVERLOAD  (21)

#define GPIO_ATT_LE    (21)
#define GPIO_ATT_CLK   (22)
#define GPIO_ATT_DATA  (23)
#define LED_KIT		   (54)		// This is also UART_CTS

#define OUTXIO0 (1) 	// ATT_LE		 GPIO21
#define OUTXIO1 (2) 	// ATT_CLK  	 GPIO22
#define OUTXIO2 (4) 	// ATT_DATA  	 GPIO23
#define OUTXIO3 (8)  	// SEL0  		 GPIO26
#define OUTXIO4 (16) 	// SEL1  		 GPIO27
#define OUTXIO5 (32)  	// SHDWN 		 GPIO18
#define OUTXIO6 (64)  	// DITH  		 GPIO17
#define OUTXIO7 (128)  	// RAND   		 GPIO29

#define OUTXIO8 (1) 	// 256
#define OUTXIO9 (2) 	// 512
#define OUTXI10 (4) 	// 1024
#define OUTXI11 (8)  	// 2048
#define OUTXI12 (16) 	// 4096
#define OUTXI13 (32)  	// 8192
#define OUTXI14 (64)  	// 16384
#define OUTXI15 (128)  	// 32768


#define SHDWN           (32)
#define DITH            (64)
#define RANDO           (128)

#define BIAS_HF         (256)
#define BIAS_VHF        (512)
#define LED_YELLOW      (1024)
#define LED_RED         (2048)
#define LED_BLUE        (4096)
#define ATT_SEL0        (8192)
#define ATT_SEL1		(16384)

typedef struct outxio_t
{
    uint8_t  buffer[4];         /* The actual byte used is [0][1]  */
} outxio_t;

#define STARTFX3 		(0xAA)
#define STOPFX3 		(0xAB)
#define TESTFX3			(0xAC)
#define GPIOWFX3 		(0xAD)
#define I2CWFX3  		(0xAE)
#define I2CRFX3			(0xAF)
#define DAT31FX3 		(0xB0)
#define RESETFX3		(0xB1)
#define SI5351A 		(0xB2)
#define SI5351ATUNE  	(0xB3)
#define R820T2INIT  	(0xB4)
#define R820T2TUNE 		(0xB5)
#define R820T2SETATT 	(0xB6)
#define R820T2GETATT 	(0xB7)
#define R820T2STDBY 	(0xB8)

#define BBRF103 (0x01)
#define HF103 (0x02)
#define RX888 (0x03)


#include "cyu3externcend.h"
#endif // _INCLUDED_APPLICATION_H_
