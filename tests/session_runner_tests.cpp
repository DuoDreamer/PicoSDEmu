#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "cdc_transport.hpp"
#include "image_file.hpp"
#include "session_dispatcher.hpp"
#include "session_runner.hpp"

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
    const auto path = std::filesystem::temp_directory_path() / "picosd-runner.img";
    {
        std::ofstream file{path, std::ios::binary | std::ios::trunc};
        std::array<char, 512> contents{};
        file.write(contents.data(), contents.size());
    }
    picosd::host::ImageFile image;
    expect(image.open(path, true), "opens image");
    picosd::host::SessionDispatcher dispatcher{image, "SDSC", true, "runner-session"};
    picosd::host::MemoryCdcTransport transport;
    transport.open("test");
    transport.inject_received_line("HELLO id=1 version=0.1");
    expect(picosd::host::process_one_request(transport, dispatcher) ==
               picosd::host::SessionRunResult::Processed,
           "processes handshake");
    expect(transport.written_lines().back() ==
               "OK id=1 version=0.1 session=runner-session",
           "writes negotiated session");
    expect(picosd::host::process_one_request(transport, dispatcher) ==
               picosd::host::SessionRunResult::NoRequest,
           "reports empty transport");
    std::filesystem::remove(path);
    return failures == 0 ? 0 : 1;
}
