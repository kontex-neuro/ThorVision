cmake_minimum_required(VERSION 3.5)

# if (POLICY CMP0149)
#     cmake_policy(SET CMP0149 NEW)
# endif()

project(video-capture 
    VERSION 1.0.0 
    LANGUAGES CXX
    DESCRIPTION "XDAQ Video Capture"
)

# add_compile_definitions($<$<CXX_COMPILER_ID:MSVC>:_HAS_STD_BYTE=0>)

set(CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake
    ${CMAKE_MODULE_PATH}
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
# set(CMAKE_AUTOUIC ON)

# if(CMAKE_VERSION VERSION_LESS "3.7.0")
#     set(CMAKE_INCLUDE_CURRENT_DIR ON)
# endif()

# add_link_options(LINKER:-no_warn_duplicate_libraries)

set(Boost_USE_STATIC_LIBS ON)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(nlohmann_json_schema_validator REQUIRED)
find_package(cpr REQUIRED)
find_package(Boost 1.81.0 REQUIRED COMPONENTS program_options)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core OpenGLWidgets)
find_package(PkgConfig REQUIRED)
pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
pkg_search_module(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
pkg_search_module(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)
pkg_search_module(gstreamer-codecparsers REQUIRED IMPORTED_TARGET gstreamer-codecparsers-1.0>=1.4)

find_package(xdaqmetadata REQUIRED)

add_subdirectory(src)

add_library(libxvc STATIC
    libxvc.h
    libxvc.cc
)
# target_sources(libxvc PUBLIC
#     FILE_SET public_headers
#     TYPE HEADERS
#     FILES
#         libxvc.h
# )
target_include_directories(libxvc INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
    # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_compile_features(libxvc PUBLIC cxx_std_20)
target_compile_options(libxvc PRIVATE $<$<CXX_COMPILER_ID:Clang,GNU>:-Wno-deprecated>)
target_compile_definitions(libxvc PUBLIC $<$<CONFIG:Debug>:DEBUG>)
target_link_libraries(libxvc PUBLIC
    fmt::fmt
    cpr::cpr
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    PkgConfig::gstreamer
    PkgConfig::gstreamer-app
    PkgConfig::gstreamer-video
    PkgConfig::gstreamer-codecparsers
)
# set_property(TARGET libxvc PROPERTY POSITION_INDEPENDENT_CODE ON)

add_executable(
    video-capture MACOSX_BUNDLE
    main.cpp
    ${srcSources}
    ${includeSources}
)

# add_executable(
#     xvc-tool xvc_tool.cc
# )
# target_link_libraries(xvc-tool PRIVATE
#     fmt::fmt
#     cpr::cpr
#     Boost::program_options
#     nlohmann_json::nlohmann_json
#     spdlog::spdlog
#     PkgConfig::gstreamer
#     PkgConfig::gstreamer-app
#     PkgConfig::gstreamer-video
#     PkgConfig::gstreamer-codecparsers
#     libxvc
# )
# target_compile_features(xvc-tool PUBLIC cxx_std_20)
# target_compile_options(xvc-tool PRIVATE -Wall)

target_include_directories(video-capture PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

option(Sanitize "Enable sanitizers" OFF)
if(Sanitize)
    target_compile_options(video-capture PRIVATE -fsanitize=address,undefined)
    target_link_options(video-capture PRIVATE -fsanitize=address,undefined)
endif()
target_compile_features(video-capture PRIVATE cxx_std_20)
target_compile_options(video-capture PRIVATE $<$<CXX_COMPILER_ID:Clang,GNU>:-Wno-deprecated>)
target_compile_definitions(video-capture PUBLIC $<$<CONFIG:Debug>:DEBUG>)

target_link_libraries(video-capture PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGLWidgets
    nlohmann_json::nlohmann_json
    PkgConfig::gstreamer
    PkgConfig::gstreamer-app
    PkgConfig::gstreamer-video
    spdlog::spdlog
    fmt::fmt
    libxvc
    xdaqmetadata::xdaqmetadata
)

# if(WIN32)
#     get_filename_component(_qt_bin_dir "${qmake_executable}" DIRECTORY)
#     find_program(DEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
#     add_custom_target(deploy ALL
#         COMMAND ${DEPLOYQT_EXECUTABLE} --release $<TARGET_FILE:video-capture> --dir video_capture
#         COMMENT "Running windeployqt to bundle Qt dependencies"
#         DEPENDS video-capture
#     )
#     install(TARGETS video-capture
#         DESTINATION video_capture
#     )
#     install(DIRECTORY ${CMAKE_BINARY_DIR}/video_capture/
#         DESTINATION video_capture
#         FILES_MATCHING PATTERN "*"
#     )
# endif()