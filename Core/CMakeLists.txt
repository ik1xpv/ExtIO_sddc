# Core/CmakeLists.txt
cmake_minimum_required(VERSION 3.13)
message(STATUS "Enter CORE directory")
add_subdirectory(usb)

if (MSVC)
    set(CMAKE_CXX_FLAGS " /std:c++17 /EHsc /W3")

    get_filename_component(SDKPATH ${CMAKE_LINKER} DIRECTORY)
    find_program(LIBEXE lib HINTS ${SDKPATH} REQUIRED)

    # External Project FFTW on Windows
    if(${CMAKE_EXE_LINKER_FLAGS} MATCHES "X86")
        SET(FFTW_URL "ftp://ftp.fftw.org/pub/fftw/fftw-3.3.5-dll32.zip")
        SET(ARCH x86)
        SET(HASH 29882a43033c9393479a4df52a2e9120589c06a2b724155b1a682747fa3e57d4)
    else()
        SET(FFTW_URL "ftp://ftp.fftw.org/pub/fftw/fftw-3.3.5-dll64.zip")
        SET(ARCH x64)
        SET(HASH cfd88dc0e8d7001115ea79e069a2c695d52c8947f5b4f3b7ac54a192756f439f)
    endif()

    include(ExternalProject)
    ExternalProject_Add(
        LIBFFTW
        PREFIX LIBFFTW
        URL ${FFTW_URL}
        URL_HASH SHA256=${HASH}
        BUILD_IN_SOURCE TRUE
        CONFIGURE_COMMAND   ""
        BUILD_COMMAND       ${LIBEXE} /def:./libfftw3f-3.def /MACHINE:${ARCH} /OUT:./fftw3f-3.lib
        INSTALL_COMMAND     ""
    )
    ExternalProject_Get_Property(LIBFFTW SOURCE_DIR)
    SET(LIBFFTW_INCLUDE_DIRS ${SOURCE_DIR})
    SET(LIBFFTW_LIBRARY_DIRS ${SOURCE_DIR})
    SET(LIBFFTW_LIBRARIES fftw3f-3)
    message(STATUS "LIBFFTW_INCLUDE_DIRS=${LIBFFTW_INCLUDE_DIRS}")
    message(STATUS "LIBFFTW_LIBRARY_DIRS=${LIBFFTW_LIBRARY_DIRS}")

else()

    if (USE_DEBUG_ASAN)
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
        set(CMAKE_CXX_FLAGS "-O0 -std=c++17 -Wall -Werror")
    else()
        set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -Wall -Werror")
    endif(USE_DEBUG_ASAN)
    #add_compile_options(-Wall -Wextra -pedantic)
    include(FindPkgConfig)

    find_package(Threads REQUIRED)

    pkg_check_modules(LIBFFTW REQUIRED fftw3f)
    
endif (MSVC)

file(GLOB SRC "config.cpp"
              "fft_mt_r2iq.cpp"
              "fft_mt_r2iq_avx.cpp"
              "fft_mt_r2iq_avx2.cpp"
              "fft_mt_r2iq_avx512.cpp"
              "fft_mt_r2iq_def.cpp"
              "fft_mt_r2iq_impl.hpp"
              "fft_mt_r2iq_neon.cpp"
              "fft_mt_r2iq.cpp"
              "fir.cpp"
              "PScope_uti.cpp"
              "RadioHandler.cpp"
              "radio/*.cpp" "pffft/*.cpp")

if (MSVC)
              # Assume Windows/x86 target ;)
              set_source_files_properties(fft_mt_r2iq_avx.cpp PROPERTIES COMPILE_FLAGS /arch:AVX)
              set_source_files_properties(fft_mt_r2iq_avx2.cpp PROPERTIES COMPILE_FLAGS /arch:AVX2)
              set_source_files_properties(fft_mt_r2iq_avx512.cpp PROPERTIES COMPILE_FLAGS /arch:AVX512)
          else()
              # Assume GCC, check target CPU/FPU
              if ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "AMD64" OR "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
                  message(STATUS "Compiling for AVX")
                  set_source_files_properties(fft_mt_r2iq_avx.cpp PROPERTIES COMPILE_FLAGS -mavx)
                  set_source_files_properties(fft_mt_r2iq_avx2.cpp PROPERTIES COMPILE_FLAGS -mavx2)
                  set_source_files_properties(fft_mt_r2iq_avx512.cpp PROPERTIES COMPILE_FLAGS -mavx512f)
              elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "arm.*" OR "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "arm64")
                  # We may have Neon..
                  message(STATUS "Compiling for Neon")
                  list(FILTER SRC EXCLUDE REGEX "fft_mt_r2iq_avx.*")
                  list(APPEND SRC fft_mt_r2iq_neon.cpp)
                  set_source_files_properties(fft_mt_r2iq_neon.cpp PROPERTIES COMPILE_FLAGS "")
                  set_source_files_properties(pffft/pf_mixer.cpp PROPERTIES COMPILE_FLAGS "-D PFFFT_ENABLE_NEON -Wno-strict-aliasing")
              else()
                  message(FATAL_ERROR "Unable to identify CPU: ${CMAKE_SYSTEM_PROCESSOR}")
              endif()
          
          endif (MSVC)
          


set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")


add_library(SDDC_CORE STATIC ${SRC})
target_include_directories(SDDC_CORE PRIVATE ${LIBFFTW_INCLUDE_DIRS} ${LIBUSB_INCLUDE_DIRS})

if (MSVC)
    add_dependencies(SDDC_CORE LIBFFTW)
endif()

target_link_libraries(SDDC_CORE PUBLIC usbLib ${LIBFFTW_LIBRARIES} ${LIBUSB_LIBRARIES} Threads::Threads)
target_link_directories(SDDC_CORE PUBLIC ${LIBFFTW_LIBRARY_DIRS} ${LIBUSB_LIBRARY_DIRS})

set_target_properties(SDDC_CORE PROPERTIES POSITION_INDEPENDENT_CODE True)
target_compile_definitions(SDDC_CORE PUBLIC _CRT_SECURE_NO_WARNINGS)
if(VERBOSE_DEBUG)
    target_compile_definitions(SDDC_CORE PUBLIC -DVERBOSE_DEBUG)
endif()

if (NOT USE_SIMD_OPTIMIZATIONS)
   target_compile_definitions(SDDC_CORE PRIVATE NO_SIMD_OPTIM)
endif()
