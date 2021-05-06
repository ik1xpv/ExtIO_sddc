#include "CppUnitTestFramework.hpp"
#include <thread>
#include <chrono>
#include "dsp/r2freq.h"
#include "dsp/freq2iq.h"

using namespace std::chrono;

namespace {
    struct DspFixture {};
}

TEST_CASE(DspFixture, BasicTest1)
{
    ringbuffer<int16_t> input;
    input.setBlockSize(transferSize / sizeof(int16_t));
    r2freq c;
    c.Initialize(&input);

    c.Start();
    input.WriteDone();
    input.WriteDone();
    c.Stop();
}

TEST_CASE(DspFixture, BasicTest2)
{
    ringbuffer<fftwf_complex> input;
    input.setBlockSize(sizeof(fftwf_complex) * (halfFft + 1));
    freq2iq c(0.01f, 3);
    c.Initialize(&input);

    c.SetChannel(0, 1, 0.001f, true);
    c.SetChannel(1, 0, -0.003f, false);
    c.SetChannel(2, 8, 0.025f, false);
    c.Start();

    input.WriteDone();
    input.WriteDone();
    std::this_thread::sleep_for(1s);
    c.Stop();

}
