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

Use `picosd-host discover` to list likely CDC serial devices. `serve` (or its
`mount` alias) opens and exclusively owns an image until the process exits;
`status`, `flush`, and `eject` are available as one-shot device commands:

```sh
picosd-host --port /dev/ttyACM0 status
picosd-host --port /dev/ttyACM0 mount card.img --type sdhc --rw
```

On a multi-configuration Windows generator, add `--config Debug` to the build
command and run the executable from the corresponding configuration directory.
Windows builders and operators must also follow the approved
[Windows host prerequisites and native API boundary](docs/windows_host_prerequisites.md).

## Build the Pico 2 firmware

Install the Raspberry Pi Pico SDK and export `PICO_SDK_PATH`. The firmware CMake
configuration intentionally rejects boards other than `pico2`.

```sh
cmake -S firmware -B build/firmware -DPICO_BOARD=pico2
cmake --build build/firmware
```

The resulting `picosd_firmware.uf2` is a bring-up image. Its `TARGET_ON` CDC
command enables a small diagnostic-pattern SD model and connects the PIO
capture/transmit path to the client SPI pins without per-byte logging.
`TARGET_TRACE_ON` enables the same target with verbose USB diagnostics and is
only suitable for functional tracing, not timing measurements. Use `TARGET_OFF`
to stop the target and return MISO to its passive state. On CDC connection the
firmware now initiates the versioned image-host handshake, validates mounted
media metadata, tracks its cache generation, and services bounded, retryable
write-through backend requests. A non-blocking SD storage adapter now connects
that sector pool to the SD model's storage interface: cache misses enqueue CDC
work, and writes become successful only after a matching host acknowledgement
is retained. `TARGET_ON` continues to expose the diagnostic RAM image until the
SPI response worker can defer a data token or busy completion across an
asynchronous cache miss.
