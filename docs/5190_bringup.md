# Nokia 5190 NSB-1 source boundary

## Current boundary

The acquired service package supplies independently identified NSB-1 v6.71
MCU and PPM A/B streams. It is not a full phone dump and does not establish
graphical boot or phone services. The separate `noki5190` MCU research
configuration executes the own input, initializes the own EEPROM geometry,
installs its exact ROM4-catalogue program and reaches a supervisor flag loop.
The next work is that flag's scheduler/interrupt contract and the DSP release
lifecycle. Do not equate this loop with a missing DSP acknowledgement without
tracing its owner, or inherit the 5110 machine merely because the restore
manifest names ROM4.

## Qualified executable input

Source: [archived Nokia service package](https://archive.org/download/Nokia_DCT3_firmwares/5190e671.zip).
Archive SHA-256:
`7c7167aba12342fa1ef3810c93bf518b0f8aeb79f67e9637e19d76b34e781128`.
The acquisition manifest is `roms/archive-dct3-packages/manifest.json`.

| Stream | Supplied CPU span | Decoded SHA-1 |
| --- | --- | --- |
| `nsb1___6.710` MCU | `200000..2d2cff` | `f953fd19093f17f36845cf46417f7ddd750ace80` |
| `nsb1___6.71a` PPM A | `2e0000..2fffff` | `0c55c1825cb722872c18e10a714f4be27cae3871` |
| `nsb1___6.71b` PPM B | `2e0000..2fffff` | `8c3aa2858a3bc82cda1644a81d03263983d1c491` |

All three independently contain `V  6.71`, `25-01-01`, `NSB-1`, `(c) NMP.`.
The restore INI names MCU/PPM ROM4 files and erase span `200000..2fffff`;
USA selects A, USA_T9 selects B. A ROM4 filename is not a resident DSP dump
or proof that the recovered NSE-1 mask fits this product.

`tools/nsb1_package_check.py` pins the archive, closes its six-member
inventory, validates the restore map, strictly decodes all three record
streams and checks embedded PPM identities. Normalization writes a 1 MiB
flash view with the **unsupplied** `2d2d00..2dffff` gap erased. This gap is
not acquired data or calibrated storage. No external EEPROM, identity,
security state or internal boot/DSP mask is supplied or synthesized.

```sh
.venv/bin/python tools/nsb1_package_check.py \
  roms/archive-dct3-packages/5190e671.zip \
  --ppm A --output roms/noki5190/5190f671a.fls
```

The normalized A SHA-1 is
`27b8f0cb8640e5b423960b206e9aee065f32d726`. Omitting `--output` is read-only.
The checker rejects source overwrite and never extracts archive paths.

## Own EEPROM initializer

Routine `2ab168` reads ROM byte `200005` through its literal at `2ab1dc`.
The own source byte is `f4`. It constructs the runtime descriptor at
`10c4a4`: byte 0 is `(f4 >> 6) & 1 = 1`, byte 1 is
`1 << (((f4 >> 3) & 7) - 1) = 32`, and word 4 is
`1 << ((f4 & 7) + 9) = 8192`. This establishes the firmware-declared
two-byte word addressing, 32-byte write page and 8 KiB capacity; it does
not independently identify the fitted EEPROM part.

Read helper `2ab0e6` calls address setup `2aaf58`. The latter checks the
address against descriptor word 4, selects slave byte `a0` for nonzero
descriptor byte 0, sends `address >> 8` at `2aafa8`, then sends its low byte
at `2aafb4`, and issues repeated START/read slave `a1` for reads.
Its zero-mode branch instead folds high address bits into the slave byte.
Consequently a one-byte-addressed 24C16 fixture is not faithful to this
initializer, even if its donor contents happen to satisfy a checksum.

Validator `26cc4c` reads `120` bytes, calls byte-sum routine `2a0fbc` over
the first `11c`, and compares the resulting 16-bit sum with the big-endian
32-bit field at `11c`. This confirms one external checksum clue without
supplying the absent calibration/identity contents. The separate checksum
at `1fe` still requires its complete producer/reader derivation.

## MCU research configuration

The own ARM entry at `200040` initializes stacks using `10fc54` and
switches to Thumb at `2000e8`. The trial maps only the minimum 64 KiB SRAM
window covering these stacks, not the generic driver's larger window;
physical fitted capacity and aliases remain unproved. Flash uses a
capacity-compatible 1 MiB trial part. The external I2C device implements
the own 8 KiB/32-byte-page protocol, with erased initial contents and an
unmeasured 5 ms write cycle. Boot/DSP masks and EEPROM dump remain separate
`NO_DUMP` entries. Shared MCU boot-transfer HLE remains explicit; no DSP
acknowledgement, service, radio or SIM peer is enabled.

Own serial routine `2ad4b0` selects control `22` at `20028`, sends at
`2002a`, polls `20029` bit 2 and reads `2002d`. LCD clear `2ae0ac` shares
selection/status at `28/29`, command `2c`, data `2b`, six banks and 84 columns.
EEPROM byte transfer `2ac704` independently drives PUP signal `20020` bit 0
as SDA and bit 2 as SCL, releasing SDA through direction `20024` bit 0.
These port-level contracts justify the existing device composition, not
measured serial timing or fitted controller identification.

Key scanner `2af050` uses five rows, signal `31`, direction `2f`, columns
`30` and mask `33`. Decoder `2af768` selects normal table `2d2984` and
special table `2d29a0` using selector byte `10c466`. The normal table matches
the existing NSE-1 host layout; the active special table is
`3e 3e 3e 3e 0d`, selecting Power on bit 4, not bit 1. These are independently
mapped source facts; physical input/UI delivery has not yet been accepted.

Fresh passive runs observe the following without firmware RAM/MMIO writes:

- runtime EEPROM descriptor is mode 1, page `20`, capacity `2000`;
- both own catalogues initialize at `10adc0` and `10ad50`, whose first
  pointers are `2b35a4` and `2b94e4`; family selector is 4;
- all 186 words installed at MCU `11e00` match the own descriptor at
  `2be564`, payload SHA-1 `e1e8f15a305bf32aea98f444074f30828d933fc4`;
- 246 shared-memory writes occur before the observed supervisor park;
- through five seconds PC is `2a4672..2a4678`, SP `10c6dc`, with EEPROM
  geometry/catalogues stable. The loop tests flag `10c540`; its owner and
  expected interrupt transition remain unresolved;
- persisted flash equals the normalized source. The 8 KiB EEPROM contains
  firmware-created writes, which are not proof of valid calibration or identity.

Use a fresh `RUN` directory:

```sh
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame --run-dir RUN -- \
  noki5190 -rompath ../roms -video none -sound none -nothrottle \
  -seconds_to_run 6 -log -autoboot_delay 0 \
  -autoboot_script ../tools/nsb1_boot_observe.lua
.venv/bin/python tools/nsb1_boot_check.py roms/noki5190/5190f671a.fls RUN
```

The checker pins the source, exact installed program, five ordered geometry/
catalogue observations, stack bounds, supervisor park and unchanged executable
flash. It is a boundary regression, not a graphical boot oracle.
