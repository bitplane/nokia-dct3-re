# ROM4 capture and independent reference methods

RF/XF observation boundaries, acquisition methods and vendor-tool setup controls. Acquisition or a working simulator is not independently validated handset RF or timing. [The loader contract](rom4_dsp_loader.md) owns native boot; [COBBA control](cobba_control_boundary.md) owns hardware interpretation.

## RF ownership and capture target

## Architectural XF diagnostic

Nokia's NSE-1 *Disassembly & Troubleshooting Instructions*, original 03/98,
pp. 11-12, identifies MAD2 D200 pin 91/test point J222 as DSPXF. Its
diagnostic diagrams distinguish initialization, network synchronization and
registration, with intermediate search/access stages. This is a potential
independent milestone signal, not a specification of RF sample packing.
Source: [Nokia manual, pp. 11-12](https://www.manualslib.com/manual/3283741/Nokia-Nse-1-Series.html?page=11).
The accessible text confirms the stage labels; exact waveform polarity and
edge-to-stage correspondence are not yet established from the figures.

TI SPRU131G table 4-2 identifies ST1 bit 13 as XF, set/cleared by SSBX/RSBX:
[TI CPU reference](https://e2echina.ti.com/cfs-file/__key/telligent-evolution-components-attachments/13-106-00-00-00-00-46-75/TMS320C54x-DSP-CPU-and-Peripherals-Reference-Set-Volume-1.pdf).
The acquired `nse1_rom4_dsp_program.bin` has seven candidate `f6bd/f7bd`
words. A fresh 30-second native run with
`tools/c54x_rom4_xf_observe.lua` observes `2603/2604` twice,
`260b/260c` once, `2613/2615` zero times and `309c` 6,461 times.
Successor fetches show XF low after `2604` near 0.134 and 2.091 seconds,
then high after `260c` near 2.114 seconds. The first `309c` changes XF from
low to high near 0.134 seconds; subsequent captured writes retain high.
The existing RF-boundary checker independently passes with 6,499 frames,
207,040 reads and the same three control pairs.

This is a bounded architectural observation. It is not an all-writer XF
census, a mapped handset-board pin, or proof of synchronization or
registration. Whole-register ST1 loads and other uploaded code may also
change XF. Matching the manual's figures to firmware-owned callers remains
necessary before assigning stage semantics.

The active caller contract is now explicit. Entry `2523` branches to
`25fe`, not to separately called helpers. `CMPM` tests the low B MMR at
`000b` for **equality** with 1, then 2: selector 1 sets and clears XF,
ending low at `2605`; selector 2 clears and sets XF, ending high at
`260d`. Other selectors reach `260e`, whose `BITF` tests ST1.XF and
selects the opposite state. That third arm is statically decoded, not
observed in this run.

Three live `2523` entries carry selectors `1,1,2`, with stack-top return
addresses `0df3,30c8,30eb`. Read-only program windows confirm actual
`CALL 2523` at `0df1,30c6,30e9` and the preceding immediate B selectors.
The first site's program words are zero in the acquired flat program image
but populated at runtime. A raw CALL/CALLD census finds ten candidate
`2523` call sites in that image and therefore does not close the uploaded
caller set. The two late callers sit in the `30xx` initialization/control
spine; their execution alone does not establish network synchronization.
`309c` independently sets XF during the periodic path, so observing XF high
cannot by itself establish registration. The native RF-boundary regression
remains unchanged under the extended observer.

The core now exposes `xf_out_cb()` as a latched architectural output, derived
from ST1 bit 13. Reset publishes high; instruction retirement and debugger
ST1 imports publish only changed levels; post-load forcibly re-drives the
restored level rather than preserving a stale external output. The output
cache is derived state, not a second saved flag. Eight executable status
variants check edge counts and rejection of unrelated-bit changes, and the
core fixture explicitly changes the debugger level then restores a saved
low output. `check-c54x-core` requires the XF-output success marker.
The 30-second native NSE-1 RF regression is unchanged. This callback does
not model OFF/HOLD high impedance, decode Nokia stage pulses or connect an
RF source; no board-level or acquisition claim follows from exporting it.

`tools/c54x_rom4_port_census.py` inventories candidate `PORTR`/`PORTW` sites
in the recovered big-endian ROM image. It accounts for the extra Smem address
word in absolute `74f8`/`75f8` instructions; treating that word as the port
would mislabel sites. The static counts are 16 reads and seven writes on
`0x27`, six reads and four writes on `0x38`, three reads and five writes on
`0x39`, and four writes each on `0x31`/`0x32`. These are opcode-pattern
candidate sites, not proof that every word is executed code.

The `0x38/0x39` cluster has a polling routine at `0x41ad`: it rereads
port `0x38` while bit 7 is set, then returns the low `0x60` status bits.
ROM functions reached from `0x5074/0x5078` and `0x7b0c/0x7b10` call nearby
parallel-interface routines; `0x7b0a` calls `0x410e` and `0x4185` in order.
The latter can read one port-`0x39` word at `0x41a4`. In the coherent
12-second run, port `0x27` is read 82,432 times, while ports `0x38` and
`0x39` are never read; both firmware mode words `0x00aa/0x00ac` remain zero.
`tools/c54x_rom4_port_census.py --call-target 0x7b0a` finds seven candidate
direct `CALLD` sites: `0x50b6`, `0x77a0`, `0x77af`, `0x7843`, `0x7ac2`,
`0x7ad9` and `0x7bc0`. The first sits behind conditional branches at
`0x50ac/0x50b0/0x50b4`; the other six are in the `0x77xx--0x7bxx` routine
cluster. These are two-word opcode matches in a mixed code/data ROM, not a
closed call graph or proof of execution.

A read-only 30-second no-signal watch of those seven sites and `0x7b0a`
recorded zero hits; a changed-write watch on DSP data words
`0x00aa/0x00ac/0x00b0/0x00b1` also recorded zero. The existing RF gate still
passed with 6,497 frame expiries and 207,232 port-`0x27` reads, but no
port-`0x38/0x39` reads or port-`0x32` writes. The temporary watch hooks were
removed. This excludes a missed live call into this cluster during that boot;
it does not identify the dormant MCU request, establish SCU register ownership,
or decode a port-`0x27` sample word. DSP I/O ports `0x38/0x39` must not be confused
with MCU MAD2 offsets `0x38/0x39`, which are SIMI registers in this driver.
The callers are guarded by a second, compact control block rather than one
isolated enable. Static decoding shows the `0x50b6` caller selects values
`0x0070`, `0x0080`, `0x00b0` or `0x00b1` from DSP data word `0x121f`; the
neighboring callers consume words in `0x18df..0x1973`. A read-only terminal
snapshot after the same 30-second run found `0x121f` and all 16 referenced
words in that range still zero. This excludes a final missed edge into an
otherwise initialized `0x7b0a` path: the operating-mode state consumed by the
whole caller family was never established. It also weakens the assumption that
this family is the ordinary cell-search entrance. Traffic-channel or dedicated-
channel activation is a plausible interpretation, not yet an established name.
`0x121f` is also the final word of the host search-list scratch copy described
below; its address alone does not establish persistent receiver-mode ownership.
A big-endian word scan of the recovered program finds 15 literal candidates:
`2733`, `3da7`, `4efa`, `5007`, `5013`, `502a`, `5040`, `504b`, `50a5`,
`50d8`, `5226`, `5299`, `52dd`, `5327`, and `53d6`. Two candidate absolute
stores are `80f8 121f` at `4ef9` (low accumulator A) and
`76f8 121f 00c0` at `5006` (immediate `00c0`). These are raw code/data
matches, not a closed writer census; indirect copies and overlays remain
outside this literal scan. The first follows a candidate direct call to
`23d0`, also used by neighboring control routines. The unresolved target is
the lifecycle entering these control routines and initializing the
`0x18xx/0x19xx` block, not another search for the already observed scratch
copy. Do not synthesize port-`0x38/0x39` readiness from these candidates.

`tools/c54x_rom4_mode_observe.lua` passively watches every write to `121f`,
`1835`, `07fb` and `1974`. Run it with `-autoboot_script` in the same isolated,
fresh-generated-EEPROM composition as `check-c54x-rom4-rf-boundary`.
Under the current absolute CTSI compare model, a 30-second run records 12
writes, all zero: startup clearing at PCs `0f10/0f15/3074`, the search-list
scratch tail at `3da3` (2.091269308 s), another write at `33e7`
(2.114326750 s), and clearing at `3074` (2.180219058 s). All four watched
words finish zero. The RF gate passes independently with 6,499 frame
expiries, 207,040 port-`27` reads, three port-`32` writes and no port-`38/39`
reads. This refreshes the earlier inactive-control observation without
assuming that the older relative-compare timing still applies. It does not
close the writer census or prove these routines are the cell-search path.
Preserved-NVRAM runs have a different RF sequence and must not be checked
against that gate's fresh-profile port-sequence expectation.

A separate fresh 30-second opcode-fetch watch records zero fetches at the
candidate stores `4ef9/5006`, their common predecessor `23d0`, and its branch
target `2626`. The positive control at `3da2` records 31 fetches of the
search-list copy instruction `e5ca`; this validates cached instruction-fetch
observation rather than treating an untested tap's silence as absence.
`23d0` contains `f073 2626` (a branch veneer); the recovered `2626` routine
contains guarded comparisons and branch tails, not a direct mode assignment.
These four candidate sites therefore do not execute in this no-cell boot.
They remain static candidates in other lifecycles, not missing peer responses
or proven ordinary cell-search initializers. The passive observer includes
the positive control and these candidate counts for reproduction.
The DROM function-list mechanism is an immediate call walker, not a persistent
frame schedule. Routine `0x9a56` calls `0x771c`; the recovered data ROM contains
`0x9a56` in 17 `0x00ff`-terminated lists between `0xedf7` and `0xeee6`.
Callers in `0xa280..0xa47f` use AR2 to select a branch, put the chosen list
address in AR5, then call `0x9903`. That routine reads a function address from
AR5, preserves AR5 on the stack, calls the function through `CALA A` at
`0x9905`, restores AR5 and repeats until the terminator. The separate
mode-dependent callback at DSP word `0x07fb` is called through another
`CALA A` at `0x5eef`. The ROM sets `0x07fb` to `0x98b7`, `0xa27d`,
`0xa2cf` or `0xa436` in the `0x55xx/0x57xx` control paths. The `0x57xx`
selection reads mode word `0x1835`.

Read-only probes in the 30-second no-cell boot saw no call to `0x9903`,
`0x9a56`, `0x771c`, `0x5eef`, `0x5514`, `0x5e62` or the list-selecting entry
points. Neither `0x07fb` nor `0x1835` changed; both ended at zero. The
type-`0x1a` handler's state was live (`0x0284=0x0001`, `0x0287=0x7fff`),
while `0x121f`, `0x1974` and the downstream mode block were zero. Thus this
control path is not active in the observed search lifecycle. Its first missing
boundary is the command or mode transition that sets `0x1835` and `0x07fb`;
the evidence does not yet establish that this path owns ordinary acquisition.
The temporary probes were removed.

Both indirect calls use `CALA A` (`f4e3`), which the C54x core previously
lacked. The core now implements `CALA[D]` for A and B, with focused return-
address and delay-slot tests. This is required for these list and callback
paths when firmware enters them, but it does not activate them by itself.
The opcode and return semantics follow the
[TI C54x CPU reference](https://www.ti.com/lit/ug/spru131g/spru131g.pdf).
Focused long-immediate load conformance also covers the dormant control
paths: `LD #lk[,SHFT],dst` now respects `SXM` rather than unconditionally
sign-extending its constant. The `LD #lk,16,B` encoding is `0xf162`, not the
previously accepted `0xf362`. Six executable cases exercise signed/unsigned
loads, the 15-bit shift, 40-bit guard extension, destination selection and
preservation of the other accumulator. The encoding and sign-extension
contract follow [TI SPRU172C, LD, pages 4-66--4-69](https://www.ti.com/lit/ug/spru172c/spru172c.pdf).
Core conformance, coherent NSE-1 execution and the 30-second RF boundary gate
pass; these arithmetic corrections do not activate acquisition or establish
the unmeasured sample interface. They are not a claim of complete instruction
timing or overflow-flag conformance.
`ABS` additionally clamps overflow to the signed 32-bit maximum under `OVM`,
sets carry for a zero result, and preserves sticky destination overflow.
Executable cases cover the TI guard-bit example `03 1234 5678` and a following
zero result; these are unary-operation checks, not general ALU conformance.
The saturation and sticky-flag rules follow SPRU131G section 4.2.2 and
SPRU172C's ABS description.
NEG uses the same signed-result overflow/saturation rules as subtraction and
retains its zero-only carry contract. Executable tests cover TI's 40-bit minimum
example with OVM off and on; the guard-bit overflow is no longer limited to the
previous special case at the signed-32-bit minimum.
Ordinary immediate, accumulator and memory ADD/SUB forms now share bit-32
carry/borrow, sticky destination overflow and signed-32-bit OVM saturation.
The shifted-16 memory ADD form preserves carry when no carry occurs. Boundary
tests cover positive/negative saturation and memory wraparound/borrow. SFTA's
right shift obeys SXM and publishes the outgoing bit as carry. This does not
yet establish multiply-accumulate, dual-16-bit mode or complete shift/load
overflow conformance; those require separate instruction-family tests.
Shifted accumulator, ASM, long-immediate and extended-memory LD forms share
SXM-aware shifting and sticky overflow/OVM handling while preserving carry.
The executable ASM test crosses the signed-32-bit limit without requiring a
40-bit wrap. Fixed-16 memory loads have separate SXM-on/off, pointer/timing,
and OVM-on fixtures: the fixed shift does not saturate or change overflow
status under OVM, unlike variable-SHIFT loads (SPRU172C, LD page 4-66).
Other multiplier and shift combinations remain separate audit surfaces.
The fractional multiply/MAC paths now double signed products with bounded
multiplication instead of left-shifting potentially negative C++ values.
Negative-product fixtures check memory MAC and dual-memory MPY results,
pointer/T effects, and one-cycle costs. A fixture-only `SQDST` case checks
negative `(Xmem - Ymem) << 16` and fractional-square accumulation; its signed
scaling is likewise defined C++ multiplication. These checks agree with TI
SPRU172C's arithmetic expressions and class-7 timing, but do not establish
all multiplier flag and rounding variants.
Fixture-only `MPYR`, `MACR`, and `MASR` Smem tests now check TI SPRU172C's
rounding rule (add `0x8000`, then clear the low word) for negative product,
accumulate-before-round, and negative subtract results. Each also checks
T/AR preservation, carry preservation, and one-cycle DARAM timing. This
does not establish every OVM, FRCT, or long-offset combination.
An `MPYU *AR2,A` fixture checks that FRCT doubles an **unsigned** 16-by-16
product without changing T or AR2, at the documented one-cycle DARAM cost.
SPRU172C explicitly lists FRCT as affecting MPYU; unsigned multiplication
must not bypass the fractional shift.
`DADD`/`DSUB` now implement double-precision and C16 dual-lane arithmetic.
Fixture-only `5193`/`518b` and `5493`/`548b` reproduce TI SPRU172C's
published add/subtract examples, including independent lane results, long
operand pointer steps, carry, and one-cycle DARAM timing. `50f8` checks
absolute-address extension and two-cycle cost with OVM saturation; `5083`
and `5483` check that low-lane carry/borrow does not enter the high lane and
that C16 mode does not saturate under OVM. None of these words appeared in the
captured ROM4 boot. Fixture-only `5283` and `5383` additionally check B as
the DADD source with A/B destinations, under both C16 modes. The same
low-word carry yields `0x50001` in double-precision mode and `0x40001` in
independent-lane mode; these are two opcode words across four state cases,
not four distinct encodings. `DRSUB` now shares the long-word decoder; fixture-only
`5893`/`588b` reproduce TI's reverse-subtract examples in both C16 modes,
including the cleared borrow flag and AR3 movement by two words. Fixture-only
`5a8b`/`5a93`, `5c8b`/`5c93`, and `5e8b`/`5e93` now check the
T-based long-word forms (`DADST`, `DSUBT`, `DSADT`) in both C16 modes against
TI's published result examples. `5bf8` checks absolute B-destination
addressing and its two-cycle cost. `5aea` checks long-offset preupdate,
extension consumption, and the same two-cycle cost. The published DSADT C16
example sets C despite an upper-lane subtraction borrow; the fixture does not
assert C for that case, so its carry behavior still needs independent evidence.
Fixture-only `5583`, `5983`, `5d83`, and `5f83` now check the B routes of
`DSUB`, `DRSUB`, `DSUBT`, and `DSADT`, each in double-precision and dual-lane
mode. The operands distinguish cross-lane carry/borrow from independent lanes;
all eight cases assert B, unchanged A/T, and one-cycle DARAM timing. Their
carry and overflow status outcomes remain outside this fixture batch. TI's
opcode table also exposed a pre-existing `SUBC Smem,B` decode error: its
encoding is `1f`, not `5e` (which belongs to DSADT). Fixture-only `1f83`
checks the corrected B destination, unchanged A, carry, and indirect timing.
The previously absent `SUBB` family now consumes the inverse of C as an
incoming borrow. Fixture-only `0e83` and `0f83` check TI's A/B examples,
including carry-out and one-cycle indirect timing.
Fixture-only `0183` checks ADD to B carrying across bit 32; `0b83` checks
SUBS to B using the unsigned memory word despite SXM. `1983` checks AND to B
zeroing the upper bits without changing carry, and `2183` checks a negative
signed MPY to B without changing T or carry. All four check one-cycle DARAM
timing and cover high-byte groups absent from the earlier fixture.
The table-driven fixture-only `2383`, `2583`, `2783`, `2983`, `2b83`,
`2c83`, `2d83`, `2e83`, and `2f83` cases check rounded/signed/unsigned
multiplication, square-to-B with T publication, and MAC/MAS accumulation or
rounding in both accumulators. Each checks its result, T, the untouched
accumulator, carry preservation, and one-cycle DARAM timing. They do not
cover FRCT/OVM combinations or all memory-addressing variants.
TI SPRU307A divides the dual-memory `0xb*` range into MAC/MACR (`b0..b7`)
and MAS/MASR (`b8..bf`). The core previously added the product for both
halves; it now subtracts for MAS. A 16-word table checks A/B source and
destination routing, rounded and unrounded results, T publication, the two
auxiliary-register paths, preserved carry, and one-cycle DARAM timing.
Only `b03a` was already in the fixture; none of the 15 added encodings was
observed in the current ROM4 idle trace. FRCT/OVM and other X/Y address
combinations remain outside this table.
TI SPRU307A likewise defines parallel `ST||MAC[R]` at `d0..d7` and
`ST||MAS[R]` at `d8..df`. The core previously decoded only the MAC half;
it now decodes the MAS half with a subtracted product. Sixteen fixture-only
words check the old source value stored to Ymem, destination A/B, rounded
and unrounded accumulation, untouched T, X/Y pointer updates, preserved
carry, and one-cycle DARAM timing. Other ASM, FRCT/OVM, and most aliased
X/Y addressing cases remain unaudited.
TI SPRU172C's published `ST A || MAS *AR5,B` and `ST A || MASR *AR5+,B`
examples now have exact-result fixtures. They check the negative 40-bit
accumulator outputs, ASM=5/1 stored words (`0222`/`0022`), rounding, both
pointer updates, and one-cycle cost. The second example adds one new fixture
word; the first reuses `d93a` from the routing table. Other ASM cases remain
separate checks. Fixture-only `d9b7` puts X and Y on the same AR5 with
opposing Xmod/Ymod. It checks the TI SPRU131G section 5.5.4 rule: X is read
before the parallel store overwrites that address, and only Xmod updates the
shared auxiliary register. This covers one alias case, not every dual-memory
form.
Additional `b83a` and `d93a` runs check FRCT product doubling and OVM
saturation after subtracting a negative product. They assert the appropriate
OVA/OVB flag, T and store side effects, and one-cycle DARAM cost. These are
status-mode variants of existing fixture words, not new opcode encodings;
other FRCT/OVM product and rounding boundaries remain unaudited.
Fixture-only `b43a` now checks the negative `MACR` half-word boundary from
TI SPRU172C: adding `0x8000` and clearing the low 16 bits rounds `-0x8000`
to zero, but `-0x8001` to `-0x10000`. Both cases assert T publication,
pointer update, preserved carry, and one-cycle DARAM timing. This does not
cover rounded FRCT overflow or other X/Y addressing combinations.
Fixture-only `3383`, `3583`, and `3783` check `MASA`, `MACA`, and `MACAR`
using A's high word as the multiplicand, including B accumulation, T
publication, rounding, and one-cycle DARAM timing. `3693` uses TI's `POLY`
example operands and results with simple `*AR3+` addressing, not the example's
circular `*AR3+%`: it checks the rounded A result, B's shifted memory word,
unchanged T, postincremented AR3, and one-cycle cost. Sixty-four additional
`3682` cases check old A-high=1 and T=1 with four old-B rounding boundaries,
FRCT/OVM/SXM clear/set and pre-existing OVA clear/set. They assert positive
rounding overflow and saturation, sticky OVA, preserved C/OVB/T, and the
independent signed/unsigned memory load into B. All pass without changing
the implementation, following TI SPRU172C POLY (4-126). These cases fix
SMUL=0 and C16=0; other addressing modes, negative saturation and physical
pipeline timing remain unestablished. Native FR encoding and decoding retain
their exact independent comparisons on this build.
Fixture-only `3083` checks `LD Smem,T`; `3983` and `3a83` check the
remaining square-add/subtract accumulator destinations. `3c83` through
`3f83` check all four `ADD Smem,16,src,dst` A/B routes, signed memory
extension under SXM, untouched source accumulators, carry preservation when
no carry is generated, and one-cycle DARAM timing. TI SPRU307A specifies the
`0011 11SD` routing; these fixtures do not cover OVM/FRCT combinations,
absolute operands, or every flag outcome.
TI SPRU172C specifies that `SUB Smem,16,src,dst` clears C only when the
subtraction borrows; a no-borrow result leaves C unchanged. The existing
`0x40..0x43` handler instead overwrote C on every result. Fixture-only
`4083` through `4383` now check all four A/B source/destination routes,
borrow and no-borrow carry behavior, preserved T and source accumulator,
and one-cycle DARAM timing. Other addressing and overflow variants remain
outside this check.
External-memory wait states also remain outside this fixture audit.
`SQUR Smem` now copies its source word to T as SPRU172C specifies; the prior
handler calculated the square but left T unchanged. Fixture-only `2682`
checks FRCT doubling, OVM saturation, sticky OVA, and one-cycle indirect
timing. Fixture-only `27f8` checks signed squaring into B, T publication,
absolute-address extension consumption, and the two-cycle cost. These tests
do not establish every square/rounding mode or external-memory wait state.
The accumulator-source `SQUR A,dst` forms are now implemented. Fixture-only
`f48d`/`f58d` check signed AH as the source, unchanged T, destination choice,
FRCT/OVM saturation, and one-cycle cost. Existing `SQURA`/`SQURS` handlers now
have fixture-only `3882`/`3bf8` checks for signed square addition/subtraction,
T publication, preserved carry, and indirect/absolute timing. Their encodings
were cross-checked against TI SPRU307A's opcode table: text extraction from
SPRU172C's bit diagrams misleadingly suggested a collision with XOR.
Indirect MMR addressing was a separate core defect: `LDM`/`STLM` and the
other seven-bit MMR operand forms previously read the opcode's low bits as a
fixed address even when the indirect bit was set. They now select the AR's
low seven bits, apply pre/post modification, and clear its upper nine bits
after the access, per SPRU131G section 5.6. Fixture-only `4882` and `8892`
check read and postincrement write at one cycle. `489a` instead rejects:
short MOD=3 is write-only, including in the MMR grammar. LDM, PSHM and
PSHD reject this source encoding before data, AR masking/modification or
stack effects; the CPU-only `source-domain` census owns these exclusions,
including `489a` in all sixteen base contexts. Its duplicate legacy example
is removed. The ordinary Smem long-offset preincrement form remains
read-capable, but long forms are separately excluded from MMR addressing
by section 5.6. Implicit-copy and read/modify/write forms are not decided
by these tests.
SFTL shifts only the low 32 bits, clears destination guard bits and sets carry
from the outgoing bit (or clears carry for shift zero), per SPRU172C page
4-158. Executable left/right/zero-shift cases distinguish this from SFTA's
40-bit arithmetic operation.
SFTA left-shift tests cover the guard-bit operand `80 AA00 1234`, shift +5,
with OVM off/on. SPRU172C page 4-156's example prints C=1, while page 4-155's
explicit rule `src(39-SHIFT)` selects bit 34, which is zero for this operand.
The implementation and fixture follow the explicit rule (C=0); this source
contradiction remains unresolved by silicon evidence and is not advertised as
hardware-validated carry behavior.
Control-flow conformance includes an IRQ raised on the first operand read of
`RPT #3; ADD`: the ISR observes all four results, and operand reads are one
cycle apart. A separate BD fixture raises IRQ inside the first delay slot;
both slots complete before service, and cycle stamps establish BD's documented
two-cycle cost (SPRU172C B, page 4-14). This corrects a previous four-cycle
charge. These fixtures do not claim full pipeline/wait-state timing coverage.
SPRU131G section 6.10.7 also protects the complete instruction following
`RSBX INTM` or `SSBX INTM`. The core retires this separate guard by
instructions, not program words, and saves/resets its pending state.
Three pending-interrupt fixtures distinguish a one-word load, a three-word
store and an interrupt-return sequence: the ISR's stacked PC proves the
following instruction completed before entry. These fixtures failed before
the guard was implemented. They do not establish general six-stage pipeline
drain timing or protect arbitrary writes to ST1.
Single-repeat endpoints retain every 16-bit PC value: `ffff` is not an
inactive-state sentinel. Twelve firmware-free `MVMD` cases cover ordinary,
`ffff` and wrapped `0000` endpoints, compact/long repeat counts, and IRQ/NMI
requested after the penultimate transfer. All three transfers and their
destination progression must finish before entry, with the post-body PC
stacked in the unchanged executing page (SPRU131G sections 6.7 and 6.10.7).
The previous 16-bit sentinel fails at body `fffd`, leaving the final word
unwritten and stacking `fffd` in `tmp/c54x-repeat-endpoint-red.log`.
The internal endpoint uses an out-of-range sentinel and remains save-state
registered; conformance requires `TMS320C54x repeat endpoints: PASS`.
All twelve cases pass in `tmp/c54x-repeat-endpoint-green.log`. Native RF
counts and save-state restoration retain their acceptance contracts in
`tmp/c54x-repeat-endpoint-regression.log`; the timer observer still shows
short-period starvation. This is repeat-state/protection evidence, not
physical pipeline latency or recorder completion.
Block-repeat activation follows architectural ST1.BRAF, not a separate
private latch (SPRU131G section 6.8). Clearing BRAF through `RSBX`, `STM`
or `POPM` cancels looping without consuming another BRC iteration; restoring
RSA/REA/BRC and setting BRAF resumes looping without a new `RPTB`.
Four executable cases require `TMS320C54x BRAF control: PASS`, checking body
counts, remaining BRC and stack balance. The old duplicated latch fails the
cancellation assertion in `tmp/c54x-braf-control-red.log`. BRAF and its loop
registers are already saved architectural state, so no parallel active flag
is retained. These are execution-state checks, not Decode-stage BRC decrement
timing or general interrupt-pipeline evidence.
Single-instruction repeat completion must also honor an active block boundary.
In particular, `RPT #0` executes its body once but does not cancel or bypass
the enclosing BRAF loop. Four executable fixtures place that body at REA,
with short/long counts zero and two and BRC three; they require respectively
four/twelve body address updates, exhausted BRC and cleared BRAF. The old
early return leaves AR1 one, BRC three and BRAF set in
`tmp/c54x-nested-repeat-red.log`; the corrected core passes all four cases in
`tmp/c54x-nested-repeat-green.log`. The runner requires
`TMS320C54x nested repeats: PASS`. This checks architectural execution counts,
not Decode-stage loop-counter timing or a physical pipeline implementation.
`tmp/c54x-nested-repeat-regression.log` preserves the exact native RF snapshot,
5110 save-state restoration and original MU4 storage/native-entry acceptance.
The timer observer still exposes short-period starvation; recorder finalization
and native speech are not established by these gates.
XC's two-word interrupt protection (SPRU172C page 4-199) must count the
retired body's words before repeat handling redirects PC. Otherwise an
ordinary one-word body ending a block appears to consume a large PC delta,
admitting IRQ/NMI before the second protected word. Four executable cases
cover one/two-word bodies and both sources: the short body requires ISR entry
after the next loop's first word; the long body exhausts protection immediately.
The old ordering stacks `0802` with observed A one for the short body in
`tmp/c54x-loop-xc-red.log`; corrected short cases stack `0803` with A nine in
`tmp/c54x-loop-xc-green.log`. The runner requires
`TMS320C54x loop XC protection: PASS`. This is word-guard accounting across
repeat redirection, not a general decoded-instruction frontend or the six-stage
interrupt pipeline.
`tmp/c54x-loop-xc-regression.log` preserves the exact native RF counts,
5110 save-state round trip and original MU4 storage/native entry. The separate
timer observation still exposes short-period starvation.
Operand-fetch conformance separates Smem address preparation from its read/write
so MVDK, MVDP, PORTW, PORTR, READA, MVKD, CMPM, BITF, POPD, WRITA and MVPD fetch their instruction extensions
before reading data, I/O or program operands. FIRS likewise fetches its coefficient
address before reading X/Y. Fifty-three passive bus-tap cases cover direct,
postincrement, long-offset and absolute forms plus FIRS, MVPD preincrement
and all eight MVPD absolute-address encodings; the runner requires
`TMS320C54x operand fetch order: PASS`. The original MVDK direct case reads
data before its destination extension and fails this check. The original CMPM
direct case independently fails before its immediate fetch. Comparison/test
cases also check TC and retain displacement-before-immediate encoding.
POPD's long-offset negative control observes the stack read before its address
extension; resolving the destination before `pop()` closes that ordering defect.
Its four forms also check a distinct destination sentinel, unchanged stack word,
single SP increment and selected pointer updates.
WRITA's four forms additionally check a distinct program destination and
unchanged accumulator A; they verify existing execution, without a timing change.
MVPD's preincrement negative control writes the old destination, failing the
distinct destination sentinel in `tmp/c54x-mvpd-preincrement-red.log`.
Shared address preparation now increments before the write and completion
does not increment a second time (SPRU131G table 5-4, MOD 3). The preceding
destination and program source remain unchanged. MOD 15's unused AR bits
use the same absolute address-before-pmad order for all eight encodings.
Instruction-family extension layouts and aggregate cycle charges remain unchanged;
MVPD's preincrement ordering is the separately evidenced correction above.
This enforces Fetch-before-Read ordering (SPRU131G section 7.1), not physical
Access-stage address-update timing, overlapping execution or interrupt drain.

The accepted isolated 30-second RF snapshot is 20,280 frame expiries,
648,032 RF reads and 1,537 port-32 writes. Independent queued-transfer
completion accounts for twenty native `3712` writes; four immediate
`a240` writes and 1,513 scan `4028` writes complete the inventory.
The multi-entry processing profile requires 1,024 mode-1 frames, twenty
early reductions, 1,004 steady reductions and two control continuations,
with zero RF/reduction inputs. See
[the boundary contract](cobba_control_boundary.md) for descriptor, selector,
operand and ordering evidence. Neither continuation establishes acquisition.
The checker also requires the
ordered programming sequence, frame-30 activation, 32-word read cadence,
firmware queue producers, IMR `035f` and zero port-38/39 reads. Two isolated
long transfer runs reproduce its complete inventory; independent fresh RF
and processing acceptance gates pass. Each generates this handset's own EEPROM
and fresh flash state. This is an emulator regression snapshot, not measured
RF timing or acquisition.

The earlier 20,285/648,192/1,520 snapshot depended on the card contract:
EF_SMSS (`6f43`) belongs to
DF_TELECOM (`7f10`), not DF_GSM (`7f20`). A reversible source control changing
only that parent to the incorrect `7f20` reproduces the previous exact
20,282/648,096/1,519 totals. Restoring the correct parent reproduces
+3 frames, +96 reads and +1 write against the old snapshot. Evidence:
`tmp/rf-smss-placement-control.log` and
`tmp/rf-smss-correct-placement-restored.log`. The correct placement is retained;
the checker is deliberately re-banked, not made advisory or widened to accept
both regimes. Its tests reject the old counts and changed programming sequences.

The first observed card divergence is SELECT `6f43` at `4.41506615` seconds:
the incorrect placement returns not-found, while the correct placement selects
the file and permits a two-byte READ BINARY before selecting `6f42`.
The first 23 MCU TX commits still match within 2 microseconds. Commit 24 carries
the same type-`1a`, 68-byte body (`001198` followed by zeroes), but moves from
`6.425337` to `6.418223` seconds. DSP work-flag publication follows by
2 microseconds in both traces; RF sequence 6 moves one 4.615-millisecond frame
earlier. This localizes the schedule change upstream of DSP dispatch.
No CPU timing, peer latency or firmware state is changed to match the oracle.
Separate RPT-zero and XC-word-accounting controls did not remove the drift;
their corrected instruction contracts retain their executable regressions.
Retained post-run NVRAM does not reconstruct the cold fixture.

The late MCU publication chain is observed through passive taps:

| Boundary | Observed caller / publication |
| --- | --- |
| Task-15 `09ec` | Constructor `21fd22`, caller `223010`; post `21fe24` |
| Task-16 `07d5` | Branch `208d40` receives `09ec`; caller `208d44`, post `206d3e` |
| Task-13 `05df` | Callers `24f388/24f392`, selectors 0/3; post `26c896` |
| Task-16 `05e8` | Constructor `237df8`, caller `238f86`; post `237f56` |
| Task-10 `03e9` | Receive returns `05e8` at `24f47c`; caller `24f49e`, post `26c6ce` |
| MCU search packet | Caller `211548` enters `20fc94`; branch `20fe38`, TX commit `27183e` |

`tools/c54x_rom4_scan_schedule.lua` is a capped, observation-only NSE-1 v5.30
instrument, not another handset acceptance gate. It reads CPU registers and
bounded ordinary-RAM task headers; it writes no firmware state. A shared
publication helper or register snapshot alone does not establish timer
ownership. These checks establish native ROM execution and the recorded
RF-port lifecycle, not physical RF acquisition, handset speech or end-to-end calls.

`make check-c54x-timer-registers` asserts the stopped TRB reload contract
before running the separate timing observations. Forty-eight firmware-free
instruction cases write PRD `1234`, reload with TSS set and each TDDR value,
then read TCR through `LDM`: reload alone, `ORM #0,TCR`, and an attempted
write of all PSC bits. TIM must remain `1234`, PSC must equal TDDR,
and TRB must read zero (SPRU131G table 8-14). Section 8.4.2 explicitly
identifies PSC as readable but not directly writable. TCR writes therefore
retain the counter rather than clearing it or accepting the supplied PSC;
TRB and timer expiry reload it from TDDR. Timer rearming includes the current
PSC, and IDLE2/3 reads use their existing saved timer phase. The correctly
initialized identity-write baseline fails divider one (`11` instead of `51`).
These assertions do not establish arbitrary live TDDR changes, general
stop/restart timing, interrupt latency or native recording completion.
Fifteen additional controller-only cases load PSC `1..15` with TIM `2`,
change TDDR to zero while stopped, then restart without TRB. After one CPU
clock TIM must still be `2` and PSC must be one less than its loaded value.
Each then checks the first borrow, subsequent one-clock decrement and PRD
reload/TINT at expiry. This follows section 8.4.2: the current read-only PSC
survives a TDDR change, with the new TDDR used on the next prescaler reload.
The core saves the initial TIM for that first interval, which can be longer
than later intervals; division by the new TDDR alone cannot recover it.
Remaining clocks are rounded upward so fractional attosecond conversion
does not advance the hardware counter early. Save replay overwrites a timer
reload, restores the captured current interval and repeats the same checks.
The baseline prematurely reports TIM `1` in the first one-clock case.
Fourteen additional controller cases write TIM after one elapsed CPU clock
without an intervening counter read. TIM writes synchronize the live PSC
before replacing TIM, preserving its current interval rather than rearming
from stale state. Sixty-four TCR cases check all reserved high nibbles and
Free/Soft storage combinations: bits 15..12 read zero, stopped PSC is retained,
and Free/Soft remain writable (table 8-14). These storage checks do not
establish debugger-dependent Free/Soft clock behavior.
The full CPU suite passes, and the native NSE-1 receiver regression retains
frame-driven port-27 reads with no port-38/39 reads. The exact current scan
snapshot is specified in the BRAF contract above. This preserves the existing
no-cell boundary, not radio acquisition.

`make probe-c54x-timer-boundary` runs a separate, firmware-free observation
profile (`tms54test -bios timer`). It reloads PRD through indirect stores,
then sets AR1 to IMR and masks TINT. The ISR consists only of `PSHM XPC`
and `FRETE`. With PRD 1 the atomic core repeatedly returns to `01:0608`,
before the mask, with IMR/IFR `0008`; the observation deadline may fall
inside the ISR with SP `0ffe` or `0fff`. With PRD 256
it reaches the marker after masking, then the branch loop at `01:060f`;
SP is again `1000`, IMR is zero and IFR retains pending TINT. No ROM,
NAND, serial peripheral or handset state participates in this reproduction.
The long-period control is checked, but the short-period result is printed
rather than asserted as correct: this probe is not a fidelity gate. It is
the small executable comparison for generic interrupt/write/drain modeling,
not proof that a timer period shorter than ISR service time must permit
foreground progress. Full pipeline behavior still requires an independently
derived timing oracle; changing PRD to make the handset proceed is not a fix.
The same profile now checks direct-address masking as well: replacing the
post-reload AR1 setup and indirect mask with `ANDM #fff7,*(0000)` leaves
the short-period marker clear, while the long-period control completes.
It reports four cases, with both long-period controls asserted and neither
short-period outcome treated as physical acceptance. This rules out the
extra pointer-setup instruction as the sole cause; it does not resolve
Decode-stage acceptance versus write visibility.
The passive probe distinguishes reload-opcode fetch, TCR bus write, IMR
mask bus write, first vector fetch and the first stacked return PC. CPU-owned
MMRs now take precedence over broad board RAM windows through a sparse
MAME data-space device map. CPU instructions, passive taps and debugger data
reads therefore use the same register state. Reserved gaps remain board-owned;
partial writes preserve unselected bits, with IFR's write-one-clear rule
handled separately. Snapshot fixture loading initializes writable RAM only,
not live CPU or peripheral registers. The core gate requires an executable
`TMS320C54x MMR bus: PASS` test of this boundary. No per-instruction diagnostic
callback is added to the core.

The timer observer requires one actual TCR reload write per case and one
IMR mask write in each long-period control. It reports short-period outcomes
without asserting them as physical acceptance. These are emulator-accounting
timestamps at observable bus accesses, not Fetch/Decode/Execute pipeline
phases or silicon latency. Retained vector prefetch precedes the stack write;
the continuation is captured from the latter's passive tap. Do not choose a
foreground delay from this trace or interpret bus observability as a pipeline
correction.
In `tmp/c54x-decode-timer-boundary.log`, opcode fetch and TCR bus write have
the same accounting timestamp; the first short-period vector fetch follows
five accounting cycles later with explicit INTR Decode before stack Access. Both
short cases still enter 108 times and stack `0608`,
with no mask write. Long-period indirect/direct cases mask four/two cycles
after the reload respectively and never enter the vector. This makes the
collapsed fetch/write boundary directly observable without changing PRD,
interrupt admission or firmware state; it does not settle physical acceptance.
The changed vector-fetch timestamp does not resolve the short-period stall.
`tmp/c54x-mmr-map-rf.log` independently retains the ROM4 no-cell boundary:
648,288 RF reads, 1,580 port-32 writes and no port-38/39 reads. The tool suite
passes 2,452 tests with one skip. None of these gates proves recorder save
completion, RF acquisition or handset speech.
SPRU131G section 7.2, example 7-18, constrains that correction more tightly:
recognition at the end of cycle 3 inserts INTR at Decode in cycle 4,
replacing `i2`; already-decoded `i1` completes at Execute in cycle 6.
Interrupt overhead covers cycles 7--9 before the first vector instruction
executes in cycle 10; the inserted INTR's actual stage placement is detailed
below. The saved continuation is `i2`, not a fixed number of later
foreground instructions. A blanket three-instruction deferral would therefore
implement the wrong contract. The former atomic `service_interrupt()` joined
recognition, IFR clearing, return-PC capture, stack update and vector transfer
at one retirement boundary and charged five cycles. The current entry splits
the bus operations and defers IFR clearance to vector fetch; foreground
recognition still occurs at an instruction boundary. The five-cycle aggregate
is not a full pipeline model; the manual's three-cycle INTR overhead alone does not justify
replacing it without accounting for recognition and drain separately.
The next executable timing fixture must distinguish request latching,
Decode-stage acceptance, memory-write visibility and vector entry. Timer
requests accepted before a later IMR write must not simply be cancelled by
that write. The existing short/long-period probe does not resolve this order.
SPRU131G section 6.10.4 ties IACK and IFR clearance to the first vector-word
fetch. The rendered example 7-18 must be read alongside that prose: its
vector fetch occurs in cycle 6, overlapping `i1` Execute, before the inserted
INTR's stack write in cycle 7. Therefore moving IFR clearance to the core's
former post-stack `fetch()` would implement the wrong bus order. The core
now prefetches and retains the first hardware-vector opcode before the
stack write, calling MAME's IRQ acknowledgement callback once for the accepted
IRQ/NMI source. INTR Access decrements SP before that fetch; only the return
word's memory write follows it. A combined post-fetch `push()` would preserve
memory-write order but incorrectly leave the old SP visible at Fetch/IACK.
The pointer update and memory write are therefore separate operations.
Extension words are fetched normally. This is a bus-order
correction, not a Decode-stage recognition/drain model. The figure places
INTR Access/Read/Execute in cycles 5/6/7; the prose's cycles 7--9 describe the
interrupt overhead before first ISR execution in cycle 10, and must not be
treated as three later foreground executions. A stage-aware fixture must
observe acceptance, foreground mask write, vector prefetch, acknowledgement,
stack write and ISR execution separately. Preserve the accepted source and
continuation across later IMR changes. Neither the atomic core's fetch tap
nor its unchanged aggregate five-cycle charge measures these physical
boundaries.
An executable IRQ/NMI fixture observes decremented SP with unchanged stack
memory at vector fetch and acknowledgement, then one continuation write
without a second pointer decrement. It checks selective IFR clearance and
one source acknowledgement. It changes the
vector opcode after that read and checks execution of the retained word,
normal extension fetch, two save/replays, survival of later IMR masking,
reset discard and explicit debugger-PC redirect discard. The combined-push
core fails the Access-before-Fetch assertion in `tmp/c54x-intr-access-red.log`.
The core gate also replays five modeled entry boundaries for both IRQ and NMI
(ten independent save/load cases): acceptance, Decode, stack Access, vector
Fetch/IACK and stack write. Each continuation requires exactly one vector
read, acknowledgement and original-continuation write, including when IMR
changes after acceptance. Post-Fetch cases replace vector memory and still
execute the retained opcode with its normally fetched extension. These are
entry-state ownership tests, not foreground-pipeline drain or silicon-cycle
validation; the instruction-atomic timing frontier remains unchanged.
The complete core gate requires `TMS320C54x vector prefetch: PASS` with
`sp_access=1`. Core, timer observation, original MU4 storage/loader/serial and
native NSE-1 RF gates pass in `tmp/c54x-intr-access-final.log`; the RF boundary
retains 648,288 reads and 1,580 port-32 writes, with no port-38/39 reads.
The tool suite passes 2,452 tests with one skip. These gates do not include
recorder save completion or handset speech. The separate recorder-control
and undelivered-metadata observations remain documented in `5510_bringup.md`.
Vector bus order is corrected; the recognition/drain timing question is open.
An independent return-bus assertion covers `RETF` (`f49b`): SPRU172C
section 4-141 and SPRU131G example 7-11 specify PC from RTN and an SP
increment, without reading the stacked PC. A passive tap in the existing
hardware-interrupt fixture detects the former discarded `pop()` read.
The implementation now increments SP directly; return state and the
three-cycle instruction cost are unchanged. This does not establish
interrupt recognition/drain timing or native recorder completion.
The delayed form `RETFD` (`f69b`) uses the same RTN/SP contract and the
existing two-word delayed-transfer machinery. The fixture covers two
one-word loads and one two-word load, checks their destination accumulator,
the returned continuation, balanced SP and cleared INTM, and requires zero
stack reads in all three return cases. Disassembler fixtures cover both
forms. The specified delayed instruction cost is one cycle; these state/bus
assertions do not independently validate pipeline stage timing.
The full CPU suite and native NSE-1 receiver gate pass. The original MU4
recorder replay retains cursor `27`, pending RX and PC `02:3d76`; eliminating
the discarded stack read does not advance that independent boundary.
The existing `RPT #3`/one-cycle `ADD` interrupt fixture also reports cycle
markers without treating them as hardware acceptance. Four operand reads span
three cycles; the final body operand read to the first vector operand read
spans six cycles in the current atomic core. This separates the protected
repeat interval from its five-cycle aggregate vector-entry charge. Neither
marker is a Decode-stage observation, so this measurement cannot justify a
pipeline-drain delay or reinterpret the TI example's three-cycle overhead.
TI's [SPRU598B simulator overview](https://www.ti.com/lit/pdf/spru598),
sections 1.1 and 2.3, documents timer-capable C54x configurations and a
pipelined execution mode. This is a potential software-only differential
oracle for the synthetic probe, not a simulator run already performed.
Pipeline-flush-on-halt mode changes observation and profiling behavior;
an eventual comparison must identify the selected device and execution mode.
TI's [C54x simulator support clarification](https://e2e.ti.com/support/tools/code-composer-studio-group/ccs/f/code-composer-studio-forum/249280/how-to-use-ccs-v5-1-for-tms320c54xx)
places that simulator in CCS 3.x or earlier, not CCS 5.x. The referenced
CCS 3.3.83.20 TI archive paths returned HTTP 404, but the independent mirror
acquisition below now supplies the C54x driver and legacy automation package.
No simulator execution or timing comparison has yet been performed. The
acquired SR12 update described below is not itself that engine.

## Acquired CCS 3.3 C54x simulator package

The [FreeCalypso CCS archive](https://www.freecalypso.org/pub/GSM/TI_tools/CCS/CCS_3.3.83.20_win32.zip)
returned HTTP 200 and supplied a complete 677,575,677-byte ZIP. The ignored
collection retains `roms/research/ti-ccs/CCS_3.3.83.20_win32.zip`, SHA-256
`77e20d9daf28ffe188d4430d06667e49b79f9698dc77b088b51096f2b401715f`.
This is a third-party mirror of a vendor package, not an authenticated TI
download or a silicon timing capture.

Static cabinet inspection establishes these available components:

- `Drivers_C5400_SIM.CAB`: `TISIM54X.DVR` (528,384 bytes), C541 through
  C5416 target configurations and corresponding GEL initialization files.
  Driver strings identify simulator version 4.6.5 and pipeline information
  support; configuration descriptions include Timer0 on C549.
- `CCS_Scripting.CAB`: legacy server/COM/Perl interfaces, a C server header
  and register/memory/GEL examples. These are not modern DSS bindings.
- `Tools_C54x_sim_analysis.CAB`: the accompanying analysis component.

Only static extraction/inspection has been performed; no installer, driver
or scripting binary had been executed at acquisition. Subsequent isolated
setup attempts are described below. The PE import table includes
`CCReg.dll`, MFC42 and Windows runtime dependencies. Availability is not
proof that this package runs under the current host or licensed setup.

The next comparison must first establish a runnable C54x target and explicitly
select pipelined rather than flush-on-halt execution. Then run the existing
firmware-free short/long PRD and direct/indirect IMR-mask fixtures unchanged,
recording cycle markers, IMR/IFR, SP and return continuation. Identify the
target, package hash and execution mode in the result. Do not change Nokia
timing, cancel accepted requests or add a foreground-instruction delay from
archive availability alone. Native recorder completion remains unresolved.

## Standalone vendor assembler operand control

The acquired `CodeGeneration_C5400.CAB` supplies an independently runnable
C54x assembler/compiler driver, version 4.2.0. This does not resolve the CCS
simulator automation boundary. Extract `ASM500.EXE*` and `CL500.EXE*` from
that cabinet and retain copies named `bin/asm500.exe` and `bin/cl500.exe` in
a disposable work directory. `c54x_vendor_operand_check.py` pins both exact
executable hashes and the separately authored assembly source.

Copy `fixtures/c54x_vendor_operands/operand_probe.asm` and `probe.cmd` into
the work directory. Using the isolated runner below, initialize a fresh
WoW64 prefix with `wineboot -i`, setting `WINEPREFIX=/mnt/work/wow64` and
`WINEDLLOVERRIDES=mscoree,mshtml,winemenubuilder.exe=` in the command's
environment. Then invoke `wine cmd /c probe.cmd` with those same settings.
The command file sets Windows TMP/TEMP explicitly to writable scratch;
on the current Wine host, Unix TMP/TEMP appear only as `WINE_HOST_*` and
are not usable vendor scratch settings. All persistent files remain inside
the disk-backed work directory; no vendor installer is needed.

The reviewed listing emits 13 words for eight instructions. It gives
write-only warnings for `LD *+AR2,A` and all four short-preincrement
ANDM/ORM/XORM/ADDM controls. Ordinary postincrement LD, long-offset
preincrement LD and short-preincrement STL are unflagged. The assembler
still emits the warned encodings and exits zero; warnings must therefore
be checked, not mistaken for successful admission. This agrees with
SPRU131G table 5-4's restriction and disambiguates the RMW source role for
our fail-closed core. It is not a measured silicon trap or timing result.

Check `operand_probe.lst` using `tools/c54x_vendor_operand_check.py`, supplying
`--source`, `--assembler` and `--driver` paths. It verifies version, exact
word/address sequence, warning identities/count and assembly completion,
and explicitly leaves DSP execution and silicon-trap validation false.
Accepted artifacts are under `tmp/c54x-vendor-operand-probe/`. The separate
[execution rejection matrix](c54x_execution_contracts.md#operand-domain-acceptance)
now verifies the corresponding early core guard and both frontend/save-state
paths. The assembler probe itself remains a toolchain observation, not
execution, timing or silicon-trap evidence.

The post-operand-guard default regression is independently complete in
`tmp/run_c54x_core_hwymx7ti`: all 293 required markers pass, the fixture
returns its intentional completion status 3, and the runner reports native
conformance success. This covers the default instruction-model regression
after the RMW guard, in addition to the focused rejection matrices above.
It does not replace the separate optional profiles, provide an exact-opcode
execution inventory, or establish fitted silicon, RF acquisition or speech.
Large replay matrices make this a long-running gate:

```sh
.venv/bin/python tools/run_c54x_core.py --mame mame/mame \
  --rompath mame/roms --output-root tmp
```

## Isolated simulator setup boundary

`tools/run_ccs33_isolated.sh WORK COMMAND ...` supplies a disposable network-
and process-isolated namespace around Wine/Xvfb. Persistent writes are limited
to `WORK`; host `/data`, `/home` and `/root` are hidden. This optional vendor-
tool runner is not a MAME acceptance gate. It requires `bwrap`, `timeout`,
`xvfb-run` and Wine; prepare `WORK/home` and `WORK/prefix` first.
The runner creates `WORK/tmp` and binds it as the isolated `/tmp`; choose
a disk-backed `WORK` so vendor unpacking and Xvfb scratch do not consume tmpfs.
The default timeout is 120 seconds; `CCS33_TIMEOUT_SECONDS` accepts an
integer from 1 to 1800 for vendor installation steps. Normal command exit
also stops and waits for Wine's server so registry writes can finish before
the namespace disappears. This does not make an interrupted installation
successful.
The runner disables GLX: the host NVIDIA/EGL loader crashes Xvfb in this
namespace otherwise. Harmless `/bin/true` and `wine --version` controls both
exit zero with that setting. A child process printing output is insufficient
display evidence; the stock Xvfb wrapper can run it after exhausting failed
server starts and then report a cleanup error.

Administrative MSI unpacking populated
`run_ccs33_oracle/prefix/drive_c/CCS33Admin/CCStudio_v3.1`, including the IDE,
simulator driver and scripting DLL. The directory label is the MSI's layout,
not evidence of a different package version. Full installation stopped at
`TIBuildInstallDirRegKey` before completing installation. Inspection shows
that action constructs a registry path from `INSTALLDIR` and commits the MSI
database; it is not a target program or simulator execution. Registering
Wine's supplied `vbscript.dll` and `msi.dll`, together with the standard
`.vbs`/`VBSFile` script-engine association, lets the original installer pass
that action. A read-only query of the original MSI confirms its Component
Manager `Target` registry value comes from `[CommonFilesFolder]Texas Instruments`.
No substitute component database or modified installer is used.

The longer installation attempt reaches the bundled DSP/BIOS installer,
but its silent mode displays a hidden **Error in startup script** dialog:
`InstallProgramFolderShortcut` fails with Windows error `-2147221164`
(class not registered). Wine separately reports the missing Shell Link COM
class `{00021401-0000-0000-c000-000000000046}`. Acknowledging the dialog makes
`BIOS_Install_CA` return zero and the parent installation roll back; the
runner exits 91. This is a Windows setup dependency, not a timer result or
evidence that a longer wait would succeed. Retain
`run_ccs33_oracle/full-install-long.log`, `bios-installer-error.png` and
`run_ccs33_oracle_full_long.log` for this boundary.

Registering Wine's supplied `shell32.dll` resolves that prerequisite; a
registry query verifies the Shell Link `InprocServer32` points to Wine's
`C:\windows\system32\shell32.dll`. The unchanged installer then passes
DSP/BIOS installation. Its XDS560 physical-debugger driver reports a DevCon
installation error under Wine; acknowledging that error lets setup reach
plugin registration, not proven target initialization.

The Component Manager path and installation boundary are now resolved.
The earlier `SETUPinstallPlugins` attempt used literal `%SystemDrive%` in
its registry path and was explicitly stopped, not completed or timed out.
Expanding the isolated prefix's `ProgramFilesDir` to its existing
`C:\Program Files (x86)` value lets the unchanged MSI exit zero and write
the correct Component Manager target. This is setup success, not simulator
success: its INI remains empty until the original plugin registrar runs.

The registrar needs Wine's legacy `msxml.dll` registration for COM class
`{2933BF90-7B36-11D2-B20E-00C04F983E60}`; registering `msxml3.dll` alone
does not supply that class. With the legacy class registered, the original
registrar writes a 41,745-byte INI containing C54x analysis plugins, but
exits 6 with an XML parse error. The installed IDE then displays
**A problem has been detected in the component manager database**. Its log
reported missing Wine Mono. Installing the official Wine Mono 10.4.1 MSI
does not clear the registrar's XML failure. Its SHA-256 is
`071f4b2887e1c97a11d791ff3d65be9429eed6dec4c2708888bfd546ba358e23`,
matching the value pinned in
[Wine 11.0's addon source](https://raw.githubusercontent.com/wine-mirror/wine/wine-11.0/dlls/appwiz.cpl/addons.c).

The IDE also reports RPC service startup failure; a registry query confirms
RpcSs is absent. Wine's standard `wineboot -u` restores that service, but
reveals a registration-view mismatch: the original Component Manager target
exists in the 64-bit view and is absent with `/reg:32`. The 32-bit registrar
then reports an empty path. Running the original MSI through
`C:\windows\syswow64\msiexec.exe` completes with exit zero, including
`SETUPinstallPlugins` return value 1. The `/reg:32` query now returns
`C:\Program Files (x86)\Common Files\Texas Instruments`; the original
installer generates a 43,030-byte component INI there. The earlier database
and empty-path warnings are superseded by this corrected installation, not
evidence of a permanent simulator incompatibility.

The current boundary is target automation initialization. Registering the
original `CCS_Scripting_Com.dll` with 32-bit `regsvr32.exe` makes its public
`CCS_Scripting_Com.CCS_Scripting` object respond with **CCStudio Scripting
Version 1.52**. Its documented `CCSOpen(0x54, 0, 0, 1, 0)` call then fails
with a CreateInstance automation error `0x80040154` rather than returning
a board/CPU identity. Wine also reports an unsupported interface on
`CodeComposerSetup.SystemSetup` (class `{3620D2F0-F091-11D1-8BE4-0020182ABDCA}`);
the 32-bit class registration exists and points to the installed
`cc_setup.exe`. This does not establish that this class alone causes the
failure. No target instruction, timer fixture or pipeline mode has executed.

A COM activation trace identifies the requested application class as
`CodeComposer.Application` (`{DE4FA3E7-B47F-11D1-B009-0040055C1953}`),
registered in the 32-bit view to the installed `cc_app.exe`. Thus
`0x80040154` is not proof that its registry key is absent. Observing the
application during the actual call captures **Error reading Code Composer
Setup information**, not a usable target. The documented alternative
`CCSConfigClear`/`CCSConfigImport` sequence fails already at `CCSConfigClear`
with the same unsupported SystemSetup interface; it never reaches import of
the package's `drivers/import/c549sim.ccs`. This rules out an unnoticed
drag/drop dialog as a sufficient explanation for that attempt. The remaining
dependency is application-side setup activation, not a changed DSP fixture.
Direct `CreateObject("CodeComposerSetup.SystemSetup")` succeeds and reports
`ISystemSetup`. Its documented `Clear`, `Import` of the supplied
`c549sim.ccs`, and `SaveConfig` methods each return zero. Thus setup is not
universally unavailable, and the scripting convenience wrapper's failure
does not prove a missing board configuration. Opening the application after
that saved configuration still fails. The setup server's own `/RegServer`
entry point also exits zero without resolving application initialization.

The failed interface request is `IOleObject`, IID
`{00000112-0000-0000-c000-000000000046}`, not the SystemSetup class ID or
its custom setup interface (see the
[Microsoft SDK definition](https://raw.githubusercontent.com/microsoft/win32metadata/main/generation/WinSDK/RecompiledIdlHeaders/um/oleidl.h)).
The COM trace records `0x80004002` at that request before the scripting
wrapper reports `0x80040154`.

Reversible controls in a disposable copy of the retained prefix explicitly
removed `InprocHandler32` first in `/reg:32` alone, then in both registry
views. Queries verified each removal; both controls reproduced the baseline
failure and restored the registrations. Installed and administrative copies
of `cc_setup.exe` and `ccreg.dll` are byte-identical. A fresh direct setup
control still returns zero for `Clear`, `Import` and `SaveConfig` after
restoration. The handler-view hypothesis is exhausted; do not repeat it.
No vendor binary or generated board database was patched. A different
compatibility environment or a working direct application API is required
before this route can provide timing evidence.

### Direct driver loading boundary

`tools/ccs33_driver_load_probe.c` loads `tisim54x.dvr` without COM and checks
eight GTI exports, including initialization, simulator mode, pipeline
inspection and stepping. The installed driver loads successfully under the
isolated Wine runner; a nonexistent-driver control exits 1. Evidence is in
`tmp/ccs33-driver-load.log` and `tmp/ccs33-driver-load-negative.log`.
This establishes DLL loading only: no GTI function is called, no target is
initialized and no instruction executes. The SDK header search did not find
the direct GTI initialization ABI. Recover a documented initialization and
configuration contract before attempting execution; exported names alone
do not establish function signatures or pipeline behavior.

Static export-body audit further limits this route. For `tisim54x.dvr`
SHA-256 `2d98a6d9729c4ebd5534d8e1d46ec92444102ce7067a03da530470d476e9c9f0`,
`GTI_GET_PIPELINE_INFO` (RVA `0x3dc0`) selects a context, clears 30 DWORDs
at output offsets `0x100..0x177`, and returns zero; it does not obtain stage
state. `GTI_GET_PHASE_NAME` (`0x3d90`) bounds its index against a helper
that unconditionally returns zero, so it always returns null. Neither
export supplies pipeline inspection in this binary. The mode getter/setter
(`0x3e40`/`0x3df0`) instead dispatch simulator operations `0x44`/`0x45`;
their existence keeps execution-mode testing a candidate, not a verified
timing oracle. Initialization uses context/configuration data and pointer
arguments whose complete contract is still unestablished. Retain the
disassemblies in `tmp/ccs33-pipeline-export-audit.log` and
`tmp/ccs33-init-export-audit.log`; do not infer callable prototypes from
export names or replace missing configuration with fabricated structures.

Stage the source in `WORK` and compile with the installed Win32 compiler:

```sh
sh tools/run_ccs33_isolated.sh WORK wine \
  'C:\CCS33\bin\utilities\sim\gcc\bin\gcc.exe' \
  -Wall -Wextra -o driver_load_probe.exe driver_load_probe.c
sh tools/run_ccs33_isolated.sh WORK wine driver_load_probe.exe \
  'C:\CCS33\drivers\tisim54x.dvr'
```

`tools/ccs33_activation_probe.vbs` is the fail-closed activation control:
scripting version alone is insufficient; successful opening and nonempty
board/CPU identities are required. Even its success would establish target
identity only, not instruction execution or pipeline timing. Stage it in a
disposable work directory and invoke it through
`sh tools/run_ccs33_isolated.sh`, never directly against the host prefix.
The explicit-view controls are retained in
`tmp/ccs33-interface-{baseline,without32,withoutboth}.log`; the fresh setup
control is `tmp/ccs33-interface-setup-control.log`.

Retain `full-install-expanded-path.log`, `installed-ide.log`,
`installed-ide-screen.png`, `run_ccs33_oracle_plugin_msxml_legacy.log`,
and the earlier `full-install-shell-registered.log`, `xds-install-error.png`
and `plugin-install-error.png`. Additional evidence is `mono-install.log`,
`run_ccs33_oracle_plugin_with_mono.log`,
`run_ccs33_oracle_wineboot_repair.log` and `plugin-prefix-repair-error.png`.
Current verification evidence is `full-install-wow32.log` and
`run_ccs33_oracle_automation_registered.log`,
`run_archive/cleanup-20261009/root/run_ccs33_oracle_automation_com_trace.log.zst`,
`automation-observed-screen.png` and `run_ccs33_oracle_automation_import.log`.
The direct-API results are in `run_ccs33_oracle_system_setup_direct.log`,
`run_ccs33_oracle_system_setup_import.log` and
`run_ccs33_oracle_automation_direct_config.log`; the rejected handler trial
is `run_ccs33_oracle_automation_without_handler.log` with registration backup
`system-setup-original.reg`.
The verbose COM trace is also retained uncompressed. Resolve the remaining COM
initialization boundary using the vendor tools; do not edit the generated
component database or interpret copied files as a working simulator.

The acquired vendor SDK's `CCDspUser.h` defines `GetSimMode()` and, under
`USE_TI_INTERNAL_APIs`, `SetSimMode(long)`: mode 0 flushes the pipeline as an
emulator, mode 1 does not. Its comments explicitly restrict support to
certain simulators and require querying the mode to verify it. These are
automation leads only; neither mode has been selected or measured here.

The package-owned MFC42/Xerces directories in `WINEPATH`, a working software
display and acknowledgement of the drag/drop registration warning allow
`cc_setup.exe` to reach its real factory-board UI. The C54xx list contains
`C549 Device Simulator`; adding it displays the Texas Instruments C54x
simulator CPU with `drivers/tisim54x.dvr`. Save-and-launch starts `cc_app.exe`
and `ccsmonitor.exe`. The missing Component Manager path was resolved by
the correct 32-bit installation above. No simulator initialization, pipeline-mode
selection or timer result is proved by these UI observations.

Retained setup evidence is `run_ccs33_oracle/admin-install.log`,
`full-install.log`, `setup-observed.log`, `setup-windows.log` and
`setup-screen.png`, plus `run_ccs33_oracle_setup_display_fixed.log` and
`run_ccs33_oracle_launch_c549.log`. The next dependency is functioning legacy
installation/component registration and automation, not a changed Nokia
fixture or an invented interrupt delay. The Wine log also reports missing
Mono for a launched runtime component; it does not prove Mono alone prevents
the C54x driver from running.
TI's [C5400 scripting support answer](https://e2e.ti.com/support/tools/code-composer-studio-group/ccs/f/code-composer-studio-forum/1026307/code-composer-c5400-debugging-with-dss-api)
also rules out a modern DSS-based shortcut: C54x simulation ends at CCS
3.3, whereas DSS begins at CCS 4. TI explicitly states that no version
supports both. An acquired legacy simulator therefore needs its original
automation interface, not a modern target-configuration transplant.
TI's [C54x design workshop](https://e2echina.ti.com/cfs-file/__key/telligent-evolution-components-attachments/00-120-00-00-00-00-18-64/TI-TMS320C54x-DSP-_BE8BA18B0763FC5B_.pdf)
documents GEL-based simulator initialization as a legacy automation lead;
it is not an acquired engine or an executed differential test. The separate
[CODECOMPOSER product](https://www.ti.com/tool/CODECOMPOSER) page concerns
older C2x/C5x/C3x/C4x products and offers only a C3x/C4x service pack,
not a replacement C54x simulator download.
Two publicly accessible alternatives do not cover this boundary:
[gDSPsim 0.30](https://sourceforge.net/projects/gdspsim/files/gdspsim/gdspsim-0.30/)
has a staged C54x pipeline and software INTR implementation, but the inspected
C54x/common source has only register-display uses of IMR/IFR and no hardware
timer or maskable-request delivery implementation. Its source RPM SHA-256 is
`a57eda4aa6402aba0439146ddcc60ff8665ed1d116b82d4c2158e1d211b1bb7b`;
the supplied GUI build requires unavailable legacy `gnome`/`gtk+`
dependencies. The repository-owned adapter in `tools/gdspsim_reference/`
instead compiles the original execution sources unchanged with modern GLib,
opaque GTK widget types and no-op display callbacks. It preserves the
original memory preference defaults and aborts on file-I/O execution.
No external execution code is imported into the MAME core.

Reproduce the bounded comparison with:

```sh
.venv/bin/python tools/gdspsim_reference/build.py \
  /path/to/gdspsim-0.30 run_gdspsim_reference
```

The NOP fixture advances PC from `0100` to `0120` in 32 pipeline steps,
leaving ST1 `2900`. The XF fixture places `f6bd` at `0100` and `f7bd` at
`0108`; ST1 becomes `0900` at step 6 and `2900` at step 14. These status
results agree with TI's bit-13 contract and the native core's XF fixtures.
The five-step publication delay describes this simulator's pipeline, not
a Nokia device latency or independently established MAME cycle correction.
The runner rejects the reference's unknown-opcode-to-NOP fallback and
retains each fixture's output in the isolated build directory. Only these
NOP and XF fixtures agree with the architectural expectations; neither
establishes general arithmetic or hardware IRQ timing. A separate `INTR 16` probe executes
`f7d0` at `0100`, with PMST `2000`, SP `1000`, IMR zero and IFR `ffff`.
The original pipeline sets INTM and reaches the `2040` vector, but stacks
`0102` instead of the next-instruction address `0101`, and leaves IFR
`ffff` instead of clearing the selected bit to `fffe`. These are measured
reference discrepancies, not desired native-core behavior. The runner
explicitly reproduces and reports them; its PASS means reproduction, not
INTR conformance. The native core's existing 64 INTR cases check next-PC
stacking and selected-IFR clearing against SPRU172C. This reference cannot
substitute for the RTN contract either: SPRU131G example 7-6 explicitly
loads RTN during software INTR. Each native case now continues through
RETF after replacing the stacked word with a sentinel, proving the saved
RTN supplies the original next PC while SP balances and INTM clears.
Ordinary near `CALL pmad` also publishes RTN, per SPRU131G section 7.1.2,
example 7-4. The executable page-2 case calls a paused callee, checks the
stacked continuation, replaces it with `dead`, and executes `RETF`; return
must use RTN while balancing SP and preserving XPC. It failed on the former
stack-only CALL implementation and passes with RTN assigned before the push
(`run_c54x_call_rtn_red`, `run_c54x_call_rtn_green`). The four-cycle CALL cost
is unchanged. This establishes the non-delayed immediate CALL contract, not
delayed/conditional-call RTN behavior or interrupt-pipeline timing.
Nondelayed `CALA A/B` also publishes the next-instruction address to RTN.
Two executable cases pause in the callee, replace its stacked continuation
with `dead`, then require RETF to return correctly and balance SP without
reading that sentinel. Both accumulator forms retain their six-cycle charge;
the core runner requires `TMS320C54x accumulator call fast return: PASS`.
The former stack-only implementation fails this test. Delayed and far-call
RTN publication remain separate, unvalidated contracts.
The same executable case now saves the paused callee with a sentinel stack
word, completes its fast return, then executes another CALL on page three
with a different continuation. Restoring the snapshot must recover page two,
the sentinel and the original RTN: a second RETF reaches the same original
continuation with balanced SP. This validates restoration of the otherwise
inaccessible return register after real architectural overwrite, rather than
merely checking its save-item declaration (`run_c54x_call_rtn_restore.log`).
The full executable core suite and tool suite pass. The post-change ROM4
RF-boundary integration check also passes: 207,040 RF reads, three port-32
writes, and no port-38/39 reads. This preserves the known native no-cell
boundary, not completed radio operation. The initial invocation failed
during fixture generation under `/tmp`; rerunning with workspace `TMPDIR`
resolved that storage failure without changing the fixture or acceptance
criteria (`run_c54x_call_rtn_rom4_workspace.log`).

Adjacent call forms remain a separate conformance question. `CALA`,
`CALAD` and `CALLD` currently push their continuation without publishing
RTN; their existing executable cases test target, stack and delay slots,
not fast return through RTN. Do not extend the immediate-CALL fix by
analogy: [SPRU131G example 7-5](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
labels delayed-call RTN as `a3`, the first delay-slot address, whereas
[SPRU172C pages 4-27/4-28](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)
explicitly stack instruction-PC plus four (after both delay words).
The stack continuation is unambiguous; whether that diagram mislabels
RTN or describes a distinct fast-return value is not settled by these
sources. Resolve that distinction before asserting a CALLD-to-RETF
expectation or changing its RTN behavior. The non-delayed immediate CALL
regression does not close accumulator, conditional or far-call coverage.
An isolated 30-second native 5110 run with passive `-verbose` opcode
observation executes `f074` CALL 103,727 times (first PC `0f15`) and `f274`
CALLD 6,502 times (first PC `0db3`). No `f4e3`/`f5e3` CALA or
`f6e3`/`f7e3` CALAD is observed in that window; this is not a static absence
claim. The same run passes the RF-boundary checker with the unchanged
207,040 reads and three port-32 writes. Its raw evidence is retained in
`run_rom4_call_inventory_verbose/error.log`. CALLD therefore has a measured
native workload, but execution frequency cannot resolve the RTN ambiguity
or prove its fast-return contract.

Software INTR publishes RTN just as hardware interrupt entry already does;
the existing three-cycle cost is unchanged. These are 64 architectural
return assertions, not a pipeline-latency oracle. The external reference cannot
adjudicate the outstanding interrupt-pipeline question without separate
validation. Adding our own timer/request model would remove the
independence needed to validate that model.

A bounded arithmetic comparison executes `2883` (`MAC *AR3,A`) with
T/source `1`, A `007fffffff` and ST0.C set. With OVM clear, the reference
agrees on A `0080000000`, sticky OVA and unchanged C/T/AR3. With OVM set,
it still publishes `0080000000`, rather than the TI-defined saturation
result `007fffffff`. The native core's existing MAC overflow fixtures
independently pass both outcomes against the primary contract. The runner
reports and reproduces the reference's OVM discrepancy without patching
its execution engine. Its PASS is not an arithmetic-conformance claim;
the simulator is not a drop-in oracle for status-mode behavior.
[DSPsim](https://mpcomplete.org/proj/dspsim/) instead implements a C audio
interface, not instruction execution. Neither is a differential oracle for
the short-period timer probe. The TI legacy simulator remains an acquisition
lead, not evidence that the current outcome is correct.
TI's public [CCS 3.3 service-release index](https://software-dl.ti.com/ccs/non-esd/releases/CCSv3/CCSv3.3/CCSv3.3ServiceReleases.htm)
still serves the SR12 update archive
`SR12_CCS_v3.3_SR_3.3.82.13.exe` (135,946,254 bytes; SHA-256
`a05ce0fae2485869ac55aaeb00b3a466fbdde24fe599e9db8279cce4b036bd33`).
The acquired archive is retained locally under ignored `run_ccs33_oracle/`.
Static extraction with 7-Zip and the distribution's `unshield` reader exposes
1,677 nested payload files. `drivers/import/ccs5400sim.txt` describes C5402
and C5410 device simulators with cycle-accurate CPU and Timer0 support;
C5402 additionally lists Timer1. These are vendor capability declarations,
not measurements of the short-period probe. The manifest contains C54x GEL
files, compiler interface `bti5400.dll` and hardware-emulator drivers, but
none of the C54x `.ccs` simulator configurations named by that table and no
identifiable C54x simulator driver. Do not mistake the XDS drivers or the
compiler interface for the simulator engine. Static PE inspection of the
included `cc_app.exe` also identifies vendor runtime dependencies, including
`CWDLL32.dll`, `CWSTART.dll` and `TISCC.dll`.
The index explicitly requires a previous CCS 3.3 installation. This update
therefore supplies useful capability/configuration evidence, not a standalone
timing oracle; acquisition still targets the base installation and its C54x
simulator component. No installer has been run or license accepted.
The official base-package referral linked by TI's
[CCS download support response](https://e2e.ti.com/support/processors-group/processors/f/processors-forum/1213508/ccstudio-ccs-compile-error)
redirects an unauthenticated request to the myTI login page. Acquisition via
that route requires user account access; no credentials or export/license
forms have been submitted. This is an observed access boundary, not evidence
that no legitimate independent copy exists.
TI's surviving [evaluation-tools landing page](https://www.ti.com/sc/eur)
also links a C5000 free-tools referral at
`https://www.ti.com/sc/docs/tools/dsp/c5000freetools.html`. An unauthenticated
HTTP check follows that referral to the current DSP product overview, not an
evaluation installer. A successful final HTTP 200 therefore does not establish
package availability. The pre-CCS standalone simulator remains a distinct
acquisition lead: TI's [SPRU302B reference](https://www.ti.com/lit/ug/spru302b/spru302b.pdf)
identifies the C54x Simulator Getting Started manual (SPRU137), with DOS and
Unix installations. That reference establishes historical software availability,
not timer/pipeline accuracy or a presently obtainable binary. Neither referral
provides a runnable differential oracle for the current timer probe.
The acquired [SPRU099D debugger guide](https://www.ti.com/lit/ug/spru099d/spru099d.pdf)
narrows this lead: sections 2.5/4.11 list timer simulation for `-mv540`
through `-mv549`; section 8.8 exposes pipeline opcode/address pseudoregisters
`p_ins/p_add`, `f_ins/f_add`, `d_ins/d_add`, `a_ins/a_add`, `r_ins/r_add`
and `x_ins/x_add`. It places ARAU updates in operand-access-1. A comparison
must therefore record Decode acceptance, auxiliary-register visibility and
Execute timing separately, not infer them from the final stacked PC. This
is a documented observation interface, not an acquired executable or measured
timer result. The 419-page PDF is retained at
`roms/reference-docs/c54x/ti_tms320c54x_debugger_spru099d.pdf`, SHA-256
`20e6c1573d959f3752d9f32187bd6119dee2c4a848b020af66ddbf23f685c909`.
TI's [C54x family datasheet](https://www.ti.com/lit/gpn/tms320lc543),
development-support table 14, identifies the standalone simulator products
as `TMDS324L851-02` (PC DOS/Windows) and `TMDS324L551-09` (SPARC).
These are acquisition identifiers, not the similarly named assembler/linker
or XDS hardware-debugger packages. Searches by these identifiers and by
`sim54x` have not yielded a runnable distribution; this is a bounded search
result, not proof that the software is unavailable.
The GNU-toolchain lead also needs this distinction: the author's
[BFD page-support discussion](https://sourceware.org/pipermail/binutils/2000-January/003156.html)
describes remote-debugger address-space encoding and assembler/linker work.
It does not establish an instruction simulator, timer implementation or
pipeline oracle. A C54x target name in binutils/GDB is insufficient evidence
for any of those capabilities. No GNU simulator result is used to validate
the short-period interrupt probe.
CALLD and RETD likewise use two and three cycles respectively, rather than
four each. A cycle-stamped call/return fixture checks both delay pairs and
stack balance against SPRU172C pages 4-27 and 4-139. CALA/CALAD retain their
documented six/four-cycle costs.
`XC 2` now charges the two rejected instruction words as NOP slots. Its
interrupt guard retires by instruction words, so a single two-word guarded
instruction releases a pending IRQ before the next instruction. Exact ROM4
`ff4d` fixtures cover taken/rejected BEQ timing, and a port/ISR fixture
checks the two-word guard boundary (SPRU172C, XC pages 4-198--200).
SPRU172C page 4-199 separately specifies a one-word conditional-execution
span for `XC 1` and states that XC plus the two following instruction words
are uninterruptible. Protection therefore remains two words for accepted
`XC 1`; rejecting its first slot leaves one protected word. The core keeps
this word-counted protection distinct from the complete-instruction INTM
guard above. Four firmware-free fixtures assert IRQ during XC's opcode fetch
and check accepted/rejected `XC 1` and `XC 2`: the ISR samples A only after
the two following words, with stacked continuation `0803`. Conditional
execution remains unchanged. The new assertion fails on the previous core
(`tmp/c54x-xc-irq-red.log`); the corrected core passes in
`tmp/c54x-xc-irq-green.log`, and conformance requires the
`TMS320C54x XC interrupt protection: PASS` marker. This is an architectural
interrupt boundary, not physical pipeline latency or evidence of the
recorder's failure cause.
The timer-boundary observer and original ROM4 RF gate still pass
(`tmp/c54x-xc-irq-regression.log`), preserving short-period mask starvation
and the no-acquisition boundary. Original MU4 storage/native-entry regressions
also pass (`tmp/mu4-xc-irq-regression.log`).
The long-immediate ALU decoder charges two cycles, including the separately
decoded XOR form. A cycle-stamped LD fixture checks the additional cycle;
the LD contract is SPRU172C page 4-68. This does not cover all extended-address
or repeated multicycle instruction timing.
The current HINT/absolute-compare model starts fresh-profile port-27 reads
on frame 30. The 30-second observation has 6,499 frame expiries and 207,040
reads (`32 * (6499 - 29)`), terminal IMR `035f` and no burst-port
activity. Fresh execution on the built `552db94` checkpoint reproduces
these counts (`run_5110_native_rf_current_20261009`), with terminal IFR
`0020`: INT0 is drained, but other pending sources are not required to be
zero by the gate. The gate pins that first-read frame, the three ordered RF control
pairs above and the 32-reads-per-frame cadence, allowing only the bounded
in-flight frame count at the fixed-time cutoff. The older frame-29,
zero-control-write oracle described the superseded timer boundary.
The twelve-second verbose MCU trace contains a type-`0x1a` search-list
publication at 1.511395 s (`00109800...`, 68 payload bytes). The seven
type-`0x51` packets at 2.065--2.071 s are segmented command-`0x22` DSP memory
uploads: their destination words advance from `0x2286` through `0x2370`.
Calling them radio configuration was wrong. The search-list publication proves
an MCU-side search request, but neither it nor the current MAD2 register map
establishes an SCU synthesizer write or a request that enters `0x7b0a`.
A conservative NSE-1 swap16 literal-seeded MMIO census resolved 562 direct
accesses from 235 seeds; all resolved offsets are below `0x40`. This does not
exclude dynamic/table-derived accesses or identify the SCU register. The
coherent first-access ledger likewise shows no offset above `0x3f` by 12 s.
The opt-in `mad2_static_census.py --constant-indexes` analysis tracks immediate
MOV values after each literal seed and admits register-indexed accesses with
known constants. On the same NSE-1 image it produces 691 candidates from
the same 235 seeds, versus the original 562; all still lie at offsets
`00..3f` (`run_archive/cleanup-20261009/root/run_5110_constant_index_census_20261009.json`). This removes one
specific census blind spot without identifying SCU. The scanner remains
linear rather than control-flow complete; pre-seed constants, computed
indexes, table pointers, ARM-mode code and other address windows remain
outside this result. Do not promote a candidate to silicon ownership
without checking its branch path and observed transaction.
A wider literal-seeded Thumb scan over `000000..0fffff` yields 942
linear candidates from 6,029 seeds
(`run_archive/cleanup-20261009/root/run_5110_internal_window_census_20261009.json`). Candidates above
`040000` originate at seeds `2a8026` and `2ab3b2`, in data-like instruction
streams; they are not established executable MMIO paths. Passive taps on
`040000..07ffff` during an isolated 30-second native boot observe just one
32-bit MCUIF write at `040000`, data `6a0f6120`, reported PC `20005c`.
The separate DSPIF `030000..030003` positive control observes 20 writes.
No wider candidate transaction is observed; the RF gate still reports
6,499 frames and 207,040 reads
(`run_5110_internal_window_runtime_20261009/error.log`). This bounds that
boot lifecycle only, not all internal windows or dormant SCU behavior.
The boot write itself demonstrates why the Thumb census cannot close
ARM-mode initialization ownership. DSP-local programming and computed
addresses remain independent open routes.
Neither `0x27` nor `0x39` is established as the complete FCCH/SCH sample
stream. The sibling emulator supplies only a constant for port `0x27`, so it
offers no independent sample-format evidence. No valid signal fixture follows yet.

A focused consumer trace under the earlier relative-compare timer model
closes the direct type-`0x1a` activation hypothesis for that measured lifecycle.
The resident host-command dispatcher advances the transmit-ring consumer at
C54x PC `0x3909`. The selected handler spans `0x3d70..0x3db6`: it derives
local value `0x0010`, copies the payload into scratch words `0x1200..0x121f`,
updates control
words `0x0284/0x0286/0x0287`, sets bit 3 at `0x06bc`, and increments `0x06e3`
from zero to one before returning. It performs no I/O-port access. Across the
same 30-second receiver gate, none of those identified control words is read
after the handler returns; the existing port-`0x27` cadence continues and the
`0x32/0x38/0x39` counts remain zero in that earlier run. The current absolute-
compare gate records three port-`0x32` writes; the old zero count is not a
current invariant. Temporary write/read taps used for this
classification were removed. Thus the observed search-list packet is accepted
and stored, but does not by itself enter the dormant parallel receive path or
establish an RF acquisition. Which lifecycle consumes the stored control state
and which DSP path owns ordinary acquisition remain unresolved. Separately,
the `0x7b0a` mode initializer remains useful for later dedicated-channel work;
injecting a reply or waveform at type-`0x1a` would skip both boundaries.

The earlier relative-compare post-handler reader census covers `0x1200..0x121f` and the complete
`0x0284..0x02b0` control/vector interval: 77 DSP data words. Temporary
read/write taps at the backend data bus observed zero accesses to those words
from 1.52 seconds through the end of a coherent 30-second no-cell run. These
taps see indirect and table-derived addresses as well as literal accesses;
they do not cover another lifecycle or unexecuted ROM paths. The RF gate
still recorded 6,497 frames and 207,232 sample reads. The taps were removed.
The original `0x1219` bound was too short: the handler stores one word and
repeats the payload copy 31 times, reaching `0x121f`.

Static word scanning finds 95 occurrences of literal `0x1200`, 28 of
`0x0284`, five of `0x0286`, and twelve of `0x0287`. These are candidate
references, not a complete decoded call graph. `0x1200` is also used by host
packet construction and memory-upload handlers, so its address alone does
not identify persistent search-list ownership. Candidate control consumers
include the `0x0926/0x0974` comparisons and the `0x6bxx/0x77xx/0x7axx`
mode routines; low program addresses additionally require overlay validation
before their raw-ROM instructions can be treated as the executing code.
No active consumer or missing emulated activation input follows from this
census. It does not distinguish an inactive command lifecycle from
signal-dependent activation. The next discriminating evidence is the
no-cell/carrier capture specified below, not a synthesized host reply or
forced dormant mode.

The next evidence should compare a no-cell boot with a real NSE-1 receiving
one known GSM-900 test carrier. Capture the ordered MCU-to-DSP request and
DSP-to-MCU response words, DSP program counter around `0x407c`, `0x410e`,
`0x4185`, `0x4414`, `0x3268..0x32c3` and `0x7b0a`, and DSP data words
`0x00ac/0x00ad/0x00af/0x1949/0x1973` before and after each INT0 frame.
Capture reads/writes
of DSP I/O ports `0x27`, `0x38`,
`0x39`, `0x31` and `0x32` with timestamps. To establish electrical sample
packing and tuning, also capture the COBBA parallel address/data, read/write
and data-available strobes and MAD2 `SynthEna/SynthClk/SynthData` pins (or
equivalent DSP/MMIO instrumentation). Record the 13 MHz reference and TDMA
frame edges so data order and latency can be aligned. An HPI RAM snapshot
alone cannot establish DSP I/O-port traffic or 12-bit bus sign extension.
The pin names and bus width come from Nokia's NSE-1 service manual cited
above; the requested trace contents are an experiment specification, not a
claim that any particular request or encoding has been recovered.

TI's C54x CPU guide places an important limit on this result: hardware reset
clears IFR and sets INTM, but does not initialise IMR or SP. The clean core
preserves IMR across MAD2 reset pulses. The recovered ROM's OR-mask sequence,
its INT0 receiver vector and the one-bit A/B result establish a product cold
entry value of `0x0001`; firmware then produces `0x0205` and `0x035f`. The available
external co-sim instead starts from `IMR=0x52fd`, an imported post-handshake
processor snapshot rather than a ROM4 power-on capture, and consequently
reaches `0x53ff`, executes `0x3065` (`IMR |= 1`), services INT0 at `0x3204`,
and reads port `0x27`. An A/B run with its optional RF model disabled still
takes that path with zero-valued samples, proving that the RF model supplies
sample contents rather than activation. It does not prove that the unrelated
bits in `0x52fd` belong to NSE-1.

An aligned-word census closes the literal variant of that question. Neither
the recovered `dsp_full.bin` nor the transform-entry program snapshot contains
an aligned big-endian word `0x52fd` or `0x53ff`. The program image contains four
aligned `0x0204` words and no aligned `0x035a`; runtime attribution identifies
the relevant OR-mask operations rather than assigning semantics to every
literal match. The transform-entry data snapshot likewise contains no aligned
`0x52fd`; its two aligned `0x53ff` words are captured data, not register
provenance. The reference implementation's own source describes `0x52fd` as a
post-bootloader Osmocom/Calypso register snapshot. It is therefore cross-silicon
differential evidence, not a Nokia MAD2 reset contract.

`make check-c54x-rom4-coherent` protects the short coherent boot and interface
initialization. `make check-c54x-rom4-rf-boundary` separately runs for 30
emulated seconds and protects the current receiver boundary: 20,280 CTSI frame
expiries, 648,032 RF reads, 1,537 port-`0x32` writes with the reviewed ordered
prefix and queue producers, terminal `IMR=0x035f`, serviced INT0 and no
port-`0x38/0x39` reads. The earlier zero-port-32 observation above is not a
current gate invariant. This is a receiver/programming-activity gate, not an
acquisition gate. With
the current evidence, a generated FCCH signal would require guessing both
the active receive interface and the COBBA sample representation; neither
is admitted as a hardware model.

`make verify-5110-save-state` saves the running real-DSP composition at seven
seconds, restores it, verifies MAD2 and C54x idle state, and then opens the same
Phone book menu through a physical key transition. This guards the C54x core,
uploaded program/data overlays, DSPIF, COBBA, timers, and keypad composition
against state-registration regressions.
The core fixture additionally saves during a 65,536-iteration ADD repeat with
IRQ2 pending, then compares original and restored completion (accumulator,
ISR observation and continuation PC). Its DSPIF instance contains two queued
RX packets; loading restores all 2,048 shared words, including payloads and
ring cursors, and the interface register after deliberate fixture disturbance.
The internal timer is stopped by an STM instruction in this fixture to isolate
repeat/IRQ replay from unrelated timer wakeups. This is executable state
restoration evidence, not just a source-level save-registration check.

The extended-program core fixture also saves between the two delay words of
`FCALLD` and `FRETD`. It completes each transfer, deliberately disturbs the
page, registers and stack, restores the snapshot, and compares replay with
uninterrupted execution. Distinct program-page contents check the pending XPC
transfer as well as the return PC, stack order and delay-slot effects. The
checkpoint is a scheduler boundary after the first operand instruction retires,
not a save from inside that instruction. This validates generic core state;
it does not establish the DA150 memory map or a working 5510 music subsystem.

The historical harness's headline `74 acknowledgements` counter is not a count
of DSP port-1 completion strobes. The local gate separately records shared
mailbox writes and completion strobes; their counts depend on the observation
window and repeated firmware polling. The external counter was attached to
intercepted MCU mailbox writes with different boot-phase lifetime rules. Keep
these as separately named measurements; do not tune the local transport merely
to make the integers equal.

An older persisted donor EEPROM produces the distinct `c9f4 cd44 ... 6075`
challenge and the structurally valid but rejected `3532 0000 312b ... 88b2
0000` response. That run repeatedly requests a reason-4 reset and is a negative
control, not a coherent-boot result. The coherent gate therefore regenerates
the external 24C16 profile and uses a fresh NVRAM directory before requiring
both the C54x completion and the MCU validator's `r6=1` continuation. Resident
slot `0x250b` is still an organically uploaded no-op; the recovered acquisition
path is the later COBBA serial-register read rather than a missing overlay at
that slot. No response field is synthesized.

The NSE-1 external EEPROM participates in this transaction. Firmware organically reads
its board-level 24C16 through PUP GenIO (SDA bit 0, SCL bit 2). A virgin repair
image generates a different challenge; a provisioned image generates the exact
known-good challenge. Runs must use a fresh NVRAM directory when changing the
ROM seed because MAME correctly persists the device contents.

The ROM4 data map also contains read-only dispatcher entries at offsets ending
in `0x07` across `0x9000..0xdfff`. A live run proved code at `0x37fc` otherwise
attempts to overwrite `0xb707` immediately before the challenge. The backend now treats
those mask-ROM writes as no-ops. This prevents later dispatcher corruption but
does not by itself correct the first challenge's task selection.
DSP I/O control ports `0x2c/0x2d` terminate in the opaque COBBA transport.
Data-space BDXR/BDRR use the separately owned BSP state and evidenced untimed
boot echo; I/O port `0x21` remains an opaque mask/write-back word, not a proven
microphone/DAC port. Operational PCM attachment remains open; see
[the address-space contract](rom4_audio_transport.md#native-nse-1-address-space-audit).

The core corrections required to reach this point are generic C54x semantics:
absolute `STM` extension order, `MVDM`, `DLD`/`DST`, `CMPL`, immediate `XOR`,
compound `XC`, all `IDLE` and ST0/ST1 bit-set/reset variants, `RPTZ`, and
memory-counted repeat of multiword instructions, repeated `MVDP` and `MVDM`
destination update, sign-extended shifted loads, carry rotations, delayed and
conditional control flow, interrupt return,
extended absolute arithmetic/load/store, and accumulator-indirect branch.
Operational ROM execution has additionally established signed/unsigned
accumulator arithmetic, accumulator-shift-mode loads, memory compare,
accumulator-addressed program writes, stack data pushes, conditional
branch/call/return families, signed `FRAME`, immediate cross-accumulator ALU,
and dual-memory moves. Focused core tests cover selected encodings in these
families; execution alone does not establish every variant's semantics. None
recognizes a Nokia address or loader byte pattern.

Operand admission is separate from observed instruction coverage. MMR
addressing excludes the long-offset/absolute Smem forms even where the same
modifier is legal for an ordinary memory instruction. The nine byte-encoded
MMR classes have a 4,608-case rejection/replay fixture, including reserved
bytes and both execution frontends. See the current
[operand-domain contract](c54x_execution_contracts.md#mmr-long-form-boundary) for scope;
this is not a hardware-trap claim or complete closure of RMW/copy operands.
