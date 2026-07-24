#pragma once

#include <cstdint>

namespace picosd::firmware {

// Starts the capture-only client SPI PIO state machine. This stage only
// samples MOSI; it leaves MISO as a high-impedance GPIO input.
void initialize_spi_capture();
bool try_read_spi_capture_byte(std::uint8_t& output);
void poll_spi_capture_trace();

}  // namespace picosd::firmware
