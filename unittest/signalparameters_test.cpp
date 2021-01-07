#include "dsp/signalparameters.h"

#include "CppUnitTestFramework.hpp"

namespace {
    class SignalFixture : public SignalParameters {};
}

TEST_CASE(SignalFixture, BasicTest)
{
    auto *signal = new SignalParameters();
    delete signal;
}

TEST_CASE(SignalFixture, BasicTest2)
{
    this->setDecimate(1);

    REQUIRE_EQUAL(1, m_decimation);

    this->setDecimate(2);
    REQUIRE_EQUAL(2, m_decimation);
    REQUIRE_EQUAL(1024, m_fft);
    REQUIRE_EQUAL(4, m_ratio);

    this->setFreqOffset(0.5);
    this->setDecimate(1);
    REQUIRE_EQUAL(2048, m_tunebin);

}
