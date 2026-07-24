#include "picosd/protocol/sd_spi_card_engine.hpp"

#include "picosd/protocol/crc.hpp"

namespace picosd::protocol {

SdSpiCardEngine::SdSpiCardEngine(SdCardModel& model) : model_(model) {}

std::optional<SdSpiEngineOutput> SdSpiCardEngine::push_byte(std::uint8_t byte) {
    if (data_framer_.receiving()) {
        const auto event = data_framer_.push_byte(byte);
        if (!event.has_value()) return std::nullopt;

        SdSpiEngineOutput output;
        if (event->type == SdSpiDataEventType::Stop) {
            model_.finish_multi_write();
            return output;
        }

        const SdWriteResult write_result = model_.write_block(event->payload, event->crc);
        output.bytes[0] = write_result.data_response;
        output.size = 1;
        output.busy = write_result.busy;
        return output;
    }

    const auto decoded = command_framer_.push_byte(
        byte,
        model_.command_crc_enabled() ? SdCrcPolicy::Required : SdCrcPolicy::Ignored);
    if (!decoded.has_value()) return std::nullopt;

    SdSpiEngineOutput output;
    if (decoded->error != SdCommandError::None) {
        append_response(output, make_r1(static_cast<std::uint8_t>(SdR1::ComCrcError)));
        return output;
    }

    const SdModelResult result = model_.execute(decoded->command);
    append_response(output, result.response);
    if (result.has_read_block) append_read_block(output, result.read_block);
    if (result.has_register_data) append_register_data(output, result.register_data);

    if (decoded->command.index == 24U && model_.state() == SdCardState::ReceivingData) {
        data_framer_.begin(SdSpiWriteMode::SingleBlock);
    }
    if (decoded->command.index == 25U && model_.state() == SdCardState::ReceivingData) {
        data_framer_.begin(SdSpiWriteMode::MultiBlock);
    }
    return output;
}

void SdSpiCardEngine::chip_select_released() {
    command_framer_.reset();
    data_framer_.reset();
    model_.abort_pending_write();
}

void SdSpiCardEngine::append_response(SdSpiEngineOutput& output, const SdResponse& response) {
    for (std::size_t index = 0; index < response.size; ++index) {
        output.bytes[output.size++] = response.bytes[index];
    }
}

void SdSpiCardEngine::append_read_block(SdSpiEngineOutput& output, const SdBlock& block) {
    const SdDataBlock data = make_read_data_block(block);
    for (const std::uint8_t byte : data.bytes) output.bytes[output.size++] = byte;
}

void SdSpiCardEngine::append_register_data(
    SdSpiEngineOutput& output,
    const std::array<std::uint8_t, 16>& register_data) {
    output.bytes[output.size++] = kSdStartBlockToken;
    for (const std::uint8_t byte : register_data) output.bytes[output.size++] = byte;
    const std::uint16_t crc = crc16(register_data.data(), register_data.size());
    output.bytes[output.size++] = static_cast<std::uint8_t>(crc >> 8U);
    output.bytes[output.size++] = static_cast<std::uint8_t>(crc);
}

}  // namespace picosd::protocol
