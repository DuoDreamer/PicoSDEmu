#include <cstdint>
#include <iostream>

#include "picosd/protocol/cdc_sector_buffer_pool.hpp"

namespace {
int failures = 0;
void expect(bool condition, const char *description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}
} // namespace

int main() {
    using namespace picosd::protocol;
    CdcSectorBufferPool<2> pool;
    CdcBlockData read_data{};
    read_data[0] = 0x42;
    CdcBlockData write_data{};
    write_data[511] = 0xa5;

    const auto read = pool.reserve_read(7, 3);
    const auto write = pool.reserve_write(8, 3, write_data);
    expect(read != pool.kInvalidHandle && write != pool.kInvalidHandle,
           "fixed slots can be reserved");
    expect(pool.available() == 0 && pool.reserve_read(9, 3) == pool.kInvalidHandle,
           "pool exhaustion is bounded");
    expect(pool.get(write) != nullptr && pool.get(write)->data[511] == 0xa5,
           "write payload is retained before USB acknowledgement");

    expect(!pool.complete_read(read, 7, 4, read_data), "a stale generation cannot complete a read");
    expect(pool.complete_read(read, 7, 3, read_data), "matching read completes");
    expect(pool.find_ready(7, 3) == read && pool.get(read)->data[0] == 0x42,
           "completed read is discoverable by LBA and generation");
    expect(pool.complete_write(write, 8, 3), "write becomes ready only after acknowledgement");

    pool.release_generation(3);
    expect(pool.available() == 2 && pool.find_ready(7, 3) == pool.kInvalidHandle,
           "media generation invalidation releases every matching slot");
    expect(!pool.release(read), "released handles cannot be released twice");
    return failures == 0 ? 0 : 1;
}
