#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "picosd/protocol/cdc_block_response.hpp"
#include "picosd/protocol/cdc_operation_coordinator.hpp"
#include "picosd/protocol/cdc_sector_buffer_pool.hpp"

namespace picosd::protocol {

enum class CdcBackendTransferType {
    None,
    Read,
    Write
};

struct CdcBackendStatistics {
    static constexpr std::array<std::uint64_t, 6> kLatencyBucketUpperBounds = {1,  2,  5,
                                                                               10, 50, 100};

    std::uint64_t read_requests = 0;
    std::uint64_t write_requests = 0;
    std::uint64_t completed_reads = 0;
    std::uint64_t completed_writes = 0;
    std::uint64_t completed_bytes = 0;
    std::uint64_t sectors_delivered = 0;
    std::uint64_t cache_hits = 0;
    std::uint64_t cache_misses = 0;
    std::uint64_t read_coalesces = 0;
    std::uint64_t prefetch_requests = 0;
    std::uint64_t prefetch_skips = 0;
    std::uint64_t timeouts = 0;
    std::uint64_t retries = 0;
    std::uint64_t protocol_faults = 0;
    std::uint64_t total_latency = 0;
    std::uint64_t maximum_latency = 0;
    std::uint64_t total_write_busy_duration = 0;
    std::uint64_t maximum_write_busy_duration = 0;
    // Each entry counts completions at or below the corresponding upper bound.
    // The final entry counts completions above every configured bound.
    std::array<std::uint64_t, kLatencyBucketUpperBounds.size() + 1> latency_buckets{};

    [[nodiscard]] std::uint64_t completed_transfers() const {
        return completed_reads + completed_writes;
    }

    [[nodiscard]] std::uint64_t average_latency() const {
        const auto completed = completed_transfers();
        return completed == 0 ? 0 : total_latency / completed;
    }

    // Time values use the caller-provided clock units, so this result is bytes
    // per clock tick. Callers can scale it to bytes per second when the clock
    // frequency is known without requiring floating point in firmware.
    [[nodiscard]] std::uint64_t average_throughput() const {
        return total_latency == 0 ? 0 : completed_bytes / total_latency;
    }

    // Integer permille avoids floating point in firmware while retaining more
    // useful precision than a whole percentage for small buffer pools.
    [[nodiscard]] std::uint64_t cache_hit_permille() const {
        const auto lookups = cache_hits + cache_misses;
        return lookups == 0 ? 0 : (cache_hits * 1000) / lookups;
    }
};

// Owns the fixed sector storage and the single in-flight CDC request used by
// the firmware backend. Writes are not made visible as complete until the host
// has returned OK, which enforces the project's write-through policy.
template <std::size_t Capacity> class CdcWriteThroughBackend {
  public:
    CdcWriteThroughBackend(std::uint64_t request_timeout, std::size_t maximum_retries,
                           std::uint64_t initial_backoff, std::uint64_t maximum_backoff)
        : coordinator_(request_timeout, maximum_retries, initial_backoff, maximum_backoff) {}

    [[nodiscard]] CdcSessionClientRequest begin_handshake() {
        return client_.begin_handshake();
    }

    [[nodiscard]] CdcSessionClientError accept_handshake(std::string_view response) {
        return client_.accept_response(response);
    }

    [[nodiscard]] CdcSessionClientRequest begin_read(std::uint64_t lba, std::uint64_t generation,
                                                     std::uint64_t now) {
        if (!ready_for_transfer())
            return {CdcSessionClientError::Busy, {}};
        if (!select_generation(generation))
            return {CdcSessionClientError::Busy, {}};
        // A ready sector already satisfies the foreground operation. Preserve
        // it in the LRU and let copy_ready() account for its eventual delivery.
        // An empty successful request tells the caller not to emit a line.
        if (buffers_.find_ready(lba, generation) != BufferPool::kInvalidHandle) {
            ++statistics_.read_coalesces;
            return {};
        }
        const auto handle = buffers_.reserve_read(lba, generation);
        if (handle == BufferPool::kInvalidHandle)
            return {CdcSessionClientError::Busy, {}};

        CdcRetainedOperation operation;
        operation.retain_read_block(lba);
        const auto request = coordinator_.start(operation, now, client_);
        if (request.error != CdcSessionClientError::None) {
            (void)buffers_.release(handle);
            return request;
        }
        retain_transfer(CdcBackendTransferType::Read, handle, lba, generation, now);
        ++statistics_.read_requests;
        return request;
    }

    // Starts a speculative single-sector read only when the sector is not
    // already cached. An empty successful request means that existing buffered
    // work safely coalesced the prefetch, so callers must not write a line to
    // the transport. Foreground writes retain priority because this method
    // reports Busy whenever another transfer is active.
    [[nodiscard]] CdcSessionClientRequest
    begin_prefetch(std::uint64_t lba, std::uint64_t generation, std::uint64_t now) {
        if (!ready_for_transfer())
            return {CdcSessionClientError::Busy, {}};
        if (!select_generation(generation))
            return {CdcSessionClientError::Busy, {}};
        if (buffers_.contains(lba, generation)) {
            ++statistics_.prefetch_skips;
            return {};
        }
        auto request = begin_read(lba, generation, now);
        if (request.error == CdcSessionClientError::None)
            ++statistics_.prefetch_requests;
        return request;
    }

    [[nodiscard]] CdcSessionClientRequest begin_sequential_prefetch(std::uint64_t delivered_lba,
                                                                    std::uint64_t generation,
                                                                    std::uint64_t now) {
        if (delivered_lba == UINT64_MAX)
            return {CdcSessionClientError::InvalidRequest, {}};
        return begin_prefetch(delivered_lba + 1, generation, now);
    }

    [[nodiscard]] CdcSessionClientRequest begin_write(std::uint64_t lba, std::uint64_t generation,
                                                      const CdcBlockData &data, std::uint64_t now) {
        if (!ready_for_transfer())
            return {CdcSessionClientError::Busy, {}};
        if (!select_generation(generation))
            return {CdcSessionClientError::Busy, {}};
        // Once a write is accepted for dispatch, an older cached version of
        // the same sector must no longer satisfy reads while the host write is
        // pending. The acknowledged write becomes the sole ready copy below.
        buffers_.invalidate_ready(lba, generation);
        const auto handle = buffers_.reserve_write(lba, generation, data);
        if (handle == BufferPool::kInvalidHandle)
            return {CdcSessionClientError::Busy, {}};

        CdcRetainedOperation operation;
        operation.retain_write_block(lba, data);
        const auto request = coordinator_.start(operation, now, client_);
        if (request.error != CdcSessionClientError::None) {
            (void)buffers_.release(handle);
            return request;
        }
        retain_transfer(CdcBackendTransferType::Write, handle, lba, generation, now);
        ++statistics_.write_requests;
        return request;
    }

    [[nodiscard]] CdcOperationState accept_response(std::string_view response, std::uint64_t now) {
        CdcReadBlock decoded;
        const bool valid_read =
            transfer_type_ != CdcBackendTransferType::Read || response.rfind("ERR ", 0) == 0 ||
            (decode_read_block_response(response, decoded) == CdcBlockResponseError::None &&
             decoded.lba == lba_);
        if (!valid_read) {
            ++statistics_.protocol_faults;
            return coordinator_.state();
        }

        const auto state = coordinator_.accept_response(response, now, client_);
        if (state == CdcOperationState::Idle) {
            const bool completed =
                transfer_type_ == CdcBackendTransferType::Read
                    ? buffers_.complete_read(handle_, lba_, generation_, decoded.data)
                    : buffers_.complete_write(handle_, lba_, generation_);
            if (!completed) {
                ++statistics_.protocol_faults;
                abandon_transfer();
                return CdcOperationState::Failed;
            }
            record_completion(now);
            clear_transfer_tracking();
        } else if (state == CdcOperationState::Failed) {
            ++statistics_.protocol_faults;
            record_write_busy_duration(now);
            abandon_transfer();
        }
        return state;
    }

    [[nodiscard]] CdcOperationState expire_if_due(std::uint64_t now) {
        const auto previous = coordinator_.state();
        const auto state = coordinator_.expire_if_due(now, client_);
        if (previous == CdcOperationState::AwaitingResponse &&
            state != CdcOperationState::AwaitingResponse)
            ++statistics_.timeouts;
        if (state == CdcOperationState::Failed) {
            record_write_busy_duration(now);
            abandon_transfer();
        }
        return state;
    }

    [[nodiscard]] CdcOperationState poll(std::uint64_t now) {
        return coordinator_.poll(now);
    }

    [[nodiscard]] CdcSessionClientRequest issue_retry(std::uint64_t now) {
        auto request = coordinator_.issue_retry(now, client_);
        if (request.error == CdcSessionClientError::None)
            ++statistics_.retries;
        return request;
    }

    [[nodiscard]] bool copy_ready(std::uint64_t lba, std::uint64_t generation,
                                  CdcBlockData &output) {
        if (!buffers_.copy_ready(lba, generation, output)) {
            ++statistics_.cache_misses;
            return false;
        }
        ++statistics_.cache_hits;
        ++statistics_.sectors_delivered;
        return true;
    }

    void disconnect() {
        coordinator_.disconnect(client_, CdcDisconnectPolicy::CancelOperation);
        buffers_.clear();
        clear_transfer_tracking();
        has_generation_ = false;
        media_generation_ = 0;
    }

    void invalidate() {
        coordinator_.cancel(client_);
        buffers_.clear();
        clear_transfer_tracking();
        has_generation_ = false;
        media_generation_ = 0;
    }

    // A media generation identifies one stable mounted image. Changing it is
    // an invalidation barrier: no response or buffered sector from the old
    // image may become visible after this call. The CDC session remains
    // negotiated because a host-side remount does not imply a USB reconnect.
    void media_changed(std::uint64_t generation) {
        if (has_generation_ && generation == media_generation_)
            return;
        coordinator_.cancel(client_);
        buffers_.clear();
        clear_transfer_tracking();
        media_generation_ = generation;
        has_generation_ = true;
    }

    [[nodiscard]] bool has_media_generation() const {
        return has_generation_;
    }
    [[nodiscard]] std::uint64_t media_generation() const {
        return media_generation_;
    }

    [[nodiscard]] bool negotiated() const {
        return client_.negotiated();
    }
    [[nodiscard]] bool transfer_active() const {
        return transfer_type_ != CdcBackendTransferType::None;
    }
    [[nodiscard]] std::size_t available_buffers() const {
        return buffers_.available();
    }
    [[nodiscard]] CdcOperationState state() const {
        return coordinator_.state();
    }
    [[nodiscard]] const CdcBackendStatistics &statistics() const {
        return statistics_;
    }
    void reset_statistics() {
        statistics_ = {};
    }

  private:
    using BufferPool = CdcSectorBufferPool<Capacity>;

    [[nodiscard]] bool ready_for_transfer() const {
        return client_.negotiated() && !transfer_active() &&
               coordinator_.state() == CdcOperationState::Idle;
    }

    [[nodiscard]] bool select_generation(std::uint64_t generation) {
        if (!has_generation_) {
            media_generation_ = generation;
            has_generation_ = true;
            return true;
        }
        return media_generation_ == generation;
    }

    void retain_transfer(CdcBackendTransferType type, std::size_t handle, std::uint64_t lba,
                         std::uint64_t generation, std::uint64_t now) {
        transfer_type_ = type;
        handle_ = handle;
        lba_ = lba;
        generation_ = generation;
        transfer_started_at_ = now;
    }

    void record_completion(std::uint64_t now) {
        if (transfer_type_ == CdcBackendTransferType::Read)
            ++statistics_.completed_reads;
        else if (transfer_type_ == CdcBackendTransferType::Write)
            ++statistics_.completed_writes;
        statistics_.completed_bytes += CdcBlockData{}.size();
        const auto latency = now >= transfer_started_at_ ? now - transfer_started_at_ : 0;
        record_write_busy_duration(now);
        statistics_.total_latency += latency;
        if (latency > statistics_.maximum_latency)
            statistics_.maximum_latency = latency;
        std::size_t bucket = 0;
        while (bucket < CdcBackendStatistics::kLatencyBucketUpperBounds.size() &&
               latency > CdcBackendStatistics::kLatencyBucketUpperBounds[bucket])
            ++bucket;
        ++statistics_.latency_buckets[bucket];
    }

    void record_write_busy_duration(std::uint64_t now) {
        if (transfer_type_ != CdcBackendTransferType::Write)
            return;
        const auto duration = now >= transfer_started_at_ ? now - transfer_started_at_ : 0;
        statistics_.total_write_busy_duration += duration;
        if (duration > statistics_.maximum_write_busy_duration)
            statistics_.maximum_write_busy_duration = duration;
    }

    void clear_transfer_tracking() {
        transfer_type_ = CdcBackendTransferType::None;
        handle_ = BufferPool::kInvalidHandle;
        lba_ = 0;
        generation_ = 0;
    }

    void abandon_transfer() {
        if (handle_ != BufferPool::kInvalidHandle)
            (void)buffers_.release(handle_);
        clear_transfer_tracking();
    }

    CdcSessionClient client_;
    CdcOperationCoordinator coordinator_;
    BufferPool buffers_;
    CdcBackendTransferType transfer_type_ = CdcBackendTransferType::None;
    std::size_t handle_ = BufferPool::kInvalidHandle;
    std::uint64_t lba_ = 0;
    std::uint64_t generation_ = 0;
    std::uint64_t media_generation_ = 0;
    std::uint64_t transfer_started_at_ = 0;
    bool has_generation_ = false;
    CdcBackendStatistics statistics_{};
};

} // namespace picosd::protocol
