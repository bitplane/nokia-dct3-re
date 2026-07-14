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

These are Wintesla record streams. Every record is nine header bytes followed
by `0x2000` payload bytes. Header byte 0 is type `0x0b`, bytes 1–3 are the
big-endian target address, and bytes 5–7 are payload length `0x002000`.
`tools/extract_dct3_wintesla.py` validates type, length and contiguous target
addresses before extracting:

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

## Acceptance criteria

The next confidence increment is a 3330-specific structural summary: first
executed PC, interrupt activity, last stable startup phase and MAD2/CCONT
requests. A useful failure is an organic firmware-visible hardware request; a
firmware-PC hook added only for the 3330 is a portability failure.
