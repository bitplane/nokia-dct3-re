# Cross-ROM confidence

## Purpose

The first cross-ROM test is a portability check, not a new boot target. A
second firmware should exercise the same MAD2, EEPROM, CCONT, timer and display
boundaries without adding firmware-address special cases.

## Nokia 3330 NHM-6 v4.50

The firmware.center service archive `NHM-6 v.04.50 3330.rar` was acquired
locally from the Nokia/3330 archive. It contains:

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `NHM6NX04.500` | 2,689,928 | `fac546ceaf7f56e536383730ec6cadc6519f9a11a08a3a9688f2439e77f3eca8` |
| `NHM6NX04.50E` | 787,296 | `318fcc3fa9d2a926e574fea89af9d8cda81796acb904f196636b886ef4e6ce9e` |
| `3330 virgin eeprom.pmm` | 65,608 | `83e58fe48153a7c1d60247ba7b80b72e05ddb8fd5443aa0d9ed976ec3ad88808` |

These are Wintesla record streams. Each record is a nine-byte header followed
by the payload length encoded in header bytes 5–7, up to `0x2000` bytes. Header
byte 0 is type `0x0b`, and bytes 1–3 are the big-endian target address.
`tools/extract_dct3_wintesla.py` validates type, length, truncation, ordering,
and non-overlap before extracting:

| Component | Target range | Raw size | SHA-1 |
| --- | --- | ---: | --- |
| MCU `.500` | `0x200000..0x48ffff` | `0x290000` | combined below |
| PPM `.50E` | `0x490000..0x54ffff` | `0x0c0000` | combined below |
| PMM | `0x5f0000..0x5fffff` | `0x010000` | `68481effb39d90a1639e8f261009c66e97d3e668` |

The MCU+PPM E result is `0x350000` bytes with SHA-1
`7e88caa4963c57ebbd4d919023e38103ff8b528a`. The PMM matches MAME's existing
canonical declaration exactly. The flash differs from MAME's PPM C image
because the acquired language pack is PPM E, so the driver declares it as a
separate `450e` BIOS rather than disguising it as `3330f450c.fls`.

## Current result

- 3210 v6.00: exact CONTACT SERVICE oracle reproduced after native 24C128 and
  CCONT device extraction (`d8a9a7a58e587be8`).
- 3330 v4.50 PPM E: `make smoke-3330e RUN_DIR=run_3330e_smoke SECONDS=3`
  completed three emulated seconds at 207% average speed. Firmware performed
  one soft reset and issued a blank LCD transfer (`f102/z504`); no phone-specific
  hook or service model was enabled.
- Confidence gained already: the build and machine definitions are
  multi-source/multi-ROM capable; the first two extracted hardware components
  contain no 3210 firmware addresses.

### Static code-7 topology

A signature search independent of absolute addresses finds the task-1 report
code-7 stub at `0x386a94` in 3330 v4.50. Like the 3210 stub at `0x2af190`, it
has exactly four callers. The 3330 callback caller at `0x25fc86` sits in a
dispatcher structurally identical to 3210 callback `0x5d`: the same
`0x05e1`/`0x05e7` timer-start branches and direct `0x05eb`/`0x06c5` completion
branches are present. Two other callers remain power/charger owned and the
fourth remains controller owned, matching the 3210 ownership split.

This is static portability evidence, not a successful-boot oracle. It proves
that the four-owner code-7 topology is shared across the DCT3 firmware family;
it does not identify which owner completes during an ordinary healthy boot.
No 3330 firmware-PC hook was added.

### Nokia 5110 healthy-run control

A forcing-free NSE-1 v5.30 run with its native DSP execution was traced through
the Security-code editor, accepted `12345`, and reached standby. Across 53
task-1 posts, report code `0x07` occurred zero times. The ROM's fifteen report
stubs were enumerated; only `0x14`, `0x15`, `0x16`, and `0x17` fired during the
healthy startup. Code `0x14` and the dormant code-7 call are adjacent branches
of the same battery/charger dispatcher. Predicate `0x260cea` skips code 7 when
the selected measurement exceeds its stored threshold plus `0x15e`. This is a
behavioral cross-ROM control for conditional power ownership, not evidence that
3210 report numbers or startup-state transitions are interchangeable.

## Nokia 3210 NSE-8 v5.01

An independent full-flash control is stored locally as
`nokia_3210_nse-8_v05_01_full_hu.fls`:

| Bytes | SHA-256 | Identification |
| ---: | --- | --- |
| 2,097,152 | `62de70cd5451444cfcd4ed6c6d8a9a84c0e783a557f8489f2dc5faa283b66272` | `V 05.01`, `NSE-8` |

Its report-7 wrapper is at `0x2ac5bc`. It publishes resource `0x6a010000`,
posts code 7 to task 1, and has exactly four callers at `0x21e22c`,
`0x21f772`, `0x252a4a`, and `0x277d06`. This is a same-product control for the
stable four-owner topology.

The caller comparison is also complete. The blocks around the first power
caller preserve the same measurement predicate and shutdown outcome; the
second power caller preserves the same charging-completed state update; and
the controller caller preserves the same adjacent helper sequence before the
report. Callback `0x5d` has the same subtract cascade, start statuses
`0x05e1`/`0x05e7`/`0x05dc`, and terminal statuses `0x05eb`/`0x06c5`. Its only
material local change is timer class `0x51` in v5.01 versus `0x52` in v6.00;
the completion statuses and code-7 action are unchanged. The same-product
comparison therefore exposes no alternate caller predicate, NV selector, or
ordinary producer omitted from the v6.00 ownership analysis.

The task-1 branch comparison is now complete. The v5.01 block at
`0x26dc20..0x26df14` is instruction-for-instruction equivalent to v6.00
`0x27120e..0x271502`, including both explicit code-7 waits, the six report
flags, code-6 exit-selector path, event-`0x74` gate, and final mode-`0x000c`
tail. The corresponding v5.01 state block is `0x1121bc..0x1121c9`, relocated
from v6.00 `0x112390..0x11239d`. Code 7 is therefore a stable 3210 contract,
not an isolated v6.00 branch choice.

The CCONT comparison is also complete at the firmware boundary. The v6.00 ADC
reader `0x2b52cc`, register read/write helpers `0x2afb44`/`0x2afa74`, and IRQ
ISR `0x2b08c6` align uniquely with v5.01 `0x2b2680`, `0x2acf70`/`0x2acea0`,
and `0x2adcf2`. The wider helper blocks are instruction-equivalent, including
GENSIO polling, command construction, watchdog/power helpers, status/mask
calculation and write-one-to-clear acknowledgement. The ADC route tables
(`0x2e2d74`/`0x2d7770`) and CCONT shadow/default tables
(`0x2e2da8`/`0x2d777c`) are byte-identical. This supports one shared 3210
device contract; it does not establish analog units or timing.

The MAD2 timer-0 setup is likewise stable. v6.00 `0x2aa934` aligns uniquely
and byte-for-byte with v5.01 `0x2a75c4`: both program live divider `0xf9`, wait
for it to reach `0xea`, compare at `counter+2`, consume FIQ4 and acknowledge it
through the active-status register. This establishes the register protocol but
not the physical timer input clock.

The MAD2 power-key IRQ handler also aligns from v6.00 `0x2b3084` to v5.01
`0x2b02fc`; both publish task-1 event `0x41` through corresponding wrappers
`0x2b4662` and `0x2b18ea`. This makes the line ownership and firmware-visible
transition stable. The optional edge-delay fixture is not enabled by the
canonical profile because neither ROM provides a duration.

The ROM is now also packaged as MAME BIOS `501` with a BIOS-specific generated
EEPROM profile. `make verify-3210-v501` provides the first forcing-free runtime
comparison. It observes startup modes `0x0001 -> 0x000d -> 0x0004` and
readiness flags `0x0f`: the same task-1 code-7 wall as
v6.00. The earlier lifecycle is not equivalent yet: v5.01 currently finishes
with contact status `0x00c9`, SIM enable set, and no nonblank LCD frame. The
SIM block is relocated by `-0x1d0`: v5.01 no-SIM/ENABLE are
`0x111a94`/`0x111aa9`, corresponding to v6.00
`0x111c64`/`0x111c79`. The valid-reply setter is instruction-equivalent and a
forcing-free run proves both ROMs clear no-SIM and set ENABLE through the same
device contract. This
is therefore evidence that the blocking task-1 contract spans both 3210 ROMs,
not a claim of general v5.01 support.

A DSP-boundary trace confirms that this is not superficial state coincidence.
The v5.01 firmware performs D0 discovery, the type-`0x70` self-test exchange,
contact registration `0x64`, channel-map `0x70`, healthy result 5, transport
acknowledgements, service-empty `0x622a`, and the later type-`0x1a` publication
through the same shared-memory device. Its instruction-equivalent contact-ready
consumer is `0x299314` (v6.00 `0x29bc70`), inside initialization loop
`0x2a67aa` (v6.00 `0x2a92d2`). The v5 run does not consume ready bit 7 after
the peer's healthy result and therefore retains `0xc9`; no timing change or
firmware-state forcing has been introduced. This lifecycle difference and the
blank display are now the v5 portability frontier.

## Interactive sibling controls

- A forcing-free Nokia 5110 v5.30 run in an independent message-level emulator
  reaches its exact Security-code frame without executing its structurally
  equivalent report-7 wrapper. The 5110 uses a serial-keypad lifecycle, so it
  is a protocol-family negative control rather than a 3210 input-path oracle.
- A Nokia 3310 v6.39 run reaches an interactive idle-like frame without
  executing its equivalent report-7 wrapper.

Report 7 is therefore not a generic DCT3 DSP/MMI-ready event. Any 3210 model
that produces it must be derived from the 3210's own observed boundary.

## Acceptance criteria

The next confidence increment is a 3330-specific structural summary: first
executed PC, interrupt activity, last stable startup phase and MAD2/CCONT
requests. A useful failure is an organic firmware-visible hardware request; a
firmware-PC hook added only for the 3330 is a portability failure.
