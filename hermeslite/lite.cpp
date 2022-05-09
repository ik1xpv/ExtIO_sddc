#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <thread>
#include <mutex>
#include <limits.h>

#include <fcntl.h>
#include <math.h>

#if _WIN32
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <Ws2tcpip.h>

#include "resource.h"
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define closesocket(fd) close(fd)

#endif

#include "config.h"

#include "lite.h"

#include "RadioHandler.h"
#include "fft_mt_r2iq.h"
#include "pffft/pf_mixer.h"

//#define MAX_CHANNELS 7

#define ADC_RATE (48000 * 1024)

float mBps;
float mSpsIF[MAX_CHANNELS];

const uint32_t freq_min = 150000; // 150K
const uint32_t freq_max = ADC_RATE / 2;

int receivers = 1;
int decimate;

int sock_ep2;

int enable_thread = 0;
int active_thread = 0;

static void process_ep2(char *frame);
static void *handler_ep6(void *arg);

std::mutex mtx_finetune; // mutex for critical section

//#define EXT_BLOCKLEN 512 * 16 /* 32768 only multiples of 512 */

uint32_t buffers[MAX_CHANNELS][EXT_BLOCKLEN * 2];

int att = -1000;
int att_idx = 0;
int gain = -1000;
int gain_idx = 0;

RadioHardware *Radio;
fft_mt_r2iq r2iqCntrl;

uint32_t frequence[MAX_CHANNELS];
shift_limited_unroll_C_sse_data_t stateFineTune[MAX_CHANNELS];

static void SetChannel(int channel, uint32_t freq)
{
    if (channel >= receivers)
        return;

    if (freq < freq_min || freq > freq_max)
        return;

    if (frequence[channel] == freq)
        return;

    frequence[channel] = freq;

    // Set channel freq
    uint32_t actLo = (uint32_t)Radio->TuneLo(freq);

    // we need shift the samples
    int32_t offset = freq - actLo;

    mtx_finetune.lock();
    float fc = r2iqCntrl.setFreqOffset((float)offset / (ADC_RATE / 2), channel);
    stateFineTune[channel] = shift_limited_unroll_C_sse_init(fc, 0.0F);
    mtx_finetune.unlock();

    DbgPrintf("Freq[%d] : %ld (%f) fc=%f\n", channel, freq, (float)freq / (ADC_RATE / 2), fc);
}

static void SetAtt(int att_val)
{
    if (att_val != att)
    {
        const float *values;
        int max_step = Radio->getRFSteps(&values);

        att = att_val;
        DbgPrintf("Set ATT to : %d\n", att_val);
        for (int i = 0; i < max_step; i++)
        {
            if (values[i] >= -att)
            {
                att_idx = i;
                Radio->UpdateattRF(i);
                return;
            }
        }

        att_idx = max_step - 1;
        Radio->UpdateattRF(att_idx);
    }
}

static void SetRfGain(int gain_val)
{
    if (gain_val != gain)
    {
        const float *values;
        int max_step = Radio->getIFSteps(&values);

        gain = gain_val;
        DbgPrintf("Set gain to : %d\n", gain_val);
        for (int i = 0; i < max_step - 1; i++)
        {
            if (values[i + 1] > gain)
            {
                gain_idx = i;
                Radio->UpdateGainIF(i);
                return;
            }
        }

        gain_idx = max_step - 1;
        Radio->UpdateGainIF(gain_idx);
    }
}

ringbuffer<int16_t> input_buffer;
std::vector<ringbuffer<float> *> output_buffers;

static int server_proc();
bool running = false;
fx3class *Fx3;
uint8_t reply[20] = {0xef, 0xfe, 2, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 18, 1, 'R', 'X', '8', '8', '8', 'M', 'K', '0', MAX_CHANNELS};

bool str_begins_with(char *s, const char *cs)
{
    int slen = strlen(cs);
    return (strncmp(s, cs, slen) == 0);
}

static void CaculateStats();

int main(int argc, char *argv[])
{

#if _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    printf("HermesLite Emulator For RX888/MK2\n");
    printf("Support %d Channels\n", MAX_CHANNELS);

    // Initialize Hardware
    // open the data
    unsigned char *res_data;
    uint32_t res_size;

#if (_WIN32)
    // Load firmware
    HMODULE hInst = NULL;
    GetModuleHandleEx(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        (LPCTSTR)main,
        &hInst);

    HRSRC res = FindResource(hInst, MAKEINTRESOURCE(RES_BIN_FIRMWARE), RT_RCDATA);
    HGLOBAL res_handle = LoadResource(hInst, res);
    res_data = (unsigned char *)LockResource(res_handle);
    res_size = SizeofResource(hInst, res);
#else
    FILE *fp = fopen("SDDC_FX3.img", "rb");
    if (fp == nullptr)
    {
        return -2;
    }

    fseek(fp, 0, SEEK_END);
    res_size = ftell(fp);
    res_data = (unsigned char *)malloc(res_size);
    fseek(fp, 0, SEEK_SET);
    if (fread(res_data, 1, res_size, fp) != res_size)
    {
        perror("Fail to load firmware");
    }
#endif

    Fx3 = CreateUsbHandler();
    unsigned char idx = 0;
    char name[1024];
    while (Fx3->Enumerate(idx, name, res_data, res_size) && (idx < MAXNDEV))
    {
        // https://en.wikipedia.org/wiki/West_Bridge
        int retry = 2;
        while ((strncmp("WestBridge", name, sizeof("WestBridge")) != NULL) && retry-- > 0)
            Fx3->Enumerate(idx, name, res_data, res_size); // if it enumerates as BootLoader retry
        idx++;
    }

    if (idx > 1)
    {
        printf("%d RX888 receivers are found, please use -device to select one\n", idx);
    }
    else
    {
        idx = 0;
    }

    Fx3->Enumerate(idx, name, res_data, res_size);
    auto gbInitHW = Fx3->Open(res_data, res_size);

    uint8_t rdata[4];
    Fx3->GetHardwareInfo((uint32_t *)rdata);
    RadioModel radiomodel = (RadioModel)rdata[0];

    switch (radiomodel)
    {
    case RX888:
        Radio = new RX888Radio(Fx3);
        break;
    case RX888r2:
        Radio = new RX888R2Radio(Fx3);
        break;
    default:
        perror("Unsupported Hardware, please use RX888 serial.");
        return -3;
    }
    DbgPrintf("Found device: %s\n", name);
    Radio->Initialize(ADC_RATE);

    for (int i = 0; i < MAX_CHANNELS; i++)
    {
        ringbuffer<float> *outputbuffer = new ringbuffer<float>();
        outputbuffer->setBlockSize(EXT_BLOCKLEN * 2 * sizeof(float));
        output_buffers.push_back(outputbuffer);
    }

    input_buffer.setBlockSize(transferSamples);
    r2iqCntrl.Init(Radio->getGain(), &input_buffer, output_buffers);

    if (!gbInitHW)
    {
        perror("Load firmware failed.");
        return -2;
    }

    running = true;

    // switch to RF mode
    Radio->UpdatemodeRF(HFMODE);

    // Set ATT & RF Gain
    SetAtt(0);
    SetRfGain(0);

    reply[18] = '0' + (radiomodel - RX888);

    std::thread server = std::thread([]
                                     { server_proc(); });
    std::thread stats = std::thread([]
                                    { CaculateStats(); });

    while (running)
    {
        char cmdline[255];
        int new_val;

        // print promot
        printf("\n%s>", Radio->getName());
        int r = scanf("%[^\n]", cmdline);

        if (r != 1)
        {
            char c;
            // clear buf
            if (scanf("%c", &c) != 1)
            {
                perror("Parse failed");
            }
            continue;
        }

        if (str_begins_with(cmdline, "quit"))
        {
            running = false;
        }
        else if (str_begins_with(cmdline, "help"))
        {
            printf("Command List:\n");
            printf("quit\n");
            printf("stats\n");
            printf("set att=[val]\n");
            printf("set gain=[val]\n");

            printf("\n");
        }
        else if (str_begins_with(cmdline, "stats"))
        {
            const float *values;

            printf("Sample Rate: %d Decimate: %d ADC Rate: %3.1fMsps\n", int(ADC_RATE / 2 / powf(2, 1.0f * decimate)), decimate, mBps);
            Radio->getRFSteps(&values);
            printf("Att: %.1fDb\t", values[att_idx]);

            Radio->getIFSteps(&values);
            printf("Gain: %.1fDb\n", values[gain_idx]);

            for (int i = 0; i < MAX_CHANNELS; i++)
            {
                printf("Channel[%d] %dHZ Bandwidth=%3.3fMsps\n", i, frequence[i], mSpsIF[i]);
            }
        }
        else if (sscanf(cmdline, "set att=%d", &new_val) == 1)
        {
            SetAtt(new_val);
        }
        else if (sscanf(cmdline, "set gain=%d", &new_val) == 1)
        {
            SetRfGain(new_val);
        }
    }
}

using namespace std::chrono;
void CaculateStats()
{
#if 0
    uint32_t lastADCCount = 0;
    uint32_t lastIFCount[MAX_CHANNELS] = {0};
#endif
    high_resolution_clock::time_point EndingTime;

    auto StartingTime = high_resolution_clock::now();

    while (running)
    {

        EndingTime = high_resolution_clock::now();

        duration<float, std::ratio<1, 1>> timeElapsed(EndingTime - StartingTime);
#if 0
        float kbRead = float((RadioHandler.getOutput()->getWriteCount() - lastADCCount) * RadioHandler.getOutput()->getBlockSize()) / 1000.0f;
        mBps = (float)kbRead / timeElapsed.count() / 1000 / sizeof(int16_t);

        for (int i = 0; i < MAX_CHANNELS; i++)
        {
            float kSReadIF;
            auto output = freq2iq_i->getChannelOutput(i);

            if (output)
            {
                kSReadIF = float((output->getWriteCount() - lastIFCount[i]) * output->getBlockSize()) / 1000.0f;
                mSpsIF[i] = (float)kSReadIF / timeElapsed.count() / 1000;
                lastIFCount[i] = output->getWriteCount();
            }
            else
            {
                mSpsIF[i] = 0.0f;
                lastIFCount[i] = 0;
            }
        }

        lastADCCount = RadioHandler.getOutput()->getWriteCount();
#endif
        StartingTime = high_resolution_clock::now();

        std::this_thread::sleep_for(0.5s);
    }
    return;
}

int server_proc()
{
    size_t size;
    std::thread thread;
    char buffer[1032];
    uint32_t code;
    struct sockaddr_in addr_ep2, addr_from;
    socklen_t size_from;
    int yes = 1;

    /* set default rx sample rate */
    if ((sock_ep2 = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Failed to open socket");
        return EXIT_FAILURE;
    }

    setsockopt(sock_ep2, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes));

    memset(&addr_ep2, 0, sizeof(addr_ep2));
    addr_ep2.sin_family = AF_INET;
    addr_ep2.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_ep2.sin_port = htons(SERVICE_PORT);

    if (bind(sock_ep2, (struct sockaddr *)&addr_ep2, sizeof(addr_ep2)) < 0)
    {
        perror("Failed to bind to network");
        return -1;
    }

    int bufsize = 65535;
    setsockopt(sock_ep2, SOL_SOCKET, SO_SNDBUF, (const char *)&bufsize, sizeof(int));
    setsockopt(sock_ep2, SOL_SOCKET, SO_RCVBUF, (const char *)&bufsize, sizeof(int));

    while (running)
    {
        size_from = sizeof(addr_from);
        size = recvfrom(sock_ep2, buffer, 1032, 0, (struct sockaddr *)&addr_from, &size_from);
        if (size < 0)
        {
            perror("Failed to recv from network");
            return -1;
        }

        code = htonl(*(uint32_t *)buffer);
        // DbgPrintf("Code: %08x\n", code);
        switch (code)
        {
        case 0xEFFE0102:
            // DbgPrintf("Configuring Receiver\n");
            process_ep2(buffer + 11);
            process_ep2(buffer + 523);
            break;
        case 0xEFFE0200:
            DbgPrintf("Get Discovery from: %s:%d\n", inet_ntop(AF_INET, &addr_from.sin_addr, buffer, 100), addr_from.sin_port);
            // reply[2] = 2 + active_thread;
            memset(buffer, 0, 60);
            memcpy(buffer, reply, 20);
            if (sendto(sock_ep2, buffer, 60, 0, (struct sockaddr *)&addr_from, size_from) == -1)
            {
                DbgPrintf("Response disovery packet fail to send\n");
            }
            break;
        case 0xEFFE0400:
            DbgPrintf("Stop Receiver\n");
            if (!enable_thread)
                continue;
            enable_thread = 0;

            // Stop collect thread
            Fx3->StopStream();
            r2iqCntrl.TurnOff();
            Radio->FX3producerOff(); // FX3 stop the producer

            if (thread.joinable())
                thread.join();

            for (int i = receivers; i < MAX_CHANNELS; i++)
            {
                SetChannel(i, 0);
            }
            break;
        case 0xEFFE0401:
            // restart
        case 0xEFFE0402:
        case 0xEFFE0403:
        {
            DbgPrintf("Start Receiver\n");
            if (enable_thread)
                continue;

            DbgPrintf("Start Recever: %s:%d\n", inet_ntop(AF_INET, &addr_from.sin_addr, buffer, 100), addr_from.sin_port);

            struct sockaddr_in *addr_ep6 = new sockaddr_in();
            memcpy(addr_ep6, &addr_from, sizeof(addr_from));

            DbgPrintf("Target: %s:%d\n", inet_ntop(AF_INET, &addr_ep6->sin_addr, buffer, 100), addr_ep6->sin_port);

            enable_thread = 1;
            thread = std::thread(
                [addr_ep6]()
                { handler_ep6(addr_ep6); });
            break;
        }
        case 0x00000000:
            DbgPrintf("Protocol 2 discovery packet received\n");
            break;
        default:
            DbgPrintf("Unknow code: %08x\n", code);
        }
    }

    closesocket(sock_ep2);

    return EXIT_SUCCESS;
}

void process_ep2(char *frame)
{
    static int dither_last = 0;
    static int random_last = 0;

    uint32_t samplerate;

    // int ptt = frame[0] & 0x01; // LSB

    switch (frame[0] >> 1)
    {
    case 0:
    {
        receivers = ((frame[4] >> 3) & 7) + 1;
        // int data = (frame[4] >> 7) & 1;
        // int att = frame[3] & 0x03;
        /* set rx sample rate */
        switch (frame[1] & 3)
        {
        case 0:
            samplerate = 48000;
            break;
        case 1:
            samplerate = 96000;
            break;
        case 2:
            samplerate = 192000;
            break;
        case 3:
            samplerate = 384000;
            break;
        }
        int new_decimate = int(log2f(ADC_RATE / 2.0f / samplerate));

        int dither = frame[3] & 0x08;
        int random = frame[3] & 0x10;

        if (dither_last != dither)
        {
            dither_last = dither;
            if (dither)
                Radio->FX3SetGPIO(DITH);
            else
                Radio->FX3UnsetGPIO(DITH);
        }

        if (random_last != random)
        {
            random_last = random;
            if (random)
                Radio->FX3SetGPIO(RANDO);
            else
                Radio->FX3UnsetGPIO(RANDO);

            r2iqCntrl.updateRand(random != 0);
        }

        if (new_decimate != decimate)
        {
            decimate = new_decimate;
            DbgPrintf("Sample Rate changed to %d\n", samplerate);
            r2iqCntrl.setDecimate(decimate);
        }

        for (int i = receivers; i < MAX_CHANNELS; i++)
        {
            SetChannel(i, 0);
        }

        // DbgPrintf("Set SR=%d Recivers:%d (dec: %d)\n", receivers, samplerate, decimate);
        break;
    }
    /*
     0 0 0 0 0 1 x C1, C2, C3, C4 NCO Frequency in Hz for Transmitter, Apollo ATU
                                (32 bit binary representation - MSB in C1)
    */
    case 1:
        break;
    /*
    C0
    0 0 0 0 0 1 0 x C1, C2, C3, C4 NCO Frequency in Hz for Receiver _1
    0 0 0 0 0 1 1 x C1, C2, C3, C4 NCO Frequency in Hz for Receiver _2
    0 0 0 0 1 0 0 x C1, C2, C3, C4 NCO Frequency in Hz for Receiver _3
    0 0 0 0 1 0 1 x C1, C2, C3, C4 NCO Frequency in Hz for Receiver _4
    0 0 0 0 1 1 0 x C1, C2, C3, C4 NCO Frequency in Hz for Receiver _5
    0 0 0 0 1 1 1 x C1, C2, C3, C4 NCO Frequency in Hz for Receiver _6
    0 0 0 1 0 0 0 x C1, C2, C3, C4 NCO Frequency in Hz for Receiver _7
    */
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    {
        /* set rx phase increment */
        uint32_t freq = ntohl(*(uint32_t *)(frame + 1));

        SetChannel((frame[0] >> 1) - 2, freq);
        break;
    }

    case 9:
    {
        // set PA, filter, etc
        break;
    }

    /*
    C0
    0 0 0 1 0 1 0 x
    */
    case 10:
    {
        int rx_att_data = frame[4] & 0x1f;
        if (frame[4] & 0b00100000)
        {
            SetAtt(rx_att_data);
        }
        else
        {
            SetAtt(0);
        }
        break;
    }
    case 11:
    {
        // printf("i2c_ard_rxatt_data 0\n");
        break;
    }
    case 14:
        /*
        24 bit data field 000000BA000RRRRR00TTTTTT
        BA=PA disable, 6m LNA
        RRRRR=5 bits RX att when in TX
        TTTTTT=6 bits TX att (0.5 dB units)
        */
        // printf("i2c_ard_rxatt_data  1\n");
        break;
    case 15:
    {
        // int cw_int_data = frame[1] & 1;
        // int dac_level_data = frame[2];
        // int cw_delay = frame[3];
        // printf("CW int=%d, dac_level=%d, cw_delay=%d\n", cw_int_data, dac_level_data, cw_delay);
        break;
    }
    case 16:
    {
        // output_buffer[C0] = 0x20;
        // output_buffer[C1] = (cw_keyer_hang_time >> 2) & 0xFF;
        // output_buffer[C2] = cw_keyer_hang_time & 0x03;
        // output_buffer[C3] = (cw_keyer_sidetone_frequency >> 4) & 0xFF;
        // output_buffer[C4] = cw_keyer_sidetone_frequency & 0x0F;
        break;
    }
    case 17:
    {
        // output_buffer[C0] = 0x22;
        // output_buffer[C1] = (eer_pwm_min >> 2) & 0xFF;
        // output_buffer[C2] = eer_pwm_min & 0x03;
        // output_buffer[C3] = (eer_pwm_max >> 3) & 0xFF;
        // output_buffer[C4] = eer_pwm_max & 0x03;
        break;
    }
    case 18:
    {
        break;
    }
    default:
        // DbgPrintf("Unknown Frame id=%d\n", frame[0] >> 1);
        break;
    }
}

static void GetMoreData()
{
    for (int ch = 0; ch < receivers; ch++)
    {
        auto data = output_buffers[ch];
        if (frequence[ch] == 0)
            continue;

        auto ptr = data->getReadPtr();
        mtx_finetune.lock();
        shift_limited_unroll_C_sse_inp_c((complexf *)ptr, EXT_BLOCKLEN, &stateFineTune[ch]);
        mtx_finetune.unlock();
        for (int i = 0; i < EXT_BLOCKLEN; i++)
        {
            buffers[ch][i * 2] = htonl((int32_t)(ptr[i * 2 + 1] * INT32_MAX));
            buffers[ch][i * 2 + 1] = htonl((int32_t)(ptr[i * 2] * INT32_MAX));
        }

        data->ReadDone();
    }
}

#define PREPARE_HEADER 0
#define PREPARE_DATA1HEADER 1
#define PREPARE_DATA1 2
#define PREPARE_DATA2HEADER 3
#define PREPARE_DATA2 4
#define SEND 5

void *handler_ep6(void *arg)
{
    uint8_t packet[1032];
    struct sockaddr_in *addr = (struct sockaddr_in *)arg;

    uint32_t sequence = 0;

    memset(packet, 0, sizeof(packet));

    int state = PREPARE_HEADER;

    int outputptr = 0;
    int dataindex = EXT_BLOCKLEN;

    Radio->FX3producerOn(); // FX3 start the producer
    // 0,1,2,3,4 => 32,16,8,4,2 MHz
    r2iqCntrl.setDecimate(decimate);
    r2iqCntrl.TurnOn();
    Fx3->StartStream(input_buffer, QUEUE_SIZE);

    while (enable_thread)
    {

        switch (state)
        {
        case PREPARE_HEADER:
            *(uint32_t *)packet = 0x0601feef;
            *(uint32_t *)(packet + 4) = htonl(sequence);
            state++;
            break;

        case PREPARE_DATA2HEADER:
        case PREPARE_DATA1HEADER:
            if (state == PREPARE_DATA1HEADER)
                outputptr = 8;
            else
                outputptr = 520;
            packet[outputptr++] = 0x7f; // 8
            packet[outputptr++] = 0x7f; // 9
            packet[outputptr++] = 0x7f; // 10

            packet[outputptr++] = 0; // 11 - cverflow high, ptt
            packet[outputptr++] = 0; // 12 - overflow count
            packet[outputptr++] = 0; // 13
            packet[outputptr++] = 0; // 14
            packet[outputptr++] = 0; // 15
            state++;
            break;

        case PREPARE_DATA1:
        case PREPARE_DATA2:
        {
            // Every packet data section is 504 bytes
            // 512 total - 3 bytes sync , 5 bytes command
            // 504bytes to contain N samples
            // 1 receiver   504 / (2 * 3 + 2) = 63
            // 2 receivers  504 / (4 * 3 + 2) = 36
            // 3 receivers  504 / (6 * 3 + 2) = 25, 4bytes pending
            // 4 receivers  504 / (8 * 3 + 2) = 19, 10byts pending

            int nsamples = (512 - 3 - 5) / (receivers * 2 * 3 + 2);
            for (int i = 0; i < nsamples; i++)
            {
                if (dataindex == EXT_BLOCKLEN) // no more data
                {
                    GetMoreData();
                    dataindex = 0;
                }

                for (int ch = 0; ch < receivers; ch++)
                {
                    memcpy(&packet[outputptr], &buffers[ch][dataindex * 2], 3);
                    memcpy(&packet[outputptr + 3 * receivers], &buffers[ch][dataindex * 2 + 1], 3);
                    outputptr += 3;
                }

                outputptr += 3 * receivers; // skip over Q samples
                dataindex++;
                outputptr += 2; // microphone int16 input
            }

            state++;
            break;
        }
        case SEND:
            if (sendto(sock_ep2, (const char *)packet, 1032, 0, (sockaddr *)addr, sizeof(*addr)) == -1)
            {
                DbgPrintf("Fail to send data packet\n");
            }
            sequence++;
            state = PREPARE_HEADER;
            break;
        }
    }

    delete addr;
    return NULL;
}
