#include "picosd/cdc_sd_backend.hpp"

#include <algorithm>
#include <limits>

#include "picosd/cdc_backend_service.hpp"

namespace picosd::firmware {

void CdcSdBackend::refresh_media() {
    const auto &media = cdc_media_info();
    const bool ready = cdc_media_ready();
    const bool same_medium = present_ && ready && media.present && generation_ == media.generation;
    present_ = ready && media.present;
    blocks_ = present_ ? media.blocks : 0;
    generation_ = present_ ? media.generation : 0;
    read_only_ = !present_ || media.read_only;
    if (!same_medium)
        write_pending_ = false;
}

std::size_t CdcSdBackend::block_count() const {
    const auto maximum = static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max());
    return static_cast<std::size_t>(std::min(blocks_, maximum));
}

bool CdcSdBackend::address_valid(std::size_t lba) const {
    return present_ && static_cast<std::uint64_t>(lba) < blocks_;
}

bool CdcSdBackend::read(std::size_t lba, picosd::protocol::SdBlock &output) const {
    if (!address_valid(lba))
        return false;
    if (copy_cdc_ready(lba, generation_, output))
        return true;
    (void)begin_cdc_read(lba, generation_);
    return false;
}

bool CdcSdBackend::write(std::size_t lba, const picosd::protocol::SdBlock &input) {
    if (read_only_ || !address_valid(lba))
        return false;

    if (write_pending_) {
        if (pending_write_lba_ != lba || pending_write_data_ != input)
            return false;
        picosd::protocol::CdcBlockData ready{};
        if (!copy_cdc_ready(lba, generation_, ready) || ready != input)
            return false;
        write_pending_ = false;
        return true;
    }

    if (!begin_cdc_write(lba, generation_, input))
        return false;
    pending_write_lba_ = lba;
    pending_write_data_ = input;
    write_pending_ = true;
    return false;
}

} // namespace picosd::firmware
