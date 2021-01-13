cmake_minimum_required(VERSION 3.13)

if (MSVC)
    file(GLOB ARCH_SRC "arch/win32/*.cpp" "arch/win32/CyAPI/*.cpp")
    include_directories("arch/win32/FFTW3/")
else()
    file(GLOB ARCH_SRC "arch/linux/*.c" "arch/linux/*.cpp")
endif (MSVC)

file(GLOB SRC "*.cpp" "radio/*.cpp" "pffft/*.cpp" ${ARCH_SRC})

include_directories("." "CyAPI/" "pffft/")
if (MSVC)
    set_source_files_properties(fft_mt_r2iq_avx.cpp PROPERTIES COMPILE_FLAGS /arch:AVX)
    set_source_files_properties(fft_mt_r2iq_avx2.cpp PROPERTIES COMPILE_FLAGS /arch:AVX2)
    set_source_files_properties(fft_mt_r2iq_avx512.cpp PROPERTIES COMPILE_FLAGS /arch:AVX512)
else()
    include_directories(${LIBUSB_INCLUDE_DIRS})
    include_directories(${LIBFFTW_INCLUDE_DIRS})
endif (MSVC)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

add_library(SDDC_CORE STATIC ${SRC})
set_target_properties(SDDC_CORE PROPERTIES POSITION_INDEPENDENT_CODE True)
target_compile_definitions(SDDC_CORE PUBLIC _CRT_SECURE_NO_WARNINGS)