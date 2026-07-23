#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iostream>

#include "picosd/protocol/sd_command.hpp"

namespace {

int failures = 0;

void expect(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

void expect_response(const picosd::protocol::SdResponse& response,
                     picosd::protocol::SdResponseType expected_type,
                     std::initializer_list<std::uint8_t> expected_bytes,
                     const char* description) {
    expect(response.type == expected_type, description);
    expect(response.size == expected_bytes.size(), description);

    std::size_t index = 0;
    for (const std::uint8_t expected_byte : expected_bytes) {
        if (index < response.size) {
            expect(response.bytes[index] == expected_byte, description);
        }
        ++index;
    }
}

}  // namespace

int main() {
    constexpr std::array<std::uint8_t, 6> cmd0{{0x40U, 0x00U, 0x00U, 0x00U, 0x00U, 0x95U}};
    constexpr std::array<std::uint8_t, 6> cmd8{{0x48U, 0x00U, 0x00U, 0x01U, 0xaaU, 0x87U}};

    const auto decoded_cmd0 = picosd::protocol::decode_sd_command(
        cmd0, picosd::protocol::SdCrcPolicy::Required);
    expect(decoded_cmd0.error == picosd::protocol::SdCommandError::None,
           "CMD0 decodes with required CRC");
    expect(decoded_cmd0.command.index == 0, "CMD0 index is zero");
    expect(decoded_cmd0.command.argument == 0U, "CMD0 argument is zero");
    expect(decoded_cmd0.command.crc_byte == 0x95U, "CMD0 CRC byte is retained");

    const auto decoded_cmd8 = picosd::protocol::decode_sd_command(
        cmd8, picosd::protocol::SdCrcPolicy::Required);
    expect(decoded_cmd8.error == picosd::protocol::SdCommandError::None,
           "CMD8 decodes with required CRC");
    expect(decoded_cmd8.command.index == 8, "CMD8 index is eight");
    expect(decoded_cmd8.command.argument == 0x000001aaU, "CMD8 argument is big-endian");

    auto bad_start = cmd0;
    bad_start[0] = 0x00U;
    expect(picosd::protocol::decode_sd_command(bad_start, picosd::protocol::SdCrcPolicy::Required).error ==
               picosd::protocol::SdCommandError::InvalidStartBits,
           "invalid start bits are rejected");

    auto bad_end = cmd0;
    bad_end[5] = 0x94U;
    expect(picosd::protocol::decode_sd_command(bad_end, picosd::protocol::SdCrcPolicy::Required).error ==
               picosd::protocol::SdCommandError::InvalidEndBit,
           "invalid end bit is rejected");

    auto bad_crc = cmd8;
    bad_crc[5] = 0x85U;
    expect(picosd::protocol::decode_sd_command(bad_crc, picosd::protocol::SdCrcPolicy::Required).error ==
               picosd::protocol::SdCommandError::CrcMismatch,
           "incorrect required CRC is rejected");
    expect(picosd::protocol::decode_sd_command(bad_crc, picosd::protocol::SdCrcPolicy::Ignored).error ==
               picosd::protocol::SdCommandError::None,
           "incorrect optional CRC is accepted");

    expect_response(picosd::protocol::make_r1(0x01U), picosd::protocol::SdResponseType::R1, {0x01U},
                    "R1 construction");
    expect_response(picosd::protocol::make_r2(0x00U, 0x80U), picosd::protocol::SdResponseType::R2,
                    {0x00U, 0x80U}, "R2 construction");
    expect_response(picosd::protocol::make_r3(0x00U, 0x40ff8000U), picosd::protocol::SdResponseType::R3,
                    {0x00U, 0x40U, 0xffU, 0x80U, 0x00U}, "R3 construction");
    expect_response(picosd::protocol::make_r7(0x01U, 0x000001aaU), picosd::protocol::SdResponseType::R7,
                    {0x01U, 0x00U, 0x00U, 0x01U, 0xaaU}, "R7 construction");

    std::array<std::uint8_t, 512> payload{};
    payload[0] = 0x12U;
    payload[511] = 0x34U;
    const auto data_block = picosd::protocol::make_read_data_block(payload);
    expect(data_block.bytes[0] == picosd::protocol::kSdStartBlockToken, "read data token");
    expect(data_block.bytes[1] == 0x12U && data_block.bytes[512] == 0x34U, "read data payload");
    const auto crc = static_cast<std::uint16_t>((data_block.bytes[513] << 8U) | data_block.bytes[514]);
    expect(picosd::protocol::verify_data_block_crc(payload, crc), "read data CRC");
    expect(!picosd::protocol::verify_data_block_crc(payload, static_cast<std::uint16_t>(crc ^ 1U)), "bad data CRC");

    if (failures != 0) {
        return 1;
    }

    std::cout << "All SD command tests passed.\n";
    return 0;
}
