#include <iostream>

#include "picosd/protocol/cdc_retained_operation.hpp"

namespace {
int failures = 0;
void expect(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}
}  // namespace

int main() {
    using namespace picosd::protocol;
    CdcSessionClient client;
    CdcRetainedOperation operation;
    expect(operation.issue(client).error == CdcSessionClientError::InvalidRequest,
           "rejects issuing an empty retained operation");

    const auto hello = client.begin_handshake();
    expect(hello.error == CdcSessionClientError::None &&
               client.accept_response("OK id=1 version=0.1 session=retained") ==
                   CdcSessionClientError::None,
           "negotiates retained-operation test session");

    operation.retain_flush();
    const auto first_flush = operation.issue(client);
    expect(first_flush.line == "FLUSH id=2 session=retained" &&
               client.cancel_pending_request(),
           "issues retained operation with initial request id");
    const auto retried_flush = operation.issue(client);
    expect(retried_flush.line == "FLUSH id=3 session=retained" &&
               client.accept_response("OK id=3 session=retained") ==
                   CdcSessionClientError::None,
           "reissues retained operation with fresh request id");

    CdcBlockData data{};
    data[0] = 0x5a;
    data[511] = 0xa5;
    operation.retain_write_block(7, data);
    const auto first_write = operation.issue(client);
    expect(first_write.line.find("WRITE_BLOCKS id=4 session=retained lba=7") == 0 &&
               client.cancel_pending_request(),
           "issues retained write parameters");
    const auto retried_write = operation.issue(client);
    expect(first_write.line.substr(first_write.line.find(" lba=")) ==
               retried_write.line.substr(retried_write.line.find(" lba=")),
           "preserves write LBA, checksum, and payload across retry");
    expect(client.accept_response("OK id=5 session=retained") ==
               CdcSessionClientError::None,
           "completes retained write retry");

    operation.clear();
    expect(!operation.active() && operation.type() == CdcOperationType::None &&
               operation.issue(client).error == CdcSessionClientError::InvalidRequest,
           "clears retained operation and payload state");
    return failures == 0 ? 0 : 1;
}
