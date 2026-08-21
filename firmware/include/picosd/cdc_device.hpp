#pragma once

#include <cstddef>
#include <cstdint>

namespace picosd::firmware {

// Owns the TinyUSB CDC device service. All operations are non-blocking so USB
// servicing cannot delay the client-facing SD state machine.
void initialize_cdc_device();
void poll_cdc_device();
bool read_cdc_byte(std::uint8_t &value);
bool write_cdc(const char *data, std::size_t length);
bool cdc_connected();

} // namespace picosd::firmware
