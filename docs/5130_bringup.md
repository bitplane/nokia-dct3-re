# Nokia 5130 NSK-1 bring-up

## Current boundary

Own NSK-1 v5.61 service streams are qualified and normalized; no MAME
configuration, graphical boot, input, SIM or radio acceptance is claimed.
The next step is mapping the own reset/loader and nonvolatile-storage
contracts, then choosing a fail-closed hardware composition. Do not inherit
NSE-1/NSB-1 EEPROM contents, DSP identities, acknowledgement counts or clocks.

## Acquired inputs

Source: [archived NSK-1 service package](https://archive.org/download/Nokia_DCT3_firmwares/nsk1_561.zip).
Archive SHA-256:
`2cd19ef2618cb6678274704041845301918fef0de052f1b077c8399ec38a81e6`.
The existing collection manifest records its acquisition. ZIP CRC checking
and strict contiguous Wintesla decoding pass for all 18 members: two MCU
streams and eight PPM streams per family (`A/B/D/E/F/O/S/Y`). Every decoded
stream contains the NSK-1 product marker. Installers are not executed.

The filename families are kept separate; `2915` is a filename discriminator,
not an established hardware revision or DSP-mask identity.

| Stream | CPU span | Decoded SHA-1 |
| --- | --- | --- |
| `nsk1__05.610` | `200000..2adeff` | `7ff6a6371d06cb4ab8b09cc5ce766484ae84fa36` |
| `nsk1__05.61a` | `2c0000..2fffff` | `93e23bbc816918cc82efc52a017caa802869eea3` |
| `nsk12915.610` | `200000..2aebff` | `ccf88c1a5c20200d34ee636d5ae10d96970a3a5c` |
| `nsk1ppm5.61a` | `2c0000..2fffff` | `5ba9c0c34ac93d73886c5d548942b36e3c7106d0` |

The first family embeds `V 05.61`, the second `V  5.61`; both give
`18-11-99`, `NSK-1`, `(c) NMP.`. The PPM headers match their own MCU family.
Normalization preserves all supplied bytes and fills only the unsupplied
MCU-to-PPM gap with `ff`; it does not create EEPROM or calibration data.

```sh
7z x -y roms/archive-dct3-packages/nsk1_561.zip -otmp/nsk1-v561-package
.venv/bin/python tools/extract_dct3_wintesla.py \
  --mcu tmp/nsk1-v561-package/nsk1__05.610 \
  --ppm tmp/nsk1-v561-package/nsk1__05.61a \
  --flash-output roms/noki5130/5130f561a.fls \
  --expect-flash-sha1 a2a99ec6c52104eebcbae281cf8e845bdc8ea1d9
.venv/bin/python tools/extract_dct3_wintesla.py \
  --mcu tmp/nsk1-v561-package/nsk12915.610 \
  --ppm tmp/nsk1-v561-package/nsk1ppm5.61a \
  --flash-output roms/noki5130/5130f561a-2915.fls \
  --expect-flash-sha1 64068df6b7950b47f34ac141d0ef9c51dc666cca
```

Both outputs are 1 MiB CPU-order images. Their header-declared
`200024..2bffff` big-endian halfword sums match stored values `3d67` and
`e26b`, respectively. This is a bounded integrity calculation, not execution
of the own validator or whole-image authentication. ARM big-endian entry
`200040` reads the header at `200000`, rearranges its bytes and writes the
result to `040000` before setting banked stacks. Disassembling that entry
as Thumb is invalid; later interworking boundaries must be established.

No fitted internal boot/DSP image, external EEPROM or factory identity is
supplied by this package. Native compatibility requires an explicit,
separately labelled instrument; source acquisition alone is not support.
