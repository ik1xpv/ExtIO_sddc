#include "config.h"
#include "freq2iq.h"
#include "simd.h"

#include "fir.h"

static const int fftPerBuf = transferSize / sizeof(short) / (3 * halfFft / 2) + 1; // number of ffts per buffer with 256|768 overlap

freq2iq::freq2iq(float gain, int channel_num)
{
    channels.resize(channel_num);
    for (int i = 0; i < channel_num; i++)
    {
        channels[i].enabled = false;
        channels[i].output = nullptr;
    }

    mfftdim[0] = halfFft;
    for (int i = 1; i < NDECIDX; i++)
    {
        mfftdim[i] = mfftdim[i - 1] / 2;
    }

    mratio[0] = 1; // 1,2,4,8,16
    for (int i = 1; i < NDECIDX; i++)
    {
        mratio[i] = mratio[i - 1] * 2;
    }

    InitFilter(gain);
}

freq2iq::~freq2iq()
{
    for (size_t i = 0; i < channels.size(); i++)
    {
        if (channels[i].output)
            delete channels[i].output;
    }

    fftwf_free(filterHw);
}

void freq2iq::InitFilter(float gain)
{
    // filters
    fftwf_complex *pfilterht;                                                   // time filter ht
    pfilterht = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * halfFft); // halfFft
    filterHw = (fftwf_complex **)fftwf_malloc(sizeof(fftwf_complex *) * NDECIDX);
    for (int d = 0; d < NDECIDX; d++)
    {
        filterHw[d] = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * halfFft); // halfFft
    }

    fftwf_plan filterplan_t2f_c2c = fftwf_plan_dft_1d(halfFft, pfilterht, filterHw[0], FFTW_FORWARD, FFTW_MEASURE);
    float *pht = new float[halfFft / 4 + 1];
    const float Astop = 120.0f;
    const float relPass = 0.85f; // 85% of Nyquist should be usable
    const float relStop = 1.1f;  // 'some' alias back into transition band is OK

    fftwf_complex *inFreqTmp = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * (halfFft)); // 1024

    for (int d = 0; d < NDECIDX; d++) // @todo when increasing NDECIDX
    {
        // @todo: have dynamic bandpass filter size - depending on decimation
        //   to allow same stopband-attenuation for all decimations
        float Bw = 64.0f / mratio[d];
        // Bw *= 0.8f;  // easily visualize Kaiser filter's response
        KaiserWindow(halfFft / 4 + 1, Astop, relPass * Bw / 128.0f, relStop * Bw / 128.0f, pht);

        float gainadj = gain * 2048.0f / (float)FFTN_R_ADC; // reference is FFTN_R_ADC == 2048

        for (int t = 0; t < halfFft; t++)
        {
            pfilterht[t][0] = pfilterht[t][1] = 0.0F;
        }

        for (int t = 0; t < (halfFft / 4 + 1); t++)
        {
            pfilterht[halfFft - 1 - t][0] = gainadj * pht[t];
        }

        fftwf_execute_dft(filterplan_t2f_c2c, pfilterht, filterHw[d]);

        // Create iFFT
        plans_f2t_c2c[d] = fftwf_plan_dft_1d(mfftdim[d], inFreqTmp, inFreqTmp, FFTW_BACKWARD, FFTW_MEASURE);
    }
    delete[] pht;
    fftwf_free(inFreqTmp);
    fftwf_destroy_plan(filterplan_t2f_c2c);
    fftwf_free(pfilterht);
}

#define round(number) ((number > 0.0f) ? floorf(number + 0.5f) : ceilf(number - 0.5f))

bool freq2iq::SetChannel(unsigned channel, int decimate, float offsetbin, bool sideband)
{
    if (channel >= channels.size())
        return false;

    ChannelType &chan = channels[channel];

    if (decimate < 0)
    {
        chan.enabled = false;
        return false;
    }
    else
    {
        chan.decimation = decimate;
        chan.tunebin = round(offsetbin);
        chan.sideband = sideband;

        chan.fc = offsetbin - chan.tunebin;
        if (chan.output == nullptr)
            chan.output = new ringbuffer<float>();

        chan.output->setBlockSize(EXT_BLOCKLEN * 2 * sizeof(float));
        chan.enabled = true;

        if (chan.fc != 0)
        {
            chan.stateFineTune = shift_limited_unroll_C_sse_init(chan.fc, 0.0F);
        }
        return true;
    }
}

bool freq2iq::GetChannel(unsigned channel, int &decimate, float &offsetbin, bool &sideband)
{
    if (channel >= channels.size())
        return false;

    if (!channels[channel].enabled)
        return false;

    decimate = channels[channel].decimation;
    offsetbin = channels[channel].tunebin;
    sideband = channels[channel].sideband;

    return true;
}

void freq2iq::DataProcessor()
{
    fftwf_complex *inFreqTmp; // tmp decimation output buffers (after tune shift)

    inFreqTmp = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * (halfFft)); // 1024

    fftwf_complex *pout = nullptr;

    while (this->isRunning())
    {
        for (int k = 0; k < fftPerBuf; k++)
        {
            const fftwf_complex *ADCinFreq; // buffers in frequency

            ADCinFreq = input->getReadPtr();

            if (!this->isRunning())
                return;

            for (size_t i = 0; i < channels.size(); i++)
            {
                auto c = channels[i];

                if (!c.enabled)
                    continue;

                const int decimate = c.decimation;
                const int mfft = this->mfftdim[decimate]; // = halfFft / 2^mdecimation
                const fftwf_complex *filter = filterHw[decimate];
                const bool lsb = c.sideband;
                const auto filter2 = &filter[halfFft - mfft / 2];
                const int mtunebin = c.tunebin; // Update LO tune is possible during run
                auto plan_f2t_c2c = plans_f2t_c2c[decimate];

                if (c.decimate_count == 0)
                    c.pout = (fftwf_complex *)c.output->getWritePtr();

                c.decimate_count = (c.decimate_count + 1) & ((1 << decimate) - 1);
                const auto count = std::min(mfft / 2, halfFft - mtunebin);
                const auto start = std::max(0, mfft / 2 - mtunebin);
                const auto dest = &inFreqTmp[mfft / 2];

                // Calculate the parameters for the first half
                const auto source = &ADCinFreq[mtunebin];

                // Calculate the parameters for the second half
                const auto source2 = &ADCinFreq[mtunebin - mfft / 2];
                // circular shift (mixing in full bins) and low/bandpass filtering (complex multiplication)
                {
                    // circular shift tune fs/2 first half array into inFreqTmp[]
                    shift_freq(inFreqTmp, source, filter, 0, count);
                    if (mfft / 2 != count)
                        memset(inFreqTmp[count], 0, sizeof(float) * 2 * (mfft / 2 - count));

                    // circular shift tune fs/2 second half array
                    shift_freq(dest, source2, filter2, start, mfft / 2);
                    if (start != 0)
                        memset(inFreqTmp[mfft / 2], 0, sizeof(float) * 2 * start);
                }
                // result now in inFreqTmp[]

                // 'shorter' inverse FFT transform (decimation); frequency (back) to COMPLEX time domain
                // transform size: mfft = mfftdim[k] = halfFft / 2^k with k = mdecimation
                fftwf_execute_dft(plan_f2t_c2c, inFreqTmp, inFreqTmp); //  c2c decimation
                                                                       // result now in inFreqTmp[]

                // postprocessing
                // @todo: is it possible to ..
                //  1)
                //    let inverse FFT produce/save it's result directly
                //    in "this->obuffers[modx] + offset" (pout)
                //    ( obuffers[] would need to have additional space ..;
                //      need to move 'scrap' of 'ovelap-scrap'? )
                //    at least FFTW would allow so,
                //      see http://www.fftw.org/fftw3_doc/New_002darray-Execute-Functions.html
                //    attention: multithreading!
                //  2)
                //    could mirroring (lower sideband) get calculated together
                //    with fine mixer - modifying the mixer frequency? (fs - fc)/fs
                //    (this would reduce one memory pass)
                if (lsb) // lower sideband
                {
                    // mirror just by negating the imaginary Q of complex I/Q
                    if (k == 0)
                    {
                        copy<true>(c.pout, &inFreqTmp[mfft / 4], mfft / 2);
                        c.pout += mfft / 2;
                    }
                    else
                    {
                        copy<true>(c.pout + mfft / 2 + (3 * mfft / 4) * (k - 1), &inFreqTmp[0], (3 * mfft / 4));
                        c.pout += 3 * mfft / 4;
                    }
                }
                else // upper sideband
                {
                    if (k == 0)
                    {
                        copy<false>(c.pout, &inFreqTmp[mfft / 4], mfft / 2);
                        c.pout += mfft / 2;
                    }
                    else
                    {
                        copy<false>(c.pout + mfft / 2 + (3 * mfft / 4) * (k - 1), &inFreqTmp[0], (3 * mfft / 4));
                        c.pout += 3 * mfft / 4;
                    }
                }
                // result now in this->obuffers[]

                if (c.decimate_count == 0)
                {
                    if (c.fc != 0)
                    {
                        // fine tune
                        shift_limited_unroll_C_sse_inp_c((complexf *)c.output->getWritePtr(), mfft / 2 + (3 * mfft / 4) * fftPerBuf, &c.stateFineTune);
                    }
                    c.output->WriteDone();
                }
            } // for

            input->ReadDone();
        } // while(run)
          //    DbgPrintf((char *) "r2iqThreadf idx %d pthread_exit %u\n",(int)this->t, pthread_self());

        fftwf_free(inFreqTmp);

        return;
    }
}