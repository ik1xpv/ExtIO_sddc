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
        output_block() : output(1024) {}
        ringbuffer<TOUT> *getOutput(){return &this->output;};

    protected:
        ringbuffer<TOUT> output;
    };

    template <typename TIN, typename TOUT>
    class dsp_block : public input_block<TIN>, public output_block<TOUT>
    {
    protected:
        ~dsp_block() {}

    public:
        virtual void Start()
        {
            Stop(); // make sure we really stopped
            running = true;
            this->input->Start();
            this->output.Start();
            process_thread = std::thread(
                [this]() {
                    this->DataProcessor();
                });
        }

        virtual void Stop()
        {
            running = false;
            this->input->Stop();
            this->output.Stop();
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
