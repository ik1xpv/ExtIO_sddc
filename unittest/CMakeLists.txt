# unittest/CMakeLists.txt
cmake_minimum_required(VERSION 3.13)

cmake_policy(SET CMP0135 NEW)
include(ExternalProject)
ExternalProject_Add(
    LIBCPPUNIT
    GIT_REPOSITORY https://github.com/drleq/CppUnitTestFramework.git
    GIT_TAG 1.2.2
    CONFIGURE_COMMAND   ""
    BUILD_COMMAND       ""
    INSTALL_COMMAND     ""
)
ExternalProject_Get_Property(LIBCPPUNIT SOURCE_DIR)
SET(LIBCPPUNIT_INCLUDE_DIRS ${SOURCE_DIR})


set(UNITTESTS 
    core_test.cpp
    ringbuffer_test.cpp
    unittests.cpp
    # ... and so on
)


add_executable(unittest ${UNITTESTS})
add_dependencies(unittest LIBCPPUNIT usbLib SDDC_CORE)
target_compile_features(unittest PRIVATE cxx_std_17)
target_include_directories(unittest PUBLIC "${LIBFFTW_INCLUDE_DIR}" "../Core/include" 
                            ${LIBCPPUNIT_INCLUDE_DIRS})
#target_link_directories(unittest PUBLIC  ${LIBFFTW_LIBRARY_DIRS} )

target_link_libraries(unittest PRIVATE SDDC_CORE)

if (MSVC)
  target_link_libraries(unittest PUBLIC ${LIBFFTW_LIBRARIES} Setupapi.lib)
else()
  target_link_libraries(unittest PUBLIC  pthread ${ASANLIB})
endif (MSVC)


