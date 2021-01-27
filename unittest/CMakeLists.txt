cmake_minimum_required(VERSION 3.13)

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

file(GLOB UNITTESTS "./*.cpp")

add_executable(unittest ${UNITTESTS})
add_dependencies(unittest LIBCPPUNIT)

target_include_directories(unittest PUBLIC "${LIBFFTW_INCLUDE_DIR}")
target_link_directories(unittest PUBLIC "${LIBFFTW_LIBRARY_DIRS}")

include_directories("." "../Core")
include_directories(${LIBCPPUNIT_INCLUDE_DIRS})

target_link_libraries(unittest PRIVATE SDDC_CORE)
if (MSVC)
  target_link_libraries(unittest PUBLIC ${LIBFFTW_LIBRARIES})
else()
  target_link_libraries(unittest PUBLIC ${LIBFFTW_LIBRARIES} pthread ${ASANLIB})
endif (MSVC)


foreach(TESTSRC ${UNITTESTS})
    file(STRINGS ${TESTSRC} TESTS REGEX "^TEST_CASE\(.+\)")
    foreach(TEST ${TESTS})
        if (${TEST} MATCHES "TEST_CASE\\(([^,]+)[\\s,]*([^\\)]+)\\)")
            set(FIXTURE ${CMAKE_MATCH_1})
            set(TESTNAME ${CMAKE_MATCH_2})
            string(STRIP ${FIXTURE} FIXTURE)
            string(STRIP ${TESTNAME} TESTNAME)
            add_test(${FIXTURE}_${TESTNAME} unittest ${FIXTURE}::${TESTNAME})
        endif()
    endforeach()
endforeach()
