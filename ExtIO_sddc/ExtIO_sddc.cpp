#include "license.txt" 

#define EXTIO_EXPORTS

#include "framework.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "config.h"
#include "ExtIO_sddc.h"
#include "RadioHandler.h"
#include "FX3class.h"
#include "uti.h"
#include "tdialog.h"
#include "splashwindow.h"
#include "PScope_uti.h"

#define   snprintf	_snprintf

static bool SDR_settings_valid = false;		// assume settings are for some other ExtIO
static char SDR_progname[32 + 1] = "\0";
static int  SDR_ver_major = -1;
static int  SDR_ver_minor = -1;
static const int	gHwType = exthwUSBfloat32;
int  giExtSrateIdxVHF = 4;
int  giExtSrateIdxHF = 4;

bool bSupportDynamicSRate;

int64_t	glLOfreq = 2000000;
int64_t	glTunefreq = 999000;	// Turin MW broadcast !

bool	gbInitHW = false;
int		giAttIdxHF = 0;
int		giAttIdxVHF = 0;

int		giMgcIdxHF = 0;
int		giMgcIdxVHF = 0;

pfnExtIOCallback	pfnCallback = 0;
HWND Hconsole;

static bool gshdwn;

RadioHandlerClass RadioHandler;

// Dialog callback

HWND h_dialog = NULL;

SplashWindow  splashW;

#define IDD_SDDC_SETTINGS	100

//---------------------------------------------------------------------------

HMODULE hInst;

//---------------------------------------------------------------------------

extern "C"
BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		hInst = hModule;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

std::mutex readyMutex;
std::condition_variable readyCV;
std::thread readyThread;

#define READY_QUEUE_SIZE 128
float* dataQueue[READY_QUEUE_SIZE];
int read_index;
int write_index;

void* callbackthread()
{
	for(;;)
	{
		int index;
		if (read_index == write_index) {
			std::unique_lock<std::mutex> lk(readyMutex);
			readyCV.wait(lk, [] {
					return read_index != write_index;
				});
		}

		index = read_index;
		read_index++;
		read_index = read_index % READY_QUEUE_SIZE;

		pfnCallback(EXT_BLOCKLEN, 0, 0.0F, dataQueue[index]);
	}
}

static void Callback(float* data, uint32_t len)
{
	if (data)
	{
		std::unique_lock<std::mutex> lk(readyMutex);
		dataQueue[write_index] = data;
		write_index++;
		write_index = write_index % READY_QUEUE_SIZE;

		readyCV.notify_one();
	}
	else
	{
		pfnCallback(-1, extHw_Stop, 0.0F, 0); // Stop realtime see Failures
	}
}

//---------------------------------------------------------------------------
extern "C"
bool __declspec(dllexport) __stdcall InitHW(char *name, char *model, int& type)
{
//	EnterFunction();  Not yet ready debug console
	type = gHwType;
	if (!gbInitHW)
	{
		// do initialization
		h_dialog = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DLG_MAIN), NULL, (DLGPROC)&DlgMainFn);
		RECT rect;
		GetWindowRect(h_dialog, &rect);
		SetWindowPos(h_dialog, HWND_TOPMOST, 0, 24, rect.right - rect.left, rect.bottom - rect.top, SWP_HIDEWINDOW);
		splashW.createSplashWindow(hInst, IDB_BITMAP2, 15, 15, 15);

#ifdef _DEBUG
		if (AllocConsole())
		{
			freopen("CONOUT$", "wt", stdout);
			SetConsoleTitle(TEXT("Debug Black Box Console ExtIO"));
			Hconsole = GetConsoleWindow();
			RECT rect;
			GetWindowRect(GetDesktopWindow(), &rect);
			SetWindowPos(Hconsole, HWND_TOPMOST, rect.right - 800, 24, 800, 420, SWP_SHOWWINDOW);
			DbgPrintf((char *) "Oscar Steila IK1XPV fecit MMXVIII - MMXX\n");
			MakeWindowTransparent(Hconsole, 0xC0);
		}
#endif
		EnterFunction();  // now works

		// open the data
		unsigned char* res_data;
		uint32_t res_size;

		FILE *fp = fopen("SDDC_FX3.img", "rb");
		if (fp != nullptr)
		{
			fseek(fp, 0, SEEK_END);
			res_size = ftell(fp);
			res_data = (unsigned char*)malloc(res_size);
			fseek(fp, 0, SEEK_SET);
			fread(res_data, 1, res_size, fp);
		}
		else
		{
			HRSRC res = FindResource(hInst, MAKEINTRESOURCE(RES_BIN_FIRMWARE), RT_RCDATA);
			HGLOBAL res_handle = LoadResource(hInst, res);
			res_data = (unsigned char*)LockResource(res_handle);
			res_size = SizeofResource(hInst, res);
		}

		auto Fx3 = CreateUsbHandler();
		gbInitHW = Fx3->Open(res_data, res_size) &&
				   RadioHandler.Init(Fx3, Callback); // Check if it there hardware
		if (!gbInitHW)
		{
			MessageBox(NULL, "Is SDR powered on and connected ?\r\n\r\nPlease start HDSDR again",
				"WARNING SDR NOT FOUND", MB_OK | MB_ICONWARNING);
				ExitProcess(0); // exit without saving settings
			return gbInitHW;
		}

		strcpy(name, RadioHandler.getName());
		strcpy(model, RadioHandler.getName());

		DbgPrintf("Init Values:\n");
		DbgPrintf("SDR_settings_valid = %d\n", SDR_settings_valid);  // settings are version specific !
		DbgPrintf("giExtSrateIdxHF = %d   %f Msps\n", giExtSrateIdxHF, pow(2.0, 1.0 + giExtSrateIdxHF));
		DbgPrintf("giExtSrateIdxVHF = %d   %f Msps\n", giExtSrateIdxVHF, pow(2.0, 1.0 + giExtSrateIdxVHF));
		DbgPrintf("giAttIdxHF = %d\n", giAttIdxHF);
		DbgPrintf("giAttIdxVHF = %d\n", giAttIdxVHF);
		DbgPrintf("giMgcIdxHF = %d\n", giMgcIdxHF);
		DbgPrintf("giMgcIdxVHF = %d\n", giMgcIdxVHF);
		DbgPrintf("glTunefreq = %ld\n", (long int)glTunefreq);
		DbgPrintf("______________________________________\n");
		DbgPrintf("adcfixedfreq = %ld\n", (long int)RadioHandler.getSampleRate());
		DbgPrintf("adcfixedfr/4 = %ld\n", (long int)(RadioHandler.getSampleRate() / 4.0));
		DbgPrintf("______________________________________\n");

		read_index = write_index = 0;
		readyThread = std::thread(
			[](void *){
				return callbackthread();
			}, nullptr);
	}

	EXTIO_STATUS_CHANGE(pfnCallback, extHw_READY);

	return gbInitHW;
}

//---------------------------------------------------------------------------
extern "C"
bool EXTIO_API OpenHW(void)
{
	EnterFunction();

	// .... display here the DLL panel ,if any....

//    ShowWindow(h_dialog, SW_SHOW);
//     ShowWindow(h_dialog, SW_HIDE);
#ifndef _DEBUG
	splashW.showWindow();
#endif

	//EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_ATT);

	splashW.destroySplashWindow();

	SetWindowPos(h_dialog, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

	// in the above statement, F->handle is the window handle of the panel displayed
	// by the DLL, if such a panel exists
	//init_AdcSamplesProc();  // allocate buffers...
	return gbInitHW;
}

//---------------------------------------------------------------------------
extern "C"
int  EXTIO_API StartHW(long LOfreq)
{
    EnterFunction();
	int64_t ret = StartHW64((int64_t)LOfreq);
	return (int)ret;
}

//---------------------------------------------------------------------------
extern "C"
int64_t EXTIO_API StartHW64(int64_t LOfreq)
{
	EnterFunction1((int) LOfreq);
	if (!gbInitHW)
		return 0;

	RadioHandler.Start(ExtIoGetActualSrateIdx());
	SetHWLO64(LOfreq);

	if (RadioHandler.IsReady()) //  HF103 connected
	{
		char ebuffer[64];
		uint16_t fw = RadioHandler.GetFirmware();
		uint8_t hb, lb;
		hb = fw >> 8;
		lb = (uint8_t) fw;
		sprintf(ebuffer, "%s v%0.2f  |  FX3 v%d.%02d  |  %s ",SWNAME, VERSION ,hb,lb, RadioHandler.getName() );
		SetWindowText(h_dialog, ebuffer);
		EXTIO_STATUS_CHANGE(pfnCallback, extHw_RUNNING);
	}
	else
	{
		MessageBox(NULL, "HDSDR will exit\r\nPlease verify USB connection\r\nand restart",
				"WARNING SDR not found", MB_OK | MB_ICONWARNING);
		EXTIO_STATUS_CHANGE(pfnCallback, extHw_Disconnected);
		SendF4();
	}
	// number of complex elements returned each
	// invocation of the callback routine
	return (int64_t) EXT_BLOCKLEN;
}

//---------------------------------------------------------------------------
extern "C"
void EXTIO_API StopHW(void)
{
    EnterFunction();
	RadioHandler.Stop();
	if (Failures > 0)
	{
		MessageBox(NULL, "Please close box\r\nand press Start",
		"WARNING transfer fails", MB_OK | MB_ICONWARNING);
		EXTIO_STATUS_CHANGE(pfnCallback, extHw_ERROR);
	}
	else
	{
		EXTIO_STATUS_CHANGE(pfnCallback, extHw_READY);
	}

	return;
}

//---------------------------------------------------------------------------
extern "C"
void EXTIO_API CloseHW(void)
{
    EnterFunction();
	// ..... here you can shutdown your graphical interface, if any............
	if (h_dialog != NULL)
		DestroyWindow(h_dialog);
	if (gbInitHW)
	{
		RadioHandler.Close();
	}
	gbInitHW = false;
}

/*
ShowGUI
void __stdcall __declspec(dllexport) ShowGUI(void)
This entry point is used by Winrad to tell the DLL that the user did ask to see the GUI of the DLL itself, if it has one.
The implementation of this call is optional 
It has  no return value.
*/
extern "C"
void EXTIO_API ShowGUI()
{
	EnterFunction();
	ShowWindow(h_dialog, SW_SHOW);
	SetForegroundWindow(h_dialog);
	return;
}

/*
HideGUI
void __stdcall __declspec(dllexport) HideGUI(void)
This entry point is used by Winrad to tell the DLL that it has to hide its GUI, if it has one. The implementation of
this call is optional 
It has  no return value
*/
extern "C"
void EXTIO_API HideGUI()
{
	EnterFunction();
	ShowWindow(h_dialog, SW_HIDE);
	return;
}

extern "C"
void EXTIO_API SwitchGUI()
{
    EnterFunction();

	if (IsWindowVisible(h_dialog))
	{
		ShowWindow(h_dialog, SW_HIDE);
	}
	else
	{
		ShowWindow(h_dialog, SW_SHOW);
	}
	return;
}

//---------------------------------------------------------------------------
extern "C"
int  EXTIO_API SetHWLO(long LOfreq)
{
	EnterFunction();
	int64_t ret = SetHWLO64((int64_t)LOfreq);
	return (ret & 0xFFFFFFFF);
}

extern "C"
int64_t EXTIO_API SetHWLO64(int64_t LOfreq)
{
	EnterFunction1((int) LOfreq );
	// ..... set here the LO frequency in the controlled hardware
	// Set here the frequency of the controlled hardware to LOfreq
	const int64_t wishedLO = LOfreq;
	int64_t ret = 0;

	if (RadioHandler.getModel() == HF103) // HF frequency limits
	{
		if (glTunefreq > HF_HIGH) glTunefreq = HF_HIGH;
		if (LOfreq > HF_HIGH - 1000000) LOfreq = (HF_HIGH) - 1000000;
	}

	rf_mode rfmode = RadioHandler.GetmodeRF();
	if ((LOfreq > 32000000) && (rfmode != VHFMODE))
	{
			RadioHandler.UpdatemodeRF(VHFMODE);
			ExtIoSetMGC(giMgcIdxVHF);
			SetAttenuator(giAttIdxVHF);
			ExtIoSetSrate(giExtSrateIdxVHF);

			EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_SRATES);
			EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_RF_IF);

			if (giExtSrateIdxHF != giExtSrateIdxVHF)
				EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_SampleRate);
	}
	if ((LOfreq < 31000000) && (rfmode != HFMODE))
	{
			RadioHandler.UpdatemodeRF(HFMODE);
			ExtIoSetMGC(giMgcIdxHF);
			SetAttenuator(giAttIdxHF);
			ExtIoSetSrate(giExtSrateIdxHF);

			EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_SRATES);
			EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_RF_IF);

			if (giExtSrateIdxHF != giExtSrateIdxVHF)
				EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_SampleRate);
	}

	LOfreq = RadioHandler.TuneLO(LOfreq);
	if (wishedLO != LOfreq)
	{
		EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_LO);
	}
	glLOfreq = LOfreq;

	// 0 The function did complete without errors.
	// < 0 (a negative number N)
	//     The specified frequency  is  lower than the minimum that the hardware  is capable to generate.
	//     The absolute value of N indicates what is the minimum supported by the HW.
	// > 0 (a positive number N) The specified frequency is greater than the maximum that the hardware
	//     is capable to generate.
	//     The value of N indicates what is the maximum supported by the HW.
	return ret;
}


//---------------------------------------------------------------------------
extern "C"
int  EXTIO_API GetStatus(void)
{
    EnterFunction();

	return 0;  // status not supported by this specific HW,
}

//---------------------------------------------------------------------------
extern "C"
void EXTIO_API SetCallback(pfnExtIOCallback funcptr)
{
    EnterFunction();
	pfnCallback = funcptr;
	return;
}


//---------------------------------------------------------------------------
extern "C"
long EXTIO_API GetHWLO(void)
{
    EnterFunction();

	return (long)(glLOfreq & 0xFFFFFFFF);
}

extern "C"
int64_t EXTIO_API GetHWLO64(void)
{
	EnterFunction1((int) glLOfreq);
	return glLOfreq;
}

//---------------------------------------------------------------------------
extern "C"
long EXTIO_API GetHWSR(void)
{
    EnterFunction();

	double newSrate;
	int srate;
	long sampleRate;
	if (RadioHandler.GetmodeRF() == VHFMODE)
		srate = giExtSrateIdxVHF;
	else
		srate = giExtSrateIdxHF;

	if (0 == ExtIoGetSrates(srate, &newSrate))
	{
		sampleRate = (unsigned)(newSrate + 0.5);
	}
	return sampleRate;

}


//---------------------------------------------------------------------------
extern "C" 
long EXTIO_API GetTune(void)
{
	return  (long) GetTune64();
}

// extern "C" void EXTIO_API GetFilters(int& loCut, int& hiCut, int& pitch);
// extern "C" char EXTIO_API GetMode(void);
// extern "C" void EXTIO_API ModeChanged(char mode);
// extern "C" void EXTIO_API IFLimitsChanged(long low, long high);
extern "C" 
void    EXTIO_API TuneChanged(long freq)
{ 
	TuneChanged64(freq);
}
extern "C" 
void    EXTIO_API TuneChanged64(int64_t freq)
{
	EnterFunction1((int)freq);
	glTunefreq = freq;
}
extern "C" int64_t EXTIO_API GetTune64(void)
{
    EnterFunction1((int) glTunefreq);
	return glTunefreq;
}
// extern "C" void    EXTIO_API IFLimitsChanged64(int64_t low, int64_t high);

//---------------------------------------------------------------------------

// extern "C" void EXTIO_API RawDataReady(long samplerate, int *Ldata, int *Rdata, int numsamples)

//---------------------------------------------------------------------------
extern "C"
void EXTIO_API VersionInfo(const char * progname, int ver_major, int ver_minor)
{
    EnterFunction();

	SDR_progname[0] = 0;
	SDR_ver_major = -1;
	SDR_ver_minor = -1;

	if (progname)
	{
		strncpy(SDR_progname, progname, sizeof(SDR_progname) - 1);
		SDR_ver_major = ver_major;
		SDR_ver_minor = ver_minor;

		if (strcmp(SDR_progname, "HDSDR") == 0)
		  bSupportDynamicSRate = true;
		else
		  bSupportDynamicSRate = false;
	}
}

extern "C"
int EXTIO_API GetAttenuators(int atten_idx, float* attenuation)
{
	// fill in attenuation
	// use positive attenuation levels if signal is amplified (LNA)
	// use negative attenuation levels if signal is attenuated
	// sort by attenuation: use idx 0 for highest attenuation / most damping
	// this functions is called with incrementing idx
	//    - until this functions return != 0 for no more attenuator setting
    EnterFunction();

	const float *steps;
	int max_step = RadioHandler.GetRFAttSteps(&steps);
	if (atten_idx < max_step) {
		*attenuation = steps[atten_idx];
		return 0;
	}

	return 1;
}

extern "C"
int EXTIO_API GetActualAttIdx(void)
{
	int AttIdx;
	if (RadioHandler.GetmodeRF() == VHFMODE)
		AttIdx = giAttIdxVHF;
	else
		AttIdx = giAttIdxHF;

	const float *steps;
	int max_step = RadioHandler.GetRFAttSteps(&steps);
	if (AttIdx >= max_step)
	{
		AttIdx = max_step - 1;
	}

	EnterFunction1(AttIdx);
	return AttIdx;
}

extern "C"
int EXTIO_API SetAttenuator(int atten_idx)
{
    EnterFunction1(atten_idx);
	RadioHandler.UpdateattRF(atten_idx);
	if (RadioHandler.GetmodeRF() == VHFMODE)
		giAttIdxVHF = atten_idx;
	else
		giAttIdxHF = atten_idx;

	EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_ATT);

	return 0;
}

//
// MGC
// sort by ascending gain: use idx 0 for lowest gain
// this functions is called with incrementing idx
//    - until this functions returns != 0, which means that all gains are already delivered
extern "C" int EXTIO_API ExtIoGetMGCs(int mgc_idx, float * gain)
{
    EnterFunction();

	const float *steps;
	int max_step = RadioHandler.GetIFGainSteps(&steps);
	if (mgc_idx < max_step) {
		*gain = steps[mgc_idx];
		return 0;
	}

	return 1;
}

// returns -1 on error
extern "C" int EXTIO_API ExtIoGetActualMgcIdx(void)
{
	int MgcIdx;
	if (RadioHandler.GetmodeRF() == VHFMODE)
		MgcIdx = giMgcIdxVHF;
	else
		MgcIdx = giMgcIdxHF;

	const float *steps;
	int max_step = RadioHandler.GetIFGainSteps(&steps);
	if (MgcIdx >= max_step)
	{
		MgcIdx = max_step - 1;
	}

	EnterFunction1(MgcIdx);
	return MgcIdx;
}

// returns != 0 on error
extern "C"  int EXTIO_API ExtIoSetMGC(int mgc_idx)
{
    EnterFunction1(mgc_idx);
	RadioHandler.UpdateIFGain(mgc_idx);
	if (RadioHandler.GetmodeRF() == VHFMODE)
		giMgcIdxVHF = mgc_idx;
	else
		giMgcIdxHF = mgc_idx;

	EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_MGC);
	return 0;
}

//---------------------------------------------------------------------------
// fill in possible samplerates
extern "C"
int EXTIO_API ExtIoGetSrates(int srate_idx, double * samplerate)
{
	EnterFunction1(srate_idx);
	double div = pow(2.0, srate_idx);
	*samplerate = 1000000.0 * (div * 2);
	if (*samplerate / RadioHandler.getSampleRate() * 2.0 > 1.1)
		return -1;
     DbgPrintf("*ExtIoGetSrate idx %d  %e\n", srate_idx, *samplerate);
	return 0;
}

extern "C"
int  EXTIO_API ExtIoGetActualSrateIdx(void)
{
    EnterFunction();
	if (RadioHandler.GetmodeRF() == VHFMODE)
		return giExtSrateIdxVHF;
	else
		return giExtSrateIdxHF;
}

extern "C"
int  EXTIO_API ExtIoSetSrate(int srate_idx)
{
    EnterFunction1(srate_idx);
	double newSrate = 0.0;

	if (0 == ExtIoGetSrates(srate_idx, &newSrate))
	{
		if (RadioHandler.GetmodeRF() == VHFMODE)
			giExtSrateIdxVHF = srate_idx;
		else
			giExtSrateIdxHF = srate_idx;

		EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_SampleRate);

		return 0;
	}
	return 1;	// ERROR
}

//---------------------------------------------------------------------------
// will be called (at least) before exiting application
extern "C"
int  EXTIO_API ExtIoGetSetting(int idx, char * description, char * value)
{
    EnterFunction();

	switch (idx)
	{
	case 0: strcpy(description, "Identifier");	snprintf(value, 1024, "%s", SETTINGS_IDENTIFIER);	return 0;
	case 1:	strcpy(description, "RadioHWtype");	snprintf(value, 1024, "%d", RadioHandler.getModel());			return 0;
	case 2:	strcpy(description, "HF_SampleRateIdx");	snprintf(value, 1024, "%d", giExtSrateIdxHF);			return 0;
	case 3: strcpy(description, "VHF_SampleRateIdx");	snprintf(value, 1024, "%d", giExtSrateIdxVHF);			return 0;
	case 4:	strcpy(description, "HF_AttenuationIdx");	snprintf(value, 1024, "%d", giAttIdxHF);			return 0;
	case 5:	strcpy(description, "HF_VGAIdx");	snprintf(value, 1024, "%d", giMgcIdxHF);			return 0;
	case 6:	strcpy(description, "VHF_AttenuationIdx");	snprintf(value, 1024, "%d", giAttIdxVHF);			return 0;
	case 7:	strcpy(description, "VHF_VGAIdx");	snprintf(value, 1024, "%d", giMgcIdxVHF);			return 0;
	case 8:	strcpy(description, "HF_Bias");   snprintf(value, 1024, "%d", 0);		return 0;
	case 9: strcpy(description, "VHF_Bias");   snprintf(value, 1024, "%d", 0);		return 0;
	case 10: strcpy(description, "LoFrequencyHz");   snprintf(value, 1024, "%ld",(unsigned long) glLOfreq); return 0;
	case 11: strcpy(description, "TuneFrequencyHz");   snprintf(value, 1024, "%ld", (unsigned long) glTunefreq); return 0;
	default: return -1;	// ERROR
	}
	return -1;	// ERROR
}


extern "C"
void EXTIO_API ExtIoSetSetting(int idx, const char * value)
{
    EnterFunction();

	double newSrate;
	float  newAtten = 0.0F;
	int tempInt;
	unsigned long tempulong;

	// now we know that there's no need to save our settings into some (.ini) file,
	// what won't be possible without admin rights!!!,
	// if the program (and ExtIO) is installed in C:\Program files\..
	if (idx != 0 && !SDR_settings_valid)
		return;	// ignore settings for some other ExtIO

	switch (idx)
	{
	case 0:
		SDR_settings_valid = (value && !strcmp(value, SETTINGS_IDENTIFIER));
		// settings are version specific ! 
		break;
	case 1:
		break;
	case 2:
		tempInt = atoi(value);
		if (0 == ExtIoGetSrates(tempInt, &newSrate))
		{
			giExtSrateIdxHF = tempInt;
		}
		break;
	case 3:
		tempInt = atoi(value);
		if (0 == ExtIoGetSrates(tempInt, &newSrate))
		{
			giExtSrateIdxVHF = tempInt;
		}
		break;
	case 4:
		tempInt = atoi(value);
		giAttIdxHF = tempInt;
		break;
	case 5:
		tempInt = atoi(value);
		giMgcIdxHF = tempInt;
		break;
	case 6:
		tempInt = atoi(value);
		giAttIdxVHF = tempInt;
		break;
	case 7:
		tempInt = atoi(value);
		giMgcIdxVHF = tempInt;
		break;
	case 10:
		if (sscanf(value, "%ld", &tempulong) > 0)
		{
			glLOfreq = (int64_t) tempulong;
		}
		else
			glLOfreq = 2000000;
		break;
	case 11:
		if (sscanf(value, "%ld", &tempulong) > 0)
		{
			glTunefreq = (int64_t)tempulong;
		}
		else
			glTunefreq = 999000; //default
		break;

	  break;
	}

}



//---------------------------------------------------------------------------
