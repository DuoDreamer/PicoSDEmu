#include "picosd/protocol/cdc_request_deadline.hpp"

#include <limits>

namespace picosd::protocol {

bool CdcRequestDeadline::arm(std::uint64_t now, std::uint64_t timeout,
                             const CdcSessionClient& client) {
    if (timeout == 0 || !client.request_pending()) return false;
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    deadline_ = timeout > maximum - now ? maximum : now + timeout;
    armed_ = true;
    return true;
}

bool CdcRequestDeadline::expire_if_due(std::uint64_t now, CdcSessionClient& client) {
    if (!armed_) return false;
    if (!client.request_pending()) {
        clear();
        return false;
    }
    if (now < deadline_) return false;
    const bool cancelled = client.cancel_pending_request();
    clear();
    return cancelled;
}

void CdcRequestDeadline::clear() {
    deadline_ = 0;
    armed_ = false;
}

}  // namespace picosd::protocol
