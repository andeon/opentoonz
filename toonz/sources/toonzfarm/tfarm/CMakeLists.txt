# Define headers
set(HEADERS
    ../include/service.h
    ../../include/tfarmcontroller.h
    ../include/tfarmexecutor.h
    ../../include/tfarmplatforms.h
    ../include/tfarmproxy.h
    ../../include/tfarmserver.h
    ../../include/tfarmtask.h
    ../include/tlog.h
)

# Define source files
set(SOURCES
    service.cpp
    tfarmcontroller_c.cpp
    tfarmexecutor.cpp
    tfarmproxy.cpp
    tfarmserver_c.cpp
    tfarmserverstub.cpp
    tfarmtask.cpp
    tlog.cpp
    ttcpipclient.cpp
    ttcpipserver.cpp
)

# Create the shared library
add_library(tfarm SHARED
    ${HEADERS}
    ${SOURCES}
)

# Add definitions
target_compile_definitions(tfarm PRIVATE
    TFARM_EXPORTS
)

# Set linker flags for Apple
if(BUILD_ENV_APPLE)
    set_target_properties(tfarm PROPERTIES
        LINK_FLAGS "-Wl,-install_name,@executable_path/libtfarm.dylib"
    )
    add_dependencies(tfarm tnzcore tnzbase toonzlib tnzext)
endif()

# Debug messages
message(STATUS "Subdirectory: tfarm")
message(STATUS "Binary dir: ${CMAKE_CURRENT_BINARY_DIR}")

# Include directories
target_include_directories(tfarm PRIVATE
    ../include
)

# Define and link libraries
if(BUILD_ENV_MSVC)
    set(EXTRA_LIBS
        tnzcore
        tnzbase
        toonzlib
        ws2_32.lib
    )
    target_link_libraries(tfarm
        Qt5::Core
        Qt5::Gui
        Qt5::OpenGL
        Qt5::Network
        ${GLUT_LIB}
        ${GL_LIB}
        ${EXTRA_LIBS}
    )
elseif(BUILD_ENV_APPLE)
    set(EXTRA_LIBS
        "$<TARGET_FILE:tnzcore>"
        "$<TARGET_FILE:tnzbase>"
        "$<TARGET_FILE:toonzlib>"
    )
    target_link_libraries(tfarm
        Qt5::Core
        Qt5::Gui
        Qt5::OpenGL
        Qt5::Network
        ${GLUT_LIB}
        ${GL_LIB}
        ${EXTRA_LIBS}
    )
elseif(BUILD_ENV_UNIXLIKE)
    _find_toonz_library(EXTRA_LIBS "tnzcore;tnzbase;toonzlib;tnzext")
    if(BUILD_TARGET_WIN)
        list(APPEND EXTRA_LIBS -lws2_32)
    endif()
    target_link_libraries(tfarm
        Qt5::Core
        Qt5::Gui
        Qt5::OpenGL
        Qt5::Network
        ${GLUT_LIB}
        ${GL_LIB}
        ${EXTRA_LIBS}
    )
endif()