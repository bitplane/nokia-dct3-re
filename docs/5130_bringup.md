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

## Startup and upload structure

`tools/nsk1_static_check.py` pins both normalized hashes and checks the
catalogue initializer, all 28 pointer slots and descriptor payload bounds.
Each variant has 25 nonzero descriptors; slots 11, 26 and 27 are null.
The copy initializer declares 112 bytes destined for MCU RAM `10b260`.
Its pointer table starts at `296d10` in the standard stream and `296d00`
in the `2915` stream. Slot 25's descriptor is respectively `29c2e4` and
`29d044`, with raw header `0f00 0000 00ba 0700 00b4 0000` and 186 payload
words. The bootstrap's familiar ABI prefix does not prove a fitted DSP mask
or that firmware selects this slot at runtime. Other descriptor addresses
and lengths genuinely differ between variants; they are not interchangeable.

ARM entry sets banked stack pointers using literal `10f370`, clears the
own BSS starting at `100020`, then uses `ADD/BX` at `2000e4/2000e8` to
enter Thumb at `2000ec`. These establish a 64-KiB addressed RAM requirement
for this startup, not a fitted SRAM capacity or mirror decode.

EEPROM initializer `28efb4` (standard) / `28efa4` (`2915`) reads the
literal header address `200005` and derives the same bit fields as NSB-1:
capacity `1 << ((descriptor & 7)+9)`, transfer page
`1 << (((descriptor >> 3) & 7)-1)`, and address-mode bit 6. Own descriptor
`92` therefore declares 2048 bytes, two-byte transfer pages and one-byte
addressing. The shared helper `dct3_eeprom_descriptor.py` decodes that
grammar while retaining distinct product values and no fitted-part claim.
This says nothing about provisioned identity/security/calibration contents.

```sh
.venv/bin/python tools/nsk1_static_check.py roms/noki5130/5130f561a.fls
.venv/bin/python tools/nsk1_static_check.py \
  roms/noki5130/5130f561a-2915.fls --variant 2915
```

Both real images pass, along with initializer/payload/null-slot mutation
controls. The next boundary is the own MCU's catalogue selection and DSP
release/identity grammar; no release result has been synthesized.

## Cold-transfer contract

The standard image's cold-transfer entry is `273488`; the separately
located `2915` counterpart is `273478`. The former loads catalogue cell
`10b2c4` from literal `2737ac`, exactly slot 25 of the initialized table,
then copies the descriptor payload using its declared count/transfer fields.
It initializes shared words `10000/10002/10004/10006` with zero plus `ffff`
sentinels, and constructs seven argument halfwords beginning at `100f6`:

```
0100 0300 0000 8000 0001 0001 0200
```

The argument-write fragment is at `2734c6..2734f6` (standard) and
`2734b6..2734e6` (`2915`). After the firmware's release/control update,
`27351a` compares shared `10004` against `ffff`; later alternating
handshake loops are at `2735c8/2735d2`, with final `10002` sentinel polling
at `2735da`. Runtime transfer counts and successful results are not yet
observed. Do not replace any of these reads with fabricated values.

The later initializer `273624` clears the shared window, sets ring cursors,
loads descriptor 0 through catalogue base `10b260` at `27373c`, and copies
its first five header halfwords to the argument area before signaling it.
The standard descriptor-0 header matches the established NSE-1 loader
grammar (`fd00 ff80 0244 0500 0078 0000`), while the `2915` payload count
is `0241`. Shared grammar does not make the payloads, variants or resident
masks interchangeable.

These own MCU contracts justify a separately labelled native compatibility
spike against the acquired NSE-1 ROM4 input. Such a spike must keep own MCU
and PPM bytes, erased own external storage, and bounded read-only upload/
CPU observations. It must not promote board/clock/fitted-mask correctness
from a successful handshake, or seed NSE-1 EEPROM to get past later checks.
