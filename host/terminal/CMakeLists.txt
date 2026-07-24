add_executable(picosd-host
    src/main.cpp
    src/image_file.cpp
    src/serve_options.cpp
    src/cdc_transport.cpp
    src/posix_cdc_transport.cpp
    src/windows_cdc_transport.cpp
    src/session_dispatcher.cpp
    src/session_runner.cpp
)

target_link_libraries(picosd-host PRIVATE picosd-protocol-core)

target_include_directories(picosd-host PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../shared/protocol/include
)

target_compile_features(picosd-host PRIVATE cxx_std_17)
set_target_properties(picosd-host PROPERTIES
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(picosd-host PRIVATE /W4 /permissive-)
else()
    target_compile_options(picosd-host PRIVATE -Wall -Wextra -Wpedantic)
endif()
