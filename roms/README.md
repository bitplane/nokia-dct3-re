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

## DSP region caveat

The currently declared `dsp_prom`, `dsp_drom`, and `dsp_pdrom` files are
checksum-valid MAME set members, but the local files are uniform `0xFF` fill:

| file | size | SHA-256 | classification |
|---|---:|---|---|
| `dsp_prom` | 49,152 | `abd8e2a70d51a53287941838446e6ed141005d401faf597fd7c6de0bbc8c329d` | placeholder fill |
| `dsp_drom` | 16,384 | `0fbba07a833d4dcfc7024eaf313661a0ba8f80a05c6d29b8801c612e10e60dee` | placeholder fill |
| `dsp_pdrom` | 4,096 | `f47a8ec3e9aff2318d896942282ad4fe37d6391c82914f54a5da8a37de1300c6` | placeholder fill |

They contain no executable C54x program and cannot support DSP disassembly or
cycle-accurate execution. MAME ROM-audit success proves only that these files
match the current declaration. Run `make audit-dsp-roms PHONE=noki3210` to
classify the actual local regions before using them as evidence. A future real
DSP dump must be identified by product and hash and must not silently replace
these placeholders in a test baseline.

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

### Nokia 3410 NHM-2 v5.46

Place `NHM2NX05.460`, `NHM2NX05.46E`, and `3410 virgin eeprom.pmm` in
`roms/3410-nhm2-v546/`. `make normalize-3410` reconstructs the MCU+PPM and PMM
regions, verifies their pinned hashes, and prepares the `noki3410` ROM set.
The v5.46 set is the default 3410 BIOS; the older local v5.06 MCU/PPM image is
retained as a separate, unprovisioned comparison BIOS.

### Nokia 3310 NHM-5 v6.39 local spike

The bounded 3310 portability spike uses a local, already-combined 2 MiB flash
and its 192 KiB PMM tail as BIOS `639`:

| file | size | SHA-256 |
|---|---:|---|
| `3310f639e.fls` | `0x200000` | `975ec791205f026d647254ee772d7fa32691fa50c72a68eecdaff7c8a5921442` |
| `3310 v2 pmm.bin` | `0x30000` | `dcb2212579f2a2a7059ed85ef81174d337003566ce2f83f284f20bc70aef8bf4` |

This pair is a labelled portability input rather than the canonical 3310 MAME
set. Its PMM is BIOS-specific and must not be combined with the older declared
3310 images.

### Nokia 6110 NSE-3 acquisition status

The primary service manual specifies a 1 MiB Intel TE28F800 program flash and
an independent 8 KiB serial EEPROM. Historical firmware indexes identify
NSE-3 v5.48 MCU + PPM B as the likely baseline, but the surviving indexed
download links found in the July 2026 audit are unavailable or terminate at
obsolete hosts. There is therefore no declared `noki6110` ROM set yet.

Do not pad a later image, reuse a 3210 EEPROM, or declare a 2 MiB flash device.
When an NSE-3 image is obtained, preserve the original archive outside this
repository and record member names, sizes and SHA-256 values. ROM3 and ROM4
variants must be distinguished before runtime results are compared. The
hardware and staged acceptance requirements are in `docs/6110_bringup.md`.
