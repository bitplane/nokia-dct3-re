# Nokia 6190 / NSB-3 admission

## Current boundary

Own NSB-3 v6.13 MCU and PPM A/B/C service regions are qualified and extracted;
there is no machine profile or runtime acceptance yet. Startup, peripheral
instructions, logical key tables and both DSP catalogues are independently
mapped. Next recover the runtime catalogue selector and upload/reset contract
before choosing a resident mask or HLE composition. No donor EEPROM is supplied.

Collection provenance and the unprovided MCU-to-PPM gap are in
`roms/README.md`. Reproduce the static report:

```sh
.venv/bin/python tools/nsb3_static_check.py roms/noki6190/v613/nsb3___6.130.bin
```

The normalized MCU SHA256 is
`4ae57f0eed01125efbe0f4e63abf5412b44539069b424f5d6dcf030659a8ebda`.
Addresses below refer to this own image in big-endian CPU order.

## Startup and peripheral contract

ARM startup begins at `200040`; the ADD/BX pair at `2000e4` enters Thumb
at `2000ec`. The own startup stack literal is `10fbf4`.

| Boundary | Own instruction/table evidence | Recovered contract |
| --- | --- | --- |
| EEPROM | `2b057e`, `2b064a`, `2b06bc` | SDA bit 0 and SCL bit 2 at `20020`; direction at `20024` |
| CCONT serial | `2b129e`, pool `2b1374` | Select `28=22`, byte write `2a`, status `29` bit 2, receive `2d` |
| LCD serial | `2b19e4`, `2b1a4a` | Status `29` bit 0; command `2c`, data `2b` |
| Keypad scanner | Entry `2b2a2c`, fragment `2b2a60`, pool `2b2bc0` | Five-row scan, row signal `31`, direction `2f`, columns `30`; raw `row*5+column` |
| SIMI | `2a19c0` | Own IIR/control initialization fragment; no card traffic qualified |

Offsets in the table are relative to MAD2 `20000`. Header descriptor `f4`
decodes to 8 KiB, two-byte addressing and 32-byte pages; this is not a fitted
part identification. No calibration values, serial timings or fitted LCD
geometry follow from these instruction fragments.

Decoder `2b359e` calls the scanner and translates layout zero through normal
table `2d69f8`; selector RAM is `10c482`. The own normal table is:

```text
3e 3e 3e 3e 3e
11 19 01 02 03
0e 17 04 05 06
0f 18 07 08 09
10 1a 0c 0a 0b
```

Special table `2d6a14` is `3e 3e 3e 3e 0d`, selecting raw `84` / column
bit 4 as logical Power `0d`. Matching sibling logical tables do not prove
shared physical side-key labels, IRQ delivery or interactive UI acceptance.

## Dual DSP catalogues

Two `70`-byte initializers each provide 28 pointer slots, with null slots
11/26/27 and 25 bounded nonzero descriptors. The checker uses the already
tested shared catalogue reader, not a guessed descriptor extent.

| Pointer table | Initialized RAM | Slot-25 bootstrap | Identity written to DSP data `0803` | Payload SHA1 |
| --- | --- | --- | --- | --- |
| `2b67d0` | `10ad9c` | `2bcf08` | 3 | `73b0cc6e6fc602f3a2f7934eb0c35d7f31c96b3f` |
| `2b6848` | `10ad2c` | `2c2100` | 4 | `e1e8f15a305bf32aea98f444074f30828d933fc4` |

Both bootstrap headers are `0f00 0000 00ba 0700 00b4 0000` (186 words).
The identities are explicit `76f8 0803 0003/0004` instructions, not inferred
from a product label. The installer names ROM4 files, but the runtime selector
and compatibility with acquired resident masks remain unproved. Declared
payload ranges are not proof of execution, relocation or complete mask content.
