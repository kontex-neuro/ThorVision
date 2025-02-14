cmake_minimum_required(VERSION 3.27)

set(desktop_app_version 0.0.3)

project(ThorVision
    VERSION "${desktop_app_version}"
    LANGUAGES CXX
    DESCRIPTION "Thor Vision Desktop Video Capture GUI App"
    HOMEPAGE_URL "https://kontex.io/" 
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(thorvision)

include(cmake/cpack_app.cmake)

option(BUILD_DOC "Build documentation for Thor Vision GUI" OFF)

if(BUILD_DOC)
    find_program(MKDOCS mkdocs)
    
    if(NOT MKDOCS)
        message(WARNING "Failed to build documentation: MkDocs not found.")
        return()
    endif()

    set(DOC_SOURCES)
    foreach(SOURCE
        index.md
        getting-started.md
        deployment.md
        user-manual.md
        xdaq-metadata.md
        usb-cameras.md
        camera-latency.md
        post-processing.md
        FAQs.md
        extra.css
    )
        list(APPEND DOC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/docs/docs/${SOURCE}")
    endforeach()

    add_custom_target(
        doc
        COMMAND ${MKDOCS} build 
                --config-file "${CMAKE_CURRENT_SOURCE_DIR}/docs/mkdocs.yml"
                --site-dir "${CMAKE_CURRENT_BINARY_DIR}/site"
                --no-directory-urls
        COMMENT "Building documentation with MkDocs"
        SOURCES "${DOC_SOURCES}"
    )
endif()