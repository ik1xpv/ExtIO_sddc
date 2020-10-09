#ifndef _CONFIG_H_
#define _CONFIG_H_
#include "license.txt" // Oscar Steila ik1xpv

#include "windows.h"   // LARGE_INTEGER
#include <math.h>      // atan => PI

#ifdef __cplusplus
inline void null_func(const char *format, ...) { }
#define DbgEmpty null_func
#else
#define DbgEmpty { }
#endif


#ifdef  NDEBUG
  #define DbgPrintf DbgEmpty
#else
  #define DbgPrintf (printf)
#endif

#ifndef  NEXTIOTRACE
  #define DbgExtio DbgEmpty
#else
  #define DbgExtio (printf)
#endif



//#define ANTPWRMAX4995AL
// MAX4945AL antenna power switch. The control logic is low = antenna voltage on
// undefine to use positive logic switch as NPN logic


#ifdef ANTPWRMAX4995AL
    #define ANTPOLARITY (false)   // +VANT reverse   MAX4995AL == 0
    #define ANTPWRLABEL "low"    // +VANT positive
#else
    #define ANTPOLARITY (true)   // +VANT reverse   MAX4995AL == 0
    #define ANTPWRLABEL "high"   // +VANT positive
#endif

//#define _NO_TUNER_  // R820T2 disabled

#define VERSION   "ExtIO_sddc.dll ver 0.98"

#define HWNAME				"SDDC-0.98"
#define SETTINGS_IDENTIFIER	"SDDC-0.98"
#define SWVERSION			"0.98_RC02"
#define HWMODEL				"BBRF103 "
#define HWVERSION			"0.2"  // 0.1 first pcb, 0.2 RAND wire added
#define SWNAME				"ExtIO_sddc.dll"

#define AUTHOR				"ik1xpv"

#define	QUEUE_SIZE 64
#define QUEUE_OUT  64
#define FFTN_R_ADC (2048)       // FFTN used for ADC real stream DDC
#ifdef OFFSET_BINARY
 #define ADCSAMPLE UINT16
 #define DC 32768
#else
 #define ADCSAMPLE INT16
 #define DC 0
#endif
#define EXT_BLOCKLEN		512	* 64	/* 32768 only multiples of 512 */
#define RFDDCNAME ("NVIA L768M256")
#define RFDDCVER ("v0.6")

#define ADC_FREQ  (64000000)	    // ADC sampling frequency
#define R820T2_FREQ (32000000)	    // R820T reference frequency
#define R820T_ZERO (0)              // freq 0
#define SI5351_XTAL	27000000	    // Crystal frequency in Hz
#define FREQCORRECTION 	(-1761.0)   // Default xtal frequency correction in Hz


enum rf_mode {  VLFMODE = 0x0, HFMODE = 0x1, VHFMODE = 0x2 , NOMODE = 3};





// URL definitions
#define URLBBRF103          "http://www.steila.com/blog/index.php?controller=post&action=view&id_post=18"
#define URL1B               "<a>BreadBoard RF103               by ik1xpv</a>"
#define URL1                "http://www.steila.com/blog/"
#define URL1A               "<a>http://www.steila.com/blog/</a>"
#define URL_HDSR            "http://www.hdsdr.de/"
#define URL_HDSDRA          "<a>http://www.hdsdr.de/</a>"


#define TIMEOUT (2000)

enum Inject_Signal { ADCstream = 0x0, ToneRF = 0x1, SweepRF = 0x2, ToneIF = 0x3 , SweepIF = 0x4, Freecase = 0x5 };
extern const char *signal_mode[];
extern double pi;
extern double count2usec;
extern double count2msec;
extern double count2sec;
extern int Xfreq;
extern double freqcorrection;

class cglobal {
    public:
    bool start;
    bool run;
    bool FX3isopen;
    double fs;
    int transferSize;

    cglobal() {

        start = false;
        run = false;
        FX3isopen = false;
        fs =  ADC_FREQ;
        transferSize = 131072;
        LARGE_INTEGER Frequency;
        QueryPerformanceFrequency(&Frequency);
        count2sec  = 1  /(double)Frequency.QuadPart;
        count2msec = 1000/(double)Frequency.QuadPart;
        count2usec = 1000000/(double)Frequency.QuadPart;
        pi = 4. * atan(1);
        Xfreq = 10000;
    }
};
extern class cglobal global;

#endif // _CONFIG_H_

