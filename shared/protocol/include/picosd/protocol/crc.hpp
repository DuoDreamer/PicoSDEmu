#pragma once

#include <cstddef>
#include <cstdint>

namespace picosd::protocol {

// SD command CRC7 uses x^7 + x^3 + 1 and is transmitted as the upper seven
// bits of a command CRC byte. Bit zero of that byte is always one.
std::uint8_t crc7(const std::uint8_t* data, std::size_t size);
std::uint8_t crc7_command_byte(const std::uint8_t* data, std::size_t size);

// SD data CRC16 uses x^16 + x^12 + x^5 + 1 with an all-zero initial value.
std::uint16_t crc16(const std::uint8_t* data, std::size_t size);

// USB CDC text-protocol payloads use the standard reflected CRC-32 polynomial
// with an all-one initial value and a final all-one xor value.
std::uint32_t crc32(const std::uint8_t* data, std::size_t size);

}  // namespace picosd::protocol
