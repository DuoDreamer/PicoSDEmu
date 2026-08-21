#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "picosd/protocol/sd_card_state.hpp"

namespace picosd::firmware {

// Non-blocking bridge between the synchronous SD storage boundary and the CDC
// sector pool. A cache miss queues a host read and reports not-ready; retrying
// the read after the main loop has received the response returns the sector.
// Writes are deliberately exposed as a two-step operation: the first call
// queues the write and reports not-ready, and an identical retry succeeds only
// after the host acknowledgement is present in the write-through cache.
class CdcSdBackend final : public picosd::protocol::BlockBackend {
  public:
    void refresh_media();

    [[nodiscard]] std::size_t block_count() const override;
    [[nodiscard]] bool read(std::size_t lba, picosd::protocol::SdBlock &output) const override;
    [[nodiscard]] bool write(std::size_t lba, const picosd::protocol::SdBlock &input) override;

  private:
    [[nodiscard]] bool address_valid(std::size_t lba) const;

    std::uint64_t blocks_ = 0;
    std::uint64_t generation_ = 0;
    bool present_ = false;
    bool read_only_ = true;
    bool write_pending_ = false;
    std::size_t pending_write_lba_ = 0;
    picosd::protocol::SdBlock pending_write_data_{};
};

} // namespace picosd::firmware
