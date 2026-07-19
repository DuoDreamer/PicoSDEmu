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
