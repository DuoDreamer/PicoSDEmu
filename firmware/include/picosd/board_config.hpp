#pragma once

#include "pico/platform.h"

#if !PICO_RP2350
#error "Pico SD Emulator firmware requires an RP2350-based Raspberry Pi Pico 2"
#endif

namespace picosd::board {

inline constexpr char kName[] = "Raspberry Pi Pico 2";

// Preliminary Pico 2 pin map for the Phase 2 SPI-target proof of concept.
// These assignments are intentionally centralized so a future board revision
// can change wiring without touching the timing-critical target code.
inline constexpr unsigned int kClientChipSelectPin = 2;
inline constexpr unsigned int kClientClockPin = 3;
inline constexpr unsigned int kClientMosiPin = 4;
inline constexpr unsigned int kClientMisoPin = 5;

// Optional future physical-SD logical-proxy wiring. These pins are reserved;
// the initial firmware leaves them unconfigured.
inline constexpr unsigned int kPhysicalSdClockPin = 10;
inline constexpr unsigned int kPhysicalSdMosiPin = 11;
inline constexpr unsigned int kPhysicalSdMisoPin = 12;
inline constexpr unsigned int kPhysicalSdChipSelectPin = 13;

// Optional dry-contact card-detect transistor control. It remains inactive for
// the SC126 cable adapter, which does not provide a card-detect connection.
inline constexpr unsigned int kCardDetectControlPin = 14;

}  // namespace picosd::board
