# Define headers
set(HEADERS
    wav/tsio_wav.h
    aiff/tsio_aiff.h
    raw/tsio_raw.h
    ffmpeg/tsio_ffmpeg.h
    ../include/tnzsound.h
)

# Define source files
set(SOURCES
    tsio.cpp
    tsioutils.cpp
    wav/tsio_wav.cpp
    aiff/tsio_aiff.cpp
    raw/tsio_raw.cpp
    ffmpeg/tsio_ffmpeg.cpp
)

# Create the shared library
add_library(sound SHARED
    ${HEADERS}
    ${SOURCES}
)

# Add definitions
target_compile_definitions(sound PRIVATE
    SOUND_EXPORTS
)

# Set linker flags for Apple
if(BUILD_ENV_APPLE)
    set_target_properties(sound PROPERTIES
        LINK_FLAGS "-Wl,-install_name,@executable_path/libsound.dylib"
    )
    add_dependencies(sound tnzcore tnzbase toonzlib)
endif()

# Debug messages
message(STATUS "Subdirectory: sound")
message(STATUS "Shared library suffix: ${CMAKE_SHARED_LIBRARY_SUFFIX}")
message(STATUS "Binary dir: ${CMAKE_CURRENT_BINARY_DIR}")

# Find dependent libraries
_find_toonz_library(EXTRA_LIBS "tnzcore;tnzbase;toonzlib")
message(STATUS "Found libraries: ${EXTRA_LIBS}")

# Link libraries
target_link_libraries(sound
    Qt5::Core
    ${EXTRA_LIBS}
)