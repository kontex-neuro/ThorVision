cmake_minimum_required(VERSION 3.5)

project(video-capture VERSION 1.0.0 LANGUAGES CXX)

# set(CMAKE_CXX_STANDARD 11)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

# add_compile_definitions($<$<CXX_COMPILER_ID:MSVC>:_HAS_STD_BYTE=0>)


set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
# set(CMAKE_AUTOUIC ON)

# if(CMAKE_VERSION VERSION_LESS "3.7.0")
#     set(CMAKE_INCLUDE_CURRENT_DIR ON)
# endif()

add_link_options(LINKER:-no_warn_duplicate_libraries)

set(Boost_USE_STATIC_LIBS ON)
find_package(fmt REQUIRED)
find_package(Boost 1.81.0 REQUIRED COMPONENTS program_options)
find_package(Qt5 COMPONENTS Widgets Multimedia MultimediaWidgets REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(nlohmann_json_schema_validator REQUIRED)
# find_package(CURL REQUIRED)
find_package(cpr REQUIRED)
pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
pkg_search_module(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
pkg_search_module(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)
pkg_search_module(gstreamer-codecparsers REQUIRED IMPORTED_TARGET gstreamer-codecparsers-1.0>=1.4)

# add_subdirectory(includes)
add_subdirectory(src)

# set(includeSources
#     libxvc.cc
#     libxvc.h
# )
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
# target_include_directories(libxvc INTERFACE
#     # ${CMAKE_CURRENT_SOURCE_DIR}/includes
#     $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
# )
target_compile_features(libxvc PUBLIC cxx_std_23)
target_compile_options(libxvc PRIVATE -Wall)
target_link_libraries(libxvc PUBLIC
    fmt::fmt
    cpr::cpr
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
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
    Qt5::Widgets
    Qt5::Multimedia
    Qt5::MultimediaWidgets
    nlohmann_json::nlohmann_json
    PkgConfig::gstreamer
    PkgConfig::gstreamer-app
    PkgConfig::gstreamer-video
    fmt::fmt
    libxvc
)
