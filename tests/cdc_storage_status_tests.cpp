#include <iostream>

#include "picosd/protocol/cdc_storage_status.hpp"

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
    const auto idle = cdc_storage_status(CdcOperationState::Idle);
    expect(idle.availability == CdcStorageAvailability::Ready &&
               !idle.assert_busy && !idle.retryable,
           "idle storage is ready");

    for (const auto state : {CdcOperationState::AwaitingResponse,
                             CdcOperationState::WaitingRetry,
                             CdcOperationState::RetryReady}) {
        const auto pending = cdc_storage_status(state);
        expect(pending.availability == CdcStorageAvailability::Busy &&
                   pending.assert_busy && pending.retryable,
               "pending operations keep the SD client busy and are retryable");
    }

    const auto reconnecting = cdc_storage_status(CdcOperationState::Renegotiate);
    expect(reconnecting.availability == CdcStorageAvailability::Reconnecting &&
               reconnecting.assert_busy && reconnecting.retryable,
           "session renegotiation preserves SD busy state");

    const auto no_media = cdc_storage_status(CdcOperationState::MediaUnavailable);
    expect(no_media.availability == CdcStorageAvailability::MediaUnavailable &&
               !no_media.assert_busy && no_media.retryable,
           "missing media is exposed without an indefinite busy state");

    const auto failed = cdc_storage_status(CdcOperationState::Failed);
    expect(failed.availability == CdcStorageAvailability::Failed &&
               !failed.assert_busy && !failed.retryable,
           "terminal failures are not advertised as retryable");
    return failures == 0 ? 0 : 1;
}
