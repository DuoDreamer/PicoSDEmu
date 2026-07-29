#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace picosd::protocol {

enum class SdSpiWriteMode {
    SingleBlock,
    MultiBlock,
};

enum class SdSpiDataEventType {
    Block,
    Stop,
};

struct SdSpiDataEvent {
    SdSpiDataEventType type = SdSpiDataEventType::Block;
    std::array<std::uint8_t, 512> payload{};
    std::uint16_t crc = 0;
};

// Accumulates an SD SPI write data token, its 512-byte payload, and its CRC16.
// Call begin() after the card model accepts CMD24 or CMD25, then reset() on CS
// release or a command abort. CRC validation belongs to the card model because
// it owns the data-response and state-transition policy.
class SdSpiDataFramer {
public:
    void begin(SdSpiWriteMode mode);
    [[nodiscard]] std::optional<SdSpiDataEvent> push_byte(std::uint8_t byte);
    void reset();
    [[nodiscard]] bool receiving() const;

private:
    enum class State {
        Idle,
        WaitingForToken,
        Payload,
        CrcHigh,
        CrcLow,
    };

    State state_ = State::Idle;
    SdSpiWriteMode mode_ = SdSpiWriteMode::SingleBlock;
    std::array<std::uint8_t, 512> payload{};
    std::size_t payload_size_ = 0;
    std::uint16_t crc_ = 0;
};

}  // namespace picosd::protocol
