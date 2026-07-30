#include <cstdint>
#include <iostream>
#include <limits>

#include "picosd/protocol/cdc_request_deadline.hpp"

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
    CdcRequestDeadline deadline;
    expect(!deadline.arm(100, 20, client), "cannot arm without pending request");
    expect(client.begin_handshake().error == CdcSessionClientError::None,
           "starts request for deadline test");
    expect(!deadline.arm(100, 0, client), "rejects zero timeout");
    expect(deadline.arm(100, 20, client) && deadline.armed() && deadline.deadline() == 120,
           "arms deadline from monotonic ticks");
    expect(!deadline.expire_if_due(119, client) && client.request_pending(),
           "keeps request before deadline");
    expect(deadline.expire_if_due(120, client) && !deadline.armed() &&
               !client.request_pending(),
           "cancels request at deadline");

    const auto replacement = client.begin_handshake();
    expect(deadline.arm(200, 10, client), "rearms for replacement request");
    expect(client.accept_response("OK id=2 version=0.1 session=deadline") ==
               CdcSessionClientError::None,
           "response completes request before deadline poll");
    expect(!deadline.expire_if_due(210, client) && !deadline.armed(),
           "completed request clears deadline without cancellation");

    expect(client.begin_flush().error == CdcSessionClientError::None,
           "starts request near tick limit");
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    expect(deadline.arm(maximum - 5, 10, client) && deadline.deadline() == maximum,
           "saturates overflowing deadline");
    expect(deadline.expire_if_due(maximum, client), "expires saturated deadline");
    (void)replacement;
    return failures == 0 ? 0 : 1;
}
