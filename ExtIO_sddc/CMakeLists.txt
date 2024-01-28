# ExtIO_SDDC/CMakeLists.txt
cmake_minimum_required(VERSION 3.13)

# Check if building for Windows
if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")

    # List source files explicitly
    set(EXTIOSDDC_SRC
        "ExtIO_sddc.cpp"
        "SplashWindow.cpp"
        "tdialog.cpp"
        "uti.cpp"
        extio.def
        resource.rc
        manifest.xml
    )

    add_library(ExtIO_SDDC SHARED ${EXTIOSDDC_SRC})

    add_dependencies(ExtIO_SDDC usbLib)

    # Specify include directories for this target
    target_include_directories(ExtIO_SDDC PRIVATE
        "."
        "../Core"
    )

    # Define debug-specific compile definitions in a target-specific manner
    target_compile_definitions(ExtIO_SDDC PRIVATE
        $<$<CONFIG:Debug>:_DEBUG>
        _CRT_SECURE_NO_WARNINGS
    )

    # Link libraries
    target_include_directories(ExtIO_SDDC PUBLIC "${LIBFFTW_INCLUDE_DIRS}")
    message(STATUS "LIBFFTW_LIBRARIES: ${LIBFFTW_LIBRARIES}")
    target_link_directories(ExtIO_SDDC PUBLIC ${LIBFFTW_LIBRARY_DIRS})
    target_link_libraries(ExtIO_SDDC PRIVATE ${LIBFFTW_LIBRARIES})
    target_link_libraries(ExtIO_SDDC PRIVATE git_version SDDC_CORE Setupapi.lib)

    # Set other properties for the target
    set_target_properties(ExtIO_SDDC PROPERTIES PREFIX "")

endif()
