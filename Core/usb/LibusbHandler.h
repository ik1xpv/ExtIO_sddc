#ifndef LIBUSB_HANDLER_HPP
#define LIBUSB_HANDLER_HPP


#include "../include/ringbuffer.h"
#include "IUsbHandler.h"
#include <stdint.h>

class FX3Device;
class USBEndPoint;
class LibusbHandler : public IUsbHandler
{
public:
	LibusbHandler();
	virtual ~LibusbHandler(void);

	bool Open(const uint8_t* fw_data, uint32_t fw_size);
	bool IsOn() { return Fx3IsOn; }
	bool Control(FX3Command command, uint8_t data);
	bool Control(FX3Command command, uint32_t data = 0);
	bool Control(FX3Command command, uint64_t data);
	bool SetArgument(uint16_t index, uint16_t value);
	bool GetHardwareInfo(uint32_t* data);
	bool ReadDebugTrace(uint8_t* pdata, uint8_t len);
	void StartStream(ringbuffer<int16_t>& input, int numofblock);
	void StopStream();
	bool Enumerate(unsigned char& idx, char* lbuf, size_t bufSize, const uint8_t* fw_data, uint32_t fw_size);
private:
	bool SendI2cbytes(uint8_t i2caddr, uint8_t regaddr, uint8_t* pdata, uint8_t len);
	bool ReadI2cbytes(uint8_t i2caddr, uint8_t regaddr, uint8_t* pdata, uint8_t len);

	bool BeginDataXfer(uint8_t *buffer, long transferSize, void** context);
	bool FinishDataXfer(void** context);
	void CleanupDataXfer(void** context);

	FX3Device* fx3dev;
	[[maybe_unused]] USBEndPoint* EndPt;

    [[maybe_unused]] std::thread *adc_samples_thread;

	bool GetFx3DeviceStreamer();
	bool Fx3IsOn;
	bool Close(void);
	void AdcSamplesProcess();

	[[maybe_unused]] ringbuffer<int16_t> *inputbuffer;
	[[maybe_unused]] int numofblock;
	[[maybe_unused]] bool run;
	uint8_t devidx;
};


#endif // LIBUSB_HPP