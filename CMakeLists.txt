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

option(XVC_DOC "Build documentation for XDAQ-VC" OFF)

if(XVC_DOC)
    find_program(MKDOCS mkdocs)
    
    if(NOT MKDOCS)
        message(WARNING "Documentation build disabled: MkDocs not found.")
        return()
    endif()

    set(DOC_SOURCES)
    foreach(SOURCE
        index.md
        getting-started.md
        deployment.md
        user-manual.md
        metadata.md
        usb-cameras.md
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
                --site-dir "${CMAKE_CURRENT_BINARY_DIR}/docs-html"
                --no-directory-urls
        COMMENT "Building documentation with MkDocs"  
        SOURCES "${DOC_SOURCES}"
    )

    include(GNUInstallDirs)
    install(
        DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/doc-html/"
        DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/docs/"
        COMPONENT doc OPTIONAL
    )
endif()