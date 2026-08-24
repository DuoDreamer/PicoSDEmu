#include "picosd/protocol/block_media_copier.hpp"

namespace picosd::protocol {
namespace {

bool cancelled(const BlockCopyObserver *observer) {
    return observer != nullptr && observer->cancellation_requested();
}

void report(BlockCopyObserver *observer, std::size_t completed, std::size_t total) {
    if (observer != nullptr) {
        observer->progress({completed, total});
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

    if (destination.flush() != BlockOperationResult::Complete)
        return BlockCopyResult::FlushFailed;
    if (!verify)
        return BlockCopyResult::Complete;

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
    }
    return BlockCopyResult::Complete;
}

} // namespace picosd::protocol
