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

These are Nokia flashing-container components. They do not match the current
MAME declarations for `3330f450c.fls` (0x350000 bytes) and
`3330 virgin eeprom 005f0000.fls` (0x10000 bytes), even after removing the
observed 0x48-byte service headers. No unverified concatenation or renaming has
been performed.

## Current result

- 3210 v6.00: exact CONTACT SERVICE oracle reproduced after native 24C128 and
  CCONT device extraction (`d8a9a7a58e587be8`).
- 3330 v4.50: execution baseline pending a documented conversion from the
  service files or a legitimately obtained normalized dump matching MAME.
- Confidence gained already: the build and machine definitions are
  multi-source/multi-ROM capable; the first two extracted hardware components
  contain no 3210 firmware addresses.

## Acceptance criteria

Once normalized inputs exist, record ROM audit results, first executed PC,
interrupt activity, last stable startup phase, display writes/frame hash, and
the first divergence from the 3210 path. A useful failure is an organic
firmware-visible hardware request; a firmware-PC hook added only for the 3330
is a portability failure.
