#pragma once

#include <cstdint>
#include <string_view>

#include "picosd/protocol/cdc_write_through_backend.hpp"

namespace picosd::firmware {

// Services the firmware side of the image-host session without blocking the
// SPI worker. Complete response lines are supplied by the CDC shell so control
// commands and backend traffic share one bounded line reader.
void initialize_cdc_backend_service();
void poll_cdc_backend_service();
bool handle_cdc_backend_response(std::string_view line);

// The service owns the fixed sector pool used by the eventual SD-model
// adapter. Exposing it here keeps all request dispatch on the main loop.
using FirmwareCdcBackend = picosd::protocol::CdcWriteThroughBackend<8>;
FirmwareCdcBackend &cdc_backend();
bool begin_cdc_read(std::uint64_t lba, std::uint64_t generation);
bool begin_cdc_write(std::uint64_t lba, std::uint64_t generation,
                     const picosd::protocol::CdcBlockData &data);
bool copy_cdc_ready(std::uint64_t lba, std::uint64_t generation,
                    picosd::protocol::CdcBlockData &output);

} // namespace picosd::firmware
