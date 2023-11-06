#ifndef __USBHANDLERFACTORY_H__
#define __USBHANDLERFACTORY_H__

#include "IUsbHandler.h"
#include <sys/stat.h>

class UsbHandlerFactory {
public:
     static IUsbHandler* CreateUsbHandler();
};

#endif // __USBHANDLERFACTORY_H__