# Nokia 8290 / NSB-7 inputs

## Current boundary

The acquired NSB-7 v5.22 package now has a qualified, contiguous MCU/PPM-A
image. `noki8290` is a fail-closed MCU research configuration: it reaches
the supervisor and renders the firmware's complete `CONTACT SERVICE`
failure frame. Idle, organic keypad/SIM, registration, calls and SMS are
not established. DSP exchange/service replies are disabled; own internal
ROM regions are explicitly undumped rather than attached sibling files.
No PMM, resident DSP mask, hardware profile or provisioning is supplied by
this normalization. The [Nokia 8290 user guide](https://www.instructionsmanuals.com/sites/default/files/2019-05/Nokia-8290-en.pdf)
identifies the product as NSB-7 and GSM1900; the [NSB-7 service manual](https://www.manualslib.com/manual/2171641/Nokia-Nsb-7-Series.html)
is the independent hardware-reference starting point.

## Qualified package contract

`roms/archive-dct3-packages/nsb7_5.22.exe` is a ZIP self-extractor with five
members, not the adjacent-gzip installer format. Its SHA256 is
`5c28267a14e5d3e36ed4dc166987da42e07b04a83ff88499dc38ee4485df0040`.
The own restore INI selects `AMERICA_A`, names both phone streams and gives
erase limits `200000..3cffff`. Both phone headers identify `V  5.22`,
`08-11-01`, `NSB-7`; the PPM identifies pack A.

| Member | CPU start | Bytes | Decoded SHA1 |
| --- | --- | --- | --- |
| `nsb7___5.220` | `200000` | `160000` | `6f2c32bfdb5dce75019f85ac56271fbf05a216cf` |
| `nsb7___5.22a` | `360000` | `70000` | `e09921c52e90888992792eceb57d2241f4eb091c` |

The joined image is `1d0000` bytes, SHA1
`4dacc50f57fe295168a53d3e5855a81acf35ab96`, with no padding or donor tail.

```sh
.venv/bin/python tools/nsb7_package_check.py \
  roms/archive-dct3-packages/nsb7_5.22.exe \
  --output roms/noki8290/8290f522a.fls
```

## MCU baseline and direct-access boundary

The ARM entry at `200040` selects the `20000` MAD2 window. A constant-index
MMIO census finds 243 literal seeds and 747 resolved candidate accesses;
these are linear-scan candidates, not exhaustive control-flow ownership.
The reviewed CCONT read path selects `22` at `305796`, writes `2c` at
`3057b4`, polls `6d` bit 2 at `3057b8`, and reads `6c` at `3057c0`.
Thus the byte-write-ready GENSIO mechanism is selected independently.

The six-second sample is in supervisor `2f82fe..2f8304`, with global
control `05`; shared observations remain `0000,ffff,0001,0001` at
`10000/10004/100fe/10100`. These are observations, not a DSP success verdict.
The 84x48 presentation matches the failure text, but keypad wiring and the
remaining board defaults are provisional, not NSB-7 hardware acceptance.

```sh
make build
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame \
  --run-dir tmp/nsb7-boot -- noki8290 -rompath ../roms -video none \
  -sound none -nothrottle -seconds_to_run 7 -log -verbose \
  -autoboot_delay 0 -autoboot_script ../tools/nsb7_boot_observe.lua
.venv/bin/python tools/nsb7_boot_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-boot
```

Use a fresh run directory for the erased-storage baseline. The checker pins
own source bytes, sampled continuation/shared state and exact RGB failure
pixels. It does not test task messages, input, native DSP or phone services.
Next recover the own DSP identity/upload and startup self-test consumers.
A sibling's working HLE or EEPROM is not evidence for those contracts.
