#include "dsp/ringbuffer.h"

#include "CppUnitTestFramework.hpp"
#include <thread>
#include <chrono>

using namespace std::chrono;

namespace {
    struct RingBufferFixture {};
}

TEST_CASE(RingBufferFixture, RingBuffer_BasicTest)
{
    auto *ringbuf = new ringbuffer<int16_t>(128);
    ringbuf->setBlockSize(128 * 1024);

    REQUIRE_EQUAL(ringbuf->getBlockSize(), 128*1024);
    auto ptr = ringbuf->getWritePtr();
    memset(ptr, 0x5A, ringbuf->getBlockSize() * sizeof(int16_t));
    ringbuf->WriteDone();
    auto ptr2 = ringbuf->getReadPtr();
    REQUIRE_EQUAL(*ptr2, 0x5a5a);
    REQUIRE_EQUAL(*(ptr2 + 0x100), 0x5a5a);
    delete ptr;
}

TEST_CASE(RingBufferFixture, RingBuffer_TwoThreadsTest)
{
    auto buffer = ringbuffer<int16_t>(2);
    buffer.setBlockSize(1024);
    auto thread1 = std::thread(
        [&buffer](){
            for(int i = 0; i < 1000; i++) {
                auto ptr = buffer.getWritePtr();
                memset(ptr, 0x5A, buffer.getBlockSize());
                buffer.WriteDone();
            }
        }
    );

    auto thread2 = std::thread(
        [&buffer, this](){
            for(int i = 0; i < 1000; i++) {
                auto ptr = buffer.getReadPtr();
                REQUIRE_EQUAL(*ptr, 0x5A5A);
                buffer.ReadDone();
            }
        }
    );

    thread1.join();
    thread2.join();
}


TEST_CASE(RingBufferFixture, RingBuffer_TwoThreadsLargeBufferTest)
{
    auto buffer = ringbuffer<int16_t>(128);
    buffer.setBlockSize(1024);
    auto thread1 = std::thread(
        [&buffer](){
            for(int i = 0; i < 1000; i++) {
                auto ptr = buffer.getWritePtr();
                memset(ptr, 0x5A, buffer.getBlockSize());
                buffer.WriteDone();
            }
        }
    );

    auto thread2 = std::thread(
        [&buffer, this](){
            for(int i = 0; i < 1000; i++) {
                auto ptr = buffer.getReadPtr();
                REQUIRE_EQUAL(*ptr, 0x5A5A);
                buffer.ReadDone();
            }
        }
    );

    thread1.join();
    thread2.join();
}


TEST_CASE(RingBufferFixture, RingBuffer_TwoThreadsTwoBufferTest)
{
    auto buffer = ringbuffer<int16_t>(2);
    buffer.setBlockSize(1024);
    auto thread1 = std::thread(
        [&buffer](){
            for(int i = 0; i < 1000; i++) {
                auto ptr = buffer.getWritePtr();
                memset(ptr, 0x5A, buffer.getBlockSize());
                buffer.WriteDone();
            }
        }
    );

    auto thread2 = std::thread(
        [&buffer, this](){
            for(int i = 0; i < 1000; i++) {
                auto ptr = buffer.getReadPtr();
                REQUIRE_EQUAL(*ptr, 0x5A5A);
                buffer.ReadDone();
            }
        }
    );

    thread1.join();
    thread2.join();
}


TEST_CASE(RingBufferFixture, RingBuffer_StopTest)
{
    bool run = true;
    auto buffer = ringbuffer<int16_t>(128);
    buffer.setBlockSize(1024);
    auto thread1 = std::thread(
        [&buffer, &run, this](){
            for(int i = 0; i < 1000 && run; i++) {
                auto ptr = buffer.getReadPtr();
                if (!run) break;
                REQUIRE_EQUAL(*ptr, 0x5A5A);
                buffer.ReadDone();
            }
        }
    );

    auto thread2 = std::thread(
        [&buffer, &run, this](){
            for(int i = 0; i < 1000 && run; i++) {
                auto ptr = buffer.getReadPtr();
                if (!run) break;
                REQUIRE_EQUAL(*ptr, 0x5A5A);
                buffer.ReadDone();
            }
        }
    );
    auto thread3 = std::thread(
        [&thread1, &thread2](){
            thread1.join();
            thread2.join();
        });

    std::this_thread::sleep_for(1s);

    run = false;
    buffer.Stop();
    thread3.join();
}
