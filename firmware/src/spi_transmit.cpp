#include "picosd/spi_transmit.hpp"

#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "picosd/board_config.hpp"
#include "sd_spi_transmit.pio.h"

namespace picosd::firmware {
namespace {
PIO pio = pio0;
unsigned int state_machine = 0;
}  // namespace

void initialize_spi_transmit() {
    state_machine = static_cast<unsigned int>(pio_claim_unused_sm(pio, true));
    const unsigned int offset = pio_add_program(pio, &picosd_spi_transmit_program);
    pio_sm_config configuration = picosd_spi_transmit_program_get_default_config(offset);
    sm_config_set_out_pins(&configuration, board::kClientMisoPin, 1);
    sm_config_set_set_pins(&configuration, board::kClientMisoPin, 1);
    sm_config_set_jmp_pin(&configuration, board::kClientChipSelectPin);
    sm_config_set_out_shift(&configuration, false, false, 32);

    // Connecting the pad to PIO does not make it an output. The program owns
    // the direction and only asserts it after observing active-low CS.
    pio_gpio_init(pio, board::kClientMisoPin);
    pio_sm_set_consecutive_pindirs(pio, state_machine, board::kClientMisoPin, 1, false);
    pio_sm_init(pio, state_machine, offset, &configuration);
    pio_sm_set_enabled(pio, state_machine, true);
}

bool spi_transmit_ready() { return !pio_sm_is_tx_fifo_full(pio, state_machine); }

bool try_write_spi_transmit_byte(std::uint8_t byte) {
    if (!spi_transmit_ready()) return false;
    pio_sm_put(pio, state_machine, static_cast<std::uint32_t>(byte) << 24U);
    return true;
}

void cancel_spi_transmit() { pio_sm_clear_fifos(pio, state_machine); }

}  // namespace picosd::firmware
