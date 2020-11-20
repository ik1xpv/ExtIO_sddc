cmake_minimum_required(VERSION 3.13)
project(ExtIO_RX888)

set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc /W3")

file(GLOB SRC "*.cpp" "radio/*.cpp" "CyAPI/*.cpp" extio.def resource.rc manifest.xml)

include_directories("." "CyAPI/" "FFTW3/")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DTRACE")

get_filename_component(SDKPATH CMAKE_LINKER DIRECTORY)
find_program(LIBEXE lib HINTS ${SDKPATH})
message(${LIBEXE})

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/libfftw3f-3.lib
    COMMAND ${LIBEXE} /def:${CMAKE_SOURCE_DIR}\\fftw3\\libfftw3f-3.def /MACHINE:x86 /OUT:${CMAKE_BINARY_DIR}\\libfftw3f-3.lib
)

add_library(ExtIO_RX888 SHARED ${SRC}  ${CMAKE_BINARY_DIR}/libfftw3f-3.lib)

target_link_libraries(ExtIO_RX888 PUBLIC ${CMAKE_BINARY_DIR}/libfftw3f-3.lib)
target_link_libraries(ExtIO_RX888 PUBLIC Setupapi.lib)
set_target_properties(ExtIO_RX888 PROPERTIES PREFIX "")
target_compile_definitions(ExtIO_RX888 PUBLIC _CRT_SECURE_NO_WARNINGS)

