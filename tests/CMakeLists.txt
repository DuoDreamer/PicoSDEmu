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
