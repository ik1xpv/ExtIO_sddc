#ifndef FX3HANDLER_H
#define FX3HANDLER_H

#include "config.h"

#define	VENDOR_ID     (0x04B4)
#define	STREAMER_ID   (0x00F1)
#define	BOOTLOADER_ID (0x00F3)

#include "FX3Class.h"
#include "usb_device.h"


class fx3handler : public fx3class
{
public:
	fx3handler();
	virtual ~fx3handler(void);
	bool Open(uint8_t* fw_data, uint32_t fw_size) override;
	bool Control(FX3Command command, uint8_t data) override;
	bool Control(FX3Command command, uint32_t data) override;
	bool Control(FX3Command command, uint64_t data) override;
	bool SetArgument(uint16_t index, uint16_t value) override;
	bool GetHardwareInfo(uint32_t* data) override;

	bool BeginDataXfer(uint8_t *buffer, long transferSize, void** context) override;
	bool FinishDataXfer(void** context) override;
	void CleanupDataXfer(void** context) override;

private:
	bool ReadUsb(uint8_t command, uint16_t value, uint16_t index, uint8_t *data, size_t size);
	bool WriteUsb(uint8_t command, uint16_t value, uint16_t index, uint8_t *data, size_t size);
	usb_device_t *dev;
};


#endif // FX3HANDLER_H
