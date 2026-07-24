#pragma once

#include "picosd/protocol/sd_card_state.hpp"
#include "picosd/protocol/sd_command.hpp"
#include "picosd/protocol/sd_registers.hpp"

namespace picosd::protocol {

struct SdModelResult {
    SdResponse response{};
    bool has_read_block = false;
    SdBlock read_block{};
    bool has_register_data = false;
    std::array<std::uint8_t, 16> register_data{};
};

class SdCardModel {
public:
    SdCardModel(SdCardType type, RamBlockBackend& backend);
    [[nodiscard]] SdCardState state() const;
    [[nodiscard]] bool command_crc_enabled() const;
    [[nodiscard]] const SdCardRegisters& registers() const;
    SdModelResult execute(const SdCommand& command);
    bool read_next_multi_block(SdBlock& output);
    bool finish_multi_write();
    SdResponse write_block(const SdBlock& block, std::uint16_t crc);

private:
    [[nodiscard]] bool command_lba(std::uint32_t argument, std::size_t& lba) const;
    [[nodiscard]] std::uint8_t r1_status() const;

    RamBlockBackend& backend_;
    SdCardRegisters registers_;
    SdCardType type_;
    SdCardStateMachine state_;
    bool app_command_pending_ = false;
    std::uint32_t preerase_block_count_ = 0;
    bool multi_read_active_ = false;
    std::size_t next_multi_read_lba_ = 0;
    bool multi_write_active_ = false;
    bool command_crc_enabled_ = false;
    std::size_t pending_write_lba_ = 0;
};

}  // namespace picosd::protocol
