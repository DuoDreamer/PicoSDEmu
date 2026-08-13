#include "picosd/sd_target_monitor.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "hardware/gpio.h"
#include "picosd/board_config.hpp"
#include "picosd/protocol/sd_model.hpp"
#include "picosd/protocol/sd_spi_card_engine.hpp"
#include "picosd/protocol/sd_spi_target_worker.hpp"
#include "picosd/spi_capture.hpp"
#include "picosd/spi_transmit.hpp"

namespace picosd::firmware {
namespace {
constexpr std::size_t kMonitorBlocks = 16;
constexpr std::size_t kReceiveQueueBytes = 128;
constexpr std::size_t kTransmitQueueBytes = 1024;
constexpr unsigned int kMaximumBytesPerPoll = 16;

picosd::protocol::RamBlockBackend backend{kMonitorBlocks};
picosd::protocol::SdCardModel card{picosd::protocol::SdCardType::Sdsc, backend};
picosd::protocol::SdSpiCardEngine engine{card};
picosd::protocol::SdSpiTargetWorker<kReceiveQueueBytes, kTransmitQueueBytes> worker{engine};
bool trace_enabled = false;
bool chip_select_was_asserted = false;

void drain_transmit_queue() {
    std::uint8_t byte = 0;
    // The RP2350 FIFO is deliberately the back-pressure boundary: leave bytes
    // in the portable worker until PIO has room rather than discarding them.
    while (worker.pending_transmit_bytes() != 0) {
        if (!spi_transmit_ready()) return;
        const bool dequeued = worker.dequeue_transmit_byte(byte);
        (void)dequeued;
        (void)try_write_spi_transmit_byte(byte);
        if (trace_enabled) std::printf("TRACE_TARGET_TX %02X\n", static_cast<unsigned>(byte));
    }
}

void update_chip_select_state() {
    const bool chip_select_asserted = gpio_get(board::kClientChipSelectPin) == 0;
    if (chip_select_was_asserted && !chip_select_asserted) {
        worker.chip_select_released();
        cancel_spi_transmit();
        if (trace_enabled) std::printf("TRACE_TARGET_CS released\n");
    }
    chip_select_was_asserted = chip_select_asserted;
}
}  // namespace

void initialize_sd_target_monitor() {
    picosd::protocol::SdBlock block{};
    for (std::size_t block_index = 0; block_index < backend.block_count(); ++block_index) {
        for (std::size_t byte_index = 0; byte_index < block.size(); ++byte_index) {
            block[byte_index] = static_cast<std::uint8_t>((block_index + byte_index) & 0xffU);
        }
        (void)backend.write_block(block_index, block);
    }
}

bool poll_sd_target_monitor() {
    if (!trace_enabled) return false;
    update_chip_select_state();

    for (unsigned int count = 0; count < kMaximumBytesPerPoll; ++count) {
        std::uint8_t byte = 0;
        if (!try_read_spi_capture_byte(byte)) break;
        (void)worker.capture_byte(byte);
        std::printf("TRACE_TARGET_RX %02X\n", static_cast<unsigned>(byte));
    }

    worker.process(kMaximumBytesPerPoll);
    drain_transmit_queue();
    return true;
}

void set_sd_target_monitor_trace_enabled(bool enabled) {
    trace_enabled = enabled;
    if (!enabled) {
        worker.chip_select_released();
        cancel_spi_transmit();
        chip_select_was_asserted = false;
    }
}

}  // namespace picosd::firmware
