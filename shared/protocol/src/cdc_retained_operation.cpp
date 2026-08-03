#include "picosd/protocol/cdc_retained_operation.hpp"

namespace picosd::protocol {

void CdcRetainedOperation::retain_get_info() {
    clear();
    type_ = CdcOperationType::GetInfo;
}

void CdcRetainedOperation::retain_read_block(std::uint64_t lba) {
    clear();
    type_ = CdcOperationType::ReadBlock;
    lba_ = lba;
}

void CdcRetainedOperation::retain_write_block(std::uint64_t lba,
                                              const CdcBlockData& data) {
    type_ = CdcOperationType::WriteBlock;
    lba_ = lba;
    data_ = data;
}

void CdcRetainedOperation::retain_flush() {
    clear();
    type_ = CdcOperationType::Flush;
}

void CdcRetainedOperation::retain_eject() {
    clear();
    type_ = CdcOperationType::Eject;
}

void CdcRetainedOperation::clear() {
    type_ = CdcOperationType::None;
    lba_ = 0;
    data_ = {};
}

CdcSessionClientRequest CdcRetainedOperation::issue(CdcSessionClient& client) const {
    switch (type_) {
        case CdcOperationType::GetInfo:
            return client.begin_get_info();
        case CdcOperationType::ReadBlock:
            return client.begin_read_block(lba_);
        case CdcOperationType::WriteBlock:
            return client.begin_write_block(lba_, data_);
        case CdcOperationType::Flush:
            return client.begin_flush();
        case CdcOperationType::Eject:
            return client.begin_eject();
        case CdcOperationType::None:
            return {CdcSessionClientError::InvalidRequest, {}};
    }
    return {CdcSessionClientError::InvalidRequest, {}};
}

}  // namespace picosd::protocol
