#pragma once

#include <cstddef>
#include <cstdint>

#include "hardware/spi.h"
#include "picosd/protocol/sd_card_state.hpp"

namespace picosd::firmware {

// SPI-master backend for the optional physical SD socket. It is deliberately
// not selected by the application yet: arbitration and control commands are a
// separate Phase 6 deliverable. Calls are bounded by a deadline and use no
// dynamic allocation.
class PhysicalSdBackend final : public picosd::protocol::BlockBackend {
  public:
    struct Pins {
        unsigned int clock;
        unsigned int mosi;
        unsigned int miso;
        unsigned int chip_select;
    };

    PhysicalSdBackend(spi_inst_t *spi, Pins pins, std::uint32_t baud_hz,
                      std::uint32_t timeout_ms);

    [[nodiscard]] bool initialize();
    void deinitialize();
    [[nodiscard]] bool media_present() const;

    [[nodiscard]] std::size_t block_count() const override;
    [[nodiscard]] picosd::protocol::BlockOperationResult
    read(std::size_t lba, picosd::protocol::SdBlock &output) const override;
    [[nodiscard]] picosd::protocol::BlockOperationResult
    write(std::size_t lba, const picosd::protocol::SdBlock &input) override;

  private:
    [[nodiscard]] std::uint8_t command(std::uint8_t index, std::uint32_t argument,
                                       std::uint8_t crc,
                                       std::uint32_t *trailing = nullptr) const;
    [[nodiscard]] bool select() const;
    void deselect() const;
    [[nodiscard]] bool wait_byte(std::uint8_t expected) const;
    [[nodiscard]] std::uint8_t transfer(std::uint8_t output) const;
    [[nodiscard]] std::uint32_t address(std::size_t lba) const;

    spi_inst_t *spi_;
    Pins pins_;
    std::uint32_t baud_hz_;
    std::uint32_t timeout_ms_;
    std::size_t blocks_ = 0;
    bool initialized_ = false;
    bool high_capacity_ = false;
};

} // namespace picosd::firmware
