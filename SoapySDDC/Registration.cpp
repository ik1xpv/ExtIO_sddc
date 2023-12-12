#include "SoapySDDC.hpp"
#include <SoapySDR/Registry.hpp>




SoapySDR::KwargsList findSDDC(const SoapySDR::Kwargs &args)
{
    DbgPrintf("entering findSDDC method\n");
    SoapySDR::KwargsList results;
    SoapySDR::Kwargs sddc;
    sddc["key"] = "SDDC"; // Set properties as needed
    results.push_back(sddc);
    return results;
}

SoapySDR::Device *makeSDDC(const SoapySDR::Kwargs &args)
{ 
    DbgPrintf("makeSDDC\n");
    return new SoapySDDC(args);
}

static SoapySDR::Registry registerSDDC("SDDC", &findSDDC, &makeSDDC, SOAPY_SDR_ABI_VERSION);
