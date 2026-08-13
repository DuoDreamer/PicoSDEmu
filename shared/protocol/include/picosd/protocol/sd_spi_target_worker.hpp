#pragma once

#include <cstddef>
#include <cstdint>

#include "picosd/protocol/fixed_ring_queue.hpp"
#include "picosd/protocol/sd_spi_card_engine.hpp"

namespace picosd::protocol {

struct SdSpiTargetWorkerCounters {
    std::size_t received_bytes = 0;
    std::size_t transmitted_bytes = 0;
    std::size_t receive_overflows = 0;
    std::size_t transmit_overflows = 0;
};

// Bounded queue worker that decouples captured SPI bytes from the portable
// card engine. Firmware will connect its PIO/DMA queues to this same policy;
// this class deliberately does not touch hardware or start a timing loop.
template <std::size_t ReceiveCapacity, std::size_t TransmitCapacity>
class SdSpiTargetWorker {
public:
    explicit SdSpiTargetWorker(SdSpiCardEngine& engine) : engine_(engine) {}

    bool capture_byte(std::uint8_t byte) {
        if (!received_.try_push(byte)) {
            ++counters_.receive_overflows;
            return false;
        }
        ++counters_.received_bytes;
        return true;
    }

    // Handles no more than max_bytes captured bytes, keeping a firmware main
    // loop bounded even if a producer is active.
    void process(std::size_t max_bytes) {
        std::uint8_t byte = 0;
        while (max_bytes-- != 0 && received_.try_pop(byte)) {
            const auto output = engine_.push_byte(byte);
            if (output.has_value()) enqueue_output(*output);
        }
    }

    bool dequeue_transmit_byte(std::uint8_t& byte) {
        if (!transmit_.try_pop(byte)) return false;
        ++counters_.transmitted_bytes;
        return true;
    }

    bool queue_next_multi_read_block() {
        const auto output = engine_.next_multi_read_block();
        return output.has_value() && enqueue_output(*output);
    }

    void chip_select_released() {
        received_.clear();
        transmit_.clear();
        engine_.chip_select_released();
    }

    [[nodiscard]] const SdSpiTargetWorkerCounters& counters() const { return counters_; }
    [[nodiscard]] std::size_t pending_receive_bytes() const { return received_.size(); }
    [[nodiscard]] std::size_t pending_transmit_bytes() const { return transmit_.size(); }

private:
    bool enqueue_output(const SdSpiEngineOutput& output) {
        if (output.size > transmit_.available()) {
            ++counters_.transmit_overflows;
            return false;
        }
        for (std::size_t index = 0; index < output.size; ++index) {
            const bool pushed = transmit_.try_push(output.bytes[index]);
            if (!pushed) return false;
        }
        return true;
    }

    SdSpiCardEngine& engine_;
    FixedRingQueue<std::uint8_t, ReceiveCapacity> received_;
    FixedRingQueue<std::uint8_t, TransmitCapacity> transmit_;
    SdSpiTargetWorkerCounters counters_{};
};

}  // namespace picosd::protocol
