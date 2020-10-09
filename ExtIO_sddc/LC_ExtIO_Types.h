//---------------------------------------------------------------------------

#ifndef LC_ExtIO_TypesH
#define LC_ExtIO_TypesH

// specification from http://www.sdradio.eu/weaksignals/bin/Winrad_Extio.pdf
// linked referenced from http://www.weaksignals.com/

// for C99 compiler just #include <stdint.h>
// MS VC++ 2008 Express does not have stdint.h Try http://msinttypes.googlecode.com/svn/trunk/stdint.h
#include "pstdint.h"
// for other compiles you may try http://www.azillionmonkeys.com/qed/pstdint.h
// or try boost: http://www.boost.org/doc/libs/1_36_0/boost/cstdint.hpp

/*
 * I. INITIALIZATION + OPEN SEQUENCE
 * =================================
 * ExtIoSDRFeatures()
 *   optional: inform ExtIO of SDR software's supported features
 * ExtIoSetSetting()
 *   optional: previously saved settings are delivered to ExtIO
 *   call once with idx -1 to sign ExtIO that this functionality is supported,
 *   so that ExtIO may inhibit loading/storing any .ini
 * InitHW()
 *   mandatory: initialize ExtIO. May do nothing!
 * VersionInfo()
 *   optional: delivers SDR program name and version to ExtIO. Allows to check if some necessary
 *     functional extension is (not) supported by SDR program
 * GetAttenuators()
 *   optional: show ExtIO, that SDR program supports controlling RF Gain/Attenuator(s)
 *     you should prefer this over determining SDR program's capabilities over VersionInfo()
 * ExtIoGetMGCs()
 *   optional: show ExtIO, that SDR program supports controlling IF Gain/Attenuator(s)
 *     you should prefer this over determining SDR program's capabilities over VersionInfo()
 * [ ActivateTx() ]
 *   optional: try's to activate Tx functionality
 * SetCallback()
 *   mandatory: callback function pointer is given to ExtIO. ExtIO may inform SDR program of events
 *     using this callback interface and the extHWstatusT enums
 * OpenHW()
 *   mandatory: prepare ExtIO for start .. or fail for any reason!
 *
 *
 * II. START SEQUENCE
 * ==================
 * StartHW()
 *   mandatory: start processing
 *
 *
 * III. WORK
 * =========
 * SetHWLO() and many other functions ...
 *
 *
 * IV. STOP SEQUENCE (== 'undo' of Start sequence)
 * =================
 * StopHW()
 *   mandatory: start processing
 *
 *
 * V. CLOSE SEQUENCE (== 'undo' of init + stop sequence)
 * =================
 * ExtIoGetSetting()
 *   optional: get and save settings for next time.
 *    call ExtIoGetSetting() before CloseHW() to get correct settings.
 *    do not call without súccessful OpenHW()
 * CloseHW()
 *   mandatory: close hardware. Processing is not started again (with StartHW),
 *   unless OpenHW() is called again
 *   this function is called only when prior OpenHW() was successful
 *   take care not to free already freed or never allocated resources
 *
 */

// function implemented by Winrad / HDSDR; see enum extHWstatusT below
typedef int               (* pfnExtIOCallback)  (int cntr, int status, float IQoffs, void *IQdata);

// mandatory functions, which have to be implemented by ExtIO DLL
#define EXTIO_MAX_NAME_LEN    16    /* name is displayed in Winrad/HDSDR's menu */
#define EXTIO_MAX_MODEL_LEN   16    /* model is not used */
typedef bool    (__stdcall * pfnInitHW)         (char *name, char *model, int& hwtype);
                // name: descriptive name of the hardware.
                //     Preferably not longer than about 16 characters,
                //     as it will be used in a Winrad menu
                // model: model code of the hardware,
                //     or its Serial Number.
                //     Keep also this field not too long,
                //     for the same reason of the previous one
                // hwtype: see enum extHWtypeT below
                // return: true if everything went well

typedef bool    (__stdcall * pfnOpenHW)         (void);
                // return: true if everything went well

typedef void    (__stdcall * pfnCloseHW)        (void);
typedef int     (__stdcall * pfnStartHW)        (long extLOfreq);
                // return: An integer specifying how many I/Q pairs are returned
                //     by the DLL each time the callback function is invoked (see later).
                //     This information is used of course only when the input data
                //     are not coming from the sound card, but through the callback device.
                //     If the number is negative, that means that an error has occurred,
                //     Winrad interrupts the starting process and returns to the
                //     idle status.
                //     The number of I/Q pairs must be at least 512, or an integer
                //     multiple of that value,

typedef void    (__stdcall * pfnStopHW)         (void);
typedef void    (__stdcall * pfnSetCallback)    (pfnExtIOCallback funcptr);
typedef int     (__stdcall * pfnSetHWLO)        (long extLOfreq);   // see also SetHWLO64
                // return values:
                //     == 0: The function did complete without errors.
                //     < 0 (a negative number N):
                //           The specified frequency is lower than the minimum that
                //           the hardware is capable to generate. The absolute value
                //           of N indicates what is the minimum supported by the HW.
                //     > 0 (a positive number N):
                //           The specified frequency is greater than the maximum
                //           that the hardware is capable to generate. The value
                //           of N indicates what is the maximum supported by the HW.

typedef int     (__stdcall * pfnGetStatus)      (void);
                //     This entry point is meant to allow the DLL to return a status
                //     information to Winrad, upon request.
                //     Presently it is never called by Winrad, though its existence
                //     is checked when the DLL is loaded. So it must implemented,
                //     even if in a dummy way.
                //     It is meant for future expansions, for complex HW that implement
                //     e.g. a preselector or some other controls other than a simple
                //     LO frequency selection.
                //     The return value is an integer that is application dependent.

// optional functions, which can be implemented by ExtIO DLL
// for performance reasons prefer not implementing rather then implementing empty functions
//   especially for RawDataReady
typedef long    (__stdcall * pfnGetHWLO)        (void);             // see also GetHWLO64
typedef long    (__stdcall * pfnGetHWSR)        (void);
typedef void    (__stdcall * pfnRawDataReady)   (long samprate, void *Ldata, void *Rdata, int numsamples);
typedef void    (__stdcall * pfnShowGUI)        (void);
typedef void    (__stdcall * pfnHideGUI)        (void);
typedef void    (__stdcall * pfnSwitchGUI)      (void);             // new: switch visibility of GUI
typedef void    (__stdcall * pfnTuneChanged)    (long tunefreq);    // see also TuneChanged64
typedef long    (__stdcall * pfnGetTune)        (void);             // see also GetTune64
typedef void    (__stdcall * pfnModeChanged)    (char mode);
typedef char    (__stdcall * pfnGetMode)        (void);
typedef void    (__stdcall * pfnIFLimitsChanged)(long lowfreq, long highfreq);  // see also IFLimitsChanged64
typedef void    (__stdcall * pfnFiltersChanged) (int loCut, int hiCut, int pitch);  // lo/hiCut relative to tuneFreq
typedef void    (__stdcall * pfnMuteChanged)    (bool muted);
typedef void    (__stdcall * pfnGetFilters)     (int& loCut, int& hiCut, int& pitch);

// optional functions - extended for receivers with frequency range over 2147 MHz - used from HDSDR
// these functions 64 bit functions are prefered rather than using the 32 bit ones
// for other Winrad derivations you should additionally implement the above "usual" 32 bit functions
typedef int     (__stdcall * pfnStartHW64)      (int64_t extLOfreq);    // "StartHW64" with HDSDR >= 2.14
typedef int64_t (__stdcall * pfnSetHWLO64)      (int64_t extLOfreq);
typedef int64_t (__stdcall * pfnGetHWLO64)      (void);
typedef void    (__stdcall * pfnTuneChanged64)  (int64_t tunefreq);
typedef int64_t (__stdcall * pfnGetTune64)      (void);
typedef void    (__stdcall * pfnIFLimitsChanged64)  (int64_t lowfreq, int64_t highfreq);

// optional functions - extended for high precision
typedef int     (__stdcall * pfnStartHW_dbl)    (double extLOfreq);
typedef double  (__stdcall * pfnSetHWLO_dbl)    (double extLOfreq);
typedef double  (__stdcall * pfnGetHWLO_dbl)    (void);
typedef void    (__stdcall * pfnTuneChanged_dbl)(double tunefreq);
typedef double  (__stdcall * pfnGetTune_dbl)    (void);
typedef void    (__stdcall * pfnIFLimitsChanged_dbl)  (double lowfreq, double highfreq);


// optional functions, which can be implemented by ExtIO DLL
// following functions may get called from HDSDR 2.13 and above

// "VersionInfo" is called - when existing - after successful InitHW()
// with this information an ExtIO may check which extHWstatusT enums are properly processed from application
// this call shall no longer be used to determine features of the SDR
//   use "ExtIoSDRInfo" for this purpose
typedef void    (__stdcall * pfnVersionInfo)    (const char * progname, int ver_major, int ver_minor);


// "GetAttenuators" allows HDSDR to display a knob or slider for Attenuation / Amplification
// see & use extHw_Changed_ATT enum if ATT can get changed by ExtIO dialog window or from hardware
#define EXTIO_MAX_ATT_GAIN_VALUES 128
typedef int     (__stdcall * pfnGetAttenuators) (int idx, float * attenuation);  // fill in attenuation
                             // use positive attenuation levels if signal is amplified (LNA)
                             // use negative attenuation levels if signal is attenuated
                             // sort by attenuation: use idx 0 for highest attenuation / most damping
                             // this functions is called with incrementing idx
                             //    - until this functions returns != 0, which means that all attens are already delivered
typedef int     (__stdcall * pfnGetActualAttIdx)(void);                          // returns -1 on error
typedef int     (__stdcall * pfnSetAttenuator)  (int idx);                       // returns != 0 on error

// "SetModeRxTx" will only get called after a successful activation with ActivateTx()
// see extHw_TX_Request/extHw_RX_Request enums below if modehange can get triggered from user / hardware
typedef int     (__stdcall * pfnSetModeRxTx)    (int modeRxTx);             // see enum extHw_ModeRxTxT

// generation and transmission of I/Q samples for TX mode requires activation of the ExtIO DLL
// contact LC at hdsdr.de  to ask for activation keys and algorithm
typedef int     (__stdcall * pfnActivateTx)     (int magicA, int magicB);

// (de)activate all bandpass filter to allow "bandpass undersampling" (with external analog bandpass filter)
// intended for future use: it may get set automatically depending on LO frequency and the "ExtIO Frequency Options"
//   deactivation of bp/lp-filters when real LO (in HDSDR) is > ADC_Samplerate/2 in undersampling mode
typedef int     (__stdcall * pfnDeactivateBP)   (int deactivate);
                             // deactivate == 1 to deactivate all bandpass and lowpass filters of hardware
                             // deactivate == 0 to reactivate automatic bandpass selection depending on frequency

// optional "ExtIoGetSrates" is for replacing the Soundcard Samplerate values in the Samplerate selection dialog
//   by these values supported from the SDR hardware.
// see & use extHw_Changed_SampleRate enum ... and "GetHWSR". Enumeration API as with "GetAttenuators"
// intended for future use - actually not implemented/called
#define EXTIO_MAX_SRATE_VALUES  32
typedef int     (__stdcall * pfnExtIoGetSrates) (int idx, double * samplerate);  // fill in possible samplerates
                             // this functions is called with incrementing idx
                             //    - until this functions returns != 0, which means that all srates are already delivered
typedef int     (__stdcall * pfnExtIoGetActualSrateIdx) (void);               // returns -1 on error
typedef int     (__stdcall * pfnExtIoSetSrate)  (int idx);                    // returns != 0 on error

// optional function to get 3dB bandwidth from samplerate
typedef long    (__stdcall * pfnExtIoGetBandwidth)  (int srate_idx);       // returns <= 0 on error

// optional function to get center (= IF frequency) of 3dB band in Hz - for non I/Q receivers with 0 center
typedef long    (__stdcall * pfnExtIoGetBwCenter)   (int srate_idx);       // returns 0 on error, which is default

// optional function to get AGC Mode: AGC_OFF (always agc_index = 0), AGC_SLOW, AGC_MEDIUM, AGC_FAST, ...
// this functions is called with incrementing idx
//    - until this functions returns != 0, which means that all agc modes are already delivered
#define EXTIO_MAX_AGC_VALUES  16
typedef int     (__stdcall * pfnExtIoGetAGCs) (int agc_idx, char * text);  // text limited to max 16 char
typedef int     (__stdcall * pfnExtIoGetActualAGCidx)(void);               // returns -1 on error
typedef int     (__stdcall * pfnExtIoSetAGC) (int agc_idx);                // returns != 0 on error
// optional: HDSDR >= 2.62
typedef int     (__stdcall * pfnExtIoShowMGC)(int agc_idx);                // return 1, to continue showing MGC slider on AGC
                                                                           // return 0, is default for not showing MGC slider

// for AGC in AGC_OFF (agc_idx == 0), which is (M)anual (G)ain (C)ontrol
// sometimes referred as "IFgain" - as in SDR-14/IP
#define EXTIO_MAX_MGC_VALUES  128
typedef int     (__stdcall * pfnExtIoGetMGCs)(int mgc_idx, float * gain);  // fill in gain
                             // sort by ascending gain: use idx 0 for lowest gain
                             // this functions is called with incrementing idx
                             //    - until this functions returns != 0, which means that all gains are already delivered
typedef int     (__stdcall * pfnExtIoGetActualMgcIdx) (void);              // returns -1 on error
typedef int     (__stdcall * pfnExtIoSetMGC) (int mgc_idx);                // returns != 0 on error


// not used in HDSDR - for now
// optional function to get 3dB band of Preselectors
// this functions is called with incrementing idx
//    - until this functions returns != 0, which means that all preselectors are already delivered
// ExtIoSetPresel() with idx = -1 to activate automatic preselector selection
// ExtIoSetPresel() with valid idx (>=0) deactivates automatic preselection
typedef int     (__stdcall * pfnExtIoGetPresels)         ( int idx, int64_t * freq_low, int64_t * freq_high );
typedef int     (__stdcall * pfnExtIoGetActualPreselIdx) ( void );      // returns -1 on error
typedef int     (__stdcall * pfnExtIoSetPresel)          ( int idx );   // returns != 0 on error

// not used in HDSDR - for now
// optional function to get frequency ranges usable with SetHWLO(),
//   f.e. the FUNcube Dongle Pro+ should deliver idx 0: low=0.15 high=250 MHz and idx 1: low=420 high=1900 MHz
//        with a gap from 250MHz to 420 MHz. see http://www.funcubedongle.com/?page_id=1073
//   if extIO is told to set a not-supported frequency with SetHWLO(), then the extIO should callback with extHw_Changed_LO
//     and set a new frequency, which is supported
// this functions is called with incrementing idx
//    - until this functions returns != 0, which means that all frequency ranges are already delivered
typedef int     (__stdcall * pfnExtIoGetFreqRanges)        ( int idx, int64_t * freq_low, int64_t * freq_high );

// not used in HDSDR - for now
// optional function to get full samplerate of A/D Converter
//   useful to know with direct samplers in bandpass undersampling mode
//   example: Perseus = 80 000 000 ; SDR-14 = 66 666 667
//   return <= 0 if undersampling not supported (when preselectors not deactivatable)
typedef double  (__stdcall * pfnExtIoGetAdcSrate) ( void );

// HDSDR >= 2.51
// optional functions to receive and set all special receiver settings (for save/restore in application)
//   allows application and profile specific settings.
//   easy to handle without problems with newer Windows versions saving a .ini file below programs as non-admin-user
// Settings shall be zero-terminated C-Strings.
// example settings: USB-Identifier(for opening specific device), IP/Port, AGC, Srate, ..
// idx in 0 .. 999 => NOT more than 1000 values storable!
// description max 1024 char
// value max 1024 char
// these functions are called with incrementing idx: 0, 1, ...
// until ExtIoGetSetting() returns != 0, which means that all settings are already delivered
typedef int     (__stdcall * pfnExtIoGetSetting) ( int idx, char * description, char * value ); // will be called (at least) before exiting application
typedef void    (__stdcall * pfnExtIoSetSetting) ( int idx, const char * value ); // before calling InitHW() !!!
  // there will be an extra call with idx = -1, if theses functions are supported by the SDR app
  // suggestion: use index 0 as ExtIO identifier (save/check ExtIO name) to allow fast skipping of all following SetSetting calls
  //   when this identifier does not match

// not used in HDSDR - for now
// handling of VFOs - see also extHw_Changed_VFO
// VFOindex is in 0 .. numVFO-1
typedef void    (__stdcall * pfnExtIoVFOchanged) ( int VFOindex, int numVFO, int64_t extLOfreq, int64_t tunefreq, char mode );
typedef int     (__stdcall * pfnExtIoGetVFOindex)( void );   // returns new VFOindex


// HDSDR > 2.70

typedef void    (__stdcall * pfnExtIoSDRInfo)( int extSDRInfo, int additionalValue, void * additionalPtr );




// hwtype codes to be set with pfnInitHW
// Please ask Alberto di Bene (i2phd@weaksignals.com) for the assignment of an index code
// for cases different from the above.
// note: "exthwUSBdataNN" don't need to be from USB. The keyword "USB" is just for historical reasons,
//   which may get removed later ..
typedef enum
{
    exthwNone       = 0
  , exthwSDR14      = 1
  , exthwSDRX       = 2
  , exthwUSBdata16  = 3 // the hardware does its own digitization and the audio data are returned to Winrad
                        // via the callback device. Data must be in 16-bit  (short) format, little endian.
                        //   each sample occupies 2 bytes (=16 bits) with values from  -2^15 to +2^15 -1
  , exthwSCdata     = 4 // The audio data are returned via the (S)ound (C)ard managed by Winrad. The external
                        // hardware just controls the LO, and possibly a preselector, under DLL control.
  , exthwUSBdata24  = 5 // the hardware does its own digitization and the audio data are returned to Winrad
                        // via the callback device. Data are in 24-bit  integer format, little endian.
                        //   each sample just occupies 3 bytes (=24 bits) with values from -2^23 to +2^23 -1
  , exthwUSBdata32  = 6 // the hardware does its own digitization and the audio data are returned to Winrad
                        // via the callback device. Data are in 32-bit  integer format, little endian.
                        //   each sample occupies 4 bytes (=32 bits) but with values from  -2^23 to +2^23 -1
  , exthwUSBfloat32 = 7 // the hardware does its own digitization and the audio data are returned to Winrad
                        // via the callback device. Data are in 32-bit  float format, little endian.
  , exthwHPSDR      = 8 // for HPSDR only!

  // HDSDR > 2.70
  , exthwUSBdataU8  = 9 // the hardware does its own digitization and the audio data are returned to Winrad
                        // via the callback device. Data must be in 8-bit  (unsigned) format, little endian.
                        //   intended for RTL2832U based DVB-T USB sticks
                        //   each sample occupies 1 byte (=8 bit) with values from 0 to 255
  , exthwUSBdataS8  = 10// the hardware does its own digitization and the audio data are returned to Winrad
                        // via the callback device. Data must be in 8-bit  (signed) format, little endian.
                        //   each sample occupies 1 byte (=8 bit) with values from -128 to 127
  , exthwFullPCM32  = 11 // the hardware does its own digitization and the audio data are returned to Winrad
                        // via the callback device. Data are in 32-bit  integer format, little endian.
                        //   each sample occupies 4 bytes (=32 bits) with full range: from  -2^31 to +2^31 -1
} extHWtypeT;

// status codes for pfnExtIOCallback; used when cnt < 0
typedef enum
{
  // only processed/understood for SDR14
    extHw_Disconnected        = 0     // SDR-14/IQ not connected or powered off
  , extHw_READY               = 1     // IDLE / Ready
  , extHw_RUNNING             = 2     // RUNNING  => not disconnected
  , extHw_ERROR               = 3     // ??
  , extHw_OVERLOAD            = 4     // OVERLOAD => not disconnected

  // for all extIO's
  , extHw_Changed_SampleRate  = 100   // sampling speed has changed in the external HW
  , extHw_Changed_LO          = 101   // LO frequency has changed in the external HW
  , extHw_Lock_LO             = 102
  , extHw_Unlock_LO           = 103
  , extHw_Changed_LO_Not_TUNE = 104   // CURRENTLY NOT YET IMPLEMENTED
                                      // LO freq. has changed, Winrad must keep the Tune freq. unchanged
                                      // (must immediately call GetHWLO() )
  , extHw_Changed_TUNE        = 105   // a change of the Tune freq. is being requested.
                                      // Winrad must call GetTune() to know which value is wanted
  , extHw_Changed_MODE        = 106   // a change of demod. mode is being requested.
                                      // Winrad must call GetMode() to know the new mode
  , extHw_Start               = 107   // The DLL wants Winrad to Start
  , extHw_Stop                = 108   // The DLL wants Winrad to Stop
  , extHw_Changed_FILTER      = 109   // a change in the band limits is being requested
                                      // Winrad must call GetFilters()

  // Above status codes are processed with Winrad 1.32.
  //   All Winrad derivation like WRplus, WinradF, WinradHD and HDSDR should understand them,
  //   but these do not provide version info with VersionInfo(progname, ver_major, ver_minor).

  , extHw_Mercury_DAC_ON      = 110   // enable audio output on the Mercury DAC when using the HPSDR
  , extHw_Mercury_DAC_OFF     = 111   // disable audio output on the Mercury DAC when using the HPSDR
  , extHw_PC_Audio_ON         = 112   // enable audio output on the PC sound card when using the HPSDR
  , extHw_PC_Audio_OFF        = 113   // disable audio output on the PC sound card when using the HPSDR

  , extHw_Audio_MUTE_ON       = 114   // the DLL is asking Winrad to mute the audio output
  , extHw_Audio_MUTE_OFF      = 115   // the DLL is asking Winrad to unmute the audio output

  // Above status codes are processed with Winrad 1.33 and HDSDR
  //   Winrad 1.33 and HDSDR still do not provide their version with VersionInfo()


  // Following status codes are processed when VersionInfo delivers
  //  0 == strcmp(progname, "HDSDR") && ( ver_major > 2 || ( ver_major == 2 && ver_minor >= 13 ) )

  // all extHw_XX_SwapIQ_YYY callbacks shall be reported after each OpenHW() call
  , extHw_RX_SwapIQ_ON        = 116   // additionaly swap IQ - this does not modify the menu point / user selection
  , extHw_RX_SwapIQ_OFF       = 117   //   the user selected swapIQ is additionally applied
  , extHw_TX_SwapIQ_ON        = 118   // additionaly swap IQ - this does not modify the menu point / user selection
  , extHw_TX_SwapIQ_OFF       = 119   //   the user selected swapIQ is additionally applied


  // Following status codes (for I/Q transceivers) are processed when VersionInfo delivers
  //  0 == strcmp(progname, "HDSDR") && ( ver_major > 2 || ( ver_major == 2 && ver_minor >= 13 ) )

  , extHw_TX_Request          = 120   // DLL requests TX mode / User pressed PTT
                                      //   exciter/transmitter must wait until SetModeRxTx() is called!
  , extHw_RX_Request          = 121   // DLL wants to leave TX mode / User released PTT
                                      //   exciter/transmitter must wait until SetModeRxTx() is called!
  , extHw_CW_Pressed          = 122   // User pressed  CW key
  , extHw_CW_Released         = 123   // User released CW key
  , extHw_PTT_as_CWkey        = 124   // handle extHw_TX_Request as extHw_CW_Pressed in CW mode
                                      //  and   extHw_RX_Request as extHw_CW_Released
  , extHw_Changed_ATT         = 125   // Attenuator changed => call GetActualAttIdx()


  // Following status codes are processed when VersionInfo delivers
  //  0 == strcmp(progname, "HDSDR") && ( ver_major > 2 || ( ver_major == 2 && ver_minor >= 14 ) )

#if 0
  // Following status codes are for future use - actually not implemented !
  //  0 == strcmp(progname, "HDSDR") && ( ver_major > 2 || ( ver_major == 2 && ver_minor >>> 14 ) )

  // following status codes to change sampleformat at runtime
  , extHw_SampleFmt_IQ_UINT8  = 126   // change sample format to unsigned 8 bit INT (Realtek RTL2832U)
  , extHw_SampleFmt_IQ_INT16  = 127   //           -"-           signed 16 bit INT
  , extHw_SampleFmt_IQ_INT24  = 128   //           -"-           signed 24 bit INT
  , extHw_SampleFmt_IQ_INT32  = 129   //           -"-           signed 32 bit INT
  , extHw_SampleFmt_IQ_FLT32  = 130   //           -"-           signed 16 bit FLOAT
#endif
  // following status codes to change channel mode at runtime
  , extHw_RX_ChanMode_LEFT    = 131   // left channel only
  , extHw_RX_ChanMode_RIGHT   = 132   // right channel only
  , extHw_RX_ChanMode_SUM_LR  = 133   // sum of left + right channel
  , extHw_RX_ChanMode_I_Q     = 134   // I/Q with left  channel = Inphase and right channel = Quadrature
                                      // last option set I/Q and clear internal swap as with extHw_RX_SwapIQ_OFF
  , extHw_RX_ChanMode_Q_I     = 135   // I/Q with right channel = Inphase and left  channel = Quadrature
                                      // last option set I/Q and internal swap as with extHw_RX_SwapIQ_ON

  , extHw_Changed_RF_IF       = 136   // refresh selectable attenuators and Gains
                                        // => starts calling GetAttenuators(), GetAGCs() & GetMGCs()
  , extHw_Changed_SRATES      = 137   // refresh selectable samplerates => starts calling GetSamplerates()

  // Following status codes are for 3rd Party Software, currently not implemented in HDSDR
  , extHw_Changed_PRESEL      = 138  // Preselector changed => call ExtIoGetActualPreselIdx()
  , extHw_Changed_PRESELS     = 139  // refresh selectable preselectors => start calling ExtIoGetPresels()
  , extHw_Changed_AGC         = 140  // AGC changed => call ExtIoGetActualAGCidx()
  , extHw_Changed_AGCS        = 141  // refresh selectable AGCs => start calling ExtIoGetAGCs()
  , extHw_Changed_SETTINGS    = 142  // settings changed, call ExtIoGetSetting()
  , extHw_Changed_FREQRANGES  = 143  // refresh selectable frequency ranges, call ExtIoGetFreqRanges()

  , extHw_Changed_VFO         = 144  // refresh selectable VFO => starts calling ExtIoGetVFOindex()

  // Following status codes are processed when VersionInfo delivers
  //  0 == strcmp(progname, "HDSDR") && ( ver_major > 2 || ( ver_major == 2 && ver_minor >= 60 ) )
  , extHw_Changed_MGC         = 145  // MGC changed => call ExtIoGetMGC()

} extHWstatusT;

// codes for pfnSetModeRxTx:
typedef enum
{
    extHw_modeRX  = 0
  , extHw_modeTX  = 1
} extHw_ModeRxTxT;


// codes for pfnExt
typedef enum
{
    extSDR_NoInfo                 = 0   // sign SDR features would be signed with subsequent calls
  , extSDR_supports_Settings      = 1
  , extSDR_supports_Atten         = 2   // RF Attenuation / Gain may be set via pfnSetAttenuator()
  , extSDR_supports_TX            = 3   // pfnSetModeRxTx() may be called
  , extSDR_controls_BP            = 4   // pfnDeactivateBP() may be called
  , extSDR_supports_AGC           = 5   // pfnExtIoSetAGC() may be called
  , extSDR_supports_MGC           = 6   // IF Attenuation / Gain may be set via pfnExtIoSetMGC()
  , extSDR_supports_PCMU8         = 7   // exthwUSBdataU8 is supported
  , extSDR_supports_PCMS8         = 8   // exthwUSBdataS8 is supported
  , extSDR_supports_PCM32         = 9   // exthwFullPCM32 is supported
} extSDR_InfoT;


// macro to call callback function with just status extHWstatusT
#define EXTIO_STATUS_CHANGE( CB, STATUS )   CB( -1, STATUS, 0, NULL )

#endif /* LC_ExtIO_TypesH */

