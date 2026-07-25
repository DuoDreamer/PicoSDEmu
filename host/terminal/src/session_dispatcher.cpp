#include "session_dispatcher.hpp"

#include <array>
#include <charconv>
#include <cstdio>
#include <utility>

#include "picosd/protocol/codec.hpp"
#include "picosd/protocol/crc.hpp"
#include "picosd/protocol/text_line.hpp"

namespace picosd::host {
namespace {
std::string error(std::string_view id, std::string_view code) {
    return "ERR id=" + std::string(id.empty() ? "0" : id) + " code=" + std::string(code);
}
bool decimal(std::string_view text, std::uint64_t& value) {
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}
bool hexadecimal(std::string_view text, std::uint32_t& value) {
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value, 16);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}
}  // namespace

SessionDispatcher::SessionDispatcher(ImageFile& image, std::string card_type, bool writable,
                                     std::string session_id)
    : image_(image), card_type_(std::move(card_type)), session_id_(std::move(session_id)),
      writable_(writable) {}

std::string SessionDispatcher::dispatch(std::string_view request) {
    picosd::protocol::TextLine line;
    if (picosd::protocol::parse_text_line(request, line) != picosd::protocol::TextLineError::None) return error({}, "BAD_LINE");
    const auto id = line.value_for("id");
    if (id.empty()) return error({}, "MISSING_ID");
    if (line.command() == "HELLO") {
        if (line.value_for("version") != "0.1") return error(id, "UNSUPPORTED_VERSION");
        established_ = true;
        return "OK id=" + std::string(id) + " version=0.1 session=" + session_id_;
    }
    if (!established_) return error(id, "HANDSHAKE_REQUIRED");
    const auto session = line.value_for("session");
    if (session.empty()) return error(id, "MISSING_SESSION");
    if (session != session_id_) return error(id, "BAD_SESSION");
    const auto ok = "OK id=" + std::string(id) + " session=" + session_id_;
    if (ejected_ && line.command() != "EJECT") return error(id, "NO_MEDIA");
    if (line.command() == "GET_INFO") return ok + " present=1 type=" + card_type_ + " blocks=" + std::to_string(image_.block_count()) + " block_size=512 readonly=" + (writable_ ? "0" : "1");
    if (line.command() == "FLUSH") return image_.flush() ? ok : error(id, "IO_ERROR");
    if (line.command() == "EJECT") {
        if (!image_.flush()) return error(id, "IO_ERROR");
        ejected_ = true;
        return ok;
    }
    if (line.command() == "READ_BLOCKS") {
        std::uint64_t lba = 0, count = 0;
        if (!decimal(line.value_for("lba"), lba) || !decimal(line.value_for("count"), count) || count != 1) return error(id, "BAD_RANGE");
        std::array<std::uint8_t, 512> block{};
        if (!image_.read_block(lba, block.data())) return error(id, "RANGE");
        char checksum[9]{};
        std::snprintf(checksum, sizeof(checksum), "%08X", picosd::protocol::crc32(block.data(), block.size()));
        return ok + " lba=" + std::to_string(lba) + " count=1 encoding=BASE64 crc32=" + checksum + " data=" + picosd::protocol::encode_base64(block.data(), block.size());
    }
    if (line.command() == "WRITE_BLOCKS") {
        if (!writable_) return error(id, "READ_ONLY");
        std::uint64_t lba = 0, count = 0;
        std::uint32_t checksum = 0;
        const auto data = line.value_for("data");
        if (!decimal(line.value_for("lba"), lba) || !decimal(line.value_for("count"), count) || count != 1 ||
            !hexadecimal(line.value_for("crc32"), checksum)) return error(id, "BAD_RANGE");
        std::array<std::uint8_t, 512> block{};
        std::size_t written = 0;
        if (picosd::protocol::decode_base64(data, block.data(), block.size(), written) != picosd::protocol::CodecError::None || written != block.size()) return error(id, "BAD_DATA");
        if (picosd::protocol::crc32(block.data(), block.size()) != checksum) return error(id, "BAD_CRC");
        if (!image_.write_block(lba, block.data())) return error(id, "RANGE");
        return ok;
    }
    return error(id, "UNSUPPORTED");
}

}  // namespace picosd::host
