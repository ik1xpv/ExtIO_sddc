#include "fir.h"
#include <math.h>

#define K_PI 3.141592653f
#define K_2PI (2*K_PI)

static float Izero(float x)
{
    float x2 = x / 2.0f;
    float sum = 1.0f;
    float ds = 1.0f;
    float di = 1.0f;
    float errorlimit = 1e-9f;
    float tmp;
    do
    {
        tmp = x2 / di;
        tmp *= tmp;
        ds *= tmp;
        sum += ds;
        di += 1.0;
    } while (ds >= errorlimit * sum);
    //qDebug()<<"x="<<x<<" I0="<<sum;
    return(sum);
}

void KaiserWindow(int num_taps, float Astop, float normFpass, float normFstop, float *m_Coef)
{
    int n;
    float Beta;

    float Scale = 1.0f; //+106dB over 1.0 to have high level in wavosaur spectrum analisys out. otherwise set to 1.0
//    float Astop = 100.0f; // we want high attenuation 100 dB

    //create normalized frequency parameters
    float normFcut = (normFstop + normFpass) / 2.0f;   //low pass filter 6dB cutoff

    //calculate Kaiser-Bessel window shape factor, Beta, from stopband attenuation
    if (Astop < 20.96f)
        Beta = 0.0f;
    else if (Astop >= 50.0f)
        Beta = .1102f * (Astop - 8.71f);
    else
        Beta = .5842f * powf((Astop - 20.96f), 0.4f) + .07886f * (Astop - 20.96f);

    /* I used this way but beta is Beta way is better
    float Alpha = 3.5;
    Beta = K_PI * Alpha;
    */

    //number of filter taps we fix to 1025
    int m_NumTaps = num_taps; // (Astop - 8.0) / (2.285*K_2PI*(normFstop - normFpass) ) + 1;


    float fCenter = .5f * (float)(m_NumTaps - 1);
    float izb = Izero(Beta);        //precalculate denominator since is same for all points
    for (n = 0; n < m_NumTaps; n++)
    {
        float x = (float)n - fCenter;
        float c;
        // create ideal Sinc() LP filter with normFcut
        if ((float)n == fCenter)   //deal with odd size filter singularity where sin(0)/0==1
            c = 2.0f * normFcut;
        else
            c = (float)sinf(K_2PI * x * normFcut) / (K_PI * x);
        //calculate Kaiser window and multiply to get coefficient
        x = ((float)n - ((float)m_NumTaps - 1.0f) / 2.0f) / (((float)m_NumTaps - 1.0f) / 2.0f);
        m_Coef[n] = Scale * c * Izero(Beta * sqrtf(1 - (x * x))) / izb;
    }

    return;
}
