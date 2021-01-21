#include "license.txt" 

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "pf_mixer.h"
#include "RadioHandler.h"
#include "config.h"
#include "fft_mt_r2iq.h"
#include "config.h"
#include "PScope_uti.h"

#include <chrono>

using namespace std::chrono;

#define USB_READ_CONCURRENT  4

// transfer variables

unsigned long Failures = 0;

void RadioHandlerClass::OnDataPacket()
{

	auto len = outputbuffer.getBlockSize() / 2 / sizeof(float);

	while(run)
	{
		auto buf = outputbuffer.getReadPtr();

		if (!run)
			break;

		if (fc != 0.0f)
		{
			std::unique_lock<std::mutex> lk(fc_mutex);
			shift_limited_unroll_C_sse_inp_c((complexf*)buf, len, stateFineTune);
		}

    	Callback(buf, len);

		outputbuffer.ReadDone();

		SamplesXIF += len;
	}
}

void RadioHandlerClass::AdcSamplesProcess()
{
	DbgPrintf("AdcSamplesProc thread runs\n");
	int read_idx;
	void*		contexts[USB_READ_CONCURRENT];

	Failures = 0;

	memset(contexts, 0, sizeof(contexts));

	// Queue-up the first batch of transfer requests
	for (int n = 0; n < USB_READ_CONCURRENT; n++) {
		auto ptr = inputbuffer.peekWritePtr(n);
        if (!fx3->BeginDataXfer((uint8_t*)ptr, transferSize, &contexts[n])) {
			DbgPrintf("Xfer request rejected.\n");
			return;
		}
	}

	read_idx = 0;	// context cycle index

	// The infinite xfer loop.
	while (run) {
		if (!fx3->FinishDataXfer(&contexts[read_idx])) {
		}

		inputbuffer.WriteDone();
		BytesXferred += transferSize;

#ifdef _DEBUG		//PScope buffer screenshot
		if (saveADCsamplesflag == true)
		{
			saveADCsamplesflag = false; // do it once
			auto pi = inputbuffer.peekWritePtr(-1);
			unsigned int numsamples = transferSize / sizeof(int16_t);
			float samplerate  = (float) getSampleRate();
			PScopeShot("ADCrealsamples.adc", "ExtIO_sddc.dll",
				"ADCrealsamples.adc input real ADC 16 bit samples",
				pi, samplerate, numsamples);
		}
#endif

		// Re-submit this queue element to keep the queue full
        auto ptr = inputbuffer.peekWritePtr(USB_READ_CONCURRENT - 1);
        if (!fx3->BeginDataXfer((uint8_t*)ptr, transferSize, &contexts[read_idx])) { // BeginDataXfer failed
			DbgPrintf("Xfer request rejected.\n");
			Failures++;
			break;
		}

		read_idx = (read_idx + 1) % USB_READ_CONCURRENT;
	}  // End of the infinite loop

	for (int n = 0; n < USB_READ_CONCURRENT; n++) {
		fx3->CleanupDataXfer(&contexts[n]);
	}

	if (Failures)
	{
		Callback(nullptr, 0);
	}

	DbgPrintf("AdcSamplesProc thread_exit\n");
	return;  // void *
}

RadioHandlerClass::RadioHandlerClass() :
	run(false),
	pga(false),
	dither(false),
	randout(false),
	biasT_HF(false),
	biasT_VHF(false),
	firmware(0),
	modeRF(NOMODE),
	adcrate(DEFAULT_ADC_FREQ),
	fc(0.0f),
	hardware(new DummyRadio(nullptr))
{
	inputbuffer.setBlockSize(transferSamples);

	stateFineTune = new shift_limited_unroll_C_sse_data_t();
}

RadioHandlerClass::~RadioHandlerClass()
{
	delete stateFineTune;
}

const char *RadioHandlerClass::getName()
{
	return hardware->getName();
}

bool RadioHandlerClass::Init(fx3class* Fx3, void (*callback)(const float*, uint32_t), r2iqControlClass *r2iqCntrl)
{
	uint8_t rdata[4];
	this->fx3 = Fx3;
	this->Callback = callback;

	if (r2iqCntrl == nullptr)
		r2iqCntrl = new fft_mt_r2iq();

	Fx3->GetHardwareInfo((uint32_t*)rdata);

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
		hardware = new DummyRadio(Fx3);
		DbgPrintf("WARNING no SDR connected\n");
		break;
	}

	hardware->Initialize(adcrate);
	DbgPrintf("%s | firmware %x\n", hardware->getName(), firmware);
	this->r2iqCntrl = r2iqCntrl;
	r2iqCntrl->Init(hardware->getGain(), &inputbuffer, &outputbuffer);

	return true;
}

bool RadioHandlerClass::Start(int decimate)
{
	Stop();

	DbgPrintf("RadioHandlerClass::Start\n");
	run = true;
	count = 0;

	hardware->FX3producerOn();  // FX3 start the producer

	outputbuffer.setBlockSize(EXT_BLOCKLEN * 2 * sizeof(float));

	// 0,1,2,3,4 => 32,16,8,4,2 MHz
	r2iqCntrl->setDecimate(decimate);
	r2iqCntrl->TurnOn();
	adc_samples_thread = std::thread(
		[this](void* arg){
			this->AdcSamplesProcess();
		}, nullptr);

	submit_thread = std::thread(
		[this]() {
			this->OnDataPacket();
		});

	show_stats_thread = std::thread([this](void*) {
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
		run = false; // now waits for threads

		r2iqCntrl->TurnOff();

		show_stats_thread.join(); //first to be joined
		DbgPrintf("show_stats_thread join2\n");

		adc_samples_thread.join();
		DbgPrintf("adc_samples_thread join1\n");

		submit_thread.join();
		DbgPrintf("submit_thread join1\n");

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

bool RadioHandlerClass::UpdateSampleRate(uint32_t samplefreq)
{
	hardware->Initialize(samplefreq);

	this->adcrate = samplefreq;

	return 0;
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

		if (mode == VHFMODE)
			r2iqCntrl->setSideband(true);
		else
			r2iqCntrl->setSideband(false);
	}
	return true;
}

uint64_t RadioHandlerClass::TuneLO(uint64_t wishedFreq)
{
	uint64_t actLo;

	actLo = hardware->TuneLo(wishedFreq);

	// we need shift the samples
	int64_t offset = wishedFreq - actLo;
	DbgPrintf("Offset freq %" PRIi64 "\n", offset);
	float fc = r2iqCntrl->setFreqOffset(offset / (getSampleRate() / 2.0f));

	if (this->fc != fc)
	{
		std::unique_lock<std::mutex> lk(fc_mutex);
		*stateFineTune = shift_limited_unroll_C_sse_init(fc, 0.0F);
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

bool RadioHandlerClass::UptPga(bool b)
{
	pga = b;
	if (pga)
		hardware->FX3SetGPIO(PGA_EN);
	else
		hardware->FX3UnsetGPIO(PGA_EN);
	return pga;
}

bool RadioHandlerClass::UptRand(bool b)
{
	randout = b;
	if (randout)
		hardware->FX3SetGPIO(RANDO);
	else
		hardware->FX3UnsetGPIO(RANDO);
	r2iqCntrl->updateRand(randout);
	return randout;
}

void RadioHandlerClass::CaculateStats()
{
	high_resolution_clock::time_point EndingTime;
	float kbRead = 0;
	float kSReadIF = 0;

	kbRead = 0; // zeros the kilobytes counter
	kSReadIF = 0;

	BytesXferred = 0;
	SamplesXIF = 0;
	auto StartingTime = high_resolution_clock::now();

	while (run) {
		kbRead = float(BytesXferred) / 1000.0f;
		kSReadIF = float(SamplesXIF) / 1000.0f;

		EndingTime = high_resolution_clock::now();

		duration<float,std::ratio<1,1>> timeElapsed(EndingTime-StartingTime);

		mBps = (float)kbRead / timeElapsed.count() / 1000 / sizeof(int16_t);
		mSpsIF = (float)kSReadIF / timeElapsed.count() / 1000;

		BytesXferred = 0;
		SamplesXIF = 0;

		StartingTime = high_resolution_clock::now();
		std::this_thread::sleep_for(0.5s);
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

void RadioHandlerClass::uptLed(int led, bool on)
{
	int pin;
	switch(led)
	{
		case 0:
			pin = LED_YELLOW;
			break;
		case 1:
			pin = LED_RED;
			break;
		case 2:
			pin = LED_BLUE;
			break;
		default:
			return;
	}

	if (on)
		hardware->FX3SetGPIO(pin);
	else
		hardware->FX3UnsetGPIO(pin);
}
