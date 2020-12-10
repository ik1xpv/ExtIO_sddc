cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc /W3")

include_directories("." "../Core")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

get_filename_component(SDKPATH ${CMAKE_LINKER} DIRECTORY)
find_program(LIBEXE lib HINTS ${SDKPATH} REQUIRED)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/libfftw3f-3.lib
    COMMAND ${LIBEXE} /def:${CMAKE_SOURCE_DIR}\\Core\\arch\\win32\\FFTW3\\libfftw3f-3.def /MACHINE:x86 /OUT:${CMAKE_BINARY_DIR}\\libfftw3f-3.lib
)

add_library(sddc SHARED
    libsddc.cpp
    ${CMAKE_BINARY_DIR}/libfftw3f-3.lib
)
target_link_libraries(sddc PUBLIC ${CMAKE_BINARY_DIR}/libfftw3f-3.lib)
target_link_libraries(sddc PRIVATE SDDC_CORE)
target_link_libraries(sddc PUBLIC Setupapi.lib)
set_target_properties(sddc PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(sddc PROPERTIES SOVERSION 0)

# applications
add_executable(sddc_test sddc_test.c)
target_link_libraries(sddc_test PRIVATE sddc)

# install
install(TARGETS sddc
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(TARGETS sddc_test
  DESTINATION ${CMAKE_INSTALL_BINDIR}
)