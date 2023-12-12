#include "SoapySDDC.hpp"
#include <SoapySDR/Types.hpp>
#include <SoapySDR/Time.hpp>

class SoapySDDC;

SoapySDDC::SoapySDDC(const SoapySDR::Kwargs &args)
{
    DbgPrintf("SoapySDDC::SoapySDDC\n");
}

SoapySDDC::~SoapySDDC(void)
{
    DbgPrintf("SoapySDDC::~SoapySDDC\n");
}

std::string SoapySDDC::getDriverKey(void) const
{
    DbgPrintf("SoapySDDC::getDriverKey\n");
    return "SDDC";
}

std::string SoapySDDC::getHardwareKey(void) const
{
    DbgPrintf("SoapySDDC::getHardwareKey\n");
    return "SDDC";
}

SoapySDR::Kwargs SoapySDDC::getHardwareInfo(void) const
{
    DbgPrintf("SoapySDDC::getHardwareInfo\n");
    return SoapySDR::Kwargs();
}

size_t SoapySDDC::getNumChannels(const int dir) const
{
    DbgPrintf("SoapySDDC::getNumChannels\n");
    return (dir == SOAPY_SDR_RX) ? 1 : 0;
}

bool SoapySDDC::getFullDuplex(const int, const size_t) const
{
    DbgPrintf("SoapySDDC::getFullDuplex\n");
    return false;
}

// list antennas
std::vector<std::string> SoapySDDC::listAntennas(const int, const size_t) const
{
    DbgPrintf("SoapySDDC::listAntennas\n");
    std::vector<std::string> antennas;
    antennas.push_back("RX");
    return antennas;
}

// set the selected antenna
void SoapySDDC::setAntenna(const int dir, const size_t, const std::string &)
{
    DbgPrintf("SoapySDDC::setAntenna\n");
    if (dir != SOAPY_SDR_RX) throw std::runtime_error("setAntena failed: SDDC only supports RX");
}

// get the selected antenna
std::string SoapySDDC::getAntenna(const int dir, const size_t) const
{
    DbgPrintf("SoapySDDC::getAntenna\n");
    if (dir != SOAPY_SDR_RX) throw std::runtime_error("getAntena failed: SDDC only supports RX");
    return "RX";
}

bool SoapySDDC::hasDCOffsetMode(const int, const size_t) const
{
    DbgPrintf("Does not have DC offset mode\n");
    return false;
}

bool SoapySDDC::hasFrequencyCorrection(const int, const size_t) const
{
    DbgPrintf("Doest not have frequency correction\n");
    return false;
}

void SoapySDDC::setFrequencyCorrection(const int, const size_t, const double)
{
    DbgPrintf("SoapySDDC::setFrequencyCorrection\n");
}

double SoapySDDC::getFrequencyCorrection(const int, const size_t) const
{
    DbgPrintf("SoapySDDC::getFrequencyCorrection\n");
    return 0.0;
}

std::vector<std::string> SoapySDDC::listGains(const int, const size_t) const
{
    DbgPrintf("SoapySDDC::listGains\n");
    std::vector<std::string> gains;
    gains.push_back("RX");
    return gains;
}

bool SoapySDDC::hasGainMode(const int, const size_t) const
{
    DbgPrintf("SoapySDDC::hasGainMode\n");
    return false;
}

//void SoapySDDC::setGainMode(const int, const size_t, const bool)

//bool SoapySDDC::getGainMode(const int, const size_t) const

//void SoapySDDC::setGain(const int, const size_t, const double)

void SoapySDDC::setGain(const int, const size_t, const std::string &, const double)
{
    DbgPrintf("SoapySDDC::setGain\n");
}

SoapySDR::Range SoapySDDC::getGainRange(const int, const size_t, const std::string &) const
{
    DbgPrintf("SoapySDDC::getGainRange\n");
    return SoapySDR::Range();
}

SoapySDR::ArgInfoList SoapySDDC::getFrequencyArgsInfo(const int, const size_t) const
{
    DbgPrintf("SoapySDDC::getFrequencyArgsInfo\n");
    return SoapySDR::ArgInfoList();
}

void SoapySDDC::setSampleRate(const int, const size_t, const double)
{
    DbgPrintf("SoapySDDC::setSampleRate\n");
}

double SoapySDDC::getSampleRate(const int, const size_t) const
{
    DbgPrintf("SoapySDDC::getSampleRate\n");
    return sampleRate;
}

std::vector<double> SoapySDDC::listSampleRates(const int, const size_t) const
{
    DbgPrintf("SoapySDDC::listSampleRates\n");
    std::vector<double> results;

    results.push_back(250000);
    results.push_back(500000);
    results.push_back(1000000);
    results.push_back(2000000);
    results.push_back(4000000);
    results.push_back(8000000);
    results.push_back(16000000);
    results.push_back(32000000);
    results.push_back(64000000);
    results.push_back(128000000);
    results.push_back(150000000);

    return results;
}

std::vector<std::string> SoapySDDC::listTimeSources(void) const
{
    DbgPrintf("SoapySDDC::listTimeSources\n");
    std::vector<std::string> sources;
    sources.push_back("sw_ticks");
    return sources;
}

std::string SoapySDDC::getTimeSource(void) const
{
    DbgPrintf("SoapySDDC::getTimeSource\n");
    return "sw_ticks";
}

bool SoapySDDC::hasHardwareTime(const std::string &what) const
{
    DbgPrintf("SoapySDDC::hasHardwareTime\n");
    return what == "" || what == "sw_ticks";
}

long long SoapySDDC::getHardwareTime(const std::string &what) const
{
    DbgPrintf("SoapySDDC::getHardwareTime\n");
    return SoapySDR::ticksToTimeNs(ticks, sampleRate);
}

void SoapySDDC::setHardwareTime(const long long timeNs, const std::string &what)
{
    DbgPrintf("SoapySDDC::setHardwareTime\n");
    ticks = SoapySDR::timeNsToTicks(timeNs, sampleRate);
}