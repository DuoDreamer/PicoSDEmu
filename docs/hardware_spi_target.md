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
capturing when CS deasserts. It does **not** drive MISO, decode bytes, or
emulate an SD response. The firmware must configure its IN base for
GPIO4 and its JMP pin for GPIO2 before loading the program.

This limited capture-only stage is intentional: it permits logic-analyzer and
second-Pico verification of selection, clocking, bit order, and CS-abort
recovery before any client-visible SD response is enabled.

The firmware initializes this capture state machine at boot with automatic
eight-bit RX FIFO pushes. A PIO-paced DMA channel drains those pushes into an
aligned 256-byte circular buffer, which the main-loop target monitor consumes
without blocking. CS release discards unread bytes and clears any partial PIO
input shift, preventing an aborted command from crossing a transaction
boundary. The capture trace remains disabled by default and is only a
low-speed bring-up aid.

## MISO transmit primitive

`firmware/pio/sd_spi_transmit.pio` is an original transmit-only primitive. It
shifts one queued byte onto GPIO5 on falling
SCK edges so a client can sample it on rising edges. It uses CS as its jump pin,
changes GPIO5 to a PIO output only while CS is low, and returns it to an input
when CS is released or it is waiting for the next queued byte. Firmware
configures GPIO5 as an input before enabling the state machine. The target
monitor feeds a fixed 1024-byte transmit ring, and a PIO-paced DMA channel
moves contiguous spans into the TX FIFO. CS release aborts DMA and clears both
the ring and FIFO. Its timing and high-impedance behavior still require
hardware validation before connecting a client.

## Command-frame handoff

The portable `SdSpiCommandFramer` is the next handoff point between the PIO RX
byte stream and the SD card model. Firmware will feed each captured MOSI byte
to it and call `reset()` when CS rises, so bytes from an aborted transaction
cannot be combined with a later command. The framer ignores idle clocks and
only starts a frame when the SD SPI command start bits are present; it does not
perform I/O or produce MISO output.

`SdSpiDataFramer` provides the equivalent handoff after the model accepts a
single- or multi-block write command. It extracts a start token, 512-byte
payload, and CRC16, and recognizes the multi-block stop token. The firmware
will pass completed events to the model, which validates the CRC and decides
the data-response and busy behavior.

`SdSpiCardEngine` composes the command and data framers with `SdCardModel` for
the later byte-queue worker. It produces bounded response byte sequences but
does not access PIO, DMA, GPIO, USB, or blocking storage, keeping electrical
and timing policy outside the portable protocol core. Its CS-release hook also
abandons an incomplete write frame so the model can recover for the next client
transaction. CMD9/CMD10 register payloads are serialized with the normal data
token and CRC16 format before they enter the transmit queue. For CMD18, the
queue worker automatically requests the next block after the preceding response
leaves its software transmit queue, and stops when CMD12, CS release, or the
backend capacity ends the sequence. This keeps multi-read state out of the
hardware adapter while limiting the queued data to one response block.

## Bounded queue primitive

`FixedRingQueue` is the project-owned, allocation-free FIFO intended for the
future PIO/DMA-to-engine and engine-to-transmit handoffs. Its `try_push` and
`try_pop` operations make full and empty conditions explicit so firmware can
count queue overflow and underrun rather than silently losing SPI bytes.

`SdSpiTargetWorker` applies that queue policy to captured bytes and serialized
engine outputs. It uses bounded processing, preserves FIFO order, drops a
response atomically when the transmit queue lacks capacity, and exposes receive
and transmit overflow/underrun counters for firmware diagnostics. The card
engine separately counts command CRC errors, data CRC errors, and CS releases
that abort a partial command or write-data transaction. These counters are
updated inline without printing or otherwise changing the SPI timing path. On
CS release it discards both pending receive and transmit bytes, preventing a
stale response from crossing into the next client transaction.

The firmware abandons a transaction if CS remains asserted without a captured
byte for 250 ms. Cleanup clears the software and PIO response queues, increments
the timeout counter once, and waits for CS to be released before accepting a
new transaction. `TARGET_COUNTERS` prints a snapshot of receive/transmit totals,
queue errors, aborted transactions, CRC errors, and timeouts over CDC; polling
and printing are never performed from the SPI timing path.

## Firmware target monitor

The firmware now links the portable protocol core into the Pico target. The
`TARGET_ON` CDC command feeds captured MOSI bytes through the target worker and
queues decoded responses for the PIO transmitter without per-byte logging.
`TARGET_TRACE_ON` runs the same path while printing RX, TX, and CS events over
USB; that diagnostic mode is intentionally not suitable for timing tests.
`TARGET_OFF` clears both software and PIO response queues so stale bytes cannot
cross into a later transaction, and MISO remains input/high-impedance.
