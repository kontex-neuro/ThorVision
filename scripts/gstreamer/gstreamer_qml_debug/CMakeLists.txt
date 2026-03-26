cmake_minimum_required(VERSION 3.21)
project(gstqt6d3d11_qml_plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(PkgConfig REQUIRED)

# Use the debug-built GStreamer from our local build
set(GST_BUILD_DIR "${CMAKE_SOURCE_DIR}/../gstreamer/builddir")
set(GST_SRC_DIR "${CMAKE_SOURCE_DIR}/../gstreamer")

# GStreamer paths - use installed GStreamer for base libs
set(GST_INSTALL_DIR "C:/gstreamer/1.0/msvc_x86_64")

# Set PKG_CONFIG_PATH to find GStreamer
set(ENV{PKG_CONFIG_PATH} "${GST_INSTALL_DIR}/lib/pkgconfig;$ENV{PKG_CONFIG_PATH}")

pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0)
pkg_search_module(gstreamer_video REQUIRED IMPORTED_TARGET gstreamer-video-1.0)

# Source files from GStreamer qt6d3d11 plugin
set(GST_QT6_SRC_DIR "${GST_SRC_DIR}/subprojects/gst-plugins-bad/ext/qt6d3d11")
set(GST_D3D11_SRC_DIR "${GST_SRC_DIR}/subprojects/gst-plugins-bad/gst-libs/gst/d3d11")

# Create QML module with proper Qt plugin metadata
qt_add_qml_module(gstqt6d3d11
    URI org.freedesktop.gstreamer.Qt6D3D11VideoItem
    VERSION 1.0
    PLUGIN_TARGET gstqt6d3d11
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/org/freedesktop/gstreamer/Qt6D3D11VideoItem"
    NO_GENERATE_PLUGIN_SOURCE
    SOURCES
        ${GST_QT6_SRC_DIR}/gstqt6d3d11videoitem.cpp
        ${GST_QT6_SRC_DIR}/gstqt6d3d11videoitem.h
        ${GST_QT6_SRC_DIR}/gstqsg6d3d11node.cpp
        ${GST_QT6_SRC_DIR}/gstqsg6d3d11node.h
        plugin_init.cpp
)

target_compile_definitions(gstqt6d3d11 PRIVATE
    GST_USE_UNSTABLE_API
    HAVE_CONFIG_H
)

target_include_directories(gstqt6d3d11 PRIVATE
    ${GST_QT6_SRC_DIR}
    ${GST_D3D11_SRC_DIR}
    ${GST_BUILD_DIR}/subprojects/gst-plugins-bad
    ${GST_BUILD_DIR}/subprojects/gst-plugins-bad/gst-libs
    ${GST_BUILD_DIR}/subprojects/gstreamer
    ${GST_SRC_DIR}/subprojects/gst-plugins-bad/gst-libs
    ${GST_SRC_DIR}/subprojects/gstreamer/libs
    ${GST_INSTALL_DIR}/include/gstreamer-1.0
    ${GST_INSTALL_DIR}/include/glib-2.0
    ${GST_INSTALL_DIR}/lib/glib-2.0/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link to debug-built d3d11 library and installed GStreamer
target_link_libraries(gstqt6d3d11 PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    PkgConfig::gstreamer
    PkgConfig::gstreamer_video
    ${GST_BUILD_DIR}/subprojects/gst-plugins-bad/gst-libs/gst/d3d11/gstd3d11-1.0.lib
    d3d11
    dxgi
)

# MSVC debug settings
if(MSVC)
    target_compile_options(gstqt6d3d11 PRIVATE /Zi /Od)
    target_link_options(gstqt6d3d11 PRIVATE /DEBUG:FULL)
endif()

# Copy output to gstreamer_qml folder after build
add_custom_command(TARGET gstqt6d3d11 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/org/freedesktop/gstreamer/Qt6D3D11VideoItem"
        "${CMAKE_SOURCE_DIR}/../gstreamer_qml/org/freedesktop/gstreamer/Qt6D3D11VideoItem"
    COMMENT "Copying QML plugin to gstreamer_qml folder"
)
