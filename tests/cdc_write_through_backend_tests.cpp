#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <string>

#include "picosd/protocol/cdc_write_through_backend.hpp"
#include "picosd/protocol/codec.hpp"
#include "picosd/protocol/crc.hpp"

namespace {
using picosd::protocol::CdcBlockData;
using picosd::protocol::CdcOperationState;
using Backend = picosd::protocol::CdcWriteThroughBackend<2>;

void expect(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void negotiate(Backend &backend) {
    const auto request = backend.begin_handshake();
    expect(request.error == picosd::protocol::CdcSessionClientError::None,
           "handshake starts");
    expect(backend.accept_handshake("OK id=1 version=0.1 session=test") ==
               picosd::protocol::CdcSessionClientError::None,
           "handshake completes");
}

std::string read_response(std::uint64_t id, std::uint64_t lba, const CdcBlockData &data) {
    char checksum[9]{};
    std::snprintf(checksum, sizeof(checksum), "%08X",
                  picosd::protocol::crc32(data.data(), data.size()));
    return "OK id=" + std::to_string(id) + " session=test lba=" + std::to_string(lba) +
           " count=1 encoding=BASE64 crc32=" + checksum + " data=" +
           picosd::protocol::encode_base64(data.data(), data.size());
}
} // namespace

int main() {
    Backend backend{100, 0, 1, 1};
    expect(backend.begin_read(4, 1, 0).error ==
               picosd::protocol::CdcSessionClientError::Busy,
           "I/O is rejected before negotiation");
    negotiate(backend);

    CdcBlockData source{};
    for (std::size_t index = 0; index < source.size(); ++index)
        source[index] = static_cast<std::uint8_t>(index);
    const auto read = backend.begin_read(4, 7, 10);
    expect(read.error == picosd::protocol::CdcSessionClientError::None,
           "read request starts");
    expect(backend.available_buffers() == 1, "read reserves one fixed buffer");
    expect(backend.accept_response(read_response(2, 5, source), 11) ==
               CdcOperationState::AwaitingResponse,
           "wrong LBA cannot complete a read");
    expect(backend.accept_response(read_response(2, 4, source), 12) == CdcOperationState::Idle,
           "matching read response completes");
    CdcBlockData output{};
    expect(backend.copy_ready(4, 7, output) && output == source,
           "completed read transfers buffered bytes");

    source[0] = 0xa5;
    const auto write = backend.begin_write(9, 7, source, 20);
    expect(write.error == picosd::protocol::CdcSessionClientError::None,
           "write request starts");
    expect(!backend.copy_ready(9, 7, output), "write is not ready before host acknowledgement");
    expect(backend.accept_response("OK id=3 session=test", 21) == CdcOperationState::Idle,
           "host acknowledgement completes write-through operation");
    expect(backend.copy_ready(9, 7, output) && output == source,
           "acknowledged write retains its sector until consumed");

    const auto pending = backend.begin_read(10, 8, 30);
    expect(pending.error == picosd::protocol::CdcSessionClientError::None,
           "second read starts");
    backend.disconnect();
    expect(!backend.negotiated() && !backend.transfer_active() &&
               backend.available_buffers() == 2,
           "disconnect cancels I/O and invalidates every buffer");
    expect(!backend.copy_ready(10, 8, output), "disconnected read cannot become visible");

    std::cout << "cdc write-through backend tests passed\n";
    return 0;
}
