#ifndef BBRF103DEV_H
#define BBRF103DEV_H

#include "license.txt" // Oscar Steila ik1xpv
#include "config.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pstdint.h" //int64_t
#include "pthread.h"

#define LED_RED (1)     //   GPIO21
#define LED_YELLOW (2) 	//   GPIO22
#define LED_BLUE (4) 	//   GPIO22
#define SEL0 (8)  		//   SEL0  GPIO26
#define SEL1 (16) 		//   SEL1  GPIO27
#define SHDWN (32)  	//   SHDWN  GPIO28
#define DITH (64)		//   DITH   GPIO29
#define RANDO (128)		//   RAND   GPIO20
#define PWRANTHF  (256) //   PWRANTHF GPIO19
#define PWRANTVHF (512) //   PWRANTVHF GPIO18
#define SPARE1    (1024)//   GPIO17




class bbRF103 {
public:
    bbRF103();
    bool Init();
    bool Close();
    bool ClockInit();
    bool IsReady(){return IsOn;}
    int UpdateattRF(int att);
    int64_t GetLO();
    int64_t GetLO(rf_mode mode);
    void SetLO( int64_t lo);
    void SetLO( int64_t lo, rf_mode mode);
    int UpdateattRFdB();
    int GetAttRF(){return attRF;}
    bool UpdatemodeRF(rf_mode mode);
    rf_mode GetmodeRF(){return modeRF;}
    bool UptLedBlue (bool b);
    bool UptLedYellow (bool b);
    bool UptLedRed (bool b);
    bool UptDither (bool b);
    bool GetDither () {return dither;}
    bool UptRand (bool b);
    bool GetRand () {return randout;}
    bool UptTrace( bool trace){ traceflag = trace; return traceflag;}
    bool GetTrace( ){return traceflag; }
    bool UptGainadjust( bool gainflg){ gainadjust = gainflg; return gainadjust;}
    bool GetGainadjust(){return gainadjust; }
    bool UptGainadjustHF( bool gainflg){ gainadjustHF = gainflg; return gainadjustHF;}
    bool GetGainadjustHF(){return gainadjustHF; }
    bool UptVAntVHF( bool gflg);
    bool GetVAntVHF(){return gpwrVHF; }
    bool UptVAntHF( bool gflg);
    bool GetVAntHF(){return gpwrHF; }
    bool ledB;
    bool ledY;
    bool ledR;

    bool SendI2cbytes(UINT8 i2caddr, UINT8 regaddr, UINT8 * pdata, UINT8 len);
    bool SendI2cbyte(UINT8 i2caddr, UINT8 regaddr, UINT8 data);
    void ReadI2cbytes(UINT8 i2caddr, UINT8 regaddr, UINT8 * pdata, UINT8 len);
    bool R820T2isalive;
private:
    void ControlHouseKeeping(void);
    void HouseKeeping();
    int64_t lofreqm[3]; // lo oscillator memory
    bool IsOn;
    bool dither;
    bool randout;
    bool traceflag;
    bool gainadjust;
    bool gainadjustHF;
    bool gpwrHF;
    bool gpwrVHF;
    int  matt;
    UINT16 Bgpio; // buffer GPIO
    rf_mode modeRF;  // VLF, HF, VHF-UHF
    int attRF;
};

extern class bbRF103 BBRF103;



#endif
