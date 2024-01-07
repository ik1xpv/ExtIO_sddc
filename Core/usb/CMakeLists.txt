# Core/usb/CMakeLists.txt
# Declare the usbLib static library (without any sources initially)
add_library(usbLib STATIC)

set_target_properties(usbLib PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Gather source files
set(USB_SRC "UsbHandlerFactory.cpp")

# For headers which might be used outside this directory, you might want to specify them too.
set(USB_HEADERS "IUsbHandler.h")


if(MSVC)
    
        SET(LIBUSB_URL "https://github.com/libusb/libusb/releases/download/v1.0.26/libusb-1.0.26-binaries.7z")
        include(ExternalProject)
        ExternalProject_Add(
            LIBUSB
            PREFIX LIBUSB
            URL ${LIBUSB_URL}
            BUILD_IN_SOURCE TRUE
            CONFIGURE_COMMAND   ""
            BUILD_COMMAND      ""
            INSTALL_COMMAND     ""
        )
        ExternalProject_Get_Property(LIBUSB SOURCE_DIR)
        SET(LIBUSB_INCLUDE_DIRS ${SOURCE_DIR}/libusb-MinGW-Win32/include)
        SET(LIBUSB_LIBRARY_DIRS ${SOURCE_DIR}/VS2015-Win32/lib)
        message(STATUS "LIBUSB_INCLUDE_DIRS=${LIBUSB_INCLUDE_DIRS}")
        message(STATUS "LIBUSB_LIBRARY_DIRS=${LIBUSB_LIBRARY_DIRS}")
        
        SET(LIBUSB_LIBRARIES libusb-1.0)
        # Append libusb specific sources and headers for Windows
        if(USE_LIBUSB)
            target_compile_definitions(usbLib PUBLIC -DUSE_LIBUSB_DRIVER)
        endif()
        list(APPEND USB_SRC "LibusbHandler.cpp" "libusb/libusb.cpp")
        list(APPEND USB_HEADERS "LibusbHandler.h" "libusb/libusb.hpp")
        target_include_directories(usbLib PUBLIC ${LIBUSB_INCLUDE_DIRS})
        target_link_directories(usbLib PUBLIC ${LIBUSB_LIBRARY_DIRS})
        target_link_libraries(usbLib PUBLIC ${LIBUSB_LIBRARIES})
   
        # Append CyAPI specific sources and headers
        list(APPEND USB_SRC "CyAPI/CyAPI.cpp" "CyApiHandler.cpp")
        list(APPEND USB_HEADERS "CyApiHandler.h" "CyAPI/CyAPI.h" "CyAPI/cyioctl.h" "CyAPI/CyUSB30_def.h" "CyAPI/devioctl.h"
                                "CyAPI/usb100.h" "CyAPI/usb200.h" "CyAPI/UsbdStatus.h" "CyAPI/VersionNo.h")
        # Include CyAPI directories and libraries
        target_include_directories(usbLib PUBLIC "CyAPI" ${LIBUSB_INCLUDE_DIRS})
        target_link_directories(usbLib PUBLIC "CyAPI" ${LIBUSB_LIBRARY_DIRS})
       # target_link_libraries(usbLib PUBLIC "CyAPI" ${LIBUSB_LIBRARIES})
        # Ensure usbLib is built after LIBUSB.
        add_dependencies(usbLib LIBUSB)
        
   
else() # if not MSVC
    include(FindPkgConfig)
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
    target_compile_definitions(usbLib PUBLIC -DUSE_LIBUSB_DRIVER)
    # Append libusb specific sources and headers for Unix-like systems
    list(APPEND USB_SRC "libusb/libusb.cpp" "libusbHandler.cpp")
    list(APPEND USB_HEADERS "libusb/libusb.hpp" "LibusbHandler.h")
    target_include_directories(usbLib PUBLIC ${LIBUSB_INCLUDE_DIRS})
   
    target_link_directories(usbLib PRIVATE ${LIBUSB_LIBRARY_DIRS})

    target_link_libraries(usbLib PRIVATE ${LIBUSB_LIBRARIES})

endif() # MSVC

# Add the sources and headers to your main target
target_sources(usbLib PUBLIC ${USB_SRC} ${USB_HEADERS})
