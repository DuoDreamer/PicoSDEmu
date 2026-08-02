#include <array>
#include <cstdio>
#include <iostream>
#include <string>

#include "picosd/protocol/cdc_block_response.hpp"
#include "picosd/protocol/codec.hpp"
#include "picosd/protocol/crc.hpp"

namespace {
int failures = 0;
void expect(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}
}  // namespace

int main() {
    using namespace picosd::protocol;
    CdcCardInfo info;
    expect(decode_get_info_response(
               "OK id=2 session=s present=1 type=SDHC blocks=1024 block_size=512 readonly=1",
               info) == CdcBlockResponseError::None && info.present && info.readonly &&
               info.type == CdcCardType::Sdhc && info.blocks == 1024,
           "decodes card metadata");
    expect(decode_get_info_response(
               "OK id=2 session=s present=1 type=SDHC blocks=1024 readonly=1", info) ==
               CdcBlockResponseError::MissingField,
           "rejects missing block size");
    expect(decode_get_info_response(
               "OK id=2 session=s present=1 type=MMC blocks=1024 block_size=512 readonly=1",
               info) == CdcBlockResponseError::InvalidValue,
           "rejects unsupported card type");
    expect(decode_get_info_response("ERR id=2 code=NO_MEDIA", info) ==
               CdcBlockResponseError::ErrorResponse,
           "recognizes remote error response");

    std::array<std::uint8_t, kCdcBlockSize> block{};
    block[0] = 0x2a;
    block[511] = 0xa5;
    char checksum[9]{};
    std::snprintf(checksum, sizeof(checksum), "%08X", crc32(block.data(), block.size()));
    const auto response = "OK id=3 session=s lba=7 count=1 encoding=BASE64 crc32=" +
                          std::string(checksum) + " data=" +
                          encode_base64(block.data(), block.size());
    CdcReadBlock decoded;
    expect(decode_read_block_response(response, decoded) == CdcBlockResponseError::None &&
               decoded.lba == 7 && decoded.data == block,
           "decodes and verifies block payload");

    auto bad_crc = response;
    const auto crc_position = bad_crc.find("crc32=") + 6;
    bad_crc[crc_position] = bad_crc[crc_position] == '0' ? '1' : '0';
    expect(decode_read_block_response(bad_crc, decoded) == CdcBlockResponseError::BadCrc,
           "rejects block with mismatched CRC");
    expect(decode_read_block_response(
               "OK id=3 session=s lba=7 count=1 encoding=HEX crc32=00000000 data=00",
               decoded) == CdcBlockResponseError::InvalidValue,
           "rejects unsupported block encoding");
    expect(decode_read_block_response(
               "OK id=3 session=s lba=7 count=1 encoding=BASE64 crc32=00000000 data=AAAA",
               decoded) == CdcBlockResponseError::InvalidData,
           "rejects incorrectly sized block payload");
    return failures == 0 ? 0 : 1;
}
