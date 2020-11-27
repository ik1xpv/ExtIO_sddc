#include "license.txt" 

#include <stdio.h>
#include "mytypes.h"
#include "RadioHandler.h"
#include "config.h"
#include "r2iq.h"
#include "config.h"
#include "PScope_uti.h"

#define BLOCK_TIMEOUT (80) // block 65.536 ms timeout is 80

extern pfnExtIOCallback	pfnCallback;

RadioHandlerClass RadioHandler;
std::thread* adc_samples_thread;
std::thread* show_stats_thread;
// transfer variables
static PUCHAR* buffers;                       // export, main data buffers
static PUCHAR* contexts;
static OVERLAPPED	inOvLap[QUEUE_SIZE];
static float** obuffers;

int idx;            // queue index              // export
unsigned long IDX;  // absolute index

// transfer variables
double kbRead = 0;
LARGE_INTEGER StartingTime;
LARGE_INTEGER Time1, Time2;
unsigned long BytesXferred = 0;
unsigned long SamplesXIF = 0;
double kSReadIF = 0;
unsigned long Failures = 0;
float	g_Bps;
float	g_SpsIF;

std::condition_variable mutexShowStats;     // unlock to show stats

void* AdcSamplesProc(void*);
void AbortXferLoop(int qidx);
void* tShowStats(void* args);

void RadioHandlerClass::AdcSamplesProcess()
{
	DbgPrintf("AdcSamplesProc thread runs\n");
	int callShowStatsRate = 32 * QUEUE_SIZE;
	unsigned skip = QUEUE_SIZE;
	Failures = 0;
	int odx = 0;
	unsigned int kidx = 0, strd = 0;
	int rd = r2iqCntrl.getRatio();
	
	if (EndPt) { // real data
		long pktSize = EndPt->MaxPktSize;
		EndPt->SetXferSize(transferSize);
		long ppx = transferSize / pktSize;
		DbgPrintf("buffer transferSize = %d. packet size = %ld. packets per transfer = %ld\n"
			, transferSize, pktSize, ppx);
	}

	// Queue-up the first batch of transfer requests
	for (int n = 0; n < QUEUE_SIZE; n++) {
		contexts[n] = EndPt->BeginDataXfer(buffers[n], transferSize, &inOvLap[n]);
		if (EndPt->NtStatus || EndPt->UsbdStatus) {// BeginDataXfer failed
			DbgPrintf((char*)"Xfer request rejected. 1 STATUS = %ld %ld\n", EndPt->NtStatus, EndPt->UsbdStatus);
			return;
		}
	}
	hardware->FX3producerOn();  // FX3 start the producer
	run = true;		
	idx = 0;    // buffer cycle index
	IDX = 1;    // absolute index
	QueryPerformanceCounter(&StartingTime);  // set the start time
	// The infinite xfer loop.
	while (run) {
		LONG rLen = transferSize;	// Reset this each time through because
		// FinishDataXfer may modify it
		if (!EndPt->WaitForXfer(&inOvLap[idx], BLOCK_TIMEOUT)) { // block on transfer
			EndPt->Abort(); // abort if timeout
			DbgPrintf("BUG1001 IDX %d idx %d", IDX, idx);
			// Re-submit this queue element to keep the queue full
			contexts[idx] = EndPt->BeginDataXfer(buffers[idx], transferSize, &inOvLap[idx]);
			if (EndPt->NtStatus || EndPt->UsbdStatus) { // BeginDataXfer failed
				DbgPrintf("Xfer request rejected. NTSTATUS = 0x%08X\n", (UINT)EndPt->NtStatus);
				AbortXferLoop(idx);
				break;
			}

		}
		if (EndPt->Attributes == 2) { // BULK Endpoint
			if (EndPt->FinishDataXfer(buffers[idx], rLen, &inOvLap[idx], contexts[idx])) {
				BytesXferred += rLen;
				if (rLen < transferSize) printf("rLen = %ld\n", rLen);
			}
			else
			{
				if (IDX > 64) { // avoid first 64 block
					Failures++;
					pfnCallback(-1, extHw_Stop, 0.0F, 0); // Stop realtime see Failures
				}
			}

		}

		if (skip > 0)
			skip--;
		else if (pfnCallback && run)
		{
			if (rd == 1)
			{
				pfnCallback(EXT_BLOCKLEN, 0, 0.0F, obuffers[idx]);
				SamplesXIF += EXT_BLOCKLEN;
			}
			else
			{
				odx = (idx + 1) / rd;
				if ((odx * rd) == (idx + 1))
				{
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
			float samplerate  = (float) adcfixedfreq;
			PScopeShot("ADCrealsamples.adc", "ExtIO_sddc.dll",
				"ADCrealsamples.adc input real ADC 16 bit samples",
				pi, samplerate, numsamples);
		}
#endif
		// Re-submit this queue element to keep the queue full
		contexts[idx] = EndPt->BeginDataXfer(buffers[idx], transferSize, &inOvLap[idx]);
		if (EndPt->NtStatus || EndPt->UsbdStatus) { // BeginDataXfer failed
			DbgPrintf("Xfer request rejected.2 NTSTATUS = 0x%08X\n", (UINT)EndPt->NtStatus);
			AbortXferLoop(idx);
			break;
		}

		if ((IDX % callShowStatsRate) == 0) { //Only update the display at the call rate
			mutexShowStats.notify_all();
		}
		++idx;
		++IDX;
		idx = idx % QUEUE_SIZE;
	}  // End of the infinite loop


	hardware->FX3producerOff();     //FX3 stop the producer
	Sleep(10);
	mutexShowStats.notify_all(); //  allows exit of
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
}


RadioHandlerClass::RadioHandlerClass() :
	dither(false),
	randout(false),
	biasT_HF(false),
	biasT_VHF(false),
	modeRF(NOMODE),
	firmware(0),
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
	}
	// Allocate the buffers for the output queue
	for (int i = 0; i < QUEUE_SIZE; i++) {
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

bool RadioHandlerClass::Init(HMODULE hInst)
{
	int r = -1;
	auto Fx3 = new fx3class();
	if (!Fx3->Open(hInst))
	{
		return false;
	}
	UINT8 rdata[64];
	RadioModel oldradio = radio;
	Fx3->Control(TESTFX3, &rdata[0]);

	radio = HF103;
	firmware = (rdata[1] << 8) + rdata[2];

	delete hardware; // delete dummy instance
	switch (rdata[0])
	{
	case HF103:
		hardware = new HF103Radio(Fx3);	
		radio = HF103;
		break;

	case BBRF103:
		hardware = new BBRF103Radio(Fx3);
		radio = BBRF103;
		break;

	case RX888:
		hardware = new RX888Radio(Fx3);
		radio = RX888;
		break;

	default:
		hardware = new DummyRadio();
		radio = NORADIO;
		DbgPrintf("WARNING no SDR connected\n");
		break;
	}

	DbgPrintf("%s | firmware %x\n", hardware->getName(), firmware);
	hardware->Initialize();

#ifdef _DEBUG
	if (oldradio != radio)
	{
		char buffer[128];
		sprintf(buffer, "%s\tnow connected\r\n%s\tprevious radio", radioname[radio], radioname[oldradio]);
		MessageBox(NULL, buffer, "WARNING settings changed", MB_OK | MB_ICONINFORMATION);
	}
#endif

	return true;
}

bool RadioHandlerClass::Start(int srate_idx)
{
	Stop();
	DbgPrintf("RadioHandlerClass::Start\n");
	kbRead = 0; // zeros the kilobytes counter
	kSReadIF = 0;
	run = true;
	int t = 0;
	show_stats_thread = new std::thread(tShowStats, (void*)t);
	// 0,1,2,3,4 => 32,16,8,4,2 MHz
	r2iqCntrl.Init( 4 - srate_idx, hardware->getGain(), buffers, obuffers);
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
		
		IDX = 0;  // patch for BUG1001
		// it restart the IDX counter
		// failures count is allowed in first first 64 block
		// see
		// line 110		if (IDX > 64) { // avoid first 64 block
		// line 64  DbgPrintf("BUG1001 IDX %d idx %d", IDX, idx);
	}
	return true;
}

uint64_t RadioHandlerClass::TuneLO(uint64_t freq)
{
	return hardware->TuneLo(freq);
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

void* tShowStats(void* args)
{
	double count2sec;
	LARGE_INTEGER EndingTime;
	//   double timeElapsed = 0;

	LARGE_INTEGER Frequency;
	QueryPerformanceFrequency(&Frequency);
	count2sec = 1.0 / Frequency.QuadPart;

	BytesXferred = 0;
	SamplesXIF = 0;
	UINT8 cnt = 0;
	while (run) {
		std::mutex k;
		std::unique_lock<std::mutex> lk(k);
		mutexShowStats.wait(lk);
		if (run == false)
			return NULL;

		double timeStart = double(StartingTime.QuadPart) * count2sec;
		QueryPerformanceCounter(&EndingTime);
		double timeElapsed = double(EndingTime.QuadPart) * count2sec - timeStart;
		kbRead += double(BytesXferred) / 1000.;
		double mBps = (double)kbRead / timeElapsed / 1000;
		kSReadIF += double(SamplesXIF) / 1000.;
		double mSpsIF = (double)kSReadIF / timeElapsed / 1000;


		BytesXferred = 0;
		SamplesXIF = 0;

		g_Bps = (float)mBps;
		g_SpsIF = (float)mSpsIF;

	}
	return 0;
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

