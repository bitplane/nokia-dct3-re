# Firmware images (bring your own)

This repository contains **no firmware**. The Nokia 3210 flash image is Nokia's
copyrighted code and is **not redistributable**, so it is git-ignored and must be
supplied by you. Everything in this repo is original tooling, analysis, and
annotations that operate *on* such an image — it is useless to anyone who does not
already have a legitimately-obtained dump.

## Reference 3210 image

The **NSE-8/9 v06.00 3210** flash file. It is distributed (by third parties) inside
a `.rar` of flash files, e.g.:

- `https://firmware.center/firmware/Nokia/3210%20(NSE-8-9)/Flash%20Files/NSE-8%20v.06.00%203210%20NSE-9.rar`

Extract the `.fls` from that archive.

## Verify what you have

Match one of these SHA-256 sums so analysis/addresses line up with the docs:

| file | sha256 | notes |
|---|---|---|
| `3210f600a.fls` | `7bf29b96e544b682c4d6d01c7a6eaef89909c4191a52d829115d37b31c0c0d8a` | raw flash dump as extracted |
| `3210f600a_swap16.bin` | `66d2ec57385099d6dca8d93b75d72fcde496f3f8a3246331351d8ebce6fac8c1` | halfword-swapped image used by the tools/Ghidra (32-bit literals are halfword-swapped in the raw `.fls`; the tools expect the swapped form) |
| `nokia_3210_nse-8_v05_01_full_hu.fls` | `62de70cd5451444cfcd4ed6c6d8a9a84c0e783a557f8489f2dc5faa283b66272` | local v5.01 same-product runtime/static control; installed into the MAME set as BIOS `501` under `3210f501.fls` |

If your sums differ you have a different firmware version; the absolute addresses
in `docs/` and `ghidra/symbols/3210.csv` are specific to `3210f600a`.

## Where to put it

Place the files here (git-ignored):

```
roms/
  3210f600a.fls
  3210f600a_swap16.bin      # produce via the swap step (see Makefile / docs)
  noki3210/                 # MAME ROM set layout, for `make run`
```

The EEPROM is a separate 24C128 used for product, calibration, identity,
security, and user NV data. An all-`0xFF` image represents an erased chip, not a
factory-provisioned handset. The project normally generates an explicit test
profile; see `docs/eeprom_analysis.md`.

## Portability ROMs

Additional firmware is used only as local validation material. Put each complete MAME set in its
own ignored directory (`roms/noki3330/`, and so on), run `make audit-roms PHONE=<set>`, and never
commit an image or extracted archive.

### Nokia 3330 NHM-6 v4.50

The first portability target is the 3330. Firmware.center provides service-format archive
`NHM-6 v.04.50 3330.rar` containing:

| archive member | size | sha256 |
|---|---:|---|
| `NHM6NX04.500` | 2,689,928 | `fac546ceaf7f56e536383730ec6cadc6519f9a11a08a3a9688f2439e77f3eca8` |
| `NHM6NX04.50E` | 787,296 | `318fcc3fa9d2a926e574fea89af9d8cda81796acb904f196636b886ef4e6ce9e` |
| `3330 virgin eeprom.pmm` | 65,608 | `83e58fe48153a7c1d60247ba7b80b72e05ddb8fd5443aa0d9ed976ec3ad88808` |

These are Wintesla record streams. `make normalize-3330` validates and removes
their nine-byte per-`0x2000` record headers, then combines the contiguous MCU
and PPM E regions. The generated files remain git-ignored.

The existing MAME declaration expects:

| file | size | CRC32 | SHA-1 |
|---|---:|---|---|
| `3330f450c.fls` | `0x350000` | `259313e7` | `88bcc39d9358fd8a8562fe3a0280f0ce82f5897f` |
| `3330 virgin eeprom 005f0000.fls` | `0x10000` | `23459c10` | `68481effb39d90a1639e8f261009c66e97d3e668` |

Only an exact MAME audit match establishes the canonical 3330 baseline. Differently sourced
service images remain useful RE inputs, but their results must be labelled with their own hashes.
The local PPM E result is declared separately as BIOS `450e` with SHA-1
`7e88caa4963c57ebbd4d919023e38103ff8b528a`; run it with `make smoke-3330e`.
