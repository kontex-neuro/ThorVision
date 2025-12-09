cmake_minimum_required(VERSION 3.27)

set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum OS X deployment version" FORCE)

project(ThorVision
    VERSION "1.0.1"
    LANGUAGES CXX
    DESCRIPTION "ThorVision Desktop Video Capture GUI App"
    HOMEPAGE_URL "https://github.com/kontex-neuro/ThorVision" 
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(thorvision)

include(cmake/cpack_app.cmake)