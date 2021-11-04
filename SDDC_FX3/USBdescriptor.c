/*
 * USB_Descriptors.c - these descriptors define a Cypress Streamer device
 *  Created on: 21/set/2020
 *
 *  HF103_FX3 project
 *  Author: Oscar Steila
 *  modified from: SuperSpeed Device Design By Example - John Hyde
 *
 *  https://sdr-prototypes.blogspot.com/
 */

#include "Application.h"
#include "cyu3utils.h"

extern void CheckStatus(char* StringPtr, CyU3PReturnStatus_t Status);

/* Standard device descriptor for USB 3.0 */
const uint8_t CyFxUSB30DeviceDscr[] __attribute__ ((aligned (32))) =
{
    0x12,                           /* Descriptor size */
    CY_U3P_USB_DEVICE_DESCR,        /* Device descriptor type */
    0x00,0x03,                      /* USB 3.0 */
    0x00,                           /* Device class */
    0x00,                           /* Device sub-class */
    0x00,                           /* Device protocol */
    0x09,                           /* Maxpacket size for EP0 : 2^9 */
    0xB4,0x04,                      /* Vendor ID */
    0xF1,0x00,                      /* Product ID */
    0x00,0x00,                      /* Device release number */
    0x01,                           /* Manufacture string index */
    0x02,                           /* Product string index */
    0x03,                           /* Serial number string index */
    0x01                            /* Number of configurations */
};

/* Standard device descriptor for USB 2.0 */
const uint8_t CyFxUSB20DeviceDscr[] __attribute__ ((aligned (32))) =
{
    0x12,                           /* Descriptor size */
    CY_U3P_USB_DEVICE_DESCR,        /* Device descriptor type */
    0x10,0x02,                      /* USB 2.10 */
    0x00,                           /* Device class */
    0x00,                           /* Device sub-class */
    0x00,                           /* Device protocol */
    0x40,                           /* Maxpacket size for EP0 : 64 bytes */
    0xB4,0x04,                      /* Vendor ID */
    0xF1,0x00,                      /* Product ID */
    0x00,0x00,                      /* Device release number */
    0x01,                           /* Manufacture string index */
    0x02,                           /* Product string index */
    0x00,                           /* Serial number string index */
    0x01                            /* Number of configurations */
};

/* Binary device object store descriptor */
const uint8_t CyFxUSBBOSDscr[] __attribute__ ((aligned (32))) =
{
    0x05,                           /* Descriptor size */
    CY_U3P_BOS_DESCR,               /* Device descriptor type */
    0x16,0x00,                      /* Length of this descriptor and all sub descriptors */
    0x02,                           /* Number of device capability descriptors */

    /* USB 2.0 extension */
    0x07,                           /* Descriptor size */
    CY_U3P_DEVICE_CAPB_DESCR,       /* Device capability type descriptor */
    CY_U3P_USB2_EXTN_CAPB_TYPE,     /* USB 2.0 extension capability type */
    0x02,0x00,0x00,0x00,            /* Supported device level features: LPM support  */

    /* SuperSpeed device capability */
    0x0A,                           /* Descriptor size */
    CY_U3P_DEVICE_CAPB_DESCR,       /* Device capability type descriptor */
    CY_U3P_SS_USB_CAPB_TYPE,        /* SuperSpeed device capability type */
    0x00,                           /* Supported device level features  */
    0x0E,0x00,                      /* Speeds supported by the device : SS, HS and FS */
    0x03,                           /* Functionality support */
    0x0A,                           /* U1 Device Exit latency */
    0xFF,0x07                       /* U2 Device Exit latency */
};

/* Standard device qualifier descriptor */
const uint8_t CyFxUSBDeviceQualDscr[] __attribute__ ((aligned (32))) =
{
    0x0A,                           /* Descriptor size */
    CY_U3P_USB_DEVQUAL_DESCR,       /* Device qualifier descriptor type */
    0x00,0x02,                      /* USB 2.0 */
    0x00,                           /* Device class */
    0x00,                           /* Device sub-class */
    0x00,                           /* Device protocol */
    0x40,                           /* Maxpacket size for EP0 : 64 bytes */
    0x01,                           /* Number of configurations */
    0x00                            /* Reserved */
};

/* Standard super speed configuration descriptor */
const uint8_t CyFxUSBSSConfigDscr[] __attribute__ ((aligned (32))) =
{
    /* Configuration descriptor */
    0x09,                           /* Descriptor size */
    CY_U3P_USB_CONFIG_DESCR,        /* Configuration descriptor type */
    0x1F,0x00,                      /* Length of this descriptor and all sub descriptors */
    0x01,                           /* Number of interfaces */
    0x01,                           /* Configuration number */
    0x00,                           /* COnfiguration string index */
    0x80,                           /* Config characteristics - Bus powered */
    0x64,                           /* Max power consumption of device (in 8mA unit) : 800mA */

    /* Interface descriptor */
    0x09,                           /* Descriptor size */
    CY_U3P_USB_INTRFC_DESCR,        /* Interface Descriptor type */
    0x00,                           /* Interface number */
    0x00,                           /* Alternate setting number */
    0x01,                           /* Number of end points */
    0xFF,                           /* Interface class */
    0x00,                           /* Interface sub class */
    0x00,                           /* Interface protocol code */
    0x00,                           /* Interface descriptor string index */

    /* Endpoint descriptor for consumer EP */
    0x07,                           /* Descriptor size */
    CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint descriptor type */
    CY_FX_EP_CONSUMER,              /* Endpoint address and description */
    CY_U3P_USB_EP_BULK,             /* Bulk endpoint type */
    0x00,0x04,                      /* Max packet size = 1024 bytes */
    0x00,                           /* Servicing interval for data transfers : 0 for Bulk */

    /* Super speed endpoint companion descriptor for consumer EP */
    0x06,                           /* Descriptor size */
    CY_U3P_SS_EP_COMPN_DESCR,       /* SS endpoint companion descriptor type */
    (ENDPOINT_BURST_LENGTH - 1),    /* Max no. of packets in a burst(0-15) - 0: burst 1 packet at a time */
    0x00,                           /* Max streams for bulk EP = 0 (No streams) */
    0x00,0x00                       /* Service interval for the EP : 0 for bulk */
};

/* Standard high speed configuration descriptor */
const uint8_t CyFxUSBHSConfigDscr[] __attribute__ ((aligned (32))) =
{
    /* Configuration descriptor */
    0x09,                           /* Descriptor size */
    CY_U3P_USB_CONFIG_DESCR,        /* Configuration descriptor type */
    0x19,0x00,                      /* Length of this descriptor and all sub descriptors */
    0x01,                           /* Number of interfaces */
    0x01,                           /* Configuration number */
    0x00,                           /* COnfiguration string index */
    0x80,                           /* Config characteristics - bus powered */
    0x96,                           /* Max power consumption of device (in 2mA unit) : 300mA */

    /* Interface descriptor */
    0x09,                           /* Descriptor size */
    CY_U3P_USB_INTRFC_DESCR,        /* Interface Descriptor type */
    0x00,                           /* Interface number */
    0x00,                           /* Alternate setting number */
    0x01,                           /* Number of endpoints */
    0xFF,                           /* Interface class */
    0x00,                           /* Interface sub class */
    0x00,                           /* Interface protocol code */
    0x00,                           /* Interface descriptor string index */

    /* Endpoint descriptor for consumer EP */
    0x07,                           /* Descriptor size */
    CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint descriptor type */
    CY_FX_EP_CONSUMER,              /* Endpoint address and description */
    CY_U3P_USB_EP_BULK,             /* Bulk endpoint type */
    0x00,0x02,                      /* Max packet size = 512 bytes */
    0x00                            /* Servicing interval for data transfers : 0 for bulk */
};

/* Standard full speed configuration descriptor */
const uint8_t CyFxUSBFSConfigDscr[] __attribute__ ((aligned (32))) =
{
    /* Configuration descriptor */
    0x09,                           /* Descriptor size */
    CY_U3P_USB_CONFIG_DESCR,        /* Configuration descriptor type */
    0x19,0x00,                      /* Length of this descriptor and all sub descriptors */
    0x01,                           /* Number of interfaces */
    0x01,                           /* Configuration number */
    0x00,                           /* COnfiguration string index */
    0x80,                           /* Config characteristics - bus powered */
    0x32,                           /* Max power consumption of device (in 2mA unit) : 100mA */

    /* Interface descriptor */
    0x09,                           /* Descriptor size */
    CY_U3P_USB_INTRFC_DESCR,        /* Interface descriptor type */
    0x00,                           /* Interface number */
    0x00,                           /* Alternate setting number */
    0x01,                           /* Number of endpoints */
    0xFF,                           /* Interface class */
    0x00,                           /* Interface sub class */
    0x00,                           /* Interface protocol code */
    0x00,                           /* Interface descriptor string index */

    /* Endpoint descriptor for consumer EP */
    0x07,                           /* Descriptor size */
    CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint descriptor type */
    CY_FX_EP_CONSUMER,              /* Endpoint address and description */
    CY_U3P_USB_EP_BULK,             /* Bulk endpoint type */
    0x40,0x00,                      /* Max packet size = 64 bytes */
    0x00                            /* Servicing interval for data transfers : 0 for bulk */
};

/* Standard language ID string descriptor */
const uint8_t CyFxUSBStringLangIDDscr[] __attribute__ ((aligned (32))) =
{
    4,                           	/* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    9,4                       		/* Language ID supported */
};

/* Standard manufacturer string descriptor */
const uint8_t CyFxUSBManufactureDscr[] __attribute__ ((aligned (32))) =
{
    32,                             /* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    //http://sdr-prototypes.blogspot.com/
    's',0,'d',0,'r',0,' ',0,'p',0,'r',0,'o',0,'t',0,
    'o',0,'t',0,'y',0,'p',0,'e',0,'s',0
};

/* Standard product string descriptor */
const uint8_t CyFxUSBProductDscr[] __attribute__ ((aligned (32))) =
{
    8,                              /* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    'S',0,'D',0,'R',0
};

const uint8_t CyFxUSBProductDscrHF103[] __attribute__ ((aligned (32))) =
{
    12,                             /* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    'H',0,'F',0,'1',0,'0',0,'3',0
};

const uint8_t CyFxUSBProductDscrBBRF103[] __attribute__ ((aligned (32))) =
{
    16,                             /* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    'B',0,'B',0,'R',0,'F',0,'1',0,'0',0,'3',0
};

const uint8_t CyFxUSBProductDscrRX888[] __attribute__ ((aligned (32))) =
{
    12,                             /* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    'R',0,'X',0,'8',0,'8',0,'8',0,
};

const uint8_t CyFxUSBProductDscrRX888mk2[] __attribute__ ((aligned (32))) =
{
    18,                             /* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    'R',0,'X',0,'8',0,'8',0,'8',0,'m',0,'k',0,'2',0
};


/* Microsoft OS Descriptor. */
const uint8_t CyFxUsbOSDscr[] __attribute__ ((aligned (32))) =
{
    16,
    CY_U3P_USB_STRING_DESCR,
    'O',0,'S',0,' ',0,'D',0,'e',0,'s',0,'c',0
};

/* Serial number string descriptor */
uint8_t CyFxUSBSerialNumberDscr[] __attribute__ ((aligned (32))) =
{
    34,                           	/* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

/* Place this buffer as the last buffer so that no other variable / code shares
 * the same cache line. Do not add any other variables / arrays in this file.
 * This will lead to variables sharing the same cache line. */
const uint8_t CyFxUsbDscrAlignBuffer[32] __attribute__ ((aligned (32)));


CyU3PReturnStatus_t SetUSBdescriptors(uint8_t hwconfig)
{
	CyU3PReturnStatus_t OverallStatus, Status;
	OverallStatus = Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_DEVICE_DESCR,(int) NULL, (uint8_t *)CyFxUSB30DeviceDscr);
	CheckStatus("SET_SS_DEVICE_DESCR", Status);
	OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_HS_DEVICE_DESCR,(int) NULL, (uint8_t *)CyFxUSB20DeviceDscr);
	CheckStatus("SET_HS_DEVICE_DESCR", Status);
	OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_BOS_DESCR,(int) NULL, (uint8_t *)CyFxUSBBOSDscr);
	CheckStatus("SET_SS_BOS_DESCR", Status);
	OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_DEVQUAL_DESCR,(int) NULL, (uint8_t *)CyFxUSBDeviceQualDscr);
	CheckStatus("SET_DEVQUAL_DESCR", Status);
	OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_CONFIG_DESCR,(int) NULL, (uint8_t *)CyFxUSBSSConfigDscr);
	CheckStatus("SET_SS_CONFIG_DESCR", Status);
	OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_HS_CONFIG_DESCR,(int) NULL, (uint8_t *)CyFxUSBHSConfigDscr);
	CheckStatus("SET_HS_CONFIG_DESCR", Status);
	OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_FS_CONFIG_DESCR,(int) NULL, (uint8_t *)CyFxUSBFSConfigDscr);
	CheckStatus("SET_FS_CONFIG_DESCR", Status);
	OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 0, (uint8_t *)CyFxUSBStringLangIDDscr);
	CheckStatus("SET_STRING0_DESCR", Status);
	OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 1, (uint8_t *)CyFxUSBManufactureDscr);
	CheckStatus("SET_STRING1_DESCR", Status);
	switch(hwconfig)
	{
		case HF103:
			OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 2, (uint8_t *)CyFxUSBProductDscrHF103);
			break;
		case BBRF103:
			OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 2, (uint8_t *)CyFxUSBProductDscrBBRF103);
			break;
		case RX888:
			OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 2, (uint8_t *)CyFxUSBProductDscrRX888);
			break;
		case RX888r2:
			OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 2, (uint8_t *)CyFxUSBProductDscrRX888mk2);
			break;
		case RX888r3:
		case RX999:
		case RXLUCY:		
	        default :
	        	OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 2, (uint8_t *)CyFxUSBProductDscr);
	        	break;
	}
	CheckStatus("SET_STRING2_DESCR", Status);
	
	// serial number
	uint32_t FX3_ID[2] = { 0 };
	OverallStatus |= Status = CyU3PReadDeviceRegisters((uvint32_t *)0xE0055010, 2, FX3_ID);
	CheckStatus("READ_FX3_ID", Status);
	const uint8_t hexdigits[] = { '0', '1', '2', '3', '4', '5', '6', '7',
		                  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
	CyFxUSBSerialNumberDscr[32] = hexdigits[FX3_ID[0] & 0xf]; FX3_ID[0] >>= 4;
	CyFxUSBSerialNumberDscr[30] = hexdigits[FX3_ID[0] & 0xf]; FX3_ID[0] >>= 4;
	CyFxUSBSerialNumberDscr[28] = hexdigits[FX3_ID[0] & 0xf]; FX3_ID[0] >>= 4;
	CyFxUSBSerialNumberDscr[26] = hexdigits[FX3_ID[0] & 0xf]; FX3_ID[0] >>= 4;
	CyFxUSBSerialNumberDscr[24] = hexdigits[FX3_ID[0] & 0xf]; FX3_ID[0] >>= 4;
	CyFxUSBSerialNumberDscr[22] = hexdigits[FX3_ID[0] & 0xf]; FX3_ID[0] >>= 4;
	CyFxUSBSerialNumberDscr[20] = hexdigits[FX3_ID[0] & 0xf]; FX3_ID[0] >>= 4;
	CyFxUSBSerialNumberDscr[18] = hexdigits[FX3_ID[0] & 0xf]; FX3_ID[0] >>= 4;
	CyFxUSBSerialNumberDscr[16] = hexdigits[FX3_ID[1] & 0xf]; FX3_ID[1] >>= 4;
	CyFxUSBSerialNumberDscr[14] = hexdigits[FX3_ID[1] & 0xf]; FX3_ID[1] >>= 4;
	CyFxUSBSerialNumberDscr[12] = hexdigits[FX3_ID[1] & 0xf]; FX3_ID[1] >>= 4;
	CyFxUSBSerialNumberDscr[10] = hexdigits[FX3_ID[1] & 0xf]; FX3_ID[1] >>= 4;
	CyFxUSBSerialNumberDscr[ 8] = hexdigits[FX3_ID[1] & 0xf]; FX3_ID[1] >>= 4;
	CyFxUSBSerialNumberDscr[ 6] = hexdigits[FX3_ID[1] & 0xf]; FX3_ID[1] >>= 4;
	CyFxUSBSerialNumberDscr[ 4] = hexdigits[FX3_ID[1] & 0xf]; FX3_ID[1] >>= 4;
	CyFxUSBSerialNumberDscr[ 2] = hexdigits[FX3_ID[1] & 0xf]; FX3_ID[1] >>= 4;
	OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 3, (uint8_t *)CyFxUSBSerialNumberDscr);
	CheckStatus("SET_STRING3_DESCR", Status);
	
	return OverallStatus;
}
