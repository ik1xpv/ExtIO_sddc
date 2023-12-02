#include "LibusbHandler.h"
#include "../config.h"
#include "libusb-1.0/libusb.h"
#include "libusb/libusb.hpp"
#include <cstring>

LibusbHandler::LibusbHandler() :
	fx3dev (nullptr),
	Fx3IsOn (false),
	devidx (0)
{
    (void)devidx;
	DbgPrintf("\r\nLibusbHandler\r\n");
}


LibusbHandler::~LibusbHandler() // reset USB device and exit
{
	DbgPrintf("\r\n~LibusbHandler\r\n");
	//Close();
}

bool LibusbHandler::GetFx3DeviceStreamer()
{
    DbgPrintf("\r\nGetFx3DeviceStreamer\r\n");
    bool r = false;
    if (fx3dev == nullptr) return r;

    return true;
}

bool LibusbHandler::Enumerate(unsigned char& idx, char* lbuf, size_t bufSize, const uint8_t* fw_data, uint32_t fw_size)
{
    DbgPrintf("\r\nEnumerate\r\n");
    bool r = false;
    strncpy(lbuf, "", bufSize);
    lbuf[bufSize - 1] = '\0';
    
    if (fx3dev == nullptr) {
        fx3dev = new FX3Device();
    }

    if (fx3dev == nullptr) {
        return r;
    }

    if(!fx3dev->Open(idx)) {
        return r;
    }

    
    return true;
}

bool LibusbHandler::Open(const uint8_t* fw_data, uint32_t fw_size)
{
    DbgPrintf("\r\nOpen\r\n");
    int r = false;
    if (!GetFx3DeviceStreamer()) {
		DbgPrintf("Failed to open device\n");
		return r;
	}

    fx3dev->device = libusb_get_device(fx3dev->hDevice);
    r = libusb_get_device_descriptor(fx3dev->device, &fx3dev->descriptor);
    if (r < 0) {
        DbgPrintf("Failed to get device descriptor\n");
        return r;
    }

    r = libusb_get_config_descriptor(fx3dev->device, 0, &fx3dev->config);
    if (r < 0) {
        DbgPrintf("Failed to get config descriptor\n");
        return r;
    }

    fx3dev->interface = &fx3dev->config->interface[0];
    fx3dev->altsetting = &fx3dev->interface->altsetting[0];

    for (int i = 0; i < fx3dev->altsetting->bNumEndpoints; i++) {
        const libusb_endpoint_descriptor *ep = &fx3dev->altsetting->endpoint[i];
        if ((ep->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK) {
            if (ep->bEndpointAddress & LIBUSB_ENDPOINT_IN) {
                fx3dev->epBulkIn = (libusb_endpoint_descriptor *)ep;
            }
        }
    }
    unsigned short PktsPerFrame = (fx3dev->epBulkIn->wMaxPacketSize & 0x1800) >> 11;
    int pktSize = (fx3dev->epBulkIn->wMaxPacketSize & 0x7ff) * (PktsPerFrame + 1);

    //long pktSize = fx3dev->epBulkIn->wMaxPacketSize;
    
    long ppx = transferSize / pktSize;
    DbgPrintf("buffer transferSize = %d. packet size = %ld. packets per transfer = %ld\n"
		, transferSize, pktSize, ppx);

	uint8_t data[4];
    GetHardwareInfo((uint32_t*)&data);

    if (data[1] != FIRMWARE_VER_MAJOR ||
		data[2] != FIRMWARE_VER_MINOR)
	{
		DbgPrintf("Firmware version mismatch %d.%d != %d.%d (actual)\n", FIRMWARE_VER_MAJOR, FIRMWARE_VER_MINOR, data[1], data[2]);
		Control(RESETFX3);
		return false;
	}

	Fx3IsOn = true;
	return Fx3IsOn;          // init success
}

bool LibusbHandler::Control(FX3Command command, uint8_t data)
{
    DbgPrintf("\r\nControl\r\n");
    return true;
}

bool LibusbHandler::Control(FX3Command command, uint32_t data)
{
    EnterFunction();
     uint8_t requestType = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;
    uint8_t request = static_cast<uint8_t>(command);
    uint16_t value = 0;
    uint16_t index = 0;
    unsigned char buffer[4];
    int length = 4;

    // Prepare data for sending
    memcpy(buffer, &data, sizeof(UINT32));

    int r = libusb_control_transfer(fx3dev->hDevice, requestType, request, value, index, buffer, length, 0);

    DbgPrintf("FX3FWControl %x .%x %x\n", r, command, data);

    if (r < 0) {
        // Handle the error
        Close();
        return false;
    }

    return true;

   
}

bool LibusbHandler::Control(FX3Command command, uint64_t data)
{
    DbgPrintf("\r\nControl\r\n");
    return true;
}

bool LibusbHandler::SetArgument(uint16_t index, uint16_t value)
{
    DbgPrintf("\r\nSetArgument\r\n");
    return true;
}

bool LibusbHandler::GetHardwareInfo(uint32_t* data)
{
    DbgPrintf("\r\nGetHardwareInfo\r\n");


    uint8_t requestType = LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;
    uint8_t request = TESTFX3;
    uint16_t value = 0;
    uint16_t index = 0;
    unsigned char buffer[4];
    int length = 4;

#ifdef _DEBUG
    value = 1;
#endif

    int r = libusb_control_transfer(fx3dev->hDevice, requestType, request, value, index, buffer, length, 0);

    if (r < 0) {
        // Handle the error
        DbgPrintf("GetHardwareInfo failed: %d\n", r);
        Close();
        return false;
    }

    // Assuming data is a 32-bit integer and buffer contains 4 bytes
    memcpy(data, buffer, sizeof(UINT32));
    DbgPrintf("GetHardwareInfo %x .%x %x\n", r, TESTFX3, *data);

    return true;
    
}

bool LibusbHandler::ReadDebugTrace(uint8_t* pdata, uint8_t len)
{
    DbgPrintf("\r\nReadDebugTrace\r\n");
    return true;
}

bool LibusbHandler::SendI2cbytes(uint8_t i2caddr, uint8_t regaddr, uint8_t* pdata, uint8_t len)
{
    DbgPrintf("\r\nSendI2cbytes\r\n");
    return true;
}

bool LibusbHandler::ReadI2cbytes(uint8_t i2caddr, uint8_t regaddr, uint8_t* pdata, uint8_t len)
{
    DbgPrintf("\r\nReadI2cbytes\r\n");
    return true;
}

bool LibusbHandler::Close(void)
{
    DbgPrintf("\r\nClose\r\n");
    return true;
}

bool LibusbHandler::BeginDataXfer(uint8_t *buffer, long transferSize, void** context)
{
    DbgPrintf("\r\nBeginDataXfer\r\n");
    return true;
}

bool LibusbHandler::FinishDataXfer(void** context)
{
    DbgPrintf("\r\nFinishDataXfer\r\n");
    return true;
}

void LibusbHandler::CleanupDataXfer(void** context)
{
    DbgPrintf("\r\nCleanupDataXfer\r\n");
}

void LibusbHandler::AdcSamplesProcess()
{
    DbgPrintf("\r\nAdcSamplesProcess\r\n");
}

void LibusbHandler::StartStream(ringbuffer<int16_t>& input, int numofblock)
{
    DbgPrintf("\r\nStartStream\r\n");
}

void LibusbHandler::StopStream()
{
    DbgPrintf("\r\nStopStream\r\n");
}


