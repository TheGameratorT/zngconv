function(GenerateArgs SOURCE_DIR BUILD_DIR)
    # Set path for generated files
    set(ARGS_OUTPUT_DIR "${BUILD_DIR}/genargs")

    # Define the input and output files
    set(ARGS_INPUT_FILE "${SOURCE_DIR}/args.ggo")
    set(ARGS_OUTPUT_C "${ARGS_OUTPUT_DIR}/args.c")
    set(ARGS_OUTPUT_H "${ARGS_OUTPUT_DIR}/args.h")

    if(NOT DEFINED PRE_BUILD_STEP)
        # Copy to parent scope
        set(ARGS_OUTPUT_DIR ${ARGS_OUTPUT_DIR} PARENT_SCOPE)
        set(ARGS_INPUT_FILE ${ARGS_INPUT_FILE} PARENT_SCOPE)
        set(ARGS_OUTPUT_C ${ARGS_OUTPUT_C} PARENT_SCOPE)
        set(ARGS_OUTPUT_H ${ARGS_OUTPUT_H} PARENT_SCOPE)
    endif()

    # Check if output files are older than the input file
    set(OUTPUT_FILES "${ARGS_OUTPUT_C}" "${ARGS_OUTPUT_H}")
    set(OUTPUT_OLDER false)
    foreach(OUTPUT_FILE IN LISTS OUTPUT_FILES)
        if(NOT EXISTS ${OUTPUT_FILE} OR ${ARGS_INPUT_FILE} IS_NEWER_THAN ${OUTPUT_FILE})
            set(OUTPUT_OLDER true)
            break()
        endif()
    endforeach()

    if (OUTPUT_OLDER)
        # Execute on configuration too
        file(MAKE_DIRECTORY ${ARGS_OUTPUT_DIR})

        message(STATUS "Generating command line arguments")
        execute_process(
            COMMAND gengetopt "--include-getopt" "-F" "args"
            INPUT_FILE "${SOURCE_DIR}/args.ggo"
            WORKING_DIRECTORY ${ARGS_OUTPUT_DIR}
        )
    endif()
endfunction()

if(DEFINED PRE_BUILD_STEP)
    GenerateArgs(${SOURCE_DIR} ${BUILD_DIR})
    return()
endif()

# Run the code on pre-build as well
GenerateArgs(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

add_custom_target(args_depends)
add_custom_command(
    TARGET args_depends
    COMMAND ${CMAKE_COMMAND} . -D SOURCE_DIR=${CMAKE_SOURCE_DIR} -D BUILD_DIR=${CMAKE_BINARY_DIR} -D PRE_BUILD_STEP= -P ${CMAKE_SOURCE_DIR}/args.cmake
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Define target imports
target_sources(${PROJECT_NAME} PRIVATE ${ARGS_OUTPUT_C})
target_include_directories(${PROJECT_NAME} PRIVATE ${ARGS_OUTPUT_DIR})

# Depend on the args
add_dependencies(${PROJECT_NAME} args_depends)
