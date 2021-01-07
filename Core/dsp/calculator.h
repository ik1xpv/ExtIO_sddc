#pragma once

#include "ringbuffer.h"
#include "signalparameters.h"
#include <chrono>
#include <thread>

using namespace std::chrono;

template<typename TINPUT, typename TOUTPUT> class calculator: public SignalParameters {
public:
    // start the thread
    void start() {
        setInputBlockSize(input->getBlockSize());
        run = true;
        timesum = 0s;
        count = 0;
        thread = std::thread(
            [this]()
            {
                while(run){
                    auto starttime = high_resolution_clock::now();
                    count++;
                    process();

                    timesum += high_resolution_clock::now() - starttime;
                }
            });
    }

    void stop() {
        run = false;
        input->Stop();
        if (output)
            output->Stop();
        thread.join();
    }

    float getProcessTime() const {
        return timesum.count() / count;
    }

    virtual void setInputBlockSize(int inputSize)
    {
        InputBlockSize = inputSize;
        if (output)
            output->setBlockSize(inputSize);
    }

    int getInputBlockSize() const { return InputBlockSize; }

    ringbuffer<TOUTPUT> *getOutput() { return output; }

protected:
    calculator(ringbuffer<TINPUT> *_input) :
        input(_input)
    {
        output = new ringbuffer<TOUTPUT>();
    }

    virtual void process() = 0;

    ringbuffer<TINPUT> *input;
    ringbuffer<TOUTPUT> *output;

    int InputBlockSize;
private:
    std::thread thread;
    duration<float> timesum;
    int count;
    bool run;
};