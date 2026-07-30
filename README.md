# Pico SD Emulator

Pico SD Emulator is an experimental SD-card SPI target for the **Raspberry Pi
Pico 2**. The planned firmware will translate block requests from an attached SD
host into requests to a console image-host application on Linux or Windows.

The repository currently contains the initial project skeleton:

- `firmware/` — C++ firmware for the non-wireless Pico 2 (`pico2`, RP2350);
- `host/terminal/` — portable C++ console application for developing the image
  host protocol;
- `shared/protocol/` — transport definitions shared by both programs;
- `IMPLEMENTATION_PLAN.md` — architecture, milestones, and verification plan.

## Build the terminal application

A C++17 compiler and CMake 3.20 or newer are required.

```sh
cmake -S . -B build/host
cmake --build build/host
./build/host/host/terminal/picosd-host --help
```

On a multi-configuration Windows generator, add `--config Debug` to the build
command and run the executable from the corresponding configuration directory.

## Build the Pico 2 firmware

Install the Raspberry Pi Pico SDK and export `PICO_SDK_PATH`. The firmware CMake
configuration intentionally rejects boards other than `pico2`.

```sh
cmake -S firmware -B build/firmware -DPICO_BOARD=pico2
cmake --build build/firmware
```

The resulting `picosd_firmware.uf2` is only a bring-up image at this stage. It
initializes USB serial output and prints the firmware and protocol versions; it
does not emulate an SD card yet.

