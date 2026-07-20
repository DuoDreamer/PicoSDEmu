#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "image_file.hpp"
#include "session_dispatcher.hpp"
namespace { int failures=0; void expect(bool b,const char*s){if(!b){std::cerr<<"FAIL: "<<s<<'\n';++failures;}} }
int main(){const auto path=std::filesystem::temp_directory_path()/"picosd-dispatcher.img";{std::ofstream file{path,std::ios::binary|std::ios::trunc};std::array<char,512> data{};data[0]=42;file.write(data.data(),data.size());}
picosd::host::ImageFile image;expect(image.open(path,true),"opens image");picosd::host::SessionDispatcher session{image,"SDSC",true};expect(session.dispatch("HELLO id=1")=="OK id=1 version=0.1","hello");expect(session.dispatch("GET_INFO id=2").find("blocks=1")!=std::string::npos,"info");expect(session.dispatch("READ_BLOCKS id=3 lba=0 count=1 encoding=BASE64").find("data=Kg")!=std::string::npos,"read");expect(session.dispatch("NOPE id=4")=="ERR id=4 code=UNSUPPORTED","unsupported");std::filesystem::remove(path);return failures?1:0;}
