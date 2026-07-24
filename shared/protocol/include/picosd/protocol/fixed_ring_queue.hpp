#pragma once

#include <array>
#include <cstddef>
#include <utility>

namespace picosd::protocol {

// Fixed-capacity FIFO for firmware handoff paths. It never allocates and makes
// overflow explicit to its caller, which is required for PIO/DMA diagnostics.
template <typename T, std::size_t Capacity>
class FixedRingQueue {
    static_assert(Capacity > 0, "FixedRingQueue capacity must be positive");

public:
    [[nodiscard]] static constexpr std::size_t capacity() { return Capacity; }
    [[nodiscard]] bool empty() const { return size_ == 0; }
    [[nodiscard]] bool full() const { return size_ == Capacity; }
    [[nodiscard]] std::size_t size() const { return size_; }

    bool try_push(const T& value) {
        if (full()) return false;
        values_[tail_] = value;
        advance(tail_);
        ++size_;
        return true;
    }

    bool try_push(T&& value) {
        if (full()) return false;
        values_[tail_] = std::move(value);
        advance(tail_);
        ++size_;
        return true;
    }

    bool try_pop(T& output) {
        if (empty()) return false;
        output = std::move(values_[head_]);
        advance(head_);
        --size_;
        return true;
    }

    void clear() {
        head_ = 0;
        tail_ = 0;
        size_ = 0;
    }

private:
    static void advance(std::size_t& index) {
        ++index;
        if (index == Capacity) index = 0;
    }

    std::array<T, Capacity> values_{};
    std::size_t head_ = 0;
    std::size_t tail_ = 0;
    std::size_t size_ = 0;
};

}  // namespace picosd::protocol
