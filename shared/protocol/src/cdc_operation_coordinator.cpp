#include "picosd/protocol/cdc_operation_coordinator.hpp"

namespace picosd::protocol {

CdcOperationCoordinator::CdcOperationCoordinator(std::uint64_t request_timeout,
                                                 std::size_t maximum_retries,
                                                 std::uint64_t initial_backoff,
                                                 std::uint64_t maximum_backoff)
    : request_timeout_(request_timeout),
      retry_(maximum_retries, initial_backoff, maximum_backoff) {}

CdcSessionClientRequest CdcOperationCoordinator::start(
    const CdcRetainedOperation& operation, std::uint64_t now,
    CdcSessionClient& client) {
    if (state_ != CdcOperationState::Idle) {
        return {CdcSessionClientError::Busy, {}};
    }
    if (!operation.active()) return {CdcSessionClientError::InvalidRequest, {}};
    operation_ = operation;
    retry_.record_success();
    return issue(now, client);
}

CdcSessionClientRequest CdcOperationCoordinator::issue(std::uint64_t now,
                                                       CdcSessionClient& client) {
    auto request = operation_.issue(client);
    if (request.error != CdcSessionClientError::None) {
        state_ = CdcOperationState::Failed;
        operation_.clear();
        return request;
    }
    if (!deadline_.arm(now, request_timeout_, client)) {
        (void)client.cancel_pending_request();
        state_ = CdcOperationState::Failed;
        operation_.clear();
        return {CdcSessionClientError::InvalidRequest, {}};
    }
    state_ = CdcOperationState::AwaitingResponse;
    return request;
}

CdcOperationState CdcOperationCoordinator::accept_response(
    std::string_view response, std::uint64_t now, CdcSessionClient& client) {
    if (state_ != CdcOperationState::AwaitingResponse) return state_;
    const auto result = client.accept_response(response);
    if (result == CdcSessionClientError::None) {
        deadline_.clear();
        retry_.record_success();
        operation_.clear();
        state_ = CdcOperationState::Idle;
    } else if (result == CdcSessionClientError::RemoteError) {
        deadline_.clear();
        state_ = route_failure(retry_advice(client.remote_error()), now);
    }
    return state_;
}

CdcOperationState CdcOperationCoordinator::expire_if_due(std::uint64_t now,
                                                         CdcSessionClient& client) {
    if (state_ == CdcOperationState::AwaitingResponse &&
        deadline_.expire_if_due(now, client)) {
        state_ = route_failure(CdcRetryAdvice::RetrySameSession, now);
    }
    return state_;
}

CdcOperationState CdcOperationCoordinator::route_failure(CdcRetryAdvice advice,
                                                         std::uint64_t now) {
    switch (retry_.record_failure(advice, now)) {
        case CdcRetryDecision::Wait:
            return CdcOperationState::WaitingRetry;
        case CdcRetryDecision::Renegotiate:
            return CdcOperationState::Renegotiate;
        case CdcRetryDecision::MediaUnavailable:
            return CdcOperationState::MediaUnavailable;
        case CdcRetryDecision::NoRetry:
        case CdcRetryDecision::Exhausted:
            operation_.clear();
            return CdcOperationState::Failed;
        case CdcRetryDecision::Ready:
            break;
    }
    operation_.clear();
    return CdcOperationState::Failed;
}

CdcOperationState CdcOperationCoordinator::poll(std::uint64_t now) {
    if (state_ == CdcOperationState::WaitingRetry &&
        retry_.poll(now) == CdcRetryDecision::Ready) {
        state_ = CdcOperationState::RetryReady;
    }
    return state_;
}

CdcSessionClientRequest CdcOperationCoordinator::issue_retry(
    std::uint64_t now, CdcSessionClient& client) {
    if ((state_ != CdcOperationState::WaitingRetry &&
         state_ != CdcOperationState::RetryReady) ||
        !retry_.consume_retry(now)) {
        return {CdcSessionClientError::Busy, {}};
    }
    return issue(now, client);
}

CdcSessionClientRequest CdcOperationCoordinator::resume_after_renegotiation(
    std::uint64_t now, CdcSessionClient& client) {
    if (state_ != CdcOperationState::Renegotiate || !client.negotiated()) {
        return {CdcSessionClientError::NotNegotiated, {}};
    }
    retry_.record_success();
    return issue(now, client);
}

CdcSessionClientRequest CdcOperationCoordinator::resume_after_media_available(
    std::uint64_t now, CdcSessionClient& client) {
    if (state_ != CdcOperationState::MediaUnavailable || !client.negotiated()) {
        return {CdcSessionClientError::NotNegotiated, {}};
    }
    retry_.record_success();
    return issue(now, client);
}

void CdcOperationCoordinator::disconnect(CdcSessionClient& client,
                                          CdcDisconnectPolicy policy) {
    deadline_.clear();
    retry_.record_success();
    (void)client.cancel_pending_request();
    client.reset();
    if (policy == CdcDisconnectPolicy::RetainOperation && operation_.active()) {
        state_ = CdcOperationState::Renegotiate;
        return;
    }
    operation_.clear();
    state_ = CdcOperationState::Idle;
}

void CdcOperationCoordinator::cancel(CdcSessionClient& client) {
    deadline_.clear();
    retry_.record_success();
    operation_.clear();
    state_ = CdcOperationState::Idle;
    (void)client.cancel_pending_request();
}

void CdcOperationCoordinator::reset(CdcSessionClient& client) {
    cancel(client);
    client.reset();
}

}  // namespace picosd::protocol
