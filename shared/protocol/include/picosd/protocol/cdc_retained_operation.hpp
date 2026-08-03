#pragma once

#include <cstdint>

#include "picosd/protocol/cdc_session_client.hpp"

namespace picosd::protocol {

enum class CdcOperationType { None, GetInfo, ReadBlock, WriteBlock, Flush, Eject };

// Retains the logical parameters of one operation so timeout and transient-error
// retries can issue a fresh request ID without reconstructing payload state.
class CdcRetainedOperation {
public:
    void retain_get_info();
    void retain_read_block(std::uint64_t lba);
    void retain_write_block(std::uint64_t lba, const CdcBlockData& data);
    void retain_flush();
    void retain_eject();
    void clear();

    [[nodiscard]] CdcSessionClientRequest issue(CdcSessionClient& client) const;
    [[nodiscard]] bool active() const { return type_ != CdcOperationType::None; }
    [[nodiscard]] CdcOperationType type() const { return type_; }

private:
    CdcOperationType type_ = CdcOperationType::None;
    std::uint64_t lba_ = 0;
    CdcBlockData data_{};
};

}  // namespace picosd::protocol
