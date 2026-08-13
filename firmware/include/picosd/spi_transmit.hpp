#pragma once

#include <cstdint>

namespace picosd::firmware {

// Starts the client SPI MISO PIO state machine. The state machine leaves MISO
// high impedance while CS is inactive and whenever no response byte is queued.
void initialize_spi_transmit();

[[nodiscard]] bool spi_transmit_ready();

// Queues one byte for transmission, returning false when the fixed DMA staging
// ring is full. Bytes are shifted most-significant bit first.
bool try_write_spi_transmit_byte(std::uint8_t byte);
// Reaps a completed DMA span and starts the next contiguous queued span.
void service_spi_transmit();

// Discards response bytes belonging to an SPI transaction that has ended.
void cancel_spi_transmit();

}  // namespace picosd::firmware
