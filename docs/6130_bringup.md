# Nokia 6130 NSK-3 bring-up boundary

## Current result

The acquired v5.61 installer contains two independently qualified MCU/PPM-A
pairs. Both normalize to a1MiB flash extent200000..2fffff. No NSK-3 machine
configuration or runtime acceptance is established. External EEPROM content,
fitted flash/RAM geometry and DSP mask compatibility remain unproved.

## Own firmware inputs

Source: `roms/archive-dct3-packages/nsk3_561.exe`, downloaded from
<https://archive.org/download/Nokia_DCT3_firmwares/nsk3_561.exe>.
SHA-256: `43d9413dd8972ca76fbfe899228cbc3ca9df3d2b028267fa9fdc4230ce15baae`.
The installer is extracted as an archive, never executed.

| Label | MCU member | MCU bytes (hex) | PPM-A member | Catalogue | Flash SHA-1 |
| --- | --- | --- | --- | --- | --- |
| boot4 | `nsk3__05.610` | `c7e00` | `nsk3__05.61a` | `2ae384` | `5469c8fe048036673124dd2aece4ef71da1cce22` |
| boot3 | `nsk3___5.610` | `c8b00` | `nsk3___5.61a` | `2ae374` | `bac9e599da510c97c59eca0412db987d7e0bf946` |

Both MCU streams start200000; both PPM streams start2d0000 and contain30000
bytes. Only the unprogrammed gap is filled withff. No identity, calibration,
lock or EEPROM data is fabricated. PPM-A members differ and are not swapped.
Own MCU strings distinguish `V 05.61` and `V  5.61`, both dated22-11-99 and
naming NSK-3.

```sh
7z x -y -oroms/noki6130/source roms/archive-dct3-packages/nsk3_561.exe
.venv/bin/python tools/nsk3_static_check.py roms/noki6130/source \
  --output roms/noki6130
```

The checker pins decoded member hashes, spans and identity strings, then
validates the28-slot catalogue using the existing bounded descriptor parser.
Slot25 has header `0f00 0000 00ba 0700 00b4 0000`, followed by186 DSP words.
Its C54x instruction at payload byte24 stores an immediate to data0803:
4 in boot4 and3 in boot3. These labels record a software bootstrap distinction,
not a fitted-mask identity or compatibility with the acquired NSE-1 ROM4.
The372-byte bootstrap payloads differ at only four byte positions: the
identity immediate and three absolute CALL operands. Boot3 calls8008 twice
and8010 once; boot4 instead calls8023 twice and802b once. The checker anchors
all three C54x CALL instructions. Resident ABI selection is therefore a real
code difference, not merely alternative product text or a PPM choice.

## Next boundary

Recover reset/interworking, bootstrap publication and own memory/peripheral
layout. Compare both loaders before choosing any native compatibility backend.
Then establish a fail-closed configuration and measure its first unmet cold
execution boundary. Do not inherit the 5130 hardware profile, EEPROM fixture
or addresses from the NSK prefix or a shared descriptor shape.
