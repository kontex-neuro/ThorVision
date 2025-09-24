cmake_minimum_required(VERSION 3.27)

set(major "1")
set(minor "0")
set(patch "0")
set(version "${major}.${minor}.${patch}")

set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum OS X deployment version" FORCE)

project(ThorVision
    VERSION "${version}"
    LANGUAGES CXX
    DESCRIPTION "Thor Vision Desktop Video Capture GUI App"
    HOMEPAGE_URL "https://github.com/kontex-neuro/ThorVision" 
)

if(APPLE)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fexperimental-library>)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(thorvision)

include(cmake/cpack_app.cmake)

option(BUILD_DOC "Build documentation for Thor Vision" OFF)

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