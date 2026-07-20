#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "image_file.hpp"

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
int failures = 0;
void expect(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}
}  // namespace

int main() {
    const auto path = std::filesystem::temp_directory_path() / "picosd-image-file-test.img";
    {
        std::ofstream seed{path, std::ios::binary | std::ios::trunc};
        std::array<char, 1024> zeroes{};
        seed.write(zeroes.data(), zeroes.size());
    }

    std::array<std::uint8_t, 512> written{};
    written[0] = 0xa5U;
    written[511] = 0x5aU;
    std::array<std::uint8_t, 512> readback{};

    picosd::host::ImageFile image;
    expect(image.open(path, true), "opens aligned writable image");
    expect(image.block_count() == 2U, "reports block count");
    expect(image.write_block(1, written.data()), "writes in-range block");
    expect(image.flush(), "flushes write");
    expect(image.read_block(1, readback.data()), "reads in-range block");
    expect(readback == written, "readback matches write");
    expect(!image.read_block(2, readback.data()), "rejects out-of-range read");
    expect(!image.write_block(2, written.data()), "rejects out-of-range write");

#if !defined(_WIN32)
    const pid_t child = fork();
    if (child == 0) {
        picosd::host::ImageFile contender;
        _exit(contender.open(path, true) ? 1 : 0);
    }
    int child_status = 0;
    expect(child > 0 && waitpid(child, &child_status, 0) == child,
           "starts and waits for lock contender");
    expect(WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0,
           "second process cannot acquire image lock");
#endif

    const auto bad_path = std::filesystem::temp_directory_path() / "picosd-image-file-bad.img";
    { std::ofstream bad{bad_path, std::ios::binary | std::ios::trunc}; bad.put('x'); }
    picosd::host::ImageFile bad_image;
    expect(!bad_image.open(bad_path, false), "rejects unaligned image");

    std::filesystem::remove(path);
    std::filesystem::remove(bad_path);
    return failures == 0 ? 0 : 1;
}
