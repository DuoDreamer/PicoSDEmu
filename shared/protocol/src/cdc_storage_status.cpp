#include "picosd/protocol/cdc_storage_status.hpp"

namespace picosd::protocol {

CdcStorageStatus cdc_storage_status(CdcOperationState state) {
    switch (state) {
        case CdcOperationState::Idle:
            return {CdcStorageAvailability::Ready, false, false};
        case CdcOperationState::AwaitingResponse:
        case CdcOperationState::WaitingRetry:
        case CdcOperationState::RetryReady:
            return {CdcStorageAvailability::Busy, true, true};
        case CdcOperationState::Renegotiate:
            return {CdcStorageAvailability::Reconnecting, true, true};
        case CdcOperationState::MediaUnavailable:
            return {CdcStorageAvailability::MediaUnavailable, false, true};
        case CdcOperationState::Failed:
            return {CdcStorageAvailability::Failed, false, false};
    }
    return {CdcStorageAvailability::Failed, false, false};
}

}  // namespace picosd::protocol
