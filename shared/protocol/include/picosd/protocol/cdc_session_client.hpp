#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "picosd/protocol/cdc_block_response.hpp"

namespace picosd::protocol {

enum class CdcSessionClientError {
    None,
    Busy,
    NotNegotiated,
    InvalidRequest,
    InvalidResponse,
    MismatchedResponse,
    RemoteError,
    IdExhausted,
};

struct CdcSessionClientRequest {
    CdcSessionClientError error = CdcSessionClientError::None;
    std::string line;
};

// Portable single-request CDC session state used by firmware-facing control
// code. Transport and timeout policy remain outside this class.
class CdcSessionClient {
public:
    CdcSessionClientRequest begin_handshake();
    CdcSessionClientRequest begin_request(std::string_view command,
                                          std::string_view fields = {});
    CdcSessionClientRequest begin_get_info();
    CdcSessionClientRequest begin_read_block(std::uint64_t lba);
    CdcSessionClientRequest begin_write_block(std::uint64_t lba,
                                              const CdcBlockData& data);
    CdcSessionClientRequest begin_flush();
    CdcSessionClientRequest begin_eject();
    CdcSessionClientError accept_response(std::string_view response);
    void reset();

    [[nodiscard]] bool negotiated() const { return negotiated_; }
    [[nodiscard]] bool request_pending() const { return pending_id_ != 0; }
    [[nodiscard]] std::string_view session_id() const { return session_id_; }
    [[nodiscard]] std::string_view remote_error_code() const { return remote_error_code_; }

private:
    CdcSessionClientRequest reserve_request(std::string line, bool handshake);

    std::uint64_t next_id_ = 1;
    std::uint64_t pending_id_ = 0;
    bool pending_handshake_ = false;
    bool negotiated_ = false;
    std::string session_id_;
    std::string remote_error_code_;
};

}  // namespace picosd::protocol
