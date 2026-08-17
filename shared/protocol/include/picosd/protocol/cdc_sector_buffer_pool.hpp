#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "picosd/protocol/cdc_block_response.hpp"

namespace picosd::protocol {

enum class CdcSectorBufferState {
    Free,
    Reading,
    Ready,
    Writing,
};

// Fixed storage used at the firmware/USB boundary. In-flight slots cannot be
// evicted, so a delayed USB response cannot overwrite another operation. Ready
// slots form a small LRU cache and remain reusable until invalidated or replaced.
template <std::size_t Capacity> class CdcSectorBufferPool {
    static_assert(Capacity != 0, "a sector buffer pool must contain at least one slot");

  public:
    static constexpr std::size_t kInvalidHandle = std::numeric_limits<std::size_t>::max();

    // The metadata contains 64-bit values, so its natural alignment is also
    // sufficient for the RP2350 DMA engine's 32-bit sector transfers.
    struct alignas(std::uint64_t) Slot {
        CdcBlockData data{};
        std::uint64_t lba = 0;
        std::uint64_t generation = 0;
        std::uint64_t last_used = 0;
        CdcSectorBufferState state = CdcSectorBufferState::Free;
    };

    [[nodiscard]] std::size_t reserve_read(std::uint64_t lba, std::uint64_t generation) {
        return reserve(lba, generation, CdcSectorBufferState::Reading);
    }

    [[nodiscard]] std::size_t reserve_write(std::uint64_t lba, std::uint64_t generation,
                                            const CdcBlockData &data) {
        const auto handle = reserve(lba, generation, CdcSectorBufferState::Writing);
        if (handle != kInvalidHandle)
            slots_[handle].data = data;
        return handle;
    }

    [[nodiscard]] bool complete_read(std::size_t handle, std::uint64_t lba,
                                     std::uint64_t generation, const CdcBlockData &data) {
        Slot *slot = checked(handle, CdcSectorBufferState::Reading, lba, generation);
        if (slot == nullptr)
            return false;
        slot->data = data;
        slot->state = CdcSectorBufferState::Ready;
        return true;
    }

    [[nodiscard]] bool complete_write(std::size_t handle, std::uint64_t lba,
                                      std::uint64_t generation) {
        Slot *slot = checked(handle, CdcSectorBufferState::Writing, lba, generation);
        if (slot == nullptr)
            return false;
        slot->state = CdcSectorBufferState::Ready;
        return true;
    }

    [[nodiscard]] std::size_t find_ready(std::uint64_t lba, std::uint64_t generation) const {
        for (std::size_t index = 0; index < Capacity; ++index) {
            const auto &slot = slots_[index];
            if (slot.state == CdcSectorBufferState::Ready && slot.lba == lba &&
                slot.generation == generation) {
                return index;
            }
        }
        return kInvalidHandle;
    }

    [[nodiscard]] bool copy_ready(std::uint64_t lba, std::uint64_t generation,
                                  CdcBlockData &output) {
        const auto handle = find_ready(lba, generation);
        if (handle == kInvalidHandle)
            return false;
        output = slots_[handle].data;
        slots_[handle].last_used = next_use_++;
        return true;
    }

    [[nodiscard]] Slot *get(std::size_t handle) {
        return handle < Capacity && slots_[handle].state != CdcSectorBufferState::Free
                   ? &slots_[handle]
                   : nullptr;
    }

    [[nodiscard]] const Slot *get(std::size_t handle) const {
        return handle < Capacity && slots_[handle].state != CdcSectorBufferState::Free
                   ? &slots_[handle]
                   : nullptr;
    }

    [[nodiscard]] bool release(std::size_t handle) {
        if (handle >= Capacity || slots_[handle].state == CdcSectorBufferState::Free) {
            return false;
        }
        slots_[handle] = {};
        return true;
    }

    void release_generation(std::uint64_t generation) {
        for (auto &slot : slots_) {
            if (slot.state != CdcSectorBufferState::Free && slot.generation == generation) {
                slot = {};
            }
        }
    }

    void clear() {
        for (auto &slot : slots_)
            slot = {};
    }

    [[nodiscard]] std::size_t available() const {
        std::size_t count = 0;
        for (const auto &slot : slots_) {
            if (slot.state == CdcSectorBufferState::Free)
                ++count;
        }
        return count;
    }

  private:
    [[nodiscard]] std::size_t reserve(std::uint64_t lba, std::uint64_t generation,
                                      CdcSectorBufferState state) {
        std::size_t candidate = kInvalidHandle;
        for (std::size_t index = 0; index < Capacity; ++index) {
            if (slots_[index].state == CdcSectorBufferState::Free) {
                candidate = index;
                break;
            }
            if (slots_[index].state == CdcSectorBufferState::Ready &&
                (candidate == kInvalidHandle ||
                 slots_[index].last_used < slots_[candidate].last_used))
                candidate = index;
        }
        if (candidate == kInvalidHandle)
            return kInvalidHandle;
        slots_[candidate] = {};
        slots_[candidate].lba = lba;
        slots_[candidate].generation = generation;
        slots_[candidate].last_used = next_use_++;
        slots_[candidate].state = state;
        return candidate;
    }

    [[nodiscard]] Slot *checked(std::size_t handle, CdcSectorBufferState state, std::uint64_t lba,
                                std::uint64_t generation) {
        if (handle >= Capacity)
            return nullptr;
        auto &slot = slots_[handle];
        return slot.state == state && slot.lba == lba && slot.generation == generation ? &slot
                                                                                       : nullptr;
    }

    std::array<Slot, Capacity> slots_{};
    std::uint64_t next_use_ = 1;
};

} // namespace picosd::protocol
