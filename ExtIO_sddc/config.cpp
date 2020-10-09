#include "license.txt" // Oscar Steila ik1xpv
#include "config.h"

class cglobal global;
const char *signal_mode[] = { "RF ADC stream", "RF virtual tone","RF virtual sweep", "IF virtual tone" , "IF virtual sweep" };
Inject_Signal inject_tone = ADCstream;
int Xfreq;
double pi;
double count2usec;
double count2msec;
double count2sec;
double freqcorrection = FREQCORRECTION;


