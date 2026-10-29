# C54x original-code arithmetic observations

TI instruction-family and original-firmware arithmetic maps. Independent operand/result controls are retained here; [C54x execution contracts](c54x_execution_contracts.md) owns processor admission and the full regression checkpoint.

## TI Arithmetic Family Audit

This is a syntax-level comparison against SPRU172C tables 2-1 through 2-6,
not a claim that every opcode variant or memory-bank timing is validated.
The newly added immediate-16 arithmetic, immediate MAC and DELAY forms are
fixture-only in the current 5110 idle/Menu/power union; the exact observed-word
assertion gate remains the higher-priority regression.

| TI table | Decoder and fixture state | Remaining contract |
| --- | --- | --- |
| 2-1 ADD | Smem, TS, shifted Smem/Xmem, dual X/Y, immediate SHFT/16, and accumulator SHIFT/ASM forms have selected value/status/cycle fixtures. | Other status/addressing variants remain unasserted. |
| 2-2 SUB | Smem, TS, shifted Smem/Xmem, dual X/Y, immediate SHFT/16, and accumulator SHIFT/ASM forms have selected fixtures; SUBB/SUBC are separately asserted. | Other status/addressing variants remain unasserted. |
| 2-3 multiply | MPY/MPYR, MPYU, MPYA, and SQUR syntax families have decoder paths and selected fixtures, including signed 17-bit A-high cases and an unsigned MPYU FRCT/OVM overflow boundary. | Other FRCT/OVM and long-offset variants are only partly asserted. |
| 2-4 MAC/MAS | Smem, dual X/Y, T-source A-high, immediate MAC, MACSU, MACD/P, and SQURA/SQURS families have selected fixtures. | Rounded/overflow and remaining addressing variants are incomplete. |
| 2-5 double operand | All six named Lmem families are decoded; C16=0/1 and long-offset fixtures cover selected forms. | Other addressing and overflow/carry boundaries remain unasserted. |
| 2-6 application | The named arithmetic/application families have decoder paths and selected fixtures, including DELAY, EXP/NORM, MAX/MIN, RND/SAT, FIRS/LMS and SQDST. | Full operand-mode and status coverage remains open. |

Every syntax listed in those six TI summary tables now has a decoder owner.
That is the result of a manual syntax inventory, not an exhaustive opcode or
silicon-conformance claim. The next audit layer is the unasserted status and
addressing combinations in the right-hand column.

The eight memory/T-source families MPY, MPYR, MPYU, SQUR, MAC, MACR, MAS and
MASR now share the staged frontend's fail-closed admission for unvalidated
absolute aliases `f9..ff`. Before the correction, the first atomic exclusion
case consumed an operand and changed A instead of stopping. The atomic
guard now precedes the data read and SQUR's T publication; admitted `f8`
absolute addressing is unchanged. This is an emulator-supported-domain
boundary, not a claim that physical silicon traps on those aliases, and it
does not close operand admission for every other Smem family.

The CPU-only `source-domain` census owns these rejection controls across
aliases, accumulators, CMPT, pages, frontends and before/after checkpoints.
It checks zero operand effects and supplied-state preservation. The former
per-family alias profiles are merged there; full regression and handset
acceptance remain separate evidence, not a silicon-conformance claim.

The six scalar load families LD, LDU, LD with TS, LDR, LD shifted by 16 and
DLD have the same bounded absolute-alias admission. The negative native
fixture failed at the first atomic case (56): an operand transaction and an
A change occurred without rejection. The atomic guard now rejects `f9..ff`
before those effects, matching staged admission without changing `f8`,
long-offset reads or store instructions. The same CPU-only census checks
rejection and preservation for these forms. This is an emulator-domain
consistency correction, not a newly observed handset opcode gap or proof
that silicon traps on these aliases.

SPRU172C's listed cycle counts assume DARAM. The focused tests assert those
baseline costs and selected absolute/long-offset surcharges; they do not
model external-memory wait states. A static decoder mask match is not counted
as a validated instruction in this matrix.

The immediate-MAC fixtures reproduce SPRU172C's `MAC #345h,A,B` fractional
example and `MAC *AR5+,#1234h,A` example. Immediate multiply word `6383`
(`MPY *AR3,#fffe,B`) additionally checks signed products -6 and -12 with
FRCT clear/set, T publication, unchanged A/source/AR3/carry and the two-cycle
DARAM cost. This destination variant is fixture-only, not newly observed
ROM4 boot execution.
The B-source immediate-MAC words `6683` and `6783` independently select A
and B destinations: with source B=10 and memory/immediate product -6, the
nonfractional cross-destination case yields A=4 while retaining B=10; the
fractional same-destination case yields B=-2 while retaining A=4. Both
assert T/source-memory/AR3/carry preservation and two-cycle DARAM cost.
These are fixture-only destination checks, not RF or speech acceptance.
Four additional B-source cases distinguish positive overflow for both
destinations with OVM clear/set: `7ffffffe + 3*2` retains `0080000004`
without OVM or saturates to `007fffffff` with it. Only the destination's
OVA/OVB bit is set, carry is retained and the other accumulator is unchanged.
T, AR3, source memory and the two-cycle DARAM cost are independently checked.
These destination fixtures are additional arithmetic evidence, not a new
observed-word inventory. Their fixture-only classification and the current
idle/Menu/power inventory remain separate.
The immediate-MAC fixtures also assert absolute extension
order, preincrement timing, T publication, and DARAM cycle cost. A long-offset
fixture verifies immediate-before-offset fetch and the extra cycle; a separate
FRCT/OVM fixture verifies saturation and OVB for the immediate form. The shared
Smem/immediate operand reader preserves the corresponding MPY behavior.
The `ADD/SUB #lk,16` fixtures assert the TI table-2-1/2-2 two-word,
two-cycle forms, including SXM sign extension, a distinct destination,
carry/no-borrow, and overflow with SXM clear. These forms are fixture-only;
the observed 5110 union has not executed them.
The table-2-6 comparison exposed a missing `DELAY Smem` decoder path. It now
shares `LTD`'s source-to-successor memory copy without updating T. Fixtures
cover the manual's `*AR3` example, preincrement, and both absolute and
long-offset extension-cycle surcharges; no current 5110 trace executes this
family.
Fixture-only `2483` checks that FRCT doubles the unsigned `MPYU` product,
OVM clamps the overflowing accumulator result, and OVA is set without
modifying T or AR3. It runs in one DARAM cycle.

The coverage tool separates fixture execution from explicit result
assertions. An `opassert` marker follows a passing exact-word check;
execution alone is not assertion coverage. Neither establishes unexecuted
ROM4 paths, unobserved encodings, or the instruction-family audit. Existing result
checks now explicitly
assert `7214`, `f5e2`, `f520`, `3292`, `e902`, `e903`, and `f120`, removing
them from the executed-only class without changing CPU behavior. Observed
`1093`, `1084`, and `1094` now check `LD` sign/zero extension, pointer behavior,
preserved status and one-cycle cost. Observed `4912` and `730b` now assert
MMR load/move direction and their one-/two-cycle costs; `6882` and `6884`
assert indirect `ANDM` masking, distinct AR selection, unchanged status, and
two-cycle costs. `6db1` and `6dc2` now assert AR0-offset and circular `MAR`
updates in one cycle; `e900` and `e901` assert short-immediate B loads reset
the guard and leave status unchanged. A synthetic fixture combining the
observed `ec1f` and `7693` words checks that `RPT #31` executes 32
consecutive `ST #lk,*AR3+` stores at TI's two-cycle body rate; it does not
claim those words are adjacent in the ROM. TI SPRU172C's opcode diagrams
distinguish `0x76xx` ordinary-memory `ST` from `0x77xx` page-zero `STM`;
the core comment was corrected, and the earlier `768a` ordinary-memory
fixture remains valid.
Observed `f032`/`f035` assert shifted long-immediate `AND` results,
unchanged carry and two-cycle cost. `f0fb`/`f0fe` assert one-cycle logical
right shifts, outgoing-bit carry and cleared accumulator guard bits against
TI SPRU172C's `SFTL` contract.
The previously leading absent word,
`fa20`, now has taken and not-taken `BCD NTC` fixtures
that check both delay slots, branch destination, and cycle cost. The leading
observed `1183` and `0093` fixtures cover negative Smem sign extension under
SXM, a fixed versus postincrementing AR3, OVM saturation and sticky overflow,
and the one-cycle DARAM cost. Together they account for 18 ROM4 executions.
Exact `f1a0`/`f2c0` test 40-bit cross-accumulator OR/XOR without source or
status changes; `f620` tests SUB B,A with OVM saturation and sticky overflow.
All three assert one-cycle timing and cover 23 observed ROM4 executions.
Exact `818a` and `81d3` store tests distinguish ordinary postdecrement from
post-access circular increment/wrap, preserve carry, and assert one-cycle
timing. They cover 16 observed ROM4 executions (SPRU131G section 5.3).
The observed MMR-load words `4815` and `4915` assert that LDM AR5 zero-extends
under SXM, replaces the full destination accumulator without changing the
other one or status, and costs one cycle (SPRU172C, LDM page 4-73). They
remove 18 observed executions from the exact-word gap list. The leading
MMR-store words `8814`, `8813`, `8811`, and `8912` now assert accumulator
low-word transfer to their respective address registers in one cycle. The
`ec0c` fixture checks thirteen iterations of `MAR` after `RPT #12` and the
one-cycle setup; the following `e80f` checks `LD #15,A` independently. The
`13f8`, `81f8`, and `7582` fixtures separately check unsigned absolute load,
absolute B low-word store, and the value and timing of an indirect port write.
Long-offset `BITF` and `CMPM` now charge TI's extra cycle; fixture-only
`61ea`/`60e2` check TC, AR updates, and three-cycle timing. Exact ROM4 `6180`
checks a false bit-field test, while `e736` checks AR3-to-AR6 MMR transfer.
Exact `fa4d` and `fc4d` fixtures check B-equal branch/return decisions in
both directions, delayed-slot execution, stack effects, and TI's cycle counts.
Fixture-only `47e2`, `4bea`, and `8bea` now check the long-offset surcharge
for `RPT Smem`, `PSHD Smem`, and `POPD Smem`, including repeat count, stack
data, AR update, and cycle cost.
Fixture-only `4fea` and `57e2` check the Lmem long-offset decoder: `DST`
preupdates AR and writes the high/low pair in three cycles; `DLD` leaves AR
unchanged, reads both words, and sign-extends the result in two cycles.
Fixture-only `40ea`, `34e2`, and `20ea` check the arithmetic/multiply Smem
long-offset surcharge against TI's instruction tables: `SUB`, `BITT`, and
`MPY` each consume an extension word and two cycles, with the expected AR
preupdate or preservation and an asserted result. These tests correct timing
for the corresponding decoded instruction families, but do not add a new
exact-word assertion for the observed ROM4 run.
Exact ROM4 `1282` (`LDU *AR2,A`) and `1082` (`LD *AR2,A`) fixtures use the same
negative 16-bit operand to distinguish zero- from sign-extension under SXM.
Both assert unchanged AR2 and the one-cycle indirect cost; this retires the
previously most-executed absent word (`1282`, 32 observed executions).
Exact ROM4 `f84e` (`BC pmad,BGT`) now asserts both the taken and fallthrough
targets, a negative 40-bit B test, and TI's five/three-cycle costs. It was the
next most-executed absent word (28 observed executions).
Exact ROM4 `ec01` (`RPT #1`) asserts two MAR body executions, the resulting
AR0 value, and the one-cycle repeat setup. It was observed 25 times.
Exact ROM4 `fa30` (`BCD pmad,TC`) checks both condition outcomes, both delay
words, the selected destination, and delayed-branch timing. Exact `8914`
(`STLM B,AR4`) checks the low-word transfer and one-cycle cost.
Fixture-only `71ea` and `70ea` check long-offset MVDK/MVKD: the Smem
displacement precedes the direct destination/source extension, with AR2
preupdate, copied data and three-cycle cost. Original MU4 `71e7/70e7`
execution independently exercises this ordering through cache program and
read-back. Fixture-only `75ea` checks PORTW's port-before-offset ordering,
AR2 preupdate, transferred data and three-cycle timing. These fixtures do not
change the captured ROM4 opcode-set coverage.
Fixture-only `7dea` checks MVDP with long-offset source addressing: program
destination before offset, AR2 preupdate, the program-memory word written,
and TI's five-cycle cost. This also leaves observed ROM4 coverage unchanged.
Fixture-only `7fea` and `7eea` check long-offset WRITA and READA in opposite
directions: the word transferred between data and A-addressed program memory,
AR2 preupdate, and TI's six-cycle cost. They do not alter observed ROM4 word
coverage.
TI SPRU131G section 5.5.3.2 states that the long offset is the last code
word of a two- or three-word instruction. The three-word fixtures for MVDK,
PORTW, and MVDP were re-encoded accordingly; fixture-only `7cea` now checks
MVPD's long-offset destination decode and four-cycle cost. Absolute Smem
forms retain their separately tested address-before-immediate ordering.
Fixture-only `74ea` checks PORTR's port-before-offset order, port-read value,
AR2 preupdate, and TI's three-cycle long-offset cost.
`6fea` checks the shifted Smem decoder's offset-before-extension-word
order, accumulator result, AR2 preupdate, and three-cycle long-offset cost.
Original MU4 InitDisk independently establishes this encoding with
`6fe1 0010 0c48` at `3ac3`: `LD *AR1(0010),8,A`. The prior synthetic
shift-before-offset encoding was wrong and is corrected. The generic `6f`
decoder resolves long Smem before reading its opcode-extension word;
absolute and short forms retain their existing regression coverage.
Original InitDisk's arithmetic helper also uses `f808` (`BC NC`). The core
implements carry-clear/set branches, including delayed forms, against TI's
condition codes; eight fixtures cover both carry states and both delay
forms while requiring status preservation.
Original InitDisk also corroborates long `CMPM` displacement-before-immediate
with `60e1 0002 0001` at `431f`. Four fixtures cover equal/unequal memory
and preupdate/no-update addressing while requiring TC-only status changes.
The previous decoder reversed these fields and rejected a valid context.
Original MCUSI16 `61e1 0001 0002` at logical `02:9521` likewise requires
BITF displacement-before-mask, selecting McBSP1 status `0049` and mask 2.
The reversed decoder read adjacent `004a`; the corrected native run reads
`0049` only. Four executable long-BITF fixtures cover set/clear TC,
preupdate/no-update, wrong-address sentinels and preserved unrelated status.
The earlier synthetic preupdate BITF encoding is corrected without changing
its expected result or cycle cost.
Original InitDisk's file-write path uses `9488` at `3be7`, the compact
`LD *AR2+,8,A` form documented by TI SPRU172C LD syntax 5 (pages 4-66/68).
The core implements `94/95` Xmem loads through the existing shifted-load
and dual-address update helpers. Sixteen executable fixtures cover both
accumulators, signed/unsigned input, shifts 0/5/8/15, postincrement and
preservation of the other accumulator. With this instruction the original
receiver accepts all seven R060 segments; this is not yet file-backed DMA
load or complete MU4 execution.
Exact ROM4 `e58b` (`MVDD *AR2+,*AR5+`) asserts the copied word, both pointer
increments, and its one-cycle cost; it was the most-used absent word in the
captured boot (24 executions).
Exact ROM4 `13d2` (`LDU *AR2+%,B`) asserts unsigned load of `0x8001`,
post-read circular wrap of AR2 with BK=4, and one-cycle cost. Exact `1882`
(`AND *AR2,A`) checks its masked result, unchanged pointer, and one-cycle
cost. They were the next two most-used absent words (23 executions each).
Exact `1081` (`LD *AR1,A`) checks SXM sign extension, unchanged AR1, and
one-cycle cost. Exact `ee01` (`FRAME #1`) checks SP advancement across a
page boundary and its one-cycle cost. Both had 22 observed executions.
Exact `f340` (`OR #lk,B`) checks preservation of B's upper bits; exact `f230`
(`AND #lk,B,A`) checks the zero-filled result in A and unchanged B. Both assert
TI's two-cycle immediate-logical timing and had 22 and 21 observed executions,
respectively.
Exact `f517` (`ADD A >> 9,B`) now checks both SXM=0 zero-fill and SXM=1
sign-fill of a negative 40-bit source, with A unchanged and one-cycle timing.
TI SPRU172C specifies SXM-controlled fill for a right-shifted ADD operand; the
generic shifted-ADD/SUB decoder previously always sign-filled. The old
`f517` and `f508` exact cases were unreachable behind that generic decoder and
have been removed. Exact `f4e2` (`BACC A`) checks the A-selected target,
unchanged stack pointer, and six-cycle branch cost.
Exact `f474` (`SFTA A,-12`) checks zero-fill and sign-fill under SXM=0/1,
the shifted-out bit in carry, and one-cycle timing. Exact `7722`
(`STM #lk,MMR22`) checks immediate-to-register transfer and two-cycle timing.
They had 20 and 16 observed executions, respectively.
Fixture-only `f537` (`SUB A >> 9,B`) checks SXM=0 zero-fill and SXM=1
sign-fill of a negative 40-bit source, the unchanged A source, and one-cycle
timing. This guards the SUB half of the generic shifted-arithmetic correction
without claiming that ROM4 executed this exact word in the captured boot.
Exact `6184` (`BITF *AR4,#lk`) now checks both TC outcomes, preservation of
carry and AR4, and TI's two-cycle cost; it had 14 observed executions.
Exact `e908` (`LD #8,B`) checks replacement of B without changing A in one
cycle. Exact `4914` (`LDM AR4,B`) checks that the MMR value zero-fills B even
with SXM set, leaving AR4 and A unchanged in one cycle. They had 14 and 13
observed executions, respectively.
Exact `f3ff` (`SFTL B,-1`) checks the logical result, cleared guard bits,
carry from bit 0, and one-cycle timing despite SXM=1. Exact `60f8`
(`CMPM *(lk),#lk`) checks the absolute address-before-immediate order, TC and
carry, and the three-cycle absolute surcharge. Both had 13 and 12 observed
executions, respectively.
Exact `0892` (`SUB *AR2+,A`) checks SXM sign extension of a negative Smem
operand, post-read AR2 increment, and one-cycle timing. Exact `8082`
(`STL A,*AR2`) checks low-word storage with A and AR2 unchanged in one cycle.
They had 10 and 13 observed executions, respectively.
Exact `10d2` (`LD *AR2+%,A`) checks SXM sign extension and circular AR2
post-update with BK=4 in one cycle. Exact `e50b` (`MVDD *AR2,*AR5+`)
checks the copied word, unchanged X pointer, Y post-increment, and one-cycle
cost. Both had 12 observed executions.
Exact `1083` (`LD *AR3,A`) and `1284` (`LDU *AR4,A`) check signed versus
unsigned loading of the same negative-looking word with SXM set. Both
preserve their pointers and take one cycle; they had 13 and 11 observed
executions. Exact `890e` (`STLM B,T`) checks B's low-word transfer to T,
unchanged B, and one-cycle timing; it had 11 observed executions.
Exact `f1f4` (`SFTL A,-12,B`) checks a logical right shift, carry from
source bit 11, unchanged A, and one-cycle timing. Exact `f280` (`AND B,A`)
checks that B remains intact while A changes in one cycle. Exact `f350`
(`XOR #lk,B`) checks a two-word immediate logical operation, unchanged A,
and two-cycle timing. Each had 10 observed executions.
Exact `71d2` (`MVDK *AR2+%,dmad`) checks the copied word, circular AR2
post-update with BK=4, and two-cycle cost. Exact `f842` (`BC pmad,AGEQ`)
checks both the zero-A taken path and negative-A fallthrough, their PCs,
and five/three-cycle costs. Both had 10 observed executions.
Five older result checks now carry exact-word markers: `f062` and `f070`
for the long-immediate load/repeat sequence, `f944` for conditional call,
`f485` for signed absolute value, and `ec03` for the four-iteration repeat.
These are existing scenario assertions, not new isolated timing fixtures.
The last three executed-only words now have isolated checks: `0083`
(`ADD *AR3,A`) checks SXM sign extension, unchanged AR3, and one-cycle cost;
`e726` (`MVMM AR2,AR6`) checks the register transfer and one-cycle cost;
`7726` (`STM #TSS,TCR`) checks the two-cycle store by reading TCR back
through fixture-only `4826` (`LDM TCR,A`).
Exact `f945` (`CC pmad,AEQ`) now checks taken and fallthrough PCs, the
pushed return address and stack pointer, and five/three-cycle costs. Exact
`fc44` (`RC ANEQ`) checks both return outcomes, stack effects, and the same
five/three-cycle costs. Each had 10 observed executions.
The challenge-loop words `6d8c`, `8084`, `108a`, `1a8b`, `1c84`, and `e598`
now have isolated
assertions as well as the aggregate transform-result check.
The 143-word class is a priority list
for new fixtures, ordered by observed execution count, not proof that those
instructions are incorrect. A marker establishes the checked outcome only,
not complete coverage of an instruction's operand or flag variants.
`--group-report` with `--fixture-log` ranks unasserted ROM4 executions by
opcode high byte. This is a workload ranking, not an instruction-family
decoder. `--all-gaps` lists every unasserted word by observed use, with an
`absent` or `executed-only` class; the default report shows only the leaders.
In the current 30-second idle/Menu/power union, `f4` leads with 3,155,723
executions of `f495` NOP and 1,572,960 of `f491` ROL A. Both exact words
are asserted; group frequency is not an untested-opcode count. The formerly
dominant `4a`/`8a` gaps are now covered by
a twelve-register MMR save/restore fixture: it initializes each register,
checks all twelve stack values, overwrites the registers, and checks the
restored values after the reverse pops. This adds exact checks for 24 ROM4
stack words and the ROM4 `STM` initializer words. The current group report
ranks `ff` highest among unasserted groups (7 absent words, 26 executions).
The asserted MMR stack words `4a09`/`4a0a` and `8a0a`/`8a09` check
accumulator-A high and guard-word stack order, guard width, preservation of
the low accumulator word, and final SP restoration. The corresponding
`4a0b`/`4a0c`/`4a0d` and `8a0d`/`8a0c`/`8a0b` fixtures check all three B
slices, LIFO order, A preservation, and SP restoration. TI SPRU172C specifies
one word and one cycle for each PSHM/POPM form; the core has no extra cycle
charge for these opcodes. It specifies two cycles for `STM #lk, MMR`;
exact ROM4 `7712`, `4a12`, and `8a12` now check AR2 transfer, stack
movement, and those costs. Other MMR register encodings remain separate tests.
The high-use ROM4 `8807`/`4807`/`4a07`/`8a07` forms now check the ST1 MMR
write, read, and stack round trip at their one-cycle costs. TI SPRU172C
states that `LDM MMR,dst` ignores SXM; a status value with bit 15 set exposed
the core's erroneous sign extension. Both A and B LDM destinations now
zero-extend, with `4907` exercising the B form in the fixture.
TI SPRU172C gives `IDLE K` a four-cycle minimum before its unbounded idle
interval. The core now charges that minimum instead of one cycle. An exact
ROM4 `f4e1` fixture checks `IDLE 1` entry, retained continuation PC, and no
premature execution of the next word. Six additional executable cases check
SPRU131G section 6.11.1: external IRQ and timer requests remain latched but
do not wake IDLE1 while masked in IMR. Enabled requests wake independently
of INTM; INTM clear enters the ISR, while INTM set resumes the continuation
without servicing the request. The timer continues during IDLE1. These
fixtures do not independently time the minimum or establish reset wake.
Four further cases implement the timer-clock part of SPRU131G sections
6.11.2/6.11.3: `f6e1` (IDLE2, NN=10) and `f5e1` (IDLE3, NN=01) freeze TIM
and its prescaler while retaining TSS=0. Masked external INT0 leaves the
timer frozen; enabled INT0 with INTM set resumes the foreground and timer,
which can subsequently latch TINT during IDLE1. All four cases save and
restore the frozen counter/prescaler, then repeat the wake outcome. The core
saves the selected idle mode and remaining timer ticks; it does not substitute
a period reload on wake. Reserved NN=11 is not accepted as an idle mode.
This establishes timer suspension and restoration, not PLL lock time,
cycle-exact sleep-entry latency, pulse-width qualification, reset wake,
or complete clock-source behavior of attached peripherals.

External NMI is a separate architectural input, not an IFR/IMR source.
[SPRU131G sections 6.10/6.11 and table 6-20](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
specify NMI priority, vector 1 at `IPTR:0004`, global masking on ISR entry,
and wake independently of IMR/INTM. The core latches an assertion, wakes any
idle mode and preserves maskable IFR bits when servicing NMI. Twelve
executable cases cover IDLE1/2/3, both INTM values and empty/full IMR;
simultaneous pending maskable sources do not displace NMI. Acceptance checks
the vector, stacked idle continuation, unchanged IFR/IMR and ISR INTM state.
`check-c54x-core` requires this result and twelve pending-request save/replay
cases. The fixture suspends instruction execution after idle, presents NMI,
and saves after the input has latched but before the vector/stack write.
Restoration recovers the pre-entry PC, SP, original INTM and stack sentinel;
resuming reaches the same ISR result exactly once. A repeated held assertion
does not enqueue a second NMI after restoration. This fixture-only execution
suspension is not a modeled physical clock-stop or interrupt latency.
[SPRS039C's interrupt timing requirements](https://www.ti.com/lit/gpn/tms320lc543)
describe the physical synchronizer and pulse qualification. These logical
fixtures do not establish that timing, PLL restart, nested-NMI behavior or
pipeline drain; the existing aggregate entry-cycle charge remains calibrated.
No handset NMI wiring or boot-unblocking timer delay is inferred.
SPRU302B sections 2.10/3.8 explicitly qualify the generic power-down summary:
externally clocked/frame-synchronized McBSP and DMA transfers may continue and
wake IDLE2/3. DMA automatically gates its transfer clock and uses X2/CLKIN
instead of the halted PLL in IDLE3. A blanket CPU interrupt-source filter or
blanket DMA suspension would therefore be incorrect.
The CPU now publishes its peripheral-clock stop line to the MU4 McBSP devices.
Only the internally generated bit timer pauses; its pending interval and shift
state survive save/load and resume. Four coupled CPU/McBSP cases verify a
queued internal word stays pending during deep idle and completes after
enabled external wake, including save-state replay. Four additional pin-driven
cases verify externally clocked words complete while asleep and wake the CPU
only with the serial IMR bit enabled. These synthetic edges establish ordering,
not a physical baud rate. PLL stabilization and unsupported serial clock/frame
configurations remain outside this coverage.
Ten coupled CPU/DMA cases additionally cover all three IDLE modes with masked
and enabled completion interrupts while INTM remains set. The real DMA device
copies a four-word data-memory block after the CPU has entered idle; masked
completion leaves the foreground asleep, while enabled completion resumes it.
Each case saves before the first transfer, restores the pending device timer
and sleeping CPU, and reproduces the block and wake result. The fixture selects
the documented DMAC0/BRINT2 vector slot explicitly; it does not validate a
product's interrupt mux. Its 100-us observation window establishes completion
and ordering. An additional completion-time assertion checks sixteen DMA
device clocks for each four-word block, including every save-state replay,
against SPRU302B section 3.5's four-clock internal-word transfer. This does not
establish DA150 arbitration, external wait states, separate read/write bus
phases or a product's physical IDLE3 input-clock rate. The generic DMA now
receives a separate CPU PLL-stop output: IDLE3 selects configured, undivided
X2/CLKIN while IDLE1/2 retain the system clock, as specified in SPRU302B 3.8.
Two cases and their replays assert sixteen clocks at a deliberately different
3.25 MHz test input. Two additional controller cases leave X2/CLKIN absent,
check that an enabled transfer remains pending without modifying its destination,
and verify completion after restoring the system clock, including replay.
Two further cases interrupt the first DMA word after exactly two clocks,
retain its pending state through a 100-us clockless interval, and assert the
remaining two clocks rather than a restarted four-clock word, including replay.
Pending clock switches round up to the next whole transfer clock; sub-cycle
switching and PLL startup latency remain unvalidated. MU4 now supplies the
11.9952 MHz Y101 rate evidenced by its own schematic sheet 4 to this connection.
The original MU4 system-clock requests now drive delayed PLLSTATUS, CPU
execution and system-clocked peripheral rates. The MU4 schematic's `111`
straps select an explicitly C5416-compatible DIV/2 reset, and IDLE3
invalidates retained lock for software-managed PLL re-entry. Exact DA150
reset-table identity, automatic PLL-selected wake and sub-cycle switching
remain unvalidated; see the
[MU4 CLKMD contract](mu4_hardware.md#original-clkmd-contract).
ROM4 `ed00` now clears a preloaded ASM=-1 before an ASM-based accumulator
load; `8083` then stores A's low word through *AR3 without changing the
pointer. The fixture checks both results and the three one-cycle operations
as an aggregate. Other ASM values and STL addressing forms remain separate
tests.
Exact ROM4 `f490` (`ROR A`) and fixture-only `f590` (`ROR B`) now check
incoming carry into bit 31, outgoing bit 0 into C, cleared accumulator guard
bits, other-accumulator preservation, and TI SPRU172C's one-cycle cost.
Exact ROM4 `f845` (`BC pmad, AEQ`) checks both branch outcomes: zero A takes
the branch in five cycles; A with only a nonzero guard byte falls through in
three cycles. The intervening port-write markers check destination and cycle
cost without assuming a cycle-exact wall-clock timer.
Exact ROM4 `f3b0` (`OR B >> 16, B`) checks zero-filled right shift of a
negative 40-bit source, unchanged A and carry, and one-cycle cost. TI SPRU172C
specifies no sign extension for this OR shift. The generic decoder already
implemented that rule; two unreachable switch cases beneath it instead used
arithmetic shift and have been removed to prevent a misleading fallback.
Exact ROM4 `f3e8` and `f3f8` are `SFTL B, 8` and `SFTL B, -8`, not 40-bit
rotates. Their fixtures check the low-32-bit shift, cleared guard byte,
outgoing carry bit, unchanged A, and TI SPRU172C's one-cycle cost. Six
unreachable switch cases labeling these shifts as rotates or arithmetic shifts
were removed; the generic SFTL decoder was already taking precedence.
Exact ROM4 `f330` (`AND #lk, B`) and `f130` (`AND #lk, A, B`) now check the
zero-filled immediate mask, source preservation in the cross-accumulator form,
unchanged carry, and TI SPRU172C's two-cycle cost. The latter was previously
absent from the fixture; neither marker claims all shift-count variants.
Exact ROM4 `f010` (`SUB #lk, A`) and `f000` (`ADD #lk, A`) now check both SXM
states with immediate `0xff80` and the two-cycle cost. The SXM-clear SUB test
failed before the correction: the shared long-immediate arithmetic path
sign-extended the operand unconditionally. It now uses the existing SXM-aware
operand helper; logical immediates remain unextended. These checks do not
cover all shift counts or saturation cases.
Exact `f310` (`SUB #lk,B`, 82 observed dispatches) now checks SXM sign
extension, B-only destination, and the two-cycle cost.
Exact `6d8a` (`MAR *AR2-`, 96 dispatches) checks one-cycle pointer modification.
Exact `f846` (`BC AGT`, 81 dispatches) and `f84d` (`BC BEQ`, 81 dispatches)
check the 40-bit accumulator conditions and taken/untaken five-/three-cycle
costs respectively; these are selected outcomes, not full condition coverage.
Exact `11f8` (`LD *(lk),B`, 80 dispatches) checks SXM sign extension and the
two-cycle absolute-address surcharge. Exact `f110` (`SUB #lk,A,B`, 68
dispatches) checks the source/destination split and two-cycle cost. Exact
`7482` (`PORTR PA,*AR2`, 79 dispatches) checks the port-to-memory value,
unchanged pointer, and two-cycle interval between consecutive reads; external
I/O wait-state timing remains outside this fixture.
Exact `56f8` (`DLD *(lk),A`, 65 dispatches) checks high/low word ordering,
SXM guard extension, and the absolute-address two-cycle cost. Exact `7213`
(`MVDM dmad,AR3`, 66 dispatches) checks the MMR destination and two-cycle
cost. Exact `708a` (`MVKD dmad,*AR2-`, 64 dispatches) checks source/destination
direction, postdecrement, and two-cycle cost.
Exact `ec0e` (`RPT #14`, 68 dispatches) checks fifteen one-cycle ADD
iterations after one-cycle setup. Exact `e753` (`MVMM AR5,AR3`, 60
dispatches) checks register direction and one-cycle cost. Exact `7194`
(`MVDK *AR4+,dmad`, 64 dispatches) checks source postincrement, destination,
and two-cycle cost.
Exact ROM4 `6c88` (`BANZ *AR0-`, 55 dispatches) now checks both outcomes in
compatibility mode with physical AR0 and ARP-selected AR4 holding opposing
values. The core had tested physical AR0 while updating AR4; both BANZ and
BANZD now select the same register for test and modification. Fixture-only
`6e88` checks the delayed form's selected-register test and two delay words.
Exact ROM4 `f063` (`AND #lk,16,A`) now checks the upper-word mask, preserved
carry and TI SPRU172C's two-cycle cost; its special-case decoder had charged
one cycle. Exact `8184` (`STL B,*AR4`) checks the low-word store and one-cycle
cost. Fixture-only `f162` (`LD #lk,16,B`) checks signed guard extension and
the same two-cycle immediate-load cost; both fixed-shift `LD` special cases
had also undercharged one cycle.
Exact ROM4 `6d96` (`MAR *AR6+`) now checks the selected register's increment
and one-cycle cost. `e712`/`e713` (`MVMM AR1,AR2/AR3`) check source preservation,
both destinations and one-cycle costs.
Exact ROM4 `6c8b` (`BANZ *AR3-`) checks the pre-decrement predicate and
taken/false cycle costs. `f847` (`BC ALEQ`) checks both signs of the 40-bit
accumulator and both branch costs. `880e` (`STLM A,T`) checks the low-word
transfer to T and one-cycle cost.
Exact ROM4 `0af8` (`SUBS *abs,A`) now checks sign-extension suppression with
SXM enabled, carry for both borrow outcomes, and its two-cycle absolute cost.
Exact ROM4 `1e82` (`SUBC *AR2,A`, 16 observed executions) now checks both
conditional-subtraction outcomes: the shifted quotient bit, carry, unchanged
address register and one-cycle indirect cost. The earlier `1ef8` fixture
covered only a successful subtraction.
Exact ROM4 `6dea` (`MAR *+AR2(-7)`, 25 observed executions) now checks signed
long-offset consumption, pointer result, and TI's extra cycle.
Exact ROM4 `f300` (`ADD #lk,B`, 29 observed executions) now checks SXM-driven
negative-immediate extension, carry at the 32-bit boundary into the guard
byte, and two-cycle cost in the shared immediate ALU path.
Exact ROM4 `f210` (`SUB #lk,B,A`) checks B-to-A source/destination selection,
source preservation, both borrow outcomes and the two-cycle cost. Exact
`f150` (`XOR #lk,A,B`) checks the unextended logical immediate, source/guard
preservation, unchanged carry and two-cycle cost.
Exact ROM4 `0282` (`ADDS *AR2,A`, 21 observed executions) now checks
sign-extension suppression despite SXM, carry at the 32-bit boundary, stable
AR2 and the one-cycle indirect cost. The decoder's older `ADD uns` comment
has been aligned with TI's instruction name.
Exact ROM4 `6d90` (`MAR *AR0+`) now checks the compatibility-mode AR0 alias:
with CMPT set, an AR0 operand modifies the register selected by ST0.ARP and
leaves physical AR0 and ARP unchanged. Fixture-only `1090` checks the same
selection for an indirect read and postincrement. The previous core always
used physical AR0. TI SPRU172C's MAR compatibility table specifies this
distinction; standard mode still names physical AR0. The existing `RPTBD`
fixture was corrected to set standard mode explicitly, matching its expected
physical-AR0 result.
Exact ROM4 `1c8b`, `1d93`, and `1c82` now check the indirect XOR sequence:
`*AR3-` reads before decrementing, `*AR3+` reads the new address before
incrementing, and `*AR2` leaves its pointer unchanged. The A/B results keep
the 16-bit memory words unextended even with SXM set; carry is preserved.
Port markers bound the three operations to three cycles in total, matching
[TI SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)'s one-cycle
indirect XOR specification for DARAM. This fixture does not claim cycle
accuracy for external memory wait states.
Exact ROM4 `768a` (`ST #lk,*AR2-`) and `8092` (`STL A,*AR2+`) now form a
pointer round trip: the immediate is stored before decrement, then A's low
word is stored at the decremented address before increment. A middle port
marker checks their separate two-cycle and one-cycle costs, matching TI
SPRU172C's DARAM instruction table. This does not establish external-memory
wait-state behavior.
Exact ROM4 `e5a8` and `e5ca` now check MVDD X/Y memory transfers. The first
copies `*AR4+` to `*AR2+`; the second reads `*AR2+0%`, writes `*AR4+`, and
wraps AR2 with BK=4. Both fetch the source and destination addresses before
modifying either pointer. Port markers check one cycle each for the fixture's
DARAM operands, consistent with [TI SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf);
[TI SPRU131G](https://www.ti.com/lit/ug/spru131g/spru131g.pdf) defines the
dual-operand address-modification rules. The fixture does not model external
memory contention or wait states.
Exact ROM4 `12f8` (`LDU *(lk),A`) and `138b` (`LDU *AR3-,B`) now check
zero-extension of high-bit-set 16-bit data even when SXM is enabled. The
absolute operand consumes its address extension and costs two cycles; the
indirect form reads before decrementing AR3 and costs one. Separate port
markers verify those costs against [TI SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)'s
DARAM LDU table. External-memory wait states are not covered.
Exact ROM4 `f3c8` (`XOR B,8,B`) now checks that the source is B's original
40-bit value, the left shift discards bits beyond bit 39, the result uses
the guard byte, A and carry are unchanged, and the operation costs one cycle.
This matches [TI SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)'s
accumulator-XOR definition; other source/destination and shift variants
remain separate checks.
Exact ROM4 `6b8a` (`ADDM #lk,*AR2-`) exposed a missing memory-arithmetic
contract. With SXM and OVM set, TI SPRU172C's `0x8007 + 0xfff8` example
saturates to `0x8000`; the core previously wrapped to `0x7fff` and left C/OVA
unchanged. It now updates those flags and applies 16-bit saturation, with
separate fixtures for positive overflow with OVM clear and unsigned carry
without signed overflow. Fixture-only `6b80` verifies compatibility-mode AR0
selection through ARP; `6880`/`6980` check the same selection for ANDM/ORM.
[TI SPRU538](https://www.ti.com/lit/ug/spru538/spru538.pdf) confirms that a
memory-plus-immediate addition affects C. [TI SPRU131G](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
defines carry at bit 32, even though ADDM stores a 16-bit result. A separate
SXM-clear fixture checks `0x8000 + 0x8000`: the stored word wraps to zero and
sets OVA, but produces no bit-32 carry. This corrected the first
implementation's 16-bit carry test.
The fixtures do not cover every SXM-clear overflow and OVM combination.
Exact ROM4 `7182` (`MVDK *AR2,dmad`) checks the extension-addressed copy and
unchanged AR2 in two cycles. Exact `8183` (`STL B,*AR3`) checks that only BL
is stored while AR3 stays fixed in one cycle. Separate port markers verify
the DARAM costs against [TI SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf);
external-memory wait states remain outside this fixture.
Exact ROM4 `f820` (`BC pmad, NTC`) and `f84c` (`BC pmad, BNEQ`) check taken
and fall-through destinations at TI SPRU172C's five- and three-cycle costs.
The BNEQ true case has a nonzero guard byte and zero low 32 bits, so a
truncated accumulator predicate would fail. Other branch conditions remain
separate cases.
The high-frequency control words `f495` (`NOP`), `f074` (`CALL pmad`), and
`fc00` (`RET`) now have separate port-marker checks for TI SPRU172C's one-,
four-, and five-cycle costs, respectively. The CALL/RET checks also verify
the pushed return address, destination, and final stack pointer. Existing
exact-word checks for `f273` (`BD pmad`) and `f4eb` (`RETE`) verify delay-slot
continuation and interrupt-return PC/SP/INTM state; they do not independently
measure those two cycle costs. This separates validated outcomes from words
that merely execute during a larger transform.
Four further high-use words already had exact-result checks and now carry
assertion markers: `f484` checks negate result, carry, and overflow;
`f0b0` checks zero-filled `OR A >> 16`; `fa44` checks delayed ANEQ branch
continuation; and `e801` checks the loaded A value in a BANZD delay slot.
These markers do not add cycle-cost claims for those words.
Exact ROM4 `f493` (`CMPL A`) now checks all 40 complemented bits, unaffected
B and carry, and the one-cycle cost. Exact `f0e8` (`SFTL A, 8`) checks a
guarded input's low-32-bit shift, cleared guard, outgoing carry, unaffected B,
and the one-cycle cost. Other source/destination variants remain separate.
Exact `f065` (`XOR #lk,16,A`) now checks the shifted result and TI SPRU172C's
two-cycle cost. The core previously charged one cycle to all four
source/destination variants of this encoding; the shared path now charges
two. The 30-second RF-boundary run retains 6,498 frame expiries and
`IMR=0x035f`, with 207,168 RF reads; at 40 seconds it has 8,664 expiries
and 276,480 reads. Both differ from the ideal completed-frame count by
two 32-read frames at the fixed-time cutoff, so the gate admits up to two
in-flight frames and still rejects a three-frame deficit.
TI SPRU172C table 2-16 specifies two cycles for both `RPTZ dst,#lk` and the
delayed `RPTBD pmad` form. Exact ROM4 words `f071` and `f272` now have
result/control-flow and cycle assertions; their shared A/B and block-repeat
paths previously charged one and four cycles respectively. The non-delayed
`RPTB` form remains at its documented four cycles, now checked directly with
exact `f072` and the same one-word-block fixture. The five-ROM cross-regression
passes after both timing corrections.
TI SPRU172C's Smem tables add one word and cycle for absolute addressing.
ROM4 `34f8` (`BITT absolute`) now checks both T=0 (bit 15) and T=15 (bit 0),
and its two-cycle cost. The decoder's broad `0x30..0x3f` Smem handler already
implemented `15 - T[3:0]` correctly; a later exact-`34f8` handler was
unreachable and used the wrong bit mapping. The unreachable case is removed,
and the absolute cycle surcharge is applied in the actual broad handler.
Exact `70f8` (`MVKD dmad, absolute Smem`) now checks its data movement and
three-cycle cost: two base cycles plus one for absolute Smem. The five-ROM
cross-regression passes with these corrections.
Exact ROM4 `61f8` (`BITF absolute`) and `6182` (`BITF *AR2`) fixtures now
check the TC result and TI SPRU172C's three- and two-cycle costs. The shared
handler previously charged one cycle in both forms. Exact `68f8`, `69f8`,
and `6bf8` fixtures check ANDM, ORM, and ADDM data movement plus their
three-cycle absolute costs; their handlers likewise charged one cycle.
The underlying two-cycle indirect forms receive the same corrected base
charge. The five-ROM cross-regression passes after these timing changes.
The most-executed non-NOP fixture-executed-only word was `ff0c` (over one
million ROM4 executions). TI SPRU172C's condition table identifies it as
`XC 2,C`: it tests carry, not TC. Exact-word fixtures now check both C=0
(skip two following words) and C=1 (execute both), the one-cycle XC cost,
and the resulting `f793` full-width B complement. No core behavior changed
for this test; both words are now explicit assertions in the coverage report.

Execution counts identified `ROL A` (`f491`) and `ROL B` (`f591`) as the largest
previously unasserted ROM4 encodings, at roughly 524,000 executions each in
this run. Both now have core assertions for carry transfer, cleared guard bits,
and preservation of the other accumulator, checked against TI SPRU172C
section 4. Immediate `XOR` (`f050`, about 262,000 executions) now has an
exact-encoding result and extension-word assertion. The next high-use group
was `74d6`/`b03a`/`b0be`/`b3be` (155,000-207,000 executions each). `74d6`
is `PORTR` into circularly post-incremented AR6; its exact-word fixture now
checks port data, pointer wrap, and TI SPRU172C's two-cycle base cost. The
paired `PORTW` path has the same cycle-cost correction and a symmetric
fixture. `b03a`, `b0be`, and `b3be` now have exact-word signed-MAC and
pointer-update assertions. The I/O wait-state component of `PORTR`/`PORTW`
timing remains unmodeled. `f844` (`BC pmad, ANEQ`) exposed a shared branch timing error:
TI specifies five cycles taken and three not taken. Both outcomes now have
executable cycle assertions, and the decoded `BC` variants share that charge.
`10f8` (`LD` with absolute Smem) likewise has an exact-word sign-extension
and two-cycle assertion; the other signed/unsigned accumulator-load variants share
the absolute-address surcharge. In the next loop at DSP PC
`0x324c..0x3268`, exact `e2e4` (`SQDST`) asserts that B receives the square
of A's *old* high word while A receives the signed vector difference. Exact
`4f81` (`DST B,Lmem`) asserts high/low store order and TI's two-cycle cost;
the absolute `DST` and `DLD` forms also receive their documented extension
cycle surcharge. TI SPRU131G table 5-4 specifies that single-operand MOD
increment/decrement is two words for a 32-bit operand. Separate `DST`/`DLD`
fixtures now cover post-increment, pre-increment-before-read, and circular
wrap with a two-word step; `4f81` itself uses a non-modifying AR mode.

The same loop exposed a decoder error at `0x3262`: `6ded fff9` is a two-word
`MAR *+AR5(-7)`, not `MAR` followed by an `XC` opcode. Consuming the signed
extension reveals `4092` (`SUB *AR2+,16,A`) and `5781` (`DLD *AR1,B`), both now
implemented or asserted in the core fixture. The corrected stream still
passes the 5110 coherent, 30-second RF-boundary, and menu gates. Exact-word
fixtures now cover signed dual-memory `a5be` (`MPY`), rounded `b736`
(`MACR`), and parallel `d6e1` (`ST`/`MACR`). The most-executed remaining
ROM4-only word was `4593` (`LD Smem,16,B`, 32,770 executions); an exact-word
fixture now checks sign extension, post-increment, and its one-cycle cost.
The same one-cycle `LD Smem,16` family now has exact-word fixtures for
`4492`/`4493` (A with AR2/AR3 post-increment) and `458a`/`458b` (B with
AR2/AR3 post-decrement). They assert signed and positive 40-bit results,
pointer direction, and cycle cost, covering four further ROM4-dispatched
words without changing the decoder.
The register-indirect `OR Smem` group now has independent result, pointer,
and one-cycle assertions for `1a82`/`1a83` (A) and `1b82`/`1b83` (B),
matching TI SPRU172C table 2-8's base cycle cost.
The challenge loop's `6d8c` (`MAR *AR4-`) and `1c84` (`XOR *AR4,A`) now have
independent one-cycle assertions. The paired test also proves that `XOR`
reads the address selected by the preceding pointer decrement.
Exact-word fixtures also separate `108a` (`LD *AR2-,A`), `8084` (`STL A,*AR4`),
`1a8b` (`OR *AR3-,A`), and `e598` (`MVDD *AR3+,*AR2+`) from the aggregate
challenge response. They check signed loads, stores, source/destination
addressing, both pointer updates, and TI's one-cycle base costs.
The high-use `e742`/`e743` words are `MVMM AR4,AR2/AR3`, not `MVDK` as the
decoder comment formerly claimed. TI SPRU172C includes SP as the ninth
allowed register. The decoder now uses the full four-bit source/destination
fields and MMR accessors; fixtures cover those two ROM4 words and the SP forms
`e748`/`e782`. The 30-second ROM4 trace also dispatches `e782` 25 times.
Exact `e5c9` (`MVDD *AR2+0%,*AR3+`) now verifies a five-word circular
source wrap, independent linear destination increment, data copy, and TI's
one-cycle base cost. It was previously only executed in a broader fixture.
Exact `f0f8` (`SFTL A,-8`) now checks the logical low-32-bit shift, cleared
guard bits, carry from source bit 7, and one-cycle cost against TI SPRU172C.
Independent exact-word tests now also cover `1293` (`LDU *AR3+,A`) with SXM
set, `f0c8` (`XOR A<<8,A`) across the 40-bit guard field, and `e800`
(`LD #0,A`). Each checks its one-cycle cost rather than relying on the
combined startup/challenge sequence.
TI SPRU172C specifies five cycles for a taken `RC` and three when false.
The core previously charged only one for false conditions. A shared return
path now applies the documented costs to the decoded `RC` variants; exact
`fc30` (`RC TC`) fixtures verify both outcomes, stack behavior, and timing.
TI SPRU131G section 5.8 specifies that the addressed word of a 32-bit
operand is the most-significant word; the second word is at the next address
when even and the previous address when odd. DST/DLD formerly rounded every
address down, swapping the halves for odd addresses. The corrected handlers
have an exact ROM4 `4ef8` absolute-DST test for word order and three-cycle
cost, plus odd-address DST/DLD fixtures for the shared memory rule.
Exact ROM4 `fe00` (`RETD`) now checks that two one-word delay-slot instructions
retire before the saved return target, the stack pops once, and the return
itself has TI's three-cycle delayed-return cost.
ROM4 `1092` (`LD *AR2+,A`) now has an isolated signed-load, postincrement,
and one-cycle assertion. The existing 26-word `e59c` (`MVDD`) receive-ring
copy checks its copied words and final X/Y pointers and is now tagged as an
exact-word assertion rather than misclassified as executed-only.
Exact ROM4 `fc4b` (`RC BLT`) fixtures check the signed 40-bit B predicate
and TI's five/three-cycle taken/false costs. `fa45` (`BCD pmad,AEQ`) fixtures
check both outcomes, both delay words, and the three-cycle cost. The latter
found a shared-core defect: the false path had cost only one cycle because
the cycle charge was inside the taken branch; both paths now pay it.
Exact ROM4 `f947` (`CC pmad,ALEQ`) now checks the signed 40-bit predicate,
stack return address, and TI's five/three-cycle taken/false costs. Its shared
handler had charged four/one cycles. Exact `6e8f` (`BANZD pmad,*AR7-`) checks
that the branch uses the pre-decrement value, both delay words retire, and
both outcomes cost TI's two cycles; the handler had charged one.
ROM4 `f020` (`LD #lk,A`) now has an isolated negative-immediate fixture for
both SXM settings and TI's two-cycle cost. Existing result/timing checks for
`e802` as a RETD delay word and `f274` (`CALLD`) are now tagged as exact-word
assertions rather than left in the executed-only class.
ROM4 `8094` (`STL A,*AR4+`) now checks the low-word store, pointer update,
and one-cycle cost. Absolute arithmetic `00f8` (`ADD Smem,A`) and `09f8`
(`SUB Smem,B`) now have SXM-on/off, result, carry, and two-cycle assertions.
Exact `f0ff` (`SFTL A,-1,A`) checks the low-32-bit logical shift, outgoing
carry, and one-cycle cost.
The previously highest-frequency absent word, `ec05` (`RPT #5`, 188 observed
dispatches), now checks six executions of its one-cycle arithmetic body and
TI's one-cycle immediate-repeat setup.
Exact ROM4 `8091` (`STL A,*AR1+`, 160 observed dispatches) checks its AL
store, postincrement, and one-cycle cost. `7593` (`PORTW *AR3+,PA`, 128
dispatches) checks the transferred source word, postincrement, and TI's
two-cycle indirect port-write cost.
Exact ROM4 `fc20` (`RC NTC`, 127 dispatches) checks the three-cycle false
and five-cycle taken paths, including SP behavior. `1086` (`LD *AR6,A`, 120
dispatches) checks SXM sign extension, an unchanged AR6, and one-cycle cost.
Exact `fc45` (`RC AEQ`, 109 dispatches) checks both paths, their three- and
five-cycle costs, and a nonzero guard byte with a zero low word.
Exact `ff20` (`XC 2,NTC`, five dispatches) checks both TC states: the next
two one-word MAR instructions execute when TC is clear, or consume two NOP
slots without changing their ARs when TC is set. Both paths take five cycles
between the fixture's port markers.
The next observed words now checked are `e741` (`MVMM AR4,AR1`, source
preservation and one-cycle cost), `1181` (`LD *AR1,B`, SXM sign extension and
one-cycle cost), and `1a8a` (`OR *AR2-,A`, old-address read, decrement, and
one-cycle cost).
Exact `12d2` (`LDU *AR2+%,A`) now asserts unsigned extension under SXM,
post-read circular wrap, and one cycle. Exact `1cf8` (`XOR *(lk),A`) asserts
the absolute-address extension, unchanged status, and the two-cycle cost
specified by TI SPRU172C.
The `ORM #lk,Smem` family now has exact `6982` and `6984` checks for distinct
AR-selected addresses, unchanged pointers and status, and TI's two-cycle
immediate form. Exact `1b8d` (`OR *AR5-,B`) checks old-address read,
post-decrement, unchanged status, and one cycle.
Exact `4595` checks shifted signed load into B and AR5 post-increment, while
`4917` checks that LDM reads AR7 without SXM extension. Both cost one cycle.
Exact `6883` and `6983` check the complementary ANDM/ORM operations through
AR3 with immediate extension, unchanged pointer/status, and two-cycle cost.
The ROM4 stream at DSP PC `0x3c5e` contains exact `6fd2 0c45` (`LD
*AR2+%,5,A`). Its fixture checks SXM sign extension before the five-bit
left shift, AR2's post-read circular wrap, and TI's two-cycle cost for the
two-word shifted-load form.
Exact `7e8b` (`READA *AR3-`) checks the accumulator-addressed program read,
the old AR3 data destination, post-decrement, and TI's five-cycle cost.
Accumulator ALU exact-word checks now cover ROM4 `f1c0` (`XOR A,B`) preserving
the 40-bit guard and status, `f508` (`ADD A<<8,B`) updating only B, and
`f400` (`ADD A,A`) setting OVA across the signed 32-bit boundary. Each takes
one cycle, as specified in TI SPRU172C.
Exact `f028 00aa` checks the ROM4 two-word, two-cycle `LD #lk,8,A` stream.
Exact `f1b8` checks a logical right shift in `OR A>>8,B`, source preservation,
and unchanged status; `f468` checks one-cycle `SFTA A,8,A` and OVA when a
positive source crosses the signed 32-bit boundary.
Exact `3182` (`MPYA *AR2`) now checks a negative product from A's signed high
word, transfer of Smem to T, unchanged AR2, and one-cycle timing. Exact `0885`
(`SUB *AR5,A`) checks SXM sign extension without pointer motion in one cycle;
`1df8` (`XOR *(lk),B`) checks absolute-address extension, guard/status
preservation, and the two-cycle surcharge.
Exact `80d3`, `818d`, `81d2`, and `8395` now check low/high accumulator stores,
old-address writes before circular or linear AR updates, preserved accumulator
and status values, and one cycle per unshifted store.
Exact `8815`, `e552`, and `e754` now check low-accumulator storage into AR5,
an AR3-to-AR4 data move with source post-decrement, and the AR5-to-AR4 MMR
transfer. Their composed marker span checks one cycle per move.
Exact `f843` now checks `BC pmad,ALT` on both signs of A's 40-bit guard with
five-/three-cycle taken/rejected costs. Exact `ff30` checks `XC 2,TC` with two
executed MAR slots and two rejected NOP-equivalent slots at the same marker
span. Both conditions and costs match TI SPRU172C's control-flow tables.
Exact `1192`, `6dd2`, `7683`, and `8085` now check a sign-extending load through
AR2, circular MAR wrap, an immediate store through AR3, and accumulator-low
storage through AR5. A composed marker span checks their 1+1+2+1 cycle costs.
Exact `771d` is a direct `STM #lk,PMST`, not indirect AR5 addressing; its fixture
checks PMST publication, unchanged ARs/status, and two cycles. Exact `ff45`
(`XC 2,AEQ`) and `ff4e` (`XC 2,BGT`) each check accepted and rejected two-slot
paths, with BGT decided by B's 40-bit guard sign.
Exact `4812`, `4813`, `4814`, `4817`, and `4910` are LDM reads of AR2/3/4/7/0,
not stack operations. Interleaved STL stores preserve each A result for
independent checks; all five loads zero-extend despite SXM and cost one cycle.
Exact `f200` checks signed `ADD #lk,B,A`, source preservation, carry, and two
cycles. Exact `f2a0` checks 40-bit `OR B,A` without status changes; exact `f540`
checks unshifted `LD A,B`, including OVB when A is outside signed 32-bit range.
Both accumulator-to-accumulator forms cost one cycle.
Exact `10da`, `6d93`, `7308`, `7313`, and `7681` now check a circular load
through AR2 using AR0's step, a linear MAR update, two MMR-to-data moves, and
an immediate store. The marker span verifies their combined 1+1+2+2+2 cycles.
Exact `7718`, `7728`, and `7758` check direct `STM` writes to SP, page-zero
data, and CLKMD at two cycles each. A fixture-only `LDM CLKMD,A` reads back
the model's status bit; this is not a silicon PLL-lock timing assertion.
The separate 16-case CLKMD fixture exercises writes through original
`STM`, reads through `LDM` and captures through `STL`: once in PLL mode,
attempted changes to PLLMUL, PLLDIV, PLLCOUNT and PLLON/OFF preserve the
active configuration. PLLNDIV can still select DIV, after which new control
fields are accepted; software cannot assign PLLSTATUS. This follows
[SPRU131G table 8-18 and the mode-switch procedure](https://www.ti.com/lit/ug/spru131g/spru131g.pdf).
The old core fails the protected-field assertion (`tmp/c54x-clkmd-red.log`);
the corrected core passes all 16 settings and the full native suite
(`tmp/c54x-clkmd-green.log`). The native NSE-1 RF-boundary regression also
passes (`tmp/c54x-clkmd-rom4-rf.log`: 648,288 reads, 1,580 port-32 writes,
no port-38/39 reads). These tests establish register write eligibility,
not PLL lock latency, input-clock geometry or clock-rate switching. The
unconfigured core retains its declared immediate-status limitation. The
separate input-clock-configured fixtures now cover delayed status and selected
execution/peripheral rates, without asserting measured DA150 timing; see the
[MU4 clock contract](mu4_hardware.md#original-clkmd-contract).
Exact `8812`, `881a`, `8915`, and `891a` check direct `STLM` writes to AR2,
BRC, AR5, and BRC. An intervening `MVMD BRC,dmad` preserves A's first BRC
write before B overwrites it; the marker span checks four one-cycle stores and
one two-cycle MMR move.
Exact `e589` checks a dual-memory move through AR2+ and AR3+, with both words
addressed before the pointers advance. Exact `e734` then copies the advanced
AR3 into AR4. Both moves cost one cycle under TI SPRU172C.
Exact `ec06` checks `RPT #6` executing `MAR *AR1+` seven times after a
one-cycle setup. Exact `f84a` and `f84b` check `BC pmad,BGEQ/BLT` against
B's 40-bit guard sign, each with taken and rejected paths and TI SPRU172C's
five-/three-cycle costs.
Exact `fc47` checks `RC ALEQ` against A's 40-bit guard sign, including the
stack pop only on return and the five-/three-cycle costs. Exact `ff47` and
`ff4c` check `XC 2,ALEQ/BNEQ` with both accepted and rejected two-slot paths;
the guarded MAR effects and five-cycle marker spans agree with TI SPRU172C.
Exact `0882` checks an SXM-sign-extending indirect subtraction without AR2
motion. `1b84` and `1bf8` check indirect and absolute OR into B, preserving
its guard byte and status. `45f8` checks an absolute `LD Smem,16,B` with SXM
extension before shifting. Their marker spans cover TI SPRU172C's one-cycle
indirect forms and the extra word/cycle for absolute Smem.
Exact `4816`/`4913` check direct AR6/AR3 MMR loads into A/B, including zero
extension despite SXM and one-cycle costs. The AR6 case uses a non-mutating
port marker so the timing probe cannot advance the register before the load.
Exact `7215`/`7314`/`7315` check a data-to-AR5 move followed by AR4/AR5-to-data
moves; their preserved source values and eight-cycle marker span establish
three two-cycle transfers under TI SPRU172C. The ROM4 `4920` MMR-0x20 load
is tested separately below because its peripheral contents are product-owned.
Exact `8816`/`8817`/`8910`/`8911` check accumulator-low stores to AR6/AR7
and AR0/AR1. Exact `8821`/`4920` check the CPU-side transfer to MMR `0x21`
and from MMR `0x20` through test backing memory, including LDM zero extension.
The six one-cycle transfers have an eight-cycle marker span. TI's C54x MMR
map identifies `0x20/0x21` as peripheral space on related parts; this fixture
does not validate the Nokia product's peripheral semantics or external timing.
Exact `71da`, `7692`, `7c8b`, `7d82`, and `8182` form a composed transfer
fixture: MVDK reads the old AR2 word before an AR0-step circular wrap; ST
writes at the wrapped address before incrementing; MVPD copies program to
data at the old AR3 address; MVDP copies data back to a separate program
address; STL overwrites only the data word. The marker span checks TI
SPRU172C's 2+2+3+4+1 cycle costs and the direction and order of each move.
Exact `e762` checks a one-cycle MVMM AR6-to-AR2 transfer without changing
the source. A seven-word table checks `e80d`, `e820`, `e905`, `e906`, `e90c`,
`e90f`, and `e97c`: each one-cycle `LD #K` changes only its destination
accumulator and preserves status. Seven `RPT #K` words (`ec09`, `ec0a`,
`ec0f`, `ec11`, `ec13`, `ec1e`, `ec9f`) repeat MAR exactly K+1 times after
one setup cycle; the longest fixture executes 160 MAR iterations.
Exact `f6bd`, `f6bf`, `f7bd`, and `f7be` check one-cycle ST1 bit clear/set
operations while preserving ST0. Exact `fd30`, `fd4b`, and `fd4d` check both
accepted and rejected `XC 1` guarded slots, including 40-bit B sign tests and
the one-cycle conditional-execute cost. Exact `f1fc`, `f463`, `f470`, `f578`,
and `f763` check logical and arithmetic shifts, carry, sign/guard behavior,
and one-cycle costs. These checks follow TI SPRU172C; they do not establish
unobserved status-bit selectors or shift variants.
Exact-word fixtures now also cover `f830` (`BC pmad,TC`) taken/not-taken timing,
`f030` (`AND #lk,A`) zero-extended immediate and two-cycle cost, and `f073`
(`B pmad`) four-cycle cost. Absolute port transfers `75f8` and `74f8`
(about 13,000 executions each) now have exact-word assertions for their
three-cycle cost, extension-word order, and data movement. The highest-use
ROM4-only word was `f040` (`OR #lk,A`); it now has a zero-extension and
two-cycle fixture. `6082` (`CMPM *AR2,#lk`) exposed a missing cycle charge:
TI specifies two cycles, or three with absolute/long-offset addressing.
The exact-word fixture checks both TC outcomes, pointer preservation, and the
two-cycle form. `8093` (`STL A,*AR3+`) now checks the stored low word and
post-increment. Exact `f7bb`/`f6bb` fixtures cover setting and clearing
ST1.INTM; `4a08`/`8a08` cover an AL push/pop with stack-pointer restoration.
`7212` (`MVDM`) and `7312` (`MVMD`) now have exact-word data-direction and
two-cycle assertions. Their core handlers previously charged one cycle.
Repeated-move fixtures cover three successive data addresses in each direction
and TI's one-cycle pipeline rate after the first two-cycle move.
The formerly uncovered `4a`/`8a` MMR save/restore burst is now asserted
by the twelve-register fixture described above.
`6c8a` (`BANZ`) has taken/not-taken timing and pointer-update assertions; TI SPRU172C
specifies four and two cycles, respectively. Long-offset Smem access outside
`MAR` remains a separate
decoder audit; this correction does not establish those addressing forms.
The multiplier paths now set sticky `OVdst` on 32-bit overflow and clamp the
destination when `OVM` is set. A dedicated `MAC *AR3,A` fixture checks both
OVM states, preservation of carry, and TI SPRU131G's `FRCT`/`SMUL` example
(`0x7fffffff` versus `0x7ffffffe`). Exact `b03a` and `d6e1` fixtures also
assert saturated overflow in dual-memory and parallel-store MAC forms.
Other rounded boundaries and encoded variants remain unaudited; fixture
overlap does not claim those cases are complete.
The executable rounded-multiplier boundary matrix covers twenty-six explicit
MPYR/MACR/MASR vectors across A/B destinations, `*AR2`/absolute operands,
and clear/pre-existing destination overflow: 208 cases. It checks fractional
minimum-times-minimum multiplication, positive rounding across the 32-bit
boundary, negative subtraction overflow, OVM saturation, sticky destination
and opposite overflow, preserved carry/TC, T, address and opposite accumulator.
The arithmetic follows TI [SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)
MPY/MAC/MAS definitions. Six additional vectors apply TI
[SPRU131G](https://www.ti.com/lit/ug/spru131g/spru131g.pdf), section 4.5.3 and
PMST table 4-3: rounded MAC/MAS distinguish product saturation before
accumulation with SMUL clear/set, and SMUL has no effect when either OVM or
FRCT is clear. Eight MPYR vectors additionally check that SMUL does not alter
its result under the four OVM/FRCT combinations, negative exact-half rounding,
negative fractional rounding, positive below-half and positive exact-half
rounding. Each case initializes and preserves PMST explicitly. This
also covers positive MACR and negative MASR crossing the signed 40-bit guard
boundary, with OVM clear/set: unsaturated results wrap to the accumulator
width, while OVM saturates the mathematical result before truncation. These
four additional vectors preserve opposite accumulator/status, carry, TC, T,
PMST and addressing state across both destinations and addressing modes.
It does not cover every addressing form or SMUL combination, cycle cost or
Nokia RF sample format.
An additional 192-case literal-result control matrix compares Smem,
dual-memory and parallel-store MAC/MAS, rounded and unrounded, across all
eight FRCT/OVM/SMUL combinations and both destinations. It uses
`8000h * 8000h` with -1/+1 accumulation to distinguish pre-product
saturation, checks rounding-owned overflow separately, and preserves the
opposite accumulator/overflow, carry/TC, control registers and operands.
Pointer updates and the parallel store are checked too. This extends the
existing rounded Smem boundary tests to the other two families; it is not
exhaustive operand or pipeline coverage and requires no arithmetic change.
Removing the OVM guard from product saturation makes this matrix fail;
the restored core passes. TI limits SMUL to particular C54x revisions, so
these generic-core fixtures do not prove its presence on every MAD2 DSP.
PMST.SST now saturates the shifted 40-bit accumulator value to 32 bits before
ordinary `STH`, `STL`, `STLM`, and `DST` stores, without modifying the
accumulator. Focused fixtures reproduce TI SPRU131G's signed `STH` and
unsigned `DST` examples and preserve their one- and two-cycle costs. The
conditional `SACCD` store is different: TI [SPRU172C, page 4-152](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)
specifies ASM/SXM, and [SPRU131G, table 4-3](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
excludes it from the SST instruction list. It therefore stores the shifted
accumulator lane without SST saturation; negative ASM shifts obey SXM.
The core previously saturated this lane and always sign-extended right
shifts. In addition to the 576 existing cases, 96 literal-result boundary
fixtures cover guard-bit overflow, ASM -16/-1/0/15, both SXM/SST settings,
both sources, false-condition write-back, preserved accumulators/status and
unconditional pointer update. They failed before the correction and pass
after it; the full core suite and isolated ROM4 FR encoder/decoder comparisons
also pass. These are instruction results, not physical pipeline timing,
native RF acquisition or handset speech evidence.
ASM `STL/STH`, long-immediate `STL/STH`, and parallel stores also obey SXM
when shifting right, before applying SST. Their previous handlers always
sign-extended. A separate 120-case literal-result fixture covers shifts
-16/-1, negative guard-bit inputs, both SXM/SST settings and both sources
across these five forms (parallel `ST||LD` is the fifth). It checks stored
lanes, preserved accumulators/status and address updates. The fixtures fail
against the old handlers and pass with SXM-aware right shifts. This is not
exhaustive parallel-store or pipeline coverage. The
implemented parallel `ST||MAC[R]` form also saturates the pre-MAC store
source after its ASM shift. Exact `d6e1` fixtures check both zero and nonzero
ASM shifts, the old B value, unchanged B, A's MACR result, and the one-cycle
cost. The additional `ST||ADD/SUB/LD/MPY` tests below cover selected encodings,
not the entire parallel-store family. The opcode-coverage gate also ranks
untested static decoder matches
within high-byte groups observed on the 5110. The ranking discounts each
group's most-executed word so the `f495` NOP does not make every untested `f4`
variant look urgent. It is a prioritization list, not an implementation count:
the static mask scan does not evaluate nested validity checks or prove the
instruction's arithmetic, flags, addressing, or cycle cost. Exact-word
fixtures remain the evidence for those claims.
On the current 30-second ROM4 idle/Menu/power union, `f4` still leads after
discounting `f495`: 1,626,477 executions of its other observed words, versus
1,029,136 for `f0`. The result prompted an exact-word `f4` audit; it does not
make its 137 untested static matches valid instruction encodings.
The exact-word report resolves the apparent `f4` lead: `f491` ROL A accounts
for 1,572,960 of those remaining executions and already has carry/guard
assertions. In `f0`, `f050` and `f065` XOR-immediate forms account for
786,564 and 786,540 executions and are likewise asserted. Fixture-only
`f165` now checks the two-cycle, shifted-immediate XOR across accumulators:
it updates B from A while preserving A's guard and carry. The report does
not establish the other `f0` encodings or all XOR status variants.
Fixture-only `f43f` (`SUB A>>1,A`) now checks the accumulator arithmetic
family's right-shift fill under both SXM states: a negative 40-bit source
sign-fills when SXM is set and zero-fills when clear. Both cases assert TI
SPRU172C's one-cycle cost. This does not establish the remaining shift
amounts, source/destination combinations, or status variants.
Exact `47f8` (`RPT *(absolute)`) now checks the three executions of its
repeated instruction and the four-cycle setup cost specified by TI SPRU172C.
The `4782` indirect form checks the three-cycle base cost and unchanged AR2.
The core previously charged only its default single cycle to both forms.
Observed `7d92`/`7c92` program-memory moves now have exact-word direction,
pointer, and cycle assertions. TI SPRU172C gives `MVDP` four cycles and
`MVPD` three, with one additional cycle for absolute Smem; the `7df8`/`7cf8`
fixtures check that surcharge and extension-word order. A repeated `7c92`
fixture checks three distinct program source words, consecutive data addresses,
and one cycle per move after the first. Previously `MVPD` reread its initial
program word on every repeat iteration. These checks do not establish bus
wait-state timing or untested addressing variants.
Exact `7f92` (`WRITA *AR2+`) now checks the A-addressed program write, pointer
update, unchanged accumulator, and TI SPRU172C's five-cycle base cost. Exact
`7ef8` (`READA *(absolute)`) checks the reciprocal read and six-cycle
absolute cost. A repeated `7f92` fixture checks three consecutive program
writes and the documented one-cycle rate after the repeat pipeline starts.
The shared handlers previously charged only one cycle per transfer. Other
Smem addressing variants and external-memory wait states remain unaudited.
Immediate stores have exact `771a` (`STM #lk,BRC`), `76f8`
(`ST #lk,*(absolute)`), and `7682` (`ST #lk,*AR2`) result and cycle assertions.
TI SPRU172C specifies two cycles for the MMR and indirect Smem forms, and
three for absolute Smem. The core previously charged one cycle to all three.
The absolute fixture also checks address-before-immediate extension order.
Exact `4bf8`/`8bf8` stack fixtures now check an absolute-source push and
absolute-destination pop, SP round trip, and TI SPRU172C's two-cycle cost.
The core previously charged one cycle to both. Non-absolute forms retain the
documented one-cycle charge; long-offset addressing is still unaudited.
Exact `4a06`/`8a06` now check a one-cycle ST0 push/pop through the MMR
stack path. The saved word and SP round trip match TI SPRU172C; no core
change was needed. The ROM4 trace exercises long-offset `MAR`, covered by
exact `6dea`. The shared Smem read/write path now consumes MOD12-14 extension
words and applies no-update, signed preupdate, or circular preupdate as
specified in TI SPRU131G table 5-4. Fixture-only `06ea`, `02e2`, `02f2`,
and `80ea` check the three read modes and a long-offset store, including
the ALU/STL extra cycle. Handlers that compute their own memory addresses
still need separate long-offset review. Fixture-only `6bea` checks ADDM:
the address displacement precedes the immediate, preupdate reaches the
destination and the instruction costs three cycles. Original MU4 `6be6`
context-counter updates exercise the same ordering during erased-media mount.
Fixture-only `76ea` checks long-offset `ST #lk,Smem` with displacement before
immediate, preupdate and three-cycle cost; original `76e1` context setup
independently uses that encoding. The existing `68e2/69f2` ANDM/ORM cases
check addressing and timing, but their extension order still needs independent
machine-code corroboration rather than reliance on synthetic fixtures.
Other manual-address handlers remain open.
Fixture-only `82ea` and `83e2` assert the two-cycle long-offset surcharge
for `STH A/B,Smem` and the distinct preupdate/no-update addressing behavior.
Fixture-only `8cea` checks the same surcharge for `ST T,Smem`.
The direct `LD`/`LDU` accumulator handlers now charge that surcharge too.
An arithmetic decoder audit removed shadowed exact-case switch arms for
`f000`/`f300` ADD, `f010`/`f310`/`f210` SUB, several immediate AND/OR forms,
and `f0c8`/`f3c8` shifted XOR. The earlier generic ALU and shifted-XOR paths
already own these encodings; the dead arms omitted some flag or cycle handling
and could be mistaken for live behavior. Existing exact-word fixtures and the
cross-ROM gate establish that deleting them does not change execution.
Fixture-only `10ea`, `12e2`, and `45ea` distinguish sign extension, unsigned
loading, 16-bit shift, address update, and two-cycle long-offset timing.
The one-word Smem arithmetic/logical handlers (`ADD`, `ADDC`, `SUB`, `SUBS`,
`AND`, `OR`, `XOR`, and `SUBC`) now charge the extra cycle TI SPRU172C assigns
to absolute addressing. Exact `07f8`, `1af8`, and `08f8` fixtures check an
arithmetic carry input, a logical result, a signed subtraction result, and
their two-cycle costs. The core previously charged one cycle to these
absolute forms. The shared ALU operand path now handles long-offset forms,
but this does not establish every flag and accumulator variant in the family.
An indirect ROM4 sequence now asserts `0883` signed SUB under SXM,
`1d83`/`1c83` unextended XOR source words, and `1c93` postincrement against
two distinct memory operands. The four arithmetic operations take four
cycles in aggregate as specified by TI SPRU172C; `7713` changes the source
address between XORs and adds its separate two-cycle cost. Individual flag
variants and unexecuted addressing forms remain unaudited.
The Smem multiply/MAC handler now charges TI SPRU172C's extra cycle for an
absolute operand. Exact ROM4 `2494` checks an unsigned `MPYU` product,
postincrement, and one-cycle indirect cost. Fixture-only `24f8` checks the
same product and the two-cycle absolute form. ROM4 did not execute `24f8`
in this capture; this fixture tests a family rule, not observed boot traffic.
The `0x7100` handler is `MVDK Smem,dmad`, not `MVDM`; exact ROM4 `7192`
now checks source postincrement, destination data, and the two-cycle base
cost. `MVKD` now advances its immediate source address on repeated transfers;
exact `7093` checks three distinct words, consecutive data destinations, and
the one-cycle repeat rate after the first move. The existing exact `70f8`
handler and fixture continue to establish that absolute variant's extension
order and three-cycle cost; that special encoding is a separate contract.
Absolute one-word load/store forms now charge TI SPRU172C's extension cycle.
Exact ROM4 `44f8`, `80f8`, `82f8`, and `8cf8` fixtures check shifted load,
low/high accumulator stores, T-register store, and their two-cycle costs.
The correction covers the corresponding A/B shifted-load and STL/STH paths;
it does not claim generic long-offset Smem support.
Exact `71f8` now checks absolute-source `MVDK` address-before-destination
extension order, data movement, and TI's three-cycle cost. A repeated
`7192` fixture checks three distinct source and destination addresses and
the one-cycle rate after its first two-cycle move. Both passed on the
existing core; this was a coverage and contract check, not a behavior fix.
The live extended `0x6f` Smem decoder now charges TI's two-cycle indirect
and three-cycle absolute costs. Exact ROM4 `6f8a`/`6ff8` fixtures check
shifted-load data, pointer/extension consumption, and both costs. The later
`case 0x6f00` arm was unreachable behind the earlier unconditional decoder
and has been removed. Exact `6f82`/`6f83` fixtures check shifted ADD/SUB;
`6f92`/`6f93` check shifted low/high stores, indirect pointer updates, and
two-cycle indirect costs. Exact ROM4 `6f8b` additionally checks SXM extension,
postdecrement, extension consumption, and the two-cycle cost. Other shift, flag, and long-offset variants remain
unasserted.
TI SPRU307A's opcode table and the locally retained SPRU172C page 4-80
assign `LMS Xmem,Ymem` to `E1xx`, one word and one cycle.
Atomic fixture `e13a` checks TI's LMS
example values for A and B, unchanged T, dual-address update, and one-cycle
cost. No observed 5110 trace executes this family, so this is instruction
coverage, not evidence about the ROM4 boot path.
TI SPRU307A assigns `MACSU` to `A6xx/A7xx`, one word and one cycle.
Fixture-only `a6ab` reproduces TI SPRU172C's unsigned-X positive-product
example; `a7ab` additionally checks signed-negative Y, FRCT doubling, the B
destination, unchanged A, T update, and both pointer increments. Neither
encoding appeared in the observed 5110 trace union.
The `78xx/79xx` MACP and `7axx/7bxx` MACD program-memory multiply families
now have TI SPRU172C example fixtures (`788b` and `7a8b`). Both check the
three-cycle base cost; MACD additionally copies the source word to its
successor, while MACP leaves that word alone. A repeated `7893` fixture
checks consecutive program coefficients, T and AR3 updates, and the
one-cycle rate after the first multiply. These are fixture-only encodings;
other addressing and status variants remain unasserted.
Fixture-only `798b` and `7b8b` extend those assertions to destination B:
they check A preservation, B's signed product, T publication, AR3 decrement,
the MACP/MACD successor distinction, and the same three-cycle base cost.
The `79` and `7b` decoder groups therefore no longer have zero fixture words;
neither group is observed in the current ROM4 trace union.
Fixture-only `79f8`/`7bf8` check absolute Smem address before program
coefficient, while `79ea`/`7bea` check the opposite long-offset extension
order, AR2 preupdate, and the successor write only for MACD. All four check
TI SPRU172C's four-cycle cost for a three-word form. These tests do not
establish external-memory wait states or every repeat/address combination.
TI SPRU307A assigns `FIRS` to `E0xx`, with a three-cycle first pass and
one-cycle repeat rate. As with LMS, the detailed SPRU172C opcode diagram
conflicts with the opcode table and live `B0xx` MAC encodings; the decoder
uses `E0xx`. Fixture-only `e03a` checks TI's X/Y sum into A, previous-A
coefficient multiply into B, unchanged T, pointer update, and three-cycle
cost. A repeated case checks coefficient progression, use of the previous
A value, and one-cycle steady rate. The observed 5110 trace union does not
execute FIRS; other status and addressing variants remain unasserted.
The dual-memory `A0xx/A1xx` ADD and `A2xx/A3xx` SUB families now have
fixtures for both accumulator destinations, shifted X/Y results, pointer
updates, and TI's one-cycle cost. `E3xx` ABDST reproduces TI SPRU172C's
absolute-distance example and a separate SXM-negative/FRCT case. These are
fixture-only families in the current 5110 trace union; overflow and shared-AR
variants still need separate evidence.
TI SPRU307A assigns `LTD Smem` to `4Cxx`; `4c83` now checks the SPRU172C
source/T/successor example and one-cycle cost. `4c93` checks postincrement
after the successor write. These fixture-only encodings guard the standalone
delay primitive used by program-memory MACD. Fixture-only `4cf8` and `4cea`
also check absolute and long-offset addressing: T receives the selected
word, its successor receives the same word, AR2 preupdates only for the
long-offset form, and both cost two cycles. Other addressing and bus
wait-state combinations remain unasserted.
TI SPRU131G defines `Smem` bit 7 clear as direct addressing: with
`ST1.CPL=0`, `ST0.DP` supplies the page and the operand supplies a seven-bit
offset; with `CPL=1`, the address is `SP+offset`. The core formerly treated
those operands as AR-indirect. Fixtures now check DP-relative load/store and
`ADDM`, SP-relative load/store and `LTD`, their cycle costs, and unchanged ARs.
The hand-written delay, long-word arithmetic, read-modify-write, MACP/MACD,
and program/data move paths share that direct-address calculation. Existing
AR-increment fixtures were corrected to use bit-7-set encodings. DP/CPL
write-pipeline timing and external-memory wait states remain unmodeled.
Fixture-only `2045` and `2945` check the arithmetic side of the same boundary:
DP-relative `MPY` produces a signed product, SP-relative `MAC` accumulates a
negative product into B, both preserve AR5, and both cost one cycle. They do
not cover every multiplier status or overflow variant.
Fixture-only `26ea` checks TI SPRU172C's two-cycle long-offset `SQUR` form:
the signed source squares into A, T receives that source, and AR2 preupdates
by the extension offset. Earlier `2682` and `f58d` fixtures already assert
FRCT/OVM saturation for short Smem and accumulator-source SQUR respectively;
`3882` asserts SQURA's carry preservation. Those assertions do not establish
every status and source/destination combination.
TI SPRU172C's direct-address examples for `SQUR 30,B`, `SQURA 30,B`, and
`SQURS 9,A` now have exact fixtures (`271e`, `391e`, `3a09`). They check DP
selection, the documented accumulator and T results, and the one-cycle
short-operand cost. FRCT, OVM, and other source/destination variants are not
inferred from these examples; the separate status fixtures above supply the
specific FRCT/OVM evidence.
Fixture-only `0605` and `0e05` check direct-page carry arithmetic under SXM:
`ADDC` zero-extends `ffffh` before adding incoming C, while TI SPRU172C's
`SUBB 5,A` example subtracts the inverse of C and leaves the 40-bit result
at `ff ffffffff`. Both assert outgoing C and the one-cycle short-Smem cost.
Fixture-only `6b94` reproduces TI SPRU172C's saturated `ADDM` example:
`8007h + fff8h` stores `8000h` under SXM/OVM, sets OVA, advances AR4, and
charges two cycles. The fixture does not establish every RMW flag combination.
The TI SPRU172C opcode diagrams were checked from the PDF page image, not its
scrambled extracted text: `e5xx` is `MVDD`, while `ccxx..cfxx` is the distinct
`ST src,Ymem || MPY Xmem,dst` family. The core now decodes that parallel
multiply. Fixture-only `cdb9` reproduces TI's FRCT example (old A stored,
`4000h * 4000h` into B, both ARs advanced, one cycle); `cfb9` checks that
a same-register store uses old B before the multiply replaces B. These tests
do not establish the other parallel store arithmetic families or all ASM/SST
and overflow variants.
TI's adjacent opcode diagrams also assign `c0xx..c3xx` to `ST||ADD`,
`c4xx..c7xx` to `ST||SUB`, `c8xx..cbxx` to `ST||LD` into an accumulator, and
`e4xx/e6xx` to `ST||LD` into T. The core now implements those forms through a
shared pre-operation store path. Fixture-only `c131` checks the opposite
accumulator as the ADD source; `c5f5` reproduces TI's signed `ST||SUB` result,
ASM-shifted store, and circular pointer update; `ca31` checks signed load into
A; `e411` checks that Xmem is read before the same cell is overwritten as
Ymem. Each asserts the one-cycle DARAM cost. Store-side carry and hardware
memory-bank wait states remain outside these fixtures. A further `c131` OVM
fixture checks saturated B, sticky OVB, cleared carry, and the unaffected
pre-arithmetic store. A `c531` OVM fixture checks negative saturation, sticky
OVB, set carry, and the opposite subtraction order (`Xmem<<16 - A`).
The TI SPRU172C `ADD`, `SUB`, and `LD` diagrams also assign `04xx/05xx`,
`0cxx/0dxx`, and `14xx/15xx` to the single-memory forms shifted by TS, the
signed shift quantity in T's low six bits. These groups were absent from the
decoder. The core now applies SXM to the memory source, shifts by TS, and
charges one DARAM cycle (plus the existing long-offset/absolute surcharge).
Fixture-only `0483` and `0d83` check positive TS with ADD/SUB carry behavior;
`1583` reproduces TI's signed `LD *AR1,TS,B` example using `fedc` shifted by
eight; `1483` checks arithmetic right shift for TS=-1. Exact `0583` additionally
checks B-destination ADD with SXM, TS=-1 encoded in `T=abff`, unchanged A,
T and AR3, and one-cycle arithmetic. This guards sign fill and ignoring T's
upper ten bits independently of the load variant. Exact `0c83` checks the
complementary A-destination SUB with SXM clear and the same negative TS:
zero-filled memory, no-borrow carry, unchanged B/T/AR3 and one-cycle arithmetic.
These fixtures do not establish out-of-range TS values or memory wait states.
SPRU172C's adjacent `ADD/SUB Xmem,SHFT` diagrams assign `90xx/91xx` and
`92xx/93xx` to the single-X dual-address forms. The core now reads Xmem,
applies SXM and the four-bit left shift, updates the selected accumulator and
its X pointer, and charges one DARAM cycle. Fixture-only `9083` checks an
X-pointer postincrement and positive ADD; `9304` checks signed SUB, borrow,
unchanged other accumulator, and the unmodified-pointer form. Neither opcode
appears in the observed 5110 trace union; other X modes and overflow cases
remain unasserted.
TI SPRU172C also assigns `f486/f586` to `MAX A/B` and `f487/f587` to
`MIN A/B`, each one word and one cycle. The core now compares signed 40-bit
accumulators, copies the chosen value to the destination, and clears C when
A wins or sets C when B wins. In a MIN tie, TI's example chooses B. Fixtures
assert both destinations, negative values, tie selection, guard-byte
comparison, C, and one-cycle timing. These four words are not in the current
5110 trace union.
TI SPRU172C's `EXP` (`f48e/f58e`) counts redundant sign bits in the full
40-bit accumulator, subtracts eight, and writes the signed exponent to T;
zero writes T=0. `NORM` (`f48f/f58f/f68f/f78f`) shifts the selected source
by T's low-six-bit signed TS and writes the selected destination, applying
SXM and OVM. Both are one-cycle instructions. Fixtures reproduce TI's EXP
examples for -53 (T=25) and a positive guard-byte value (T=-4), and its
NORM examples for A shifted left by 19 and B shifted right by seven into A.
Additional cases assert EXP zero, NORM saturation/overflow, carry preservation,
and timing. Neither family occurs in the observed 5110 trace union; TS values
outside TI's -16..31 range remain unasserted.
TI SPRU172C assigns `RND` to `f49f/f59f/f69f/f79f`: it adds `8000h`
to the selected source, clamps on OVM, writes the destination, and does not
change status flags. `SAT A/B` (`f483/f583`) instead clamps the signed
40-bit source to 32 bits regardless of OVM and sets or clears the destination
overflow bit according to whether clamping occurred. Both cost one cycle.
Fixtures reproduce TI's negative-source and OVM RND examples and all three
SAT examples (positive overflow, negative overflow, and in-range); they also
assert carry preservation and the unaffected other accumulator. These are
fixture-only words in the current 5110 trace union.
TI SPRU172C's multiply table assigns `62xx/63xx` to `MPY Smem,#lk,A/B`
and `f066/f166` to `MPY #lk,A/B`. Both use signed 16-bit operands and FRCT,
and the Smem form publishes its memory operand to T. They cost two DARAM
cycles, plus one for long-offset or absolute Smem. Fixture-only `6283`
checks FRCT, T, and two-cycle timing; `f166` reproduces TI's signed
`MPY #fffe,B` example and preserves T; `62f8` checks address-before-immediate
extension order and the absolute three-cycle cost. `629b` is a write-only
short-preincrement source, covered by the CPU-only `source-domain` census,
not a valid MPY timing fixture or a second legacy rejection probe.
Long-offset MOD=13 remains read-capable; its retained
memory/immediate coverage is described above. Other addressing and multiplier
saturation variants require their own acceptance scope.
SPRU172C specifies A(32–16) as a signed **17-bit** multiplicand. The existing
`MPYA/MACA/MASA` Smem path had narrowed it to `s16`, reversing the product's
sign when bit 31 was set but bit 32 was clear. A shared 17-bit extraction now
serves those forms, FIRS, POLY, ABDST, SQDST, and accumulator-source SQUR.
The T-source `MPYA` (`f48c/f58c`) and `MACA[R]/MASA[R]`
(`f488..f78b`) forms are now decoded at TI's one-cycle cost. Fixtures
`f58c`, `3183`, and `f488` distinguish the 17-bit result from the old 16-bit
result; `f48b` checks rounded subtraction, while `e211` and `f48d` check
the shared high-word rule for SQDST and SQUR.
The SQDST B accumulation now uses the shared multiplier/accumulator overflow
path instead of an unchecked 40-bit assignment. Eight executable `e201`
fixtures pin old A-high=32768, FRCT=1, B=0/-1, OVM clear/set and pre-existing
OVB clear/set: B=0 produces `0x80000000` with sticky OVB, or `0x7fffffff`
under OVM; adding B=-1 produces the in-range `0x7fffffff` without clearing
an existing OVB. They preserve OVA, T and both operand addresses. The new
fixtures fail before this correction and pass afterward. The rule follows
[TI SPRU172C, SQDST, 4-160](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)
and the existing accumulator saturation contract.
The parallel difference now uses SXM-controlled memory extension and the
shared ALU subtraction path, rather than unconditional signed-16 narrowing
and a raw accumulator assignment. Thirty-two `e201` fixtures use four
high-bit X/Y pairs, SXM clear/set, OVM clear/set and pre-existing OVA
clear/set. Explicit signed/unsigned differences assert carry/borrow, sticky
OVA and both signs of saturation; B still receives the square of old A,
and its OVB remains unchanged. This follows SQDST's status list and
[TI SPRU131G, ST0/ST1, 4-3/4-5](https://www.ti.com/lit/ug/spru131g/spru131g.pdf).
The earlier signed-input fixture now explicitly sets SXM instead of inheriting
SXM=0 from its preceding multiply test; its expected signed value is unchanged.
These cases fix SMUL=0 and C16=0; SMUL saturation variants, C16 behavior,
physical pipeline timing and native speech remain unestablished.
FIRS now passes its two SXM-extended, shifted memory operands separately to
the ALU addition helper. Adding them first and passing `0 + sum` preserved
the result but lost carry from the original operands. Thirty-two executable
`e001` fixtures cover four high-bit X/Y pairs, SXM and OVM clear/set, and
pre-existing OVA clear/set. They assert carry, sticky OVA, signed saturation,
unchanged T and operand addresses, and B accumulation from old A. These
fixtures fail on the previous implementation and pass on the corrected core,
following [TI SPRU172C, FIRS, 4-59](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)
and the ST0/ST1 rules above. This selected coverage fixes FRCT=0, SMUL=0
and C16=0; it establishes neither physical pipeline timing nor RF acquisition.
ABDST has sixteen additional executable `e301` cases for old A-high=-65536,
B=`0x7fffffff`/-1, FRCT and OVM clear/set, and pre-existing OVB clear/set.
They verify the absolute signed-17 distance, its documented FRCT doubling,
B overflow/saturation and sticky OVB, while the parallel zero difference
preserves OVA, T and operand addresses and sets subtraction carry. All pass
without changing the implementation. The reference is
[TI SPRU172C, ABDST, 4-2](https://www.ti.com/lit/ug/spru172c/spru172c.pdf);
these cases fix SXM=1 and C16=0 and do not establish other addressing modes
or physical pipeline timing.
The core, bounded mixed RF filter, 5110
Menu/save-state and short/long physical power gates pass on the corrected
build. Remaining FRCT/OVM and source/destination combinations are not
established by these selected fixtures.
TI SPRU172C's `ADD/SUB src,ASM,dst` diagrams assign `f480..f780` and
`f481..f781` to the one-word, one-cycle accumulator forms. They were absent
from the decoder despite the corresponding immediate-SHIFT forms already
working. The core now shifts the selected source by ST1.ASM with SXM-controlled
right fill, then performs the signed addition/subtraction into the selected
destination. Fixture-only `f480` checks negative ASM, sign fill and carry;
`f681` checks B-to-A subtraction, unchanged B and borrow. Additional selected
overflow and shift/fill combinations are covered below.
An additional exact `f480` vector checks ASM=3: A=1 becomes 9, B's full
40-bit sentinel remains unchanged, carry stays clear, and the instruction
retains its one-cycle cost. The native `tms54test` suite passes after this
extension (`run_c54x_asm_core_retry_20261009.log`). This asserts one positive
left-shift outcome, not all ASM values, saturation, Nokia RF behavior or
native speech. The suite deliberately exits via its completion exception
after printing `TMS320C54x core conformance: PASS`.
`check-c54x-core` uses `tools/run_c54x_core.py` to retain independent
`run_c54x_core_*` directories containing `console.log` and the opcode
assertions in `error.log`. Acceptance requires every current marker in
`run_c54x_core.REQUIRED`, exit status3 and the terminal test-completion
exception; a marker followed by an unrelated crash or interruption is not
success. Earlier smaller marker sets are not current full-core acceptance.
Four additional ASM=0 `f480` vectors cover positive and negative 32-bit
overflow with OVM clear/set: guard-preserving 40-bit results versus signed
32-bit saturation, A-owned sticky overflow, low-word carry, unchanged B and
one-cycle cost. `check-c54x-core` requires their dedicated pass marker.
The complete native suite reproduces in
`run_c54x_asm_overflow_core_20261009.log`; these selected outcomes do not
establish other accumulator selections, ASM values or speech behavior.
Four exact ASM=0 `f581` (`SUB A,ASM,B`) vectors independently cover positive
and negative overflow with OVM clear/set, B-owned sticky overflow,
borrow/carry, unchanged source A and one-cycle cost. The dedicated subtraction
marker and whole-core conformance pass in
`run_c54x_asm_sub_core_20261009.log`. ADD's A-owned result is not substituted
for this cross-accumulator test. Nonzero subtraction shifts, all other
operand/status combinations and native RF/speech remain outside this claim.
Systematic `f580/f581` (`ADD/SUB A,ASM,B`) fixtures cover all 32 signed
ASM values with SXM and OVM clear/set and initial OVA/OVB clear/set,
for 512 selected vectors. Source A is fixed
at -65537 and destination B at 17, so negative right shifts distinguish
sign fill from zero fill and positive shifts include 32-bit overflow.
Expected signed right-shift values use explicit division/rounding rather
than the host's negative shift behavior. Each vector checks unchanged A
and ST1, the guard-preserving or saturated B result, sticky overflow,
low-word carry/borrow and one-cycle cost. The reference is SPRU172C's
`ADD/SUB src,ASM[,dst]` contract and SPRU131G's ST0/ST1 arithmetic rules.
The dedicated family marker and whole-core conformance pass in
`tmp/c54x-asm-saturation-matrix.log`; no core implementation change is needed.
This closes selected shift/fill/saturation coverage for these operands and
encodings, not other accumulator selections, all operands, RF acquisition
or speech. Initial OVA and OVB are varied together, not independently.
Re-run
`make check-c54x-opcode-coverage LOG=<rom4-log> ROM4_IDLE=1` to check the
opcode-set fingerprint; add `FIXTURE_LOG=<core-log> GROUPS=1` to rank
unasserted words and high-byte groups by observed execution count.
`make check-c54x-cross-rom`
runs the 3210 frontier, 5110 menu, and 3310/3330/3410 idle gates in order.

The retained NSE-1 trace fixes reset polarity and edge behavior without an
inference: MCU writes to MAD2 byte `0x20002` are `1` (cold release), `0`
(hold), `0`, then `1` (warm release). A later value `3` leaves the DSP running.
MAD2 now exports this product-mask-derived level to the backend; the C54x
implementation drives the CPU reset input so CPU state restarts while the
external DSPIF/on-chip DARAM stores survive the warm transition.
