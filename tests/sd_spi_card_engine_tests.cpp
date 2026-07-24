#include <array>
#include <cassert>
#include <cstdint>

#include "picosd/protocol/crc.hpp"
#include "picosd/protocol/sd_spi_card_engine.hpp"

namespace {

using picosd::protocol::RamBlockBackend;
using picosd::protocol::SdCardModel;
using picosd::protocol::SdCardType;
using picosd::protocol::SdSpiCardEngine;

auto send_command(SdSpiCardEngine& engine, const std::array<std::uint8_t, 6>& command) {
    for (std::size_t index = 0; index + 1 < command.size(); ++index) {
        assert(!engine.push_byte(command[index]).has_value());
    }
    const auto output = engine.push_byte(command.back());
    assert(output.has_value());
    return *output;
}

void initialize(SdSpiCardEngine& engine) {
    assert(send_command(engine, {0x40U, 0, 0, 0, 0, 0x95U}).bytes[0] == 0x01U);
    assert(send_command(engine, {0x48U, 0, 0, 1, 0xaaU, 0x87U}).bytes[0] == 0x01U);
    assert(send_command(engine, {0x77U, 0, 0, 0, 0, 0x65U}).bytes[0] == 0x01U);
    assert(send_command(engine, {0x69U, 0x40U, 0, 0, 0, 0x77U}).bytes[0] == 0x00U);
}

}  // namespace

int main() {
    RamBlockBackend backend(16);
    backend.fill_diagnostic_pattern();
    SdCardModel model(SdCardType::Sdsc, backend);
    SdSpiCardEngine engine(model);
    initialize(engine);

    const auto csd = send_command(engine, {0x49U, 0, 0, 0, 0, 0x01U});
    assert(csd.size == 1U + 1U + model.registers().csd.size() + 2U);
    assert(csd.bytes[0] == 0x00U);
    assert(csd.bytes[1] == picosd::protocol::kSdStartBlockToken);
    for (std::size_t index = 0; index < model.registers().csd.size(); ++index) {
        assert(csd.bytes[2U + index] == model.registers().csd[index]);
    }
    const std::uint16_t csd_crc = picosd::protocol::crc16(
        model.registers().csd.data(), model.registers().csd.size());
    assert(csd.bytes[18] == static_cast<std::uint8_t>(csd_crc >> 8U));
    assert(csd.bytes[19] == static_cast<std::uint8_t>(csd_crc));

    const auto read = send_command(engine, {0x51U, 0, 0, 0, 0, 0x01U});
    assert(read.size == 1U + picosd::protocol::kSdDataBlockWireSize);
    assert(read.bytes[0] == 0x00U);
    assert(read.bytes[1] == picosd::protocol::kSdStartBlockToken);
    assert(read.bytes[2] == 0x00U);
    assert(read.bytes[513] == 0xffU);

    const auto write_command = send_command(engine, {0x58U, 0, 0, 2, 0, 0x01U});
    assert(write_command.bytes[0] == 0x00U);
    std::array<std::uint8_t, 512> payload{};
    payload[0] = 0xa5U;
    payload[511] = 0x5aU;
    assert(!engine.push_byte(picosd::protocol::kSdStartBlockToken).has_value());
    for (const std::uint8_t byte : payload) assert(!engine.push_byte(byte).has_value());
    const std::uint16_t crc = picosd::protocol::crc16(payload.data(), payload.size());
    assert(!engine.push_byte(static_cast<std::uint8_t>(crc >> 8U)).has_value());
    const auto write = engine.push_byte(static_cast<std::uint8_t>(crc));
    assert(write.has_value());
    assert(write->size == 1U);
    assert(write->bytes[0] == picosd::protocol::kSdDataResponseAccepted);
    assert(write->busy);

    const auto read_back = send_command(engine, {0x51U, 0, 0, 2, 0, 0x01U});
    assert(read_back.bytes[2] == 0xa5U);
    assert(read_back.bytes[513] == 0x5aU);

    // A CS release during data framing abandons the pending write and leaves
    // the model able to accept a later command.
    assert(send_command(engine, {0x58U, 0, 0, 4, 0, 0x01U}).bytes[0] == 0x00U);
    assert(!engine.push_byte(picosd::protocol::kSdStartBlockToken).has_value());
    assert(!engine.push_byte(0x99U).has_value());
    engine.chip_select_released();
    const auto post_abort_read = send_command(engine, {0x51U, 0, 0, 4, 0, 0x01U});
    assert(post_abort_read.bytes[0] == 0x00U);
    assert(post_abort_read.bytes[2] != 0x99U);

    return 0;
}
