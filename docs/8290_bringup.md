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
The own native-upload evidence is separate below; startup self-test consumers
remain open.
A sibling's working HLE or EEPROM is not evidence for those contracts.

## Own identity-6 native upload boundary

`nsb7stage` is a separate research composition, not the normal machine.
It selects identity 6 from the own compiled fragment, a declared unmeasured
13 MHz execution clock and the own reset-release bit-0 path. No resident
mask is supplied, and identity 6 is not a measured fitted-part assertion.
The MCU routine at `2cb2e4..2cb2f2` compares the identity with 6 before
selecting one of two descriptor roots. Both branches exist in this image;
only the identity-6 composition is qualified below.

| Own descriptor (file offset) | Destination / words | Payload SHA1 |
| --- | --- | --- |
| `11ee88` verifier | `0f00` / 223 | `6646da3c5be9c70deda7e0b5b9f257d5d2ace815` |
| `11dca0` program fragment | `ff80` / 104 | `440bf49f1eba4cadb12f7f7581c992b0025807d6` |
| `1183a4` support upload | `fd00` / 638 | `70c931c40b3e7cdebc8d09f2488e3cc1e3c848d9` |
| `11ddcc` second loader | `0a00` / 623 | `89080bea5785eb944586574a3e1463145d8d4460` |

The support upload is not the second loader: the latter's descriptor is
`0a00,1000,026f,0200,03e8,0000`, and the staged device compares every one of
its 623 words against the own source before entry. The extraction tool's
`--product 8290` verifier/fragment/loader modes pin the corresponding source
hash and descriptor; `--loader` denotes the support upload, consistent with
that tool's existing contract.

Native execution releases the 223-word verifier twice, publishes
`0000/0006/0006/0006` at DSP PC `0f64`, executes the 126-word first loader,
verifies the complete 623-word second loader at `0a00`, then stops at absent
resident program address `2c75`. This is not a returned success or completed
runtime handshake. No substitute opcode, return or runtime HLE is enabled.

```sh
.venv/bin/python tools/nsb7_native_boundary_check.py \
  mame/mame roms tmp/nsb7-native-boundary
```

The fresh-directory runner requires the ordered native publications and
controlled missing-code stop. The MCU failure-frame baseline remains separate.
Next recover startup self-test consumers and the post-upload transport
contract; native continuation itself requires an authentic resident source.
