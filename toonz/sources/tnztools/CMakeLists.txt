# Define MOC headers for Qt processing
set(MOC_HEADERS
    controlpointselection.h
    edittoolgadgets.h
    filltool.h
    fullcolorbrushtool.h
    fullcolorfilltool.h
    plastictool.h
    skeletonsubtools.h
    tooloptionscontrols.h
    toonzrasterbrushtool.h
    viewtools.h
    selectiontool.h
    edittool.h
    controlpointeditortool.h
    geometrictool.h
    ../include/tools/imagegrouping.h
    ../include/tools/screenpicker.h
    ../include/tools/toolhandle.h
    ../include/tools/tooloptions.h
)

# Define headers
set(HEADERS
    ${MOC_HEADERS}
    autofill.h
    bluredbrush.h
    hookselection.h
    mypainttoonzbrush.h
    rasterselectiontool.h
    rgbpickertool.h
    rulertool.h
    setsaveboxtool.h
    shifttracetool.h
    stylepickertool.h
    toonzvectorbrushtool.h
    vectorselectiontool.h
    assistants/assistantellipse.h
    assistants/assistantline.h
    assistants/assistantvanishingpoint.h
    ../include/tools/RGBpicker.h
    ../include/tools/cursormanager.h
    ../include/tools/cursors.h
    ../include/tools/levelselection.h
    ../include/tools/rasterselection.h
    ../include/tools/strokeselection.h
    ../include/tools/stylepicker.h
    ../include/tools/tool.h
    ../include/tools/toolcommandids.h
    ../include/tools/toolutils.h
    ../include/tools/tooltimer.h
    ../include/tools/keyhistory.h
    ../include/tools/inputstate.h
    ../include/tools/track.h
    ../include/tools/inputmanager.h
    ../include/tools/assistant.h
    ../include/tools/replicator.h
    ../include/tools/modifiers/modifierassistants.h
    ../include/tools/modifiers/modifierclone.h
    ../include/tools/modifiers/modifierjitter.h
    ../include/tools/modifiers/modifierline.h
    ../include/tools/modifiers/modifiersegmentation.h
    ../include/tools/modifiers/modifiersimplify.h
    ../include/tools/modifiers/modifiersmooth.h
    ../include/tools/modifiers/modifiertangents.h
    ../include/tools/modifiers/modifiertest.h
    ../include/tools/assistants/guidelineline.h
    ../include/tools/assistants/guidelineellipse.h
)

# Define source files
set(SOURCES
    autofillpli.cpp
    autofilltlv.cpp
    cursormanager.cpp
    stylepicker.cpp
    tool.cpp
    toolhandle.cpp
    toolutils.cpp
    bendertool.cpp
    bluredbrush.cpp
    controlpointeditortool.cpp
    cuttertool.cpp
    edittool.cpp
    edittoolgadgets.cpp
    filltool.cpp
    fullcolorbrushtool.cpp
    fullcolorerasertool.cpp
    fullcolorfilltool.cpp
    geometrictool.cpp
    hooktool.cpp
    hookselection.cpp
    imagegrouping.cpp
    irontool.cpp
    levelselection.cpp
    magnettool.cpp
    morphtool.cpp
    paintbrushtool.cpp
    pinchtool.cpp
    plastictool.cpp
    plastictool_animate.cpp
    plastictool_build.cpp
    plastictool_meshedit.cpp
    plastictool_rigidity.cpp
    pumptool.cpp
    rastererasertool.cpp
    rastertapetool.cpp
    rasterselectiontool.cpp
    rgbpickertool.cpp
    selectiontool.cpp
    setsaveboxtool.cpp
    skeletonsubtools.cpp
    skeletontool.cpp
    stylepickertool.cpp
    trackertool.cpp
    typetool.cpp
    tooloptionscontrols.cpp
    tooloptions.cpp
    vectorerasertool.cpp
    vectorselectiontool.cpp
    vectortapetool.cpp
    viewtools.cpp
    controlpointselection.cpp
    rasterselection.cpp
    strokeselection.cpp
    screenpicker.cpp
    fingertool.cpp
    rulertool.cpp
    mypainttoonzbrush.cpp
    shifttracetool.cpp
    toonzrasterbrushtool.cpp
    toonzvectorbrushtool.cpp
    tooltimer.cpp
    inputstate.cpp
    track.cpp
    inputmanager.cpp
    assistant.cpp
    replicator.cpp
    modifiers/modifierassistants.cpp
    modifiers/modifierclone.cpp
    modifiers/modifierjitter.cpp
    modifiers/modifierline.cpp
    modifiers/modifiersegmentation.cpp
    modifiers/modifiersimplify.cpp
    modifiers/modifiersmooth.cpp
    modifiers/modifiertangents.cpp
    modifiers/modifiertest.cpp
    assistants/guidelineline.cpp
    assistants/guidelineellipse.cpp
    assistants/assistantellipse.cpp
    assistants/assistantfisheye.cpp
    assistants/assistantline.cpp
    assistants/assistantperspective.cpp
    assistants/assistantvanishingpoint.cpp
    assistants/replicatoraffine.cpp
    assistants/replicatorgrid.cpp
    assistants/replicatorjitter.cpp
    assistants/replicatormirror.cpp
    assistants/replicatorstar.cpp
    editassistantstool.cpp
)

# Resource files
set(RESOURCES tnztools.qrc)
qt5_add_resources(RESOURCES_RCC ${RESOURCES})

# Translation support
if(WITH_TRANSLATION)
    add_translation(tnztools ${HEADERS} ${SOURCES})
endif()

# Process MOC headers
qt5_wrap_cpp(MOC_SOURCES ${MOC_HEADERS})

# Create the shared library
add_library(tnztools SHARED
    ${HEADERS}
    ${SOURCES}
    ${MOC_SOURCES}
    ${RESOURCES_RCC}
)

# Add definitions
target_compile_definitions(tnztools PRIVATE TNZTOOLS_EXPORTS)

# Set linker flags for Apple
if(BUILD_ENV_APPLE)
    set_target_properties(tnztools PROPERTIES
        LINK_FLAGS "-Wl,-install_name,@executable_path/libtnztools.dylib"
    )
    add_dependencies(tnztools tnzcore tnzbase tnzext toonzlib toonzqt)
endif()

# Debug messages
message(STATUS "Subdirectory: tnztools")
message(STATUS "Binary dir: ${CMAKE_CURRENT_BINARY_DIR}")

# Include directories
target_include_directories(tnztools SYSTEM PRIVATE
    ${SDKROOT}/superlu/SupperLU_4.1/SRC
)
target_include_directories(tnztools PRIVATE
    ../include/tools
)

# Define and link extra libraries
_find_toonz_library(EXTRA_LIBS "tnzcore;tnzbase;tnzext;toonzlib;toonzqt")
target_link_libraries(tnztools
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
    Qt5::OpenGL
    ${GLUT_LIB}
    ${GL_LIB}
    ${EXTRA_LIBS}
    ${MYPAINT_LIB_LDFLAGS}
)
