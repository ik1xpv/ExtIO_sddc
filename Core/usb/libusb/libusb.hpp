#ifndef __LIBUSB_HPP__
#define __LIBUSB_HPP__

#include <stdint.h>

#if defined(_WIN32) 
    #include <libusb-1.0/libusb.h>
#elif defined(__APPLE__) || defined(__linux__)
    #include <libusb.h>
#else
    #error "Unsupported platform"   
#endif

const int USB_STRING_MAXLEN = 256;

class USBDevice {

public:

    USBDevice(libusb_context *ctx = nullptr, bool open = true);
    ~USBDevice();

    unsigned long LastError;

    char        DeviceName[USB_STRING_MAXLEN];
    char        FriendlyName[USB_STRING_MAXLEN];
    wchar_t     Manufacturer[USB_STRING_MAXLEN];
    wchar_t     Product[USB_STRING_MAXLEN];
    wchar_t     SerialNumber[USB_STRING_MAXLEN];

    unsigned char DeviceCount(void);
    bool CreateHandle(unsigned char dev);
    bool Open(uint8_t dev);
    

    libusb_device *device;
    libusb_device_descriptor descriptor;
    libusb_config_descriptor *config;
    libusb_device_handle *hDevice;
    libusb_endpoint_descriptor *epBulkIn;
    const libusb_interface *interface;
    const libusb_interface_descriptor *altsetting;

private:
    //libusb_context *ctx;

    int Devices;
    void GetDevDescriptor(void);
    void GetDeviceName(void);

};


class FX3Device : public USBDevice
{
public:
    FX3Device();
    ~FX3Device();
};

typedef struct _WORD_SPLIT {
    unsigned char lowByte;
    unsigned char hiByte;
} WORD_SPLIT, *PWORD_SPLIT;

typedef struct _BM_REQ_TYPE {
    unsigned char   Recipient:2;
    unsigned char   Reserved:3;
    unsigned char   Type:2;
    unsigned char   Direction:1;
} BM_REQ_TYPE, *PBM_REQ_TYPE;

typedef struct _SETUP_PACKET {

    union {
        BM_REQ_TYPE bmReqType;
        unsigned char bmRequest;
    };

    unsigned char bRequest;

    union {
        WORD_SPLIT wVal;
        unsigned short wValue;
    };

    union {
        WORD_SPLIT wIndx;
        unsigned short wIndex;
    };

    union {
        WORD_SPLIT wLen;
        unsigned short wLength;
    };

    unsigned long ulTimeOut;

} SETUP_PACKET, *PSETUP_PACKET;

typedef struct _ISO_ADV_PARAMS {

    unsigned short isoId;
    unsigned short isoCmd;

    unsigned long ulParam1;
    unsigned long ulParam2;

} ISO_ADV_PARAMS, *PISO_ADV_PARAMS;


typedef struct _SINGLE_TRANSFER {
    union {
        SETUP_PACKET SetupPacket;
        ISO_ADV_PARAMS IsoParams;
    };

    unsigned char reserved;

    unsigned char ucEndpointAddress;
    unsigned long NtStatus;
    unsigned long UsbdStatus;
    unsigned long IsoPacketOffset;
    unsigned long IsoPacketLength;
    unsigned long BufferOffset;
    unsigned long BufferLength;
} SINGLE_TRANSFER, *PSINGLE_TRANSFER;

#endif // __LIBUSB_HPP__