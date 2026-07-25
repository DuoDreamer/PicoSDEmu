#pragma once
#include <filesystem>
#include <string>
namespace picosd::host {
enum class ServeParseResult { NotServe, Invalid, Valid };
struct ServeOptions { std::string port; std::filesystem::path image_path; std::string card_type; bool writable = false; };
ServeParseResult parse_serve_options(int argc, char* argv[], ServeOptions& output);
}  // namespace picosd::host
