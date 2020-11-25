#include "license.txt" 

#define EXTIO_EXPORTS

#include "framework.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "mytypes.h"
#include "config.h"
#include "ExtIO_sddc.h"
#include "RadioHandler.h"
#include "FX3handler.h"
#include "uti.h"
#include "tdialog.h"
#include "r2iq.h"
#include "fftw3.h"
#include "splashwindow.h"
#include "PScope_uti.h"

#define   snprintf	_snprintf


static bool SDR_settings_valid = false;		// assume settings are for some other ExtIO
static char SDR_progname[32 + 1] = "\0";
static int  SDR_ver_major = -1;
static int  SDR_ver_minor = -1;
static const int	gHwType = exthwUSBfloat32;
int  giExtSrateIdx = 4;
int  giExtSrateIdxHF = 4;
unsigned    gExtSampleRate = ADC_FREQ / 2;

int64_t	glLOfreq = 2000000;
int64_t	glTunefreq = 999000;	// Turin MW broadcast !

bool	gbInitHW = false;
bool    LWMode = false;
int		giAttIdxHF = 0;
int		giAttIdxVHF = 0;

int		giMgcIdxHF = 0;
int		giMgcIdxVHF = 0;

pfnExtIOCallback	pfnCallback = 0;
HWND Hconsole;

static bool gshdwn;


// Dialog callback

HWND h_dialog = NULL;

SplashWindow  splashW;

#define IDD_SDDC_SETTINGS	100

void ADCFrequencyCorrection()
{
	switch (radio)
	{
	case HF103:
		adcfixedfreq = (double)ADC_FREQ * (1.0 + (gdFreqCorr_ppm * 0.000001));
		break;
	default: //BBRF103, RX888...  exact frequency, tuning made with Si5351a
		adcfixedfreq = (double)ADC_FREQ; 
		break;
	}
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

//---------------------------------------------------------------------------
extern "C"
bool __declspec(dllexport) __stdcall InitHW(char *name, char *model, int& type)
{
//	EnterFunction();  Not yet ready debug console
	type = gHwType;
	strcpy(name, HWNAME);
	strcpy(model, HWMODEL);
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
			SetConsoleTitle(TEXT("Debug Black Box Console ExtIO_" HWNAME));
			Hconsole = GetConsoleWindow();
			RECT rect;
			GetWindowRect(GetDesktopWindow(), &rect);
			SetWindowPos(Hconsole, HWND_TOPMOST, rect.right - 800, 24, 800, 420, SWP_SHOWWINDOW);
			DbgPrintf((char *) "Oscar Steila IK1XPV fecit MMXVIII - MMXX\n");
			MakeWindowTransparent(Hconsole, 0xC0);
		}
#endif	
		EnterFunction();  // now works
	
		gbInitHW = RadioHandler.Init(hInst); // Check if it there hardware
		if (!gbInitHW)
		{
			MessageBox(NULL, "Is SDR powered on and connected ?\r\n\r\nPlease start HDSDR again",
				"WARNING SDR NOT FOUND", MB_OK | MB_ICONWARNING);
				ExitProcess(0); // exit without saving settings
			return gbInitHW;
		}

		fftwf_import_wisdom_from_filename("wisdom");

		ADCFrequencyCorrection();

		DbgPrintf((char*)"Init values \n");
		DbgPrintf("SDR_settings_valid = %d \n", SDR_settings_valid);  // settings are version specific !
		DbgPrintf("giExtSrateIdx = %d   %f Msps \n", giExtSrateIdx, pow(2.0, 1.0 + giExtSrateIdx));
		DbgPrintf("giAttIdxHF = %d \n", giAttIdxHF);
		DbgPrintf("giAttIdxVHF = %d \n", giAttIdxVHF);
		DbgPrintf("giMgcIdxHF = %d \n", giMgcIdxHF);
		DbgPrintf("giMgcIdxVHF = %d \n", giMgcIdxVHF);
		DbgPrintf("gdGainCorr = %2.1f \n", gdGainCorr_dB);
		DbgPrintf("glTunefreq = %ld \n", (long int)glTunefreq);
		DbgPrintf("______________________________________\n");
		DbgPrintf("freqcorrection = %f \n", gdFreqCorr_ppm);
		DbgPrintf("adcfixedfreq = %ld \n", (long int)adcfixedfreq);
		DbgPrintf("adcfixedfr/4 = %ld \n", (long int)(adcfixedfreq / 4.0));
		DbgPrintf("______________________________________\n");
	}
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

	RadioHandler.Start();
	SetHWLO64(LOfreq);
	
	if (RadioHandler.IsReady()) //  HF103 connected
	{
		char ebuffer[64];
		uint16_t fw = RadioHandler.GetFirmware();
		uint8_t hb, lb;
		hb = fw >> 8;
		lb = (uint8_t) fw;
		sprintf(ebuffer, "%s v%0.2f  |  FX3 v%d.%02d  |  %s ",SWNAME, VERSION ,hb,lb, radioname[radio] );
		SetWindowText(h_dialog, ebuffer);
		if (radio == HF103)
			SetWindowText(GetDlgItem(h_dialog, IDC_RESTART), "Restart");
		else
			SetWindowText(GetDlgItem(h_dialog, IDC_RESTART), "Set");
	}
	else
	{
		MessageBox(NULL, "HDSDR will exit\r\nPlease verify USB connection\r\nand restart",
				"WARNING SDR not found", MB_OK | MB_ICONWARNING);
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
	fftwf_export_wisdom_to_filename("wisdom");
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

	if (radio == HF103) // HF frequency limits
	{
		if (glTunefreq > HF_HIGH) glTunefreq = HF_HIGH;
		if (LOfreq > HF_HIGH - 1000000) LOfreq = (HF_HIGH) - 1000000;  
	}

	rf_mode rfmode = RadioHandler.GetmodeRF();
	if ((LOfreq > 32000000) && (rfmode != VHFMODE))
	{
			if (rfmode != NOMODE) giExtSrateIdxHF = giExtSrateIdx;	// save HF SRate
			RadioHandler.UpdatemodeRF(VHFMODE);
			ExtIoSetMGC(giMgcIdxVHF);
			SetAttenuator(giAttIdxVHF);
			giExtSrateIdx = 2;
			r2iqCntrl.Setdecimate(giExtSrateIdx);
			if (pfnCallback) EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_SampleRate);
			if (pfnCallback) EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_TUNE);
			RedrawWindow(h_dialog, NULL, NULL, RDW_INVALIDATE);
	}
	if (LOfreq < 31000000)
	{
		switch (rfmode)
		{
		case VHFMODE:
			RadioHandler.UpdatemodeRF(HFMODE);
			giExtSrateIdx = giExtSrateIdxHF;  // restore HF SRate
			r2iqCntrl.Setdecimate(giExtSrateIdx);
			ExtIoSetMGC(giMgcIdxHF);
			SetAttenuator(giAttIdxHF);
			if (pfnCallback) EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_SampleRate);
			if (pfnCallback) EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_TUNE);
			RedrawWindow(h_dialog, NULL, NULL, RDW_INVALIDATE);
			break;
		}
	}

	LOfreq = r2iqCntrl.UptTuneFrq(LOfreq, glTunefreq); //update LO freq
	glLOfreq = LOfreq; 

	if (wishedLO != LOfreq && pfnCallback)  
	{
		EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_LO);
	}
   
	RadioHandler.TuneLO(LOfreq);
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
	glLOfreq = r2iqCntrl.UptTuneFrq(glLOfreq, glTunefreq);
	EnterFunction1((int) glLOfreq);
	return glLOfreq;
}

//---------------------------------------------------------------------------
extern "C"
long EXTIO_API GetHWSR(void)
{
    EnterFunction1(giExtSrateIdx);

	double newSrate;
	if (0 == ExtIoGetSrates(giExtSrateIdx, &newSrate))
	{
		gExtSampleRate = (unsigned)(newSrate + 0.5);
	}
	return gExtSampleRate;

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

		// possibility to check program's capabilities
		// depending on SDR program name and version,
		// f.e. if specific extHWstatusT enums are supported

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
	return 0; 
}

//---------------------------------------------------------------------------
// fill in possible samplerates
extern "C"
int EXTIO_API ExtIoGetSrates(int srate_idx, double * samplerate)
{
	EnterFunction1(srate_idx);
	double div;
	switch (srate_idx)
	{
	case 0:		div = 32.0;	break;
	case 1:		div = 16.0;	break;
	case 2:		div = 8.0;	break;
	case 3:		div = 4.0;	break;
	case 4:		div = 2.0;	break;
	default:    return -1;
	}
	*samplerate = (double)((UINT32)((adcfixedfreq / div)));
     DbgPrintf("*ExtIoGetSrate idx %d  %e\n", srate_idx, *samplerate);
	return 0;	
}

extern "C"
int  EXTIO_API ExtIoGetActualSrateIdx(void)
{
    EnterFunction();
	return giExtSrateIdx;
}

extern "C"
int  EXTIO_API ExtIoSetSrate(int srate_idx)
{
    EnterFunction1(srate_idx);
	double newSrate = 0.0;
	if ((RadioHandler.GetmodeRF() == VHFMODE) && (srate_idx>2)) return 1;	// RT820T2 max 2

	if (0 == ExtIoGetSrates(srate_idx, &newSrate))
	{
		giExtSrateIdx = srate_idx;
		gExtSampleRate = (unsigned)(newSrate + 0.5);      // update

		r2iqCntrl.Updt_SR_LO_TUNE(srate_idx, &glLOfreq, &glTunefreq);

		EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_LO);
		EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_TUNE);

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
	case 1:	strcpy(description, "RadioHWtype");	snprintf(value, 1024, "%d", radio);			return 0;
	case 2:	strcpy(description, "SampleRateIdx");	snprintf(value, 1024, "%d", giExtSrateIdx);			return 0;
	case 3:	strcpy(description, "HF_AttenuationIdx");	snprintf(value, 1024, "%d", giAttIdxHF);			return 0;
	case 4:	strcpy(description, "HF_VGAIdx");	snprintf(value, 1024, "%d", giMgcIdxHF);			return 0;
	case 5:	strcpy(description, "VHF_AttenuationIdx");	snprintf(value, 1024, "%d", giAttIdxVHF);			return 0;
	case 6:	strcpy(description, "VHF_VGAIdx");	snprintf(value, 1024, "%d", giMgcIdxVHF);			return 0;
	case 7:	strcpy(description, "GainCorrection_dB");   snprintf(value, 1024, "%f",gdGainCorr_dB);	return 0;
	case 8:	strcpy(description, "FreqCorrection_dB");   snprintf(value, 1024, "%f", gdFreqCorr_ppm);	return 0;
	case 9:	strcpy(description, "HF_Bias");   snprintf(value, 1024, "%d", 0);		return 0;
	case 10: strcpy(description, "VHF_Bias");   snprintf(value, 1024, "%d", 0);		return 0;
	case 11: strcpy(description, "LoFrequencyHz");   snprintf(value, 1024, "%ld",(unsigned long) glLOfreq); return 0;
	case 12: strcpy(description, "TuneFrequencyHz");   snprintf(value, 1024, "%ld", (unsigned long) glTunefreq); return 0;
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
	double tempDouble;
	
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
		tempInt = atoi(value);
		if ((tempInt >= BBRF103 ) && (tempInt <= RX888))
		{
			radio = (RadioModel)(tempInt);
		}
		break;
	case 2:
		tempInt = 4 - atoi(value);
		if (0 == ExtIoGetSrates(tempInt, &newSrate))
		{
			giExtSrateIdx = tempInt;
			gExtSampleRate = (unsigned)(newSrate + 0.5);
		}
		break;
	case 3:
		tempInt = atoi(value);
		giAttIdxHF = tempInt;
		break;
	case 4:
		tempInt = atoi(value);
		giMgcIdxHF = tempInt;
		break;
	case 5:
		tempInt = atoi(value);
		giAttIdxVHF = tempInt;
		break;
	case 6:
		tempInt = atoi(value);
		giMgcIdxVHF = tempInt;
		break;		
	case 7:
		if (sscanf(value, "%lf", &tempDouble) > 0)
		{
			if ((tempDouble < 40.0) && (tempDouble > -40.0))    //    if abs< 40dB
				gdGainCorr_dB = tempDouble;
		}
		break;
	case 8:
		if (sscanf(value, "%lf", &tempDouble) > 0)
		{
			if ((tempDouble < 200.0) && (tempDouble > -200.0))    //    if abs< 200 ppm
				gdFreqCorr_ppm = tempDouble;
		}
		break;

	case 11:
		if (sscanf(value, "%ld", &tempulong) > 0)
		{
			glLOfreq = (int64_t) tempulong;
		}
		else
			glLOfreq = 2000000;
		break;

	case 12:
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
