#include "picosd/protocol/cdc_media_info.hpp"
#include <charconv>
#include "picosd/protocol/text_line.hpp"
namespace picosd::protocol {
namespace {
bool parse_u64(std::string_view value, std::uint64_t &output) {
    if (value.empty()) return false;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), output);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size();
}
} // namespace
CdcMediaInfoError decode_cdc_media_info(std::string_view input, CdcMediaInfo &output) {
    TextLine line;
    if (parse_text_line(input, line) != TextLineError::None || line.command() != "OK") return CdcMediaInfoError::InvalidLine;
    const auto present = line.value_for("present");
    const auto readonly = line.value_for("readonly");
    const auto block_size = line.value_for("block_size");
    const auto blocks = line.value_for("blocks");
    if (present.empty() || readonly.empty() || block_size.empty() || blocks.empty()) return CdcMediaInfoError::MissingField;
    if ((present != "0" && present != "1") || (readonly != "0" && readonly != "1")) return CdcMediaInfoError::InvalidField;
    std::uint64_t decoded_block_size = 0, decoded_blocks = 0;
    if (!parse_u64(block_size, decoded_block_size) || !parse_u64(blocks, decoded_blocks)) return CdcMediaInfoError::InvalidField;
    if (decoded_block_size != 512) return CdcMediaInfoError::UnsupportedBlockSize;
    if (present == "1" && decoded_blocks == 0) return CdcMediaInfoError::InvalidField;
    std::uint64_t generation = 0;
    const auto generation_field = line.value_for("generation");
    if (!generation_field.empty() && !parse_u64(generation_field, generation)) return CdcMediaInfoError::InvalidField;
    output = {present == "1", readonly == "1", present == "1" ? decoded_blocks : 0, generation};
    return CdcMediaInfoError::None;
}
} // namespace picosd::protocol
