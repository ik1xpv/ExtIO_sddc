#include "CppUnitTestFramework.hpp"

#include "config.h"
#include "fft_mt_r2iq.h"

#include "mipp.h"

#include <chrono>
#include <thread>

#include "fft_mt_r2iq_impl.hpp"

using namespace std::chrono;

namespace {
    struct R2IQ_TEST : fft_mt_r2iq {
     };
}

TEST_CASE(R2IQ_TEST, AlignedTest)
{
    auto aligned_input = mipp::vector<int16_t>(transferSamples);
    auto aligned_ouput = mipp::vector<float>(transferSamples);
    auto validate = std::vector<float>(transferSamples);

    for(int i = 0; i < transferSamples; i++)
        aligned_input[i] = std::rand();

    this->simd_convert_float<false, true>(&aligned_input[0], &aligned_ouput[0], transferSamples);

    this->norm_convert_float<false>(&aligned_input[0], &validate[0], transferSamples);

    for(int i = 0; i < transferSamples; i++)
        CHECK_EQUAL(validate[i], aligned_ouput[i]);
}

TEST_CASE(R2IQ_TEST, UnalignedTest)
{
    auto unaligned_input = std::vector<int16_t>(transferSamples);
    auto unaligned_ouput = std::vector<float>(transferSamples);
    auto validate = std::vector<float>(transferSamples);

    for(int i = 0; i < transferSamples; i++)
        unaligned_input[i] = std::rand();

    this->simd_convert_float<false, false>(&unaligned_input[0], &unaligned_ouput[0], transferSamples);

    this->norm_convert_float<false>(&unaligned_input[0], &validate[0], transferSamples);

    for(int i = 0; i < transferSamples; i++)
        CHECK_EQUAL(validate[i], unaligned_ouput[i]);
}

TEST_CASE(R2IQ_TEST, AlignedRANDTest)
{
    auto aligned_input = mipp::vector<int16_t>(transferSamples);
    auto aligned_ouput = mipp::vector<float>(transferSamples);
    auto validate = std::vector<float>(transferSamples);

    for(int i = 0; i < transferSamples; i++)
        aligned_input[i] = std::rand();

    this->simd_convert_float<true, true>(&aligned_input[0], &aligned_ouput[0], transferSamples);

    this->norm_convert_float<true>(&aligned_input[0], &validate[0], transferSamples);

    for(int i = 0; i < transferSamples; i++)
        CHECK_EQUAL(validate[i], aligned_ouput[i]);

}

TEST_CASE(R2IQ_TEST, UnalignedRANDTest)
{
    auto unaligned_input = std::vector<int16_t>(transferSamples);
    auto unaligned_ouput = std::vector<float>(transferSamples);
    auto validate = std::vector<float>(transferSamples);

    for(int i = 0; i < transferSamples; i++)
        unaligned_input[i] = std::rand();

    this->simd_convert_float<true, false>(&unaligned_input[0], &unaligned_ouput[0], transferSamples);

    this->norm_convert_float<true>(&unaligned_input[0], &validate[0], transferSamples);

    for(int i = 0; i < transferSamples; i++)
        CHECK_EQUAL(validate[i], unaligned_ouput[i]);
}

TEST_CASE(R2IQ_TEST, UnalignedSizeTest)
{
    const int size = transferSamples + 7;
    auto aligned_input = mipp::vector<int16_t>(size);
    auto aligned_ouput = mipp::vector<float>(size);
    auto validate = std::vector<float>(size);

    for(int i = 0; i < size; i++)
        aligned_input[i] = std::rand();

    this->simd_convert_float<false, true>(&aligned_input[0], &aligned_ouput[0], size);

    this->norm_convert_float<false>(&aligned_input[0], &validate[0], size);

    for(int i = 0; i < size; i++)
        CHECK_EQUAL(validate[i], aligned_ouput[i]);
}

#ifdef NDEBUG

TEST_CASE(R2IQ_TEST, AlignedPerfRun)
{
    auto aligned_input = mipp::vector<int16_t>(transferSamples);
    auto aligned_ouput = mipp::vector<float>(transferSamples);
    auto validate = std::vector<float>(transferSamples);

    auto p = std::rand();
    auto StartTime = high_resolution_clock::now();
    for (volatile int i = 0; i < 50000; i++)
    {
        for(int i = 0; i < transferSamples; i++)
            aligned_input[i] = p * i;

        this->simd_convert_float<false, true>(&aligned_input[0], &aligned_ouput[0], transferSamples);
    }

    auto EndTime = high_resolution_clock::now();
    for (volatile int i = 0; i < 50000; i++)
    {
        for(int i = 0; i < transferSamples; i++)
            aligned_input[i] = p * i;

        this->norm_convert_float<false>(&aligned_input[0], &aligned_ouput[0], transferSamples);
    }
    auto FinsihTime = high_resolution_clock::now();

    printf("Speed up Ratio: %.2lf\n", nanoseconds(FinsihTime - EndTime) *1.0f / nanoseconds(EndTime - StartTime));
    REQUIRE_TRUE(nanoseconds(EndTime - StartTime) < nanoseconds(FinsihTime - EndTime));
}


TEST_CASE(R2IQ_TEST, RandPerfRun)
{
    auto aligned_input = mipp::vector<int16_t>(transferSamples);
    auto aligned_ouput = mipp::vector<float>(transferSamples);
    auto validate = std::vector<float>(transferSamples);

    auto p = std::rand();
    auto StartTime = high_resolution_clock::now();
    for (volatile int i = 0; i < 50000; i++)
    {
        for(int i = 0; i < transferSamples; i++)
            aligned_input[i] = p * i;

        this->simd_convert_float<true, true>(&aligned_input[0], &aligned_ouput[0], transferSamples);
    }

    auto EndTime = high_resolution_clock::now();
    for (volatile int i = 0; i < 50000; i++)
    {
        for(int i = 0; i < transferSamples; i++)
            aligned_input[i] = p * i;

        this->norm_convert_float<true>(&aligned_input[0], &aligned_ouput[0], transferSamples);
    }
    auto FinsihTime = high_resolution_clock::now();

    printf("Speed up Ratio: %.2lf\n", nanoseconds(FinsihTime - EndTime) *1.0f / nanoseconds(EndTime - StartTime));
    REQUIRE_TRUE(nanoseconds(EndTime - StartTime) < nanoseconds(FinsihTime - EndTime));
}

TEST_CASE(R2IQ_TEST, ShiftPerfTest)
{
    const int Count = FFTN_R_ADC;
    fftwf_complex source1[Count];
    fftwf_complex source2[Count];
    fftwf_complex dest1[Count];
    fftwf_complex dest2[Count];

    for(int i = 0; i < Count; i++) {
        source1[i][0] = float(std::rand());
        source1[i][1] = float(std::rand());
        source2[i][0] = float(std::rand());
        source2[i][1] = float(std::rand());
    }

    auto p = float(std::rand());
    auto StartTime = high_resolution_clock::now();
    for (volatile int i = 0; i < 50000; i++)
    {
        for(int i = 0; i < Count; i++) {
            source1[i][0] = float(p * i);
        }

        this->simd_shift_freq(&dest1[0], &source1[0], &source2[0], 0, Count);
    }

    auto EndTime = high_resolution_clock::now();
    for (volatile int i = 0; i < 50000; i++)
    {
        for(int i = 0; i < Count; i++)
        {
            source1[i][0] = float(p * i);
        }

        this->norm_shift_freq(&dest2[0], &source1[0], &source2[0], 0, Count);
    }
    auto FinsihTime = high_resolution_clock::now();

    printf("Speed up Ratio: %.2lf\n", nanoseconds(FinsihTime - EndTime) *1.0f / nanoseconds(EndTime - StartTime));
    REQUIRE_TRUE(nanoseconds(EndTime - StartTime) < nanoseconds(FinsihTime - EndTime));

}
#endif

TEST_CASE(R2IQ_TEST, ShiftTest)
{
    const int Count = 16;
    fftwf_complex source1[Count];
    fftwf_complex source2[Count];
    fftwf_complex dest1[Count];
    fftwf_complex dest2[Count];

    for(int i = 0; i < Count; i++) {
        source1[i][0] = float(std::rand())/32768.0f;
        source1[i][1] = float(std::rand())/32768.0f;
        source2[i][0] = float(std::rand())/32768.0f;
        source2[i][1] = float(std::rand())/32768.0f;
    }

    this->simd_shift_freq(&dest1[0], &source1[0], &source2[0], 0, Count);

    this->norm_shift_freq(&dest2[0], &source1[0], &source2[0], 0, Count);

    for(int i = 0; i < Count; i++) {
        CHECK_TRUE(fabs(dest1[i][0] - dest2[i][0]) < 0.0001f);
        CHECK_TRUE(fabs(dest1[i][1] - dest2[i][1]) < 0.0001f);
    }
}

TEST_CASE(R2IQ_TEST, OddSizeShiftTest)
{
    const int Count = 27;
    fftwf_complex source1[Count];
    fftwf_complex source2[Count];
    fftwf_complex dest1[Count];
    fftwf_complex dest2[Count];

    for(int i = 0; i < Count; i++) {
        source1[i][0] = float(std::rand())/32768.0f;
        source1[i][1] = float(std::rand())/32768.0f;
        source2[i][0] = float(std::rand())/32768.0f;
        source2[i][1] = float(std::rand())/32768.0f;
    }

    this->simd_shift_freq(&dest1[0], &source1[0], &source2[0], 0, Count);

    this->norm_shift_freq(&dest2[0], &source1[0], &source2[0], 0, Count);

    for(int i = 0; i < Count; i++) {
        CHECK_TRUE(fabs(dest1[i][0] - dest2[i][0]) < 0.0001f);
        CHECK_TRUE(fabs(dest1[i][1] - dest2[i][1]) < 0.0001f);
    }
}