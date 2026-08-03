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

    CdcOperationCoordinator terminal{10, 1, 1, 1};
    operation.retain_flush();
    const auto terminal_request = terminal.start(operation, 300, client);
    expect(terminal_request.line == "FLUSH id=3 session=replacement" &&
               terminal.accept_response("ERR id=3 code=BAD_CRC", 301, client) ==
                   CdcOperationState::Failed &&
               !terminal.operation_active(),
           "terminal remote error clears retained operation without retry");
    terminal.cancel(client);
    expect(terminal.state() == CdcOperationState::Idle && client.negotiated(),
           "cancels failed operation without discarding negotiated session");
    terminal.reset(client);
    expect(terminal.state() == CdcOperationState::Idle && !client.negotiated(),
           "reset atomically clears coordinator and client session state");
    return failures == 0 ? 0 : 1;
}
