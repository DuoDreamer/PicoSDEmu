cmake_minimum_required(VERSION 3.20)

project(picosd_image_host LANGUAGES CXX)

include(CTest)

# The Pico firmware has its own toolchain-aware CMake entry point and is built
# separately with: cmake -S firmware -B build/firmware.
add_subdirectory(shared/protocol)
add_subdirectory(host/terminal)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
