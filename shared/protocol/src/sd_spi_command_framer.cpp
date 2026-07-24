#include "picosd/protocol/sd_spi_command_framer.hpp"

namespace picosd::protocol {

std::optional<SdCommandDecodeResult> SdSpiCommandFramer::push_byte(
    std::uint8_t byte,
    SdCrcPolicy crc_policy) {
    if (count_ == 0) {
        // SD SPI command frames begin with a zero start bit and one
        // transmission bit. The command index occupies the remaining six
        // bits, so 0x40 through 0x7f are the only valid first bytes.
        if ((byte & 0xc0U) != 0x40U) return std::nullopt;
    }

    frame_[count_++] = byte;
    if (count_ != frame_.size()) return std::nullopt;

    const SdCommandDecodeResult result = decode_sd_command(frame_, crc_policy);
    reset();
    return result;
}

void SdSpiCommandFramer::reset() { count_ = 0; }

bool SdSpiCommandFramer::receiving() const { return count_ != 0; }

}  // namespace picosd::protocol
