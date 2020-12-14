#include "FX3handler.h"
#include "usb_device.h"

fx3class* CreateUsbHandler()
{
	return new fx3handler();
}

fx3handler::fx3handler()
{
}

fx3handler::~fx3handler()
{
}

bool fx3handler::Open(uint8_t* fw_data, uint32_t fw_size)
{
    dev = usb_device_open(0, (const char*)fw_data, fw_size);

    return dev != nullptr;
}

bool fx3handler::Control(FX3Command command, uint8_t data)
{
    return usb_device_control(this->dev, command, 0, 0, (uint8_t *) &data, sizeof(data), 0) == 0;
}

bool fx3handler::Control(FX3Command command, uint32_t data)
{
    return usb_device_control(this->dev, command, 0, 0, (uint8_t *) &data, sizeof(data), 0) == 0;
}

bool fx3handler::Control(FX3Command command, uint64_t data)
{
    return usb_device_control(this->dev, command, 0, 0, (uint8_t *) &data, sizeof(data), 0) == 0;
}

bool fx3handler::SetArgument(uint16_t index, uint16_t value)
{
    uint8_t data = 0;
    return usb_device_control(this->dev, SETARGFX3, value, index, (uint8_t *) &data, sizeof(data), 0) == 0;
}

bool fx3handler::GetHardwareInfo(uint32_t* data)
{
    return usb_device_control(this->dev, TESTFX3, 0, 0, (uint8_t *) data, sizeof(*data), 1) == 0;
}

bool fx3handler::BeginDataXfer(uint8_t *buffer, long transferSize, void** context)
{
    //TODO
    return true;
}

bool fx3handler::FinishDataXfer(void** context)
{
    // TODO
    return true;
}

void fx3handler::CleanupDataXfer(void** context)
{
    //TODO
}