#pragma once

namespace picosd::firmware {

// Polls USB stdio and processes complete bounded CDC control lines. The SD SPI
// target is intentionally not enabled by this early transport shell.
void poll_cdc_shell();

}  // namespace picosd::firmware
