#include "license.txt" // Oscar Steila ik1xpv

#include <stdio.h>
#include "mytypes.h"

#include "BBRF103.h"
#include "Si5351.h"
#include "R820T2.h"
#include "openFX3.h"
#include "config.h"
#include "r2iq.h"
extern pfnExtIOCallback	pfnCallback;

UINT8 aa;
bbRF103 BBRF103;


bbRF103::bbRF103():
    IsOn (false),
    dither(false),
    randout(false),
    matt(-1),
    Bgpio (0X17),
    modeRF(NOMODE),
    attRF(0)
{



}

bool bbRF103::Init()
{
    if (!global.FX3isopen)
        global.FX3isopen = openFX3();
    if (global.FX3isopen)
    {
        IsOn = true;
        UINT8 rdata[64];
#ifndef _NO_TUNER_
        fx3Control(TESTFX3,&rdata[0]);
#else
        rdata[0] = 1;
#endif
        if (rdata[0]== 0)
        {
            R820T2isalive = true;
            DbgPrintf("RT820T2 found\n");
        }
        else
        {
            R820T2isalive = false;
            DbgPrintf("WARNING NO RT820T2\n");
        }
        UpdatemodeRF(modeRF); // blu led Update
        UptLedRed(false);     // red off
        UptDither(false);
        Si5351init();

    }
    else
    {
        IsOn = false;
    }
    return IsOn;
}
bool bbRF103::Close()
{
    if (IsOn)
    {
        fx3Control(RESETFX3);       // reset the fx3 firmware
        closeFX3();
        IsOn = false;
        global.FX3isopen = false;
    }
    return IsOn;
}

// attenuator RF used in HF
int bbRF103::UpdateattRF(int att)
{
    if (matt != att)
    {
        matt = att;
        if ((modeRF == HFMODE) || (modeRF == VLFMODE)|| (R820T2isalive == false))
        {
            int dbatt;
            switch (att)
            {
                case 0:  dbatt=-20; break;
                case 1:  dbatt=-10; break;
                case 2:
                default: dbatt=0; break;
            }
            attRF = dbatt;
            UINT8 tmp = Bgpio &(0xFF ^ (SEL0 ||SEL1));
            switch(attRF)
                {
                case -10: //11
                    Bgpio = tmp | SEL0 | SEL1;
                    break;
                case -20: //01
                    Bgpio = (tmp | SEL0) & (0xFF ^ SEL1);
                    break;
                case 0:   //10
                default:
                    Bgpio = (tmp | SEL1) & (0xFF ^ SEL0);
                    break;
                }
        }
        else  //VHF mode
        {
            Bgpio = Bgpio & (0xFF ^ (SEL1 |SEL0));
        }
        if(IsOn)
           {
               DbgPrintf("UpdateattRF  %02X \n", Bgpio & (SEL0 | SEL1) );
           if (!fx3Control(GPIOFX3, &Bgpio))
                HouseKeeping();
                BBRF103.UptLedYellow(!BBRF103.ledY);  //blink  gadget Yellow
           }
    }
    return attRF;
}

// attenuator RF used in HF dB
int bbRF103::UpdateattRFdB()
{
    if ((modeRF == HFMODE) || (modeRF == VLFMODE) || (R820T2isalive == false))
    {
        UINT8 tmp = Bgpio &(0xFF ^ (SEL0 ||SEL1));
        switch(attRF)
            {
            case -10: //11
                Bgpio = tmp | SEL0 | SEL1;
                break;
            case -20: //01
                Bgpio = (tmp | SEL0) & (0xFF ^ SEL1);
                break;
            case 0:   //10
            default:
                Bgpio = (tmp | SEL1) & (0xFF ^ SEL0);
                break;
            }
    }
    else  //VHF mode
    {
        Bgpio = Bgpio & (0xFF ^ (SEL1 |SEL0));
    }
    if(IsOn)
       {
           DbgPrintf("bbRF103::UpdateattRFdB fx3 %02X  %02X \n",GPIOFX3, Bgpio & (SEL0 | SEL1) );
       if (!fx3Control(GPIOFX3, &Bgpio)) HouseKeeping();
       }
    return attRF;
}

int64_t bbRF103::GetLO()
{
    return lofreqm[modeRF];
}
int64_t bbRF103::GetLO(rf_mode mode)
{
    return lofreqm[mode];
}

void bbRF103::SetLO( int64_t lo)
{
    lofreqm[modeRF]= lo;
}

void bbRF103::SetLO( int64_t lo, rf_mode mode)
{
    lofreqm[mode]= lo;
}


bool bbRF103::UpdatemodeRF(rf_mode mode)
{
    bool r = true;
    if ((mode == VHFMODE) && (!R820T2isalive))
    {
        mode = HFMODE;
    }
    if( modeRF != mode )
    {
         modeRF = mode;
         if((modeRF == HFMODE) || (modeRF == VLFMODE))
         {
             if(IsOn){
                si5351aSetFrequency(ADC_FREQ, R820T_ZERO);
                UpdateattRFdB();
                UptLedBlue(false);
             }
         }
         else
         {
            r = false;
             if(IsOn){
                si5351aSetFrequency(ADC_FREQ, R820T_FREQ);
                r = R820T2.init();
                UpdateattRFdB();
                UptLedBlue(r);
            }
         }
// Update RF_IF
        Sleep(300);
        if (pfnCallback )
        {
            pfnCallback( -1, extHw_Changed_RF_IF , 0.0F, 0 );
            pfnCallback( -1, extHw_Changed_AGCS , 0.0F, 0 );
            pfnCallback( -1, extHw_Changed_AGC , 0.0F, 0 );
            pfnCallback( -1, extHw_Changed_LO, 0.0F, 0 );
        }
    }
    return r;
}

bool bbRF103::UptDither(bool b)
{
    if (IsOn)
    {
    dither = b;
    if (dither == true)
        Bgpio = Bgpio | DITH;
    else
        Bgpio = Bgpio & (0xff ^ DITH);
    if (!fx3Control(GPIOFX3, &Bgpio)) HouseKeeping();
    }
    return dither;
}

bool bbRF103::UptRand (bool b)
{
    if (IsOn)
    {
        randout = b;
        if (randout == true)
            Bgpio = Bgpio | RANDO;
        else
            Bgpio = Bgpio & (0xff ^ RANDO);
    if (!fx3Control(GPIOFX3, &Bgpio)) HouseKeeping();
    }
    r2iqCntrl.randADC =  randout;
    return randout;
}

bool bbRF103::UptLedBlue (bool b)
{
    if (IsOn)
    {
    ledB = b;
    if (ledB == true)
        Bgpio = Bgpio | LED_BLUE;
    else
        Bgpio = Bgpio & (0xff ^ LED_BLUE);
    if (!fx3Control(GPIOFX3, &Bgpio)) HouseKeeping();
    }
    return ledB;
}

bool bbRF103::UptLedYellow (bool b)
{
    if (IsOn)
    {
    ledY = b;
    if (ledY == true)
        Bgpio = Bgpio | LED_YELLOW;
    else
        Bgpio = Bgpio & (0xff ^ LED_YELLOW);
    if (!fx3Control(GPIOFX3, &Bgpio)) HouseKeeping();
    }
    return ledY;
}
bool bbRF103::UptLedRed (bool b)
{
    if (IsOn)
    {
    ledR = b;
    if (ledR == true)
        Bgpio = Bgpio | LED_RED;
    else
        Bgpio = Bgpio & (0xff ^ LED_RED);
    if (!fx3Control(GPIOFX3, &Bgpio)) HouseKeeping();
    }
    return ledR;
}


bool bbRF103::SendI2cbyte(UINT8 i2caddr, UINT8 regaddr, UINT8 data)
{
    return SendI2cbytes(i2caddr, regaddr, &data, 1);
}


bool bbRF103::SendI2cbytes(UINT8 i2caddr, UINT8 regaddr, UINT8 * pdata, UINT8 len)
{
    if (IsOn)
    {
       return  fx3SendI2cbytes( i2caddr, regaddr, pdata, len);
    }
    return false;
}

void bbRF103::ReadI2cbytes(UINT8 i2caddr, UINT8 regaddr, UINT8 * pdata, UINT8 len)
{
    if (IsOn )
        fx3ReadI2cbytes( i2caddr, regaddr, pdata, len);
}
void  bbRF103::HouseKeeping()
{
     BBRF103.UptLedRed(true);
     if (IsOn )
     {
            IsOn = false;
     }
}

