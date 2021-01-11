#include "CppUnitTestFramework.hpp"

#include "config.h"
#include "fft_mt_r2iq.h"

#include "mipp.h"

#include <chrono>
#include <thread>
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