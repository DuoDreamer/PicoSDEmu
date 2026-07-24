#include "picosd/protocol/sd_model.hpp"

#include "picosd/protocol/crc.hpp"

namespace picosd::protocol {
SdCardModel::SdCardModel(SdCardType type, RamBlockBackend& backend)
    : backend_(backend), registers_(make_sd_registers(type, static_cast<std::uint32_t>(backend.block_count()))), type_(type) {}

SdCardState SdCardModel::state() const { return state_.state(); }
bool SdCardModel::command_crc_enabled() const { return command_crc_enabled_; }
const SdCardRegisters& SdCardModel::registers() const { return registers_; }

std::uint8_t SdCardModel::r1_status() const {
    return state() == SdCardState::Idle ? static_cast<std::uint8_t>(SdR1::Idle) : 0U;
}

bool SdCardModel::command_lba(std::uint32_t argument, std::size_t& lba) const {
    if (type_ == SdCardType::Sdsc) {
        if ((argument % kSdBlockSize) != 0U) return false;
        lba = argument / kSdBlockSize;
    } else {
        lba = argument;
    }
    return lba < registers_.exposed_blocks;
}

SdModelResult SdCardModel::execute(const SdCommand& command) {
    SdModelResult result;
    if (command.index != 55U && command.index != 41U && command.index != 23U) app_command_pending_ = false;
    if (command.index == 0U) {
        state_.enter_idle();
        command_crc_enabled_ = false;
        multi_read_active_ = false;
        result.response = make_r1(static_cast<std::uint8_t>(SdR1::Idle));
        return result;
    }
    if (command.index == 8U && state() == SdCardState::Idle) {
        result.response = make_r7(r1_status(), command.argument);
        return result;
    }
    if (command.index == 13U && state() != SdCardState::PowerUp && state() != SdCardState::Fault) {
        result.response = make_r2(r1_status(), 0U);
        return result;
    }
    if (command.index == 12U && state() == SdCardState::Transfer) {
        multi_read_active_ = false;
        result.response = make_r1(0U);
        return result;
    }
    if (command.index == 59U && command.argument <= 1U &&
        state() != SdCardState::PowerUp && state() != SdCardState::Fault) {
        command_crc_enabled_ = command.argument == 1U;
        result.response = make_r1(r1_status());
        return result;
    }
    if (command.index == 55U) {
        app_command_pending_ = true;
        result.response = make_r1(r1_status());
        return result;
    }
    if (command.index == 41U && app_command_pending_ && state() == SdCardState::Idle) {
        state_.make_ready();
        app_command_pending_ = false;
        result.response = make_r1(0U);
        return result;
    }
    if (command.index == 23U && app_command_pending_ &&
        (state() == SdCardState::Ready || state() == SdCardState::Transfer)) {
        preerase_block_count_ = command.argument;
        app_command_pending_ = false;
        result.response = make_r1(r1_status());
        return result;
    }
    if (command.index == 58U && (state() == SdCardState::Ready || state() == SdCardState::Transfer)) {
        result.response = make_r3(r1_status(), (static_cast<std::uint32_t>(registers_.ocr[0]) << 24U) |
                                             (static_cast<std::uint32_t>(registers_.ocr[1]) << 16U) |
                                             (static_cast<std::uint32_t>(registers_.ocr[2]) << 8U) | registers_.ocr[3]);
        return result;
    }
    if ((command.index == 9U || command.index == 10U) &&
        (state() == SdCardState::Ready || state() == SdCardState::Transfer)) {
        state_.begin_transfer();
        result.response = make_r1(0U);
        result.has_register_data = true;
        result.register_data = command.index == 9U ? registers_.csd : registers_.cid;
        return result;
    }
    if (command.index == 16U && command.argument == kSdBlockSize &&
        (type_ == SdCardType::Sdsc || type_ == SdCardType::Sdhc)) {
        result.response = make_r1(r1_status());
        return result;
    }
    if ((command.index == 17U || command.index == 18U || command.index == 24U) &&
        (state() == SdCardState::Ready || state() == SdCardState::Transfer)) {
        std::size_t lba = 0;
        if (!command_lba(command.argument, lba)) { result.response = make_r1(static_cast<std::uint8_t>(SdR1::AddressError)); return result; }
        state_.begin_transfer();
        result.response = make_r1(0U);
        if (command.index == 17U || command.index == 18U) {
            result.has_read_block = backend_.read(lba, result.read_block);
            if (command.index == 18U && result.has_read_block) {
                multi_read_active_ = true;
                next_multi_read_lba_ = lba + 1;
            }
        }
        else { pending_write_lba_ = lba; state_.begin_receiving_data(); }
        return result;
    }
    result.response = make_r1(static_cast<std::uint8_t>(SdR1::IllegalCommand) | r1_status());
    return result;
}

bool SdCardModel::read_next_multi_block(SdBlock& output) {
    if (!multi_read_active_ || next_multi_read_lba_ >= registers_.exposed_blocks) return false;
    if (!backend_.read(next_multi_read_lba_, output)) return false;
    ++next_multi_read_lba_;
    return true;
}

SdResponse SdCardModel::write_block(const SdBlock& block, std::uint16_t crc) {
    if (state() != SdCardState::ReceivingData || crc16(block.data(), block.size()) != crc) {
        return make_r1(static_cast<std::uint8_t>(SdR1::ComCrcError));
    }
    if (!backend_.write(pending_write_lba_, block)) { state_.fault(); return make_r1(static_cast<std::uint8_t>(SdR1::AddressError)); }
    state_.begin_busy(); state_.finish_busy();
    return make_r1(0U);
}
}  // namespace picosd::protocol
