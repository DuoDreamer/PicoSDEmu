cmake_minimum_required(VERSION 3.20)

project(picosd_image_host LANGUAGES CXX)

include(CTest)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(PICOSD_CLANG_FORMAT NAMES clang-format)
find_program(PICOSD_CLANG_TIDY NAMES clang-tidy)

add_custom_target(format
    COMMAND "${CMAKE_COMMAND}"
        -DTOOL=${PICOSD_CLANG_FORMAT}
        -DMODE=format
        -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunClangTool.cmake"
    COMMENT "Formatting project-owned C++ sources"
    VERBATIM
)

add_custom_target(static-analysis
    COMMAND "${CMAKE_COMMAND}"
        -DTOOL=${PICOSD_CLANG_TIDY}
        -DMODE=tidy
        -DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}
        -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunClangTool.cmake"
    DEPENDS picosd-host
    COMMENT "Running clang-tidy over native C++ translation units"
    VERBATIM
)

# The Pico firmware has its own toolchain-aware CMake entry point and is built
# separately with: cmake -S firmware -B build/firmware.
add_subdirectory(shared/protocol)
add_subdirectory(host/terminal)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
