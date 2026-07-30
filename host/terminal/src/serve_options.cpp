#include "serve_options.hpp"
#include <string_view>
namespace picosd::host {
ServeParseResult parse_serve_options(int argc, char* argv[], ServeOptions& output) {
    output = {};
    if (argc < 2 || std::string_view{argv[1]} != "--port") return ServeParseResult::NotServe;
    if (argc != 8) return ServeParseResult::Invalid;
    const std::string_view port{argv[2]}, command{argv[3]}, type_option{argv[5]}, type{argv[6]}, mode{argv[7]};
    if (port.empty() || command != "serve" || type_option != "--type" ||
        (type != "sdsc" && type != "sdhc") || (mode != "--ro" && mode != "--rw")) return ServeParseResult::Invalid;
    output.port = argv[2]; output.image_path = argv[4]; output.card_type = argv[6]; output.writable = mode == "--rw";
    return ServeParseResult::Valid;
}
}  // namespace picosd::host
