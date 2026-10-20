# Nokia 6130 NSK-3 bring-up boundary

## Current result

The acquired v5.61 installer contains two independently qualified MCU/PPM-A
pairs. Both normalize to a1MiB flash extent200000..2fffff. Conservative
`noki6130` executes both variants from erased external storage and renders
the exact CONTACT SERVICE frame. The silent DSP leaves shared bootstrap
cells unacknowledged; no usable boot, input or phone service is accepted.
Factory EEPROM content, fitted flash/RAM geometry, display viewport and DSP
mask compatibility remain unproved.

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

## Reset and peripheral contract

Both images enter ARM at200040 and switch to Thumb at2000ec, with initial
stack literal10fa5c. Header descriptorf4 declares two-byte EEPROM addresses,
32-byte pages and8192-byte capacity. This is not the 5130's2KiB contract.
The profile uses capacity-compatible trial flash and a bounded64KiB RAM
decode covering the stack; neither is a fitted-part identification.

The static checker anchors the own instruction fragments in both variants
(boot3 code sites are10h lower than boot4):

| boot4 site | Contract |
| --- | --- |
| `2a765e/2a772a/2a779c` | EEPROM SDA on PUP bit0, SCL on bit2 |
| `2a8a62` | GENSIO select28/value22, command2a, RX2d, status29/ready bit2 |
| `2a8fa0/2a9006` | LCD command2c/data2b |
| `2aa788` | Five-row scanner, row31/direction2f/column30 |
| `2ac6ea` | Raw power-key comparison84 |
| `29916c` | SIMI IIR38/control39 initialization |

The default mandatory DSP backend has disabled bootstrap and service
contracts: it supplies neither a completion nor a foreign mask. Ordinary
host key labels, physical timing and SIM operation remain unvalidated.

## Cold execution checkpoint

```sh
make build
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame \
  --run-dir tmp/nsk3-boot4-qualified -- noki6130 -bios 561a4 \
  -rompath ../roms -video none -sound none -nothrottle -seconds_to_run 16 \
  -log -autoboot_delay 0 -autoboot_script ../tools/nsk3_boot_observe.lua
.venv/bin/python tools/nsk3_boot_check.py roms/noki6130/source \
  tmp/nsk3-boot4-qualified --variant boot4
```

Use a fresh run directory. Boot3 uses BIOS561a3, a separate directory and
`--variant boot3`. Both16-second runs retain unchanged executable flash and
initialize the erased8KiB EEPROM to the same firmware-owned bytes. The5s/15s
frames are identical CONTACT SERVICE. Shared cells remain0000/ffff/ffff/ffff
after12 MCU writes; the sampled PC is the supervisor wait2a02f6..2a02fc
(boot3 minus10h), not a direct DSP-poll instruction. No native, graphical
idle or fitted-hardware claim follows from this failure presentation.

## Next boundary

The separate native checkpoint below closes the boot4 upload boundary, not
the startup checks. Recover the direct producer/caller of the early reason04
restart and the surviving failed self-test/storage predicate. Do not import
5130 EEPROM state or synthesize a successful verdict to clear the frame.

## Native ROM4 compatibility checkpoint

`nsk3r4t` uses the same own boot4 executable, erased8KiB EEPROM and declared
partial hardware, replacing only the silent peer with the acquired NSE-1
ROM4 program/DROM and its52MHz research clock. It is a separate instrument,
not the normal machine or evidence of a fitted NSK-3 mask. Boot3 is not paired
with this resident ABI.

```sh
mkdir -p roms/nsk3r4t
cp roms/noki5110/nse1_rom4_dsp_program.bin \
  roms/noki5110/nse1_rom4_dsp_data.bin roms/nsk3r4t/
make build
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame \
  --run-dir tmp/nsk3-native-checkpoint -- nsk3r4t -rompath ../roms \
  -video none -sound none -nothrottle -seconds_to_run 16 -log \
  -autoboot_delay 0 -autoboot_script ../tools/nsk3_boot_observe.lua
.venv/bin/python tools/nsk3_boot_check.py roms/noki6130/source \
  tmp/nsk3-native-checkpoint --variant boot4 --native-roms roms/nsk3r4t
```

Use a fresh run directory. The first release contains the exact own372-byte
bootstrap. Two passes each accept4 into loader state10b840 before64 alternating
zero handoffs at MCU100fe/10100. The loader copies63 groups of512 words plus
510 words and two sentinel words; the checker observes all128 handoffs,
their ordering and final identity/verdict stores4/0 at10b846/10b844. The first
pass ends around0.1182s, the second around0.50195s. A firmware reset request
at2ac696,0.385927692s carries reason04 stored at10ff60; it is retained as an
unresolved startup observation, not suppressed or attributed by analogy.

By1s the native DSP is sampled at resident408d and remains there through16s;
no unsupported/illegal instruction is logged in this bounded run. The MCU
returns to its supervisor wait and both reviewed frames still say CONTACT
SERVICE. Own executable flash stays unchanged; the firmware-initialized EEPROM
has a separate reviewed hash from the silent control. Loader acceptance does
not establish interactivity, SIM initialization, stable radio, speech or
long-run native execution. Those remain unaccepted.
