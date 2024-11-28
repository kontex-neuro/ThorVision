cmake_minimum_required(VERSION 3.27)

project(XDAQ-VC
    VERSION 0.0.1
    LANGUAGES CXX
    DESCRIPTION "XDAQ Video Capture Desktop"
    HOMEPAGE_URL "https://kontex.io/" 
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(xdaqvc)

include(cmake/xdaqvc-cpack.cmake)