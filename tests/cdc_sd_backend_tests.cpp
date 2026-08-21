#include <cstdint>
#include <cstdio>

#include "picosd/cdc_backend_service.hpp"
#include "picosd/cdc_sd_backend.hpp"

namespace {
bool media_ready = false;
picosd::protocol::CdcMediaInfo media{};
bool cached = false;
picosd::protocol::CdcBlockData cached_data{};
std::uint64_t requested_lba = UINT64_MAX;
std::uint64_t requested_generation = UINT64_MAX;
unsigned int reads = 0;
unsigned int writes = 0;
} // namespace

namespace picosd::firmware {
bool cdc_media_ready() {
    return media_ready;
}
const picosd::protocol::CdcMediaInfo &cdc_media_info() {
    return media;
}
bool begin_cdc_read(std::uint64_t lba, std::uint64_t generation) {
    requested_lba = lba;
    requested_generation = generation;
    ++reads;
    return true;
}
bool begin_cdc_write(std::uint64_t lba, std::uint64_t generation,
                     const picosd::protocol::CdcBlockData &data) {
    requested_lba = lba;
    requested_generation = generation;
    cached_data = data;
    ++writes;
    return true;
}
bool copy_cdc_ready(std::uint64_t lba, std::uint64_t generation,
                    picosd::protocol::CdcBlockData &output) {
    if (!cached || lba != requested_lba || generation != requested_generation)
        return false;
    output = cached_data;
    return true;
}
} // namespace picosd::firmware

int main() {
    picosd::firmware::CdcSdBackend backend;
    backend.refresh_media();
    if (backend.block_count() != 0)
        return 1;

    media_ready = true;
    media = {true, false, 32, 7};
    backend.refresh_media();
    if (backend.block_count() != 32)
        return 2;

    picosd::protocol::SdBlock block{};
    if (backend.read(4, block) || reads != 1 || requested_lba != 4 || requested_generation != 7)
        return 3;
    cached_data.fill(0xa5U);
    cached = true;
    if (!backend.read(4, block) || block != cached_data || reads != 1)
        return 4;

    block.fill(0x3cU);
    // A matching cached read is not a write acknowledgement.
    cached_data = block;
    if (backend.write(5, block) || writes != 1 || requested_lba != 5)
        return 5;
    cached = true;
    if (!backend.write(5, block) || writes != 1)
        return 6;
    if (backend.read(32, block) || backend.write(32, block))
        return 7;

    std::puts("cdc sd backend tests passed");
    return 0;
}
