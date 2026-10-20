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
