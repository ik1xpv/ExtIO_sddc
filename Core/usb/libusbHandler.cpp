#include "LibusbHandler.h"
#include "../config.h"
#include "libusb/libusb.hpp"

/* IUsbHandler* CreateUsbHandler()
{
	return new LibusbHandler();
} */

LibusbHandler::LibusbHandler() :
	//fx3dev (nullptr),
	Fx3IsOn (false)
	//devidx (0)
{
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
    return true;
}

bool LibusbHandler::Control(FX3Command command, uint8_t data)
{
    DbgPrintf("\r\nControl\r\n");
    return true;
}

bool LibusbHandler::Control(FX3Command command, uint32_t data)
{
    DbgPrintf("\r\nControl\r\n");
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

