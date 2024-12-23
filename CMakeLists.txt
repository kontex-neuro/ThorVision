cmake_minimum_required(VERSION 3.27)

set(desktop_app_version 0.0.2)

project(XDAQ-VC
    VERSION "${desktop_app_version}"
    LANGUAGES CXX
    DESCRIPTION "XDAQ Desktop Video Capture App"
    HOMEPAGE_URL "https://kontex.io/" 
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(xdaqvc)

include(cmake/xdaqvc-cpack.cmake)