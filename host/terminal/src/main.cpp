#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

#include "image_file.hpp"
#include "serve_options.hpp"
#include "picosd/protocol/version.hpp"

namespace {

constexpr std::string_view kApplicationVersion = "0.1.0";

void print_usage(std::ostream& output) {
    output << "Usage: picosd-host [--help] [--version]\n"
              "       picosd-host --port PORT serve IMAGE --type {sdsc|sdhc} --ro|--rw\n"
              "\n"
              "Console image host for the Raspberry Pi Pico 2 SD emulator.\n"
              "\n"
              "Options:\n"
              "  -h, --help       Show this help text\n"
              "  -v, --version    Show application and protocol versions\n"
              "\n"
              "Serve options:\n"
              "  --port PORT      USB CDC serial port (for example /dev/ttyACM0 or COM3)\n"
              "  --type TYPE      Emulated card type: sdsc or sdhc\n"
              "  --ro, --rw       Reject or permit client writes\n";
}

void print_version(std::ostream& output) {
    output << "picosd-host " << kApplicationVersion << '\n'
           << "protocol " << picosd::protocol::kVersionMajor << '.'
           << picosd::protocol::kVersionMinor << '\n';
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 1) {
        print_usage(std::cout);
        return 0;
    }

    const std::string_view argument{argv[1]};
    if (argument == "--help" || argument == "-h") {
        print_usage(std::cout);
        return 0;
    }
    if (argument == "--version" || argument == "-v") {
        print_version(std::cout);
        return 0;
    }

    picosd::host::ServeOptions options;
    const auto parse_result = picosd::host::parse_serve_options(argc, argv, options);
    if (parse_result != picosd::host::ServeParseResult::Valid) {
        std::cerr << "Invalid command line.\n\n";
        print_usage(std::cerr);
        return 2;
    }

    // USB transport is attached in the next host-backend step. This command
    // already validates the explicit card and access-mode configuration.
    std::error_code error;
    const auto size = std::filesystem::file_size(options.image_path, error);
    if (error || size == 0 || (size % 512U) != 0U) {
        std::cerr << "Image must exist, be non-empty, and have a size divisible by 512 bytes.\n";
        return 1;
    }

    picosd::host::ImageFile image;
    if (!image.open(options.image_path, options.writable)) {
        std::cerr << "Could not open and exclusively lock image: " << options.image_path.string() << '\n';
        return 1;
    }

    std::cout << "Validated " << options.image_path.string() << " (" << image.block_count() << " blocks) for "
              << options.card_type << " on " << options.port << ".\n"
              << "Access mode: " << (options.writable ? "read-write" : "read-only") << ".\n"
              << "USB CDC serving will be enabled when the transport backend is added.\n";
    return 0;
}
