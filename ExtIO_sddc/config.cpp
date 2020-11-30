#include "license.txt" 
#include "config.h"

int Xfreq = 10000;
char strversion[] = "ExtIO_SDDC.dll v1.01";
double gdFreqCorr_ppm = FREQCORRECTION;
double adcfixedfreq = ADC_FREQ;
double gdGainCorr_dB = GAIN_ADJ;            // in dB
bool saveADCsamplesflag = false;
bool run = false;
RadioModel radio = NORADIO;
