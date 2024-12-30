find_package(Qt6 REQUIRED COMPONENTS Core)
get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${qmake_executable}" DIRECTORY)

function(deploy_qt TARGET_NAME)
    # if (NOT DEPLOYQT_EXECUTABLE)
    #     message(FATAL_ERROR "DEPLOYQT_EXECUTABLE is not set. Please specify the path to the deployqt executable.")
    # endif()

    if(WIN32)
        find_program(DEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

        install(CODE "
            message(STATUS \"Deploy Qt on ${CMAKE_INSTALL_PREFIX}/${TARGET_NAME}.exe\")
            execute_process(
                COMMAND \"${DEPLOYQT_EXECUTABLE}\"
                    \"\${CMAKE_INSTALL_PREFIX}/${TARGET_NAME}.exe\"
                RESULT_VARIABLE DEPLOY_RESULT
            )
            if(NOT DEPLOY_RESULT EQUAL 0)
                message(FATAL_ERROR \"Deploy Qt failed with error: ${DEPLOY_RESULT}\")
            endif()
        ")
    elseif(APPLE)
        find_program(DEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")

        install(CODE "
            message(STATUS \"Deploy Qt on \${CMAKE_INSTALL_PREFIX}/${TARGET_NAME}.app\")
            execute_process(
                COMMAND \"${DEPLOYQT_EXECUTABLE}\"
                        \"\${CMAKE_INSTALL_PREFIX}/${TARGET_NAME}.app\"
                RESULT_VARIABLE DEPLOY_RESULT
            )
            if(NOT DEPLOY_RESULT EQUAL 0)
                message(FATAL_ERROR \"Deploy Qt failed with error: ${DEPLOY_RESULT}\")
            endif()
        ")
    endif()

endfunction()