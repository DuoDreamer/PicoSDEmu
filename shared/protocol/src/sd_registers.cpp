#include "picosd/protocol/sd_registers.hpp"

namespace picosd::protocol {
namespace {
void set_bits(std::array<std::uint8_t, 16>& bytes, int high, int low, std::uint32_t value) {
    for (int bit = low; bit <= high; ++bit) {
        const std::uint32_t source_bit = (value >> (bit - low)) & 1U;
        const int byte = 15 - bit / 8;
        const int byte_bit = bit % 8;
        bytes[byte] = static_cast<std::uint8_t>(bytes[byte] | (source_bit << byte_bit));
    }
}
}  // namespace

SdCardRegisters make_sd_registers(SdCardType type, std::uint32_t requested_blocks) {
    SdCardRegisters result;
    result.ocr = {{0x00U, 0xffU, 0x80U, 0x00U}};
    if (type == SdCardType::Sdhc) result.ocr[0] = 0x40U;
    result.cid = {{0x50U, 0x53U, 0x44U, 0x45U, 0x4dU, 0x55U, 0x30U, 0x30U,
                   0x01U, 0x00U, 0x00U, 0x00U, 0x01U, 0x26U, 0x01U, 0x00U}};

    if (type == SdCardType::Sdhc) {
        const std::uint32_t groups = requested_blocks / 1024U;
        if (groups != 0U) {
            const std::uint32_t c_size = groups - 1U;
            if (c_size <= 0x3fffffU) {
                result.exposed_blocks = groups * 1024U;
                set_bits(result.csd, 127, 126, 1U);
                set_bits(result.csd, 83, 80, 9U);
                set_bits(result.csd, 69, 48, c_size);
            }
        }
    } else {
        for (std::uint32_t multiplier = 9U; multiplier >= 2U; --multiplier) {
            const std::uint32_t unit_blocks = 1U << (multiplier + 2U);
            const std::uint32_t c_size_plus_one = requested_blocks / unit_blocks;
            if (c_size_plus_one != 0U && c_size_plus_one <= 4096U) {
                result.exposed_blocks = c_size_plus_one * unit_blocks;
                set_bits(result.csd, 127, 126, 0U);
                set_bits(result.csd, 83, 80, 9U);
                set_bits(result.csd, 73, 62, c_size_plus_one - 1U);
                set_bits(result.csd, 49, 47, multiplier);
                break;
            }
        }
    }
    return result;
}
}  // namespace picosd::protocol
