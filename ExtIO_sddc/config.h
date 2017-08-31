#pragma once
// Oscar Steila 2017 ExtIO_sddc.dll configuration

#ifdef _DEBUG
#define _MYCONSOLE			// activate a debug console
//	#define _ZERORUN		// look for zero runs in the output sequence
//  #define _USB_TIMING		// tentative measure timing usb buffers pool
//	#define _RFDDC_SEQUENCE	// tentative measure RFDDC thread sequence
#endif

#ifdef __cplusplus
inline void null_func(char *format, ...) { }
#define DbgEmpty null_func
#else
#define DbgEmpty { }
#endif
#ifdef  _MYCONSOLE
/* Debug Trace Enabled */
#include <stdio.h>
#define DbgPrintf printf
#else
/* Debug Trace Disabled */
#define DbgPrintf DbgEmpty
#endif


#define VERSION   "ExtIO_sddc.dll ver 0.95"

#define HWNAME				"SDDC-0.95"			// VERSION NUMBER
#define HWMODEL				"BBRF103 "
#define HWVERSION			"0.01"
#define SETTINGS_IDENTIFIER	"SDDC-1.0"
#define SWNAME				"ExtIO_sddc.dll"
#define SWVERSION			"0.95"
#define AUTHOR				"by Oscar Steila, ik1xpv"

#define FRQSMP  (64000000)		// ADC sampling frequency
#define CORRECTION (0)
#define FRQINIT (8000000)		// default initial frequency if no valid settings
#define R820T_FREQ (32000000)	// R820T reference frequency

