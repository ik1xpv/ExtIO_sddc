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

#define USB_READ_CONCURRENT  4

extern pfnExtIOCallback	pfnCallback;

RadioHandlerClass RadioHandler;
std::thread* adc_samples_thread;
std::thread* show_stats_thread;
// transfer variables
static int16_t*		buffers[QUEUE_SIZE];                       // export, main data buffers
static PUCHAR		contexts[USB_READ_CONCURRENT];
static OVERLAPPED	inOvLap[USB_READ_CONCURRENT];
static float*		obuffers[QUEUE_SIZE];

// transfer variables
float kbRead = 0;

unsigned long BytesXferred = 0;
unsigned long SamplesXIF = 0;
float kSReadIF = 0;
unsigned long Failures = 0;
float	g_Bps;
float	g_SpsIF;

void RadioHandlerClass::OnDataPacket(int idx)
{
	int rd = r2iqCntrl.getRatio();
	// submit result to SDR application before processing next packet
	++count;

	if (run &&						// app is running
		count > QUEUE_SIZE &&		// skip first batch
		(idx % rd == 0))		// if decimate, every *rd* packages
	{
		int oidx = idx / rd;
		std::unique_lock<std::mutex> lk(fc_mutex);
		if (fc != 0.0f)
		{
			shift_limited_unroll_C_sse_inp_c((complexf*)obuffers[oidx], EXT_BLOCKLEN, &stateFineTune);
		}

		pfnCallback(EXT_BLOCKLEN, 0, 0.0F, obuffers[oidx]);
		SamplesXIF += EXT_BLOCKLEN;
	}

	r2iqCntrl.DataReady();   // inform r2iq buffer ready
}

void RadioHandlerClass::AdcSamplesProcess()
{
	DbgPrintf("AdcSamplesProc thread runs\n");
	int idx;            // queue index              // export

	Failures = 0;

	for (int i = 0; i < USB_READ_CONCURRENT; i++) {
		inOvLap[i].hEvent = CreateEvent(NULL, false, false, NULL);
	}

	// Queue-up the first batch of transfer requests
	for (int n = 0; n < USB_READ_CONCURRENT; n++) {
		contexts[n] = EndPt->BeginDataXfer((PUCHAR)buffers[n], transferSize, &inOvLap[n]);
		if (EndPt->NtStatus || EndPt->UsbdStatus) {// BeginDataXfer failed
			DbgPrintf((char*)"Xfer request rejected. 1 STATUS = %ld %ld\n", EndPt->NtStatus, EndPt->UsbdStatus);
			return;
		}
	}
	idx = 0;    // buffer cycle index

	// The infinite xfer loop.
	while (run) {
		LONG rLen = transferSize;	// Reset this each time through because
		// FinishDataXfer may modify it
		if (!EndPt->WaitForXfer(&inOvLap[idx % USB_READ_CONCURRENT], BLOCK_TIMEOUT)) { // block on transfer
			EndPt->Abort(); // abort if timeout
			DbgPrintf("BUG1001 Count %d idx %d", count, idx);
			// Re-submit this queue element to keep the queue full
			contexts[idx % USB_READ_CONCURRENT] = EndPt->BeginDataXfer((PUCHAR)buffers[idx], transferSize, &inOvLap[idx % USB_READ_CONCURRENT]);
			if (EndPt->NtStatus || EndPt->UsbdStatus) { // BeginDataXfer failed
				DbgPrintf("Xfer request rejected. NTSTATUS = 0x%08X\n", (UINT)EndPt->NtStatus);
				AbortXferLoop(idx);
				break;
			}

		}

		if (EndPt->FinishDataXfer((PUCHAR)buffers[idx], rLen, &inOvLap[idx % USB_READ_CONCURRENT], contexts[idx % USB_READ_CONCURRENT])) {
			BytesXferred += rLen;
			if (rLen < transferSize) DbgPrintf("rLen = %ld\n", rLen);

			OnDataPacket(idx);

#ifdef _DEBUG		//PScope buffer screenshot
			if (saveADCsamplesflag == true)
			{
				saveADCsamplesflag = false; // do it once
				short* pi = (short*)buffers[idx];
				unsigned int numsamples = transferSize / sizeof(short);
				float samplerate  = (float) getSampleRate();
				PScopeShot("ADCrealsamples.adc", "ExtIO_sddc.dll",
					"ADCrealsamples.adc input real ADC 16 bit samples",
					pi, samplerate, numsamples);
			}
#endif
		}

		// Re-submit this queue element to keep the queue full
		contexts[idx % USB_READ_CONCURRENT] = EndPt->BeginDataXfer((PUCHAR)buffers[idx], transferSize, &inOvLap[idx % USB_READ_CONCURRENT]);
		if (EndPt->NtStatus || EndPt->UsbdStatus) { // BeginDataXfer failed
			DbgPrintf("Xfer request rejected.2 NTSTATUS = 0x%08X\n", (UINT)EndPt->NtStatus);
			AbortXferLoop(idx);
			break;
		}

		idx = (idx + 1) % QUEUE_SIZE;
	}  // End of the infinite loop


	DbgPrintf("AdcSamplesProc thread_exit\n");
	return;  // void *
}

void RadioHandlerClass::AbortXferLoop(int qidx)
{
	long len = transferSize;
	bool r = true;
	EndPt->Abort();
	for (int n = 0; n < USB_READ_CONCURRENT; n++) {
		//   if (n< qidx+1)
		{
			EndPt->WaitForXfer(&inOvLap[n], TIMEOUT);
			EndPt->FinishDataXfer((PUCHAR)buffers[n], len, &inOvLap[n], contexts[n]);
		}
		DbgPrintf("CloseHandle[%d]  %d\n", n, r);
	}
	pfnCallback(-1, extHw_Stop, 0.0F, 0); // Stop realtime see Failures
}


RadioHandlerClass::RadioHandlerClass() :
	run(false),
	dither(false),
	randout(false),
	biasT_HF(false),
	biasT_VHF(false),
	modeRF(NOMODE),
	firmware(0),
	fc(0.0f),
	hardware(new DummyRadio())
{
	for (int i = 0; i < QUEUE_SIZE; i++) {
		buffers[i] = new int16_t[transferSize / sizeof(int16_t)];

		// Allocate the buffers for the output queue
		obuffers[i] = new float[transferSize / 2];
	}
}

RadioHandlerClass::~RadioHandlerClass()
{
	for (int n = 0; n < QUEUE_SIZE; n++) {
		delete[] obuffers[n];
		delete[] buffers[n];
	}
}

const char *RadioHandlerClass::getName()
{
	return hardware->getName();
}

bool RadioHandlerClass::Init(fx3class* Fx3)
{
	int r = -1;
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

	case RX999:
		hardware = new RX999Radio(Fx3);
		break;

	default:
		hardware = new DummyRadio();
		DbgPrintf("WARNING no SDR connected\n");
		break;
	}

	DbgPrintf("%s | firmware %x\n", hardware->getName(), firmware);
	r2iqCntrl.Init(hardware->getGain(), buffers, obuffers);

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
	count = 0;

	hardware->Initialize();
	hardware->FX3producerOn();  // FX3 start the producer

	// 0,1,2,3,4 => 32,16,8,4,2 MHz
	r2iqCntrl.setDecimate(decimate);
	r2iqCntrl.TurnOn();
	adc_samples_thread = new std::thread(
		[this](void* arg){
			this->AdcSamplesProcess();
		}, nullptr);

	show_stats_thread = new std::thread([this](void*) {
		this->CaculateStats();
	}, nullptr);

	return true;
}

bool RadioHandlerClass::Stop()
{
	std::unique_lock<std::mutex> lk(stop_mutex);
	DbgPrintf("RadioHandlerClass::Stop %d\n", run);
	if (run)
	{
		r2iqCntrl.TurnOff();

		run = false; // now waits for threads
		show_stats_thread->join(); //first to be joined
		delete show_stats_thread;
		show_stats_thread = nullptr;
		DbgPrintf("show_stats_thread join2\n");

		adc_samples_thread->join();
		delete adc_samples_thread;
		adc_samples_thread = nullptr;
		DbgPrintf("adc_samples_thread join1\n");

		hardware->FX3producerOff();     //FX3 stop the producer
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
		DbgPrintf("Switch to mode: %d\n", modeRF);

		hardware->UpdatemodeRF(mode);
	}
	return true;
}

uint64_t RadioHandlerClass::TuneLO(uint64_t wishedFreq)
{
	uint64_t actLo;

	actLo = hardware->TuneLo(wishedFreq);

	// we need shift the samples
	DbgPrintf("Offset freq %lld\n", (wishedFreq - actLo));
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
	auto StartingTime = high_resolution_clock::now();

	while (run) {
		kbRead += float(BytesXferred) / 1000.0f;
		kSReadIF += float(SamplesXIF) / 1000.0f;

		EndingTime = high_resolution_clock::now();

		duration<float,std::ratio<1,1>> timeElapsed(EndingTime-StartingTime);

		float mBps = (float)kbRead / timeElapsed.count() / 1000 / sizeof(int16_t);
		float mSpsIF = (float)kSReadIF / timeElapsed.count() / 1000;

		BytesXferred = 0;
		SamplesXIF = 0;

		g_Bps = (float)mBps;
		g_SpsIF = (float)mSpsIF;

		std::this_thread::sleep_for(0.2s);
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