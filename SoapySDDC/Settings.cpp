#include "SoapySDDC.hpp"
#include <SoapySDR/Types.hpp>
#include <SoapySDR/Time.hpp>




SoapySDDC::SoapySDDC(const SoapySDR::Kwargs &args):
    Fx3(UsbHandlerFactory::CreateUsbHandler()),
    numBuffers(16),
    sampleRate(32000000)
{
    DbgPrintf("SoapySDDC::SoapySDDC\n");
    unsigned char* fw_data;
    uint32_t fw_size;
    unsigned char idx = 0;
    DevContext  devicelist; 
    Fx3->Enumerate(idx, devicelist.dev[0], sizeof(devicelist.dev[0]), fw_data, fw_size);
    Fx3->Open(fw_data, fw_size);
    //RadioHandler.Init(Fx3, Callback);
    //RadioHandler.Start(0);
    
    
}

SoapySDDC::~SoapySDDC(void)
{
    DbgPrintf("SoapySDDC::~SoapySDDC\n");
    RadioHandler.Stop();
    delete Fx3;
    Fx3 = nullptr;
    

    //RadioHandler.Close();
}

std::string SoapySDDC::getDriverKey(void) const
{
    DbgPrintf("SoapySDDC::getDriverKey\n");
    
}

std::string SoapySDDC::getHardwareKey(void) const
{
    DbgPrintf("SoapySDDC::getHardwareKey\n");
    return std::string(RadioHandler.getName());
}

SoapySDR::Kwargs SoapySDDC::getHardwareInfo(void) const
{
    DbgPrintf("SoapySDDC::getHardwareInfo\n");
    return SoapySDR::Kwargs();
}

/*******************************************************************
 * Channels API
 ******************************************************************/

size_t SoapySDDC::getNumChannels(const int dir) const
{
    DbgPrintf("SoapySDDC::getNumChannels\n");
    return (dir == SOAPY_SDR_RX) ? 2 : 0;
}

bool SoapySDDC::getFullDuplex(const int, const size_t) const
{
    DbgPrintf("SoapySDDC::getFullDuplex\n");
    return false;
}

/*******************************************************************
 * Antenna API
 ******************************************************************/

std::vector<std::string> SoapySDDC::listAntennas(const int direction, const size_t) const
{
    DbgPrintf("SoapySDDC::listAntennas : %d\n", direction);
    std::vector<std::string> antennas;
    if (direction == SOAPY_SDR_TX)
    {
        return antennas;
    }
    
    antennas.push_back("HF");
    antennas.push_back("VHF");
    // i want to list antennas names in dbgprintf
    for (auto &antenna : antennas)
    {
        DbgPrintf("SoapySDDC::listAntennas : %s\n", antenna.c_str());
    }
    return antennas;
}

// set the selected antenna
void SoapySDDC::setAntenna(const int direction, const size_t, const std::string &name)
{
    DbgPrintf("SoapySDDC::setAntenna : %d\n", direction);
    if (direction != SOAPY_SDR_RX)
    {
        return;
    }
    if (name == "HF")
    {
        RadioHandler.UpdatemodeRF(HFMODE);
        RadioHandler.UpdBiasT_HF(true);
    }
    else if (name == "VHF")
    {
        RadioHandler.UpdatemodeRF(VHFMODE);
        RadioHandler.UpdBiasT_VHF(true);
    }
    else
    {
        RadioHandler.UpdBiasT_HF(false);
        RadioHandler.UpdBiasT_VHF(false);
    }

    //what antenna is set print in dbgprintf
    DbgPrintf("SoapySDDC::setAntenna : %s\n", name.c_str());

}

// get the selected antenna
std::string SoapySDDC::getAntenna(const int direction, const size_t) const
{
    DbgPrintf("SoapySDDC::getAntenna\n");
    if (direction == SOAPY_SDR_TX)
    {
        return "";
    }
    return "RX";
}

bool SoapySDDC::hasDCOffsetMode(const int, const size_t) const
{
    DbgPrintf("SoapySDDC::hasDCOffsetMode\n");
    return false;
}

bool SoapySDDC::hasFrequencyCorrection(const int, const size_t) const
{
    DbgPrintf("SoapySDDC::hasFrequencyCorrection\n");
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

void SoapySDDC::setFrequency(const int, const size_t, const double frequency, const SoapySDR::Kwargs &)
{
    DbgPrintf("SoapySDDC::setFrequency %f\n", frequency);
}

void SoapySDDC::setFrequency(const int, const size_t, const std::string &, const double, const SoapySDR::Kwargs &)
{
    DbgPrintf("SoapySDDC::setFrequency\n");
}

double SoapySDDC::getFrequency(const int, const size_t, const std::string &) const
{
    DbgPrintf("SoapySDDC::getFrequency\n");
    if (sampleRate == 32000000)
        {
            return 8000000.000000;
        }

    return 0.0;
}

SoapySDR::ArgInfoList SoapySDDC::getFrequencyArgsInfo(const int, const size_t) const
{
    DbgPrintf("SoapySDDC::getFrequencyArgsInfo\n");
    return SoapySDR::ArgInfoList();
}

void SoapySDDC::setSampleRate(const int, const size_t, const double rate)
{
    DbgPrintf("SoapySDDC::setSampleRate %f\n", rate);
    switch ((int)rate)
    {
        case 32000000:
            sampleRate = 32000000;
            samplerateidx = 4;
            break;
        case 16000000:
            sampleRate = 16000000;
            samplerateidx = 3;
            break;
        case 8000000:
            sampleRate = 8000000;
            samplerateidx = 2;
            break;
        case 4000000:
            sampleRate = 4000000;
            samplerateidx = 1;
            break;
        case 2000000:
            sampleRate = 2000000;
            samplerateidx = 0;
            break;
        default:
            return;
    }
        
}

double SoapySDDC::getSampleRate(const int, const size_t) const
{
    DbgPrintf("SoapySDDC::getSampleRate %f\n", sampleRate);
    return sampleRate;
}

std::vector<double> SoapySDDC::listSampleRates(const int, const size_t) const
{
    DbgPrintf("SoapySDDC::listSampleRates\n");
    std::vector<double> results;

    //results.push_back(250000);
    //results.push_back(500000);
    //results.push_back(1000000);
    results.push_back(2000000);
    results.push_back(4000000);
    results.push_back(8000000);
    results.push_back(16000000);
    results.push_back(32000000);
    //results.push_back(64000000);
    //results.push_back(128000000);
    //results.push_back(150000000);

    return results;
}

void SoapySDDC::setMasterClockRate(const double rate)
{
    DbgPrintf("SoapySDDC::setMasterClockRate %f\n", rate);
    masterClockRate = rate;
}

double SoapySDDC::getMasterClockRate(void) const
{
    DbgPrintf("SoapySDDC::getMasterClockRate %f\n", masterClockRate);
    return masterClockRate;
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