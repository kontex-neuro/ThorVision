find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(xdaqmetadata REQUIRED)
find_package(libxvc REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Qml Quick QuickControls2 Svg)
find_package(PkgConfig REQUIRED) 
pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(oatpp REQUIRED)

if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

qt_standard_project_setup(REQUIRES 6.7)

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
    src/D3D11GstVideoItem.qml
    src/GLGstVideoItem.qml
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
    src/HttpServer.h
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
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        INSTALL_RPATH "$ORIGIN;@executable_path/../Frameworks"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.KonteX.ThorVision"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ThorVision.plist"
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
endif()

target_compile_features(ThorVision PRIVATE cxx_std_20)
target_compile_options(ThorVision
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:AppleClang,Clang>>:-fexperimental-library>
)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
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
        spdlog::spdlog
        fmt::fmt
        nlohmann_json::nlohmann_json
        xdaqmetadata::xdaqmetadata
        libxvc::libxvc
        oatpp::oatpp
)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "default install path" FORCE)
endif()

install(
    TARGETS ThorVision
    BUNDLE DESTINATION "."
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

qt_generate_deploy_qml_app_script(
    TARGET ThorVision
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    NO_TRANSLATIONS
    NO_COMPILER_RUNTIME
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)
install(SCRIPT "${deploy_script}")

if(WIN32)
    set(gst_dir "C:/Program\ Files/gstreamer/1.0/msvc_x86_64" CACHE PATH "Path to gstreamer root")
    set(gst_lib_dir "${gst_dir}/lib")
elseif(APPLE)
    set(gst_dir "/Library/Frameworks/GStreamer.framework" CACHE PATH "Path to gstreamer root")
    set(gst_lib_dir "${gst_dir}/Libraries")
endif()

set(gst_plugin_dir "${gst_lib_dir}/gstreamer-1.0")

if(WIN32)
    install(
        FILES "${CMAKE_BINARY_DIR}/xdaqmetadata.dll"
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
    
    set(gst_plugins
        "gstsrt.dll"               # https://gstreamer.freedesktop.org/documentation/srt/index.html?gi-language=c
        "gstcoreelements.dll"      # https://gstreamer.freedesktop.org/documentation/coreelements/index.html?gi-language=c
        "gstjpegformat.dll"        # https://gstreamer.freedesktop.org/documentation/jpegformat/index.html?gi-language=c
        "gstvideoparsersbad.dll"   # https://gstreamer.freedesktop.org/documentation/videoparsersbad/index.html?gi-language=c
        "gstjpeg.dll"              # https://gstreamer.freedesktop.org/documentation/jpeg/index.html?gi-language=c
        # "gstvideotestsrc.dll"      # https://gstreamer.freedesktop.org/documentation/videotestsrc/?gi-language=c
        "gstmultifile.dll"         # https://gstreamer.freedesktop.org/documentation/multifile/index.html?gi-language=c
        "gstmatroska.dll"          # https://gstreamer.freedesktop.org/documentation/matroska/index.html?gi-language=c
        # "gstvideoconvertscale.dll" # https://gstreamer.freedesktop.org/documentation/videoconvertscale/index.html?gi-language=c
        # "gstlibav.dll"             # https://gstreamer.freedesktop.org/documentation/libav/index.html?gi-language=c
        "gstdebugutilsbad.dll"     # https://gstreamer.freedesktop.org/documentation/debugutilsbad/index.html?gi-language=c
        # "gstqsv.dll"               # https://gstreamer.freedesktop.org/documentation/qsv/index.html?gi-language=c
        "gstd3d11.dll"             # https://gstreamer.freedesktop.org/documentation/d3d11/index.html?gi-language=c
        "gstqt6d3d11.dll"          # https://gstreamer.freedesktop.org/documentation/qt6d3d11/index.html?gi-language=c
    )

    list(TRANSFORM gst_plugins PREPEND "${gst_plugin_dir}/")

    install(FILES ${gst_plugins} DESTINATION "plugins/gstreamer")

    install(CODE
        "execute_process(
            COMMAND ${Python3_EXECUTABLE} \"${CMAKE_SOURCE_DIR}/cmake/copy_gst_deps_windows.py\"
                    \"plugins/gstreamer\"
                    \"${gst_dir}\"
                    \"${CMAKE_INSTALL_BINDIR}\"
            WORKING_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}\"
            RESULT_VARIABLE result
        )
        if(NOT result EQUAL 0)
            message(FATAL_ERROR \"copy_gst_deps_windows.py failed for ${plugin}\")
        endif()"
    )

elseif(APPLE)
    set(gst_plugins
        "libgstsrt.dylib"               # https://gstreamer.freedesktop.org/documentation/srt/index.html?gi-language=c
        "libgstcoreelements.dylib"      # https://gstreamer.freedesktop.org/documentation/coreelements/index.html?gi-language=c
        "libgstjpegformat.dylib"        # https://gstreamer.freedesktop.org/documentation/jpegformat/index.html?gi-language=c
        "libgstvideoparsersbad.dylib"   # https://gstreamer.freedesktop.org/documentation/videoparsersbad/index.html?gi-language=c
        "libgstjpeg.dylib"              # https://gstreamer.freedesktop.org/documentation/jpeg/index.html?gi-language=c
        "libgstapplemedia.dylib"        # https://gstreamer.freedesktop.org/documentation/applemedia/index.html?gi-language=c
        # "libgstvideotestsrc.dylib"      # https://gstreamer.freedesktop.org/documentation/videotestsrc/?gi-language=c
        "libgstmultifile.dylib"         # https://gstreamer.freedesktop.org/documentation/multifile/index.html?gi-language=c
        "libgstmatroska.dylib"          # https://gstreamer.freedesktop.org/documentation/matroska/index.html?gi-language=c
        # "libgstvideoconvertscale.dylib" # https://gstreamer.freedesktop.org/documentation/videoconvertscale/index.html?gi-language=c
        # "libgstlibav.dylib"             # https://gstreamer.freedesktop.org/documentation/libav/index.html?gi-language=c
        "libgstdebugutilsbad.dylib"     # https://gstreamer.freedesktop.org/documentation/debugutilsbad/index.html?gi-language=c
        "libgstopengl.dylib"            # https://gstreamer.freedesktop.org/documentation/opengl/index.html?gi-language=c
        "libgstqml6.dylib"              # https://gstreamer.freedesktop.org/documentation/qml6/index.html?gi-language=c
    )

    list(TRANSFORM gst_plugins PREPEND "${gst_plugin_dir}/")

    install(FILES ${gst_plugins} DESTINATION "${output_name}.app/Contents/PlugIns/gstreamer")

    install(CODE
        "execute_process(
            COMMAND ${Python3_EXECUTABLE} \"${CMAKE_SOURCE_DIR}/cmake/copy_patch_gst_deps.py\"
                    \"${output_name}.app/Contents/MacOS/${output_name}\"
                    \"${output_name}.app\"
            WORKING_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}\"
            RESULT_VARIABLE result
        )
        if(NOT result EQUAL 0)
            message(FATAL_ERROR \"copy_patch_gst_deps.py failed for ${output_name}\")
        endif()"
    )

    install(CODE
        "execute_process(
            COMMAND ${Python3_EXECUTABLE} \"${CMAKE_SOURCE_DIR}/cmake/copy_patch_gst_deps.py\"
                    \"${output_name}.app/Contents/PlugIns/gstreamer\"
                    \"${output_name}.app\"
            WORKING_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}\"
            RESULT_VARIABLE result
        )"
    )
endif()