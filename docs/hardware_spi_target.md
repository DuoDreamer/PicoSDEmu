# Pico 2 SPI-target proof-of-concept wiring

This document defines the preliminary wiring for the Phase 2 client-facing SD
SPI-target proof of concept. It is a development pin map, not a claim that the
target implementation or adapter PCB is complete.

## Pico 2 GPIO assignment

| Function | Pico 2 GPIO | Direction at the Pico | Initial behavior |
| --- | ---: | --- | --- |
| Client CS | GPIO2 | Input | Detect card selection; no response while inactive. |
| Client SCK | GPIO3 | Input | Clock sampled by the target PIO program. |
| Client MOSI | GPIO4 | Input | Receive SD command and write data bits. |
| Client MISO | GPIO5 | Output / high impedance | Drive only during an active, valid response. |
| Physical SD SCK | GPIO10 | Output | Reserved for the future logical-proxy backend. |
| Physical SD MOSI | GPIO11 | Output | Reserved for the future logical-proxy backend. |
| Physical SD MISO | GPIO12 | Input | Reserved for the future logical-proxy backend. |
| Physical SD CS | GPIO13 | Output | Reserved for the future logical-proxy backend. |
| Card-detect control | GPIO14 | Output | Reserved for an optional external dry-contact transistor. |

All client-facing signals must share ground. The initial adapter assumes normal
3.3 V signaling. RP2350 fault-tolerant input behavior does not make Pico output
levels 5 V; adapter electrical compatibility must be validated against the
client before connecting it.

## Safety requirements

- At reset, before mounting a backend, after USB-host loss, and when client CS
  is inactive, client MISO must be high impedance as far as the selected Pico
  GPIO/PIO configuration permits.
- The proof-of-concept must not enable the reserved physical-SD or card-detect
  pins until those features are implemented and tested.
- Verify idle levels, response timing, and MISO tri-state behavior using the
  second-Pico exerciser and a logic analyzer before connecting the SC126.
- Do not rely on a card-detect connection for the initial SC126 adapter; use
  silent/high-impedance SPI absence for simulated removal.

## Initial PIO capture primitive

`firmware/pio/sd_spi_capture.pio` is the first original PIO proof-of-concept
artifact. It samples MOSI on rising SCK edges while CS is asserted and stops
capturing when CS deasserts. It does **not** drive MISO, decode bytes, use DMA,
or emulate an SD response yet. The firmware must configure its IN base for
GPIO4 and its JMP pin for GPIO2 before loading the program.

This limited capture-only stage is intentional: it permits logic-analyzer and
second-Pico verification of selection, clocking, bit order, and CS-abort
recovery before any client-visible SD response is enabled.

The firmware initializes this capture state machine at boot with automatic
eight-bit RX FIFO pushes. It does not yet consume the captured bytes in a
production command decoder. The capture proof-of-concept trace is disabled by
default. A development terminal can send `TRACE_ON` or `TRACE_OFF` over USB CDC;
when enabled, it emits at most sixteen `TRACE_SPI XX` lines per firmware
main-loop iteration. This diagnostic path must not remain in the timing path
once MISO responses or DMA queues are enabled.

## Deferred MISO transmit primitive

`firmware/pio/sd_spi_transmit.pio` is an original transmit-only primitive
prepared for the next stage. It shifts one queued byte onto GPIO5 on falling
SCK edges so a client can sample it on rising edges. It uses CS as its jump pin,
changes GPIO5 to a PIO output only while CS is low, and returns it to an input
when CS is released or it is waiting for the next queued byte. Firmware must
configure GPIO5 as an input before enabling the state machine. This primitive
must **not** be enabled until capture timing and high-impedance idle behavior
have been verified on hardware; the later response controller will add byte
queue ownership, response timing, and underrun handling.

## Command-frame handoff

The portable `SdSpiCommandFramer` is the next handoff point between the PIO RX
byte stream and the SD card model. Firmware will feed each captured MOSI byte
to it and call `reset()` when CS rises, so bytes from an aborted transaction
cannot be combined with a later command. The framer ignores idle clocks and
only starts a frame when the SD SPI command start bits are present; it does not
perform I/O or produce MISO output.
