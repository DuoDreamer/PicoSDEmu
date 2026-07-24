#include <array>
#include <iostream>
#include "picosd/protocol/codec.hpp"
namespace { int failures=0; void expect(bool ok,const char* s){if(!ok){std::cerr<<"FAIL: "<<s<<'\n';++failures;}} }
int main(){ using namespace picosd::protocol; constexpr std::array<std::uint8_t,3> bytes{{0x00,0xff,0x42}}; std::array<std::uint8_t,4> out{};std::size_t written=0;
expect(encode_hex(bytes.data(),bytes.size())=="00FF42","hex encoding");expect(decode_hex("00ff42",out.data(),out.size(),written)==CodecError::None&&written==3&&out[2]==0x42,"hex decoding");expect(decode_hex("0",out.data(),out.size(),written)==CodecError::InvalidLength,"odd hex rejected");
expect(encode_base64(bytes.data(),bytes.size())=="AP9C","base64 encoding");expect(decode_base64("AP9C",out.data(),out.size(),written)==CodecError::None&&written==3&&out[1]==0xff,"base64 decoding");expect(decode_base64("YQ==",out.data(),out.size(),written)==CodecError::None&&written==1&&out[0]=='a',"base64 padding");expect(decode_base64("Y=Q=",out.data(),out.size(),written)==CodecError::InvalidCharacter,"bad base64 rejected");return failures?1:0; }
