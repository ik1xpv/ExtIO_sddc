#pragma once
#pragma warning(disable : 4996)
// Oscar Steila ik1xpv 2017 - ik1xpv<at>gmail.com
// RFddc.h 
// converts the real input stream at 64Msps to 16Msps, 8Msps, 4Msps , 2Msps Complex I&Q

#include "config.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>
#include "fftw3.h"

#define RFDDCNAME ("NVIA L768M256")
#define RFDDCVER ("v0.5")

#define FRAMEN (65536)  // processed frame buffer len
#define FFTN (1024)     // FFT dimension
#define FFTXBUF (FRAMEN/FFTN)  	 // number of FFT per frame buffer (64)

#define HLEN (257)		// fir ht len max
#define WLEN (FFTN - HLEN +1)	// ie768

#define WLENXBUF (FRAMEN/WLEN)  	 // number of FFT per frame buffer (85)
#define EVENODD (2)		// even odd processing
#define NVIA (16)
#define D_FFTN (256)    // Maximum decimated fft dimension: max ( 256, 128, 64 )
#define D_256  (256)	// Decimnated FFT dimension 256
#define D_128  (128)	// Decimnated FFT dimension 128
#define D_64    (64) 	// Decimnated FFT dimension 64
#define D_32    (32) 	// Decimnated FFT dimension 32
#define DFRAMEN ( D_FFTN * FFTXBUF )  // decimated frame len max (64*256 = 16384)

// RF DDC globals
extern int gtunebin;
extern bool gR820Ton;

class rfdata 
{
public:
	rfdata():
	isdying(false),
	iipath(0),
	gainscale((float).51e-7), // FFT gain normalization
	initialized(false),
	tunebin(0)
	{}

	~rfdata() { 
	}
	std::mutex mut[NVIA];
	std::condition_variable data_cond[NVIA];
	bool active[NVIA];
	bool initialized;
	bool isdying;
	int iipath;
	unsigned char * pbufin[NVIA];			// ptr input
	float gainscale;
	int d_fftn;
	int tunebin;
};


/*
class thread_guard:
Using RAII to wait for a thread to complete
see ::
C++ Concurrency in Action
PRACTICAL M ULTITHREADING
ANTHONY WILLIAMS
ISBN: 9781933988771
*/
class thread_guard
{
	std::thread& t;
public:
	explicit thread_guard(std::thread& t_) :
		t(t_)
	{}
	~thread_guard()
	{
		if (t.joinable())
		{
			t.join();
		}
	}
	thread_guard(thread_guard const&) = delete;
	thread_guard& operator=(thread_guard const&) = delete;
};

// RF Digital Down Converter
class rfddc
{
public:
	rfddc();
	~rfddc();
	void initp(int dfftn, unsigned char * buffers[16]);   // init decimated fft dimension 256, 128, 64 
	int arun(int iix, fftwf_complex * bufout); // input short samples FRAMEN
private:
	rfdata mdata;    // initialization order is important
	std::thread t0;  
	std::thread t1;
	std::thread t2;
	std::thread t3;
	std::thread t4;
	std::thread t5;    
	std::thread t6;
	std::thread t7;
	std::thread t8;
	std::thread t9;
	std::thread t10;
	std::thread t11;
	std::thread t12;
	std::thread t13;
	std::thread t14;
	std::thread t15;
	thread_guard grd0;
	thread_guard grd1;
	thread_guard grd2;
	thread_guard grd3;
	thread_guard grd4;
	thread_guard grd5;
	thread_guard grd6;
	thread_guard grd7;
	thread_guard grd8;
	thread_guard grd9;
	thread_guard grd10;
	thread_guard grd11;
	thread_guard grd12;
	thread_guard grd13;
	thread_guard grd14;
	thread_guard grd15;

#ifdef _RFDDC_SEQUENCE
	FILE * handleW;
#endif
};





