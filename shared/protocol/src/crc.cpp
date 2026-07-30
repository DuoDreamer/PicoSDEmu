#include "picosd/protocol/crc.hpp"

namespace picosd::protocol {

std::uint8_t crc7(const std::uint8_t* data, std::size_t size) {
    std::uint8_t value = 0;

    for (std::size_t index = 0; index < size; ++index) {
        const std::uint8_t byte = data[index];
        for (int bit_index = 7; bit_index >= 0; --bit_index) {
            const std::uint8_t input_bit = static_cast<std::uint8_t>((byte >> bit_index) & 1U);
            const std::uint8_t feedback = static_cast<std::uint8_t>(((value >> 6U) & 1U) ^ input_bit);

            value = static_cast<std::uint8_t>((value << 1U) & 0x7fU);
            if (feedback != 0U) {
                value = static_cast<std::uint8_t>(value ^ 0x09U);
            }
        }
    }

    return value;
}

std::uint8_t crc7_command_byte(const std::uint8_t* data, std::size_t size) {
    return static_cast<std::uint8_t>((crc7(data, size) << 1U) | 1U);
}

std::uint16_t crc16(const std::uint8_t* data, std::size_t size) {
    std::uint16_t value = 0;

    for (std::size_t index = 0; index < size; ++index) {
        value = static_cast<std::uint16_t>(value ^ (static_cast<std::uint16_t>(data[index]) << 8U));
        for (int bit_index = 0; bit_index < 8; ++bit_index) {
            const bool top_bit_set = (value & 0x8000U) != 0U;
            value = static_cast<std::uint16_t>(value << 1U);
            if (top_bit_set) {
                value = static_cast<std::uint16_t>(value ^ 0x1021U);
            }
        }
    }

    return value;
}

std::uint32_t crc32(const std::uint8_t* data, std::size_t size) {
    std::uint32_t value = 0xffffffffU;

    for (std::size_t index = 0; index < size; ++index) {
        value ^= data[index];
        for (int bit_index = 0; bit_index < 8; ++bit_index) {
            const bool low_bit_set = (value & 1U) != 0U;
            value >>= 1U;
            if (low_bit_set) {
                value ^= 0xedb88320U;
            }
        }
    }

    return value ^ 0xffffffffU;
}

}  // namespace picosd::protocol
