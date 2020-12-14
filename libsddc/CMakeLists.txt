cmake_minimum_required(VERSION 3.13)

if (MSVC)
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
  get_filename_component(SDKPATH ${CMAKE_LINKER} DIRECTORY)
  find_program(LIBEXE lib HINTS ${SDKPATH} REQUIRED)

  add_custom_command(
      OUTPUT ${CMAKE_BINARY_DIR}/libfftw3f-3.lib
      COMMAND ${LIBEXE} /def:${CMAKE_SOURCE_DIR}\\Core\\arch\\win32\\FFTW3\\libfftw3f-3.def /MACHINE:x86 /OUT:${CMAKE_BINARY_DIR}\\libfftw3f-3.lib
  )

  set(EXTRA_SRC ${CMAKE_BINARY_DIR}/libfftw3f-3.lib)
endif (MSVC)

include_directories("." "../Core")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

add_library(sddc SHARED
    libsddc.cpp
)

if (MSVC)
  target_link_libraries(sddc PUBLIC ${CMAKE_BINARY_DIR}/libfftw3f-3.lib)
  target_link_libraries(sddc PUBLIC Setupapi.lib)
else()
  target_link_libraries(sddc PUBLIC usb-1.0)
  target_link_libraries(sddc PUBLIC fftw3f)
endif (MSVC)

target_link_libraries(sddc PRIVATE SDDC_CORE)
set_target_properties(sddc PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(sddc PROPERTIES SOVERSION 0)

# applications
add_executable(sddc_test sddc_test.c)
target_link_libraries(sddc_test PRIVATE sddc)

add_executable(sddc_stream_test sddc_stream_test.c wavewrite.c)
target_link_libraries(sddc_stream_test sddc)

add_executable(sddc_vhf_stream_test sddc_vhf_stream_test.c wavewrite.c)
target_link_libraries(sddc_vhf_stream_test sddc)
