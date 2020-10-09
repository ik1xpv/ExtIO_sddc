#ifndef R2IQ_H
#define R2IQ_H
#include "license.txt" // Oscar Steila ik1xpv
#define N_R2IQ_THREAD 2
#define GAINFACTOR (0.00000007079)
#define NDECIDX 5
#include "LC_ExtIO_Types.h"

class r2iqControlClass {
public:
    r2iqControlClass();
    bool r2iqOn;        // r2iq on flag
    PUCHAR *buffers;    // pointer to input buffers
    float** obuffers;   // pointer to output buffers
    int bufIdx;         // index to next buffer to be processed
    int cntr;           // counter of input buffer to be processed
    int Setdecimate(int dec);
    bool randADC;       // randomized ADC output
    long lastThread;    // last thread previous to the current
    bool Initialized;   // r2iq already initialized
    int getDecidx() {return mdecimation;}
    int getFftN()   {return mfftdim [mdecimation];}
    int getFftN(int x)   {return mfftdim [x];}
    int getRatio()  {return mratio [mdecimation];}
    int getTunebin() {return mtunebin;}
    int64_t UptTuneFrq(int64_t freq);  // Update tunebin and return normalized LO frequency.

private:
    int mdecimation ;   // selected decimation ratio
                        // 0 => 32Msps, 1=> 16Msps, 2 = 8Msps, 3 = 4Msps, 5 = 2Msps
    int mfftdim [NDECIDX]; // FFT N dimensions
    int mratio [NDECIDX];  // ratio
    int mtunebin;

};

extern class r2iqControlClass r2iqCntrl;

void initR2iq( int downsample );
void *r2iqThreadf(void *arg);
void r2iqTurnOn(int idx);
void r2iqTurnOff(void);
bool r2iqIsOn(void);
void r2iqDataReady(void);



#endif
