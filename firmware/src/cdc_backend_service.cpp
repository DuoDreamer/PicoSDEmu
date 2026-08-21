#include "picosd/cdc_backend_service.hpp"

#include <string>

#include "pico/time.h"
#include "picosd/cdc_device.hpp"

namespace picosd::firmware {
namespace {
constexpr std::uint64_t kRequestTimeoutUs = 250'000;
constexpr std::uint64_t kInitialRetryBackoffUs = 10'000;
constexpr std::uint64_t kMaximumRetryBackoffUs = 100'000;

FirmwareCdcBackend backend{kRequestTimeoutUs, 2, kInitialRetryBackoffUs, kMaximumRetryBackoffUs};
bool was_connected = false;
bool handshake_pending = false;
bool info_pending = false;
bool info_known = false;
std::uint64_t handshake_started_at = 0;
picosd::protocol::CdcMediaInfo media_info{};

bool send_request(const picosd::protocol::CdcSessionClientRequest &request) {
    if (request.error != picosd::protocol::CdcSessionClientError::None || request.line.empty())
        return false;
    const std::string framed = request.line + '\n';
    return write_cdc(framed.data(), framed.size());
}
} // namespace

FirmwareCdcBackend &cdc_backend() {
    return backend;
}

bool cdc_media_ready() { return backend.negotiated() && media_info.present; }

const picosd::protocol::CdcMediaInfo &cdc_media_info() { return media_info; }

bool begin_cdc_read(std::uint64_t lba, std::uint64_t generation) {
    const auto request = backend.begin_read(lba, generation, time_us_64());
    if (request.error != picosd::protocol::CdcSessionClientError::None)
        return false;
    if (request.line.empty())
        return true;
    if (send_request(request))
        return true;
    backend.invalidate();
    return false;
}

bool begin_cdc_write(std::uint64_t lba, std::uint64_t generation,
                     const picosd::protocol::CdcBlockData &data) {
    const auto request = backend.begin_write(lba, generation, data, time_us_64());
    if (request.error != picosd::protocol::CdcSessionClientError::None)
        return false;
    if (send_request(request))
        return true;
    backend.invalidate();
    return false;
}

bool copy_cdc_ready(std::uint64_t lba, std::uint64_t generation,
                    picosd::protocol::CdcBlockData &output) {
    return backend.copy_ready(lba, generation, output);
}

void initialize_cdc_backend_service() {
    was_connected = false;
    handshake_pending = false;
    info_pending = false;
    info_known = false;
    handshake_started_at = 0;
    media_info = {};
}

void poll_cdc_backend_service() {
    const bool connected = cdc_connected();
    if (!connected) {
        if (was_connected)
            backend.disconnect();
        was_connected = false;
        handshake_pending = false;
        info_pending = false;
        info_known = false;
        handshake_started_at = 0;
        media_info = {};
        return;
    }

    if (!was_connected) {
        was_connected = true;
        const auto request = backend.begin_handshake();
        handshake_pending = send_request(request);
        handshake_started_at = handshake_pending ? time_us_64() : 0;
        if (!handshake_pending)
            backend.disconnect();
        return;
    }

    if (!backend.negotiated() && !handshake_pending) {
        const auto request = backend.begin_handshake();
        handshake_pending = send_request(request);
        handshake_started_at = handshake_pending ? time_us_64() : 0;
        if (!handshake_pending)
            backend.disconnect();
        return;
    }

    if (backend.negotiated() && !info_pending && !backend.transfer_active() && !info_known) {
        const auto request = backend.begin_get_info();
        info_pending = send_request(request);
        handshake_started_at = info_pending ? time_us_64() : 0;
        if (!info_pending) backend.disconnect();
        return;
    }

    const auto now = time_us_64();
    if (handshake_pending || info_pending) {
        if (now - handshake_started_at >= kRequestTimeoutUs) {
            backend.disconnect();
            handshake_pending = false;
            info_pending = false;
            info_known = false;
            handshake_started_at = 0;
            media_info = {};
        }
        return;
    }
    auto state = backend.expire_if_due(now);
    if (state == picosd::protocol::CdcOperationState::WaitingRetry ||
        state == picosd::protocol::CdcOperationState::RetryReady) {
        state = backend.poll(now);
        if (state == picosd::protocol::CdcOperationState::RetryReady)
            (void)send_request(backend.issue_retry(now));
    }
}

bool handle_cdc_backend_response(std::string_view line) {
    if (line.rfind("OK ", 0) != 0 && line.rfind("ERR ", 0) != 0)
        return false;
    if (handshake_pending) {
        const auto result = backend.accept_handshake(line);
        // A syntactically matching response belongs to the backend even when
        // negotiation fails; reconnecting starts a fresh, unambiguous session.
        handshake_pending = false;
        handshake_started_at = 0;
        if (result != picosd::protocol::CdcSessionClientError::None)
            backend.disconnect();
        return true;
    }
    if (info_pending) {
        const auto result = backend.accept_get_info(line, media_info);
        info_pending = false;
        info_known = result == picosd::protocol::CdcSessionClientError::None;
        handshake_started_at = 0;
        if (result != picosd::protocol::CdcSessionClientError::None) {
            media_info = {};
            backend.disconnect();
        }
        return true;
    }
    if (!backend.transfer_active())
        return false;
    (void)backend.accept_response(line, time_us_64());
    return true;
}

} // namespace picosd::firmware
