#pragma once

#include "ringbuffer.h"

// repsent the dsp modules
namespace dsp
{
    template <typename TIN>
    class input_block
    {
    public:
        void Initialize(ringbuffer<TIN> *input)
        {
            this->input = input;
        }

    protected:
        input_block() : input(nullptr) {}
        ringbuffer<TIN> *input;
    };

    template <typename TOUT>
    class output_block
    {
    public:
        ringbuffer<TOUT> *getOutput(){return &this->output;};

    protected:
        ringbuffer<TOUT> output;
    };

    template <typename TIN, typename TOUT>
    class dsp_block : public input_block<TIN>, public output_block<TOUT>
    {
    public:
        virtual void Start()
        {
            running = true;
            process_thread = std::thread(
                [this]() {
                    this->DataProcessor();
                });
        }

        void Stop()
        {
            input->Stop();
            output.Stop();
            running = false;
            if (process_thread.joinable())
                process_thread.join();
        }

        virtual void DataProcessor() = 0;

        bool isRunning() const { return running; }

    private:
        std::thread process_thread;
        bool running;
    };
}
