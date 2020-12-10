cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc /W3")

file(GLOB SRC "*.cpp" "radio/*.cpp" "arch/win32/*.cpp" "arch/win32/CyAPI/*.cpp" "pffft/*.cpp")

include_directories("." "CyAPI/" "arch/win32/FFTW3/" "pffft/")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

add_library(SDDC_CORE STATIC ${SRC})
