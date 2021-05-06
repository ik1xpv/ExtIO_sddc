#include "license.txt" 

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "pf_mixer.h"
#include "RadioHandler.h"
#include "config.h"
#include "config.h"
#include "PScope_uti.h"
#include "../Interface.h"

#include <chrono>

using namespace std::chrono;

RadioHandlerClass::RadioHandlerClass() :
	DbgPrintFX3(nullptr),
	GetConsoleIn(nullptr),
	run(false),
	pga(false),
	dither(false),
	randout(false),
	biasT_HF(false),
	biasT_VHF(false),
	firmware(0),
	modeRF(NOMODE),
	adcrate(DEFAULT_ADC_FREQ),
	hardware(new DummyRadio(nullptr))
{
	inputbuffer.setBlockSize(transferSamples);
}

RadioHandlerClass::~RadioHandlerClass()
{
}

const char *RadioHandlerClass::getName()
{
	return hardware->getName();
}

bool RadioHandlerClass::Init(fx3class* Fx3)
{
	uint8_t rdata[4];
	this->fx3 = Fx3;

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

	case RX888r3:
		hardware = new RX888R3Radio(Fx3);
		break;

	case RX999:
		hardware = new RX999Radio(Fx3);
		break;

	case RXLUCY:
		hardware = new RXLucyRadio(Fx3);
		break;

	default:
		hardware = new DummyRadio(Fx3);
		DbgPrintf("WARNING no SDR connected\n");
		break;
	}
	DbgPrintf("%s | firmware %x\n", hardware->getName(), firmware);

	return true;
}

bool RadioHandlerClass::Start(uint32_t adcrate)
{
	Stop();
	DbgPrintf("RadioHandlerClass::Start\n");

	run = true;
	count = 0;

	this->adcrate = adcrate;
	hardware->Initialize(adcrate);

	hardware->FX3producerOn();  // FX3 start the producer

	fx3->StartStream(inputbuffer, QUEUE_SIZE);

	return true;
}

bool RadioHandlerClass::Stop()
{
	std::unique_lock<std::mutex> lk(stop_mutex);
	DbgPrintf("RadioHandlerClass::Stop %d\n", run);
	if (run)
	{
		run = false; // now waits for threads

		fx3->StopStream();

		hardware->FX3producerOff();     //FX3 stop the producer

		this->inputbuffer.Stop();
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

rf_mode RadioHandlerClass::PrepareLo(uint64_t lo)
{
	return hardware->PrepareLo(lo);
}

uint64_t RadioHandlerClass::TuneLO(uint64_t wishedFreq)
{
	uint64_t actLo;

	actLo = hardware->TuneLo(wishedFreq);
#if 0
	// we need shift the samples
	int64_t offset = wishedFreq - actLo;
	DbgPrintf("Offset freq %" PRIi64 "\n", offset);
	float fc = r2iqCntrl->setFreqOffset(offset / (getSampleRate() / 2.0f));
	if (GetmodeRF() == VHFMODE)
		fc = -fc;   // sign change with sideband used
	if (this->fc != fc)
	{
		std::unique_lock<std::mutex> lk(fc_mutex);
		*stateFineTune = shift_limited_unroll_C_sse_init(fc, 0.0F);
		this->fc = fc;
	}
#endif
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
	return randout;
}

void RadioHandlerClass::CaculateStats()
{
	#if 0
	high_resolution_clock::time_point EndingTime;
	float kbRead = 0;
	float kSReadIF = 0;

	kbRead = 0; // zeros the kilobytes counter
	kSReadIF = 0;

	BytesXferred = 0;
	SamplesXIF = 0;

	uint8_t  debdata[MAXLEN_D_USB];
	memset(debdata, 0, MAXLEN_D_USB);

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
	
#ifdef _DEBUG  
		int nt = 10;
		while (nt-- > 0)
		{
			std::this_thread::sleep_for(0.05s);
			debdata[0] = 0; //clean buffer 
			if (GetConsoleIn != nullptr)
			{
				GetConsoleIn((char *)debdata, MAXLEN_D_USB);
				if (debdata[0] !=0) 
					DbgPrintf("%s", (char*)debdata);
			}

			if (hardware->ReadDebugTrace(debdata, MAXLEN_D_USB) == true) // there are message from FX3 ?
			{
				int len = strlen((char*)debdata);
				if (len > MAXLEN_D_USB - 1) len = MAXLEN_D_USB - 1;
				debdata[len] = 0;
				if ((len > 0)&&(DbgPrintFX3 != nullptr))
				{
					DbgPrintFX3("%s", (char*)debdata);
					memset(debdata, 0, sizeof(debdata));
				}
			}
			
		}
#else
		std::this_thread::sleep_for(0.5s);
#endif
	}
	return;
#endif
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
