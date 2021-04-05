#include "libsddc.h"
#include "config.h"
#include "r2iq.h"
#include "RadioHandler.h"

#include <stdarg.h>

struct sddc
{
    SDDCStatus status;
    RadioHandlerClass* handler;
    uint8_t led;
    int samplerateidx;
    double freq;

    sddc_read_async_cb_t callback;
    sddc_raw_read_async_cb_t raw_callback;
    bool raw;
    void *callback_context;
};

sddc_t *current_running;

void logFX3(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}

static void Callback(const float* data, uint32_t len)
{
    if(current_running->callback != nullptr && !current_running->raw) {
        current_running->callback(len, (float*) data, current_running->callback_context);
    }
}

class rawdata : public r2iqControlClass {
    void Init(float gain, ringbuffer<int16_t>* buffers, ringbuffer<float>* obuffers) override
    {
        buf = buffers;
    }

    void TurnOn() override
    {
        this->r2iqOn = true;
        pull_thread = std::thread(
            [this]() {
                while(this->r2iqOn)
                {
                    const int16_t* data = buf->getReadPtr();
                    if(!this->r2iqOn) {
                        break;
                    }
                    if(current_running->raw_callback != nullptr) {
                        current_running->raw_callback(transferSize, (uint8_t*) data, current_running->callback_context);
                    }
                    buf->ReadDone();
                }
            });
    }

    void TurnOff() override
    {
        this->r2iqOn = false;
        pull_thread.join();
    }

private:
    ringbuffer<int16_t>* buf;
    std::thread pull_thread;
};

int sddc_get_device_count()
{
    return 1;
}

int sddc_get_device_info(struct sddc_device_info **sddc_device_infos)
{
    auto ret = new sddc_device_info();
    const char *todo = "TODO";
    ret->manufacturer = todo;
    ret->product = todo;
    ret->serial_number = todo;

    *sddc_device_infos = ret;

    return 1;
}

int sddc_free_device_info(struct sddc_device_info *sddc_device_infos)
{
    delete sddc_device_infos;
    return 0;
}

sddc_t *sddc_open(int index, const char* imagefile, int raw)
{
    auto ret_val = new sddc_t();

    fx3class *fx3 = CreateUsbHandler();
    if (fx3 == nullptr)
    {
        return nullptr;
    }

    // open the firmware
    unsigned char* res_data;
    uint32_t res_size;

    FILE *fp = fopen(imagefile, "rb");
    if (fp == nullptr)
    {
        return nullptr;
    }

    fseek(fp, 0, SEEK_END);
    res_size = ftell(fp);
    res_data = (unsigned char*)malloc(res_size);
    fseek(fp, 0, SEEK_SET);
    if (fread(res_data, 1, res_size, fp) != res_size)
        return nullptr;

    bool openOK = fx3->Open(res_data, res_size);
    if (!openOK)
        return nullptr;

    ret_val->handler = new RadioHandlerClass();
    ret_val->handler->EnableDebug(logFX3, nullptr);

    if (ret_val->handler->Init(fx3, Callback, raw != 0 ? new rawdata() : nullptr))
    {
        ret_val->status = SDDC_STATUS_READY;
        ret_val->samplerateidx = 0;
        ret_val->raw = raw;
    }

    return ret_val;
}

void sddc_close(sddc_t *that)
{
    if (that->handler)
        delete that->handler;
    delete that;
}

enum SDDCStatus sddc_get_status(sddc_t *t)
{
    return t->status;
}

enum SDDCHWModel sddc_get_hw_model(sddc_t *t)
{
    switch(t->handler->getModel())
    {
        case RadioModel::BBRF103:
            return HW_BBRF103;
        case RadioModel::HF103:
            return HW_HF103;
        case RadioModel::RX888:
            return HW_RX888;
        case RadioModel::RX888r2:
            return HW_RX888R2;
        case RadioModel::RX888r3:
            return HW_RX888R3;
        case RadioModel::RX999:
            return HW_RX999;
        default:
            return HW_NORADIO;
    }
}

const char *sddc_get_hw_model_name(sddc_t *t)
{
    return t->handler->getName();
}

uint16_t sddc_get_firmware(sddc_t *t)
{
    return t->handler->GetFirmware();
}

const double *sddc_get_frequency_range(sddc_t *t)
{
    return nullptr;
}

enum RFMode sddc_get_rf_mode(sddc_t *t)
{
    switch(t->handler->GetmodeRF())
    {
        case HFMODE:
            return RFMode::HF_MODE;
        case VHFMODE:
            return RFMode::VHF_MODE;
        default:
            return RFMode::NO_RF_MODE;
    }
}

int sddc_set_rf_mode(sddc_t *t, enum RFMode rf_mode)
{
    switch (rf_mode)
    {
    case VHF_MODE:
        t->handler->UpdatemodeRF(VHFMODE);
        break;
    case HF_MODE:
        t->handler->UpdatemodeRF(HFMODE);
    default:
        return -1;
    }

    return 0;
}

int sddc_support_pga(sddc_t *t)
{
    switch(t->handler->getModel())
    {
        case RadioModel::RX888r2:
        case RadioModel::RX888r3:
        case RadioModel::RX999:
            return 1;
        default:
            return 0;
    }
}

int sddc_support_128adc(sddc_t *t)
{
    switch(t->handler->getModel())
    {
        case RadioModel::RX888:
        case RadioModel::RX888r2:
        case RadioModel::RX888r3:
        case RadioModel::RX999:
            return 1;
        default:
            return 0;
    }
}

/* LED functions */
int sddc_led_on(sddc_t *t, uint8_t led_pattern)
{
    if (led_pattern & YELLOW_LED)
        t->handler->uptLed(0, true);
    if (led_pattern & RED_LED)
        t->handler->uptLed(1, true);
    if (led_pattern & BLUE_LED)
        t->handler->uptLed(2, true);

    t->led |= led_pattern;

    return 0;
}

int sddc_led_off(sddc_t *t, uint8_t led_pattern)
{
    if (led_pattern & YELLOW_LED)
        t->handler->uptLed(0, false);
    if (led_pattern & RED_LED)
        t->handler->uptLed(1, false);
    if (led_pattern & BLUE_LED)
        t->handler->uptLed(2, false);

    t->led &= ~led_pattern;

    return 0;
}

int sddc_led_toggle(sddc_t *t, uint8_t led_pattern)
{
    t->led = t->led ^ led_pattern;
    if (t->led & YELLOW_LED)
        t->handler->uptLed(0, false);
    if (t->led & RED_LED)
        t->handler->uptLed(1, false);
    if (t->led & BLUE_LED)
        t->handler->uptLed(2, false);

    return 0;
}


/* ADC functions */
int sddc_get_adc_dither(sddc_t *t)
{
    return t->handler->GetDither();
}

int sddc_set_adc_dither(sddc_t *t, int dither)
{
    t->handler->UptDither(dither != 0);
    return 0;
}

int sddc_get_adc_random(sddc_t *t)
{
    return t->handler->GetRand();
}

int sddc_set_adc_random(sddc_t *t, int random)
{
    t->handler->UptRand(random != 0);
    return 0;
}

double sddc_get_adc_rate(sddc_t *t)
{
    return t->handler->getSampleRate();
}

int sddc_set_adc_rate(sddc_t *t, double rate)
{
    if(rate > MAX_ADC_FREQ)
        rate = MAX_ADC_FREQ;
    if(rate < MIN_ADC_FREQ)
        rate = MIN_ADC_FREQ;

    //TODO: different rates
    if(sddc_support_128adc(t) && rate > DEFAULT_ADC_FREQ)
        rate = DEFAULT_ADC_FREQ*2;
    else
        rate = DEFAULT_ADC_FREQ;

    t->handler->UpdateSampleRate(rate);
    return 0;
}

int sddc_get_pga(sddc_t *t)
{
    return t->handler->GetPga() && sddc_support_pga(t);
}

int sddc_set_pga(sddc_t *t, int pga)
{
    if(sddc_support_pga(t))
        t->handler->UptPga(pga != 0);
    return 0;
}

/* HF block functions */
int sddc_get_hf_bias(sddc_t *t)
{
    return t->handler->GetBiasT_HF();
}

int sddc_set_hf_bias(sddc_t *t, int bias)
{
    t->handler->UpdBiasT_HF(bias != 0);
    return 0;
}


/* VHF block and VHF/UHF tuner functions */
double sddc_get_tuner_frequency(sddc_t *t)
{
    return t->freq;
}

int sddc_set_tuner_frequency(sddc_t *t, double frequency)
{
    t->freq = t->handler->TuneLO((int64_t)frequency);

    return 0;
}

int sddc_get_tuner_rf_attenuations(sddc_t *t, const float *attenuations[])
{
    return t->handler->GetRFAttSteps(attenuations);
}

double sddc_get_tuner_rf_attenuation(sddc_t *t)
{
    return 0;
}

int sddc_set_tuner_rf_attenuation(sddc_t *t, float attenuation)
{
    const float *attenuations;
    int size = t->handler->GetRFAttSteps(&attenuations);
    int idx = 0;
    for(int i=0; i<size;i++) {
        if(fabs(attenuation-attenuations[i])<0.00001) {
            idx = i;
        }
    }
    t->handler->UpdateattRF(idx);
    return 0;
}

int sddc_get_tuner_if_attenuations(sddc_t *t, const float *attenuations[])
{
    return t->handler->GetIFGainSteps(attenuations);
}

double sddc_get_tuner_if_attenuation(sddc_t *t)
{
    return 0;
}

int sddc_set_tuner_if_attenuation(sddc_t *t, float attenuation)
{
    const float *attenuations;
    int size = t->handler->GetIFGainSteps(&attenuations);
    int idx = 0;
    for(int i=0; i<size;i++) {
        if(fabs(attenuation-attenuations[i])<0.00001) {
            idx = i;
        }
    }
    t->handler->UpdateIFGain(idx);
    return 0;
}

int sddc_get_vhf_bias(sddc_t *t)
{
    return t->handler->GetBiasT_VHF();
}

int sddc_set_vhf_bias(sddc_t *t, int bias)
{
    t->handler->UpdBiasT_VHF(bias != 0);
    return 0;
}

uint64_t sddc_get_sample_rate(sddc_t *t)
{
    switch(t->samplerateidx)
    {
        case 5:
            return 64000000;
        case 4:
            return 32000000;
        case 3:
            return 16000000;
        case 2:
            return 8000000;
        case 1:
            return 4000000;
        case 0:
            return 2000000;
        default:
            return 0;
    }
}

int sddc_set_sample_rate(sddc_t *t, uint64_t sample_rate)
{
    switch(sample_rate)
    {
        case 64000000:
            t->samplerateidx = 5;
            break;
        case 32000000:
            t->samplerateidx = 4;
            break;
        case 16000000:
            t->samplerateidx = 3;
            break;
        case 8000000:
            t->samplerateidx = 2;
            break;
        case 4000000:
            t->samplerateidx = 1;
            break;
        case 2000000:
            t->samplerateidx = 0;
            break;
        default:
            return -1;
    }
    return 0;
}

int sddc_set_async_params(sddc_t *t, uint32_t frame_size,
                          uint32_t num_frames,
                          sddc_read_async_cb_t callback,
                          sddc_raw_read_async_cb_t raw_callback,
                          void *callback_context)
{
    // TODO: ignore frame_size, num_frames
    if(t->raw) {
        t->raw_callback = raw_callback;
        t->callback = nullptr;
    } else {
        t->callback = callback;
        t->raw_callback = nullptr;
    }
    t->callback_context = callback_context;
    return 0;
}

int sddc_start_streaming(sddc_t *t)
{
    current_running = t;
    t->handler->Start(t->samplerateidx);
    return 0;
}

int sddc_handle_events(sddc_t *t)
{
    return 0;
}

int sddc_stop_streaming(sddc_t *t)
{
    t->handler->Stop();
    current_running = nullptr;
    return 0;
}

int sddc_reset_status(sddc_t *t)
{
    return 0;
}

int sddc_read_sync(sddc_t *t, uint8_t *data, int length, int *transferred)
{
    return 0;
}
