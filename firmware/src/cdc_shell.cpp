#include "picosd/cdc_shell.hpp"

#include <cstdio>
#include <cstring>

#include "pico/stdlib.h"
#include "picosd/protocol/version.hpp"
#include "picosd/spi_capture.hpp"

namespace picosd::firmware {
namespace {
constexpr std::size_t kMaximumLineLength = 256;
char line[kMaximumLineLength]{};
std::size_t length = 0;

void respond(const char* text) { std::printf("%s\n", text); }
void handle_line() {
    if (std::strncmp(line, "HELLO id=", 9) == 0) {
        std::printf("OK id=%s version=%u.%u\n", line + 9,
                    static_cast<unsigned>(picosd::protocol::kVersionMajor),
                    static_cast<unsigned>(picosd::protocol::kVersionMinor));
    } else if (std::strncmp(line, "GET_INFO id=", 12) == 0) {
        std::printf("OK id=%s present=0 type=NONE blocks=0 block_size=512 readonly=1\n", line + 12);
    } else if (std::strncmp(line, "EJECT id=", 9) == 0 || std::strncmp(line, "FLUSH id=", 9) == 0) {
        std::printf("OK id=%s\n", line + 9);
    } else if (std::strcmp(line, "TRACE_ON") == 0) {
        set_spi_capture_trace_enabled(true);
        respond("OK trace=on");
    } else if (std::strcmp(line, "TRACE_OFF") == 0) {
        set_spi_capture_trace_enabled(false);
        respond("OK trace=off");
    } else {
        respond("ERR id=0 code=UNSUPPORTED");
    }
}
}  // namespace

void poll_cdc_shell() {
    const int value = getchar_timeout_us(0);
    if (value == PICO_ERROR_TIMEOUT) return;
    if (value == '\r') return;
    if (value == '\n') {
        line[length] = '\0';
        if (length != 0) handle_line();
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

}  // namespace picosd::firmware
