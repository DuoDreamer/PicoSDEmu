#include <iostream>

#include "picosd/protocol/sd_card_state.hpp"

namespace {
int failures = 0;
void expect(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
}  // namespace

int main() {
    using namespace picosd::protocol;
    SdCardStateMachine card;
    expect(card.state() == SdCardState::PowerUp, "initial state is PowerUp");
    expect(card.make_ready() == SdCardStateError::InvalidTransition, "cannot ready during PowerUp");
    expect(card.finish_power_up() == SdCardStateError::None, "power-up completes");
    expect(card.make_ready() == SdCardStateError::None, "idle becomes ready");
    expect(card.begin_transfer() == SdCardStateError::None, "ready begins transfer");
    expect(card.begin_receiving_data() == SdCardStateError::None, "transfer receives data");
    expect(card.begin_busy() == SdCardStateError::None, "receive enters busy");
    expect(card.finish_busy() == SdCardStateError::None && card.state() == SdCardState::Transfer,
           "busy returns to transfer");
    expect(card.reset() == SdCardStateError::None && card.state() == SdCardState::Idle, "reset returns idle");
    card.fault();
    expect(card.reset() == SdCardStateError::InvalidTransition, "fault requires explicit recovery");

    RamBlockBackend backend{2};
    SdBlock block{};
    expect(!backend.read(2, block), "out-of-range read is rejected");
    backend.fill_diagnostic_pattern();
    expect(backend.read(1, block), "in-range read succeeds");
    expect(block[0] == 37U && block[511] == 36U, "diagnostic pattern is deterministic");
    block[0] = 0xa5U;
    expect(backend.write(1, block), "in-range write succeeds");
    SdBlock readback{};
    expect(backend.read(1, readback) && readback[0] == 0xa5U, "written block reads back");
    expect(!backend.write(2, block), "out-of-range write is rejected");
    expect(RamBlockBackend{kRamBackendMaximumBlocks + 1}.block_count() == 0,
           "oversized backend is unavailable");

    if (failures != 0) return 1;
    std::cout << "All SD card state tests passed.\n";
    return 0;
}
