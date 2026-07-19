#pragma once

#include "pico/platform.h"

#if !PICO_RP2350
#error "Pico SD Emulator firmware requires an RP2350-based Raspberry Pi Pico 2"
#endif

namespace picosd::board {

inline constexpr char kName[] = "Raspberry Pi Pico 2";

// Pin assignments will be added after the PIO SPI-target proof of concept.

}  // namespace picosd::board

