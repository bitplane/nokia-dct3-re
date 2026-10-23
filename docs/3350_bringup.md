# Nokia 3350 NHM-9 bring-up

## Current boundary

`noki3350` is a fail-closed MCU research configuration, not a booting phone.
Own v5.30 execution installs the exact 247-word DSP program and reaches its
first ownership wait at `31dc82..31dc86`. No DSP completion, service or radio
peer is enabled. Graphical boot, firmware UI input delivery, SIM, registration,
calls and SMS remain unestablished. The package supplies no PMM record stream;
the firmware creates its own catalogue headers from initially erased storage.
Do not attach a 3330 or other donor's calibration tail. The external emulator's
3350 v5.22 profile reports interactive presentation after grafting a 3330
EEPROM block; that is a protocol/provisioning clue, not an admissible own-data
fixture for this v5.30 configuration.

The separate `nhm9stage` research configuration executes the uploaded verifier
with the product's own 104-word fragment and 247-word program extent. At the
declared, unmeasured 13 MHz execution cadence it starts at `0f00` at
0.127803 seconds, rejects the verifier's write to immutable fragment cell
`ff87`, and stops on an unavailable program fetch at `861f` at 0.129389
seconds. The qualified verifier itself contains `f074 861f` at DSP
`0fd7..0fd8`: an explicit CALL to the missing address, not an inferred target
from a corrupted return. The recovered 104-word fragment covers only
`ff80..ffe7`; it does not supply `861f`. The call's implementation and its
effect on the verifier remain unknown without a matching resident program
source. This is not a completed handshake. No
ready-bit projection, fabricated return or HLE completion is enabled.

Reproduce this negative native boundary with a fresh run directory:

```sh
.venv/bin/python tools/nhm9_native_boundary_check.py \
  mame/mame roms tmp/nhm9-native-boundary
```

The checker qualifies the source image and CALL words, requires the own
247-word release and immutable-fragment probe, rejects crashes and any
completion/handoff, and records the native output beside the MAME log.

## Own keypad contract

The qualified v5.30 image supplies the normal 25-byte map at `45bd94` and
five-byte special map at `45bdb0`. Decoder `3a94e8` calls scanner `3a3568`;
normal input indexes `table[raw + 25 * selector]`, while special input indexes
`special[raw - 80 + 5 * selector]`. The selector byte is at `1244fe`.
Scanner `3a35ce..3a36ae` independently establishes five rows, active-low
columns and raw index `row * 5 + column`. Its literal pool selects row signal
`20028`, column input `2002a`, direction `200a8` and IRQ mask `2006b`.
The special map is `3e 0d 3e 3e 3e`, selecting Power on column bit 1.

| Row | Column 0 | Column 1 | Column 2 | Column 3 | Column 4 |
| --- | --- | --- | --- | --- | --- |
| 0 | 3e | 1a | 0a | 18 | 19 |
| 1 | 3e | 0f | 01 | 02 | 0e |
| 2 | 3e | 3e | 06 | 05 | 04 |
| 3 | 3e | 3e | 09 | 08 | 07 |
| 4 | 3e | 03 | 0b | 17 | 0c |

The bytes match the separately mapped NHM-2 key table, supporting the
external emulator's shared-layout hypothesis with own-image evidence.
The machine selects its own five-row/bit-1 wiring contract and reuses only
the matching host input layout. `nhm9_keypad_check.py` qualifies the own image,
table bytes and decoder pointers, then exercises all 18 physical contacts
against all five row drives (90 reads), plus Power press/release. This is
MMIO controller conformance, not an organic firmware scan or UI-delivery
claim. The fixture temporarily drives controller registers while firmware
remains in its measured DSP ownership wait; it does not alter firmware RAM.

```sh
.venv/bin/python tools/nhm9_keypad_check.py \
  mame/mame roms tmp/nhm9-keypad
```

`tools/nhm9_package_check.py` hash-qualifies the installer, walks every one of
its 29 adjacent gzip members and strictly decodes all seven phone-record
streams. The MCU header and all six PPM headers independently identify
`V 05.30`, `15-03-03`, `NHM-9`. PPM identities A–F come from each header,
not the installer's compressed-member ordering. Its restore manifest names
the `Rom6Imagefile` and `Rom6PpmFile` families; that does not supply the
resident ROM6 mask.

| Input | Gzip offset | CPU span | Raw SHA-1 |
| --- | --- | --- | --- |
| MCU | `ee42a` | `200000..46ffff` | `2ba87fcd10619b6eb80f7a0f9542c09a52922dd0` |
| PPM A | `8e105` | `470000..52ffff` | `d98d55996cdf6cdd8c7b0fa39ede9002384464e1` |
| Combined A | none; contiguous join | `200000..52ffff` | `3d0dce9c098bdd619c6f3684f739796f64b66169` |

The source installer SHA-256 is
`e9686c1c5171157b4308a0bdf96008ac040e866e6f5c31ca1ed993bac4e34acd`.
Source acquisition is recorded in `roms/archive-dct3-packages/manifest.json`.
`nhm9_530.exe` is an identical wrapper, not independent firmware evidence.

```sh
.venv/bin/python tools/nhm9_package_check.py \
  roms/archive-dct3-packages/nhm9_05.30_install.EXE \
  --ppm A --output roms/noki3350/3350f530a.fls
```

The output is exactly `0x330000` bytes in CPU order, with neither padding nor
fabricated provisioning. Inspection without `--output` is read-only.
`make normalize-3350` prepares this same input. Shared appearance or nearby
model numbers are not evidence for the 3330's display, keys or peer grammar.

## MCU acceptance boundary

The configuration uses capacity-compatible 4 MiB Intel flash and the documented
256 KiB SRAM window (`100000..13ffff`), leaving upper aliases unmapped.
Matching boot/DSP masks are separately `NO_DUMP`; no sibling mask is supplied.
The shared MCU boot-transfer HLE is retained, not presented as boot-ROM
execution. Key contacts have separate controller conformance, not firmware
UI acceptance. The 96x72 screen is a diagnostic
storage view, not a validated panel viewport or orientation.

The passive observer establishes:

- all `0xd0000` bytes after the executable are erased at fixture entry;
- MCU flash programming makes 200 bus writes and creates `PMM` at `530006`
  and `EEPROM` at `5e0006`, while preserving every executable byte;
- the complete 28-entry catalogue relocates to `120f8c`;
- the installed program at MCU `11e00` matches all 247 own source words;
- release at `31dc3a` supplies `0100,0300,0019,8000,0001,0001,0200`
  at `100f6..10102`, not the sibling verifier's argument sequence;
- the transfer loop has source `200040`, stride `0400`, buffers `10200/10600`
  and loop bound `0cbf` (3263), but the silent DSP does not acknowledge its
  **first** transfer; control remains `05` and sampled execution stays at
  `31dc82..31dc86` through ten seconds.

Fresh catalogue creation is not a valid calibration or identity result.
The checker protects this first-wait boundary, not a completed 3263-transfer
exchange. Use a new run directory:

```sh
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame --run-dir RUN -- \
  noki3350 -rompath ../roms -video none -sound none -nothrottle \
  -seconds_to_run 12 -log -autoboot_delay 0 \
  -autoboot_script ../tools/noki3350_boot_observe.lua
.venv/bin/python tools/noki3350_boot_check.py roms/noki3350/3350f530a.fls RUN
```

Native execution of the own upload is separately bounded by its explicit
missing-code CALL above. Further native progress requires that routine's
matching implementation. Do not acknowledge the transfer from an HLE count
or install a final verdict merely to reach self-tests.

## Hardware and firmware boundaries

Nokia's combined NHM-2/5/6/9 **General Information**, issue 2 (April 2002),
printed page 7, explicitly lists the 3350 as module UW4 III (0201880), with
Intel 32-Mbit flash and Samsung 2-Mbit SRAM. These are 4 MiB and 256 KiB,
respectively, not a justification for inheriting the driver's larger SRAM
window. The exact fitted flash identity/geometry and upper SRAM aliases still
need product evidence.

The combined **System Module**, printed page 47, describes an 84x48 GD47
panel in its generic UI section. That is not a reliable 3350-specific geometry:
the own-image candidate initializer at `3a3b22` clears nine banks of 96 bytes
(`3a3b7e`, `3a3b98`) and additionally handles command `4a` with 101-byte
loops. Its command/data/status offsets are `6e/2e/6d`; control `25` is written
at `3a3b34`. Driver selection and visible-area/segment orientation remain
unvalidated. Do not flatten this to the generic manual's GD47 description.

The own keypad scan at `3a359c..3a36a0` drives rows at offset `28`, reads
columns at `2a` and examines five column bits. This bounds the matrix interface,
not the physical key-name map. A conservative 32-instruction literal-seeded
MMIO analysis resolves 646 candidate accesses from 314 seeds; 137 seeds hit
the span bound and four hit decode gaps. Indexed, indirect and table-driven
accesses remain outside that count, including parts of GENSIO.

Primary documents retained locally (SHA-256):

- [General Information](https://emoc.org/materiel/telephone_mobile_nokia_3310/3310_sm_level_34/2-NHM-2.PDF): `roms/reference-docs/nhm9/nhm9-general-info.pdf`, `97e05ab8ff7fb44d3e9a924d0887d71258c85efc733c060a0f0f9b1cc56e20e4`.
- [System Module](https://emoc.org/materiel/telephone_mobile_nokia_3310/3310_sm_level_34/3-NHM-2.PDF): `roms/reference-docs/nhm9/nhm9-system-module.pdf`, `eee7ef3ff0380cd042dca2cdc20416da6b51a623107bc1c9ad735593a35251c0`.

## Product-state reference

The ignored reference collection also holds a 65,536-byte ZIP member labelled
`3350 virgin eeprom 005F0000.fls` from the
[Twister firmware archive](https://archive.org/details/Twister_Mobile_Phone_Flashing_Box).
SHA-256 is `337e7c97f5bf8e0f73965f816a845b10f271581566edaeb7eca5a3fe75b910be`;
the ZIP member CRC is verified. Its label is not factory provenance: apparent
user-data remnants are present, the PMMCAT parser finds no catalogue, and no
match to the v5.30 validators has been established. It remains an opaque
reference, not a default seed, checksum-repair fixture or substitute for the
missing resident DSP routine. Source member identity is in
`roms/twister-dct3/manifest.json`; no raw firmware or personal fields are
committed to this repository.

## Own DSP uploads

The source contains a 28-pointer descriptor catalogue at `3c2248..3c22b7`.
The following are statically extracted inputs, **not observed execution or a
recovered complete resident mask**:

| Slot | Descriptor | Words | Payload SHA-1 |
| --- | --- | --- | --- |
| 0 | `3ead0c`: `fd00 ff80 027e 0500 0078 0000` | 638 | `dc3c2a37913e07a2962d09f6a17cde676a04dd76` |
| 15 | `3f0c48`: `ff80 ff80 0068 0200 008c 0000` | 104 | `440bf49f1eba4cadb12f7f7581c992b0025807d6` |
| 27 | `3f1e30`: `0f00 0000 00f7 0f00 00dc 0000` | 247 | `1fc2116ac98a106df41f3822322377a3101aabd5` |

The 104-word bootstrap fragment is byte-identical to the separately qualified
8250 fragment. The loader differs, and the verifier-format program is 247
words rather than the 223-word sibling program. Reuse only the identical
fragment bytes; do not inherit upload counts, arguments, callbacks or a final
verdict. The passive MCU run independently confirms relocation and selection
of slot 27; runtime loader/fragment selection is not yet established.

`tools/extract_nsm3_verifier.py --product 3350` independently checks the full
source hash, exact descriptor and payload hash. `--loader` and `--fragment`
select the other two own uploads. The retained files in `roms/research/nhm9/`
are `nhm9-v530-{verifier,loader,fragment}.bin`. None enables an HLE completion
or a handset configuration.
