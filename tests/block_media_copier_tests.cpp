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
               observer.reports_ == 4 && destination.flushes == 1,
           "progress and flush are reported");
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
    expect(copy_to_exclusive_backend(source, physical, true) == BlockCopyResult::Complete &&
               physical.owner() == BlockBackendArbiter::Owner::None,
           "copy to physical media owns and releases the backend");
    expect(physical.expose_to_client(), "physical media can be exposed");
    expect(copy_to_exclusive_backend(source, physical, false) ==
               BlockCopyResult::OwnershipUnavailable &&
               physical.owner() == BlockBackendArbiter::Owner::EmulatedClient,
           "copy is rejected while physical media is exposed");
    expect(physical.hide_from_client(), "physical media can be hidden");

    ConfigurableBackend restored_image{3};
    expect(copy_from_exclusive_backend(physical, restored_image, true) ==
               BlockCopyResult::Complete &&
               physical.owner() == BlockBackendArbiter::Owner::None,
           "copy from physical media owns, verifies, and releases the backend");
    for (std::size_t lba = 0; lba < 3; ++lba) {
        SdBlock restored{};
        expect(restored_image.read(lba, restored) == BlockOperationResult::Complete &&
                   restored.front() == lba + 1,
               "restored image matches source");
    }

    physical_storage.fail_write_lba = 1;
    expect(copy_to_exclusive_backend(source, physical, false) == BlockCopyResult::WriteFailed &&
               physical.owner() == BlockBackendArbiter::Owner::None,
           "failed copy releases physical ownership");
}
