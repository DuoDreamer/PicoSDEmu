#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "image_file.hpp"
#include "session_dispatcher.hpp"
#include "picosd/protocol/codec.hpp"
#include "picosd/protocol/crc.hpp"

namespace {
int failures = 0;

void expect(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}
}  // namespace

int main() {
    const auto path = std::filesystem::temp_directory_path() / "picosd-dispatcher.img";
    {
        std::ofstream file{path, std::ios::binary | std::ios::trunc};
        std::array<char, 1536> data{};
        data[0] = 42;
        data[512] = 43;
        data[1024] = 44;
        file.write(data.data(), data.size());
    }

    picosd::host::ImageFile image;
    expect(image.open(path, true), "opens image");
    picosd::host::SessionDispatcher session{image, "SDSC", true, "test-session"};
    expect(session.dispatch("HELLO id=1 id=2 version=0.1") ==
               "ERR id=0 code=BAD_LINE",
           "rejects ambiguous duplicate fields before negotiation");
    expect(session.dispatch("HELLO id=invalid version=0.1") ==
               "ERR id=invalid code=BAD_ID",
           "requires a positive decimal request id");
    expect(session.dispatch("HELLO id=18446744073709551616 version=0.1") ==
               "ERR id=18446744073709551616 code=BAD_ID",
           "rejects an overflowing request id");
    expect(session.dispatch("GET_INFO id=1 session=test-session") ==
               "ERR id=1 code=HANDSHAKE_REQUIRED",
           "requires handshake before media commands");
    expect(session.dispatch("HELLO id=2 version=9.9") ==
               "ERR id=2 code=UNSUPPORTED_VERSION",
           "rejects unsupported protocol version");
    expect(session.dispatch("HELLO id=3 version=0.1") ==
               "OK id=3 version=0.1 session=test-session",
           "negotiates protocol and session");
    expect(session.dispatch("GET_INFO id=3 session=test-session") ==
               "ERR id=3 code=STALE_ID",
           "rejects a duplicate request id");
    expect(session.dispatch("GET_INFO id=2 session=test-session") ==
               "ERR id=2 code=STALE_ID",
           "rejects a decreasing request id");
    expect(session.dispatch("GET_INFO id=4") == "ERR id=4 code=MISSING_SESSION",
           "requires session on established requests");
    expect(session.dispatch("GET_INFO id=999 session=stale") ==
               "ERR id=999 code=BAD_SESSION",
           "rejects stale session without consuming its request id");
    const auto info = session.dispatch("GET_INFO id=6 session=test-session");
    expect(info.find("blocks=3") != std::string::npos &&
               info.find("max_blocks=16") != std::string::npos,
           "returns image metadata and bounded transfer limit");
    expect(session.dispatch(
               "READ_BLOCKS id=7 session=test-session lba=0 count=1 encoding=BASE64")
               .find("data=Kg") != std::string::npos,
           "reads a block for active session");

    std::array<std::uint8_t, 512> block{};
    block[1] = 0xa5U;
    const auto encoded = picosd::protocol::encode_base64(block.data(), block.size());
    char crc[9]{};
    std::snprintf(crc, sizeof(crc), "%08X",
                  picosd::protocol::crc32(block.data(), block.size()));
    const auto write = "WRITE_BLOCKS id=8 session=test-session lba=0 count=1 "
                       "encoding=BASE64 crc32=" + std::string(crc) + " data=" + encoded;
    expect(session.dispatch(write) == "OK id=8 session=test-session", "writes block");
    expect(session.dispatch(
               "READ_BLOCKS id=9 session=test-session lba=0 count=1 encoding=BASE64")
               .find("AKU") != std::string::npos,
           "reads written block");

    std::array<std::uint8_t, 1024> two_blocks{};
    two_blocks[0] = 0x31U;
    two_blocks[512] = 0x32U;
    const auto multi_encoded =
        picosd::protocol::encode_base64(two_blocks.data(), two_blocks.size());
    std::snprintf(crc, sizeof(crc), "%08X",
                  picosd::protocol::crc32(two_blocks.data(), two_blocks.size()));
    const auto multi_write = "WRITE_BLOCKS id=10 session=test-session lba=1 count=2 "
                             "encoding=BASE64 crc32=" + std::string(crc) +
                             " data=" + multi_encoded;
    expect(session.dispatch(multi_write) == "OK id=10 session=test-session",
           "writes a multi-block transfer");
    const auto multi_read = session.dispatch(
        "READ_BLOCKS id=11 session=test-session lba=1 count=2 encoding=BASE64");
    expect(multi_read.find("count=2") != std::string::npos &&
               multi_read.find("data=" + multi_encoded) != std::string::npos,
           "reads a multi-block transfer with one payload checksum");
    expect(session.dispatch(
               "READ_BLOCKS id=12 session=test-session lba=0 count=17 encoding=BASE64") ==
               "ERR id=12 code=RANGE",
           "rejects transfers above the advertised limit");
    expect(session.dispatch("EJECT id=13 session=test-session") ==
               "OK id=13 session=test-session",
           "ejects active session");
    expect(session.dispatch("GET_INFO id=14 session=test-session") ==
               "ERR id=14 code=NO_MEDIA",
           "eject blocks future I/O");
    std::filesystem::remove(path);
    return failures == 0 ? 0 : 1;
}
