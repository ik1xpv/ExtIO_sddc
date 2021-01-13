#include "CppUnitTestFramework.hpp"

#include "config.h"
#include "fft_mt_r2iq.h"

#include "mipp.h"

#include <chrono>
#include <thread>
#include <functional>

#include "fft_mt_r2iq_impl.hpp"

using namespace std::chrono;

#define ITERATES 100000

namespace {
    struct SIMD_PERF : fft_mt_r2iq {

        nanoseconds run(std::function<void()> callback)
        {
            auto StartTime = high_resolution_clock::now();
            for (int i = 0; i < ITERATES; i++)
                callback();
            auto EndTime = high_resolution_clock::now();

            return nanoseconds(EndTime - StartTime);
        }
     };
}


TEST_CASE(SIMD_PERF, AlignedPerfRun)
{
    auto aligned_input = mipp::vector<int16_t>(transferSamples);
    auto aligned_ouput = mipp::vector<float>(transferSamples);
    auto validate = std::vector<float>(transferSamples);

    auto p = std::rand();
    for(int i = 0; i < transferSamples; i++)
        aligned_input[i] = p * i;

    auto simd_run = run([&]() {
        this->simd_convert_float<false, true>(&aligned_input[0], &aligned_ouput[0], transferSamples);
    });

    auto norm_run = run([&]() {
        this->norm_convert_float<false, true>(&aligned_input[0], &aligned_ouput[0], transferSamples);
    });

    printf("Speed up Ratio: %.2lf\n", norm_run * 1.0 / simd_run);
    CHECK_TRUE(simd_run < norm_run);
}


TEST_CASE(SIMD_PERF, RandPerfRun)
{
    auto aligned_input = mipp::vector<int16_t>(transferSamples);
    auto aligned_ouput = mipp::vector<float>(transferSamples);
    auto validate = std::vector<float>(transferSamples);

    auto p = std::rand();
    auto StartTime = high_resolution_clock::now();
    for(int i = 0; i < transferSamples; i++)
        aligned_input[i] = p * i;

    auto simd_run = run([&]() {
        this->simd_convert_float<true, true>(&aligned_input[0], &aligned_ouput[0], transferSamples);
    });

    auto norm_run = run([&]() {
        this->norm_convert_float<true, true>(&aligned_input[0], &aligned_ouput[0], transferSamples);
    });

    printf("Speed up Ratio: %.2lf\n", norm_run * 1.0 / simd_run);
    CHECK_TRUE(simd_run < norm_run);
}

TEST_CASE(SIMD_PERF, ShiftPerfTest)
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

    auto simd_run = run([&]() {
        this->simd_shift_freq<false>(&dest1[0], &source1[0], &source2[0], 0, Count);
    });

    auto norm_run = run([&]() {
        this->norm_shift_freq<false>(&dest2[0], &source1[0], &source2[0], 0, Count);
    });

    printf("Speed up Ratio: %.2lf\n", norm_run * 1.0 / simd_run);
    CHECK_TRUE(simd_run < norm_run);
}

TEST_CASE(SIMD_PERF, FlipCopyTest)
{
    const int Count = FFTN_R_ADC;
    fftwf_complex source1[Count];
    fftwf_complex dest1[Count];
    fftwf_complex dest2[Count];

    for(int i = 0; i < Count; i++) {
        source1[i][0] = float(std::rand())/32768.0f;
        source1[i][1] = float(std::rand())/32768.0f;
    }

    auto simd_run = run([&]() {
        this->simd_copy<true, false>(&dest1[0], &source1[0], Count);
    });

    auto norm_run = run([&]() {
        this->norm_copy<true, false>(&dest2[0], &source1[0], Count);
    });

    printf("Speed up Ratio: %.2lf\n", norm_run * 1.0 / simd_run);
    CHECK_TRUE(simd_run < norm_run);
}

TEST_CASE(SIMD_PERF, CopyTest)
{
    const int Count = FFTN_R_ADC;
    fftwf_complex source1[Count];
    fftwf_complex dest1[Count];
    fftwf_complex dest2[Count];

    for(int i = 0; i < Count; i++) {
        source1[i][0] = float(std::rand())/32768.0f;
        source1[i][1] = float(std::rand())/32768.0f;
    }

    auto simd_run = run([&]() {
        this->simd_copy<false, false>(&dest1[0], &source1[0], Count);
    });

    auto norm_run = run([&]() {
        this->norm_copy<false, false>(&dest2[0], &source1[0], Count);
    });

    printf("Speed up Ratio: %.2lf\n", norm_run * 1.0 / simd_run);
    CHECK_TRUE(simd_run < norm_run);
}