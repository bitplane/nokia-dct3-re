# ROM4 execution and timing observations

Address-level native-loader execution, staged/atomic comparisons and bus/timing evidence. [C54x execution contracts](c54x_execution_contracts.md) owns current processor qualification; [the ROM4 loader](rom4_dsp_loader.md) owns handset lifecycle. Older bounded captures are not renewed by a later focused test.

## ST1 memory-access constraints

[SPRU131G table 4-2](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
requires bit 10 to read zero and prohibits memory writes from setting
INTM. The shared CPU register map enforces both: full/partial writes cannot
cause an INTM 0-to-1 transition, and reserved-bit writes/imports cannot
produce bit-10 readback. Data-bus taps still see the requested write word;
register readback applies the hardware constraints. Debugger setup and
save-state restoration can seed architectural INTM independently of the
firmware memory-write path. Dedicated SSBX/RSBX and interrupt-entry/return
operations retain their explicit INTM behavior.

The passage does not explicitly resolve whether a memory write of zero
may clear INTM. That behavior is retained from the legacy model and is
**unverified**, not promoted to a silicon contract. No C55x compatibility
rule or another emulator's implementation settles this C54x question.
Staged STLM A/B to ST1 uses this same constrained register map.

`st1-rules` exhausts 65,536 input words, both old INTM states and four
write masks (524,288 cases), checking readback, other fields, XF and one
save-state restoration. Its marker labels the retained clear policy
`legacy_unverified`. `st1-bits` separately checks 128 executable status-bit
cases plus replay: SSBX still sets INTM, and bit 10 remains zero. The
576-case MVMD IRQ/NMI fixture covers four **requested** INTM pairs, not
four effective transitions; write-one cannot mask an already unmasked CPU.
These gates establish the declared register model, not independent silicon
measurement of the unresolved clearing behavior.

The DST regression uses PMST.SST and ST1.SXM for signed saturation; ST1
bit 10 is not a saturation control. Its overflow inputs have low words
different from the clamped result, so a disabled clamp cannot satisfy the
data oracle accidentally.

## Staged immediate status publication

`STM #literal,ST1` retains its second word in Decode and publishes once at
Read, without reading the destination or applying SST saturation. The
`stm-st1` fixture passes 18,432 cases and cleared-pipeline replays over raw
status, SXM loads and ASM load/add/subtract consumers, four gaps, two pages
and three checkpoints. Poisoning the consumed literal cannot change the
publication. Reserved-bit, XF and INTM handling uses the existing register
contract; zero-write INTM clearing remains explicitly unverified.

`stm-cpl` and `stm-cmpt` each pass sixteen cases plus replay for basic LD/ST
followers: two directions and gaps 0/1/2/3. They select the new addressing
mode after two separating instructions, matching SPRU131G tables 7-16/7-19.
The tested SXM/ASM followers need no separating instruction. Sixteen negative
cases plus replay reject delay, short-repeat, block-body and XC compositions
before data effects. Other status-producer hazards, interrupt composition
and physical bus-stage timing are not qualified by these fixtures.

## Staged accumulator-to-status publication

STLM A/B to ST1 publishes the Execute-live, SST-qualified low word once
through the data bus, independent of DP/CPL, without reading the destination.
The `stlm-st1` fixture checks 18,432 cases with replay: both sources, raw
status words and negative-source saturation, SXM loads, ASM register
loads/add/subtract, long-offset SXM loads, four gaps, two program pages and
three save points.
It mutates the source before Execute to distinguish live publication from
an incorrectly retained Decode operand.

SPRU131G tables 7-20/7-22 require one separating instruction for short
SXM/ASM consumers. Their Read-owned mode sample is retained across Execute
and save/load; the long-offset load supplies its own separation. Dedicated
ASM loads still forward with zero latency. This qualification covers the
tested load/register-ASM families, not every arithmetic/multiply consumer.
The `stlm-cpl` and `stlm-cmpt` profiles each check 16 cases with replay:
basic load/store consumers retain the old addressing mode with zero to two
NOPs and use the new mode with three (tables 7-16/7-19).

`stlm-st1-irq` checks 576 cases with replay, using B, a separating NOP,
IRQ/NMI, all requested INTM pairs, IMR enabled/disabled, two follower forms,
three arrival clocks, two pages and three save points. It requires one
status publication, no source-memory read, at most one interrupt frame
and exactly one foreground marker. It does not inherit the special
SSBX/RSBX INTM guard or settle the unverified clearing policy.

Delay/repeat/XC and adjacent RPTB/RPTBD status-store compositions remain
implementation boundaries, not claims that silicon forbids them. The
adjacent-repeat case is a documented BRAF overwrite hazard (7.5.7.1), not
a safe composition or an emulated hardware trap.

## Shifted peripheral operand reads

The staged `LD Smem, SHIFT, A/B` path admits peripheral sources
`0020..005f` through the ordinary data bus. It retains the computed address,
address/operation words and source operand, paying the existing one-clock
peripheral hold before Read and publishing in the following Execute clock.
This follows the peripheral-access cost in classes 4A/4B, pages 3-7/3-8
of [SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf), not an
independently measured placement of the peripheral bus cycle.

`shift-periph` checks 245,760 cases with replay: all 64 peripheral addresses,
shifts -16/-1/4/15, DP/SP/absolute/postincrement/long-offset addressing,
SXM/OVM, A/B, four values, two pages and three snapshots. These are volatile
synthetic endpoints, not assertions about the identity of every mapped
register. Literals are poisoned after Decode and postincrement's pointer
is changed after Access; the original address and word must still be used.

`shift-bsp` checks 40,960 cases with replay against the existing completed-word
BSP state, using all 32 shifts and an additional post-Execute snapshot.
It starts with receive-ready and overrun, requires exactly one read to clear
both, then supplies a second frame which must remain pending after Execute
and replay. Debugger peeks cannot acknowledge either frame. This validates
CPU/register composition, not a physical serial clock or handset speech.

Mapped shifted ADD/SUB remain outside this admission, as do ST1/BRC sources
and the tested repeat/block/XC/delay compositions. The 64-case
`shift-per-edge` fixture requires rejection before data-side effects and
reproduces it across save/load. These are implementation boundaries, not
claims that real silicon prohibits those instructions.

## Repeat endpoint and page hazard

The C54x repeat-end comparator is 16-bit, not XPC-qualified. An active
loop can be consumed by code on another program page if its low PC reaches
REA; an RSA reload retains that code's current XPC. TI documents this
extended-addressing hazard in [SPRA599A, section 2, page 4](https://www.ti.com/lit/an/spra599a/spra599a.pdf).
Do not make a repeat loop page-private to protect a caller or ISR. The
instruction-reference restriction on far calls inside repeat blocks does
not specify a page-qualified comparator or a hardware trap, and does not
justify importing the C55x compatibility mode's automatic BRAF clearing.

The `rptb-page` fixture checks 384 cases with replay: both atomic and staged
frontends, BRC 0/1/2/7, caller pages 0/2/127, foreign-page REA collisions
and safe controls, FCALL/FCALLD, and four save points. In the colliding case, a foreign
callee consumes the remaining loop count and repeats at its own page's
RSA. Caller iteration counts, foreign prefix/endpoint counts, BRC/BRAF,
SP, return PC/page, ordered two-word frames and once-only delay effects
are checked independently.
The former staged page qualifier suppresses this result; the atomic
frontend already follows the 16-bit comparator. This is a manual-derived
regression, not a silicon timing measurement or permission to admit every
far-call/repeat composition.

```sh
python3 tools/run_c54x_core.py --profile rptb-page \
  --mame mame/mame --rompath roms --output-root tmp
```

The focused repeat/control profiles, native 5110 save-state gate and
MU4 cold saved-track catalogue gate pass. Delayed far-call/loop composition
has separate `rptb-fcalld` acceptance; none of these is full
[REL playback acceptance](mu4_playback.md#saved-track-playback-boundary).
The reference PDF is retained locally at
`roms/reference-docs/c54x/ti_tms320c54x_extended_addressing_spra599a.pdf`,
SHA-256 `fbbe43aa06609af2491e71213b19145f11db3d05169939daae5358efcea52432`.

## Extended program overlay

The staged frontend retains the physical program address selected by each
Prefetch slot in save state, separately from its logical PC. The isolated
`pmst-map`, `pmst-pop` and `pmst-prefetch` fixtures each pass 64 cases with
replay. The last deliberately changes debugger PMST state between Prefetch
and Fetch; it is CPU conformance, not a firmware-ordering fixture. The full
288-marker core run and native 5110 save-state gate pass. These results do
not certify all PMST fields or full interrupt composition. The `pmst-iptr`
profile separately passes 1,024 cases/restores across all 16 maskable IRQ
sources, early/late pending CPU latches, four PMST values and two source
pages. IPTR is selected at ISR Prefetch after decoded foreground writes,
while the accepted interrupt number is retained. NMI, ISR return and a
second OVLY change before vector Fetch are outside this fixture. Inactive
repeat-control restoration now passes 192 replay cases plus three active-loop
exclusions. POPM BRC followed by one NOP and RPTB passes 30 replay cases,
with independently checked count-plus-one output; active-loop writes remain
excluded. The staged SP-ordering correction lets FRETE at
`02:8b76` resume at the populated `03:9b08`; interrupt Access reserves
its slot after older foreground Read-owned stack updates. The focused
`stack-irq` gate passes 48 cases/restores, including IRQ0/NMI and three
snapshots; `stack-irq-return` passes 80 replays, adding RETE, caller cleanup
and snapshots of the retained return operand and returned caller. The
original readiness gate reaches 1. The mapped-ST1 LDU at `03:8f13` now
passes through the existing retained Read path, with 288 focused replay
cases and the existing 384-case IMR control. Short CMPM at `03:91eb` is
now admitted for direct RAM through the existing retained
operand path. Its 384-case DP/SP branch/replay fixture and 96-case absolute
control pass; mapped/peripheral and indirect comparisons remain excluded.
Immediate TIM/PRD/TCR writes now use retained literals and one peripheral
wait before Execute publication. The `timer-stm` profile passes 36 replay
cases checking stopped-timer register effects, writable masks and TRB reload;
it does not claim new running-timer timing validation. Original R060 now
executes conditional return `fc43` at `02:3e44` after the full signed
accumulator predicate family gained 768-case replay coverage. Repeated PSHD
at `02:3f7e`, EXP B (`f58e`) at `02:3fa8` and ADDS at `02:3fad` also
execute. Syntax-5 LD Xmem at `02:3fbc` and CALA B at `02:40e1` also
execute. Repeated MVDD (`e598`) at `02:a169` and absolute OR
(`1af8 b582`) at `03:abca` now execute too, as do both repeated POPD
restoration loops at `02:4062`/`02:4066`. Mapped AR/BK writes retain their
Read-stage ownership, including older-store conflicts. BACC A (`f4e2`) at
`03:9de4` now retains the accumulator target after READA's mapped write,
without allocating a call frame. Long-offset AND (`18e2 1462`) at
`03:9dff` now executes through retained address and source stages. Its
294,912-case replay fixture covers AND/OR/XOR, both accumulators, all
24 offset/preincrement/circular encodings, CMPT selection and BK=0/13.
Mapped/peripheral sources, repeats and control-delay composition remain
fail-closed, with 24 exclusion cases. Mapped MVMD (`730e 0011`, T to AR1)
at `03:9ba2` now executes too. T/TRN sources use the existing retained
move path; 432 replay cases cover all eight AR destinations and BK.
Producer/interrupt composition is outside that fixture. MVDD (`e501`) at
`03:9c70` now sends RAM `14b2=007f` to the bench's McBSP2 TX register
`0033`. RAM-to-peripheral transfers retain their operands and pay the
Class-14 extra cycle; 1,536 replay cases and ordinary/repeated RAM controls
pass. The original firmware emits the pin-decoded acknowledgement
`007f,0001,0055`. ABS A,A (`f485`) at `02:e5e5` and ABS B,B (`f785`)
at `02:e5e7` now execute, with 13,824 arithmetic/status/producer replay
cases and an unchanged NEG control. MPY (`62f8 0008 012c`, mapped AL
times immediate 300) at `03:95b5` now executes through retained
memory/immediate operands, with Read-owned T publication and 98,304 legal
replay cases; 256 additional replay cases reject write-only short MOD=3
in both execution frontends. The original recorder routine at `02:9e90` is entered and
output words `0580,0a09` are observed. Forward near B/BC inside
`02:a0ea..02:a0fd` now executes, with 12,288 IRQ/NMI/replay cases and
32 exclusion cases. Software INTR 25 (`f7d9`) at `02:9c74` now
executes, as does unsigned load `1281` at `02:9edc`. Its source `002a`
is backed by bench RAM, not an identified DA150 register. Same-location
STL/STH/T/TRN-to-load bypass is now covered by 288 replay cases, avoiding
stale stack locals and the resulting IMR corruption/default-vector loop.
Absolute ADD from BK (`01f8 0019`) at `02:e1e5` and STLM B,BK
and MVMD BK,AR0 are admitted. Long-offset STL `80e2 0001` at
`02:e206` fits the complete two-word delay of `f273 e209` at `02:e204`
and now executes, as does MVMD AR6,T (`7316 000e`) at `02:e109`.
Absolute MPY (`20f8 0017`, AR7 source) at `02:e10f` in FCALLD's
delay now executes. Smem CMPM now passes its retained addressing and
adjacent TC/NTC branch fixtures, and the original recorder overlay loads
and enters startup. The single authoritative staged execution stop is in
[5510 Current boundary](5510_bringup.md#current-boundary).
This is not a complete command transaction or native
audio, and the physical DA150 register map remains unverified.
The exact resident program/data comparison still passes; this is not full boot.
Single PSHD now reads CPU-register sources `0000..000f` through the
existing mapped Read path. The `pshd-mapped` fixture covers the twelve
owned registers with 288 Access/Read/Execute replays, retaining one source
word despite later source/pointer mutation. The `pshd-repeat` fixture adds
768 replay cases for short/long immediate repeats, four counts, upward and
downward RAM/MMR reads, two pages, and IRQ/NMI deferral with ISR return.
Each copy owns real Decode/Access/Read/Execute slots rather than using the
atomic stream shortcut. Ordinary DAGEN reads are admitted, but SP,
modifying self-AR aliases, peripheral sources and memory-count RPT remain
excluded. The original `4b97` increments AR7 (`MOD=2`); its first window is
BL..AR6. The decrementing form is `4b8f` (`MOD=1`).
The CPU-only `exp-t` profile retains 3,072 inputs/restores for both EXP sources, sixteen 40-bit vectors,
eight status configurations, optional adjacent LDU producers, two pages
and zero/one/two-gap T consumers. T publishes at Execute: an adjacent LDM
samples old T and a gap samples the new exponent. Source/flag preservation,
mutation after the consumer Read and four balanced checkpoints are checked,
without replaying every tuple at each checkpoint. NORM and
interrupt composition remain outside this acceptance.
ADDS retains 17,408 read-capable RAM-source inputs with balanced replay checkpoints through the shared memory-arithmetic
fixture. The unsigned operand ignores SXM; carry, sticky destination overflow,
OVM saturation, retained address/literal/read operands and single-access
counts are checked over all seventeen read-capable forms. Its SUBS control also
retains 17,408 inputs with balanced replay. Mapped/peripheral sources remain excluded.
Syntax-5 Xmem LD covers all 512 encodings in 196,608 replay cases, retaining
the selected AR2..AR5 RAM address and Read word before Execute's shift.
Distinct source pointers, source mutation, BK=0/nonzero wrap, both
destinations, all sixteen shifts and three snapshots are checked. T,
unselected accumulator and status are preserved. Mapped/peripheral sources,
repeats and interrupt composition are not covered.
Nondelayed CALA now retains a low-16-bit target on the caller's logical
page and one PC+1 frame word through Class 30B's held Read clock. Its
216-case IRQ/NMI/return/replay gate and separate 216-case DLD-producer
gate pass; the existing far-call controls remain green. CALAD, repeat and
other producer/composition cases remain unaccepted. An executed-address
trace confirms CALA's `02:3e55 -> 02:40e2` return path; the later MVDD
worker is entered by FCALL at `02:993d`, not directly by this near call.
Resident loading remains exact;
see [5510 bring-up](5510_bringup.md) for the bounded scope and frame-provenance
question. Suite version, profile counts, runner scopes and native 5110
restoration coverage have one authoritative home in the
[core regression checkpoint](c54x_execution_contracts.md#core-regression-checkpoint).
These checks establish the declared execution models, not full MU4 boot,
the physical DA150 map or native speech.

[TI SPRU131G section 3.2.5](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
defines a common lower 32K program window when `PMST.OVLY=1`: CPU accesses
to `xx0000..xx7fff` resolve to page zero, while the upper half remains
page-specific. The core applies this to fetches, program operands and
accumulator-addressed `READA`/`WRITA`. Logical PC/XPC and far-call stack
values retain the executing page. DMA addresses are not passed through this
CPU translation. This does not establish the physical RAM extent or the
data/program alias map of any particular C54x variant.

`check-c54x-core` exercises overlay-on fetch and accumulator transfers,
wrong-page sentinels, the distinct upper-page window and logical XPC.
Existing overlay-off far-control, repeat and save/replay fixtures remain
independent-page tests.

The MU4 loaded entry independently exposes `f4a0` (`LD #0,ARP`) and
`46f8` (`LD Smem,DP` with an absolute address extension). TI SPRU172C's
Load T/DP/ASM/ARP syntaxes 5 and 2 specify one and three cycles respectively,
plus the long-address extension cost. Eight ARP immediates and three absolute
DP values check field masking, unrelated ST0 preservation and extension
consumption in the core gate.

MU4 opcode `fa47` is delayed conditional branch ALEQ (SPRU172C condition
`47`): signed 40-bit A less than or equal to zero. Its three-cycle branch
decision precedes two delay words. Five executable fixtures test both signed
extremes, zero and positive values; changing A in the first delay word must
not change the captured decision. Both paths retain the expected delay-word
effects and measured port-to-port cycle intervals.

## Executed-opcode coverage

## Current audit boundary

The current native cold-idle inventory is 626 words in 110 high-byte groups,
SHA-256 `05356c59d7b0843b8023b7a68822b1b258a2a18cd161db6dd33955580bf33a59`.
Two isolated controls (`tmp/c54x-frontend-idle-a` and `-b`) reproduce the full
ranked report exactly. All 626 forms now have executable fixture assertions,
with no execution-only or absent words. The reviewed fingerprint deliberately
replaces the former 594-word expectation after those gaps were closed; a unit
negative control rejects the previous fingerprint and a wrong current hash.

The original `nse1pcm`/`nse1bit` native DTMF tests have a separate 642-form inventory, jointly
covered by executable assertions in completed default, `mac-smem` and
`xc-signed` CPU fixtures. Its fingerprint, strict comparison and scope limits
are maintained in `cobba_control_boundary.md`; it does not change the cold-idle
fingerprint or establish native call speech.
The clock-owned boot-loopback configuration `nse1pin` additionally executes
retry-store `7721`; its 643-form inventory is covered when the native backend
controller fixture is included. The ordinary handset still has no evidenced
production audio-clock attachment.

With verbose logging, the core emits `[opdecode]` alongside `[opcov]`, using
the actual debugger decoder and synthetic zero extensions without target
memory accesses. `tools/c54x_opcode_coverage.py --frontend-report` requires a
consistent decoder record for every observed word, rejects invalid lengths
and ranks unsupported forms as `length=unknown`; raw-word fallback is never
promoted to a boundary. The synthetic-extension census reports 617 decoded
and nine unsupported first words. Those nine are extension-controlled `6f`
forms, resolved separately from actual fetched tuples below. Conditional
BC/CC/XC and unshifted Smem loads have reviewed boundaries. These reports
measure observed frontend coverage, not ISA completeness or pipeline timing.
`check-c54x-observed-coverage` passes the reviewed cold-idle fingerprint,
physical Menu and long-power runs, and all 626 union assertions. Menu adds no
new words; long power executes 625, also adding none. Execution assertions
and debugger boundaries remain independent evidence; neither establishes
physical operand visibility or interrupt drain.
The synthetic-extension report cannot characterize `6Fxx` forms whose
operation is selected by a second opcode word. Verbose `[opwords]` records now
capture these actual execution fetches without extra target-memory reads.
The isolated coverage gate requires `--extension-report`: every observed `6f`
word must have complete two/three-word tuples whose counts sum to its execution
count. Idle/Menu/power currently capture 18 distinct tuples across nine first
words. `6f83` selects both `0c1e` and `0c5f`; `6ff8` has nine distinct
address/operation pairs. `[optupledecode]` applies the real decoder to those
captured words using a bounded buffer, never rereading target memory. The gate
also requires `--require-extension-decoded`: every variant must decode at its
actual fetched length. All 18 tuples pass; together with first-word-independent
decoding this covers all 626 observed words. Missing variants, invalid lengths,
absent opcodes, malformed records and count drift fail validation. A zero
extension still cannot stand in for the executed operation.

Shifted memory ADD/SUB/LD/STL/STH boundaries follow SPRU172C syntax 4.
The required marker is `TMS320C54x shifted memory disassembler: PASS`:
114,688 legal address/shift/accumulator combinations on two program pages,
130,176 reserved-extension rejection cases, all 18 captured tuples and two
additional STH syntax cases. Signed shifts are -16 through 15; LD/STL/STH
reserve the source-selection bit that ADD/SUB use independently of destination.
Long/absolute Smem's address word precedes the opcode-extension word.
This decoder closure changes no execution semantics. A physical frontend still
needs operand effects, hazards, overlapping stages and interrupt-drain timing;
the observed boundaries alone do not provide those contracts.

ADDM immediate-memory decoding covers all 256 addressing forms on ordinary
and wrapping program pages, with bounded two/three-word extension reads and
four exact syntax checks. Long-offset and absolute forms retain address-before-
literal layout; this is debugger coverage, not a change to arithmetic execution
or physical pipeline timing. The required gate marker is
`TMS320C54x ADDM disassembler: PASS` (SPRU172C, ADDM and classes 18A/18B).

NEG, ABS and NORM decode all twelve source/destination combinations as
one-word instructions, checked on ordinary and wrapping program pages without
extension reads. Source is bit 9 and destination bit 8 (SPRU172C instruction
definitions); the required marker is `TMS320C54x unary disassembler: PASS`.
This adds debugger boundaries only, not arithmetic or pipeline validation.

RND adds four source/destination forms; SAT, MAX, MIN and EXP each have only
the bit-8 accumulator selector. Twelve exact syntax cases and four rejected
extra-selector cases are checked on two program pages without extension reads.
The required marker is `TMS320C54x normalization disassembler: PASS`, based on
the corresponding SPRU172C instruction definitions; arithmetic and physical
timing remain separate contracts.

Accumulator ADD/SUB signed shifts decode all 256 combinations of shift and
source/destination; ADD/SUB/LD ASM forms add twelve combinations. Each is
one word and checked on ordinary and wrapping program pages without extension
reads. The required marker is `TMS320C54x accumulator ALU disassembler: PASS`
(SPRU172C, ADD/SUB syntaxes 9/10 and LD syntax 9). This establishes debugger
boundaries only, not arithmetic validation or physical pipeline timing.
The executable `6f82/0c01` ADD and `6f83/0c21` SUB vectors separately assert
that their A results (14 and 6) leave B's full 40-bit `8176543210` sentinel
unchanged. `make check-c54x-core` verifies these assertions; they do not
establish other source/destination selections or physical pipeline timing.
A 104-case execution matrix verifies ADD and SUB with all four A/B
source-destination selections. It includes memory 8 at shifts -1, 0 and +1,
and memory `fff8` with SXM clear/set at shifts -16, -1, 0, +1 and +15.
Initial A/B are 10/30; expected shifted operands are explicit constants,
independent of the core's shift helper. Assertions also require unchanged
memory and AR2, preservation of the unselected accumulator, legal execution
and the two-word continuation. The gate requires the 104-case completion
marker. This does not cover every operand, intermediate shift magnitude,
saturation or physical pipeline timing.

Immediate LD decodes shifts 0–16 and both destinations (34 forms), checked
on ordinary and wrapping program pages with one literal extension. Unlike
immediate arithmetic, its documented opcode fixes bit 9 to zero; 34 extra-
source-bit aliases remain unsupported despite the execution handler's broader
mask. The required marker is `TMS320C54x immediate load disassembler: PASS`
(SPRU172C, LD syntaxes 7/8). This is decoder evidence, not alias-legality or
physical timing evidence.

LDU has reviewed one/two-word boundaries for all 512 addressing/destination
forms, with bounded extension reads on ordinary and wrapping program pages
and six exact syntax cases. The required marker is
`TMS320C54x unsigned load disassembler: PASS` (SPRU172C, LDU). Its unsigned
execution behavior is unchanged; decoder coverage is not a timing claim.

WRITA decodes all 256 data-source addressing forms with bounded one/two-word
reads on two program pages and four exact syntax cases. Its required marker is
`TMS320C54x WRITA disassembler: PASS` (SPRU172C, WRITA). Separate executable
fetch-order cases validate its existing data-to-program transfer, not physical
pipeline or repeat timing.

MVPD decodes all 256 destination-address forms on two program pages with
five exact syntax cases. Long offsets follow pmad; absolute addresses precede
it, matching the independently tested execution layouts. The required marker
is `TMS320C54x MVPD disassembler: PASS` (SPRU172C, MVPD; SPRU131G
section 5.5.3.2). Separate passive bus-tap fixtures validate Fetch-before-Read
and destination effects; neither proves physical pipeline stages or wait states.

Parallel ST/LD decodes 1,536 one-word forms: four accumulator source/destination
combinations and two accumulator-to-T variants across all dual-memory modifiers.
The gate checks two program pages, bounded reads and four exact syntax examples.
Its required marker is `TMS320C54x parallel load disassembler: PASS`
(SPRU172C, ST||LD). Parallel syntax does not establish physical overlap timing.

FRAME decodes all 256 signed eight-bit immediates. MVMM admits only AR0–AR7
and SP: 81 register pairs, with 175 rejected pairs. Both are one word, checked
on two program pages; the required marker is
`TMS320C54x frame/MMR move disassembler: PASS` (SPRU172C, FRAME/MVMM).
FRAME latency has conflicting primary wording: SPRU172C says no next-instruction
address latency, while SPRU131G table 7-9 assigns category-I latency 1 and
category-II latency 0 in compiler mode. Existing execution and its 24-case
stack-address gate follow the latter detailed table; decoding does not resolve
that physical-timing discrepancy or justify changing the execution model.

MVDD decodes all 256 packed dual-memory operands independently of ST/LD;
immediate DP loads decode all 512 nine-bit values. Both are one word and checked
on two program pages without extension reads, including four exact MVDD source/
destination examples. The required marker is
`TMS320C54x dual move/DP disassembler: PASS` (SPRU172C, MVDD and LD syntax 3).
Execution, address visibility and physical timing are unchanged.

Special-register loads cover 768 Smem forms targeting T, DP or ASM and all
eight immediate ARP selectors. Tests bound one/two-word reads on two program
pages, check destination syntax and retain an undocumented ARP-alias negative
case. The required marker is `TMS320C54x special load disassembler: PASS`
(SPRU172C, LD T/DP/ASM/ARP). Decoder lengths do not imply equal cycle costs:
the memory-to-DP execution remains its separately modeled three-cycle form.

Accumulator AND/OR signed-shift forms cover 256 combinations of shift −16–15
and independent source/destination selectors. Tests check exact syntax and
one-word bounded reads on two program pages; the required marker is
`TMS320C54x accumulator logical disassembler: PASS` (SPRU172C, AND/OR syntax 4).
This complements the reviewed XOR form without changing logical execution or
claiming physical pipeline timing.

Smem MPY, MPYR and MPYU retain separate signed/rounded/unsigned mnemonics
and both destinations; BITT names the T-selected bit test without an accumulator
operand. All 1,792 addressing forms have bounded one/two-word reads on two
program pages, with seven exact syntax cases. The required marker is
`TMS320C54x memory multiply/bit disassembler: PASS` (SPRU172C, MPY syntax 1,
MPYU and BITT). Arithmetic and timing execution are unchanged.

SFTL decodes all 128 signed-shift/source/destination combinations as one-word
logical shifts. Exact syntax and bounded reads are checked on ordinary and
wrapping program pages; the required marker is
`TMS320C54x logical shift disassembler: PASS` (SPRU172C, SFTL). Its existing
low-32-bit execution and carry behavior remain separate from this decoder test;
no physical pipeline timing is established.

Dual-memory ADD/SUB decodes 1,024 packed operand/destination forms; accumulator
MACA/MACAR/MASA/MASAR adds sixteen source/destination/rounding forms. Tests
bound reads to one word on two program pages and check exact operand ordering.
The required marker is `TMS320C54x dual/accumulator arithmetic disassembler: PASS`
(SPRU172C, ADD/SUB syntax 6 and MACA/MASA syntax 2). This does not change
arithmetic execution or establish parallel physical timing.

Memory-sourced RPT and ST T/TRN decode 768 addressing forms with bounded
one/two-word reads on two program pages and six exact syntax cases. The required
marker is `TMS320C54x memory repeat/store disassembler: PASS` (SPRU172C, RPT
syntax 1 and ST syntaxes 1/2). Repeat scheduling and register-store execution
remain unchanged; these are debugger boundaries, not physical timing evidence.

DADD, DSUB and DRSUB decode 2,048 long-memory addressing/accumulator forms
with bounded one/two-word reads on two program pages and seven exact syntax
cases. DADD retains independent source/destination selectors; the subtract
forms retain their direction and single accumulator selector. The required marker
is `TMS320C54x double arithmetic disassembler: PASS` (SPRU172C instruction
definitions). Paired-word execution and physical timing are unchanged.

SUBC decodes all 512 Smem/accumulator forms with bounded one/two-word reads
on two program pages and five exact syntax cases. The required marker is
`TMS320C54x conditional subtract disassembler: PASS` (SPRU172C, SUBC).
Its divide-step arithmetic, carry/overflow behavior and timing remain separately
tested execution contracts, not conclusions of debugger decoding.

Accumulator-A MPYA/MACA/MACAR/MASA memory forms cover 1,024 addressing
encodings; short MPYA/SQUR adds four forms with selected destinations. Tests
check two program pages and exact syntax separately from arithmetic execution.
The required marker is `TMS320C54x accumulator-A multiply disassembler: PASS`
(SPRU172C, MPYA, MACA/MASA syntax 1 and SQUR syntax 2). Fixed accumulator-A
inputs are not interchangeable with the T-based ordinary multiply family.

T-based memory MAC/MACR/MAS/MASR covers 2,048 addressing, rounding and
destination encodings on two program pages, with eight exact syntax cases.
The required marker is `TMS320C54x memory MAC/MAS disassembler: PASS`
(SPRU172C, MAC/MAS syntax 1). Extension reads are bounded; arithmetic,
accumulator visibility and physical timing are unchanged by this decoder work.

Short shifted STL/STH syntax 3 covers 1,024 one-word forms on two program
pages, with four exact syntax cases including acquired `9a5f`. The shift is
unsigned 0-15; the address uses the dual-address modifier, not Smem encoding.
The required marker is `TMS320C54x short shifted store disassembler: PASS`
(SPRU172C, STL/STH syntax 3). Execution semantics and timing are unchanged.

Conditional RC/RCD use the same 22 reviewed individual predicates as BC/CC/XC,
giving 44 one-word forms checked on two program pages. Non-delayed RC has
conditional step-out flags; delayed RCD deliberately omits immediate step-out
because two delay words still execute. Compound predicates remain unreviewed.
The required marker is `TMS320C54x conditional return disassembler: PASS`
(SPRU172C, RC[D]); no execution or physical delay timing is inferred from it.

Exact acquired `6f95/0e20` (`SUB *AR5+,0,B,A`) and indirect loads `1087`
(`LD *AR7,A`) and `1182` (`LD *AR2,B`) have six independent execution cases,
each with SXM clear/set and memory `8001`. They assert signed/unsigned results,
preserved source accumulator and memory, and the single AR5 postincrement
versus unchanged load pointers. The required marker is
`TMS320C54x native gap arithmetic/load: PASS`; this is selected operand/status
coverage, not all inputs or physical pipeline timing.

Twelve additional cases assert `BANZ` forms `6c8d/6c8e` with zero and nonzero
AR5/AR6, including comparison before postdecrement; `MAR 6db7` with positive
and negative AR0 increments; and stores `80b4/838a/8383` with independent low/
high-word patterns, addressed writes and post-updates. Unselected ARs and
accumulators are also checked. The core runner requires
`TMS320C54x native gap branch/address: PASS`. Its simulation ceiling is two
seconds because the expanded fixture chain exceeds the former one-second
limit; completion still requires every marker and the intentional terminal
exit, never a timeout. No instruction timing is changed by this deadline.

Eight cases additionally assert `f67b` (`SFTA B,-5,A`) under both SXM
settings, `3185` accumulator-high/memory multiply under both FRCT settings,
`f301/4000` immediate addition with clear/set carry outcomes, and `f648`
(`LD B,8,A`) with positive/negative source values. They check full 40-bit
results, carry, T publication, unchanged source/memory and AR5. The required
marker is `TMS320C54x native gap math: PASS`. Overflow/saturation boundaries
and physical stage timing are not established by these selected operands.

Twelve memory cases assert `6b82` ADDM carry/overflow and 16-bit writeback,
`4583` signed/unsigned high-halfword loads, `21f8` absolute MPY under both
FRCT settings, `3ed3` partial-carry add with circular BK zero/eight, `5793`
double load with guard extension and two-word postincrement, and `818b`
low-word store with postdecrement. They check selected result/status values,
unchanged T/source/memory where applicable and exact AR2/AR3 changes. The
required marker is `TMS320C54x native gap memory: PASS`; other operand/flag
combinations and physical timing remain outside this coverage.

The final twelve cases assert `9a5f` shifted high-word store/postdecrement,
`f20f` immediate ADD, `f47d` signed/logical right shift, `f50f` shifted
accumulator ADD, `f736` shifted accumulator SUB and `f761` overflow/saturation.
They check full results, exact carry/overflow flags, source preservation and
the compact store's address update. The runner requires
`TMS320C54x native final gaps: PASS`. This closes assertion coverage for the
captured 626-word trace, not complete ISA, stage timing or native speech.

## Debugger instruction boundaries

MVDM/MVMD decode their two-word MMR transfers; PSHD/POPD decode ordinary
and extended Smem addresses. The gate checks 960 forms, 64 rejected MMR
encodings, bounded extension reads and ten exact examples on two pages.
Its marker is `TMS320C54x move/stack disassembler: PASS`. POPD separately
has four executable fetch-order cases: destination extensions precede the
stack read, with preserved value, SP and pointer results. Those assertions
are part of `TMS320C54x operand fetch order: PASS`, not inferred from
debugger coverage or promoted to physical pipeline timing.

Ordinary and ASM-shifted STL/STH decode their accumulator and Smem address;
short accumulator and signed ASM loads decode without extensions. The gate
checks 2,048 store encodings and 544 short-immediate encodings on two pages,
fourteen exact examples and one invalid ASM encoding. Its marker is
`TMS320C54x store/short disassembler: PASS`. Address-update and store-stage
semantics remain those separately covered by execution fixtures.

Parallel ST||MAC/MACR/MAS/MASR and SQDST decode both dual-memory operands
without extension reads. The gate checks 4,352 forms on two pages and ten
exact examples, including all four CMPL accumulator pairings. Its marker is
`TMS320C54x parallel disassembler: PASS`. Parallel syntax does not establish
physical concurrency: the core's existing effects remain instruction-atomic.

LDM, STLM, PSHM and POPM decode their single-word direct/short-indirect
MMR forms. The gate checks 1,344 forms and 192 rejected extended encodings
on two pages, no extension reads and ten exact syntax/rejection examples.
Its marker is `TMS320C54x MMR disassembler: PASS`. Indirect POPM separately
has four executable cases for pre/postincrement, compatibility-mode ARP
selection and an SP destination. It uses the shared page-zero MMR helper;
forbidden extended encodings remain unimplemented. The fixtures failed on
the former direct-only handler and pass with the corrected handler, requiring
`TMS320C54x indirect POPM: PASS`. This is selected stack/addressing coverage,
not physical stack-write or subsequent compiler-mode DAGEN timing evidence.

BANZ/BANZD decode short-indirect and long-offset Sind operands. The gate
checks 240 supported forms and 272 unclaimed direct/absolute forms on two
pages, bounded target/displacement reads and seven exact examples. Long
forms consume the displacement before the target, consistent with the
reviewed execution fixture; TI's BANZ example 3 confirms a three-word
fallthrough. Its marker is `TMS320C54x BANZ disassembler: PASS`. This is not
a new branch timing or delay-slot execution claim.

DLD/DST decode the accumulator and Lmem operand in load/store order,
including one-word ordinary and two-word extended-address layouts. The gate
checks 1,024 mode encodings on two pages, bounded address reads and eight
exact examples. Its marker is `TMS320C54x double memory disassembler: PASS`.
These frontend checks do not establish memory alignment, address-update or
write-stage timing beyond the existing execution fixtures.

Shift-16 Smem ADD/SUB decode independent accumulator selectors and the
one-word direct/short-indirect or two-word extended-address layout. The gate
checks 2,048 mode encodings on two pages and eight exact operand examples.
Its marker is `TMS320C54x high memory ALU disassembler: PASS`. Execution
assertions remain separate from these debugger length/syntax checks.

MAR decodes the 128 explicit indirect forms, including long-offset and
absolute extensions; 128 direct forms stay unsupported. The gate checks both
sets on two pages and seven exact examples. Its marker is
`TMS320C54x MAR disassembler: PASS`. The older RPTBD fixture's mislabeled
`6d10` was corrected to `6d90` (`MAR *AR0+`, I=1). This retires an accidental
fixture assumption, not a runtime direct-MAR legality claim: the execution
handler's treatment of direct encodings remains unaudited.

Dual-memory MPY and MAC/MACR/MAS/MASR decode their X/Y pointer modifiers
and accumulator fields as single-word forms. The gate checks 4,608 encodings
on two pages with no extension reads and seven exact operand/rounding examples.
Its marker is `TMS320C54x dual multiply disassembler: PASS`. This describes
the TI operand layout; it does not expand arithmetic or memory-hazard
execution coverage.

Immediate ADD, SUB, AND, OR and XOR decode the literal, independent accumulator
selectors and shifts 0 through 16, including the separate shift-16 opcode
forms. The gate checks 340 forms on two pages with bounded literal reads,
thirteen exact syntax examples and a negative unsupported-family example.
Its marker is `TMS320C54x immediate ALU disassembler: PASS`. Other immediate
families remain raw until reviewed; execution and timing are unchanged.

Accumulator SFTA, shifted LD and shifted XOR decode the signed five-bit
shift and independent source/destination bits. ROL, ROR and ROLTC decode
their accumulator source without reading an extension. The gate checks
384 shift forms on two pages and exact examples including all six rotates
and the -16/+15 shift endpoints. Its marker is
`TMS320C54x accumulator disassembler: PASS`. These are debugger boundaries,
not new arithmetic execution assertions or pipeline timing evidence.

The ordinary Smem ADD/SUB, ADDS/SUBS, TS-shifted ADD/SUB and ADDC/SUBB
families expose one-word direct/short-indirect and two-word extended address
layouts. The gate checks 4,096 mode encodings on two pages, bounded reads
and eight exact syntax examples. Its marker is
`TMS320C54x memory ALU disassembler: PASS`. These frontend assertions do
not establish execution for every operand or alter arithmetic/timing.

Unshifted, TS-shifted and shift-16 `LD Smem,A/B` expose their one-word
direct/short-indirect and two-word long-offset/absolute layouts. The gate
checks 1,536 mode encodings on two pages, bounded extension reads and ten exact address/destination
examples. Its required marker is `TMS320C54x load disassembler: PASS`.
This is a frontend contract; it neither changes execution nor supplies
pipeline-stage timing.

Memory AND/OR/XOR decode their Smem address and accumulator. The gate checks
1,536 encodings on two pages with bounded extension reads and six exact
examples; its marker is `TMS320C54x memory logical disassembler: PASS`.
This does not establish additional arithmetic/status semantics.

BC/BCD, CC/CCD and XC decode 22 explicit named predicates. The core gate
checks 132 forms on two pages, bounded extension reads, stepping flags and
exact asymmetric examples; unreviewed predicate combinations remain raw.
This validates debugger syntax and lengths, not execution of every condition
or physical pipeline timing. The required marker is
`TMS320C54x conditional disassembler: PASS`. A fresh native coverage run
retains all 626 asserted words and the protected opcode-set fingerprint.

The native core disassembler decodes the reviewed branch/call/return,
repeat, idle, interrupt and status-bit forms, including extended accumulator
branches and calls. `check-c54x-core` checks 48
exact encodings at two program addresses, including an extension word crossing
the 16-bit page boundary. Near targets retain their program page; far targets
carry their explicit extended address. The native 5110 Phone book menu gate
also passes with this decoder enabled.

The timer reload/mask families `STM #lk,MMR`, `ST #lk,Smem`,
`ANDM #lk,Smem` and `ORM #lk,Smem` are decoded as complete two- or
three-word instructions, with direct, short-indirect, long-offset and absolute
operands. STM excludes the long-offset/absolute modes forbidden for MMR
addressing by SPRU131G section 5.6. The gate checks 992 decoded mode encodings
at ordinary and page-end addresses, all 32 invalid STM/MMR encodings, exact
extension-read bounds and twelve asymmetric operand/modifier examples.
Operand ordering matches the reviewed executable decoder fixtures:
long-offset logical immediates precede the offset; immediate stores and
absolute logical forms put the address extension first. This agreement is not
an independent silicon oracle for extension ordering. Evidence is
`tmp/c54x-literal-boundary-final.log`, requiring
`TMS320C54x literal disassembler: PASS`. This checks debugger decoding, not
the runtime legality of every addressing mode or the physical pipeline.

MVDK, MVKD, MVDP, PORTR, PORTW, READA, CMPM and BITF now expose complete
instruction lengths and operand layouts. The gate checks 2,048 mode encodings
on two pages, exact extension-read bounds and nine asymmetric operand examples.
READA has no extra immediate; long-offset PORTR/PORTW/MVDP put their port or
destination before the displacement, unlike the displacement-first move/test
forms. These are debugger/frontend boundary checks against the reviewed
execution layouts, not proof of every mode's silicon legality or stage timing.
The required marker is `TMS320C54x move/test disassembler: PASS`.

Unknown forms still return a raw one-word display entry without `SUPPORTED`.
That fallback is not a verified instruction boundary and must not be used to
populate a future decoded-instruction pipeline. The current observed 626-word
set, including its 18 captured extension-controlled tuples, now has reviewed
boundaries. This satisfies the observed-set frontend prerequisite, not full
ISA coverage or a physical pipeline implementation.

## Native timing continuation

Native configurations still recognize interrupts before an instruction-atomic
`execute_one()`. Hardware interrupt entry retains the accepted vector,
source, then captures the continuation at INTR Decode before stepping SP
decrement, vector fetch/IACK and stack write on separate CPU cycles. All
entry continuation state is saved.
The explicit IRQ/NMI fixture requires the stack write one cycle after vector
fetch; retained-vector save/replay snapshots now interrupt entry before that
write and verify its completion without a second fetch or acknowledgement.
Reset and debugger redirection cancel the pending entry. The aggregate entry
cost remains the existing calibrated five cycles. Its former residual cycle
is now assigned to Decode before Access; this does not establish foreground
drain or the complete physical pipeline.
IFR remains pending after source acceptance and is selectively cleared at
the first vector fetch/IACK, as specified by SPRU131G 6.10.4. A one-clock
checkpoint after acceptance asserts unchanged SP/stack, both original IFR bits
and no acknowledgement. A second one-clock checkpoint requires those same
properties after Decode and saves the entry before Access. Changing IMR to
enable a different pending source cannot reselect the accepted vector; normal
and restored continuations both clear only the originally accepted bit and
stack the original PC once. The early-clear implementation fails this
checkpoint (`tmp/c54x-ifr-fetch-red.log`); the required marker is
`TMS320C54x accepted interrupt checkpoint: PASS`. This checks entry ordering
and replay, not Decode-stage foreground drain or full pipeline latency.
`check-c54x-core` requires the IRQ/NMI entry-spacing marker.
Maskable INTM setting belongs to that same vector Fetch/IACK stage, following
SPRU131G 6.10.8, rather than the earlier source selection. The accepted-entry
checkpoint requires INTM clear before IACK and set inside the acknowledgement
callback, both before and after save/load; its marker includes `intm_at_iack=1`.
The prior early-setting implementation fails this checkpoint. NMI's existing
acceptance behavior and the aggregate entry cycle count are unchanged. This
is an entry-side ordering correction, not foreground pipeline retirement or
proof of the full instruction pipeline.
The `decode_before_access=1` checkpoint fails the former Access-first entry
(`tmp/c54x-decode-before-red.log`). Return-PC capture now belongs to Decode,
not source acceptance; vector Fetch/IACK follows Access and stack write follows
Fetch by one cycle for both IRQ and NMI. The first ISR execution retains its
previous aggregate placement. The short-period timer still enters 108 times
without reaching the foreground mask write, so this ordering correction does
not resolve that pipeline-drain boundary or promote recorder completion.
The previously banked `check-c54x-rom4-rf-boundary` snapshot is 20,282 frame
expiries, 648,096 RF reads and 1,519 port-`32` writes. Current fresh-storage
runs at that milestone instead reproduced 20,285 / 648,192 / 1,520, with no port-`38/39` reads.
A generated-overlay control using the original instruction-boundary interrupt
routine reproduces the same changed totals: the staged-frontend refactor is
not their cause. A second generated-overlay control restores the CPU's
`device_start`, `device_reset` and timer-clock reader from `146b9ff8` (the last
documented passing interrupt-entry milestone); it reproduces those same totals
too. These clock/reset additions therefore do not explain the mismatch either.
Both controls use newly generated own EEPROM and no retained flash/SIM state;
reusing the first run's NVRAM is not a valid cold control. The exact snapshot
gate was deliberately left failing pending the provenance audit. The
EF_SMSS parent correction above settled that mismatch; the current
independent-transfer scan snapshot supersedes both historical counts.
The earlier save-state pass protects its tested restoration boundary, not
RF acquisition or handset speech.

### Staged frontend prototype

The optional `set_pipeline_frontend` path owns Prefetch, Fetch, Decode,
Access, Read and Execute slots inside the existing CPU, reusing its semantic
backend and interrupt-source/entry operations. The separate `nse1pipe`
research machine enables it; ordinary handset and MU4 configurations do not.
The basic memory/ALU subset covers NOP, one-word immediate loads, two-word
shifted immediate loads, direct DP-relative loads and STL/STH stores, and
short indirect loads using plain, post-decrement, post-increment or
pre-increment addressing, and ANDM/ORM with DP-relative, plain/post-decrement/
post-increment short indirect or three-word absolute (`f8`) operands, and
RETF/RETFD with one-word load/NOP or two-word immediate-load delay slots.
Two-word unconditional B/BD use the same retained-literal and redirect path.
ADDM now shares the retained RAM read/modify/write path with ANDM/ORM.
SPRU172C page 4-9 specifies SXM/OVM-dependent arithmetic and C/OVA updates,
not unconditional 16-bit wrapping. The atomic and staged frontends use one
unchanged arithmetic helper: staged Read retains the source word, address
and immediate; Execute publishes the result and flags once. The original
worker's `6b82 0002` form is included. There are 768 replay cases across
DP-relative, stationary/postincrement AR2 and absolute RAM addresses, eight
value pairs and eight SXM/OVM/sticky-OVA combinations. They include TI's
`8007 + fff8 -> 8000` saturation example, signed/unsigned boundaries and
carry clearing. Source RAM changes after Read cannot alter the retained
result. Accumulators remain unchanged. Absolute ADDM also admits AR0..AR7
and SP destinations, but not other mapped registers. The `addm-ar` profile
checks all eight ARs, four initial values, four immediates, all sixteen
SXM/OVM/C/OVA combinations, both CMPT settings and two instruction pages,
with save/load before Read, before Execute and after publication (24,576
cases plus replay). Operand mutation after Decode/Read verifies retention;
the result and C/OVA oracle is independent of the CPU helper. The
`addm-ar-load` profile checks 768 cases plus replay with short and long-offset
unshifted LD consumers, two or three separating NOPs, and one AR publication
before the consumer's data read. Both LD forms are Category I under SPRU131G
table 7-7; the address extension alone does not make the long-offset form a
Category II consumer. Forbidden-gap behavior is not claimed. Other mapped
destinations, long-offset ADDM addressing, repeats, compiler-mode direct
sources and adjacent producer/consumer hazards remain excluded or unvalidated.
The phase placement inherits the bounded RMW prototype, not measured silicon.

Register-only ROR, ROL and ROLTC on A/B are admitted through the existing
semantic backend. SPRU172C pages 4-143..4-145 specify rotation of the low
32 bits and clearing of bits 39..32: ROR/ROL use C as input; ROLTC uses TC;
all publish the shifted-out bit to C and leave other status bits unchanged.
The `rotate` profile independently checks 4,608 cases plus save/load replay:
six forms, eight guard/sign/endpoint patterns, all C/TC combinations, four
SXM/OVM combinations, two pages and three snapshots. Each case executes two
adjacent rotates and checks state every clock, including carry forwarding
and preservation of the other accumulator. Repeat-body admission remains
excluded; these fixtures make no silicon bus-phase claim.

RPTZ A/B,#lk shares immediate RPT's retained-count, body-latching and
interrupt-protection path, additionally clearing the selected 40-bit
accumulator once at Execute (SPRU172C 4-150). The `rptz` profile checks
768 cases plus replay across both accumulators, counts 0/1/3/17, C/TC and
SXM/OVM settings, two pages and three save points. An existing immediate
store body proves exactly count-plus-one ordered writes and AR updates;
mutating the setup literal after Decode cannot change the repeat. Other
accumulator and status bits remain unchanged. Existing nested, block and
control-delay repeat exclusions also apply to RPTZ. This does not admit
new body instructions by itself.

Short indirect STL/STH A/B bodies now use the pipelined repeat issuer:
each iteration owns a Read-stage address update and Execute-stage store,
not a host-side memory fill. The `rpt-store` profile checks 49,152 cases plus
replay across all ARs, plain/postdecrement/postincrement/preincrement modes,
counts 0/1/7/255, RPT/RPTZ setup, SST/SXM combinations, two source patterns,
two instruction pages and three save points. A test-only source mutation
after each write verifies Execute-live sampling and independent saturation
results. There are no data reads, exactly count-plus-one writes, and no
changes to other ARs, the other accumulator or status. `rpt-store-edge`
checks 4,096 cases plus replay for mapped first destinations 0000/0010/
0020/005f: rejection precedes both DAGEN updates and bus effects, including
preincrement wrap. These are implementation exclusions, not claimed silicon
traps. Direct, indexed, extended, ASM-shift and mapped repeated stores remain
outside this admitted subset; physical bus phase is not claimed.

BITT consumes retained Smem and T(3..0), setting only TC from bit
`15-(T&15)` (SPRU172C 4-23). It shares Smem address resolution and the
single Read-owned bus access, but does not publish T as LD/MPYA do.
The selector is retained in Read, not re-read at Execute; SPRU131G 7.5.6
defines T-consumer latency and protected T writers. The `bitt` profile
checks 6,144 cases plus replay: DP/SP direct, short plain/postdecrement/
postincrement, absolute and two long-offset forms, all selectors, two
T-high-bit patterns, complementary data and both initial TC values, two
pages and three save points. Mutating both operands after Read cannot
change the result; T, accumulators and other status bits are preserved.
`bitt-retd` adds 1,536 replay cases with absolute or long-offset BITT
occupying RETD's two delay words: exactly one stack read, one operand read,
one extension fetch and target-only continuation. This two-word delay
admission is limited to RETD; other control owners retain their exclusions.

Short BITT also admits either delay word of BD, BCD, CALLD, FCALLD and
RETD. `bitt-ctrl` checks 49,152 cases plus cleared-pipeline replays: DP,
plain/postincrement AR and SP sources, all sixteen selectors, two T-high-bit
patterns, complementary words, both initial TC values, two pages and three
checkpoints. The second-slot cases use an organic first-slot LD to T.
Separate post-Read memory and T poison cohorts check retention without
allowing the two mutations to cancel. BITT publishes TC at Execute, while
TC/NTC BCD retains its earlier branch decision. The fixture checks owner-only
stack effects, poisoned opcode/target/return data and exact continuation.
These are prototype ordering checks, not measured physical pipeline phases;
mapped operands, extended BITT under other controls and arbitrary interrupt
composition remain outside this qualification.

Nondelayed RC TC/NTC shares the existing conditional-return stack and
redirect path. SPRU172C 4-133 specifies no status changes, one stack pop
only on a taken return, and the Class 32 taken/untaken distinction.
The decision samples TC after the older Execute publication, then retains
both that decision and the popped target. `tc-return` checks 384 cases plus
replay: both predicates/outcomes, initial status or short/absolute BITT
producers, zero/one separating NOP, two bit selectors, two pages and four
save points. Mutating TC and the stack after the decision cannot change
the path or target; untaken paths perform no stack access. The fixtures
retain the existing three/five-cycle untaken/taken cadence. Delayed and
compound status predicates remain outside the admitted subset.

Short circular MAR admits the four modifiers -1/-AR0/+1/+AR0 through the
existing Access-owned DAGEN update. `mar-circular` checks 49,152 cases plus
replay across all AR encodings, CMPT states, BK=1/2/3/13/16/31/32/250,
four legal positions/stride classes, two pages and three save points.
Explicit alignments and a signed-residue oracle are independent of the
CPU's mask/adjust implementation; inputs obey SPRU131G 5.5.3.4's buffer
and stride constraints, including the AR0 pointer/index alias. The opcode
performs no data access, updates only the selected AR/ARP, and does not
repeat its update after restore. The native 6dc2 entry used BK=00fa and
AR2=0701, a nonzero-buffer case. BK=0 remains the qualified ROM4 behavior
described in `djr_dsp_integration.md`, not a newly validated silicon rule.
Long-offset circular MAR and close BK/AR producer compositions remain
unvalidated; no physical pipeline-phase claim is made.

BCD AEQ/BEQ admits a complete two-word Smem ADD/SUB/ADDS/SUBS instruction
in its delay span, using the existing retained operand and ALU semantics.
`bcd-arithmetic` checks 36,864 cases plus replay across both accumulators,
both branch predicates/outcomes, absolute and two long-offset forms, four
operand/source patterns, SXM/OVM combinations, two pages and three save
points. Arithmetic may change the branch-tested accumulator without
changing the already sampled decision. An independent signed-40-bit oracle
checks the result, saturation and C/OV flags; memory mutation after Read
cannot change the operand. There is one data read and no data write or call
frame. `bcd-arith-edge` checks 288 replay cases preserving the new subset's
ANEQ/BNEQ exclusions and rejecting a two-word operand starting in the final
delay word before DAGEN or operand effects. The pre-existing absolute ADD
admission for other owners is unchanged. Other new owner compositions and
physical pipeline timing are not claimed.

Nondelayed CC now admits all twelve signed A/B predicates, sharing CCD's
existing signed-40-bit predicate decoder without changing frame ownership.
The `signed-call` profile extends the existing CC/RC fixture to 768 replay
cases: eight zero/sign/guard patterns, two pages and four save points.
Changing the predicate after Read and the target literal after Decode
cannot change the selected path. Taken calls publish one frame, fetch the
target once and return with balanced SP; untaken calls never access stack
memory. The original 64-case AEQ/BEQ and 768-case signed-return profiles
remain unchanged. Compound/status-call predicates are not newly admitted.

Short RAM PSHD now retains its source address/value and stack destination.
SPRU172C page 4-131 places its SP read in Decode and update in Access;
the prototype preserves that allocation separately from the existing PSHM
Read-owned allocation. Thirty-two cases cover DP/compiler-relative and
stationary/postincrement AR2 sources with four save/replay boundaries.
Six compiler-mode consumers verify table 7-9's Category-I one-clock SP
latency: the adjacent load uses pre-push SP, while one NOP exposes updated SP.
Six cases compose the original `6b82 0002 4b92` ADDM/PSHD sequence.
SPRU131G table 7-2 assigns DB reads to the first DARAM half-cycle and EB
writes to the second, but this does not determine the consumed value:
section 7.3.2 specifies an internal-bus bypass for a same-location on-chip
read with a pending write. PSHD retains the forwarded ADDM result without
a second physical RAM read; the ADDM Write clock and flag publication
remain unchanged.
All 44 cases require one stack allocation/publication, retained samples and
no duplicate replay accesses. PSHD preserves status and accumulator state.
Peripheral sources, extended addressing, general stack/DAGEN conflicts and
physical bank arbitration remain excluded or unvalidated; these fixtures do
not execute the complete zero-control-word worker body or native recorder.

Short RAM POPD retains a captured destination and one stack sample before
publication (SPRU172C page 4-127, class 17A). Its Read-owned SP increment
shares the partial POPM placement model, rather than asserting a measured
silicon update phase. Thirty-two cases cover DP/compiler-relative and
stationary/postincrement AR3 destinations with four replay boundaries;
six compiler-mode consumers verify one-clock Category-I SP latency.
Twelve cases execute the original six words at `02:3dc5`--`02:3dca`:
ADDM #2,*AR2; PSHD *AR2+; MVDK *AR2,AR3; POPD *AR3+.
Both SXM states retain the forwarded result for the stack copy, while ADDM
updates source RAM and its arithmetic flags. MVDK loads the second source
word as the destination pointer; POPD writes to that captured address despite
AR3 postincrement. All 50 cases require one stack consumption/publication,
no duplicate replay access and preserved non-arithmetic context. The static
context tool pins the six original words, but does not assign meanings to
the operands. Peripheral/extended destinations, general stack/DAGEN/bank
conflicts and native recorder behavior remain unvalidated. Complete worker
composition is specified below.

Short accumulator stores now include stationary/postdecrement/postincrement
indirect addressing, and short ST T/TRN use the same retained destination
path (SPRU172C page 4-167, class 10A). Read captures the address and applies
its AR update once; Execute publishes the selected register value to that
captured address. Seventy-two cases cover A/B high/low words, T/TRN,
DP/compiler-relative and stationary/postincrement AR3 destinations, and
three save/replay boundaries. Changing AR3 after address capture must not
retarget the write. Three sequence cases verify consecutive high/low B and T
publications with a neutral gap; that gap does not claim to execute the
original intervening CMPR. All 75 cases preserve register/status values and
produce no duplicate writes on replay. Source-register producer latency,
extended modes and physical bus placement remain unvalidated; T/TRN stores
are limited to RAM destinations. Complete worker composition is specified
below.

Staged CMPR samples unsigned ARx/AR0 in Access and retains its predicate
until Execute changes TC, without re-reading live address registers
(SPRU172C page 4-34; SPRU131G table 7-7 Category I). The 435 replay cases
include all four comparisons, all eight auxiliary registers and four
unsigned boundary pairs. Mutating the registers after Access must not
change the comparison result. Forty-eight producer controls require zero
intervening instructions after STM/MVDK, one after POPM and two after STL
before the new AR value becomes visible. Those controls use DP zero for
the direct memory/register operands, rather than relying on an inherited
data page.

Three cases execute the original `8393 8193 f4ab 8c93` at
`02:3dcb`--`02:3dce`: high/low B stores, CMPR EQ,AR3, then ST T.
With initial AR3=`0470` and AR0=`0471`, CMPR samples `0471` in Access
before the low-word store's Read-stage increment; its Execute result must
stay true after the pointer advances to `0473`. Replay around Access,
Read and Execute must preserve all three writes exactly once. This is
bounded composition of original instructions, not completion of the
whole worker, physical bank arbitration or native recording.

Staged MAR supports stationary, single-word postdecrement/postincrement
and signed linear long-offset modification (`6d80..6d97`, `6de8..6def`).
SPRU172C pages 4-92--4-93 define compatibility-mode AR0 redirection through
ARP and nonzero ARF publication of ARP; the standard mode leaves ARP alone.
The complete slot owns the retained long-offset word and applies its AR
modification once in Access. The following short LD must see the new
pointer with no intervening NOP, consistent with SPRU131G table 7-7's
unlisted-instruction zero-latency rule. This placement is a bounded model,
not measured silicon phase timing or a general DAGEN arbitration claim.

The 294 replay cases cover all eight AR fields, both compatibility states,
three short forms and long offsets `0003`, `ffff`, `8000`. A pointer
mutation after the following LD captures its address must not retarget
that load or cause MAR to modify the pointer again at Execute. Program-bus
taps require exactly one offset fetch per leg, including restoration of
the retained word before Access. Six cases
execute the contiguous original `108a 6d8a fd30 8082 6dea 0003` at
`02:3dcf`--`02:3dd4`, covering both XC outcomes and three checkpoints.
The initial AR2=`0470` decrements twice, then advances by three to `0471`;
an accepted store publishes the captured source at `046e`, despite the
same-clock long MAR update, while a rejected store leaves that location
untouched. Each replay performs one data read and zero or one store.
Circular/bit-reversed/AR0-indexed MAR forms, absolute MAR and general
producer conflicts remain excluded. Complete worker composition is
specified below; live native recording remains a separate gap.

The original worker's `1892` and `1a8a` are **AND** `*AR2+,A` and
**OR** `*AR2-,A`, not memory ADD/SUB. SPRU172C syntax-1 logical opcode
diagrams and the existing atomic decoder agree. The six words at
`02:3dd5`--`02:3dda` are now pinned by the read-only context tool:
`f000 0004 1892 1a8a 6d8a 8082`. The immediate operation is ADD #4;
the following memory operations perform mask/merge, not arithmetic.
Operand meanings remain unclassified.

Staged short AND/OR/XOR A/B now retain their Access-stage address and
Read-stage zero-extended word, then operate on the selected accumulator
in Execute without changing status flags. Basic DP/CPL sampling and
short AR updates use the existing LD address path. The 576 replay cases
cover three operations, both accumulators, four addressing forms, four
value pairs and both SXM states. Changing source RAM after Read must not
alter the result; the unselected accumulator and status remain unchanged.
Register/peripheral operands, extended forms and general source-producer
latencies remain excluded or unvalidated.

Five cases execute the contiguous original six-word logical tail on
page 2. With A=`1234`, AR2=`0470`, mask=`ff0f` and merge word=`00f0`,
the result is `12f8`, written at `046f`. Both memory operands are changed
after Read and AR2 changes after the final destination capture; neither
mutation may affect the retained result or store. Replay around both
logical reads and the final store requires exactly two reads and one
write. This closes the selected tail composition, not the complete
zero-control worker, IRQ entry or a native recorder transaction.

The complete original worker is now separately pinned at
`02:3dbc`--`02:3de1`: 38 words, including its 22-word zero-control body.
The composed fixture also executes the four original CALLD/STM-parameter
words at `02:3d92`--`02:3d95`. Forty-eight cases cover both CMPR/XC
outcomes, both saved INTM states, both SXM states and six replay boundaries.
Each leg requires ten ordered data reads and nine or ten writes,
including the forwarded RMW source through PSHD/POPD, consecutive B high/low
and T publications, the conditional control-word store and final mask/merge
store. The stack balances, the worker restores the saved INTM predicate,
and RETD reaches its page-2 continuation without fetching a third delay
word. The caller continuation is an explicit test marker, not a claim to
execute the rest of the original outer handler.

The non-taken BC retains the already-fetched first body word; its body
timeline is one clock earlier than a redirect-style expectation. With
the selected fixture, ADDM Read is clock 20, PSHD retains the pending result
through the internal bypass before ADDM Write in clock 21, and the final
result is written in clock 41.
The selected CMPR match controls a store in clock 33. The exact values
and clocks are bounded CPU-fixture observations, not measured silicon or
recovered recorder operand provenance.

The logical and worker fixtures use ST1 bit 8 for SXM; bit 9 is OVM.
The corrected 576-case matrix checks zero-extension against actual SXM.
The original outer-context handler is also composed with the active body:
51 handler words plus all 38 worker words, rather than the former
skip-body subset. The 128 cases cover pages 0/2, both saved INTM and SXM
states, both CMPR/XC outcomes and eight replay checkpoints. Every leg
requires sixteen ordered stack writes, seven ordered worker-source reads,
six or seven worker RAM writes, one reserved-return read and one caller
continuation fetch. The worker's B publications contain the handler's
incremented `13572469` counter, while all original caller registers and
ST0/ST1 are restored on return with SP=`1000`. The original compiler-mode
reserved-return store and real caller restoration instructions execute;
there is no synthetic continuation inside this composition.

This closes the original outer-handler/body composition for the supplied
CPU fixtures. Real worker operand provenance, application foreground
progress under short recurring interrupts and coherent native
recording remain unvalidated; handsets and MU4 still use the atomic frontend.

Non-delayed staged FB retains both the seven-bit target page and its
16-bit target word (SPRU172C page 4-53, class 29A). It uses the existing
unconditional-branch redirect/flush path without call stack effects.
Forty replay cases cover source pages 0/2 and target pages 0/1/2/3/7f,
requiring one literal fetch, one target fetch, no sequential instruction
effects and the committed target page. The frontend exposes Fetch-time
XPC before retirement; that intermediate register value is not a claimed
silicon commit phase. General XPC-consumer hazards, FBD/FCALLD and nested
far control/IRQ interactions remain unvalidated or excluded.

The original MU4 timer wrapper is pinned separately by the read-only
context tool. Uploaded `02:204c`--`02:204f` contains
`4a1e f495 f882 3d73`: PSHM XPC (MMR `001e`), NOP and FB `02:3d73`.
The original reserved return at `02:4044` is `f4e5` (FRETE), which restores
the saved page/PC and clears INTM. Thus a complete timer fixture must
include the page save and far return, not merely jump to the handler or
stop at its reserved-return address. Static upload words do not by
themselves validate their low-vector runtime mapping or hardware IRQ
entry; the executable composition below covers a bounded fixture.

The original timer vector, all 89 handler/worker words and original FRETE
now execute together through a CPU-timer interrupt. The 48 replay cases
vary foreground pages 0/3, SXM and CMPR/XC outcome across six checkpoints.
The fixture uses the observed warm lower-32K OVLY mapping, PMST=`2020`,
IMR bit 3, initial TIM=`001f` and PRD=`ffff`: one timer expiry followed by
a deliberately long next period. Those are explicit CPU-conformance
inputs, not a recovered application timer configuration.

Every replay leg requires one timer IACK, one original vector/handler
entry and one FRETE. The eighteen stack writes include the hardware PC,
vector XPC and context saves, with FRAME's reserved return slot skipped
until its final `4044` publication. FRETE consumes the saved XPC then
hardware PC exactly once, returns to the original foreground page and
clears INTM. The unmodified context restoration balances SP and preserves
the caller's saved status/registers. A subsequent foreground marker is
both fetched and retired (B=`66`), so the check does not equate a return
address or speculative fetch with actual foreground execution.

This proves the bounded original ISR composition with supplied CPU
inputs; it does not prove real recorder operand provenance, recurring
interrupt fairness, the warm application's short timer lifecycle or
recording completion. No handset or MU4 profile is switched to the
experimental frontend by this gate.

Recurring original-ISR coverage uses 96 cases, with eight complete timer
returns per replay leg. It varies PRD=`00ff`/`0001`, foreground pages 0/3,
SXM and the initial CMPR outcome, with six checkpoint classes. The
foreground executes an absolute ADDM counter loop; a counter write is a
retirement observation, not a fetched marker. Original worker state is
never reseeded between entries. A successful conditional store changes
the control word from zero to `0470`, so later entries naturally skip
the body; the other fixture continues its original copy/mask progression.
Every case restores caller context, increments the original tick counter
eight times, and reproduces the exact eighth-return clock and foreground
counter after save/restore.

The period comparison is intentionally discriminating. PRD=`00ff`
continues foreground work after the first return: representative cases
finish with 146 or 164 writes, versus five at the first return. With
PRD=`0001`, the original ISR still returns eight times, but the counter
stays at five and timer IFR bit 3 remains pending. Thus completing the
original staged ISR does **not** alone remove the short-period starvation.
These are supplied CPU-timer workloads, not validated DA150 timer-clock
units or proof that the real application should use either fixture rate.
The next boundary is the original warm application's timer configuration,
masking and scheduling lifecycle, not a guessed ISR completion message.

The timed replay harness resumes through the restored checker deadline.
Restoring a stream inside a callback and then scheduling from the cached
pre-load callback time shifts emulated timer phase; the exact-cycle gate
guards against that harness error. Neither device timer semantics nor
the default atomic handset/MU4 path are changed by this fixture correction.

Staged `10f8`/`11f8` absolute LD now retains its 16-bit address word and
Read-stage data, then loads A/B with the selected SXM behavior (SPRU172C
page 4-66, syntax 1/class 3B). The 64 replay cases vary accumulator,
actual SXM bit and four data values across four boundaries. DP and CPL
are deliberately unrelated to the absolute source. Mutating the program
literal after Fetch and RAM after Read must not retarget or change the
load; each leg performs one literal fetch and one operand read. The gate
includes the original idle flag address `a531`, but does not establish
absolute peripheral timing or a complete idle/wake lifecycle.

The read-only context tool pins all 29 original words at
`02:98d0`--`02:98ec` and the 15-word wrapper at `02:3d2f`--`02:3d3d`.
The routine's absolute load of `a531` is followed by BC ANEQ to IDLE1:
zero executes the ordered PRD=`1`, TCR=`20`, TINT-mask, wake-mask and
flag=`1` writes; all 65,535 other input words skip that setup. This is an
exhaustive **local** predicate, not a global flag-writer or reset census.
After wake the routine removes its local frame, restores AR1 and uses
FRET. Its wrapper aligns SP, performs FCALL `02:98d0`, restores CPL/SP
and returns. The staged IDLE1 primitive has the bounded coverage below;
the complete call and bounded timer-ISR composition have the coverage below.

Staged IDLE1 (`f4e1`) is a Decode barrier: older instructions drain,
younger fetched words are discarded, and the continuation is fetched anew
after wake. CPU pipeline clocks stop during the idle interval while the
peripheral timer continues. IMR-enabled requests wake independently of
INTM; INTM=0 accepts the ISR before continuation, whereas INTM=1 resumes
without servicing it. Masked pending requests do not wake the CPU, and NMI
wakes and vectors regardless of IMR/INTM (SPRU131G section 6.11.1).

`check-c54x-core` covers 48 staged IDLE1 cases: external IRQ, masked IRQ
followed by enabled IRQ, timer IRQ, INTM-blocked external/timer wake and
NMI; source pages 0/2; and four save/restore boundaries before entry,
during sleep and after return. The fixture changes its own continuation
word during sleep to distinguish a fresh fetch from stale speculative
execution. ISR cases verify continuation PC and page stack traffic; blocked
cases retain IFR without an IACK. The saved barrier/resume state reproduces
each leg. This verifies the declared restart/refetch model, not a measured
silicon restart bus phase or execution of the original 44-word call.
Staged IDLE2/3 use the same drain/refetch barrier and the shared mode-specific
clock-stop/wake implementation as the atomic frontend. The focused
`idle-modes` profile covers 96 cases: all three modes, enabled external IRQ,
INTM-blocked IRQ, a masked request followed by enabled IRQ, NMI, pages 0/2
and four save/load boundaries. Its timer continues in IDLE1 and freezes in
IDLE2/3; continuation replacement, real ISR stack/return traffic and replay
are checked. The unchanged core fails at the first IDLE2 case with `illegal=1`;
the corrected staged decoder passes all 96. IDLE in a control-delay window
and reserved NN=11 remain excluded. Physical clock/restart latency is not
established by this fixture.

```sh
.venv/bin/python tools/run_c54x_core.py --mame mame/mame \
  --rompath mame/roms --profile idle-modes --output-root tmp/c54x-idle-modes
```

The original AA22 instruction `028922: f5e1` is a concrete native consumer
after a serial playback-control transition; AA99 has different contents at
that address. Instruction admission does not itself qualify that complete
control transaction, EOF draining or handset boot.

The complete original 44-word idle routine/wrapper executes in eight staged
cases with INTM set: zero/nonzero local flag, even/odd caller SP and two
save/restore checkpoints during sleep. The zero path owns exactly the ordered
PRD=`1`, TCR=`20`, IMR TINT-clear, IMR wake-mask and local flag writes.
Its short-period timer remains pending but masked; the nonzero path leaves
timer/masks untouched. A real external IRQ6 wakes both paths without an ISR
under INTM, then original FRAME/POPM/FRET and the wrapper restore AR1,
caller SP and CPL and reach the same-page caller. Both legs retain the
unserviced wake request. These are typed CPU inputs, not a claim about the
real flag's initialization or a promoted MU4 execution profile.

Shared SP updates must respect instruction age: a younger FRAME must not
retarget an older PSHM/POPM's Read-owned stack slot. FRAME's Access placement
remains a partial model, but its update follows those older stack operations
within the clock. The original adjacent PSHM AR1/FRAME pair and final AR1
restoration regression-test this constraint.

Eight additional cases compose the nonzero-flag idle path with INTM clear,
the original 94-word timer vector/context handler/worker/FRETE route and
the observed lower-32K shared program mapping. A real timer expires during
IDLE1, acknowledges TINT with continuation `98ea`, visits each original
entry once and returns before the wrapper reaches its caller. Even/odd SP
and four replay checkpoints cover sleep, active ISR and completed return.
The final caller SP, AR1, B, CPL/page and cleared IFR reproduce without
re-seeding worker state. The initial timer interval and worker inputs are
typed CPU-fixture inputs, not recovered native startup provisioning.
Four zero-flag cases additionally begin with TINT enabled and INTM clear.
The original PRD/TCR writes start the short timer, but the staged frontend
drains the already-decoded ANDM mask before vector execution. The ISR stacks
continuation `98e4`, executes the original context route once and returns to
the ORM wake-mask write. All five setup writes complete, `a531` becomes `1`
and the CPU sleeps with IMR=`08c0`, masked IFR=`0008`, PC=`98ea` and SP=`0ffa`.
Even/odd caller stacks and two in-flight replay checkpoints reproduce this
result without a post-return guard, timer divisor change or forced
continuation. This is a bounded 138-word staged composition, not acceptance
of native warm startup or measured silicon interrupt timing. Global flag
writers, complete surrounding opcode coverage and native warm scheduling
remain open; production MU4 profiles still use the atomic frontend.

Non-delayed staged FCALL now retains a distinct caller-page and page-stack
destination alongside the PC return word (SPRU172C page 4-57, class 29B).
The original opcode word owns the first SP allocation; the complete
two-word slot owns the second. It publishes PC then XPC in separate
clocks, while target Prefetch uses the explicit target page. It must not
save the live Fetch-stage XPC, which can already belong to the callee.
These retained fields are registered for save states. The placement is
a bounded class-29B model, not measured silicon bank arbitration.

The 80 FCALL/FRET replay cases cover source pages 0/2, target pages
0/1/2/3/7f and eight boundaries across literal assembly, both stack writes
and both return reads. FRET consumes saved page then PC; changing their
RAM locations after the respective reads must not alter the return.
The speculative caller continuation is flushed, then fetched again and
retired only after the callee returns. Each leg has exactly two pushes,
two stack reads, balanced SP, unchanged status/RTN and the correct source
page. FCALLD, nested control/IRQ interactions and general stack-bank
conflicts remain excluded or unvalidated. This covers the idle wrapper's
far-call primitive, not execution of the complete original idle call.

Direct STM immediate initialization of AR0--AR7, SP, BK and T is supported;
PMST is the separately covered non-DAGEN exception below. Qualified low-RAM
direct and plain-indirect forms are described above; other MMR destinations
and extended indirect STM forms require their own contracts.

STM immediate to PMST (`771d`, class 12A in SPRU172C page 4-176) retains
its second word and publishes once in Execute rather than inheriting DAGEN
early writes. This publication phase is a declared partial model, not a
measured PMST latency. Sixty-four replay cases cover four full register
values, source pages 2/3, lower/upper program targets and four boundaries
around literal assembly, publication and redirection. Changing the fixture
literal after Fetch must not alter the write. Subsequent lower-bank fetches
use the new OVLY bit; upper targets stay in their original logical page.
The full PMST value (including IPTR), XPC, SP and ST1 reproduce without
duplicate literal fetches or writes. This STM fixture alone does not
establish IRQ/IPTR selection or prefetch ownership; the isolated PMST
profiles in the current overlay contract cover their separately stated
subsets. No native production profile is promoted by these cases.

Short-form MVPD (`7cxx`, destination mode below `98`) retains its absolute
program-source literal separately from the data destination. Its Read stage
owns destination addressing/postincrement; an extra held program-bus clock
samples program space through the source instruction's logical page and
current OVLY mapping. Execute writes that retained word once. SPRU172C
pages 4-117/118 distinguish this program-to-data transfer from a data move;
the held Read placement is a declared partial model of its extra cycle,
not measured bus arbitration.

The 128 replay cases cover DP/stack direct, indirect and postincrement
destinations, source pages 2/3, lower/upper source regions, both OVLY states
and four checkpoints around literal assembly, held program read and data
write. Mutating the literal after Fetch and source RAM after its read cannot
retarget or change the stored word; each leg has one literal fetch, one
program read and one data write. Long-offset forms, register/peripheral
destinations and control-delay placement fail closed. Interrupt/mapping
changes during an ordinary held read remain unvalidated.

Long-immediate RPT (`f070`) now owns a retained short MVPD body. Repeat
Decode disables acceptance, including NMI, before its literal finishes;
complete body Decode discards younger speculative words. The first MVPD
uses its held program read, then the stream performs one program read and
data write per clock, with implicit source increment and encoded destination
modification. Exactly count+1 transfers retire before a fresh continuation.
First/steady-state bus placement is partial, not measured silicon timing.
MVPD and the separately covered READA stream below are the only admitted
repeat bodies. Nested repeats and control-delay RPT fail closed.

The 128 transfer replay cases cover counts 0/1/3/15, pages 2/3, both OVLY
states and indirect/postincrement destinations, with four checkpoints across
capture, initial read, streaming and completion. Sixteen additional cases
present IRQ or NMI at RPT Decode and replay with the request pending. IACK
must follow the final write, stack continuation `9004` and restore a balanced
stack before the changed continuation retires. Count-zero pending-interrupt
cases drain an additional already-prefetched continuation before discarding
it; it must not retire. No timer/return guard or atomic fallback is used.

The original `bootrecstage` reset sequence also executes its full 65,313-word
copy: observation-only taps verify every program source address, every data
destination and value equality, then the diagnostic reaches `00:0e41`.
This closes the reset copy, not native startup or recorder completion; the
current unsupported startup boundary is in
[the MU4 contract](5510_bringup.md).

Absolute ADDM to SP (`6bf8 0018 lk`) is the covered exception to ADDM's
register/peripheral exclusion. It retains the SP Read operand and immediate,
then publishes the wrapped result in Execute using the existing ADDM
arithmetic. The original serial-start prefix `STM #1200,SP; ADDM #03ff,*(SP);
ANDM #fffe,*(SP)` therefore selects SP=`15fe`, without a firmware-state poke.
The 256 replay cases cover four initial values, four deltas (including wrap),
both SXM states, two pages and four boundaries around retained Read/write.
Bus taps verify exactly three ordered SP reads and writes, including a later
LDM read of the aligned result. Other ADDM register destinations stay
fail-closed. General compiler-SP latency, concurrent stack operations and
register-bank conflicts for this RMW form remain unvalidated; these cases
prove the bounded original prefix, not every SP consumer hazard.

Immediate LD #k,ARP (`f4a0`--`f4a7`) has no latency to its following
indirect consumer (SPRU131G tables 7-3/7-15 and example 7-41). The staged
model publishes ARP once in Access, after older stack reads; Execute must
not overwrite a younger indirect instruction's ARP update. This publication
phase remains partial. In CMPT mode encoded ARF=0 selects loaded ARP;
nonzero ARF selects its encoded register and updates ARP. Standard mode uses
the encoded register without that update; TI requires software to keep ARP
zero in standard mode.

The 512 replay cases cover all eight immediate values, encoded ARF=0/2,
both CMPT states, with/without postincrement, two pages and four checkpoints
around publication, destination selection and retained operand Read.
Nonzero ARP in standard mode is an adversarial CPU fixture, not handset
provisioning. Each leg verifies one source read, only the selected auxiliary
register's update and preservation of unrelated ST0/ST1 bits. Mutation after
Read cannot alter the loaded accumulator. General ST0-load/ARP conflicts,
CMPT-change hazards and interrupt acceptance during publication remain open.

READA (`7exx`) captures accumulator A as its program address; extended
devices use A(22:0), independent of the instruction's XPC (SPRU172C page
4-136). The short destination forms and absolute `7ef8 dmad` retain their
data destination separately. Four extra held Read clocks model the
instruction's documented non-repeat cost; A is sampled after older Execute
publications, then the selected program word is retained for one Execute
write. Sampling and bus placement remain partial, not measured silicon
phases. RAM and AR0--AR7 destinations are admitted for nonrepeated READA;
the repeated RAM stream is covered separately below. Other register/
peripheral destinations, long-offset forms and control-delay placement fail
closed.

The 192 absolute-to-AR1 replay cases compose a preceding ADD #1,A with
READA, three source pages (0/2/7f), lower/upper program regions, both OVLY
states, zero through three consumer gaps and four checkpoints. Mutation of
A after source capture and program RAM after Read cannot retarget the
transfer. Younger indirect consumers with zero/one gap retain their old
AR1 address; two/three gaps select the published pointer under the declared
stage model. Forty-eight short RAM/postincrement cases independently check
the one-word form's program read, data write and single auxiliary update.
Each replay leg has one program read and one destination write. General
DAGEN conflict chains, conditional-execute placement and interrupt acceptance
across an ordinary held bus remain unvalidated; repeated RAM READA has the
separate coverage below.

Short non-delayed BANZ (`6c80`--`6c97`) retains its target literal, tests
the selected auxiliary register before modification and owns its encoded
update in Access. A taken branch discards the two sequential Decode slots;
a zero predicate leaves the fall-through intact. SPRU172C page 4-16 assigns
class 29A four taken/two not-taken clocks; the Access redirect models that
gap without BC's additional conditional-evaluation bubble. Exact publication
and bank arbitration remain partial.

The 288 replay cases cover values 0/1/ffff, no modification/decrement/
increment, encoded ARF=0/1, both CMPT states, pages 0/2 and four checkpoints
around predicate/update/redirect. Tests distinguish zero-before-decrement
from nonzero-after-decrement and one-before-decrement from zero-afterward.
Only the selected AR changes, once, on either outcome; BANZ does not read or
write data memory. Mutation of its target literal after Fetch cannot
retarget it, and taken branches must not retire fall-through markers.
Delayed/long-offset forms, repeat and control-delay placement fail closed.
Preceding DAGEN-writer conflicts, conditional-execute placement and interrupt
acceptance through the redirect remain unvalidated.

Absolute memory-sourced RPT (`47f8 Smem`) samples its AR0--AR7 or RAM
count in Read and retains it through two extra clocks (SPRU172C page 4-146,
class 5B). Repeat protection begins in Decode, not after that Read. The
common repeat stream now also admits RAM-destination READA: it retains A's
initial program pointer separately, increments that internal pointer per
iteration and does not update architectural A. A one-word READA body resumes
at body+1; the two-word MVPD body still resumes at body+2. Bus placement is
partial. Other count addressing forms and control/peripheral count sources
remain excluded; repeated READA register destinations fail closed.

The 384 replay cases cover counts 0/1/3/15, program pages 0/2/7f, both OVLY
states, ordinary and 7fff-to-8000 crossing sources, AR1/RAM count sources and
four checkpoints around count assembly/Read, captured program pointer and
streaming. Mutating the count after Read, A after pointer capture and the
first source word after its program Read cannot change the copy. Each leg
has one count read and exactly count+1 ordered transfers and postincrements.
Sixteen AR1-count cases additionally present IRQ/NMI at repeat Decode and
replay with the request pending: IACK follows the last write and stacks the
one-word body's actual continuation. The ISR's discarded prefetched words
are distinguished from copy-source reads. No source re-seeding, return
guard or atomic fallback is used. General preceding-writer conflicts,
conditional-execute placement and repeated register/peripheral publication
are not established by these fixtures.

Absolute accumulator ADD (`00f8`/`01f8`) retains its source-address literal,
selects the AR0--AR7 or RAM operand in Access and samples it once in Read.
Execute uses that word with the existing 40-bit ADD arithmetic; source
mutation after Read cannot change the result. Other register/peripheral
sources, shifted/short forms, repeat and control-delay placement remain
excluded. This does not infer a physical MMR arbitration delay.

The 2,048 replay cases cover four operands, four accumulator values (including
32-bit overflow and 40-bit negative values), both SXM/OVM states, A/B,
AR1/RAM sources, two pages and four checkpoints around operand ownership
and retirement. Independent integer expectations verify the result, low-32
carry, destination overflow flag and OVM saturation; the other accumulator,
SP and unrelated status bits are preserved. Each leg has one literal fetch
and one operand read. Conditional-execute placement, preceding accumulator/
status-writer hazards and peripheral source timing remain unvalidated.

Short direct ANDM/ORM/ADDM now admit compiler addressing as well as DP
addressing. CPL/DP belong to complete Decode; SP is sampled in Access using
the existing basic-compiler visibility path. The effective address and Read
operand are retained through Execute, rather than consulting live CPL/SP/DP
again. The same retained context also applies to noncompiler direct RMW;
absolute and indirect forms retain their existing ownership.

The 768 replay cases cover three operations, two offsets, four data values,
two immediates, both initial CPL states, two program pages and four checkpoints.
They deliberately change CPL after Decode, SP/DP after Access, the immediate
after Fetch and RAM after Read. Each leg still has one literal fetch, one
operand read and one write to the original frame/page address, while the
later context changes survive. This is a CPU ownership fixture, not a
firmware-state intervention. General SP-producer/stack interleavings,
memory-sourced DP forwarding, conditional-execute placement and RMW
peripheral arbitration remain unvalidated.

Short accumulator AND/OR/XOR now admit peripheral-window sources
`0020`--`005f` through the same paid extra-access-clock path as peripheral
loads. Lower register sources remain excluded. Access owns the effective
address and auxiliary modification; the wait holds the stages without
issuing a read, then Read samples the peripheral once and Execute consumes
the retained zero-extended word. This models CPU access cost, not a specific
peripheral's electrical response latency or bus-priority arbitration.

The 864 replay cases cover three operations, DP direct/indirect/postincrement
addressing, three PRD values, A/B, both SXM states, two pages and four
checkpoints before/during/after the wait and publication. Each instruction
has one peripheral read, no peripheral write and at most one auxiliary
update. A later PRD mutation cannot change the retained logical result.
Fixture mutations and debugger peeks are excluded from instruction bus
counters: timer state import/export traverses MMIO. The tests do not establish
GPIO `003c`'s electrical contract; the native bench maps it through the
existing GPIO model. IRQ overlap and simultaneous peripheral read/write
arbitration remain separate work.

PORTW uses opcode family `75xx`, not MVKD (`70xx`). The short and absolute
forms retain data-source and I/O-destination operands separately (SPRU172C
page 4-130, class 28A/28B). Absolute `75f8` consumes source address, then
port address; Access owns source addressing/modification, Read captures its
word and Execute writes through AS_IO. Peripheral data sources use the
existing extra-access-clock path. External READY/IOSTROBE timing is not
modeled by these minimum-cycle fixtures. Repeat, long-offset and control-delay
forms remain excluded.

The 320 replay cases cover absolute low-accumulator/RAM/peripheral sources,
short postincrement and compiler direct sources, four values, both SXM
states, two pages and four ownership checkpoints. Each leg has the original
literal order, one source read, one real I/O-handler write and at most one
auxiliary update. Mutating the literals after assembly and source after Read
cannot retarget/change the write. This is CPU transfer conformance, not
validation of a connected peripheral's behavior.

The MU4 diagnostic's `75f8 0008 0080` reads AL at data MMR `0008` and writes
external port `0080`. That port has no modeled device; `bootrecstage` exposes
an observation-only diagnostic sink with no invented state or response.
A following unsupported Decode can stop an older accepted instruction before
retirement, so advancing the reported opcode boundary alone does not prove
the external write occurred. With compiler immediate ST covered below, the
sink now observes value `00df` at port `0080`; its external meaning remains
unknown. Existing atomic profiles are unchanged.

Short immediate ST now retains complete-Decode CPL/DP and Access SP for its
direct destination, admitting compiler stack-frame stores. Read presents the
cached address and Execute writes its original immediate without reading the
destination. This reuses the basic compiler-addressing path rather than
consulting live context during the store.

The 128 replay cases cover four values (including opcode-shaped `f495`), two
offsets, both initial CPL states, two pages and four ownership checkpoints.
They mutate CPL/literal after Decode and SP/DP after Access. Each leg still
has one literal fetch, no destination read and one write to the original
address, preserving the later context changes. General SP-producer/stack
interleavings, memory-sourced DP forwarding, conditional-execute placement
and peripheral-write arbitration remain unvalidated.
Supported compiler-mode, stack, branch/call/return and conditional-execute
contracts are specified below. Remaining forms fail closed without atomic fallback.

Direct STLM A/B to AR0-AR7 publishes the saturated low accumulator word in
Execute, with no destination read and no DP/CPL-derived address. SPRU172C
4-175 establishes page-zero addressing; SPRU131G 4.3.3 establishes SST/SXM
saturation, table 7-5 places store-type DAGEN writes in Execute, and table
7-7 requires two intervening instructions for a normal indirect AR consumer.
The source/saturation phase remains this partial frontend's Execute model,
not an independently measured silicon phase. Indirect STLM and other MMR
destinations remain unsupported. STLM also participates in the existing
DAGEN-bank conflict checks for younger STM, MVDK and POPM early writes.

The 768 replay cases cover all eight AR destinations, both source
accumulators, four input patterns, both SST/SXM states, zero through three
intervening NOPs and saves after Decode, Read or Execute. Each leg verifies
one register write, one consumer read, an unchanged source accumulator and
the old/new pointer boundary at two intervening instructions. Original MU4
startup advances through `8811` at `00:3092`. This remains coverage evidence,
not native startup or recorder acceptance; existing atomic profiles are unchanged.

Extended DST A/B now retains its second-word offset/absolute address in
Decode. Access applies table 5-4's ordinary offset (AR unchanged), linear
preincrement or circular preincrement once; absolute addressing uses the
retained literal without consulting AR. Read captures the saturated pair
after older Execute writes. Execute writes the high word, then the held
clock writes the low word at `address ^ 1`, preserving odd-address ordering
(SPRU172C 4-47). The extra extension word adds one clock to the existing
two-write path. Stage placement remains partial, not measured silicon.

Short-indirect DST A/B uses the same retained pair and two-write path.
Access applies MOD 0--11 once: linear increment/decrement and circular
unit steps are two words, while AR0-indexed and reverse-carry steps retain
their index semantics. The first write is clock 6 and the paired write is
clock 7 in this partial frontend model. Another 9216 replay cases cover
all twelve modes, eight encoded registers, both accumulators, even/odd
addresses, CMPT and SST, with signed-store source patterns and BK=16.
They poison AR/BK after Access and the accumulator after Read, and restore
after Decode, source capture or the first write. No destination reads,
duplicate pointer updates or duplicate writes are permitted. Unsigned
saturation, arbitrary BK regions and control-delay compositions for these
new short forms remain outside this fixture matrix.

The 3072 replay cases cover four address forms, eight encoded registers,
both accumulators, four offsets/patterns, both compatibility and SST states,
and checkpoints after Decode, Read or the first write. They change the
literal after Decode, AR/BK after Access and source accumulator after Read;
the original address/pair still publishes exactly once with no destination
read. Opcode-shaped offset `f495`, negative offsets and both address parities
are included. Register/peripheral destinations, control-delay placement,
general bank arbitration and circular-region provenance remain unvalidated;
the first two fail closed. The original upload passes `4ee1` at `00:309b`,
still without native startup/recorder acceptance or atomic fallback.

Zero-shift long-offset LD A/B (`10xx/11xx`, modes `e0..f7`) retains
the signed offset in Decode. Access uses the shared ordinary-offset,
linear-premodify or circular-premodify helper once; Read samples RAM or
AR0--AR7 once; Execute sign-extends according to SXM and publishes only
the selected accumulator. The existing absolute `f8` path is unchanged.
The offset literal adds one clock: Read is clock 6 and Execute clock 7
in this partial frontend model. These are declared stage placements, not
measured silicon timings. New offset forms reject other control/peripheral
sources and control-delay placement before any source read.

The 12288 RAM replay cases cover the three offset forms plus absolute,
eight encoded registers, both accumulators, eight offsets/patterns,
SXM/OVM and compatibility mode. Another 384 cases resolve an ordinary
offset into each mapped AR source under both SXM states and both destinations,
including a source/address-register alias. Fixtures change the literal after
Decode, the selected pointer/BK after Access and the source word after Read;
replays after Decode, Access or Read require one source read, no data writes,
unchanged non-ARP status and an unchanged unselected accumulator. Arbitrary
circular-region provenance, operand-bank conflicts, XC/interrupt composition
and silicon phase fidelity remain unvalidated.

Unsigned memory load LDU (`12xx/13xx`) shares the retained LD address/
Read path, but Execute always publishes the raw 16-bit sample with all
upper accumulator bits clear, independently of SXM and OVM. Like signed LD,
it excludes write-only short MOD=3. LDU admits direct DP/CPL,
the eleven read-capable short-indirect modifiers and the offset/absolute forms; other
control/peripheral sources fail closed before Read, while mapped AR0--AR7
are explicitly covered. Extended control-delay placement and repeats
remain unsupported.

The 52,224 RAM replay cases cover seventeen address forms, eight encoded
registers, both destinations, eight values including `8000,ffff,f495`,
SXM/OVM, compatibility mode and Decode/Access/Read saves. Another 2304
cases cover each mapped AR source through direct, absolute and ordinary
offset addressing, including an address/source-register alias. The shared
signed-load matrix remains a separate acceptance marker. Fixtures change
the literal after Decode, AR/SP/BK after Access and the source after Read;
each replay requires one source read, no data writes and no literal refetch.
Every fixture tick checks that only Access-owned ARP changes before Execute,
and that the accumulator publishes at clock 6 for short or 7 for extended
forms in this partial frontend model. These checks are not silicon timing
evidence. Physical memory-bank arbitration, mixed status/producer hazards
and wider control/interrupt composition remain unvalidated.

Indirect DLD A/B (`56xx/57xx`) admits short MOD 0--11 except write-only MOD=3, and long
offset/absolute MOD 12--15. Decode retains an extended operand literal;
Access applies the shared long-word address helper or short pointer step
once. Linear and circular unit steps are two words; AR0-indexed and
reverse-carry steps retain their index semantics. Read retains the high
word at `address`, then the low word at `address ^ 1`. Execute combines
the 32-bit pair and extends its high sign bit into the guard byte when
SXM is set, independently of C16. It changes only the chosen accumulator
and any Access-owned compatibility ARP update.

The 23,040 replay cases cover fifteen address forms, eight encoded
registers, both accumulators, four payload patterns, both address parities,
SXM/C16 and compatibility mode. They change extended literals after
Decode, AR/BK after Access and both source words after Read. Saves in
Decode, Access or Read require exactly two ordered source reads and no
data writes on each replay. Short paired reads occur at clock 5, extended
paired reads at clock 6, with publication one clock later in this partial
frontend model. This does not establish physical bus arbitration or
silicon phase timing. Register/peripheral pairs and extended control-delay
placement fail closed; arbitrary BK regions and short control-delay,
XC/interrupt and source-bank conflict compositions remain unvalidated.

Long-offset BANZ (`6cxx`, modes `e0..f7`) is a three-word branch:
opcode, signed offset, then same-page target. It is not a memory RMW
instruction. Decode retains both literals separately. Access tests the
resolved offset address for zero, with ordinary offsets leaving AR
unchanged and linear/circular premodification updating AR once. Thus an
ordinary `AR=1, offset=-1` falls through while retaining AR=1. No data
memory is read or written. The retained decision redirects taken paths
to the original target even if AR/BK or the program literals subsequently
change. Instruction extent is three words for control and retirement
bookkeeping; existing two-word short BANZ remains covered independently.

The 2304 replay cases cover all three offset forms, eight registers, eight
initial-AR/offset pairs, compatibility mode, two program pages and saves
after the first literal, complete Decode or the Access decision. They
cover zero/nonzero resolved addresses, 16-bit wrap, circular wrap and
opcode-shaped offset `f495`. Taken paths fetch the retained target once
at clock 7 in this partial frontend model; not-taken paths execute the
fallthrough markers without fetching the target. Predicate/redirect phase
timing is not silicon-validated. BANZD, broader short-modifier coverage,
arbitrary BK regions and XC/interrupt or preceding DAGEN-writer composition
remain outside this fixture matrix.

Short-indirect BANZD (`6e80..6e97`, SPRU172C BANZ[D] 4-16) shares
BANZ's Access-owned pre-modification count, selected AR update and retained
same-page target. A taken delayed branch redirects Prefetch without flushing
the two delay words; a not-taken branch retains its sequential continuation.
The existing control-delay fence covers the branch and both delay words.
The 3,072 replay cases cover all eight AR selectors, values `0/1/ffff/8000`,
stationary/postdecrement/postincrement modes, CMPT, two program pages and
both two one-word instructions and one two-word instruction in the delay.
Target poisoning after Decode and count changes after Access must not alter
the outcome. Delay effects publish before the chosen continuation; snapshots
cover retained target/count and both delay-publication clocks. Tests require
one target fetch when taken, no data accesses and no repeated AR update.
These are partial frontend timings, not measured class-29 silicon timing.
Long-offset/preincrement/circular BANZD and new pending-IRQ cases are not
covered by this matrix; unsupported delayed addressing forms stay rejected.

Short-memory immediate ST repeat bodies (`76xx`, modes below `98`) retain
one opcode and literal. ST is not in SPRU131G table 6-17's accelerated-repeat
list; its declared stream therefore retains separate address and write clocks
for each subsequent iteration. The address clock captures EAB and updates the
selected pointer once, and the write clock publishes the original literal and
consumes one repeat count. Pending address/phase are saved explicitly. The
38,400 replay cases cover DP, CPL, stationary AR1, postincrement and
postdecrement; short/long RPT; counts `0,1,15,32,255`; literals
`0000/0001/8000/ffff`; SST/SXM/OVM; two program pages and none/IRQ/NMI.
They require count+1 writes, one opcode/literal fetch after poisoning, no
destination reads, unchanged accumulators/ST0, correct half-iteration
pointer/counter ownership, deferred interrupt return and restoration before,
between or after writes. Long-offset/absolute, preincrement and
circular/bit-reversed repeated ST forms remain rejected, as do mapped/
peripheral destinations. Clock placement is partial, not measured silicon.

SUBC (`1exx/1fxx`, [SPRU172C 4-192](https://www.ti.com/lit/ug/spru172c/spru172c.pdf))
has single-step staged coverage. Access retains the source address, Read
samples one memory word and Execute performs the conditional division step
through the helper shared with atomic execution. SXM extends the divisor;
the ALU difference is `src - divisor*32768`. A nonnegative difference yields
`difference*2+1` and C=1; a negative difference yields `src*2` and C=0.
The result wraps to 40 bits without OVM saturation. The helper now projects
sticky OVsrc from the ALU difference being outside signed 32-bit range;
this follows the core's common ALU-overflow model but the exact SUBC
overflow-edge mechanism remains unvalidated against silicon. This replaces
the atomic path's complete omission of OVsrc, not a claim of final fidelity.
The 30,720 replay cases cover six RAM forms with AR1/AR6, all eight mapped
AR sources, both accumulators, 64 dividend/divisor pairs and SXM/OVM.
TI's intended division domain uses positive dividends and divisors; negative
or guarded fixture values test the declared model, not validated division use.
Changed RAM/pointers and Execute-live accumulator inputs distinguish operand
ownership; Decode/Access/Read snapshots check one read and no data writes.
Other addressing forms and mapped accumulator/T/TRN inputs remain outside
this matrix. Control-delay SUBC remains rejected.

Short-memory SUBC repeat bodies now use the retained repeat stream after
the first staged iteration. Each subsequent clock reads the current divisor,
updates the selected pointer once, applies the shared conditional-subtract
helper and consumes one repeat count. The opcode is fetched once; DP/CPL
context is retained, and interrupts remain pending until count+1 executions
complete. The 38,400 legal replay cases cover DP, CPL, stationary AR1,
postincrement and postdecrement; short/long RPT; counts
`0,1,15,32,255`; both accumulators; four dividend/divisor pairs; SXM/OVM;
two program pages and none/IRQ/NMI inputs. Decode/first/middle/final
snapshots check arithmetic at every iteration, one divisor read per iteration,
no opcode refetch after poisoning, correct pointer footprint, deferred IRQ
return address and only interrupt-owned stack writes. TI's independent
65/7 example must produce `00020009` after sixteen iterations. Long-offset,
circular/bit-reversed and mapped/peripheral repeated sources remain rejected.
These checks establish the declared repeat model, not silicon timing or the
unresolved SUBC overflow-edge behavior.
Short MOD=3 preincrement is write-only, not a valid SUBC source. A separate
512-case replay bank covers its rejection in both frontends, standalone and
under short RPT, without data, pointer or arithmetic-result effects. The
adjacent repeated immediate-store bank retains its 38,400 legal cases.

Variable-shift memory ADD/SUB (`6fxx` with operation-extension masks
`0c00/0c20`, SPRU172C extended Smem arithmetic) retains ordered address
and operation extensions. Access owns DAGEN; Read samples memory once.
Execute extends according to SXM, applies the signed five-bit shift without
independent shift flags, and adds/subtracts into the selected destination
using the live source accumulator and the shared carry/OVM/overflow helper.
The 245,760 replay cases cover ADD/SUB with all four source/destination
pairs, all 32 shifts, SXM/OVM and four memory/guarded-accumulator patterns.
RAM cases cover DP, CPL, postincrement, long offset/no-update,
long preincrement and absolute forms with AR1/AR6; mapped cases cover all
eight AR registers via absolute addresses. Changed pointers and poisoned
extensions test address retention; changed memory after Read tests sampling;
changed accumulators between Read and Execute test live-source ownership.
Decode/Access/Read snapshots check no duplicate reads or data writes.
Other allowed addressing modes and mapped accumulator/T/TRN sources are
not covered by this matrix; producer interlocks, physical bank timing,
repeat bodies and control-delay slots remain unvalidated/restricted.

Variable-shift STL/STH (`6fxx` with `0c80/0d80` or `0c60/0d60`
operation extension, SPRU172C syntax 4 classes 11A/11B) retain address
then operation extensions. Read presents the destination and updates its
pointer once; Execute uses the live source accumulator, applies the signed
five-bit shift, applies SST with SXM, and writes the selected low/high word.
There is no destination read or arithmetic-status update. This follows the
existing store frontend's partial Read-EAB/Execute-EB placement, not measured
silicon operand timing. The 172,032 replay cases cover seven long-offset/
absolute forms (AR1/AR6, offset/no-update, preincrement, circular and absolute),
four offsets, both sources, both word halves, all 32 shifts, SST/SXM and
four guarded/sign-boundary accumulator patterns. Extension poisoning,
changed pointers after Read and changed source before Execute distinguish
address retention from live-source sampling; Decode/Read/Execute snapshots
check one write and no repeated pointer update. Short forms are admitted
but not covered by this matrix; mapped/peripheral destinations, repeat
bodies and control-delay slots remain rejected.

Variable-shift memory LD (`6fxx` plus `0c40/0d40` operation extension,
SPRU172C LD syntax 4) retains the address-extension word, when present,
before the operation extension. Access owns DAGEN and Read samples memory
once; Execute applies SXM and the signed five-bit shift through the shared
shifted-load helper. Its 104,448 replay cases cover 17 read-capable RAM address forms,
AR1/AR6, both accumulators, four values, all 32 shifts and SXM/OVM states.
Separate Decode/Access/Read checkpoints, poisoned extension words and
changed sources/pointers check retention and single publication. The 16-bit
source and shift range `-16..15` cannot exceed signed 32-bit range on a
left shift, so these fixtures check preservation of existing overflow flags
rather than claiming an unreachable new overflow. Mapped `0008..0017`
sources are permitted but are not covered by this matrix. The shifted
store and arithmetic extensions have separate coverage; other `6fxx` operation extensions,
repeat bodies and control-delay slots remain rejected.
The stage assignment remains a partial frontend model, not measured timing.

Memory LD shifted by 16 (`44xx/45xx`, SPRU172C LD syntax 3) shares
the retained memory-load address and Read path. Execute sign-extends
according to SXM, shifts by 16 and masks to 40 bits, without changing
OVdst, carry or other status flags. Its 54,528 replay cases cover 17 RAM
address forms, all AR selectors, both accumulators, eight memory values,
SXM/OVM combinations, compatibility addressing and mapped AR sources.
Snapshots at Decode, Access and Read check one sample, one address update
and no data writes; source/literal/pointer changes after their sampling
boundaries must not alter the result. Mapped accumulator/T/TRN sources
are permitted but are not covered by that matrix. Control/peripheral
sources below `0060` outside `0008..0017` remain rejected. The stage
assignment is a partial frontend model, not measured silicon timing;
repeat and delayed-control bodies remain restricted separately.

Immediate MPY (`f066/f166`, SPRU172C MPY syntax 4, class 2) retains one
literal in Decode and signed T in Read before older Execute. Execute forms
the signed 16x16 product, applies FRCT and publishes through the shared
destination overflow/OVM helper; it does not rewrite T or access data memory.
The 86,016 replay cases include 12,288 isolated operations with 64 value
pairs, both destinations, FRCT/OVM/SXM/SMUL and two program pages.
The 73,728 producer cases add both STLM sources/destinations, SST and
`0..2` intervening NOPs. Gap-zero STLM consumers remain an out-of-contract
stage projection; the defined spacing cases are distinguished from them.
Opcode-shaped literals are poisoned after Decode and T changes after Read;
complete-Decode, Access and Read snapshots check one literal fetch and no
data accesses except the optional producer's single register write. Source
and destination differ independently in the producer matrix. SXM does not
reinterpret the multiply's signed operands; SMUL's pre-MAC/MAS saturation
does not apply to this standalone multiply. Physical bank/interlock timing,
repeat bodies and delayed-control slots remain unvalidated/restricted.

Direct STLM now admits T/TRN (`880e/890e/880f/890f`) in addition to
the covered AR destinations. It ignores DP/CPL, publishes the saturated low
source word in Execute and does not read its destination. SST handling is
supported by SPRU131G's saturation-on-store list, which explicitly includes
STLM; the instruction's unchanged status flags do not imply SST is ignored.
T latency depends on the consumer: SPRU131G 7.5.6/table 7-13 distinguishes
multiply from Category-I TS consumers. The multiply path retains T in Read
after older Execute but before younger Read publications. A zero-gap
STLM-to-multiply therefore sees the published value without a separate
store-forwarding mechanism; this does not relax spacing for TS consumers.
The 12,288 replay cases cover A/B sources, eight guarded/boundary values,
SST/SXM/OVM/FRCT, gaps `0..3`, MPY/MPYR/MPYU T consumers and LDM TRN
consumers. Source changes after Read test Execute-live STLM sampling;
changed T/TRN and RAM after consumer Read test retained operands.
Decode/Read/consumer snapshots require one register write, one consumer
read, unchanged source and no replay duplication. Gap-zero TRN behavior
remains a stage projection, not a measured silicon guarantee. Spaced consumers
have distinct acceptance coverage; other non-DAGEN MMR destinations and
indirect STLM forms remain outside this extension.
The focused `stlm-t` runner uses the separate `tms54xtra` fixture namespace
because `tms54test` has exhausted its eight-bit BIOS index. It also checks
3,072 STLM BRC/LDM cases and restores; neither bank substitutes for an
interrupt or block-repeat composition test.

Memory LD T (`30xx`, SPRU172C LD syntax 1 at 4-70) publishes one
retained Smem sample in Read, after older Execute. Retirement advances the
instruction but never rewrites T. Signed MPY/MPYR (`20xx..23xx`) retain
Smem in Read and T after older Execute; Execute multiplies signed 16-bit inputs, applies FRCT,
rounds MPYR by adding `8000` and clearing the low word, then applies the
existing destination OVM/overflow handling. SXM does not reinterpret their
signed operands. Each family has 52,224 RAM/mapped-source replay cases;
LD T preserves both accumulators and all arithmetic flags. Changed T after
Read distinguishes retained multiply operands from live Execute inputs.
The additional 768 adjacent `30e1 0004; 20e1/21e1 0003` fixtures cover
eight T values, four memory values, both destinations and FRCT/OVM across
three restoration points. They require loader Read at clock 6, consumer
Read at 8 and result publication at 9, with no resampling or data writes.
These are explicit partial frontend clocks, not measured silicon interlocks;
repeat and delayed-control bodies remain restricted separately.

Unsigned memory MPYU (`24xx/25xx`, SPRU172C 4-106) retains one
Smem sample and its unsigned T input at Read, before older Execute work.
Execute multiplies the retained 16-bit unsigned operands, applies FRCT,
then the existing OVM/result-overflow behavior; it does not publish T.
The partial Read ownership is not measured silicon interlock timing.
Its 52,224 replay cases cover both accumulators, T=`8000/ffff`, eight
memory/source patterns, 16 flag combinations, nine RAM address forms and
all sixteen mapped register sources `0008..0017`. Decode/Access/Read
snapshots, changed T after Read, poisoned literals and changed RAM/pointers
check no resampling or duplicate pointer update. PMST.SMUL does not
saturate this unsigned product before destination publication; repeat and
control-delay forms remain separately restricted.

Memory MPYA/MASA/MACA/MACAR (`31xx/33xx/35xx/37xx`) retain one Smem
sample through Read and use a shared operand-only arithmetic helper at
Execute. The multiplier uses signed A bits 32--16 as a 17-bit value and
a signed 16-bit memory operand, independently of SXM. A's lower word and
bits above bit 32 do not participate. FRCT doubles the product. MPYA
overwrites B; MASA subtracts from B; MACA adds to B; MACAR adds and rounds
by adding `8000` then clearing the low word. The existing product/result
SMUL/OVM policies are shared with the atomic decoder, not newly inferred.
A and carry remain unchanged; result overflow affects sticky OVB.

TI SPRU172C specifies memory-to-T publication in Read for these forms.
The staged path publishes retained memory to T after older Execute in
that clock and does not write T again at retirement. B and its flags
publish in Execute. The 52,224 replay cases cover 27,648 RAM cases over
nine read-capable address forms and 24,576 mapped-register cases over AL/AH/AG, BL/BH/BG,
T/TRN and AR0--AR7. They cover all four operations, eight independent
input patterns and FRCT/OVM/SMUL/SXM combinations. Saves in Decode,
Access or Read require one source sample, no data writes and no literal
refetch. RAM/literals/pointers are changed after their owning phases;
T is deliberately changed after Read to prove Execute cannot republish it.

Short Read/T publication is clock 5 and B publication clock 6; extended
forms add one clock in this partial model. A/B/flag sampling, operand-bank
arbitration and older/younger producer interactions are not independently
silicon-validated. The RAM matrix uses AR1/AR6 with BK=16 and standard
addressing; broader modifiers/compatibility and status/producer conflicts
remain outside it. Other control/peripheral sources, repeat bodies and
control-delay composition fail closed. Opcode coverage is not native
startup or speech acceptance.

Register ADD/SUB admits the one-word encoded-shift forms (`f400/f420`
families) and ASM-selected forms (`f480/f481` with source/destination
selectors). These known register-only operations use the existing atomic
arithmetic semantics at staged Execute; they fetch no operand and perform
no control redirect. Unknown families still fail closed in Decode.
Register SUB computes destination minus shifted source, unlike immediate
SUB's source-minus-literal form. Aliasing uses the original source and
destination values. Right shifts fill from bit 39 under SXM and otherwise
zero-fill; left shifts discard bits above the 40-bit accumulator. Carry
is computed before OVM saturation and destination overflow is sticky.
The unselected accumulator and other status fields are preserved.

The 196608 standalone replay cases cover ADD/SUB, all four A/B selections,
every signed shift -16--15, both shift sources, eight independent A/B
patterns, eight SXM/OVM/sticky-overflow states and two pages. A bit-level
shift oracle is independent of the CPU helper. Encoded-shift fixtures set
ASM to an unrelated -13, while ASM fixtures use its signed five-bit value.
Saves during Decode, Access or Read require one opcode fetch, no data
access and publication only at Execute, clock 6 in this partial model.
Another 288 producer/replay cases compose a signed immediate load and
register ADD/SUB through zero, one or two NOPs, checking aliasing, retained
load literals and drainage to IDLE. Load publishes at clock 7 and register
arithmetic at `8 + gap`. This is not silicon phase evidence; mixed status
producers, wider operand conflicts, repeat and delayed-control/interrupt
composition remain unvalidated.

Shifted register LD (`f440` family and ASM-selected `f482`, with source/
destination selectors) uses the known one-word register-only Execute path.
It overwrites the destination with the shifted original source rather than
combining the two accumulators. Right shifts sign-fill from bit 39 under
SXM and otherwise zero-fill, without adding overflow or applying OVM.
Nonnegative shifts, including zero, check the full signed source/shift
against the signed-32 range before 40-bit truncation, set sticky destination
overflow and saturate under OVM. Thus `LD A,0,B` may saturate an out-of-range
guard value; it is not an unconditional raw copy. Carry is preserved.

The 196608 standalone replay cases cover all signed shifts -16--15, four
A/B selections, both shift sources, eight source/destination patterns,
sixteen SXM/OVM/carry/sticky-overflow states, two pages and Decode/Access/Read
saves. A bit-level shift oracle and signed full-width overflow calculation
are independent of the CPU helper. Another 144 signed-load producer/replay
cases cover zero/one/two NOP gaps and drainage to IDLE. The shared register-ALU
fixture still runs the separate ADD/SUB matrix with its different flag
contract. LD publishes at clock 6 standalone and `8 + gap` after the load
producer in this partial model. No data-memory access, extension fetch or
duplicate opcode is permitted. Mixed status producers, silicon source-read
phases, wider operand conflicts and repeat/control/interrupt composition
remain unvalidated.

The isolated core runner permits five emulated seconds as a finite stop
bound for the full matrix. Reaching that bound is not acceptance: all
required markers and the intentional test-completion exit are mandatory,
and a normal exit is rejected even if it contains PASS markers.

Accumulator AND/OR/XOR register forms now retire in Execute using the
existing one-word arithmetic semantics. They read no memory, consume no
extension and change no flags. SPRU172C 4-11/4-123/4-201 specifies a logical
zero-filled shift over the 40-bit accumulator, independent of SXM, before
combining with the selected destination. Source/destination aliasing uses
the original value of that instruction's source.

The 6144 save/replay cases cover all three operations, four A/B selections,
all 32 encoded shifts (-16 through 15), four independently chosen operand
pairs, both SXM states and Decode/Read checkpoints. A bit-by-bit oracle
checks the result and unchanged status each clock, with one opcode fetch
and no data access. Original AND A,0,B (`f180`) at `00:2fed` is accepted;
CMPL A,A (`f493`) at `00:2fef` is also accepted. This is opcode
coverage, not native startup or speech acceptance. Control-delay and
interrupt composition for these logical forms are not separately validated.

CMPL uses the same one-word Execute path with no memory access or flags.
SPRU172C 4-32 specifies the full 40-bit ones complement of the selected
source into the selected destination. Its 64 replay cases cover four A/B
selections, four operand pairs, both SXM states and Decode/Read checkpoints;
the bitwise oracle checks the result and preserved status every clock.
The original upload passes CMPL without establishing native startup completion.

MVKD now retains its immediate data-source address and separately owned
Smem destination. Short DP/CPL, plain/postincrement/postdecrement/
preincrement indirect and absolute destinations are supported. Absolute
Decode consumes destination then source, not PORTW's source then port.
Source and destination are RAM or AR0-AR7; other control/peripheral operands,
long offsets, repeated MVKD and control-delay placement remain unsupported.

Read samples the source once, after Access's encoded pointer modification.
Thus a literal AR source which also addresses the destination must expose
the modified AR, not a stale sample. RAM publishes in the complete slot's
Execute. AR publication belongs to the opcode's Execute (the complete
two-word slot's Read), satisfying SPRU131G table 7-7's one-gap Category-I
latency. A first implementation using the RAM publication clock failed the
CMPR one-gap consumer test and was corrected; this is a CPU conformance
constraint, not a firmware-timing workaround. Precise bank/bus placement
remains partial, with no claim of physical silicon measurement.

The 672 replay cases cover seven destination forms, AR1/RAM/same-address/
AR2 sources, four values, two pages and three checkpoints. They poison
literal/CPL after Decode, SP/DP after Access and source/pointer after Read,
while retaining one read and one write. AR consumers with zero and one
intervening instruction verify the old/new value boundary. Sources which
alias their pointer and destinations which alias their source are explicit
fixtures. Original compiler MVKD `7001` at `00:30b5` now runs; the upload
passes SFTA A,-1,A (`f47f`) at `00:323f`. Native startup, recorder acceptance
and speech remain unproven; no atomic profile is promoted.

SFTA/SFTL now use the admitted one-word Execute path without data access.
The staged fixture has 32768 save/replay cases: both operations, all four
A/B selections, all encoded shifts (-16 through 15), eight source patterns,
both SXM/OVM and initial carry states, and Decode/Read checkpoints. A
bit-by-bit shift oracle plus signed-range checks validates result, carry,
sticky overflow and saturation only at Execute, with one opcode fetch.
SFTA right shifts sign-fill from bit 39 when SXM is set; SFTL operates on
bits 31-0, clears guard bits and explicitly clears carry for shift zero.

SFTA left carry is not a closed hardware contract: SPRU172C 4-155 specifies
source bit `39-SHIFT`, but 4-156 example 2 (`80aa001234 << 5`) prints C=1
where that formula gives zero. Its zero-shift carry is also not explicit in
the descriptive cases. The existing formula/preserve-zero policy is kept
and named in the fixture marker; passing it is conformance to that policy,
not proof against silicon. A bounded TI-document search found the same
manuals, not a resolving correction. Independent simulator/hardware
evidence is still required before changing this shared arithmetic helper.
This ambiguity does not affect the upload's negative-shift instruction.

The original upload passes long-offset DSUB `54e6` at `00:3244`.
General shift interrupt/control-delay composition remains unvalidated;
opcode coverage does not establish native startup or recorder acceptance.

DSUB now retains its long-word data operand before Execute. Addressing
reuses the extended DST path for ordinary offset, linear/circular
preincrement and absolute forms; short indirect modifications use a
two-word step. Complete-Decode CPL/DP and Access SP own direct operands.
Read samples MSW at the retained address and LSW at `address ^ 1` in the
same clock, including odd-address ordering (SPRU131G 5.6). Execute calls a
shared, operand-only long-arithmetic helper; atomic DADD/DSUB/DRSUB and
T variants use that same calculation without changing their bus paths.

The 6,144 replay cases cover eight read-capable address forms, A/B destinations, eight
operand pairs (including TI's C16 example), both C16/SXM/OVM states,
both parities and Decode/Read/Execute checkpoints. They verify one paired
read, no memory write, retained values after RAM/literal/AR/SP changes,
32-bit borrow/overflow/saturation and separate C16 lanes. The existing
dual-mode sign-extension and overflow-retention policies remain modeled,
not independently silicon-validated. Exact bank arbitration, peripheral
pairs, repeats and control-delay composition are not claimed; the last
three remain unsupported. Original `54e6` now executes and the upload
passes long-offset immediate ST `76e6` at `00:322e`. No native startup,
recorder or speech acceptance follows from this opcode boundary.

DADD (`5000`--`5300` families) shares that retained-pair/address path,
but bit 9 independently selects its source accumulator and bit 8 its
destination. Execute reads the selected original source and adds the
captured memory pair; it does not use an unrelated destination value.
The 12,288 replay cases cover all four A/B selections, eight read-capable address
forms, eight source/memory pairs, C16/SXM/OVM and both parities. Direct
offsets 4/5 include exact `5304` under compiler-relative addressing.
Fixtures change AR/SP after Access, literals after Decode and RAM after
Read, then restore in Decode, Read or Execute; each leg requires one
ordered pair, no memory writes and unchanged unselected accumulator.

Short MOD=3 is omitted from these read fixtures with raw IDs preserved;
SPRU131G table 5-4 restricts it to writes. Atomic paired arithmetic and the
staged DADD/DSUB/DRSUB classifier reject it before operand effects. The
`long-pair` and `drsub` gates additionally require 3,584 rejection/replay
cases across all 14 paired-read opcode classes, all ARs, CMPT/C16 settings,
pages, frontends and two snapshots, with no data or pointer effects.
This adds no positive staged T-variant support; paired DST writes and long
MOD=13 reads retain their separate domains.

Ordinary mode extends the memory high word under SXM, computes full
40-bit addition, low-32 carry, sticky destination overflow and OVM
saturation. The modeled C16 path adds independent low/high 16-bit lanes,
does not propagate low carry into high, extends the high result into
guard bits and derives carry from the high lane. It retains overflow
and does not clamp the independent lanes under OVM, as in the existing
long-arithmetic model. Those dual-mode flag policies and paired Read
phase remain not independently silicon-validated. The matrix uses encoded
AR1/AR6 and BK=16; other modifiers/compatibility ARP, producer/status
conflicts and physical bank arbitration remain outside it. Register/
peripheral pairs and control-delay placement fail closed.

Long-offset immediate ST now consumes offset then value as a retained
three-word operation. Its Read-owned destination calculation shares the
same offset/preincrement/circular helper as atomic addressing and staged
DST/DSUB, with no operand fetch at address generation or Execute. Execute
writes the cached value once. Register/peripheral destinations for these
new forms, repeats and control-delay placement remain unsupported; existing
short and absolute immediate-ST paths retain their behavior.

The 6144 replay cases cover three offset modes, eight encoded registers,
four offsets, four values, both compatibility states, two pages and saves
after partial Decode, complete Decode, address generation or publication.
They poison each extension after capture and change AR/BK/DP after address
generation, verifying no destination read, no refetch and no repeated write.
Opcode-shaped offsets/values are data, not independently decoded opcodes.

Control-delay rejection is likewise restricted to unowned instruction words,
not extensions of an already decoded instruction. The call matrix now has
132 replay cases with nine STM-T parameters, including `76e6`, `4ee6`,
`7001` and `54e6`, inside valid CALLD delay slots. The unchanged call/stack
and retained-parameter results protect that ownership rule while actual
unsupported instructions still fail closed. Original startup passes
BIO-conditioned BC `f803` at `00:2fca`; a new opcode boundary alone proves
neither native startup nor physical timing.

Staged BC/BCD admit BIO and NBIO via the CPU's existing board-supplied
line callback. BIO is low-active and NBIO high-active (SPRU172C condition
codes); SPRU131G 8.3.1 places branch sampling in Read. Both forms sample
once in the retained operation's Read, before Execute, and store the outcome
across branch/delay retirement. Delayed accumulator predicates retain their
separate existing Evaluate path; BIO is not sampled through that path again.
The retained-slot clock alignment is still a partial model, not measured
silicon phase placement.

The 256 replay cases cover plain/delayed forms, both conditions, initial
low/high levels, static/change-after-Decode/change-before-Read/change-after-
Read inputs, two pages and four checkpoints. They verify one line callback,
one literal fetch, the selected target and real execution/suppression of
delay markers, with unchanged status. Fixtures explicitly wake between
IDLE endpoints and restore their external input observation state; no
handset firmware state is changed.

MU4 already connects NAND R/B to the bench's `m_ready` and `bio_in_cb`,
backed by the recovered schematic. The original staged upload now consumes
that live model output without forcing it and passes `f803` and
MVMM SP,AR5 (`e785`) at `00:2fbe`. Interrupt/held-peripheral composition of
these branches remains unvalidated. This is bounded startup coverage, not
native recorder acceptance or a physical-board proof.

MVMM admits exactly AR0-AR7/SP source/destination encodings. SPRU172C
4-116 explicitly places the source read in Decode and destination write
in Access. The staged implementation retains the source once and orders
Access publication before a younger Decode sample, after older publications.
This preserves immediate destination visibility and the older producer's
required source latency. Execute does not reread or rewrite the registers.
SP writes use the existing compiler-addressing visibility window rather
than delaying the architectural SP update.

The 1944 all-pairs replay cases cover all 81 register pairs, four values,
two pages and Decode/Access/consumer checkpoints. They change the source
after Decode and verify one read/write plus zero-gap AR consumption.
Another 24 cases verify normal DP and compiler SP consumers with zero/one
intervening NOP. The 48 producer-chain cases verify STM's zero latency and
STLM's two-gap Category-I source latency, including simultaneous older
Execute publication and MVMM Decode sampling. All preserve the retained
sample through replay. Wider stack/interrupt and register-bank contention
compositions remain unvalidated; repeat MVMM remains unsupported.

The original upload passes short RPT #32 (`ec20`) at `00:2fc1`. An opcode
boundary alone remains insufficient to prove older I/O retirement, native
startup acceptance or speech validation.

Short immediate RPT now loads its encoded eight-bit count at Execute and
activates interrupt protection in Decode. NOP is a supported retained body
for short/long RPT: the first instruction takes its normal frontend path,
then each stream clock retires one further NOP and decrements RC without
program/data refetch or pointer modification. Count zero still executes
the body once. The existing saved counter is exposed as debugger state
`RC`; no new repeat-runtime state or firmware intervention is introduced.

The 240 replay cases cover both immediate forms, counts 0/1/3/32/255,
no interrupt/IRQ/NMI, two program pages and four checkpoints. They check
RC every clock, poison the already fetched body and speculative
continuation, and verify one body fetch, fresh continuation, deferred
acknowledgement and balanced interrupt return. With OVLY clear, even the
NMI vector's low slot inherits XPC, so each fixture's ISR is placed in its
active page. Input edges are queued before the Decode checkpoint; the
maskable fixture asserts that IFR already owns the event before saving.
Saving an undelivered MAME input synchronization request is not a test of
saved CPU interrupt state. Short-repeat transfer-body composition and wider
peripheral/HOLD interactions remain unvalidated; other bodies fail closed.

Staged BIT admits the `96xx` Xmem/BITC family (SPRU172C 4-21).
Access captures the selected AR2-AR5 address and applies its packed pointer
modifier once; Read retains one RAM word; Execute copies bit `15-BITC`
into ST0.TC without changing other status or either accumulator. Sources
below `0060` fail closed before pointer modification; peripheral wait and
control-register composition are not enabled by the RAM tests.

The 9,216 replay cases cover all 16 packed Xmem fields, all 16 bit codes,
both selected-bit values and initial TC states, zero/one/two-gap XC consumers,
and Decode/Access/Read checkpoints. Poisoning RAM after Read must not
change the result. XC samples before Execute, so zero/one gaps observe old
TC while two gaps observe the BIT result. The original `9634` at `00:2fc3`
uses unmodified AR5 and tests bit 11; its two NOPs before `fd20` match the
tested window. Percent addressing is tested with BK=0 and AR0=3; nonzero
BK, page variation, repeat bodies and wider producer conflicts remain
unvalidated. These are staged-model contracts, not measured silicon phases.

The staged-only, first-16-strobes observer records five original-upload
NAND writes: command `00` with HD012=`1`, then address bytes `00,01,00,00`
with HD012=`2`, direction=`7`, ready=`1`. CLE/ALE distinguish the cycles;
CE is low. No device state or response is synthesized by that observer.
Staged PORTR retains its port literal and, for `74f8`, the preceding
absolute destination literal. Basic direct destinations retain Decode
CPL/DP and Access SP through the existing consumer-latency machinery;
short indirect destinations retain the address and postmodify in Access.
Read samples the external I/O space once; Execute writes that retained
word to data space. Only RAM and AL/AH/AG/BL/BH/BG/T/TRN destinations
are accepted. Control, DAGEN and peripheral destinations fail closed before
the I/O read; long-offset forms, control-delay composition, repeat bodies
and external READY/HOLD wait behavior remain unvalidated. This is a partial
Class-27 stage model, not measured silicon bus timing.

The 416 replay cases cover those eight register destinations and absolute
RAM, DP, CPL, AR postincrement and postdecrement forms; four values, two
program pages and four checkpoints cover partial Decode through publication.
Literal poisoning after complete Decode and port changes after Read must
not retarget or resample the operation. Tests assert no data-destination
read, one I/O read, one data write, unchanged status/unselected accumulator
lanes and no duplicate pointer update after restoration.

The original upload now retires PORTR `74f8` at `00:302f`. The bounded
NAND read observer captures the first 512 bytes with HD012=`0`, direction=`7`
and ready=`1`. They compare byte-for-byte with retained NAND row 1
at raw offset `528`; the prefix is `fa33c08ed0bc007c8bf45007501ffbfc` and
the full captured page SHA-256 is
`7876777bc75f8e33789d0b917ac2ca71debc977170e6bc6b0f2dcf9b2e2c9310`.
The bounded observer also sees two further commands `00` with address
bytes `00,20,00,00`, selecting row 32.
Only the first 512 bytes are captured and verified. The first-page
comparison validates device-read data,
not the resulting firmware buffer, spare-area handling or native startup.
The observer returns the single existing device read, never a second
read or a synthesized reply. STL A `80e6` at `00:3269` and long-offset
MVDK `71e6` at `00:326b` are now accepted by the staged decoder.
RPTB `f072` at `00:32ee` now executes.
Same-address RMW forwarding now lets the unchanged directory builder
construct `MCUSI16 .BIN` with descriptor length `0001:e1e8`, matching the
atomic reference. NEG `f484` at `00:0e35` now has 13,824 staged replay
cases, covering all source/destination selectors, carry, sticky overflow,
OVM saturation, 32/40-bit boundary patterns and immediate-load producers
without data-memory accesses or frontend fallback (SPRU172C 4-119/4-120).
The pre-correction one-second observation enters loader `3538` with descriptor
zero after 4,612 NAND reads and reads block count `0010`. Its mapped AL
subtraction (`09f8 0008` at `00:2f46`) and mapped AL BITF
(`61f8 0008 0001` at `00:2fa5`) now execute. The preceding load reads
DMPREC; BITF itself reads AL, not a DMA peripheral. Mapped BITF has
13,824 replay cases for the eight accumulator/T/TRN words, four masks,
two initial TC states, direct/absolute forms and AL producers, with one
retained source read and no data writes. Only TC changes in Execute;
zero mask clears it (SPRU172C 4-28/4-29).
That pre-correction observation reached resident `02:6d62`, stopping at
`STM #lk,IMR` (`7700` at `02:6d64`). Its read-only program comparison
found 40,657 mismatches among 48,261 words: first `02:8e8b`,
actual `826c`, expected `0001`. It excludes 3,660 data words from the
51,921-word source and uses the configured common-window mapping without
claiming physical bank identity. Correct resident transfer is unproven;
a fresh atomic `bootrecsave` control with the same NAND/shared-memory
profile matches every compared program word. The first staged divergence
is record 434, word 11, file offset `4000` (16 KiB): its first five wrong
words match byte-swapped source bytes at `3fda`, one 38-byte record earlier.
The crossing record gets only 11 fresh buffer words before the original
16-word swap/DMA, leaving five stale words; DMA still reads the original
buffer. Atomic advances both descriptor cluster fields from 3 to 4;
staged resets the cursor without advancing either field. Their initial
cluster comparison reads equal values, but next-cluster helper `343a`
returns `0fff` staged versus 4 atomic. Offset helper `345c` has an earlier
fetch-snapshot discrepancy at `3482` (A=0 staged versus 2 atomic).
Snapshots do not establish retired operand values. A subsequent actual
mapped read observation establishes AL=3 staged versus AL=9 atomic at
`LDM AL,A` (`347b`), immediately following two-word `MPY #3,A`
(`3479..347a`). `SFTA A,-2,A` then gives offset 0 instead of 2.
SPRU131G table 7-28 requires no gap for this two-word producer/consumer
composition, unlike a one-word producer with a short mapped consumer.
Matching mapped accumulator reads now finish after the older two-word
immediate MPY Execute without changing other reads or one-word latency.
The isolated `mpy-mapped` fixture passes 3,072 saved-state replays over
six accumulator words, both source/destination accumulators, signed
patterns, FRCT/OVM and zero/one-word gaps, including mutations after T
and mapped-word capture. The unmodified CPU fails the same fixture.
Long-offset `MVKD` (`70e1 0016 0012`, `00:33aa`) now retains offset,
source, destination and pointer modification through the existing move
path, publishing the recovered cluster value into `36c6` organically.
Its 19,200-case isolated replay fixture covers 25 destination forms,
four offsets, selected-AR/AR2/RAM/alias sources, four values, both
compatibility states and two program pages, with one read, one write and
two ordered extension fetches. Snapshots cover partial Decode, retained
Read and completed write (SPRU172C 4-113).
The full C54x conformance runner and native 5110 save-state gate pass.
The current original bootstrap reaches resident `02:6d62`; all 48,261
compared program words match, including the former 16-KiB crossing.
A separate side-effect-free data audit now checks all
3,656 ordinary data words with zero mismatches in both staged and atomic
controls. It preserves later common-RAM program/data alias writes in
record order for the configured profile. The four low register-window
writes at `007c..007f` remain excluded; their live values cannot establish
write effects. `noki5510_bootstrap_check.py --resident-memory LOG` requires
both complete audit reports with exact coverage and no mismatches or
duplicate evidence. Original program/data RAM contents are verified;
physical bank identity, register effects and full boot are not.
The immediate IMR clear (`7700 0000`, `02:6d64`) now publishes through
the retained non-DAGEN Execute path. Its 4,608-case replay fixture covers
16 IRQ bits, four masks, both initial masks/INTM states, no source/IRQ/NMI
and two pages, with one literal fetch/write and no destination read.
Acceptance before a clear is preserved; a newly enabled pending source
is accepted only after publication. Bus-phase timing is a partial model
(SPRU172C 4-176/class 12A; SPRU131G 6.10.2).
The full C54x conformance run, native 5110 save-state gate and original
resident-memory checker pass; fixture IRQ/NMI inputs return low on exit.
The MU4 sequence includes `STM #7fff,*(0028)` (`7728 7fff`, `02:6d05`).
SWWSR at `0028` and the following BSCR write (`7729 8006`, `02:6d07`)
configure external wait states/bank switching (SPRU131G 10.3.1/10.3.2).
Storage alone must not be described as faithful wait-state modeling. No
filesystem or cluster state is synthesized; full boot remains unaccepted.
The [MU4 external-bus contract](c54x_mu4_observations.md#external-bus-compatibility-contract)
pins these original requests using SPRS095P's C5416-compatible dialect:
BSCR's upper fields differ from the older generic family table. Its field
decoder and retained I/O paths are tested as described below, but physical
DA150 identity, program/data bank ownership and physical bus phases remain
unvalidated. This compatibility profile is distinct from the optional
standard SWWSR model used by the NSE-1 staged execution probe.
The isolated C5416-compatible register profile now validates reset,
masking, partial writes and saved-state/output restoration across all
65,536 encodings. Its CLKOUT divider and IACK-enable callbacks are
configuration outputs, not pin waveforms; logical IRQ acknowledgement
remains independent. No handset selects this compatibility profile.
The isolated staged I/O profile now validates software waits `0..14`,
READY eligibility after at least two software waits, single read/write
side effects and saved transaction ownership across 576 replays. Timers
continue and IRQ/NMI acknowledgement follows the transfer. Whole-pipeline
stall placement is partial; simultaneous I/O arbitration is fail-closed.
The atomic frontend now retains waited I/O and retirement context.
Its separate gate passes 2,880 saved/replayed cases across short,
long-offset, absolute, peripheral-MMR and AR-alias addressing. A 24-case composition gate checks
RPT counts 0..3, delayed branch slots and INTM-protected followers,
including pointer updates and stacked interrupt continuations.
The MMR fixture checks the extra access clock at `0030`; the AR1 alias
checks write-before-postincrement ordering. Other control-register side
effects and physical bus phases remain outside this bounded acceptance.
Bus-control STM admission has a separate 36-case saved/replayed gate:
one retained literal and one masked Execute write at clock 8, including
the peripheral clock. The profile is also selected by the isolated
`bootrecstage` diagnostic, not a handset composition. Original resident
memory still matches all checked words. Mapped-accumulator READA now has
288 saved/replayed cases for AL/AH/AG and BL/BH/BG, direct/absolute forms
and two source pages, preserving status and untouched accumulator words.
The original `7ef8 000b` transfer at `02:6d12` executes. Absolute LD
Smem,DP has a 1,536-case saved/replayed gate for all 512 DP values,
retained extension/source and zero-gap consumer forwarding. The original
`46f8 007f` at `02:6d2b` also executes. That checkpoint passes full CPU
conformance, native 5110 restoration and the isolated profile gates.
Direct MVDM-to-AR now has 1,152 saved/replayed cases for all eight
registers, RAM/self/other-AR sources and table-7-7 consumer gaps. The
original `7212 b6be` at `02:df9b` executes. MVDM `7211 b6a8` at
`02:dfa6` also executes after a separate 576-case branch-delay gate,
including both outcomes, pending IRQ/NMI and RETE/save/replay. Repeat
remains unqualified in this AR gate; accumulator destinations have the
separate [slice qualification](c54x_execution_contracts.md#mvdm-accumulator-slices). Other non-AR
destinations remain excluded. FCALLD `fb82 dfc2` at
`02:dfaf` has a 243-case saved/replayed gate for caller-fetched delay
words, stacked PC/XPC and IRQ/NMI return. Delay-slot mapped-XPC read
visibility is explicitly unvalidated, not inferred from the frame contract.
The original enters `02:dfc2` and executes absolute MAC Smem,B
`29f8 0011` at `02:dfc9`. The MAC/MACR/MAS/MASR family has a 73,728-case
saved/replayed gate for retained memory/T, both accumulators, rounding,
FRCT/OVM/SMUL/SXM and sticky overflow. MVKD
`7084 0013` at `02:dfd8` now executes as both delay words of BCD at
`02:dfd6`, after a 1,728-case saved/replayed MVKD delay gate. It includes
AR sources, postincrement and same-address RAM read-before-write. Three-
word extended delay forms remain excluded. MVMD `7316 b6a9` at
`02:e078` now executes after a 576-case saved/replayed gate for direct
AR sources, RAM/register destination ownership and unchanged status.
The original returns to the init spine and executes STLM A,IFR `8801`
at `02:6d3f`, immediately following LDM IFR,A. The isolated pair gate
passes 96 cases/restores: sampled flags, late physical IRQ, selected IRQ
whose flag is cleared, independent NMI, and RETE stack restoration. The
absolute CMPM `60f8 3762 000c` at `02:909c` now executes after a 96-case
retained-extension/source and adjacent TC/NTC branch replay gate.
Mapped-register CMPM is separately qualified for `0008..0017` above;
the shared mapped-BITF gate also passes.
MVKD `7082 0010` at `02:8fbd` now copies AR0 through AR2 to McBSP's
mapped control-data register `0049`, after a 648-case peripheral
wait/publication/replay gate. The extra peripheral clock precedes a
single Execute-owned write; DAGEN destinations retain their earlier
publication path. BK sources now share retained Read ownership and
post-capture-change replay; adjacent BK-producer latency remains
unvalidated. T/TRN peripheral stores now pass an 864-case STLM-producer
and peripheral-publication replay gate. Peripheral BITF now passes
384 status-read/TC-branch replays with its extra access clock. Original
`61e1 0001 0002` at `02:9521` polls the McBSP register-1 transmit-ready
bit through `0049`, following index selection through `0048`. The
original emits six native serial words and executes FCALA B `f5e7`
at `02:40ef`. The nondelayed staged prototype
now passes 216 saved/replayed page/frame round trips with a full-width
retained accumulator target and two separately owned stack writes.
IRQ/NMI cannot split the call frame; RETE resumes one callee execution
and FRET restores the caller. A separate 216-case DLD-producer gate
validates target publication with gaps 0/1/2 and retained RAM inputs.
Other producer families and the producer-plus-interrupt cross-product
remain unvalidated. Nondelayed CC AEQ/BEQ now passes 64 replay cases
with retained predicates/literals, conditional PC+2 frame publication
and zero untaken stack accesses. Original `f945 3ff1` executes; the
conditional returns now cover all twelve signed A/B predicates in 768
predicate/pop/target replay cases (eight 40-bit patterns, two pages and four
snapshots). Untaken returns have zero stack
accesses. SUBS passes 52,224 unsigned-source/address/arithmetic
replays across 18 forms, A/B and SXM/OVM states; mapped sources and C16
remain excluded. The pre-status checkpoint stopped at MVMD AL `7308 0007` at
`02:3f62`, live PC `02:3f65`, immediately after POPM AL. AL/BL-to-RAM
now passes 288 POPM/mapped-STL producer replays, with mapped-write
no-conflict ordering and retained source publication. The staged ST1 subset
passes 288 transfer/replay cases and 16 basic CPL consumer/replay cases;
the original passed `3f83` at `02:3df3` after a separate 208,896-case
memory ADD/SUB shift-16 gate. That new RAM-only subset covers all eight
operation/source/destination selectors and the syntax-3 partial carry rule;
its full checkpoint regression passes. The original's LDU
`1282` at `02:3dfa` reads actual IMR at `0000` after a separate 384-case
unsigned mapped-load gate, including the adjacent pointer-producing MVDK.
The preincrement STL/STH subset passes 6,144 save/replays; immediate
preincrement ST passes 1,536; STM BRC followed immediately by RPTB passes
30, including count 127. These additions pass the original clearing loop.
Long-offset peripheral ST passes 6,144 save/replays; ADDM/ANDM/ORM passes
368,640, including RAM controls with identical offsets, one peripheral
extra clock total and captured source/destination ownership. These are RAM-
backed CPU MMIO conformance tests, not device-side-effect acceptance.
The original McBSP0 SPSA/SPSD store/mask sequence at `02:97d9..02:97de`
executes through the real device model. A separate 16-case coupled McBSP
gate validates selector/read/reset order, volatile XRDY capture and joint
CPU/device restoration. STLM A/B to IMR passes 9,216 mask/IRQ/NMI replays.
The current original stop is `8a1d` (POPM PMST) at `02:8b3b`, live `02:8b3d`.
McBSP0 stream words are now observed, but their source/audio meaning is not
validated. Physical DA150 mapping and PMST restoration/fetch ownership
remain open; checkpoint regression for the admitted peripheral/control
subset passes.
The earlier zero address is observed, not proof that the pointer lifecycle is correct.
Adjacent SXM/ASM LD consumers pass 768 save/replay cases; immediately
following RPTB passes 24, including preservation and final clearing of BRAF.
Basic CMPT indirect LD/ST passes 16 cases/restores after replacing live-mode
address selection with saved Decode sampling shared by pointer updates.
The status IRQ/NMI fixture passes 576 save/replay cases with requested mask
pairs, three arrivals and one-/two-word followers. Memory writes cannot set
INTM; zero-write clearing retains the explicitly unverified policy in the
[ST1 access contract](#st1-memory-access-constraints). Generic word writes
do not inherit special INTM-bit-instruction protection. Other CMPT categories, ARP-writer latency and
arbitrary cross-products of consumers/block repeats with IRQ remain unvalidated;
the accepted subset does not establish every status-register interaction.
Delayed FCALAD has separate staged frame, delay and IRQ/NMI coverage;
see the [delayed accumulator-call contract](c54x_execution_contracts.md#delayed-far-accumulator-call).
Serial output does not prove audio or full board boot.
Resident program/data memory still matches exactly. Shared regression and
native 5110 restoration coverage are indexed in the
[core regression checkpoint](c54x_execution_contracts.md#core-regression-checkpoint).
Excluded instruction and producer/interrupt compositions remain unaccepted.
Mapped ADD/SUB has 6,912 replay cases for AL/AH/AG,
BL/BH/BG and T/TRN, direct/absolute forms, destination aliases and retained
Read values. Producer cases distinguish the short-consumer one-word
latency from the long-offset zero-gap case (SPRU131G table 7-28).
Controls, peripherals and mapped SUBC remain excluded. Physical bus
phases and bank contention are not established by these fixtures.
Repeated NEG and measured silicon phases remain unvalidated. The former malformed
names/zero-length loader retry tail is superseded. Board boot, resident
transfer, music decoding and recording remain unaccepted.
The scoped bypass uses retained RMW operands, preserves EB/flag timing and
excludes register/peripheral reads; physical memory-bank contention and
other pending-write producer families remain unvalidated. The mandatory
768-case `rmw-forward` fixture covers adjacent/spaced loads, same/different
addresses, boundary arithmetic, SXM/OVM, two pages, DP/compiler addressing
and save/replay. It does not claim measured DA150 bus phases. See
[the original-bootstrap contract](5510_bringup.md) for the bounded
descriptor and stale-offset data-bus witnesses. No firmware state is forced.
STLM B,BRC at `00:32ec` now has 3,072 staged replay cases covering
Execute publication, saturation, both accumulator sources and retained
LDM reads. The context tool pins the original nine-word sequence:
`891a f495 f072 32f4 1386 f678 f3e8 f1a0 8196`.
Its intervening NOP satisfies SPRU131G table 7-3 category 9's BRC-store
spacing rule; the requested block spans `00:32f0`--`00:32f4`.
Straight-line block execution and the forward near-branch subset have
bounded conformance below; delayed/nested and other control-changing
blocks remain outside it.
Zero-gap LDM cases test the declared pipeline
projection, not silicon forwarding or RPTB latency.

The static setup contract follows SPRU172C's RPTB[D] entry: two instruction
words, RSA=`PC+2` (RPTB) or `PC+4` (RPTBD), REA=the literal last word,
nominal four/two cycles respectively, BRC+1 executions, and interruptible
blocks. RPTBD executes two delay words and is not single-repeatable.
SPRU131G sections 6.8/7.5.8 place BRC decrement at the last block
instruction's Decode stage. The atomic frontend's post-instruction
`REA+1` check is therefore not sufficient evidence for staged timing.
The staged implementation retains endpoint predictions before younger
fall-through work executes, independently of Decode-owned BRC updates.
Interrupt acceptance discards undecoded fetch predictions and reconstitutes
the remaining count from the architectural state at its saved return PC.
Software BRC/BRAF modifications have separate five/six-word latencies
(tables 7-3/7-25), not ordinary Execute forwarding. The context tool's
setup arithmetic tests do not prove these runtime requirements.

The `tms54test -bios rptb` runtime fixture covers 6,144 cases with counts
0/1/2/7, sixteen source patterns, sixteen flag configurations, two program
pages and three save points. The original five-word body has uninterrupted
reads at clocks 9+5i and writes at 14+5i, with no loop padding. Twenty-four
IRQ/NMI cases save before/through interrupt acceptance, use the existing
same-page vector contract and return through RETE. Sixteen further replay
cases cover one-word MAR and two-word LD endpoints and twelve fail-closed
cases: nested RPTBD, a backward branch occupying the endpoint, STLM BRC, endpoints inside opcode
or extension words, and an aliased STL to BRC. All block-prefetch tags,
retained extension tags, active context and speculative count are saved.
Terminal BRC remains zero in this declared model, matching the atomic
frontend; exact silicon terminal-counter and phase behavior are not claimed.
This fixture is not the frontend's complete composition surface. Separate
RPTBD, branch/call, XC and context-restoration scopes are maintained in
[the retained execution contracts](c54x_execution_contracts.md#retained-recorder-execution-contracts).
Nested block repeats, IDLE inside a block, general in-loop register-modification
latencies, wrapped endpoints and measured silicon timing remain outside those
bounded contracts. The forward near B/nondelayed BC cases below establish
their own subset, not an exclusion of separately validated compositions.
These tests do not establish complete DSP startup or speech.

Forward near B/BC may target the same block at or after the sequential
continuation, but neither word of the branch may occupy REA. Taken
redirects rebuild the discarded endpoint prediction from architectural
BRC. Untaken BC restores held block tags, including a held endpoint;
three pending-tag booleans are explicitly saved. The `rptb-branch` profile
passes 12,288 replays over two layouts, counts 0/1/2/7, four data patterns,
both accumulators, two pages, eight snapshots and IRQ/NMI arrivals at
model clocks 9/10/12/14, plus uninterrupted controls. Each iteration has
one source read and selected write, with Decode-owned BRC/BRAF and
balanced ISR return. A later outside branch changes the held tags before
restoration. Thirty-two `rptb-br-edges` cases preserve rejection of
below-block targets, endpoint-overlapping branch words, delayed endpoint
control, call, nested and far-branch compositions without a frame or body
write. Forward internal-branch interrupt composition is covered by these
fixtures; arbitrary control flow, software BRC/BRAF changes and measured
silicon timing are not.

The separate `rptb-exit` profile admits forward nondelayed near B/BC targets
beyond REA, with both branch words before the endpoint. Its 1,152 cases
plus replay cover B/A-equal/B-equal, counts 0/1/2/7, first/middle/last/never
exit conditions, two endpoint layouts (including a store's literal at REA),
two pages and six snapshots. Taken exits annul the remaining body while
preserving BRC and BRAF; final-iteration BRAF clearing still belongs to
Decode, not the branch. Untaken paths retain their endpoint tags and
complete all iterations. An outside RSBX BRAF then retires the private
fetch predictor only when no younger body words are pending. A deliberate
revisit to the old endpoint after six NOPs and a branch checks that the
cleared loop cannot restart or consume BRC. RSA/REA remain unchanged.
This follows SPRU131G 7.5.8.2's software-clear spacing and does not import
C55x automatic clearing on exit. In-loop software-clear latency, pending-body
clear arbitration and interrupt composition of these exits remain outside
this bounded gate; silicon phase timing is not claimed.

The original block has a concrete data oracle: `1386` is
`LDU *AR6,B`; `f678` is `SFTA B,-8,A`; `f3e8` is
`SFTL B,8,B`; `f1a0` is `OR A,0,B`; `8196` is `STL B,*AR6+`.
The unsigned load bounds B to 16 bits, so the stored low word is
`((source << 8) | (source >> 8)) & ffff`, an in-place byte swap.
AR6 advances once per executed block. The context-tool desk check covers
all 65,536 source words independently using byte serialization; it does
not execute this loop or prove its source range, BRC value, final buffer,
flags, pipeline timing or native startup. Those remain runtime obligations.

The isolated staged CPU body fixture (`tms54test -bios swap-body`) now
executes those five words without RPTB in 1,536 cases: sixteen input
patterns, sixteen SXM/OVM/FRCT/CPL combinations, two program pages and
three save points. It poisons the source after the single Read sample,
checks dependent A/B results and carry, and restores before/after Read
and before the store. One read, one write and one AR6 increment survive
each replay. Store address/AR6 publication is in the current frontend's
Read phase, and the write is in Execute; this remains a model phase,
not recovered silicon timing. The full core runner requires this marker.

The zero-gap join fixture (`tms54test -bios swap-join`) executes two
consecutive copies in another 1,536 replay cases. It reads successive
words at clocks 5/10, writes their byte-swapped results at clocks 10/15,
and advances AR6 twice. Both sources are poisoned after their respective
Read samples. Save points cover pre-Read, the first postincrement, and
the overlapping first write/second read. The older STL's implicit DAGEN
update must precede the younger LDU's same-clock Access: resolving them
in the opposite order rereads the first address. Store data publication
still belongs to Execute; explicit MMR-store forwarding is unchanged.
These checks prove the straight-line iteration join without inserting a
gap, not RPTB count, endpoint, interrupt or control-flow behavior.

Reproduce the isolated fixture in a disk-backed working directory:

```sh
/absolute/path/to/mame tms54test -bios swap-body -video none -sound none \
  -nothrottle -noreadconfig -log -seconds_to_run 5
```

Require the single `staged original swap body: PASS cases=1536` marker and
terminal `Fatal error: TMS320C54x isolated swap body complete` (exit 3).
Another exit-3 exception is failure, not completion.

For the join, use `-bios swap-join` and require its single
`staged original swap join: PASS cases=1536` marker and terminal
`Fatal error: TMS320C54x isolated swap join complete` (exit 3).
`MPY #lk,A`, `f066` at `00:2eda`, now has retained literal/T,
arithmetic and producer/replay coverage. STLM A,T,
`880e` at `00:2ed8`, now has publication and consumer/replay coverage.
Unsupported Decode can still stop older Execute; the final PC is not
retirement proof for instructions still in the pipeline. Repeated
`ST #lk,*AR1+`, `7691` at `00:08ea`, now has retained-literal,
two-clock stream and pending-IRQ/restoration coverage. BANZD,
`6e87` at `00:0a50`, now has short-indirect delayed-branch coverage.
The repeated `SUBC Smem,A`, `1e02`
behind `RPT #15` (`ec0f` at `00:352e`), now has staged repeat coverage.
The observed runtime reaches a different program range, but this does not
identify or validate the complete loaded firmware buffer or startup.
`ADD *(0016),0,A,B` at `00:319c`, encoded
`6ff8 0016 0d00`, now has staged coverage. Address `0016` is
mapped AR6, not RAM. The preceding `LD *AR1(0004),T` at `00:3194`, encoded
`30e1 0004`, and dependent MPY at `00:3196` now have staged coverage.
The preceding
`STL A,-1,*AR1(0003)` at `00:313e`, encoded `6fe1 0003 0c9f`, now has
staged store coverage. Its `0c9f` operation extension selects a low-word
store with shift `-1`, not a load.
`LD *AR6(0010),8,B` at `00:3291`, encoded `6fe6 0010 0d48`, now has
staged coverage. The checked original AA55 contract in
`mu4_context_contract.py` pins all three words and decodes the address
extension before the operation extension (SPRU172C LD syntax 4).
The signed five-bit shift is `+8`; destination is B. Unlike fixed
shift-by-16 syntax 3, syntax 4 affects OVdst and uses OVM saturation.
The static decoder tests all 32 shifts, both destinations and short/extended
forms; the separate executable matrix covers retained RAM/DAGEN behavior.
Short-indirect DST `4e81` at `00:30c6`, long-offset
LD `10e1` at `00:30c7`, short-indirect DLD `5681` at `00:30d3`
long-offset BANZ `6ce2` at `00:30df`, immediate SUB `f010` at `00:3216`
register SUB `f620` at `00:321c`, register LD `f540` at `00:30f7`
long-word DADD `5304` at `00:30fa`, LDU `12e1` at `00:3114`
MPYA `310b` at `00:3520`, LD Smem,16,A `440a` at `00:3521`
and MPYU `240b` at `00:3523`
now have staged coverage, but
unsupported Decode can stop older Execute work; later PC
observations alone are not a substitute for retirement evidence.

Long-offset/absolute STL/STH admit `80xx..83xx` with modes `e0..f8`.
Decode retains one offset/absolute literal; Read presents the destination
and applies the shared offset/preincrement/circular helper once; Execute
stores the selected low/high word using existing live-source SST/SXM
saturation. This preserves the short-store source policy rather than
adding an early accumulator sample. Sources/status are not changed by
saturation. RAM destinations only are accepted; peripheral/MMR, control-delay,
repeat-body and broader producer-conflict compositions remain unvalidated.
Read address/Execute source placement is a declared partial stage model,
not independently measured silicon timing (SPRU131G 7.1, SPRU172C STL/STH).

The 38,400 replay cases cover 24 long-offset forms plus absolute addressing,
four offsets including an opcode-shaped literal, four accumulator/lane
selectors, four SST/SXM combinations, four guard/sign patterns, both
compatibility modes and three checkpoints. Register 6's fixtures use the
high program page. BK=16 circular wrap and compatibility ARP selection
are checked. Poisoning the retained literal, changing the AR after Read,
and changing the source before Execute test the address/source boundaries.
Every leg must write once without reading the destination, preserve
unselected registers/status, and restore without duplicate pointer updates.
Extended MVDK admits `71xx` source modes `e0..f8` as three-word operations:
source offset/absolute address precedes the destination literal. Its complete
slot's Access computes and modifies the source address once, retains one RAM
sample, and follows the existing early DAGEN publication/defer machinery.
RAM destinations publish the retained word in Read after older Execute;
Execute advances past both extension words without another write. Only RAM
sources and RAM/AR0-AR7/SP/BK destinations are accepted. Peripheral/MMR
sources, control destinations, repeat bodies, control-delay slots, extended
bank-conflict cases and compiler-SP consumer composition remain unvalidated.
Class-19 bus placement is partial, not independently measured silicon timing.

The 38,400 replay cases use the independently calculated 25 source forms
and four offsets from the accumulator-store fixture, twelve destination
classes, four unsigned values, both compatibility states and four checkpoints.
They cover BK=16 circular wrap, high-page register-6 cases, source-pointer
and RAM aliases, and zero-gap CMPR consumption of the early AR publication.
Literal/source poisoning must not change the retained operands; every leg
must read and write once, preserve accumulators and avoid duplicate pointer
updates. Eight paired fixtures use the original `80e6,0010,71e6,0011,0012`
words: STL writes at clock 7, MVDK reads and publishes AR2 at 8, and an
immediate CMPR sees the new AR2. Four checkpoints replay both writes once.

TC/NTC BC and BCD (`f820/f830/fa20/fa30`) retain their predicate in the
complete slot's Read, corresponding to the opcode's Evaluate clock in
SPRU131G examples 7-16/17. Plain forms use the existing held-word/redirect
path; delayed forms use the existing late Evaluate path after older Execute.
Later delay-slot TC changes cannot reselect the destination. No new CPU
state or atomic fallback is introduced. Phase placement remains the partial
frontend model; overflow/carry and other conditional-branch families remain
outside this acceptance scope.

The 1,536 replay cases cover both forms/conditions/outcomes, BIT, CMPR-EQ,
status-bit and CMPR-LT producers (including the original `f5aa` family),
zero/one/two gaps, two pages, four checkpoints and a post-Evaluate TC-change
control. The latter changes debug CPU state in a firmware-free conformance
fixture, never the original upload. Tests assert one target-literal fetch,
one taken-target fetch, real delay-slot effects, the selected continuation,
unchanged unrelated flags and one BIT memory read where applicable.

Long-offset ADDM/ANDM/ORM now retain the offset and immediate as separate
Decode operands. Access uses the shared long-offset helper and records one
RAM address; Read retains the source word; Execute performs one write at
that same address. Ordinary/preincrement/circular modes `e0..f7` use the
existing three-word absolute grammar without treating the offset as a second
instruction. ADDM reuses the established SXM/OVM, bit-32 carry and sticky
OVA behavior; ANDM/ORM preserve arithmetic flags. No atomic arithmetic or
short/absolute publication rule is changed. New long-offset operands below
`0060` fail closed before the memory read. Repeat bodies, control-delay
slots and broader producer/peripheral arbitration remain unvalidated.
Class-18 Access/Read/Execute placement remains a partial stage model.

The 153,600 replay cases cover the three operations, 24 long-offset forms
plus absolute addressing, four offsets, eight value pairs and eight
SXM/OVM/initial-OVA combinations, both compatibility states and four
checkpoints. They reuse independent address and arithmetic fixture calculations;
register-6 cases use the high program page. Opcode-shaped offset retention,
BK=16 circular wrap, compatibility ARP, changed pointers after Access and
changed RAM after Read are checked. Each leg must read and write once with
the expected operation-specific flags and unchanged accumulators. The original
upload now consumes counter ADDM `6be6` at `00:3277`, long-offset SUB
`08e6` at `00:327c` and `BC AGT` at `00:327e` through repeated NAND reads.
The following BITF, short-indirect DST, long-offset LD and short-indirect
DLD, long-offset BANZ, immediate SUB, register SUB, register LD, DADD and
LDU, memory MPYA and memory LD shifted by 16 now have staged coverage;
unsigned memory multiply now has staged coverage. The subsequent extended
single-step and short-memory repeated SUBC now have coverage;
short-indirect BANZD and short-memory repeated immediate stores now have
coverage; the subsequent accumulator-to-BRC MMR store remains incomplete.
This is bounded transfer
coverage, not startup acceptance.

Zero-shift memory ADD/SUB A/B (`00xx/01xx/08xx/09xx`) now retain their
memory operand through Read and use the established 40-bit arithmetic
helper at Execute. Direct forms retain Decode CPL/DP and Access SP;
short indirect modes apply one pointer modification in Access, including
postincrement/postdecrement, AR0 indexing, reverse carry and BK circular addressing.
Short MOD=3 preincrement is write-only and is rejected for these sources.
Long-offset forms retain one Decode literal and use the shared address helper;
absolute forms retain the address directly. The existing absolute-ADD path
is preserved. Sources are bounded to RAM and mapped AR0-AR7 words; control
and peripheral sources fail closed. Carry is taken at bit 32, destination
overflow is sticky, and OVM clamps to signed 32 bits. Shifted/TS, unsigned,
carry-consuming, repeat and wider producer/control-delay compositions remain
outside this acceptance. Source/mode placement is a partial stage model,
not measured silicon timing.

The 104,448 RAM replay cases cover DP/CPL, the eleven read-capable short indirect
modifiers, three long-offset forms and absolute addressing; eight encoded
registers, four operation/destination selectors, eight guard/sign patterns,
four SXM/OVM states, both compatibility states and three checkpoints.
The direct-field matrix spans all 128 encoded offsets. Register-6 cases use
the high program page. Independent arithmetic expectations assert bit-32
carry, overflow and saturation without invoking the production helper.
Changed literals, pointers after Access and RAM after Read must not retarget
or resample the operand. An additional 1,024 cases read mapped AR0-AR7 via
direct/absolute addressing, change the source register after Read, and restore
before/after that sample. Each leg reads once, performs no data write, and
preserves the other accumulator and unrelated status.

Protected absolute ADD is explicitly classified as two words at retirement,
despite retaining its separate legacy decode/arithmetic path. Omitting it
missed XC's protected-window endpoint: the bounded pre-fix fixture read the
operand but observed zero vector fetches, acknowledgements or stack writes
by clock 36. The correction affects word accounting, not arithmetic or memory
sampling. The mirrored control-retirement classifier uses the same correction.

The 32 IRQ/replay cases cover XC1/XC2, absolute ADD and long-offset SUB,
both accumulators, two program pages and checkpoints before/after retirement.
An input-line edge is delivered before saving, with IFR owning the request.
After the full arithmetic instruction retires, each leg must acknowledge and
fetch one IRQ vector, save/restore XPC and the return address, execute the
continuation and reach IDLE with balanced SP. The data source is read once;
only the interrupt frame writes RAM. Other multiword protected-instruction
families still require their own retirement/composition coverage.

Signed accumulator BC/BCD now admit all six zero comparisons for A and B:
GE, LT, NE, EQ, GT and LE (SPRU172C condition table, page 4-18).
The predicate uses the full 40-bit accumulator, including guard sign and
nonzero guard bits, independently of SXM/OVM. Plain and delayed forms retain
the same existing Read/Evaluate and redirect paths; this extends the former
zero/nonzero predicate without changing TC/BIO behavior or atomic execution.
Phase placement remains a partial stage model, not silicon timing proof.

The 12,288 replay cases cover both accumulators/forms, six conditions, eight
signed/guard patterns including 40-bit limits, four SXM/OVM combinations, two
pages, four checkpoints and a post-Evaluate source-change control. The latter
is a firmware-free CPU conformance mutation, never an original-upload hook.
Target-literal poisoning, one taken-target fetch, real delay-slot source
changes, unchanged status and no data accesses are asserted. Twelve additional
LDM/long-offset-SUB/AGT fixtures use counts `0,1,2,65535`: the source reads
occur at clocks 5/7, SUB retires before evaluation, and taken targets fetch
at 10. Three checkpoints replay negative/zero/positive outcomes without
duplicate reads or writes.

BITF `61xx` retains its immediate mask separately from its source address.
Direct/short-indirect forms are two words; long-offset/absolute forms retain
the address extension before the mask and are three words (SPRU172C 4-22).
They share the existing captured-address machinery with memory arithmetic;
Read samples one RAM/mapped-AR word, Execute changes only TC according to
`(source & mask) != 0`. RAM and AR0-AR7 sources are accepted; control/peripheral,
repeat, protected/control-delay and broader producer compositions remain
unvalidated. Stage placement remains partial, not measured silicon timing.

The 52,224 RAM replay cases cover 17 read-capable encoding forms, eight encoded registers,
eight word/mask pairs (zero, bit 15 and opcode-shaped masks included), both
initial TC states, four SXM/OVM combinations, both compatibility states and
three checkpoints. Literal, pointer and post-Read RAM poisoning must not
change the result. Tests assert one read, no data writes, TC-only status
publication and unchanged accumulators. Ninety-six mapped-AR7 cases cover
immediate TC/NTC consumers, both initial TC values and three checkpoints;
source reads occur at 7, TC publishes at 8 and taken targets fetch at 10.
They include the exact upload sequence `61f8,0017,0001`; `007f` is an additional
conformance mask, not the original literal. The register changes after Read
must not be resampled, and neither leg may duplicate the source/target read.
Short MOD=3 is write-only and is rejected by CMPM/BITF in both frontends.
A separate 512-case replay bank checks rejection across both initial TC
states without data, pointer, accumulator/T or status effects. Long MOD=13
remains read-capable. These encoding checks do not settle the older circular
fixtures' input-domain constraints; see the [addressing audit](c54x_execution_contracts.md#operand-domain-acceptance).

This establishes bounded CPU and real I/O-retirement evidence, not complete
NAND-page, recorder, startup or speech acceptance. Existing atomic profiles
remain unchanged.

The frontend's firmware-free fixture reproduces the entry-side ordering of SPRU131G
example 7-18: acceptance after Decode at cycle 3, INTR Decode at 4, SP
decrement at 5, vector Fetch/IACK and the older decoded foreground instruction's
Execute at 6, continuation stack write at 7, and first ISR Execute at 10.
The already-prefetched flush word reaches Fetch at cycle 4 but never Decode
or Execute. The continuation is the first not-decoded instruction, not the
prefetch cursor. The test uses a one-word immediate load as the first ISR
instruction. Separate fast-return fixtures cover the example's return side.

RETF/RETFD increment SP and read RTN in Access, initiating target Prefetch
without a stack-memory read (SPRU131G examples 7-11/12). Execute clears INTM.
RETF discards the two sequential delay words; RETFD preserves them and
defers interrupt acceptance until they retire. Eight fixtures cover both
forms on base and extended program pages, restoring either pre-Access or
post-Access checkpoints. With interrupt acceptance at 3, return Access is
at 8, target Fetch at 9, return Execute at 10, delayed loads Execute at
11/12 and the resumed target Execute at 13 (example 7-18). No replay
increments SP twice or reads the stacked continuation. Four additional
RETFD cases fill both delay words with one two-word immediate load, on
base/extended pages. They restore partial Decode at cycle 8 or completed
Decode at 9, require one literal Fetch at 8, retire the load at 12 and
resume the target at 13 without treating the literal as an opcode.
Four pending-IRQ cases pulse INT0 after cycle 9 on base/extended pages and
checkpoint cycle 11. RETF accepts after Execute at 10, fetches the second
vector at 13 and writes continuation `0602` at 14. RETFD retains the pending
flag through both delay words, accepts at 12, fetches at 15 and stacks
`0604` at 16. Already-decoded target instructions drain while this entry
progresses. Both restored forms acknowledge exactly once per entry and
perform no return-side stack read. The input pulse is scheduler-synchronized
after the fixture callback, so IFR is checked from cycle 10, not inside the
callback issuing the pulse. These are executable conformance checks of the
staged acceptance/drain contract, not independent silicon latency measurements
or general control-flow/hazard completeness.

B/BD consume the target literal in Decode and initiate target Prefetch in
Access, not at atomic retirement. Eight base/extended-page fixtures match
SPRU131G examples 7-2/7-3: opcode/literal Fetch at 2/3, target Fetch at 6
and target Execute at 10. B flushes the two sequential words; BD executes
them at 8/9. The opcode and literal together retire as one retained operation
at 7; this frontend does not expose the manual's separate first-word Execute
occupancy at 6. Restoring partial Decode at 3 or redirected Prefetch at 5
fetches neither literal nor target twice. SP and INTM remain unchanged.
Eight further B/BD fixtures pulse INT0 after cycle 5, then restore at 6
or 8. The implemented acceptance contract keeps BD protected through
delay-word retirement at 9; its vector Fetch is at 12, continuation `0703`
is stacked at 13 and first ISR Execute is at 16. B accepts after its
retained two-word operation retires at 7: target Execute at 10 drains
alongside vector Fetch, `0701` is stacked at 11 and ISR Execute is at 14.
Every leg performs one vector acknowledgment and one literal/target Fetch.
This tests the model's interrupt acceptance/drain contract and restoration,
not independently measured silicon timing during a nondelayed branch's
flush occupancy. Conditional/far branches and calls remain outside coverage.

Separate empty-frontend IRQ and NMI fixtures accept at cycle 1, fetch the
vector at 4, write the continuation at 5 and retire the first ISR load at 8.
They stack the first unexecuted instruction at `0600`, not the advanced
prefetch cursor, and NMI preserves an unrelated latched IRQ flag.

DP-relative loads capture their operand address in Access, read the data bus
once in Read at cycle 5 and retire the captured value at Execute at cycle 6.
The fixtures change backing memory after Read and checkpoint that boundary:
both normal and restored retirement use the captured value, not a second bus
read. This covers both accumulators, not address/data hazards or multiple
data accesses.

Direct DP-relative STL/STH stores capture EAB in Read and commit EB in
Execute, as described in SPRU131G 7.1. Twelve fixtures cover A/B low/high
halves and existing unsigned/signed saturation semantics. Changing DP after
Read cannot redirect the pending write. All twelve restore a post-Read
checkpoint and require exactly one Execute-stage write per leg while the
older store drains alongside interrupt entry. These fixtures use ordinary
RAM; they do not establish MMR-write hazards or memory arbitration.

Short indirect loads perform DAGEN and the AR modification in Access, read
the retained address in Read and retire the captured value in Execute.
Eight AR1 fixtures cover both accumulators and plain/post-decrement/
post-increment/pre-increment addressing. They change AR1 after Access,
checkpoint there and require one read of the original address per leg,
without repeating the AR modification on restoration. Extended, indexed,
circular and compatibility-mode hazard contracts remain outside this scope.

Direct STM to DAGEN registers follows SPRU131G 7.5.2/Table 7-4 rather than
ordinary store retirement: the opcode word writes in Read, coincident with
Access of the retained two-word instruction. With STM first in the stream,
this is cycle 5. If an older STL/STH or RMW store writes ARx/SP/BK that cycle,
the register bank conflict defers STM by one clock; the deferred flag travels
with the instruction and is saved. Twelve executable fixtures cover AR2,
SP and BK, with and without a preceding STL to AR1. The older store writes
at 6 and the conflicting STM at 7, matching example 7-22's cross-register
bank arbitration. Checkpoints before/after the normal write and during/after
the deferred write restore without duplicate bus writes. These tests cover
that store/STM conflict, not general DAGEN hazard resolution or peripheral
register initialization timing. The negative Decode fixture now uses an
unsupported extended indirect STM `77e1`, not the qualified plain-indirect
RAM forms, rather than the supported direct AR1 form.

### Peripheral MMR access cost

[TI SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf), March 2001,
classes 12A and 18A (pages 3-26/3-39), specify a two-cycle base cost for
STM and ANDM respectively, plus one cycle for a peripheral MMR access.
The downloaded reference is
`roms/reference-docs/c54x/ti_tms320c54x_mnemonic_spru172c.pdf`, SHA-256
`c4f89ce4a774a76de1d93a2413e882a71b82d236fc2c52af63f3159368a37e75`.

The native semantic backend records peripheral data accesses during each
instruction and charges one extra clock at retirement. RMW read and write
share that charge. SPRU131G's memory maps distinguish peripheral registers
`0020..005f` from CPU registers `0000..001f` and scratch DARAM `0060..007f`;
scratch accesses must not inherit the penalty. Debugger/DMA bus accesses
do not run the semantic accounting path. Its transient flag is reset before
every synchronous instruction, not saved as an in-flight wait.

Twenty executable cases measure back-to-back operand bus intervals with
the timer stopped and INTM set. STM, ANDM, ORM, ST and LD each test AR2,
PRD, the last peripheral address `005f` and first scratch address `0060`.
Read-only accesses receive the same one-clock increment; RMW receives it
once rather than twice. Representative checked intervals are:

| Operation | Destination | Observed Write Interval | TI Class Cost |
| --- | --- | --- | --- |
| STM | AR2 | 2 clocks | 2 clocks |
| STM | PRD | 3 clocks | 3 clocks |
| ANDM | AR2 | 2 clocks | 2 clocks |
| ANDM | PRD | 3 clocks | 3 clocks |
| LD | PRD | 2 clocks | 2 clocks |
| LD | Scratch `0060` | 1 clock | 1 clock |

All twenty costs are asserted. Existing instruction fixtures for MMR22,
TCR, CLKMD, STLM/LDM20 and direct operand `0045` now include the documented
peripheral cycles; their former blanket CPU-register costs were wrong.
The shared `0060` bench callback serves stored scratch data for this gate,
not its separate IRQ-raising fixture behavior.

Native 5110 save/load passes. The four timer-boundary observations still
stall with short PRD and complete with long PRD: this cost correction does
not replace Decode/Execute ownership. A new fresh 30-second 5110 run retains
20,285 frame expiries, 648,192 RF reads and 1,520 port-32 writes, exactly the
previously reproduced mismatching snapshot. The strict RF gate was not
re-banked as part of that correction; the current independent-transfer
snapshot is documented separately above.
The opt-in staged frontend holds pipeline advancement for one clock before
a peripheral LD read, STL/STH write or ANDM/ORM read. A saved per-operation
flag owns the paid wait and travels with the retained operand; RMW does not
pay again at its write. Sixteen executable cases cover LD, STL, DP-relative
ANDM and absolute ANDM in both program pages, restoring either the held
stages or the resumed access. They assert the access/retirement clocks and
that save/load never repeats a bus side effect.

This stall placement is a declared prototype latency model, not recovered
silicon bus-phase timing. These class tables establish instruction costs,
not every multiword operand's exact bus phase; do not extrapolate first-word
DAGEN Read timing to all peripheral accesses. Peripheral STM remains outside
the staged instruction subset. Simultaneous peripheral read/write stage
occupancy stops as unsupported rather than inventing arbitration; that
exclusion is checked by adjacent peripheral STL/LD in the extended page.
The fixture stops at cycle 6 with zero CPU bus accesses and unchanged PRD
and accumulator contents. Debugger PRD export itself reads the bus, so its
observation occurs only after the CPU-access taps have been removed.
This proves the exclusion, not a silicon arbitration rule. No handset enables
the staged frontend, and these fixtures do not establish native RF or speech
completion.

ANDM/ORM retain the immediate separately from the sampled data operand.
The three-word absolute form consumes address then literal; its two pending
extensions are saved independently from the completed pipeline slots.
Access owns the target address, Read samples the value and Execute commits
the logical result at that retained address. Twelve fixtures cover both
operations with DP-relative, post-increment and absolute operands, restoring
either incomplete extension assembly or a post-Read/pre-write checkpoint.
Changing DP/AR after Access or backing RAM after Read does not alter the
pending operation. Restoring after Read performs no second operand read.

A separate absolute ANDM fixture targets the real IMR register through the
ordinary CPU data map: Read at 7 samples mask 1, Execute at 8 writes mask 0,
the already-accepted interrupt finishes, and a subsequent IRQ remains pending
with INTM clear. Its restored post-Read leg writes once without rereading IMR.
The complete synthetic timer program now executes in the staged subset,
as described below. General control flow, idle rules and native enablement
remain incomplete; this IMR fixture alone does not establish timer or recorder
completion.

Two-word immediate loads retain the opcode while the following fetched word
completes Decode as its literal. They consume distinct program fetches
(SPRU131G 7.1), never interpret opcode-shaped literals as instructions and
retire one clock later than one-word loads. Four IRQ fixtures cover A/B,
unsigned/signed extension and shifts 0/15/16: literal Fetch is at cycle 3,
complete Decode at 4, load Execute/vector Fetch at 7, and stacked continuation
`0602` at 8. All four replay a saved incomplete-Decode checkpoint without
rereading the literal. A separate uninterrupted stream retires consecutive
long loads at 7/9 and the following short load at 10, fetching each literal
once. The prototype defers interrupt acceptance until its two-word Decode
completes; these are executable implementation checks, not independent
silicon validation of multiword interrupt latency or pipeline hazards.

Slot addresses, fetched words, operand addresses/values, incomplete Decode,
validity, pending frontend redirection,
continuation and active-clock counter are saved. A checkpoint after cycle-4
Decode reproduces every subsequent observed stage and architectural result,
with one vector fetch/acknowledgement per leg. The bench suspends execution
for scheduler settlement after loading inside a live callback; that pause is
not emulated interrupt latency or a physical wall-time replay claim.
`check-c54x-core` requires staged-frontend, direct-load, store, indirect-load,
RAM/MMR RMW, multiword/replay/stream, empty IRQ/NMI and unsupported-form markers.
Existing native execution remains protected by its full conformance suite.
`verify-5110-save-state` passes with the added partial-Decode fields; this
revalidates native CPU/register/I/O restoration independently of the exact
RF scan snapshot described above.

The short-period timer probe has the following remaining staged dependencies.
These are exact encodings in `start_timer_boundary_probe`, not a general ISA
completion list:

| Probe encoding | Meaning | Staged ownership still missing |
| --- | --- | --- |
| `7681 literal` | Immediate ST through AR1 to PRD/TCR | Implemented bounded short-indirect ST with retained literal/address and peripheral write wait; this is not STM |
| `76f8 0501 0001` | Absolute immediate ST of the completion marker | Implemented three-word Decode and retained memory write |
| `4a1e` | PSHM XPC at the interrupt vector | Implemented retained source sample, stack decrement/address and stack write |
| `f4e5` | FRETE | Implemented plain retained far return; pending IRQ/adjacent push timing not independently validated |

The probe's `7711` DAGEN initialization, `6881`/`68f8` mask ANDM and `f073`
branch are already in the bounded subset. Peripheral STM coverage alone
cannot make this probe executable. SPRU172C identifies immediate ST as
classes 12A/12B (two/three words and base clocks), PSHM as class 16A
(single source read and stack write), and FRETE as class 34 (six clocks,
four when delayed). FRETE restores XPC from TOS, increments SP, restores PC
from the next TOS, increments SP again and clears INTM. Those architectural
operations and costs do not independently establish each prototype bus phase.
SPRU131G Table 7-4's early Read write rule for immediate ST applies to DAGEN
destinations, not automatically to PRD/TCR.

Immediate ST now retains its two/three-word literal through Decode, owns its
short destination in Read and writes the retained value in Execute. Absolute
destinations come from the saved extension word. The existing peripheral
wait applies once before a PRD/TCR-class write. Eight executable cases cover
DP, post-increment, absolute and peripheral destinations, restoring before
or after the write. An opcode-shaped literal remains data, and post-increment
and bus writes occur once across replay. This is the same declared partial
bus-placement model as other staged stores, not independently measured
silicon timing. DAGEN destinations stop before their write because their
early-write contract is not implemented; indirect address modification may
already have occurred. Long-offset, compiler-mode direct and other indirect
encodings remain outside this subset.

Direct PSHM forms `4a00..4a1f` sample their MMR once in Read, retain the
decremented stack destination, then commit the sampled value in Execute
(SPRU172C page 4-132, class 16A). The 198 cases retain the six explicit XPC
controls and cover all 32 direct sources in both program pages, restoring
before Read, after Read or after Write. Each requires one source access,
one SP decrement and one stack write per leg. PSHM SP samples the original
SP before decrement; the original MU4 prologue's `4a06`/`4a07` ST0/ST1
pushes are now accepted by this staged subset.
Read-owned stack allocation is a declared partial stage model, not a silicon
phase claim. Other PSHM sources, indirect forms, SP/DAGEN conflicts and
preceding accumulator-producer latency are not covered by this implementation
or gate.

Staged direct POPM supports `8a06..8a19`: ST0/ST1, accumulator
parts, T/TRN, AR0--AR7, SP and BK. It retains the stack word and increments SP
in Read. AR0--AR7, SP and BK also publish in Read (SPRU131G table 7-4);
other supported destinations publish in Execute. An older Execute-stage
store into the DAGEN bank defers the early publication one clock, as
specified by table 7-5. The previous all-destinations Execute placement
was a prototype assumption contradicted by that table, not silicon evidence.
The 19 non-SP destinations have both program pages and three replay
boundaries, giving 114 cases. Each requires one stack read/increment and
one register write per leg, correct guard-byte masking and ST1/XF output.
XPC, other control/peripheral destinations and indirect forms are
rejected. Twelve additional replay cases compare an older STL to AR1
with an older STL to RAM: POPM AR2 keeps its stack read/increment in
clock 6, but only the DAGEN-bank case defers its destination write to
clock 7. Changing the stack source after Read must not change the
retained result. General mixed DAGEN/status-producer conflicts remain
partial, not measured silicon behavior.

Basic compiler-mode LD/ST retain their Access-stage SP sample separately
from Decode-time CPL/DP. This gives the one-instruction category-I consumer
latency after ordinary PSHM/POPM MMR in SPRU131G table 7-9: an immediate consumer uses the old
SP, while one intervening NOP allows the updated SP. Sixteen cases cover
PSHM AR1/SP and POPM AR1/BL, LD/ST and both spacings, restoring after the SP
sample and before consumer Read. Each verifies the operand or destination, untouched
alternate address, retained push value and final SP. Remaining SP producer classes,
extended-shift category-II consumers, indirect forms and physical stage
placement remain unvalidated. The atomic frontend is unchanged.

POPM SP is distinct: it publishes the popped value instead of leaving
the postincremented stack address, while a basic compiler-mode consumer
requires two cycles of separation (table 7-9 Category I). STM/MVDK SP
and FRAME require one; a DAGEN-bank conflict adds one to POPM SP.
The staged frontend retains the prior compiler-address SP separately
from architectural SP, including save-state registration. Stack operations
use architectural SP directly; the compiler-address window must not
delay them or generate another register publication.

The 320 replay cases cover those four producer kinds plus conflicted
POPM SP, basic LD/ST consumers, zero through three NOPs and two program
pages. A popped/moved source changed after Read must not alter SP.
They distinguish the intermediate postincrement during a deferred pop
from both the old compiler-address base and the final popped SP. No
extra source reads or register/store writes are permitted on replay.
This closes the tested basic Category-I cases, not general multiword
consumers, other SP producers or all bank-conflict chains.

Four cases execute the original `8a18 f495 f495 fc00` at
`02:3d3a`--`02:3d3d`. POPM SP restores the caller frame, then the two
NOPs and RET consume its captured return word at that restored address.
Changing the popped SP word and return word after their respective reads
cannot retarget the return. The same-page caller marker retires with one
SP publication and two data reads per leg. This is the original wrapper
suffix, not the complete idle/wake call.

Plain RET/RETE retain the stack address and increment SP in Access, sample
the return word in Read and redirect at Execute (SPRU131G example 7-7;
SPRU172C pages 4-139--140, class 32). Allocation accounts for older
PSHM/POPM Read updates. Table 7-2's first-half DB read precedes a same-clock
second-half EB write: an adjacent push exposes the old DARAM word, while
one intervening NOP exposes its completed write. No stack-return bypass is
inferred from the old prototype fixture. RET preserves INTM; RETE
clears it at retirement. Target Prefetch/Fetch/Execute is clock 6/7/11 for
a return starting at Prefetch 1. Thirty-six cases cover both forms/pages,
isolated, adjacent-push and one-NOP returns, with restoration around Access/Read.
They require one return read/pop/fetch and no sequential execution. General
DAGEN conflicts and pending IRQ timing for these plain forms remain unvalidated.

RETD follows example 7-8: its two prefetched delay words are saved rather
than decoded early or fetched again. After return Read they enter Decode at
clocks 6/7, with target Prefetch/Fetch/Execute at 6/7/11. Twenty-four cases
cover two pages, two one-word delays, one two-word STM-to-T delay, and the
original `fd45 f6bb` (XC 1,AEQ / RSBX INTM) pair. Each replays queued words
and the retained stack sample without duplicate reads or a third delay Fetch.
Twelve DARAM ordering controls require the old stack word for adjacent
PSHM/RETD and its new word after one NOP, without altering delay execution.
XC protection inside the return delay is mapped onto the resumed word stream,
not the abandoned sequential address. Twelve cases make an IRQ pending
during return: A=0 allows the INTM clear, but the first resumed instruction
must retire before IRQ acknowledgement; A=0800 rejects the clear and leaves
the source pending. This validates that bounded delay/IRQ composition, not
RETED, nested controls, NMI, other XC placements or physical silicon timing.
Ten further cases execute all seven original `02:3ddb`--`02:3de1` return-tail
words with saved ST1 values `0034`/`0834`. POPM samples the saved status,
AND reduces A to its INTM bit, and the original XC/RSBX pair either restores
unmasked state or preserves the mask. Both stack words are changed after
sampling; five replay boundaries must still reproduce the retained values,
one target Fetch and the correct delayed IRQ outcome. The first continuation
instruction retires before IRQ acknowledgement. The continuation/vector are
neutral test instructions, not the native caller/ISR. The read-only contract
tool pins all seven words and classifies every saved ST1 value: 32,768 masked
and 32,768 unmasked. It describes restoration of INTM only, not full ST1.
Complete worker-body execution and the native recorder remain unvalidated.

The original nonzero-control-word route has a coherent call/return fixture:
four CALLD/STM parameter words at `02:3d92`, nine worker-entry words and
seven return-tail words, all unchanged from R060. Sixteen cases cover saved
masked/unmasked ST1 and eight replay boundaries. The call stacks `3d96`,
the worker stacks that ST1, and its skip-body branch consumes two source
samples before the tail pops both owned words and balances the caller SP.
T receives parameter 6. Source and stack RAM changes after sampling must
not affect retained values; CPU bus counters exclude those debugger-side
mutations. Pending IRQ delivery resumes only after the caller continuation's
first instruction. There are no duplicate stack/source reads or route Fetches
on replay. The source words, caller continuation and vector remain fixture
inputs; this does not execute the zero-control-word body, outer interrupt
handler or live recorder and does not establish native speech.

The complete original outer routine now has a bounded call-route fixture:
all 51 main words plus the nine-word worker entry and seven-word return tail.
Its input status `c004` selects the call and a nonzero second worker word
selects the mapped skip-body route. Forty cases cover pages 0/2, masked and
unmasked entry ST1, and ten replay boundaries across context save, DP delay,
nested call, branch, return and restoration. The fifteen ordered stack writes
include all outer/extra saves, the nested return, worker ST1 and reserved
return slot. Terminal checks require original A/B, ST0/ST1, AR0/AR2/AR3/T,
balanced SP, one counter increment and one reserved-return Fetch. Checkpoint
bus counts are retained separately from emulated device state so replay
cannot hide duplicate operations. The routine is entered directly with
fixture RAM and a neutral final continuation; timer-driven interrupt entry,
the zero-control-word worker body and live recorder remain unvalidated.

Staged CALL/CALLD retain a stack destination at the opcode's Access clock
(complete target Decode), load RTN in the next clock and write the retained
return word at the opcode's Execute clock (SPRU131G examples 7-4/5).
For a call starting at Prefetch 1 these are clocks 4/5/6, with target Fetch 6.
CALL stacks the PC after its two instruction words; CALLD includes both delay words.
RTN is also exposed as a debugger state register without changing the MMR map.
Eighty-four round trips cover both forms/pages, with or without an older
PSHM, restoring after allocation, RTN setup or stack commitment. Each requires
one call write, target fetch and callee RET read, correct delay execution and
balanced call SP. The older push retains its own distinct stack word. General
DAGEN conflicts, pending IRQ timing and nested in-flight controls remain
unvalidated by these cases.

The exact `770e literal` STM-to-T form now uses the retained two-word register
write path. In this prototype it publishes at the opcode's Read clock, like
the existing early DAGEN STM path, but is not deferred by DAGEN-bank conflicts.
This is a partial placement model of SPRU131G example 7-39's zero T latency,
not a measured physical write phase. Sixty of the call cases use STM-to-T as
the complete CALLD delay, then LDM T in the callee. They cover parameters
`0006`, `0000`, `8000`, `f495` and `ffff`, both pages, adjacent pushes and
checkpoints after call allocation/RTN/stack write. Each requires one T write
before callee Read, correct unsigned parameter delivery, balanced call SP
and no duplicate publication on replay. The original `770e 0006` is therefore
executable at the call boundary; general STM destinations and concurrent
MMR-producer conflicts remain unvalidated.

The original worker begins at `02:3dbc` with `fd00 4a07 fd00 f7bb`:
unconditional `XC 1` around PSHM ST1 and SSBX INTM. All four words are
present in the original R060 upload. This is not a conditional worker-entry
predicate. SPRU131G section 7.1.4/example
7-13 places XC condition sampling in Access, before the following instruction
is decoded; the two preceding one-word instructions have not retired yet.
SPRU172C pages 4-198--200 permit XC 1 over one one-word instruction and XC 2
over two words, and protect XC plus two following words from interrupts even
for XC 1. Rejected slots must not decode or perform memory accesses.
The atomic implementation's execution-time predicate evaluation therefore
cannot be reused as a physical staged sampling contract. Staged acceptance
now covers the original unconditional prefix in four checkpoint/replay
cases: PSHM publishes the old ST1 once before SSBX sets INTM, with the
stack allocation preserved on restoration. Four further cases execute the
original seven words through `7192 0010 1092`, with neutral NOPs afterward.
MVDK samples the old AR2 address once, increments AR2 and publishes AR0;
the adjacent LD samples the next word and increments AR2 again. Source RAM
changes after the move sample cannot change the retained AR0 value. Saved
context at four boundaries reproduces one stack push and two ordered RAM
reads without duplicate pointer updates. This is bounded prefix composition,
not a complete worker, interrupt entry or native recorder run.
A saved word-address guard
implements the two-word protection window. Four additional XC 1/XC 2 cases
make a maskable IRQ pending after Decode: two following stores must retire
before the vector is fetched once. Restoration during protection or at its
completion preserves both stores, IFR clearance and one interrupt stack
allocation. Multiword protected slots, NMI and protected control transfers
remain unvalidated. The four TC/NTC conditional forms now sample TC in
Access before older Execute and the next word's Decode. A saved skip-word
counter replaces rejected words with NOPs before operand decoding.
Thirty-two cases cover both conditions and word counts, changing TC between
Decode and Access and again after Access. Only the Access value controls the
two following stores; rejected stores never publish, including after four
checkpoint/restoration boundaries. Another 32 cases use one two-word
immediate store with the opcode-like literal `fd30`: accepted execution
stores that literal as data, rejected execution suppresses both words, and
the following independent store still retires. Sixty-four instruction-driven
cases place SSBX/RSBX TC before XC with zero through three NOPs. XC observes
the old TC with zero/one NOP and the new TC with two/three, matching the
Access-time latency in example 7-13. All 128 cases replay saved Decode,
sampling or retirement state without duplicate writes. General multiword
operands and conditional IRQ integration remain unvalidated.
The four accumulator zero/nonzero predicates are also supported in Access.
Forty-eight cases cover AEQ/ANEQ/BEQ/BNEQ, both word counts and three replay
boundaries. Accumulators switch between Decode and Access and switch back
after sampling; a value nonzero only in the high accumulator bits must still
select the nonzero condition. Accepted stores retain their publication
clocks, rejected stores perform no writes, and ST0/ST1 are unchanged.
This makes the return tail's exact `fd45` executable; its placement within
RETD is covered by the delayed-return fixtures. Accumulator producer latency,
compound/overflow predicates and other delayed-control/XC combinations
remain unvalidated or unsupported.
The complete worker remains unsupported.

Direct `MVDK Smem,dmad`, including the original worker setup `7131 0012`,
now retains its source sample and destination literal in the staged frontend.
The bounded subset accepts RAM sources and RAM or DAGEN destinations
(`10`--`19`); peripheral/control transfers and extended/repeated forms remain
excluded. Following SPRU131G section 7.5.2 and SPRU172C class 19A, the
prototype writes DAGEN at the opcode's Read clock and RAM one clock later.
Sixty cases cover DP/CPL source addressing, AR2/SP/BK/RAM destinations,
unsigned values and restoration before/after source sampling or destination
publication. Changing source RAM after sampling must not change the copied
value; replay requires exactly one source read and destination write.
Nine cases additionally require one-clock DAGEN deferral behind an older
direct store to AR1; three RAM-destination controls retain ordinary write
timing. Checkpoints straddle source sampling and deferred publication.
General producer hazards and precise physical phase placement therefore
remain unvalidated; this does not enable the staged frontend for native firmware.

Short indirect MVDK now uses the same retained transfer. Forty-eight additional
cases exercise `7182`/`7192` (stationary/postincrement AR2), copying to AR0,
AR2, SP or RAM. The source is sampled at its old address once, then the
source AR is updated before the destination is published. Twelve overlapping
AR2-destination cases require the copied value to replace the incremented
pointer, rather than a later increment corrupting the result. Source RAM is
changed after sampling, and all three replay boundaries must retain the old
word with no duplicate read/write. This makes the original `7192 0010` worker
setup executable in isolation. Short modes below `98` use the existing
address generator; other AR/mode combinations, producer hazards and physical
phase placement are not established by these fixtures. Complete worker and
native recorder execution remain separate, incomplete requirements.

The read-only context tool also pins all nine original worker-entry words
`02:3dbc`--`02:3dc4`. Its non-delayed `f844 3ddb` branch skips the body at
`02:3dc5` when the second source word loaded through AR2 is nonzero; zero
falls through into that body. Exhaustive unsigned-word classification gives
65,535 taken inputs and one fallthrough input, without claiming which value
native recorder execution supplies. The first source word goes to AR0, the
second to A, and AR2 advances twice; neither source's producer is identified
by this bounded census. SPRU131G section 7.1.5/example 7-16 requires BC to
take five cycles when taken and three when not taken, discarding sequential
work only for the taken outcome. BCD's unconditional delay-slot execution
cannot be reused for this branch. The staged BC subset now supports
AEQ/ANEQ/BEQ/BNEQ with a saved prefetched sequential word. At the prototype's
Evaluate clock it either releases that word to Decode or discards it and
the second prefetched word before either can access data RAM. Forty-eight
cases cover both pages and outcomes, with restoration before/after word
retention and predicate evaluation. Each sequential word is fetched once;
taken branches fetch the target at clock 7 (Execute 11) and perform zero
sequential data reads, while rejected branches retain the two reads at
clocks 8/9. Changing the tested accumulator after evaluation cannot change
the route. Existing BCD delay-slot gates remain unchanged. This is bounded
comparison against example 7-16, not independent silicon timing evidence;
multiword sequential slots and IRQ integration remain unvalidated.
Six further fixtures compose the original nine words at their original
page-2 addresses, with neutral NOPs after the prefix and at the target.
The adjacent LD's sampled second word is zero or one, causing BC to
fall through or take `02:3ddb` respectively. Three checkpoints surround
the load sample/retirement and branch decision. Source RAM changes after
sampling and A changes after evaluation cannot alter the retained route.
The selected target or fallthrough Fetch occurs once at clock 14; both
outcomes retain one old-ST1 push, two source reads and two AR2 updates
without duplicates on replay. This validates that specific adjacent producer
composition, not the unknown worker body, return tail or native recorder.

Plain FRET and FRETE retain two ordered stack samples, mask the restored XPC
to seven bits, pop SP twice and redirect. FRET preserves INTM; FRETE clears it
after both samples, following SPRU172C pages 4-61--62.
The prototype holds Read for the second sample, flushes sequential words
and restarts target Prefetch at clock 7 (Fetch 8, Execute 12 for a return
starting at Prefetch 1). Twelve cases cover both instructions, cross-page
directions and restore before the first read, between reads or after both reads. Older
Execute writes precede the first stack sample, so an adjacent PSHM's word
can be observed. This is a declared bus-placement model of the documented
six-clock return, not independent silicon phase evidence. The FRET cases
start with INTM set and require it to remain set across return and replay.
Delayed FRETD/FRETED retain XPC in Access and PC in Read, then start target
Prefetch at clock 5 (Fetch 6, Execute 10). This is a declared placement model
of the documented four-cycle transfer, not independent silicon phase evidence.
Two one-word delay instructions retire at clocks 7/8, or one two-word delay
instruction retires at clock 8. The next sequential word is discarded;
control-word ownership prevents maskable entry before the delay completes.
Twenty-four delayed cases cover both return forms, both cross-page directions,
both delay-slot layouts and restore before, between or after stack samples.
Together with the twelve plain cases, all 36 require one target fetch and two
ordered stack reads across replay, correct SP/INTM and no extra sequential
instruction execution. Nested control transfers in delay slots are rejected.
Eight pending-IRQ cases cover plain/delayed returns in both cross-page
directions, with a request after clock 4 and replay from clock 5. FRETE/FRETED
fetch the vector in the restored page and stack the first undecoded target
continuation (`0700` plain, `0702` delayed); decoded target work drains before
entry. FRET/FRETD controls preserve INTM and the pending request, with no
vector fetch or entry stack write. Every enabled case requires one vector
fetch and one continuation write per replay leg. These are staged-model
observations, not independent evidence of silicon re-entry phases. General
stack conflicts and physical pending-interrupt timing remain unvalidated.

The exact `f6bb`/`f7bb` RSBX/SSBX INTM forms establish protection in Decode
and retain it through retirement of the next complete instruction, including
its extension words (SPRU131G section 6.10.7). This is instruction-owned
state, not a fixed clock delay; both the following-instruction latch and
per-stage protection survive save/restore. Twelve cases cover both forms with
one-, two- and three-word followers, injecting a pending maskable interrupt
after INTM Decode and replaying a checkpoint during follower Decode. Six
include adjacent opposite INTM operations: protection extends through the
second operation's complete follower without an intervening vector fetch.
RSBX
permits one vector fetch only after the follower retires; SSBX leaves the
request pending without a vector fetch. The three-word store commits once
per replay leg. NMI interaction and silicon phase
placement are not validated by these cases. Handset configurations still
use the atomic frontend; this does not resolve the MU4 recorder boundary.

Both RSBX and SSBX support all sixteen bits in ST0 and ST1, matching the
one-word class-1 forms in SPRU172C pages 4-151 and 4-166. The staged
implementation changes only the selected bit at Execute, without invoking
the atomic backend's interrupt guard. Only the ST1 INTM forms establish
Decode-owned follower protection. All 128 combinations of register, bit,
set/clear and initially all-zero/all-one status are executable fixtures;
each restores immediately before Execute and verifies both status registers
on every observed cycle. The device's XF output must match ST1 bit 13 on
every cycle and after checkpoint restoration; staged retirement publishes
the output through the same memoized callback as the atomic frontend.
This includes the original MU4 handler's `f6be`
RSBX CPL. Repeat legality and full handler execution through the staged
subset remain unvalidated.

Basic direct LD/ST retain Decode-time CPL and DP independently of the later
address-generation stage. SPRU131G tables 7-18 and 7-19 require three
intervening instructions after RSBX/SSBX DP or CPL for these category-I
consumers. Thirty-two cases cover CPL and DP bit zero, set/clear, LD/ST and
zero through three NOPs, with distinct data-page and stack-relative
sentinels (or two data pages for DP). Zero through two NOPs use the old
field; three use the new field. Every case restores after consumer Decode,
before its address is generated, and reproduces the selected address and
untouched alternate location. Other DP producers, extended-shift category-II
consumers, SP conflicts and immediate/RMW direct forms are not established
by this gate. The atomic frontend and handset configurations are unchanged.

The staged `LD #k9,DP` form forwards the nearest in-flight immediate DP to a
basic direct consumer's retained Decode state, implementing the zero-latency
contract in SPRU131G table 7-17 and example 7-44. Architectural ST0 changes
at Execute, not early Decode. All 512 page values are tested with an adjacent
LD, and again behind two consecutive immediate-DP producers (1,024 cases).
Each case restores after consumer Decode but before producer retirement;
the new page supplies the operand, while upper ST0 bits and ST1 remain
unchanged and the old architectural DP persists until Execute. This is a
bounded forwarding implementation, not measured silicon bus placement;
mixed status-register producers and other DP consumer families remain open.

Direct `LD Smem,DP` retains one memory sample, holds pipeline advancement
for two additional clocks and commits the low nine bits into DP at Execute
(SPRU172C page 4-71, class 5A). Forwarding updates younger basic direct
operands, giving the zero-DP-latency contract in SPRU131G table 7-17 without
early architectural ST0 commitment. All 512 page values have restore before
Read, after Read or during the held interval (1,536 cases). Each requires
one source read, one adjacent-consumer read and preserved upper ST0/ST1/B;
changing source memory after Read must not affect either DP or the consumer.
The declared prototype samples at clock 5, retires the producer at 8 and
the consumer at 9; physical stage placement remains unvalidated. The gate
covers a RAM source and adjacent basic LD only. Indirect/extended forms,
peripheral-source timing, mixed producers, store consumers, interrupt overlap
and delayed-control composition remain outside this acceptance scope.

Staged FRAME accepts all signed eight-bit offsets. The prototype updates SP
in Access and does not apply the offset again at Execute; the immediately
following PSHM therefore allocates from the adjusted stack, as required by
SPRU172C page 4-60. All 256 offsets have three checkpoint/replay boundaries
(before/after FRAME's update and after the push's source sample), giving 768
cases. Each verifies unchanged status, one FRAME update, one push decrement
and one retained write per leg. Access placement is a declared partial model,
not measured silicon timing. Basic compiler-mode consumers, earlier SP
writers, interrupt overlap and wrapping SP values remain unvalidated. The
original MU4 prologue's `eeff` is accepted, not a claim that its full handler
or the recorder completes.

Direct DLD retains the high word at its generated address and the low word
at address XOR 1 in Read, then assembles the accumulator at Execute
(SPRU172C page 4-42, class 9A). Both destinations, SXM/C16 settings,
high-word signs, odd/even addresses and DP/CPL addressing have 128 cases
with restore before/after Read. Changing both source words after Read must
not affect retirement. The other accumulator, SP and both status registers
remain unchanged; negative SXM loads extend the guard byte. Register/peripheral
pairs, indirect and extended-address forms are rejected. Preceding status-
producer latency remains unvalidated. Paired Read placement is a declared
prototype model, not independent physical-bus timing evidence.

Staged two-word immediate ADD retains its literal through complete Decode
and uses the shared arithmetic helper at Execute without another program
read (SPRU172C page 4-4, syntax 7). All source/destination combinations,
shifts 0/1/7/15, SXM/OVM settings and positive/negative boundary operands
have 256 cases with restore during partial or complete Decode. They require
correct carry, destination overflow and saturation; changing the literal
after Fetch must not affect retirement. The additional SUB/shift-16 matrix
below covers the other arithmetic forms; mixed status producers and
delayed-control overlap remain unvalidated.

Immediate SUB (`f010` family, shifts 0--15) and the separate shift-16
ADD/SUB encodings (`f060/f061` with source/destination selectors) use
the same retained-literal path. Execute extends the 16-bit literal under
SXM, shifts it, then uses the shared 40-bit arithmetic helper without
fetching program or data memory. SUB computes selected source minus
operand, not selected destination minus operand; source/destination
aliasing reads the original source. Carry reflects the low-32-bit
operation before OVM saturation, and destination overflow is sticky.
The unselected accumulator, other overflow bit and remaining status
fields are preserved.

The new matrix has 26112 SUB cases across shifts 0--16, all four A/B
selections, eight source/literal patterns, eight SXM/OVM/sticky-overflow
states, two pages and saves during partial Decode, complete Decode or
Read. Another 1536 cases cover shift-16 ADD. Opcode-shaped literal `f495`,
40-bit guard/sign boundaries, borrow/carry and saturation are included.
Fixtures change the program literal after Decode and assert publication
only at Execute, clock 7 in this partial frontend model, with one literal
fetch and no data-memory access on each replay.

Another 144 firmware-free producer/replay cases compose a two-word
signed immediate load with SUB through zero, one or two intervening NOPs,
all four A/B selections and source values `0,1,8000,ffff`. They verify
that SUB sees the load's Execute publication and that the sequence drains
to IDLE without literal refetch. Load publishes at clock 7 and SUB at
clock `9 + gap` in this model. These fixtures do not validate silicon
source-read phases, mixed status producers, wider producer conflicts or
delayed-control overlap.

Immediate AND/OR/XOR retain unsigned literals through the same two-word
Decode path and leave both status registers unchanged, independently of
SXM/OVM (SPRU172C instruction contracts). Each operation has all four
source/destination combinations, shifts 0/1/7/15, positive/guard-negative
sources and both partial/complete-Decode replay points: 768 cases total.
Changing the literal after Fetch must not alter the result or cause another
program read. The other accumulator is preserved and guard bits are not
arithmetically saturated. Special shift-16 forms, mixed status producers
and delayed-control overlap remain unvalidated.

Direct DST retains the saturated 32-bit source after older Execute writes,
then writes the high word and XOR-1 low word in two clocks (SPRU172C page
4-47, class 13A). The pending second write holds pipeline advancement and
is saved explicitly; interrupt acceptance is reconsidered only after the
pair completes. All 192 cases cover A/B, SST on/off, in-range/guard-overflow
signs, odd/even addresses, DP/CPL and restore before/after source capture
or between writes. Changing the source after capture must not alter either
write. The gate verifies partial memory state, one write per word and
unchanged status/SP. Exact write-phase placement, interrupt overlap and
preceding SST-producer latency remain unvalidated; indirect, extended and
register/peripheral pairs are rejected.

Direct LDM retains a page-zero register/RAM operand in Read and zero-extends
it into A/B at Execute, independently of DP, CPL and SXM (SPRU172C page
4-73, class 3A). The existing peripheral-read wait applies at addresses
`20..5f`. Nine representative sources cover status, accumulator parts, AR1,
SP, XPC, negative-valued RAM and PRD. Both destinations, program pages,
SXM/CPL and pre/post-Read replay give 288 cases, requiring one read and no
status/SP or other-accumulator change. PRD has the modeled extra clock;
post-Read RAM changes must not affect the result. Indirect forms, preceding
MMR producer conflicts and physical wait placement remain unvalidated.

Staged BCD supports AEQ/ANEQ/BEQ/BNEQ. The complete two-word slot retains
its 40-bit zero predicate in Read (the opcode's Evaluate clock), after older
Execute writes, and uses that predicate at redirect. One empty prefetch slot
models SPRU131G example 7-17; both delay words execute on either outcome.
Taken branches discard the next sequential word. Ninety-six cases cover all
four conditions, both program pages, both outcomes and three checkpoints;
changing the selected accumulator after sampling must not change the route.
Forty-eight cases compose a memory-to-DP delay slot and NOP, verifying its
single source sample, held clocks and the target load through the new DP.
Target Fetch/Execute is clock 8/12 with immediate-load delays, or 10/14 with
the memory-DP delay. These are prototype stage observations, not silicon
measurements. Nondelayed BC, other condition families, interrupt overlap and
general multiword delay instructions remain unvalidated.

The exact original R060 AA22 skip-worker path composes 37 words:
`02:3d73..3d8b` and `02:3d9a..3da5`. It saves status/accumulator context,
increments a long-memory value, computes the status mask, executes BCD with
the real memory-DP/NOP delays, then restores context and executes RET through
the common tail.
Six executable cases cover pages 0 and 2 with checkpoints after the first
retained source, a partial accumulator save or the POPM BL/store overlap.
Each requires eight ordered context saves and one reserved-slot write,
restored A/B/ST0/ST1, final SP `1000`, the incremented long-memory value and
return slot `0fff=4044`, followed by one fetch at `4044` in the same program
page. The store uses the pre-POPM SP sample, not the live
incremented SP. Replay must not duplicate writes. The bytes were checked
through the original upload decoder against `InitData_R060.a00`; this
CPU-conformance fixture runs the copied path through the original `fc00`,
then executes NOPs at the reserved continuation, leaving the original image
and firmware runs unchanged. This establishes the skip-worker routine path,
not its real interrupt-entry integration or live recorder completion. The
worker-call path is not executed by this fixture.
Native handset/recorder execution still uses the atomic core.

The read-only `mu4_context_contract.py` pins 51 original AA22 words through
`02:3da5` against the R060 source identity. `487e; f030 c004; f050 c004`
leaves A equal to `(status[007e] & c004) XOR c004`. Therefore BCD ANEQ
skips the worker exactly when one of bits 15, 14 or 2 is absent. Exhaustive
predicate coverage gives 57,344 skip values and 8,192 worker-call values;
this is not an observation of the live recorder's status or branch choice.

The branch always has delay words `467f` and `f495`: LD from page-zero
`007f` into DP (three documented base cycles) and NOP. BCD itself has three
base cycles; SPRU131G example 7-17 explicitly shows conditional evaluation
and a prefetch bubble. Reusing unconditional BD's Access-stage redirect
without that distinction is not justified. The untaken branch additionally
saves AR0/AR2/AR3/T, reaches CALLD `02:3dbc`, executes `770e 0006` (STM #6,T)
as its two-word call delay, with return continuation `02:3d96`. The skip
path and returning-worker continuation reach `02:3d9a`, where a current-DP
load precedes CPL setup and context pops.
The skip-worker path's DP comes from the branch delay; the worker's effects
on DP remain unclassified. The pinned `POPM BL -> STL A,6` pair remains
the known compiler-mode SP-latency boundary. The final `fc00` is pinned,
not a claim of correct interrupt-return semantics or full handler execution.

Reproduce the bounded original-code audit with:
`python3 tools/mu4_context_contract.py roms/5510-mu4-reference/InitData_R060.a00`.
Its unit tests reject missing or changed witness words and enumerate the
complete status predicate. CALL/CALLD round trips and the original worker's
setup MVDK are supported in isolation; the callee instruction stream remains
incomplete. Establish that and interrupt integration before a full
original-handler fixture; neither skipping
delay effects nor a fixed interrupt delay is a substitute.

The original four short/long-period, direct/indirect-mask streams run through
this staged subset without an unsupported-opcode stop. Each records one
foreground fetch, one mask write and marker 1, with SP back at `1000`, but
zero vector entries. The timer is not silent: TCR reload occurs at clock 15,
and short-period IFR is `0008` by the mask at clock 19 (indirect) or 18
(direct). In this dense stream, incomplete younger multiword Decode defers
acceptance until the older mask can retire. That interpretation still needs
silicon validation; these cases alone do not test ISR execution.

Four controls insert four explicit NOPs after the reload, leaving timer
periods and hardware state unchanged. The short-period direct control enters
the adjacent PSHM/FRETE ISR once, completes the mask/marker and restores SP
to `1000`. The short-period indirect control instead records 143 vector
entries, one foreground fetch and no mask/marker, with a live ISR at the
observation deadline. Both long-period controls complete without an entry.
The gate requires actual ISR entry for short gap controls and successful
direct-mask stack restoration; the indirect result remains an observation,
not a passing completion claim. It demonstrates that the staged frontend
does not generally solve short-period starvation.

Staged taps observe program bus reads without the atomic backend's
PC-at-fetch guard. Reload/mask/final observations include PRD, TIM, TCR,
IMR, IFR and ST1; they separate a live timer from deferred acceptance.
Evidence: `tmp/c54x-staged-timer-audit.log`. The atomic probe still stalls
for short PRD and completes for long PRD; its behavior is unchanged.

The capped continuation trace distinguishes loss from repeated entry. In the
short-period indirect gap control, the first entry stacks PC `060e` at clock
22; FRETE samples XPC `0001` and PC `060e` at clocks 25/26. The mask opcode
at `01:060e` is fetched at 28, then again at 37/46/55 as successive entries
replace Decode before the literal arrives. The continuation is preserved,
but immediate re-entry prevents its two-word Decode completing. In the
direct control, the already-decoded absolute mask drains during entry at
clock 22; PC `060f` is stacked at 23, returned through reads 26/27, then
fetched at 29 before the marker completes. This is implementation evidence
for retained drain versus pre-Decode replacement, not a silicon proof or
justification for an added interrupt guard. Evidence:
`tmp/c54x-staged-continuation.log`.

Next validate far-return re-entry timing against independent pipeline evidence;
repeat/delayed-control and idle rules remain separate completeness work.
Those contracts must precede enabling
the frontend for native firmware or claiming the timer/recorder stall fixed.
A constant interrupt delay or adjusted timer period cannot replace in-flight
instruction ownership. The legacy vendor activation failure limits the
optional differential oracle; it does not block this software implementation.
Native acquisition, recorder finalization and handset speech remain unpromoted
until their own end-to-end gates pass.

The passive foreground-fetch tap in `probe-c54x-timer-boundary` narrows that
implementation boundary: both period-one cases accept 108 entries with
return PC `0608`, but never fetch the opcode at `010608` and never write the
IMR mask. Long-period controls fetch that opcode exactly once. Indirect
addressing fetches it two scheduler cycles after the reload and writes IMR
four cycles after reload; direct addressing fetches and writes IMR two cycles
after reload. Evidence: `tmp/c54x-foreground-fetch.log`. Thus the starvation
is upstream of operand access or instruction retirement, not a fetched mask
instruction whose store was lost. The staged controls above retain in-flight
instruction ownership and expose a different result according to whether
the mask was decoded before entry. A late write queue alone cannot model
that distinction. Neither observation proves that overloaded short-period
foreground starvation is itself a silicon defect. These are scheduler-accounting
observations, not measured silicon latencies or permission to insert a fixed
two-cycle interrupt delay.

This is bounded debugger coverage, not a complete ISA disassembler or new
execution/timing evidence. Other forms remain unsupported raw words.
Non-delayed calls and returns carry debugger stepping flags. Delayed calls
and returns deliberately omit those flags: C54x delay slots count words,
whereas MAME's extra-step count counts instructions, so blindly requesting
two extra instructions would be wrong for a two-word slot instruction.
Encodings and delay-slot rules follow the
[TI C54x instruction reference](https://www.ti.com/lit/ug/spru172c/spru172c.pdf).

`tools/c54x_opcode_coverage.py` compares `[opcov]` records from a verbose
30-second 5110 v5.30 run with the standalone `tms54test` fixture. The current
fingerprint and assertion totals are defined only in **Current audit boundary**
above; `--require-all-asserted` rejects executed-only and absent words.
The instruction contracts below retain their value/status/addressing evidence,
not a separate coverage baseline. Exact `5086` checks
stationary AR6 double-word ADD into A using absolute port markers independent
of AR6, distinct AR3 source data, preserved B/status/source/pointers,
continuation PC and one-cycle execution.
Exact `f3e6` checks B logical shift by six with guard clearing and outgoing
bit-26 carry; exact `f640` checks zero-shift B-to-A load with guard preservation.
Both preserve the unrelated accumulator/TC/ST1, with the zero-shift load
preserving carry, and execute in one cycle.
Exact `f78f` checks B-to-B normalization left by 19/right by seven from TI
example values, signed six-bit T interpretation, preserved A/T/carry/status
and one-cycle execution.
Exact `f677`, `f762`, `f767`, `f77c` and `f77e` check B-source shifts by
-9, +2, +7, -4 and -2, including cross-accumulator destination selection,
SXM clear/set right shifts, shifted-out carry, preserved TC/unrelated
accumulator/ST1 and one-cycle execution.
Exact `f462`, `f47c`, `f47e` and `f57a` check A-source shifts by +2, -4,
-2 and -6, including cross-accumulator destination selection, SXM clear/set
right shifts, shifted-out carry, preserved TC/unrelated accumulator/ST1 and
one-cycle execution.
Exact `808c` and `80d2` check low A stores before AR4 decrement/AR2 circular
wrap, preserved neighbors/accumulators/status, unchanged AR3, continuation PC
and one-cycle execution.
Exact `82d2` and `83d2` check A/B high-word stores at the old AR2 address
before circular wrap, preserved neighbors/accumulators/status, unchanged AR3
and one-cycle execution between absolute port markers.
Exact `7184` checks stationary AR4-to-immediate-data-address transfer,
preserved source/neighbors/A/B/status/pointer, extension-complete continuation
PC and two-cycle execution.
Exact `6b84` and `6b8c` check stationary/decrementing AR4 immediate memory
adds using the TI negative-overflow example: saturated stored result, bit-32
carry plus overflow, preserved A/B/neighbors/ST1, write before pointer update,
extension-complete continuation PC and two-cycle execution.
Exact `6885` and `6985` check AR5 immediate AND/OR memory updates against a
distinct AR3 word, preserved neighbors/A/B/status/pointers, extension-complete
continuation PC and two-cycle execution.
Exact `5685` and `56d3` check stationary AR5 and circular AR3 double-word
loads into A with SXM clear/set, preserved B/status, long-pointer wrap and
continuation PC. One-cycle DLD gives a four-cycle delta with the absolute
port-write markers used by these fixtures.
Exact `5385` checks double-word ADD into B through stationary AR5 with C16
clear/set, distinct AR3 data, preserved A/status/pointers/source memory and
one-cycle DARAM arithmetic; C16 prevents carry crossing between the halves.
Exact `5082` and `5085` check double-word ADD from stationary AR2/AR5 into A,
using distinct AR3 data to verify source selection, preserved B/status/source
memory and pointers, and one-cycle DARAM arithmetic between port markers.
Exact `4e82`, `4e95` and `4f8d` check stationary A stores through AR2,
incrementing A stores through AR5 and decrementing B stores through AR5.
The checks cover high/low placement (including odd addresses), long-pointer
updates by two, preserved neighbors/A/B/status, continuation PC and two-cycle
execution.
Exact `4488` checks shifted loads into A with SXM clear/set, decrement after
reading AR0, preserved B/status/source, unchanged AR2/AR3 and one-cycle
execution.
Exact `308a` checks loading T from the old AR2 address before decrement,
with a distinct AR3 source, preserved A/B/carry/source memory and one-cycle
execution.
Exact `2882` checks stationary AR2 MAC into A with signed factors and FRCT
clear/set; exact `2d92` checks MAS into B and increment after reading AR2.
Both preserve the unrelated accumulator, T, carry and ST1, leave AR3 unchanged,
and execute in one cycle.
Exact `2082` and `218a` check signed T-times-memory multiplication into A/B,
preserved unrelated accumulator/T/carry, stationary AR2 versus decrement
after reading its old address, unchanged AR3 and one-cycle execution.
Exact `01f8` checks absolute ADD into B with both SXM settings, preserved
A/TC/source, carry clear, continuation PC and two-cycle execution. Exact
`1080` checks stationary AR0-to-A loading against a distinct AR1 source,
both SXM settings, unchanged pointers/B/T/status and one-cycle execution.
Exact `f930` checks TC-clear/set conditional calls, unchanged stack sentinel
when rejected, extension-complete return PC pushed when taken, preserved
A/B/status, and three/five-cycle call costs. Exact `ff44` checks ANEQ for
zero and guard-bit-only nonzero A, preserved A/B/status, both guarded MAR
slots, continuation PC and equal accepted/rejected cycle cost.
Exact `f684`/`f784` check B-source negation into A/B for negative and zero
inputs plus signed 32-bit overflow with OVM clear/set: preserved unrelated
accumulator/TC, zero-only carry, destination overflow flags, continuation PC
and one-cycle execution. Exact `e5e9` checks copying old AR4 before AR0-step
circular wrap, independent AR3 increment, preserved source/accumulators/
status and one-cycle execution.
Exact `f477`/`f67d`/`f77a` check arithmetic shifts by -9/-3/-6 with both
SXM settings, signed/logical guard-bit fill, outgoing-bit carry, preserved
TC/source accumulator, and one-cycle execution. Exact `f561`/`f766` check
A-to-B left shift by one and B-to-B left shift by six, preserved unrelated
accumulator/TC, cleared outgoing-bit carry and one-cycle execution.
Exact `8180` checks B-low storage through stationary AR0, unchanged
accumulators/status/pointers/adjacent memory and one-cycle execution.
Exact `7708` checks immediate replacement of AL while retaining A's high
and guard bits, unchanged B/status, continuation PC and two-cycle execution.
Exact `8292` checks A-high storage at old AR2 before increment, preserved
accumulators/status/adjacent memory and one-cycle execution.
Exact `7483` checks stationary AR3 port-input selection, untouched adjacent
cells/pointers/accumulators/status, continuation PC and two-cycle transfers.
Exact `758a` checks old-AR2 output before post-decrement, preserved source
and unrelated pointers, and two-cycle execution. Exact `3c82` checks signed
shifted ADD from stationary AR2 against a distinct AR3 word, preserved B/T,
retained set carry and one-cycle execution.
Exact `ff4f` checks two-slot BLEQ with negative, zero and positive 40-bit B
values (including guard-bit-only sign differences), preserved A/B/status,
continuation PC and equal accepted/rejected cycle cost. Exact `4582` checks
stationary shifted AR2-to-B loads with both SXM settings, unchanged pointer,
preserved A/status/source and one-cycle execution.
Exact `7088` checks copying the source word to old AR0 before decrement,
preserved source/unrelated pointers and two-cycle extension-consuming
execution. Exact `720b` checks replacing B's low word while retaining its
high and guard bits, preserved A/status/AR3/source, continuation PC and
two-cycle memory-to-MMR transfer.
Exact `4082` checks stationary AR2 source selection against a distinct AR3
word, shifted subtraction into A, retained set carry on no borrow, preserved
B/T/source and one-cycle execution. Exact `4f90` checks B high/low word
ordering through AR0, two-word post-increment, unchanged adjacent cells,
unrelated pointers/accumulators/status, continuation PC and two-cycle store.
Exact `4911` checks AR1-to-B selection against a distinct AR2 value,
zero-extension with SXM set, preserved A/status/source registers and
one-cycle execution. Exact `f689` checks rounded MACAR with signed old-A
high word as multiplicand and old B as accumulation source; FRCT clear/set
straddle the rounding boundary, while B/T/status remain unchanged.
Exact `7690` checks immediate-word consumption, old-AR0 destination and
single post-increment, untouched neighboring cells/AR3/arithmetic state,
continuation PC and two-cycle execution. Exact `f500`/`f600` check opposite
accumulator ADD at positive signed overflow, destination-specific overflow
flags, cleared carry with TC retained, preserved source/T, OVM clear/set
results and one-cycle execution.
The short-immediate load fixture asserts all 512 `e800..e9ff` encodings
with SXM clear and set: unsigned byte values, independent A/B destination,
preserved other accumulator/status, continuation PC and one-cycle loads.
This closes the formerly executed-only `e809`; it does not establish
long-immediate or memory-load behavior, which have separate fixtures.
Exact `f5bc`/`f4bc` check setting/clearing ST0.TC while preserving carry
and ST1. Exact `f6b8`/`f7b8` check clearing/setting ST1.SXM while preserving
OVM and ST0. All four check one-cycle execution between port markers.
Exact `7595`/`7592` check independent AR5/AR2 port-write selection using
distinct source values, pre-increment output, one pointer increment,
preserved source/unrelated pointers and two-cycle execution.
Exact `fd20` checks both TC outcomes for single-slot conditional execution,
unchanged status/B, continuation PC and equal accepted/rejected cycle cost.
Exact `6e82` checks zero/nonzero AR2 without modification, both executed
delay words, distinct fallthrough/taken continuations, preserved B/AR7 and
the two-cycle delayed branch cost.
Exact `8090`/`8190`/`8191` check low-word A/B selection with distinct
accumulator values, AR0/AR1 destination selection and post-increment,
preserved accumulators and unrelated pointers, and one-cycle stores.
Exact `0881` now checks SUB through AR1 with both SXM settings, 32-bit
no-borrow carry, unchanged B/source/pointers and one-cycle arithmetic.
Exact `e4e9` checks loading T while storing old A, independent Y increment,
circular X wrap, unchanged accumulators/status and one-cycle execution.
Exact `d6e9` checks the signed rounded MAC into A, old-B store, unchanged
B/T/source and independent circular-X and incrementing-Y updates.
Exact `4392` checks shifted SUB B-to-B, preserved A/T/source, AR2
post-increment, retained cleared carry on no borrow, and one-cycle execution.
Exact `e245`/`e249` check SQDST's signed X-Y difference and old-A-high square
accumulation into B, preserved T/source cells, and independent decrementing X
with decrementing/incrementing Y. The two fixtures separately exercise FRCT
clear/set, including the doubled square in fractional mode.
Exact `6183` checks both BITF TC outcomes, preservation of other ST0 bits,
accumulators, pointer and source memory, and two-cycle execution with its
mask extension word. Exact `3c92`/`3f92` check shifted ADD routing into A/B
and AR2 post-increment; `3f82` checks the non-modifying AR2 form. All three
check T, the unaffected accumulator, retained carry and one-cycle arithmetic.
The immediate-repeat fixture asserts every `ec00..ecff` encoding: `K+1`
iterations, exact AR1 advancement, continuation PC, one setup cycle and one
cycle per repeated MAR instruction. This includes the newly observed `ec0d`
and the previously executed-only `ec02`. It does not establish all repeatable
instruction classes or interrupt behavior; those have separate fixtures.
Exact `4592` checks signed/unsigned shifted loads into B, unchanged A/status,
AR2 post-increment, preserved source and one-cycle execution. Exact
`82d3`/`83d3` check A/B high-word stores before four-word circular wrap,
unchanged accumulators/status and the established store cycle cost.
Exact `4a60`/`4a61` and `8a61`/`8a60` check seven-bit data-address ownership,
descending stack order, balanced restore, and preserved A. The fixture uses
an explicit register read/write mode at its otherwise interrupting-read
peripheral addresses `60/61`; these tests do not assign Nokia silicon meaning
to those addresses. Exact `4fd2` checks high/low word order, a two-word circular
AR2 advance, unchanged AR3/accumulators, and untouched adjacent memory cells.
Exact `a249`/`a201` check signed dual-memory SUB into A, 32-bit carry,
unchanged B/T/source and decrementing-X/incrementing-Y versus stationary
pointers, with one-cycle execution. Exact `e723` distinguishes AR2-to-AR3
from the adjacent AR5 source and checks source preservation and one-cycle
execution. Exact `7083` checks dmad-to-AR3-indirect movement, preserved source
and pointers, extension consumption and two-cycle execution.
Exact `74d4` checks one port read, extension consumption, destination-before-
circular-wrap ordering and unchanged AR6. Exact `0092`/`0192` check both SXM
settings, A/B destination routing, the `1 + ffff` carry boundary, preserved
source, single AR2 post-increment and one-cycle arithmetic. Exact `438a`
checks shifted SUB's read-before-decrement ordering and partial-carry rule.
Exact `b43e` checks signed rounded MAC into A, preserved B/source, X publication
in T, and independent stationary-X/circular-Y pointers.
Exact `e6e9` complements `e4e9` with distinct A/B contents, proving old-B
store selection while loading T, preserving both accumulators/status and
independently advancing/wrapping Y/X. Exact `7092`/`7090` extend MVKD to
post-incrementing AR2/AR0 with unchanged source and two-cycle execution.
Exact `1e84` checks both SUBC quotient/carry outcomes through AR4, preserved
B/source/pointer and one-cycle arithmetic.
Exact `7583` checks AR3-selected port output against a distinct AR2 source,
unchanged source/pointers, port-extension consumption and two-cycle execution.
Exact `8192` checks low-B store-before-increment and preserved B in one cycle.
Exact `6d95` checks one AR5 increment, unchanged AR6/memory and one cycle.
Exact `7e92` distinguishes A-addressed program memory from different data
memory at the same address, checks AR2 post-increment, unchanged A/AR3 and
five-cycle execution.
The tap is in `execute_run` immediately before `execute_one`, not in the
extension-word fetch helper: these are instructions dispatched by the current
emulated core, not raw program-memory reads or independent silicon evidence.
`first_pc` is only the first observed site; each count aggregates all sites.
The coverage gate also runs seeded 5110 Menu and long power-key traces. It
checks the Menu press/release and the power lifecycle, then unions all three
opcode sets before checking fixture assertions. Neither additional trace
currently adds an opcode word; these are measured negative results, not
evidence that other interactive or radio paths cannot execute more DSP
instructions. The fixed idle fingerprint still applies only to the untouched
primary run.
The fixture asserts the current observed set plus fixture-only words. This
closes exact-word assertion coverage for that captured run, not every operand,
status or hardware interaction those instructions can encounter.
The generated report gives the current fixture-only total; these are *word*
counts, not instruction-family counts. Fixture execution alone is not
proof that a particular result is asserted, and this one boot is not a census
of every possible ROM4 path.
The coverage reader rejects malformed execution, assertion, decoder and
extension records rather than silently dropping them from the measured set;
unrelated emulator output remains permitted.
`make check-c54x-observed-coverage` regenerates the fixture and handset logs and fails if
the ROM4 idle word set changes or any observed word lacks an assertion. It is
also part of `make check-c54x-cross-rom`; the gate does not validate unobserved
instruction variants or silicon-level timing. The tool is given the current
core source for a separate static decoder inventory.
All four MAME invocations disable host joystick and MIDI providers: these
headless scenarios use scripted emulated keypad inputs, not physical host
devices. This avoids irrelevant host-device initialization without changing
handset inputs or CPU/peripheral semantics.
Its core-fixture leg uses the standalone runner's five-emulated-second stop
bound and `check_result` contract: every required conformance marker, the
intentional completion exit, and the terminal completion message are mandatory.
A coverage capture keeps verbose mode to emit opcode records; only its exact
two-line SDL teardown suffix is permitted after completion.
A timed normal exit is not acceptance. Failed coverage runs retain their
isolated directory and print its location; successful runs remove scratch
artifacts. The stop bound is a harness budget, not a DSP timing claim.
`make check-c54x-cross-rom` combines
native core conformance, ROM4 RF-boundary and observed coverage checks,
3210 coherent frontier, native 5110 physical Phone book menu, and the
3310/3330/3410 product frontiers. The sibling frontier checks retain their
configured backend distinctions; this aggregate pass is regression evidence,
not native DSP, RF acquisition or speech validation for every handset.
That inventory matches top-level cases and opcode masks and reports the
current number of matching words that do and do not execute in the fixture.
The generated report identifies matching high-byte groups without a fixture word;
even covered groups do not establish their remaining words. These are **candidates**,
not a verified implemented-instruction count: nested validity, extension-word
grammar, and behavior are not established by a source mask. The report keeps
the ROM4-observed/fixture-asserted class separate, so a new observed gap can
be ranked by execution count before expanding tests into unused encodings.

## Resolved flat-image dependency

Before the decoder correction, sampled words at
`0x2000`, `0x216a`, `0x2286`, `0x23c3`, `0x2470`, `0x25b4`, and `0x27ff` are all
zero. Subsequent demand loads populate catalogue-owned ranges, but the gap
`0x23c3..0x25b3` is outside every declared destination. Without the assist the
co-sim completes 70 rather than 74 DSP acknowledgements and does not reach a
stable UI.

The gap's bytes occur in MCU flash, but location alone is not a destination
proof. In particular, the bytes matching `0x23c3..0x25b3` immediately follow
descriptor 0, whose declared output destination is `0xfd00`. They may be
encoded input, shared source material, or an artefact of how the recovered flat
image combined address spaces.

The producer is loader1 at `0x0f1f..0x0f28`. It computes a count from the
`0x23c4..0x252a` bounds, selects source `0x0d00`, and executes repeated MVDP to
program destination `0x23c4`. With `PMST.OVLY=1`, those program stores must
resolve to the same DARAM cells used by program fetches.

The external experimental core originally wrote MVDP directly to an immutable
program store, bypassing its overlay helper. MAME's backend already routes
program writes through the DSPIF overlay, but initially repeated the encoded
program destination without the C54x's repeat-mode address update. Advancing
that destination per repetition populates the resident range organically. The
external unassisted run's 74 acknowledgements and stable UI remain the complete
ROM4 reference; MAME has now independently crossed the same loader boundary.
This is an instruction-semantics correction, not a reconstructed image or
timing adjustment.

## Delayed conditional return

The core implements `0xfe44` as `RCD ANEQ`: test the full accumulator A
before the two following delay words, pop the return address only when A
is nonzero, and charge three cycles on either path. This follows
[TI SPRU172C, RC[D], pages 4-133/4-134](https://www.ti.com/lit/ug/spru172c/spru172c.pdf).
The executable core fixture tests both paths, including a delay-word write
that changes A after the decision, stack preservation on a false condition,
and port-to-port cycle counts. Separately tested `0xfe4c` (`RCD BNEQ`) uses
the same documented three-cycle contract for accumulator B. Both paths test B
before delay words overwrite it and preserve or pop the stack as appropriate.
This is manual-derived core conformance, not an observed handset requirement
or interrupt-pipeline timing evidence. Other conditional delayed-return
encodings are not implied by these two words.

ROM4 uses this instruction at `0x90eb` when additional demand-load
continuations are delivered through BSCR.HINT. The bounded NSE-5 investigation
documents that signal, accepted counter/compare model and research-only overlay
in [7110 bring-up](7110_bringup.md#diagnostic-publication-and-partial-code-block-transfer).
