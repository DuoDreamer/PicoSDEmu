#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

#include "cdc_transport.hpp"
#include "image_file.hpp"
#include "session_dispatcher.hpp"
#include "session_runner.hpp"
#include "picosd/protocol/cdc_block_response.hpp"
#include "picosd/protocol/cdc_request_deadline.hpp"
#include "picosd/protocol/cdc_retry_controller.hpp"
#include "picosd/protocol/cdc_session_client.hpp"

namespace {
int failures = 0;

void expect(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

std::string exchange(picosd::host::MemoryCdcTransport& transport,
                     picosd::host::SessionDispatcher& dispatcher,
                     std::string request) {
    transport.inject_received_line(std::move(request));
    if (picosd::host::process_one_request(transport, dispatcher) !=
        picosd::host::SessionRunResult::Processed) {
        return {};
    }
    return transport.written_lines().back();
}
}  // namespace

int main() {
    using picosd::protocol::CdcSessionClient;
    using picosd::protocol::CdcSessionClientError;

    const auto path = std::filesystem::temp_directory_path() / "picosd-session-integration.img";
    {
        std::ofstream file{path, std::ios::binary | std::ios::trunc};
        std::array<char, 512> block{};
        block[0] = 0x2a;
        file.write(block.data(), block.size());
    }

    picosd::host::ImageFile image;
    expect(image.open(path, true), "opens integration image");
    picosd::host::MemoryCdcTransport transport;
    expect(transport.open("integration") == picosd::host::CdcTransportError::None,
           "opens in-memory link");
    picosd::host::SessionDispatcher dispatcher{image, "sdsc", true, "first-session"};
    CdcSessionClient client;

    const auto hello = client.begin_handshake();
    const auto hello_response = exchange(transport, dispatcher, hello.line);
    expect(hello_response == "OK id=1 version=0.1 session=first-session",
           "host negotiates client session");
    expect(client.accept_response(hello_response) == CdcSessionClientError::None &&
               client.session_id() == "first-session",
           "client accepts host negotiation");

    const auto info = client.begin_get_info();
    const auto info_response = exchange(transport, dispatcher, info.line);
    expect(info_response.find("OK id=2 session=first-session") == 0 &&
               info_response.find("blocks=1") != std::string::npos,
           "session-bound metadata round trip succeeds");
    picosd::protocol::CdcCardInfo card_info;
    expect(picosd::protocol::decode_get_info_response(info_response, card_info) ==
               picosd::protocol::CdcBlockResponseError::None &&
               card_info.blocks == 1 && card_info.type == picosd::protocol::CdcCardType::Sdsc,
           "client decodes typed metadata");
    expect(client.accept_response("OK id=2 session=unrelated present=1") ==
               CdcSessionClientError::MismatchedResponse && client.request_pending(),
           "unrelated response does not consume pending request");
    expect(client.accept_response(info_response) == CdcSessionClientError::None,
           "client accepts correlated metadata response");

    const auto read = client.begin_read_block(0);
    const auto read_response = exchange(transport, dispatcher, read.line);
    expect(read_response.find("data=Kg") != std::string::npos,
           "block payload crosses complete session stack");
    picosd::protocol::CdcReadBlock read_block;
    expect(picosd::protocol::decode_read_block_response(read_response, read_block) ==
               picosd::protocol::CdcBlockResponseError::None &&
               read_block.lba == 0 && read_block.data[0] == 0x2a,
           "client decodes and verifies typed block response");
    expect(client.accept_response(read_response) == CdcSessionClientError::None,
           "client correlates block response");

    picosd::protocol::CdcBlockData replacement{};
    replacement[0] = 0x7c;
    replacement[511] = 0xa5;
    const auto write = client.begin_write_block(0, replacement);
    const auto write_response = exchange(transport, dispatcher, write.line);
    expect(write_response == "OK id=4 session=first-session" &&
               client.accept_response(write_response) == CdcSessionClientError::None,
           "typed block write crosses complete session stack");
    const auto read_back = client.begin_read_block(0);
    const auto read_back_response = exchange(transport, dispatcher, read_back.line);
    picosd::protocol::CdcReadBlock written_block;
    expect(picosd::protocol::decode_read_block_response(read_back_response, written_block) ==
               picosd::protocol::CdcBlockResponseError::None &&
               written_block.data == replacement &&
               client.accept_response(read_back_response) == CdcSessionClientError::None,
           "typed write reads back with verified payload");

    picosd::protocol::CdcBlockData rejected{};
    rejected[0] = 0x11;
    auto corrupt_write = client.begin_write_block(0, rejected);
    const auto crc_position = corrupt_write.line.find("crc32=") + 6;
    corrupt_write.line[crc_position] =
        corrupt_write.line[crc_position] == '0' ? '1' : '0';
    const auto corrupt_response = exchange(transport, dispatcher, corrupt_write.line);
    expect(corrupt_response == "ERR id=6 code=BAD_CRC" &&
               client.accept_response(corrupt_response) == CdcSessionClientError::RemoteError &&
               client.remote_error() == picosd::protocol::CdcRemoteError::BadCrc &&
               client.remote_error_code() == "BAD_CRC",
           "corrupt typed write is rejected with a specific remote error");
    const auto unchanged_read = client.begin_read_block(0);
    const auto unchanged_response = exchange(transport, dispatcher, unchanged_read.line);
    picosd::protocol::CdcReadBlock unchanged_block;
    expect(picosd::protocol::decode_read_block_response(unchanged_response, unchanged_block) ==
               picosd::protocol::CdcBlockResponseError::None &&
               unchanged_block.data == replacement &&
               client.accept_response(unchanged_response) == CdcSessionClientError::None,
           "bad CRC leaves image block unchanged");

    const auto delayed_flush = client.begin_flush();
    const auto delayed_response = exchange(transport, dispatcher, delayed_flush.line);
    expect(delayed_response == "OK id=8 session=first-session" &&
               client.cancel_pending_request(),
           "caller can cancel a response after its deadline");
    expect(client.accept_response(delayed_response) ==
               CdcSessionClientError::MismatchedResponse,
           "late cancelled response is ignored");
    const auto after_timeout = client.begin_get_info();
    const auto after_timeout_response = exchange(transport, dispatcher, after_timeout.line);
    expect(after_timeout.line == "GET_INFO id=9 session=first-session" &&
               client.accept_response(after_timeout_response) == CdcSessionClientError::None,
           "session continues with a fresh id after cancellation");

    const auto flush = client.begin_flush();
    const auto flush_response = exchange(transport, dispatcher, flush.line);
    expect(flush_response == "OK id=10 session=first-session" &&
               client.accept_response(flush_response) == CdcSessionClientError::None,
           "flush completes through typed session path");
    const auto out_of_range_read = client.begin_read_block(1);
    const auto range_read_response =
        exchange(transport, dispatcher, out_of_range_read.line);
    expect(range_read_response == "ERR id=11 code=RANGE" &&
               client.accept_response(range_read_response) ==
                   CdcSessionClientError::RemoteError &&
               client.remote_error() == picosd::protocol::CdcRemoteError::Range,
           "out-of-range read returns typed range error");
    const auto out_of_range_write = client.begin_write_block(1, replacement);
    const auto range_write_response =
        exchange(transport, dispatcher, out_of_range_write.line);
    expect(range_write_response == "ERR id=12 code=RANGE" &&
               client.accept_response(range_write_response) ==
                   CdcSessionClientError::RemoteError &&
               client.remote_error() == picosd::protocol::CdcRemoteError::Range,
           "out-of-range write returns typed range error");
    const auto eject = client.begin_eject();
    const auto eject_response = exchange(transport, dispatcher, eject.line);
    expect(eject_response == "OK id=13 session=first-session" &&
               client.accept_response(eject_response) == CdcSessionClientError::None,
           "eject completes through typed session path");
    const auto after_eject = client.begin_get_info();
    const auto no_media_response = exchange(transport, dispatcher, after_eject.line);
    expect(no_media_response == "ERR id=14 code=NO_MEDIA" &&
               client.accept_response(no_media_response) ==
                   CdcSessionClientError::RemoteError &&
               client.remote_error() == picosd::protocol::CdcRemoteError::NoMedia &&
               picosd::protocol::retry_advice(client.remote_error()) ==
                   picosd::protocol::CdcRetryAdvice::MediaUnavailable,
           "ejected image reports unavailable media without retry");

    transport.close();
    client.reset();
    expect(transport.open("reconnected") == picosd::host::CdcTransportError::None,
           "reopens in-memory link");
    picosd::host::SessionDispatcher reconnected_dispatcher{
        image, "SDSC", false, "second-session"};
    const auto reconnect_hello = client.begin_handshake();
    const auto reconnect_response =
        exchange(transport, reconnected_dispatcher, reconnect_hello.line);
    expect(reconnect_hello.line == "HELLO id=1 version=0.1" &&
               reconnect_response == "OK id=1 version=0.1 session=second-session" &&
               client.accept_response(reconnect_response) == CdcSessionClientError::None &&
               client.session_id() == "second-session",
           "disconnect creates fresh request and host session state");
    const auto readonly_write = client.begin_write_block(0, rejected);
    const auto readonly_response =
        exchange(transport, reconnected_dispatcher, readonly_write.line);
    expect(readonly_response == "ERR id=2 code=READ_ONLY" &&
               client.accept_response(readonly_response) == CdcSessionClientError::RemoteError &&
               client.remote_error() == picosd::protocol::CdcRemoteError::ReadOnly &&
               client.remote_error_code() == "READ_ONLY",
           "read-only session rejects typed writes with a specific error");

    picosd::protocol::CdcRequestDeadline deadline;
    picosd::protocol::CdcRetryController retry{2, 5, 20};
    const auto timed_out_info = client.begin_get_info();
    const auto late_info_response =
        exchange(transport, reconnected_dispatcher, timed_out_info.line);
    expect(deadline.arm(100, 10, client) &&
               !deadline.expire_if_due(109, client) && client.request_pending(),
           "keeps an integrated request pending before its deadline");
    expect(deadline.expire_if_due(110, client) && !client.request_pending() &&
               retry.record_failure(picosd::protocol::CdcRetryAdvice::RetrySameSession,
                                    110) == picosd::protocol::CdcRetryDecision::Wait &&
               retry.retry_at() == 115,
           "cancels timed-out request and schedules bounded retry");
    expect(client.accept_response(late_info_response) ==
               CdcSessionClientError::MismatchedResponse &&
               !retry.consume_retry(114) && retry.consume_retry(115),
           "rejects late response and releases retry only after backoff");
    const auto retried_info = client.begin_get_info();
    const auto retried_info_response =
        exchange(transport, reconnected_dispatcher, retried_info.line);
    expect(timed_out_info.line == "GET_INFO id=3 session=second-session" &&
               retried_info.line == "GET_INFO id=4 session=second-session" &&
               client.accept_response(retried_info_response) ==
                   CdcSessionClientError::None,
           "retry uses a fresh request id in the same negotiated session");
    retry.record_success();
    expect(retry.retries_scheduled() == 0,
           "successful integrated retry restores retry budget");

    std::filesystem::remove(path);
    return failures == 0 ? 0 : 1;
}
