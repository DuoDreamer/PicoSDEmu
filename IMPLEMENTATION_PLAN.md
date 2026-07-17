# Pico SD-Card Emulator: Implementation Plan

## 1. Project goal

Build firmware for a Raspberry Pi Pico that appears to a client as an SD card in
SPI mode while storing the card contents in a disk-image file managed by a host
PC. The Pico forwards sector reads and writes over USB to a small host service.
As an optional second backend, the Pico can access a physical SD card on another
SPI bus; the host can copy an image to that card and the Pico can expose it to
the client.

The first release should deliberately support **SD SPI mode only**, not the
native 1-bit or 4-bit SD bus. It should target an RP2040 Pico and the Pico SDK,
with C++17 for firmware and the host application.

## 2. Feasibility constraints and early proof of concept

An SD card is the SPI **slave**, whereas the RP2040 hardware SPI blocks are
normally used as masters by Pico SDK applications. Implement the client-facing
port with PIO state machines and DMA. Validate this before building the rest of
the product because it is the highest-risk part of the design.

SD clients can expect the beginning of a response within a small number of SPI
clocks. A round trip to the PC cannot satisfy that requirement reliably.
Therefore the Pico must:

- parse commands and emit immediate protocol tokens locally;
- buffer complete 512-byte sectors in SRAM;
- prefetch reads and queue writes asynchronously;
- insert only delays that SD SPI permits (for example `0xFF` before a read data
  token and a busy indication after accepting a write);
- define and test a maximum supported client SPI clock; begin at 1 MHz during
  bring-up and raise it only after logic-analyser testing;
- report itself as not ready, or return a defined error, when the host/backend
  cannot supply data within a configured timeout.

The proof of concept is complete when a microcontroller SPI master can issue
`CMD0`, `CMD8`, `CMD55`/`ACMD41`, `CMD58`, and `CMD17`, and capture a known sector
from Pico SRAM. Only then proceed to host-backed I/O.

## 3. Proposed architecture

```text
 Client device                    Raspberry Pi Pico                 Host PC
 +--------------+     SPI      +----------------------+    USB    +-----------+
 | SD host      |<------------>| PIO SD SPI target    |<-------->| image     |
 | (master)     |              | command/state engine |  bulk    | service   |
 +--------------+              | sector cache         |          +-----------+
                               | backend abstraction  |
                               +----------+-----------+
                                          |
                                      SPI master
                                          |
                               +----------v-----------+
                               | optional physical SD |
                               +----------------------+
```

Keep protocol handling separate from storage. Suggested firmware modules are:

- `sd_target/`: PIO programs, DMA setup, command framing, response generation,
  CRC7/CRC16, and the SD SPI state machine;
- `storage/`: a `BlockBackend` interface with asynchronous `read`, `write`,
  `flush`, `capacity`, and `media_present` operations;
- `storage/usb_backend`: request queue, sector cache, timeouts, and USB framing;
- `storage/sd_backend`: optional physical-card driver on the second SPI block;
- `control/`: backend selection, media insertion/ejection, statistics, logging,
  and fault handling;
- `app/`: initialization and the non-blocking main scheduler.

The host application should contain matching transport, image-file, and control
modules. Use CMake for both projects and keep the wire-protocol definitions in a
small shared header that contains only fixed-width types and serialization
helpers.

## 4. Hardware plan

### Client-facing SPI target

Use four GPIOs for `CS`, `SCK`, `MOSI`, and `MISO`. Add a common ground and use
3.3 V logic only. MISO must be high impedance whenever CS is inactive. Consider
a 33–68 ohm series resistor on driven signals for prototypes with long wires.
Do not connect a 5 V SD host directly; specify an appropriate level shifter if
the client is not 3.3 V.

Assign PIO resources only after prototyping. A likely design uses one state
machine to sample MOSI and CS, and another to shift MISO, with DMA rings feeding
RX and TX buffers. Measure CS transitions independently so an aborted command
or data transfer resets framing cleanly.

### Host connection

Use the Pico USB device controller. Start with TinyUSB vendor-specific bulk
endpoints for predictable binary transfer and a CDC serial interface for logs
and control. If endpoint/resource limits make the composite device cumbersome,
put control messages on the same framed bulk channel and reserve CDC for debug
builds.

### Optional physical SD backend

Use a hardware SPI controller as master with separate GPIOs and chip select.
Add card-detect and write-protect inputs if the socket provides them. The design
must prevent the client and host from changing the same physical medium at the
same time; all access goes through the Pico's backend arbiter.

## 5. SD SPI behavior to implement

Start with a minimal SDHC-compatible, fixed 512-byte block device:

1. Power-up clocks with CS high and idle-state entry.
2. `CMD0` (reset) and `CMD8` (interface condition).
3. `CMD55` plus `ACMD41` (initialization), and `CMD58` (OCR/CCS).
4. `CMD9`/`CMD10` (synthetic CSD/CID consistent with configured capacity).
5. `CMD13` (status), `CMD16` (accept 512 only), and optional `CMD59` (CRC mode).
6. `CMD17` and `CMD24` for single-block reads and writes.
7. `CMD18`, `CMD12`, `CMD25`, and `ACMD23` for multi-block performance.

Generate CSD, OCR, and capacity from the mounted image and keep them stable
while media is presented. Verify CRC7 for commands when required and always
generate/validate CRC16 for data. Initially advertise conservative capabilities
and reject unsupported commands with the illegal-command bit rather than
silently approximating them.

Model the card explicitly with states such as `PowerUp`, `Idle`, `Ready`,
`Transfer`, `ReceivingData`, `Busy`, and `Fault`. Command parsing must never
block on USB or physical-SD I/O; it submits work to queues and advances when a
completion arrives.

## 6. Pico-to-host protocol

Define a versioned, little-endian binary protocol. Every message should include:

- magic value and protocol version;
- message type, flags, header length, and payload length;
- monotonically increasing request ID;
- logical block address and block count where applicable;
- header/payload integrity check (CRC32C is suitable);
- explicit status/error code in responses.

Required operations are `HELLO`, `GET_INFO`, `READ_BLOCKS`, `WRITE_BLOCKS`,
`FLUSH`, `MOUNT`, `EJECT`, `SET_BACKEND`, `GET_STATS`, and asynchronous
`MEDIA_CHANGED`/`FAULT`. Cap each transfer (for example at 16 sectors) so one
request cannot monopolize buffers. Support several outstanding reads to enable
prefetch, but order overlapping writes and flush barriers.

On disconnect, the firmware must stop acknowledging new writes, drain nothing
to an absent host, invalidate uncommitted cache entries, and expose a clear
not-ready state. On reconnect, require a new session identifier and fresh media
metadata so delayed packets from an old session cannot be applied.

## 7. Cache and consistency policy

Reserve a compile-time-configurable pool of aligned 512-byte buffers. Begin with
8–16 sectors, subject to SRAM measurements. Maintain buffer states (`free`,
`loading`, `clean`, `dirty`, `writing`) and an LRU replacement policy.

For correctness-first releases:

- acknowledge a client write only after the host confirms it is written to the
  image (write-through);
- permit the SD busy token while that confirmation is pending, with a timeout;
- coalesce sequential reads and prefetch the next sectors;
- invalidate all cache entries on mount, eject, backend change, reconnect, or
  host-reported external modification;
- serialize reads against overlapping pending writes.

A later opt-in write-back mode may improve performance, but it needs an explicit
data-loss warning, dirty-sector accounting, flush barriers, and power-failure
semantics.

“Modified on the fly” needs a strict ownership contract. The safe default is
that only the host service opens the image read/write; other tools request a
coordinated pause/eject, modify it, flush it, and remount it. If external file
changes must be allowed while mounted, the service should detect them, lock
around each operation, flush OS caches as appropriate, bump a media generation,
and notify the Pico to invalidate its cache. Concurrent filesystem-level access
by both the emulated client and a host filesystem should be documented as unsafe
unless the filesystem itself supports shared access.

## 8. Host application

Implement a command-line service first. Responsibilities include:

- discover a Pico by USB VID/PID and negotiate protocol/session versions;
- open a raw image with exclusive locking where the OS supports it;
- validate that its size is nonzero and a multiple of 512 bytes;
- serve positional reads/writes without sharing a mutable file offset;
- implement flush (`fsync`/platform equivalent), read-only mode, and optional
  sparse-image handling;
- mount/eject and backend-selection commands;
- report throughput, latency, cache statistics, disconnects, and errors;
- refuse image resizing while it is exposed.

Abstract platform USB and file operations so Linux is supported first and
Windows/macOS can be added without changing protocol logic. libusb is a suitable
starting point for bulk transport. Avoid kernel mass-storage drivers on the host
side: this is a private USB protocol, not USB MSC.

For physical-card image transfer, expose explicit `copy-image-to-sd` and
`copy-sd-to-image` operations. Transfer in chunks, show progress, verify capacity
before writing, flush at completion, and optionally read back hashes. Require
the emulated medium to be ejected during a destructive copy.

## 9. Repository and build layout

```text
 firmware/
   CMakeLists.txt
   src/{app,control,sd_target,storage}/
   pio/
 host/
   CMakeLists.txt
   src/
 shared/protocol/
 tests/{protocol,sd_model,integration}/
 tools/
 docs/{hardware,protocol,troubleshooting}/
```

Pin a supported Pico SDK release, generate PIO headers with
`pico_generate_pio_header`, and enable warnings-as-errors in project code.
Provide CMake presets for native host tests and RP2040 cross-compilation. Keep
hardware pin assignments and feature flags in a board configuration header,
with an example schematic and wiring table in `docs/hardware`.

## 10. Implementation milestones

### Milestone 0 — specifications and test harness

- Write the SD command subset and USB protocol specifications.
- Create golden CRC and serialization tests on the host.
- Build an SPI-master exerciser using a second Pico or programmable test tool.
- Capture reference traffic from a real SDHC card with a logic analyser.

**Exit:** protocol structs have round-trip tests and the exerciser can replay
initialization/read/write sequences.

### Milestone 1 — PIO SD target with RAM backend

- Implement CS-aware byte receive/transmit PIO programs and DMA rings.
- Add command parser, card state machine, synthetic registers, and CRC.
- Serve a small deterministic RAM disk with single-sector read/write.
- Add timeouts, counters, and trace events without logging in timing-critical
  interrupt paths.

**Exit:** the exerciser and at least one real client initialize the Pico, read a
sector, write it, and read it back at the agreed initial SPI frequency.

### Milestone 2 — USB image backend

- Implement TinyUSB framing and request queues.
- Implement the Linux host daemon and raw-image provider.
- Add cache, write-through behavior, disconnect recovery, and flush/eject.
- Test injected latency, dropped/corrupt frames, host crashes, and reconnects.

**Exit:** a client can repeatedly read and write a FAT image served from Linux;
after clean eject, filesystem checks find no corruption.

### Milestone 3 — compatibility and performance

- Add multi-block commands and sequential prefetch.
- Test several client types, image sizes, and SPI frequencies.
- Profile PIO/DMA under simultaneous USB traffic and document hard limits.
- Add long-duration random I/O and power/disconnect fault tests.

**Exit:** meet a documented sustained throughput target and pass a 24-hour
randomized endurance run without data mismatch.

### Milestone 4 — physical SD backend

- Integrate the SPI-master SD driver behind `BlockBackend`.
- Implement card detect, capacity checks, arbitration, and safe removal.
- Add image copy in both directions, progress, cancellation at safe boundaries,
  flush, and optional verification.

**Exit:** byte-for-byte verified image copies work, and the same client tests
pass using either USB image or physical SD backend.

### Milestone 5 — packaging

- Add Windows/macOS transport implementations if required.
- Publish wiring, flashing, CLI, recovery, and troubleshooting documentation.
- Version firmware and host protocol together and provide upgrade diagnostics.
- Produce reproducible firmware and host release artifacts.

## 11. Verification strategy

Use layered tests rather than relying only on filesystem mounting:

- **Unit tests:** CRC7/CRC16/CRC32C, command decoding, CSD encoding, bounds and
  overflow checks, framing, cache transitions, and timeout behavior.
- **PIO tests:** byte-level loopback, CS abort at every bit position, clock-rate
  sweep, DMA wraparound, underrun behavior, and MISO tri-state verification.
- **Protocol tests:** fragmentation/coalescing, bad lengths, stale session IDs,
  duplicate request IDs, corrupt payloads, disconnects, and version mismatch.
- **Integration tests:** deterministic and randomized reads/writes compared with
  the source image, flush/eject/remount, read-only images, and end-of-device I/O.
- **Filesystem tests:** create/modify files through a client, eject, then run the
  appropriate offline checker and compare expected files/hashes.
- **Hardware tests:** logic-analyser timing assertions, multiple client boards,
  cable lengths, reset during each state, USB unplug, and physical-card removal.
- **Fuzzing:** host-native fuzz targets for command and USB frame parsers; never
  trust lengths or block addresses received from either side.

Automate native tests in CI. Run hardware-in-the-loop tests on dedicated Picos
for release candidates and archive logic traces for regressions.

## 12. Safety and failure handling

- Check `LBA + count` for integer overflow and capacity before every operation.
- Use bounded queues and buffers; do not allocate dynamically in timing-critical
  firmware paths.
- Add watchdog recovery while preserving a diagnostic reset reason.
- Treat USB corruption, backend timeout, and partial physical-card writes as
  explicit faults visible to both client and host.
- Make read-only mode enforceable in firmware as well as on the host.
- Require confirmation for destructive whole-card copies and never infer the
  destination solely from enumeration order.
- Document that sudden Pico/PC power loss can corrupt a writable filesystem;
  safe eject and host flush are mandatory before disconnecting.

## 13. Initial deliverable and success criteria

The minimum useful release consists of one documented Pico wiring arrangement,
firmware supporting SDHC initialization plus single/multi-sector I/O, and a
Linux CLI service exposing a fixed-size raw image over USB. It is successful
when:

1. at least two representative SPI-mode clients recognize it;
2. all bytes written by randomized tests survive flush/eject/remount;
3. disconnects and backend failures produce bounded, recoverable errors rather
   than hangs or silent corruption;
4. measured maximum SPI frequency, throughput, response latency, image-size
   limit, and unsupported commands are published;
5. the optional physical SD work remains isolated behind the backend interface
   and does not complicate the first release.

