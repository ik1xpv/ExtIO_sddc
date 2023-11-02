#pragma once

#include "IUsbHandler.h"
#include <sys/stat.h>

class UsbHandlerFactory {
public:
     static IUsbHandler* CreateUsbHandler();
};
