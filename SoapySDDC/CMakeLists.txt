# SoapySDR Lib

find_package(SoapySDR CONFIG)

# use, i.e. don't skip the full RPATH for the build tree
# set(CMAKE_SKIP_BUILD_RPATH FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
# set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

include_directories("." "../Core")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
# set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
SOAPY_SDR_MODULE_UTIL(
    TARGET SDDCSupport
    SOURCES
    SoapySDDC.hpp
    Registration.cpp
    Settings.cpp
    Streaming.cpp
    LIBRARIES
    SDDC_CORE
)

add_dependencies(SDDCSupport SDDC_CORE)

# set_target_properties(SDDCSupport PROPERTIES
#     CXX_STANDARD 17
#     CXX_STANDARD_REQUIRED ON
# )
target_include_directories(SDDCSupport PUBLIC "${LIBUSB_INCLUDE_DIR}")
target_link_directories(SDDCSupport PUBLIC "${LIBUSB_LIBRARY_DIRS}")
target_link_libraries(SDDCSupport PUBLIC ${LIBUSB_LIBRARIES})

target_include_directories(SDDCSupport PUBLIC "${LIBFFTW_INCLUDE_DIR}")
target_link_directories(SDDCSupport PUBLIC "${LIBFFTW_LIBRARY_DIRS}")
target_link_libraries(SDDCSupport PUBLIC ${LIBFFTW_LIBRARIES})

target_link_libraries(sddc PRIVATE SDDC_CORE)
set_target_properties(sddc PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(sddc PROPERTIES SOVERSION 1)
