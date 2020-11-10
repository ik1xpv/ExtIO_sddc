## ExtIO_sddc.dll (software digital down converter) - Oscar Steila, ik1xpv

### tag  v1.01 date 06/11/2020
       v1.01 RC1
        - SDDC_FX3 directory contains ARM sources and GPIFII project to compile sddc_fx3.img 
    - new software detects HW type: BBRF103, BBRF103, RX888 at run time.
    - Si5351a and R820T2 programming moved to FX3 code,
    - redesign of FX3 control commands.
    - rename of FX3handler (ex: OpenFX3) and RadioHandler (ex: BBRF103) modules
    - simplified ExtIO GUI Antenna BiasT, Dither, Rand.
    - reference frequency correction via software +/- 200 ppm range
     - gain adjust +/-20 dB step 1dB
    - R820T2 control rf gains via a single control in HDSDR GUI
    - ExtIO.dll designed for HDSDR use.
    - HF103 added tuning limit at ADC_FREQ/2.

### tag  v0.98  Version "SDDC-0.98" date  13/06/2019
   R820T2 is enabled 
   

### tag  v0.96  Version "SDDC-0.96" date  25/02/2018.


### tag  v0.95  Version "SDDC-0.95" date 31/08/2017
Init

## Build Instructions for ExtIO
    1. Install Visual Studio 2019
    1. Install CMake 3.19+
    1. Running the following commands
```
        > cd ExtIO_sddc
        > mkdir build
        > cd build
        > cmake ..
        > cmake --build .
```

* You need download fftw x86 library when you run the SDR application with this EXTIO DLL. Put fftw.dll with extio.

## Build Instructions for firmware

## Directory structure:

    \ExtIO_sddc\ 		> ExtIO_sddc sources,
        r2iq.cpp			> The logic to demodulize IQ from ADC real samples
        FX3handler.cpp		> Interface with firmware
        RadioHandler.cpp    > The abstraction of different radios
        tdialog.cpp			> The Configuration GUI Dialog

    \SDDC_FX3\          > Firmware sources


## References
    - Specification from http://www.sdradio.eu/weaksignals/bin/Winrad_Extio.pdf
    - http://www.weaksignals.com/
    - http://www.hdsdr.de
    - https://groups.io/g/NextGenSDRs
    - http://www.hdsdr.de/
    - http://booyasdr.sourceforge.net/
    - http://www.cypress.com/
   

## Many thanks to
            - Alberto di Bene, I2PHD
            - Mario Taeubel
            - LightCoder (aka LC)
            - Howard Su
            - Hayati Ayguen
            - Franco Venturi
            - All the Others !

2016,2017,2018,2019,2020  IK1XPV Oscar Steila - ik1xpv(at)gmail.com,
https://sdr-prototypes.blogspot.com/
http://www.steila.com/blog
