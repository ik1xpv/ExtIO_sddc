cmake_minimum_required(VERSION 3.13)
project(SDDC VERSION 1.0.1)

### build options
# default build type: Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif(NOT CMAKE_BUILD_TYPE)
message(STATUS "Build type: " ${CMAKE_BUILD_TYPE} " - Version: " ${VERSION} " / " ${LIBVER})

add_subdirectory(CORE)
add_subdirectory(ExtIO_sddc)
add_subdirectory(libsddc)
