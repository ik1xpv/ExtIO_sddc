#include "../LibusbHandler.h"
#include "libusb.hpp"
#include "../../config.h"


bool USBDevice::Open(uint8_t dev)
{
    DbgPrintf("\r\nUSBDevice::Open\r\n");
    // Assuming 'device_handle' is of type 'libusb_device_handle*' and
    // it's a member of your device handling class.

    // If this device is already open, close it using libusb.
    if (hDevice != nullptr) {
        libusb_close(hDevice);
        hDevice = nullptr;  // Ensure the handle is marked invalid after closing.
    }

    return true;
}

unsigned char USBDevice::DeviceCount(void) {
    libusb_device **devs; // Pointer to pointer of device, used to retrieve a list of devices
    ssize_t count = 0;    // Number of devices in list
    ssize_t i = 0;        // Iterator variable

    // Initialize the library if it has not been initialized
    libusb_init(NULL); // Normally you would check for errors here

    // Get the list of devices
    count = libusb_get_device_list(NULL, &devs);
    if (count < 0) {
        // Error handling here
        libusb_exit(NULL); // Cleanup on error
        return 0;          // Return 0 devices in case of error
    }

    unsigned char DeviceCount = 0; // Count of devices with the desired characteristics

    for (i = 0; i < count; i++) {
        libusb_device *device = devs[i];
        libusb_device_descriptor desc;
        
        // Get the device descriptor for each device and increase the count if it matches your criteria
        if (libusb_get_device_descriptor(device, &desc) == 0) {
            // Here you would typically check if the device's vendor ID and product ID match your target device
            // For example: if (desc.idVendor == MY_VENDOR_ID && desc.idProduct == MY_PRODUCT_ID)
            DeviceCount++;
        }
    }

    // Free the list after use, unref_devices set to 1 to decrement the reference count on each device
    libusb_free_device_list(devs, 1);

    // Exit libusb
    libusb_exit(NULL);

    return DeviceCount;
}


FX3Device::FX3Device()
{
    DbgPrintf("\r\nFX3Device::FX3Device\r\n");
}

FX3Device::~FX3Device()
{
    DbgPrintf("\r\nFX3Device::~FX3Device\r\n");
}

USBDevice::USBDevice(libusb_context *ctx, bool open)
{
    DbgPrintf("\r\nUSBDevice::USBDevice\r\n");
}

USBDevice::~USBDevice()
{
    DbgPrintf("\r\nUSBDevice::~USBDevice\r\n");
}

