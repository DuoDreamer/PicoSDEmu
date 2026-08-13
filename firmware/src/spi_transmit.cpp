#include "picosd/spi_transmit.hpp"

#include <array>
#include <cstddef>

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "picosd/board_config.hpp"
#include "sd_spi_transmit.pio.h"

namespace picosd::firmware {
namespace {
PIO pio = pio0;
unsigned int state_machine = 0;
int dma_channel = -1;
constexpr std::size_t kDmaQueueBytes = 1024;
std::array<std::uint32_t, kDmaQueueBytes> dma_queue{};
std::size_t queue_head = 0;
std::size_t queue_tail = 0;
std::size_t queued_bytes = 0;
std::size_t active_bytes = 0;

void finish_completed_transfer() {
    if (active_bytes == 0 || dma_channel_is_busy(dma_channel)) return;
    queue_head = (queue_head + active_bytes) % kDmaQueueBytes;
    queued_bytes -= active_bytes;
    active_bytes = 0;
}
}  // namespace

void initialize_spi_transmit() {
    state_machine = static_cast<unsigned int>(pio_claim_unused_sm(pio, true));
    const unsigned int offset = pio_add_program(pio, &picosd_spi_transmit_program);
    pio_sm_config configuration = picosd_spi_transmit_program_get_default_config(offset);
    sm_config_set_out_pins(&configuration, board::kClientMisoPin, 1);
    sm_config_set_set_pins(&configuration, board::kClientMisoPin, 1);
    sm_config_set_jmp_pin(&configuration, board::kClientChipSelectPin);
    sm_config_set_out_shift(&configuration, false, false, 32);

    // Connecting the pad to PIO does not make it an output. The program owns
    // the direction and only asserts it after observing active-low CS.
    pio_gpio_init(pio, board::kClientMisoPin);
    pio_sm_set_consecutive_pindirs(pio, state_machine, board::kClientMisoPin, 1, false);
    pio_sm_init(pio, state_machine, offset, &configuration);
    pio_sm_set_enabled(pio, state_machine, true);
    dma_channel = dma_claim_unused_channel(true);
}

bool spi_transmit_ready() {
    finish_completed_transfer();
    return queued_bytes < kDmaQueueBytes;
}

bool try_write_spi_transmit_byte(std::uint8_t byte) {
    if (!spi_transmit_ready()) return false;
    dma_queue[queue_tail] = static_cast<std::uint32_t>(byte) << 24U;
    queue_tail = (queue_tail + 1U) % kDmaQueueBytes;
    ++queued_bytes;
    return true;
}

void service_spi_transmit() {
    finish_completed_transfer();
    if (active_bytes != 0 || queued_bytes == 0) return;
    active_bytes = queued_bytes;
    const std::size_t contiguous_bytes = kDmaQueueBytes - queue_head;
    if (active_bytes > contiguous_bytes) active_bytes = contiguous_bytes;
    dma_channel_config configuration = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&configuration, DMA_SIZE_32);
    channel_config_set_read_increment(&configuration, true);
    channel_config_set_write_increment(&configuration, false);
    channel_config_set_dreq(&configuration, pio_get_dreq(pio, state_machine, true));
    dma_channel_configure(dma_channel, &configuration, &pio->txf[state_machine],
                          dma_queue.data() + queue_head, active_bytes, true);
}

void cancel_spi_transmit() {
    dma_channel_abort(dma_channel);
    queue_head = queue_tail = queued_bytes = active_bytes = 0;
    pio_sm_clear_fifos(pio, state_machine);
}

}  // namespace picosd::firmware
