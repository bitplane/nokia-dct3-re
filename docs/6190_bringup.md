# Nokia 6190 / NSB-3 admission

## Current boundary

`noki6190` is a conservative MCU research instrument. Two independent
erased-storage runs initialize the same 8 KiB EEPROM, preserve the own flash
and settle in a firmware idle loop. Passive startup capture additionally
qualifies a graphical `CONTACT SERVICE` failure and the exact own 186-word
identity-4 bootstrap installation. Native execution, fitted-mask compatibility,
interactive input, SIM and radio remain unqualified. Next identify the failed
self-test and validate the upload/reset boundary before choosing a resident
mask or HLE completion. No donor EEPROM is supplied.

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
is statically recovered: initial consumer `2946a0` reads context word
`10ad0a` and uses catalogue RAM `10ad9c` only for value 3, otherwise
`10ad2c`. Indexed consumer `294acc` applies the same test. Both initial cold
runs observe value 4. This is runtime branch selection, not fitted-mask
compatibility. Declared
payload ranges are not proof of execution, relocation or complete mask content.

## Initial erased-storage instrument

The own installer INI says `EraseDefinedArea=Yes`, `LowEraseLimit=200000`,
`HighEraseLimit=2FFFFF`. Therefore the image builder fills the unsupplied
`2d6d00..2e0000` gap with erased bytes, not donor content. Reproduce into a
fresh file:

```sh
.venv/bin/python tools/nsb3_archive.py roms/archive-dct3-packages/nsb3_613.exe --image roms/noki6190/6190f613a.fls
```

The 1 MiB image SHA256 is
`927f94aed4ece8e84ad2eda28614bcede1272e89a2d046b16192f88499da19c2`.
The machine uses capacity-compatible Intel 28F800B3T and 24C64 devices;
fitted identities are not proved. It shares only hardware construction with
the 5190 profile, then applies its own independently checked contracts.
Its minimum RAM window through `10ffff` covers stack `10fbf4`; upper aliases
remain unmapped. Host logical key positions match the own table, not a claim
that all 5210 physical legends apply to the 6190. Boot and DSP masks are
`NO_DUMP`, boot transfer remains HLE, SIM socket is empty, and no DSP or
application peer acknowledges firmware work.

After `make build`, use a fresh isolated directory:

```sh
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame --run-dir tmp/nsb3-new -- noki6190 -rompath /data/gaz/src/nokia-dct3-re/roms -noreadconfig -video none -sound none -nothrottle -log -seconds_to_run 6 -autoboot_delay 0 -autoboot_script /data/gaz/src/nokia-dct3-re/tools/nsb3_boot_observe.lua
.venv/bin/python tools/nsb3_boot_check.py roms/noki6190/6190f613a.fls tmp/nsb3-new
```

Samples at seconds 1..5 show PC `2a8ac8/2a8acc`, LR `2a8a89`, selector
`0004`, catalogue first pointers `2b7168/2bd0a8`, DSP control `02`, shared
words `10000=0000`, `10002=ffff`. This is the idle-loop latch test at
`2a8ac6..2a8acc`, not by itself a DSP ownership wait or proof of completed
boot. Firmware changes 68 EEPROM bytes from erased state; the resulting
8 KiB SHA256 is `4dc2b8ea96b930813282e930ebb9cddfddb916809c8f17d972540d59dd004917`.
Complete flash remains byte-identical to the own installer-derived image.
`tmp/nsb3-erased-first` and `tmp/nsb3-erased-repeat` agree on full EEPROM,
flash and card files. The next trace must distinguish pending hardware work
from a normally idle task; do not synthesize messages from a sampled PC alone.

## Passive graphical and upload boundary

`tools/nsb3_startup_observe.lua` adds bounded read-only debugger samples,
an installed-program dump and a five-second screenshot. Use the same fresh
isolated command above with `-debug -debugger none` and this autoboot script,
then check with:

```sh
.venv/bin/python tools/nsb3_boot_check.py roms/noki6190/6190f613a.fls tmp/nsb3-startup-qualified --startup
```

The 372-byte shared-window dump exactly matches the own catalogue-4 bootstrap
at `2c210c`, SHA1 `e1e8f15a305bf32aea98f444074f30828d933fc4`. This proves
installation, not execution by a DSP. The reviewed 84x48 screenshot says
`CONTACT SERVICE`; its RGB pixel SHA256 is
`ed09afeeadae383b77840654453b183a05e10b2cad5ea5757e8c803cbd6b674b`.
Own flash and complete EEPROM remain equal to the cold-run controls.

The sampled idle permission routine `28dff4` returns zero. Its complete
short-circuit chain tests byte `10be1a`, predicate `27562c`, then byte
`1080ef`; it returns one only if all pass. Zero is not a direct DSP wait.
At five seconds the idle latch is `01`, companion `00`, global control `05`,
FIQ/IRQ pending both zero, masks `e3/de`. Early catalogue breakpoints did not
fire in this autoboot observation window, so they do not establish absence
of earlier activity; the exact installed payload is the stronger evidence.

## Explicit native ROM4 compatibility input

`nsb3r4t` replaces only the DSP HLE backend with the acquired NSE-1 ROM4
program/data inputs at a declared 52 MHz research cadence. It retains own
NSB-3 executable, erased EEPROM, full-scale default ADC2 and empty SIM socket.
This is not a relabelled NSB-3 mask, verified clock or native RF/voice claim.
The ordinary `noki6190` remains unchanged. Mask filenames and hashes retain
their NSE-1 provenance in the ROM declarations and `roms/README.md`.

The first run exposed a real compatibility-configuration defect: the default
program overlay ended at `2800`, while own catalogue-4 entry 1 (descriptor
`2bd53c`, payload `2bd548`, destination `25b4`) installs through `2b2a`.
A passive program write tap observes exactly 811 writes at `2800..2b2a`.
Before widening the overlay, 800 of those words read as resident ROM instead
of their uploaded values. This profile now admits overlay through `2fff`;
the extent is a research geometry, not physical SRAM identification.
All 811 words then match own source bytes beginning at `2bd9e0` exactly.

`tools/nsb3_native_upload_observe.lua` captures that upload window during a
fresh two-second `nsb3r4t` run. A separate fresh six-second run with the
startup observer captures native state and the panel. Check both with:

```sh
.venv/bin/python tools/nsb3_native_check.py roms/noki6190/6190f613a.fls tmp/nsb3-native-upload-fixed tmp/nsb3-native-fixed-graphical
```

Optional `--negative-upload tmp/nsb3-native-upload/nsb3_program_2800.bin`
checks the retained pre-fix 800-word mismatch, not merely a passing source
convention. The accepted run observes DSP PC `408d`, `ILLEGAL=0`, `IDLE=1`
at five seconds and a terminal interface summary with 1313 completion strobes,
76 mailbox writes, 1272 slot expiries and 1296 frame expiries. These are
bounded execution/liveness observations, not RF acquisition or speech proof.

The five-second frame is entirely white (RGB SHA256
`5fdab89cdada4b6ac821d32facb589dfb541e9856fe3b569e8da49646fc3d249`),
not standby. Own flash remains unchanged; the independently initialized
8 KiB EEPROM SHA256 is
`53584321c1f9e317e59cf67401fb5a28aa84fc4db32c67d264731825aa194d19`,
33 bytes different from the silent-peer cold control. No edited provision or
identity is imported. Next decode own acquisition/self-test gates under the
full-scale ADC input; do not infer an analogue fix from another product's
successful nominal-value experiment.

## Own acquisition rejection and nominal comparison

`acquisition_contract` pins the complete helper `27a3e0..27a4b4`, including
both conditional sample paths and signed range comparisons. Reader `2b1eee`
uses own channel table `2d6a1c` (`00 04 05 06 07 03 01 02 02 01`);
source 7 selects ADC2 through the own serial ADC routine `2b5244`.
Gain/offset are RAM `10fe54/10fe58`; both transformed samples must be in
inclusive `1800..5500`. At identity gain and zero offset, the transform is
integer `raw * 1500 / 229`. Accumulator `27a4b4` decrements remaining byte
`10a0e2` only after a successful helper return.

The full-scale native control records 16 failed attempts at return site
`27a4c2`, each with both samples `1a2c` (6700), gain `3f800000`, offset
zero and remaining `0a`. This independently explains the uncompleted
acquisition under raw ADC2 `3ff`; it does not assign physical units by itself.

The [original 6190 owner manual](https://www.instructionsmanuals.com/sites/default/files/2019-05/Nokia-6190-en.pdf),
Technical Information, specifies nominal operating voltage 3.6 V. The retained
PDF is `roms/reference-docs/nsb3/nokia-6190-owner-manual.pdf`, SHA256
`a0a5a8e039bc3cb6548553b4bcf546aee2ef685ee51a8f571b65a1e46c3ce144`.
Combined with the previously documented typical CCONT VBAT transfer of
6.5 mV/bit, this supplies one external raw-input hypothesis, `round(3600/6.5)=554`.
It is not measured NSB-3 board calibration or a value chosen by sweeping.

Only the research machine exposes the existing two-choice ADC2 input. Its
default remains full-scale. Prepare a fresh config-only comparison:

```sh
.venv/bin/python tools/nsb3_nominal_input.py roms/noki6190/6190f613a.fls tmp/nsb3-acquisition-nominal-new
```

Run the prepared directory using the isolated `nsb3r4t` command with
`-debug -debugger none`, six seconds and `tools/nsb3_acquisition_observe.lua`.
An independently fresh full-scale run with the same observer supplies the
control. Compare them with:

```sh
.venv/bin/python tools/nsb3_acquisition_check.py roms/noki6190/6190f613a.fls tmp/nsb3-acquisition-fullscale tmp/nsb3-acquisition-nominal
```

The nominal run logs external raw `22a`, ten successful attempts with both
samples `0e2c` (3628), and remaining values `0a..01` before each decrement.
The entire EEPROM remains equal to the full-scale native control, and both
flash images remain unchanged. The five-second panel stays white. Thus this
acquisition rejection is removed without resolving presentation; no report,
counter, timer, identity or calibration content was injected. The remaining
self-test/provisioning and application lifecycle must be investigated separately.
