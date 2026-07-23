#include <iostream>
#include "picosd/protocol/crc.hpp"
#include "picosd/protocol/sd_model.hpp"
namespace { int failures = 0; void expect(bool ok, const char* s) { if (!ok) { std::cerr << "FAIL: " << s << '\n'; ++failures; } }
picosd::protocol::SdCommand cmd(unsigned i, unsigned a = 0) { return {static_cast<std::uint8_t>(i), a, 0}; } }
int main() {
    using namespace picosd::protocol;
    RamBlockBackend store{16}; store.fill_diagnostic_pattern(); SdCardModel card{SdCardType::Sdsc, store};
    expect(card.execute(cmd(0)).response.bytes[0] == 1U, "CMD0 enters idle");
    expect(card.execute(cmd(13)).response.type == SdResponseType::R2, "CMD13 returns R2 status");
    expect(!card.command_crc_enabled(), "command CRC starts disabled");
    expect(card.execute(cmd(59, 1)).response.bytes[0] == 1U && card.command_crc_enabled(),
           "CMD59 enables command CRC while idle");
    expect(card.execute(cmd(8, 0x1aaU)).response.type == SdResponseType::R7, "CMD8 returns R7");
    card.execute(cmd(55)); expect(card.execute(cmd(41)).response.bytes[0] == 0U, "ACMD41 leaves idle");
    expect(card.execute(cmd(58)).response.type == SdResponseType::R3, "CMD58 returns OCR");
    card.execute(cmd(55));
    expect(card.execute(cmd(23, 4)).response.bytes[0] == 0U, "ACMD23 accepts preerase count");
    expect(card.execute(cmd(59, 0)).response.bytes[0] == 0U && !card.command_crc_enabled(),
           "CMD59 disables command CRC after initialization");
    expect(card.execute(cmd(16, 512)).response.bytes[0] == 0U, "CMD16 accepts 512-byte blocks");
    const auto csd = card.execute(cmd(9));
    expect(csd.has_register_data && csd.register_data == card.registers().csd, "CMD9 returns CSD");
    const auto cid = card.execute(cmd(10));
    expect(cid.has_register_data && cid.register_data == card.registers().cid, "CMD10 returns CID");
    auto read = card.execute(cmd(17, 512)); expect(read.has_read_block && read.read_block[0] == 37U, "SDSC CMD17 uses byte addressing");
    expect(card.execute(cmd(12)).response.bytes[0] == 0U, "CMD12 stops transfer cleanly");
    expect(card.execute(cmd(17, 1)).response.bytes[0] == static_cast<std::uint8_t>(SdR1::AddressError), "unaligned SDSC address rejected");
    card.execute(cmd(24, 0)); SdBlock block{}; block[4] = 0xa5U;
    expect(card.write_block(block, crc16(block.data(), block.size())).bytes[0] == 0U, "CMD24 data writes");
    expect(card.execute(cmd(17, 0)).read_block[4] == 0xa5U, "written data reads back");

    RamBlockBackend high_capacity_store{16};
    SdCardModel high_capacity{SdCardType::Sdhc, high_capacity_store};
    high_capacity.execute(cmd(0));
    high_capacity.execute(cmd(55));
    high_capacity.execute(cmd(41));
    expect(high_capacity.execute(cmd(16, 512)).response.bytes[0] == 0U,
           "SDHC accepts CMD16 with the fixed block length");
    if (failures != 0) {
        return 1;
    }
    std::cout << "All SD model tests passed.\n";
    return 0;
}
