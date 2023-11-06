#include "r2iq.h"
#include "../Core/usb/IUsbHandler.h"
#include "CppUnitTestFramework.hpp"
#include <thread>
#include <chrono>
#include <vector>
#include <inttypes.h>  // For portable 64-bit type printf codes

#include "RadioHandler.h"

using namespace std::chrono;

class UsbHandlerTest : public IUsbHandler
{
    bool Open(const uint8_t* fw_data, uint32_t fw_size)
    {
        return true;
    }

    bool Control(FX3Command command, uint8_t data = 0)
    {
        return true;
    }

    bool Control(FX3Command command, uint32_t data)
    {
        return true;
    }

    bool Control(FX3Command command, uint64_t data)
    {
        return true;
    }

    bool SetArgument(uint16_t index, uint16_t value)
    {
        return true;
    }

    bool GetHardwareInfo(uint32_t* data) {
        const uint8_t d[4] = {
            0, FIRMWARE_VER_MAJOR, FIRMWARE_VER_MINOR, 0
        };

        *data = *(uint32_t*)d;
        return true;
    }

    bool Enumerate(unsigned char& idx, char* lbuf, size_t bufSize, const uint8_t* fw_data, uint32_t fw_size)
    {
        return true;
    }

    bool ReadDebugTrace(uint8_t* pdata, uint8_t len) {
        return true;
    }

    std::thread emuthread;
    bool run;
	long nxfers;
    void StartStream(ringbuffer<int16_t>& input, int numofblock)
    {
        run = true;
        emuthread = std::thread([&input, this]{
            while(run)
            {
                auto ptr = input.getWritePtr();
                memset(ptr, 0x5A, input.getWriteCount());
                input.WriteDone();
                ++nxfers;
                std::this_thread::sleep_for(1ms);
            }
        });
    }

	void StopStream() {
        run = false;
        emuthread.join();
    }
public:
	long Xfers(bool clear) { long rv=nxfers; if (clear) nxfers=0; return rv; }
};

static uint32_t count;
static uint64_t totalsize;

static void Callback(void* context, const float* data, uint32_t len)
{
    count++;
    totalsize += len;
}

namespace {
    struct CoreFixture {};
}

TEST_CASE(CoreFixture, BasicTest)
{
    auto usb = new UsbHandlerTest();

    auto radio = new RadioHandlerClass();

    radio->Init(usb, Callback);

    REQUIRE_EQUAL(radio->getModel(), NORADIO);
    REQUIRE_EQUAL(radio->getName(), "Dummy");

    REQUIRE_EQUAL(radio->getSampleRate(), 64000000u);
    radio->UpdateSampleRate(128000000);
    REQUIRE_EQUAL(radio->getSampleRate(), 128000000u);

    REQUIRE_EQUAL(radio->GetDither(), false);
    radio->UptDither(true);
    REQUIRE_EQUAL(radio->GetDither(), true);

    REQUIRE_EQUAL(radio->GetRand(), false);
    radio->UptRand(true);
    REQUIRE_EQUAL(radio->GetRand(), true);

    REQUIRE_EQUAL(radio->GetPga(), false);
    radio->UptPga(true);
    REQUIRE_EQUAL(radio->GetPga(), true);

    REQUIRE_EQUAL(radio->GetBiasT_HF(), false);
    radio->UpdBiasT_HF(true);
    REQUIRE_EQUAL(radio->GetBiasT_HF(), true);

    REQUIRE_EQUAL(radio->GetBiasT_VHF(), false);
    radio->UpdBiasT_VHF(true);
    REQUIRE_EQUAL(radio->GetBiasT_VHF(), true);

    delete radio;
    delete usb;
}

TEST_CASE(CoreFixture, R2IQTest)
{
    auto usb = new UsbHandlerTest();

    auto radio = new RadioHandlerClass();

    radio->Init(usb, Callback);

    for (int decimate = 0; decimate < 5; decimate++)
    {
        count = 0;
        totalsize = 0;
        radio->Start(decimate); // full bandwidth
        std::this_thread::sleep_for(1s);
        radio->Stop();

        REQUIRE_TRUE(count > 0);
        REQUIRE_TRUE(totalsize > 0);
        REQUIRE_EQUAL(totalsize / count, transferSamples / 2);
        printf("decimate=%d nxfers=%ld count=%u totalsize=%" PRIu64 "\n",
            decimate, usb->Xfers(true), count, totalsize);
    }

    delete radio;
    delete usb;
}

TEST_CASE(CoreFixture, TuneTest)
{
    auto usb = new UsbHandlerTest();

    auto radio = new RadioHandlerClass();

    radio->Init(usb, Callback);

    radio->Start(1); // full bandwidth

    for (uint64_t i = 1000; i < 15000000;  i+=377000)
    {
        radio->TuneLO(i);
        std::this_thread::sleep_for(0.011s);
    }

    radio->Stop();


    delete radio;
    delete usb;
}
