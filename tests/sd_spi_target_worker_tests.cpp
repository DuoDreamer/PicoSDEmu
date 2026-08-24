#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

#include "picosd/protocol/crc.hpp"
#include "picosd/protocol/sd_model.hpp"
#include "picosd/protocol/sd_spi_card_engine.hpp"
#include "picosd/protocol/sd_spi_target_worker.hpp"

namespace {

class DeferredBackend final : public picosd::protocol::BlockBackend {
  public:
    std::size_t block_count() const override {
        return 2048;
    }
    picosd::protocol::BlockOperationResult read(std::size_t,
                                                picosd::protocol::SdBlock &output) const override {
        if (!read_ready)
            return picosd::protocol::BlockOperationResult::Pending;
        output.fill(0x6bU);
        return picosd::protocol::BlockOperationResult::Complete;
    }
    picosd::protocol::BlockOperationResult write(std::size_t,
                                                 const picosd::protocol::SdBlock &) override {
        return write_ready ? picosd::protocol::BlockOperationResult::Complete
                           : picosd::protocol::BlockOperationResult::Pending;
    }

    mutable bool read_ready = false;
    bool write_ready = false;
};

template <std::size_t ReceiveCapacity, std::size_t TransmitCapacity>
void capture_command(picosd::protocol::SdSpiTargetWorker<ReceiveCapacity, TransmitCapacity> &worker,
                     const std::array<std::uint8_t, 6> &command) {
    for (const std::uint8_t byte : command)
        assert(worker.capture_byte(byte));
    worker.process(command.size());
}

template <std::size_t ReceiveCapacity, std::size_t TransmitCapacity>
std::uint8_t
pop_one(picosd::protocol::SdSpiTargetWorker<ReceiveCapacity, TransmitCapacity> &worker) {
    std::uint8_t byte = 0;
    assert(worker.dequeue_transmit_byte(byte));
    return byte;
}

} // namespace

int main() {
    using namespace picosd::protocol;

    RamBlockBackend backend(16);
    backend.fill_diagnostic_pattern();
    SdCardModel model(SdCardType::Sdsc, backend);
    SdSpiCardEngine engine(model);
    SdSpiTargetWorker<16, 600> worker(engine);

    capture_command(worker, {0x40U, 0, 0, 0, 0, 0x95U});
    assert(pop_one(worker) == 0x01U);
    capture_command(worker, {0x48U, 0, 0, 1, 0xaaU, 0x87U});
    assert(pop_one(worker) == 0x01U);
    assert(pop_one(worker) == 0x00U);
    assert(pop_one(worker) == 0x00U);
    assert(pop_one(worker) == 0x01U);
    assert(pop_one(worker) == 0xaaU);
    capture_command(worker, {0x77U, 0, 0, 0, 0, 0x65U});
    assert(pop_one(worker) == 0x01U);
    capture_command(worker, {0x69U, 0x40U, 0, 0, 0, 0x77U});
    assert(pop_one(worker) == 0x00U);

    capture_command(worker, {0x51U, 0, 0, 0, 0, 0x01U});
    assert(worker.pending_transmit_bytes() == 1U + kSdDataBlockWireSize);
    assert(pop_one(worker) == 0x00U);
    assert(pop_one(worker) == kSdStartBlockToken);

    for (std::size_t index = 0; index < 514U; ++index) {
        std::uint8_t discarded = 0;
        assert(worker.dequeue_transmit_byte(discarded));
    }
    assert(worker.pending_transmit_bytes() == 0U);
    assert(worker.counters().received_bytes == 30U);
    assert(worker.counters().transmitted_bytes == 1U + 5U + 1U + 1U + 1U + kSdDataBlockWireSize);

    // Draining a CMD18 response automatically queues the following block. The
    // hardware-facing loop therefore does not need to duplicate model state.
    capture_command(worker, {0x52U, 0, 0, 0, 0, 0x01U});
    assert(pop_one(worker) == 0x00U);
    for (std::size_t index = 0; index < kSdDataBlockWireSize; ++index) {
        std::uint8_t discarded = 0;
        assert(worker.dequeue_transmit_byte(discarded));
    }
    assert(worker.pending_transmit_bytes() == kSdDataBlockWireSize);
    assert(pop_one(worker) == kSdStartBlockToken);
    assert(pop_one(worker) == 37U);
    worker.chip_select_released();

    // A new transaction must not inherit response bytes that were queued for
    // a transaction whose CS line has already been released.
    capture_command(worker, {0x51U, 0, 0, 0, 0, 0x01U});
    assert(worker.pending_transmit_bytes() == 1U + kSdDataBlockWireSize);
    worker.chip_select_released();
    assert(worker.pending_receive_bytes() == 0U);
    assert(worker.pending_transmit_bytes() == 0U);

    SdSpiTargetWorker<2, 4> constrained_worker(engine);
    assert(constrained_worker.capture_byte(0xffU));
    assert(constrained_worker.capture_byte(0xffU));
    assert(!constrained_worker.capture_byte(0xffU));
    assert(constrained_worker.counters().receive_overflows == 1U);
    constrained_worker.chip_select_released();

    RamBlockBackend output_backend(16);
    SdCardModel output_model(SdCardType::Sdsc, output_backend);
    SdSpiCardEngine output_engine(output_model);
    SdSpiTargetWorker<8, 4> output_worker(output_engine);
    capture_command(output_worker, {0x40U, 0, 0, 0, 0, 0x95U});
    assert(pop_one(output_worker) == 0x01U);
    capture_command(output_worker, {0x48U, 0, 0, 1, 0xaaU, 0x87U});
    assert(output_worker.pending_transmit_bytes() == 0U);
    assert(output_worker.counters().transmit_overflows == 1U);
    std::uint8_t unavailable = 0;
    assert(!output_worker.dequeue_transmit_byte(unavailable));
    assert(output_worker.counters().transmit_underruns == 1U);

    assert(output_worker.capture_byte(0x40U));
    output_worker.transaction_timed_out();
    assert(output_worker.pending_receive_bytes() == 0U);
    assert(output_worker.pending_transmit_bytes() == 0U);
    assert(output_worker.counters().timeouts == 1U);

    DeferredBackend deferred_backend;
    SdCardModel deferred_model(SdCardType::Sdhc, deferred_backend);
    SdSpiCardEngine deferred_engine(deferred_model);
    SdSpiTargetWorker<600, 600> deferred_worker(deferred_engine);
    capture_command(deferred_worker, {0x40U, 0, 0, 0, 0, 0x95U});
    assert(pop_one(deferred_worker) == 0x01U);
    capture_command(deferred_worker, {0x77U, 0, 0, 0, 0, 0x65U});
    assert(pop_one(deferred_worker) == 0x01U);
    capture_command(deferred_worker, {0x69U, 0x40U, 0, 0, 0, 0x77U});
    assert(pop_one(deferred_worker) == 0x00U);

    capture_command(deferred_worker, {0x51U, 0, 0, 0, 3, 0x01U});
    assert(pop_one(deferred_worker) == 0x00U);
    deferred_worker.process(0);
    assert(deferred_worker.pending_transmit_bytes() == 0U);
    deferred_backend.read_ready = true;
    deferred_worker.process(0);
    assert(pop_one(deferred_worker) == kSdStartBlockToken);
    assert(pop_one(deferred_worker) == 0x6bU);
    deferred_worker.chip_select_released();

    capture_command(deferred_worker, {0x58U, 0, 0, 0, 4, 0x01U});
    assert(pop_one(deferred_worker) == 0x00U);
    SdBlock deferred_write{};
    deferred_write.fill(0x42U);
    assert(deferred_worker.capture_byte(kSdStartBlockToken));
    for (const std::uint8_t byte : deferred_write)
        assert(deferred_worker.capture_byte(byte));
    const std::uint16_t deferred_crc = crc16(deferred_write.data(), deferred_write.size());
    assert(deferred_worker.capture_byte(static_cast<std::uint8_t>(deferred_crc >> 8U)));
    assert(deferred_worker.capture_byte(static_cast<std::uint8_t>(deferred_crc)));
    deferred_worker.process(kSdDataBlockWireSize);
    assert(deferred_worker.pending_transmit_bytes() == 0U);
    deferred_backend.write_ready = true;
    deferred_worker.process(0);
    assert(pop_one(deferred_worker) == kSdDataResponseAccepted);

    return 0;
}
