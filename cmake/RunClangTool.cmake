if(NOT TOOL OR TOOL MATCHES "-NOTFOUND$")
    message(FATAL_ERROR "The requested Clang tool was not found. Install clang-format and clang-tidy, then reconfigure.")
endif()

if(MODE STREQUAL "format")
    file(GLOB_RECURSE FILES
        "${SOURCE_ROOT}/firmware/*.cpp" "${SOURCE_ROOT}/firmware/*.hpp"
        "${SOURCE_ROOT}/host/*.cpp" "${SOURCE_ROOT}/host/*.hpp"
        "${SOURCE_ROOT}/shared/*.cpp" "${SOURCE_ROOT}/shared/*.hpp"
        "${SOURCE_ROOT}/tests/*.cpp"
    )
    execute_process(COMMAND "${TOOL}" -i ${FILES} RESULT_VARIABLE result)
elseif(MODE STREQUAL "tidy")
    file(GLOB_RECURSE FILES
        "${SOURCE_ROOT}/host/*.cpp"
        "${SOURCE_ROOT}/shared/*.cpp"
        "${SOURCE_ROOT}/tests/*.cpp"
    )
    execute_process(
        COMMAND "${TOOL}" -p "${BUILD_DIRECTORY}" ${FILES}
        RESULT_VARIABLE result
    )
else()
    message(FATAL_ERROR "Unknown Clang tool mode: ${MODE}")
endif()

if(NOT FILES)
    message(FATAL_ERROR "No source files were supplied to ${TOOL}")
endif()

if(NOT result EQUAL 0)
    message(FATAL_ERROR "${TOOL} exited with status ${result}")
endif()
