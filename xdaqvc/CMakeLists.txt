add_executable(XDAQ-VC WIN32 MACOSX_BUNDLE)

if(WIN32)
    enable_language("RC")
    set(WIN32_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/resource.rc")
elseif(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE "xdaq-icon.icns")
    set(BUNDLE_ICON "${CMAKE_SOURCE_DIR}/resources/${MACOSX_BUNDLE_ICON_FILE}")
    set_source_files_properties("${BUNDLE_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()

set(output_name "XDAQ-VC")

target_sources(XDAQ-VC 
    PRIVATE
        "${WIN32_RESOURCES}"
        "${BUNDLE_ICON}"
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
        OUTPUT_NAME "${output_name}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.KonteX.XDAQ-VC"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ThorVision.plist"
        # XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/ThorVision/${bundle_entitlements}"
        # XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "6383JHQ5DF"
        # XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development"
        # XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual"
        # XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.KonteX.XDAQ-VC"
        # XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION "0.0.2"
        # XCODE_ATTRIBUTE_PRODUCT_NAME "${output_name}"
        # XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
        # XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
        # XCODE_ATTRIBUTE_COMBINE_HIDPI_IMAGES YES
        # XCODE_ATTRIBUTE_COPY_PHASE_STRIP NO
        # XCODE_ATTRIBUTE_ALWAYS_SEARCH_USER_PATHS NO
        # XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY libc++
        # XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS --deep
        # XCODE_ATTRIBUTE_CLANG_DEBUG_INFORMATION_LEVEL $<IF:$<CONFIG:Debug>,default,line-tables-only>
)

option(Sanitize "Enable sanitizers" OFF)
if(Sanitize)
    target_compile_options(XDAQ-VC PRIVATE -fsanitize=address,undefined)
    target_link_options(XDAQ-VC PRIVATE -fsanitize=address,undefined)
endif()

target_compile_features(XDAQ-VC PRIVATE cxx_std_20)
target_compile_options(XDAQ-VC
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        # $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
        # $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)

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

install(
    TARGETS XDAQ-VC 
    BUNDLE DESTINATION "."
    RUNTIME DESTINATION "."
)

include("${CMAKE_SOURCE_DIR}/cmake/deploy_qt.cmake")
deploy_qt("${PROJECT_NAME}")

if(WIN32)
    install(
        FILES "${CMAKE_BINARY_DIR}/xdaqvc/xdaqmetadata.dll"
        DESTINATION "."
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