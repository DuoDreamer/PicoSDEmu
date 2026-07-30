#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <unistd.h>

#include "cdc_transport.hpp"
#include "image_file.hpp"
#include "session_dispatcher.hpp"
#include "session_runner.hpp"
#include "picosd/protocol/cdc_session_client.hpp"

namespace {
int failures = 0;
void expect(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

bool write_all(int descriptor, std::string_view bytes) {
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto count = ::write(descriptor, bytes.data() + offset, bytes.size() - offset);
        if (count <= 0) return false;
        offset += static_cast<std::size_t>(count);
    }
    return true;
}

std::string read_lines(int descriptor, std::size_t line_count) {
    std::string result;
    std::array<char, 512> bytes{};
    while (static_cast<std::size_t>(std::count(result.begin(), result.end(), '\n')) <
           line_count) {
        const auto count = ::read(descriptor, bytes.data(), bytes.size());
        if (count <= 0) return {};
        result.append(bytes.data(), static_cast<std::size_t>(count));
    }
    return result;
}
}  // namespace

int main() {
    using namespace picosd;
    const auto path = std::filesystem::temp_directory_path() / "picosd-stream.img";
    {
        std::ofstream file{path, std::ios::binary | std::ios::trunc};
        std::array<char, 512> block{};
        file.write(block.data(), block.size());
    }
    host::ImageFile image;
    expect(image.open(path, true), "opens stream integration image");

    const int controller = ::posix_openpt(O_RDWR | O_NOCTTY);
    const bool controller_ready =
        controller >= 0 && ::grantpt(controller) == 0 && ::unlockpt(controller) == 0;
    expect(controller_ready, "creates pseudo-terminal stream");
    if (!controller_ready) {
        if (controller >= 0) ::close(controller);
        std::filesystem::remove(path);
        return 1;
    }
    const char* endpoint = ::ptsname(controller);
    expect(endpoint != nullptr, "gets pseudo-terminal stream endpoint");
    if (endpoint == nullptr) {
        ::close(controller);
        std::filesystem::remove(path);
        return 1;
    }

    host::PosixCdcTransport transport;
    expect(transport.open(endpoint) == host::CdcTransportError::None,
           "opens host side of stream");
    host::SessionDispatcher dispatcher{image, "SDSC", true, "stream-session"};
    protocol::CdcSessionClient client;

    const auto hello = client.begin_handshake();
    expect(write_all(controller, hello.line.substr(0, 5)) &&
               host::process_one_request(transport, dispatcher) ==
                   host::SessionRunResult::NoRequest,
           "retains first handshake fragment");
    expect(write_all(controller, hello.line.substr(5, 7)) &&
               host::process_one_request(transport, dispatcher) ==
                   host::SessionRunResult::NoRequest,
           "retains second handshake fragment");
    expect(write_all(controller, hello.line.substr(12) + "\r\n") &&
               host::process_one_request(transport, dispatcher) ==
                   host::SessionRunResult::Processed,
           "processes completed fragmented CRLF handshake");
    auto responses = read_lines(controller, 1);
    expect(!responses.empty() && responses.back() == '\n',
           "reads complete handshake response line");
    if (!responses.empty()) responses.pop_back();
    expect(client.accept_response(responses) == protocol::CdcSessionClientError::None &&
               client.session_id() == "stream-session",
           "client accepts response from byte stream");

    const auto info = client.begin_get_info();
    const std::string queued_flush = "FLUSH id=3 session=stream-session";
    expect(write_all(controller, info.line + "\n" + queued_flush + "\n"),
           "coalesces two requests into one stream write");
    expect(host::process_one_request(transport, dispatcher) ==
               host::SessionRunResult::Processed &&
               host::process_one_request(transport, dispatcher) ==
                   host::SessionRunResult::Processed,
           "host drains coalesced requests independently");
    responses = read_lines(controller, 2);
    const auto separator = responses.find('\n');
    const bool complete_pair = separator != std::string::npos && !responses.empty() &&
                               responses.back() == '\n';
    expect(complete_pair, "reads both complete coalesced response lines");
    const auto info_response = complete_pair ? responses.substr(0, separator) : std::string{};
    const auto flush_response =
        complete_pair ? responses.substr(separator + 1, responses.size() - separator - 2)
                      : std::string{};
    expect(client.accept_response(info_response) == protocol::CdcSessionClientError::None,
           "correlates first coalesced response");
    const auto flush = client.begin_flush();
    expect(flush.line == queued_flush &&
               client.accept_response(flush_response) == protocol::CdcSessionClientError::None,
           "correlates buffered second response");

    transport.close();
    ::close(controller);
    std::filesystem::remove(path);
    return failures == 0 ? 0 : 1;
}
