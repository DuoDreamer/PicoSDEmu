#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "picosd/board_config.hpp"
#include "picosd/cdc_device.hpp"
#include "picosd/cdc_shell.hpp"
#include "picosd/sd_target_monitor.hpp"
#include "picosd/spi_capture.hpp"
#include "picosd/spi_transmit.hpp"

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

    picosd::firmware::initialize_cdc_device();
    picosd::firmware::initialize_spi_capture();
    picosd::firmware::initialize_spi_transmit();
    picosd::firmware::initialize_sd_target_monitor();

    while (true) {
        picosd::firmware::poll_cdc_device();
        picosd::firmware::poll_cdc_shell();
        if (!picosd::firmware::poll_sd_target_monitor()) {
            picosd::firmware::poll_spi_capture_trace();
        }
        tight_loop_contents();
    }
}
