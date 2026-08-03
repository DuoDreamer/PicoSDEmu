# PicoSDEmu Feature Specification

This document captures the agreed project concepts and feature decisions gathered before implementation work begins. It is intended to drive the phased implementation plan, test gates, and acceptance criteria for the PicoSDEmu project.

## Licensing and originality requirements

- The project is MIT licensed.
- All project code, tests, examples, generated images, and documentation must be written specifically for this repository.
- Code must not be copied from other repositories, SD-card emulators, protocol analyzers, or sample projects.
- No vendored third-party source code or binary dependency may be added unless explicitly reviewed and approved later.
- Public datasheets, manuals, and specifications may be used to understand behavior, but their text, diagrams, tables, and code must not be copied into the repository beyond short attributed references where legally appropriate.
- Test media committed to the repository must be original and minimal. Prefer generated test images or scripts that create deterministic images during tests.

## Initial product goal

The first useful product milestone is a Raspberry Pi Pico 2 module acting as an SPI-mode SD card for a real retro-computer client. The client must be able to initialize the Pico as an SD card and correctly read and write a host-served disk image over USB CDC.

The first real client target is a Z180-based SC126 single-board computer using its existing level-shifted SD card adapter. The physical connection is expected to use a flexible cable and an empty SD card shell or equivalent adapter. The SC126 card-detect signal is not expected to be wired for the initial target.

## Hardware scope

- The supported firmware target is the Raspberry Pi Pico 2 module / RP2350 platform.
- The initial client-facing storage interface is SD SPI mode.
- Native 1-bit SD and 4-bit SD modes are deferred future features.
- The initial device should prioritize correctness and compatibility with slow or timing-tolerant retro SPI and bit-banged SPI hosts.
- The first target is expected to use hardware-clocked serial pins at approximately 7 MHz, but the implementation must tolerate slower hosts as well.
- The project should support both direct SPI pin wiring and microSD/SD-slot adapter wiring.
- The initial design assumes normal 3.3 V Pico output signaling. Optional buffering, protection, or level shifting may be added later for more universal hardware adapters.
- RP2350 fault-tolerant GPIO behavior allows many 5 V input scenarios when the I/O rail is powered correctly, but Pico outputs remain 3.3 V and must be validated against each client.
- If no backend is available, the Pico should become electrically quiet on the client SPI bus as far as practical: MISO high-impedance, no active response, and no intentional interference.
- Card-detect emulation should be supported as an optional GPIO feature using a dry-contact-style external transistor circuit, but it is not required for the first SC126 milestone.
- Write-protect emulation is deferred. Protocol-level read-only mode is still required.
- The preferred power design is dual-source-capable, allowing power from USB or from the client side with suitable protection.

## Emulated card model

- The Pico presents block-level storage; it does not interpret or modify file systems.
- The default emulation presents a fixed image-backed SD card.
- The card should support general SD SPI compatibility from the start, rather than only the minimum commands required by one client.
- The implementation should begin with a standards-oriented command core. Additional optional commands should be added when real client traces show they are needed.
- SDSC and SDHC emulation should both be supported.
- The user should be able to choose SDSC or SDHC explicitly. Defaults may choose SDSC for images up to 2 GB and SDHC for larger images, but the choice should remain configurable.
- SDSC and SDHC addressing behavior must be implemented correctly from the start, including byte addressing for SDSC where required and block addressing for SDHC.
- Initial capacity testing should cover approximately 32 MB through 4 GB, with common early images expected around 64 MB through 2 GB.
- In emulation modes, exposed capacity follows the image size by default.
- A configured device size may be smaller or larger than the backing image.
- If the configured device size is smaller than the image, only the exposed prefix is visible and client access beyond the exposed size is rejected.
- If the configured device size is larger than the image, this mode is rejected for now.
- CSD capacity fields should advertise the largest representable capacity that does not exceed the configured exposed size.
- Card identity values may be fixed initially. No sidecar identity file is desired for the initial implementation.

## Host-served image backend

- The primary backend is a host PC application serving a live disk image file over USB CDC.
- The Pico does not keep the whole disk image in RAM. It requests sectors from the host and maintains only a bounded RAM sector cache.
- The cache should be build-time and/or runtime configurable, with an adaptive policy preferred. A medium default, such as several dozen sectors if memory permits, is appropriate.
- Read handling should use a hybrid latency strategy: stretch the SD read response when a requested sector is not cached, aggressively prefetch sequential sectors, and expose statistics for tuning.
- Read prefetch should use sequential stream detection rather than always fetching only the requested block.
- Writes are write-through for the initial implementation. The Pico should not signal an SD write as accepted until the host has completed and acknowledged the positional write to the image file.
- Flush commands, eject, and clean shutdown must force host-side file flush behavior.
- The host application must hold an exclusive lock on the image file while serving it.
- External image modification is not supported initially and should be prevented by exclusive locking.
- Live host-side filesystem modification may be considered later, but is out of scope for the initial block device implementation.

## USB CDC command protocol

- USB CDC serial is the transport between the Pico and host PC application.
- The protocol should remain terminal-friendly: a terminal capable of sending text can issue commands.
- The real image-serving workflow is expected to use the host console application, not manual typing, but commands should remain readable for debugging.
- The protocol is line-oriented. Commands and responses are newline-terminated text lines.
- Sector data should support both hex and base64 encodings. Base64 is preferred for the host application; hex remains useful for diagnostics.
- Commands should use descriptive names such as `GET_INFO`, `READ_BLOCKS`, `WRITE_BLOCKS`, and `FLUSH`.
- Data transfers should include CRC32 payload checksums for host-application use.
- The design may add a binary protocol later, but the initial implementation should not require binary framing.
- The first intended host workflow is similar to:

  ```sh
  picosd-host --port /dev/ttyACM0 serve disk.img --type sdsc --rw
  ```

  Windows COM-port equivalents should also be supported.

## Mount, eject, and disconnect behavior

- In host-served image mode, loss of the USB host connection simulates card removal.
- Without a client card-detect signal, simulated removal means the Pico should stop responding on the SPI bus and place MISO in a high-impedance state as far as practical.
- If USB reconnects after a host-image disconnect, the firmware listens for a host command connection and remounts only after a fresh host session provides image metadata.
- Coordinated eject is required. The host application should flush the image, tell the Pico to stop presenting the card, and wait for confirmation before closing the image.
- Unexpected host image loss or fatal host errors should force simulated card removal rather than continuing with stale data.
- Read-only mode is required. In read-only mode, client writes must be rejected explicitly rather than silently ignored.

## Physical SD backend

- A secondary physical SD card backend is optional but planned.
- The physical backend should operate as a logical block proxy rather than an electrical pass-through.
- The long-term goal is a close identity proxy, but implementation may start with Pico-owned identity values and improve identity mirroring later.
- Physical SD proxy mode should expose the complete physical card initially. Other partition, range, or image-file-on-card modes may be added later.
- Physical SD writes are disabled by default until explicitly enabled over USB or configuration.
- If the Pico boots without a USB host session, it waits a short timeout, approximately 1-2 seconds, and then attempts to initialize the physical SD backend.
- If both USB and a physical SD card are present at boot, USB host mode wins by default.
- If USB is connected after the Pico is already operating in physical SD proxy mode, the USB host may inspect status but must not automatically switch the active backend.
- Switching between host image mode and physical SD proxy mode requires an explicit command and safe unmount/remount workflow.
- If the physical SD card is removed while active and no card-detect line is available to the client, the Pico should go absent/high-impedance rather than pretending the card remains usable.

## SC126 bring-up assumptions

- The first real client is the SC126 Z180 single-board computer.
- The first expected software stack is RomWBW.
- The SC126 SD command set is currently unknown, so command tracing is an early requirement.
- The final target should be stock SC126/RomWBW compatibility. Development-only timeout or driver tuning may be acceptable for investigation, but the product goal should not depend on modified client software.
- The first test image should be a raw diagnostic pattern image before testing a real operating-system image.

## Implementation phases and test gates

### Phase 0: specification and governance

- Capture feature decisions in repository documentation.
- Document MIT licensing and original-code-only requirements.
- Define no-vendoring and no-external-license-encumbrance rules.
- Establish acceptance tests for each later phase.

Exit gate:

- Feature specification is reviewed and committed.

### Phase 1: host terminal protocol prototype

- Implement line-oriented command parsing in the host terminal application.
- Add `serve` workflow with explicit serial port, image path, card type, and read/write mode.
- Add exclusive image locking.
- Add generated diagnostic image tooling or test fixtures.
- Implement hex and base64 helpers written specifically for this project.
- Add CRC32 helper written specifically for this project.

Test gate:

- Unit tests cover command parsing, encoding/decoding, CRC32, image bounds, and read-only rejection.
- Generated diagnostic images can be read and written through the host-side block abstraction without firmware.

### Phase 2: firmware protocol shell

- Replace plain startup-only firmware behavior with a USB CDC command session loop.
- Implement `GET_INFO`, mount metadata handling, read request, write request, flush, eject, and status commands.
- Add bounded sector cache data structures.
- Add cache statistics and tracing hooks.

Test gate:

- Host terminal and Pico can complete command handshakes over USB CDC.
- Synthetic read/write requests round-trip correctly without the SD SPI target enabled.
- USB disconnect and reconnect behavior can be observed through status transitions.

### Phase 3: SPI command trace target

- Implement a minimal client-facing SPI capture mode for the Pico 2.
- Capture SC126 command bytes, chip-select timing, and observed clock behavior.
- Keep MISO safe when not intentionally responding.

Test gate:

- SC126 command traces are captured and decoded well enough to identify the required initialization sequence and optional commands.
- Logic-analyzer traces confirm safe idle and high-impedance behavior.

### Phase 4: minimal read-only emulated card

- Implement the SD SPI initialization command subset required for stock SC126/RomWBW detection.
- Support SDSC and SDHC card-type selection.
- Serve deterministic fixed-pattern sectors from firmware RAM or generated data.
- Implement correct external addressing semantics for SDSC and SDHC.

Test gate:

- SC126 initializes the Pico as an SD card.
- The client can read known sectors and verify deterministic contents.
- Timeout and response-delay behavior are measured against the real client.

### Phase 5: host-served read/write image

- Connect SD SPI read/write commands to the USB CDC host-served image backend.
- Implement read stretching, sequential prefetch, cache hit/miss handling, and statistics.
- Implement write-through acknowledgement semantics.
- Implement safe eject/remount and USB-loss simulated removal.

Test gate:

- SC126 initializes the Pico as SDSC or SDHC as configured.
- SC126 reads and writes a host-served image correctly.
- Host-side image contents match client writes after flush/eject.
- USB disconnect causes simulated card removal.
- USB reconnect requires a fresh host session before remounting.

### Phase 6: compatibility expansion

- Add optional SD SPI commands required by real traces.
- Expand tested image sizes across the 32 MB-4 GB initial range.
- Tune cache size, prefetch depth, and timeout behavior using measured SC126 behavior.
- Add additional retro clients as available.

Test gate:

- Compatibility matrix records card type, image size, command set, read/write behavior, and known limitations for each tested client.

### Phase 7: physical SD logical proxy

- Implement secondary SPI controller support for an attached physical SD card.
- Add boot-time backend selection with USB preference, short timeout, and physical SD fallback.
- Add explicit safe switching between host image and physical SD proxy modes.
- Start with Pico identity if necessary, then improve identity mirroring toward close proxy behavior.

Test gate:

- Without a USB host session, Pico can proxy a physical SD card to the client.
- USB connection during physical proxy mode reports status but does not automatically switch modes.
- Physical card removal causes absent/high-impedance behavior.
- Writes remain disabled by default unless explicitly enabled.

### Phase 8: hardware adapter refinement

- Document direct wiring and adapter-shell wiring.
- Design optional card-detect dry-contact transistor support.
- Reserve optional write-protect, buffering, ESD protection, and level-shifting provisions.
- Validate dual-source power protection assumptions.

Test gate:

- Recommended wiring is documented with measured safe idle behavior.
- Optional card-detect behavior is verified on a test fixture even if the first SC126 adapter does not use it.

## Future enhancements — folder-backed RAM image source

After the main image-backed product is complete and stable, consider an optional
host mode that accepts a local folder as its source. This is explicitly a
future enhancement and is not part of the initial host-served image milestone.

At server startup, the host application would scan a selected folder subject to
documented limitations, construct a complete filesystem disk image in host RAM,
and then serve that RAM image through the same block protocol used for a normal
image file. The Pico and retro client would therefore continue to see an
ordinary fixed-capacity SD card; they would not receive direct host filesystem
operations.

The feature must be specified and reviewed in a separate design discussion
before implementation. That discussion must settle at least:

- filesystem format and client compatibility target;
- capacity calculation, free-space policy, and deterministic directory order;
- permitted names, file types, timestamps, attributes, and folder-depth limits;
- whether the exported image is read-only or how client writes are staged and
  committed back to the source folder;
- folder-change policy while the server is running, including remount and cache
  invalidation behavior;
- RAM-size limits, startup-time limits, error reporting, and fallback behavior;
- atomicity, crash recovery, and ownership rules for the source folder.

The default image-file backend remains the authoritative implementation for the
initial product. A folder source must not compromise its exclusive ownership,
write-through, flush, eject, or USB-loss safety guarantees.
