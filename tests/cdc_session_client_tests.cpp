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
    using picosd::protocol::CdcBlockData;
    using picosd::protocol::CdcRemoteError;

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
               CdcSessionClientError::RemoteError && !client.request_pending() &&
               client.remote_error() == CdcRemoteError::IoError &&
               client.remote_error_code() == "IO_ERROR",
           "completes request on remote error");
    expect(client.begin_flush().error == CdcSessionClientError::None &&
               client.remote_error() == CdcRemoteError::None &&
               client.remote_error_code().empty(),
           "new request clears previous remote error");
    expect(client.accept_response("ERR id=4") == CdcSessionClientError::InvalidResponse &&
               client.request_pending(),
           "remote error requires a code");

    client.reset();
    const auto reset_hello = client.begin_handshake();
    expect(!client.negotiated() && client.session_id().empty() &&
               reset_hello.line == "HELLO id=1 version=0.1",
           "reset clears session and request sequence");
    expect(client.accept_response("OK id=1 version=0.1 session=typed") ==
               CdcSessionClientError::None,
           "renegotiates typed request session");
    expect(client.begin_get_info().line == "GET_INFO id=2 session=typed",
           "builds typed metadata request");
    expect(client.accept_response("OK id=2 session=typed present=1") ==
               CdcSessionClientError::None,
           "completes typed metadata request");
    expect(client.begin_read_block(17).line ==
               "READ_BLOCKS id=3 session=typed lba=17 count=1 encoding=BASE64",
           "builds typed block read request");
    expect(client.accept_response("OK id=3 session=typed") == CdcSessionClientError::None,
           "completes typed read request");
    CdcBlockData block{};
    block[0] = 0x2a;
    const auto write = client.begin_write_block(18, block);
    expect(write.line.find("WRITE_BLOCKS id=4 session=typed lba=18 count=1 encoding=BASE64 ") == 0 &&
               write.line.find("crc32=") != std::string::npos &&
               write.line.find("data=Kg") != std::string::npos,
           "builds typed checksummed block write request");
    expect(client.accept_response("OK id=4 session=typed") == CdcSessionClientError::None,
           "completes typed write request");
    expect(client.begin_flush().line == "FLUSH id=5 session=typed",
           "builds typed flush request");
    expect(client.accept_response("OK id=5 session=typed") == CdcSessionClientError::None,
           "completes typed flush request");
    expect(client.begin_eject().line == "EJECT id=6 session=typed",
           "builds typed eject request");
    expect(client.accept_response("OK id=6 session=typed") == CdcSessionClientError::None,
           "completes typed eject request");
    expect(client.begin_flush().line == "FLUSH id=7 session=typed" &&
               client.accept_response("ERR id=7 code=FUTURE_ERROR") ==
                   CdcSessionClientError::RemoteError &&
               client.remote_error() == CdcRemoteError::Unknown &&
               client.remote_error_code() == "FUTURE_ERROR",
           "preserves unknown future remote error code");
    return failures == 0 ? 0 : 1;
}
