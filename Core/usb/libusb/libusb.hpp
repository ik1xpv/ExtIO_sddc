#ifndef __LIBUSB_HPP__
#define __LIBUSB_HPP__

#if defined(_WIN32) || defined(USE_LIBUSB_DRIVER)
    #include <libusb-1.0/libusb.h>
#else
    #include <libusb.h>
#endif

#endif // __LIBUSB_HPP__