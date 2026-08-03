#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "picosd/protocol/cdc_request_deadline.hpp"
#include "picosd/protocol/cdc_retained_operation.hpp"
#include "picosd/protocol/cdc_retry_controller.hpp"

namespace picosd::protocol {

enum class CdcOperationState {
    Idle,
    AwaitingResponse,
    WaitingRetry,
    RetryReady,
    Renegotiate,
    MediaUnavailable,
    Failed,
};

class CdcOperationCoordinator {
public:
    CdcOperationCoordinator(std::uint64_t request_timeout, std::size_t maximum_retries,
                            std::uint64_t initial_backoff,
                            std::uint64_t maximum_backoff);

    CdcSessionClientRequest start(const CdcRetainedOperation& operation,
                                  std::uint64_t now, CdcSessionClient& client);
    CdcOperationState accept_response(std::string_view response, std::uint64_t now,
                                      CdcSessionClient& client);
    CdcOperationState expire_if_due(std::uint64_t now, CdcSessionClient& client);
    CdcOperationState poll(std::uint64_t now);
    CdcSessionClientRequest issue_retry(std::uint64_t now, CdcSessionClient& client);
    CdcSessionClientRequest resume_after_renegotiation(std::uint64_t now,
                                                       CdcSessionClient& client);
    void cancel(CdcSessionClient& client);
    void reset(CdcSessionClient& client);

    [[nodiscard]] CdcOperationState state() const { return state_; }
    [[nodiscard]] bool operation_active() const { return operation_.active(); }

private:
    CdcSessionClientRequest issue(std::uint64_t now, CdcSessionClient& client);
    CdcOperationState route_failure(CdcRetryAdvice advice, std::uint64_t now);

    std::uint64_t request_timeout_;
    CdcRequestDeadline deadline_;
    CdcRetryController retry_;
    CdcRetainedOperation operation_;
    CdcOperationState state_ = CdcOperationState::Idle;
};

}  // namespace picosd::protocol
