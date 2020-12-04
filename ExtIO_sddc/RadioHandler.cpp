#include "license.txt" 

#include <stdio.h>
#include "RadioHandler.h"
#include "config.h"
#include "r2iq.h"
#include "config.h"
#include "PScope_uti.h"

#include <chrono>

#define BLOCK_TIMEOUT (80) // block 65.536 ms timeout is 80

using namespace std::chrono;

extern pfnExtIOCallback	pfnCallback;

RadioHandlerClass RadioHandler;
std::thread* adc_samples_thread;
std::thread* show_stats_thread;
// transfer variables
static PUCHAR* buffers;                       // export, main data buffers
static PUCHAR* contexts;
static OVERLAPPED	inOvLap[QUEUE_SIZE];
static float** obuffers;

// transfer variables
double kbRead = 0;
high_resolution_clock::time_point StartingTime;

unsigned long BytesXferred = 0;
unsigned long SamplesXIF = 0;
double kSReadIF = 0;
unsigned long Failures = 0;
float	g_Bps;
float	g_SpsIF;

void RadioHandlerClass::AdcSamplesProcess()
{
	DbgPrintf("AdcSamplesProc thread runs\n");
	int callShowStatsRate = 32 * QUEUE_SIZE;
	int idx;            // queue index              // export
	unsigned long count;  // absolute index

	Failures = 0;
	int odx = 0;
	int rd = r2iqCntrl.getRatio();

	hardware->FX3producerOn();  // FX3 start the producer

	// Queue-up the first batch of transfer requests
	for (int n = 0; n < QUEUE_SIZE; n++) {
		contexts[n] = EndPt->BeginDataXfer(buffers[n], transferSize, &inOvLap[n]);
		if (EndPt->NtStatus || EndPt->UsbdStatus) {// BeginDataXfer failed
			DbgPrintf((char*)"Xfer request rejected. 1 STATUS = %ld %ld\n", EndPt->NtStatus, EndPt->UsbdStatus);
			return;
		}
	}
	run = true;
	idx = 0;    // buffer cycle index
	count = 0;    // absolute index
	StartingTime = high_resolution_clock::now();
	// The infinite xfer loop.
	while (run) {
		LONG rLen = transferSize;	// Reset this each time through because
		// FinishDataXfer may modify it
		if (!EndPt->WaitForXfer(&inOvLap[idx], BLOCK_TIMEOUT)) { // block on transfer
			EndPt->Abort(); // abort if timeout
			DbgPrintf("BUG1001 Count %d idx %d", count, idx);
			// Re-submit this queue element to keep the queue full
			contexts[idx] = EndPt->BeginDataXfer(buffers[idx], transferSize, &inOvLap[idx]);
			if (EndPt->NtStatus || EndPt->UsbdStatus) { // BeginDataXfer failed
				DbgPrintf("Xfer request rejected. NTSTATUS = 0x%08X\n", (UINT)EndPt->NtStatus);
				AbortXferLoop(idx);
				break;
			}

		}

		if (EndPt->FinishDataXfer(buffers[idx], rLen, &inOvLap[idx], contexts[idx])) {
			BytesXferred += rLen;
			if (rLen < transferSize) DbgPrintf("rLen = %ld\n", rLen);
			// submit result to SDR application before processing next packet
			if (pfnCallback && run && count > QUEUE_SIZE + 1)
			{
				if (rd == 1)
				{
					{
						std::unique_lock<std::mutex> lk(fc_mutex);
						if (fc != 0.0f)
						{
							shift_limited_unroll_C_sse_inp_c((complexf*)obuffers[idx], EXT_BLOCKLEN, &stateFineTune);
						}
					}

					pfnCallback(EXT_BLOCKLEN, 0, 0.0F, obuffers[idx]);
					SamplesXIF += EXT_BLOCKLEN;
				}
				else
				{
					odx = (idx + 1) / rd;
					if ((odx * rd) == (idx + 1))
					{
						{
							std::unique_lock<std::mutex> lk(fc_mutex);
							if (fc != 0.0f)
							{
								shift_limited_unroll_C_sse_inp_c((complexf*)obuffers[idx / rd], EXT_BLOCKLEN, &stateFineTune);
							}
						}
						pfnCallback(EXT_BLOCKLEN, 0, 0.0F, obuffers[idx / rd]);
						SamplesXIF += EXT_BLOCKLEN;
					}
				}
			}

			r2iqCntrl.DataReady();   // inform r2iq buffer ready

#ifdef _DEBUG		//PScope buffer screenshot
			if (saveADCsamplesflag == true)
			{
				saveADCsamplesflag = false; // do it once
				short* pi = (short*)&buffers[idx][0];
				unsigned int numsamples = transferSize / sizeof(short);
				float samplerate  = (float) getSampleRate();
				PScopeShot("ADCrealsamples.adc", "ExtIO_sddc.dll",
					"ADCrealsamples.adc input real ADC 16 bit samples",
					pi, samplerate, numsamples);
			}
#endif
		}

		// Re-submit this queue element to keep the queue full
		contexts[idx] = EndPt->BeginDataXfer(buffers[idx], transferSize, &inOvLap[idx]);
		if (EndPt->NtStatus || EndPt->UsbdStatus) { // BeginDataXfer failed
			DbgPrintf("Xfer request rejected.2 NTSTATUS = 0x%08X\n", (UINT)EndPt->NtStatus);
			AbortXferLoop(idx);
			break;
		}

		if ((count % callShowStatsRate) == 0) { //Only update the display at the call rate
			mutexShowStats.notify_one();
		}
		idx = (idx + 1) % QUEUE_SIZE;
		++count;
	}  // End of the infinite loop


	hardware->FX3producerOff();     //FX3 stop the producer
	mutexShowStats.notify_one(); //  allows exit of
	r2iqCntrl.TurnOff();

	DbgPrintf("AdcSamplesProc thread_exit\n");
	return;  // void *
}

void RadioHandlerClass::AbortXferLoop(int qidx)
{
	long len = transferSize;
	bool r = true;
	EndPt->Abort();
	for (int n = 0; n < QUEUE_SIZE; n++) {
		//   if (n< qidx+1)
		{
			EndPt->WaitForXfer(&inOvLap[n], TIMEOUT);
			EndPt->FinishDataXfer(buffers[n], len, &inOvLap[n], contexts[n]);
		}
		DbgPrintf("CloseHandle[%d]  %d\n", n, r);
	}
	pfnCallback(-1, extHw_Stop, 0.0F, 0); // Stop realtime see Failures
}


RadioHandlerClass::RadioHandlerClass() :
	dither(false),
	randout(false),
	biasT_HF(false),
	biasT_VHF(false),
	modeRF(NOMODE),
	firmware(0),
	fc(0.0f),
	hardware(new DummyRadio())
{
	buffers = new PUCHAR[QUEUE_SIZE];
	contexts = new PUCHAR[QUEUE_SIZE];
	obuffers = new float* [QUEUE_SIZE];

	// Allocate one big contitues buffer for all buffers in the input queues
	// Buffer is formated as the following:
	// [FFTN_R_ADC] + [FFTN_R_ADC] + [FFTN_R_ADC] + [FFTN_R_ADC] + [FFTN_R_ADC] + ....
	//                ^                             ^
	//                buffer[0]                     buffer[1]
	// use float to grantee the alignment
	PUCHAR buffer = (PUCHAR)(new float[(FFTN_R_ADC + transferSize * QUEUE_SIZE) / sizeof(float) ]);
	for (int i = 0; i < QUEUE_SIZE; i++) {
		buffers[i] = buffer + FFTN_R_ADC + transferSize * i;

		inOvLap[i].hEvent = CreateEvent(NULL, false, false, NULL);

		// Allocate the buffers for the output queue
		obuffers[i] = new float[transferSize / 2];
	}
}

RadioHandlerClass::~RadioHandlerClass()
{
	for (int n = 0; n < QUEUE_SIZE; n++) {
		CloseHandle(inOvLap[n].hEvent);
		delete[] obuffers[n];
	}

	delete[] (buffers[0] - FFTN_R_ADC);

	delete[] buffers;
	delete[] obuffers;
	delete[] contexts;
}

const char *RadioHandlerClass::getName()
{
	return hardware->getName();
}

bool RadioHandlerClass::Init(HMODULE hInst)
{
	int r = -1;
	auto Fx3 = new fx3class();
	if (!Fx3->Open(hInst))
	{
		return false;
	}
	UINT8 rdata[4];
	Fx3->GetHardwareInfo((UINT32*)rdata);

	radio = (RadioModel)rdata[0];
	firmware = (rdata[1] << 8) + rdata[2];

	delete hardware; // delete dummy instance
	switch (radio)
	{
	case HF103:
		hardware = new HF103Radio(Fx3);
		break;

	case BBRF103:
		hardware = new BBRF103Radio(Fx3);
		break;

	case RX888:
		hardware = new RX888Radio(Fx3);
		break;

	case RX888r2:
		hardware = new RX888R2Radio(Fx3);
		break;

	default:
		hardware = new DummyRadio();
		DbgPrintf("WARNING no SDR connected\n");
		break;
	}

	DbgPrintf("%s | firmware %x\n", hardware->getName(), firmware);

	return true;
}

bool RadioHandlerClass::Start(int srate_idx)
{
	Stop();
	double div = pow(2.0, srate_idx);
	auto samplerate = 1000000.0 * (div * 2);
	int decimate = (int)log2(RadioHandler.getSampleRate() / (2 * samplerate));

	DbgPrintf("RadioHandlerClass::Start\n");
	kbRead = 0; // zeros the kilobytes counter
	kSReadIF = 0;
	run = true;

	hardware->Initialize();

	show_stats_thread = new std::thread([this](void*) {
		this->CaculateStats();
	}, nullptr);
	// 0,1,2,3,4 => 32,16,8,4,2 MHz
	r2iqCntrl.Init( decimate, hardware->getGain(), buffers, obuffers);
	adc_samples_thread = new std::thread(
		[this](void* arg){
			this->AdcSamplesProcess();
		}, nullptr);
	r2iqCntrl.TurnOn(0);

	return true;
}

bool RadioHandlerClass::Stop()
{
	DbgPrintf("RadioHandlerClass::Stop %d\n", run);
	if (run)
	{
		run = false; // now waits for threads
		mutexShowStats.notify_all(); //  allows exit of
		show_stats_thread->join(); //first to be joined
		DbgPrintf("show_stats_thread join2\n");
		adc_samples_thread->join();
		DbgPrintf("adc_samples_thread join1\n");
	}
	return true;
}


bool RadioHandlerClass::Close()
{
	delete hardware;
	hardware = nullptr;

	return true;
}

// attenuator RF used in HF
int RadioHandlerClass::UpdateattRF(int att)
{
	if (hardware->UpdateattRF(att))
	{
		return att;
	}
	return 0;
}

// attenuator RF used in HF
int RadioHandlerClass::UpdateIFGain(int idx)
{
	if (hardware->UpdateGainIF(idx))
	{
		return idx;
	}

	return 0;
}

int RadioHandlerClass::GetRFAttSteps(const float **steps)
{
	return hardware->getRFSteps(steps);
}

int RadioHandlerClass::GetIFGainSteps(const float **steps)
{
	return hardware->getIFSteps(steps);
}

bool RadioHandlerClass::UpdatemodeRF(rf_mode mode)
{
	if (modeRF != mode){
		modeRF = mode;

		hardware->UpdatemodeRF(mode);
	}
	return true;
}

uint64_t RadioHandlerClass::TuneLO(uint64_t wishedFreq)
{
	uint64_t actLo;

	actLo = hardware->TuneLo(wishedFreq);

	// we need shift the samples
	float fc = r2iqCntrl.setFreqOffset((wishedFreq - actLo) / (getSampleRate() / 2.0f));

	if (this->fc != fc)
	{
		std::unique_lock<std::mutex> lk(fc_mutex);
		stateFineTune = shift_limited_unroll_C_sse_init(fc, 0.0F);
		this->fc = fc;
	}

	return wishedFreq;
}

bool RadioHandlerClass::UptDither(bool b)
{
	dither = b;
	if (dither)
		hardware->FX3SetGPIO(DITH);
	else
		hardware->FX3UnsetGPIO(DITH);
	return dither;
}

bool RadioHandlerClass::UptRand(bool b)
{
	randout = b;
	if (randout)
		hardware->FX3SetGPIO(RANDO);
	else
		hardware->FX3UnsetGPIO(RANDO);
	r2iqCntrl.updateRand(randout);
	return randout;
}

void RadioHandlerClass::CaculateStats()
{
	high_resolution_clock::time_point EndingTime;

	BytesXferred = 0;
	SamplesXIF = 0;
	while (run) {
		std::mutex k;
		std::unique_lock<std::mutex> lk(k);
		mutexShowStats.wait(lk);
		if (run == false)
			return;

		kbRead += double(BytesXferred) / 1000.;
		kSReadIF += double(SamplesXIF) / 1000.;

		EndingTime = high_resolution_clock::now();

		duration<double,std::ratio<1,1>> timeElapsed(EndingTime-StartingTime);

		double mBps = (double)kbRead / timeElapsed.count() / 1000 / sizeof(int16_t);
		double mSpsIF = (double)kSReadIF / timeElapsed.count() / 1000;

		BytesXferred = 0;
		SamplesXIF = 0;

		g_Bps = (float)mBps;
		g_SpsIF = (float)mSpsIF;

	}
	return;
}

void RadioHandlerClass::UpdBiasT_HF(bool flag) 
{
	biasT_HF = flag;

	if (biasT_HF)
		hardware->FX3SetGPIO(BIAS_HF);
	else
		hardware->FX3UnsetGPIO(BIAS_HF);
}

void RadioHandlerClass::UpdBiasT_VHF(bool flag)
{
	biasT_VHF = flag;
	if (biasT_VHF)
		hardware->FX3SetGPIO(BIAS_VHF);
	else
		hardware->FX3UnsetGPIO(BIAS_VHF);
}

uint32_t RadioHandlerClass::getSampleRate()
{
	return hardware->getSampleRate();
}