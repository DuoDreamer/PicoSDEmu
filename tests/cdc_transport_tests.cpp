#include <algorithm>
#include <iostream>
#include <string>

#if !defined(_WIN32)
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "cdc_transport.hpp"

namespace {
int failures = 0;

void expect(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

void test_memory_transport() {
    using namespace picosd::host;
    MemoryCdcTransport transport;
    std::string line;
    expect(transport.write_line("HELLO id=1") == CdcTransportError::NotOpen,
           "closed write rejected");
    expect(transport.open("memory") == CdcTransportError::None && transport.is_open(),
           "opens memory transport");
    expect(transport.write_line(std::string(kMaximumCdcLineLength + 1, 'X')) ==
               CdcTransportError::LineTooLong,
           "enforces the shared line-size limit");
    expect(transport.read_line(line) == CdcTransportError::WouldBlock,
           "empty memory transport would block");
    expect(transport.write_line("HELLO id=1") == CdcTransportError::None &&
               transport.written_lines().front() == "HELLO id=1",
           "writes memory line");
    expect(transport.write_line("BAD\n") == CdcTransportError::InvalidLine,
           "terminator rejected");
    transport.inject_received_line("OK id=1");
    expect(transport.read_line(line) == CdcTransportError::None && line == "OK id=1",
           "reads injected line");
    transport.close();
    expect(!transport.is_open(), "closes memory transport");
}

#if !defined(_WIN32)
void test_posix_transport() {
    using namespace picosd::host;
    const int controller = ::posix_openpt(O_RDWR | O_NOCTTY);
    expect(controller >= 0, "creates pseudo-terminal controller");
    if (controller < 0) return;
    expect(::grantpt(controller) == 0 && ::unlockpt(controller) == 0,
           "prepares pseudo-terminal peer");
    const char* endpoint = ::ptsname(controller);
    expect(endpoint != nullptr, "gets pseudo-terminal endpoint");
    if (endpoint == nullptr) {
        ::close(controller);
        return;
    }

    PosixCdcTransport transport;
    expect(transport.open(endpoint) == CdcTransportError::None,
           "opens POSIX CDC endpoint");
    std::string line;
    expect(transport.read_line(line) == CdcTransportError::WouldBlock,
           "idle POSIX endpoint would block");

    expect(::write(controller, "OK id=", 6) == 6, "writes first fragmented portion");
    expect(transport.read_line(line) == CdcTransportError::WouldBlock,
           "fragment remains incomplete");
    expect(::write(controller, "7\r\nNEXT id=8\n", 13) == 13,
           "writes final fragment and coalesced line");
    expect(transport.read_line(line) == CdcTransportError::None && line == "OK id=7",
           "assembles fragmented CRLF line");
    expect(transport.read_line(line) == CdcTransportError::None && line == "NEXT id=8",
           "retains coalesced line");

    expect(transport.write_line("HELLO id=9") == CdcTransportError::None,
           "writes complete POSIX line");
    char response[32]{};
    const auto count = ::read(controller, response, sizeof(response));
    expect(count == 11 && std::string(response, static_cast<std::size_t>(count)) == "HELLO id=9\n",
           "POSIX peer receives newline-terminated line");
    expect(::close(controller) == 0, "closes pseudo-terminal peer");
    expect(transport.read_line(line) == CdcTransportError::NotOpen,
           "reports disconnected POSIX peer");
    transport.close();
}

void test_posix_oversized_input() {
    using namespace picosd::host;
    const int controller = ::posix_openpt(O_RDWR | O_NOCTTY);
    expect(controller >= 0, "creates oversized-input pseudo-terminal");
    if (controller < 0) return;
    expect(::grantpt(controller) == 0 && ::unlockpt(controller) == 0,
           "prepares oversized-input pseudo-terminal");
    const char* endpoint = ::ptsname(controller);
    expect(endpoint != nullptr, "gets oversized-input endpoint");
    if (endpoint == nullptr) {
        ::close(controller);
        return;
    }

    PosixCdcTransport transport;
    expect(transport.open(endpoint) == CdcTransportError::None,
           "opens oversized-input endpoint");
    const std::string oversized_line(kMaximumCdcLineLength + 1, 'X');
    std::string line;
    CdcTransportError result = CdcTransportError::WouldBlock;
    std::size_t offset = 0;
    while (offset < oversized_line.size() && result == CdcTransportError::WouldBlock) {
        const auto chunk = std::min<std::size_t>(256, oversized_line.size() - offset);
        const auto count = ::write(controller, oversized_line.data() + offset, chunk);
        expect(count == static_cast<ssize_t>(chunk),
               "writes oversized unterminated line fragment");
        if (count <= 0) break;
        offset += static_cast<std::size_t>(count);
        result = transport.read_line(line);
    }
    expect(result == CdcTransportError::LineTooLong,
           "rejects oversized unterminated line");
    expect(transport.is_open(), "keeps endpoint open while discarding oversized line");
    expect(::write(controller, "discarded\nOK id=10\n", 19) == 19,
           "terminates oversized line and sends a valid successor");
    result = CdcTransportError::WouldBlock;
    for (std::size_t attempt = 0;
         attempt < 4 && result == CdcTransportError::WouldBlock;
         ++attempt) {
        result = transport.read_line(line);
    }
    expect(result == CdcTransportError::None && line == "OK id=10",
           "resynchronizes after oversized input");
    transport.close();
    ::close(controller);
}
#endif
}  // namespace

int main() {
    test_memory_transport();
#if !defined(_WIN32)
    test_posix_transport();
    test_posix_oversized_input();
#endif
    return failures == 0 ? 0 : 1;
}
