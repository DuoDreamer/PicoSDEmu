#pragma once

namespace picosd::firmware {

void initialize_sd_target_monitor();
bool poll_sd_target_monitor();
void set_sd_target_monitor_trace_enabled(bool enabled);

}  // namespace picosd::firmware
