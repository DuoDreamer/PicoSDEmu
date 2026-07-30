#include "picosd/protocol/cdc_retry_controller.hpp"

#include <limits>

namespace picosd::protocol {
namespace {

std::uint64_t saturating_add(std::uint64_t left, std::uint64_t right) {
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    return right > maximum - left ? maximum : left + right;
}

std::uint64_t bounded_backoff(std::uint64_t initial, std::uint64_t maximum,
                              std::size_t retry_index) {
    std::uint64_t delay = initial;
    while (retry_index-- != 0 && delay < maximum) {
        delay = delay > maximum - delay ? maximum : delay * 2;
    }
    return delay > maximum ? maximum : delay;
}

}  // namespace

CdcRetryController::CdcRetryController(std::size_t maximum_retries,
                                       std::uint64_t initial_backoff,
                                       std::uint64_t maximum_backoff)
    : maximum_retries_(maximum_retries),
      initial_backoff_(initial_backoff == 0 ? 1 : initial_backoff),
      maximum_backoff_(maximum_backoff < initial_backoff_ ? initial_backoff_
                                                          : maximum_backoff) {}

CdcRetryDecision CdcRetryController::record_failure(CdcRetryAdvice advice,
                                                    std::uint64_t now) {
    waiting_ = false;
    retry_at_ = 0;
    if (advice == CdcRetryAdvice::Renegotiate) return CdcRetryDecision::Renegotiate;
    if (advice == CdcRetryAdvice::MediaUnavailable) {
        return CdcRetryDecision::MediaUnavailable;
    }
    if (advice != CdcRetryAdvice::RetrySameSession) return CdcRetryDecision::NoRetry;
    if (retries_scheduled_ >= maximum_retries_) return CdcRetryDecision::Exhausted;

    const auto delay = bounded_backoff(initial_backoff_, maximum_backoff_,
                                       retries_scheduled_);
    ++retries_scheduled_;
    retry_at_ = saturating_add(now, delay);
    waiting_ = true;
    return CdcRetryDecision::Wait;
}

CdcRetryDecision CdcRetryController::poll(std::uint64_t now) const {
    if (!waiting_) return CdcRetryDecision::NoRetry;
    return now < retry_at_ ? CdcRetryDecision::Wait : CdcRetryDecision::Ready;
}

bool CdcRetryController::consume_retry(std::uint64_t now) {
    if (poll(now) != CdcRetryDecision::Ready) return false;
    waiting_ = false;
    retry_at_ = 0;
    return true;
}

void CdcRetryController::record_success() {
    retries_scheduled_ = 0;
    waiting_ = false;
    retry_at_ = 0;
}

}  // namespace picosd::protocol
