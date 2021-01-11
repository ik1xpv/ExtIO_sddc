cmake_minimum_required(VERSION 3.13)
project(SDDC VERSION 1.0.1)

include(CTest)

### build options
# default build type: Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif(NOT CMAKE_BUILD_TYPE)
message(STATUS "Build type: " ${CMAKE_BUILD_TYPE} " - Version: " ${VERSION} " / " ${LIBVER})

# add mipp
include(ExternalProject)
ExternalProject_Add(
    LIBMIPP
    GIT_REPOSITORY https://github.com/aff3ct/MIPP.git
    GIT_TAG master
    CONFIGURE_COMMAND   ""
    BUILD_COMMAND       ""
    INSTALL_COMMAND     ""
)
ExternalProject_Get_Property(LIBMIPP SOURCE_DIR)
SET(LIBMIPP_INCLUDE_DIRS ${SOURCE_DIR}/src)

if (MSVC)
    add_subdirectory(ExtIO_sddc)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc /W3 /arch:AVX2")
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -Wall")
    #add_compile_options(-Wall -Wextra -pedantic)
    include(FindPkgConfig)
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
    pkg_check_modules(LIBFFTW REQUIRED fftw3f)
endif (MSVC)

add_subdirectory(Core)
add_subdirectory(libsddc)
add_subdirectory(unittest)