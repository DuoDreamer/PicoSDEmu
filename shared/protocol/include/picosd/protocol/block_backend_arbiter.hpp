#pragma once

#include <atomic>
#include <cstddef>

#include "picosd/protocol/sd_card_state.hpp"

namespace picosd::protocol {

// Grants exclusive access to a block backend either to the emulated SD client
// or to a host-directed whole-media copy. The arbiter itself is the backend
// presented to the SD command engine; copy code must use the explicitly named
// host_copy_* methods after acquiring the copy ownership state.
class BlockBackendArbiter final : public BlockBackend {
  public:
    enum class Owner {
        None,
        EmulatedClient,
        HostCopy,
    };

    explicit BlockBackendArbiter(BlockBackend &backend);

    [[nodiscard]] Owner owner() const;
    [[nodiscard]] bool expose_to_client();
    [[nodiscard]] bool hide_from_client();
    [[nodiscard]] bool begin_host_copy();
    [[nodiscard]] bool end_host_copy();

    [[nodiscard]] std::size_t block_count() const override;
    [[nodiscard]] bool media_present() const override;
    [[nodiscard]] bool write_protected() const override;
    [[nodiscard]] BlockOperationResult flush() override;
    [[nodiscard]] BlockOperationResult read(std::size_t lba, SdBlock &output) const override;
    [[nodiscard]] BlockOperationResult write(std::size_t lba, const SdBlock &input) override;

    [[nodiscard]] std::size_t host_copy_block_count() const;
    [[nodiscard]] bool host_copy_media_present() const;
    [[nodiscard]] bool host_copy_write_protected() const;
    [[nodiscard]] BlockOperationResult host_copy_flush();
    [[nodiscard]] BlockOperationResult host_copy_read(std::size_t lba, SdBlock &output) const;
    [[nodiscard]] BlockOperationResult host_copy_write(std::size_t lba, const SdBlock &input);

  private:
    [[nodiscard]] bool transition(Owner expected, Owner desired);

    BlockBackend &backend_;
    std::atomic<Owner> owner_{Owner::None};
};

} // namespace picosd::protocol
