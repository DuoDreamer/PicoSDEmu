#pragma once

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
        retain_transfer(CdcBackendTransferType::Read, handle, lba, generation);
        return request;
    }

    [[nodiscard]] CdcSessionClientRequest begin_write(std::uint64_t lba, std::uint64_t generation,
                                                      const CdcBlockData &data, std::uint64_t now) {
        if (!ready_for_transfer())
            return {CdcSessionClientError::Busy, {}};
        if (!select_generation(generation))
            return {CdcSessionClientError::Busy, {}};
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
        retain_transfer(CdcBackendTransferType::Write, handle, lba, generation);
        return request;
    }

    [[nodiscard]] CdcOperationState accept_response(std::string_view response, std::uint64_t now) {
        CdcReadBlock decoded;
        const bool valid_read =
            transfer_type_ != CdcBackendTransferType::Read || response.rfind("ERR ", 0) == 0 ||
            (decode_read_block_response(response, decoded) == CdcBlockResponseError::None &&
             decoded.lba == lba_);
        if (!valid_read)
            return coordinator_.state();

        const auto state = coordinator_.accept_response(response, now, client_);
        if (state == CdcOperationState::Idle) {
            const bool completed =
                transfer_type_ == CdcBackendTransferType::Read
                    ? buffers_.complete_read(handle_, lba_, generation_, decoded.data)
                    : buffers_.complete_write(handle_, lba_, generation_);
            if (!completed) {
                abandon_transfer();
                return CdcOperationState::Failed;
            }
            clear_transfer_tracking();
        } else if (state == CdcOperationState::Failed) {
            abandon_transfer();
        }
        return state;
    }

    [[nodiscard]] CdcOperationState expire_if_due(std::uint64_t now) {
        const auto state = coordinator_.expire_if_due(now, client_);
        if (state == CdcOperationState::Failed)
            abandon_transfer();
        return state;
    }

    [[nodiscard]] CdcOperationState poll(std::uint64_t now) {
        return coordinator_.poll(now);
    }

    [[nodiscard]] CdcSessionClientRequest issue_retry(std::uint64_t now) {
        return coordinator_.issue_retry(now, client_);
    }

    [[nodiscard]] bool copy_ready(std::uint64_t lba, std::uint64_t generation,
                                  CdcBlockData &output) {
        const auto handle = buffers_.find_ready(lba, generation);
        const auto *slot = buffers_.get(handle);
        if (slot == nullptr)
            return false;
        output = slot->data;
        return buffers_.release(handle);
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
                         std::uint64_t generation) {
        transfer_type_ = type;
        handle_ = handle;
        lba_ = lba;
        generation_ = generation;
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
    bool has_generation_ = false;
};

} // namespace picosd::protocol
