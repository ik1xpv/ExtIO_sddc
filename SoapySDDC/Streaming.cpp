#include <SoapySDR/Device.hpp>
#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Formats.h>
#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Formats.hpp>

#include <SoapySDR/Time.hpp>
#include <cstdint>
#include <cstring>
#include "SoapySDDC.hpp"

std::vector<std::string> SoapySDDC::getStreamFormats(const int direction, const size_t channel) const
{
    DbgPrintf("SoapySDDC::getStreamFormats\n");
    std::vector<std::string> formats;
    formats.push_back(SOAPY_SDR_CF32);
    return formats;
}

std::string SoapySDDC::getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const
{
    DbgPrintf("SoapySDDC::getNativeStreamFormat\n");
    fullScale = 1.0;
    return SOAPY_SDR_CF32;
}

SoapySDR::ArgInfoList SoapySDDC::getStreamArgsInfo(const int direction, const size_t channel) const
{
    DbgPrintf("SoapySDDC::getStreamArgsInfo\n");
    SoapySDR::ArgInfoList streamArgs;

    return streamArgs;
}

SoapySDR::Stream *SoapySDDC::setupStream(const int direction,
                                         const std::string &format,
                                         const std::vector<size_t> &channels,
                                         const SoapySDR::Kwargs &args)
{
    DbgPrintf("SoapySDDC::setupStream\n");
    if (direction != SOAPY_SDR_RX)
        throw std::runtime_error("setupStream failed: SDDC only supports RX");
    // if (channels.size() != 1) throw std::runtime_error("setupStream failed: SDDC only supports one channel");
    if (format == SOAPY_SDR_CF32)
    {
        SoapySDR_logf(SOAPY_SDR_INFO, "Using format CF32.");
    }
    else
    {
        throw std::runtime_error("setupStream failed: SDDC only supports CF32.");
    }

    bytesPerSample = 8;

    bufferLength = 262144 / bytesPerSample;

    _buf_tail = 0;
    _buf_head = 0;
    _buf_count = 0;

    // allocate buffers
    _buffs.resize(numBuffers);
    for (auto &buff : _buffs)
        buff.reserve(bufferLength * bytesPerSample);
    for (auto &buff : _buffs)
        buff.resize(bufferLength * bytesPerSample);

    // RadioHandler.Init(Fx3, _Callback, nullptr,this);
    // RadioHandler.Start(samplerateidx);

    // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    return (SoapySDR::Stream *)this;
    ;
}

void SoapySDDC::closeStream(SoapySDR::Stream *stream)
{
    DbgPrintf("SoapySDDC::closeStream\n");
    RadioHandler.Stop();
    // RadioHandler.Close();
}

size_t SoapySDDC::getStreamMTU(SoapySDR::Stream *stream) const
{
    DbgPrintf("SoapySDDC::getStreamMTU\n");
    return bufferLength;
}

int SoapySDDC::activateStream(SoapySDR::Stream *stream,
                              const int flags,
                              const long long timeNs,
                              const size_t numElems)
{
    DbgPrintf("SoapySDDC::activateStream %d\n", samplerateidx);
    resetBuffer = true;
    bufferedElems = 0;
    RadioHandler.Start(samplerateidx);

    return 0;
}

int SoapySDDC::deactivateStream(SoapySDR::Stream *stream,
                                const int flags,
                                const long long timeNs)
{
    DbgPrintf("SoapySDDC::deactivateStream\n");
    RadioHandler.Stop();
    return 0;
}

int SoapySDDC::readStream(SoapySDR::Stream *stream,
                          void *const *buffs,
                          const size_t numElems,
                          int &flags,
                          long long &timeNs,
                          const long timeoutUs)
{
    // DbgPrintf("SoapySDDC::readStream\n");
    void *buff0 = buffs[0];
    if (bufferedElems == 0)
    {
        int ret = this->acquireReadBuffer(stream, _currentHandle, (const void **)&_currentBuff, flags, timeNs, timeoutUs);
        if (ret < 0)
            return ret;
        bufferedElems = ret;
    }

    size_t returnedElems = std::min(bufferedElems, numElems);

    // convert into user's buff0
    std::memcpy(buff0, _currentBuff, returnedElems * bytesPerSample);

    // bump variables for next call into readStream
    bufferedElems -= returnedElems;
    _currentBuff += returnedElems * bytesPerSample;

    // return number of elements written to buff0
    if (bufferedElems != 0)
        flags |= SOAPY_SDR_MORE_FRAGMENTS;
    else
        this->releaseReadBuffer(stream, _currentHandle);
    return returnedElems;
}

int SoapySDDC::acquireReadBuffer(SoapySDR::Stream *stream,
                                 size_t &handle,
                                 const void **buffs,
                                 int &flags,
                                 long long &timeNs,
                                 const long timeoutUs)
{
    if (resetBuffer)
    {
        _buf_head = (_buf_head + _buf_count.exchange(0)) % numBuffers;
        resetBuffer = false;
        _overflowEvent = false;
    }

    if (_overflowEvent)
    {
        _buf_head = (_buf_head + _buf_count.exchange(0)) % numBuffers;
        _overflowEvent = false;
        SoapySDR::log(SOAPY_SDR_SSI, "O");
        return SOAPY_SDR_OVERFLOW;
    }
    // wait for a buffer to become available
    if (_buf_count == 0)
    {
        std::unique_lock<std::mutex> lock(_buf_mutex);
        _buf_cond.wait_for(lock, std::chrono::microseconds(timeoutUs), [this]
                           { return _buf_count != 0; });
        if (_buf_count == 0)
            return SOAPY_SDR_TIMEOUT;
    }
    // extract handle and buffer
    handle = _buf_head;
    _buf_head = (_buf_head + 1) % numBuffers;
    buffs[0] = (void *)_buffs[handle].data();
    flags = 0;

    // return number available
    return _buffs[handle].size() / bytesPerSample;
}

void SoapySDDC::releaseReadBuffer(SoapySDR::Stream *stream,
                                  const size_t handle)
{
    // DbgPrintf("SoapySDDC::releaseReadBuffer\n");
    std::lock_guard<std::mutex> lock(_buf_mutex);
    _buf_count--;
}