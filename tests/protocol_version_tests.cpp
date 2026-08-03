#include <iostream>

#include "picosd/protocol/version.hpp"

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
    expect(picosd::protocol::kVersionMajor == 0,
           "the initial protocol major version is zero");
    expect(picosd::protocol::kVersionMinor == 1,
           "the initial protocol minor version is one");

    if (failures != 0) {
        return 1;
    }

    std::cout << "All protocol version tests passed.\n";
    return 0;
}
