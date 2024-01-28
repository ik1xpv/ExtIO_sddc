

find_package(SoapySDR CONFIG)

# use, i.e. don't skip the full RPATH for the build tree
set(CMAKE_SKIP_BUILD_RPATH FALSE)
 
# when building, don't use the install RPATH already
# (but later on when installing)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
#set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
 
# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
#set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


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



set_target_properties(SDDCSupport PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
