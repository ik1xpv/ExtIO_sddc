#include "UsbHandlerFactory.h"
#ifdef USE_LIBUSB
    #include "LibusbHandler.h"
#else
    #include "CyApiHandler.h"
#endif

IUsbHandler* UsbHandlerFactory::CreateUsbHandler() {
    #ifdef USE_LIBUSB
        return new LibusbHandler();
    #else
        return new CyApiHandler();
    #endif
}
