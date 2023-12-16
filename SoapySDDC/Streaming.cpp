#include <SoapySDR/Device.hpp>
#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Formats.h>
#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Formats.hpp>

#include <SoapySDR/Time.hpp>

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
    if (direction != SOAPY_SDR_RX) throw std::runtime_error("setupStream failed: SDDC only supports RX");
    if (channels.size() != 1) throw std::runtime_error("setupStream failed: SDDC only supports one channel");
    if (format == SOAPY_SDR_CF32) {
        SoapySDR_logf(SOAPY_SDR_INFO, "Using format CF32.");
    } else {
        throw std::runtime_error("setupStream failed: SDDC only supports CF32.");
    }

    bytesPerSample = SoapySDR::formatToSize(format);

    RadioHandler.Start(0);
    return (SoapySDR::Stream *) this;;
}

int SoapySDDC::activateStream(SoapySDR::Stream *stream,
                             const int flags,
                              const long long timeNs,
                               const size_t numElems)
{
    DbgPrintf("SoapySDDC::activateStream\n");
    
    return 0;
}

int SoapySDDC::readStream(SoapySDR::Stream *stream,
                          void * const *buffs,
                           const size_t numElems,
                            int &flags,
                             long long &timeNs,
                              const long timeoutUs)
{
    DbgPrintf("SoapySDDC::readStream\n");
    
    return numElems;
}