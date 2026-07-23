#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace picosd::protocol {

inline constexpr std::size_t kSdCommandFrameSize = 6;
inline constexpr std::size_t kSdMaximumResponseSize = 5;
inline constexpr std::uint8_t kSdStartBlockToken = 0xfeU;
inline constexpr std::uint8_t kSdDataResponseAccepted = 0x05U;
inline constexpr std::uint8_t kSdDataResponseCrcError = 0x0bU;
inline constexpr std::uint8_t kSdDataResponseWriteError = 0x0dU;
inline constexpr std::size_t kSdDataBlockWireSize = 515;

enum class SdCommandError {
    None,
    InvalidStartBits,
    InvalidEndBit,
    CrcMismatch,
};

enum class SdCrcPolicy {
    Required,
    Ignored,
};

struct SdCommand {
    std::uint8_t index = 0;
    std::uint32_t argument = 0;
    std::uint8_t crc_byte = 0;
};

struct SdCommandDecodeResult {
    SdCommandError error = SdCommandError::None;
    SdCommand command{};
};

// Decodes one complete six-byte SD SPI command frame. The caller selects
// whether command CRC7 must match for the current card state.
SdCommandDecodeResult decode_sd_command(
    const std::array<std::uint8_t, kSdCommandFrameSize>& frame,
    SdCrcPolicy crc_policy);

enum class SdResponseType {
    R1,
    R2,
    R3,
    R7,
};

enum class SdR1 : std::uint8_t {
    Idle = 0x01,
    EraseReset = 0x02,
    IllegalCommand = 0x04,
    ComCrcError = 0x08,
    EraseSequenceError = 0x10,
    AddressError = 0x20,
    ParameterError = 0x40,
};

struct SdResponse {
    std::array<std::uint8_t, kSdMaximumResponseSize> bytes{};
    std::size_t size = 0;
    SdResponseType type = SdResponseType::R1;
};

SdResponse make_r1(std::uint8_t status);
SdResponse make_r2(std::uint8_t status, std::uint8_t card_status);
SdResponse make_r3(std::uint8_t status, std::uint32_t ocr);
SdResponse make_r7(std::uint8_t status, std::uint32_t echo);

struct SdDataBlock {
    std::array<std::uint8_t, kSdDataBlockWireSize> bytes{};
};

SdDataBlock make_read_data_block(const std::array<std::uint8_t, 512>& payload);
bool verify_data_block_crc(const std::array<std::uint8_t, 512>& payload, std::uint16_t received_crc);
std::uint8_t make_data_response(std::uint8_t status);

}  // namespace picosd::protocol
