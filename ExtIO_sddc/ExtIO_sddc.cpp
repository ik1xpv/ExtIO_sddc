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
#include "r2iq.h"

#define   snprintf	_snprintf

#define DEFAULT_TUNE_FREQ	999000.0	/* Turin MW broadcast ! */

static bool SDR_settings_valid = false;		// assume settings are for some other ExtIO
static char SDR_progname[2048 + 1] = "\0";
static int  SDR_ver_major = -1;
static int  SDR_ver_minor = -1;
static const int	gHwType = exthwUSBfloat32;
int  giExtSrateIdxVHF = 4;
int  giExtSrateIdxHF = 4;

bool bSupportDynamicSRate;

double	gfLOfreq = 2000000.0;
#if EXPORT_EXTIO_TUNE_FUNCTIONS
double	gfTunefreq = DEFAULT_TUNE_FREQ;
#endif
double	gfFreqCorrectionPpm = 0.0;

bool	gbInitHW = false;
int		giAttIdxHF = 0;
int		giAttIdxVHF = 0;

int		giMgcIdxHF = 0;
int		giMgcIdxVHF = 0;

pfnExtIOCallback	pfnCallback = 0;
HWND Hconsole;

static bool gshdwn;
static bool needSaveSettings;
const char RegKeyName[] = "SOFTWARE\\SDDC";


RadioHandlerClass RadioHandler;

// Dialog callback

HWND h_dialog = NULL;

DevContext  devicelist; // list of FX3 devices

SplashWindow  splashW;

#define IDD_SDDC_SETTINGS	100

static int SetSrateInternal(int srate_idx, bool internal_call = true);

static void LoadSettings();
static void SaveSettings();

static inline
double FreqCorrectionFactor()
{
	return 1.0 + gfFreqCorrectionPpm / 1E6;
}

std::mutex mtx_print;           // mutex for critical section

// printf console from ExtIO debug
void dbgprintf(const char* fmt, ...) {
	mtx_print.lock();
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	mtx_print.unlock();
}
// printf console from FX3 via USB callback
static void printf_USB_cb(const char* fmt, ...) {
	mtx_print.lock();
	SetConsoleColorTXT(TXT_CYAN);
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	SetConsoleColorTXT(TXT_GREEN);
	mtx_print.unlock();
}

static bool GetConsoleInput(char* buf, int maxlen)
{
	DWORD nevents = 0;
	INPUT_RECORD irInBuf[128];
	KEY_EVENT_RECORD key;
	int i;
	bool rc = false;
	int counter = 0;
	if (Hconsole == nullptr) return rc;
	HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
	if (GetNumberOfConsoleInputEvents(hInput, &nevents))
	{
		if (nevents > 0)
		{
			if (!ReadConsoleInput(hInput, irInBuf, 128, &nevents))
			{
				dbgprintf("ReadConsoleInput error\n");
				return rc;
			}
			for (i = 0; i < nevents; i++)
			{
				if (irInBuf[i].EventType == KEY_EVENT)
				{
					if (((KEY_EVENT_RECORD&)irInBuf[i].Event).bKeyDown)
					{
						buf[counter] = ((KEY_EVENT_RECORD&)irInBuf[i].Event).uChar.AsciiChar;
						rc = true;
						if (counter < maxlen - 2) counter++;
					}
				}
			}
			buf[counter] = 0; // terminate string
		}
	}
	return rc;
}

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

static void Callback(const float* data, uint32_t len)
{
	if (data)
	{
		pfnCallback(len, 0, 0.0F, (void*)data);
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
			SetConsoleColorTXT(TXT_GREEN);
		}
#endif
		EnterFunction();  // now works

		// open the data
		unsigned char* res_data;
		uint32_t res_size;

#ifdef _DEBUG
		FILE *fp = fopen("TestFX3.img", "rb");
		if (fp != nullptr)
		{
			fseek(fp, 0, SEEK_END);
			res_size = ftell(fp);
			res_data = (unsigned char*)malloc(res_size);
			fseek(fp, 0, SEEK_SET);
			fread(res_data, 1, res_size, fp);
		}
		else
#endif
		{
			HRSRC res = FindResource(hInst, MAKEINTRESOURCE(RES_BIN_FIRMWARE), RT_RCDATA);
			HGLOBAL res_handle = LoadResource(hInst, res);
			res_data = (unsigned char*)LockResource(res_handle);
			res_size = SizeofResource(hInst, res);
		}

		auto Fx3 = CreateUsbHandler();
		unsigned char idx = 0;
		int selected = 0;
		while (Fx3->Enumerate(idx, devicelist.dev[idx], res_data, res_size) && (idx < MAXNDEV))
		{
			// https://en.wikipedia.org/wiki/West_Bridge
			int retry = 2;
			while ((strncmp("WestBridge", devicelist.dev[idx],sizeof("WestBridge")) != NULL) && retry-- > 0)
				Fx3->Enumerate(idx, devicelist.dev[idx], res_data, res_size); // if it enumerates as BootLoader retry
			idx++;
		}
		devicelist.numdev = idx;
		if (idx > 1){	
			selected =  DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_SELECTDEVICE), NULL, DlgSelectDevice, (LPARAM) &devicelist);
		}
		DbgPrintf("selected %d \n",selected);
		idx = selected;
		Fx3->Enumerate(idx, devicelist.dev[idx], res_data, res_size);

		gbInitHW = Fx3->Open(res_data, res_size) &&
				RadioHandler.Init(Fx3, Callback); // Check if it there hardware
	
#ifdef _DEBUG
			RadioHandler.EnableDebug( printf_USB_cb , GetConsoleInput);
#endif 
  
		if (!gbInitHW)
		{
			MessageBox(NULL, "Is SDR powered on and connected ?\r\n\r\nPlease start HDSDR again",
				"WARNING SDR NOT FOUND", MB_OK | MB_ICONWARNING);
				ExitProcess(0); // exit without saving settings
			return gbInitHW;
		}

		strcpy(name, RadioHandler.getName());
		strcpy(model, RadioHandler.getName());

		double srate;
		DbgPrintf("Init Values:\n");
		DbgPrintf("SDR_settings_valid = %d\n", SDR_settings_valid);  // settings are version specific !
		ExtIoGetSrates(giExtSrateIdxHF, &srate);
		DbgPrintf("giExtSrateIdxHF = %d   %f Msps\n", giExtSrateIdxHF, srate/1000000.0);
		ExtIoGetSrates(giExtSrateIdxVHF, &srate);
		DbgPrintf("giExtSrateIdxVHF = %d   %f Msps\n", giExtSrateIdxVHF, srate/1000000.0);
		DbgPrintf("giAttIdxHF = %d\n", giAttIdxHF);
		DbgPrintf("giAttIdxVHF = %d\n", giAttIdxVHF);
		DbgPrintf("giMgcIdxHF = %d\n", giMgcIdxHF);
		DbgPrintf("giMgcIdxVHF = %d\n", giMgcIdxVHF);
#if EXPORT_EXTIO_TUNE_FUNCTIONS
		DbgPrintf("gfTunefreq = %lf\n", gfTunefreq);
#endif
		DbgPrintf("______________________________________\n");
		DbgPrintf("adcfixedfreq = %ld\n", (long int)RadioHandler.getSampleRate());
		DbgPrintf("adcfixedfr/4 = %ld\n", (long int)(RadioHandler.getSampleRate() / 4.0));
		DbgPrintf("______________________________________\n");
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

#ifndef _DEBUG
	splashW.showWindow();
#endif


	splashW.destroySplashWindow();
	// do initialization
//   verify if HDSDR host name 
	if (strstr(SDR_progname, "HDSDR") == nullptr)
	{
		h_dialog = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DLG_MAIN), NULL, (DLGPROC)&DlgMainFn);

		// load system settings
		needSaveSettings = true;
		LoadSettings();
	}
	else
	{
		if (( SDR_ver_major >= 2) && (SDR_ver_minor >=81))
			h_dialog = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DLG_HDSDR281), NULL, (DLGPROC)&DlgMainFn);
		else
			h_dialog = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DLG_HDSDR), NULL, (DLGPROC)&DlgMainFn);
	}
	RECT rect;
	GetWindowRect(h_dialog, &rect);
	SetWindowPos(h_dialog, HWND_TOPMOST, 0, 24, rect.right - rect.left, rect.bottom - rect.top, SWP_HIDEWINDOW);

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
	int ret = StartHWdbl((double)LOfreq);
	return ret;
}

//---------------------------------------------------------------------------
extern "C"
int EXTIO_API StartHW64(int64_t LOfreq)
{
	EnterFunction();
	int ret = StartHWdbl((double)LOfreq);
	return ret;
}

//---------------------------------------------------------------------------
extern "C"
int EXTIO_API StartHWdbl(double LOfreq)
{
	EnterFunction1((int) LOfreq);
	if (!gbInitHW)
		return 0;

	RadioHandler.Start(ExtIoGetActualSrateIdx());
	SetHWLOdbl(LOfreq);

	if (RadioHandler.IsReady()) //  HF103 connected
	{
		char ebuffer[64];
		uint16_t fw = RadioHandler.GetFirmware();
		uint8_t hb, lb;
		hb = fw >> 8;
		lb = (uint8_t) fw;
		sprintf(ebuffer, "%s v%s | FX3 v%d.%02d | %s ",SWNAME, SWVERSION ,hb,lb, RadioHandler.getName() );
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
	return (int64_t) (EXT_BLOCKLEN);
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

	if (needSaveSettings)
	{
		SaveSettings();
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
	double retDbl = SetHWLOdbl((double)LOfreq);
	int64_t ret = (int64_t)retDbl;
	return (ret & 0xFFFFFFFF);
}

extern "C"
int64_t EXTIO_API SetHWLO64(int64_t LOfreq)
{
	EnterFunction();
	double retDbl = SetHWLOdbl((double)LOfreq);
	int64_t ret = (int64_t)retDbl;
	return (ret & 0xFFFFFFFF);
}

extern "C"
double EXTIO_API SetHWLOdbl(double LOfreq)
{
	EnterFunction1((int) LOfreq );
	// ..... set here the LO frequency in the controlled hardware
	// Set here the frequency of the controlled hardware to LOfreq
	const double wishedLO = LOfreq;
	double ret = 0;
	rf_mode rfmode = RadioHandler.GetmodeRF();
	rf_mode newmode = RadioHandler.PrepareLo(LOfreq);

	if (newmode == NOMODE) // this freq is not supported
		return -1;

	if ((newmode == VHFMODE) && (rfmode != VHFMODE))
	{
			RadioHandler.UpdatemodeRF(VHFMODE);
			ExtIoSetMGC(giMgcIdxVHF);
			SetAttenuator(giAttIdxVHF);
			SetSrateInternal(giExtSrateIdxVHF, false);

			EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_SRATES);
			EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_RF_IF);

			if (giExtSrateIdxHF != giExtSrateIdxVHF)
				EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_SampleRate);
	}
	else if ((newmode == HFMODE) && (rfmode != HFMODE))
	{
			RadioHandler.UpdatemodeRF(HFMODE);
			ExtIoSetMGC(giMgcIdxHF);
			SetAttenuator(giAttIdxHF);
			SetSrateInternal(giExtSrateIdxHF, false);

			EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_SRATES);
			EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_RF_IF);

			if (giExtSrateIdxHF != giExtSrateIdxVHF)
				EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_SampleRate);
	}

	double internal_LOfreq = LOfreq / FreqCorrectionFactor();
	internal_LOfreq = RadioHandler.TuneLO(internal_LOfreq);
	gfLOfreq = LOfreq = internal_LOfreq * FreqCorrectionFactor();
	if (wishedLO != LOfreq)
	{
		EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_LO);
	}

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
	int64_t ret = (int64_t)gfLOfreq;
	return (long)(ret & 0xFFFFFFFF);
}

extern "C"
int64_t EXTIO_API GetHWLO64(void)
{
	int64_t ret = (int64_t)gfLOfreq;
	EnterFunction1((int) ret);
	return ret;
}

extern "C"
double EXTIO_API GetHWLOdbl(void)
{
	double ret = gfLOfreq;
	EnterFunction1((int)ret);
	return gfLOfreq;
}

//---------------------------------------------------------------------------
extern "C"
long EXTIO_API GetHWSR(void)
{
	double srate = GetHWSRdbl();
	return (long)srate;
}

extern "C"
double EXTIO_API GetHWSRdbl(void)
{
	EnterFunction();
	double newSrate = 1E6;
	int srateIdx;
	if (RadioHandler.GetmodeRF() == VHFMODE)
		srateIdx = giExtSrateIdxVHF;
	else
		srateIdx = giExtSrateIdxHF;

	if (0 != ExtIoGetSrates(srateIdx, &newSrate))
		newSrate = 2E6;  // fallback
	return newSrate;
}

//---------------------------------------------------------------------------

#if EXPORT_EXTIO_TUNE_FUNCTIONS

extern "C"
long EXTIO_API GetTune(void)
{
	int64_t ret = (int64_t)gfTunefreq;
	return (long)(ret & 0xFFFFFFFF);
}

extern "C"
int64_t EXTIO_API GetTune64(void)
{
	int64_t ret = (int64_t)gfTunefreq;
	return ret;
}

extern "C"
double EXTIO_API GetTunedbl(void)
{
	int64_t ret = (int64_t)gfTunefreq;
	EnterFunction1((int)ret);
	return gfTunefreq;
}

extern "C"
void    EXTIO_API TuneChanged(long freq)
{ 
	TuneChanged64(freq);
}

extern "C"
void    EXTIO_API TuneChanged64(int64_t freq)
{
	EnterFunction1((int)freq);
	gfTunefreq = (double)freq;
}

extern "C"
void    EXTIO_API TuneChangeddbl(double freq)
{
	int64_t t = (int64_t)freq;
	EnterFunction1((int)t);
	gfTunefreq = freq;
}

#endif

//---------------------------------------------------------------------------
// extern "C" void EXTIO_API GetFilters(int& loCut, int& hiCut, int& pitch);
// extern "C" char EXTIO_API GetMode(void);
// extern "C" void EXTIO_API ModeChanged(char mode);
// extern "C" void EXTIO_API IFLimitsChanged(long low, long high);
// extern "C" void    EXTIO_API IFLimitsChanged64(int64_t low, int64_t high);
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
	double srateM = div * 2.0;
	double bwmin = adcnominalfreq / 64.0;
	if (adcnominalfreq > N2_BANDSWITCH) bwmin /= 2.0;
	double srate = bwmin * srateM;

	if (srate / adcnominalfreq * 2.0 > 1.1)
		return -1;

	*samplerate = srate * FreqCorrectionFactor();
	DbgPrintf("*ExtIoGetSrate idx %d  %e\n", srate_idx, *samplerate);
	return 0;
}

extern "C"
int EXTIO_API ExtIoSrateSelText(int srate_idx, char* text)
{
	EnterFunction1(srate_idx);
	double div = pow(2.0, srate_idx);
	double srateM = div * 2.0;
	double bwmin = adcnominalfreq / 64.0;
	if (adcnominalfreq > N2_BANDSWITCH) bwmin /= 2.0;
	double srate = bwmin * srateM;
	if ((srate / adcnominalfreq) * 2.0 > 1.1)
		return -1;
	snprintf(text, 15, "%.1lf MHz", srate/1000000);
	return 0;	// return != 0 on error
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

static int SetSrateInternal(int srate_idx, bool internal_call)
{
	EnterFunction1(srate_idx);
	double newSrate = 0.0;

	if (0 == ExtIoGetSrates(srate_idx, &newSrate))
	{
		if (RadioHandler.GetmodeRF() == VHFMODE)
			giExtSrateIdxVHF = srate_idx;
		else
			giExtSrateIdxHF = srate_idx;

		if (internal_call)
			EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_SampleRate);

		return 0;
	}
	return 1;	// ERROR
}


extern "C"
int EXTIO_API ExtIoSetSrate(int srate_idx)
{
	EnterFunction1(srate_idx);
	return SetSrateInternal(srate_idx, false);
}

extern "C"
int SetOverclock(uint32_t adcfreq)
{
	adcnominalfreq = adcfreq;
	RadioHandler.UpdateSampleRate(adcfreq);
	int index = ExtIoGetActualSrateIdx();
	double rate;
	while (ExtIoGetSrates(index, &rate) == -1)
	{
		index--;
	}
	SetSrateInternal(index);

	EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_RF_IF);
	EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_SampleRate);
	EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_SRATES);

	RadioHandler.Start(ExtIoGetActualSrateIdx());
	double internal_LOfreq = gfLOfreq / FreqCorrectionFactor();
	RadioHandler.TuneLO(internal_LOfreq);
	return 0;
}

//---------------------------------------------------------------------------
// will be called by HDSDR with reference correction in ppm
extern "C"
void EXTIO_API SetPPMvalue(double new_ppm_value)
{
	gfFreqCorrectionPpm = new_ppm_value;
	SetOverclock(adcnominalfreq);
	UpdatePPM(h_dialog);  // update dialog PPM
}

//---------------------------------------------------------------------------
// will be called by HDSDR with reference correction in ppm
extern "C"
double EXTIO_API GetPPMvalue(void)
{
	return gfFreqCorrectionPpm;
}

//---------------------------------------------------------------------------
//  base audio if rate 
extern "C"
void EXTIO_API IFrateInfo(int rate)
{
	// TODO

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
	case 10: strcpy(description, "LoFrequencyHz");   snprintf(value, 1024, "%lf", gfLOfreq); return 0;
#if EXPORT_EXTIO_TUNE_FUNCTIONS
	case 11: strcpy(description, "TuneFrequencyHz");   snprintf(value, 1024, "%lf", gfTunefreq); return 0;
#else
	case 11: strcpy(description, "TuneFrequencyHz");   snprintf(value, 1024, "%lf", DEFAULT_TUNE_FREQ); return 0;
#endif
	case 12: strcpy(description, "Correction_ppm");   snprintf(value, 1024, "%lf", gfFreqCorrectionPpm); return 0;
	case 13: strcpy(description, "ADC_nominal_freq");   snprintf(value, 1024, "%d", adcnominalfreq); return 0;
	default: return -1;	// ERROR
	}
	return -1;	// ERROR
}


extern "C"
void EXTIO_API ExtIoSetSetting(int idx, const char * value)
{
    EnterFunction();

	double newSrate;
	double tempDbl;
	float  newAtten = 0.0F;
	int tempInt;
	uint32_t tempuint32;

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
		gfLOfreq = 2000000.0;
		if (sscanf(value, "%lf", &tempDbl) > 0)
			gfLOfreq = tempDbl;
		break;
	case 11:
#if EXPORT_EXTIO_TUNE_FUNCTIONS
		gfTunefreq = 999000.0; // default
		if (sscanf(value, "%lf", &tempDbl) > 0)
			gfTunefreq = tempDbl;
#endif
		break;
	case 12:
		gfFreqCorrectionPpm = 0.0;
		if (sscanf(value, "%lf", &tempDbl) > 0)
			gfFreqCorrectionPpm = tempDbl;
		break;
	case 13:
		adcnominalfreq = DEFAULT_ADC_FREQ;
		if (sscanf(value, "%d", &tempuint32) > 0)
			adcnominalfreq = tempuint32;
		break;

	default:
		break;
	}

}


void SaveSettings()
{
	HKEY handle;
	DWORD diposition;
	RegCreateKeyEx(HKEY_CURRENT_USER, RegKeyName, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &handle, &diposition);

	// enum all the settings
	int idx = 0;
	char desc[256];
	char value[1024];
	int ret = ExtIoGetSetting(idx, desc, value);
	while(ret == 0)
	{
		char idxvalue[30];
		sprintf(idxvalue, "%03d_Value", idx);
		// write the value
		RegSetValueEx(handle, idxvalue, 0, REG_SZ, (const BYTE*)value, strlen(value));

		sprintf(idxvalue, "%03d_Description", idx);
		// write the value
		RegSetValueEx(handle, idxvalue, 0, REG_SZ, (const BYTE*)desc, strlen(desc));
		// fetch next
		idx++;
		ret = ExtIoGetSetting(idx, desc, value);
	}

	CloseHandle(handle);
}

void LoadSettings()
{
	HKEY handle;
	DWORD diposition;
	RegCreateKeyEx(HKEY_CURRENT_USER, RegKeyName, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &handle, &diposition);

	// enum all the settings
	int idx = 0;
	char value[1024];
	char idxvalue[30];
	DWORD len;
	DWORD type;
	sprintf(idxvalue, "%03d_Value", idx);
	LSTATUS status = RegQueryValueEx(handle, idxvalue, 0, &type, (BYTE*)value, &len);

	while(status == ERROR_SUCCESS && type == REG_SZ)
	{
		ExtIoSetSetting(idx, value);

		// next setting
		idx++;
		sprintf(idxvalue, "%03d_Value", idx);

		len = 1023;
		status = RegQueryValueEx(handle, idxvalue, 0, &type, (BYTE*)value, &len);
	}

	CloseHandle(handle);
}

//---------------------------------------------------------------------------
