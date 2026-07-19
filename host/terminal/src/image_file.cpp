#include "image_file.hpp"

#include <limits>

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace picosd::host {
namespace { constexpr std::uint64_t kBlockSize = 512; }

ImageFile::~ImageFile() {
#if !defined(_WIN32)
    if (file_descriptor_ >= 0) {
        struct flock lock {};
        lock.l_type = F_UNLCK;
        lock.l_whence = SEEK_SET;
        (void)fcntl(file_descriptor_, F_SETLK, &lock);
        (void)close(file_descriptor_);
    }
#endif
}

bool ImageFile::open(const std::filesystem::path& path, bool writable) {
    const auto size = std::filesystem::file_size(path);
    if (size == 0 || size % kBlockSize != 0 || size / kBlockSize > std::numeric_limits<std::uint64_t>::max()) return false;
#if !defined(_WIN32)
    file_descriptor_ = ::open(path.c_str(), writable ? O_RDWR : O_RDONLY);
    if (file_descriptor_ < 0) return false;
    struct flock lock {};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    if (fcntl(file_descriptor_, F_SETLK, &lock) != 0) { (void)close(file_descriptor_); file_descriptor_ = -1; return false; }
#endif
    stream_.open(path, std::ios::binary | std::ios::in | (writable ? std::ios::out : std::ios::openmode{}));
    if (!stream_) return false;
    block_count_ = size / kBlockSize;
    writable_ = writable;
    return true;
}

bool ImageFile::read_block(std::uint64_t lba, std::uint8_t* output) {
    if (!stream_ || lba >= block_count_) return false;
    stream_.seekg(static_cast<std::streamoff>(lba * kBlockSize)); stream_.read(reinterpret_cast<char*>(output), kBlockSize);
    return stream_.good();
}
bool ImageFile::write_block(std::uint64_t lba, const std::uint8_t* input) {
    if (!stream_ || !writable_ || lba >= block_count_) return false;
    stream_.seekp(static_cast<std::streamoff>(lba * kBlockSize)); stream_.write(reinterpret_cast<const char*>(input), kBlockSize);
    return stream_.good();
}
bool ImageFile::flush() { stream_.flush(); return stream_.good(); }
std::uint64_t ImageFile::block_count() const { return block_count_; }
}  // namespace picosd::host
