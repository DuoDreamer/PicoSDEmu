add_executable(picosd-protocol-version-tests
    protocol_version_tests.cpp
)

target_include_directories(picosd-protocol-version-tests PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../shared/protocol/include
)

target_compile_features(picosd-protocol-version-tests PRIVATE cxx_std_17)
set_target_properties(picosd-protocol-version-tests PROPERTIES
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(picosd-protocol-version-tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(picosd-protocol-version-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME picosd.protocol.version COMMAND picosd-protocol-version-tests)

add_executable(picosd-crc-tests
    crc_tests.cpp
)

target_link_libraries(picosd-crc-tests PRIVATE
    picosd-protocol-core
)

target_compile_features(picosd-crc-tests PRIVATE cxx_std_17)
set_target_properties(picosd-crc-tests PROPERTIES
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(picosd-crc-tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(picosd-crc-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME picosd.protocol.crc COMMAND picosd-crc-tests)

add_executable(picosd-text-line-tests
    text_line_tests.cpp
)

target_link_libraries(picosd-text-line-tests PRIVATE
    picosd-protocol-core
)

target_compile_features(picosd-text-line-tests PRIVATE cxx_std_17)
set_target_properties(picosd-text-line-tests PROPERTIES
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(picosd-text-line-tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(picosd-text-line-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME picosd.protocol.text_line COMMAND picosd-text-line-tests)

add_executable(picosd-sd-command-tests
    sd_command_tests.cpp
)

target_link_libraries(picosd-sd-command-tests PRIVATE
    picosd-protocol-core
)

target_compile_features(picosd-sd-command-tests PRIVATE cxx_std_17)
set_target_properties(picosd-sd-command-tests PROPERTIES
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(picosd-sd-command-tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(picosd-sd-command-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME picosd.protocol.sd_command COMMAND picosd-sd-command-tests)

add_executable(picosd-sd-spi-command-framer-tests
    sd_spi_command_framer_tests.cpp
)
target_link_libraries(picosd-sd-spi-command-framer-tests PRIVATE picosd-protocol-core)
target_compile_features(picosd-sd-spi-command-framer-tests PRIVATE cxx_std_17)
set_target_properties(picosd-sd-spi-command-framer-tests PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(picosd-sd-spi-command-framer-tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(picosd-sd-spi-command-framer-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME picosd.protocol.sd_spi_command_framer COMMAND picosd-sd-spi-command-framer-tests)

add_executable(picosd-sd-spi-data-framer-tests
    sd_spi_data_framer_tests.cpp
)
target_link_libraries(picosd-sd-spi-data-framer-tests PRIVATE picosd-protocol-core)
target_compile_features(picosd-sd-spi-data-framer-tests PRIVATE cxx_std_17)
set_target_properties(picosd-sd-spi-data-framer-tests PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(picosd-sd-spi-data-framer-tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(picosd-sd-spi-data-framer-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME picosd.protocol.sd_spi_data_framer COMMAND picosd-sd-spi-data-framer-tests)

add_executable(picosd-sd-spi-card-engine-tests
    sd_spi_card_engine_tests.cpp
)
target_link_libraries(picosd-sd-spi-card-engine-tests PRIVATE picosd-protocol-core)
target_compile_features(picosd-sd-spi-card-engine-tests PRIVATE cxx_std_17)
set_target_properties(picosd-sd-spi-card-engine-tests PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(picosd-sd-spi-card-engine-tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(picosd-sd-spi-card-engine-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME picosd.protocol.sd_spi_card_engine COMMAND picosd-sd-spi-card-engine-tests)

add_executable(picosd-sd-card-state-tests sd_card_state_tests.cpp)
target_link_libraries(picosd-sd-card-state-tests PRIVATE picosd-protocol-core)
target_compile_features(picosd-sd-card-state-tests PRIVATE cxx_std_17)
set_target_properties(picosd-sd-card-state-tests PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(picosd-sd-card-state-tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(picosd-sd-card-state-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME picosd.protocol.sd_card_state COMMAND picosd-sd-card-state-tests)

add_executable(picosd-sd-registers-tests sd_registers_tests.cpp)
target_link_libraries(picosd-sd-registers-tests PRIVATE picosd-protocol-core)
target_compile_features(picosd-sd-registers-tests PRIVATE cxx_std_17)
set_target_properties(picosd-sd-registers-tests PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(picosd-sd-registers-tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(picosd-sd-registers-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME picosd.protocol.sd_registers COMMAND picosd-sd-registers-tests)

add_executable(picosd-sd-model-tests sd_model_tests.cpp)
target_link_libraries(picosd-sd-model-tests PRIVATE picosd-protocol-core)
target_compile_features(picosd-sd-model-tests PRIVATE cxx_std_17)
set_target_properties(picosd-sd-model-tests PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(picosd-sd-model-tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(picosd-sd-model-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME picosd.protocol.sd_model COMMAND picosd-sd-model-tests)
add_executable(picosd-codec-tests codec_tests.cpp)
target_link_libraries(picosd-codec-tests PRIVATE picosd-protocol-core)
target_compile_features(picosd-codec-tests PRIVATE cxx_std_17)
set_target_properties(picosd-codec-tests PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(picosd-codec-tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(picosd-codec-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME picosd.protocol.codec COMMAND picosd-codec-tests)

add_executable(picosd-host-image-file-tests
    image_file_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../host/terminal/src/image_file.cpp
)
target_include_directories(picosd-host-image-file-tests PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../host/terminal/src
)
target_compile_features(picosd-host-image-file-tests PRIVATE cxx_std_17)
set_target_properties(picosd-host-image-file-tests PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(picosd-host-image-file-tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(picosd-host-image-file-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME picosd.host.image_file COMMAND picosd-host-image-file-tests)
add_executable(picosd-host-serve-options-tests serve_options_tests.cpp ${CMAKE_CURRENT_LIST_DIR}/../host/terminal/src/serve_options.cpp)
target_include_directories(picosd-host-serve-options-tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../host/terminal/src)
target_compile_features(picosd-host-serve-options-tests PRIVATE cxx_std_17)
set_target_properties(picosd-host-serve-options-tests PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(picosd-host-serve-options-tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(picosd-host-serve-options-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME picosd.host.serve_options COMMAND picosd-host-serve-options-tests)
add_executable(picosd-host-cdc-transport-tests cdc_transport_tests.cpp ${CMAKE_CURRENT_LIST_DIR}/../host/terminal/src/cdc_transport.cpp)
target_include_directories(picosd-host-cdc-transport-tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../host/terminal/src)
target_compile_features(picosd-host-cdc-transport-tests PRIVATE cxx_std_17)
set_target_properties(picosd-host-cdc-transport-tests PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(picosd-host-cdc-transport-tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(picosd-host-cdc-transport-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME picosd.host.cdc_transport COMMAND picosd-host-cdc-transport-tests)
add_executable(picosd-host-session-dispatcher-tests session_dispatcher_tests.cpp ${CMAKE_CURRENT_LIST_DIR}/../host/terminal/src/image_file.cpp ${CMAKE_CURRENT_LIST_DIR}/../host/terminal/src/session_dispatcher.cpp)
target_include_directories(picosd-host-session-dispatcher-tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../host/terminal/src)
target_link_libraries(picosd-host-session-dispatcher-tests PRIVATE picosd-protocol-core)
target_compile_features(picosd-host-session-dispatcher-tests PRIVATE cxx_std_17)
set_target_properties(picosd-host-session-dispatcher-tests PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(picosd-host-session-dispatcher-tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(picosd-host-session-dispatcher-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME picosd.host.session_dispatcher COMMAND picosd-host-session-dispatcher-tests)
add_executable(picosd-host-session-runner-tests session_runner_tests.cpp ${CMAKE_CURRENT_LIST_DIR}/../host/terminal/src/cdc_transport.cpp ${CMAKE_CURRENT_LIST_DIR}/../host/terminal/src/image_file.cpp ${CMAKE_CURRENT_LIST_DIR}/../host/terminal/src/session_dispatcher.cpp ${CMAKE_CURRENT_LIST_DIR}/../host/terminal/src/session_runner.cpp)
target_include_directories(picosd-host-session-runner-tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../host/terminal/src)
target_link_libraries(picosd-host-session-runner-tests PRIVATE picosd-protocol-core)
target_compile_features(picosd-host-session-runner-tests PRIVATE cxx_std_17)
set_target_properties(picosd-host-session-runner-tests PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(picosd-host-session-runner-tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(picosd-host-session-runner-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME picosd.host.session_runner COMMAND picosd-host-session-runner-tests)
