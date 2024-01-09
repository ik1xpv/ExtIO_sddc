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
    void Close(void);
    
    libusb_ss_endpoint_companion_descriptor *ep_comp_desc;
    libusb_device *device;
    libusb_device_descriptor descriptor;
    libusb_config_descriptor *config;
    libusb_device_handle *hDevice;
    libusb_endpoint_descriptor *epBulkIn;
    const libusb_interface *interface;
    const libusb_interface_descriptor *altsetting;

private:
    
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

#endif // __LIBUSB_HPP__