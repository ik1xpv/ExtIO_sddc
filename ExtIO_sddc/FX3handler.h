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

// HF103 commands !!!
enum FX3Command {
	STARTFX3 = 0xAA,
	STOPFX3 = 0xAB,
	TESTFX3 = 0xAC,
	GPIOFX3 = 0xAD,
	I2CWFX3 = 0xAE,
	I2CRFX3 = 0xAF,
	DAT31FX3 = 0xB0,
	RESETFX3 = 0xB1,
	SI5351A = 0xB2,
	SI5351ATUNE = 0xB3,
	R820T2INIT = 0xB4,
	R820T2TUNE = 0xB5,
	R820T2SETATT = 0xB6,
	R820T2GETATT = 0xB7,
	R820T2STDBY = 0xB8
};

extern CCyUSBEndPoint* EndPt;

class fx3class
{
public:
	fx3class();
	~fx3class(void);
	bool Open(HMODULE hInst);
	bool IsOn() { return Fx3IsOn; }
	void Control(FX3Command command);
	bool Control(FX3Command command, PUINT8 data);
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
