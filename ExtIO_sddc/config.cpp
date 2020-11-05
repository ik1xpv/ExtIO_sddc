#include "license.txt" 
#include "config.h"

const char* radioname[4] = { "NORADIO","BBRF103","HF103", "RX888" };

class cglobal global;
int Xfreq;
double count2usec;
double count2msec;
double count2sec;
char strversion[] = "ExtIO_SDDC.dll v1.01";
double gdFreqCorr_ppm = FREQCORRECTION;
double adcfixedfreq = ADC_FREQ;
double gdGainCorr_dB = GAIN_ADJ;            // in dB
bool saveADCsamplesflag = false;
