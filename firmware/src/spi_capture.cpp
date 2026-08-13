#include "picosd/spi_capture.hpp"

#include <array>
#include <cstdio>

#include "hardware/dma.h"
#include "hardware/pio.h"
#include "picosd/board_config.hpp"
#include "sd_spi_capture.pio.h"

namespace picosd::firmware {
namespace {
PIO pio = pio0;
unsigned int state_machine = 0;
int dma_channel = -1;
bool trace_enabled = false;
constexpr std::size_t kDmaBufferBytes = 256;
alignas(kDmaBufferBytes) std::array<std::uint8_t, kDmaBufferBytes> dma_buffer{};
std::size_t read_offset = 0;

std::size_t dma_write_offset() {
    const auto address = dma_hw->ch[dma_channel].write_addr;
    return static_cast<std::size_t>(static_cast<std::uintptr_t>(address) -
                                    reinterpret_cast<std::uintptr_t>(dma_buffer.data())) &
           (kDmaBufferBytes - 1U);
}
}  // namespace

void initialize_spi_capture() {
    state_machine = static_cast<unsigned int>(pio_claim_unused_sm(pio, true));
    const unsigned int offset = pio_add_program(pio, &picosd_spi_capture_program);
    pio_sm_config configuration = picosd_spi_capture_program_get_default_config(offset);
    sm_config_set_in_pins(&configuration, board::kClientMosiPin);
    sm_config_set_jmp_pin(&configuration, board::kClientChipSelectPin);
    sm_config_set_in_shift(&configuration, false, true, 8);
    pio_sm_init(pio, state_machine, offset, &configuration);
    pio_sm_set_enabled(pio, state_machine, true);

    dma_channel = dma_claim_unused_channel(true);
    dma_channel_config dma_configuration = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&dma_configuration, DMA_SIZE_8);
    channel_config_set_read_increment(&dma_configuration, false);
    channel_config_set_write_increment(&dma_configuration, true);
    channel_config_set_ring(&dma_configuration, true, 8);
    channel_config_set_dreq(&dma_configuration, pio_get_dreq(pio, state_machine, false));
    dma_channel_configure(dma_channel, &dma_configuration, dma_buffer.data(),
                          &pio->rxf[state_machine], UINT32_MAX, true);
}

bool try_read_spi_capture_byte(std::uint8_t& output) {
    const std::size_t write_offset = dma_write_offset();
    if (read_offset == write_offset) return false;
    output = dma_buffer[read_offset];
    read_offset = (read_offset + 1U) & (kDmaBufferBytes - 1U);
    return true;
}

void discard_spi_capture_bytes() { read_offset = dma_write_offset(); }

void poll_spi_capture_trace() {
    // This is a deliberately bounded diagnostic path for the capture-only
    // proof of concept. It must be removed from the timing path before SD
    // responses are enabled.
    if (!trace_enabled) return;
    for (unsigned int count = 0; count < 16; ++count) {
        std::uint8_t byte = 0;
        if (!try_read_spi_capture_byte(byte)) return;
        std::printf("TRACE_SPI %02X\n", static_cast<unsigned>(byte));
    }
}

void set_spi_capture_trace_enabled(bool enabled) { trace_enabled = enabled; }

}  // namespace picosd::firmware
