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

## Next question

Extract the verifier and its own live inputs behind `429842`, then execute
those acquired bytes at an exclusive DSP boundary. Counts and completion
values from 6210, 8250 or other firmware are not a substitute for that result.
