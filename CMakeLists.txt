cmake_minimum_required(VERSION 3.13)
project(SDDC VERSION 1.0.1)

include(CTest)

### build options
# default build type: Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif(NOT CMAKE_BUILD_TYPE)
message(STATUS "Build type: " ${CMAKE_BUILD_TYPE} " - Version: " ${VERSION} " / " ${LIBVER})

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc /W3")

    get_filename_component(SDKPATH ${CMAKE_LINKER} DIRECTORY)
    find_program(LIBEXE lib HINTS ${SDKPATH} REQUIRED)

    # External Project FFTW on Windows
    if(${CMAKE_EXE_LINKER_FLAGS} MATCHES "X86")
        SET(FFTW_URL "ftp://ftp.fftw.org/pub/fftw/fftw-3.3.5-dll32.zip")
        SET(ARCH x86)
    else()
        SET(FFTW_URL "ftp://ftp.fftw.org/pub/fftw/fftw-3.3.5-dll64.zip")
        SET(ARCH x64)
    endif()

    include(ExternalProject)
    ExternalProject_Add(
        LIBFFTW
        URL ${FFTW_URL}
        BUILD_IN_SOURCE TRUE
        CONFIGURE_COMMAND   ""
        BUILD_COMMAND       ${LIBEXE} /def:./libfftw3f-3.def /MACHINE:${ARCH} /OUT:./fftw3f-3.lib
        INSTALL_COMMAND     ""
    )
    ExternalProject_Get_Property(LIBFFTW SOURCE_DIR)
    SET(LIBFFTW_INCLUDE_DIRS ${SOURCE_DIR})
    SET(LIBFFTW_LIBRARY_DIRS ${SOURCE_DIR})
    SET(LIBFFTW_LIBRARIES fftw3f-3)

    add_subdirectory(ExtIO_sddc)
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -Wall -Werror")
    #add_compile_options(-Wall -Wextra -pedantic)
    include(FindPkgConfig)
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
    pkg_check_modules(LIBFFTW REQUIRED fftw3f)
endif (MSVC)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

add_subdirectory(Core)
add_subdirectory(libsddc)
add_subdirectory(unittest)