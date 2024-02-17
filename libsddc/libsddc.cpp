#include "libsddc.h"
#include "config.h"
#include "r2iq.h"
#include "RadioHandler.h"
#include "string.h"

#include <vector>
#include <string>

extern "C"
{
    #include "SDDC_FX3_img.h"
}

struct sddc
{
    SDDCStatus status;
    RadioHandlerClass* handler;
    uint8_t led;
    int samplerateidx;
    uint64_t freq;

    std::vector<void*> frame_buffers;
    size_t frame_size;

    sddc_read_async_cb_t callback;
    void *callback_context;
};

sddc_t *current_running;
fx3class *Fx3 = NULL;
std::vector<std::string> devices;

// Callback of data
// Fill the ring buffer
static void Callback(void* context, const float* data, uint32_t len)
{
    struct sddc *sddc = (struct sddc *)context;

    sddc->callback(len, data, sddc->callback_context);
}

class rawdata : public r2iqControlClass {
    void Init(float gain, ringbuffer<int16_t>* buffers, ringbuffer<float>* obuffers) override
    {
        idx = 0;
    }

    void TurnOn() override
    {
        this->r2iqOn = true;
        idx = 0;
    }

private:
    int idx;
};

static void probe_devices()
{
    if (Fx3)
        return;

    if (Fx3 == NULL)
    {
        Fx3 = CreateUsbHandler();
    }

    unsigned char idx = 0;
    char devicename[1024];
    while (Fx3->Enumerate(idx, devicename, SDDC_FX3_img_data, SDDC_FX3_img_size) && (idx < MAXNDEV))
    {
        // https://en.wikipedia.org/wiki/West_Bridge
        int retry = 2;
        while ((strncmp("WestBridge", devicename,sizeof("WestBridge")) != 0) && retry-- > 0)
            Fx3->Enumerate(idx, devicename, SDDC_FX3_img_data, SDDC_FX3_img_size); // if it enumerates as BootLoader retry
        devices.push_back(devicename);
        idx++;
    }
    
}

int sddc_get_device_count()
{
    probe_devices();

    return devices.size();
}

int sddc_get_device_info(struct sddc_device_info **sddc_device_infos)
{
    int count = 0;
    *sddc_device_infos = new sddc_device_info[devices.size()];
    probe_devices();

    for(auto &s : devices)
    {
        auto ret = &(*sddc_device_infos)[count];
        ret->manufacturer = "SDDC";

        char *product = new char[256];
        auto len = strchr(s.c_str(), ' ') - s.c_str();
        strncpy(product, s.c_str(), len);
        product[len] = '\0';
        ret->product = product;

        char *serial = new char[256];
        strcpy(serial, strstr(s.c_str(), "sn:") + 3);
        ret->serial_number = serial;

        count++;
    }

    return sddc_get_device_count();
}

int sddc_free_device_info(struct sddc_device_info *sddc_device_infos)
{
    for (int i = 0; i < sddc_get_device_count(); i++)
    {
        delete[] sddc_device_infos[i].product;
        delete[] sddc_device_infos[i].serial_number;
    }

    delete[] sddc_device_infos;
    return 0;
}

sddc_t *sddc_open(int index)
{
    auto ret_val = new sddc_t();
    probe_devices();

    bool openOK = Fx3->Open(index, SDDC_FX3_img_data, SDDC_FX3_img_size);
    if (!openOK)
        return nullptr;

    ret_val->handler = new RadioHandlerClass();

    if (ret_val->handler->Init(Fx3, Callback, nullptr, ret_val))
    {
        ret_val->status = SDDC_STATUS_READY;
        ret_val->samplerateidx = 0;
    }

    return ret_val;
}

/*
sddc_t *sddc_open_raw(int index)
{
    auto ret_val = new sddc_t();
    probe_devices();

    bool openOK = Fx3->Open(index, SDDC_FX3_img_data, SDDC_FX3_img_size);
    if (!openOK)
        return nullptr;

    ret_val->handler = new RadioHandlerClass();

    if (ret_val->handler->Init(Fx3, Callback, new rawdata()))
    {
        ret_val->status = SDDC_STATUS_READY;
        ret_val->samplerateidx = 0;
    }

    return ret_val;
}
*/

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

/* HF block functions */
float sddc_get_hf_attenuation(sddc_t *t)
{
    return 0;
}

int sddc_set_hf_attenuation(sddc_t *t, float attenuation)
{
    return 0;
}

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
    return (double)t->freq;
}

int sddc_set_tuner_frequency(sddc_t *t, double frequency)
{
    t->freq = t->handler->TuneLO((uint64_t)frequency);

    return 0;
}

int sddc_get_tuner_rf_attenuations(sddc_t *t, const float *attenuations[])
{
    return 0;
}

float sddc_get_tuner_rf_attenuation(sddc_t *t)
{
    return 0;
}

int sddc_set_tuner_rf_attenuation(sddc_t *t, float attenuation)
{
    t->handler->UpdateattRF(5);
    return 0;
}

int sddc_get_tuner_if_attenuations(sddc_t *t, const float *attenuations[])
{
    // TODO
    return 0;
}

float sddc_get_tuner_if_attenuation(sddc_t *t)
{
    return 0;
}

int sddc_set_tuner_if_attenuation(sddc_t *t, float attenuation)
{
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

float sddc_get_sample_rate(sddc_t *t)
{
    return 0;
}

int sddc_set_sample_rate(sddc_t *t, uint64_t sample_rate)
{
    if (t->status != SDDC_STATUS_READY)
    {
        return ERROR_INVALID_STATE;
    }

    switch(sample_rate)
    {
        case 32000000:
            t->samplerateidx = 0;
            break;
        case 16000000:
            t->samplerateidx = 1;
            break;
        case 8000000:
            t->samplerateidx = 2;
            break;
        case 4000000:
            t->samplerateidx = 3;
            break;
        case 2000000:
            t->samplerateidx = 4;
            break;
        default:
            return -1;
    }
    return 0;
}

int sddc_set_async_params(sddc_t *t, uint32_t frame_size, 
                          uint32_t num_frames, sddc_read_async_cb_t callback,
                          void *callback_context)
{
    // TODO: ignore frame_size, num_frames
    t->callback = callback;
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
