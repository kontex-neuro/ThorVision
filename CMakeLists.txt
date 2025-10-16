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

option(BUILD_DOC "Build documentation for ThorVision" OFF)

if(BUILD_DOC)
    find_program(MKDOCS mkdocs)
    
    if(NOT MKDOCS)
        message(WARNING "Failed to build documentation: MkDocs not found.")
        return()
    endif()

    add_custom_target(
        doc
        COMMAND ${MKDOCS} build 
                --config-file "${CMAKE_SOURCE_DIR}/docs/mkdocs.yml"
                --site-dir "${CMAKE_BINARY_DIR}/site"
        COMMENT "Building documentation with MkDocs"
    )
endif()