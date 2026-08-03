#pragma once

#include <cstdint>

#include "picosd/protocol/cdc_session_client.hpp"

namespace picosd::protocol {

// Clock-agnostic deadline policy for the single outstanding CDC request. The
// caller supplies monotonically increasing ticks in any convenient unit.
class CdcRequestDeadline {
public:
    bool arm(std::uint64_t now, std::uint64_t timeout,
             const CdcSessionClient& client);
    bool expire_if_due(std::uint64_t now, CdcSessionClient& client);
    void clear();

    [[nodiscard]] bool armed() const { return armed_; }
    [[nodiscard]] std::uint64_t deadline() const { return deadline_; }

private:
    std::uint64_t deadline_ = 0;
    bool armed_ = false;
};

}  // namespace picosd::protocol
