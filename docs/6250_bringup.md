# Nokia 6250 v5.03 bring-up

## Current boundary

The acquired v5.03 PPM C and product-local PMM execute through independently
recovered MAD2 release and GENSIO/CCONT contracts. A fresh eight-second run
observes 116 zero-valued ownership writes to each DSP buffer, then parks at
`429842` waiting for the final DSP-owned publication. No final verdict is
supplied. LCD, physical input and phone features are not yet validated.

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
execution stops at absent program word `2c75`, around 1.8971 s. No mask
instruction is fabricated. This proves upload progression, not acceptance
of provisioning, graphical boot or physical peripheral identity.

Reproduce a fresh `nhm3stage` run with the same private ROM path and isolated
NVRAM/config directories as above, without the early observer. Expected
process status is 1 for the explicit unavailable-program boundary. Validate
the resulting log with `tools/noki6250_staged_check.py error.log`; exit status
alone is insufficient. `make verify`, `make verify-frontier` and the tool
suite remain the regression guards for existing products.

## Next question

Recover what the MCU requests after this loader boundary and whether the
existing declared runtime DSP peer can answer it from this product's own
records. Missing native mask code and immutable peripheral identity remain
explicitly unvalidated; runtime HLE must not manufacture record/self-test
verdicts merely to reach a frame.
