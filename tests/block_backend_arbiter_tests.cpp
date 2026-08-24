#include <cstdlib>
#include <iostream>

#include "picosd/protocol/block_backend_arbiter.hpp"

namespace {

using picosd::protocol::BlockBackendArbiter;
using picosd::protocol::BlockOperationResult;
using picosd::protocol::RamBlockBackend;
using picosd::protocol::SdBlock;

void expect(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    RamBlockBackend storage{2};
    BlockBackendArbiter arbiter{storage};
    SdBlock first{};
    first[0] = 0xa5;
    SdBlock output{};

    expect(arbiter.owner() == BlockBackendArbiter::Owner::None && !arbiter.media_present() &&
               arbiter.block_count() == 0 && arbiter.write_protected(),
           "an unowned backend is not exposed");
    expect(arbiter.read(0, output) == BlockOperationResult::Failed &&
               arbiter.write(0, first) == BlockOperationResult::Failed &&
               arbiter.flush() == BlockOperationResult::Failed,
           "client operations fail while the medium is hidden");
    expect(!arbiter.host_copy_media_present() && arbiter.host_copy_block_count() == 0 &&
               arbiter.host_copy_write_protected() &&
               arbiter.host_copy_read(0, output) == BlockOperationResult::Failed,
           "copy operations fail without copy ownership");

    expect(arbiter.expose_to_client() && arbiter.media_present() && arbiter.block_count() == 2,
           "the client can exclusively expose the medium");
    expect(!arbiter.expose_to_client() && !arbiter.begin_host_copy() && !arbiter.end_host_copy(),
           "copy ownership cannot overlap client exposure");
    expect(arbiter.write(0, first) == BlockOperationResult::Complete &&
               arbiter.read(0, output) == BlockOperationResult::Complete && output == first &&
               arbiter.flush() == BlockOperationResult::Complete,
           "client ownership forwards block operations");

    expect(arbiter.hide_from_client() && !arbiter.hide_from_client() && arbiter.begin_host_copy(),
           "copy ownership starts only after client ejection");
    expect(!arbiter.expose_to_client() && !arbiter.begin_host_copy() && !arbiter.hide_from_client(),
           "client exposure cannot overlap a host copy");
    first[0] = 0x5a;
    expect(arbiter.host_copy_media_present() && arbiter.host_copy_block_count() == 2 &&
               !arbiter.host_copy_write_protected() &&
               arbiter.host_copy_write(1, first) == BlockOperationResult::Complete &&
               arbiter.host_copy_read(1, output) == BlockOperationResult::Complete &&
               output == first && arbiter.host_copy_flush() == BlockOperationResult::Complete,
           "copy ownership forwards host-directed block operations");
    expect(arbiter.read(1, output) == BlockOperationResult::Failed &&
               arbiter.write(1, first) == BlockOperationResult::Failed,
           "emulated-client I/O is blocked for the entire copy");
    expect(arbiter.end_host_copy() && !arbiter.end_host_copy() && arbiter.expose_to_client(),
           "ending a copy permits a later client exposure");
    expect(arbiter.read(1, output) == BlockOperationResult::Complete && output == first,
           "copy results become visible after the medium is exposed again");
}
