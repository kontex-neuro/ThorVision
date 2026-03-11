cmake_minimum_required(VERSION 3.27)

project(ThorVision
    VERSION "1.2.1"
    LANGUAGES CXX
    DESCRIPTION "ThorVision Desktop Video Capture GUI App"
    HOMEPAGE_URL "https://github.com/kontex-neuro/ThorVision" 
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(thorvision)

include(cmake/cpack_app.cmake)