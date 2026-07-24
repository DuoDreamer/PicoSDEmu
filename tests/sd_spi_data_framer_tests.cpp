#include <cassert>
#include <cstdint>

#include "picosd/protocol/crc.hpp"
#include "picosd/protocol/sd_command.hpp"
#include "picosd/protocol/sd_spi_data_framer.hpp"

namespace {

using picosd::protocol::SdSpiDataEventType;
using picosd::protocol::SdSpiDataFramer;
using picosd::protocol::SdSpiWriteMode;

void push_block(
    SdSpiDataFramer& framer,
    std::uint8_t token,
    const std::array<std::uint8_t, 512>& payload) {
    assert(!framer.push_byte(token).has_value());
    for (const std::uint8_t byte : payload) assert(!framer.push_byte(byte).has_value());
    const std::uint16_t crc = picosd::protocol::crc16(payload.data(), payload.size());
    assert(!framer.push_byte(static_cast<std::uint8_t>(crc >> 8U)).has_value());
    const auto event = framer.push_byte(static_cast<std::uint8_t>(crc));
    assert(event.has_value());
    assert(event->type == SdSpiDataEventType::Block);
    assert(event->payload == payload);
    assert(event->crc == crc);
}

}  // namespace

int main() {
    std::array<std::uint8_t, 512> first{};
    std::array<std::uint8_t, 512> second{};
    first[0] = 0x12U;
    first[511] = 0x34U;
    second[0] = 0x56U;
    second[511] = 0x78U;

    SdSpiDataFramer framer;
    assert(!framer.push_byte(0xfeU).has_value());
    assert(!framer.receiving());

    framer.begin(SdSpiWriteMode::SingleBlock);
    assert(framer.receiving());
    assert(!framer.push_byte(0xffU).has_value());
    push_block(framer, picosd::protocol::kSdStartBlockToken, first);
    assert(!framer.receiving());

    framer.begin(SdSpiWriteMode::MultiBlock);
    // The single-block token is not valid for CMD25 data.
    assert(!framer.push_byte(picosd::protocol::kSdStartBlockToken).has_value());
    push_block(framer, picosd::protocol::kSdStartMultiWriteToken, first);
    assert(framer.receiving());
    push_block(framer, picosd::protocol::kSdStartMultiWriteToken, second);
    const auto stop = framer.push_byte(picosd::protocol::kSdStopMultiWriteToken);
    assert(stop.has_value());
    assert(stop->type == SdSpiDataEventType::Stop);
    assert(!framer.receiving());

    framer.begin(SdSpiWriteMode::SingleBlock);
    assert(!framer.push_byte(picosd::protocol::kSdStartBlockToken).has_value());
    assert(!framer.push_byte(first[0]).has_value());
    framer.reset();
    assert(!framer.receiving());
    assert(!framer.push_byte(first[1]).has_value());

    return 0;
}
