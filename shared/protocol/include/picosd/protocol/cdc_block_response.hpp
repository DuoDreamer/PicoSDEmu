#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace picosd::protocol {

inline constexpr std::size_t kCdcBlockSize = 512;
using CdcBlockData = std::array<std::uint8_t, kCdcBlockSize>;

enum class CdcBlockResponseError {
    None,
    InvalidLine,
    ErrorResponse,
    MissingField,
    InvalidValue,
    InvalidData,
    BadCrc,
};

enum class CdcCardType { Sdsc, Sdhc };

struct CdcCardInfo {
    bool present = false;
    CdcCardType type = CdcCardType::Sdsc;
    std::uint64_t blocks = 0;
    bool readonly = false;
};

struct CdcReadBlock {
    std::uint64_t lba = 0;
    CdcBlockData data{};
};

CdcBlockResponseError decode_get_info_response(std::string_view response,
                                               CdcCardInfo& output);
CdcBlockResponseError decode_read_block_response(std::string_view response,
                                                 CdcReadBlock& output);

}  // namespace picosd::protocol
