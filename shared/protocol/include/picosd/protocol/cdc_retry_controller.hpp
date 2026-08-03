#pragma once

#include <cstddef>
#include <cstdint>

#include "picosd/protocol/cdc_session_client.hpp"

namespace picosd::protocol {

enum class CdcRetryDecision {
    NoRetry,
    Wait,
    Ready,
    Exhausted,
    Renegotiate,
    MediaUnavailable,
};

// Bounded exponential-backoff policy. Callers retain the request parameters
// and create a new monotonically numbered request when consume_retry() succeeds.
class CdcRetryController {
public:
    CdcRetryController(std::size_t maximum_retries, std::uint64_t initial_backoff,
                       std::uint64_t maximum_backoff);

    CdcRetryDecision record_failure(CdcRetryAdvice advice, std::uint64_t now);
    [[nodiscard]] CdcRetryDecision poll(std::uint64_t now) const;
    bool consume_retry(std::uint64_t now);
    void record_success();

    [[nodiscard]] std::size_t retries_scheduled() const { return retries_scheduled_; }
    [[nodiscard]] std::uint64_t retry_at() const { return retry_at_; }

private:
    std::size_t maximum_retries_;
    std::uint64_t initial_backoff_;
    std::uint64_t maximum_backoff_;
    std::size_t retries_scheduled_ = 0;
    std::uint64_t retry_at_ = 0;
    bool waiting_ = false;
};

}  // namespace picosd::protocol
