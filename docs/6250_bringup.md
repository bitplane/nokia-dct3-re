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
the existing MAD2 release-mask `04` / running-status `10` contract.

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

## Next question

Recover the live bootstrap/loader order and attach this product's acquired
bytes to an exclusive staged DSP executor. Any immutable PROM or peripheral
input remains explicitly unvalidated until independent evidence supports it;
an isolated fixture completion must not become a forced handset verdict.
