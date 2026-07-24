#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "picosd/protocol/sd_command.hpp"

namespace picosd::protocol {

// Accumulates complete SD SPI command frames from a byte stream. Bytes clocked
// while the card is not receiving a command are ignored unless they contain
// the required `01` command start bits. Call reset() whenever client CS rises
// so an aborted command cannot be combined with the next transaction.
class SdSpiCommandFramer {
public:
    [[nodiscard]] std::optional<SdCommandDecodeResult> push_byte(
        std::uint8_t byte,
        SdCrcPolicy crc_policy);
    void reset();
    [[nodiscard]] bool receiving() const;

private:
    std::array<std::uint8_t, kSdCommandFrameSize> frame_{};
    std::size_t count_ = 0;
};

}  // namespace picosd::protocol
