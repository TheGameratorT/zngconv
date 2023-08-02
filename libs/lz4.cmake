if(NOT EXISTS "${LIB_DOWNLOAD_DIR}/lz4")
    message(STATUS "Downloading lz4 library")
    file(MAKE_DIRECTORY ${LIB_DOWNLOAD_DIR})
    execute_process(
        COMMAND git clone https://github.com/lz4/lz4.git
        WORKING_DIRECTORY ${LIB_DOWNLOAD_DIR}
        RESULT_VARIABLE GIT_RESULT
    )
    if(NOT GIT_RESULT EQUAL 0)
        message(FATAL_ERROR "Git clone failed with exit code ${GIT_RESULT}")
    endif()
    execute_process(
        COMMAND git -c advice.detachedHead=false checkout 839edadd09bd653b7e9237a2fbf405d9e8bfc14e
        WORKING_DIRECTORY ${LIB_DOWNLOAD_DIR}/lz4
        RESULT_VARIABLE GIT_RESULT
    )
    if(NOT GIT_RESULT EQUAL 0)
        message(FATAL_ERROR "Git checkout failed with exit code ${GIT_RESULT}")
    endif()
endif()

add_subdirectory("${LIB_DOWNLOAD_DIR}/lz4/build/cmake")

target_link_libraries(${PROJECT_NAME} PRIVATE lz4_static)
