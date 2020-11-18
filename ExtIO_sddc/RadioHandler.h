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

class RadioHardware;

class RadioHandlerClass {
public:
    RadioHandlerClass();
    virtual ~RadioHandlerClass();
    bool Init(HMODULE hInst);
    bool Start();
    bool Stop();
    bool Close();
    bool IsReady(){return true;}
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

    bool GetBiasT_HF() { return biasT_HF; }
    void UpdBiasT_HF(bool flag);
    bool GetBiasT_VHF() { return biasT_VHF; }
    void UpdBiasT_VHF(bool flag);

    int64_t TuneLO(int64_t lo);

#ifdef TRACE
    bool UptTrace( bool trace){ traceflag = trace; return traceflag; }
    bool GetTrace( ){return traceflag; }
#endif

private:
    void AdcSamplesProcess();
    void AbortXferLoop(int qidx);

    bool dither;
    bool randout;
    bool biasT_HF;
    bool biasT_VHF;
    bool traceflag;
    bool samplesADCflag;
    int  matt;
    UINT16 firmware;
    rf_mode modeRF;  
    int attRF;

    RadioHardware* hardware;
};

extern class RadioHandlerClass RadioHandler;
extern unsigned long Failures;
extern int	giAttIdx;


class RadioHardware {
public:
    RadioHardware(fx3class* fx3) : Fx3(fx3), gpios(0) {}

    virtual ~RadioHardware() {
        Fx3->Control(RESETFX3);       //FX3    reset the fx3 firmware
        delete Fx3;
    }

    virtual const char* getName() = 0;
    virtual void getFrequencyRange(int64_t& low, int64_t& high) = 0;
    virtual void Initialize() = 0;
    virtual bool UpdatemodeRF(rf_mode mode) = 0;
    virtual bool UpdateattRF(int attIndex) = 0;
    virtual int64_t TuneLo(int64_t freq) = 0;

    bool FX3producerOn() { return Fx3->Control(STARTFX3); }
    bool FX3producerOff() { return Fx3->Control(STOPFX3); }

    bool FX3SetGPIO(int32_t mask);
    bool FX3UnsetGPIO(int32_t mask);

protected:
    fx3class* Fx3;
    uint32_t gpios;
};

class BBRF103Radio : public RadioHardware {
public:
    BBRF103Radio(fx3class* fx3);
    const char* getName() override { return "BBRF103"; }
    void getFrequencyRange(int64_t& low, int64_t& high) override;
    void Initialize() override;
    bool UpdatemodeRF(rf_mode mode) override;
    int64_t TuneLo(int64_t freq) override;
    bool UpdateattRF(int attIndex) override;
};

class RX888Radio : public BBRF103Radio {
public:
    RX888Radio(fx3class* fx3) : BBRF103Radio(fx3) {}
    const char* getName() override { return "RX888"; }
};

class HF103Radio : public RadioHardware {
public:
    HF103Radio(fx3class* fx3) : RadioHardware(fx3) {}
    const char* getName() override { return "HF103"; }

    void getFrequencyRange(int64_t& low, int64_t& high) override;

    void Initialize() override;

    bool UpdatemodeRF(rf_mode mode) override;

    int64_t TuneLo(int64_t freq) override { return ADC_FREQ / 2; }
    
    bool UpdateattRF(int attIndex) override;
};

class DummyRadio : public RadioHardware {
public:
    DummyRadio(fx3class* fx3) : RadioHardware(fx3) {}
    const char* getName() override { return "HF103"; }

    void getFrequencyRange(int64_t& low, int64_t& high) override
    { low = 0; high = 6ll*1000*1000*1000;}
    void Initialize() override {}
    bool UpdatemodeRF(rf_mode mode) override { return true; }
    bool UpdateattRF(int attIndex) override { return true; }
    int64_t TuneLo(int64_t freq) override { return freq; }
};

#endif  RADIOHANDLER_H
