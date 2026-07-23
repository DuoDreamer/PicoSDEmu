cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED ENV{PICO_SDK_PATH})
    message(FATAL_ERROR "PICO_SDK_PATH must point to a Raspberry Pi Pico SDK checkout")
endif()

set(PICO_BOARD "pico2" CACHE STRING "Pico SDK board definition")
if(NOT PICO_BOARD STREQUAL "pico2")
    message(FATAL_ERROR "This firmware supports only the Raspberry Pi Pico 2 (PICO_BOARD=pico2)")
endif()

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(picosd_firmware VERSION 0.1.0 LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

pico_sdk_init()

add_executable(picosd_firmware
    src/cdc_shell.cpp
    src/main.cpp
)

target_include_directories(picosd_firmware PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/../shared/protocol/include
)

target_link_libraries(picosd_firmware PRIVATE
    pico_stdlib
)

target_compile_options(picosd_firmware PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

pico_enable_stdio_usb(picosd_firmware 1)
pico_enable_stdio_uart(picosd_firmware 0)
pico_add_extra_outputs(picosd_firmware)
