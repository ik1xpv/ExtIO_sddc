#ifndef __LIBUSB_HPP__
#define __LIBUSB_HPP__

#if defined(_WIN32) && defined(USE_LIBUSB_DRIVER)
    #include <libusb-1.0/libusb.h>
#elif defined(__APPLE__)
    #include <libusb.h>
#else
    #error "Unsupported platform"   
#endif

#endif // __LIBUSB_HPP__