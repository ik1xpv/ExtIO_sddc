# Core/usb/CMakeLists.txt
# Declare the usbLib static library (without any sources initially)
add_library(usbLib STATIC)

# Gather source files
set(USB_SRC "UsbHandlerFactory.cpp")

# For headers which might be used outside this directory, you might want to specify them too.
set(USB_HEADERS "IUsbHandler.h")


if(WIN32)
    if(USE_LIBUSB)
        # Append libusb specific sources and headers for Windows
        target_compile_definitions(usbLib PUBLIC -DUSE_LIBUSB_DRIVER)
        list(APPEND USB_SRC "LibusbHandler.cpp" "libusb/libusb.cpp")
        list(APPEND USB_HEADERS "LibusbHandler.h" "libusb/libusb.hpp")
        target_include_directories(usbLib PUBLIC ${LIBUSB_INCLUDE_DIRS})
    else()
        # Append CyAPI specific sources and headers
        list(APPEND USB_SRC "CyAPI/CyAPI.cpp" "CyApiHandler.cpp")
        list(APPEND USB_HEADERS "CyApiHandler.h" "CyAPI/CyAPI.h" "CyAPI/cyioctl.h" "CyAPI/CyUSB30_def.h" "CyAPI/devioctl.h"
                                "CyAPI/usb100.h" "CyAPI/usb200.h" "CyAPI/UsbdStatus.h" "CyAPI/VersionNo.h")
        # Include CyAPI directories and libraries
        target_include_directories(usbLib PUBLIC "CyAPI")
        
    endif()
else()
    add_compile_definitions(-DUSE_LIBUSB_DRIVER)
    # Append libusb specific sources and headers for Unix-like systems
    list(APPEND USB_SRC "libusb/libusb.cpp")
    list(APPEND USB_HEADERS "libusb/libusb.hpp")
endif()

# Add the sources and headers to your main target
target_sources(usbLib PUBLIC ${USB_SRC} ${USB_HEADERS})
