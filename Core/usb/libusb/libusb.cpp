#include "../LibusbHandler.h"
#include "libusb.hpp"
#include "../../config.h"


bool USBDevice::Open(uint8_t dev)
{
    DbgPrintf("\r\nUSBDevice::Open\r\n");
    return true;
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