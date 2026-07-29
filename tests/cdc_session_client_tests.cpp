#include <iostream>

#include "picosd/protocol/cdc_session_client.hpp"

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
    using picosd::protocol::CdcSessionClient;
    using picosd::protocol::CdcSessionClientError;

    CdcSessionClient client;
    expect(client.begin_request("GET_INFO").error == CdcSessionClientError::NotNegotiated,
           "media request requires negotiation");
    const auto hello = client.begin_handshake();
    expect(hello.error == CdcSessionClientError::None &&
               hello.line == "HELLO id=1 version=0.1",
           "builds initial handshake");
    expect(client.begin_handshake().error == CdcSessionClientError::Busy,
           "allows only one outstanding request");
    expect(client.accept_response("OK id=2 version=0.1 session=abc") ==
               CdcSessionClientError::MismatchedResponse && client.request_pending(),
           "retains request after mismatched response id");
    expect(client.accept_response("OK id=1 version=9.9 session=abc") ==
               CdcSessionClientError::InvalidResponse && client.request_pending(),
           "retains request after incompatible response");
    expect(client.accept_response("OK id=1 version=0.1 session=abc") ==
               CdcSessionClientError::None && client.negotiated() &&
               client.session_id() == "abc",
           "accepts negotiated session");

    expect(client.begin_request("GET_INFO", "session=override").error ==
               CdcSessionClientError::InvalidRequest,
           "prevents caller from overriding reserved fields");
    const auto info = client.begin_request("GET_INFO");
    expect(info.error == CdcSessionClientError::None &&
               info.line == "GET_INFO id=2 session=abc",
           "builds session-bound request with increasing id");
    expect(client.accept_response("OK id=2 session=stale present=1") ==
               CdcSessionClientError::MismatchedResponse && client.request_pending(),
           "rejects response from stale session");
    expect(client.accept_response("OK id=2 session=abc present=1") ==
               CdcSessionClientError::None && !client.request_pending(),
           "accepts correlated session response");

    const auto flush = client.begin_request("FLUSH");
    expect(flush.line == "FLUSH id=3 session=abc", "advances request id");
    expect(client.accept_response("ERR id=3 code=IO_ERROR") ==
               CdcSessionClientError::RemoteError && !client.request_pending(),
           "completes request on remote error");

    client.reset();
    expect(!client.negotiated() && client.session_id().empty() &&
               client.begin_handshake().line == "HELLO id=1 version=0.1",
           "reset clears session and request sequence");
    return failures == 0 ? 0 : 1;
}
