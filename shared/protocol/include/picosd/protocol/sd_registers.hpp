#pragma once

#include <array>
#include <cstdint>

namespace picosd::protocol {

enum class SdCardType { Sdsc, Sdhc };

struct SdCardRegisters {
    std::array<std::uint8_t, 4> ocr{};
    std::array<std::uint8_t, 16> cid{};
    std::array<std::uint8_t, 16> csd{};
    std::uint32_t exposed_blocks = 0;
};

// Produces project-owned, deterministic register values. Capacity is rounded
// down to the largest representation supported by the selected CSD format.
SdCardRegisters make_sd_registers(SdCardType type, std::uint32_t requested_blocks);

}  // namespace picosd::protocol
