#include <cstdio>
#include <cstdlib>
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
    expect(request.error == picosd::protocol::CdcSessionClientError::None, "handshake starts");
    expect(backend.accept_handshake("OK id=1 version=0.1 session=test") ==
               picosd::protocol::CdcSessionClientError::None,
           "handshake completes");
}

std::string read_response(std::uint64_t id, std::uint64_t lba, const CdcBlockData &data) {
    char checksum[9]{};
    std::snprintf(checksum, sizeof(checksum), "%08X",
                  picosd::protocol::crc32(data.data(), data.size()));
    return "OK id=" + std::to_string(id) + " session=test lba=" + std::to_string(lba) +
           " count=1 encoding=BASE64 crc32=" + checksum +
           " data=" + picosd::protocol::encode_base64(data.data(), data.size());
}
} // namespace

int main() {
    Backend backend{100, 0, 1, 1};
    expect(backend.begin_read(4, 1, 0).error == picosd::protocol::CdcSessionClientError::Busy,
           "I/O is rejected before negotiation");
    negotiate(backend);

    CdcBlockData source{};
    for (std::size_t index = 0; index < source.size(); ++index)
        source[index] = static_cast<std::uint8_t>(index);
    const auto read = backend.begin_read(4, 7, 10);
    expect(read.error == picosd::protocol::CdcSessionClientError::None, "read request starts");
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
    expect(write.error == picosd::protocol::CdcSessionClientError::None, "write request starts");
    expect(!backend.copy_ready(9, 7, output), "write is not ready before host acknowledgement");
    expect(backend.accept_response("OK id=3 session=test", 21) == CdcOperationState::Idle,
           "host acknowledgement completes write-through operation");
    expect(backend.copy_ready(9, 7, output) && output == source,
           "acknowledged write retains its sector until consumed");
    const auto &completed_statistics = backend.statistics();
    expect(completed_statistics.read_requests == 1 && completed_statistics.write_requests == 1 &&
               completed_statistics.completed_reads == 1 &&
               completed_statistics.completed_writes == 1 &&
               completed_statistics.sectors_delivered == 2,
           "statistics count requested, completed, and delivered sectors");
    expect(completed_statistics.total_latency == 3 && completed_statistics.maximum_latency == 2 &&
               completed_statistics.protocol_faults == 1,
           "statistics record transfer latency and malformed responses");
    expect(completed_statistics.completed_transfers() == 2 &&
               completed_statistics.average_latency() == 1 &&
               completed_statistics.latency_buckets[0] == 1 &&
               completed_statistics.latency_buckets[1] == 1,
           "statistics summarize completed transfer latency distribution");

    const auto cached = backend.begin_read(11, 7, 22);
    expect(cached.error == picosd::protocol::CdcSessionClientError::None,
           "same media generation remains available");
    expect(backend.accept_response(read_response(4, 11, source), 23) == CdcOperationState::Idle,
           "read for current generation completes");
    backend.media_changed(8);
    expect(backend.has_media_generation() && backend.media_generation() == 8,
           "media change records the new generation");
    expect(!backend.copy_ready(11, 7, output),
           "media change invalidates ready data from the old generation");
    expect(backend.begin_read(12, 7, 24).error == picosd::protocol::CdcSessionClientError::Busy,
           "old-generation work is rejected after a media change");

    const auto pending = backend.begin_read(10, 8, 30);
    expect(pending.error == picosd::protocol::CdcSessionClientError::None, "second read starts");
    backend.disconnect();
    expect(!backend.negotiated() && !backend.transfer_active() && backend.available_buffers() == 2,
           "disconnect cancels I/O and invalidates every buffer");
    expect(!backend.copy_ready(10, 8, output), "disconnected read cannot become visible");

    backend.invalidate();
    expect(!backend.has_media_generation(), "full invalidation forgets media identity");

    backend.reset_statistics();
    expect(backend.statistics().read_requests == 0 && backend.statistics().timeouts == 0,
           "statistics can be reset without changing backend state");
    expect(backend.statistics().completed_transfers() == 0 &&
               backend.statistics().average_latency() == 0,
           "empty statistics have a safe zero average");
    negotiate(backend);
    const auto timed_out = backend.begin_read(3, 9, 100);
    expect(timed_out.error == picosd::protocol::CdcSessionClientError::None,
           "read starts for timeout accounting");
    expect(backend.expire_if_due(200) == CdcOperationState::Failed &&
               backend.statistics().timeouts == 1 && !backend.transfer_active(),
           "terminal request timeout is counted and releases its transfer");

    Backend retrying{100, 1, 5, 5};
    negotiate(retrying);
    expect(retrying.begin_read(4, 1, 0).error ==
               picosd::protocol::CdcSessionClientError::None,
           "retry statistics operation starts");
    expect(retrying.expire_if_due(100) == CdcOperationState::WaitingRetry &&
               retrying.statistics().timeouts == 1,
           "retryable timeout is counted");
    expect(retrying.poll(105) == CdcOperationState::RetryReady &&
               retrying.issue_retry(105).error ==
                   picosd::protocol::CdcSessionClientError::None &&
               retrying.statistics().retries == 1,
           "issued retry is counted");

    Backend slow{200, 0, 1, 1};
    negotiate(slow);
    const auto slow_read = slow.begin_read(6, 1, 0);
    expect(slow_read.error == picosd::protocol::CdcSessionClientError::None &&
               slow.accept_response(read_response(2, 6, source), 150) == CdcOperationState::Idle &&
               slow.statistics().latency_buckets.back() == 1,
           "latencies above every bound use the overflow bucket");

    std::cout << "cdc write-through backend tests passed\n";
    return 0;
}
