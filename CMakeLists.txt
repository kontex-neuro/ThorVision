cmake_minimum_required(VERSION 3.27)

project(ThorVision
    LANGUAGES CXX
    VERSION "1.2.2"
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "default install path" FORCE)
endif()

option(USE_ASAN "Enable AddressSanitizer (ASan)" OFF)
option(USE_UBSAN "Enable UndefinedBehaviorSanitizer (UBSan)" OFF)
option(USE_TSAN "Enable ThreadSanitizer (TSan)" OFF)

if(USE_ASAN AND USE_TSAN)
    message(
        FATAL_ERROR
        "AddressSanitizer (ASan) and ThreadSanitizer (TSan) are mutually exclusive and cannot be enabled at the same time."
    )
endif()

set(SANITIZER_COMPILE_FLAGS "")
set(SANITIZER_LINK_FLAGS "")

if(USE_ASAN)
    if(WIN32 AND MSVC)
        list(APPEND SANITIZER_COMPILE_FLAGS /fsanitize=address)
        list(APPEND SANITIZER_LINK_FLAGS /fsanitize=address)
    else()
        list(APPEND SANITIZER_COMPILE_FLAGS -fsanitize=address)
        list(APPEND SANITIZER_LINK_FLAGS -fsanitize=address)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            list(APPEND SANITIZER_LINK_FLAGS -lpthread)
        endif()
    endif()
endif()

if(USE_UBSAN)
    if(WIN32 AND MSVC)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            list(APPEND SANITIZER_COMPILE_FLAGS -fsanitize=undefined)
            list(APPEND SANITIZER_LINK_FLAGS -fsanitize=undefined)
        else()
            message(
                WARNING
                "UndefinedBehaviorSanitizer (UBSan) is not directly supported via a simple flag in MSVC."
            )
        endif()
    else()
        list(APPEND SANITIZER_COMPILE_FLAGS -fsanitize=undefined)
        list(APPEND SANITIZER_LINK_FLAGS -fsanitize=undefined)
    endif()
endif()

if(USE_TSAN)
    if(WIN32 AND MSVC)
        message(
            FATAL_ERROR
            "ThreadSanitizer (TSan) is not supported by the MSVC compiler. Please use Clang or GCC."
        )
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        message(FATAL_ERROR "ThreadSanitizer (TSan) is not supported on macOS.")
    else()
        list(APPEND SANITIZER_COMPILE_FLAGS -fsanitize=thread)
        list(APPEND SANITIZER_LINK_FLAGS -fsanitize=thread)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            list(APPEND SANITIZER_LINK_FLAGS -lpthread)
        endif()
    endif()
endif()

find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(xdaqmetadata REQUIRED)
find_package(libxvc REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Qml Quick QuickControls2 Svg)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(oatpp REQUIRED)
find_package(PkgConfig REQUIRED) 
pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
pkg_search_module(gstreamer-codecparsers REQUIRED IMPORTED_TARGET gstreamer-codecparsers-1.0>=1.4)

add_subdirectory(thorvision)

include(cmake/cpack_app.cmake)