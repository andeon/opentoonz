# Define headers
set(HEADERS
    ../include/colorfx.h
    colorfxutils.h
    rasterstyles.h
    regionstyles.h
    strokestyles.h
    zigzagstyles.h
    flowlinestrokestyle.h
)

# Define source files
set(SOURCES
    colorfx.cpp
    colorfxutils.cpp
    rasterstyles.cpp
    regionstyles.cpp
    strokestyles.cpp
    zigzagstyles.cpp
    flowlinestrokestyle.cpp
)

# Translation support
if(WITH_TRANSLATION)
    add_translation(colorfx ${HEADERS} ${SOURCES})
endif()

# Create the shared library
add_library(colorfx SHARED
    ${HEADERS}
    ${SOURCES}
)

# Add definitions
target_compile_definitions(colorfx PRIVATE
    _USRDLL
    COLORFX_EXPORTS
    USE_MESA
)

# Set linker flags for Apple
if(BUILD_ENV_APPLE)
    set_target_properties(colorfx PROPERTIES
        LINK_FLAGS "-Wl,-install_name,@executable_path/libcolorfx.dylib"
    )
    add_dependencies(colorfx tnzcore tnzbase)
endif()

# Debug messages
message(STATUS "Subdirectory: colorfx")
message(STATUS "Shared library suffix: ${CMAKE_SHARED_LIBRARY_SUFFIX}")
message(STATUS "Binary dir: ${CMAKE_CURRENT_BINARY_DIR}")

# Find dependent libraries
_find_toonz_library(EXTRA_LIBS "tnzcore;tnzbase")
message(STATUS "Found libraries: ${EXTRA_LIBS}")

# Link libraries
target_link_libraries(colorfx
    Qt5::Core
    Qt5::Gui
    ${GL_LIB}
    ${EXTRA_LIBS}
)