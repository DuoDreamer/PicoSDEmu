add_library(picosd-protocol-core
    src/crc.cpp
)

target_include_directories(picosd-protocol-core PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_features(picosd-protocol-core PUBLIC cxx_std_17)
set_target_properties(picosd-protocol-core PROPERTIES
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(picosd-protocol-core PRIVATE /W4 /permissive-)
else()
    target_compile_options(picosd-protocol-core PRIVATE -Wall -Wextra -Wpedantic)
endif()
