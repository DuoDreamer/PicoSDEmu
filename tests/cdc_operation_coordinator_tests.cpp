#include <iostream>

#include "picosd/protocol/cdc_operation_coordinator.hpp"

namespace {
int failures = 0;
void expect(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

bool negotiate(picosd::protocol::CdcSessionClient& client, const char* session) {
    const auto hello = client.begin_handshake();
    return hello.error == picosd::protocol::CdcSessionClientError::None &&
           client.accept_response(std::string{"OK id=1 version=0.1 session="} + session) ==
               picosd::protocol::CdcSessionClientError::None;
}
}  // namespace

int main() {
    using namespace picosd::protocol;
    CdcSessionClient client;
    expect(negotiate(client, "coordinated"), "negotiates coordinator session");

    CdcRetainedOperation operation;
    operation.retain_flush();
    CdcOperationCoordinator coordinator{10, 2, 5, 20};
    const auto initial = coordinator.start(operation, 100, client);
    expect(initial.line == "FLUSH id=2 session=coordinated" &&
               coordinator.state() == CdcOperationState::AwaitingResponse,
           "starts retained operation and deadline atomically");
    expect(coordinator.expire_if_due(109, client) ==
               CdcOperationState::AwaitingResponse &&
               coordinator.expire_if_due(110, client) ==
                   CdcOperationState::WaitingRetry,
           "routes deadline expiry into retry wait");
    expect(coordinator.issue_retry(114, client).error == CdcSessionClientError::Busy &&
               coordinator.poll(115) == CdcOperationState::RetryReady,
           "blocks retry until bounded backoff expires");
    const auto retry = coordinator.issue_retry(115, client);
    expect(retry.line == "FLUSH id=3 session=coordinated" &&
               coordinator.accept_response("OK id=3 session=coordinated", 116, client) ==
                   CdcOperationState::Idle &&
               !coordinator.operation_active(),
           "completes retry and clears retained operation");

    operation.retain_get_info();
    const auto stale = coordinator.start(operation, 200, client);
    expect(stale.line == "GET_INFO id=4 session=coordinated" &&
               coordinator.accept_response("ERR id=4 code=BAD_SESSION", 201, client) ==
                   CdcOperationState::Renegotiate &&
               coordinator.operation_active(),
           "retains operation while requesting renegotiation");
    client.reset();
    expect(negotiate(client, "replacement"), "negotiates replacement session");
    const auto resumed = coordinator.resume_after_renegotiation(202, client);
    expect(resumed.line == "GET_INFO id=2 session=replacement" &&
               coordinator.accept_response(
                   "OK id=2 session=replacement present=1 type=SDSC blocks=1", 203,
                   client) == CdcOperationState::Idle,
           "resumes retained operation in replacement session");

    operation.retain_get_info();
    const auto missing_media = coordinator.start(operation, 210, client);
    expect(missing_media.line == "GET_INFO id=3 session=replacement" &&
               coordinator.accept_response("ERR id=3 code=NO_MEDIA", 211, client) ==
                   CdcOperationState::MediaUnavailable &&
               coordinator.operation_active(),
           "retains operation while media is unavailable");
    const auto media_resumed = coordinator.resume_after_media_available(212, client);
    expect(media_resumed.line == "GET_INFO id=4 session=replacement" &&
               coordinator.accept_response(
                   "OK id=4 session=replacement present=1 type=SDHC blocks=16", 213,
                   client) == CdcOperationState::Idle &&
               !coordinator.operation_active(),
           "resumes retained operation after media becomes available");

    operation.retain_flush();
    const auto disconnecting = coordinator.start(operation, 220, client);
    expect(disconnecting.line == "FLUSH id=5 session=replacement",
           "starts operation before transport disconnect");
    coordinator.disconnect(client, CdcDisconnectPolicy::RetainOperation);
    expect(coordinator.state() == CdcOperationState::Renegotiate &&
               coordinator.operation_active() && !client.negotiated(),
           "retains in-flight operation across disconnect and requests renegotiation");
    expect(negotiate(client, "after-disconnect"), "renegotiates after disconnect");
    const auto disconnect_resumed = coordinator.resume_after_renegotiation(221, client);
    expect(disconnect_resumed.line == "FLUSH id=2 session=after-disconnect" &&
               coordinator.accept_response("OK id=2 session=after-disconnect", 222,
                                           client) == CdcOperationState::Idle,
           "reissues retained operation after disconnect renegotiation");

    operation.retain_flush();
    const auto canceling_disconnect = coordinator.start(operation, 230, client);
    expect(canceling_disconnect.line == "FLUSH id=3 session=after-disconnect",
           "starts operation before canceling disconnect");
    coordinator.disconnect(client, CdcDisconnectPolicy::CancelOperation);
    expect(coordinator.state() == CdcOperationState::Idle &&
               !coordinator.operation_active() && !client.negotiated(),
           "canceling disconnect clears retained operation and client session");
    expect(negotiate(client, "terminal"), "renegotiates terminal session");

    CdcOperationCoordinator terminal{10, 1, 1, 1};
    operation.retain_flush();
    const auto terminal_request = terminal.start(operation, 300, client);
    expect(terminal_request.line == "FLUSH id=2 session=terminal" &&
               terminal.accept_response("ERR id=2 code=BAD_CRC", 301, client) ==
                   CdcOperationState::Failed &&
               !terminal.operation_active(),
           "terminal remote error clears retained operation without retry");
    terminal.cancel(client);
    expect(terminal.state() == CdcOperationState::Idle && client.negotiated(),
           "cancels failed operation without discarding negotiated session");

    CdcOperationCoordinator exhausted{10, 1, 1, 1};
    operation.retain_flush();
    const auto exhausted_request = exhausted.start(operation, 400, client);
    expect(exhausted_request.line == "FLUSH id=3 session=terminal" &&
               exhausted.expire_if_due(410, client) == CdcOperationState::WaitingRetry,
           "first timeout is retryable");
    const auto exhausted_retry = exhausted.issue_retry(411, client);
    expect(exhausted_retry.line == "FLUSH id=4 session=terminal" &&
               exhausted.expire_if_due(421, client) == CdcOperationState::Failed &&
               !exhausted.operation_active(),
           "retry exhaustion clears retained operation");

    terminal.reset(client);
    expect(terminal.state() == CdcOperationState::Idle && !client.negotiated(),
           "reset atomically clears coordinator and client session state");
    return failures == 0 ? 0 : 1;
}
