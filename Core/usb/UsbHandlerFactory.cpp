#include "UsbHandlerFactory.h"

#ifdef USE_LIBUSB_DRIVER
    #include "LibusbHandler.hpp"
#else
    #include "CyApiHandler.h"
#endif

IUsbHandler* UsbHandlerFactory::CreateUsbHandler() {
    #ifdef USE_LIBUSB_DRIVER
        return new LibusbHandler();
    #else
        return new CyApiHandler();
    #endif
}
