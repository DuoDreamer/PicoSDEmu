#pragma once

#include "picosd/protocol/cdc_operation_coordinator.hpp"

namespace picosd::protocol {

enum class CdcStorageAvailability {
    Ready,
    Busy,
    Reconnecting,
    MediaUnavailable,
    Failed,
};

struct CdcStorageStatus {
    CdcStorageAvailability availability = CdcStorageAvailability::Failed;
    bool assert_busy = false;
    bool retryable = false;
};

[[nodiscard]] CdcStorageStatus cdc_storage_status(CdcOperationState state);

}  // namespace picosd::protocol
