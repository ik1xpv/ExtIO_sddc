# Declare the usbLib static library (without any sources initially)
add_library(usbLib STATIC)

# Gather source files
set(USB_SRC "")

# For headers which might be used outside this directory, you might want to specify them too.
set(USB_HEADERS "IUsbHandler.h")


if(WIN32)
    if(USE_LIBUSB)
        # Append libusb specific sources and headers for Windows
        list(APPEND USB_SRC "libusbHandler.cpp" "libusb/libusb.cpp")
        list(APPEND USB_HEADERS "libusbHandler.hpp" "libusb/libusb.hpp")
    else()
        # Append CyAPI specific sources and headers
        list(APPEND USB_SRC "CyAPI/CyAPI.cpp" "CyApiHandler.cpp")
        list(APPEND USB_HEADERS "CyApiHandler.h" "CyAPI/CyAPI.h" "CyAPI/cyioctl.h" "CyAPI/CyUSB30_def.h" "CyAPI/devioctl.h"
                                "CyAPI/usb100.h" "CyAPI/usb200.h" "CyAPI/UsbdStatus.h" "CyAPI/VersionNo.h")
        # Include CyAPI directories and libraries
        target_include_directories(usbLib PUBLIC "CyAPI")
        
    endif()
else()
    # Append libusb specific sources and headers for Unix-like systems
    list(APPEND USB_SRC "libusb/*.cpp")
    list(APPEND USB_HEADERS "libusb/*.h")
endif()

# Add the sources and headers to your main target
target_sources(usbLib PUBLIC ${USB_SRC} ${USB_HEADERS})
