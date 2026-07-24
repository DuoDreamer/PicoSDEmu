#include <cstdio>

#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "picosd/board_config.hpp"
#include "picosd/cdc_shell.hpp"
#include "picosd/spi_capture.hpp"
#include "picosd/protocol/version.hpp"

int main() {
    // Before any SPI target PIO is installed, keep client-facing pins passive.
    // In particular, MISO must not drive the shared client bus at boot.
    gpio_init(picosd::board::kClientChipSelectPin);
    gpio_set_dir(picosd::board::kClientChipSelectPin, GPIO_IN);
    gpio_disable_pulls(picosd::board::kClientChipSelectPin);
    gpio_init(picosd::board::kClientClockPin);
    gpio_set_dir(picosd::board::kClientClockPin, GPIO_IN);
    gpio_disable_pulls(picosd::board::kClientClockPin);
    gpio_init(picosd::board::kClientMosiPin);
    gpio_set_dir(picosd::board::kClientMosiPin, GPIO_IN);
    gpio_disable_pulls(picosd::board::kClientMosiPin);
    gpio_init(picosd::board::kClientMisoPin);
    gpio_set_dir(picosd::board::kClientMisoPin, GPIO_IN);
    gpio_disable_pulls(picosd::board::kClientMisoPin);

    stdio_init_all();
    picosd::firmware::initialize_spi_capture();

    sleep_ms(1500);
    std::printf("Pico SD Emulator firmware 0.1.0\n");
    std::printf("Board: %s\n", picosd::board::kName);
    std::printf("Host protocol: %u.%u\n",
                static_cast<unsigned>(picosd::protocol::kVersionMajor),
                static_cast<unsigned>(picosd::protocol::kVersionMinor));

    while (true) {
        picosd::firmware::poll_cdc_shell();
        picosd::firmware::poll_spi_capture_trace();
        tight_loop_contents();
    }
}
