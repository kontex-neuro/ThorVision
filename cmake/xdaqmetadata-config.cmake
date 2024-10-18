add_library(xdaqmetadata::xdaqmetadata SHARED IMPORTED)

message(STATUS "PROJECT_SOURCE_DIR = ${PROJECT_SOURCE_DIR}")
target_include_directories(xdaqmetadata::xdaqmetadata INTERFACE
    ${PROJECT_SOURCE_DIR}/ThirdParty/xdaqmetadata
)

if(APPLE)
    set_target_properties(xdaqmetadata::xdaqmetadata PROPERTIES
        IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/ThirdParty/xdaqmetadata/lib/xdaqmetadata.dylib
        # INTERFACE_INCLUDE_DIRECTORIES ${PROJECT_SOURCE_DIR}/ThirdParty/xdaqmetadata/include/
    )
elseif(WIN32)
    set_target_properties(xdaqmetadata::xdaqmetadata PROPERTIES
        IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/ThirdParty/xdaqmetadata/bin/xdaqmetadata.dll
        IMPORTED_IMPLIB ${PROJECT_SOURCE_DIR}/ThirdParty/xdaqmetadata/lib/xdaqmetadata.lib
        # INTERFACE_INCLUDE_DIRECTORIES ${PROJECT_SOURCE_DIR}/ThirdParty/xdaqmetadata/include/
    )
else()
    set_target_properties(xdaqmetadata::xdaqmetadata PROPERTIES
        IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/ThirdParty/xdaqmetadata/bin/xdaqmetadata.so
        IMPORTED_NO_SONAME TRUE
        # INTERFACE_INCLUDE_DIRECTORIES ${PROJECT_SOURCE_DIR}/ThirdParty/xdaqmetadata/include/
    )
endif()

set(xdaqmetadata_LIBRARIES xdaqmetadata::xdaqmetadata)
set(xdaqmetadata_FOUND TRUE)