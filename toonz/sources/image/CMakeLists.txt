# Define headers
set(HEADERS
    sgi/tiio_sgi.h
    pli/pli_io.h
    pli/tiio_pli.h
    tga/tiio_tga.h
    compatibility/inforegion.h
    compatibility/tfile_io.h
    compatibility/tnz4.h
    quantel/filequantel.h
    quantel/filequantelP.h
    quantel/tiio_quantel.h
    png/tiio_png.h
    psd/tiio_psd.h
    svg/tiio_svg.h
    tzl/tiio_tzl.h
    ../include/tnzimage.h
    mov/tiio_mov_proxy.h
    3gp/tiio_3gp_proxy.h
    ffmpeg/tiio_gif.h
    ffmpeg/tiio_webm.h
    ffmpeg/tiio_apng.h
    ffmpeg/tiio_mp4.h
    ffmpeg/tiio_ffmpeg.h
    ffmpeg/tiio_ff_mov.h
    sprite/tiio_sprite.h
    mesh/tiio_mesh.h
    exr/tinyexr_otmod.h
    exr/tiio_exr.h
    tzm/tiio_tzm.h
)

# Define source files
set(SOURCES
    tiio.cpp
    sgi/filesgi.cpp
    pli/pli_io.cpp
    pli/tags.cpp
    pli/tiio_pli.cpp
    tga/tiio_tga.cpp
    compatibility/inforegion.c
    compatibility/tfile_io.c
    compatibility/tnz4.c
    compatibility/tnz4_cpp.cpp
    quantel/filequantel.c
    quantel/tiio_quantel.cpp
    png/tiio_png.cpp
    psd/tiio_psd.cpp
    svg/tiio_svg.cpp
    tzl/tiio_tzl.cpp
    mov/tiio_mov_proxy.cpp
    3gp/tiio_3gp_proxy.cpp
    ffmpeg/tiio_gif.cpp
    ffmpeg/tiio_webm.cpp
    ffmpeg/tiio_apng.cpp
    ffmpeg/tiio_mp4.cpp
    ffmpeg/tiio_ffmpeg.cpp
    ffmpeg/tiio_ff_mov.cpp
    sprite/tiio_sprite.cpp
    mesh/tiio_mesh.cpp
    exr/tiio_exr.cpp
    tzm/tiio_tzm.cpp
)

# Conditional headers and sources for non-BSD targets
if(NOT BUILD_TARGET_BSD)
    list(APPEND HEADERS
        tif/tiio_tif.h
        tzp/tiio_plt.h
        tzp/tiio_tzp.h
        tzp/toonztags.h
    )
    list(APPEND SOURCES
        tif/tiio_tif.cpp
        tzp/tiio_plt.cpp
        tzp/tiio_tzp.cpp
        tzp/toonzrle.cpp
        tzp/avl.c
    )
endif()

# Platform-specific headers and sources
if(BUILD_TARGET_WIN)
    list(APPEND HEADERS
        avi/tiio_avi.h
        mov/tiio_mov.h
        3gp/tiio_3gp.h
    )
    list(APPEND SOURCES
        avi/tiio_avi.cpp
        mov/tiio_movW.cpp
        3gp/tiio_3gpW.cpp
    )
elseif(BUILD_TARGET_APPLE)
    list(APPEND HEADERS
        mov/tiio_movM.h
        3gp/tiio_3gpM.h
    )
    list(APPEND SOURCES
        mov/tiio_movM.cpp
        3gp/tiio_3gpM.cpp
    )
elseif(BUILD_TARGET_UNIX)
    list(APPEND HEADERS
        3gp/tiio_3gp_proxy.h
        mov/tiio_mov_proxy.h
    )
    list(APPEND SOURCES
        mov/tiio_mov_proxy.cpp
        3gp/tiio_3gp_proxy.cpp
    )
endif()

# Translation support
if(WITH_TRANSLATION)
    add_translation(image ${HEADERS} ${SOURCES})
endif()

# Create the shared library
add_library(image SHARED
    ${HEADERS}
    ${SOURCES}
)

# Add definitions
target_compile_definitions(image PRIVATE
    TNZIMAGE_DLL
    _USRDLL
    IMAGE_EXPORTS
)

# Set linker flags for Apple
if(BUILD_ENV_APPLE)
    set_target_properties(image PROPERTIES
        LINK_FLAGS "-Wl,-install_name,@executable_path/libimage.dylib"
    )
    add_dependencies(image tnzcore tnzbase toonzlib)
endif()

# Debug messages
message(STATUS "Subdirectory: image")
message(STATUS "Binary dir: ${CMAKE_CURRENT_BINARY_DIR}")

# Include directories
target_include_directories(image SYSTEM PRIVATE
    ${TIFF_INCLUDE_DIR}
)

if(NOT BUILD_TARGET_BSD)
    target_include_directories(image PRIVATE
        ${SDKROOT}/LibJPEG/jpeg-9
    )
endif()

if(BUILD_ENV_MSVC)
    target_include_directories(image SYSTEM PRIVATE
        ${SDKROOT}/libpng-1.6.21
    )
endif()

if(BUILD_TARGET_WIN AND PLATFORM EQUAL 32)
    target_include_directories(image SYSTEM PRIVATE
        ${SDKROOT}/quicktime/QT73SDK/CIncludes
    )
endif()

target_include_directories(image PRIVATE
    ${SDKROOT}/tinyexr
)

# Find dependent Toonz libraries
_find_toonz_library(TNZLIBS "tnzcore;tnzbase;toonzlib")

# Define and link libraries
if(BUILD_ENV_MSVC)
    set(EXTRA_LIBS
        ${TNZLIBS}
        vfw32.lib
    )
elseif(BUILD_ENV_APPLE)
    if(PLATFORM EQUAL 32)
        find_library(CORE_SERVICES_LIB CoreServices)
        set(OSX_10_6_SDK_PATH "" CACHE PATH "Path to OSX 10.6 SDK")
        if(NOT OSX_10_6_SDK_PATH)
            message(FATAL_ERROR "Set variable 'OSX_10_6_SDK_PATH' to the path of the OSX 10.6 SDK")
        endif()
        find_library(QD_LIB
            NAMES QD
            PATHS ${OSX_10_6_SDK_PATH}
            NO_DEFAULT_PATH
        )
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -F ${OSX_10_6_SDK_PATH}")
    endif()
    set(EXTRA_LIBS
        Qt5::Widgets
        ${TNZLIBS}
        ${QT_LIB}
        ${CARBON_LIB}
        ${CORE_SERVICES_LIB}
        ${QD_LIB}
    )
elseif(BUILD_ENV_UNIXLIKE)
    set(EXTRA_LIBS ${TNZLIBS})
    if(BUILD_TARGET_WIN)
        list(APPEND EXTRA_LIBS -lvfw32)
    endif()
endif()

target_link_libraries(image
    Qt5::Core
    Qt5::Gui
    Qt5::Network
    ${Z_LIB}
    ${GLUT_LIB}
    ${GL_LIB}
    ${JPEG_LIB}
    ${TIFF_LIB}
    ${PNG_LIB}
    ${EXTRA_LIBS}
)