#include "../LibusbHandler.h"
#include "libusb.hpp"
#include "../../config.h"


bool USBDevice::CreateHandle(UCHAR dev) {
    libusb_device **devs;
    libusb_device_handle *dev_handle = NULL;
    int r; //for return values
    ssize_t cnt; //holding number of devices in list
    int deviceNumber = static_cast<int>(dev);

    r = libusb_init(NULL); //initialize the library for the session we just declared
    if(r < 0) {
        return false;
    }

    cnt = libusb_get_device_list(NULL, &devs); //get the list of devices
    if(cnt < 0) {
        libusb_exit(NULL);
        return false;
    }

    if(deviceNumber >= cnt) { // if the requested device number is greater than available devices
        libusb_free_device_list(devs, 1); //free the list, unref the devices in it
        libusb_exit(NULL);
        return false;
    }

    r = libusb_open(devs[deviceNumber], &dev_handle); //open the device
    if(r != LIBUSB_SUCCESS) {
        libusb_free_device_list(devs, 1);
        libusb_exit(NULL);
        return false;
    }

    libusb_free_device_list(devs, 1); //free the list, unref the devices in it

    // Store the device handle in your class' member variable, replacing the Windows HANDLE.
    this->hDevice = dev_handle; // Assuming 'hDeviceHandle' is of type 'libusb_device_handle*'

    return true;
}


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

