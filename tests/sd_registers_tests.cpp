#include <iostream>
#include "picosd/protocol/sd_registers.hpp"
namespace { int failures = 0; void expect(bool ok, const char* text) { if (!ok) { std::cerr << "FAIL: " << text << '\n'; ++failures; } } }
int main() {
    using namespace picosd::protocol;
    const auto sdsc = make_sd_registers(SdCardType::Sdsc, 131073U);
    expect(sdsc.exposed_blocks == 131072U, "SDSC rounds down to representable capacity");
    expect(sdsc.ocr[0] == 0U, "SDSC OCR does not set CCS");
    expect(sdsc.csd[0] == 0U, "SDSC CSD structure is zero");
    const auto sdhc = make_sd_registers(SdCardType::Sdhc, 131073U);
    expect(sdhc.exposed_blocks == 131072U, "SDHC rounds down to 1024-block group");
    expect(sdhc.ocr[0] == 0x40U, "SDHC OCR sets CCS");
    expect((sdhc.csd[0] & 0xc0U) == 0x40U, "SDHC CSD structure is one");
    expect(make_sd_registers(SdCardType::Sdhc, 1023U).exposed_blocks == 0U, "too-small SDHC capacity is rejected");
    expect(sdsc.cid[0] == 0x50U && sdsc.cid[7] == 0x30U, "CID is deterministic");
    return failures == 0 ? 0 : 1;
}
