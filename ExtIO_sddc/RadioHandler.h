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

class RadioHardware;

class RadioHandlerClass {
public:
    RadioHandlerClass();
    virtual ~RadioHandlerClass();
    bool Init(HMODULE hInst);
    bool Start(int srate_idx);
    bool Stop();
    bool Close();
    bool IsReady(){return true;}
    
    int GetRFAttSteps(const float **steps);
    int UpdateattRF(int attIdx);

    int GetIFGainSteps(const float **steps);
    int UpdateIFGain(int attIdx);

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
    UINT16 firmware;
    rf_mode modeRF;  

    RadioHardware* hardware;
};

extern class RadioHandlerClass RadioHandler;
extern unsigned long Failures;

class RadioHardware {
public:
    RadioHardware(fx3class* fx3) : Fx3(fx3), gpios(0) {}

    virtual ~RadioHardware();
    virtual const char* getName() = 0;
    virtual void getFrequencyRange(int64_t& low, int64_t& high) = 0;
    virtual float getGain() { return BBRF103_GAINFACTOR; }
    virtual void Initialize() = 0;
    virtual bool UpdatemodeRF(rf_mode mode) = 0;
    virtual bool UpdateattRF(int attIndex) = 0;
    virtual int64_t TuneLo(int64_t freq) = 0;

    virtual int getRFSteps(const float** steps ) { return 0; }
    virtual int getIFSteps(const float** steps ) { return 0; }
    virtual bool UpdateGainIF(int attIndex) { return false; }

    bool FX3producerOn() { return Fx3->Control(STARTFX3); }
    bool FX3producerOff() { return Fx3->Control(STOPFX3); }

    bool FX3SetGPIO(uint32_t mask);
    bool FX3UnsetGPIO(uint32_t mask);

protected:
    fx3class* Fx3;
    uint32_t gpios;
};

class BBRF103Radio : public RadioHardware {
public:
    BBRF103Radio(fx3class* fx3);
    const char* getName() override { return "BBRF103"; }
    float getGain() override { return BBRF103_GAINFACTOR; }
    void getFrequencyRange(int64_t& low, int64_t& high) override;
    void Initialize() override;
    bool UpdatemodeRF(rf_mode mode) override;
    int64_t TuneLo(int64_t freq) override;
    bool UpdateattRF(int attIndex) override;
    bool UpdateGainIF(int attIndex) override;

    int getRFSteps(const float** steps ) override;
    int getIFSteps(const float** steps ) override;

private:
    static const int step_size = 29;
    static const float steps[step_size];
    static const float hfsteps[3];

    static const int if_step_size = 16;
    static const float if_steps[if_step_size];
};

class RX888Radio : public BBRF103Radio {
public:
    RX888Radio(fx3class* fx3) : BBRF103Radio(fx3) {}
    const char* getName() override { return "RX888"; }
    float getGain() override { return RX888_GAINFACTOR; }
};

class HF103Radio : public RadioHardware {
public:
    HF103Radio(fx3class* fx3) : RadioHardware(fx3) {}
    const char* getName() override { return "HF103"; }
    float getGain() override { return HF103_GAINFACTOR; }

    void getFrequencyRange(int64_t& low, int64_t& high) override;

    void Initialize() override;

    bool UpdatemodeRF(rf_mode mode) override;

    int64_t TuneLo(int64_t freq) override { return ADC_FREQ / 2; }
    
    bool UpdateattRF(int attIndex) override;

    int getRFSteps(const float** steps ) override;

private:
    static const int step_size = 64;
    float steps[step_size];
};

class DummyRadio : public RadioHardware {
public:
    DummyRadio() : RadioHardware(nullptr) {}
    const char* getName() override { return "HF103"; }

    void getFrequencyRange(int64_t& low, int64_t& high) override
    { low = 0; high = 6ll*1000*1000*1000;}
    void Initialize() override {}
    bool UpdatemodeRF(rf_mode mode) override { return true; }
    bool UpdateattRF(int attIndex) override { return true; }
    int64_t TuneLo(int64_t freq) override { return freq; }
};

#endif  RADIOHANDLER_H