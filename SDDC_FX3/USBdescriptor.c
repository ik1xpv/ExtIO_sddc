/*
 * USB_Descriptors.c - these descriptors define a Cypress Streamer device
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
 *  Date: Wed Nov 11 08:02:24 PM EST 2020
 */

#include "Application.h"

extern void CheckStatus(char* StringPtr, CyU3PReturnStatus_t Status);

// Note this custom VID_PID needs an INF file for driver matching.  Use "NEWcyusb3.inf"

/* Standard device descriptor for USB 3.0 */
const uint8_t CyFxUSB30DeviceDscr[] __attribute__ ((aligned (32))) =
{
    0x12,                           /* Descriptor size */
    CY_U3P_USB_DEVICE_DESCR,        /* Device descriptor type */
    0x00,0x03,                      /* USB 3.0 */
    0xEF,                           /* Device class */
    0x02,                           /* Device sub-class */
    0x01,                           /* Device protocol */
    0x09,                           /* Maxpacket size for EP0 : 2^9 */
    0x42,0x42,                      /* Vendor ID */
    0xAB,0xAB,                      /* Product ID */
    0x30,0x00,                      /* Device release number */
    0x01,                           /* Manufacture string index */
    0x02,                           /* Product string index */
    0x00,                           /* Serial number string index */
    0x01                            /* Number of configurations */
};

/* Standard device descriptor for USB 2.0 */
const uint8_t CyFxUSB20DeviceDscr[] __attribute__ ((aligned (32))) =
{
    0x12,                           /* Descriptor size */
    CY_U3P_USB_DEVICE_DESCR,        /* Device descriptor type */
    0x10,0x02,                      /* USB 2.10 */
    0xEF,                           /* Device class */
    0x02,                           /* Device sub-class */
    0x01,                           /* Device protocol */
    0x40,                           /* Maxpacket size for EP0 : 64 bytes */
    0x42,0x42,                      /* Vendor ID */
    0xAB,0xAB,                      /* Product ID */
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
    0x00,                           /* U1 Device Exit latency */
    0x00,0x00                       /* U2 Device Exit latency */
};

/* Standard device qualifier descriptor */
const uint8_t CyFxUSBDeviceQualDscr[] __attribute__ ((aligned (32))) =
{
    0x0A,                           /* Descriptor size */
    CY_U3P_USB_DEVQUAL_DESCR,       /* Device qualifier descriptor type */
    0x00,0x02,                      /* USB 2.0 */
    0xEF,                           /* Device class */
    0x02,                           /* Device sub-class */
    0x01,                           /* Device protocol */
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
    0x73,0x00,                      /* Length of this descriptor and all sub descriptors */
// Note: 3 interfaces, first two are CDC (Call Management and Data), third is Bulk Loop
    0x03,                           /* Number of interfaces */
    0x01,                           /* Configuration number */
    0x00,                           /* Configuration string index */
    0x80,                           /* Config characteristics - Bus powered */
    0x96,                           /* Max power consumption of device (in 8mA unit) : 1200mA */

    /* Interface association descriptor */
    0x08,                           /* Descriptor size */
    0x0B,     				       /* Interface association descr type */
    0x00,                           /* first interface */
    0x02,                           /* InterfaceCount */
    0x02,                           /* CDC */
    0x02,                           /* abstract control model */
    0x01,                           /* AT commands*/
    0x00,                           /* String desc index for interface */

    /* Communication Interface descriptor */
    0x09,                           /* Descriptor size */
	CY_U3P_USB_INTRFC_DESCR,        /* Interface Descriptor type */
	0x00,                           /* Interface number */
	0x00,                           /* Alternate setting number */
    0x01,                           /* Number of endpoints */
    0x02,                           /* Interface class : Communication interface */
    0x02,                           /* Interface sub class */
    0x01,                           /* Interface protocol code */
    0x03,                           /* Interface descriptor string index */

    /* CDC Class-specific Descriptors */
    /* Header functional Descriptor */
    0x05,                           /* Descriptors length(5) */
    0x24,                           /* Descriptor type : CS_Interface */
    0x00,                           /* DescriptorSubType : Header Functional Descriptor */
    0x10,0x01,                      /* bcdCDC : CDC Release Number */
    /* Abstract Control Management Functional Descriptor */
    0x04,                           /* Descriptors Length (4) */
    0x24,                           /* bDescriptorType: CS_INTERFACE */
    0x02,                           /* bDescriptorSubType: Abstract Control Model Functional Descriptor */
    0x02,                           /* bmCapabilities: Supports the request combination of Set_Line_Coding,
                                       Set_Control_Line_State, Get_Line_Coding and the notification Serial_State */
     /* Union Functional Descriptor */
    0x05,                           /* Descriptors Length (5) */
    0x24,                           /* bDescriptorType: CS_INTERFACE */
    0x06,                           /* bDescriptorSubType: Union Functional Descriptor */
    0x00,                           /* bMasterInterface */
    0x01,                           /* bSlaveInterface */
     /* Call Management Functional Descriptor */
    0x05,                           /*  Descriptors Length (5) */
    0x24,                           /*  bDescriptorType: CS_INTERFACE */
    0x01,                           /*  bDescriptorSubType: Call Management Functional Descriptor */
    0x00,                           /*  bmCapabilities: Device sends/receives call management information
                                        only over the Communication Class Interface. */
    0x01,                           /*  Interface Number of Data Class interface */
     /* Endpoint Descriptor(Interrupt) */
    0x07,                           /* Descriptor size */
    CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint descriptor type */
    CY_FX_EP_INTERRUPT_CDC,         /* Endpoint address and description */
    CY_U3P_USB_EP_INTR,             /* Interrupt endpoint type */
    0x40,0x00,                      /* Max packet size = 1024 bytes */
    0x02,                           /* Servicing interval for data transfers */
     /* Super speed endpoint companion descriptor for interrupt endpoint */
    0x06,                           /* Descriptor size */
    CY_U3P_SS_EP_COMPN_DESCR,       /* SS endpoint companion descriptor type */
    0x00,                           /* Max no. of packets in a Burst : 1 */
    0x00,                           /* Mult.: Max number of packets : 1 */
    0x40,0x00,                      /* Bytes per interval : 1024 */
     /* Data Interface Descriptor */
    0x09,                           /* Descriptor size */
    CY_U3P_USB_INTRFC_DESCR,        /* Interface Descriptor type */
    0x01,                           /* Interface number */
    0x00,                           /* Alternate setting number */
    0x02,                           /* Number of endpoints */
    0x0A,                           /* Interface class: Data interface */
    0x00,                           /* Interface sub class */
    0x00,                           /* Interface protocol code */
    0x04,                           /* Interface descriptor string index */
     /* Endpoint Descriptor(CDC-PRODUCER) */
    0x07,                           /* Descriptor size */
    CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint descriptor type */
    CY_FX_EP_PRODUCER_CDC,          /* Endpoint address and description */
    CY_U3P_USB_EP_BULK,             /* BULK endpoint type */
    0x00,0x04,                      /* Max packet size = 1024 bytes */
    0x00,                           /* Servicing interval for data transfers */
     /* Super speed endpoint companion descriptor for producer ep */
    0x06,                           /* Descriptor size */
    CY_U3P_SS_EP_COMPN_DESCR,       /* SS endpoint companion descriptor type */
    0x01,                           /* Max no. of packets in a burst : 1 */
    0x01,                           /* Mult.: Max number of packets : 1 */
    0x00,0x04,                      /* Bytes per interval : 1024 */
     /* Endpoint Descriptor(CDC-CONSUMER) */
    0x07,                           /* Descriptor size */
    CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint descriptor type */
    CY_FX_EP_CONSUMER_CDC,          /* Endpoint address and description */
    CY_U3P_USB_EP_BULK,             /* BULK endpoint type */
    0x00,0x04,                      /* Max packet size = 1024 bytes */
    0x00,                           /* Servicing interval for data transfers */
     /* Super speed endpoint companion descriptor for consumer ep */
    0x06,                           /* Descriptor size */
    CY_U3P_SS_EP_COMPN_DESCR,       /* SS endpoint companion descriptor type */
    0x01,                           /* Max no. of packets in a burst : 1 */
    0x01,                           /* Mult.: Max number of packets : 1 */
    0x00,0x04,             			/* Bytes per interval : 1024 */

// Now define the Bulk IN interface
     /* Interface descriptor */
	0x09,                           /* Descriptor size */
	CY_U3P_USB_INTRFC_DESCR,        /* Interface Descriptor type */
	0x02,                           /* Interface number */
	0x00,                           /* Alternate setting number */
	0x01,                           /* Number of end points */
	0xFF,                           /* Interface class */
	0x00,                           /* Interface sub class */
	0x00,                           /* Interface protocol code */
	0x05,                           /* Interface descriptor string index */

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
	0x00,0x00,                 		/* Service interval for the EP : 0 for bulk */
};

/* Standard high speed configuration descriptor */
const uint8_t CyFxUSBHSConfigDscr[] __attribute__ ((aligned (32))) =
{
    /* Configuration descriptor */
    0x09,                           /* Descriptor size */
    CY_U3P_USB_CONFIG_DESCR,        /* Configuration descriptor type */
    0x5B,0x00,                      /* Length of this descriptor and all sub descriptors */
    0x03,                           /* Number of interfaces */
    0x01,                           /* Configuration number */
    0x00,                           /* Configuration string index */
    0x80,                           /* Config characteristics - bus powered */
    0x96,                           /* Max power consumption of device (in 2mA unit) : 300mA */

    /* Interface association descriptor */
    0x08,                           /* Descriptor size */
    0x0B,     				        /* Interface association descr type */
    0x00,                           /* first interface */
    0x02,                           /* InterfaceCount */
    0x02,                           /* CDC */
    0x02,                           /* abstract control model */
    0x01,                           /* AT commands*/
    0x00,                           /* String desc index for interface */

    /* Communication Interface descriptor */
	0x09,                           /* Descriptor size */
	CY_U3P_USB_INTRFC_DESCR,        /* Interface Descriptor type */
	0x00,                           /* Interface number */
	0x00,                           /* Alternate setting number */
	0x01,                           /* Number of endpoints */
	0x02,                           /* Interface class : Communication interface */
	0x02,                           /* Interface sub class */
	0x01,                           /* Interface protocol code */
	0x03,                           /* Interface descriptor string index */

    /* CDC Class-specific Descriptors */
    /* Header functional Descriptor */
    0x05,                           /* Descriptors length(5) */
    0x24,                           /* Descriptor type : CS_Interface */
    0x00,                           /* DescriptorSubType : Header Functional Descriptor */
    0x10,0x01,                      /* bcdCDC : CDC Release Number */

    /* Abstract Control Management Functional Descriptor */
    0x04,                           /* Descriptors Length (4) */
    0x24,                           /* bDescriptorType: CS_INTERFACE */
    0x02,                           /* bDescriptorSubType: Abstract Control Model Functional Descriptor */
    0x02,                           /* bmCapabilities: Supports the request combination of Set_Line_Coding,
                                       Set_Control_Line_State, Get_Line_Coding and the notification Serial_State */

    /* Union Functional Descriptor */
    0x05,                           /* Descriptors Length (5) */
    0x24,                           /* bDescriptorType: CS_INTERFACE */
    0x06,                           /* bDescriptorSubType: Union Functional Descriptor */
    0x00,                           /* bMasterInterface */
    0x01,                           /* bSlaveInterface */

    /* Call Management Functional Descriptor */
    0x05,                           /*  Descriptors Length (5) */
    0x24,                           /*  bDescriptorType: CS_INTERFACE */
    0x01,                           /*  bDescriptorSubType: Call Management Functional Descriptor */
    0x00,                           /*  bmCapabilities: Device sends/receives call management information
                                           only over the Communication Class Interface. */
    0x01,                           /*  Interface Number of Data Class interface */

    /* Endpoint Descriptor(Interrupt) */
    0x07,                           /* Descriptor size */
    CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint descriptor type */
    CY_FX_EP_INTERRUPT_CDC,         /* Endpoint address and description */
    CY_U3P_USB_EP_INTR,             /* Interrupt endpoint type */
    0x40,0x00,                      /* Max packet size = 64 bytes */
    0x02,                           /* Servicing interval for data transfers */

    /* Data Interface Descriptor */
    0x09,                           /* Descriptor size */
    CY_U3P_USB_INTRFC_DESCR,        /* Interface Descriptor type */
    0x01,                           /* Interface number */
    0x00,                           /* Alternate setting number */
    0x02,                           /* Number of endpoints */
    0x0A,                           /* Interface class: Data interface */
    0x00,                           /* Interface sub class */
    0x00,                           /* Interface protocol code */
    0x04,                           /* Interface descriptor string index */

    /* Endpoint Descriptor(CDC-PRODUCER) */
    0x07,                           /* Descriptor size */
    CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint descriptor type */
    CY_FX_EP_PRODUCER_CDC,          /* Endpoint address and description */
    CY_U3P_USB_EP_BULK,             /* BULK endpoint type */
    0x00,0x02,                      /* Max packet size = 512 bytes */
    0x00,                           /* Servicing interval for data transfers */

    /* Endpoint Descriptor(CDC-CONSUMER) */
    0x07,                           /* Descriptor size */
    CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint descriptor type */
    CY_FX_EP_CONSUMER_CDC,          /* Endpoint address and description */
    CY_U3P_USB_EP_BULK,             /* BULK endpoint type */
    0x00,0x02,                      /* Max packet size = 512 bytes */
    0x00,                           /* Servicing interval for data transfers */

// Now define the Bulk IN interface
    /* Interface descriptor */
    0x09,                           /* Descriptor size */
	CY_U3P_USB_INTRFC_DESCR,        /* Interface Descriptor type */
	0x02,                           /* Interface number */
	0x00,                           /* Alternate setting number */
	0x01,                           /* Number of endpoints */
	0xFF,                           /* Interface class */
	0x00,                           /* Interface sub class */
	0x00,                           /* Interface protocol code */
	0x05,                           /* Interface descriptor string index */

	/* Endpoint descriptor for consumer EP */
	0x07,                           /* Descriptor size */
	CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint descriptor type */
	CY_FX_EP_CONSUMER,              /* Endpoint address and description */
	CY_U3P_USB_EP_BULK,             /* Bulk endpoint type */
	0x00,0x02,                      /* Max packet size = 512 bytes */
	0x00,                           /* Servicing interval for data transfers : 0 for bulk */
};

/* Standard full speed configuration descriptor */
// Only difference between this and the HS version is the bulk packet size
// Should fix this up dynamically, will do in Rev 2.0
const uint8_t CyFxUSBFSConfigDscr[] __attribute__ ((aligned (32))) =
{
    /* Configuration descriptor */
    0x09,                           /* Descriptor size */
    CY_U3P_USB_CONFIG_DESCR,        /* Configuration descriptor type */
    0x5B,0x00,                      /* Length of this descriptor and all sub descriptors */
    0x03,                           /* Number of interfaces */
    0x01,                           /* Configuration number */
    0x00,                           /* COnfiguration string index */
    0x80,                           /* Config characteristics - bus powered */
    0x32,                           /* Max power consumption of device (in 2mA unit) : 100mA */

    /* Interface association descriptor */
    0x08,                           /* Descriptor size */
    0x0B,     				        /* Interface association descr type */
    0x00,                           /* first interface */
    0x02,                           /* InterfaceCount */
    0x02,                           /* CDC */
    0x02,                           /* abstract control model */
    0x01,                           /* AT commands*/
    0x00,                           /* String desc index for interface */

    /* Communication Interface descriptor */
    0x09,                           /* Descriptor size */
	CY_U3P_USB_INTRFC_DESCR,        /* Interface Descriptor type */
	0x00,                           /* Interface number */
	0x00,                           /* Alternate setting number */
	0x01,                           /* Number of endpoints */
	0x02,                           /* Interface class : Communication interface */
	0x02,                           /* Interface sub class */
	0x01,                           /* Interface protocol code */
	0x03,                           /* Interface descriptor string index */

	/* CDC Class-specific Descriptors */
	/* Header functional Descriptor */
	0x05,                           /* Descriptors length(5) */
	0x24,                           /* Descriptor type : CS_Interface */
	0x00,                           /* DescriptorSubType : Header Functional Descriptor */
	0x10,0x01,                      /* bcdCDC : CDC Release Number */

	/* Abstract Control Management Functional Descriptor */
	0x04,                           /* Descriptors Length (4) */
	0x24,                           /* bDescriptorType: CS_INTERFACE */
	0x02,                           /* bDescriptorSubType: Abstract Control Model Functional Descriptor */
	0x02,                           /* bmCapabilities: Supports the request combination of Set_Line_Coding,
	                                   Set_Control_Line_State, Get_Line_Coding and the notification Serial_State */
	/* Union Functional Descriptor */
	0x05,                           /* Descriptors Length (5) */
	0x24,                           /* bDescriptorType: CS_INTERFACE */
	0x06,                           /* bDescriptorSubType: Union Functional Descriptor */
	0x00,                           /* bMasterInterface */
	0x01,                           /* bSlaveInterface */

	/* Call Management Functional Descriptor */
	0x05,                           /*  Descriptors Length (5) */
	0x24,                           /*  bDescriptorType: CS_INTERFACE */
	0x01,                           /*  bDescriptorSubType: Call Management Functional Descriptor */
	0x00,                           /*  bmCapabilities: Device sends/receives call management information
	                                    only over the Communication Class Interface. */
	0x01,                           /*  Interface Number of Data Class interface */

	/* Endpoint Descriptor(Interrupt) */
	0x07,                           /* Descriptor size */
	CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint descriptor type */
	CY_FX_EP_INTERRUPT_CDC,         /* Endpoint address and description */
	CY_U3P_USB_EP_INTR,             /* Interrupt endpoint type */
	0x10,0x00,                      /* Max packet size = 16 bytes */
	0x02,                           /* Servicing interval for data transfers */

	/* Data Interface Descriptor */
	0x09,                           /* Descriptor size */
	CY_U3P_USB_INTRFC_DESCR,        /* Interface Descriptor type */
	0x01,                           /* Interface number */
	0x00,                           /* Alternate setting number */
	0x02,                           /* Number of endpoints */
	0x0A,                           /* Interface class: Data interface */
	0x00,                           /* Interface sub class */
	0x00,                           /* Interface protocol code */
	0x04,                           /* Interface descriptor string index */

	/* Endpoint Descriptor(CDC-PRODUCER) */
	0x07,                           /* Descriptor size */
	CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint descriptor type */
	CY_FX_EP_PRODUCER_CDC,          /* Endpoint address and description */
	CY_U3P_USB_EP_BULK,             /* BULK endpoint type */
	0x40,0x00,                      /* Max packet size = 64 bytes */
	0x00,                           /* Servicing interval for data transfers */

	/* Endpoint Descriptor(CDC-CONSUMER) */
	0x07,                           /* Descriptor size */
	CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint descriptor type */
	CY_FX_EP_CONSUMER_CDC,          /* Endpoint address and description */
	CY_U3P_USB_EP_BULK,             /* BULK endpoint type */
	0x40,0x00,                      /* Max packet size = 64 bytes */
	0x00,                           /* Servicing interval for data transfers */

// Now define the Bulk IN interface
	/* Interface descriptor */
	0x09,                           /* Descriptor size */
	CY_U3P_USB_INTRFC_DESCR,        /* Interface descriptor type */
	0x02,                           /* Interface number */
	0x00,                           /* Alternate setting number */
	0x01,                           /* Number of endpoints */
	0xFF,                           /* Interface class */
	0x00,                           /* Interface sub class */
	0x00,                           /* Interface protocol code */
	0x05,                           /* Interface descriptor string index */

	/* Endpoint descriptor for consumer EP */
	0x07,                           /* Descriptor size */
	CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint descriptor type */
	CY_FX_EP_CONSUMER,              /* Endpoint address and description */
	CY_U3P_USB_EP_BULK,             /* Bulk endpoint type */
	0x40,0x00,                      /* Max packet size = 64 bytes */
	0x00,                           /* Servicing interval for data transfers : 0 for bulk */
};

/* Standard language ID string descriptor */
const uint8_t CyFxUSBStringLangIDDscr[] __attribute__ ((aligned (32))) =
{
    4,                              /* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    0x09,0x04                       /* Language ID supported */
};

/* Standard manufacturer string descriptor */
const uint8_t CyFxUSBManufactureDscr[] __attribute__ ((aligned (32))) =
{
    58,                             /* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    //http://sdr-prototypes.blogspot.com/
    's',0,'d',0,'r',0,'-',0,'p',0,'r',0,'o',0,'t',0,'o',0,'t',0,'y',0,'p',0,'e',0,'s',0,'.',0,
    'b',0,'l',0,'o',0,'g',0,'s',0,'p',0,'o',0,'t',0,'.',0,'c',0,'o',0,'m',0,
};

/* Standard product string descriptor */
const uint8_t CyFxUSBProductDscr[] __attribute__ ((aligned (32))) =
{
    18,                           	/* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    'H',0,'F',0,'1',0,'0',0,'3',0,'r',0,'c',0,'1',0,
};

/* Standard interface string descriptors */
const uint8_t CyFxUSBInterface0Dscr[] __attribute__ ((aligned (32))) =
{
    30,                           	/* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    'C',0,'D',0,'C',0,' ',0,'M',0,'a',0,'n',0,'a',0,'g',0,'e',0,'m',0,'e',0,'n',0,'t',0
};
const uint8_t CyFxUSBInterface1Dscr[] __attribute__ ((aligned (32))) =
{
    18,                           	/* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    'C',0,'D',0,'C',0,' ',0,'D',0,'a',0,'t',0,'a',0
};
const uint8_t CyFxUSBInterface2Dscr[] __attribute__ ((aligned (32))) =
{
    18,                           	/* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    's',0,'d',0,'d',0,'c',0,' ',0,'A',0,'D',0,'C',0
};

/* NOT USED??? - fv */
/* Microsoft OS Descriptor. */
const uint8_t CyFxUsbOSDscr[] __attribute__ ((aligned (32))) =
{
    14,
    CY_U3P_USB_STRING_DESCR,
    'O',0,'S',0,' ',0,'D',0,'e',0,'s',0,'c',0
};

// Place this buffer as the last buffer so that no other variable / code shares the same cache line
const uint8_t CyFxUsbDscrAlignBuffer[32] __attribute__ ((aligned (32)));


CyU3PReturnStatus_t SetUSBdescriptors(void)
{
	CyU3PReturnStatus_t OverallStatus, Status;
	OverallStatus = Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_DEVICE_DESCR, (int) NULL, (uint8_t *)CyFxUSB30DeviceDscr);
    CheckStatus("SET_SS_DEVICE_DESCR", Status);
    OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_HS_DEVICE_DESCR, (int) NULL, (uint8_t *)CyFxUSB20DeviceDscr);
    CheckStatus("SET_HS_DEVICE_DESCR", Status);
    OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_BOS_DESCR, (int) NULL, (uint8_t *)CyFxUSBBOSDscr);
    CheckStatus("SET_SS_BOS_DESCR", Status);
    OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_DEVQUAL_DESCR, (int) NULL, (uint8_t *)CyFxUSBDeviceQualDscr);
    CheckStatus("SET_DEVQUAL_DESCR", Status);
    OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_CONFIG_DESCR, (int) NULL, (uint8_t *)CyFxUSBSSConfigDscr);
    CheckStatus("SET_SS_CONFIG_DESCR", Status);
    OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_HS_CONFIG_DESCR, (int) NULL, (uint8_t *)CyFxUSBHSConfigDscr);
    CheckStatus("SET_HS_CONFIG_DESCR", Status);
    OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_FS_CONFIG_DESCR, (int) NULL, (uint8_t *)CyFxUSBFSConfigDscr);
    CheckStatus("SET_FS_CONFIG_DESCR", Status);
    OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 0, (uint8_t *)CyFxUSBStringLangIDDscr);
    CheckStatus("SET_STRING0_DESCR", Status);
    OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 1, (uint8_t *)CyFxUSBManufactureDscr);
    CheckStatus("SET_STRING1_DESCR", Status);
    OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 2, (uint8_t *)CyFxUSBProductDscr);
    CheckStatus("SET_STRING2_DESCR", Status);
    OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 3, (uint8_t *)CyFxUSBInterface0Dscr);
    CheckStatus("SET_STRING3_DESCR", Status);
    OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 4, (uint8_t *)CyFxUSBInterface1Dscr);
    CheckStatus("SET_STRING4_DESCR", Status);
    OverallStatus |= Status = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 5, (uint8_t *)CyFxUSBInterface2Dscr);
    CheckStatus("SET_STRING5_DESCR", Status);

    return OverallStatus;
}
