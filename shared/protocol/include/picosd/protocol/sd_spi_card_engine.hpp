#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "picosd/protocol/sd_model.hpp"
#include "picosd/protocol/sd_spi_command_framer.hpp"
#include "picosd/protocol/sd_spi_data_framer.hpp"

namespace picosd::protocol {

inline constexpr std::size_t kSdSpiEngineMaximumOutputSize = 1 + kSdDataBlockWireSize;

struct SdSpiEngineOutput {
    std::array<std::uint8_t, kSdSpiEngineMaximumOutputSize> bytes{};
    std::size_t size = 0;
    bool busy = false;
};

// Coordinates byte framers with the portable card model. It is deliberately
// free of PIO, DMA, GPIO, and USB dependencies so firmware can call it from a
// bounded byte-queue worker after capture timing is verified.
class SdSpiCardEngine {
public:
    explicit SdSpiCardEngine(SdCardModel& model);

    [[nodiscard]] std::optional<SdSpiEngineOutput> push_byte(std::uint8_t byte);
    void chip_select_released();

private:
    void append_response(SdSpiEngineOutput& output, const SdResponse& response);
    void append_read_block(SdSpiEngineOutput& output, const SdBlock& block);
    void append_register_data(
        SdSpiEngineOutput& output,
        const std::array<std::uint8_t, 16>& register_data);

    SdCardModel& model_;
    SdSpiCommandFramer command_framer_;
    SdSpiDataFramer data_framer_;
};

}  // namespace picosd::protocol
