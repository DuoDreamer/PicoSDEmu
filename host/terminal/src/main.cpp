#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

#include "cdc_transport.hpp"
#include "image_file.hpp"
#include "picosd/protocol/version.hpp"
#include "serve_options.hpp"
#include "session_dispatcher.hpp"
#include "session_runner.hpp"

namespace {

constexpr std::string_view kApplicationVersion = "0.1.0";

void print_usage(std::ostream &output) {
    output << "Usage: picosd-host [--help] [--version]\n"
              "       picosd-host --port PORT serve IMAGE --type {sdsc|sdhc} --ro|--rw\n"
              "       picosd-host --port PORT mount IMAGE --type {sdsc|sdhc} --ro|--rw\n"
              "       picosd-host --port PORT {status|flush|eject}\n"
              "       picosd-host discover\n"
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

std::vector<std::string> discover_devices() {
    std::vector<std::string> devices;
#if defined(_WIN32)
    // Opening a nonexistent COM name is harmless, and avoids requiring SetupAPI
    // solely for the convenience command. Keep the scan deliberately bounded.
    for (unsigned number = 1; number <= 256; ++number) {
        const auto name = "COM" + std::to_string(number);
        picosd::host::WindowsCdcTransport transport;
        if (transport.open(name) == picosd::host::CdcTransportError::None) {
            devices.push_back(name);
        }
    }
#else
    std::error_code error;
    for (const auto &entry : std::filesystem::directory_iterator{"/dev", error}) {
        const auto name = entry.path().filename().string();
        if (name.rfind("ttyACM", 0) == 0 || name.rfind("ttyUSB", 0) == 0) {
            devices.push_back(entry.path().string());
        }
    }
#endif
    std::sort(devices.begin(), devices.end());
    return devices;
}

template <typename Transport>
int run_control_command(std::string_view port, std::string_view command) {
    Transport transport;
    if (transport.open(port) != picosd::host::CdcTransportError::None) {
        std::cerr << "Could not open USB CDC port: " << port << '\n';
        return 1;
    }
    std::string request;
    if (command == "status")
        request = "GET_INFO id=1";
    else if (command == "flush")
        request = "FLUSH id=1";
    else
        request = "EJECT id=1";
    if (transport.write_line(request) != picosd::host::CdcTransportError::None) {
        std::cerr << "Could not write USB CDC request.\n";
        return 1;
    }
    constexpr auto timeout = std::chrono::seconds{2};
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::string response;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto result = transport.read_line(response);
        if (result == picosd::host::CdcTransportError::None) {
            std::cout << response << '\n';
            return response.rfind("OK ", 0) == 0 ? 0 : 1;
        }
        if (result != picosd::host::CdcTransportError::WouldBlock)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    std::cerr << "No valid response received from USB CDC device.\n";
    return 1;
}

void print_version(std::ostream &output) {
    output << "picosd-host " << kApplicationVersion << '\n'
           << "protocol " << picosd::protocol::kVersionMajor << '.'
           << picosd::protocol::kVersionMinor << '\n';
}

} // namespace

int main(int argc, char *argv[]) {
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
    if (argument == "discover" && argc == 2) {
        const auto devices = discover_devices();
        for (const auto &device : devices)
            std::cout << device << '\n';
        return 0;
    }
    if (argument == "--port" && argc == 4) {
        const std::string_view command{argv[3]};
        if (command == "status" || command == "flush" || command == "eject") {
#if defined(_WIN32)
            return run_control_command<picosd::host::WindowsCdcTransport>(argv[2], command);
#else
            return run_control_command<picosd::host::PosixCdcTransport>(argv[2], command);
#endif
        }
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
        std::cerr << "Could not open and exclusively lock image: " << options.image_path.string()
                  << '\n';
        return 1;
    }

    const auto session_id =
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

#if defined(_WIN32)
    picosd::host::WindowsCdcTransport transport;
    if (transport.open(options.port) != picosd::host::CdcTransportError::None) {
        std::cerr << "Could not open USB CDC port: " << options.port << '\n';
        return 1;
    }
    picosd::host::SessionDispatcher dispatcher{image, options.card_type, options.writable,
                                               session_id};
    while (true) {
        const auto result = picosd::host::process_one_request(transport, dispatcher);
        if (result == picosd::host::SessionRunResult::TransportError)
            return 1;
        if (result == picosd::host::SessionRunResult::NoRequest)
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
#else
    picosd::host::PosixCdcTransport transport;
    if (transport.open(options.port) != picosd::host::CdcTransportError::None) {
        std::cerr << "Could not open USB CDC port: " << options.port << '\n';
        return 1;
    }
    picosd::host::SessionDispatcher dispatcher{image, options.card_type, options.writable,
                                               session_id};
    std::cout << "Serving " << options.image_path.string() << " (" << image.block_count()
              << " blocks) on " << options.port << ".\n";
    while (true) {
        const auto result = picosd::host::process_one_request(transport, dispatcher);
        if (result == picosd::host::SessionRunResult::TransportError) {
            std::cerr << "USB CDC transport disconnected or failed.\n";
            return 1;
        }
        if (result == picosd::host::SessionRunResult::NoRequest) {
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        }
    }
#endif
}
