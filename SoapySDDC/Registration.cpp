#include "SoapySDDC.hpp"
#include <SoapySDR/Registry.hpp>
#include <cstdint>


DevContext  devicelist; // list of FX3 devices

SoapySDR::KwargsList findSDDC(const SoapySDR::Kwargs &args)
{
    DbgPrintf("soapySDDC::findSDDC\n");
    unsigned char* fw_data;
    uint32_t fw_size;
    unsigned char idx = 0;
    SoapySDR::KwargsList results;
    SoapySDR::Kwargs sddc;
    sddc["key"] = "SDDC"; // Set properties as needed
    results.push_back(sddc);
    return results;
}

SoapySDR::Device *makeSDDC(const SoapySDR::Kwargs &args)
{ 
    DbgPrintf("soapySDDC::makeSDDC\n");
    return new SoapySDDC(args);
}

static SoapySDR::Registry registerSDDC("SDDC", &findSDDC, &makeSDDC, SOAPY_SDR_ABI_VERSION);
