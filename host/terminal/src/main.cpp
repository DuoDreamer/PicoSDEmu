#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

#include "image_file.hpp"
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

    if (argument != "--port" || argc != 8) {
        std::cerr << "Invalid command line.\n\n";
        print_usage(std::cerr);
        return 2;
    }

    const std::string_view port{argv[2]};
    const std::string_view command{argv[3]};
    const std::filesystem::path image_path{argv[4]};
    const std::string_view type_option{argv[5]};
    const std::string_view type{argv[6]};
    const std::string_view access_mode{argv[7]};

    if (port.empty() || command != "serve" || type_option != "--type" ||
        (type != "sdsc" && type != "sdhc") || (access_mode != "--ro" && access_mode != "--rw")) {
        std::cerr << "Invalid serve options.\n\n";
        print_usage(std::cerr);
        return 2;
    }

    // USB transport is attached in the next host-backend step. This command
    // already validates the explicit card and access-mode configuration.
    std::error_code error;
    const auto size = std::filesystem::file_size(image_path, error);
    if (error || size == 0 || (size % 512U) != 0U) {
        std::cerr << "Image must exist, be non-empty, and have a size divisible by 512 bytes.\n";
        return 1;
    }

    picosd::host::ImageFile image;
    if (!image.open(image_path, access_mode == "--rw")) {
        std::cerr << "Could not open and exclusively lock image: " << image_path.string() << '\n';
        return 1;
    }

    std::cout << "Validated " << image_path.string() << " (" << image.block_count() << " blocks) for "
              << type << " on " << port << ".\n"
              << "Access mode: " << (access_mode == "--rw" ? "read-write" : "read-only") << ".\n"
              << "USB CDC serving will be enabled when the transport backend is added.\n";
    return 0;
}
