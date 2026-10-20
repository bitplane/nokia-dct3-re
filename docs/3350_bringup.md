# Nokia 3350 NHM-9 bring-up

## Current boundary

The acquired v5.30 installer supplies a qualified executable baseline, not a
booting machine. No hardware profile, runtime DSP contract, input, SIM,
registration, call or SMS acceptance is yet established. The package supplies
no PMM record stream; do not attach a 3330 or other donor's calibration tail.

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
fabricated provisioning. Inspection without `--output` is read-only. The next
technical step is a product-local hardware/MMIO and bootstrap census before
adding an executable machine configuration. Shared appearance or nearby model
numbers are not evidence for the 3330's display, keys or peer grammar.

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
verdict. MCU relocation/selection of the catalogue still needs tracing.

`tools/extract_nsm3_verifier.py --product 3350` independently checks the full
source hash, exact descriptor and payload hash. `--loader` and `--fragment`
select the other two own uploads. The retained files in `roms/research/nhm9/`
are `nhm9-v530-{verifier,loader,fragment}.bin`. None enables an HLE completion
or a handset configuration.
