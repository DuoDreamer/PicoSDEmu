#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace picosd::protocol {
enum class CodecError { None, InvalidLength, InvalidCharacter, OutputTooSmall };
std::string encode_hex(const std::uint8_t* data, std::size_t size);
CodecError decode_hex(std::string_view input, std::uint8_t* output, std::size_t output_size, std::size_t& written);
std::string encode_base64(const std::uint8_t* data, std::size_t size);
CodecError decode_base64(std::string_view input, std::uint8_t* output, std::size_t output_size, std::size_t& written);
}  // namespace picosd::protocol
