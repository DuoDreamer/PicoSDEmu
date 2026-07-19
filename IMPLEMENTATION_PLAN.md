# Pico SD-Card Emulator: Implementation Plan

## 1. Project goal

Build firmware for a Raspberry Pi Pico 2 that appears to a client as an SD card in
SPI mode while storing the card contents in a disk-image file managed by a host
PC. The Pico forwards sector reads and writes over USB to a small host service.
As an optional second backend, the Pico can access a physical SD card on another
SPI bus; the host can copy an image to that card and the Pico can expose it to
the client.

The first release should deliberately support **SD SPI mode only**, not the
native 1-bit or 4-bit SD bus. It targets only the non-wireless Raspberry Pi Pico
2 module (`PICO_BOARD=pico2`, RP2350) and the Pico SDK,
with C++17 for firmware and the host application.

## 2. License and clean-room development rules

All original source code, documentation, tests, and build files produced for
this repository will be released under the repository's MIT license. These
rules apply to every phase and are release-blocking requirements:

1. **Write all project code from first principles.** Do not copy, translate,
   transcribe, or adapt source code from another repository, project, example,
   answer, or code generator. Do not use another implementation as a line-by-line
   guide.
2. **Use specifications as behavioral references, not source-code references.**
   Engineers may consult hardware datasheets, SD protocol specifications, USB
   specifications, compiler documentation, and Pico 2/Pico SDK API documentation.
   Record the title, revision, and URL of normative references in
   `docs/references.md`; do not reproduce substantial copyrighted text.
3. **Do not vendor third-party code.** No external source, binary library,
   submodule, generated source bundle, firmware blob, or copied example may be
   committed. In particular, the host program will not incorporate libusb or an
   existing SD-card, filesystem, CRC, command-line, serialization, or ring-buffer
   implementation.
4. **Keep platform prerequisites outside the repository.** The Raspberry Pi Pico
   SDK, compiler toolchains, CMake, operating-system SDKs, and device drivers are
   user-installed build/platform prerequisites, not distributed project content.
   Before supporting a particular version, document its license and confirm that
   using it to build or run the MIT-licensed project does not impose a license on
   project code or release artifacts.
5. **Use native host APIs.** Implement the small USB transport adapters directly
   against documented Linux and Windows user-space APIs. Keep them behind a
   project-owned interface so neither implementation contaminates protocol or
   image logic.
6. **Audit every contribution.** Each review must confirm authorship, provenance,
   dependency changes, generated files, and license headers. Any code with
   uncertain provenance is rejected and rewritten without reference to it.
7. **Ship only auditable artifacts.** A release inventory must map every source
   and binary artifact to project code or an approved build prerequisite. The
   dependency and license audit must pass before a phase or release can close.

Tests may compare observable behavior with standards or with hardware, but test
fixtures must be authored for this project. Captured protocol traces may be kept
only when they contain no third-party code or distributable proprietary data.

## 3. Feasibility constraints and early proof of concept

An SD card is the SPI **slave**, whereas the RP2350 hardware SPI blocks are
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

## 4. Proposed architecture

```text
 Client device                   Raspberry Pi Pico 2                Host PC
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

## 5. Hardware plan

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

## 6. SD SPI behavior to implement

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

## 7. Pico-to-host protocol

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

## 8. Cache and consistency policy

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

## 9. Host application

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
Windows can be added without changing protocol logic. Implement these adapters
using documented native operating-system APIs; do not add a third-party USB
library. Avoid kernel mass-storage drivers on the host side: this is a private
USB protocol, not USB MSC.

For physical-card image transfer, expose explicit `copy-image-to-sd` and
`copy-sd-to-image` operations. Transfer in chunks, show progress, verify capacity
before writing, flush at completion, and optionally read back hashes. Require
the emulated medium to be ejected during a destructive copy.

## 10. Repository and build layout

```text
 firmware/                       # Pico 2 cross-compiled project
   CMakeLists.txt
   src/{app,control,sd_target,storage}/
   pio/
 host/terminal/                  # Linux/Windows console application
   CMakeLists.txt
   src/main.cpp
 shared/protocol/
 tests/{protocol,sd_model,integration}/
 tools/
 docs/{hardware,protocol,troubleshooting}/
```

Pin a supported Pico SDK release, generate PIO headers with
`pico_generate_pio_header`, and enable warnings-as-errors in project code.
Provide CMake presets for native host tests and RP2350 cross-compilation. Keep
hardware pin assignments and feature flags in a board configuration header,
with an example schematic and wiring table in `docs/hardware`.

## 11. Phased implementation and test gates

Development proceeds in the phases below. A phase may start only after its entry
conditions are met. It closes only when its deliverables are committed, every
required test passes, results are recorded, and its license/provenance gate
passes. A failure returns the work to the implementation step that introduced
it; tests must not be weakened to make a gate pass.

### Phase 0 — governance, specifications, and test foundations

**Entry:** repository skeleton and MIT `LICENSE` are present.

**Implementation steps:**

1. Add contribution rules that require original work and provenance declarations.
2. Create `docs/references.md` for normative documents and a dependency register
   containing purpose, version, source, license, and distribution status.
3. Write project-owned specifications for the supported SD SPI behavior, USB
   framing, errors, timeouts, cache semantics, and media ownership.
4. Select fixed-width wire types and write byte-level golden vectors by hand from
   the project protocol specification.
5. Establish native C++ unit-test and formatting/static-analysis targets without
   importing a test framework; use a small project-owned test runner.
6. Design a second-Pico SPI-master exerciser and define reproducible logic-analyser
   measurements for CS, SCK, MOSI, MISO, response delay, and tri-state behavior.

**Test stage A — repository and specification checks:**

- configure and build the existing host skeleton on Linux and Windows;
- configure an empty Pico 2 firmware build using the approved Pico SDK version;
- verify protocol examples have unambiguous byte order, sizes, and error results;
- verify every tracked file has known authorship and no vendored content;
- inspect release/build output to ensure external sources or binaries are not
  copied into the repository or package.

**Exit:** specifications are review-approved, both build paths are reproducible,
the reference/dependency register is complete, and the license audit has no open
items.

### Phase 1 — pure C++ protocol and SD model

**Entry:** Phase 0 is complete; no hardware I/O is required by this phase.

**Implementation steps:**

1. Implement project-owned CRC7, CRC16, and CRC32 functions from their published
   polynomial definitions, with no lookup table copied from elsewhere.
2. Implement bounds-checked USB header serialization/deserialization using byte
   arrays rather than packed compiler-dependent structs.
3. Implement SD command decoding and response construction as a host-buildable
   library with no Pico SDK dependency.
4. Implement an explicit card state model (`PowerUp`, `Idle`, `Ready`, `Transfer`,
   `ReceivingData`, `Busy`, and `Fault`) and a deterministic RAM block backend.
5. Generate synthetic OCR, CID, and CSD fields from configured capacity.

**Test stage B — deterministic native tests:**

- CRC tests cover empty, short, all-zero, all-one, and independently calculated
  project vectors, plus every single-bit mutation;
- serialization tests cover round trips, truncated input, excess lengths, bad
  versions, integer overflow, corrupt checksums, and unknown message types;
- SD-model tests cover every legal state transition, every supported command in
  legal and illegal states, reset from each state, bad CRC, and out-of-range LBA;
- randomized model tests use a recorded seed and compare RAM-backend reads/writes
  against a simple project-owned byte-array oracle;
- sanitizers and compiler warnings run on the native library where supported.

**Exit:** all pure logic is platform-independent, has full branch coverage for
error paths targeted by the specification, and passes provenance review.

### Phase 2 — Pico 2 PIO SPI-target proof of concept

**Entry:** the tested SD model can serve a known sector from the RAM backend.

**Implementation steps:**

1. Choose and document Pico 2 GPIO assignments and electrical requirements.
2. Write original RP2350 PIO programs to receive MOSI and transmit MISO while
   respecting CS boundaries; add DMA-backed fixed-size queues.
3. Connect PIO byte events to the Phase 1 command model without dynamic allocation
   or blocking calls in interrupt/timing-critical paths.
4. Support power-up clocks, `CMD0`, `CMD8`, `CMD55`/`ACMD41`, `CMD58`, register
   reads, `CMD13`, `CMD16`, `CMD17`, and `CMD24` against the RAM backend.
5. Add counters for queue overflow, underrun, aborted transaction, CRC error, and
   timeout; debugging output must not alter SPI timing.

**Test stage C — electrical and timing tests:**

- host-native tests from Phase 1 remain green for the exact model used by firmware;
- verify MISO is high impedance whenever CS is inactive and after Pico reset;
- abort CS at every bit of commands and data blocks and confirm clean recovery;
- sweep SPI clock from initialization speed through the currently supported limit,
  recording response latency and setup/hold margins with a logic analyser;
- repeat known-sector read, write, and readback tests for at least 10,000 cycles;
- reset or power-cycle the Pico during each card state and confirm bounded recovery;
- run firmware under maximum PIO/DMA queue pressure and confirm no memory overrun.

**Exit:** the second-Pico exerciser and one representative real client initialize,
read, and write the RAM disk at a documented conservative clock rate with no data
mismatch or electrical violation.

### Phase 3 — USB transport and Linux terminal image host

**Entry:** Phase 2 is stable; Phase 0 has approved the native Linux USB API used.

**Implementation steps:**

1. Implement original TinyUSB device integration using only the separately
   installed Pico SDK APIs; do not copy SDK examples into project source.
2. Implement framing, request IDs, bounded multi-request queues, session IDs,
   handshake/version negotiation, and CRC32C validation.
3. Implement the Linux host adapter with native APIs and the project-owned
   transport interface.
4. Extend the console application with device discovery, `serve`, `mount`,
   `eject`, `status`, and `flush` commands.
5. Implement positional image reads/writes, image-size validation, read-only mode,
   exclusive ownership, capacity reporting, and explicit flush.
6. Connect the Pico USB backend to the SD model using write-through behavior and
   a fixed pool of aligned 512-byte sector buffers.

**Test stage D — transport and image integration:**

- fragment and coalesce every frame at every byte boundary;
- inject bad magic, lengths, checksum, request ID, session ID, LBA, status, and
  version, confirming bounded rejection without queue or buffer corruption;
- simulate host latency from zero through timeout and verify legal SD busy/not-ready
  responses rather than timing hangs;
- unplug USB and terminate/restart the host during reads, writes, and flushes;
- compare deterministic and seeded-random sector operations with the image bytes;
- create and modify files through the client, safely eject, run an offline
  filesystem checker, and compare expected file hashes;
- run at least an eight-hour mixed-I/O soak before the phase gate.

**Exit:** Linux can serve a fixed-size image reliably, all disconnect cases are
bounded and recoverable, and clean eject leaves a valid, byte-correct image.

### Phase 4 — cache correctness, multi-block I/O, and performance

**Entry:** single-block USB-backed operation passes Phase 3.

**Implementation steps:**

1. Add `CMD18`, `CMD12`, `CMD25`, and `ACMD23` behavior.
2. Add sequential read prefetch and safe request coalescing while retaining
   write-through as the default.
3. Implement cache invalidation on mount, eject, reconnect, backend change, and
   media-generation change; serialize reads against overlapping writes.
4. Add statistics for latency distributions, throughput, hit rate, busy duration,
   timeouts, retries, and protocol faults.
5. Optimize only after collecting traces; preserve a correctness-first build mode.

**Test stage E — concurrency, consistency, and endurance:**

- exhaustively test cache buffer state transitions and forced eviction;
- test overlapping reads/writes, flush barriers, duplicate requests, queue
  saturation, cache invalidation, and end-of-image multi-block requests;
- compare cached and uncached runs against the same deterministic operation log;
- sweep client clock, USB load, image size, and host latency while recording hard
  operating limits;
- run a 24-hour seeded randomized endurance test with periodic flush/eject/remount
  and a final full-image hash comparison;
- repeat filesystem checks after injected resets and disconnects, distinguishing
  expected unflushed data loss from emulator-caused corruption.

**Exit:** multi-block operation meets documented targets, no acknowledged write
is lost after flush, and the endurance image matches the oracle.

### Phase 5 — native Windows console host

**Entry:** the platform-neutral host interface and Linux behavior are stable.

**Implementation steps:**

1. Document and approve the Windows SDK/driver prerequisites and native USB API.
2. Implement the Windows USB, exclusive-file-access, positional-I/O, and flush
   adapters without third-party libraries.
3. Keep commands, messages, exit codes, and configuration compatible with Linux.
4. Add Windows build and packaging scripts that contain only project-authored text.

**Test stage F — cross-platform equivalence:**

- run common protocol and image tests unchanged on Linux and Windows;
- test device arrival/removal, stale handles, access denial, long paths, sparse
  files where supported, console interruption, and clean shutdown;
- exchange the same image and Pico between operating systems and compare hashes;
- inspect packages to prove that no external DLL, source, or restricted runtime
  has been bundled.

**Exit:** Linux and Windows hosts pass the same integration suite and expose the
same documented behavior and recovery semantics.

### Phase 6 — optional physical SD backend and image copy

**Entry:** Phases 1–5 are stable; this phase remains optional and isolated.

**Implementation steps:**

1. Write an original SPI-master physical-card driver from the applicable public
   specifications using the Pico SDK hardware APIs.
2. Implement it behind `BlockBackend` with card detect, write protect, capacity,
   timeout, flush, and removal handling.
3. Add an arbiter that prevents emulated-client access during host-directed image
   copy and prevents destructive copy while the medium is exposed.
4. Add `copy-image-to-sd` and `copy-sd-to-image` with explicit destination
   confirmation, progress, safe-boundary cancellation, flush, and verification.

**Test stage G — physical media safety:**

- test supported capacities, read-only cards, absent/removed cards, undersized
  destinations, timeout, bad blocks as reported by the card, and power interruption;
- verify source and destination with full hashes after copies in both directions;
- attempt every conflicting operation and confirm arbitration rejects it;
- run all applicable client block and filesystem tests against both backends;
- confirm physical-backend removal leaves USB-image behavior unchanged.

**Exit:** verified bidirectional copy and emulation pass without weakening the
primary USB-image backend or introducing external code.

### Phase 7 — release qualification

**Entry:** all features selected for the release have passed their phase gates.

**Implementation steps:**

1. Freeze the protocol version and supported feature matrix.
2. Document wiring, flashing, native driver setup, CLI usage, safe eject, recovery,
   limitations, measured timings, and unsupported commands.
3. Produce reproducible firmware and host binaries from a clean checkout.
4. Complete the source/provenance/dependency inventory and MIT notice review.

**Test stage H — final acceptance:**

- repeat all native, hardware, integration, filesystem, fault-injection, and soak
  gates on release builds from a clean environment;
- test at least two representative client devices and archive measurements;
- compare release artifact hashes across two clean builds where toolchains permit;
- scan the source tree and packages for unexpected licenses, copyright notices,
  copied snippets, generated bundles, third-party binaries, and undeclared inputs;
- have a reviewer sign off the supported-limits table and provenance inventory.

**Exit:** every selected feature passes, results and known limitations are
published, and all shipped project code and documentation are original MIT-licensed
work with no bundled external dependency.

## 12. Phase summary

| Phase | Primary result | Mandatory test gate |
| --- | --- | --- |
| 0 | Governance and specifications | Builds, provenance, dependency audit |
| 1 | Portable protocol and SD model | Native deterministic/randomized tests |
| 2 | Pico 2 RAM-backed SD target | PIO electrical, timing, and recovery tests |
| 3 | Linux USB image host | Fault-injected transport/filesystem integration |
| 4 | Multi-block I/O and cache | Consistency, performance, 24-hour endurance |
| 5 | Windows image host | Cross-platform behavioral equivalence |
| 6 | Optional physical SD backend | Media safety and byte-for-byte verification |
| 7 | Release artifacts | Full regression and license/provenance audit |

## 13. Detailed deliverables retained from the initial plan

The following milestone notes provide additional implementation detail. The
phase entry/exit criteria and test gates above take precedence if the sections
overlap.

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

## 14. Verification strategy

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

## 15. Safety and failure handling

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

## 16. Initial deliverable and success criteria

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
