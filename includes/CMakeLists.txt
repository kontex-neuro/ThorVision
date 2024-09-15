# set(includeSources
#     libxvc.cc
#     libxvc.h
# )

# add_library(libxvc STATIC
#     libxvc.h
#     libxvc.cc
# )
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
# target_compile_features(libxvc PUBLIC cxx_std_23)
# target_compile_options(libxvc PRIVATE -Wall)
# target_link_libraries(libxvc PUBLIC
#     fmt::fmt
#     # cpr::cpr
#     PkgConfig::gstreamer
#     PkgConfig::gstreamer-video
# )
# set_property(TARGET libxvc PROPERTY POSITION_INDEPENDENT_CODE ON)