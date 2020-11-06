# ExtIO_sddc.dll (software digital down converter) - Oscar Steila, ik1xpv

tag  v1.01 date 06/11/2020
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


It requires:
	Visual Studio 2019 
	fftw libraries ftp://ftp.fftw.org/pub/fftw/fftw-3.3.5-dll32.zip
				( copy fftw-3.3.5-dll32.zip contens to /lib/fftw )
	CyAPI.cpp library source can be downloaded at http://www.cypress.com/file/289981/download (see License)
				( directory lib\CyAPI_gcc contains the CodeBlocks project to compile under gcc)


tag  v0.98  Version "SDDC-0.98" date  13/06/2019
   R820T2 is enabled 
   

tag  v0.96  Version "SDDC-0.96" date  25/02/2018.

It requires:
	CodeBlocks 12.11 IDE or later,  https://sourceforge.net/projects/codeblocks/files/Binaries/12.11/Windows/ 
	
	pthreads   https://sourceforge.net/projects/pthreads4w/files/pthreads-w32-2-9-1-release.zip/download
				( copy  Pre-built.2 directory contens into /lib/pthreads/ )
	fftw libraries ftp://ftp.fftw.org/pub/fftw/fftw-3.3.5-dll32.zip
				( copy fftw-3.3.5-dll32.zip contens to /lib/fftw )
	CyAPI.cpp library source can be downloaded at http://www.cypress.com/file/289981/download (see License)
				( directory lib\CyAPI_gcc contains the CodeBlocks project to compile under gcc)
	
Directory structure:

	\Source\ 		> ExtIO_sddc sources + ExtIO_sddc.cbp (CodeBlocks Project),
	\Lib\fftw   	> fftw library here,
		\pthreads	> pthreads library here,
		\CyAPI_gcc	> CyAPI gcc library here,
	\bin\debug		> debug, 
		\release	> release.


tag  v0.95  Version "SDDC-0.95" date 31/08/2017
Init


// Specification from http://www.sdradio.eu/weaksignals/bin/Winrad_Extio.pdf
// http://www.weaksignals.com/
// http://www.hdsdr.de
// Many thanks to
            Alberto di Bene, I2PHD
            Mario Taeubel
            LightCoder (aka LC)
            Howard Su
            Hayati Ayguen 
	    Franco Venturi
            All the Others !

2016,2017,2018,2019,2020  IK1XPV Oscar Steila - ik1xpv(at)gmail.com,
https://sdr-prototypes.blogspot.com/
http://www.steila.com/blog
https://groups.io/g/NextGenSDRs  
http://www.hdsdr.de/
http://booyasdr.sourceforge.net/
http://www.cypress.com/  
   

