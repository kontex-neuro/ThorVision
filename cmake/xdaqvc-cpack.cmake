set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "KonteX Neuroscience")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "XDAQ Desktop Video Capture App")

set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")

# set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
# set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

if(WIN32)
    set(ICON_PATH "${CMAKE_SOURCE_DIR}/resources/xdaq-icon.ico")
    set(
        GSTREAMER_INSTALLER_PATH "gstreamer-1.0-msvc-*.msi" 
        CACHE PATH "Path to GStreamer runtime library installer"
    )
    if(NOT EXISTS "${GSTREAMER_INSTALLER_PATH}")
        message(
            FATAL_ERROR 
            "-DGSTREAMER_INSTALLER_PATH=/path/to/gstreamer-runtime-library"
        )
    endif()
    install(
        FILES "${GSTREAMER_INSTALLER_PATH}"
        DESTINATION "${PROJECT_NAME}"
    )

    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${PROJECT_NAME}")
    include(InstallRequiredSystemLibraries)
    
    set(CPACK_GENERATOR "INNOSETUP")
    set(CPACK_INNOSETUP_USE_MODERN_WIZARD ON)
    set(CPACK_INNOSETUP_ICON_FILE "${ICON_PATH}")
    list(APPEND CPACK_INNOSETUP_EXTRA_SCRIPTS 
        "${CMAKE_SOURCE_DIR}/cmake/call_gstreamer_installer.iss"
        "${CMAKE_SOURCE_DIR}/cmake/setup.iss"
    )
    set(CPACK_INNOSETUP_CREATE_UNINSTALL_LINK ON)
    
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(ICON_PATH "${CMAKE_SOURCE_DIR}/resources/xdaq-icon.icns")
endif()

include(CPack)