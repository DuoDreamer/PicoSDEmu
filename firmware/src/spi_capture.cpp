#include "picosd/spi_capture.hpp"

#include "hardware/pio.h"
#include "picosd/board_config.hpp"
#include "sd_spi_capture.pio.h"

namespace picosd::firmware {
namespace {
PIO pio = pio0;
unsigned int state_machine = 0;
}

void initialize_spi_capture() {
    state_machine = static_cast<unsigned int>(pio_claim_unused_sm(pio, true));
    const unsigned int offset = pio_add_program(pio, &picosd_spi_capture_program);
    pio_sm_config configuration = picosd_spi_capture_program_get_default_config(offset);
    sm_config_set_in_pins(&configuration, board::kClientMosiPin);
    sm_config_set_jmp_pin(&configuration, board::kClientChipSelectPin);
    sm_config_set_in_shift(&configuration, false, true, 8);
    pio_sm_init(pio, state_machine, offset, &configuration);
    pio_sm_set_enabled(pio, state_machine, true);
}

bool try_read_spi_capture_byte(std::uint8_t& output) {
    if (pio_sm_is_rx_fifo_empty(pio, state_machine)) return false;
    output = static_cast<std::uint8_t>(pio_sm_get(pio, state_machine));
    return true;
}

}  // namespace picosd::firmware
