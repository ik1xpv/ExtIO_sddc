cmake_minimum_required(VERSION 3.13)
project(ExtIO_SDDC)

set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc /W3")

file(GLOB SRC "*.cpp" "radio/*.cpp" "CyAPI/*.cpp" "pffft/*.cpp" extio.def resource.rc manifest.xml)

include_directories("." "CyAPI/" "FFTW3/" "pffft/")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

get_filename_component(SDKPATH CMAKE_LINKER DIRECTORY)
find_program(LIBEXE lib HINTS ${SDKPATH})
message(${LIBEXE})

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/libfftw3f-3.lib
    COMMAND ${LIBEXE} /def:${CMAKE_SOURCE_DIR}\\fftw3\\libfftw3f-3.def /MACHINE:x86 /OUT:${CMAKE_BINARY_DIR}\\libfftw3f-3.lib
)

add_library(ExtIO_SDDC SHARED ${SRC}  ${CMAKE_BINARY_DIR}/libfftw3f-3.lib)

target_link_libraries(ExtIO_SDDC PUBLIC ${CMAKE_BINARY_DIR}/libfftw3f-3.lib)
target_link_libraries(ExtIO_SDDC PUBLIC Setupapi.lib)
set_target_properties(ExtIO_SDDC PROPERTIES PREFIX "")
target_compile_definitions(ExtIO_SDDC PUBLIC _CRT_SECURE_NO_WARNINGS)

