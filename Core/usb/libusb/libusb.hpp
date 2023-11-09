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

    unsigned char DeviceCount(void);
    bool Open(uint8_t dev);
    

    libusb_device *device;
    libusb_device_descriptor descriptor;
    libusb_config_descriptor *config;
private:
    //libusb_context *ctx;
    libusb_device_handle *hDevice;

};


class FX3Device : public USBDevice
{
public:
    FX3Device();
    ~FX3Device();
};
 
#endif // __LIBUSB_HPP__