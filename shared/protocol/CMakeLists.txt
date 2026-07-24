add_library(picosd-protocol-core
    src/codec.cpp
    src/crc.cpp
    src/sd_card_state.cpp
    src/sd_registers.cpp
    src/sd_command.cpp
    src/sd_spi_card_engine.cpp
    src/sd_spi_data_framer.cpp
    src/sd_spi_command_framer.cpp
    src/sd_model.cpp
    src/text_line.cpp
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
