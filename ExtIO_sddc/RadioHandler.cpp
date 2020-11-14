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
PUCHAR* buffers;                       // export, main data buffers
PUCHAR* contexts;
OVERLAPPED	inOvLap[QUEUE_SIZE];
float** obuffers;

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


extern unsigned	gExtSampleRate;
extern int  giExtSrateIdx;

void* AdcSamplesProc(void*);
void AbortXferLoop(int qidx);
void* tShowStats(void* args);

//R820T2 
#define GAIN_STEPS (29)  // aprox 20*2 = 40
static const UINT8 vga_gains[GAIN_STEPS] = { 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8 };
static const UINT8 mixer_gains[GAIN_STEPS] = { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,10,10,11,11,12,12,13,13,14 };
static const UINT8 lna_gains[GAIN_STEPS] = { 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,10,10,11,11,12,12,13,13,14,15 };
// total gain dB x 10
static const UINT16 total_gain[GAIN_STEPS] = { 0, 9, 14, 27, 37, 77, 87, 125, 144, 157,
							166, 197, 207, 229, 254, 280, 297, 328,
							338, 364, 372, 386, 402, 421, 434, 439,
							445, 480, 496 };






void* AdcSamplesProc(void*)
{
	DbgPrintf("AdcSamplesProc thread runs\n");
	int callShowStatsRate = 32 * QUEUE_SIZE;
	unsigned skip = QUEUE_SIZE;
	Failures = 0;
	int odx = 0;
	unsigned int kidx = 0, strd = 0;
	int rd = r2iqCntrl.getRatio();

	r2iqTurnOn(0);  
	
	// Queue-up the first batch of transfer requests
	for (int n = 0; n < QUEUE_SIZE; n++) {
		contexts[n] = EndPt->BeginDataXfer(buffers[n], transferSize, &inOvLap[n]);
		if (EndPt->NtStatus || EndPt->UsbdStatus) {// BeginDataXfer failed
			DbgPrintf((char*)"Xfer request rejected. 1 STATUS = %ld %ld\n", EndPt->NtStatus, EndPt->UsbdStatus);
			return NULL;
		}
	}
	RadioHandler.FX3producerOn();  // FX3 start the producer
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

		r2iqDataReady();   // inform r2iq buffer ready

#ifndef NDEBUG		//PScope buffer screenshot
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


	RadioHandler.FX3producerOff();     //FX3 stop the producer
	Sleep(10);
	mutexShowStats.notify_all(); //  allows exit of
	r2iqTurnOff();

	DbgPrintf("AdcSamplesProc thread_exit\n");
	return 0;  // void *
}

void AbortXferLoop(int qidx)
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
		r = CloseHandle(inOvLap[n].hEvent);
		DbgPrintf("CloseHandle[%d]  %d\n", n, r);
	}
	//Deallocate all the buffers for the input queues
	delete[] buffers;
	delete[] contexts;

}


RadioHandlerClass::RadioHandlerClass() :
	IsOn(false),
	dither(false),
	randout(false),
	biasT_HF(false),
	biasT_VHF(false),
	matt(-1),  // force update
	modeRF(NOMODE),
	attRF(0),
	firmware(0)
{
	bgpio = 0x0;
}

bool RadioHandlerClass::Init(HMODULE hInst)
{
	IsOn = false;
	int r = -1;
	Fx3 = new fx3class();
	if (!Fx3->Open(hInst))
	{
		return IsOn;
	}
	UINT8 rdata[64];
	radiotype oldradio = radio;
	Fx3->Control(TESTFX3, &rdata[0]);
	switch (rdata[0])
	{
	case HF103:
			radio = HF103;
			IsOn = true;
			firmware = (rdata[1] << 8) + rdata[2];
			DbgPrintf("HF103 | firmware %x\n", firmware);
		break;

	case BBRF103:
			radio = BBRF103;
			IsOn = true;
			firmware = (rdata[1] << 8) + rdata[2];
			DbgPrintf("BBRF103 | firmware %x\n", firmware);
			InitSi5351a(ADC_FREQ, R820T_FREQ);
			R820T2stdby();
			if (oldradio == HF103)
			{
				RadioHandler.UpdateattRF(0);
				RadioHandler.UpdateattRF(giAttIdx);
				EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_ATT);
			}


		break;

	case RX888:
			radio = RX888;
			IsOn = true;
			firmware = (rdata[1] << 8) + rdata[2];
			DbgPrintf("RX888 | firmware %x\n", firmware);
			InitSi5351a(ADC_FREQ, R820T_FREQ);
			R820T2stdby();
			if (oldradio == HF103)
			{
				RadioHandler.UpdateattRF(0);
				RadioHandler.UpdateattRF(giAttIdx);
				EXTIO_STATUS_CHANGE(pfnCallback, extHw_Changed_ATT);
			}
		break;

	default:
			DbgPrintf("WARNING no SDR connected\n");
		break;
	}
	if (!Fx3->Control(GPIOFX3, (UINT8*)&bgpio)) IsOn = false;
	if (oldradio != radio)
	{
		char buffer[128];
		sprintf(buffer, "%s\tnow connected\r\n%s\tprevious radio", radioname[radio], radioname[oldradio]);
		MessageBox(NULL, buffer, "WARNING settings changed", MB_OK | MB_ICONINFORMATION);
	}
	return IsOn;
}
bool RadioHandlerClass::InitSi5351a(UINT32 freqa, UINT32 freqb)
{
	// freqa = nominal ADC sampling frequecy
	// freqb = nominal R820T2 reference freuency , 0 = disable
	mfreqa = freqa;
	mfreqb = freqb;
	return UpdSi5351a();
}
bool RadioHandlerClass::UpdSi5351a()
{
	// mfreqa = nominal ADC sampling frequecy
	// mfreqb = nominal R820T2 reference freuency 

	UINT32 data[2];
	data[0] = (UINT32)((double) mfreqa * (1.0 + (gdFreqCorr_ppm * 0.000001)));
	data[1] = (UINT32)((double) mfreqb * (1.0 + (gdFreqCorr_ppm * 0.000001)));

	if (Fx3 != nullptr) {
		if (!Fx3->Control(SI5351A, (UINT8*)&data[0])) IsOn = false;
		return true;
	}
	else
		return false;
}

bool RadioHandlerClass::InitBuffers() {

	buffers = new PUCHAR[QUEUE_SIZE];
	contexts = new PUCHAR[QUEUE_SIZE];
	obuffers = new float* [QUEUE_OUT];

	if (EndPt) { // real data
		long pktSize = EndPt->MaxPktSize;
		EndPt->SetXferSize(transferSize);
		long ppx = transferSize / pktSize;
		DbgPrintf("buffer transferSize = %d. packet size = %ld. packets per transfer = %ld\n"
			, transferSize, pktSize, ppx);
	}
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
	for (int i = 0; i < QUEUE_OUT; i++) {
		obuffers[i] = new float[transferSize / 2];
	}
	// UpdatemodeRF(modeRF); //  Update
	return IsOn;
}

bool RadioHandlerClass::Start()
{
	Stop();
	if (!IsOn) return IsOn;
	DbgPrintf("RadioHandlerClass::Start\n");
	kbRead = 0; // zeros the kilobytes counter
	kSReadIF = 0;
	run = true;
	int t = 0;
	show_stats_thread = new std::thread(tShowStats, (void*)t);
	initR2iq( 4 - giExtSrateIdx ); // 0,1,2,3,4 => 32,16,8,4,2 MHz
	adc_samples_thread = new std::thread(AdcSamplesProc, (void*)t);
	return IsOn;
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
	if (IsOn)
	{
		IsOn = false;
		Fx3->Control(RESETFX3);       //FX3    reset the fx3 firmware
		delete Fx3;
	}
	return IsOn;
}

// attenuator RF used in HF
int RadioHandlerClass::UpdateattRF(int att)
{
	rf_mode rfm = RadioHandler.GetmodeRF();
	if (matt != att)	// if value change
	{
		if (rfm != VHFMODE) // HFMODE, VLFMODE select att HF
		{ 
			switch (radio)
			{

			case BBRF103:
			case RX888:
				if (att > 2) att = 2;
				if (att < 0) att = 0;
				giAttIdx = att;
				matt = att;
				attRF = 20 - att * 10;
				{
					UINT16 tmp = bgpio & (0xFFFF ^ (ATT_SEL0 || ATT_SEL1));
					switch (attRF)
					{
					case 10: //11
						tmp = tmp | ATT_SEL0 | ATT_SEL1;
						break;
					case 20: //01
						tmp = (tmp | ATT_SEL0) & (0xFFFF ^ ATT_SEL1);
						break;
					case 0:   //10
					default:
						tmp = (tmp | ATT_SEL1) & (0xFFFF ^ ATT_SEL0);
						break;
					}
					bgpio = tmp;
					if (Fx3 != nullptr) {
						if (!Fx3->Control(GPIOFX3, (UINT8*)&bgpio)) IsOn = false;
					}
				}
				break;

			case HF103:
				if (att > 31) att = 31;
				if (att < 0) att = 0;
				giAttIdx = att;
				matt = att;
				attRF = 31 - att;
				UINT8 d = attRF << 1; // bit0 =0
				if (IsOn)
				{
					DbgPrintf("UpdateattRF  -%d \n", attRF);
					if (!Fx3->Control(DAT31FX3, (PUINT8)&d)) IsOn = false;
				}
				break;
			}
		}
	}
	if ((rfm == VHFMODE) && (radio != HF103))
	{
		if (radio == HF103) return -1;
		bgpio &= (0xFFFF ^ (ATT_SEL0 | ATT_SEL1)); // R820T2 input
		if (Fx3 != nullptr) {
			if (!Fx3->Control(GPIOFX3, (UINT8*)&bgpio)) IsOn = false;
		}
	    R820T2SetAttenuator(att); // R820 att

	}

	matt = att;
	return matt;
}


bool RadioHandlerClass::UpdatemodeRF(rf_mode mode)
{
	if (modeRF != mode){
		modeRF = mode;

		if (pfnCallback)
		{
			pfnCallback(-1, extHw_Changed_RF_IF, 0.0F, 0);
			pfnCallback(-1, extHw_Changed_AGCS, 0.0F, 0);
			pfnCallback(-1, extHw_Changed_AGC, 0.0F, 0);
			pfnCallback(-1, extHw_Changed_LO, 0.0F, 0);
		}
		
	}
	return true;
}

bool RadioHandlerClass::UptDither(bool b)
{
	if (IsOn)
	{
		dither = b;
		if (dither == true)
			bgpio = bgpio | DITH;
		else
			bgpio = bgpio & (0xffff ^ DITH);
		if (Fx3 != nullptr) {
			if (!Fx3->Control(GPIOFX3, (UINT8*)&bgpio)) IsOn = false;
		}
	}
	return dither;
}

bool RadioHandlerClass::UptRand(bool b)
{
	if (IsOn)
	{
		randout = b;
		if (randout == true)
			bgpio = bgpio | RANDO;
		else
			bgpio = bgpio & (0xffff ^ RANDO);
		if (Fx3 != nullptr) {
			if (!Fx3->Control(GPIOFX3, (UINT8*)&bgpio)) IsOn = false;
		}
	}
	r2iqCntrl.randADC = randout;
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

bool RadioHandlerClass::SendI2cbyte(UINT8 i2caddr, UINT8 regaddr, UINT8 data)
{
	return Fx3->SendI2cbytes(i2caddr, regaddr, &data, 1);
}


bool RadioHandlerClass::SendI2cbytes(UINT8 i2caddr, UINT8 regaddr, UINT8* pdata, UINT8 len)
{
	if (IsOn)
	{
		return  Fx3->SendI2cbytes(i2caddr, regaddr, pdata, len);
	}
	return false;
}

void RadioHandlerClass::ReadI2cbytes(UINT8 i2caddr, UINT8 regaddr, UINT8* pdata, UINT8 len)
{
	if (IsOn)
		Fx3->ReadI2cbytes(i2caddr, regaddr, pdata, len);
}

void RadioHandlerClass::UpdBiasT_HF(bool flag) 
{
	biasT_HF = flag; 
	if (biasT_HF)
		bgpio |= BIAS_HF;
	else
		bgpio &= (0xffff ^ BIAS_HF);
	if (Fx3 != nullptr)	Fx3->Control(GPIOFX3, (UINT8*)&bgpio);
}
void RadioHandlerClass::UpdBiasT_VHF(bool flag)
{
	biasT_VHF = flag;
	if (biasT_VHF)
		bgpio |= BIAS_VHF;
	else
		bgpio &= (0xffff ^ BIAS_VHF);
	if (Fx3 != nullptr)	Fx3->Control(GPIOFX3, (UINT8*)&bgpio);
}

int RadioHandlerClass::R820T2init()
{
	INT32 retcode = 0;
	if (!Fx3->Control(R820T2INIT, (UINT8*) & retcode))  IsOn = false;
	DbgPrintf( "R820T2 init ret 0x%04X\r\n", retcode );
	return (int) retcode;
}
void RadioHandlerClass::R820T2stdby()
{
	UINT8 dummy = 0;
	if (!Fx3->Control(R820T2STDBY, (UINT8*)&dummy))  IsOn = false;
}


int RadioHandlerClass::R820T2Tune(UINT64 freq)
{
	UINT32 f = (UINT32) freq;
	DbgPrintf("R820T2Tune %I64d \n", freq);
	if (Fx3 != nullptr) {
		if (!Fx3->Control(R820T2TUNE, (UINT8*)&f)){
			IsOn = false;
			return -1;
		}
	}
	return 0;
}


float RadioHandlerClass::R820T2getAttenuator(UINT8 idx)
{
	// locally computed
	float gdB = 0.0;
	if (idx < GAIN_STEPS)
		gdB = (float)( total_gain[idx] + 0.5) / 10.0F;
	return gdB;
}

int RadioHandlerClass::R820T2SetAttenuator(UINT8 idx)
{
	UINT8 index;
	UINT8 check= 0xff;
	index  = idx;
	if (idx < GAIN_STEPS)
	{
		if (Fx3 != nullptr) {
			if (!Fx3->Control(R820T2SETATT, (UINT8*) &index))
				IsOn = false;
			else
			{
				Fx3->Control(R820T2GETATT, (UINT8*)&check);
				if (check == index)
					return index;
				else
					DbgPrintf("WARNING R820T2SetAttenuator failed index %x \n", index);
			}
		}

	}
	return -1;
}