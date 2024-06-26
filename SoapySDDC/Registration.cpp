#include "SoapySDDC.hpp"
#include <SoapySDR/Registry.hpp>
#include <cstdint>


DevContext  devicelist; // list of FX3 devices

SoapySDR::KwargsList findSDDC(const SoapySDR::Kwargs &args)
{
    DbgPrintf("soapySDDC::findSDDC\n");

    std::vector<SoapySDR::Kwargs> results;

    unsigned char* fw_data;
    uint32_t fw_size;
    unsigned char idx = 0;
    SoapySDR::Kwargs devInfo;
    IUsbHandler* Fx3 (UsbHandlerFactory::CreateUsbHandler());
    Fx3->Enumerate(idx, devicelist.dev[0], fw_data, fw_size);
    uint8_t rdata[4];
    
    Fx3->GetHardwareInfo((uint32_t*)rdata);
    RadioModel radio = (RadioModel)rdata[0];
    uint16_t firmware = (rdata[1] << 8) + rdata[2];

    std::string DeviceVariant;
    switch (radio)
    {
    case HF103:
        DeviceVariant = "HF103";
        break;
    case BBRF103:
        DeviceVariant = "BBRF103";
        break;
    case RX888:
        DeviceVariant = "RX888";
        break;
    case RX888r2:

        DeviceVariant = "RX888r2";
        break;
    case RX888r3:
        DeviceVariant = "RX888r3";
        break;
    case RX999:
        DeviceVariant = "RX999";
        break;
    case RXLUCY:
        DeviceVariant = "RXLUCY";
        break;

 
    default:
        DeviceVariant = "DummyRadio";
        DbgPrintf("WARNING no SDR connected\n");
        break;
    }

    devInfo["label"] = std::string("SDDC") + " :: " + DeviceVariant;
    results.push_back(devInfo);
    delete Fx3;
    return results;
}

SoapySDR::Device *makeSDDC(const SoapySDR::Kwargs &args)
{ 
    DbgPrintf("soapySDDC::makeSDDC\n");
    return new SoapySDDC(args);
}

static SoapySDR::Registry registerSDDC("SDDC", &findSDDC, &makeSDDC, SOAPY_SDR_ABI_VERSION);
