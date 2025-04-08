add_executable(ThorVision WIN32 MACOSX_BUNDLE)

set(output_name "ThorVision")

set_target_properties(ThorVision 
    PROPERTIES 
        OUTPUT_NAME "${output_name}"
        AUTOMOC ON
)

target_sources(ThorVision 
    PRIVATE
        src/main.cpp
        src/app.h
        src/app.cc

        src/xdaq_camera_control.h
        src/xdaq_camera_control.cc
        src/camera_item_widget.h
        src/camera_item_widget.cc
        src/stream_mainwindow.h
        src/stream_mainwindow.cc
        src/stream_window.h
        src/stream_window.cc
        src/server_status_indicator.h
        src/server_status_indicator.cc
        
        src/record_confirm_dialog.h
        src/record_confirm_dialog.cc

        src/record_settings.h
        src/record_settings.cc
        src/camera_record_widget.h
        src/camera_record_widget.cc
        src/save_paths_combobox.h
        src/save_paths_combobox.cc
        src/dir_name_combobox.h
        src/dir_name_combobox.cc
        src/duration_spinbox.h
        src/duration_spinbox.cc
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

option(TEST "Enable test mode for cameras" OFF)
if(TEST)
    target_compile_definitions(ThorVision PRIVATE TEST)
endif()

find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
# find_package(nlohmann_json_schema_validator REQUIRED)
find_package(xdaqmetadata REQUIRED)
find_package(libxvc REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(PkgConfig REQUIRED) 

# https://cmake.org/cmake/help/latest/module/FindPkgConfig.html
pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
pkg_search_module(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
pkg_search_module(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)

target_link_libraries(ThorVision
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        nlohmann_json::nlohmann_json
        PkgConfig::gstreamer
        PkgConfig::gstreamer-app
        PkgConfig::gstreamer-video
        spdlog::spdlog
        fmt::fmt
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
        FILES "${CMAKE_BINARY_DIR}/thorvision/xdaqmetadata.dll"
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
        function(copy_dylibs TARGET_FILE)
            message(STATUS \" Copy dylibs: \${TARGET_FILE}\")
            set(PROCESSED_LIBS \"\")
            set(TO_PROCESS \"\${TARGET_FILE}\")

            while(TO_PROCESS)
                list(POP_FRONT TO_PROCESS CURRENT_TARGET)
                get_filename_component(CURRENT_LIB_NAME \"\${CURRENT_TARGET}\" NAME)
                execute_process(
                    COMMAND otool -L \"\${CURRENT_TARGET}\"
                    OUTPUT_VARIABLE OTOOL_OUTPUT
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                )
                string(REGEX MATCHALL \"@rpath/[^ \\n]+\\.dylib\" RPATH_LIBS \"\${OTOOL_OUTPUT}\")

                foreach(RPATH_LIB \${RPATH_LIBS})
                    get_filename_component(LIB_NAME \"\${RPATH_LIB}\" NAME)
                    if(LIB_NAME STREQUAL \"\${CURRENT_LIB_NAME}\")
                        continue()
                    endif()

                    if(LIB_NAME STREQUAL \"libxdaqmetadata.0.dylib\")
                        continue()
                    endif()

                    set(SRC_LIB \"${GSTREAMER_LIB_DIR}/\${LIB_NAME}\")
                    set(DEST_LIB \"${FRAMEWORKS_DIR}/\${LIB_NAME}\")

                    list(FIND PROCESSED_LIBS \"\${LIB_NAME}\" FOUND_INDEX)
                    if(FOUND_INDEX GREATER -1)
                        continue()
                    endif()

                    if(EXISTS \"\${SRC_LIB}\")
                        file(COPY \"\${SRC_LIB}\" DESTINATION \"${FRAMEWORKS_DIR}\")
                        list(APPEND TO_PROCESS \"\${DEST_LIB}\")
                        list(APPEND PROCESSED_LIBS \"\${LIB_NAME}\")
                    else()
                        message(WARNING \"Missing dependency: \${SRC_LIB}\")
                    endif()
                endforeach()
            endwhile()
        endfunction()

        copy_dylibs(\"${APP_BUNDLE}/Contents/MacOS/${output_name}\")

        foreach(PLUGIN ${GSTREAMER_PLUGINS})
            configure_file(
                \"${GSTREAMER_PLUGIN_DIR}/\${PLUGIN}\"
                \"${APP_BUNDLE}/Contents/PlugIns/gstreamer-1.0/\${PLUGIN}\"
                COPYONLY
            )
            copy_dylibs(\"${GSTREAMER_PLUGIN_DIR}/\${PLUGIN}\")
        endforeach()
    ")
endif()