find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(xdaqmetadata REQUIRED)
find_package(libxvc REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Qml Quick QuickControls2 Svg)
find_package(PkgConfig REQUIRED) 

# https://cmake.org/cmake/help/latest/module/FindPkgConfig.html
pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
pkg_search_module(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
pkg_search_module(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(ThorVision
    src/main.cc
)

set(qml_files
    src/main.qml
    src/CameraList.qml
    src/CameraCount.qml
    src/RecordSettings.qml
    src/Record.qml
    src/XDAQStatus.qml
    src/VideoLayout.qml
    src/PreviewButtons.qml
    src/VideoPreview.qml
    src/SettingsView.qml
    src/CameraNameEditor.qml
    src/AlertDialog.qml
    src/CustomDialogButton.qml
    src/CustomComboBox.qml
    src/CustomCheckBox.qml
    src/CustomSpinBox.qml
    src/CustomRecordButton.qml
)
set(qml_singletons
    src/AppSettings.qml
    src/Font.qml
    src/Color.qml
)
set_source_files_properties(${qml_singletons}
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)
set(cpp_files
    src/GstVideoSink.h
    src/GstVideoSink.cc
    src/ImageProvider.h
    src/ImageProvider.cc
    src/CameraModel.h
    src/CameraModel.cc
    src/CameraItem.h
    src/CameraItem.cc
    src/Recorder.h
    src/Recorder.cc
    src/RecorderSettings.h
    src/Server.h
    src/Server.cc
    src/WebSocketClient.h
    src/WebSocketClient.cc
)
set(resources
    ../resources/camera-connect.gif
    ../resources/camera-connected.svg
    ../resources/camera-icon.svg
    ../resources/change-name.svg
    ../resources/drawer.svg
    ../resources/open-folder.svg
    ../resources/select-folder.svg
    ../resources/preview-1.svg
    ../resources/preview-4.svg
    ../resources/preview-6.svg
    ../resources/preview-12.svg
    ../resources/start-record.svg
    ../resources/stop-record.svg
    ../resources/xdaq.svg
    ../resources/xdaq-connecting.gif
    ../resources/xdaq-connected.png
    ../resources/warning.svg
    ../resources/arrow-down.svg
    ../resources/spinbox-arrow-up.svg
    ../resources/spinbox-arrow-down.svg
)

qt_add_qml_module(ThorVision
    URI App.Theme
    VERSION 0.1
    QML_FILES
        "${qml_files}"
        "${qml_singletons}"
    SOURCES
        "${cpp_files}"
    RESOURCES
        "${resources}"
)

set(output_name "ThorVision")

set_target_properties(ThorVision 
    PROPERTIES 
        OUTPUT_NAME "${output_name}"
        AUTOMOC ON
        AUTORCC ON
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
)

# configure_file(
#     "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
#     "${CMAKE_CURRENT_BINARY_DIR}/src/version.h"
#     @ONLY
# )

# target_sources(ThorVision 
#     PRIVATE
#         "${CMAKE_SOURCE_DIR}/resources/thorvision.qrc"
# )
# target_include_directories(ThorVision 
#     PRIVATE
#         "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>"
# )

if(WIN32)
    enable_language("RC")
    set(WIN32_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/resource.rc")
    target_sources(ThorVision PRIVATE "${WIN32_RESOURCES}")
elseif(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE "xdaq-icon.icns")
    set(BUNDLE_ICON "${CMAKE_SOURCE_DIR}/resources/${MACOSX_BUNDLE_ICON_FILE}")
    set_source_files_properties("${BUNDLE_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(ThorVision PRIVATE "${BUNDLE_ICON}")

    set_target_properties(ThorVision 
        PROPERTIES 
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
            INSTALL_RPATH "$ORIGIN;@executable_path/../Frameworks"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.KonteX.ThorVision"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ThorVision.plist"
    )
endif()

target_compile_features(ThorVision PRIVATE cxx_std_20)
target_compile_options(ThorVision
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
        # $<$<CXX_COMPILER_ID:AppleClang,Clang>:-fexperimental-library>
)

option(Sanitize "Enable sanitizers" OFF)
if(Sanitize)
    target_compile_options(ThorVision PRIVATE -fsanitize=address,undefined)
    target_link_options(ThorVision PRIVATE -fsanitize=address,undefined)
endif()

target_link_libraries(ThorVision
    PRIVATE
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Svg
        PkgConfig::gstreamer
        PkgConfig::gstreamer-app
        # PkgConfig::gstreamer-video
        spdlog::spdlog
        fmt::fmt
        nlohmann_json::nlohmann_json
        xdaqmetadata::xdaqmetadata
        libxvc::libxvc
)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "." CACHE PATH "default install path" FORCE)
endif()

install(
    TARGETS ThorVision
    BUNDLE DESTINATION "."
    RUNTIME DESTINATION "."
)

qt_generate_deploy_qml_app_script(
    TARGET ThorVision
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)
install(SCRIPT "${deploy_script}")

if(WIN32)
    install(
        FILES "${CMAKE_BINARY_DIR}/xdaqmetadata.dll"
        DESTINATION "."
    )
elseif(APPLE)
    # TODO: If APP_BUNDLE is set to "${CMAKE_BINARY_DIR}/${output_name}.app", 
    #       running 'cmake --build --target install' followed by 'cmake --build --target package' produces a working package.
    #       However, if APP_BUNDLE is set to "${CMAKE_INSTALL_PREFIX}/${output_name}.app", 
    #       the install step works, but the packaged application is missing the GStreamer dependencies.
    set(GSTREAMER_LIB_DIR "/Library/Frameworks/GStreamer.framework/Libraries")
    set(GSTREAMER_PLUGIN_DIR "${GSTREAMER_LIB_DIR}/gstreamer-1.0")
    
    # Path to the build-tree bundle
    set(BUILD_APP_BUNDLE "${CMAKE_BINARY_DIR}/${output_name}.app")

    # Path to the install-tree bundle
    set(INSTALL_APP_BUNDLE "${CMAKE_INSTALL_PREFIX}/${output_name}.app")

    # TODO: Why gstreamer copy target to build tree bundle produces a working package?
    #       '--target install' needs to set it "${INSTALL_APP_BUNDLE}"
    #       '--target package' needs to set it "${BUILD_APP_BUNDLE}"
    set(APP_BUNDLE "${BUILD_APP_BUNDLE}")
    set(FRAMEWORKS_DIR "${APP_BUNDLE}/Contents/Frameworks")

    if(NOT EXISTS "${GSTREAMER_LIB_DIR}")
        message(FATAL_ERROR "GStreamer libraries not found at ${GSTREAMER_LIB_DIR}")
    endif()

    # TODO: Manually select required GStreamer plugins.
    #       Refer to libxvc to determine which GStreamer plugins are actually used.
    set(GSTREAMER_PLUGINS
        "libgstsrt.dylib"               # https://gstreamer.freedesktop.org/documentation/srt/index.html?gi-language=c
        "libgstcoreelements.dylib"      # https://gstreamer.freedesktop.org/documentation/coreelements/index.html?gi-language=c
        "libgstjpegformat.dylib"        # https://gstreamer.freedesktop.org/documentation/jpegformat/index.html?gi-language=c
        "libgstvideoparsersbad.dylib"   # https://gstreamer.freedesktop.org/documentation/videoparsersbad/index.html?gi-language=c
        "libgstjpeg.dylib"              # https://gstreamer.freedesktop.org/documentation/jpeg/index.html?gi-language=c
        "libgstapp.dylib"               # https://gstreamer.freedesktop.org/documentation/app/index.html?gi-language=c
        "libgstapplemedia.dylib"        # https://gstreamer.freedesktop.org/documentation/applemedia/index.html?gi-language=c
        "libgstvideotestsrc.dylib"      # https://gstreamer.freedesktop.org/documentation/videotestsrc/?gi-language=c
        "libgstmultifile.dylib"         # https://gstreamer.freedesktop.org/documentation/multifile/index.html?gi-language=c
        "libgstmatroska.dylib"          # https://gstreamer.freedesktop.org/documentation/matroska/index.html?gi-language=c
        "libgstvideoconvertscale.dylib" # https://gstreamer.freedesktop.org/documentation/videoconvertscale/index.html?gi-language=c
        "libgstlibav.dylib"             # https://gstreamer.freedesktop.org/documentation/libav/index.html?gi-language=c
        "libgstdebugutilsbad.dylib"     # https://gstreamer.freedesktop.org/documentation/debugutilsbad/index.html?gi-language=c
    )

    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/copy_dylibs.cmake.in"
        "${CMAKE_BINARY_DIR}/copy_dylibs.cmake"
        @ONLY
    )

    install(
        SCRIPT "${CMAKE_BINARY_DIR}/copy_dylibs.cmake"
        CODE
        "copy_dylibs(\"${APP_BUNDLE}/Contents/MacOS/${output_name}\")
        foreach(plugin ${GSTREAMER_PLUGINS})
            configure_file(
                \"${GSTREAMER_PLUGIN_DIR}/\${plugin}\"
                \"${APP_BUNDLE}/Contents/PlugIns/gstreamer/\${plugin}\"
                COPYONLY
            )
            copy_dylibs(\"${GSTREAMER_PLUGIN_DIR}/\${plugin}\")
        endforeach()"
    )
endif()