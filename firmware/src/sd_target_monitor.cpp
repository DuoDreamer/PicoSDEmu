#include "picosd/sd_target_monitor.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "hardware/gpio.h"
#include "pico/time.h"
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
constexpr std::uint64_t kTransactionTimeoutUs = 250'000;

picosd::protocol::RamBlockBackend backend{kMonitorBlocks};
picosd::protocol::SdCardModel card{picosd::protocol::SdCardType::Sdsc, backend};
picosd::protocol::SdSpiCardEngine engine{card};
picosd::protocol::SdSpiTargetWorker<kReceiveQueueBytes, kTransmitQueueBytes> worker{engine};
bool trace_enabled = false;
bool target_enabled = false;
bool chip_select_was_asserted = false;
bool transaction_timed_out = false;
std::uint64_t last_activity_us = 0;

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
    const std::uint64_t now = time_us_64();
    if (!chip_select_was_asserted && chip_select_asserted) {
        last_activity_us = now;
        transaction_timed_out = false;
    }
    if (chip_select_was_asserted && !chip_select_asserted) {
        worker.chip_select_released();
        cancel_spi_transmit();
        transaction_timed_out = false;
        if (trace_enabled) std::printf("TRACE_TARGET_CS released\n");
    }
    chip_select_was_asserted = chip_select_asserted;
}

void check_transaction_timeout() {
    if (!chip_select_was_asserted || transaction_timed_out) return;
    if (time_us_64() - last_activity_us < kTransactionTimeoutUs) return;
    worker.transaction_timed_out();
    cancel_spi_transmit();
    transaction_timed_out = true;
    if (trace_enabled) std::printf("TRACE_TARGET_TIMEOUT\n");
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
    if (!target_enabled) return false;
    update_chip_select_state();

    for (unsigned int count = 0; count < kMaximumBytesPerPoll; ++count) {
        std::uint8_t byte = 0;
        if (!try_read_spi_capture_byte(byte)) break;
        last_activity_us = time_us_64();
        (void)worker.capture_byte(byte);
        if (trace_enabled) std::printf("TRACE_TARGET_RX %02X\n", static_cast<unsigned>(byte));
    }

    check_transaction_timeout();
    if (transaction_timed_out) return true;
    worker.process(kMaximumBytesPerPoll);
    drain_transmit_queue();
    return true;
}

void print_sd_target_monitor_counters() {
    const auto& worker_counters = worker.counters();
    const auto& engine_counters = engine.counters();
    std::printf(
        "TARGET_COUNTERS rx=%zu tx=%zu rx_overflow=%zu tx_overflow=%zu tx_underrun=%zu "
        "aborted=%zu crc_command=%zu crc_data=%zu timeout=%zu\n",
        worker_counters.received_bytes, worker_counters.transmitted_bytes,
        worker_counters.receive_overflows, worker_counters.transmit_overflows,
        worker_counters.transmit_underruns, engine_counters.aborted_transactions,
        engine_counters.command_crc_errors, engine_counters.data_crc_errors,
        worker_counters.timeouts);
}

void set_sd_target_monitor_trace_enabled(bool enabled) {
    trace_enabled = enabled;
}

void set_sd_target_monitor_enabled(bool enabled) {
    target_enabled = enabled;
    if (!enabled) {
        worker.chip_select_released();
        cancel_spi_transmit();
        chip_select_was_asserted = false;
        transaction_timed_out = false;
    }
}

}  // namespace picosd::firmware
