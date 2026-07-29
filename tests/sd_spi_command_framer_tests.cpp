#include <array>
#include <cassert>
#include <cstdint>

#include "picosd/protocol/sd_spi_command_framer.hpp"

namespace {

using picosd::protocol::SdCommandError;
using picosd::protocol::SdCrcPolicy;
using picosd::protocol::SdSpiCommandFramer;

void push_command(
    SdSpiCommandFramer& framer,
    const std::array<std::uint8_t, 6>& command,
    SdCrcPolicy policy) {
    for (std::size_t index = 0; index + 1 < command.size(); ++index) {
        assert(!framer.push_byte(command[index], policy).has_value());
    }

    const auto result = framer.push_byte(command.back(), policy);
    assert(result.has_value());
    assert(result->error == SdCommandError::None);
}

}  // namespace

int main() {
    SdSpiCommandFramer framer;

    // Idle clocks and arbitrary non-command bytes do not begin a frame.
    assert(!framer.push_byte(0xffU, SdCrcPolicy::Required).has_value());
    assert(!framer.push_byte(0x00U, SdCrcPolicy::Required).has_value());
    assert(!framer.receiving());

    const std::array<std::uint8_t, 6> cmd0 = {0x40U, 0x00U, 0x00U,
                                               0x00U, 0x00U, 0x95U};
    push_command(framer, cmd0, SdCrcPolicy::Required);
    assert(!framer.receiving());

    const std::array<std::uint8_t, 6> cmd8 = {0x48U, 0x00U, 0x00U,
                                               0x01U, 0xaaU, 0x87U};
    for (std::size_t index = 0; index + 1 < cmd8.size(); ++index) {
        assert(!framer.push_byte(cmd8[index], SdCrcPolicy::Required).has_value());
    }
    const auto cmd8_result = framer.push_byte(cmd8.back(), SdCrcPolicy::Required);
    assert(cmd8_result.has_value());
    assert(cmd8_result->command.index == 8U);
    assert(cmd8_result->command.argument == 0x000001aaU);

    // CS deassertion discards an incomplete frame rather than splicing it to
    // the next transaction.
    assert(!framer.push_byte(cmd0[0], SdCrcPolicy::Required).has_value());
    assert(framer.receiving());
    framer.reset();
    assert(!framer.receiving());
    push_command(framer, cmd0, SdCrcPolicy::Required);

    const std::array<std::uint8_t, 6> invalid_crc = {0x40U, 0x00U, 0x00U,
                                                      0x00U, 0x00U, 0x97U};
    for (std::size_t index = 0; index + 1 < invalid_crc.size(); ++index) {
        assert(!framer.push_byte(invalid_crc[index], SdCrcPolicy::Required).has_value());
    }
    const auto invalid_result = framer.push_byte(invalid_crc.back(), SdCrcPolicy::Required);
    assert(invalid_result.has_value());
    assert(invalid_result->error == SdCommandError::CrcMismatch);

    return 0;
}
