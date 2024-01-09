#include "../Core/include/ringbuffer.h"

#include "CppUnitTestFramework.hpp"
#include <thread>
#include <chrono>

using namespace std::chrono;

namespace {
    struct RingBufferFixture {};
}

TEST_CASE(RingBufferFixture, BasicTest)
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

TEST_CASE(RingBufferFixture, TwoThreadsTest)
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


TEST_CASE(RingBufferFixture, TwoThreadsLargeBufferTest)
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


TEST_CASE(RingBufferFixture, TwoThreadsTwoBufferTest)
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


TEST_CASE(RingBufferFixture, StopTest)
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

TEST_CASE(RingBufferFixture, PeekTest)
{
    auto buffer = ringbuffer<int16_t>(128);

    auto wptr0 = buffer.getWritePtr();
    auto wptr1 = buffer.peekWritePtr(0);

    CHECK_EQUAL(wptr0, wptr1);

    buffer.WriteDone();

    auto wptr2 = buffer.peekWritePtr(-1);
    CHECK_EQUAL(wptr0, wptr2);

    auto rptr0 = buffer.getReadPtr();
    auto rptr1 = buffer.peekReadPtr(0);

    CHECK_EQUAL(rptr0, rptr1);

    buffer.ReadDone();

    auto rptr2 = buffer.peekReadPtr(-1);
    CHECK_EQUAL(rptr0, rptr2);
}