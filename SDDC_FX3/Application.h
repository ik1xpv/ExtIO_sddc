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

// detect pin for HF103
#define LED_KIT		   (54)		// This is also UART_CTS

typedef struct outxio_t
{
    uint8_t  buffer[4];         /* The actual byte used is [0][1]  */
} outxio_t;

#include "../Interface.h"


#include "cyu3externcend.h"

CyU3PReturnStatus_t ConfGPIOsimpleout( uint8_t gpioid);

// bbrf103
void bbrf103_GpioSet(uint32_t mdata);
void bbrf103_GpioInitialize();

// hf103
void hf103_GpioSet(uint32_t mdata);
void hf103_GpioInitialize();
void hf103_SetAttenuator(uint8_t value);

// rx888
void rx888_GpioSet(uint32_t mdata);
void rx888_GpioInitialize();

// rx888r2
void rx888r2_GpioSet(uint32_t mdata);
void rx888r2_GpioInitialize();
void rx888r2_SetAttenuator(uint8_t value);
void rx888r2_SetGain(uint8_t value);

// rx888r2
void rx999_GpioSet(uint32_t mdata);
void rx999_GpioInitialize();
void rx999_SetAttenuator(uint8_t value);
void rx999_SetGain(uint8_t value);
int rx999_preselect(uint32_t data);

#endif // _INCLUDED_APPLICATION_H_
