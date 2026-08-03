#include "picosd/protocol/cdc_block_response.hpp"

#include <charconv>

#include "picosd/protocol/codec.hpp"
#include "picosd/protocol/crc.hpp"
#include "picosd/protocol/text_line.hpp"

namespace picosd::protocol {
namespace {

bool decimal(std::string_view text, std::uint64_t& value) {
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return !text.empty() && result.ec == std::errc{} &&
           result.ptr == text.data() + text.size();
}

bool hexadecimal_crc(std::string_view text, std::uint32_t& value) {
    if (text.size() != 8) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value, 16);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

CdcBlockResponseError parse_ok(std::string_view response, TextLine& line) {
    if (parse_text_line(response, line) != TextLineError::None) {
        return CdcBlockResponseError::InvalidLine;
    }
    return line.command() == "OK" ? CdcBlockResponseError::None
                                  : CdcBlockResponseError::ErrorResponse;
}

bool boolean_field(std::string_view value, bool& output) {
    if (value == "0") {
        output = false;
        return true;
    }
    if (value == "1") {
        output = true;
        return true;
    }
    return false;
}

}  // namespace

CdcBlockResponseError decode_get_info_response(std::string_view response,
                                               CdcCardInfo& output) {
    output = {};
    TextLine line;
    const auto parsed = parse_ok(response, line);
    if (parsed != CdcBlockResponseError::None) return parsed;
    const auto present = line.value_for("present");
    const auto type = line.value_for("type");
    const auto blocks = line.value_for("blocks");
    const auto block_size = line.value_for("block_size");
    const auto readonly = line.value_for("readonly");
    if (present.empty() || type.empty() || blocks.empty() || block_size.empty() ||
        readonly.empty()) {
        return CdcBlockResponseError::MissingField;
    }
    std::uint64_t parsed_block_size = 0;
    if (!boolean_field(present, output.present) ||
        !boolean_field(readonly, output.readonly) ||
        !decimal(blocks, output.blocks) || output.blocks == 0 ||
        !decimal(block_size, parsed_block_size) || parsed_block_size != kCdcBlockSize) {
        return CdcBlockResponseError::InvalidValue;
    }
    if (type == "SDSC") output.type = CdcCardType::Sdsc;
    else if (type == "SDHC") output.type = CdcCardType::Sdhc;
    else return CdcBlockResponseError::InvalidValue;
    return CdcBlockResponseError::None;
}

CdcBlockResponseError decode_read_block_response(std::string_view response,
                                                 CdcReadBlock& output) {
    output = {};
    TextLine line;
    const auto parsed = parse_ok(response, line);
    if (parsed != CdcBlockResponseError::None) return parsed;
    const auto lba = line.value_for("lba");
    const auto count = line.value_for("count");
    const auto encoding = line.value_for("encoding");
    const auto checksum = line.value_for("crc32");
    const auto data = line.value_for("data");
    if (lba.empty() || count.empty() || encoding.empty() || checksum.empty() || data.empty()) {
        return CdcBlockResponseError::MissingField;
    }
    std::uint64_t parsed_count = 0;
    std::uint32_t parsed_checksum = 0;
    if (!decimal(lba, output.lba) || !decimal(count, parsed_count) || parsed_count != 1 ||
        encoding != "BASE64" || !hexadecimal_crc(checksum, parsed_checksum)) {
        return CdcBlockResponseError::InvalidValue;
    }
    std::size_t written = 0;
    if (decode_base64(data, output.data.data(), output.data.size(), written) !=
            CodecError::None ||
        written != output.data.size()) {
        return CdcBlockResponseError::InvalidData;
    }
    if (crc32(output.data.data(), output.data.size()) != parsed_checksum) {
        return CdcBlockResponseError::BadCrc;
    }
    return CdcBlockResponseError::None;
}

}  // namespace picosd::protocol
