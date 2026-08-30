#include <cstdlib>
#include <iostream>

#include "picosd/protocol/block_media_copier.hpp"

namespace {

using namespace picosd::protocol;

void expect(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

class Observer final : public BlockCopyObserver {
  public:
    explicit Observer(std::size_t cancel_after = static_cast<std::size_t>(-1))
        : cancel_after_(cancel_after) {}

    bool cancellation_requested() const override {
        return last_.completed_blocks >= cancel_after_;
    }
    void progress(BlockCopyProgress value) override {
        last_ = value;
        ++reports_;
    }

    BlockCopyProgress last_{};
    std::size_t reports_ = 0;

  private:
    std::size_t cancel_after_;
};

class ConfigurableBackend final : public BlockBackend {
  public:
    explicit ConfigurableBackend(std::size_t blocks) : storage_(blocks) {}
    std::size_t block_count() const override {
        ++block_count_queries;
        return storage_.block_count();
    }
    bool media_present() const override {
        ++media_queries;
        return present;
    }
    bool write_protected() const override {
        ++write_protect_queries;
        return readonly;
    }
    BlockOperationResult flush() override {
        ++flushes;
        return fail_flush ? BlockOperationResult::Failed : BlockOperationResult::Complete;
    }
    BlockOperationResult read(std::size_t lba, SdBlock &output) const override {
        ++reads;
        if (lba == fail_read_lba)
            return BlockOperationResult::Failed;
        return storage_.read(lba, output);
    }
    BlockOperationResult write(std::size_t lba, const SdBlock &input) override {
        ++writes;
        if (lba == fail_write_lba)
            return BlockOperationResult::Failed;
        return storage_.write(lba, input);
    }

    RamBlockBackend storage_;
    bool present = true;
    bool readonly = false;
    bool fail_flush = false;
    std::size_t fail_read_lba = static_cast<std::size_t>(-1);
    std::size_t fail_write_lba = static_cast<std::size_t>(-1);
    std::size_t flushes = 0;
    std::size_t writes = 0;
    mutable std::size_t reads = 0;
    mutable std::size_t block_count_queries = 0;
    mutable std::size_t media_queries = 0;
    mutable std::size_t write_protect_queries = 0;
};

class ReadTriggeredCancellation final : public BlockCopyObserver {
  public:
    explicit ReadTriggeredCancellation(bool &source_read) : source_read_(source_read) {}

    bool cancellation_requested() const override {
        return source_read_;
    }
    void progress(BlockCopyProgress) override {}

  private:
    bool &source_read_;
};

class CancellationSource final : public BlockBackend {
  public:
    CancellationSource(BlockBackend &backend, bool &source_read)
        : backend_(backend), source_read_(source_read) {}

    std::size_t block_count() const override {
        return backend_.block_count();
    }
    bool media_present() const override {
        return backend_.media_present();
    }
    bool write_protected() const override {
        return backend_.write_protected();
    }
    BlockOperationResult flush() override {
        return backend_.flush();
    }
    BlockOperationResult read(std::size_t lba, SdBlock &output) const override {
        const auto result = backend_.read(lba, output);
        source_read_ = true;
        return result;
    }
    BlockOperationResult write(std::size_t lba, const SdBlock &input) override {
        return backend_.write(lba, input);
    }

  private:
    BlockBackend &backend_;
    bool &source_read_;
};

class CounterCancellation final : public BlockCopyObserver {
  public:
    explicit CounterCancellation(const std::size_t &counter, std::size_t threshold = 1)
        : counter_(counter), threshold_(threshold) {}

    bool cancellation_requested() const override {
        return counter_ >= threshold_;
    }
    void progress(BlockCopyProgress) override {}

  private:
    const std::size_t &counter_;
    std::size_t threshold_;
};

} // namespace

int main() {
    ConfigurableBackend source{3};
    ConfigurableBackend destination{4};
    SdBlock block{};
    for (std::size_t lba = 0; lba < 3; ++lba) {
        block.fill(static_cast<std::uint8_t>(lba + 1));
        expect(source.write(lba, block) == BlockOperationResult::Complete, "seed source");
    }
    Observer cancellation_before_preflight{0};
    expect(copy_block_media(source, destination, false, &cancellation_before_preflight) ==
                   BlockCopyResult::Cancelled &&
               source.media_queries == 0 && source.block_count_queries == 0 &&
               destination.media_queries == 0 && destination.block_count_queries == 0 &&
               destination.write_protect_queries == 0,
           "cancellation before preflight avoids all backend queries");

    class PreflightCancellation final : public BlockCopyObserver {
      public:
        explicit PreflightCancellation(const ConfigurableBackend &source) : source_(source) {}
        bool cancellation_requested() const override {
            return source_.media_queries != 0;
        }
        void progress(BlockCopyProgress) override {}

      private:
        const ConfigurableBackend &source_;
    } cancellation_during_preflight{source};
    expect(copy_block_media(source, destination, false, &cancellation_during_preflight) ==
                   BlockCopyResult::Cancelled &&
               source.media_queries == 1 && destination.media_queries == 0,
           "cancellation during preflight stops before querying the destination");

    Observer observer;
    expect(copy_block_media(source, destination, true, &observer) == BlockCopyResult::Complete,
           "copy and verification complete");
    expect(observer.last_.completed_blocks == 3 && observer.last_.total_blocks == 3 &&
               observer.last_.verified_blocks == 3 && observer.last_.verifying &&
               observer.reports_ == 8 && destination.flushes == 1,
           "copy, verification, and flush progress are reported");
    for (std::size_t lba = 0; lba < 3; ++lba) {
        SdBlock copied{};
        expect(destination.read(lba, copied) == BlockOperationResult::Complete &&
                   copied.front() == lba + 1,
               "destination matches source");
    }

    ConfigurableBackend cancelled_destination{3};
    Observer cancellation{1};
    expect(copy_block_media(source, cancelled_destination, false, &cancellation) ==
               BlockCopyResult::Cancelled,
           "cancellation is honored at a block boundary");
    SdBlock untouched{};
    expect(cancelled_destination.read(1, untouched) == BlockOperationResult::Complete &&
               untouched.front() == 0 && cancelled_destination.flushes == 0,
           "cancellation does not start the next block or claim a flush");

    bool source_read = false;
    CancellationSource cancellation_source{source, source_read};
    ReadTriggeredCancellation cancellation_during_read{source_read};
    ConfigurableBackend read_cancel_destination{3};
    expect(copy_block_media(cancellation_source, read_cancel_destination, false,
                            &cancellation_during_read) == BlockCopyResult::Cancelled,
           "cancellation during a source read is honored before writing");
    expect(read_cancel_destination.read(0, untouched) == BlockOperationResult::Complete &&
               untouched.front() == 0 && read_cancel_destination.flushes == 0,
           "cancellation after reading leaves the destination unchanged");

    ConfigurableBackend write_cancel_destination{3};
    CounterCancellation cancellation_during_write{write_cancel_destination.writes};
    expect(copy_block_media(source, write_cancel_destination, false, &cancellation_during_write) ==
                   BlockCopyResult::Cancelled &&
               write_cancel_destination.writes == 1 && write_cancel_destination.flushes == 0,
           "cancellation during a destination write stops before progress or flush");

    ConfigurableBackend failed_read_cancellation_source{1};
    ConfigurableBackend failed_read_cancellation_destination{1};
    failed_read_cancellation_source.fail_read_lba = 0;
    CounterCancellation failed_read_cancellation{failed_read_cancellation_source.reads};
    expect(copy_block_media(failed_read_cancellation_source, failed_read_cancellation_destination,
                            false, &failed_read_cancellation) == BlockCopyResult::Cancelled &&
               failed_read_cancellation_destination.writes == 0,
           "cancellation requested during a failed read takes precedence over the error");

    ConfigurableBackend failed_write_cancellation_destination{1};
    failed_write_cancellation_destination.fail_write_lba = 0;
    CounterCancellation failed_write_cancellation{failed_write_cancellation_destination.writes};
    expect(copy_block_media(failed_read_cancellation_destination,
                            failed_write_cancellation_destination, false,
                            &failed_write_cancellation) == BlockCopyResult::Cancelled &&
               failed_write_cancellation_destination.flushes == 0,
           "cancellation requested during a failed write takes precedence over the error");

    ConfigurableBackend final_boundary_destination{3};
    Observer final_boundary_cancellation{3};
    expect(copy_block_media(source, final_boundary_destination, false,
                            &final_boundary_cancellation) == BlockCopyResult::Cancelled &&
               final_boundary_cancellation.last_.completed_blocks == 3 &&
               final_boundary_destination.flushes == 0,
           "cancellation at the final copied block stops before the flush");

    ConfigurableBackend flush_cancel_destination{3};
    const auto reads_before_flush_cancellation = source.reads;
    class FlushCancellation final : public BlockCopyObserver {
      public:
        explicit FlushCancellation(const ConfigurableBackend &destination)
            : destination_(destination) {}
        bool cancellation_requested() const override {
            return destination_.flushes != 0;
        }
        void progress(BlockCopyProgress value) override {
            progress_ = value;
        }
        BlockCopyProgress progress_{};

      private:
        const ConfigurableBackend &destination_;
    } flush_cancellation{flush_cancel_destination};
    expect(copy_block_media(source, flush_cancel_destination, true, &flush_cancellation) ==
                   BlockCopyResult::Cancelled &&
               flush_cancel_destination.flushes == 1 &&
               source.reads == reads_before_flush_cancellation + 3 &&
               flush_cancel_destination.reads == 0 &&
               flush_cancellation.progress_.completed_blocks == 3 &&
               !flush_cancellation.progress_.verifying,
           "cancellation during flush stops before verification");

    ConfigurableBackend failed_flush_cancellation_destination{3};
    failed_flush_cancellation_destination.fail_flush = true;
    CounterCancellation failed_flush_cancellation{failed_flush_cancellation_destination.flushes};
    expect(copy_block_media(source, failed_flush_cancellation_destination, false,
                            &failed_flush_cancellation) == BlockCopyResult::Cancelled,
           "cancellation requested during a failed flush takes precedence over the error");

    ConfigurableBackend verification_cancel_destination{3};
    class VerificationCancellation final : public BlockCopyObserver {
      public:
        bool cancellation_requested() const override {
            return progress_.verifying && progress_.verified_blocks == 1;
        }
        void progress(BlockCopyProgress value) override {
            progress_ = value;
        }
        BlockCopyProgress progress_{};
    } verification_cancellation;
    expect(copy_block_media(source, verification_cancel_destination, true,
                            &verification_cancellation) == BlockCopyResult::Cancelled &&
               verification_cancellation.progress_.completed_blocks == 3 &&
               verification_cancellation.progress_.verified_blocks == 1 &&
               verification_cancellation.progress_.verifying,
           "verification progress exposes a safe cancellation boundary");

    ConfigurableBackend verification_read_source{1};
    ConfigurableBackend verification_read_destination{1};
    block.fill(42);
    expect(verification_read_source.write(0, block) == BlockOperationResult::Complete,
           "seed verification cancellation source");
    class VerificationReadCancellation final : public BlockCopyObserver {
      public:
        explicit VerificationReadCancellation(const ConfigurableBackend &source)
            : source_(source) {}
        bool cancellation_requested() const override {
            return verifying_ && source_.reads == 2;
        }
        void progress(BlockCopyProgress value) override {
            verifying_ = value.verifying;
        }

      private:
        const ConfigurableBackend &source_;
        bool verifying_ = false;
    } verification_read_cancellation{verification_read_source};
    expect(copy_block_media(verification_read_source, verification_read_destination, true,
                            &verification_read_cancellation) == BlockCopyResult::Cancelled &&
               verification_read_source.reads == 2 && verification_read_destination.reads == 0,
           "cancellation during a verification source read skips the destination read");

    ConfigurableBackend destination_read_source{1};
    ConfigurableBackend destination_read_destination{1};
    expect(destination_read_source.write(0, block) == BlockOperationResult::Complete,
           "seed destination-read cancellation source");
    class DestinationReadCancellation final : public BlockCopyObserver {
      public:
        explicit DestinationReadCancellation(const ConfigurableBackend &destination)
            : destination_(destination) {}
        bool cancellation_requested() const override {
            return verifying_ && destination_.reads == 1;
        }
        void progress(BlockCopyProgress value) override {
            verifying_ = value.verifying;
        }

      private:
        const ConfigurableBackend &destination_;
        bool verifying_ = false;
    } destination_read_cancellation{destination_read_destination};
    expect(copy_block_media(destination_read_source, destination_read_destination, true,
                            &destination_read_cancellation) == BlockCopyResult::Cancelled &&
               destination_read_source.reads == 2 && destination_read_destination.reads == 1,
           "cancellation during a verification destination read is honored immediately");

    ConfigurableBackend final_verification_cancel_destination{3};
    class FinalVerificationCancellation final : public BlockCopyObserver {
      public:
        bool cancellation_requested() const override {
            return progress_.verifying && progress_.verified_blocks == progress_.total_blocks;
        }
        void progress(BlockCopyProgress value) override {
            progress_ = value;
        }
        BlockCopyProgress progress_{};
    } final_verification_cancellation;
    expect(copy_block_media(source, final_verification_cancel_destination, true,
                            &final_verification_cancellation) == BlockCopyResult::Cancelled &&
               final_verification_cancellation.progress_.completed_blocks == 3 &&
               final_verification_cancellation.progress_.verified_blocks == 3 &&
               final_verification_cancellation.progress_.verifying,
           "cancellation at the final verified block is honored before success");

    ConfigurableBackend small{2};
    expect(copy_block_media(source, small, false) == BlockCopyResult::DestinationTooSmall,
           "undersized destination is rejected before copying");
    destination.readonly = true;
    expect(copy_block_media(source, destination, false) ==
               BlockCopyResult::DestinationWriteProtected,
           "write-protected destination is rejected");
    destination.readonly = false;
    source.present = false;
    expect(copy_block_media(source, destination, false) == BlockCopyResult::SourceUnavailable,
           "missing source is rejected");
    source.present = true;
    destination.fail_flush = true;
    expect(copy_block_media(source, destination, false) == BlockCopyResult::FlushFailed,
           "flush failure is reported");
    destination.fail_flush = false;
    destination.fail_write_lba = 1;
    expect(copy_block_media(source, destination, false) == BlockCopyResult::WriteFailed,
           "write failure is reported");
    destination.fail_write_lba = static_cast<std::size_t>(-1);
    source.fail_read_lba = 1;
    expect(copy_block_media(source, destination, false) == BlockCopyResult::ReadFailed,
           "read failure is reported");

    source.fail_read_lba = static_cast<std::size_t>(-1);
    ConfigurableBackend physical_storage{3};
    BlockBackendArbiter physical{physical_storage};
    expect(copy_to_exclusive_backend(source, physical, true, true) == BlockCopyResult::Complete &&
               physical.owner() == BlockBackendArbiter::Owner::None,
           "copy to physical media owns and releases the backend");
    expect(physical.expose_to_client(), "physical media can be exposed");
    expect(copy_to_exclusive_backend(source, physical, true, false) ==
                   BlockCopyResult::OwnershipUnavailable &&
               physical.owner() == BlockBackendArbiter::Owner::EmulatedClient,
           "copy is rejected while physical media is exposed");
    expect(physical.hide_from_client(), "physical media can be hidden");

    ConfigurableBackend restored_image{3};
    expect(copy_from_exclusive_backend(physical, restored_image, true, true) ==
                   BlockCopyResult::Complete &&
               physical.owner() == BlockBackendArbiter::Owner::None,
           "copy from physical media owns, verifies, and releases the backend");
    for (std::size_t lba = 0; lba < 3; ++lba) {
        SdBlock restored{};
        expect(restored_image.read(lba, restored) == BlockOperationResult::Complete &&
                   restored.front() == lba + 1,
               "restored image matches source");
    }

    ConfigurableBackend unconfirmed_destination{3};
    BlockBackendArbiter unconfirmed_physical{unconfirmed_destination};
    expect(copy_to_exclusive_backend(source, unconfirmed_physical, false, false) ==
                   BlockCopyResult::ConfirmationRequired &&
               unconfirmed_physical.owner() == BlockBackendArbiter::Owner::None,
           "copy to physical media requires explicit destination confirmation");
    expect(copy_from_exclusive_backend(unconfirmed_physical, restored_image, false, false) ==
                   BlockCopyResult::ConfirmationRequired &&
               unconfirmed_physical.owner() == BlockBackendArbiter::Owner::None,
           "copy to an image requires explicit destination confirmation");

    Observer cancelled_exclusive_copy{0};
    expect(copy_to_exclusive_backend(source, unconfirmed_physical, true, false,
                                     &cancelled_exclusive_copy) == BlockCopyResult::Cancelled &&
               unconfirmed_physical.owner() == BlockBackendArbiter::Owner::None &&
               unconfirmed_destination.media_queries == 0,
           "cancelled copy to physical media does not acquire or query the backend");
    expect(copy_from_exclusive_backend(unconfirmed_physical, restored_image, true, false,
                                       &cancelled_exclusive_copy) == BlockCopyResult::Cancelled &&
               unconfirmed_physical.owner() == BlockBackendArbiter::Owner::None,
           "cancelled copy from physical media does not acquire the backend");

    class OwnershipCancellation final : public BlockCopyObserver {
      public:
        explicit OwnershipCancellation(const BlockBackendArbiter &backend) : backend_(backend) {}
        bool cancellation_requested() const override {
            return backend_.owner() == BlockBackendArbiter::Owner::HostCopy;
        }
        void progress(BlockCopyProgress) override {}

      private:
        const BlockBackendArbiter &backend_;
    } ownership_cancellation{unconfirmed_physical};
    expect(copy_to_exclusive_backend(source, unconfirmed_physical, true, false,
                                     &ownership_cancellation) == BlockCopyResult::Cancelled &&
               unconfirmed_physical.owner() == BlockBackendArbiter::Owner::None &&
               unconfirmed_destination.media_queries == 0,
           "cancellation after ownership acquisition releases it before preflight");

    physical_storage.fail_write_lba = 1;
    expect(copy_to_exclusive_backend(source, physical, true, false) ==
                   BlockCopyResult::WriteFailed &&
               physical.owner() == BlockBackendArbiter::Owner::None,
           "failed copy releases physical ownership");
}
