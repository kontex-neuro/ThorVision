find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(xdaqmetadata REQUIRED)
find_package(libxvc REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Qml Quick QuickControls2)
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
    src/CustomButton.qml
)
set(qml_singletons
    src/AppSettings.qml
    src/Font.qml
    src/Color.qml
)

set_source_files_properties(${qml_singletons}
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(ThorVision
    URI App.Theme
    VERSION 0.1
    QML_FILES
        "${qml_files}"
        "${qml_singletons}"
    OUTPUT_DIRECTORY
        "${CMAKE_CURRENT_BINARY_DIR}/thorvision/App/Theme"
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

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/src/version.h"
    @ONLY
)

target_sources(ThorVision 
    PRIVATE
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
        src/Server.h
        src/Server.cc

        "${CMAKE_SOURCE_DIR}/resources/thorvision.qrc"
)
target_include_directories(ThorVision 
    PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>"
)

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
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
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
        PkgConfig::gstreamer
        PkgConfig::gstreamer-app
        # PkgConfig::gstreamer-video
        spdlog::spdlog
        fmt::fmt
        nlohmann_json::nlohmann_json
        xdaqmetadata::xdaqmetadata
        libxvc::libxvc
)

install(
    TARGETS ThorVision
    BUNDLE DESTINATION "."
    RUNTIME DESTINATION "."
)

include("${CMAKE_SOURCE_DIR}/cmake/deploy_qt.cmake")
deploy_qt("${output_name}")

if(WIN32)
    install(
        FILES "${CMAKE_BINARY_DIR}/xdaqmetadata.dll"
        DESTINATION "."
    )
elseif(APPLE)
    # TODO: If APP_BUNDLE is set to "${CMAKE_BINARY_DIR}/thorvision/${output_name}.app", 
    #       running 'cmake --build --target install' followed by 'cmake --build --target package' produces a working package.
    #       However, if APP_BUNDLE is set to "${CMAKE_INSTALL_PREFIX}/${output_name}.app", 
    #       the install step works, but the packaged application is missing the GStreamer dependencies.

    # GStreamer default install path
    set(GSTREAMER_DIR "/Library/Frameworks/GStreamer.framework")
    set(GSTREAMER_LIB_DIR "${GSTREAMER_DIR}/Libraries")
    set(GSTREAMER_PLUGIN_DIR "${GSTREAMER_LIB_DIR}/gstreamer-1.0")
    set(APP_BUNDLE "${CMAKE_BINARY_DIR}/thorvision/${output_name}.app")
    # set(APP_BUNDLE "${CMAKE_INSTALL_PREFIX}/${output_name}.app")
    set(FRAMEWORKS_DIR "${APP_BUNDLE}/Contents/Frameworks")

    if(NOT EXISTS "${GSTREAMER_DIR}")
        message(FATAL_ERROR "GStreamer not found at ${GSTREAMER_DIR}")
    endif()

    # TODO: Manually select required GStreamer plugins.
    #       Refer to libxvc to determine which GStreamer plugins are actually used.
    set(GSTREAMER_PLUGINS
        "libgstsrt.dylib" # https://gstreamer.freedesktop.org/documentation/srt/index.html?gi-language=c
        "libgstcoreelements.dylib" # https://gstreamer.freedesktop.org/documentation/coreelements/index.html?gi-language=c
        "libgstjpegformat.dylib" # https://gstreamer.freedesktop.org/documentation/jpegformat/index.html?gi-language=c
        "libgstvideoparsersbad.dylib" # https://gstreamer.freedesktop.org/documentation/videoparsersbad/index.html?gi-language=c
        "libgstjpeg.dylib" # https://gstreamer.freedesktop.org/documentation/jpeg/index.html?gi-language=c
        "libgstapp.dylib" # https://gstreamer.freedesktop.org/documentation/app/index.html?gi-language=c
        "libgstapplemedia.dylib" # https://gstreamer.freedesktop.org/documentation/applemedia/index.html?gi-language=c
        "libgstvideotestsrc.dylib" # https://gstreamer.freedesktop.org/documentation/videotestsrc/?gi-language=c
        "libgstmultifile.dylib" # https://gstreamer.freedesktop.org/documentation/multifile/index.html?gi-language=c
        "libgstmatroska.dylib" # https://gstreamer.freedesktop.org/documentation/matroska/index.html?gi-language=c
        "libgstvideoconvertscale.dylib" # https://gstreamer.freedesktop.org/documentation/videoconvertscale/index.html?gi-language=c
        "libgstlibav.dylib" # https://gstreamer.freedesktop.org/documentation/libav/index.html?gi-language=c
        # https://gstreamer.freedesktop.org/documentation/d3d11/index.html?gi-language=c
    )

    install(CODE "
        function(copy_dylibs target_file)
            message(STATUS \" Copy dylibs for: \${target_file}\")

            set(processed_libs \"\")
            set(to_process \"\${target_file}\")

            while(to_process)
                list(POP_FRONT to_process current_target)
                get_filename_component(current_lib_name \"\${current_target}\" NAME)
                execute_process(
                    COMMAND otool -L \"\${current_target}\"
                    OUTPUT_VARIABLE otool_output
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                )
                string(REGEX MATCHALL \"@rpath/[^ \n]+[.]dylib\" dylibs \"\${otool_output}\")

                foreach(dylib \${dylibs})
                    get_filename_component(lib_name \"\${dylib}\" NAME)

                    if(lib_name STREQUAL \"\${current_lib_name}\" OR lib_name STREQUAL \"libxdaqmetadata.0.dylib\")
                        continue()
                    endif()

                    set(src_lib \"${GSTREAMER_LIB_DIR}/\${lib_name}\")
                    set(dest_lib \"${FRAMEWORKS_DIR}/\${lib_name}\")

                    list(FIND processed_libs \"\${lib_name}\" found_index)
                    if(found_index GREATER -1)
                        continue()
                    endif()

                    if(EXISTS \"\${src_lib}\")
                        file(COPY \"\${src_lib}\" DESTINATION \"${FRAMEWORKS_DIR}\")
                        list(APPEND to_process \"\${dest_lib}\")
                        list(APPEND processed_libs \"\${lib_name}\")
                    else()
                        message(WARNING \"Missing dependency: \${src_lib}\")
                    endif()
                endforeach()
            endwhile()
        endfunction()

        copy_dylibs(\"${APP_BUNDLE}/Contents/MacOS/${output_name}\")

        foreach(plugin ${GSTREAMER_PLUGINS})
            configure_file(
                \"${GSTREAMER_PLUGIN_DIR}/\${plugin}\"
                \"${APP_BUNDLE}/Contents/PlugIns/gstreamer/\${plugin}\"
                COPYONLY
            )
            copy_dylibs(\"${GSTREAMER_PLUGIN_DIR}/\${plugin}\")
        endforeach()
    ")
endif()