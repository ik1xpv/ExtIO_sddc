#ifndef __LIBUSB_HPP__
#define __LIBUSB_HPP__

#include <stdint.h>
#if defined(_WIN32) 
    #include <libusb-1.0/libusb.h>
#elif defined(__APPLE__)
    #include <libusb.h>
#else
    #error "Unsupported platform"   
#endif

class USBDevice {

public:

    USBDevice(libusb_context *ctx = nullptr, bool open = true);
    ~USBDevice();

    char DeviceName[256];

    unsigned char DeviceCount(void);
    bool CreateHandle(unsigned char dev);
    bool Open(uint8_t dev);
    

    libusb_device *device;
    libusb_device_descriptor descriptor;
    libusb_config_descriptor *config;
private:
    //libusb_context *ctx;
    libusb_device_handle *hDevice;

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
    UCHAR lowByte;
    UCHAR hiByte;
} WORD_SPLIT, *PWORD_SPLIT;

typedef struct _BM_REQ_TYPE {
    UCHAR   Recipient:2;
    UCHAR   Reserved:3;
    UCHAR   Type:2;
    UCHAR   Direction:1;
} BM_REQ_TYPE, *PBM_REQ_TYPE;

typedef struct _SETUP_PACKET {

    union {
        BM_REQ_TYPE bmReqType;
        UCHAR bmRequest;
    };

    UCHAR bRequest;

    union {
        WORD_SPLIT wVal;
        USHORT wValue;
    };

    union {
        WORD_SPLIT wIndx;
        USHORT wIndex;
    };

    union {
        WORD_SPLIT wLen;
        USHORT wLength;
    };

    ULONG ulTimeOut;

} SETUP_PACKET, *PSETUP_PACKET;

typedef struct _ISO_ADV_PARAMS {

    USHORT isoId;
    USHORT isoCmd;

    ULONG ulParam1;
    ULONG ulParam2;

} ISO_ADV_PARAMS, *PISO_ADV_PARAMS;


typedef struct _SINGLE_TRANSFER {
    union {
        SETUP_PACKET SetupPacket;
        ISO_ADV_PARAMS IsoParams;
    };

    UCHAR reserved;

    UCHAR ucEndpointAddress;
    ULONG NtStatus;
    ULONG UsbdStatus;
    ULONG IsoPacketOffset;
    ULONG IsoPacketLength;
    ULONG BufferOffset;
    ULONG BufferLength;
} SINGLE_TRANSFER, *PSINGLE_TRANSFER;

#endif // __LIBUSB_HPP__