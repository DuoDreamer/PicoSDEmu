#include <iostream>
#include <string_view>

#include "picosd/protocol/version.hpp"

namespace {

constexpr std::string_view kApplicationVersion = "0.1.0";

void print_usage(std::ostream& output) {
    output << "Usage: picosd-host [--help] [--version]\n"
              "\n"
              "Console image host for the Raspberry Pi Pico 2 SD emulator.\n"
              "\n"
              "Options:\n"
              "  -h, --help       Show this help text\n"
              "  -v, --version    Show application and protocol versions\n";
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

    std::cerr << "Unknown option: " << argument << "\n\n";
    print_usage(std::cerr);
    return 2;
}

