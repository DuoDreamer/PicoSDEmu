# Second-Pico SPI exerciser plan

The Phase 2 SPI-target proof of concept will use a separate Raspberry Pi Pico
2 as an original, project-owned SD SPI host exerciser. It complements logic
analyzer captures before the SC126 is used as a real client.

## Responsibilities

- Drive CS, SCK, and MOSI with selectable SPI mode, clock rate, and byte
  sequences.
- Sample MISO at the configured edge and retain a bounded transaction log.
- Generate power-up clocks, command frames, read clocks, and write data tokens.
- Intentionally abort transactions at each bit position to test recovery.
- Report the transmitted bytes, received bytes, CS timing, and configured rate
  over its own debug transport.

The exerciser is test equipment, not production firmware. Its implementation
will be authored in this repository and will not reuse another project's SD
test program or PIO source.

## Required measurements

For every SPI proof-of-concept test, record the PicoSDEmu revision, target
firmware build, exerciser build, GPIO map, supply arrangement, logic-analyzer
sample rate, and test clock rate. Capture and retain these signals:

| Signal | Measurement |
| --- | --- |
| CS | Idle level, assertion/deassertion time, and behavior after aborted commands |
| SCK | Frequency, duty cycle, and relationship to data sampling edges |
| MOSI | Command frame and write-token correctness |
| MISO | Response delay, token/data timing, idle level, and high-impedance behavior |

## Initial test sequence

1. Confirm MISO is high impedance with CS inactive and after target reset.
2. Send at least 74 power-up clocks with CS inactive.
3. Issue initialization commands and record every response byte and delay.
4. Read a known deterministic sector and compare all 512 bytes plus CRC.
5. Write a sector, read it back, and compare all bytes plus CRC.
6. Repeat each command while deasserting CS after every possible command bit.
7. Sweep from initialization speed to the documented supported rate, retaining
   the captures and observed limits.

The Phase 2 exit gate requires at least 10,000 error-free known-sector
read/write/readback cycles at a documented conservative rate, then the same
core sequence on the SC126.
