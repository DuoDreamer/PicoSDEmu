#include <cstdlib>
#include <iostream>
#include "picosd/protocol/cdc_media_info.hpp"
namespace { void expect(bool value, const char *message) { if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); } } }
int main() {
    using namespace picosd::protocol;
    CdcMediaInfo info;
    expect(decode_cdc_media_info("OK id=2 session=s present=1 type=SDHC blocks=8192 block_size=512 readonly=0 generation=7", info) == CdcMediaInfoError::None, "mounted media info decodes");
    expect(info.present && !info.read_only && info.blocks == 8192 && info.generation == 7, "mounted fields are retained");
    expect(decode_cdc_media_info("OK present=0 blocks=0 block_size=512 readonly=1", info) == CdcMediaInfoError::None && !info.present && info.blocks == 0, "absent media decodes");
    expect(decode_cdc_media_info("ERR id=2 code=NO_MEDIA", info) == CdcMediaInfoError::InvalidLine, "error response is rejected");
    expect(decode_cdc_media_info("OK present=1 blocks=8 block_size=4096 readonly=0", info) == CdcMediaInfoError::UnsupportedBlockSize, "unsupported block size is rejected");
    expect(decode_cdc_media_info("OK present=1 blocks=0 block_size=512 readonly=0", info) == CdcMediaInfoError::InvalidField, "empty present media is rejected");
    expect(decode_cdc_media_info("OK present=yes blocks=8 block_size=512 readonly=0", info) == CdcMediaInfoError::InvalidField, "booleans are strict");
    expect(decode_cdc_media_info("OK present=1 blocks=8 block_size=512", info) == CdcMediaInfoError::MissingField, "required fields cannot be omitted");
    std::cout << "cdc media info tests passed\n";
}
