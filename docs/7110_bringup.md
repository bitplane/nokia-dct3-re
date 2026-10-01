# Nokia 7110 NSE-5 bring-up

## Current boundary

NSE-5 v5.01 PPM C completes 228 alternating sparse-flash buffer handoffs and
waits at `0x432f96..0x432f9c` for the DSP-owned halfword at MCU `0x10002` to
change from `0xffff`. Graphical boot, firmware-owned button handling, SIM,
phonebook and registration are not validated. `make verify-7110-bootstrap`
protects this boundary without supplying a final result.

A bounded full-core compatibility trial with the recovered NSE-1 ROM4 program
and data images advances beyond this wait and issues LCD traffic using the
stock 7110 flash and product-local PMM. It does not establish the fitted 7110
mask-ROM identity or complete boot. The normal profile remains fail-closed.
The immediate graphical prerequisite is a SED1565-family controller, not the
previously inherited PCD8544 profile. The normal product now selects SED1565
with a 96-by-65 panel window at segment 18; the DSP completion boundary remains
unchanged.

## Display contract

The full-core trial emits 141 LCD commands and 2,592 data bytes (three
96-column, nine-page transfers). Initialization includes
`a6 a4 a3 a1 c0 22 81 35 2f e3 40 b0 10 00 af`. This command stream is
incompatible with the inherited PCD8544 decoder; its captured pixels are not
evidence of graphical boot. A 20-second trial ends without a soft reset and
with the C54x frame timer still running, but does not validate UI settlement.

Independent [physical 7110 display measurements](https://serdisplib.sourceforge.net/ser/sed1565.html)
identify an on-glass SED1565 controller and a 96-by-65 panel. Its 132-column
controller RAM exposes the panel from column 18, with nine pages and only one
visible bit in the final page. The firmware transfers start with `b0 11 02`,
selecting page zero and column `0x12`, independently corroborating the offset.
The primary
[Epson SED1565 datasheet, revision 1.2](https://serdisplib.sourceforge.net/ser/doc/sed1565.pdf)
defines the 132-by-65 RAM and serial interface. `patches/mame-sed1565.patch`
extends MAME's existing SED15xx family with a distinct command decoder and
serial interface. Existing SED1560 and PCD8544 behavior is unchanged.

`make verify-sed1565` runs 17 executable controller checks without phone
firmware: serial input, command arguments, segment/common directions, start
line, ninth page, invalid pages, column saturation, read-modify-write, display
modes, chip select and reset. Software reset preserves display RAM, display
enable and segment direction; the reset pin additionally restores those
control defaults. Analog contrast, supply/busy timing and the separate static
indicator output remain unmodeled. Controller conformance is not evidence of
a complete 7110 boot.

The passive Lua mirror uses the SED1565 grammar for this product and emits a
native screen snapshot alongside it for independent pixel comparison.

With the candidate ROM4 composition, all four captures in an eight-second
run matched native output pixel-for-pixel at 96 by 65. The frames contain
initialization patterns and then cleared RAM, not a graphical boot UI. The
run issued 141 commands and 2,592 data bytes, with zero soft resets and no
unsupported-controller commands. This establishes the display transport and
decoder for the observed stream, not application startup or mask-ROM identity.

The final sampled PC `0x49fff8` belongs to an interrupt-driven idle loop,
not the verifier wait. `0x49ffec` stores 1 to byte `0x168f04`; IRQ/FIQ
dispatcher entries `0x4cab50` and `0x4cac14` clear it at `0x4cab56` and
`0x4cac1a`. A read-only runtime write-watch observed all three writers.
`0x49fff6..0x49fffc` polls the flag, then returns to idle predicates when it
clears. A sampled PC in this range cannot alone establish a boot deadlock.
The next runtime question is which application-start/input lifecycle follows
the cleared LCD test, while keeping the mask-ROM compatibility assumption
explicit and separate from physical input claims.

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

Reproduce the candidate list with
`tools/find_literal_loads.py 0x16702c --rom roms/noki7110/7110f501_ppmc.fls --encoding big`.
The tool defaults to the older swap16 image format; omitting `--encoding big`
is not a valid absence test on this acquired FLS.

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

## Primary hardware anchor

Nokia's NSE-5 System Module manual, Issue 1 (07/99), is available as a
[readable mirror preview](https://www.eserviceinfo.com/preview_html.php?fileid=5461&previewid=3023).
The locally acquired HTML is `roms/research/nse5/manuals/ch2sys-preview.html`;
it is a partial text rendition, not the complete PDF or legible schematics.

Page 2-7 identifies three roller interrupt inputs and an independent
slide-position interrupt. Its key inventory includes roller push, two soft
keys, Send/End, digits, star/hash and power. This supports a distinct physical
input profile; it does not establish matrix positions, roller phase ordering,
interrupt numbers or slide polarity. Those must be recovered before wiring
host input to the controller.

The block diagram identifies 512 KiB SRAM and 32-Mbit flash, consistent with
the current mapped capacities. The ASIC pin table identifies `GenDet` as a
slide input; the roller uses separate flex-pool pins. The register mux and
interrupt decode are still unknown. No 3310 Up/Down event or firmware-level
navigation message is an acceptable replacement for these physical inputs.

## UIF+ firmware contract

The stock image provides a three-phase input decoder, not a quadrature
Up/Down key substitute:

| Surface | Recovered contract |
| --- | --- |
| Phase sampler `0x473d14` | reads GPIO `0xf1` bit 1, `0xf2` bit 0 and `0xf3` bit 5 |
| Valid phases | `(1,0,0)` = 1; `(0,1,0)` = 2; `(0,0,1)` = 3 |
| Phase transition | `1 -> 3 -> 2 -> 1` produces `0x17`; reverse produces `0x18`; unchanged phase produces `0x5a` |
| State | previous phase at `0x168a30`; sampled bits at offsets 6..8; current phase at offset 9 |
| Dispatch `0x473f7a..0x473fa8` | `0x17` reaches `0x4cfb28`; `0x18` reaches `0x4cfb62` |
| Key-facing paths | handlers conditionally publish matching `0x17`/`0x18` values through `0x45c724` and `0x45c704` |
| Interrupt handling | caller masks IRQ7, samples GPIO, restores the mask and acknowledges status `0x80` at MAD2 `0x09` |

The slow sampler at `0x473a9c` actively drives/probes each GPIO pair and
collects six observations. Its classifier at `0x473de8` and restoration
routine at `0x473c80` must also be respected: returning a fixed one-hot
pattern at every GPIO read is not a complete electrical model. Invalid
patterns in the fast sampler retain the previous current-phase byte rather
than defining a fourth phase. Physical rotation direction, contact topology,
pulls and transition timing remain unvalidated.

The slow classifier's initialization record at `0x4ffbd0` copies 62 bytes
to `0x168a3c`, supplying eight records with eight-byte stride and six
significant observations per record. `tools/nse5_roller_contract.py` extracts
these from the hash-pinned flash. Observation order is B,C while driving A
low; A,C while driving B low; then A,B while driving C low. Here A/B/C are
respectively `0xf1` bit 1, `0xf2` bit 0 and `0xf3` bit 5.

| Record | Six sampled levels | Classifier effect |
| --- | --- | --- |
| 0 | 111010 | phase 1 |
| 1 | 111111 | retain previous phase |
| 2 | 101101 | phase 2 |
| 3 | 111111 | retain previous phase |
| 4 | 010111 | phase 3 |
| 5 | 111111 | retain previous phase |
| 6 | 000000 | retain previous phase 1/2/3; initial 0 -> 3 |
| 7 | 111111 | shadowed by record 1 |

The classifier selects the first exact match; no match also retains the
previous phase. Rows 0/2/4 override that history. This is a decoded sampling
contract, not a proven passive-contact schematic. In particular, the table
does not justify replacing all GPIO reads with a fixed phase pattern.

A passive closed-pair model reproduces all three distinct slow patterns:
phase 1 closes B-C, phase 2 closes A-C, and phase 3 closes A-B, with released
inputs high. Restoration drives the previous phase's isolated pin low. With
no rotation the fast sample is invalid and retains that phase; either new
closed pair produces the correct one-hot sample for the next phase. All
nine previous/current combinations are checked by
`tools/test_nse5_roller_contract.py`. This supports a contact-network model
without substituting firmware events. The restoration literals establish
`0xb1..0xb3` as the relevant release/drive controls: a set roller bit releases
that pin for sensing, a cleared bit drives its `0x31..0x33` latch. Phase 1
clears B1 bit 1 and latch 31 bit 1, then sets B2 bit 0 and B3 bit 5; phases
2/3 perform the corresponding permutation. The slow sampler follows the
same direction rule for each low-drive probe. The extractor checks all
seven restoration/sampler GPIO literals, preventing confusion between a
direction-bank write and an output-latch write. This does not establish
direction polarity for every UIF pin or the separate `0x70` bank.

The 7110 board now connects that network through UIF's generic input-sample
callback. A three-position wrapping `Navi Roller` host input selects the
mechanical closed pair, not a firmware key/event. Only the identified roller
bits are driven by this callback; other products retain the register-only
input behavior. Down/Up decrement/increment the contact position; these host
bindings do not establish physical clockwise/counterclockwise naming.
`make verify-7110-keypad-controller` now also exercises 18 low-drive probe
observations and all nine restored-drive previous/current combinations on
the compiled device through MMIO and physical input fields. Pull strength,
interrupt edge/mux behavior and firmware navigation remain unvalidated.
No IRQ7 source is synthesized by the position input yet.

Separate readers at `0x4741b2` and `0x4741ec` invert GPIO `0xf1` bit 7.
The latter stores the resulting logical state and selects two software
continuations. Nokia identifies a separate slide input, but the physical
open/closed polarity and pin-mux association must still be confirmed; do not
infer them from the inversion alone. The nearby control path at `0x4741c2`
also masks/acknowledges IRQ7. Its coexistence with roller handling requires
an aggregate input interrupt model, not two independently clearing sources.

No post-bootstrap application-input acceptance run is possible at the current
fail-closed frontier. Roller pin sampling has controller conformance coverage;
the slide has no physical input wiring yet, and neither has IRQ7 acceptance.

### Keypad matrix

The conventional keypad scanner at `0x474004` iterates five rows
(`0x4740aa` compares against 5), using row signal `0x28`, direction `0xa8`
and column input `0x2a`. It scans column bits 1..4 and returns `row * 5 +
column`; bit 0 is not an ordinary scanned key. Decoder `0x4cfc5c` selects a
25-byte normal map at `0x28dcd8` or a five-byte special map at `0x28dcf4`,
indexed by byte `0x168995`. The established map is index zero; additional
product map indices have not been validated.

| Row | Column 1 | Column 2 | Column 3 | Column 4 |
| --- | --- | --- | --- | --- |
| 0 | Send (`0x0e`) | left softkey (`0x19`) | right softkey (`0x1a`) | star (`0x0c`) |
| 1 | End (`0x0f`) | 0 (`0x0a`) | hash (`0x0b`) | 6 |
| 2 | 1 | roller push (`0x12`) | 4 | 7 |
| 3 | 2 | 5 | 8 | 9 |
| 4 | 3 | unused | unused | unused |

The separate special map is `5a 0d 5a 5a 5a`: column bit 1 is Power,
requiring mask `0x02`, not the inherited `0x04`. The key-code names shared
with independently mapped products identify digits, star/hash, call keys and
softkeys. The remaining fitted matrix input `0x12` is labeled roller push by
Nokia's physical key inventory; its application action remains unvalidated.
Rotation still requires the separate three-phase UIF+ contract above.

`make verify-7110-keypad-controller` pins the flash hash and both tables,
then tests all 17 fitted matrix inputs across all five row drives (85 scans)
and Power press/release. This is MMIO controller conformance, not a firmware
message injection or proof of post-bootstrap UI key acceptance. The normal
bootstrap gate separately protects the unchanged fail-closed DSP boundary.
