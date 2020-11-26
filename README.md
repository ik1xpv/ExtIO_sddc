## ExtIO_sddc.dll (software digital down converter) - Oscar Steila, ik1xpv

### tag  v1.01 Version "V1.01 RC1" date 06/11/2020
- SDDC_FX3 directory contains ARM sources and GPIFII project to compile sddc_fx3.img
- Detects the HW type: BBRF103, BBRF103, RX888 at runtime.
- Si5351a and R820T2 driver moved to FX3 code
- Redesign of FX3 control commands
- Rename of FX3handler (ex: OpenFX3) and RadioHandler (ex: BBRF103) modules
- Simplified ExtIO GUI Antenna BiasT, Dither, and Rand.
- Reference frequency correction via software +/- 200 ppm range
- Gain adjust +/-20 dB step 1dB
- R820T2 controls RF gains via a single control from GUI
- ExtIO.dll designed for HDSDR use.
- HF103 added a tuning limit at ADC_FREQ/2.

### tag  v0.98  Version "SDDC-0.98" date  13/06/2019
   R820T2 is enabled to support VHF

### tag  v0.96  Version "SDDC-0.96" date  25/02/2018

### tag  v0.95  Version "SDDC-0.95" date 31/08/2017

Initial version

## Build Instructions for ExtIO

1. Install Visual Studio 2019 with Visual C++ support. You can use the free community version, which can be downloaded from: https://visualstudio.microsoft.com/downloads/
1. Install CMake 3.19+, https://cmake.org/download/
1. Running the following commands in your cloned repro:

```bash
> cd ExtIO_sddc
> mkdir build
> cd build
> cmake ..
> cmake --build .
```

* You need to download **32bit version** of fftw library from fftw website http://www.fftw.org/install/windows.html. Copy libfftw3f-3.dll from the downloaded zip package to the same folder of extio DLL.

* If you are running **64bit** OS, you need to run the following different commands instead of "cmake .." based on your Visual Studio Version:
```
VS2019: >cmake .. -G "Visual Studio 16 2019" -A Win32
VS2017: >cmake .. -G "Visual Studio 15 2017 Win32"
VS2015: >cmake .. -G "Visual Studio 14 2015 Win32"
```

## Build Instructions for firmware

//TODO

## Directory structure:

    \ExtIO_sddc\ 		> ExtIO_sddc sources,
        r2iq.cpp			> The logic to demodulize IQ from ADC real samples
        FX3handler.cpp		> Interface with firmware
        RadioHandler.cpp    > The abstraction of different radios
        tdialog.cpp			> The Configuration GUI Dialog
    
    \SDDC_FX3\          > Firmware sources


## References
- EXTIO Specification from http://www.sdradio.eu/weaksignals/bin/Winrad_Extio.pdf
- Discussion and Support https://groups.io/g/NextGenSDRs
- Recommended Application http://www.weaksignals.com/
- http://www.hdsdr.de
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

#### 2016,2017,2018,2019,2020  IK1XPV Oscar Steila - ik1xpv(at)gmail.com
https://sdr-prototypes.blogspot.com/

http://www.steila.com/blog
