#include <cstdint>
#include <iostream>
#include <limits>

#include "picosd/protocol/cdc_retry_controller.hpp"

namespace {
int failures = 0;
void expect(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}
}  // namespace

int main() {
    using namespace picosd::protocol;
    CdcRetryController retry{3, 10, 25};
    expect(retry.record_failure(CdcRetryAdvice::RetrySameSession, 100) ==
               CdcRetryDecision::Wait && retry.retry_at() == 110 &&
               retry.retries_scheduled() == 1,
           "schedules first bounded retry");
    expect(retry.poll(109) == CdcRetryDecision::Wait && !retry.consume_retry(109),
           "waits until first retry deadline");
    expect(retry.poll(110) == CdcRetryDecision::Ready && retry.consume_retry(110),
           "releases retry at deadline");

    expect(retry.record_failure(CdcRetryAdvice::RetrySameSession, 200) ==
               CdcRetryDecision::Wait && retry.retry_at() == 220,
           "doubles second retry delay");
    expect(retry.consume_retry(220), "consumes second retry");
    expect(retry.record_failure(CdcRetryAdvice::RetrySameSession, 300) ==
               CdcRetryDecision::Wait && retry.retry_at() == 325,
           "caps exponential backoff");
    expect(retry.consume_retry(325), "consumes capped retry");
    expect(retry.record_failure(CdcRetryAdvice::RetrySameSession, 400) ==
               CdcRetryDecision::Exhausted,
           "stops after maximum retry count");

    retry.record_success();
    expect(retry.retries_scheduled() == 0 &&
               retry.record_failure(CdcRetryAdvice::RetrySameSession, 500) ==
                   CdcRetryDecision::Wait &&
               retry.retry_at() == 510,
           "success resets retry budget and backoff");
    expect(retry.record_failure(CdcRetryAdvice::Renegotiate, 500) ==
               CdcRetryDecision::Renegotiate && retry.poll(1000) == CdcRetryDecision::NoRetry,
           "routes session failure to renegotiation without stale retry");
    expect(retry.record_failure(CdcRetryAdvice::MediaUnavailable, 500) ==
               CdcRetryDecision::MediaUnavailable,
           "routes missing media separately");
    expect(retry.record_failure(CdcRetryAdvice::DoNotRetry, 500) ==
               CdcRetryDecision::NoRetry,
           "does not schedule terminal errors");

    CdcRetryController saturated{1, 10, 10};
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    expect(saturated.record_failure(CdcRetryAdvice::RetrySameSession, maximum - 5) ==
               CdcRetryDecision::Wait && saturated.retry_at() == maximum &&
               saturated.consume_retry(maximum),
           "saturates retry timestamp overflow");
    CdcRetryController disabled{0, 0, 0};
    expect(disabled.record_failure(CdcRetryAdvice::RetrySameSession, 0) ==
               CdcRetryDecision::Exhausted,
           "supports disabled retry budget");
    return failures == 0 ? 0 : 1;
}
