#include "picosd/protocol/sd_card_state.hpp"

namespace picosd::protocol {

SdCardState SdCardStateMachine::state() const { return state_; }

SdCardStateError SdCardStateMachine::finish_power_up() {
    if (state_ != SdCardState::PowerUp) return SdCardStateError::InvalidTransition;
    state_ = SdCardState::Idle;
    return SdCardStateError::None;
}

SdCardStateError SdCardStateMachine::reset() {
    if (state_ == SdCardState::PowerUp || state_ == SdCardState::Fault) return SdCardStateError::InvalidTransition;
    state_ = SdCardState::Idle;
    return SdCardStateError::None;
}

SdCardStateError SdCardStateMachine::enter_idle() {
    state_ = SdCardState::Idle;
    return SdCardStateError::None;
}

SdCardStateError SdCardStateMachine::make_ready() {
    if (state_ != SdCardState::Idle) return SdCardStateError::InvalidTransition;
    state_ = SdCardState::Ready;
    return SdCardStateError::None;
}

SdCardStateError SdCardStateMachine::begin_transfer() {
    if (state_ != SdCardState::Ready && state_ != SdCardState::Transfer) return SdCardStateError::InvalidTransition;
    state_ = SdCardState::Transfer;
    return SdCardStateError::None;
}

SdCardStateError SdCardStateMachine::begin_receiving_data() {
    if (state_ != SdCardState::Transfer) return SdCardStateError::InvalidTransition;
    state_ = SdCardState::ReceivingData;
    return SdCardStateError::None;
}

SdCardStateError SdCardStateMachine::begin_busy() {
    if (state_ != SdCardState::ReceivingData) return SdCardStateError::InvalidTransition;
    state_ = SdCardState::Busy;
    return SdCardStateError::None;
}

SdCardStateError SdCardStateMachine::finish_busy() {
    if (state_ != SdCardState::Busy) return SdCardStateError::InvalidTransition;
    state_ = SdCardState::Transfer;
    return SdCardStateError::None;
}

SdCardStateError SdCardStateMachine::finish_receiving_data() {
    if (state_ != SdCardState::ReceivingData) return SdCardStateError::InvalidTransition;
    state_ = SdCardState::Transfer;
    return SdCardStateError::None;
}

void SdCardStateMachine::fault() { state_ = SdCardState::Fault; }

RamBlockBackend::RamBlockBackend(std::size_t block_count)
    : block_count_(block_count <= kRamBackendMaximumBlocks ? block_count : 0) {}

std::size_t RamBlockBackend::block_count() const { return block_count_; }

bool RamBlockBackend::read(std::size_t lba, SdBlock& output) const {
    if (lba >= block_count_) return false;
    output = blocks_[lba];
    return true;
}

bool RamBlockBackend::write(std::size_t lba, const SdBlock& input) {
    if (lba >= block_count_) return false;
    blocks_[lba] = input;
    return true;
}

void RamBlockBackend::fill_diagnostic_pattern() {
    for (std::size_t lba = 0; lba < block_count_; ++lba) {
        for (std::size_t byte = 0; byte < kSdBlockSize; ++byte) {
            blocks_[lba][byte] = static_cast<std::uint8_t>((lba * 37U + byte) & 0xffU);
        }
    }
}

}  // namespace picosd::protocol
