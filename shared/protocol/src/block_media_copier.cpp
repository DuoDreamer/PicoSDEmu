#include "picosd/protocol/block_media_copier.hpp"

namespace picosd::protocol {
namespace {

class HostCopyBackend final : public BlockBackend {
  public:
    explicit HostCopyBackend(BlockBackendArbiter &arbiter) : arbiter_(arbiter) {}

    std::size_t block_count() const override {
        return arbiter_.host_copy_block_count();
    }
    bool media_present() const override {
        return arbiter_.host_copy_media_present();
    }
    bool write_protected() const override {
        return arbiter_.host_copy_write_protected();
    }
    BlockOperationResult flush() override {
        return arbiter_.host_copy_flush();
    }
    BlockOperationResult read(std::size_t lba, SdBlock &output) const override {
        return arbiter_.host_copy_read(lba, output);
    }
    BlockOperationResult write(std::size_t lba, const SdBlock &input) override {
        return arbiter_.host_copy_write(lba, input);
    }

  private:
    BlockBackendArbiter &arbiter_;
};

class HostCopyOwnership final {
  public:
    explicit HostCopyOwnership(BlockBackendArbiter &arbiter)
        : arbiter_(arbiter), acquired_(arbiter_.begin_host_copy()) {}
    ~HostCopyOwnership() {
        if (acquired_)
            (void)arbiter_.end_host_copy();
    }
    [[nodiscard]] bool acquired() const {
        return acquired_;
    }

  private:
    BlockBackendArbiter &arbiter_;
    bool acquired_;
};

bool cancelled(const BlockCopyObserver *observer) {
    return observer != nullptr && observer->cancellation_requested();
}

void report(BlockCopyObserver *observer, std::size_t completed, std::size_t total,
            std::size_t verified = 0, bool verifying = false) {
    if (observer != nullptr) {
        observer->progress({completed, total, verified, verifying});
    }
}

} // namespace

BlockCopyResult copy_block_media(BlockBackend &source, BlockBackend &destination, bool verify,
                                 BlockCopyObserver *observer) {
    // Capacity and media-status queries can touch removable hardware too. An
    // operation cancelled before it starts must therefore avoid even the
    // preflight queries, and cancellation that arrives during one query must
    // stop before the next backend is consulted.
    if (cancelled(observer))
        return BlockCopyResult::Cancelled;
    const bool source_present = source.media_present();
    if (cancelled(observer))
        return BlockCopyResult::Cancelled;
    if (!source_present)
        return BlockCopyResult::SourceUnavailable;
    const bool destination_present = destination.media_present();
    if (cancelled(observer))
        return BlockCopyResult::Cancelled;
    if (!destination_present)
        return BlockCopyResult::DestinationUnavailable;
    const bool destination_read_only = destination.write_protected();
    if (cancelled(observer))
        return BlockCopyResult::Cancelled;
    if (destination_read_only)
        return BlockCopyResult::DestinationWriteProtected;

    const auto blocks = source.block_count();
    if (cancelled(observer))
        return BlockCopyResult::Cancelled;
    const auto destination_blocks = destination.block_count();
    if (cancelled(observer))
        return BlockCopyResult::Cancelled;
    if (destination_blocks < blocks)
        return BlockCopyResult::DestinationTooSmall;

    report(observer, 0, blocks);
    SdBlock sector{};
    for (std::size_t lba = 0; lba < blocks; ++lba) {
        if (cancelled(observer))
            return BlockCopyResult::Cancelled;
        const auto read_result = source.read(lba, sector);
        // Reading the source can block long enough for an asynchronous
        // cancellation request to arrive. Recheck before changing the
        // destination so cancellation never starts a write unnecessarily.
        // Cancellation also takes precedence over a failure returned by the
        // operation: once the request is observed, callers must not receive a
        // stale device error for an operation they abandoned.
        if (cancelled(observer))
            return BlockCopyResult::Cancelled;
        if (read_result != BlockOperationResult::Complete)
            return BlockCopyResult::ReadFailed;
        const auto write_result = destination.write(lba, sector);
        if (cancelled(observer))
            return BlockCopyResult::Cancelled;
        if (write_result != BlockOperationResult::Complete)
            return BlockCopyResult::WriteFailed;
        report(observer, lba + 1, blocks);
    }

    // The final sector boundary is still a cancellation boundary. Without
    // this check, an observer that reacts to the completed-copy progress
    // notification cannot stop before the potentially blocking flush.
    if (cancelled(observer))
        return BlockCopyResult::Cancelled;
    const auto flush_result = destination.flush();
    // Flushing can block while buffered writes reach removable media. Honor a
    // cancellation that arrived during that wait before reporting success or
    // beginning verification reads.
    if (cancelled(observer))
        return BlockCopyResult::Cancelled;
    if (flush_result != BlockOperationResult::Complete)
        return BlockCopyResult::FlushFailed;
    if (!verify)
        return BlockCopyResult::Complete;

    report(observer, blocks, blocks, 0, true);
    SdBlock expected{};
    SdBlock actual{};
    for (std::size_t lba = 0; lba < blocks; ++lba) {
        if (cancelled(observer))
            return BlockCopyResult::Cancelled;
        const auto source_read_result = source.read(lba, expected);
        // Verification reads can block just like copy reads. Avoid starting a
        // second device operation when cancellation arrived during the first.
        if (cancelled(observer))
            return BlockCopyResult::Cancelled;
        if (source_read_result != BlockOperationResult::Complete)
            return BlockCopyResult::ReadFailed;
        const auto destination_read_result = destination.read(lba, actual);
        if (cancelled(observer))
            return BlockCopyResult::Cancelled;
        if (destination_read_result != BlockOperationResult::Complete)
            return BlockCopyResult::ReadFailed;
        if (expected != actual)
            return BlockCopyResult::VerificationFailed;
        report(observer, blocks, blocks, lba + 1, true);
    }
    // As with the copy phase, let an observer react to the final progress
    // notification before the operation reports success.
    if (cancelled(observer))
        return BlockCopyResult::Cancelled;
    return BlockCopyResult::Complete;
}

BlockCopyResult copy_to_exclusive_backend(BlockBackend &source, BlockBackendArbiter &destination,
                                          bool destination_confirmed, bool verify,
                                          BlockCopyObserver *observer) {
    if (!destination_confirmed)
        return BlockCopyResult::ConfirmationRequired;
    // A caller can cancel while waiting to launch a confirmed copy. Avoid
    // disturbing the physical backend's ownership state when no work should
    // begin.
    if (cancelled(observer))
        return BlockCopyResult::Cancelled;
    HostCopyOwnership ownership{destination};
    if (cancelled(observer))
        return BlockCopyResult::Cancelled;
    if (!ownership.acquired())
        return BlockCopyResult::OwnershipUnavailable;
    HostCopyBackend physical{destination};
    return copy_block_media(source, physical, verify, observer);
}

BlockCopyResult copy_from_exclusive_backend(BlockBackendArbiter &source, BlockBackend &destination,
                                            bool destination_confirmed, bool verify,
                                            BlockCopyObserver *observer) {
    if (!destination_confirmed)
        return BlockCopyResult::ConfirmationRequired;
    if (cancelled(observer))
        return BlockCopyResult::Cancelled;
    HostCopyOwnership ownership{source};
    if (cancelled(observer))
        return BlockCopyResult::Cancelled;
    if (!ownership.acquired())
        return BlockCopyResult::OwnershipUnavailable;
    HostCopyBackend physical{source};
    return copy_block_media(physical, destination, verify, observer);
}

} // namespace picosd::protocol
