cmake_minimum_required(VERSION 3.13)

file(GLOB UNITTESTS "./*.cpp")

add_executable(unittest ${UNITTESTS})

if (MSVC)
    include_directories("../Core/arch/win32/FFTW3/")
endif()

include_directories("." "../Core")
target_link_libraries(unittest PRIVATE SDDC_CORE)
if (MSVC)
  target_link_libraries(unittest PUBLIC ${CMAKE_BINARY_DIR}/libfftw3f-3.lib)
else()
  target_link_libraries(unittest PUBLIC fftw3f pthread)
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
