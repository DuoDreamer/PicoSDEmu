#include <iostream>
#include <string>
#include "cdc_transport.hpp"
namespace { int failures=0; void expect(bool b,const char* s){if(!b){std::cerr<<"FAIL: "<<s<<'\n';++failures;}} }
int main(){ using namespace picosd::host; MemoryCdcTransport transport; std::string line;
expect(transport.write_line("HELLO id=1")==CdcTransportError::NotOpen,"closed write rejected");expect(transport.open("/dev/ttyACM0")==CdcTransportError::None&&transport.is_open(),"opens transport");
expect(transport.write_line("HELLO id=1")==CdcTransportError::None&&transport.written_lines().front()=="HELLO id=1","writes line");expect(transport.write_line("BAD\n")==CdcTransportError::InvalidLine,"terminator rejected");
transport.inject_received_line("OK id=1");expect(transport.read_line(line)==CdcTransportError::None&&line=="OK id=1","reads injected line");transport.close();expect(!transport.is_open(),"closes transport");return failures?1:0; }
