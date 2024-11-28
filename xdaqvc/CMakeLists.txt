add_executable(XDAQ-VC WIN32 MACOSX_BUNDLE)

if(WIN32)
    enable_language("RC")
    set(WIN32_RESOURCES "${CMAKE_SOURCE_DIR}/resource.rc")
elseif(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE "xdaq-icon.icns")
    set(BUNDLE_XDAQ_ICON "${CMAKE_SOURCE_DIR}/resources/${MACOSX_BUNDLE_ICON_FILE}")
    set_source_files_properties("${BUNDLE_XDAQ_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()

target_sources(XDAQ-VC 
    PRIVATE
        "${WIN32_RESOURCES}"
        "${BUNDLE_XDAQ_ICON}"
        src/main.cpp

        src/xdaq_camera_control.h
        src/xdaq_camera_control.cc
        src/camera_item_widget.h
        src/camera_item_widget.cc
        src/stream_mainwindow.h
        src/stream_mainwindow.cc
        src/stream_window.h
        src/stream_window.cc
        
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
set_target_properties(XDAQ-VC PROPERTIES AUTOMOC ON)
set_target_properties(XDAQ-VC 
    PROPERTIES 
        # INSTALL_RPATH "$ORIGIN;/opt/homebrew/opt/qt/Frameworks;"
        INSTALL_RPATH "@executable_path/../Frameworks"
        # XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
)

option(Sanitize "Enable sanitizers" OFF)
if(Sanitize)
    target_compile_options(XDAQ-VC PRIVATE -fsanitize=address,undefined)
    target_link_options(XDAQ-VC PRIVATE -fsanitize=address,undefined)
endif()

target_compile_features(XDAQ-VC PRIVATE cxx_std_20)

find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
# find_package(nlohmann_json_schema_validator REQUIRED)
find_package(xdaqmetadata REQUIRED)
find_package(libxvc REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core OpenGLWidgets)
find_package(PkgConfig REQUIRED)
pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
pkg_search_module(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
pkg_search_module(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)

target_link_libraries(XDAQ-VC 
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::OpenGLWidgets
        nlohmann_json::nlohmann_json
        PkgConfig::gstreamer
        PkgConfig::gstreamer-app
        PkgConfig::gstreamer-video
        spdlog::spdlog
        fmt::fmt
        xdaqmetadata::xdaqmetadata
        libxvc::libxvc
)

if(WIN32)
    install(
        TARGETS XDAQ-VC 
        BUNDLE DESTINATION "${PROJECT_NAME}"
        RUNTIME DESTINATION "${PROJECT_NAME}"
    )
elseif(APPLE)
    install(
        TARGETS XDAQ-VC 
        BUNDLE DESTINATION "."
        RUNTIME DESTINATION "."
    )
endif()

include("${CMAKE_SOURCE_DIR}/cmake/deploy_qt.cmake")
deploy_qt("${PROJECT_NAME}")

if(WIN32)
    install(
        FILES "${CMAKE_BINARY_DIR}/xdaqvc/xdaqmetadata.dll"
        DESTINATION "${PROJECT_NAME}"
    )
elseif(APPLE)
    # TODO: Hardcoded gstreamer installed path
    # TODO: No need to copy all *.dylib, use otool -L <executable>, then install what is needed
    file(GLOB GSTREAMER_DYLIBS "/Library/Frameworks/GStreamer.framework/Libraries/*.dylib")
    install(
        FILES ${GSTREAMER_DYLIBS}
        DESTINATION "./XDAQ-VC.app/Contents/Frameworks"
    )
    # install(
    #     FILES
    #         ${gstreamer_LINK_LIBRARIES}
    #         ${gstreamer-app_LINK_LIBRARIES}
    #         ${gstreamer-video_LINK_LIBRARIES}
    #     DESTINATION "${PROJECT_NAME}/XDAQ-VC.app/Contents/Frameworks"
    # )
endif()