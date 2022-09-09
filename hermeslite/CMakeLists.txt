cmake_minimum_required(VERSION 3.13)

file(GLOB SRC "lite.cpp" "../Core/pffft/*.cpp" resource.rc)

add_executable(hermeslite ${SRC})

target_include_directories(hermeslite PUBLIC "${LIBFFTW_INCLUDE_DIR}")
target_link_directories(hermeslite PUBLIC "${LIBFFTW_LIBRARY_DIRS}")
target_link_libraries(hermeslite PUBLIC ${LIBFFTW_LIBRARIES})

include_directories(${LIBFFTW_INCLUDE_DIRS})
include_directories("." "../Core")

target_link_libraries(hermeslite PRIVATE SDDC_CORE)

target_include_directories(hermeslite PUBLIC "${LIBFFTW_INCLUDE_DIR}")
target_link_directories(hermeslite PUBLIC "${LIBFFTW_LIBRARY_DIRS}")
target_link_libraries(hermeslite PUBLIC ${LIBFFTW_LIBRARIES})

if (MSVC)
target_link_libraries(hermeslite PUBLIC Setupapi.lib Ws2_32.lib)
else (MSVC)
target_include_directories(hermeslite PUBLIC "${LIBUSB_INCLUDE_DIR}")
target_link_directories(hermeslite PUBLIC "${LIBUSB_LIBRARY_DIRS}")
target_link_libraries(hermeslite PUBLIC ${LIBUSB_LIBRARIES})
target_link_libraries(hermeslite PUBLIC pthread)
endif (MSVC)

