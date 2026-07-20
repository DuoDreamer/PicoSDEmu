#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "image_file.hpp"
#include "session_dispatcher.hpp"
#include "picosd/protocol/codec.hpp"
#include "picosd/protocol/crc.hpp"
namespace { int failures=0; void expect(bool b,const char*s){if(!b){std::cerr<<"FAIL: "<<s<<'\n';++failures;}} }
int main(){const auto path=std::filesystem::temp_directory_path()/"picosd-dispatcher.img";{std::ofstream file{path,std::ios::binary|std::ios::trunc};std::array<char,512> data{};data[0]=42;file.write(data.data(),data.size());}
picosd::host::ImageFile image;expect(image.open(path,true),"opens image");picosd::host::SessionDispatcher session{image,"SDSC",true};expect(session.dispatch("HELLO id=1")=="OK id=1 version=0.1","hello");expect(session.dispatch("GET_INFO id=2").find("blocks=1")!=std::string::npos,"info");expect(session.dispatch("READ_BLOCKS id=3 lba=0 count=1 encoding=BASE64").find("data=Kg")!=std::string::npos,"read");
std::array<std::uint8_t,512> block{};block[1]=0xa5U;const auto encoded=picosd::protocol::encode_base64(block.data(),block.size());char crc[9]{};std::snprintf(crc,sizeof(crc),"%08X",picosd::protocol::crc32(block.data(),block.size()));const auto write="WRITE_BLOCKS id=4 lba=0 count=1 encoding=BASE64 crc32="+std::string(crc)+" data="+encoded;expect(session.dispatch(write)=="OK id=4","write");expect(session.dispatch("READ_BLOCKS id=5 lba=0 count=1 encoding=BASE64").find("AKU")!=std::string::npos,"written readback");expect(session.dispatch("EJECT id=6")=="OK id=6","eject");expect(session.dispatch("GET_INFO id=7")=="ERR id=7 code=NO_MEDIA","eject blocks future I/O");std::filesystem::remove(path);return failures?1:0;}
