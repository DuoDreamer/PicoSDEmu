#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "picosd/protocol/crc.hpp"

namespace {

int failures = 0;

void expect_equal(std::uint32_t actual, std::uint32_t expected, const char* description) {
    if (actual != expected) {
        std::cerr << "FAIL: " << description << ": expected 0x" << std::hex << expected
                  << ", got 0x" << actual << std::dec << '\n';
        ++failures;
    }
}

template <std::size_t Size>
void expect_all_single_bit_mutations_change_crc(const std::array<std::uint8_t, Size>& input,
                                                std::uint32_t original,
                                                std::uint32_t (*calculate)(const std::uint8_t*, std::size_t),
                                                const char* description) {
    for (std::size_t byte_index = 0; byte_index < input.size(); ++byte_index) {
        for (std::uint8_t bit_index = 0; bit_index < 8U; ++bit_index) {
            auto mutated = input;
            mutated[byte_index] = static_cast<std::uint8_t>(mutated[byte_index] ^ (1U << bit_index));
            if (calculate(mutated.data(), mutated.size()) == original) {
                std::cerr << "FAIL: " << description << " did not detect bit mutation at byte "
                          << byte_index << ", bit " << static_cast<unsigned>(bit_index) << '\n';
                ++failures;
            }
        }
    }
}

std::uint32_t crc7_as_u32(const std::uint8_t* data, std::size_t size) {
    return picosd::protocol::crc7(data, size);
}

std::uint32_t crc16_as_u32(const std::uint8_t* data, std::size_t size) {
    return picosd::protocol::crc16(data, size);
}

}  // namespace

int main() {
    constexpr std::array<std::uint8_t, 0> empty{};
    constexpr std::array<std::uint8_t, 9> check_text{{'1', '2', '3', '4', '5', '6', '7', '8', '9'}};
    constexpr std::array<std::uint8_t, 5> cmd0{{0x40U, 0x00U, 0x00U, 0x00U, 0x00U}};
    constexpr std::array<std::uint8_t, 5> cmd8{{0x48U, 0x00U, 0x00U, 0x01U, 0xaaU}};
    constexpr std::array<std::uint8_t, 8> mixed{{0x00U, 0xffU, 0x13U, 0x57U, 0x9bU, 0xdfU, 0x42U, 0x80U}};
    constexpr std::array<std::uint8_t, 8> all_zero{};
    constexpr std::array<std::uint8_t, 8> all_one{{0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU}};

    expect_equal(picosd::protocol::crc7(empty.data(), empty.size()), 0x00U, "CRC7 empty vector");
    expect_equal(picosd::protocol::crc7(cmd0.data(), cmd0.size()), 0x4aU, "CRC7 CMD0 vector");
    expect_equal(picosd::protocol::crc7_command_byte(cmd0.data(), cmd0.size()), 0x95U,
                 "CRC7 CMD0 command byte");
    expect_equal(picosd::protocol::crc7_command_byte(cmd8.data(), cmd8.size()), 0x87U,
                 "CRC7 CMD8 command byte");

    expect_equal(picosd::protocol::crc16(empty.data(), empty.size()), 0x0000U, "CRC16 empty vector");
    expect_equal(picosd::protocol::crc16(check_text.data(), check_text.size()), 0x31c3U,
                 "CRC16 check vector");
    expect_equal(picosd::protocol::crc16(all_zero.data(), all_zero.size()), 0x0000U,
                 "CRC16 all-zero vector");
    expect_equal(picosd::protocol::crc16(all_one.data(), all_one.size()), 0xa6e1U,
                 "CRC16 all-one vector");

    expect_equal(picosd::protocol::crc32(empty.data(), empty.size()), 0x00000000U, "CRC32 empty vector");
    expect_equal(picosd::protocol::crc32(check_text.data(), check_text.size()), 0xcbf43926U,
                 "CRC32 check vector");
    expect_equal(picosd::protocol::crc32(all_zero.data(), all_zero.size()), 0x6522df69U,
                 "CRC32 all-zero vector");
    expect_equal(picosd::protocol::crc32(all_one.data(), all_one.size()), 0x2144df1cU,
                 "CRC32 all-one vector");

    expect_all_single_bit_mutations_change_crc(cmd0, picosd::protocol::crc7(cmd0.data(), cmd0.size()),
                                                crc7_as_u32, "CRC7 CMD0");
    expect_all_single_bit_mutations_change_crc(mixed, picosd::protocol::crc16(mixed.data(), mixed.size()),
                                                crc16_as_u32, "CRC16 mixed vector");
    expect_all_single_bit_mutations_change_crc(mixed, picosd::protocol::crc32(mixed.data(), mixed.size()),
                                                picosd::protocol::crc32, "CRC32 mixed vector");

    if (failures != 0) {
        return 1;
    }

    std::cout << "All CRC tests passed.\n";
    return 0;
}
