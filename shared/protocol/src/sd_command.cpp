#include "picosd/protocol/sd_command.hpp"

#include "picosd/protocol/crc.hpp"

namespace picosd::protocol {
namespace {

void append_u32_big_endian(SdResponse& response, std::uint32_t value) {
    response.bytes[1] = static_cast<std::uint8_t>(value >> 24U);
    response.bytes[2] = static_cast<std::uint8_t>(value >> 16U);
    response.bytes[3] = static_cast<std::uint8_t>(value >> 8U);
    response.bytes[4] = static_cast<std::uint8_t>(value);
}

}  // namespace

SdCommandDecodeResult decode_sd_command(
    const std::array<std::uint8_t, kSdCommandFrameSize>& frame,
    SdCrcPolicy crc_policy) {
    SdCommandDecodeResult result;

    if ((frame[0] & 0xc0U) != 0x40U) {
        result.error = SdCommandError::InvalidStartBits;
        return result;
    }
    if ((frame[5] & 0x01U) == 0U) {
        result.error = SdCommandError::InvalidEndBit;
        return result;
    }
    if (crc_policy == SdCrcPolicy::Required &&
        crc7_command_byte(frame.data(), kSdCommandFrameSize - 1) != frame[5]) {
        result.error = SdCommandError::CrcMismatch;
        return result;
    }

    result.command.index = static_cast<std::uint8_t>(frame[0] & 0x3fU);
    result.command.argument = (static_cast<std::uint32_t>(frame[1]) << 24U) |
                              (static_cast<std::uint32_t>(frame[2]) << 16U) |
                              (static_cast<std::uint32_t>(frame[3]) << 8U) |
                              static_cast<std::uint32_t>(frame[4]);
    result.command.crc_byte = frame[5];
    return result;
}

SdResponse make_r1(std::uint8_t status) {
    SdResponse response;
    response.bytes[0] = status;
    response.size = 1;
    response.type = SdResponseType::R1;
    return response;
}

SdResponse make_r2(std::uint8_t status, std::uint8_t card_status) {
    SdResponse response;
    response.bytes[0] = status;
    response.bytes[1] = card_status;
    response.size = 2;
    response.type = SdResponseType::R2;
    return response;
}

SdResponse make_r3(std::uint8_t status, std::uint32_t ocr) {
    SdResponse response;
    response.bytes[0] = status;
    append_u32_big_endian(response, ocr);
    response.size = 5;
    response.type = SdResponseType::R3;
    return response;
}

SdResponse make_r7(std::uint8_t status, std::uint32_t echo) {
    SdResponse response;
    response.bytes[0] = status;
    append_u32_big_endian(response, echo);
    response.size = 5;
    response.type = SdResponseType::R7;
    return response;
}

SdDataBlock make_read_data_block(const std::array<std::uint8_t, 512>& payload) {
    SdDataBlock block;
    block.bytes[0] = kSdStartBlockToken;
    for (std::size_t index = 0; index < payload.size(); ++index) block.bytes[index + 1] = payload[index];
    const std::uint16_t checksum = crc16(payload.data(), payload.size());
    block.bytes[513] = static_cast<std::uint8_t>(checksum >> 8U);
    block.bytes[514] = static_cast<std::uint8_t>(checksum);
    return block;
}

bool verify_data_block_crc(const std::array<std::uint8_t, 512>& payload, std::uint16_t received_crc) {
    return crc16(payload.data(), payload.size()) == received_crc;
}

}  // namespace picosd::protocol
