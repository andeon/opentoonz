# Define MOC headers for Qt processing
set(MOC_HEADERS
    ../include/tcacheresourcepool.h
    ../common/tfx/trendererP.h
)

# Combine headers with MOC headers
set(HEADERS
    ${MOC_HEADERS}
    ../common/twain/ttwain.h
    ../common/twain/ttwain_capability.h
    ../common/twain/ttwain_conversion.h
    ../common/twain/ttwain_error.h
    ../common/twain/ttwain_global_def.h
    ../common/twain/ttwain_state.h
    ../common/twain/ttwain_statePD.h
    ../common/twain/ttwain_util.h
    ../common/twain/ttwain_utilP.h
    ../common/twain/ttwain_utilPD.h
    ../common/twain/ttwain_win.h
    ../common/twain/ttwain_winPD.h
    ../common/twain/ttwainP.h
    ../common/twain/twain.h
    ../include/permissionsmanager.h
    ../include/tscanner.h
    tscanner/tscannerutil.h
    tscanner/tscannerepson.h
    tscanner/tscannertwain.h
    tscanner/TScannerIO/TScannerIO.h
    tscanner/TScannerIO/TUSBScannerIO.h
    ../include/tcubicbezier.h
    ../include/tdoublekeyframe.h
    ../include/tdoubleparam.h
    ../common/tparam/tdoubleparamfile.h
    ../include/tdoubleparamrelayproperty.h
    ../include/tfxparam.h
    ../include/tnotanimatableparam.h
    ../include/tparam.h
    ../include/tparamchange.h
    ../include/tparamcontainer.h
    ../include/tparamset.h
    ../include/tspectrumparam.h
    ../include/ttonecurveparam.h
    ../include/tcolumnset.h
    ../include/tbasefx.h
    ../include/texternfx.h
    ../include/tfx.h
    ../include/tfxattributes.h
    ../include/tcacheresource.h
    ../include/tpassivecachemanager.h
    ../include/tpredictivecachemanager.h
    ../include/tfxcachemanager.h
    ../include/tfxutil.h
    ../include/tmacrofx.h
    ../include/trenderer.h
    ../include/trenderresourcemanager.h
    ../include/ttzpimagefx.h
    ../include/tcli.h
    ../include/tcolorutils.h
    ../include/tparamundo.h
    ../include/ttest.h
    ../include/texpression.h
    ../include/tgrammar.h
    ../include/tparser.h
    ../include/ttokenizer.h
    ../include/tunit.h
    ../include/tzeraryfx.h
)

# Define source files
set(SOURCES
    permissionsmanager.cpp
    stringtable.cpp
    ../common/tparam/tcubicbezier.cpp
    ../common/tparam/tdoublekeyframe.cpp
    ../common/tparam/tdoubleparam.cpp
    ../common/tparam/tdoubleparamfile.cpp
    ../common/tparam/tdoubleparamrelayproperty.cpp
    ../common/tparam/tnotanimatableparam.cpp
    ../common/tparam/tparam.cpp
    ../common/tparam/tparamcontainer.cpp
    ../common/tparam/tparamset.cpp
    ../common/tparam/tpixelparam.cpp
    ../common/tparam/tpointparam.cpp
    ../common/tparam/trangeparam.cpp
    ../common/tparam/tspectrumparam.cpp
    ../common/tparam/ttonecurveparam.cpp
    ../common/txsheet/tcolumnset.cpp
    ../common/tfx/binaryFx.cpp
    texternfx.cpp
    ../common/tfx/tfx.cpp
    ../common/tfx/tfxcachemanager.cpp
    ../common/tfx/tcacheresource.cpp
    ../common/tfx/tcacheresourcepool.cpp
    ../common/tfx/tpassivecachemanager.cpp
    ../common/tfx/tpredictivecachemanager.cpp
    tfxattributes.cpp
    tfxutil.cpp
    ../common/tfx/tmacrofx.cpp
    trasterfx.cpp
    ../common/tfx/trenderer.cpp
    ../common/tfx/trenderresourcemanager.cpp
    ../common/tfx/ttzpimagefx.cpp
    ../common/tfx/unaryFx.cpp
    ../common/tfx/zeraryFx.cpp
    ../common/tapptools/tcli.cpp
    ../common/tapptools/tcolorutils.cpp
    ../common/tapptools/tparamundo.cpp
    ../common/ttest/ttest.cpp
    ../common/expressions/texpression.cpp
    ../common/expressions/tgrammar.cpp
    ../common/expressions/tparser.cpp
    ../common/expressions/ttokenizer.cpp
    ../common/tunit/tunit.cpp
    tscanner/tscannerutil.cpp
    tscanner/tscannerepson.cpp
)

# Platform-specific source files
if(BUILD_TARGET_WIN)
    list(APPEND SOURCES tscanner/TScannerIO/TUSBScannerIO_W.cpp)
else()
    list(APPEND SOURCES tscanner/TScannerIO/TUSBScannerIO_M.cpp)
endif()

# Define Objective-C sources
set(OBJCSOURCES
    ../common/twain/ttwain_capability.c
    ../common/twain/ttwain_conversion.c
    ../common/twain/ttwain_error.c
    ../common/twain/ttwain_state.c
    ../common/twain/ttwain_util.c
    ../common/twain/ttwain_win.c
    tscanner/tscanner.cpp
    tscanner/tscannertwain.cpp
)

# Platform-specific Objective-C sources
if(BUILD_TARGET_WIN)
    list(APPEND OBJCSOURCES
        ../common/twain/ttwain_stateW.c
        ../common/twain/ttwain_utilW.c
        ../common/twain/ttwain_winW.c
    )
elseif(BUILD_TARGET_APPLE)
    list(APPEND OBJCSOURCES
        ../common/twain/ttwain_stateM.c
        ../common/twain/ttwain_utilM.c
        ../common/twain/ttwain_winM.c
    )
elseif(BUILD_TARGET_UNIX)
    list(APPEND OBJCSOURCES
        ../common/twain/ttwain_stateX.c
        ../common/twain/ttwain_utilX.c
        ../common/twain/ttwain_winX.c
    )
endif()

# Handle Objective-C++ compilation on Apple
if(BUILD_ENV_APPLE)
    set_source_files_properties(${OBJCSOURCES} PROPERTIES COMPILE_FLAGS "-x objective-c++")
endif()

# Process MOC headers with Qt
qt5_wrap_cpp(MOC_SOURCES ${MOC_HEADERS})

# Create the shared library
add_library(tnzbase SHARED
    ${HEADERS}
    ${SOURCES}
    ${MOC_SOURCES}
    ${OBJCSOURCES}
)

# Set linker flags for Apple
if(BUILD_ENV_APPLE)
    set_target_properties(tnzbase PROPERTIES
        LINK_FLAGS "-Wl,-install_name,@executable_path/libtnzbase.dylib"
    )
    add_dependencies(tnzbase tnzcore)
endif()

# Add definitions
target_compile_definitions(tnzbase PRIVATE
    TNZBASE_EXPORTS
    TPARAM_EXPORTS
    TXSHEET_EXPORTS
    TFX_EXPORTS
    TAPPTOOLS_EXPORTS
    TTEST_EXPORTS
)

# Debug messages
message(STATUS "Subdirectory: tnzbase")
message(STATUS "Binary dir: ${CMAKE_CURRENT_BINARY_DIR}")

# Include directories
target_include_directories(tnzbase SYSTEM PRIVATE
    ../common/flash
    ${SDKROOT}/libusb/libusb-1.0.9/include
)

# Define extra libraries based on platform
if(BUILD_ENV_MSVC)
    set(EXTRA_LIBS tnzcore)
elseif(BUILD_ENV_APPLE)
    find_library(TWAIN_LIB TWAIN)
    find_library(IOKIT_LIB IOKit)
    find_library(COCOA_LIB Cocoa)
    set(EXTRA_LIBS
        "$<TARGET_FILE:tnzcore>"
        ${USB_LIB}
        ${TWAIN_LIB}
        ${IOKIT_LIB}
        ${COCOA_LIB}
    )
elseif(BUILD_ENV_UNIXLIKE)
    _find_toonz_library(EXTRA_LIBS "tnzcore")
endif()

# Link libraries
target_link_libraries(tnzbase
    Qt5::Core
    Qt5::Gui
    ${EXTRA_LIBS}
)
