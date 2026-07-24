#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "cdc_transport.hpp"
#include "image_file.hpp"
#include "session_dispatcher.hpp"
#include "session_runner.hpp"
namespace { int failures=0; void expect(bool b,const char*s){if(!b){std::cerr<<"FAIL: "<<s<<'\n';++failures;}} }
int main(){const auto path=std::filesystem::temp_directory_path()/"picosd-runner.img";{std::ofstream f{path,std::ios::binary|std::ios::trunc};std::array<char,512> x{};f.write(x.data(),x.size());}picosd::host::ImageFile image;expect(image.open(path,true),"open");picosd::host::SessionDispatcher dispatcher{image,"SDSC",true};picosd::host::MemoryCdcTransport transport;transport.open("test");transport.inject_received_line("HELLO id=1");expect(picosd::host::process_one_request(transport,dispatcher)==picosd::host::SessionRunResult::Processed,"process");expect(transport.written_lines().back()=="OK id=1 version=0.1","response");expect(picosd::host::process_one_request(transport,dispatcher)==picosd::host::SessionRunResult::NoRequest,"empty");std::filesystem::remove(path);return failures?1:0;}
