#ifndef FX3HANDLER_H
#define FX3HANDLER_H

//
// FX3handler.cpp 
// 2020 10 12  Oscar Steila ik1xpv
// loading arm code.img from resource by Howard Su and Hayati Ayguen
// This module was previous named:openFX3.cpp
// MIT License Copyright (c) 2016 Booya Corp.
// booyasdr@gmail.com, http://booyasdr.sf.net
// modified 2017 11 30 ik1xpv@gmail.com, http://www.steila.com/blog
// 

#include "framework.h"
#include <sys/stat.h>
#include <iostream>
#include <stdio.h>
#include <time.h>
#include "synchapi.h"
#include "CyAPI.h"
#include "config.h"

#define PUINT8 UINT8*

#define	VENDOR_ID     (0x04B4)
#define	STREAMER_ID   (0x00F1)
#define	BOOTLOADER_ID (0x00F3)

#include "../Interface.h"

extern CCyUSBEndPoint* EndPt;

class fx3class
{
public:
	fx3class();
	~fx3class(void);
	bool Open(HMODULE hInst);
	bool IsOn() { return Fx3IsOn; }
	bool Control(FX3Command command);
	bool Control(FX3Command command, UINT8 data);
	bool Control(FX3Command command, UINT32 data);
	bool Control(FX3Command command, UINT64 data);
	bool SetArgument(UINT16 index, UINT16 value);
	bool GetHardwareInfo(UINT32* data);

	bool SendI2cbytes(UINT8 i2caddr, UINT8 regaddr, PUINT8 pdata, UINT8 len);
	bool ReadI2cbytes(UINT8 i2caddr, UINT8 regaddr, PUINT8 pdata, UINT8 len);

private:
	CCyFX3Device* fx3dev;
	bool GetFx3Device();
	bool GetFx3DeviceStreamer();
	bool Fx3IsOn;
	bool Close(void);
};


#endif // FX3HANDLER_H
