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

- 3210 v6.00: the default profile reproduces the exact fault-screen oracle
  (`d8a9a7a58e587be8`) with the native 24C128 and extracted CCONT devices.
- 3330 v4.50 PPM E: `make smoke-3330e RUN_DIR=run_3330e_smoke SECONDS=3`
  completed three emulated seconds at 207% average speed. Firmware performed
  one soft reset and issued a blank LCD transfer (`f102/z504`); no phone-specific
  hook or service model was enabled.
- Confidence gained already: the build and machine definitions are
  multi-source/multi-ROM capable; the first two extracted hardware components
  contain no 3210 firmware addresses.

### Conditional power lifecycle control

Signature matching finds the same four-owner report-7 topology in 3330 v4.50,
including a callback dispatcher equivalent to 3210 callback `0x5d`. A
forcing-free 5110 v5.30 run reaches standby while posting none of these reports;
its healthy startup uses reports `0x14`-`0x17`, and its dormant report-7 branch
is measurement-gated inside the battery/charger dispatcher. This is portability
evidence for a conditional power lifecycle, not a shared boot-readiness event.
Exact 3210 ownership and consumer semantics remain authoritative in
`mmi_settlement.md`. No firmware-PC hook was added for either sibling ROM.

## Nokia 3210 NSE-8 v5.01

An independent full-flash control is stored locally as
`nokia_3210_nse-8_v05_01_full_hu.fls`:

| Bytes | SHA-256 | Identification |
| ---: | --- | --- |
| 2,097,152 | `62de70cd5451444cfcd4ed6c6d8a9a84c0e783a557f8489f2dc5faa283b66272` | `V 05.01`, `NSE-8` |

The conditional power-report wrapper is at `0x2ac5bc`, with the same four
caller families and local predicates as v6.00. Callback `0x5d` preserves its
start and terminal statuses; only timer class `0x51` changes to `0x52` in
v6.00. The task-1 block at `0x26dc20..0x26df14` is instruction-equivalent to
v6.00 `0x27120e..0x271502`, including the continuation comparisons, report
flags, exit selector, event-`0x74` gate, and mode-`0x000c` tail. This confirms a
stable same-product shutdown contract without making it a startup frontier.

The CCONT comparison is also complete at the firmware boundary. The v6.00 ADC
reader `0x2b52cc`, register read/write helpers `0x2afb44`/`0x2afa74`, and IRQ
ISR `0x2b08c6` align uniquely with v5.01 `0x2b2680`, `0x2acf70`/`0x2acea0`,
and `0x2adcf2`. The wider helper blocks are instruction-equivalent, including
GENSIO polling, command construction, watchdog/power helpers, status/mask
calculation and write-one-to-clear acknowledgement. The ADC route tables
(`0x2e2d74`/`0x2d7770`) and CCONT shadow/default tables
(`0x2e2da8`/`0x2d777c`) are byte-identical. This supports one shared 3210
device contract; it does not establish analog units or timing.

The GENSIO SELECT setup is also instruction-equivalent. v6.00 `0x2afbf2`,
`0x2a31fa`, and `0x2a1450` relocate to v5.01 `0x2ad01e`, `0x2a06ce`, and
`0x29e974`; both initialize the same seven latches, mask `0xaf`, and assert
`0x6f.bit0`. `make verify-gensio` covers the runtime contract. The attached
SELECT peripherals remain unknown, so the evidence supports shared latches but
not a shared peer model.

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

The CCONT power domain is also shared at runtime. After an organic shutdown,
`make verify-charger-wake` connects VCHAR and both ROMs observe CCONT cause bit
2, restart the CPU and attached MAD2 peripherals, resample selector 5 and settle
in acting-dead mode `0x0005`. This establishes reset-domain composition, not
physical rail timing or a battery charging model.

The ROM is packaged as MAME BIOS `501` with a BIOS-specific generated EEPROM
profile. `make verify-3210-v501` provides the forcing-free structural runtime
comparison. It observes startup modes `0x0001 -> 0x000d -> 0x0004` and
readiness flags `0x0f`: the same task-1 terminal mode as v6.00. The service
lifecycle retains one visible difference: v5.01 finishes with service-session
status `0x00c9` rather than v6.00's `0x0049`. This is not a boot blocker. With
provisioned identity and the same physical left-softkey fixture,
`make verify-mmi-menu-501` opens the same `Phone book` menu and reproduces the
same stable-pixel oracle as v6.00. The SIM block is relocated by `-0x1d0`:
v5.01 no-SIM/ENABLE are
`0x111a94`/`0x111aa9`, corresponding to v6.00
`0x111c64`/`0x111c79`. The valid-reply setter is instruction-equivalent and a
forcing-free run proves both ROMs clear no-SIM and set ENABLE through the same
device contract. This is evidence that the interactive task-1/SIM contract
spans both 3210 ROMs, not a claim of general v5.01 support.

A DSP-boundary trace confirms that this is not superficial state coincidence.
The v5.01 firmware performs D0 discovery, the type-`0x70` self-test exchange,
contact registration `0x64`, channel-map `0x70`, transport acknowledgements,
service-empty `0x622a`, and the later type-`0x1a` publication through the same
shared-memory device. Its instruction-equivalent contact-ready
consumer is `0x299314` (v6.00 `0x29bc70`), inside initialization loop
`0x2a67aa` (v6.00 `0x2a92d2`). Both revisions settle at service status `0x49`
without the unsupported result-5 lifecycle transition (falsifications ledger:
`external_service_result5_ordinary_startup`). No firmware-state forcing is
involved.

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
