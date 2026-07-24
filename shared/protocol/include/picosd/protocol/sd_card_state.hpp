#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace picosd::protocol {

inline constexpr std::size_t kSdBlockSize = 512;
inline constexpr std::size_t kRamBackendMaximumBlocks = 64;

using SdBlock = std::array<std::uint8_t, kSdBlockSize>;

enum class SdCardState {
    PowerUp,
    Idle,
    Ready,
    Transfer,
    ReceivingData,
    Busy,
    Fault,
};

enum class SdCardStateError {
    None,
    InvalidTransition,
};

// State-only model for the SD SPI card lifecycle. Command handlers will use
// this model to keep transitions explicit and independently testable.
class SdCardStateMachine {
public:
    [[nodiscard]] SdCardState state() const;
    SdCardStateError finish_power_up();
    SdCardStateError reset();
    SdCardStateError enter_idle();
    SdCardStateError make_ready();
    SdCardStateError begin_transfer();
    SdCardStateError begin_receiving_data();
    SdCardStateError begin_busy();
    SdCardStateError finish_busy();
    SdCardStateError finish_receiving_data();
    void fault();

private:
    SdCardState state_ = SdCardState::PowerUp;
};

// Deterministic, fixed-capacity RAM block store for native SD-model tests and
// early firmware bring-up. It never dynamically allocates and is intentionally
// limited to a small number of sectors.
class RamBlockBackend {
public:
    explicit RamBlockBackend(std::size_t block_count);

    [[nodiscard]] std::size_t block_count() const;
    [[nodiscard]] bool read(std::size_t lba, SdBlock& output) const;
    [[nodiscard]] bool write(std::size_t lba, const SdBlock& input);
    void fill_diagnostic_pattern();

private:
    std::array<SdBlock, kRamBackendMaximumBlocks> blocks_{};
    std::size_t block_count_ = 0;
};

}  // namespace picosd::protocol
