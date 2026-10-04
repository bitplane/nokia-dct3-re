# Nokia 6250 v5.03 bring-up

## Current boundary

The acquired v5.03 PPM C and product-local PMM reach a correctly dimensioned
96x60 `CONTACT SERVICE` frame in research composition `nhm3hle`, after native
verifier/loaders and request-correlated service-control completion. Interactive
idle, physical input and phone features remain unvalidated. Normal `noki6250`
retains the fail-closed final publication wait at `429842`; the research
composition is not promoted to supported default boot.

## Inputs

- MCU/PPM: `roms/noki6250/6250-503mcuppmc.fls`, SHA1
  `95607ce39c383bda75f1e6aeae67a214b787b0a1`.
- PMM: `roms/noki6250/6250 virgin eeprom 005fa000.fls`, SHA1
  `57c29c8387caf864603d94a22bfb63ace427b7f9`.
- Legacy uniform-fill DSP audit members satisfy ROM declarations only; the
  HLE does not execute them. The missing boot mask is not fabricated.

## Recovered hardware contracts

The CTSI base literal at `4e7f70` is `00020000`. The release routine
`4e7d7e` writes `40` to CTSI+2, sets bit 2 at `4e7dc4`, and polls bit 4
using `LSRS #5`/carry at `4e7dca..4e7dce`. Its reset path clears bit 2 at
`4e7df0` and waits for bit 4 to fall. `PRODUCT_6250` therefore configures
the MAD2 chip-release mask `04` / running-status `10` contract. DSP code
execution is separate: after staging, `4297b6..4297be` sets bit 0 at
`20002`; `429858..42985e` clears it after retaining the publication.
Passive bus observation confirms startup `40 -> 44`, then launch `11`
with verifier word `f7bb` and fields `0100/0300/0001/d000` already present.
The core execution mask is consequently `01`, not the chip-release mask.

Its CCONT routine independently writes control `22` to GENSIO `2d` at
`4f91cc`, writes its command byte at `2c`, polls status `6d` bit 2 at
`4f91d6..4f91dc`, and reads response `6c`. Control bit 2 stays clear;
the existing byte-write receive trigger is selected for this product.

The profile disables the legacy unvalidated 64-transfer success publication.
Transport ownership acknowledgements remain, but neither final results nor
parked DSP identity values are inherited from another product.

## Reproduction

Run `noki6250` with private working/NVRAM/config directories, the acquired
product ROMs, `-noreadconfig -log -video none -sound none -nothrottle`,
`-autoboot_delay 0 -seconds_to_run 9` and
`-autoboot_script tools/noki6250_bootstrap_observe.lua` (absolute path).
The passive observer checks firmware-observed ready signals and the reviewed
ownership sequence; `6250 early hardware gate: PASS` is **not graphical boot**.
It also captures `6250_frontier.png`. Do not reuse preserved flash for a cold
comparison or interpret process exit alone as gate success.

## Staged verifier

The descriptor at flash `21e544` uploads 223 words to program `0f00`.
Its SHA1 is `6646da3c5be9c70deda7e0b5b9f257d5d2ace815`; a passive
live capture matches those bytes exactly. MCU routine `42975a` sets
`087b..0881` to `0100/0300/0001/d000/0001/0001/0200`. The ownership
flags have already fallen to zero at the observer's half-second snapshot.

The isolated `nhm3verify` core fixture consumes this product's 232 sparse
flash blocks (stride `20`, final two words `ffff`) and stops honestly at
peripheral read `002d`, PC `0f9f`. Explicit COBBA and PROM-version
sensitivity fixtures complete at `0f64`, with computed fingerprint
`c62d430c` and PMST `ffa8`. These inputs are comparisons, not measured
6250 silicon identity or an accepted phone self-test result.

Reproduce with `tools/nsm3_verifier_check.py mame/mame
roms/noki6250/6250-503mcuppmc.fls /tmp/6250-verifier --product 6250`.
This gate does not change the live handset's fail-closed configuration.

The same acquired flash contains a 104-word bootstrap fragment at descriptor
`21d384` (payload SHA1 `440bf49f1eba4cadb12f7f7581c992b0025807d6`)
and a 638-word loader at `2178f8` (payload SHA1
`a324738357565c523ddd8dc513e24491d4c89a1c`). Matching descriptors alone
do not establish their execution order or compatibility with missing mask code.

## Live native upload boundary

Research machine `nhm3stage` uses the same product ROM/PMM and configures
only product-local fragment/loader sources. The normal `noki6250` machine
remains on its fail-closed HLE profile. The native verifier starts around
0.0955 s and publishes `0000/0006/0006/0006` around 1.8613 s under the
existing declared COBBA inputs. Firmware then starts its loader, whose
control word is at `0880` rather than the earlier product's `087f`.

The loader issues selector `0014` then 124 selector-`0001` requests. Its
second upload is independently compared against 613 words from this flash
at `21d4bc` (SHA1 `b1df4b301d67c6c4421ae346478c465a7fd20ff0`). Native
execution suspends before absent program word `2c75`, around 1.8971 s. No mask
instruction is fabricated. This proves upload progression, not acceptance
of provisioning, graphical boot or physical peripheral identity.

Reproduce a fresh `nhm3stage` run with the same private ROM path and isolated
NVRAM/config directories as above, without the early observer. Expected
process status is 0 for the explicit silent observation. Validate
the resulting log with `tools/noki6250_staged_check.py error.log`; exit status
alone is insufficient. `make verify`, `make verify-frontier` and the tool
suite remain the regression guards for existing products.

The loader guard runs once per configured CPU clock during this research
stage instead of the coarse microsecond observer. It verifies the second
upload before its first fetch, then suspends at `2c75`; this is an isolation
mechanism, not a recovered physical DSP clock claim. Other staged profiles
retain their existing guard cadence.

## Runtime comparison

With `tools/noki6250_runtime_observe.lua`, silent `nhm3stage` emits ten
post-loader parameter commits at MCU `429e48`: three zero wire values,
then `8102/900f/8426/920c/920c/920f/920f`, with coefficient `3fff`.
At eight seconds the native PC remains `2c75`. The MCU eventually clears
pending itself; no DSP reply or inferred parameter success is supplied.
Validate using `noki6250_staged_check.py error.log --silent-runtime`.

Separate research machine `nhm3hle` transfers ownership to the existing
request-derived runtime HLE at that boundary. It uses the same acquired
ROM/PMM, with transport discovery enabled but no fitted record codec,
registration/channel-map profile or record-verdict override. D0
discovery traverses TX type `05` and RX `8e`; firmware subsequently sends
`70:0d00`, which is answered by the declared compact service-control peer.
Its current frame is a visible service-failure screen, not interactive idle.
Display storage/visible geometry is validated below, but the stream includes
commands `0a` and `11` unused
by the current PCD8544 model. The repeated D0 frame is
`1e0200d0000305014100`; its semantics need product-specific classification.

## Compact service-control contract

Receive worker `504642..50467e` forwards the `7x` family through `47f2f8`,
which constructs an envelope with class at `+3`, body at `+8` and posts
mailbox 2. Passive task-context observation (`100022`, recovered from
receive helper `3c363c`) identifies its receive caller as `307627`.
Dispatcher `30763a..3076ba` routes class `74` to `304494` except command
`32`, which has a separate path.

The subtract cascade at `3044b6..3044c2` selects command `0d` at `304512`.
Flag bit 2 of `17fd15` arms the wait; the handler cancels timer `17`, clears
that flag and interprets body byte `+9` bits 0/1 as faults. Clear bits clear
fault bytes `17fbf0`/`17fbf1`; set bits write `10`/`11`. Runtime research HLE
therefore uses the existing compact request-correlated `74:0d00` response.
This is declared peer behavior, not execution of the missing DSP self-test.

A fresh run observes exactly one consumer with class `74`, command `0d`,
status `00`, armed flags `84`, then endpoint flags/faults `00/00/00`.
Validate using `noki6250_staged_check.py error.log --service-control`.
The service-failure frame persists: this completion does not establish a
provisioning verdict, ordinary startup settlement or interactive idle.

## LCD geometry

Passive GENSIO captures show eight bank addresses `40..47`, each followed
by an X-address `80` and exactly 96 data bytes, repeated for whole frames.
The inferred storage is 96x64. Nokia's original
[6250 user manual, technical specifications](https://www.mobilgyujtemeny.hu/letolt/User%20manual/Nokia/nokia_6250_usermanual_en.pdf)
specifies a 96x60 visible display. `DISPLAY_6250` declares those dimensions;
the previous 84-column default wrapped data into the wrong columns and
corrupted the first text line. The corrected research snapshot is nonblank
96x60 and renders `CONTACT SERVICE` legibly.

Check the independent stream evidence using
`noki6250_staged_check.py error.log --service-control --lcd-stream`. Geometry
does not identify the controller silicon or validate all commands. Commands
`0a` in extended mode and `11` in basic mode remain unmodeled; no guessed
side effect was added. The observed lifecycle byte `17fe24=01`, written at
`4c11d8` during initialization, is not established as the screen's fault cause.

## Next question

Resolve the product-local NV checksum mismatch before adding another peer
response. Task 2 stores fault `0c` at `304330`: its sum from `3040a0` is
`6d57`, whereas logical NV `0254` contains `7095`; companion word `0170`
is `1b0b`. The sum covers bytes `0120..0253`, excluding the two bytes at
`0154..0155`. Reader `510e1c` delegates to `4030e8`, which copies from the
RAM shadow at `15c034`. A passive capture of that shadow independently
recomputes `6d57` and confirms both stored words. Its first 32 checksum-range
bytes match the acquired PMM at file offset `0146`, but a 64-byte contiguous
match does not exist: logical NV is not a proven flat PMM-file mapping.

The population trace resolves that distinction further. Copy primitive
`514648`, called by `48078c`, first loads `0a28` bytes from flash `5fa026`
to shadow `15c034`. It then applies a four-byte record from `5faae6` to
logical `0150` and a two-byte record from `5faaee` to logical `0254`.
The base image's checksum is valid (`6d61` computed and stored), but those
acquired record overlays change byte `0153` from `98` to `8e` and the
stored checksum from `6d61` to `7095`. The resulting `6d57` mismatch is
therefore already represented in the acquired PMM record stream, not a
spontaneous RAM corruption. Whether those records are intended to be active
under this firmware's catalogue rules remains the next question.

Read-only `tools/noki6250_pmm_check.py PMM --trace LOG` independently
replays 15 records, stopping at file `0ba2`, and exactly matches the captured
checksum-range shadow. Its report separates the valid initial image from
the final overlaid image. This confirms record order and write destinations,
and now checks the record checksum byte independently. Reader `402758`
requires sector state 1, decodes length/destination and applies records in
order until header `ffff` or stop bit `0200`. It skips deletion records
(`0100`) and does not validate the low checksum byte on this read path.
Writer helper `402876` computes that byte as the sum of both destination
bytes and payload, modulo 256. All 14 later records satisfy that contract;
the initial record stores `70` where its acquired payload computes `35`.
Thus the two checksum-region overlays are accepted, individually intact
records whose combined NV content is inconsistent. The input's "virgin"
filename is not evidence of factory-valid provisioning.

Decode record selection/validity before repairing any source data.
Changing `0254` to the observed sum without
establishing that contract would merely suppress a verdict and is not an
accepted correction. `noki6250_runtime_observe.lua` captures the fault and
checksum-range shadow without modifying either. LCD controller identity and
remaining command semantics also remain open.
Missing native mask code and immutable peripheral identity remain explicitly
unvalidated; runtime HLE must not manufacture record/self-test verdicts merely
to reach idle.
