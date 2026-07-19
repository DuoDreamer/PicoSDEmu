#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace picosd::host {

class ImageFile {
public:
    ImageFile() = default;
    ~ImageFile();
    ImageFile(const ImageFile&) = delete;
    ImageFile& operator=(const ImageFile&) = delete;

    bool open(const std::filesystem::path& path, bool writable);
    bool read_block(std::uint64_t lba, std::uint8_t* output);
    bool write_block(std::uint64_t lba, const std::uint8_t* input);
    bool flush();
    [[nodiscard]] std::uint64_t block_count() const;

private:
    std::fstream stream_;
    std::uint64_t block_count_ = 0;
    bool writable_ = false;
    int file_descriptor_ = -1;
};

}  // namespace picosd::host
