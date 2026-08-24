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
    if (!source.media_present())
        return BlockCopyResult::SourceUnavailable;
    if (!destination.media_present())
        return BlockCopyResult::DestinationUnavailable;
    if (destination.write_protected())
        return BlockCopyResult::DestinationWriteProtected;

    const auto blocks = source.block_count();
    if (destination.block_count() < blocks)
        return BlockCopyResult::DestinationTooSmall;

    report(observer, 0, blocks);
    SdBlock sector{};
    for (std::size_t lba = 0; lba < blocks; ++lba) {
        if (cancelled(observer))
            return BlockCopyResult::Cancelled;
        if (source.read(lba, sector) != BlockOperationResult::Complete)
            return BlockCopyResult::ReadFailed;
        if (destination.write(lba, sector) != BlockOperationResult::Complete)
            return BlockCopyResult::WriteFailed;
        report(observer, lba + 1, blocks);
    }

    // The final sector boundary is still a cancellation boundary. Without
    // this check, an observer that reacts to the completed-copy progress
    // notification cannot stop before the potentially blocking flush.
    if (cancelled(observer))
        return BlockCopyResult::Cancelled;
    if (destination.flush() != BlockOperationResult::Complete)
        return BlockCopyResult::FlushFailed;
    if (!verify)
        return BlockCopyResult::Complete;

    report(observer, blocks, blocks, 0, true);
    SdBlock expected{};
    SdBlock actual{};
    for (std::size_t lba = 0; lba < blocks; ++lba) {
        if (cancelled(observer))
            return BlockCopyResult::Cancelled;
        if (source.read(lba, expected) != BlockOperationResult::Complete ||
            destination.read(lba, actual) != BlockOperationResult::Complete)
            return BlockCopyResult::ReadFailed;
        if (expected != actual)
            return BlockCopyResult::VerificationFailed;
        report(observer, blocks, blocks, lba + 1, true);
    }
    return BlockCopyResult::Complete;
}

BlockCopyResult copy_to_exclusive_backend(BlockBackend &source, BlockBackendArbiter &destination,
                                          bool destination_confirmed, bool verify,
                                          BlockCopyObserver *observer) {
    if (!destination_confirmed)
        return BlockCopyResult::ConfirmationRequired;
    HostCopyOwnership ownership{destination};
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
    HostCopyOwnership ownership{source};
    if (!ownership.acquired())
        return BlockCopyResult::OwnershipUnavailable;
    HostCopyBackend physical{source};
    return copy_block_media(physical, destination, verify, observer);
}

} // namespace picosd::protocol
