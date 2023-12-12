#include "SoapySDDC.hpp"
#include <SoapySDR/Registry.hpp>
#include <cstdint>


struct DevContext 
{
	unsigned char numdev;
	char dev[MAXNDEV][MAXDEVSTRLEN];
};

DevContext  devicelist; // list of FX3 devices

SoapySDR::KwargsList findSDDC(const SoapySDR::Kwargs &args)
{
    DbgPrintf("entering findSDDC method\n");
    unsigned char* fw_data;
    uint32_t fw_size;
    unsigned char idx = 0;
    SoapySDR::KwargsList results;
    SoapySDR::Kwargs sddc;
    auto Fx3 = UsbHandlerFactory::CreateUsbHandler();
    Fx3->Enumerate(idx, devicelist.dev[0], sizeof(devicelist.dev[0]), fw_data, fw_size);
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
