#include <cstdio>

#include "pico/stdlib.h"
#include "picosd/board_config.hpp"
#include "picosd/cdc_shell.hpp"
#include "picosd/protocol/version.hpp"

int main() {
    stdio_init_all();

    sleep_ms(1500);
    std::printf("Pico SD Emulator firmware 0.1.0\n");
    std::printf("Board: %s\n", picosd::board::kName);
    std::printf("Host protocol: %u.%u\n",
                static_cast<unsigned>(picosd::protocol::kVersionMajor),
                static_cast<unsigned>(picosd::protocol::kVersionMinor));

    while (true) {
        picosd::firmware::poll_cdc_shell();
        tight_loop_contents();
    }
}
