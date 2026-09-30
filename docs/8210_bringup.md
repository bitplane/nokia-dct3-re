# Nokia 8210 bring-up

## Current boundary

NSM-3 v5.31 PPM C remains at the final sparse-flash verification wait,
`0x2cadce`. No working-phone promotion is claimed. The next question is the
DSP memory/operand contract governing the staged verifier's final publication:
the explicit-version fixture produces 6, while the collaborator bridge reports
an 8210 verdict of `1eff`. A matching raw capture has not been recovered, so
neither that reported value nor the fixture's assumed version is promoted.

## Inputs and hardware

The stock MCU/PPM-C composition exactly matches the canonical MAME flash.
The canonical product-local PMM tail is also acquired; provenance and hashes
are in `roms/README.md`. No sibling provisioning image is enabled.

Nokia's [NSM-3 System Module, issue 1 12/1999](https://www.eserviceinfo.com/preview_html.php?fileid=5456&previewid=3011)
identifies MAD2WD1, CCONT, CHAPS and COBBA-GJP, a 13 MHz system clock and
flash-backed product data. The stock service package assigns the same MCU
to ROM5 and ROM6; [service bulletin SB-050](https://www.eserviceinfo.com/downloadsm/225117/NOKIA_NSM3-050.html)
documents the later ROM6 board revision. The current profile selects ROM6.
Internal DSP ROM images remain absent. Uniform-fill audit members are not
executable DSP evidence.

## Established contracts

- GENSIO control `0x22` selects CCONT. The byte write at `0x302d32` is followed
  by receive-ready polling at `0x302d36`; control bit 2 is clear. NSM-3 therefore
  uses byte-write-triggered ready, independently of other products.
- `0x2cac80` parks shared offsets 2/4/6 at `0xffff`; `0x2cad46` accepts silicon
  PROM version 6 or 5 from offset 4. The HLE peer publishes version 6 after that
  physical sentinel write. It does not overlay reads.
- `0x2cad60..0x2cad9a` transfers 115 blocks of 512 halfwords; the final block
  has 510 halfwords and two `0xffff` terminators. Source starts at `0x200040`
  and advances by `0x20`, so this is sparse ARM-flash input, not DSP program
  code. The trace independently observes 58 alternating ownership pairs.
- `0x2cadce` waits for shared offset 2 to leave `0xffff`, then copies offsets
  2/0 into product-local bootstrap state at `0x135774 + 0x0e/0x0c`. The HLE
  does not publish the unvalidated final results.

The boot descriptor pointer at `0x135808` resolves to flash `0x31bcf0`.
Its six-word descriptor is `0f00 0000 00df 0f00 00dc 0000`; the 223-word
program at flash `0x31bcfc` is independently observed byte-for-byte at shared
offset `0xe00` before the sparse transfer. Its SHA-1 is
`6646da3c5be9c70deda7e0b5b9f257d5d2ace815`. The `00dc` descriptor field's
meaning remains unassigned. Execution outside that staged image must not be
filled with guessed mask-ROM instructions.

The isolated `nsm3verify` core fixture loads this program at `0x0f00`, supplies
the observed MCU buffer descriptors (`087b=0100`, `087c=0300`,
`087d:087e=0000:e800`, `0881=0200`), and sends blocks only at the program's
alternating ownership polls. It consumes all 116 blocks without leaving the
staged program. Its default case writes ports `000e=1387`, `0000=000d`,
`000c=0010` and stops fail-closed on the first port `002d` read (reported PC
`0f9f`, after the PORTR operands).

Comparison cases attach the existing COBBA register model. The verifier reads
register F, waits on register D's `bits 1:0=0`/`bits 3:2=3` status handshake,
then reads F again. Supplying F as 0 or `0016` changes only the companion
publication at data `0800`. `0016` is a metamorphic fixture value, not an
8210 hardware identity. The model's reset value 0 remains uncharacterized for
this product and is not claimed to be a measured COBBA identity.

The absolute-MVPD operand order independently agrees with GNU binutils 2.43.1:
`7cf8 0801 ff87` copies program `ff87` to data `0801`. Likewise,
`7df8 0803 ff87` attempts a write to program `ff87`. The acquired ROM4 PROM
has value 4 at that address. The verifier sets `PMST=ffa8`; the
[TI CPU reference, table 4-3](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
defines `MP/MC` as bit 6, clear here, enabling on-chip ROM. `OVLY` and `DROM`
are set. Do not mistake the high interrupt-vector-pointer bit for `MP/MC`.
The fixture explicitly supplies version 6 (the selected NSM-3 board contract),
or version 4 as a negative family comparison, and ignores the program's
attempted write of 6. Publications at data `0801` and `0802` follow that input;
data `0803` retains the program's constant 6. Treating all program space as RAM
would incorrectly turn the attempted write into a version source. No ROM6
mask image has been recovered. The fingerprint is stored at data `04f7:04f8`
under the current core; its production semantics remain unresolved.

These tests establish the publication semantics under the current clean-room
core and explicit peripheral inputs, not silicon equivalence, a measured ROM6
version-cell layout, or a valid handset completion. The collaborator bridge's
`DSPB_HLE_VERIFY` comment reports `1eff` from a physical 8210; that is a lead
requiring its raw trace, ROM revision and memory mapping, not an input to copy.
The operand-order audit is closed; the unresolved evidence is ROM6's mapping
at `ff87` under these PMST settings and the matching physical trace/program.
No matching raw verdict capture or ROM6 mask image was found in the checked-out
collaborator repository. Its hardware-bridge source reports the value but
does not include the capture. A code comment is not a substitute for those inputs.

The independent assembler input and output are:

| Assembly | Encoded words |
| --- | --- |
| `mvdp *(0803), ff87` | `7df8 0803 ff87` |
| `mvpd ff87, *(0801)` | `7cf8 0801 ff87` |
| `portw *(0008), 000e` | `75f8 0008 000e` |
| `portr 002d, *(0009)` | `74f8 0009 002d` |

The analysis tool was built outside the repository from
[GNU binutils 2.43.1](https://ftp.gnu.org/gnu/binutils/binutils-2.43.1.tar.xz),
SHA-256 `13f74202a3c4c51118b797a39ea4200d3f6cfbe224da6d1d95bb938480132dfd`,
target `tic54x-coff`. Its code is not part of the MAME implementation.

The existing independent emulator clears the final sentinel at read time.
That is a compatibility behavior, not a captured NSM-3 verification result,
and is not imported. Packet/service, SIM, keypad and radio contracts remain
unpromoted behind this boundary.

## Acceptance

`make normalize-8210` reconstructs the stock flash and checks PMM identity.
`make verify-8210-bootstrap` validates the ROM6 publication, exact upload
handoff order and fail-closed final wait. It is a frontier gate, not boot/UI
acceptance. Existing product profiles are unchanged.

`make verify-8210-verifier` extracts the pinned staged image, executes it in
an isolated run directory and checks the unsupported-peripheral case plus
COBBA and immutable-version sensitivity cases. Unsupported peripheral reads
are fatal rather than synthetic responses.
