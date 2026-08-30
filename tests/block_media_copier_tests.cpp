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
        return storage_.block_count();
    }
    bool media_present() const override {
        return present;
    }
    bool write_protected() const override {
        return readonly;
    }
    BlockOperationResult flush() override {
        ++flushes;
        return fail_flush ? BlockOperationResult::Failed : BlockOperationResult::Complete;
    }
    BlockOperationResult read(std::size_t lba, SdBlock &output) const override {
        if (lba == fail_read_lba)
            return BlockOperationResult::Failed;
        return storage_.read(lba, output);
    }
    BlockOperationResult write(std::size_t lba, const SdBlock &input) override {
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

} // namespace

int main() {
    ConfigurableBackend source{3};
    ConfigurableBackend destination{4};
    SdBlock block{};
    for (std::size_t lba = 0; lba < 3; ++lba) {
        block.fill(static_cast<std::uint8_t>(lba + 1));
        expect(source.write(lba, block) == BlockOperationResult::Complete, "seed source");
    }
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

    ConfigurableBackend final_boundary_destination{3};
    Observer final_boundary_cancellation{3};
    expect(copy_block_media(source, final_boundary_destination, false,
                            &final_boundary_cancellation) == BlockCopyResult::Cancelled &&
               final_boundary_cancellation.last_.completed_blocks == 3 &&
               final_boundary_destination.flushes == 0,
           "cancellation at the final copied block stops before the flush");

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

    physical_storage.fail_write_lba = 1;
    expect(copy_to_exclusive_backend(source, physical, true, false) ==
                   BlockCopyResult::WriteFailed &&
               physical.owner() == BlockBackendArbiter::Owner::None,
           "failed copy releases physical ownership");
}
