#include "picosd/protocol/block_backend_arbiter.hpp"

namespace picosd::protocol {

BlockBackendArbiter::BlockBackendArbiter(BlockBackend &backend) : backend_(backend) {}

BlockBackendArbiter::Owner BlockBackendArbiter::owner() const {
    return owner_.load(std::memory_order_acquire);
}

bool BlockBackendArbiter::transition(Owner expected, Owner desired) {
    return owner_.compare_exchange_strong(expected, desired, std::memory_order_acq_rel);
}

bool BlockBackendArbiter::expose_to_client() {
    return transition(Owner::None, Owner::EmulatedClient);
}

bool BlockBackendArbiter::hide_from_client() {
    return transition(Owner::EmulatedClient, Owner::None);
}

bool BlockBackendArbiter::begin_host_copy() {
    return transition(Owner::None, Owner::HostCopy);
}

bool BlockBackendArbiter::end_host_copy() {
    return transition(Owner::HostCopy, Owner::None);
}

std::size_t BlockBackendArbiter::block_count() const {
    return owner() == Owner::EmulatedClient ? backend_.block_count() : 0;
}

bool BlockBackendArbiter::media_present() const {
    return owner() == Owner::EmulatedClient && backend_.media_present();
}

bool BlockBackendArbiter::write_protected() const {
    return owner() != Owner::EmulatedClient || backend_.write_protected();
}

BlockOperationResult BlockBackendArbiter::flush() {
    return owner() == Owner::EmulatedClient ? backend_.flush() : BlockOperationResult::Failed;
}

BlockOperationResult BlockBackendArbiter::read(std::size_t lba, SdBlock &output) const {
    return owner() == Owner::EmulatedClient ? backend_.read(lba, output)
                                            : BlockOperationResult::Failed;
}

BlockOperationResult BlockBackendArbiter::write(std::size_t lba, const SdBlock &input) {
    return owner() == Owner::EmulatedClient ? backend_.write(lba, input)
                                            : BlockOperationResult::Failed;
}

std::size_t BlockBackendArbiter::host_copy_block_count() const {
    return owner() == Owner::HostCopy ? backend_.block_count() : 0;
}

bool BlockBackendArbiter::host_copy_media_present() const {
    return owner() == Owner::HostCopy && backend_.media_present();
}

bool BlockBackendArbiter::host_copy_write_protected() const {
    return owner() != Owner::HostCopy || backend_.write_protected();
}

BlockOperationResult BlockBackendArbiter::host_copy_flush() {
    return owner() == Owner::HostCopy ? backend_.flush() : BlockOperationResult::Failed;
}

BlockOperationResult BlockBackendArbiter::host_copy_read(std::size_t lba, SdBlock &output) const {
    return owner() == Owner::HostCopy ? backend_.read(lba, output) : BlockOperationResult::Failed;
}

BlockOperationResult BlockBackendArbiter::host_copy_write(std::size_t lba, const SdBlock &input) {
    return owner() == Owner::HostCopy ? backend_.write(lba, input) : BlockOperationResult::Failed;
}

} // namespace picosd::protocol
