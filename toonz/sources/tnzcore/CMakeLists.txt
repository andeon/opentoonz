# Define MOC headers for Qt processing
set(MOC_HEADERS
    ../include/tundo.h
    ../include/tthread.h
    ../common/tcore/tthreadp.h
    ../include/tipcsrv.h
    ../include/tipcsrvP.h
    ../include/tmsgcore.h
    ../include/tfunctorinvoker.h
)

# Define headers
set(HEADERS
    ${MOC_HEADERS}
    ../common/trop/loop_macros.h
    ../common/trop/optimize_for_lp64.h
    ../common/trop/quickputP.h
    ../common/tiio/compatibility/tfile_io.h
    ../common/tiio/bmp/filebmp.h
    ../include/movsettings.h
    ../include/tconst.h
    ../include/transparencycheck.h
    ../include/trastercm.h
    ../include/trasterfx.h
    ../include/ttile.h
    ../common/psdlib/psd.h
    ../common/psdlib/psdutils.h
    ../common/trop/runsmap.h
    ../common/tvectorimage/tvectorimageP.h
    ../common/tvectorimage/tsegmentadjuster.h
    ../common/tvectorimage/tl2lautocloser.h
    ../common/tvrender/tellipticbrushP.h
    ../include/tatomicvar.h
    ../include/tcommon.h
    ../include/tconvert.h
    ../include/tdata.h
    ../include/tdebugmessage.h
    ../include/texception.h
    ../include/texpression.h
    ../include/tmathutil.h
    ../include/tnztypes.h
    ../include/tproperty.h
    ../include/trandom.h
    ../include/tsmartpointer.h
    ../include/tstopwatch.h
    ../include/tthreadmessage.h
    ../include/tutil.h
    ../include/tcolorfunctions.h
    ../include/tcolorvalue.h
    ../include/tcolumnset.h
    ../include/tpixel.h
    ../include/tpixelcm.h
    ../include/tpixelgr.h
    ../include/tpixelutils.h
    ../include/tspectrum.h
    ../include/tcurves.h
    ../include/tcurveutil.h
    ../include/tgeometry.h
    ../include/traster.h
    ../include/timage.h
    ../include/tlevel.h
    ../include/tcontenthistory.h
    ../include/tfilepath.h
    ../include/tfilepath_io.h
    ../include/tfiletype.h
    ../include/timagecache.h
    ../include/tlogger.h
    ../include/tpluginmanager.h
    ../include/tsystem.h
    ../include/drawutil.h
    ../include/tregion.h
    ../include/tstroke.h
    ../include/tstrokeoutline.h
    ../include/tsweepboundary.h
    ../include/tvectorimage.h
    ../include/tgl.h
    ../include/tstencilcontrol.h
    ../include/tsop.h
    ../include/tsound.h
    ../include/tsound_io.h
    ../include/tsound_t.h
    ../include/tsoundsample.h
    ../include/timage_io.h
    ../include/timageinfo.h
    ../include/tlevel_io.h
    ../include/tcachedlevel.h
    ../include/tcodec.h
    ../include/trasterimage.h
    ../include/tcolorstyles.h
    ../include/tfont.h
    ../include/tinbetween.h
    ../include/tofflinegl.h
    ../include/qtofflinegl.h
    ../include/tpalette.h
    ../include/tpaletteutil.h
    ../include/tregionprop.h
    ../include/tsimplecolorstyles.h
    ../include/tstrokedeformations.h
    ../include/tstrokeprop.h
    ../include/tstrokeutil.h
    ../include/ttessellator.h
    ../include/tvectorgl.h
    ../include/tvectorbrushstyle.h
    ../include/tvectorrenderdata.h
    ../include/trop.h
    ../include/trop_borders.h
    ../include/tropcm.h
    ../include/tpersist.h
    ../include/tpersistset.h
    ../include/tstream.h
    ../include/tstreamexception.h
    ../include/tiio.h
    ../include/tiio_bmp.h
    ../include/tiio_jpg.h
    ../include/tiio_jpg_util.h
    ../include/tiio_jpg_exif.h
    ../include/tiio_std.h
    ../include/ttoonzimage.h
    ../include/tipc.h
    ../include/tipcmsg.h
    ../include/t32bitsrv_wrap.h
    ../include/tenv.h
    ../include/tmeshimage.h
    ../include/tgldisplaylistsmanager.h
    ../include/tversion.h
    ../include/tstringid.h
    ../include/tvariant.h
    ../include/tmetaimage.h
)

# Define source files
set(SOURCES
    ../common/tcore/tdata.cpp
    ../common/tcore/tdebugmessage.cpp
    ../common/tcore/texception.cpp
    ../common/tcore/threadmessage.cpp
    ../common/tcore/tmathutil.cpp
    ../common/tproperty.cpp
    ../common/tcore/trandom.cpp
    ../common/tcore/tsmartpointer.cpp
    ../common/tcore/tstopwatch.cpp
    ../common/tcore/tstring.cpp
    ../common/tcore/tthread.cpp
    ../common/tcore/tundo.cpp
    ../common/tcore/tfunctorinvoker.cpp
    ../common/tcolor/tcolorfunctions.cpp
    ../common/tcolor/tcolorvalue.cpp
    ../common/tcolor/tpixel.cpp
    ../common/tcolor/tpixelutils.cpp
    ../common/tcolor/tspectrum.cpp
    ../common/tgeometry/tcurves.cpp
    ../common/tgeometry/tcurveutil.cpp
    ../common/tgeometry/tgeometry.cpp
    ../common/traster/traster.cpp
    ../common/timage/timage.cpp
    ../common/timage/tlevel.cpp
    ../common/tsystem/cpuextensions.cpp
    ../common/tsystem/tbigmemorymanager.cpp
    ../common/tcontenthistory.cpp
    ../common/tsystem/tfilepath.cpp
    ../common/tsystem/tfilepath_io.cpp
    ../common/tsystem/tfiletype.cpp
    ../common/tcache/timagecache.cpp
    ../common/tsystem/tlogger.cpp
    ../common/tsystem/tpluginmanager.cpp
    ../common/tsystem/tsystem.cpp
    ../common/tvectorimage/cornerdetector.cpp
    ../common/tvectorimage/drawutil.cpp
    ../common/tvectorimage/tcomputeregions.cpp
    ../common/tvectorimage/tregion.cpp
    ../common/tvectorimage/tstroke.cpp
    ../common/tvectorimage/tstrokeoutline.cpp
    ../common/tvectorimage/tsegmentadjuster.cpp
    ../common/tvectorimage/tsweepboundary.cpp
    ../common/tvectorimage/tvectorimage.cpp
    ../common/tgl/tgl.cpp
    ../common/tgl/tstencilcontrol.cpp
    ../common/tgl/tgldisplaylistsmanager.cpp
    ../common/tsound/tsop.cpp
    ../common/tsound/tsound.cpp
    ../common/tsound/tsound_io.cpp
    ../common/timage_io/timage_io.cpp
    ../common/timage_io/tlevel_io.cpp
    ../common/trasterimage/tcodec.cpp
    ../common/trasterimage/trasterimage.cpp
    ../common/tvrender/tcolorstyles.cpp
    ../common/tvrender/tellipticbrush.cpp
    ../common/tvrender/qtofflinegl.cpp
    ../common/tvrender/tglcurves.cpp
    ../common/tvrender/tglregions.cpp
    ../common/tvrender/tinbetween.cpp
    ../common/tvrender/tofflinegl.cpp
    ../common/tvrender/tpalette.cpp
    ../common/tvrender/tpaletteutil.cpp
    ../common/tvrender/tregionprop.cpp
    ../common/tvrender/tsimplecolorstyles.cpp
    ../common/tvrender/tstrokedeformations.cpp
    ../common/tvrender/tstrokeprop.cpp
    ../common/tvrender/tstrokeutil.cpp
    ../common/tvrender/ttessellator.cpp
    ../common/tvrender/tvectorbrush.cpp
    ../common/tvrender/tvectorbrushstyle.cpp
    ../common/psdlib/psd.cpp
    ../common/psdlib/psdutils.cpp
    ../common/trop/bbox.cpp
    ../common/trop/brush.cpp
    ../common/trop/quickput.cpp
    ../common/trop/runsmap.cpp
    ../common/trop/tantialias.cpp
    ../common/trop/tblur.cpp
    ../common/trop/tcheckboard.cpp
    ../common/trop/tconvert.cpp
    ../common/trop/tconvolve.cpp
    ../common/trop/tdespeckle.cpp
    ../common/trop/tdistancetransform.cpp
    ../common/trop/terodilate.cpp
    ../common/trop/tinvert.cpp
    ../common/trop/toperators.cpp
    ../common/trop/tover.cpp
    ../common/trop/traylit.cpp
    ../common/trop/tresample.cpp
    ../common/trop/trgbmscale.cpp
    ../common/trop/trop.cpp
    ../common/trop/tropcm.cpp
    ../common/trop/trop_borders.cpp
    ../common/tstream/tstream.cpp
    ../common/tstream/tstreamexception.cpp
    ../common/tstream/tpersistset.cpp
    ../common/tiio/tiio.cpp
    ../common/tiio/tiio_bmp.cpp
    ../common/tiio/tiio_jpg.cpp
    ../common/tiio/tiio_jpg_util.cpp
    ../common/tiio/tiio_jpg_exif.cpp
    ../common/tiio/tiio_std.cpp
    ../common/tiio/movsettings.cpp
    ../common/tiio/compatibility/tfile_io.c
    ../common/tiio/bmp/filebmp.c
    ../common/trop/tfracmove.cpp
    ../common/ttoonzimage/ttonzimage.cpp
    ../common/tsystem/uncpath.cpp
    ../common/tvectorimage/tl2lautocloser.cpp
    ../common/tvectorimage/outlineApproximation.cpp
    ../common/tipc/tipc.cpp
    ../common/tipc/tipcmsg.cpp
    ../common/tipc/tipcsrv.cpp
    ../common/tipc/t32bitsrv_wrap.cpp
    ../common/tsystem/tsystempd.cpp
    ../common/tapptools/tenv.cpp
    ../common/tmeshimage/tmeshimage.cpp
    ../common/tmsgcore.cpp
    ../common/tvrender/tfont_qt.cpp
    ../common/tmetaimage/tmetaimage.cpp
    ../common/tstringid.cpp
    ../common/tvariant.cpp
)

# Platform-specific source files
if(BUILD_TARGET_WIN)
    list(APPEND SOURCES ../common/tsound/tsound_nt.cpp)
elseif(BUILD_TARGET_APPLE)
    list(APPEND SOURCES ../common/tsound/tsound_qt.cpp)
elseif(BUILD_TARGET_UNIX)
    list(APPEND SOURCES ../common/tsound/tsound_qt.cpp)
endif()

# Translation support
if(WITH_TRANSLATION)
    add_translation(tnzcore ${HEADERS} ${SOURCES})
endif()

# Process MOC headers
qt5_wrap_cpp(MOC_SOURCES ${MOC_HEADERS})

# Create the shared library
add_library(tnzcore SHARED
    ${HEADERS}
    ${SOURCES}
    ${MOC_SOURCES}
)

# Set linker flags for Apple
if(BUILD_ENV_APPLE)
    set_target_properties(tnzcore PROPERTIES
        LINK_FLAGS "-Wl,-install_name,@executable_path/libtnzcore.dylib"
    )
endif()

# Add definitions
target_compile_definitions(tnzcore PRIVATE
    TNZCORE_EXPORTS
    TCOLOR_EXPORTS
    TGEOMETRY_EXPORTS
    TRASTER_EXPORTS
    TIMAGE_EXPORTS
    TSYSTEM_EXPORTS
    TVECTORIMAGE_EXPORTS
    TGL_EXPORTS
    TSOUND_EXPORTS
    TIMAGE_IO_EXPORTS
    TRASTERIMAGE_EXPORTS
    TMETAIMAGE_EXPORTS
    TVRENDER_EXPORTS
    TFLASH_EXPORTS
    TROP_EXPORTS
    TSTREAM_EXPORTS
    TSTRINGID_EXPORTS
    TVARIANT_EXPORTS
)

# Platform-specific definitions
if(BUILD_TARGET_WIN)
    target_compile_definitions(tnzcore PRIVATE
        UNICODE
        _UNICODE
    )
endif()

# Debug messages
message(STATUS "Subdirectory: tnzcore")
message(STATUS "Binary dir: ${CMAKE_CURRENT_BINARY_DIR}")

# Include directories
target_include_directories(tnzcore SYSTEM PRIVATE
    ../common/flash
    ${SDKROOT}/Lz4/Lz4_131/lib/
)

if(BUILD_TARGET_WIN AND PLATFORM EQUAL 32)
    target_include_directories(tnzcore SYSTEM PRIVATE
        ${SDKROOT}/quicktime/QT73SDK/CIncludes
    )
endif()

if(BUILD_ENV_UNIXLIKE AND NOT BUILD_TARGET_WIN)
    target_include_directories(tnzcore SYSTEM PRIVATE
        ${FREETYPE_INCLUDE_DIRS}
    )
endif()

# Define extra libraries based on platform
if(BUILD_ENV_MSVC)
    set(EXTRA_LIBS
        netapi32.lib
        mpr.lib
        winmm.lib
        opengl32.lib
        glu32.lib
        dbghelp.lib
    )
elseif(BUILD_ENV_APPLE)
    find_library(AUDIOUNIT_LIB AudioUnit)
    find_library(AUDIOTB_LIB AudioToolbox)
    find_library(CARBON_LIB Carbon)
    set(EXTRA_LIBS
        ${AUDIOUNIT_LIB}
        ${AUDIOTB_LIB}
        ${CARBON_LIB}
    )
elseif(BUILD_ENV_UNIXLIKE)
    set(EXTRA_LIBS ${GLU_LIB})
    if(BUILD_TARGET_WIN)
        list(APPEND EXTRA_LIBS -lnetapi32 -lmpr -lwinmm -lpsapi -ldbghelp)
    endif()
endif()

# Link libraries
target_link_libraries(tnzcore
    Qt5::Core
    Qt5::OpenGL
    Qt5::Network
    Qt5::Multimedia
    ${GL_LIB}
    ${GLUT_LIB}
    ${QT_LIB}
    ${Z_LIB}
    ${JPEG_LIB}
    ${LZ4_LIB}
    ${EXTRA_LIBS}
)
