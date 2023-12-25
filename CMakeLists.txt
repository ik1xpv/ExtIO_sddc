cmake_minimum_required(VERSION 3.13)
project(SDDC VERSION 1.0.1)

include(CTest)

include(./cmake/CheckGit.cmake)
CheckGitSetup()

message(STATUS "CMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}")

### build options
# default build type: Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif(NOT CMAKE_BUILD_TYPE)
message(STATUS "Build type: " ${CMAKE_BUILD_TYPE} " - Version: " ${VERSION} " / " ${LIBVER})

# allow disabling optimizations - for debug reasons
option(USE_SIMD_OPTIMIZATIONS "enable SIMD optimizations" ON)

if(WIN32)
  option(USE_LIBUSB "Use libusb instead of CyAPI" OFF)
endif()

# allow enabling address sanitizer - for debug reasons
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    option(USE_DEBUG_ASAN "use GCC's address sanitizer?" OFF)
endif()

if (USE_DEBUG_ASAN)
  set(ASANLIB "asan")
else()
  set(ASANLIB "")
endif()

add_subdirectory(Core)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")


add_subdirectory(ExtIO_sddc)
#add_subdirectory(unittest)
if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND NOT MINGW))
  #add_subdirectory(libsddc)
  add_subdirectory(SoapySDDC)
endif()

