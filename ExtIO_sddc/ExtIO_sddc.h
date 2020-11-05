#include "license.txt" 

#ifdef EXTIO_EXPORTS
#define EXTIO_API __declspec(dllexport) __stdcall
#else
#define EXTIO_API __stdcall
#endif

#define _MYCONSOLE

#include "LC_ExtIO_Types.h"

extern "C" bool EXTIO_API InitHW(char *name, char *model, int& type);
extern "C" int64_t EXTIO_API StartHW64(int64_t freq);
extern "C" bool EXTIO_API OpenHW(void);
extern "C" int  EXTIO_API StartHW(long freq);
extern "C" void EXTIO_API StopHW(void);
extern "C" void EXTIO_API CloseHW(void);
//extern "C" void EXTIO_API ShowGUI();
//extern "C" void EXTIO_API HideGUI();
extern "C" void EXTIO_API SwitchGUI();
extern "C" int  EXTIO_API SetHWLO(long LOfreq);
extern "C" int64_t EXTIO_API SetHWLO64(int64_t LOfreq);
extern "C" int  EXTIO_API GetStatus(void);
extern "C" void EXTIO_API SetCallback(pfnExtIOCallback funcptr);
// void extIOCallback(int cnt, int status, float IQoffs, short IQdata[]);

extern "C" long EXTIO_API GetHWLO(void);
extern "C" int64_t EXTIO_API GetHWLO64(void);
extern "C" long EXTIO_API GetHWSR(void);

extern "C" long EXTIO_API GetTune(void);
// extern "C" void EXTIO_API GetFilters(int& loCut, int& hiCut, int& pitch);
// extern "C" char EXTIO_API GetMode(void);
// extern "C" void EXTIO_API ModeChanged(char mode);
// extern "C" void EXTIO_API IFLimitsChanged(long low, long high);
extern "C" void    EXTIO_API TuneChanged(long freq);
extern "C" void    EXTIO_API TuneChanged64(int64_t freq);
extern "C" int64_t EXTIO_API GetTune64(void);
// extern "C" void    EXTIO_API IFLimitsChanged64(int64_t low, int64_t high);

// extern "C" void EXTIO_API RawDataReady(long samprate, int *Ldata, int *Rdata, int numsamples);

extern "C" void EXTIO_API VersionInfo(const char * progname, int ver_major, int ver_minor);

extern "C" int EXTIO_API GetAttenuators(int idx, float * attenuation);  // fill in attenuation
							 // use positive attenuation levels if signal is amplified (LNA)
							 // use negative attenuation levels if signal is attenuated
							 // sort by attenuation: use idx 0 for highest attenuation / most damping
							 // this functions is called with incrementing idx
							 //    - until this functions return != 0 for no more attenuator setting
extern "C" int EXTIO_API GetActualAttIdx(void);                          // returns -1 on error
extern "C" int EXTIO_API SetAttenuator(int idx);                       // returns != 0 on error

extern "C" int EXTIO_API ExtIoGetAGCs(int agc_idx, char * text);
extern "C" int EXTIO_API ExtIoGetActualAGCidx(void);
extern "C" int EXTIO_API ExtIoSetAGC(int agc_idx);
extern "C" int EXTIO_API ExtIoShowMGC(int agc_idx);

extern "C" int EXTIO_API ExtIoGetMGCs(int mgc_idx, float * gain);
extern "C" int EXTIO_API ExtIoGetActualMgcIdx(void);
extern "C" int EXTIO_API ExtIoSetMGC(int mgc_idx);

extern "C" int  EXTIO_API ExtIoGetSrates(int idx, double * samplerate);  // fill in possible samplerates
extern "C" int  EXTIO_API ExtIoGetActualSrateIdx(void);               // returns -1 on error
extern "C" int  EXTIO_API ExtIoSetSrate(int idx);                    // returns != 0 on error
//extern "C" long EXTIO_API ExtIoGetBandwidth(int srate_idx);       // returns != 0 on error

extern "C" int  EXTIO_API ExtIoGetSetting(int idx, char * description, char * value); // will be called (at least) before exiting application
extern "C" void EXTIO_API ExtIoSetSetting(int idx, const char * value); // before calling InitHW() !!!

