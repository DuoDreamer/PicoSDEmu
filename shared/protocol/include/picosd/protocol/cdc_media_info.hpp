#pragma once
#include <cstdint>
#include <string_view>
namespace picosd::protocol {
enum class CdcMediaInfoError { None, InvalidLine, MissingField, InvalidField, UnsupportedBlockSize };
struct CdcMediaInfo { bool present = false; bool read_only = true; std::uint64_t blocks = 0; std::uint64_t generation = 0; };
// Correlation and session validation remain the responsibility of CdcSessionClient.
[[nodiscard]] CdcMediaInfoError decode_cdc_media_info(std::string_view line, CdcMediaInfo &output);
} // namespace picosd::protocol
