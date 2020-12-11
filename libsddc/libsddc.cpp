#include "libsddc.h"
#include "config.h"
#include "RadioHandler.h"

struct sddc
{
    SDDCStatus status;
    RadioHandlerClass* handler;
    uint8_t led;
    double freq;
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

static void Callback(float* data, uint32_t len)
{
}

sddc_t *sddc_open(int index, const char* imagefile)
{
    auto ret_val = new sddc_t();

    fx3class *fx3 = new fx3class();

    // open the firmware
    unsigned char* res_data;
    uint32_t res_size;

    FILE *fp = fopen("SDDC_FX3.img", "rb");
    if (fp == nullptr)
    {
        return nullptr;
    }

    fseek(fp, 0, SEEK_END);
    res_size = ftell(fp);
    res_data = (unsigned char*)malloc(res_size);
    fseek(fp, 0, SEEK_SET);
    fread(res_data, 1, res_size, fp);

    fx3->Open(res_data, res_size);

    ret_val->handler = new RadioHandlerClass();

    if (ret_val->handler->Init(fx3, Callback))
    {
        ret_val->status = SDDC_STATUS_READY;
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
        case RadioModel::RX999:
            return HW_RX999;
    }

    return HW_NORADIO;
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
    }

    return RFMode::NO_RF_MODE;
}

int sddc_set_rf_mode(sddc_t *t, enum RFMode rf_mode)
{
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
double sddc_get_hf_attenuation(sddc_t *t)
{
    return 0;
}

int sddc_set_hf_attenuation(sddc_t *t, double attenuation)
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
    return t->freq;
}

int sddc_set_tuner_frequency(sddc_t *t, double frequency)
{
    t->freq = t->handler->TuneLO(frequency);

    return 0;
}

int sddc_get_tuner_rf_attenuations(sddc_t *t, const double *attenuations[])
{
    return 0;
}

double sddc_get_tuner_rf_attenuation(sddc_t *t)
{
    return 0;
}

int sddc_set_tuner_rf_attenuation(sddc_t *t, double attenuation)
{
    // TODO
    return 0;
}

int sddc_get_tuner_if_attenuations(sddc_t *t, const double *attenuations[])
{
    // TODO
    return 0;
}

double sddc_get_tuner_if_attenuation(sddc_t *t)
{
    return 0;
}

int sddc_set_tuner_if_attenuation(sddc_t *t, double attenuation)
{
    return 0;
}

int sddc_get_vhf_bias(sddc_t *t)
{
    return 0;
}

int sddc_set_vhf_bias(sddc_t *t, int bias)
{
    return 0;
}


/* streaming functions */
typedef void (*sddc_read_async_cb_t)(uint32_t data_size, uint8_t *data,
                                      void *context);

double sddc_get_sample_rate(sddc_t *t)
{
    return 0;
}

int sddc_set_sample_rate(sddc_t *t, double sample_rate)
{
    return 0;
}

int sddc_set_async_params(sddc_t *t, uint32_t frame_size, 
                          uint32_t num_frames, sddc_read_async_cb_t callback,
                          void *callback_context)
{
    return 0;
}

int sddc_start_streaming(sddc_t *t)
{
    return 0;
}

int sddc_handle_events(sddc_t *t)
{
    return 0;
}

int sddc_stop_streaming(sddc_t *t)
{
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


/* Misc functions */
double sddc_get_frequency_correction(sddc_t *t)
{
    return gdFreqCorr_ppm / 0.000001;
}

int sddc_set_frequency_correction(sddc_t *t, double correction)
{
    gdFreqCorr_ppm = correction * 1000000;

    return 0;
}
