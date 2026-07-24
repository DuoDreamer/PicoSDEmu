#include "picosd/protocol/sd_spi_data_framer.hpp"

#include "picosd/protocol/sd_command.hpp"

namespace picosd::protocol {

void SdSpiDataFramer::begin(SdSpiWriteMode mode) {
    reset();
    mode_ = mode;
    state_ = State::WaitingForToken;
}

std::optional<SdSpiDataEvent> SdSpiDataFramer::push_byte(std::uint8_t byte) {
    switch (state_) {
        case State::Idle:
            return std::nullopt;

        case State::WaitingForToken: {
            if (mode_ == SdSpiWriteMode::MultiBlock && is_multi_write_stop_token(byte)) {
                state_ = State::Idle;
                return SdSpiDataEvent{SdSpiDataEventType::Stop, {}, 0};
            }
            const bool is_data_token =
                (mode_ == SdSpiWriteMode::SingleBlock && byte == kSdStartBlockToken) ||
                (mode_ == SdSpiWriteMode::MultiBlock && is_multi_write_data_token(byte));
            if (is_data_token) {
                payload_size_ = 0;
                crc_ = 0;
                state_ = State::Payload;
            }
            return std::nullopt;
        }

        case State::Payload:
            payload[payload_size_++] = byte;
            if (payload_size_ == payload.size()) state_ = State::CrcHigh;
            return std::nullopt;

        case State::CrcHigh:
            crc_ = static_cast<std::uint16_t>(byte) << 8U;
            state_ = State::CrcLow;
            return std::nullopt;

        case State::CrcLow: {
            crc_ |= byte;
            const SdSpiDataEvent event{SdSpiDataEventType::Block, payload, crc_};
            state_ = mode_ == SdSpiWriteMode::MultiBlock ? State::WaitingForToken : State::Idle;
            return event;
        }
    }

    return std::nullopt;
}

void SdSpiDataFramer::reset() {
    state_ = State::Idle;
    payload_size_ = 0;
    crc_ = 0;
}

bool SdSpiDataFramer::receiving() const { return state_ != State::Idle; }

}  // namespace picosd::protocol
