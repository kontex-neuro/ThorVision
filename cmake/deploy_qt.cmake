find_package(Qt6 REQUIRED COMPONENTS Core)
get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${qmake_executable}" DIRECTORY)

if(WIN32)
    find_program(DEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
elseif(APPLE)
    find_program(DEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")
endif()

function(deploy_qt TARGET_NAME)
    install(CODE "
        message(STATUS \"Deploy Qt on ${CMAKE_INSTALL_PREFIX}/${INSTALL_PATH}/${TARGET_NAME}.exe\")
        execute_process(
            COMMAND \"${DEPLOYQT_EXECUTABLE}\"
                \"\${CMAKE_INSTALL_PREFIX}/${TARGET_NAME}\"
                --verbose 0
            RESULT_VARIABLE DEPLOY_RESULT
        )
        if (NOT DEPLOY_RESULT EQUAL 0)
            message(FATAL_ERROR \"Deploy Qt failed with error: ${DEPLOY_RESULT}\")
        endif ()
    ")
endfunction()

# qt_generate_deploy_app_script(
#     TARGET XDAQ-VC
#     OUTPUT_SCRIPT deploy_script
#     NO_UNSUPPORTED_PLATFORM_ERROR
# )
# install(SCRIPT ${deploy_script})

# add_custom_target(deploy ALL
#     COMMAND ${DEPLOYQT_EXECUTABLE} --release $<TARGET_FILE:XDAQ-VC> --dir XDAQ-VC
#     COMMENT "Running deployqt to bundle Qt dependencies"
#     DEPENDS XDAQ-VC
# )