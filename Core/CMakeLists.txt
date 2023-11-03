# Core/CmakeLists.txt
cmake_minimum_required(VERSION 3.13)
message(STATUS "Enter CORE directory")
add_subdirectory(usb)

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
          


include_directories(${LIBFFTW_INCLUDE_DIRS})
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")



add_library(SDDC_CORE STATIC ${SRC})

target_link_libraries(SDDC_CORE PUBLIC usbLib ${LIBFFTW_LIBRARIES})
target_link_directories(SDDC_CORE PUBLIC ${LIBFFTW_LIBRARY_DIRS})

set_target_properties(SDDC_CORE PROPERTIES POSITION_INDEPENDENT_CODE True)
target_compile_definitions(SDDC_CORE PUBLIC _CRT_SECURE_NO_WARNINGS)

if (NOT USE_SIMD_OPTIMIZATIONS)
   target_compile_definitions(SDDC_CORE PRIVATE NO_SIMD_OPTIM)
endif()
