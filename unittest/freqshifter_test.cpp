#include "dsp/ringbuffer.h"
#include "dsp/freqShifter.h"
#include "dsp/converter.h"
#include "dsp/freqConverter.h"

#include "CppUnitTestFramework.hpp"

namespace {
    struct FreqShifterFixture {};
}

TEST_CASE(FreqShifterFixture, BasicTest)
{
    auto input = ringbuffer<fftwf_complex>(128);
    FreqShifter shifter(&input);
    input.setBlockSize(FFTN_R_ADC);

    shifter.start();

    shifter.stop();
}


TEST_CASE(FreqShifterFixture, End2EndTest)
{
    auto input = ringbuffer<int16_t>(8);

    converter conv(&input);
    FreqConverter r2c(conv.getOutput());
    FreqShifter shifter(r2c.getOutput());
    FreqBackConverter c2c(shifter.getOutput());

    input.setBlockSize(transferSize / sizeof(int16_t));

    conv.start();
    r2c.start();
    shifter.start();
    c2c.start();

    int count = 5 * 1024;
    auto thread1 = std::thread([&input, count] {
        for (int i = 0; i < count; i++) {
            auto *ptr = input.getWritePtr();
            ptr[0] = 0x55;
            input.WriteDone();
        }
    });

    auto timeoutput = c2c.getOutput();
    auto thread2 = std::thread([&timeoutput, count, this] {
        for (int j = 0; j < count * 8; j++) {
            auto *ptr = timeoutput->getReadPtr();
            timeoutput->ReadDone();
        }
    });

    thread1.join();
    thread2.join();

    c2c.stop();
    shifter.stop();
    r2c.stop();
    conv.stop();

    printf("buffer0 write:%d full:%d empty:%d\n", input.getWriteCount(), input.getFullCount(), input.getEmptyCount());
    auto floatoutput = conv.getOutput();
    printf("buffer1 write:%d full:%d empty:%d\n", floatoutput->getWriteCount(), floatoutput->getFullCount(), floatoutput->getEmptyCount());
    auto freqoutput = r2c.getOutput();
    printf("buffer2 write:%d full:%d empty:%d\n", freqoutput->getWriteCount(), freqoutput->getFullCount(), freqoutput->getEmptyCount());
    auto shiftoutput = shifter.getOutput();
    printf("buffer3 write:%d full:%d empty:%d\n", shiftoutput->getWriteCount(), shiftoutput->getFullCount(), shiftoutput->getEmptyCount());
    printf("buffer4 write:%d full:%d empty:%d\n", timeoutput->getWriteCount(), timeoutput->getFullCount(), timeoutput->getEmptyCount());
}
