#include "SoapySDDC.hpp"
#include <SoapySDR/Registry.hpp>
#include <cstdint>

#include "firmware.h"

DevContext devicelist; // list of FX3 devices

SoapySDR::KwargsList findSDDC(const SoapySDR::Kwargs &args)
{
    DbgPrintf("soapySDDC::findSDDC\n");

    std::vector<SoapySDR::Kwargs> results;

    unsigned char idx = 0;
    fx3class *Fx3(CreateUsbHandler());
    
    while(Fx3->Enumerate(idx, devicelist.dev[idx], FIRMWARE, sizeof(FIRMWARE)))
    {
        SoapySDR::Kwargs devInfo;

        devInfo["label"] = std::string("SDDC") + " :: " + devicelist.dev[idx];
        results.push_back(devInfo);
        idx++;
    }

    delete Fx3;
    return results;
}

SoapySDR::Device *makeSDDC(const SoapySDR::Kwargs &args)
{
    DbgPrintf("soapySDDC::makeSDDC\n");
    return new SoapySDDC(args);
}

static SoapySDR::Registry registerSDDC("SDDC", &findSDDC, &makeSDDC, SOAPY_SDR_ABI_VERSION);
