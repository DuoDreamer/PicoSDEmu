#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace picosd::protocol {

inline constexpr std::size_t kMaxTextLineTokens = 16;

enum class TextLineError {
    None,
    Empty,
    ContainsLineTerminator,
    InvalidCharacter,
    TooManyTokens,
};

struct TextLine {
    std::array<std::string_view, kMaxTextLineTokens> tokens{};
    std::size_t token_count = 0;

    [[nodiscard]] std::string_view command() const;
    [[nodiscard]] std::string_view value_for(std::string_view key) const;
};

// Parses one complete CDC protocol line without its trailing newline. Tokens
// are separated by ASCII space or tab. A token is either a command or key=value
// field and consists only of printable ASCII characters other than '=' for a
// command, or printable ASCII characters with a nonempty key and value for a
// field. A field value may contain '=' so padded base64 is representable.
TextLineError parse_text_line(std::string_view input, TextLine& output);

}  // namespace picosd::protocol
