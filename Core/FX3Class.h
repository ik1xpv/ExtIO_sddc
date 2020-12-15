#ifndef FX3CLASS_H
#define FX3CLASS_H

//
// FX3handler.cpp 
// 2020 10 12  Oscar Steila ik1xpv
// loading arm code.img from resource by Howard Su and Hayati Ayguen
// This module was previous named:openFX3.cpp
// MIT License Copyright (c) 2016 Booya Corp.
// booyasdr@gmail.com, http://booyasdr.sf.net
// modified 2017 11 30 ik1xpv@gmail.com, http://www.steila.com/blog
// 

#include <stdint.h>
#include "../Interface.h"

class fx3class
{
public:
	virtual ~fx3class(void) {}
	virtual bool Open(uint8_t* fw_data, uint32_t fw_size) = 0;
	virtual bool Control(FX3Command command, uint8_t data = 0) = 0;
	virtual bool Control(FX3Command command, uint32_t data) = 0;
	virtual bool Control(FX3Command command, uint64_t data) = 0;
	virtual bool SetArgument(uint16_t index, uint16_t value) = 0;
	virtual bool GetHardwareInfo(uint32_t* data) = 0;
	virtual bool GetOFArate(uint16_t* data) = 0;

	virtual bool BeginDataXfer(uint8_t *buffer, long transferSize, void** context) = 0;
	virtual bool FinishDataXfer(void** context) = 0;
	virtual void CleanupDataXfer(void** context) = 0;
};

extern "C" fx3class* CreateUsbHandler();

#endif // FX3CLASS_H
