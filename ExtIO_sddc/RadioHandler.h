#ifndef RADIOHANDLER_H
#define RADIOHANDLER_H

#include "license.txt" 

#include "framework.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include "FX3handler.h"




#define SHDWN           (32)  	
#define DITH            (64)	
#define RANDO           (128)	 

#define BIAS_HF         (256)	
#define BIAS_VHF        (512)	
#define LED_YELLOW      (1024)
#define LED_RED         (2048)
#define LED_BLUE        (4096)
#define ATT_SEL0        (8192)
#define ATT_SEL1		(16384)

#define GAIN_STEPS (29)  // R820 steps

class RadioHandlerClass {
public:
    RadioHandlerClass();
    bool Init(HMODULE hInst);
    bool InitBuffers();
    bool Start();
    bool Stop();
    bool Close();
    bool IsReady(){return IsOn;}
    int UpdateattRF(int att);
    int GetAttRF(){return attRF;}
    bool UpdatemodeRF(rf_mode mode);
    rf_mode GetmodeRF(){return (rf_mode)modeRF;}
    bool UptDither (bool b);
    bool GetDither () {return dither;}
    bool UptRand (bool b);
    bool GetRand () {return randout;}
    bool UptsamplesADC(bool flag) { samplesADCflag = flag; return  samplesADCflag; }
    bool GetADCsamples() { return  samplesADCflag; }
    UINT16 GetFirmware() { return firmware; }
    void FX3producerOn() { Fx3->Control(STARTFX3); }
    void FX3producerOff() { Fx3->Control(STOPFX3); }

    bool GetBiasT_HF() { return biasT_HF; }
    void UpdBiasT_HF(bool flag);
    bool GetBiasT_VHF() { return biasT_VHF; }
    void UpdBiasT_VHF(bool flag);
    bool UpdSi5351a(); //freq corr update
    int R820T2init();
    void R820T2stdby();
    int R820T2Tune(UINT64 freq);
    float R820T2getAttenuator(UINT8 idx);
    int R820T2SetAttenuator(UINT8 idx);

#ifdef TRACE
    bool UptTrace( bool trace){ traceflag = trace; return traceflag; }
    bool GetTrace( ){return traceflag; }
#endif

private:
    bool InitSi5351a(UINT32 freqa, UINT32 freqb);
    int64_t lofreqm[3]; // lo oscillator memory
    bool IsOn;
    bool dither;
    bool randout;
    bool biasT_HF;
    bool biasT_VHF;
    bool traceflag;
    bool samplesADCflag;
    int  matt;
    UINT32 mfreqa;
    UINT32 mfreqb;
    UINT16 firmware;
    UINT16 bgpio;    // buffer gpio
    rf_mode modeRF;  
    fx3class* Fx3;   
    int attRF;
};

extern class RadioHandlerClass RadioHandler;
extern unsigned long Failures;
extern int	giAttIdx;
#endif  RADIOHANDLER_H
