#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "license.txt" 

#include "../Interface.h"
#include <math.h>      // atan => PI
#include <thread>
#include <mutex>
#include <condition_variable>


#ifdef __cplusplus
inline void null_func(const char *format, ...) { }
#define DbgEmpty null_func
#else
#define DbgEmpty { }
#endif

// macro to call callback function with just status extHWstatusT
#define EXTIO_STATUS_CHANGE( CB, STATUS )   \
	do { \
	  SendMessage(h_dialog, WM_USER + 1, STATUS, 0); \
	  if (CB) { \
		  DbgPrintf("<==CALLBACK: %s\n", #STATUS); \
		  CB( -1, STATUS, 0, NULL );\
	  }\
	}while(0)

#ifdef VERBOSE_DEBUG
	#define EnterFunction() \
	DbgPrintf("==>%s\n", __FUNCDNAME__)

	#define EnterFunction1(v1) \
	DbgPrintf("==>%s(%d)\n", __FUNCDNAME__, (v1))
#else
	#define EnterFunction()
	#define EnterFunction1(v1)
#endif

#ifdef _DEBUG
#define DbgPrintf (printf)
#else
#define DbgPrintf DbgEmpty
#endif

#define VERSION             (1.01)	//	Dll version number x.xx

#define SETTINGS_IDENTIFIER	"sddc_1.03"
#define SWNAME				"ExtIO_sddc.dll"

#define	QUEUE_SIZE 64

#define FFTN_R_ADC (2048)       // FFTN used for ADC real stream DDC

// GAINFACTORS to be adjusted with lab reference source measured with HDSDR Smeter rms mode  
#define BBRF103_GAINFACTOR (0.000000078F)       // BBRF103
#define HF103_GAINFACTOR   (0.0000000114F)      // HF103
#define RX888_GAINFACTOR   (0.00000000695F)     // RX888

enum rf_mode { NOMODE = 0, HFMODE = 0x1, VHFMODE = 0x2 }; 

#define HF_HIGH (32000000)    // 32M
#define MW_HIGH ( 2000000)

#define EXT_BLOCKLEN		512	* 64	/* 32768 only multiples of 512 */

#define RFDDCNAME ("NVIA L768M256")
#define RFDDCVER ("v 1.0")

// URL definitions
#define URL1B               "16bit SDR Receiver"
#define URL1               "<a>http://www.hdsdr.de/</a>"
#define URL_HDSR            "http://www.hdsdr.de/"
#define URL_HDSDRA          "<a>http://www.hdsdr.de/</a>"

#define TIMEOUT (2000)

extern int gdFreqCorr_ppm;
extern int gdGainCorr_dB;
extern bool saveADCsamplesflag;

#define CORRECT(FREQ) ((double) FREQ * (1.0f + (gdFreqCorr_ppm * 0.000001f)))

const int transferSize = 131072;

#endif // _CONFIG_H_

