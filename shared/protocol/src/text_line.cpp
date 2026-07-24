#include "picosd/protocol/text_line.hpp"

namespace picosd::protocol {
namespace {

bool is_separator(char character) {
    return character == ' ' || character == '\t';
}

bool is_printable_ascii(char character) {
    const unsigned char value = static_cast<unsigned char>(character);
    return value >= 0x21U && value <= 0x7eU;
}

bool is_valid_token(std::string_view token, bool is_command) {
    for (const char character : token) {
        if (!is_printable_ascii(character)) {
            return false;
        }
    }

    if (is_command) {
        return token.find('=') == std::string_view::npos;
    }

    const std::size_t equals_position = token.find('=');
    return equals_position != std::string_view::npos && equals_position != 0 &&
           equals_position + 1 < token.size();
}

}  // namespace

std::string_view TextLine::command() const {
    return token_count == 0 ? std::string_view{} : tokens[0];
}

std::string_view TextLine::value_for(std::string_view key) const {
    for (std::size_t index = 1; index < token_count; ++index) {
        const std::string_view token = tokens[index];
        const std::size_t separator = token.find('=');
        if (token.substr(0, separator) == key) {
            return token.substr(separator + 1);
        }
    }
    return {};
}

TextLineError parse_text_line(std::string_view input, TextLine& output) {
    output = {};

    if (input.empty()) {
        return TextLineError::Empty;
    }

    std::size_t position = 0;
    while (position < input.size()) {
        if (input[position] == '\r' || input[position] == '\n') {
            return TextLineError::ContainsLineTerminator;
        }
        if (is_separator(input[position])) {
            ++position;
            continue;
        }

        const std::size_t token_start = position;
        while (position < input.size() && !is_separator(input[position])) {
            const char character = input[position];
            if (character == '\r' || character == '\n') {
                return TextLineError::ContainsLineTerminator;
            }
            ++position;
        }

        if (output.token_count == output.tokens.size()) {
            return TextLineError::TooManyTokens;
        }

        const std::string_view token = input.substr(token_start, position - token_start);
        if (!is_valid_token(token, output.token_count == 0)) {
            return TextLineError::InvalidCharacter;
        }
        output.tokens[output.token_count] = token;
        ++output.token_count;
    }

    return output.token_count == 0 ? TextLineError::Empty : TextLineError::None;
}

}  // namespace picosd::protocol
