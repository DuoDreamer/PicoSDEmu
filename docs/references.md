# References and dependency register

This register records documents and dependencies used to implement PicoSDEmu.
It is deliberately a register, not a source archive: no external source code,
examples, binaries, or documentation are vendored into this repository.

## Normative technical references

| Item | Purpose | Version / revision | Source | License / distribution status |
| --- | --- | --- | --- | --- |
| Raspberry Pi RP2350 datasheet | RP2350 electrical limits, GPIO behavior, PIO, DMA, and USB hardware behavior | Consult the revision used for a change | Raspberry Pi official documentation | External reference only; not copied into this repository |
| SD Specifications, Part 1: Physical Layer Simplified Specification | SD SPI command, response, addressing, and register behavior | Consult the version used for a change | SD Association official documentation | External reference only; access and reuse subject to SD Association terms |
| USB 2.0 specification | USB transport concepts relevant to CDC operation | Consult the version used for a change | USB-IF | External reference only; not copied into this repository |
| C++17 standard library documentation | Portable host-side C++ behavior | Compiler/library version used by the build | Toolchain vendor documentation | External reference only |

The implementation must be written independently from these documents. A
change that relies on a particular revision must state that revision in its
commit or pull-request description and add any needed project-owned test
vectors.

## Build and runtime dependencies

| Dependency | Purpose | Version | Source | License | Distribution status |
| --- | --- | --- | --- | --- | --- |
| CMake | Configure native host builds and tests | 3.20 or newer | System installation | External tool; verify locally | Not distributed by this repository |
| C++17 compiler and standard library | Build host tools and native tests | Toolchain selected by developer | System installation | External toolchain; verify locally | Not distributed by this repository |
| Raspberry Pi Pico SDK | Build Pico 2 firmware | Selected by developer for firmware builds | Separately installed checkout | External dependency; review before adopting a pinned version | Not vendored; firmware build requires `PICO_SDK_PATH` |

No third-party library is currently linked by the native host target or tests.
The Pico SDK is an external build dependency only; PicoSDEmu does not copy SDK
examples or source into the project.

## Change-control checklist

Before adding or updating a dependency or reference:

1. Record its purpose, exact version/revision, upstream source, and license or
   distribution terms in this file.
2. Confirm that use does not introduce a license obligation incompatible with
   the MIT-licensed PicoSDEmu source tree.
3. Keep external material out of the repository unless it receives explicit
   approval and its licensing is documented.
4. Add project-owned tests that capture the required behavior without copying
   upstream test data or examples.
