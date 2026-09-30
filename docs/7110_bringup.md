# Nokia 7110 NSE-5 bring-up

## Current boundary

NSE-5 v5.01 PPM C completes 228 alternating sparse-flash buffer handoffs and
waits at `0x432f96..0x432f9c` for the DSP-owned halfword at MCU `0x10002` to
change from `0xffff`. Graphical boot, firmware-owned button handling, SIM,
phonebook and registration are not validated. `make verify-7110-bootstrap`
protects this boundary without supplying a final result.

The initial generic configuration published three calibrated values after
64 exchanges. Those values preceded completion of this product's upload and
are not evidence of successful 7110 verification. The product-local profile
now acknowledges ownership only; it does not publish a final verdict.

## Inputs

- `roms/noki7110/7110f501_ppmc.fls`: `0x390000` bytes,
  SHA-1 `53af8324919f455ba8199d2c05f7a921cfb811d5`.
- Product-local PMM `7110 virgin eeprom 005fa000.fls`: `0x6000` bytes,
  SHA-1 `8b4dd782fc9d1306268ba63124ee463ac646912b`, mapped at ARM `0x5fa000`.
  Its filename does not independently establish factory provenance.
- The legacy `dsp_prom`, `dsp_drom` and `dsp_pdrom` placeholders satisfy
  existing ROM declarations only. They are not executable 7110 mask ROMs.

## Loader contract

`0x432eae` reads a verifier descriptor through RAM `0x1670b0`. The descriptor
at ARM `0x22d904` is `0f00 0000 00d2 0700 00b4 0000`: 210 program words,
loaded at DSP P:`0x0f00`. The extracted big-endian program SHA-1 is
`caca7599d9ca1a7dddf2df37f32be4aacd420deb`. It is **not** the 223-word
NSM-3/NPE-3 verifier.

The initial remaining count is `0x0001:c800`. `0x432f2c..0x432f60` sends
227 blocks of 512 halfwords sampled every `0x20` bytes from ARM `0x200040`.
The terminal block contains 510 further samples and two `0xffff` terminators.
Ownership cells are MCU `0x100fe` and `0x10100`, alternating 114 times each.
After completion, `0x432f9e..0x432fa6` copies words 1 and 0 into the
bootstrap state at `0x16702c`, offsets `0x0c` and `0x0a` respectively.
`0x1670b0` is the descriptor-pointer slot, not the result structure.

GENSIO selection `0x25`, CCONT command/data at `0x2c`, read at `0x6c` and
status at `0x6d` operate through the existing controller contract in this
bounded run. No alternate wiring or serial-ready projection was required.

## Staged DSP program

The program sets PMST `0xffa8`, writes 4 through `MVDP` to P:`0xff87`,
reads that location into D:`0x0802`, then processes each input block through
P:`0x8023`. Its final checksum routine calls P:`0x802b`. Both routines occur
at those addresses in the recovered NSE-1 ROM4 program; their instruction
sequences also match local routines at P:`0x0fb7` and `0x0fbf` in this upload.
This corroborates a ROM4-compatible verifier ABI, not complete interchangeability
of the 5110 and 7110 DSP images.

TI's [C54x CPU reference, SPRU131G, table 4-3](https://www.ti.com/lit/pdf/spru131)
places `MP/MC` at PMST bit 6, not bit 7. For `0xffa8`, bit 6 is clear
(on-chip program ROM enabled), bit 5 is set (data RAM overlaid into program
space), and bit 3 is set (`DROM`). Consequently the generic CPU contract does
not require the write to P:`0xff87` to replace an enabled mask-ROM word.
The fixture's read-only upper program mapping is consistent with that CPU
contract. The remaining product-specific uncertainty is the fitted mask-ROM
contents/extent, not the meaning of these PMST bits.

The computed checksum is stored at D:`0x1f0e/0x1f0f`, separately from the
MCU-visible final result. P:`0x0f65` stores a COBBA-derived result to D:`0x0800`;
P:`0x0f67` reads P:`0xff87` into D:`0x0801`, then idles. A checksum alone is
therefore not a legitimate mailbox completion value. PMST-dependent mapping,
the P:`0xff87` write/read behavior and COBBA port-`0x2d` replies must be
established before any result is promoted into the handset model.

## Executed verifier fixture

`make verify-7110-verifier` executes the stock 210-word program with the
product-local 228-block flash stream. P:`0x8000..0xffff` is read-only recovered
NSE-1 ROM4 code, including both CRC routines and the version word at `0xff87`.
The fixture checks the source hash; no ROM4 code is transcribed into sources.
This is an explicit memory-map assumption, not proof of the fitted 7110 die.

All 228 blocks execute in order. With peripheral reads left unsupported the
program stops at COBBA port `0x2d`, after the checksum calculation. With the
existing COBBA model it reaches IDLE at P:`0x0f6b` and publishes:

- D:`0x0800`: `0x0000`, or `0x0016` under the register-F sensitivity fixture;
- D:`0x0801..0x0803`: `0x0004` in both cases;
- D:`0x1f0e/0x1f0f`: checksum `0xa98692ad`, unchanged by the peripheral input.

These are reproducible executable fixture results, not a measured handset
publication. The full handset remains fail-closed: the COBBA register-F
reset/read contract and product-specific PMST mapping are not promoted merely
because the sensitivity fixture finishes.

## Next question

### Immediate MCU consumer

The loader returns at `0x432fb8`. Its direct call at `0x49ff12` is followed
by a test of the caller's saved `r4`, not the loader's mailbox fields:
`0x49ff16..0x49ff1c` optionally calls `0x45c61c`, and `0x49ff20` calls
service initialization at `0x3bc2a8`. Thus the immediate caller does not
validate the copied word-0/word-1 pair as a checksum verdict. This does not
exclude later readers of the bootstrap structure or identify the correct
COBBA reply.

The result-store literal is at `0x4330b0` and contains `0x16702c`.
The descriptor-pointer literal at `0x433160` contains `0x1670b0`.
These are distinct objects. Four halfword-aligned literal occurrences of
`0x16702c` appear in the pinned image (`0x4330b0`, `0x4334e8`, `0x433754`,
`0x49ff30`); scanning every halfword for Thumb-1 PC-relative loads targeting
those occurrences yields 15 candidate loads. Code/data classification and
indirect or derived pointers remain outside that count.

An additional literal at `0x45c33c` points directly to `0x167036`, the
word-0 result field. Its reader at `0x45bf8a..0x45bfb0` emits three bytes:
bits 8..11 plus `0x37`, bits 4..7 plus `0x30`, and bits 0..3 plus `0x30`,
then a zero terminator. This is a concrete formatting use of the returned
COBBA word, not a pass/fail comparison. The containing command selector and
the register-F hardware meaning have not yet been established; the formatting
alone does not justify assuming a zero reset value. The subtract-cascade
at `0x45bf50..0x45bf62` selects this reader for request `0x0d` to the
information handler starting at `0x45bf02`. No externally documented name
for that request has been established.

This is a bounded disassembly result from the pinned flash: the linear Thumb
scan found one direct `BL 0x432eae`. It is not an exhaustive indirect-call or
bootstrap-structure reader census. Decode the flash in big-endian Thumb
order without an additional halfword swap; swapping it again produces
plausible-looking but incorrect instructions.

Does primary hardware material or the firmware's consumers establish the
7110's COBBA register-F reset/read result and fitted mask-ROM contents used
above? The generic PMST ROM-enable semantics are established by TI, but a
matching verifier ABI alone does not identify the complete mask ROM. Answer
the product-specific questions before enabling the later service responder.
Display geometry, Navi Roller and slide wiring remain separate product
contracts, not inherited 3310 inputs.
