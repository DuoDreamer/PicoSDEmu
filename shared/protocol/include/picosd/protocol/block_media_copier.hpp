#pragma once

#include <cstddef>

#include "picosd/protocol/block_backend_arbiter.hpp"
#include "picosd/protocol/sd_card_state.hpp"

namespace picosd::protocol {

enum class BlockCopyResult {
    Complete,
    Cancelled,
    OwnershipUnavailable,
    SourceUnavailable,
    DestinationUnavailable,
    DestinationWriteProtected,
    DestinationTooSmall,
    ReadFailed,
    WriteFailed,
    FlushFailed,
    VerificationFailed,
};

struct BlockCopyProgress {
    std::size_t completed_blocks = 0;
    std::size_t total_blocks = 0;
};

// Receives progress only at sector boundaries. Returning true from
// cancellation_requested stops before the next sector is read or written.
class BlockCopyObserver {
  public:
    virtual ~BlockCopyObserver() = default;
    [[nodiscard]] virtual bool cancellation_requested() const = 0;
    virtual void progress(BlockCopyProgress value) = 0;
};

// Copies every source sector, flushes the destination, and optionally reads
// every destination sector back for byte-for-byte verification. The caller
// must hold exclusive ownership of both backends for the duration of the call.
[[nodiscard]] BlockCopyResult copy_block_media(BlockBackend &source, BlockBackend &destination,
                                               bool verify, BlockCopyObserver *observer = nullptr);

// Acquires the physical backend's host-copy ownership for the complete operation
// and releases it on every result path. These entry points are the safe boundary
// used by the two directional host copy commands: the other backend must already
// be exclusively owned by the caller (for example, a locked image file).
[[nodiscard]] BlockCopyResult copy_to_exclusive_backend(
    BlockBackend &source, BlockBackendArbiter &destination, bool verify,
    BlockCopyObserver *observer = nullptr);
[[nodiscard]] BlockCopyResult copy_from_exclusive_backend(
    BlockBackendArbiter &source, BlockBackend &destination, bool verify,
    BlockCopyObserver *observer = nullptr);

} // namespace picosd::protocol
