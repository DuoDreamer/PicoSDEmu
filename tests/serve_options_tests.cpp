#include "serve_options.hpp"
#include <iostream>
namespace {
int failures = 0;
void expect(bool b, const char *s) {
    if (!b) {
        std::cerr << "FAIL: " << s << '\n';
        ++failures;
    }
}
} // namespace
int main() {
    using namespace picosd::host;
    ServeOptions options;
    char p0[] = "picosd-host", p1[] = "--port", p2[] = "COM3", p3[] = "serve", p4[] = "disk.img",
         p5[] = "--type", p6[] = "sdhc", p7[] = "--rw";
    char *valid[] = {p0, p1, p2, p3, p4, p5, p6, p7};
    expect(parse_serve_options(8, valid, options) == ServeParseResult::Valid,
           "valid serve options");
    expect(options.port == "COM3" && options.card_type == "sdhc" && options.writable,
           "fields parsed");
    char mount[] = "mount";
    valid[3] = mount;
    expect(parse_serve_options(8, valid, options) == ServeParseResult::Valid,
           "mount alias accepted");
    valid[3] = p3;
    char bad[] = "bad";
    valid[6] = bad;
    expect(parse_serve_options(8, valid, options) == ServeParseResult::Invalid,
           "bad type rejected");
    char help[] = "--help";
    char *nonserve[] = {p0, help};
    expect(parse_serve_options(2, nonserve, options) == ServeParseResult::NotServe,
           "nonserve identified");
    return failures ? 1 : 0;
}
