#include <iostream>
#include <string>

#include "picosd/protocol/text_line.hpp"

namespace {

int failures = 0;

void expect(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

void expect_error(std::string_view input, picosd::protocol::TextLineError expected,
                  const char* description) {
    picosd::protocol::TextLine line;
    expect(picosd::protocol::parse_text_line(input, line) == expected, description);
}

}  // namespace

int main() {
    picosd::protocol::TextLine line;
    const auto error = picosd::protocol::parse_text_line(
        "READ_BLOCKS id=42 lba=7 count=1 encoding=BASE64", line);

    expect(error == picosd::protocol::TextLineError::None, "valid request is accepted");
    expect(line.token_count == 5, "valid request has five tokens");
    expect(line.command() == "READ_BLOCKS", "command is available");
    expect(line.value_for("id") == "42", "id value is available");
    expect(line.value_for("lba") == "7", "lba value is available");
    expect(line.value_for("missing").empty(), "missing value is empty");

    expect_error("WRITE_BLOCKS id=3 data=YWJjZA==", picosd::protocol::TextLineError::None,
                 "padded base64 value is accepted");

    expect_error("  GET_INFO\t id=2  ", picosd::protocol::TextLineError::None,
                 "space and tab separators are accepted");
    expect_error("", picosd::protocol::TextLineError::Empty, "empty input is rejected");
    expect_error("   \t", picosd::protocol::TextLineError::Empty, "blank input is rejected");
    expect_error("GET_INFO\n", picosd::protocol::TextLineError::ContainsLineTerminator,
                 "newline is rejected");
    expect_error("GET_INFO id=1\r", picosd::protocol::TextLineError::ContainsLineTerminator,
                 "carriage return is rejected");
    expect_error("GET_INFO id", picosd::protocol::TextLineError::InvalidCharacter,
                 "field without equals is rejected");
    expect_error("GET_INFO =value", picosd::protocol::TextLineError::InvalidCharacter,
                 "field without key is rejected");
    expect_error("GET_INFO id=", picosd::protocol::TextLineError::InvalidCharacter,
                 "field without value is rejected");
    expect_error("GET=INFO id=1", picosd::protocol::TextLineError::InvalidCharacter,
                 "command containing equals is rejected");
    expect_error(std::string{"GET_INFO id=1\x01"}, picosd::protocol::TextLineError::InvalidCharacter,
                 "control character is rejected");

    std::string too_many_tokens{"STATUS"};
    for (int index = 0; index < 16; ++index) {
        too_many_tokens += " k" + std::to_string(index) + "=v";
    }
    expect_error(too_many_tokens, picosd::protocol::TextLineError::TooManyTokens,
                 "seventeenth token is rejected");

    if (failures != 0) {
        return 1;
    }

    std::cout << "All text-line parser tests passed.\n";
    return 0;
}
