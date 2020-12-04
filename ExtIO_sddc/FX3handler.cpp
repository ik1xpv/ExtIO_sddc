//
// FX3handler.cpp 
// 2020 10 12  Oscar Steila ik1xpv
// loading arm code.img from resource by Howard Su and Hayati Ayguen
// This module was previous named:
// openFX3.cpp MIT License Copyright (c) 2016 Booya Corp.
// booyasdr@gmail.com, http://booyasdr.sf.net
// modified 2017 11 30 ik1xpv@gmail.com, http://www.steila.com/blog
// 

#include "FX3handler.h"
#include "resource.h" // for RES_BIN_FIRMWARE

using namespace std;

CCyUSBEndPoint* EndPt;

fx3class::fx3class():
	fx3dev (nullptr),
	Fx3IsOn (false)
{

}


fx3class::~fx3class() // reset USB device and exit
{
	Control(RESETFX3);
	DbgPrintf("\r\n~fx3class\r\n");
	Close();
}

bool fx3class::GetFx3Device() { 
	bool r = false;
	if (fx3dev == nullptr) return r; // no device
	int n = fx3dev->DeviceCount();
	if (n == 0)  return r;   // no one

    // Walk through all devices looking for VENDOR_ID/STREAMER_ID	
	for (int i = 0; i <= n; i++) {
		fx3dev->Open(i); // go down the list of devices to find our device
		if ((fx3dev->VendorID == VENDOR_ID) && (fx3dev->ProductID == STREAMER_ID))
		{
			r = true;
			break;
		}

		if ((fx3dev->VendorID == VENDOR_ID) && (fx3dev->ProductID == BOOTLOADER_ID))
		{
			r = true;
			break;
		}
	}
	if (r == false)
		fx3dev->Close();
	return r;
}

bool fx3class::GetFx3DeviceStreamer(void) {   // open class 
	bool r = false;
	if (fx3dev == NULL) return r;
	int n = fx3dev->DeviceCount();
	// Walk through all devices looking for VENDOR_ID/STREAMER_ID
	if (n == 0) return r; 
	// go down the list of devices to find STREAMER_ID device
	for (int i = 0; i <= n; i++) {
		fx3dev->Open(i);
		if ((fx3dev->VendorID == VENDOR_ID) && (fx3dev->ProductID == STREAMER_ID))
		{
			r = true;
			break;
		}
	}
	if (r == false)
		fx3dev->Close();
	return r;
}

bool  fx3class::Open(HMODULE hInst) {
	bool r = false;
	fx3dev = new CCyFX3Device;              // instantiate the device
	if (fx3dev == nullptr) return r;        // return if failed
	int n = fx3dev->DeviceCount();          
	if (n == 0) return r;					// return if no devices connected
	if (!GetFx3Device()) return r;          // NO FX3 device connected
	char fwname[] = "SDDC_FX3.img";        // firmware file
	const char* fw_source = "external file";

	if (!fx3dev->IsBootLoaderRunning()) {   // if not bootloader device
		Control(RESETFX3);                  // reset the fx3 firmware via CyU3PDeviceReset(false)
		Sleep(300);							// wait restart
		fx3dev->Close();					// close fx3dev
		delete fx3dev;						// delete class
		Sleep(300);
		fx3dev = new CCyFX3Device;			// create new fx3dev
		GetFx3Device();						// open class
	}
	FX3_FWDWNLOAD_ERROR_CODE dlf = FAILED;
	while (fx3dev->IsBootLoaderRunning())
	{
		dlf = fx3dev->DownloadFw(fwname, RAM);
		if (dlf == INVALID_FILE)
		{
			HRSRC res = FindResource(hInst, MAKEINTRESOURCE(RES_BIN_FIRMWARE), RT_RCDATA);
			if (!res)
				break;
			HGLOBAL res_handle = LoadResource(hInst, res);
			if (!res_handle)
				break;
			const unsigned char* res_data = (const unsigned char*)LockResource(res_handle);
			DWORD res_size = SizeofResource(hInst, res);
			if (!res_data || res_size <= 0)
				break;
			dlf = fx3dev->DownloadFwToRam(res_data, res_size);
			fw_source = "internal resource";
		}
		break;
	}
	if (dlf == 0) {
		Sleep(500); // wait for download to finish
		struct stat stbuf;
		stat(fwname, &stbuf);
		char* timestr;
		timestr = ctime(&stbuf.st_mtime);
		DbgPrintf("Loaded NEW FIRMWARE %s from %s at %s", fwname, fw_source, timestr);
	}
	else if (dlf != 0)
	{
		DbgPrintf("MISSING/OLD FIRMWARE\n");
	}
	int x = 0;
	int maxretry = 30;
	CCyFX3Device* expdev = nullptr;
	while (x++ < maxretry) // wait new firmware setup
	{
		bool r = false;
		expdev = new CCyFX3Device;              // instantiate the device
		if (expdev != NULL)
			int n = expdev->DeviceCount();      
		if (n > 0)
		{
			expdev->Open(0);
			// go down the list of devices to find our device
			for (int i = 1; i <= n; i++)
			{
				if ((expdev->VendorID == VENDOR_ID) && (expdev->ProductID == STREAMER_ID))
				{
					x = maxretry;	//got it exit
				}
			}
		}
		expdev->Close();            // close class
		delete expdev;              // destroy class
	}
	GetFx3DeviceStreamer();         // open class with new ram firmware
	if (!fx3dev->IsOpen()) {
		DbgPrintf("Failed to open device\n");
		return r;
	}
	EndPt = fx3dev->BulkInEndPt;
	if (!EndPt) {
		DbgPrintf("No Bulk In end point\n");
		return r;      // init failed
	}

	long pktSize = EndPt->MaxPktSize;
	EndPt->SetXferSize(transferSize);
	long ppx = transferSize / pktSize;
	DbgPrintf("buffer transferSize = %d. packet size = %ld. packets per transfer = %ld\n"
		, transferSize, pktSize, ppx);

	Fx3IsOn = true;
	return Fx3IsOn;          // init success
}


using namespace std;

bool fx3class::Control(FX3Command command) { // firmware control
	long lgt = 1;
	UINT8 z = 0; // dummy data = 0
	fx3dev->ControlEndPt->ReqCode = command;
	bool r = fx3dev->ControlEndPt->Write(&z, lgt);
	DbgPrintf("FX3FWControl %x .%x\n", r, command);
	return r;
}

bool fx3class::Control(FX3Command command, UINT8 data) { // firmware control BBRF
	long lgt = 1;

	fx3dev->ControlEndPt->ReqCode = command;
	fx3dev->ControlEndPt->Value = (USHORT)0;
	fx3dev->ControlEndPt->Index = (USHORT)0;
	bool r = fx3dev->ControlEndPt->Write(&data, lgt);
	DbgPrintf("FX3FWControl %x .%x %x\n", r, command, data);
	if (r == false)
	{
		Close();
	}
	return r;
}

bool fx3class::Control(FX3Command command, UINT32 data) { // firmware control BBRF
	long lgt = 4;

	fx3dev->ControlEndPt->ReqCode = command;
	fx3dev->ControlEndPt->Value = (USHORT)0;
	fx3dev->ControlEndPt->Index = (USHORT)0;
	bool r = fx3dev->ControlEndPt->Write((PUCHAR)&data, lgt);
	DbgPrintf("FX3FWControl %x .%x %x\n", r, command, data);
	if (r == false)
	{
		Close();
	}
	return r;
}

bool fx3class::Control(FX3Command command, UINT64 data) { // firmware control BBRF
	long lgt = 8;

	fx3dev->ControlEndPt->ReqCode = command;
	fx3dev->ControlEndPt->Value = (USHORT)0;
	fx3dev->ControlEndPt->Index = (USHORT)0;
	bool r = fx3dev->ControlEndPt->Write((PUCHAR)&data, lgt);
	DbgPrintf("FX3FWControl %x .%x %llx\n", r, command, data);
	if (r == false)
	{
		Close();
	}
	return r;
}


bool fx3class::SetArgument(UINT16 index, UINT16 value) { // firmware control BBRF
	long lgt = 1;
	uint8_t data = 0;

	fx3dev->ControlEndPt->ReqCode = SETARGFX3;
	fx3dev->ControlEndPt->Value = (USHORT)value;
	fx3dev->ControlEndPt->Index = (USHORT)index;
	bool r = fx3dev->ControlEndPt->Write((PUCHAR)&data, lgt);
	DbgPrintf("SetArgument %x .%x (%x, %x)\n", r, SETARGFX3, index, value);
	if (r == false)
	{
		Close();
	}
	return r;
}

bool fx3class::GetHardwareInfo(UINT32* data) { // firmware control BBRF
	long lgt = 4;

	fx3dev->ControlEndPt->ReqCode = TESTFX3;
	fx3dev->ControlEndPt->Value = (USHORT)0;
	fx3dev->ControlEndPt->Index = (USHORT)0;
	lgt = 4; // TESTFX3 len
	bool r = fx3dev->ControlEndPt->Read((PUCHAR)data, lgt);
	DbgPrintf("GetHardwareInfo %x .%x %x\n", r, TESTFX3, *data);
	if (r == false)
	{
		Close();
	}
	return r;

}

bool fx3class::SendI2cbytes(UINT8 i2caddr, UINT8 regaddr, PUINT8 pdata, UINT8 len)
{
	bool r = false;
	LONG lgt = len;
	fx3dev->ControlEndPt->ReqCode = I2CWFX3;
	fx3dev->ControlEndPt->Value = (USHORT)i2caddr;
	fx3dev->ControlEndPt->Index = (USHORT)regaddr;
	Sleep(10);
	r = fx3dev->ControlEndPt->Write(pdata, lgt);
	if (r == false)
		DbgPrintf("\nfx3FWSendI2cbytes 0x%02x regaddr 0x%02x  1data 0x%02x len 0x%02x \n",
			i2caddr, regaddr, *pdata, len);
	return r;
}

bool fx3class::ReadI2cbytes(UINT8 i2caddr, UINT8 regaddr, PUINT8 pdata, UINT8 len)
{
	bool r = false;
	LONG lgt = len;
	WORD saveValue, saveIndex;
	saveValue = fx3dev->ControlEndPt->Value;
	saveIndex = fx3dev->ControlEndPt->Index;

	fx3dev->ControlEndPt->ReqCode = I2CRFX3;
	fx3dev->ControlEndPt->Value = (USHORT)i2caddr;
	fx3dev->ControlEndPt->Index = (USHORT)regaddr;
	r = fx3dev->ControlEndPt->Read(pdata, lgt);
	if (r == false)
		printf("fx3FWReadI2cbytes %x : %02x %02x %02x %02x : %02x\n", r, I2CRFX3, i2caddr, regaddr, len, *pdata);
	fx3dev->ControlEndPt->Value = saveValue;
	fx3dev->ControlEndPt->Index = saveIndex;
	return r;
}



bool fx3class::Close() {
	fx3dev->Close();            // close class
	delete fx3dev;              // destroy class
	Fx3IsOn = false;
	return true;
}




