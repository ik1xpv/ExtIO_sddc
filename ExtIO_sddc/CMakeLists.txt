cmake_minimum_required(VERSION 3.13)

file(GLOB SRC "*.cpp" extio.def resource.rc manifest.xml)

include_directories("." "../Core")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

add_library(ExtIO_SDDC SHARED ${SRC})

target_link_libraries(ExtIO_SDDC PRIVATE git_version)
target_link_libraries(ExtIO_SDDC PRIVATE SDDC_CORE)
set_target_properties(ExtIO_SDDC PROPERTIES PREFIX "")
target_compile_definitions(ExtIO_SDDC PUBLIC _CRT_SECURE_NO_WARNINGS)
