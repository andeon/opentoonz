cmake_minimum_required(VERSION 3.10)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

project(OpenToonz LANGUAGES C CXX)

#-----------------------------------------------------------------------------
# Platform Specific Defaults

# Select build target
set(BUILD_TARGET_WIN OFF)
set(BUILD_TARGET_APPLE OFF)
set(BUILD_TARGET_UNIX OFF)

if(WIN32)
    set(BUILD_TARGET_WIN ON)
    message(STATUS "Target system is WIN")
elseif(APPLE)
    set(BUILD_TARGET_APPLE ON)
    message(STATUS "Target system is APPLE")
elseif(UNIX)
    set(BUILD_TARGET_UNIX ON)
    message(STATUS "Target system is UNIX")
else()
    set(BUILD_TARGET_UNIX ON)
    message(WARNING "Target system unknown, set UNIX by default")
    message(STATUS "Target system is UNIX (actually unknown)")
endif()

#-----------------------------------------------------------------------------
# Select build environment

set(BUILD_ENV_MSVC OFF)
set(BUILD_ENV_APPLE OFF)
set(BUILD_ENV_UNIXLIKE OFF)

if(MSVC)
    set(BUILD_ENV_MSVC ON)
    if(NOT BUILD_TARGET_WIN)
        message(WARNING "Build environment is MSVC, but target system is not WIN")
    endif()
    message(STATUS "Build environment is MSVC")
elseif(CMAKE_HOST_APPLE)
    set(BUILD_ENV_APPLE ON)
    if(NOT BUILD_TARGET_APPLE)
        message(WARNING "Build environment is APPLE, but target system is not APPLE")
    endif()
    message(STATUS "Build environment is APPLE")
elseif(CMAKE_HOST_UNIX OR CMAKE_COMPILER_IS_GNUCC)
    set(BUILD_ENV_UNIXLIKE ON)
    if(NOT BUILD_TARGET_UNIX AND NOT BUILD_TARGET_WIN)
        message(WARNING "Build environment is UNIXLIKE, but target system is not UNIX or WIN")
    endif()
    message(STATUS "Build environment is UNIXLIKE")
else()
    message(FATAL_ERROR "Unsupported build environment, only MSVC, APPLE and UNIXLIKE are supported")
endif()

#-----------------------------------------------------------------------------
# Build Options

option(WITH_SYSTEM_LZO "Use the system LZO library instead of 'thirdparty'" OFF)
option(WITH_SYSTEM_SUPERLU "Use the system SuperLU library instead of 'thirdparty'" OFF)
option(WITH_CANON "Build with Canon DSLR support - Requires Canon SDK" OFF)
option(WITH_TRANSLATION "Generate translation projects as well" ON)
option(WITH_WINTAB "(Windows only) Build with customized Qt with WinTab support." OFF)

#-----------------------------------------------------------------------------
# Platform Architecture

if(BUILD_ENV_APPLE)
    if(NOT PLATFORM)
        set(PLATFORM 64)
    endif()
else()
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(PLATFORM 64)
        set(PLATFORM2 _64)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(PLATFORM 32)
        set(PLATFORM2)
    else()
        message(WARNING "CMAKE_SIZEOF_VOID_P = ${CMAKE_SIZEOF_VOID_P}")
        set(PLATFORM 64)
        set(PLATFORM2 _64)
    endif()
endif()
message(STATUS "PLATFORM: ${PLATFORM}")

#-----------------------------------------------------------------------------
# Third Party & System Libs

include(${CMAKE_SOURCE_DIR}/../cmake/util_compiler.cmake)

get_filename_component(SDKROOT ${CMAKE_SOURCE_DIR}/../../thirdparty ABSOLUTE)
message(STATUS "SDK Root: ${SDKROOT}")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")

if(BUILD_ENV_APPLE)
    set(THIRDPARTY_LIBS_HINTS "/usr/local/Cellar/" "/opt/include" ${SDKROOT})
elseif(BUILD_ENV_MSVC OR BUILD_ENV_UNIXLIKE)
    set(THIRDPARTY_LIBS_HINTS ${SDKROOT})
endif()

if(BUILD_ENV_MSVC)
    # Configure the third party libs (only once) to avoid manual copies
    message(STATUS "Configuring the third party libs")
    if(NOT EXISTS "${SDKROOT}/libpng-1.6.21/pnglibconf.h")
        file(COPY "${SDKROOT}/libpng-1.6.21/scripts/pnglibconf.h.prebuilt" DESTINATION "${SDKROOT}/libpng-1.6.21")
        file(RENAME "${SDKROOT}/libpng-1.6.21/pnglibconf.h.prebuilt" "${SDKROOT}/libpng-1.6.21/pnglibconf.h")
    endif()
    if(NOT EXISTS "${SDKROOT}/tiff-4.0.3/libtiff/tif_config.h")
        file(COPY "${SDKROOT}/tiff-4.0.3/libtiff/tif_config.vc.h" DESTINATION "${SDKROOT}/tiff-4.0.3/libtiff/tif_config.h")
    endif()
    if(NOT EXISTS "${SDKROOT}/tiff-4.0.3/libtiff/tiffconf.h")
        file(COPY "${SDKROOT}/tiff-4.0.3/libtiff/tiffconf.vc.h" DESTINATION "${SDKROOT}/tiff-4.0.3/libtiff/tiffconf.h")
    endif()
endif()

message(STATUS "Thirdparty Library Search path: ${THIRDPARTY_LIBS_HINTS}")

#-----------------------------------------------------------------------------
# Qt Configuration

if(BUILD_ENV_MSVC)
    set(QT_PATH "C:/Qt/5.15.2/msvc2019${PLATFORM2}" CACHE PATH "Qt installation directory")
    if(NOT EXISTS ${QT_PATH})
        message(FATAL_ERROR "Specify QT_PATH properly")
    endif()
    set(QT_LIB_PATH ${QT_PATH})
    list(APPEND CMAKE_PREFIX_PATH "${QT_PATH}/lib/cmake/")
    add_compile_options(/wd4251)
    add_definitions(-DVC_EXTRALEAN -DNOMINMAX -D_USE_MATH_DEFINES)
elseif(BUILD_ENV_APPLE)
    set(QT_PATH "~/Qt5.9.2/5.9.2/clang_${PLATFORM}/lib" CACHE PATH "Qt installation directory")
    set(QT_LIB_PATH "${QT_PATH}/")
    list(APPEND CMAKE_PREFIX_PATH "${QT_LIB_PATH}cmake/")
    add_definitions(-DMACOSX -Di386 -D__MACOS__)
    if(PLATFORM EQUAL 64)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -std=c++11 -stdlib=libc++ -fno-implicit-templates")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
    elseif(PLATFORM EQUAL 32)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32 -std=c++11 -stdlib=libc++ -fno-implicit-templates -D HAS_QUICKDRAW")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
        set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -m32")
    else()
        message(FATAL_ERROR "Invalid PLATFORM: ${PLATFORM}. 'PLATFORM' must be 32 or 64.")
    endif()
elseif(BUILD_ENV_UNIXLIKE)
    set(CMAKE_CXX_STANDARD 11)
    find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)
    add_definitions(-DLINUX)
endif()

#-----------------------------------------------------------------------------
# Include Directories

include_directories(
    include
    ${Boost_INCLUDE_DIR}
    ${LZ4_LIB_INCLUDE_DIRS}
    ${USB_LIB_INCLUDE_DIRS}
    ${SUPERLU_INCLUDE_DIR}
    ${JPEG_INCLUDE_DIR}
    ${MYPAINT_LIB_INCLUDE_DIRS}
)

#-----------------------------------------------------------------------------
# Library Configuration

if(BUILD_ENV_MSVC)
    set(GLUT_LIB ${SDKROOT}/glut/3.7.6/lib/glut${PLATFORM}.lib)
    set(GL_LIB opengl32.lib)
    set(Z_LIB
        optimized ${SDKROOT}/zlib-1.2.8/lib/zlib-1.2.8_2015${PLATFORM2}.lib
        debug ${SDKROOT}/zlib-1.2.8/lib/zlib-1.2.8_2015${PLATFORM2}d.lib
    )
    set(JPEG_LIB ${SDKROOT}/libjpeg-turbo/lib/jpeg-static_2015${PLATFORM2}.lib)
    set(TURBOJPEG_LIB ${SDKROOT}/libjpeg-turbo/lib/turbojpeg-static_2015${PLATFORM2}.lib)
    set(TIFF_LIB
        optimized ${SDKROOT}/tiff-4.0.3/lib/LibTIFF-4.0.3_2015${PLATFORM2}.lib
        debug ${SDKROOT}/tiff-4.0.3/lib/LibTIFF-4.0.3_2015${PLATFORM2}d.lib
    )
    set(PNG_LIB
        optimized ${SDKROOT}/libpng-1.6.21/lib/libpng16_2015${PLATFORM2}.lib
        debug ${SDKROOT}/libpng-1.6.21/lib/libpng16_2015${PLATFORM2}d.lib
    )
    set(GLEW_LIB ${SDKROOT}/glew/glew-1.9.0/lib/glew${PLATFORM}.lib)
    set(LZ4_LIB ${SDKROOT}/Lz4/Lz4_131/lz4_${PLATFORM}.lib)
    set(SUPERLU_LIB ${SDKROOT}/superlu/SuperLU_2015_${PLATFORM}.lib)
    set(OPENBLAS_LIB ${SDKROOT}/openblas/libopenblas_${PLATFORM}.lib)
    set(USB_LIB)  # unused
    if(PLATFORM EQUAL 32)
        set(QT_LIB ${SDKROOT}/quicktime/QT73SDK/Libraries/QTMLClient.lib)
    endif()
    set(MYPAINT_LIB_INCLUDE_DIRS ${SDKROOT}/libmypaint/dist/${PLATFORM}/include/libmypaint)
    set(MYPAINT_LIB_LDFLAGS ${SDKROOT}/libmypaint/dist/${PLATFORM}/libmypaint.lib)
    if(PLATFORM EQUAL 64 AND WITH_CANON)
        set(CANON_LIB ${SDKROOT}/canon/library/EDSDK.lib)
    endif()
elseif(BUILD_ENV_APPLE)
    find_library(GLUT_LIB GLUT)
    find_library(GL_LIB OpenGL)
    find_library(Z_LIB z)
    find_package(TIFF REQUIRED)
    set(TIFF_LIB ${TIFF_LIBRARY})
    find_package(PNG REQUIRED)
    set(PNG_LIB ${PNG_LIBRARY})
    find_package(GLEW)
    find_package(SuperLU REQUIRED)
    set(SUPERLU_LIB ${SUPERLU_LIBRARY})
    find_package(JPEG REQUIRED)
    find_package(PkgConfig)
    if(GLEW-NOTFOUND)
        pkg_check_modules(GLEW REQUIRED glew)
    endif()
    set(GLEW_LIB ${GLEW_LIBRARIES})
    pkg_check_modules(LZ4_LIB REQUIRED liblz4)
    set(LZ4_LIB ${LZ4_LIB_LDFLAGS})
    find_package(LZO REQUIRED)
    if(PLATFORM EQUAL 64 AND WITH_CANON)
        set(CANON_LIB ${SDKROOT}/canon/Framework/EDSDK.framework)
    endif()
elseif(BUILD_ENV_UNIXLIKE)
    find_library(GL_LIB GL)
    find_library(GLU_LIB GLU)
    find_library(GLUT_LIB glut)
    find_library(Z_LIB z)
    find_package(TIFF REQUIRED)
    find_package(PNG REQUIRED)
    set(PNG_LIB ${PNG_LIBRARY})
    find_package(GLEW)
    find_package(SuperLU REQUIRED)
    set(SUPERLU_LIB ${SUPERLU_LIBRARY})
    find_package(JPEG REQUIRED)
    find_package(PkgConfig)
    if(GLEW-NOTFOUND)
        pkg_check_modules(GLEW REQUIRED glew)
    endif()
    set(GLEW_LIB ${GLEW_LIBRARIES})
    pkg_check_modules(LZ4_LIB REQUIRED liblz4)
    set(LZ4_LIB ${LZ4_LIB_LDFLAGS})
    find_package(LZO REQUIRED)
    if(PLATFORM EQUAL 64)
        find_library(TURBOJPEG_LIB turbojpeg)
    endif()
endif()

#-----------------------------------------------------------------------------
# Endianness Check

include(TestBigEndian)
test_big_endian(_SYSTEM_BIG_ENDIAN)
if(_SYSTEM_BIG_ENDIAN)
    add_definitions(-DTNZ_LITTLE_ENDIAN=0)
else()
    add_definitions(-DTNZ_LITTLE_ENDIAN=1)
endif()
unset(_SYSTEM_BIG_ENDIAN)

#-----------------------------------------------------------------------------
# Compiler Flags

if(CMAKE_COMPILER_IS_GNUCC)
    add_compile_options(-Wwrite-strings -Wundef -Wmissing-declarations)
endif()

#-----------------------------------------------------------------------------
# Output Directories

if(BUILD_ENV_MSVC)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    add_compile_options(/MP /D _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1 /D _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT=1)
elseif(BUILD_ENV_UNIXLIKE)
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/opentoonz)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/opentoonz)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        set(CMAKE_INSTALL_PREFIX "/opt/opentoonz" CACHE PATH "OpenToonz install prefix" FORCE)
    endif()
endif()

#-----------------------------------------------------------------------------
# Translation Support

if(WITH_TRANSLATION)
    set(LANGUAGES japanese italian french spanish chinese german russian korean czech)
    function(add_translation module)
        set(translation)
        foreach(lang ${LANGUAGES})
            set(name "${CMAKE_SOURCE_DIR}/translations/${lang}/${module}.ts")
            list(APPEND translation ${name})
            set_source_files_properties(${name} PROPERTIES
                OUTPUT_LOCATION "${CMAKE_BINARY_DIR}/loc/${lang}"
            )
        endforeach()
        qt5_create_translation(message ${translation} ${ARGN})
        add_custom_target("translation_${module}" DEPENDS ${message})
        set_target_properties("translation_${module}" PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    endfunction()
endif()

#-----------------------------------------------------------------------------
# Subdirectories

add_subdirectory(tnzcore)
add_subdirectory(tnzbase)
add_subdirectory(tnzext)
add_subdirectory(toonzlib)
add_subdirectory(toonzfarm/tfarm)
add_subdirectory(stdfx)
add_subdirectory(sound)
add_subdirectory(colorfx)
add_subdirectory(image)
add_subdirectory(toonzqt)
add_subdirectory(tnztools)

add_subdirectory(${SDKROOT}/lzo/driver lzodriver)

add_subdirectory(toonz)
add_subdirectory(tcleanupper)
add_subdirectory(tcomposer)
add_subdirectory(tconverter)
add_subdirectory(toonzfarm)

if(BUILD_ENV_APPLE)
    add_subdirectory(mousedragfilter)
endif()

if((PLATFORM EQUAL 32) AND ((BUILD_TARGET_WIN AND BUILD_ENV_MSVC) OR BUILD_TARGET_APPLE))
    add_subdirectory(t32bitsrv)
endif()

if(BUILD_ENV_APPLE)
    add_custom_command(TARGET executable
        POST_BUILD COMMAND
        ${CMAKE_INSTALL_NAME_TOOL} -add_rpath ${CMAKE_INSTALL_RPATH}
        $<TARGET_FILE:executable>
    )
    set_target_properties(OpenToonz PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/../cmake/BundleInfo.plist.in)
endif()

if(BUILD_ENV_UNIXLIKE AND NOT BUILD_TARGET_WIN)
    add_subdirectory(xdg-data)
endif()
