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

    this->norm_convert_float<false, true>(&aligned_input[0], &validate[0], transferSamples);

    for(int i = 0; i < transferSamples; i++)
        CHECK_CLOSE(validate[i], aligned_ouput[i], 0.001f);
}

TEST_CASE(R2IQ_TEST, UnalignedTest)
{
    auto unaligned_input = std::vector<int16_t>(transferSamples);
    auto unaligned_ouput = std::vector<float>(transferSamples);
    auto validate = std::vector<float>(transferSamples);

    for(int i = 0; i < transferSamples; i++)
        unaligned_input[i] = std::rand();

    this->simd_convert_float<false, false>(&unaligned_input[0], &unaligned_ouput[0], transferSamples);

    this->norm_convert_float<false, true>(&unaligned_input[0], &validate[0], transferSamples);

    for(int i = 0; i < transferSamples; i++)
        CHECK_CLOSE(validate[i], unaligned_ouput[i], 0.001f);
}

TEST_CASE(R2IQ_TEST, AlignedRANDTest)
{
    auto aligned_input = mipp::vector<int16_t>(transferSamples);
    auto aligned_ouput = mipp::vector<float>(transferSamples);
    auto validate = std::vector<float>(transferSamples);

    for(int i = 0; i < transferSamples; i++)
        aligned_input[i] = std::rand();

    this->simd_convert_float<true, true>(&aligned_input[0], &aligned_ouput[0], transferSamples);

    this->norm_convert_float<true, true>(&aligned_input[0], &validate[0], transferSamples);

    for(int i = 0; i < transferSamples; i++)
        CHECK_CLOSE(validate[i], aligned_ouput[i], 0.001f);
}

TEST_CASE(R2IQ_TEST, UnalignedRANDTest)
{
    auto unaligned_input = std::vector<int16_t>(transferSamples);
    auto unaligned_ouput = std::vector<float>(transferSamples);
    auto validate = std::vector<float>(transferSamples);

    for(int i = 0; i < transferSamples; i++)
        unaligned_input[i] = std::rand();

    this->simd_convert_float<true, false>(&unaligned_input[0], &unaligned_ouput[0], transferSamples);

    this->norm_convert_float<true, true>(&unaligned_input[0], &validate[0], transferSamples);

    for(int i = 0; i < transferSamples; i++)
        CHECK_CLOSE(validate[i], unaligned_ouput[i], 0.001f);
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

    this->norm_convert_float<false, true>(&aligned_input[0], &validate[0], size);

    for(int i = 0; i < size; i++)
        CHECK_CLOSE(validate[i], aligned_ouput[i], 0.001f);
}

TEST_CASE(R2IQ_TEST, ShiftTest)
{
    const int Count = 1024;
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

    this->simd_shift_freq<true>(&dest1[0], &source1[0], &source2[0], 0, Count);

    this->norm_shift_freq<true>(&dest2[0], &source1[0], &source2[0], 0, Count);

    for(int i = 0; i < Count; i++) {
        CHECK_CLOSE(dest1[i][0], dest2[i][0], 0.001f);
        CHECK_CLOSE(dest1[i][1], dest2[i][1], 0.001f);
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

    this->simd_shift_freq<false>(&dest1[0], &source1[0], &source2[0], 0, Count);

    this->norm_shift_freq<false>(&dest2[0], &source1[0], &source2[0], 0, Count);

    for(int i = 0; i < Count; i++) {
        CHECK_CLOSE(dest1[i][0], dest2[i][0], 0.001f);
        CHECK_CLOSE(dest1[i][1], dest2[i][1], 0.001f);
    }
}

TEST_CASE(R2IQ_TEST, FullSizeShiftTest)
{
    const int Count = 1024;
    fftwf_complex source1[Count];
    fftwf_complex source2[Count];
    fftwf_complex dest1[Count];
    fftwf_complex dest2[Count];

    memset(dest1, 0, Count * sizeof(fftwf_complex));
    memset(dest2, 0, Count * sizeof(fftwf_complex));

    for(int i = 0; i < Count; i++) {
        source1[i][0] = float(std::rand())/32768.0f;
        source1[i][1] = float(std::rand())/32768.0f;
        source2[i][0] = float(std::rand())/32768.0f;
        source2[i][1] = float(std::rand())/32768.0f;
    }

    this->simd_shift_freq<false>(&dest1[0], &source1[0], &source2[0], 7, Count - 8);

    this->norm_shift_freq<false>(&dest2[0], &source1[0], &source2[0], 7, Count - 8);

    for(int i = 0; i < Count; i++) {
        CHECK_CLOSE(dest1[i][0], dest2[i][0], 0.001f);
        CHECK_CLOSE(dest1[i][1], dest2[i][1], 0.001f);
    }
}

TEST_CASE(R2IQ_TEST, NonFlipCopyTest)
{
    const int Count = 1024;
    fftwf_complex source1[Count];
    fftwf_complex dest1[Count];
    fftwf_complex dest2[Count];

    memset(dest1, 0, Count * sizeof(fftwf_complex));
    memset(dest2, 0, Count * sizeof(fftwf_complex));

    for(int i = 0; i < Count; i++) {
        source1[i][0] = float(std::rand())/32768.0f;
        source1[i][1] = float(std::rand())/32768.0f;
    }

    this->simd_copy<false, false>(&dest1[0], &source1[0], Count - 17);

    this->norm_copy<false, false>(&dest2[0], &source1[0], Count - 17);

    for(int i = 0; i < Count; i++) {
        CHECK_CLOSE(dest1[i][0], dest2[i][0], 0.001f);
        CHECK_CLOSE(dest1[i][1], dest2[i][1], 0.001f);
    }
}

TEST_CASE(R2IQ_TEST, FlipCopyTest)
{
    const int Count = 1024;
    fftwf_complex source1[Count];
    fftwf_complex dest1[Count];
    fftwf_complex dest2[Count];

    memset(dest1, 0, Count * sizeof(fftwf_complex));
    memset(dest2, 0, Count * sizeof(fftwf_complex));

    for(int i = 0; i < Count; i++) {
        source1[i][0] = float(std::rand())/32768.0f;
        source1[i][1] = float(std::rand())/32768.0f;
    }

    this->simd_copy<true, false>(&dest1[0], &source1[0], Count - 17);

    this->norm_copy<true, false>(&dest2[0], &source1[0], Count - 17);

    for(int i = 0; i < Count; i++) {
        CHECK_CLOSE(dest1[i][0], dest2[i][0], 0.001f);
        CHECK_CLOSE(dest1[i][1], dest2[i][1], 0.001f);
    }
}