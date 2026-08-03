#include "picosd/protocol/cdc_session_client.hpp"

#include <charconv>
#include <cstdio>
#include <limits>
#include <utility>

#include "picosd/protocol/text_line.hpp"
#include "picosd/protocol/version.hpp"
#include "picosd/protocol/codec.hpp"
#include "picosd/protocol/crc.hpp"

namespace picosd::protocol {
namespace {

bool parse_id(std::string_view text, std::uint64_t& id) {
    const auto result = std::from_chars(text.data(), text.data() + text.size(), id);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size() && id != 0;
}

std::string protocol_version() {
    return std::to_string(kVersionMajor) + "." + std::to_string(kVersionMinor);
}

CdcRemoteError decode_remote_error(std::string_view code) {
    if (code == "BAD_LINE") return CdcRemoteError::BadLine;
    if (code == "MISSING_ID") return CdcRemoteError::MissingId;
    if (code == "BAD_ID") return CdcRemoteError::BadId;
    if (code == "UNSUPPORTED_VERSION") return CdcRemoteError::UnsupportedVersion;
    if (code == "STALE_ID") return CdcRemoteError::StaleId;
    if (code == "HANDSHAKE_REQUIRED") return CdcRemoteError::HandshakeRequired;
    if (code == "MISSING_SESSION") return CdcRemoteError::MissingSession;
    if (code == "BAD_SESSION") return CdcRemoteError::BadSession;
    if (code == "NO_MEDIA") return CdcRemoteError::NoMedia;
    if (code == "IO_ERROR") return CdcRemoteError::IoError;
    if (code == "BAD_RANGE") return CdcRemoteError::BadRange;
    if (code == "RANGE") return CdcRemoteError::Range;
    if (code == "READ_ONLY") return CdcRemoteError::ReadOnly;
    if (code == "BAD_DATA") return CdcRemoteError::BadData;
    if (code == "BAD_CRC") return CdcRemoteError::BadCrc;
    if (code == "UNSUPPORTED") return CdcRemoteError::Unsupported;
    return CdcRemoteError::Unknown;
}

}  // namespace

CdcRetryAdvice retry_advice(CdcRemoteError error) {
    switch (error) {
        case CdcRemoteError::IoError:
            return CdcRetryAdvice::RetrySameSession;
        case CdcRemoteError::StaleId:
        case CdcRemoteError::HandshakeRequired:
        case CdcRemoteError::MissingSession:
        case CdcRemoteError::BadSession:
            return CdcRetryAdvice::Renegotiate;
        case CdcRemoteError::NoMedia:
            return CdcRetryAdvice::MediaUnavailable;
        case CdcRemoteError::None:
        case CdcRemoteError::BadLine:
        case CdcRemoteError::MissingId:
        case CdcRemoteError::BadId:
        case CdcRemoteError::UnsupportedVersion:
        case CdcRemoteError::BadRange:
        case CdcRemoteError::Range:
        case CdcRemoteError::ReadOnly:
        case CdcRemoteError::BadData:
        case CdcRemoteError::BadCrc:
        case CdcRemoteError::Unsupported:
        case CdcRemoteError::Unknown:
            return CdcRetryAdvice::DoNotRetry;
    }
    return CdcRetryAdvice::DoNotRetry;
}

CdcSessionClientRequest CdcSessionClient::begin_handshake() {
    if (request_pending()) return {CdcSessionClientError::Busy, {}};
    if (negotiated_) return {CdcSessionClientError::InvalidRequest, {}};
    if (next_id_ == 0) return {CdcSessionClientError::IdExhausted, {}};
    return reserve_request("HELLO id=" + std::to_string(next_id_) +
                               " version=" + protocol_version(),
                           true);
}

CdcSessionClientRequest CdcSessionClient::begin_request(std::string_view command,
                                                        std::string_view fields) {
    if (request_pending()) return {CdcSessionClientError::Busy, {}};
    if (!negotiated_) return {CdcSessionClientError::NotNegotiated, {}};
    if (next_id_ == 0) return {CdcSessionClientError::IdExhausted, {}};
    if (command.empty() || command == "HELLO" || command == "OK" || command == "ERR") {
        return {CdcSessionClientError::InvalidRequest, {}};
    }

    std::string line{command};
    line += " id=" + std::to_string(next_id_) + " session=" + session_id_;
    if (!fields.empty()) line += " " + std::string(fields);
    TextLine parsed;
    if (parse_text_line(line, parsed) != TextLineError::None || parsed.command() != command) {
        return {CdcSessionClientError::InvalidRequest, {}};
    }
    return reserve_request(std::move(line), false);
}

CdcSessionClientRequest CdcSessionClient::begin_get_info() {
    return begin_request("GET_INFO");
}

CdcSessionClientRequest CdcSessionClient::begin_read_block(std::uint64_t lba) {
    return begin_request("READ_BLOCKS", "lba=" + std::to_string(lba) +
                                            " count=1 encoding=BASE64");
}

CdcSessionClientRequest CdcSessionClient::begin_write_block(std::uint64_t lba,
                                                            const CdcBlockData& data) {
    char checksum[9]{};
    std::snprintf(checksum, sizeof(checksum), "%08X", crc32(data.data(), data.size()));
    return begin_request("WRITE_BLOCKS", "lba=" + std::to_string(lba) +
                                             " count=1 encoding=BASE64 crc32=" + checksum +
                                             " data=" + encode_base64(data.data(), data.size()));
}

CdcSessionClientRequest CdcSessionClient::begin_flush() {
    return begin_request("FLUSH");
}

CdcSessionClientRequest CdcSessionClient::begin_eject() {
    return begin_request("EJECT");
}

CdcSessionClientRequest CdcSessionClient::reserve_request(std::string line, bool handshake) {
    remote_error_ = CdcRemoteError::None;
    remote_error_code_.clear();
    pending_id_ = next_id_;
    pending_handshake_ = handshake;
    next_id_ = next_id_ == std::numeric_limits<std::uint64_t>::max() ? 0 : next_id_ + 1;
    return {CdcSessionClientError::None, std::move(line)};
}

CdcSessionClientError CdcSessionClient::accept_response(std::string_view response) {
    if (!request_pending()) return CdcSessionClientError::MismatchedResponse;
    TextLine line;
    if (parse_text_line(response, line) != TextLineError::None ||
        (line.command() != "OK" && line.command() != "ERR")) {
        return CdcSessionClientError::InvalidResponse;
    }
    std::uint64_t response_id = 0;
    if (!parse_id(line.value_for("id"), response_id)) {
        return CdcSessionClientError::InvalidResponse;
    }
    if (response_id != pending_id_) return CdcSessionClientError::MismatchedResponse;
    if (line.command() == "ERR") {
        const auto code = line.value_for("code");
        if (code.empty()) return CdcSessionClientError::InvalidResponse;
        remote_error_ = decode_remote_error(code);
        remote_error_code_ = std::string(code);
        pending_id_ = 0;
        pending_handshake_ = false;
        return CdcSessionClientError::RemoteError;
    }

    if (pending_handshake_) {
        const auto session = line.value_for("session");
        if (line.value_for("version") != protocol_version() || session.empty()) {
            return CdcSessionClientError::InvalidResponse;
        }
        session_id_ = std::string(session);
        negotiated_ = true;
    } else if (line.value_for("session") != session_id_) {
        return CdcSessionClientError::MismatchedResponse;
    }
    pending_id_ = 0;
    pending_handshake_ = false;
    return CdcSessionClientError::None;
}

bool CdcSessionClient::cancel_pending_request() {
    if (!request_pending()) return false;
    pending_id_ = 0;
    pending_handshake_ = false;
    return true;
}

void CdcSessionClient::reset() {
    next_id_ = 1;
    pending_id_ = 0;
    pending_handshake_ = false;
    negotiated_ = false;
    session_id_.clear();
    remote_error_ = CdcRemoteError::None;
    remote_error_code_.clear();
}

}  // namespace picosd::protocol
