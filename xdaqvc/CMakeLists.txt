add_executable(XDAQ-VC WIN32 MACOSX_BUNDLE)

target_sources(XDAQ-VC 
    PRIVATE
        src/main.cpp

        src/xdaq_camera_control.h
        src/xdaq_camera_control.cc
        src/camera_item_widget.h
        src/camera_item_widget.cc
        src/stream_mainwindow.h
        src/stream_mainwindow.cc
        src/stream_window.h
        src/stream_window.cc
        
        src/record_settings.h
        src/record_settings.cc
        src/camera_record_widget.h
        src/camera_record_widget.cc
        src/save_paths_combobox.h
        src/save_paths_combobox.cc
        src/dir_name_combobox.h
        src/dir_name_combobox.cc
        src/duration_spinbox.h
        src/duration_spinbox.cc
)
set_target_properties(XDAQ-VC PROPERTIES AUTOMOC ON)

option(Sanitize "Enable sanitizers" OFF)
if(Sanitize)
    target_compile_options(XDAQ-VC PRIVATE -fsanitize=address,undefined)
    target_link_options(XDAQ-VC PRIVATE -fsanitize=address,undefined)
endif()

target_compile_features(XDAQ-VC PRIVATE cxx_std_20)

find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
# find_package(nlohmann_json_schema_validator REQUIRED)
find_package(xdaqmetadata REQUIRED)
find_package(libxvc REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core OpenGLWidgets)
find_package(PkgConfig REQUIRED)
pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
pkg_search_module(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
pkg_search_module(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)

target_link_libraries(XDAQ-VC 
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::OpenGLWidgets
        nlohmann_json::nlohmann_json
        PkgConfig::gstreamer
        PkgConfig::gstreamer-app
        PkgConfig::gstreamer-video
        spdlog::spdlog
        fmt::fmt
        xdaqmetadata::xdaqmetadata
        libxvc::libxvc
)

install(
    TARGETS XDAQ-VC 
    DESTINATION "${PROJECT_NAME}"
)

if (WIN32)
    # Copy gstreamer runtime library
    install(
        FILES "gstreamer-1.0-msvc-x86_64-1.24.2.msi"
        DESTINATION "${PROJECT_NAME}"
    )
    # Copy Microsoft C++ runtime library
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${PROJECT_NAME}")
    include(InstallRequiredSystemLibraries)

    deploy_qt("${PROJECT_NAME}")

    set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
    set(CPACK_PACKAGE_VENDOR "KonteX Neuroscience")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "XDAQ Video Capture")
    
    set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
    set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
    set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
    
    # set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    # set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    
    set(CPACK_GENERATOR "INNOSETUP")
    set(CPACK_INNOSETUP_USE_MODERN_WIZARD ON)
    set(CPACK_INNOSETUP_EXTRA_SCRIPTS "${CMAKE_CURRENT_SOURCE_DIR}/call.iss")

    include(CPack)
endif ()