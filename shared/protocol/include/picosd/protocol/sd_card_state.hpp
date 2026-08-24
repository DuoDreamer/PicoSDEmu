#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace picosd::protocol {

inline constexpr std::size_t kSdBlockSize = 512;
inline constexpr std::size_t kRamBackendMaximumBlocks = 64;

using SdBlock = std::array<std::uint8_t, kSdBlockSize>;

enum class BlockOperationResult {
    Complete,
    Pending,
    Failed,
};

// Storage boundary used by the SD command model. Pending lets a transport
// start work without blocking the SPI worker; callers retry the same operation
// until it completes or fails.
class BlockBackend {
  public:
    virtual ~BlockBackend() = default;
    [[nodiscard]] virtual std::size_t block_count() const = 0;
    [[nodiscard]] virtual bool media_present() const {
        return block_count() != 0;
    }
    [[nodiscard]] virtual bool write_protected() const {
        return false;
    }
    [[nodiscard]] virtual BlockOperationResult flush() {
        return media_present() ? BlockOperationResult::Complete : BlockOperationResult::Failed;
    }
    [[nodiscard]] virtual BlockOperationResult read(std::size_t lba, SdBlock &output) const = 0;
    [[nodiscard]] virtual BlockOperationResult write(std::size_t lba, const SdBlock &input) = 0;
};

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
class RamBlockBackend final : public BlockBackend {
  public:
    explicit RamBlockBackend(std::size_t block_count);

    [[nodiscard]] std::size_t block_count() const override;
    [[nodiscard]] BlockOperationResult read(std::size_t lba, SdBlock &output) const override;
    [[nodiscard]] BlockOperationResult write(std::size_t lba, const SdBlock &input) override;
    void fill_diagnostic_pattern();

  private:
    std::array<SdBlock, kRamBackendMaximumBlocks> blocks_{};
    std::size_t block_count_ = 0;
};

} // namespace picosd::protocol
