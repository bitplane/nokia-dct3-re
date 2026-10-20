# Nokia 5190 NSB-1 source boundary

## Current boundary

The acquired service package supplies independently identified NSB-1 v6.71
MCU and PPM A/B streams. It is not a full phone dump and does not establish
graphical boot, hardware configuration, input or phone services. No MAME
machine is declared yet. The next work is the own MCU entry/upload contract,
physical memory/EEPROM topology and fitted ASIC evidence; do not inherit the
5110 machine merely because the restore manifest names ROM4.

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
