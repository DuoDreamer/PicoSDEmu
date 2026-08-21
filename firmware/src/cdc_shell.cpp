#include "picosd/cdc_shell.hpp"

#include <cstdio>
#include <cstring>

#include "picosd/cdc_backend_service.hpp"
#include "picosd/cdc_device.hpp"
#include "picosd/protocol/version.hpp"
#include "picosd/sd_target_monitor.hpp"
#include "picosd/spi_capture.hpp"

namespace picosd::firmware {
namespace {
// A single BASE64 sector response is roughly 800 bytes including fields.
constexpr std::size_t kMaximumLineLength = 1024;
char line[kMaximumLineLength]{};
std::size_t length = 0;

void respond(const char *text) {
    char response[kMaximumLineLength]{};
    const int written = std::snprintf(response, sizeof(response), "%s\n", text);
    if (written > 0 && static_cast<std::size_t>(written) < sizeof(response)) {
        write_cdc(response, static_cast<std::size_t>(written));
    }
}

template <typename... Arguments> void respond_format(const char *format, Arguments... arguments) {
    char response[kMaximumLineLength]{};
    const int written = std::snprintf(response, sizeof(response), format, arguments...);
    if (written > 0 && static_cast<std::size_t>(written) < sizeof(response)) {
        write_cdc(response, static_cast<std::size_t>(written));
    }
}
void handle_line() {
    if (handle_cdc_backend_response(std::string_view(line, length))) {
        return;
    } else if (std::strncmp(line, "HELLO id=", 9) == 0) {
        respond_format("OK id=%s version=%u.%u\n", line + 9,
                       static_cast<unsigned>(picosd::protocol::kVersionMajor),
                       static_cast<unsigned>(picosd::protocol::kVersionMinor));
    } else if (std::strncmp(line, "GET_INFO id=", 12) == 0) {
        respond_format("OK id=%s present=0 type=NONE blocks=0 block_size=512 readonly=1\n",
                       line + 12);
    } else if (std::strncmp(line, "EJECT id=", 9) == 0 || std::strncmp(line, "FLUSH id=", 9) == 0) {
        respond_format("OK id=%s\n", line + 9);
    } else if (std::strcmp(line, "TRACE_ON") == 0) {
        set_sd_target_monitor_enabled(false);
        set_spi_capture_trace_enabled(true);
        respond("OK trace=on");
    } else if (std::strcmp(line, "TRACE_OFF") == 0) {
        set_spi_capture_trace_enabled(false);
        respond("OK trace=off");
    } else if (std::strcmp(line, "TARGET_TRACE_ON") == 0) {
        set_spi_capture_trace_enabled(false);
        set_sd_target_monitor_enabled(true);
        set_sd_target_monitor_trace_enabled(true);
        respond("OK target_trace=on miso=active");
    } else if (std::strcmp(line, "TARGET_TRACE_OFF") == 0) {
        set_sd_target_monitor_trace_enabled(false);
        respond("OK target_trace=off");
    } else if (std::strcmp(line, "TARGET_ON") == 0) {
        set_spi_capture_trace_enabled(false);
        set_sd_target_monitor_trace_enabled(false);
        set_sd_target_monitor_enabled(true);
        respond("OK target=on miso=active");
    } else if (std::strcmp(line, "TARGET_OFF") == 0) {
        set_sd_target_monitor_enabled(false);
        respond("OK target=off miso=passive");
    } else if (std::strcmp(line, "TARGET_COUNTERS") == 0) {
        print_sd_target_monitor_counters();
    } else {
        respond("ERR id=0 code=UNSUPPORTED");
    }
}
} // namespace

void poll_cdc_shell() {
    std::uint8_t byte = 0;
    if (!read_cdc_byte(byte))
        return;
    const int value = byte;
    if (value == '\r')
        return;
    if (value == '\n') {
        line[length] = '\0';
        if (length != 0)
            handle_line();
        length = 0;
        return;
    }
    if (length + 1 >= kMaximumLineLength) {
        length = 0;
        respond("ERR id=0 code=LINE_TOO_LONG");
        return;
    }
    line[length++] = static_cast<char>(value);
}

} // namespace picosd::firmware
