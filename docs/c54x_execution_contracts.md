# C54x execution contracts

## Authority and scope

The implementation lives in `cpu/tms320c54x/`. Current generic acceptance
uses `tests/c54x/sections/` and `tools/run_c54x_section.py`.
`fixtures/c54x_sections.json` owns section registration and expected counts;
the [test-suite contract](c54x_test_suite.md) owns coverage policy, execution
scopes and validation limits.

Execution is stage-owned only. Historical atomic comparisons below do not name
a selectable engine; [frontend retirement](c54x_frontend_retirement.md) owns
the migration and its independent firmware acceptance.

Copied/acquired firmware integrations live in `tests/firmware/`.
`tools/run_c54x_core.py` selects only `rom4-context` and `cold`;
codec and handset runners retain separate acceptance contracts.
The retired generic BIOS profiles and phase-chain checkpoints are not current
execution interfaces or renewed acceptance evidence. Complete replacement-suite,
firmware and handset validation must be established independently.

Passing CPU tests does not establish physical pipeline timing, a fitted handset
DSP mask, RF acquisition or end-to-end speech. Supporting application maps are
[ROM4 execution](rom4_dsp_execution.md),
[original-code arithmetic](c54x_arithmetic_observations.md) and
[MU4 CPU composition](c54x_mu4_observations.md).

The external observation-only ROM4 opcode inventory contains 628 distinct
words in 112 high-byte groups after 30 million MCU instructions; sorted-set
SHA-256 is `63ae45c872ccea39112898bfde7f5926acd675701675a860d765a66a35815d34`.
`tools/c54x_opcode_coverage.py` retains this reachability reference, not a
correctness or current CPU-coverage claim. Imported interrupt masks can change
reachability without establishing a legitimate hardware reset state; the loader
document owns that provenance contract.

### Circular memory logic

AND/OR/XOR Smem now admit the existing DAGEN circular modesC0..DF for
ordinary RAM, including the pipelined single-repeat path. Address calculation,
postmodify and the retained Read operand use the same stage-owned path as
the previously admitted forms; no handset address or result is special-cased.
Circular mapped/peripheral sources below60 still reject before a read or
pointer update. Indexed/reverse-carry modes98..BF are not admitted by this
change.

`circular_memory_logic` fails on the old decoder and passes84 cases/replays:
72 positives cover all three operations,A/B,four circular updates and
single/RPT/RPTB execution with sizes1/7/16,wraps,ordered RAM reads,pointer
and status preservation and a following register write. Twelve negatives
retain the mapped-source boundary. Existing memory_logic252,long_memory_logic9408
and repeated circular arithmetic controls remain separate regressions.
The tests do not establish physical issue timing or untested peripheral
semantics.

### Repeated circular stores

Repeated STL/STH accumulator stores admit circular indirect modesC0..DF
through the existing stage-owned DAGEN/Write path. The decoder previously
rejected those repeat bodies while accepting ordinary indirect80..9F stores;
the native ROM6 traffic initializer reaches `RPT / STL A,*AR4+%` (80d4).
No handset PC or RAM address is special-cased. The mapped-destination guard
still runs before DAGEN/bus effects on every iteration.
`repeated_circular_store` reproduces the old rejection and passes32 cases
with save/replay: A/B low/high stores,four circular updates,count0/6,
ordered per-clock writes,wraparound and final pointer/RC/source preservation.
The existing1024-case `repeated_store` control also passes. These are the
declared staged timing contracts,not new silicon cycle measurements.

### MVDM block counter

MVDM dma,BRC (721a) admits an ordinary RAM source through the existing
two-word mapped-move path. The source is retained in Read and BRC publishes
in Execute;the following RPTB consumes the new count. The native ROM6
traffic initializer reaches721a/2af1 atb79e,not a new ROM hole. Active block
loops,single-repeat/control-delay/XC composition and control/peripheral
sources remain rejected before source-read or BRC-write side effects.
`mvdm_brc` reproduces the old decoder rejection and passes11 save/replay
cases: eight copied-block/count checks over two program pages and counts
0/1/7/31,plus three rejection boundaries. No firmware-specific exception is
part of the CPU implementation.

### Long circular MAR

MAR modesf0..f7 implement signed long preoffset with circular update through
the existing stage-owned address operation;linear e8..ef behavior is unchanged.
[SPRU131G table5-4](https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/81/6114.spru131g.pdf)
defines MOD14 as `ARx = circ(ARx + lk)`. The64-case `long_circular_mar`
section reproduces the former decoder rejection and checks signed/zero offsets,
wraps,BK1/13/16,all auxiliary registers,CMPT/ARP selection,two program pages,
Access publication,absence of data-bus accesses and save/replay. It does not
resolve the pre-existing BK0 convention or qualify physical issue timing.

### Repeated circular squares

Repeated square add/subtract (38xx..3bxx) admits circular indirect sources
C0..DF through the existing per-issue DAGEN/Read and live accumulator Execute
paths. `repeated_circular_square` reproduces the old repeat-admission rejection
and passes33 save/replay cases: A/B add/subtract,four circular updates,FRCT
off/on,signed input,wraps,ordered reads,T/RC/pointer/final-result checks,and
mapped-source rejection before bus/pointer effects. The arithmetic itself is
unchanged;the ordinary repeated-square section owns its flag/limit cases.

### Host execution cost

The staged frontend shares immutable opcode-property and compact descriptor
tables (320 KiB per process). The byte descriptor contains extension count,
retirement span, base/extended opcode admission, Access-stage SP sampling and
the opcode-only part of generic Read-operand selection. The shifted-memory
extension check remains live; overlapping stage actions retain their ordering.
They are keyed by the fetched word, never its
program address: self-modifying code and retained prefetch still follow the
normal pipeline. Extension-word contents, CPU state and profile-dependent bus
register recognition remain live decisions. These tables are not emulated
state and need no save-state registration or invalidation.

The ASM-store classifier uses a constant mode mask: each bit represents eight
Smem encodings, preserving the ranges 00..97, a8..b7 and c0..df. It adds no
table storage and does not change addressing, supported forms or stage timing.

Fixed-index stage advancement preserves the original field arrays, slot order
and invalid-slot contents. Interrupt hazard scanning can return early only
when the existing acceptance predicate already rejects the source; peripheral
classification checks addresses before opcode families. Mapped-register and
RAM-forwarding checks reject nonmatching addresses before classifying producers.
The younger-XC scan runs only when active protection could end at the retiring
instruction; its decision remains after Execute publication. None changes clocks,
interrupt acceptance points, bus effects or supported instruction forms.

Use the [codec benchmark](../tests/README.md#codec-performance) to measure host
cost separately from emulated timing. Optimizing a host path does not strengthen
the model's silicon-timing claims.

### RND and circular I/O

Standalone RND (`f49f` with source/destination bits) is admitted through the
shared register-only Execute implementation. The `rounding` and
the shift table in `register_literals` retain independent numeric/OVM/flag expectations
on staged. RND is a one-word class-1 operation; this does not qualify repeated
RND (nonrepeatable in [SPRU172C, 4-142](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)).
PORTW now admits the short circular modes `c0..df` already handled by the shared
DAGEN path, matching PORTR's admitted mode range. Existing PORTW Read sampling
and Execute I/O publication are unchanged. Circular marker, pointer, source,
transfer and relative-clock assertions pass without fitted timing changes.
Focused admission/I/O/forwarding validation passes 42,233 cases plus restores;
both EFR frontends remain bit-exact and the ordinary 5110 menu/save-state gates
pass. This is selected regression coverage, not a complete CPU-suite renewal.

### Harness contract

Absolute `CMPM *(001a), #lk` admits BRC as a mapped source. `brc_compare`
has eight independent cases for equal/unequal words, retained Read operands,
TC-only Execute publication and restore points around Read. The 6,814-case
`mapped_compare` section retains the other control/peripheral exclusions,
including direct and indirect BRC forms. This is a bounded admission, not
general MMR comparison coverage (SPRU172C 4-33).

`mvdd_sp` qualifies nonrepeated `e51a` (`MVDD *AR3, *AR4+`) with a RAM source
and SP destination. Eight positive cases check retained source/address,
pointer update, SP publication and restoration; four negative cases retain
neighbouring destination, repeated and different-form exclusions. Status and
accumulators remain unchanged (SPRU172C 4-107). The asserted bus clocks are
the model's phase contract, not independent silicon timing measurements.

`mvdd_accumulator_destination` separately qualifies non-single-repeated
RAM-to-AL/AH/AG/BL/BH/BG copies. Its 148 positive cases/replays check retained
source after RAM replacement, both pointer directions, sibling-field/status
preservation, guard masking, immediate same/sibling mapped ADD reads, MVDK
readback and four block-repeat continuations. Four negative cases/replays
reject single-repeat, mapped-source, status and T-destination compositions
before data effects. Source Read remains at 5 and real mapped Write at 6;
the immediate mapped reader can use the retained word without moving that
write. This does not relax direct-ALU producer latency or qualify general
MMR copies, interrupt composition or measured silicon timing.

`mvpd_accumulator_destination` qualifies nonrepeated program-to-AL/AH/AG/
BL/BH/BG copies with short-indirect and absolute destinations. Its 48
positive cases/replays check both program pages, retained source/address
after replacement, sibling/status preservation, guard masking and an
immediate mapped ADD reader. The real mapped Write follows the retained
program Read; forwarding does not move either bus event. Two negative
cases retain the status-destination and single-repeat exclusions. The
50-case section and existing 832 program-transfer/48 repeated-program
controls pass. These clocks specify model ordering, not measured silicon
timing; general peripheral destinations remain unqualified.

`block_stlm_ram` distinguishes direct STLM destinations `60..7f` (ordinary
low RAM, independent of DP/CPL) from control-register mutations in a block
repeat. Its 36 positive cases/replays check three destinations, A/B, two
program pages, counts 0/1/7, exact ordered writes, live accumulator feedback
and continuation. Eight negative cases/replays preserve the BRC/PMST/
peripheral and indirect-form exclusions before data effects. No register
control, interrupt or physical timing behavior is newly qualified.

`constant_store_pointer_copy` checks an immediate ST with postdecrement
followed by MVDD using that pointer as its destination. Resolve the older
ST's Read-stage address/update before the younger memory Access, as for
other retained store addresses; do not move the actual data write. Sixteen
cases/replays cover decrement and the existing increment control, four
source words, two program pages, pointer/accumulator/status preservation,
and exact STM/Read/ST/Copy bus clocks 5/9/9/10. The pre-correction decrement
case copies to the old pointer and overwrites the constant. This is a generic
DAGEN dependency, not a handset callback or a physical timing measurement.

READA resolves its Read-stage destination DAGEN update before a younger
Access-stage instruction consumes that AR (SPRU131G 7.1). Its retained PAR
sample, program-bus hold and Execute publication are unchanged.
`program_table_copy` uses four independent pair-table loops (one, two, four
and eight pairs) to check READA/ADD/MVDD composition, final table position,
scratch-pointer balance, copied words and restoration. This catches a loop
that passes standalone instruction tests but reads the scratch word on the
wrong side of an older postdecrement.

`READA` into AL/AH/AG/BL/BH/BG is also a mapped accumulator writer.
An immediately following mapped read receives its retained program word,
including the eight-bit guard-field mask, without advancing the real EB
write or rereading program memory. This follows the mapped-write/read
case in [SPRU131G 7.5.10, example 7-67](https://www.ti.com/lit/ug/spru131g/spru131g.pdf),
not the separate direct-ALU producer latency. `mapped_program_read` has
72 independent cases across all six fields, two word patterns, A/B LDM
consumers and zero/one/two separating instructions, with restore replay.
The immediate case failed before the bypass, returning the old table address.
The isolated `brc_to_ar` positive coverage also supersedes the old BRC-source
rejection in `mapped_operand_limits`; the remaining 44 admission limits stay.

Short RAM AND/OR/XOR operands can issue through the staged single-instruction
repeat slots. Each iteration owns one operand read and one pointer update;
Execute reduces the retained zero-extended word into the selected accumulator
without changing status flags (SPRU172C 4-11, 4-123, 4-201). `memory_logic` adds
48 short/long-count, A/B and count-zero/nonzero cases with source poisoning and
replay, plus twelve mapped/peripheral-source rejection cases. Those unqualified
sources fail before a read or pointer modification. This does not qualify
long-offset logical repeats or physical bus timing.

An IDLE Decode guard belongs only to that instruction. An older backward branch
can retire while its target IDLE decodes; numeric address proximity does not make
the branch part of the younger guard. Its retirement must neither underflow nor
consume that one-word guard. `idle_wake` checks two IRQ/RETE cycles through an
IDLE/backward-branch loop, including restoration during either interrupt entry.
Delayed C/NC branches are admitted within a block repeat when both delay words
and the target remain inside the block, with polarity/carry/page contrasts in
`block_branch` and original EFR packing/unpacking coverage.

Each section owns explicit setup, independently justified expectations and
replay. Shared helpers load memory, observe buses and serialize state; they do
not call CPU implementation helpers to derive expected results.
Registration/count checks cannot establish behavior.

The generic runner has no legacy phase router. A case that starts after IDLE
must explicitly establish runnable state; changing PC alone does not clear
the IDLE latch. Bus observations and their replay cursors must remain consistent
across checkpoints, including captured reads and pending writes.

The `disassembly` section runs 44 syntax/length/page/stepping result families.
Its single IDLE case validates runner lifecycle, not instruction execution
coverage for those disassembler families.

Firmware-local timed replay cancels the observer before loading the stream,
rejects load failure, suspends the CPU, publishes its continuation and enables
the observer. Callers explicitly rewind host observations. The executable
firmware helper test checks scheduling/failure ordering, not CPU snapshot
correctness. Removed counted-checkpoint and atomic-restart helpers are not
current firmware interfaces.

The clocked case runner can also own a complete ordered data-bus trace
(direction, address, value and CPU pipeline cycle). Its host event cursor is
captured and rewound with the snapshot; replay must neither repeat a retained
read nor omit a pending one. Register-only cases retain their no-access policy.
`arith-ar` and `arith-bk` are the memory pilot: each retains all 1,024 original
keys, reference arithmetic, before/after-read checkpoints and source-register
mutation. Their former per-family taps, counters and replay phases are removed.
The instruction now ends in real IDLE instead of unobserved NOP padding;
the final arithmetic/status/source checks still occur at clock 16. This does
not yet relocate either profile out of the legacy machine or qualify other
memory families. Bus cycles are implementation observations, not silicon timing.

`address_mode_publication` owns eight RSBX/SSBX CPL/DP and forty full-status
producer/field cases. `immediate_dp_forwarding` owns all1,024 immediate-DP
cases. Explicit producer/set/store/gap and page/chain loops replace the old
ordinal decoder; no retained input is dropped. Legacy header/profile/result
plumbing is removed.
The original NOP streams, clock-11/10 observations and snapshots remain;
these bounded streams do not require IDLE. Optional pipeline-clock checking
preserves their per-clock progress check without changing other runner users.
The native sections own their setup and expectations, not legacy case keys.

SPRU131G tables 7-16/7-18/7-19 supply the required consumer spacing: two NOPs
after MVMD/STM and three after STLM/RSBX/SSBX for these Category-I forms.
Eighty shorter-gap cases are retired because the manual does not specify their
assumed old-mode outcomes. All retained inputs start with ARP=0, as required
in standard mode. The twenty CMPT cases use explicit `*AR3+`: both modes use
AR3, but only compatibility mode publishes ARP=3 (section 5.5). This replaces
the invalid nonzero-ARP standard-mode witness rather than preserving its result.
Immediate-DP cases keep their overflow sentinel without setting ARP; table
7-17 permits their adjacent DP consumer. Exact clock-10/11 observations remain
software-model/replay checks, not measured silicon phase guarantees.

`frame_stack_consumers` owns all 256 signed FRAME/adjacent-push offsets
and six safely separated PSHM/POPM compiler-mode consumers. FRAME checkpoints
3/4/6 are distributed instead of multiplying to 774 executions. All262
cases replay independently; the legacy factory and profile/result plumbing
are removed. A default-only run does not renew this section.
TI SPRU131G section 5.7 and Table 7-9, plus SPRU172C instruction descriptions,
provide the stack/result contract. Eight zero-gap cases are retired because
their old-SP outcomes are unspecified; two PSHM-SP alias cases are also retired
because their assumed source sampling lacks an unambiguous independent basis.
The retained FRAME trace checks one AR1 read and one stack write, with replay
cursor restoration. The small SP bank retains its final register/RAM checks.
Exact observation clocks remain model regressions, not physical timing claims.

Copied MU4 context/worker programs are Nokia firmware integration, not ISA
conformance. The `rom4-context` profile owns 358 cases in eight banks under
`tests/firmware/tms320c54x_test_rom4_context.cpp`; those banks no longer execute
again in the default suite. It needs no external ROM file. Corrected inputs
keep ARP zero and CMPT clear at every observation; all low status sentinels,
INTM/SXM cases, instruction words, checkpoints and result predicates remain.
The standalone baseline, corrected and relocated executions produce identical
native logs. Exact local clocks remain model regressions, not silicon evidence.

The outer skip-worker/skip-body paths, poisoned-source call/return roundtrip,
active body, timer/recurring-ISR, copied idle wrapper and poisoned SP-return
banks remain distinct. The two overlapping
smaller probes and their 58 case/restore pairs are retired with explicit loss
of separate register patterns and local observations. All callers of the
timer image setup now live in the firmware module; generic CPU tests no longer
borrow it. Shared legacy controller storage and other copied snippets remain
to be reviewed. This relocation is
ownership separation, not a net source-size reduction.

`staged_idle` owns 28 independent cases covering all distinct IDLE1/2/3 wake
combinations on both program pages. It replaces two overlapping legacy banks
and distributes decode/asleep/pending/resumed checkpoints rather than crossing
every checkpoint with every input. Unrelated masked requests snapshot pending.
The sleeping pipeline freezes at its IDLE barrier without retiring younger
words; the timer continues only in IDLE1. A speculative continuation is changed
while asleep and must be fetched fresh after wake. IRQ/NMI service pushes the
actual low continuation and page, executes an ISR and returns through the real
far-return path; INTM-blocked wake instead resumes foreground without a frame.
Exact bus events, PC page, A/B, SP, pending IFR and standard-mode status survive
replay. Physical frequencies and silicon restart latency remain unestablished.

`idle_nmi` owns twelve independent staged-frontend cases: IDLE1/2/3 crossed
with INTM and empty/full IMR. Each reaches idle before an NMI is asserted.
A fixture execution hold lets the synchronizer latch NMI while vector entry
is frozen; the snapshot retains that pending request, original INTM/SP,
unrelated IFR bits and untouched stack RAM. Releasing the hold reaches vector1
with one continuation write and no maskable-IFR consumption. The external NMI
level remains high across acknowledgement; reasserting the same high level
must not create another interrupt. Clearing it on acknowledgement makes this
negative control fail. MAME HALT is fixture control here, not an asserted
C54x pin assignment. Pending IFR is checked before the ISR explicitly
acknowledges it and enters terminal IDLE. Silicon cycle accuracy is not claimed.

`idle_wake` owns six staged-frontend IDLE1 cases: external requests with
IMR/INTM combinations, plus timer requests with the timer mask disabled or
enabled. IMR gates wake independently of INTM. Enabled/unmasked interrupts
enter the vector and stack the old continuation; enabled requests under
INTM instead resume foreground, write its marker and stop the timer. Masked
requests remain pending without executing foreground. Exact IFR, PC, SP,
marker/stack RAM and ordered writes are checked after a pre-wake checkpoint
and replay. Timer start is explicit MMIO/state conformance stimulus; no
physical clock-rate or exact wake-latency claim is made. The continuation checks
its pending flag before acknowledging it through STLM IFR, then enters terminal
IDLE. Deep-clock, BSP and DMA continuations instead mask further wakeups through
STLM IMR after verifying the original mask; their pending IFR checks remain.

`deep_idle_clocks` owns eight independent staged-frontend IDLE2/3 cases.
The CPU peripheral-clock output drives a real McBSP device. Internal timer
count/prescaler and the queued serial word remain frozen without setting TSS;
an enabled external IRQ resumes both, while a masked one does not. Separately,
external frame/clock pins complete the preserved word during deep idle and
raise the serial IFR bit; only an enabled serial mask resumes foreground.
Every case checkpoints before the clock stimulus and checks continuation PC,
marker write, pending IFR and exactly one eligible serial word through replay.
Disconnecting the peripheral-clock output fails on premature transmission.
Register inspection is nondestructive observation, not guest bus traffic.
These are clock-domain and replay contracts, not physical rate measurements.

`bsp_interrupt` owns nine staged completed-frame interrupt cases. The standard
BSP word-state implementation generates RX/TX ready transitions, then the test
attachment routes them to CPU inputs4/5. An execution hold allows the snapshot
to capture pending IFR before vector dispatch. RX wins simultaneous readiness;
IMR-masked requests stay pending, INTM-set eligible requests resume foreground,
and ordinary unmasked requests push one continuation and enter the appropriate
vector. PC, SP, status/masks, pending IFR and exact writes survive replay.
This verifies completed-word routing, not serial pin cadence or handset audio.

`idle_dma` owns ten independent staged idle/DMA contracts using the real DMA
device: IDLE1/2 system clocks, IDLE3 input clocks, absent input clocks, and
clock loss halfway through the first word, each with its completion mask off/on.
Four explicit source words must be read and copied in order; each destination
write is checked against the modeled four-clock cadence. Clockless channels
remain enabled and unwritten until the fixture restores their clock. The
partial-word checkpoint must preserve two remaining clocks, not restart a word.
All cases restore before the first transfer, complete exactly once, publish
IFR bit6 and resume foreground only when IMR allows it, independently of INTM.
The completion input is a fixture route to DMAC0/BRINT2. These are internal DMA
clock/phase and replay contracts; external waits, arbitration and handset PLL
latencies are not established by them.

`pll_transitions` isolates the CLKMD source/lock state machine with a fixed
13MHz fixture reference and execution held. The x8 cold transition is sampled
around 4086+7/16 reference clocks; x10 around 4086.35. Repeating a pending
request cannot extend its deadline. PLL fields stay protected until exit
actually completes; an opposite request cancels that exit without editing
protected fields. Warm re-entry skips the lock count, but a multiplier change
requires a new lock. A pending-lock checkpoint replays the complete sequence
over later source/multiplier changes. Sixteenth-clock observations resolve
the rational switch edge without asserting a CPU or peripheral clock-rate model.

`clock_rates` separately enables CLKMD rate following. An explicit 13MHz
reference selects 6.5MHz DIV and 130MHz x10 PLL rates; CPU, real DMA and real
McBSP must receive the same publication. A pending block and queued serial
word cross the switch without early completion; the timer retains its count
while advancing at the selected rate. A selected-rate snapshot is restored
after an explicit rate perturbation, preserving timer count and both consumers.
DMA running on 3.25MHz CLKIN must not retime across two system-rate changes:
three words exist one input tick before completion and four immediately after.
An explicit reset restores DIV/CLKMD0. A fixed fixture observation clock keeps
the timeline independent of CPU rate changes. The outer pending-lock checkpoint
replays the complete composition; these configured rates are not board evidence.

`idle3_pll` starts from an explicit prelocked reset profile, exits to DIV while
keeping PLL power, and executes a real IDLE3 instruction. An enabled external
request under INTM resumes the untouched continuation in DIV. Software PLL
re-entry must now pay the full lock count, rather than use the warm-lock shortcut;
only then may it publish x8. A checkpoint inside that pending relock restores
both the sleeping continuation and DIV clock before replaying the delay. This
does not claim automatic PLL wake, physical restart latency or handset timing.

The four `external_io_*`/`standard_io_*` sections own staged and atomic I/O
wait contracts under enhanced and original-family SWWSR ownership. They retain
every wait encoding, transfer direction, READY level and IRQ/NMI source; atomic
cases also retain all five addressing forms. Snapshot positions are distributed
across inputs, not multiplied by three. READY and elapsed-clock origin restore
with the pending transaction. Changing wait registers, source/pointer, I/O input
and instruction literals after capture cannot change retained address, source
or extension ownership. Exactly one port event occurs at its modeled deadline;
IRQ acknowledgement follows that event and the interrupt frame equals live RTN
within the body/terminal-IDLE continuation pair. Complete RAM/stack traces and
timer progress are checked through replay. A negative control omitting READY
restoration fails on premature I/O. This is CPU bus-controller conformance, not
validated board wait wiring or a physical mapping claim.

`bus_registers` and `standard_bus_registers` own register masking separately
from live transfer timing. Each uses a deduplicated37-pattern bit basis plus
dense/boundary controls, explicit masked writes and reset. Original SWWSR
masks bit15 and does not acquire29/2b: those addresses retain board RAM while
BSCR/SWCR stay zero. Enhanced ownership masks BSCR/SWCR, publishes CLKOUT/IACK
configuration without changing system rate, and restores unsaved external sinks
after deliberate corruption. Logical IRQ acknowledgement remains active with
physical IACK disabled. Every retained case checkpoints/replays; arbitrary
higher-order combinations from the former exhaustive raw sweeps are not retained.

`mmr_bus` isolates the CPU's shared register/MMIO view: AR3 full and masked
writes agree with debugger state; low/high IFR masks clear only selected flags;
the reserved address3 remains board RAM. A passive tap observes each write even
though the stimulus uses the MMIO API rather than guest instructions. Snapshot
replay begins before those writes and restores the observation counter; leaving
that counter unsaved fails on repeated observations. No inherited handset state
or physical external-bus claim belongs to this contract.

The independent `status_bitfield` section owns ST0/ST1 BITF sampling: 128
register/pattern/mask/initial-TC/addressing combinations, with pages and three
checkpoint positions distributed rather than multiplied into 768 executions.
It checks one mapped Read, no writes, retained extension words, post-Read source
mutation, TC publication and preservation of unrelated status/register state.
Persistent observations apply on every clock through real IDLE and replay.
These are model sampling contracts, not silicon timing measurements.
`mapped_accumulator_arithmetic` owns 1,152 ADD/SUB cases for AL/AH/AG,
BL/BH/BG/T/TRN, both destinations, SXM/OVM, four input patterns, direct/absolute
forms and adjacent/separated LD producers. Its reference uses signed 40-bit
destination values, independently widened word operands, 32-bit carry/overflow
and optional saturation. `mapped_accumulator_bitfield` owns 576 cases covering
the same word sources, four masks, both initial TC states, both address forms
and producer gaps. It distributes SXM/OVM because neither affects BITF.
Both sections retain exact one-Read/no-Write contracts, source poisoning,
extension-word retention and per-clock publication predicates through IDLE.
Pages and checkpoint positions are distributed, not Cartesian dimensions.
The adjacent direct arithmetic producer's older sampled AL value is explicitly
an implementation behavior, not a silicon-supported instruction schedule.
The old profile selectors, descriptor types, builders and result routing are
removed; 20,736 executions are replaced by 1,728 independent cases/replays.

`isa-words` owns fifty independently reset atomic instruction examples. The
default suite shares the same bank through one loop, replacing five separate
setup/check families. Ordered program/data/register seeds and partial result
predicates are retained, including duplicate-register seed/override writes.
Cold comparison against the old fixture formulas checks 14,500 register-bit
and 992 RAM-bit mutations; setup/reference fingerprint is `765e62e11e66de52`.
These atomic cases are not inferred redundant from staged tests. No numeric
case is removed, and the standalone runner requires neither firmware nor Nokia
devices. Five default result records become one; the full-default checkpoint
is not silently renewed by this focused extraction.

Unsupported delay combinations are not architectural conformance. The suite
retains sixteen fail-closed store-boundary cases in `composition_limits`
control, rather than a rejection matrix for each unsupported instruction family.
Nine sibling probes and their 188 case/restore pairs are retired; their exact
trap clocks, excluded extensions and per-family no-effect predicates are no
longer coverage claims. Supported delay-slot arithmetic, addressing, control
and restoration tests remain separate. Older revision-bound checkpoints below
may name retired diagnostics; they are not the current test inventory.

Block-control admission similarly retains the 14-case `block_branch_limits` and
28 delayed-block boundary cases in `composition_limits` rather than a separate exclusion profile
for every unsupported composition. The near-call, far-call and inner-repeat
exclusion matrices are retired: 108 rejected case/restore pairs, their precise
trap clocks and per-combination preservation predicates are no longer coverage
claims. This is an explicit reduction of implementation-policy tests, not proof
that the retained controls cover those combinations. Successful CALL/CALLD,
FCALL/FCALLD and inner-repeat interaction fixtures remain unchanged.

The compatibility-named `tms54isa` bench separates architectural examples
(`isa-square`, `isa-parallel`, `isa-scalars`, `mac-map-imm`) from implementation-admission checks
(`source_admission`) by explicit profile and review classification. Source-domain
owns its case state, timer cadence, bus observers and replay without the legacy
phase dispatcher. The base 30,000 cases retain their identities and checkpoint
choices; 448 absolute TC-clear controls and 14,208 short-source controls retain
status, shift-boundary and repeat contexts separately. The digest encodes the
instruction, extension, context and control tag.

The census also owns 1,408 staged MAC admission controls: 80 unsupported
absolute sources plus eight short forms, four selectors, two pages and the
original checkpoints 3/5. These retain prefetched instruction state not
covered by the base census's checkpoints 1/10. All original program words,
asserted register seeds and no-operand-effects checks remain; census-wide
program/I/O, SP and RC preservation checks also apply. The first 46,064 keys
retain digest `60d443519ef671c5`; their first 44,656 keys retain `6ae942418d722665`.
The old global-phase admission fixture is removed, not kept as a second copy.
An explicit 26-opcode table adds 8,328 mapped-source rejection tuples for
ADDC, ADDS/SUBS, shifted arithmetic, memory logic and MVDM accumulator slices.
It retains every formerly tested opcode/source/addressing-form/page combination,
with balanced original before/after checkpoints. The five separate rejection
profiles and their mixed positive/negative branches are removed. The resulting
54,392-case digest is `09bcb725ffd76435`. All census cases additionally preserve
explicit BK/TRN sentinels. Exact rejection-clock and negative-only literal-fetch
probes are retired; the contract is fail-closed admission without guest effects,
not a measured silicon fault pipeline. Positive operand-retention matrices stay separate.

`isa-scalars` runs 34 literal arithmetic-boundary cases in 19 reset groups.
Fifteen rows deliberately continue from checked predecessor state rather than
resetting: sticky overflow, retained instruction words, carry and preserved
accumulators remain observable. The legacy basic sequence shares those case
data without resetting its CPU. `legacy-atomic` is a fast continuation gate;
only its 34 named scalar results are covered by this family review, not the
whole prefix. The legacy machine contains the CPU, DMA and McBSP devices;
its source still shares firmware/GSM declarations and is not the standalone
CPU-only build. Its active-repeat restore checks an IRQ pending during 65,536
iterations, then compares live and restored completion. Unrelated DSPIF RAM,
queued-packet and register preservation runs separately in `make check-dspif-save`,
on a device-only machine without a CPU or private firmware.

Backend integration and private-ROM machine declarations have separate source
owners, as listed below. Their classes, ROM regions and registrations were
moved unchanged; this does not count as deleting tests or reviewing the
underlying ISA families. Total test C++ grows by 535 bytes/33 lines of explicit
includes and declarations. The legacy CPU class still shares firmware/GSM
members, so this source split alone does not make its build CPU-only.

This separation removes 1,215 bytes/28 lines from the C54x sources but adds a
2,504-byte device fixture: it is an ownership correction, not a net source-size
or runtime reduction. The bounded legacy run retains all 170 preceding PASS
records (including the 34 scalar IDs), with one new required repeat-restoration
record and the explicit transport-free completion. Long-RMW replay and both
3210 menu gates pass. The full-default checkpoint below also includes this
separation.

`isa-parallel` runs 60 literal cases: ten independently reset parallel
store/arithmetic controls, followed by 39 dual/parallel MAC/MACR/MAS/MASR
references and eleven signed/circular MAC, MPY, SQDST and ST/MACR examples,
each bank in one ordered reset group (twelve groups total).
The 39-case bank retains all source/destination
combinations, two ASM examples, same-AR read-before-write and four FRCT/OVM
controls. Partial RAM setup, the alias case's omitted AR4 setup/check and the
original carry/overflow masks are intentional.
The signed/circular bank retains negative products, signed X-Y, old-A square
accumulation with FRCT, rounded MAC, independent pointer updates, source-RAM
preservation and old-B stores. Its entry BK=4 is explicit standalone reset
metadata, not a reset inserted into the legacy sequence.
The default sequence reuses their literal register, masked-status and RAM
expectations through a short adapter; old phase numbers are compatibility
routing, not the case identities. The aliasing case checks the loaded old RAM
word and the newly stored word separately. These atomic cases do not replace
pipeline retention, repeat or interrupt-composition coverage. In particular,
the staged parallel-MAC matrix's carry policy is unvalidated; it does not
subsume these atomic carry controls. A shared PORTW constructor builds stimulus
only, not numeric expectations. The eleven unmarked examples instead execute
their literal instruction and IDLE without PORTW; their no-output check does
not create a timing claim. Compiled setup/predicate comparisons and native
acceptance are recorded in the family review. No case or checkpoint is retired.

`isa-long` runs 18 independently reset DADD/DSUB/DRSUB and T-based long-word
examples. Literal accumulators, address updates, source/decoy RAM, cycle
intervals and status masks are shared with the legacy reference sequence.
The T-based checks remain carry-only, with DSADT/C16 carry intentionally
unchecked; extraction does not promote omitted flags into preservation claims.
Both paths pass, including all 2,474 legacy asserted opcode words and 173 PASS
records unchanged. The independent CPU-only binary runs with an empty ROM
directory. This batch changes total C54x source size by only -44 bytes/+4 lines;
it is isolation and boilerplate replacement, not substantial source reduction.

`isa-products` retains 21 literal multiply/arithmetic/DELAY examples as one
ordered reset group. Partial setup remains partial: MPYA's memory form consumes
the previous A, fractional MACAR retains checked B/T/status, and the DELAY
addressing forms retain T. The same bank replaces the legacy reference phase
blocks without resetting that CPU. Case identities and expectation masks, not
phase numbers, describe the retained contracts.

`source_admission` owns write-only-source rejection for DLD `569b`, LDM `489a`,
MPY `629b` and MAC `649b`, including all sixteen base contexts and replay.
The duplicate legacy setup/check blocks and routes are deleted. Their old
numeric stimuli are explicitly retired in the review ledger, not claimed as
retained cases; port markers carried no separate interaction contract.
The positive DST continuations still seed A=`abcdef01` and ST1=0 explicitly,
because later stores and preservation checks consume those inputs. Successful
operand/timing examples remain separate from implementation admission policy.

`isa-flow` isolates ten conditional-delay and seven-bit stack-operand examples.
RCD A/B and BCD ALT/BLEQ must decide before delay words overwrite the tested
accumulator; taken/fall-through outcomes, RCD stack consumption and marker
intervals remain literal expectations. The PSHM/POPM case retains both swapped
data words, both stack words, balanced SP and its six-cycle interval. Standalone
cases reset independently; the legacy adapter retains its existing CPU state.
No far-control or pending-transfer restoration case is deleted. This rewrite
removes 1,781 C++ bytes/47 lines while preserving all 173 legacy PASS records
and 2,474 asserted opcode words.

`isa-memory` retains 256 memory/accumulator examples in ten ordered reset
groups (45, 30, 25, 20, 30, 12, 34, 30, 14 and 16). Legacy adapters share the literal data without resetting
their predecessor state. Carry retained across AND/MPY, shifted-SUB and
negative-half MACR rows remains observable; the 14 multiply rows keep their
full ST1 assertions, while other rows retain their original partial masks.
The second group also covers ABDST, LTD successor writes, coefficient/address
literal ordering, DP/SP addressing and preserved decoys, squares and saturating
ADDM. The third group covers signed-six-bit TS/encoded shifts, MAX/MIN signed
guards and an inherited-B tie, EXP/NORM and one-cycle RND/SAT examples.
The broader RND boundary matrix overlaps numbers but not these cycle checks.
The fourth group covers atomic ROR, shifted register OR/XOR/SFTL and immediate
AND/OR/XOR. It retains the checked carry entering ROR-B and cross-accumulator
AND, partial carry masks, guard clearing/preservation and one/two-cycle PORTW
contrasts, plus complement and partial-status arithmetic shifts. The fifth group
retains 30 full A/B/ST0/ST1 shift and load controls. Legacy entries stay at their
original continuation points; the additional 39 cases preserve their ordered
setups and predicates under 4,992 register-bit mutations and 234 marker controls.
The sixth group retains twelve absolute-marker arithmetic/address examples:
SUBS, both SUBC quotient outcomes through AR2 and AR4, negative MAR preoffset,
signed-immediate ADD/carry, cross-accumulator SUB/borrow and XOR preservation.
Their original partial carry masks, source/pointer checks and four/five-cycle
PORTW intervals remain literal references, not staged-ALU-derived expectations.
The absolute markers have their own retained program layout; circular markers
remain unchanged for the existing groups. No reset is added in legacy execution.
The seventh group covers literal/extended stores, stack round trips, status
MMRs, carry-fed ADDC, unsigned MPY and three-word repeated data copies. Each
row retains its original partial seeds, literal predicates and marker interval.
The ST1 store preserves the REA guard against reactivating an old repeat
endpoint. Repeated copies report their operation word, not the RPT setup word.
The eighth group retains absolute-marker load/store, masked carry/overflow,
fractional and rounded products, MMR addressing and timer-control readback
references. Legacy branch/call/return tests between its three segments remain
separate; running the bank alone does not qualify those control compositions.
The ninth group contrasts long-offset/address updates and extension ordering:
ADDM/ST/ANDM/ORM place the offset before the literal. It retains carry-clear/set and circular-wrap
examples. The tenth group retains NOP-primed immediate shifts, pointer and
status-preserving memory logic, signed/unsigned loads and an ANDM-to-ORM RAM
dependency under circular markers. Its NOP is stimulus, not the reported
operation word. Intervening legacy BCD/XC tests remain outside these banks.
Reset-only RAM metadata seeds inherited source words for standalone
execution; `prepare_case` deliberately ignores it. The POLY values match
the [SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf) page 4-126 example.
An old/new setup and predicate comparison checks 25,200 register/RAM/marker
mutations in the first group, 23,520 in the second and 19,600 in the third,
without CPU execution.
The independent binary runs with an empty ROM directory and lists only the
root and C54x CPU. Sharing the third group removes 10,285 C++ bytes/289 lines,
including its new literal bank and routing. Every instruction case is retained;
no runtime, all-ISA or physical-timing claim follows.

Long-offset MAC and memory-immediate logic encode **offset, then literal**.
This is independently checked with [GNU binutils 2.43](https://ftp.gnu.org/gnu/binutils/binutils-2.43.tar.xz),
configured `--target=tic54x` and assembled using `gas/as-new -al`:

| Assembly | Instruction words |
| --- | --- |
| `mac *ar3(2), #4, a, b` | `65e3 0002 0004` |
| `andm #0x1234, *ar3(2)` | `68e3 0002 1234` |
| `orm #0x1234, *ar3(2)` | `69e3 0002 1234` |
| `xorm #0x1234, *ar3(2)` | `6ae3 0002 1234` |
| `addm #0x1234, *ar3(2)` | `6be3 0002 1234` |
| `mac *(0x0200), #4, a, b` | `65f8 0200 0004` |
| `mar *ar0+` | `6d90` |

The assembler listing displays little-endian bytes within each word. It is an
encoding oracle, not evidence for pipeline clocks or arithmetic results.
Legacy atomic fixtures that reversed the two words must correct the stimulus,
not swap their independently expected source address or arithmetic result.

SPRU172C page 4-9 explicitly marks ADDM nonrepeatable; GNU as 2.43 also warns
that `rpt *(0x0918)` followed by `addm #1, *(0x091a)` is undefined.
`basic_repeat_sequences` therefore replaces its former expected
repeated-ADDM success with a fail-closed emulator check: the count source is
read once, ILLEGAL latches and the ADDM destination remains unchanged. This
is an admission policy, not a claimed silicon trap. Legitimate memory-count
repeats retain their positive coverage in `conditional_repeat`.

`memory_immediate_product` owns 7,424 independent cases/replays. Numerical
coverage uses one RAM form: all 64 word/literal pairs, FRCT/OVM for MPY,
and FRCT/OVM/SXM/SMUL plus all source/destination selectors for MAC.
Address-routing coverage crosses all 12 MPY and 8 RAM MAC forms with four
representative pairs, all prior flags/selectors and distributed pages/four
checkpoints. SXM remains a routing check for MPY, not a duplicate numerical
dimension. Exact source Read, Read-owned T publication, Execute-only result
and sticky overflow, pointer capture, source/T/literal poisoning and retained
operand observer identity are preserved. The legacy 90,112-input profile,
factory, checker, accounting and enumeration-only schedule test are removed.
Admission controls remain separately owned; repeat/interrupt compositions and
physical phases are not newly qualified. Focused build/run: 6.235/2.846 seconds.

These are SPRU172C MPY syntax 3 and MAC syntax 4 (MAC page 4-82). The retained
memory word and signed immediate produce the result at Execute; MAC adds the
selected signed 40-bit accumulator after the existing FRCT/SMUL product rule,
then applies destination OVM/sticky overflow. DP/SP selection shares the
direct-form page cases; CMPT=0 is fixed, and MPY fixes PMST=0. BK=13 circular
indexing uses an aligned 16-word queue with its initial index inside the queue.
The fixture does not qualify arbitrary peripheral/control sources, repeat,
IRQ or new producer/consumer-latency compositions.

`long_memory_rmw` replaces the long RAM/peripheral ADDM/ANDM/ORM and absolute
XORM profiles with 2,264 cases/replays. Routing banks cover all 25 RAM forms,
four offsets, compatibility selection, and all eight AR fields for paired RAM
and peripheral-window endpoints. Separate numeric banks retain eight operand
pairs and all SXM/OVM/initial-overflow combinations without crossing them with
every address. Absolute XORM retains four addresses, all value/status pairs,
both compatibility modes and an adjacent load consuming its forwarded result.
Pages and checkpoints are distributed, not Cartesian dimensions.

Each case checks ordered capture of both literals, pointer/RAM/literal poison,
one Read/Write pair, unchanged unrelated state and replay. The extra peripheral
clock remains checked. ST0 is checked inside the Write callback, preserving
publication ordering rather than only comparing state after the clock. Optional
register expectations on bus events support the same observation for data,
program and IO traces; they add no mutable observer state. The old 116,224-case
machinery, schedule header and enumeration-only tests are removed. These are
synthetic peripheral timing contracts, not real device semantics or measured
silicon phases.

The independent `long_accumulator_store` section replaces the ordinary/shifted
STL/STH bank with 3,408 cases/replays: 800 routing cases for all 25 forms,
four offsets, four selectors and both compatibility modes; 2,048 numeric cases
covering every shift -16..15, four values and SATD/SXM settings at an absolute
destination; and 560 shifted-routing cases covering the original seven forms
with offsets/selectors at five representative shift boundaries. Numeric settings
are distributed in the routing banks rather than multiplied across them.
One exact Write/no reads, ordered extension capture/poisoning, selected-pointer
update, later pointer poison, Execute-live source and preservation of unrelated
AR/status state are checked through IDLE/replay. Three checkpoints and pages
are distributed. The 70,144-execution legacy bank and its enumeration-only
schedule header/test are removed. These are model contracts, not measured clocks.

The independent `long_mvkd` section owns 2,504 cases/replays across all 25
long-address forms, four offsets, compatibility selection, selected-AR/AR2/RAM
and aliased-RAM sources. Four values remain where they affect the stimulus;
selected-AR sources use their updated pointer value rather than redundant
pattern labels. AR2-as-selected-source duplicates are removed. Pages and three
checkpoints are distributed instead of expanding to 19,200 executions.
It checks destination pointer update before Read, exact source Read at model
clock 7 and destination Write at 8, ordered retained extensions, source/pointer
poison and captured destination/status through IDLE/replay. A selected-AR poison
changes both source and pointer roles without an artificial intermediate write.
The old descriptor/setup and 253490/253500 phases are removed.

`shifted_memory_alu` owns 17,408 independent ADD/SUB cases/restores. All eight
selectors, four numeric patterns and SXM/OVM combinations remain. All 32
shifts are exercised at postincrement RAM through AR1 and at mapped AR0;
the other eleven RAM routes and seven mapped AR sources retain shifts
-16/-1/0/15. Full shift-by-route correlations are deliberately omitted.
`shifted_bk_alu` owns 4,096 cases over every selector/pattern/status/shift;
the former two identical absolute-source tuples differed only by page.
Pages and Decode/Access/Read checkpoints are distributed in both sections.

`memory_subc` retains all 10,240 RAM/mapped cases: both accumulators, every
64-value pair, SXM/OVM, twelve RAM routes and eight mapped AR sources.
The independent subtract/conditional-shift oracle tests the equality case
as a successful subtraction, no-borrow carry and sticky overflow.

All three sections seed CPU state explicitly, check one Read/no writes,
retain ordered address/operation extensions, and poison the sampled RAM,
AR or BK afterward. Late A/B assignment after Read proves Execute-live
accumulator ownership; the selected result and flags must not publish early.
Pointer/SP and extension poisoning preserve the address contract. Complete
CPU/result/observer state is checked through replay; model clocks are not
claims about physical bus contention.

`peripheral_mvdd` owns 192 cases/replays across three peripheral destinations,
four words and all sixteen dual modifiers. Pages/four checkpoints are
distributed instead of 1,536 executions. Exact Read5/Write7, Access pointer
updates, later pointer/source poison and retained flags/accumulators/SP are
checked. Inside bus callbacks, Read and MVDD Write observers must report the
logical instruction/opcode and original source, reject wrong addresses and
debugger peeks, and expire Read identity before Write. No repeat/IRQ or
physical timing claim is added. Legacy setup/result/declarations are removed.

`timer_irq_mask` retains the staged short-period timer reload followed by four
NOPs, direct IMR masking and a foreground marker. The timer generates a real
IRQ3, PSHM XPC/FRETE balances SP, the mask completes and the marker is written
once through replay. No exact interrupt latency is claimed. Four atomic/eight
staged diagnostic accounting runs are retired: their timing dumps were not
fidelity acceptance; the weaker IRQ-entry-only observation is subsumed here.
The probe taps/counters and result handler are deleted.

`timer_prescaler` owns 93 independent cases/replays: fifteen TDDR reductions
preserve the larger current PSC until borrow, then use the new divider and
latch TINT on expiry; fourteen TIM writes preserve PSC without an intervening
TIM/TCR read; sixty-four reserved/Free/Soft combinations retain stopped PSC.
Half-clock observation cadence preserves the old non-edge sampling points.
Only instantaneous timer values are asserted; intervening ticks do not read
TIM/TCR. PRD is overwritten after the final expiry check so replay must restore
reload configuration and phase. Free/Soft storage is tested, not debugger
halt behavior. The legacy setup/checker branches and private snapshot are gone.

`stopped_timer_reload` owns 48 independently initialized cases and replays:
all sixteen TDDR/PSC values, reload-only, identity ORM and attempted read-only
PSC writes. Guest instructions produce the PRD/TCR bus writes;
TRB reloads TIM and PSC while stopped, reads back zero, and later writes cannot
change PSC. Exact bus values/order and final TIM/PRD/TCR/A are checked.
Staged readback follows a NOP after ORM, avoiding unqualified simultaneous
peripheral arbitration; the settled observation is at clock 24 rather than 20.
Snapshot coverage is retained; no physical clock claim is added. The legacy setup
and phase branch are removed, not kept as an executable predecessor chain.

`immediate_subtract` owns 4,656 immediate SUB/shift-16 ADD cases/replays:
all SUB shifts 0..16, selectors, eight paired word/source patterns and eight
status combinations, plus every adjacent-load gap/value/selector. Pages and
three checkpoints are distributed instead of 27,792 executions. Signed 40-bit
source, SXM, bit-32 carry, sticky destination overflow and OVM use independent
integer expectations. Exact Execute publication, literal retention/poisoning,
producer forwarding and zero data-bus traffic survive replay. Legacy factory,
setup and result phases are removed; no physical timing claim is made.

`register_accumulator_arithmetic` and `register_accumulator_load` own 18,784
and 18,608 cases/replays. All immediate shifts -16..15 cross the original
patterns, selectors and status inputs; ASM repeats the four numerical
boundaries -16/-1/0/15 and separately checks every encoded shift/selector
with representative patterns/flags. Every adjacent-load producer/gap/value
remains, with distributed checkpoints instead of three copies. Pages are
distributed. Independent bitwise 40-bit shift and integer arithmetic references
check Execute-only publication, sticky overflow/saturation and LD carry
preservation; exact opcode/literal fetches, literal poison and no data bus
traffic are retained. The old 131,504-case owner, scheduler/digest and
enumeration-only host test are removed. No physical pipeline phase is claimed.

The CLI test exercises every isolated profile's machine/BIOS selection and
result checking using simulated process output. The inventory is a structural
preservation check, not executed coverage, an independent arithmetic oracle
or evidence of silicon timing. Native conformance and affected handset gates
remain necessary. Never automatically regenerate the baseline to make a
refactor pass; `--write` is reserved for an explicitly reviewed contract change.
Expected arithmetic/timing results must remain independent of CPU execution
helpers and shared profile-selection metadata.

Literal `instruction_case` records own `reset_before` and `reset_initial`.
The CPU-only runner applies reset-only seeds after its common CPU reset setup;
ordinary `prepare_case` ignores that metadata, so legacy adapters preserve
their predecessor state. Scalar continuation boundaries and the product/memory
groups therefore have one data owner, not a parallel reset vector or BIOS-name
seed branches. An old/new comparison preserves all 134 literal records and 55
reset groups, including every program/data/register/mask/marker field.

`tms320c54x_test_snapshot.h` centralizes buffer reset, rewind and MAME
serialization. The three-argument restore cancels the check timer before
loading; the two-argument overload leaves scheduling with its caller. Do not
substitute one for the other: zero-time and peripheral-triggered restores have
distinct timer ownership. Assertions, counter rewinds, CPU suspension and
continuation stay explicit in each family; neither helper saves or restores
host observations. Consolidating 47 captures and 82 loads across 13 modules
removes 7,001 C++ bytes/85 lines without changing guest setup, expectations,
phase routing or result records. Compiled helper tests check cursor/error
state, serialization failures and both timer-ordering contracts.

Runner unit tests exercise the acceptance grammar across every declared
isolated profile and named scope: exact marker multiplicity, every `key=value`
field, terminal completion and scope separation. They do not restate CPU
semantics per profile. Named-case and ordered-digest validation have separate
negative tests in `test_c54x_case_fixture.py`. Consolidating these checker tests
reduces host-test source only; it does not retire native CPU cases or shorten
the native regression.

`tools/c54x_profiles.py` reads literal BIOS ownership from the C++ declarations;
the runner does not maintain a second machine/profile list. Unsupported or
ambiguous declarations fail closed. Exact expected results live separately in
`tools/c54x_test_oracles.py`, without imports from the CPU or profile decoder.
`tools/run_c54x_core.py` owns CLI selection, process execution and result
validation, retaining its established checker/import API. The seven named
scopes share one selection table; the frozen baseline independently checks
their result contracts and every declared isolated machine/BIOS selection.

The bench is separate from the processor implementation. Its current source
boundaries are:

| Source | Responsibility |
| --- | --- |
| `tests/c54x/tms320c54x_test.cpp` | Synthetic core/extended conformance-machine declarations; no private ROM inputs or Nokia backend compositions |
| `tests/devices/nokia_dsp_c54x_test.cpp` | Synthetic Nokia backend/COBBA/CCONT/DSPIF integration, not CPU-only ISA conformance |
| `tests/firmware/tms320c54x_test_verifier.cpp` | Stock staged-verifier programs and product flash inputs; peripheral sensitivity, not fitted handset identity |
| `tests/firmware/tms320c54x_test_rom4_machine.cpp` | Private ROM4/ETSI vector declarations and firmware-machine registration |
| `tests/c54x/tms320c54x_test_state.h` | Shared bench state, fixture types and method declarations; no CPU execution implementation |
| `tests/c54x/tms320c54x_test_state.cpp` | Bench lifecycle, address maps, assertions and device wiring |
| `tests/c54x/tms320c54x_test_isa.cpp` | CPU-only examples/admission bench with explicit reset groups and no Nokia/firmware dependencies |
| `tests/c54x/tms320c54x_test_case.h` | Generic stimulus loading and independent register/bus comparisons; no instruction semantics |
| `tests/c54x/tms320c54x_test_square.h` | Reviewed literal SQUR/SQURA/SQURS cases shared by generic and default-sequence entries |
| `tests/c54x/tms320c54x_test_parallel.h` | Reviewed parallel store/arithmetic vectors, including same-cell ordering and circular pointer controls |
| `tests/c54x/tms320c54x_test_dual_reference.h` | Ordered dual/parallel MAC literal references with explicit partial setup and status masks |
| `tests/c54x/tms320c54x_test_dual_operand_section.h` | Explicit MPY/ADD/SUB/MACSU/MAC-family/parallel-store cases using common clocked execution and replay |
| `tests/c54x/tms320c54x_test_dual_input.h` | Shared fixture-owned integer inputs/expectations; no CPU implementation helpers or Nokia dependencies |
| `tests/c54x/tms320c54x_test_memory_continuations.h` | Ordered literal continuations with inherited RAM, partial masks and coefficient/address ordering |
| `tests/c54x/tms320c54x_test_shift_examples.h` | Ordered TS/Xmem shifts, signed comparisons, normalization and one-cycle rounding/saturation references |
| `tests/c54x/tms320c54x_test_normalization.h` | CPU-only staged NORM/EXP observations and shared snapshot continuation |
| `tests/c54x/tms320c54x_test_normalization_input.h` | Independent bit-position/leading-bit expectations, shared with legacy EXP-product controls |
| `tests/c54x/tms320c54x_test_normalization_schedule.h` | Unique normalization/exponent inputs with balanced replay checkpoints |
| `tests/c54x/tms320c54x_test_register_sequence.h` | CPU-only clocked register observations and snapshot continuation; no instruction arithmetic |
| `tests/c54x/tms320c54x_test_address_mode_section.h` | Address-mode/DP section inputs and observations; programming-state limitations remain explicit |
| `tests/c54x/sections/frame_stack_consumers.h` | CPU-only FRAME/push and safely separated compiler-SP inputs |
| `tests/firmware/tms320c54x_test_rom4_context.cpp` | Copied Nokia context/worker/timer integration; separate profile, legal standard-mode status seeds |
| `tests/c54x/tms320c54x_test_register_pairs.h` | SAT and carry/TC rotate pairs with independent expected register states |
| `tests/c54x/tms320c54x_test_composition_limits.h` | CPU-only CLKMD/interrupt-POPM and control-span admission limits; fixed inputs, replay and frozen guard clocks |
| `tests/c54x/tms320c54x_test_register_alu.h` | Signed40 MIN/MAX and signed17 SQUR expectations, retained inputs and explicit late operand writes |
| `tests/c54x/tms320c54x_test_port_case.h` | Common PORTW stimulus construction; no instruction arithmetic or reset policy |
| `tests/c54x/tms320c54x_test_scalars.h` | Literal scalar boundaries with explicit reset/continuation groups, shared with the legacy basic adapter |
| `tests/c54x/tms320c54x_test_memory_examples.h` | Ordered literal memory arithmetic/multiply/T examples, with original partial setup and assertion masks |
| `tests/c54x/tms320c54x_test_domains.h` | Independent single-memory admission fixture: catalogue, preserved state, bus observers and replay |
| `tests/c54x/tms320c54x_test_dispatch.cpp` | Ordered callback routing and the initial-program completion tail |
| `tests/c54x/sections/product_dependencies.h` | Independent product operand/producer dependency cases; numeric product owners remain separate |
| `tests/c54x/tms320c54x_test_modified_mvdk_section.h` | Independent circular/indexed move geometry, mapped aliases and raw payload contracts |
| `tests/c54x/tms320c54x_test_domains.h` | CPU-only admission fixture with owned progress, observations, taps and snapshots |
| `tests/c54x/tms320c54x_test_register_unary.h` | Independent register-logic, shift, NEG/ABS inputs, balanced replay keys and explicit clocked observations |
| `tests/c54x/tms320c54x_test_domain_schedule.h` | Independent instruction inventory and explicit admission-control schedule; no legacy phase routing |
| `tests/c54x/tms320c54x_test_delayed_memory_section.h` | Independent delayed-memory capture/continuation cases; no shared phase-chain state |
| `tests/c54x/sections/xc_return.h` | Selected/annulled XC return, resumed-word protection and interrupt deferral |
| `tests/c54x/tms320c54x_test_counted_copy_section.h` | Independent memory-count copy, chaining and PC-wrap contracts |
| `tests/c54x/sections/frontend_drain.h` | Decoded/empty frontend entry, flush timing and case-owned acknowledgement checks |
| `tests/c54x/tms320c54x_test_observer.cpp` | Instruction identity, ordinary-read exclusion and XC annulment observations |
| `tests/c54x/tms320c54x_test_block_repeat.cpp` | Delayed-block-repeat layout, exclusions, IRQ acknowledgement and replay |
| `tests/c54x/tms320c54x_test_pipeline_state.h` | Family-owned frontend, observer and delayed-block-repeat storage |
| `tests/c54x/tms320c54x_test_interrupts.cpp` | Interrupt composition, returns, status masks and PLL/XC continuations |
| `tests/c54x/tms320c54x_test_interrupt_entry.cpp` | Vector prefetch, entry spacing, accepted-source retention, stage restoration and their acknowledgement checks |
| `tests/c54x/tms320c54x_test_entry_state.h` | Entry-family observations, taps and replay storage; predecessor tap cleanup remains explicit |
| `tests/c54x/tms320c54x_test_peripherals.cpp` | Bus, clock, serial, DMA and timer fixtures |
| `tests/c54x/tms320c54x_test_restore.cpp` | Atomic far-transfer replay and extended program-memory continuations |
| `tests/c54x/tms320c54x_test_repeat_restore.cpp` | Saved/body IRQ repeats, active-context restoration and delayed far-call/return replay |
| `tests/c54x/tms320c54x_test_snapshot_state.h` | Active-repeat and cross-module far-transfer checkpoint storage; peripheral checkpoint streams remain family-specific bench members |
| `tests/c54x/tms320c54x_test_repeat_state.h` | Independent case inputs, observation types and per-family replay storage; no generic rewind policy |
| `tests/c54x/tms320c54x_test_disassembler.cpp` | Bounded decoding and operand-domain matrices |
| `tests/firmware/tms320c54x_test_rom4.cpp` | Acquired-ROM routine execution and independent reference comparisons |
| `tests/c54x/tms320c54x_test_atomic.cpp` | Routes atomic phases to their owning sequence without a global result scan |
| `tests/c54x/tms320c54x_test_atomic_basic.cpp` | Remaining synthetic basic sequence, scalar case adapter and legacy transport-save integration |
| `tests/c54x/tms320c54x_test_atomic_corpus.cpp` | Independently asserted instruction words observed in the ROM4 corpus, without loading the acquired image |
| `tests/c54x/tms320c54x_test_atomic_reference.cpp` | TI instruction examples and arithmetic boundaries |
| `tests/c54x/tms320c54x_test_atomic_control.cpp` | Extended program flow, RTN and pending-transfer restoration |

The result dispatcher preserves phase-check order, including paths that update
the phase and fall through within one callback. Family extraction must retain
that ordering and distinguish callback returns from returns inside observers'
lambdas. Source grouping does not establish independent fixture state; moving
a case does not authorize changing its inputs, expected values or observation
boundaries. The dispatcher delegates consecutive clauses to ordered handlers,
not an unordered subsystem sweep. A handler returning false permits subsequent
clauses to run even if an earlier clause changed the phase. Atomic routing
has a separate exhaustive guard-partition check and a compiled host control;
the three inter-sequence boundaries explicitly schedule the next timer callback.
The sequential regression still inherits state between steps. The source split
does not turn those steps into independent reset fixtures or remove their
cross-instruction checks.

Test ownership and build selection are defined in [the test contracts](../tests/README.md).
Shared setup helpers write only stimulus; expected results remain in the test
and independent runner oracles. Historical checkpoint hashes retain their
original source paths: relocation does not renew their runtime qualification.
Constant-only setup may use ordered `set_registers`/`write_words` batches;
values that depend on preceding guest writes must remain sequential calls.
`resume_atomic_at` preserves inherited state while setting PC, clearing IDLE,
and scheduling the existing 100-microsecond atomic observer, in that order.
Neither helper resets the CPU, computes expectations, or makes sequential
legacy cases independent.
`begin_atomic_case` likewise retains predecessor state; reset-only metadata
belongs to the standalone runner, not the legacy case-start helper.

Register-only logic/CMPL (`register_logic`), SFTA/SFTL (`accumulator_shifts`), `negate` and `absolute`
share the clocked CPU-only runner and independent integer case builders.
Logic currently retains its numeric/status/page tuples with balanced replay
points. Shifts select3,840 cases: every shift/pattern/selector with baseline
flags and the complete flag matrix at shifts-16,-1,0,15. Non-boundary flag
cross-products are retired; standard-mode ARP is corrected from5 to0.
NEG/ABS select400 cases each:128 numeric
pattern/selector/OVM combinations,240 additional status checks at zero and
overflow, and32 adjacent-producer cases. Second-page replication and the
remaining numeric/status cross-products are retired, not claimed equivalent.
The domains now use explicit loops rather than opaque legacy case-number
decoding. All3,104 logic,3,840 shift and400+400 NEG/ABS inputs remain.
Exact single opcode fetch, Execute-only publication, no data accesses,
adjacent producer state and replay are checked natively. Legacy profile/result
plumbing and the enumeration-only host census are removed. Checkpoints are
distributed, not multiplied. A real final IDLE replaces unobserved padding, not a
consumer interaction. Repeat, IRQ and memory-operand compositions stay separate.
The basis is [TI SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf);
publication clocks and the documented left-carry policy are not silicon measurements.

The absolute-alias fixtures share declarative family/count/status profiles in
`tests/c54x/tms320c54x_test_profiles.h`; the CPU model does not consume this test
metadata. `test_c54x_alias_profiles.py` executes the host C++ decoder and
compares all10,304 encoded cases/status inputs with their retained domains.
All ten separate MAME rejection matrices also pass with the original markers
and restoration counts. These focused matrices do not extend the separately
qualified default-suite population or change silicon admission claims.

Handset/MU4 state and media acceptance remain in
[5510 bring-up](5510_bringup.md), [AAC validation](aac_validation.md)
and [ROM4 loader](rom4_dsp_loader.md).

CPU admission remains fail-closed. In particular, short Smem MOD=3
preincrement is write-only (SPRU131G table 5-4). The
corrections below cover selected read families, not the entire ISA. The
single-address source/RMW exclusion census below closes the implemented
Smem/Lmem and MMR-source classes, not every positive fixture domain or
multi-operand grammar; passing mechanically decoded cases is not
hardware-legality evidence.
Long-offset MOD=13 preincrement is a distinct read-capable form and must not
be removed by that audit.

The debugger's shifted-memory (`6fxx`) decoder applies the same source/store
distinction: short `98..9f` is valid for STL/STH destinations, not LD/ADD/SUB
sources; address bytes `f9..ff` are reserved. Long-offset MOD=13 remains
admitted. This corrects an older disassembler fixture that treated all 256
address bytes as legal whenever the operation extension was recognized.
The exhaustive matrix distinguishes 108,992 valid and 5,696 invalid operand
tuples, each tested at two program-page positions, separately from reserved
operation extensions. It checks instruction claims and bounded word fetches,
not execution timing or completion of the remaining source/RMW audit.
`tools/run_c54x_core.py --mame mame/mame --rompath mame/roms --profile xorm
--output-root tmp` runs this matrix before its separate 1,024 XORM execution
and restoration cases. Accepted evidence: `tmp/run_c54x_core_45tucxku`.
The [full-core checkpoint](#retained-suite-acceptance) covers the current
source-admission guards; optional profiles retain their separate scopes below.

### Operand-domain acceptance

`source_admission` enumerates all 111 implemented ordinary
Smem/Lmem source/RMW high-byte classes, ten shifted read extensions and four
MMR-source forms. The base 15,000 cases and save/replays check short `98..9f` and
unadmitted `f9..ff`, CMPT, two program pages, both CPU frontends and snapshots
distributed before/after rejection. Data reads/writes, program writes and I/O accesses stay
zero; accumulators, T, status, repeat/stack pointers and all ARs are retained.
The independent fixture inventory does not call CPU admission helpers.
Another 224 cases check `68xx..6bxx` absolute aliases with TC initially clear.
There are 7,104 short-source controls, 704 MAC guards, 8,328 mapped-source
guards, 2,304 MMR guards and 4,800 repeat-transfer guards: 38,464 cases and
actual restores, replacing 61,496 executions. Duplicate checkpoint contexts
are removed from the base/control and MAC banks; every instruction/address/
status/page/frontend input remains. Each case explicitly cold-resets, matching
the original contract, including atomic repeat state. The common runner owns
the no-data, no-I/O and no-program-write checks. The host test retains only
the independent 125-entry instruction-inventory check, not a mirrored decoder
or obsolete profile digest.

A retained regression is atomic `0ef9` (SUBB), which must reject before
an operand transaction or changes to status/repeat state. The shared atomic
source-operand predicate covers the implemented single-address source classes,
including special-load, test and transfer families. Shifted `6fxx` aliases are
rejected before addressing; short `6f98..9f` still uses its operation extension
to distinguish invalid reads from valid destination stores. MMR long forms
retain their separate grammar.

This closes the source/RMW exclusion surface, not positive admission of every
addressing mode, arbitrary repeat/delay compositions, destination-only alias
policy, Xmem/Ymem encodings or physical silicon traps. Existing positive
preincrement stores and long-MOD13 reads remain distinct contracts.

[SPRU131G](https://www.ti.com/lit/ug/spru131g/spru131g.pdf) table 5-4 and
section 5.5.3.1 restrict short MOD=3 (`*+ARx`, Smem byte `98..9f`) to writes.
The rejection banks below check both frontends, CMPT settings, program
pages and two save/load checkpoints. They require no operand transaction,
pointer change, accumulator/T or status effect; stack and transfer banks
also observe stack, program and I/O effects as applicable. These are
fail-closed emulator contracts, not claims about silicon traps.

The CPU-only fixture selects CMPT through ST1 bit 5. PMST bit 14 is not a
substitute; tests that only toggle it do not establish CMPT coverage.
Ordinary short-source admissions are already covered by the base catalogue.
Additional controls retain these distinct contexts:

| Control | Cases/restores | Scope beyond the base catalogue |
| --- | --- | --- |
| Shifted extensions | 5,120 | Ten operation/destination forms at shifts 1, 15, 16 and 31; shift 0 is in the base. |
| Nonzero ASM | 4,352 | 34 multiply/load/application classes at ASM=-11; RC preserved. |
| C16 | 1,792 | 14 paired-arithmetic opcode classes with C16 set. |
| Short-repeat SUBC | 256 | Both destinations prefixed by `RPT #3`; the prefix legitimately changes RC. |
| TC clear | 2,688 | Bitfield, logic/transfer, MACP/MACD, LTD/DELAY and RMW forms. |

All controls cover eight AR selectors, CMPT, page, frontend and both
checkpoints. Shift is a distinct encoding field from source admission; the
interior shift Cartesian product is retired, not claimed as retained.
Positive arithmetic profiles still test numeric results at every shift.
The pure rejection profiles and the rejection tails of `long-pair`, `drsub`,
`logic-short` and `xorm` are merged here. Those four profiles retain their
positive operation contracts. The redundant atomic-prefix stack profile and
one-off atomic rejection probes are removed; their admission contracts live
in this CPU-only census rather than the architectural example sequence.

Absolute `f9..ff` must not silently alias canonical `f8`. The prior atomic
MPY/MAC, LTD/DELAY, MACP/MACD and RMW paths read operands despite staged
rejection; MAC/SQUR could publish T and RMW could fetch an address extension.
Admission now precedes those effects. Canonical absolute and supported long
MOD13 forms remain distinct positive contracts. This is emulator frontend
consistency, not evidence that these aliases trap on silicon.

Corrected positive load domains are:

| Profile | Admitted cases/restores | Domain |
| --- | --- | --- |
| `indirect-load` | 6 | Stationary/postdecrement/postincrement, both destinations; two invalid cases omitted. |
| `scalar_offset_load` | 4,224 | Four long forms plus mapped-AR offset reads; checkpoints distributed. |
| `scalar_unsigned_load`, `scalar_shift16_load` | 18,176 each | 17 RAM forms plus mapped AR sources; write-only short MOD3 omitted. |
| `ld-ts` | 208,896 | Reindexed 17-form domain; 12,288 invalid cases omitted. |
| `scalar_variable_load` | 5,248 | 17 forms; all32 shifts at postincrement AR1, boundary shifts elsewhere. |
| `paired_indirect_load` | 7,680 | 15 indirect/long forms; short write-only MOD3 omitted; checkpoints distributed. |

The four independent scalar-load sections initialize every AR and keep
signed/unsigned result semantics, all fixed-load status/address combinations,
and mapped AR source values through direct, absolute and long-offset reads.
AR7-as-source remains special: its original pointer value is sampled before
the post-Read poison, not overwritten by the address-retention probe.
Variable shifts retain all32 values at postincrement RAM through AR1;
the remaining33 address/register routes retain -16/-1/0/15. Interior shift
correlations across those routes are not claimed. Pages and checkpoints are
distributed rather than multiplied into every tuple.

Each checks one Read/no writes, Access-owned ARP/pointer changes, retained
address/operation extensions, pointer/SP/BK poison before Read and RAM/AR
poison afterward. Accumulators remain unchanged through Read and publish
only at Execute. SXM/OVM and unrelated registers remain unchanged through
replay. A bit-position oracle supplies signed shifts without CPU helpers;
these checks do not establish physical memory-bank or bus-clock behavior.

Independent memory-product sections omit the same write-only short form:
`memory_accumulator_product`, `memory_unsigned_product`,
`memory_signed_product` and `memory_square` each own 8,704 cases/restores;
`memory_load_t` owns 2,176. Each covers nine RAM forms at AR1/AR6 and all
sixteen CPU-mapped source words AL..AR7. The eight numeric patterns and
eight FRCT/OVM/SMUL combinations remain crossed with those sources and
operations. SXM, pages and Decode/Access/Read checkpoints are distributed
rather than crossed; their full Cartesian interactions are not claimed.
LD T's four duplicate operation labels collapse to one actual opcode.

Every section initializes all auxiliary registers, status and accumulators.
Read captures one source, publishes T where specified, and precedes
Execute's accumulator/overflow publication. Literal, pointer/SP and RAM
poisoning test retention; T is overwritten after Read and must not be
rewritten at retirement. Unexpected data writes are forbidden. These are
CPU-model contracts, not peripheral identity or physical timing evidence.

Other positive domains retain 34,816 scalar ADD/SUB inputs, 17,408 each for
ADDS/SUBS, 69,632 shift-16 and 153,984 TS arithmetic cases (6,528 address
plus 147,456 numeric). Variable-shift ADD/SUB is the 17,408-case independent
`shifted_memory_alu` section, with deliberate route/shift factoring above.
CMPM and BITF each retain 17,408 distinct inputs over 17 forms, with balanced
checkpoints rather than three replays of every input. `repeated_subc` owns 1,200
independent cases/restores and `repeated_immediate_store` owns 600. Both retain
five legal addressing forms, short/long RPT, counts0/1/15/32/255, four numeric
patterns and none/IRQ0/NMI. SUBC retains both destinations. Arithmetic modes,
page and four checkpoints are assigned instead of fully crossed. SUBC checks
per-retirement arithmetic, carry/overflow, RC and one-clock body cadence;
stores check two-clock writes with address updates preceding each publication.
Both poison the captured body (and store literal), require one fetch, and
retain the original PSHM-XPC/FRETE ISR with complete ordered data/stack
transactions and exact post-repeat RTN. FRETE clears INTM even after NMI;
the final state explicitly reflects that model behavior.
Memory/immediate product coverage is now the 7,424-case section above. Paired reads retain 6,144 DSUB,
12,288 DADD and 6,144 DRSUB cases over eight forms, omitting raw form 4.
These totals describe selected fixture domains, not every legal operand.
Circular-input constraints have their own
[contract](#circular-fixture-domains); modulo expectations alone are not
silicon evidence.

`bitf-smem` and `cmpm-smem` retain all eight original word/literal pairs,
both initial TC states, SXM/OVM combinations, register fields and compatibility
states. Their independent predicates follow [SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf),
pages 4-22/4-33: nonzero masked word and 16-bit equality, respectively.
Literal/address/source poisoning, TC publication, preservation and bus-effect
assertions remain in live and replay execution. Each fixed non-pair group
distributes its eight inputs two or three per original checkpoint;
no complete value-by-checkpoint product is claimed. Actual restores and ordered
raw-case/checkpoint digest `e8a1e8427edb11e5` are required. The focused runs drop
from 22.90/23.07 seconds to 7.69/7.77 seconds. Explicit schedule/accounting adds
1,750 C++ bytes/34 lines; this is not source shrinkage. Mapped-source, branch,
peripheral, repeat and delay interactions remain separate contracts. The
profiles still use the legacy bench, not the independent CPU-only binary.

`memory-alu`, `adds`, `subs` and `memory-shift16` share that same replay policy:
every original input gets one live/replay pair, with eight patterns distributed two/three/three
across the three checkpoints in each fixed non-pattern group. The four banks
retain 139,264 distinct inputs rather than 417,792 checkpoint copies. A compiled
audit checks that all 15 fields of every old input/reference descriptor match
its first-bank counterpart. Descriptors are pure functions of the case identity
and profile kind, cached for the live/replay pair rather than recomputed each
tick. Signed/unsigned source extension, conditional shift-16 carry updates,
destination overflow, OVM saturation, literal/source/pointer poisoning and
exact preservation/bus checks remain unchanged. Ordered digests and actual
restore counts are mandatory; the full pattern/checkpoint product is retired.

LTD and DELAY read `Smem` and copy to `Smem+1`; LTD also loads T
([SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf), pages4-81/4-41).
The implicit successor write does not make the encoded source a write operand.
Source admission rejects short `98..9f` before address modification or
operand access. `4d9b` is not a validated preincrement read; its positive control uses
legal long MOD=13, `4deb 0001`, preserving the distinct two-cycle contract.
This is fail-closed source admission, not a silicon-trap claim. Positive staged
LTD/long-DELAY qualification is separately described under
[successor memory copy](#staged-successor-memory-copy). MACP/MACD have the same explicit source
guard; MVDK, MVDP, MVMD, PORTW and WRITA are covered by the central
Decode admission/addressing checks. Destination-side transfers are distinct,
as described below; the implicit write alone is not evidence of a missing
guard. Unenumerated instruction families still require their own audit.

After the guard, `delay-copy` and `delay-copy-ctrl` pass 15,360 and 73,728
legal cases/restores. Native 5110 save/load and the 30-second RF boundary
remain unchanged (20,280 frames, 648,032 reads, 1,537 port-32 writes);
the default 3210 semantic acceptance also passes. The
[core checkpoint](#retained-suite-acceptance) additionally qualifies the
complete default suite with these source guards and corrected fixtures.

`operand_order` checks 46 independently reset instruction/form cases.
Ordered program traces include every extension and any program operand;
data and IO events require the extension cursor to have reached the complete
instruction before operand access. This preserves cross-bus ordering, not just
independent final access counts. Source/destination values, pointer updates,
comparison TC and IDLE/replay are checked. Seven old MVPD f9..ff success cases
are retired as undocumented absolute aliases; this does not change the CPU
decoder or certify those encodings. No physical timing claim is made.

Operand role is essential. MVKD/MVDM, PORTR, MVPD and POPD encode a
destination and must not inherit a read-only Smem restriction. In particular,
`7c9a` in `operand_order` is a valid MVPD destination, not a missed
source rejection. Xmem/Ymem have separate grammars. For `6fxx`, classify the
operation extension before deciding source versus destination; shifted
STL/STH remain unchanged. Rejection support does not imply positive staged
support for unenumerated operations or compositions.
MVDP's Smem is a data-memory **source** (SPRU172C p. 4-111), unlike
MVPD and READA destinations. The atomic MVDP handler formerly admitted
short `98..9f`, read data, advanced the AR and could write program memory.
Its early admission guard now rejects that encoding before any operand
effect. The unchanged-core regression fails on a data transaction;
`logic-short` adds 256 cases/restores across all ARs, initial TC, CMPT,
program pages, frontends and two checkpoints. The full bank requires zero
data/program/I/O writes, zero source reads and unchanged pointers/status.
This is fail-closed admission, not a silicon exception specification.
Ordinary long MOD=13 and the destination-side preincrement forms are not
removed. Full core conformance retains all 292 required markers, including
legal postincrement, absolute and long-offset MVDP controls. Native clocked
boot, all four keypad-tone settings, 5110 save/load, RF/processing and the
3210 baseline/frontier regressions pass without rebanking their oracles.
This closes the short-MOD=3 source-guard question for these named copy
families, not the whole ISA or their positive timing/composition domains. The
[pinned TI assembler control](rom4_dsp_capture.md#standalone-vendor-assembler-operand-control)
now warns on all four short-preincrement RMW forms just as on the forbidden
LD source, while preserving ordinary stores and long-offset reads. This
supports the atomic admission guard for ANDM/ORM/XORM/ADDM before operand
effects. The unchanged-core control fails on a data transaction; the guarded
`xorm` profile passes its 1,024 legal cases/restores and a separate 1,024-case
rejection matrix over all ARs, CMPT, initial TC, pages, frontends and two
save points. Rejection preserves pointers/status and performs no data,
program or I/O write. Accepted evidence: `tmp/run_c54x_core_g0gyh6d9`;
the unchanged-core failure is `tmp/run_c54x_core_wpl42iu_`.
The debugger likewise declines these short-preincrement forms without
fetching their immediate; ordinary stores and long-offset forms remain
separate positive controls. This is fail-closed admission, not a silicon
trap specification or an all-ISA closure.

Post-guard legal controls independently pass `long-mvkd` (427,784 shared
cases/restores in that retained exhaustive capture, including 153,600 RMW), `preinc-store` (6,144), `preinc-st`
(1,536) and `xorm-absolute` (4,096). Their accepted runs are respectively
`tmp/run_c54x_core_am84mku0`, `tmp/run_c54x_core_51wgjuyl`,
`tmp/run_c54x_core_e65578ms` and `tmp/run_c54x_core_9yfpz7bw`.
Native ROM4 external-clock boot and wire/pin restoration also pass in
`tmp/native-pcm-clock-_gqp6iqq`. Original MU4 `bootrecmeta` passes its own
resident/overlay checks and recording/metadata transaction in
`tmp/native-gates/c54x-rmw-mod3-mu4`; its complete endpoint NAND is byte-equal
to the accepted recorder-persistence endpoint (SHA256
`75a523a9c1b6a0f6d98d47812b3082717e0764a79e0956b47b2de4fa7160d6c3`).
This is a native regression result, not repaired recording-tail policy,
whole-file playback, handset boot or native speech acceptance.

MACP/MACD source admission is separately corrected. SPRU172C pages 4-87
and 4-89 define Smem as the value read into the multiply and T; MACD's
implicit copy writes its successor location, not the source operand. The
atomic handler formerly applied short preincrement before reading that
source. A negative control observed a data transaction before rejection;
the guard now rejects `98..9f` before address modification or operand access.
This is fail-closed emulation of an unsupported operand form, not a claimed
silicon exception. It does not introduce positive staged MACP/MACD support.

The CPU-only `source_admission` census owns this rejection contract, including
TC-clear controls. Existing postdecrement/postincrement, absolute and
long-offset MACP/MACD examples remain in the default core regression; long
MOD=13 is not removed. This is still not an all-ISA closure.
The cold finalized MU4 prefix preserves all seven prior digital captures
byte-for-byte (decoded returns, DMA, DXR writes, TX words/times and DIN
words/slots); its independent ideal-DAC checker also passes. This excludes
an observed prefix regression, not every media stream or completed playback.

The repeated immediate-ST fixture distinguishes first-body postincrement from
later retained iterations. Its qualified postincrement address update is in
Access, two clocks before the first literal write; later repeat updates occur
one clock before their writes. Other addressing forms retain their separately
qualified schedule. The earlier first-update assertion incorrectly applied the
later-iteration timing to postincrement; correcting it changes no store
execution. This is the existing emulator phase contract, not a new physical
timing claim. Conformance failures now print the case, tick, replay, registers
and transaction counters, and the runner rejects any such assertion diagnostic
even if completion markers are also present.
The revision-bound full-default checkpoint below includes the legal MACP/MACD
examples; its current requirement count is recorded there. The separate
program-multiply rejection and repeated-SUBC/ST profiles and the complete tool
suite also pass. These are executable emulator regressions, not all-ISA or
physical pipeline conformance.

### Standalone staged MVDP

The staged frontend now implements legal standalone data-to-program copies,
separately from the opposite-direction MVPD path. Decode retains the explicit
program destination and addressing extension in their architectural order:
absolute source before `pmad`, but long-offset source after `pmad`. Access
resolves the source once; Read captures its value and mapped program target;
Execute writes the retained word. Destination fields travel with their
instruction, rather than sharing an Access-written global latch. The two
extra hold clocks reproduce the four/five-clock standalone instruction
totals; their exact silicon bus placement remains partial.

`tms54xtra -bios mvdp` covers 336 cases and restores: seven source forms
(direct, SP-relative, postincrement, absolute, offset, preincrement-offset,
circular), eight AR selections, two instruction pages and three snapshots.
Changing the source and AR after their sampling points must not alter or
duplicate the transfer. `mvdp-pair` adds 12 adjacent-transfer cases/restores
across both pages and six checkpoints, requiring two ordered source reads
and two distinct program writes. The unchanged staged core fails the first
legal standalone case before a transfer, so this is an executable admission
regression, not a source-string invariant.

These fixtures do not establish every legal operand, external wait states,
mapped CPU/peripheral sources, self-modifying-code prefetch coherency, or
control-delay/XC/block-repeat composition. The latter compositions
remain explicitly unsupported; no atomic fallback is used. Native handset
speech and the unresolved serial-status input are not promoted by these
instruction fixtures.

### Repeated staged MVDP

Single-instruction repeat uses the same separate data-to-program direction:
the first transfer captures its source in the ordinary staged slot, then
the retained repeat stream reads a fresh data word for each later iteration.
The source's indirect modification occurs once per transfer. The explicit
`pmad` increments with 16-bit wrap in the instruction's logical program page;
OVLY translation applies to each resulting program address. The initial
instruction retains its standalone hold clocks and later iterations retire
one transfer per stream clock. No instruction-extension refetch or atomic
fallback is introduced.

`tms54xtra -bios rpt-mvdp` checks 9,600 cases/restores: five source forms
(DP, SP-relative, unmodified AR, postincrement, postdecrement), all eight
AR selections, counts 0/1/2/7/255, short and long immediate counts, two
instruction pages, ordinary and wrapping program destinations, both OVLY
states, and three snapshots. Read/write taps require exactly count+1
ordered transfers and one-clock accelerated retirement. The preceding core
fails the fixture at the repeated opcode's admission boundary.

This does not establish physical bus phase, memory-sourced counts, all
circular modes, count values outside the tested
domain, or self-modifying-code coherence. Absolute and long-offset Smem
remain forbidden in single-instruction repeat by the independent addressing
guard; standalone support must not weaken that guard. Repeat inside a block
or control-delay/XC composition remains outside this implementation.

`rpt-mvdp-irq` separately validates 288 cases/restores with IRQ and NMI,
counts 0/1/7/255, both immediate-count forms, two pages, three request times
and three snapshots. SPRU172C section 2.6 specifies that repeat Decode
disables all interrupts, including NMI but excluding reset, until completion.
The fixture requires every program write before acknowledgment, the exact
unexecuted continuation in RTN, a real PSHM/RETE ISR, a balanced stack, and
no duplicate transfers after restoration. The existing core satisfies this
contract without another timing adjustment or CPU change. Standalone MVDP
interrupt interaction and reset/HOLD during repeat remain separate questions.

Input pulses enter through MAME's scheduled `set_input_line` path. Raw
`write_stream` checkpoints must occur after the input synchronization has
reached the CPU, not inside the callback that merely enqueues the edge.
The fixture snapshots avoid those enqueue ticks; an undelivered scheduler
event is not evidence that the CPU failed to save a pending interrupt.

### MMR long-form boundary

SPRU131G section 5.6 separately excludes `*ARx(lk)`, `*+ARx(lk)`,
`*+ARx(lk)%` and `*(lk)` from MMR addressing. These are bytes `e0..f8`;
`f9..ff` are reserved. The shared guard rejects these bytes before operand
effects in the nine encoded LDM A/B, PSHM, MVDM, MVMD, STM, STLM A/B and
POPM classes. MVMM has its own compact operand grammar and is not covered
by this byte-oriented guard. Valid short MMR forms keep their page-zero and
AR-upper-bit-clearing semantics. Ordinary Smem long MOD=13 (`e8..ef` plus
offset) remains read-capable and must not be rejected by this MMR rule.

The CPU-only `source_admission` census retains 2,304 MMR guard tuples:
nine classes, 25 forbidden plus seven reserved bytes, both PMST-bit-14 inputs,
two pages and both frontends. Old checkpoints 1/12 are balanced over the tuples
instead of repeating every tuple twice. It requires no data/stack access,
AR update/truncation, accumulator/T, SP or status effect. The guard applies
to opcode words, not immediate extensions or annulled words. The old core
fails this fixture with an operand transaction. The corrected behavior is
unsupported-form rejection, not a hardware exception specification.

### Single-repeat transfer boundary

The coherent `bootrecmeta` metadata path includes `710f 0012` at `02:9cb5`:
an ordinary `MVDK TRN,AR2` before a younger repeat header, not a repeated
mapped move. The guard requires the latched repeat body's own address;
genuine mapped repeat bodies retain their exclusion. The firmware-free
`mapped_move_repeat_owner` qualifies both cases with cleared-pipeline replay.
This qualifies instruction admission, not physical pipeline timing.

SPRU131G sections 5.2.4 and 5.5.3.2 exclude absolute and long-offset
Smem operands from single-instruction repeats. This restricts the repeated
body, not an absolute-memory count operand of the repeat instruction itself.
It also does not prohibit a standalone extended transfer or a transfer in a
block repeat.

The six MVKD/MVDK, MVPD/MVDP and READA/WRITA classes reject operand bytes
`e0..f8` under an active single repeat before operand effects. `source_admission`
retains all 4,800 transfer tuples: six classes, all 25 offset/absolute bytes,
short and long immediate RPT counts 0 and 2, both PMST-bit-14 inputs,
two program pages and both frontends, with balanced checkpoints 1/12. It requires no data
or stack access, AR update/truncation, or accumulator/T/SP/status effect.
The old core fails with an operand transaction. This is fail-closed emulator
admission, not a silicon-trap claim. Other repeated operand families and
RPTZ setup/operand combinations remain outside this fixture's closure.

The former separate MMR/transfer profiles and replay paths are removed. Their
7,104 duplicate checkpoint executions are no longer covered. The original
54,392 census keys remain the exact prefix; appended keys identify the selected
old raw ordinals in separate namespaces. New cohorts retain high AR sentinels,
explicit PMST and complete prefix/body words, while using the shared full-width
accumulator/stack sentinels. Program writes and I/O accesses are also forbidden.

The staged C54x `bootrecstage` frontier is distinct from the atomic MU4
profiles below. It loads all 48,261 resident program and 3,656 compared data
words exactly, emits the original pin-decoded acknowledgement
`007f,0001,0055`, and enters the recorder routine. It also compares the
recorder overlay's 73,762 RAM destination words before execution (four
register words excluded) and enters original recorder startup at `0073f8`.
The paired copy witness records repeated MVDD (`e598`) at `03:8eda`
completing 128 ordered source reads from `0000..007f` into RAM beginning
at `0e35`. The current legacy observer does not renew that instruction-owned
read claim. Firmware then
publishes buffer pointers `1800` and `1880`. The zero source is shared with
the atomic control, not a demonstrated staged-only pointer defect; see
[Recorder low-page copy](#recorder-low-page-copy).

The stack-relative `7102 001a` at `03:e195` now transfers its captured word
to BRC before the later `RPTBD e1d2` at `03:e1aa`. Passive guest-access
taps observe source `5cee` with SP=`5cec`: 124 reads and matching BRC
publications across counts `6`, `0e`, `1e`, `3e`, `7e`. These observations
do not imply an adjacent producer/consumer hazard in the original sequence.
Execute-entry hooks separately observe 124 entries each at the move, RPTBD
and following instruction `03:e1d3`; this is not merely an advanced live PC.
The signed circular load `11df` at `03:dc39` and its following instruction
each have 49 observed Execute entries. Passive reads begin at `aa5a` and
advance by two with AR0=`2`, BK=`0`; no firmware state is injected.
In-block XC executes through `03:da71..03:da93`: Execute-entry hooks
observe `03:da85` (`XC 1,BLT`) and `03:da8f` (`XC 2,TC`) 1,168 times each,
and continuation `03:da94` twice. The alternate `03:da79` signed-XC route
has zero Execute entries in this run; its static presence is not runtime proof.
The delayed far return at `03:d95c` now reaches its original caller's
continuation `03:a1a9`; the former transfer into page `42` and eventual
`fe76` data execution are eliminated. Immediate-store forwarding also corrects
the subsequent interval count, so its DST zero-fill no longer wraps into the
register window. Software-saved repeat replacement/restoration now passes
the isolated interrupt fixture, and the original recorder advances past
`RPTB 03:faa5`. Accumulator SQUR (`f48d`) at `03:dbe6` also executes.
The internal BD (`f273`) at `03:da7e` and extended BANZ (`6ce0`) at
`02:a4cb` also execute. Absolute ADD `00f8 000b` at `02:9e9d` now
occupies the two-word delay span of FCALLD `03:dc44` at `02:9e9b`;
its source is mapped BL, not RAM. Memory SUB `08f8` at `03:d309` now
reads mapped BK (`0019`). Immediate store `76b2` at `03:df3e` now uses
`*AR2+0` addressing. FCALAD B (`f7e7`) at `03:b3a2` also executes.
Signed MAC `29f8` at `03:b305` now reads mapped BK (`0019`), and
two-word MAC `28f8 0015` at `03:cc06` reads mapped AR5 in the FCALLD
delay owned by `fb83 cb1e` at `03:cc04`. DLD `56e3 0010` at `02:a4d3`
also passes in both delay words of BD `f273 a4dc` at `02:a4d1`, inside
the `a4cb..a4dd` block repeat. Absolute immediate ST `76f8 0014 fdae`
at `03:c72c` now initializes AR4 through the early DAGEN-write path before
the following multiply consumes it. The five-second observation is free of
instruction exclusions. The subsequent ASM load at `03:a165` and its
adjacent `LD B,ASM,A` also execute after branch-delay admission.

The twenty-second empty-field control/save fixture executes without an instruction
exclusion and ends at `02:98ea`, ILLEGAL=`0`, IDLE=`1`. It processes the
ten-second control and fifteen-second poll/metadata traffic: input cursor
`50`, receive ring empty, seven responses and no pending acknowledgement.
The former NORM stop was downstream of incomplete merge-loop restoration;
the correction and its independent regression are documented below.

The empty-field metadata response has status=`00`, parameter=`07`,
reason=`03`, success=`0`. The operation finds `REL_001 .REL` and rejects
empty ID3 fields, not a missing file. Startup acceptance now uses a generic
instruction observer and still requires exactly one Execute entry at
`0073f8`, with captured opcode `fd00`. The retired program-read counter
reported two live-PC-qualified reads; a separate Execute breakpoint and the
new observer each report one actual entry. Ordinary reads, prefetch and
advanced live PCs are not instruction identity.

The separate `bootrecmeta` positive input supplies filename `TEST`, artist
`TEST` and title `MAME`. Updater `02:8385` and outer operation `03:8dc3`
return zero, and firmware sends parameter-7 status=`01`, reason=`0c`,
success=`1`. Absolute ADD `00f8 0018` at `03:a1cf` now reads mapped SP
under its independent arithmetic/producer contract. Post-save refresh
completes within the twenty-second window: PC=`02:98ea`, ILLEGAL=`0`,
IDLE=`1`, input cursor `58`, seven responses, no pending acknowledgement
and one reported track. Its internal startup, native metadata and dispatch
assertions pass without weakening the exact-one condition. The separate
staged cold-open fixture below rereads the saved catalogue and title from
NAND. File/audio completeness and playback remain open.

The active `bootplaystage` diagnostic sends an original playback request
through serial pins and loads the matching `RELSI16` image. Its 49,839
compared program/data words match original marker `aa99` before startup;
four low-window words remain excluded. Native execution passes mapped-SP
MVKD, the delayed far call inside block `02:6116..6120`, the shifted
store to AR5 at `02:5fdd` and `MVDK Smem,AL` at `02:6050`.
Pending MVKD writes now forward to same-address consumers, eliminating
the observed stack-index corruption and subsequent code overwrite without
changing the memory map. The two-word shifted store `6f93 0d9f` at
`02:41bc` now executes in the delay span of `RPTBD 41eb`; other delayed
control owners remain independently guarded. Memory-sourced `RPT Smem`
and repeated immediate/ASM-shift accumulator loads have independent
count, arithmetic, interrupt and replay coverage. Original playback now
passes the repeated `LD A,-1,A` at `02:5cf9` and the PMST store at
`02:9fae`, the long-offset PSHD at `02:a13d`, and long-offset
`POPD` (`8be1 0004`) at `02:a270`. At the 20-second observation, the
unaltered original playback run has PC `02:4e3c`, `illegal=0`, one actual
bootstrap Execute entry and 12 resident Execute entries. Its paired arithmetic
loop completes repeatedly. Correcting the DMA frame shadow removes the
out-of-buffer source walk and its misleading nonzero output. The current
boundary is why processing reaches its programmed reload tail with silent buffers and whether PCM
has the expected media provenance, not an
unsupported opcode. [Playback observations](mu4_playback.md#playback-execution-and-codec-observations)
also cover a fresh recording made with corrected DMA and MAC support:
it reaches `02:b5e1` without an illegal instruction but remains silent.
Thus the older recording is not the sole explanation for the zero output.
The two input artifacts and their observations remain distinct.
The separate varying-input recorder gate also passes, but playing its
file takes an earlier/different path: zero tracked arithmetic-loop entries,
twenty resident Execute entries and silent output, without an illegal
instruction. The next discriminating question is where this input's
decoder/read path diverges before the loop, not whether the constant
fixture alone should produce nonzero samples.
The playback observations
separate fetch hits, Execute entries and audio counts. The coverage diagnostic
still intentionally fails without audio acceptance. The generic 16-bit
repeat comparator retains the documented foreign-page hazard; delayed
calls do not acquire a hidden page-private loop controller.
This is a failed coverage diagnostic, not a playback acceptance or an
unavailable-ROM boundary.

The empty-field fixture observes 2,108 NAND program setups, 2,107 confirms and
1,098,240 data writes, but track count is zero and the recorded file is not
verified. Exact resident-memory comparison still passes (48,261 program
and 3,656 data words). These counts do not establish a complete recorder
transaction, audio output or handset boot. The positive fixture instead
observes 2,236 setups, 2,235 confirms and 1,165,824 data writes. Its captured
directory and ID3 text fields remain correct, and the reported track count
advances to one. Neither counters nor the successful protocol reply prove
file/audio completeness, playback or handset boot.

The passive observer records ILLEGAL explicitly: searching only for the
words "unsupported" or "illegal" misses some guard diagnostics. NAND
writes alone do not prove complete recording, and five seconds without
an instruction exclusion does not validate audio or handset boot.

Validated instruction and composition boundaries are collected below;
aggregate coverage is in [Core regression checkpoint](#retained-suite-acceptance).
Original-firmware probes are observation-only. Execute-entry evidence is
distinct from merely moving a Decode stop, and neither establishes complete
recording, validated audio or handset boot.

## Retained recorder execution contracts

### Repeated circular Smem ADD

`repeated_circular_add` qualifies immediate single-repeat ADD to A/B using
the four short circular addressing modes. Its 64 cases and replays cover
unit and AR0-strided increment/decrement, one/non-power-of-two/power-of-two
buffers, zero/short/multi-wrap counts and two program pages. They require
signed live-accumulator feedback, exact ordered reads one clock apart,
pointer/BK preservation and a separate continuation marker. The fixture
fails before circular ADD is admitted to the existing pipelined-repeat path.
Non-repeated address/arithmetic and repeat-controller controls remain
independent. This does not newly qualify CMPT aliases, OVM/interrupt
compositions, mapped operands or physical silicon timing. The acquired
6210 upload exercises the unit-increment form at `0d71`; no handset address
is present in CPU admission or the generic test.

`repeated_shifted_circular_add` separately qualifies the short shift-16 ADD
composition: all four source/destination accumulator selections, four circular
modes, wrapping counts and both program pages across 64 cases/replays. It
checks live source feedback, not an assumed destination-only accumulation.
Other shift-16 repeat address modes are not newly admitted.

### Circular MVKD destinations

`circular_mvkd` qualifies non-repeated fixed-RAM-source MVKD to the four short
circular destination modes. Its 64 cases/replays cover unit/AR0 strides,
increment/decrement wrap, power-of-two/non-power-of-two buffers, same-address
source/destination and two program pages. Ordered source Read and destination
Write are one clock apart; final RAM, pointer, BK, status and continuation
are checked independently. The fixture rejects before admission. This adds no
repeated MVKD, mapped source/destination or control-delay qualification.
The acquired upload uses this form at `0d98`, followed by the separately
qualified shifted repeat at `0d9e`; neither address is in CPU logic.

Fixture counts below describe focused acceptance. Shared broad-run scope,
tooling totals and native handset restoration are centralized in the
[core regression checkpoint](#retained-suite-acceptance).

### Direct interrupt return to a restored block repeat

The former NORM stop at `03:dad2` sampled T=`4421` from
`LD *AR5-,T` at `03:dacf`, reading `c423`. That word was an earlier
`DST A,*AR5+` output, later reused by an incomplete preparation pass.
The merge repeat `03:da71..03:da93` decrements AR5 by two per iteration;
its setup AR5=`c35b`, BRC=`0247` should leave AR5=`becb`. Instead one
exit left AR5=`c0f1`, BRC=`0113`: exactly 275 iterations and 550 words
short. The following full-length pass consequently consumed stale output.
This was not EXP latency: the wrong T came from incomplete preparation, not
the independent negative-count alias used by ROM4 Pow2. The producer uses CPL/SP-relative parameters, not literal AR3/AR4
reads. Seven seconds was too short to observe the stop; ten-second
debugger, observer-free and source-observer controls agreed on the failing
sample. Do not attribute that window difference to observer perturbation.

The recorder's first premature merge-loop exit follows common-window
`FRETE` at `1f18`, returning into `03:da85`. Firmware has restored BRAF,
RSA=`da71`, REA=`da93` and nonzero BRC, but the staged fetch controller
previously retained the ISR's completed loop. It therefore ran the rest of
one iteration and fell through. Ordinary near returns already reconstructed
that prediction; direct far returns did not. A debugger history rendered
these common-window instructions as zero words under logical page 3;
disassembling their actual page-zero backing identifies the restore and
`FRETE` instructions. Do not infer execution of zero-filled code from that
history rendering.

`body_repeat_restore` checks this reconstruction independently. The
return paths rebuild fetch prediction from software-restored
RSA/REA/BRC/ST1 before fetching the continuation: Execute for an ordinary
far return, Read for a delayed far return. No hidden loop-context stack,
firmware-address hook or shift-domain relaxation is introduced.

The independent fixture owns128 cases/restores: outer counts1/2/7/56,
ISR counts0/1/3/63, IRQ/NMI, both pages and FRETE/FRETED. Six checkpoints
are distributed across the decode boundary and completed bus prefixes during
save/inner-loop/restore/resume, not crossed with every input. The complete
ordered trace covers real source copies, hardware return frame, software MMR
saves, ISR loop and poisoned stack pops. Each return word equals live RTN
within the unexecuted-body range. Delayed FRETED reads the page before the
pending ST1 write; ordinary FRETE reads it later. This is observed model
ordering, not a physical bus-phase claim. A wrong BRC restoration fails at its
controller write, before the final pointer/memory checks.
It checks ordered source/destination words, exact saved register values,
seven balanced stack accesses in each direction, retained stack samples
after backing-memory mutation, and one resumed continuation. The ISR's
software saves/restores own the context; the test has no intervening near
return to conceal failed far-return reactivation. Physical silicon stage
placement remains unvalidated.

### ASM loads in branch delay words

`--profile asm-delay` passes 30,720 cases and save/replays: BD,
taken/untaken BCD and BD with an older source store; DP, SP, postincrement,
offset and absolute sources; all 32 ASM codes, positive/negative B,
SXM/OVM, two pages and three checkpoints. A one-word ASM load shares the
delay span with `LD B,ASM,A`; a two-word load fills the span and the
selected target performs that consumer before reporting the result.

The unchanged ASM execution path samples one source word in Read and
publishes only ST1's low five bits in Execute. The fixture mutates the
pointer after Access, the source after Read and the extension after Fetch;
it checks the following consumer's signed/logical shift, unchanged B/T,
unrelated status, pointer ownership and one selected target write. No CPU
state is added. Calls, interrupts, repeat composition and physical phase
placement remain outside this fixture's acceptance.

The original `3200` at `03:a165` follows BD at `03:a163` and precedes
`f682` at `03:a166`. Its stopped context has ST1=`6116` (CPL set) and
SP=`63d6`, so `$00` selects stack RAM, not peripheral zero or DP=`0f80`.
The preceding `STL A,$00` supplies that frame word. A literal opcode-zero
interpretation without the captured addressing status would misidentify
the missing contract.

### Constant DAGEN initialization

`--profile st-dagen` passes 3,840 cases plus 80 excluded repeat cases,
all with save/replay. Direct-DP and absolute ST target AR0..AR7, SP or BK;
four values, no older store/an older DAGEN store/an older RAM store,
two extra consumer gaps, two pages and four checkpoints are covered.
Indirect LD, PSHM and circular LD consume the initialized register.

SPRU131G table 7-4 classifies constant ST as an early DAGEN writer, with
the shared-bank deferral described in section 7.5.2. The retained operation
publishes once at its Access clock (5 for direct, 6 for absolute without a
prefix), representing the original opcode's Read with owned extension words.
The direct form behind an older DAGEN store defers one clock; the older RAM
store does not cause that deferral. The existing saved deferral flag owns it,
and Execute does not duplicate the write. These are frontend placements,
not measured silicon phases.

Tests poison both extensions after Fetch and check exact publication clocks,
consumer reads, pointer/stack changes and unrelated register/status preservation.
BK consumers have at least one gap word, increased to two for the bank-deferred
case so consumer Decode follows publication. The closer deferred-BK case
samples the old value in this frontend; its silicon behavior is unvalidated,
not an admitted new latency rule. The BK=1 fixture starts at `5002`, not
`5003`, so its starting index is inside the one-word circular buffer.
Repeated mapped stores reject before any register publication. Compiler-relative
aliases, CPL=1 stack behavior, interrupts and modifying indirect/control-delay
compositions need separate acceptance; the nonmodifying AR subset is below.

### Nonmodifying indirect constant-ST AR aliases

With standard indirect addressing, `ST #value,*ARn` can deliberately name
AR0..AR7 through their memory-mapped addresses `0010..0017`. This subset now
reuses the existing Access-time constant-ST publication and saved deferral
machinery. It retains the effective address before writing, so self-aliasing
cannot redirect the later store to RAM or cause a second publication.

`indirect_constant_ar_store` has 88 positive cases/replays: all eight target
registers, self/adjacent/AR4 pointer choices with duplicates removed, two
values, and ordinary or CALLD-delay execution. The CALLD cases include an
actual STM pointer producer before the call. An immediately adjacent indirect
load in the continuation consumes the new register as a RAM address; exact
register writes, stack frames and RAM reads are checked, along with unrelated
registers and four distributed snapshots. Five further cases retain rejection
without target publication for SP, BK, compatibility-mode, modifying-pointer
and repeated aliases. This is not a silicon-trap claim for excluded forms.
Older shared-bank conflicts reuse existing logic but are not independently
expanded by this alias fixture. The NSM-3 witness is constant ST at `5719`,
through AR4=`0015`, in a CALLD delay to `4384`.

### Direct STLM accumulator-field destinations

The direct MMR destinations AL/AH/AG/BL/BH/BG (`08..0d`) are admitted for
STLM from either accumulator. The existing Execute-owned store samples the
live source low word and updates only the destination field through the
ordinary register map; guard fields retain only eight bits. No second store
implementation, state member or arithmetic shortcut is introduced.

`stlm_accumulator_fields` has 96 cases/replays: six fields, either source,
four guard/low-word patterns and an optional immediately preceding LD
producer. An adjacent DST observes the changed accumulator; exact field and
RAM writes, full accumulator values, unrelated status and four distributed
save points are checked. These vectors use SST=0 and ordinary execution;
they do not newly qualify saturation modes, indirect accumulator destinations
or control/repeat/interrupt compositions. The NSM-3 witness is `STLM A,AH`
at resident `5c76` (`8809`).

### Direct STLM low-RAM destinations

Direct STLM also addresses ordinary low RAM `0060..007f`; this uses the
existing Execute-owned store and seven-bit MMR addressing, not the current
DP/CPL-selected data page. The admission adds no peripheral behavior or
second arithmetic implementation. Indirect non-AR destinations and new
repeat/control compositions remain unqualified.

`stlm_low_ram` checks 16 cases plus replay: both range endpoints, A/B sources,
DP nonzero with CPL off/on, and two program pages. It requires exactly one
correct-address bus write, unchanged accumulators/status/SP and retained RAM.
The test fails before admission. Existing register and peripheral STLM tests
remain separate controls. The acquired own-upload ROM6 witness at `17c9`
stores B to scratch word `0060`; the generic CPU does not special-case it.

### Signed MAC/MAS with a BK operand

`memory_mac` owns 15,616 cases/replays across MAC/MACR/MAS/MASR,
SQURA/SQURS, both destinations and 16 FRCT/OVM/SMUL/SXM combinations.
Full 64 T/memory pairs use indirect RAM; address-routing cases cover
indirect/absolute RAM, absolute AR1 and direct/absolute BK with four
representative pairs. Pages/checkpoints are distributed instead of the old
208,896 numerical/address cross-product executions.
The existing multiply-accumulate pipeline samples BK once in Read, preserves
T, and publishes the signed accumulation/subtraction with optional rounding,
saturation and sticky overflow. Poisoning BK and T after Read proves that
the operation retains its operands and does not overwrite their later values.
The non-destination accumulator, status and absence of data writes are checked.

Square operations alone publish T at Read. The exact Read, later operand/T
poison, Execute result/flags and retained extension are checked; no Writes
occur. NOP spacing preserves the original standalone mapped-square context:
an adjacent IDLE trips a separate control-composition admission guard.
The old factories/setup/result branches and two profiles are removed.
Admission is limited to the signed accumulating `28xx..2fxx` family;
other BK operand families, additional special registers, control-delay
composition and physical silicon phase placement are not established here.
The original recorder executes the BK MAC and reaches the separate
control-delay exclusion in [Current boundary](5510_bringup.md#current-boundary).

### Signed MAC/MAS in control delay words

`delayed_mac` owns 3,072 independent cases/restores: all four
signed accumulating operations, BD, taken/untaken BCD, CALLD, cross-page
FCALLD and RETD; short, absolute RAM, absolute AR5 and offset sources;
both destinations and sixteen T/memory pairs. FRCT/OVM/SMUL, page and three
checkpoints are assigned across interaction tuples rather than fully crossed;
the old numeric/control correlations are deliberately retired. Standalone
product sections own broader arithmetic contracts. Expected arithmetic independently rounds
the accumulation/subtraction, not the product. The conditional branch
decides from the original accumulator before the delay operation changes it.

The fixture mutates the address literal after Fetch, the pointer after
Access, and memory/T after Read. It checks one source read, retained T,
unchanged other accumulator, overflow/saturation and stack/page ownership;
the selected target copies the completed result. This reuses the existing
MAC execution path and complete delay-word accounting, with no new CPU state.
Interrupt arrival during these MAC delays, waited
memory and physical phase timing remain unvalidated.

`delayed_multiply` independently owns 768 ordinary MPY cases/restores with
the same six controls, four addressing forms, both destinations and sixteen
operand pairs. FRCT/OVM, page and checkpoints are assigned. Both sections
check captured Read operands/T, Access pointer state, post-capture poison,
Execute result/status, exact data ordering and final continuation/frame state.
For a short FCALLD delay, Read precedes the caller-page Write at the same
pipeline clock; extended forms publish that frame before the later Read.

### Absolute double-word store in RETD/RPTBD/CALLD delay words

`return_delay_absolute_dst` covers 24 independent cases/restores: either
accumulator, three absolute addresses (including an odd opcode-shaped literal
and the top RAM pair), and four in-flight save points. RETD's two delay words
contain one two-word absolute DST. The high word writes to the addressed
location, then the low word to its XOR-1 partner; the return continuation sees
both writes, with one stack pop and unchanged accumulators/status. This uses
the existing retained DST address/source and two-write path without new state.
`block_delay_absolute_dst` adds16 cases/restores for RPTBD's two delay words:
both accumulators,ordinary/top RAM pairs,count0/3 and two in-flight snapshots.
Exactly one ordered high/low pair is written before the separately counted
body;RSA/REA,BRC,continuation and pointer increments are checked. The native
ROM6 traffic initializer reaches RPTBD atb7a5 and absolute DST B atb7a7/3376.
`call_delay_absolute_dst` adds24 cases/restores for CALLD:both accumulators,
ordinary/odd-opcode-shaped/top RAM pairs,four snapshots and immediate versus
NOP-separated callee RET. Both halves and the balanced call frame are checked.
In the shared model clock,an immediate RET's independent stack Read precedes
the older DST writes;the target NOP separates them. This is a retained model
ordering,not a new measured silicon timing claim. Native CALLD atb835 targets
06b0 with absolute DST A atb837/2ad4 in its two delay words.
Other extended DST addressing forms and other delayed-control owners remain
outside this newly admitted composition. The native NSM-3 witness is RETD at
`0dfd`, DST B at `0dfe`, absolute destination `2f68` at `0dff`.

### Double-word loads in control delay words

`delayed_paired_load` owns 1,200 independent cases/restores over BD,
taken/untaken BCD, CALLD, cross-page FCALLD and RETD. Offset, preincrement,
circular and absolute forms cover eight AR selectors, offsets -1/+2 with
even/odd effective addresses and both destinations. Long-word patterns,
SXM/C16, page and three checkpoints are assigned across interaction tuples,
not fully crossed. Both compatibility states remain; absolute forms do not
duplicate an ignored AR selector. The standalone `paired_load` section owns
broader numeric/load semantics; this section does not renew the old full
numeric/control cross-product.
The first addressed word is the high half and its XOR-1 partner is the low
half; both are sampled once in Read. Sign extension follows SXM, and C16
does not change this load's final bit pattern (SPRU172C section 4-42).

The fixture poisons the extension after Fetch, all ARs after Access and both
source words after Read. It checks the full accumulator result and preserved
other accumulator/status/T, one pointer update, exact call/return frames and
the selected target's observation of the completed value. The existing DLD
execution path and saved pipeline operands are reused; no CPU state is added.
Interrupt arrival, waited memory, repeat composition and physical phase timing
are not established by these isolated fixtures.

### Software-saved repeat context

The retained diagnostic context has ST1=`e104` (BRAF set), BRC=`003f`,
RSA=`db51`, REA=`db56`, SP=`5cc2`. Its range is `03:db51..03:db56`;
it is not an observed inactive-BRAF case. The saved-context witness below
establishes software restoration, not an automatic hardware loop stack.

The outer `RPTBD` at `03:db4d` repeats `03:db51..03:db56`, with a delayed
conditional call at `03:db53` to `03:dada`. Its return address is `db57`,
one word beyond REA. The observed interrupt enters logical `03:206c`, whose
common-window vector saves XPC and branches to `03:9528`; RTN is `dadc`
and BRC is `0037`. Thus the interrupted instruction is in the called leaf,
not directly in the loop body.

The ISR saves BRC/RSA/REA at `03:954d..03:954f` and restores them at
`03:9568..03:956a`. The subsequent common-window gateway also saves the
repeat context before dispatching application callbacks: `1ef7..1efa` selects
MMRs `0019..001d` and repeats `PSHD *AR7+` five times. Passive Write taps
observe BRC=`0037` at `5cd6`, RSA=`db51` at `5cd5`, REA=`db56` at `5cd4`
and PMST=`202c` at `5cd3`; these words remain intact at the exclusion.
`1eff..1f06` provides the reverse `POPD` sequence, and ST1 is popped at
`1f16` before FRETE at `1f18`. This restores BRC before BRAF, as required
by [TI SPRU131G section 6.10.6](https://www.ti.com/lit/ug/spru131g/spru131g.pdf).
Section 6.8 establishes that there is only one physical set of repeat
registers; software must save outer contexts for nesting.

The callback reaches `03:fa5e` with the outer BRAF still set and later
publishes its own BRC=`003f` before `RPTB 03:faa5`. An outside-body RPTB
now replaces the one loop controller; a near return rebuilds its fetch
prediction from restored architectural BRAF/BRC/RSA/REA, even after an
inner loop has finished. There is no automatic context stack.

`saved_repeat_restore` owns320 independent cases/restores: five outer counts
(`0,1,2,7,56`), four inner counts (`0,1,3,63`), IRQ/NMI, two pages, two
stack alignments represented, FRETE/FRETED and six distributed bus-prefix
checkpoints. Stack/snapshot positions are not crossed with every input. Physical input interrupts
an outside-body CCD callee. The ISR saves repeat/status registers, executes
its own loop and restores the saved words; FRETE returns to the leaf, whose
near return targets either the body or REA+1. Ordered copies, balanced
frames, retained stack operands and final register state are checked. The
synthetic instruction program is CPU-only; this is not a complete recording
or a claim that every original outer-loop iteration has settled.

Companion sections are `active_repeat_context`, `active_repeat_pmst` and
`active_repeat_popd`; remaining isolated controls include `repeat-rearm`,
`block_conditional_call` and `rptbd-irq`.
Their renewal is not implied by a default run; aggregate scope is maintained
in [Core regression checkpoint](#retained-suite-acceptance).

### Accumulator SQUR

`accumulator_square` retains 512 cases/restores: two destinations, 16 A operands,
two opposite-sign B sentinels and eight FRCT/OVM/SXM combinations. Pages/four
checkpoints are distributed rather than multiplied. B is never a multiplicand; the
14 other B values are retired, not claimed as retained numeric coverage.
The sentinels `123456789a`/`edcba98765` distinguish B preservation for an A
destination from overwrite for a B destination, and neither equals any
supported square result. The independent expectation squares signed
A(32-16), including `10000` (-65536) and `0ffff` (+65535) high-part cases
that a signed 16-bit AH implementation gets wrong. Tests check Execute
operand publication, saturation, sticky destination overflow, unchanged T
and ST1, preserved non-destination accumulator and no data-memory access.
The implementation reuses the existing multiply-result contract. Other
producer compositions and physical pipeline phases are not established by
this fixture. Original recorder execution now passes SQUR and reaches the
internal BD at `03:da7e`.
The shared runner checks observations before the explicit tick5 operand
writes and captures checkpoints afterward. Cases own expected arithmetic;
the runner owns only the CPU clock, optional bus observers and snapshot
continuation. Each case has explicit setup; no handset device is imported.

### Internal block branches

`block_branch` owns 384 independent cases/restores: counts 0/1/2/7,
four zero/nonzero patterns, diamond/endpoint layouts, A/B, pages 0/2 and
none/IRQ2/NMI. Each case checks the complete ordered data trace, selected
stores, BRC and BRAF at source Read, both accumulator results and balanced
RTN-bound interrupt frames. A later ordinary branch replaces held tags
before replay. Eight snapshots are distributed. Physical interrupts are
asserted on the first selected store; the former four crossed arrival-clock
values are no longer a coverage claim. Depending on the selected arm, the
next source Read and an issued store can precede the hardware frame.
`block_branch_limits` independently owns seven unsupported control shapes
on both pages, with no data transactions and no speculative body/frame write.
These are implementation-limit tests, not silicon rejection requirements.

### Unconditional delayed branch in a block

`f273` is two-word BD, not RETD. The original instruction at `03:da7e`
targets `03:da8b`, with delay instructions at `03:da80..03:da81`; all
belong to the repeated range `03:da71..03:da93`. Admission now includes
unconditional BD when its target is inside that block and both delay words
end strictly before REA. The existing Access-owned target redirect already
rebuilds the fetch count from Decode-owned BRC; no separate redirect or
saved state was added.

`internal_delayed_branch` owns 128 independent cases/restores across counts
`0,1,2,7`, four source patterns, both accumulator delay consumers, one
two-word or two single-word delay instructions and two pages; four snapshots are distributed.
It checks ordered source reads, selected writes, exact iterations and delay
effects. `block_branch_limits` retains 14 exclusions, with its delayed-control case
now testing a BD delay span at REA rather than an admitted internal BD.
The original recorder passes this branch and stops at extended BANZ
`02:a4cb`. Interrupt composition and measured physical phases remain outside
this fixture's claim.
The conditional, nondelayed and backward block-branch profiles, exclusion
profile, saved-repeat and SQUR profiles pass as controls.

### Long BANZ in a block

The original `6ce0 fffc a4d5` at `02:a4cb` tests the resolved `AR0-4`
address without a data-memory read and selects a buffer path inside the
repeated range `02:a4cb..02:a4dd`. Nondelayed BANZ now has the same
bounded admission as B/BC: the complete instruction must end before REA
and its target must remain inside the block. A taken BANZ also resets the
speculative fetch count from Decode-owned BRC after discarding fall-through
words; AR modification remains owned by the existing address logic.

`internal_banz` owns 192 independent cases/restores: three long forms
(offset, preincrement, circular), eight initial/offset pairs, outer counts
`0,1,2,7` and two pages; four checkpoints are distributed. The fixture computes
linear wrapping and modulo-32768 circular updates without CPU address helpers. Selected-arm writes and sequential
source reads occur once per iteration, AR0 updates occur once per branch,
and taken/untaken transitions follow the computed address rather than the
loaded data word. CMPT, interrupts and physical phases remain unvalidated
for this composition. The standalone long BANZD, internal BD/BCD, branch
exclusion, forward/backward branch and saved-repeat profiles pass as controls.
Original execution advances to the separately mapped FCALLD/absolute-ADD
delay sequence above; this is not full recorder acceptance.

### Absolute ADD in a control delay

Absolute ADD `00f8/01f8` uses two words and may occupy a complete
two-word control-delay span. The original `00f8 000b` at `02:9e9d`
adds BL to A before the far-call target `03:dc44` observes A. The existing
Access address capture, Read operand and Execute arithmetic implement the
operation; admission now counts both words and retains the delay-span bound.
Control/peripheral sources outside the admitted register set remain excluded.

`delayed_absolute_add` owns 3,072 independent cases/restores across BD,
CALLD, FCALLD and RETD; RAM, AR3, AL, AH, BL and BH sources; both
destinations; four operand/initial patterns; SXM/OVM. Pages and three
snapshots are distributed. Aliased accumulator fields are included in the independent
arithmetic expectation. Carry, sticky overflow, saturation, preserved T/ST1,
one source read, one address-literal fetch, owned call/return frames and the
target's observed result are checked. RAM is replaced after Read to detect
rereads; mapped accumulator inputs are not overwritten after Read, since
that could also change the Execute-owned destination. Conditional controls,
adjacent producer ordering and measured physical phases are not established
by this fixture. Original execution passes the sequence and reaches the
separate mapped-BK SUB exclusion at `03:d309`.
Absolute ADD/BK controls, both shared delay-load profiles and their exclusion
profiles, and internal BD/BANZ pass as controls.

### BK as an ordinary arithmetic operand

BK (`0019`) is admitted as a side-effect-free 16-bit source for ordinary
ADD/SUB, matching the existing absolute-ADD BK contract. This does not
admit SP, repeat-control or peripheral registers, or other arithmetic families
whose mapped inputs remain excluded. Existing Read capture and arithmetic
helpers own the operand and result; no synthetic register value is supplied.

`--profile arith-bk` passes 1,024 cases and save/replays across ADD/SUB,
both destinations, direct/absolute forms, eight input patterns (including
zero BK), SXM/OVM, two pages and two checkpoints. Changing BK to `2222`
after Read does not alter the result. Source-read count, carry, sticky
overflow, saturation, untouched accumulator and absence of data writes are
checked. `--profile arith-ar` separately reproduces the existing 1,024-case
auxiliary-register control through the same fixture. Absolute-ADD/BK,
delay-ADD, BK move/store and internal BANZ controls also pass. The original
recorder passes its SUB and reaches the immediate-store exclusion above;
native recording and physical pipeline phases remain unproved.

### AR0-indexed immediate stores

Immediate ST admits short `*ARn-0` and `*ARn+0` modes, reusing Read-owned
address capture and AR modification. Its two-word Decode retains the literal
for Execute; accumulator saturation and later accumulator changes do not
select its data. An indexed destination below `0060` is rejected before
pointer modification or a data write. This does not change the established
direct/absolute immediate-store register contracts. Reverse-carry and circular
short immediate-store forms remain outside this admission.

`--profile imm-index` passes 98,304 cases and save/replays, including 96,000
RAM stores and 2,304 low-page exclusions. The matrix covers eight AR selectors,
CMPT and AR0 aliasing, eight signed/wrapping indices, both directions, four
literals, four accumulator patterns, SST/SXM, two pages and three checkpoints.
Pointers change before and after Read, the literal is replaced after capture,
and accumulators change before Execute. Checks require one literal/opcode fetch,
no destination read, one correct write, preserved status except compatibility
ARP and no pointer update on exclusion. Producer compositions and physical
pipeline phases are not established. Original firmware passes this store and
reaches the separate FCALAD B exclusion above.
Indexed accumulator-store/load and MAR, immediate forwarding, store-delay,
mapped-BK and saved-repeat profiles pass as controls.

### Delayed immediate far call

`delayed_far_call` owns 81 cases/replays: caller and target pages 0/2/7f,
three complete two-word delay layouts, and no interrupt/IRQ/NMI. Layouts are
NOP+STLM BRC, a two-word STM T, and NOP+LDM AR6. The former unasserted
mapped-XPC read probe is replaced by NOP; its visibility remains unqualified.
Exact ordered data/program events cover both call-frame words, delay traffic,
callee/continuation fetches, IRQ stack/RETE and FRET roundtrip. The two-word STM
publishes between the call-frame words in this model; IRQ acceptance still
requires the complete frame and delay traffic. The pending entry drains to the
callee FRET boundary before stacking its continuation. These are model contracts,
not silicon timing measurements. Three snapshots are distributed instead of
expanding to 243 executions. The old profile, phase handlers and IRQ hook go.

### Delayed far accumulator call

SPRU172C page 4-55 specifies one-word FCALAD A/B: save PC+3 then
caller XPC, transfer through source bits 22-0, and execute two following
words. Its Class 30B delayed cadence is four cycles, versus six for
nondelayed FCALA. Neither form may be repeated or placed inside RPTB.

Near CALAD (SPRU172C page 4-25) uses the same retained two-word delay span
and PC+3 return, but takes only source bits 15-0 on the caller's page and
pushes no XPC word. It now shares the delayed accumulator-transfer path.
`accumulator_calls` retains CALA A/B and CALAD A's independent target,
frame, delay-overwrite, return and replay checks. RET's Read and its preceding
literal store's Execute are observed in that order within their shared clock.

The staged model samples the 23-bit target in Read, redirects Prefetch and
discards the already fetched continuation after the delay span. PC+3 publishes
in Read and caller XPC in Execute, without the nondelayed held clock. Two
single-word or one two-word delay instructions retire before IRQ/NMI can
enter. Existing target and per-slot frame fields supply save-state ownership;
no additional CPU saved state or automatic context stack is introduced.

`delayed_accumulator_far_call` owns108 independent cases/restores: both accumulators,
three caller and three target pages, both delay layouts, no interrupt/IRQ/NMI
and six distributed checkpoints, rather than crossing every checkpoint with
every input. After target sampling the source accumulator is changed;
delay instructions and callee observe the changed data, but control still
uses the captured target. Checks cover PC+3/XPC publication, a balanced
FRET, a separate interrupt frame, delay effects once, target execution once,
and preserved flags/non-source accumulator. The nominal model has target
Fetch at clock6 and callee Execute at10, with frame writes at5 and6. For this
input timing, the callee store precedes IRQ entry and the target is fetched once;
that is not a universal interrupt-refetch claim. Cross-space observation checks
the first frame word before target Fetch and the second after it. The clocks
these are declared placements, not measured silicon bus phases. Adjacent
instruction-producer composition and waited memory remain separate contracts.
Original execution passes FCALAD and reaches the mapped-BK multiply exclusion.
Its captured sequence targets `03:b2ae`, with NOP and `LDM AR1,A` in the
caller's delay words. The callee starts with `PSHM AR1` and `FRAME #-7`.
Observed caller SP `6562` therefore becomes `6558` after the two call words,
one saved register and seven local words, matching the new stop. This is
coherent entry evidence, not completion of that callee or the recorder.
SP-modifying delay words are not covered by the isolated fixture.

`tools/mu4_repeat_context_observe.lua` reproduces the read-only witness on
`mu4nand -bios bootrecstage`: add `-debug -debugger none`, the absolute
`-autoboot_script` path, `-autoboot_delay 0`, `-log` and `-nonvram_save` to
the retained-NAND command below. The tool exits after five emulated seconds.
Require the positive outer/vector/copy-entry records and saved-word records;
MMR event PCs describe live context, not necessarily the owning pipeline slot.
This is a manual frontier instrument, not a passing nested-repeat gate. Retire
it when an isolated composition fixture and original resumption proof replace it.

### Immediate-store forwarding

[TI SPRU131G section 7.3.2](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
specifies an internal-bus bypass when an on-chip read conflicts with a pending
write to the same location. The retained RAM-read path now includes immediate
stores, taking the literal already captured by Decode. Forwarding and the real
Execute write share the store-word helper. The real Write is not moved and
register/peripheral addresses remain outside this bypass.

`imm-forward` passes 6,912 cases/restores: direct, postincrement and absolute
stores; LD A, ADD A and LD B consumers; four literals, SXM/OVM combinations,
DP/CPL, two pages, adjacent/NOP-separated and same/different addresses.
Snapshots bracket Read and Write; modifying the already fetched literal
cannot change either the forwarded value or real write. The test checks one
pointer update and write, preserved ST1, consumer results/flags and real reads
only for separated or different-address controls. The expanded fixture fails
on the preceding core; the separate 768-case RMW-forwarding control passes.
IRQ/repeat composition, mapped/peripheral forwarding and measured bus timing
are not established by this matrix.

In original recorder code, `7602 ffff` at `03:d961` stores `ffff` to stack
word `6576`, immediately followed by `ADD *SP(2),A` at `03:d963`. Previously
the ADD read stale zero before the real `ffff` write, leaving A=`31` instead
of `30`. That count caused an extra interval: its delta was `-0400`, then
`fbff` after subtracting one, producing a huge BRC count. The later
`DST B,*AR4+` at `03:d9af` eventually wrapped to the register window;
it was not evidence for relaxing the long-word register-pair guard.
With forwarding, two observed invocations publish A=`30` and finish their
zero-fill loops, reaching `03:d9b2`. Passive Write taps observe contiguous
spans `9e82..a681` and `ae2e..b62d`, each 2,048 zero words under this fixture.
The register-window exclusion remains unchanged. This advances execution,
not recorder-output acceptance.

### Delayed far returns after stack restoration

FRETD/FRETED must consume the stack after an older POPM/POPD's Read-owned
SP update, just as near returns do. The retained delayed return's first
sample remains Access-owned, but is ordered after the older stack operation;
the second sample remains in Read. No new pipeline state or atomic fallback
is introduced. Exact DA150 bus phases remain unmeasured.

The default native fixture covers 72 cases/restores: FRET/FRETE and their
delayed forms, two pages, two single-word or one two-word delay instruction,
and 36 preceding-POPM compositions. It checks SP, restored AR1, INTM, ordered
XPC/PC reads, target Fetch, delay effects and replay around both stack samples.
The expanded fixture fails on the preceding CPU implementation.
The native 5110 save-state gate, original resident-memory comparison and
154 runner unit tests also pass; none establishes complete recorder output.

In original recorder firmware, the `03:d8bb..03:d95e` function saves three
registers and allocates 20 local words, then restores them before FRETD at
`03:d95c`. Its caller is `FCALL 03:d8bb` (`f983 d8bb`) at `03:a1a7`,
whose continuation is `03:a1a9`. Passive stack taps observe that `0003,a1a9` frame
at `657a..657b`. Consuming the preceding saved-register slot as XPC instead
redirected execution to `42:a1a9`; subsequent execution through unmapped
pages eventually stopped on `fe76` at `02:6609`. That word is not evidence
that RCD is the next required instruction: the acquired aa88 record marks
`6604..6613` as data (original word `6609=beef`), outside the aa22/aa88
program spans. Debugger history records logical Execute PCs, but disassembles
current raw program memory; apply the established OVLY projection separately
for common-window instruction bytes.

### Circular Fixture Domains

[SPRU131G section 5.5.3.4](https://www.ti.com/lit/ug/spru131g/spru131g.pdf),
printed page 5-16, requires a base aligned to the smallest `2^N` strictly
larger than BK, an initial index inside the queue, and a step magnitude no
greater than BK. The old paired generator used BK=16 with offset `f495`
(-2923), and with AR0 self-index steps around 4103. Those inputs were
modulo-model stress cases, not documented nonzero-BK hardware-domain cases.

The paired generator now carries its initial pointer and BK explicitly.
Long circular forms use initial `4007`, BK=8192 and aligned base `4000`;
the same opcode-shaped negative offset exercises a legal wrap to `549c`.
Short circular AR0 self-index forms use BK=8192 so the pointer-derived step
fits the buffer; other short circular forms retain BK=16. An independent
modulo oracle first checks the documented domain, stopping on invalid
fixture inputs rather than asserting a silicon trap. Fourteen host-side
guard controls cover invalid size/step/index combinations, signed wrap and
power-of-two mask boundaries; they are not executed DSP instructions.
BK=0 behavior and physical memory-bank/timing details are outside this pass.

The independent `paired_indirect_store` section owns 4,096 cases/restores
across all sixteen short/long address forms, eight encoded registers, A/B,
four accumulator patterns, saturation and compatibility modes.
`paired_indirect_load` owns 7,680 cases/restores across fifteen admitted
forms, eight registers, A/B, four 32-bit patterns, SXM/C16/compatibility modes.
Pages and three replay boundaries are distributed instead of crossing each
input with checkpoints. Every AR is initialized explicitly; unrelated ARs
remain unchanged. Load-source rejection belongs to `source_admission`.

Both sections retain odd-address ordering (second word at address XOR 1),
linear/preincrement/reversed/circular pointer updates and instruction-shaped
long literals. Pointer/BK poisoning follows Access; destination reads and
unexpected data accesses are forbidden. DLD reads both words at clock 5
(6 long), preserves accumulators through Read, publishes at Execute, and
ignores subsequent RAM poisoning. DST captures before source poisoning,
writes at 6/7 (7/8 long), and checks final RAM after replay between writes.
These are CPU-model timing contracts, not measured silicon. Run each with
`python3 tools/run_c54x_section.py paired_indirect_load` or
`python3 tools/run_c54x_section.py paired_indirect_store`.

Scalar load sections configure BK consistently with their circular address
expectations. Short indexed circular forms start all ARs at `5007`, so AR0 supplies a signed
step of that magnitude even when another AR is selected. Those forms and
long circular forms use BK=32768, aligned base zero and initial index
`5007`: both the AR0-derived step and every signed 16-bit literal fit the
buffer. Unit-step circular forms retain BK=16. The modulo oracle computes
the indexed pointer update and long circular address within that domain.
Independent product/load sections own their explicit address contracts;
the retired shared scalar descriptor is no longer an execution dependency.
These fixtures do not establish physical memory-bank placement or timing.

Current acceptance belongs to the independently executed sections and their
disposition entries, not the retired profile chain. Other legacy users of
the circular fixture helper remain pending migration; their historical
profile runs are not acceptance for the replacement suite.

Long BANZ/BANZD and repeat-block BANZ also carry BK through their shared
fixture. Circular forms use BK=32768 with initial indices below that size;
linear forms retain their 16-bit wrap inputs. Circular zero/nonzero branch
cases include positive wrap (`7fff + 1`), negative wrap (`0234 - 2923`)
and a full-buffer negative step (`0 - 32768`). The repeated-case oracle
checks the domain on every iteration, not just the initial pointer. No
operand memory access is introduced. Signed offsets, raw case IDs, delayed
instruction layouts and replay checkpoints are retained.

The existing `banzd-long` profile covers ordinary BANZ before BANZD, without
adding a BIOS slot; its runner requires both 2,304-case and 4,608-case
markers. `internal_banz` separately covers 192 independent cases at repeat counts 0/1/2/7.
Signed and unconditional internal block branches have separate sections.
The runner rejects missing or duplicate ordinary/delayed completion markers.

Immediate-long and accumulator-long store fixtures carry their selected
initial pointer and BK too. Circular forms use `4007`, BK=8192 and base
`4000`, preserving the signed `f495` literal as a legal negative wrap to
`549c`. Noncircular forms retain `1007` and BK=16. Shifted stores, long
MVDK/MVKD and long RMW inherit the same address contract; their initializers
set only the selected pointer to the circular start, preserving unrelated
AR values and alias checks. MVDK's retained-BK assertion uses the fixture's
BK unless the instruction explicitly writes BK.

The legacy `long-mvkd` bundle is not current executable acceptance during
section migration. Ordinary/shifted STL/STH, MVKD and long RMW now have
independent sections. Their former shared address factory has no remaining
consumers and is removed, along with its descriptor and coverage schedulers.

These corrections do not constitute an exhaustive ISA or fixture-domain
audit. Other older generators' modulo expectations remain insufficient
evidence of documented input legality.

### Signed circular memory loads

Signed LD admits the four short circular modifiers `c0..df`, using the
existing retained address-generation and load paths. BK is sampled in
Decode; the source address, AR0 index and pointer update belong to Access;
the word is sampled in Read and published to the selected accumulator in
Execute. Low mapped/peripheral sources remain excluded before reading or
updating the source pointer. No address-generation arithmetic was changed.

`ld-circular` passes 393,216 cases/restores: 368,640 RAM cases and 24,576
mapped-source exclusions; four modifiers, eight encoded registers, both
compatibility states and destinations, four source words, eight SXM/OVM/SST
states, two pages and three snapshots. BK covers 0/1/7/16, with 8192 used
for valid RAM-pointer self-indexing. Post-Decode BK changes, post-Access
pointer/index changes and post-Read RAM changes cannot retarget the load or
repeat its effects. Ordinary indexed-load coverage remains a separate gate.

Nonzero buffers obey [SPRU131G section 5.5.3.4](https://www.ti.com/lit/ug/spru131g/spru131g.pdf):
aligned base, initial index inside the queue, step magnitude at most BK.
The independent expectation uses modulo, while the CPU uses iterative
wrapping; values outside that documented domain are not a conformance oracle.
BK=0 retains the independently exercised linear-update contract described in
[ROM4 codec validation](djr_dsp_integration.md). Generic IRQ/NMI, delay/repeat
composition and physical bus timing are not established by this isolated
matrix; the recorder's 49 executed iterations are separately scoped evidence.

### Memory transfers into the block counter

SPRU131G table 7-24 gives MVDK/MVMD-to-BRC zero latency before RPTB[D].
The retained frontend reads the source and publishes BRC at Access, before
younger Decode initializes the repeat. The move does not inherit DAGEN-bank
write deferral. A preceding mapped accumulator write uses the existing
late-sample path rather than a stale mapped read. No new save fields or
atomic-interpreter fallback are introduced.

`move-brc` passes 960 cases/restores: DP, SP, postincrement, absolute,
long-offset, MVMD AR4, MVMD AL and MVDK AL sources; counts 0/1/2/7/127;
adjacent/NOP-separated RPTB/RPTBD; two pages and three snapshots. The
source is read once, BRC writes once without a destination read, and the
following loop copies exactly count-plus-one words. Mutating the source
after capture cannot change the loop count. The mapped-source cases do not
independently establish every preceding accumulator-producer composition.

In-loop BRC modification (table 7-25), unsupported repeat-owner composition,
IRQ/NMI composition and physical bus timing remain unvalidated. Rejection of
these combinations is an implementation limit, not an asserted silicon trap.
The supported delayed-transfer coverage is owned by `move-brc-delay`.

### Shifted memory loads in control delays

The original `6f00 0c5f` at `03:d01d` is a two-word memory load with
encoded shift -1 into A. It fills both delay words of `BCD d036,AEQ`
at `03:d01b`. The retained Decode span now admits short shifted-load
forms and validates the load extension before Access; a store/arithmetic
extension does not inherit admission merely by sharing the first word.
The existing source Read, encoded shift and Execute result are reused,
without an atomic-interpreter fallback or additional CPU save fields.

`delayed_shift_load` owns 1,152 independent cases/restores for BD, taken/untaken BCD,
CALLD, FCALLD and RETD; DP/plain-indirect/postincrement sources; A/B;
all 32 shifts. Source patterns, SXM/OVM, pages and three snapshots are assigned
across interaction tuples, not fully crossed. Standalone shifted-load sections
own broader numeric contracts; the old numeric/control correlations are not
renewed here. Replacing the fetched extension, source memory and address
registers after capture must not change the result or duplicate updates.
The selected continuation observes the completed load and the original
call/return frame, while a conditional owner retains its earlier decision.
Mapped sources, SP-specific call/return
addressing, three-word forms, IRQ/NMI composition and physical phase
timing are not established by these fixtures.

### ASM-controlled accumulator stores

`8793` at `03:e0c9` is `STH B,ASM,*AR3+`; `84b5` at `03:e0e7` is
`STL A,ASM,*AR5+0`. The frontend admits short DP/SP/basic-indirect and
AR0-indexed add/subtract forms, with RAM destinations only. The existing
Read-owned address update retains ASM before older Execute publications;
the accumulator and SXM/SST handling remain Execute-owned. Dedicated
ASM loads forward their publication to meet SPRU131G example 7-52's
zero-gap contract. Ordinary ST1 writes do not receive that forwarding
(table 7-22). This is a partial stage model, not measured bus timing.

`asm-store` and `asm-store-ld` each pass 122,880 cases/restores: four
STL/STH A/B selectors, all 32 shifts, eight accumulator patterns, four
SXM/SST states, DP/SP/postincrement/indexed-sub/indexed-add forms, two
pages and three snapshots. The second profile prefixes an immediate ASM
load without a gap. After Read, ASM/address/index mutations must not
change the write, while accumulator mutation must affect it. Independent
shift/saturation expectations check one Execute write and no data reads.
The indexed composition uses AR5 and AR0=3; the broader ordinary indexed
store matrix remains a separate address-generation regression. Final-tree
store-index/bypass/preincrement/delay and immediate-ASM/block-branch
controls pass separately. Long-offset, preincrement, reverse/circular ASM
store forms, mapped destinations, general ST1-producer latency and
interrupt/repeat/delay composition are not established by these matrices.

The Execute write and pending-write bypass share one store-word calculation,
including the retained ASM shift, live Execute accumulator and SXM/SST
handling. `asm-store-bp` passes 24,576 cases/restores over four selectors,
all 32 shifts, four SXM/SST states, both unsigned-load destinations,
adjacent/NOP-separated consumers, same/different addresses, two pages and
three snapshots. Changing ASM after Read cannot change either publication.
The adjacent alias suppresses the backing read while the real store still
writes once. This fixture failed against the earlier unshifted bypass even
though the actual RAM write was correct. Ordinary and Xmem bypass controls
also pass; explicit-shift Smem stores, bank contention and stack ordering
remain outside this ASM composition scope.

### Immediate ASM publication

`ed1e` is `LD #-2,ASM` (SPRU172C 4-70/4-71). All 32 encoded signed
five-bit values update only ST1.ASM in Execute, using the retained opcode
without a data access or atomic-interpreter fallback. SPRU131G example
7-52 requires zero latency to the following ASM consumer; the existing
Read-after-older-Execute ordering supplies that visibility.

`asm-immediate` passes 9,216 cases/restores for LD/ADD/SUB consumers,
all 32 shifts, two accumulator patterns, four SXM/OVM states, zero/one
intervening NOP, two pages and three snapshots. Replacing the opcode
after Fetch must not alter its immediate. Per-clock checks cover ASM,
other status, accumulator results and absence of data transactions.
The memory-sourced `asm-consumer` and `asm-mapped` profiles
also pass separately. This does not establish every immediate-ASM
store consumer, interrupt/delay composition or physical phase timing.

### Delayed internal block branches

The original `BCD c85e,AGEQ` at `03:c852` has delay words `5692` and
`578a` at `03:c854..c855`, entirely inside the block `03:c851..c862`.
Both delay words execute regardless of the captured predicate. Admission
is limited to signed accumulator conditions with an internal target and
the complete delay span strictly before the endpoint. A taken redirect
restarts speculative Fetch accounting from the Decode-owned BRC without
consuming an extra iteration; it does not reset the architectural count.

`internal_signed_branch` owns 768 independent cases/restores: 12 signed A/B predicates, counts
0/1/2/7, four source patterns, two one-word delay operations or one two-word
operation and two pages; four snapshots are distributed. It checks ordered source reads,
one selected-arm write per iteration, both delay words, final pointers,
status and count. Delay arithmetic can change the tested accumulator
after its decision; source RAM changes after Read cannot change the
captured word. Existing nondelayed/negative/backward block-branch,
delayed-call and interrupt-loop profiles pass separately. This fixture
does not establish its own IRQ/NMI composition, endpoint-overlapping delay
spans, outside-block targets or physical phase timing.

Passive original Execute counts are 510 at the branch, 510 at its first
delay address, 8,702 at the second delay address and 510 at the join. The
extra second-address visits are not classified by this observer; these
totals must not be interpreted as a paired per-branch execution trace.

### Reverse-carry MAR

`6dbb` is `MAR *AR3+0B`. SPRU131G 5.5.3.5 defines AR0-indexed
reverse carry/borrow for MOD 7/4; FFT use specifies a power-of-two AR0.
The staged frontend admits both reverse forms through the existing
Access-owned auxiliary update, using the shared addressing helper without
adding a data-memory transaction or an Execute-time update.

`mar-reverse` passes 3,072 cases/restores across add/subtract, eight
auxiliary registers, both compatibility states, eight power-of-two indices,
two initial layouts, two program pages and three snapshots. Expected
addresses use an independent MSB-first bit-serial carry/borrow oracle,
not the core's reverse/add/reverse implementation. Mutation after Access
checks that AR0 and the selected register are not consumed again. AR0
self-aliasing, compatibility ARP publication, accumulators/T and remaining
status are checked; no data reads/writes or extension fetches occur.
Non-power-of-two index meaning, producer latency, interrupt composition
and physical phase timing are unvalidated. The existing 3,072-case
`mar-index` profile also passes on this source.

The `reverse_carry` section now runs staged for MAR, LD and STL: 2,688
independent bit-reversal/address/result cases plus two full 16-position
traversals. LD retains its Access-owned address/update and STL its Read-owned
address/update; neither repeats the modification at Execute. Newly admitted
reverse-address LD/ST forms reject mapped/low-page operands. The two explicit
traversals retain their original register snapshots and ordered bus events.

Short indirect MAR also uses the pipelined single-repeat engine: each injected
iteration modifies its pointer once in Access, with count retirement owned by
Execute. `transfer_repeat_literals` retains all 256 eight-bit counts
for postincrement AR1; `conditional_repeat` retains AR3 postincrement
with indirect/absolute memory-sourced counts sampled once. Their final RC,
count+1 pointer effects, continuation and replay predicates are unchanged.
This does not qualify direct/long-offset MAR or all interrupt compositions.

The RPT **count source**, independently of its MAR body, now supports
long-offset stationary/preupdate/circular forms. Read consumes the retained
offset once, samples the count and performs the source's owned AR update;
it must not also apply a short-mode modification. Absolute/long count headers
have two words for retirement/protection as well as Decode. Three added count
cases in `conditional_repeat` check a clock-7 Read, pointer results,
source poisoning after Read, and replay from that captured-count boundary.
The two original memory-count forms and XC carry cases remain. Count-Read
holds and repeat-tail refill are declared model timing, not silicon measurements.

LDR (`16xx/17xx`, SPRU172C page 4-78) retains the source and SXM and produces
the shifted word with low half `8000`, without altering arithmetic flags.
`rounded_load` retains all 64 cases/restores across signed/unsigned extension,
both destinations, four source patterns and direct, indirect, decrement and
absolute addressing, including a direct peripheral-window source.

### Post-loop BRC write ownership

STM BRC uses the existing zero-latency Access publication before a new
RPTB (SPRU131G table 7-24). Its loop membership must come from that
Access instruction, not whichever older instruction occupies Execute.
The original `STM #003f,BRC` at `03:d69d` overlaps the older loop endpoint
`DST A,*AR1+` at `03:d69c`. A guard using only Execute's body tag wrongly
attributes the value `003f` to the DST and rejects an outside-loop reload.
Early Access/Read writes now pass their owner stage to the shared guard;
ordinary Execute writes retain their own stage. No in-loop latency or
dynamic BRC-update semantics are added by this correction.

`repeat-rearm` covers 64 cases/restores: consecutive blocks with first
counts 0/1/3/7, next counts 0/2, two pages and four snapshots. It checks
one BRC publication and independently counted iterations in both blocks.
The negative `rearm-edge` fixture passes eight cases/restores for an actual
Access-owned in-body STM BRC write, excluded before publication. These are
CPU conformance guards, not silicon traps. Physical phase timing and
interrupt composition are not established by this fixture.

### Signed dual-memory MAC

SPRU172C 4-82/4-83 identifies `b0a0` as
`MAC *AR4+,*AR2,A,A`: signed X/Y product plus the source accumulator,
with X also published to T in Read. This Class-7, one-word instruction
reuses paired address capture and retained RAM reads. The accumulator
remains Execute-owned, with FRCT, SMUL product handling, OVM and sticky
destination overflow handled by the existing arithmetic helpers.

The same-accumulator MAC contract belongs to `dual_mac`, alongside its
cross-accumulator inputs. Low-page admission is represented by the shared dual-read
guard controls, not an additional MAC-specific rejection matrix.
Operand/pointer/BK mutation after capture detects rereads or repeated updates;
accumulator mutation before Execute checks feedback ownership. The complete
family includes cross-accumulator MAC, rounded MACR and MAS/MASR; physical
bank timing and interrupt/repeat composition remain unvalidated. Execution
of the original MAC is not proof of valid recording or audio.

### Far branches, delay slots and XPC

FB/FBD and BACC/BACCD/FBACC/FBACCD reuse the retained target and two-word
delay handling already used by calls. Accumulator targets are captured in
Read before delay words can replace the source. Delayed transfers redirect
Prefetch while preserving the caller's page for the delay instructions;
branches reserve no stack frame. Nested control transfers in delay slots
remain rejected. `far_edges` retains all 23 encoding/mask/page/frame/
delay cases; neighboring pending-call, return and IRQ cases guard shared paths.

The memory-mapped XPC view is modeled read-only to ordinary guest data writes.
SPRU131G table 6-3 (page 6-5) lists reset and far control operations that load
XPC and explicitly excludes other instructions. The legacy fixture's immediate
page switch from `STM #ffffh,XPC` therefore lacked an architectural basis.
Its replacement records the attempted write and checks unchanged page/XPC
readback; debugger XPC assignment remains an explicit architectural redirect.
This is a manual-derived model correction, not a captured silicon observation.

### FIRS coefficient and sum pipeline

Standalone FIRS retains two RAM addresses in Access and X/Y values in Read.
The captured physical coefficient address owns a separate program-bus hold;
its one read supplies the retained coefficient before Execute. Both the pending
request and its address are saved/reset. Execute accumulates old A(32–16)
times the signed coefficient into B, then places `(X+Y)<<16` in A using the
existing FRCT/SMUL/OVM/carry/sticky-overflow helpers (SPRU172C 4-59, Class 8).
The extra program-bus cycle models the documented initial three-cycle cost;
its precise silicon placement remains partial. Short single-instruction repeats
use the coefficient stream described below; mapped operands and
block/XC/control composition remain explicitly unqualified.

`firs_sum` keeps all 32 independent status/result cases. Two added cases poison
X/Y and the instruction literal after Read, with checkpoints before and after
the coefficient read; the latter also poisons the coefficient afterward.
Exact clocks (X/Y at 6, coefficient at 7), one read per operand, result and
replay distinguish captured data from rereads or a lost pending request.

MACP/MACD (SPRU172C 4-87..90) shares the captured program-coefficient request.
Access retains the RAM source and updates its pointer once; Read captures the
source and publishes T after older Execute. Execute accumulates the retained
signed product; MACD also copies the retained word to source+1 without
republishing T. Mapped sources and a wrapping MACD successor are rejected.
GNU binutils 2.43 tic54x-as independently emits `79ea 0005 0700` and
`7bea 0005 0700` for `MACP/MACD *+AR2(5),0700h,B`: long offset precedes
coefficient address. Both legacy parsing and the six-case literal fixture now
use that order; all original operand/address/result expectations are unchanged.

Pipelined FIRS and short MACP repeats retain the decoded opcode, advance PAR
once per iteration, and use PB for coefficients instead of refetching opcode
extensions. The first request owns an extra program-bus hold; later iterations
read one coefficient per clock. The stream-start flag and held physical request
are saved/reset. Two coefficient-FIR cases in `product_literals` snapshot
the held first MACP request and MACD before its successor write, poisoning
already-read RAM/literals/coefficients/T as appropriate. Exact reads, writes,
products, final input poison and replay are checked. Repeated MACD, extended
MACP repeats and coefficient block/XC/control composition remain rejected.

`repeat_status` checks BRAF independently of CPL. SPRU131G 7.5.8.2 clears
BRAF at Decode of the final iteration's first instruction, not after Execute
of its endpoint. Its endpoint LDM therefore captures `4800`, not atomic's old
`c800`; the exact two-iteration Execute sequence, BRC exhaustion and unchanged
CPL/INTM bits are checked across restore.

### Dual multiply interrupt-register sources

Dual MPY/MACSU (`0xa400..0xa7ff`) admits IMR/IFR as either source through
the ordinary memory map. Both words remain Read-owned and X publishes T;
no interrupt is synthesized or acknowledged by reading those registers.
Other unqualified low-page multiply sources retain the pre-effect guard.
`zero_bk` keeps all 208 pointer-wrap cases and strengthens its MPY rows with
initialized RAM/IFR values, exact Read clocks, product/T and preserved IMR/IFR.

### Parallel load and multiply-accumulate

`LD Xmem,A/B || MAC[R]/MAS[R] Ymem,other` (`0xa800..0xafff`) captures
both RAM addresses in Access and both words in Read. Read also captures T after
older Execute publications, without changing T. Execute loads X into the selected
accumulator's high word and applies signed `T*Y` to the other accumulator,
with FRCT, SMUL, rounding, OVM and sticky overflow handled by the existing
arithmetic helpers (SPRU172C 4-74..77). `parallel_load_multiply` retains its
64 independent combinations and one added T mutation/checkpoint at Read,
covering result retention across restore. Mapped operands and repeated/control
composition remain outside this admission.

### Indirect MMR addressing and DAGEN latency

Short-indirect POPM resolves and retains its MMR destination in Access using
the existing page-zero addressing/update rules, including CMPT and upper-bit
clearing (SPRU131G 5.6). Admission is bounded to AR0..AR7/SP destinations;
an unqualified destination rolls back the address preview before rejection.
Read retains the stack word and increments SP once. AR/SP destinations use
the existing Read-owned publication and compiler-SP latency handling;
Execute uses the retained destination, not the encoded mode byte.
`indirect_popm` preserves all four independent pre/post/mask/CMPT/SP cases,
exact source/destination bus events and replay.
The page-zero exclusion matrix now resolves admitted indirect POPM forms at
Access (clock 4), rather than expecting Decode rejection at 3. It still forbids
all bus accesses and stack/status changes, and additionally checks all eight
ARs remain unchanged after a rejected preview. Other exclusion clocks remain
unchanged.

STM BRC/RSA/REA (`0x771a..0x771c`) shares the retained literal/early-write
path for loop-register setup; in-body mutation of these registers remains
rejected. `mmr_burst` retains both twelve-register initialize/pop/push
cases, including complete restored register and stack-word comparisons.
PSHM T is not a Category-I consumer, so an older POPM/STLM T publication must
precede its retained Read (SPRU131G table 7-13). This forwarding reuses the
existing late PSHM Read path and performs one real read, not a synthesized
bus response. RAM stack-alias forwarding remains unchanged.

The burst's ordered events distinguish Read-owned DAGEN POPM writes from
Execute-owned T/BRC/RSA/REA writes and interleave successive PSHM reads and
stack writes. `initial_control_sequences` similarly retains its call frames,
terminal repeats and exact three-copy results while checking Read-before-Write
overlap at RET and in repeated MVDD. Neither claims physical cycle timing.

Staged LDM admits short indirect modes `0x80..0x97`; STLM admits those modes
only when the resolved destination is AR0..AR7. Access retains the page-zero
address, modifies the selected pointer once and clears its upper nine bits
(SPRU131G 5.6, SPRU172C 4-73/4-175). LDM consumes the retained Read value;
STLM writes its retained destination in Execute. Direct MMR behavior is
unchanged. The absolute table in `memory_literals` retains independent unsigned-load,
masked-address, pointer-update and store values. Its TCR readback is separated
from the write, not a claim of simultaneous peripheral arbitration.

The multiply/move table in `product_literals` has an STLM AR5 / MVDD / MVMM sequence requiring two
intervening words before using AR5 (SPRU131G table 7-3). Inserting one NOP
supplies that separation without changing its seven independent result
cases. The copy's Read precedes STLM's Execute and MVMM's Decode at clock 7;
the copy's Write precedes MVMM's Access write at clock 8. These explicit model
clocks replace atomic whole-instruction bus serialization.

### Combined conditional calls

`delayed_call` retains nine staged CCD cases with page-local return
frames and accumulator changes in both delay words. The combined AGT+AOV
condition (`0x76`) samples the positive predicate and OVA before those words;
testing OV clears OVA even if the combined predicate is false (SPRU172C
4-29..31). Other combined-condition encodings are not admitted by this result.

### Short RAM ADD repeats

Short `ADD Smem,A/B` bodies (direct or indirect modes below `0x98`) are
admitted to pipelined single-instruction repeats. Each iteration captures its
own address/source before Execute updates the accumulator; mapped/peripheral
sources remain rejected before a bus read. The address table in `memory_literals`
retains the fifteen-add result and read count, and `repeat_irq` checks
four/65,536 additions, exact count exhaustion, and the pending IRQ's continuation
frame only after the repeat completes. Both retain save/replay coverage.

The base table in `port_marked_memory` keeps eleven independent result/address/flag cases.
Its six-ADD body executes at clocks 10..15, between marker writes at 8 and 21:
the 13-clock interval includes the declared Prefetch/refill and two-word marker
Decode. All other marker intervals retain their existing expectations. This
is a staged model clock contract, not a measured peripheral timing claim.

### Fixed-MMR repeat transfers

Direct MVMD AR0..AR7 and MVDM to AR0..AR7 reuse the pipelined two-word copy
stream. Each iteration advances EAR exactly once while retaining its MMR
selector (SPRU172C 4-114/115). MVDM's repeated destination guard uses that
fixed selector, not a DP-relative address preview; source RAM and AR-only
destination restrictions remain enforced before bus/pointer effects.
MVMD retains its interleaved source reads and RAM writes; MVDM publishes each
AR destination in Read as in its existing nonrepeated path.

`mapped_moves` keeps all four baseline source/destination progressions
plus a DP=0 checkpoint mid-repeat. `repeat_endpoints` retains twelve
short/long-count, page-end, IRQ/NMI and final-copy cases with exact continuation
frames, no fourth destination write, and restore after frame publication.
It asserts three logical body executions and one captured extension, replacing
atomic's implementation-specific opcode/extension refetches.

### Software INTR and INTM followers

`software_intr_pipeline` checks all 32 software vectors with both initial INTM
states in 64 cases/replays. Decode-owned RTN, retained opcode/vector words,
exact frame Write/RET Read/handler and continuation Write clocks, frame poison,
selective IFR clearance and preserved arithmetic/configuration state are checked.
Pages and two checkpoints are distributed rather than producing 256 executions.
RET's Read precedes the handler STLM's Execute in their shared model clock.
The staged `software_interrupt` section retains 128 cases covering TRAP/INTR,
both initial INTM states, every vector, IPTR selection and RETF behavior.
TRAP shares Decode-owned RTN, vector redirection and frame delivery with INTR,
but leaves INTM and IFR unchanged (SPRU172C 4-193). This broader functional
coverage does not substitute for `software_intr_pipeline`'s publication clocks.

`intm_follower` retains all three complete-follower cases on staged after
INTM clear: one-word LD, three-word ST and RET. SPRU131G 6.10.7 requires the
decoded pipeline to drain, excluding Prefetch/Fetch; it does not require
atomic's immediate-next-instruction return boundary. The staged frame returns
at `0x9d5`/`0x9d7` after LD/ST plus three decoded NOPs, or at RET's `0x9e0`
target before any target instruction executes. Exact logical Execute sequences,
data events, frame words and register state prove those boundaries through
save/replay, rather than merely accepting a larger frame-address range.
The separate `interrupt_mask` section covers adjacent INTM changes and all
one/two/three-word followers. Neither measures silicon interrupt timing.
Both sections have explicit reset/setup and save/replay;
their old phase-chain setup/result owners are removed.

### Stack operation and interrupt entry

`stack_interrupt_order` owns sixteen PSHM/PSHD/POPM-T/POPM-SP cases with
IRQ/NMI, covering both entry-only observation and RETE/caller cleanup. Pages
and the original three/five checkpoint positions are distributed instead of
expanding to 128 executions. Foreground source Read is at model clock 5;
POPM SP publishes in Read before the interrupt reserves its stack slot.
Other foreground publications occur at 6, acknowledgement at 6, and interrupt
frame Write at 7. Exact bus progress distinguishes these ordering boundaries.
Returning cases consume only the interrupt frame before the caller cleans up
its own push. Stack reservation, return PC, T, B, INTM and final stack lifecycle
are checked through IDLE/replay. These are model timing/ownership contracts,
not silicon measurements. The two legacy profiles and phase-specific IRQ hook
are removed.

### Sampled IFR acknowledgement

`ifr_ack_pair` owns 32 A/B/pattern/scenario cases: pending-only acknowledgement,
late masked IRQ, accepted IRQ and independent NMI. The adjacent LDM/STLM pair
reads IFR at model clock 5 and writes the retained sample at 7. A late bit not
present in the sample remains pending; a previously sampled bit is cleared even
if another edge arrives before acknowledgement. IMR enable/NMI input after
Read also verifies that selected entry survives the IFR write, with one owned
frame and RETE roundtrip. Exact bus traffic, IACK progress, preserved unselected
accumulator and final flags/stack are replayed. Three checkpoints are distributed
instead of expanding to 96 cases. IMR changes are explicit latch conformance;
these are not physical interrupt timing measurements. The old profile and
phase-specific IRQ hook are removed.

### Unsigned IMR load

`unsigned_imr_load` covers 64 form/value/SXM/accumulator cases. Direct,
absolute and indirect IMR reads retain the mapped word without sign extension;
the MVDK composition publishes a zero AR2 pointer before adjacent LDU address
capture. Exact RAM/pointer/IMR transactions, later pointer/mask changes,
retained/poisoned extension words and unchanged status are checked through
IDLE/replay. Pages and three checkpoint positions are distributed instead of
producing 384 executions. These are model capture/publication contracts;
active interrupt composition is not qualified. The old LDU setup/result phase
and profile are removed.

### Atomic waited I/O control

`waited_io_control` retains 12 independent cases/replays: four repeat
lengths and one delayed-branch/INTM program in each transfer direction.
Seven C5416 software waits remain configured. Physical IRQ0 is asserted
while the protected sequence runs; its sole acknowledgement follows every
port transaction and retains RTN 0603 (repeat/INTM) or 0700 (delay). Exact
transfer counts/values and final pointer are replayed. The former 24 cases
repeated unchanged delay/INTM programs four times. No silicon timing claim
is made; its legacy setup, result branch and IRQ hook are removed.

### Immediate peripheral control writes

Immediate control writes are split into `peripheral_stm` (20 cases for
stopped TIM/PRD/TCR, unclocked CLKMD and BSP control), `c5416_bus_stm`
(12 SWWSR/BSCR/SWCR cases) and `standard_bus_stm` (4 original-family SWWSR
cases). CPU variant configuration is explicit in each section. One retained
literal, its later poisoning, exact Write8/no Reads and device-specific masks
are checked through replay. Timer cases preserve stopped PSC and exercise
TRB reload; no physical PLL/BSP phase is claimed. Checkpoints are distributed
instead of expanding 36 distinct inputs into 108 executions. The five legacy
profiles, setup and result branches are removed.

### MVKD peripheral store

`mvkd_peripheral_store` covers 108 source/form/destination/value cases and
replays. AR0, BK and RAM sources feed destinations 003b/0048/0049 through
indirect, postincrement and absolute addressing. Exact Read6 (absolute7)
precedes Write by two clocks. Poisoned source/extensions cannot change the
captured publication; flags/accumulators and one pointer update are retained.
Pages/checkpoints are distributed instead of multiplying to 648 executions.
The legacy factory/setup/result phase/profile are removed. BK source-producer
latency and physical peripheral timing remain unvalidated.

### T/TRN peripheral store

`t_peripheral_store` owns 144 instruction-driven T/TRN store cases/replays:
both source registers, indirect/postincrement forms, destinations 003b/0048/0049,
four boundary words and producer gaps 0/1/2. Exact STLM Write6 and destination
Write(8+gap), no Reads, retained source/flags/accumulators and one pointer update
replace 864 executions with distributed pages/checkpoints. The legacy factory,
setup, embedded result phase and profile are removed. These are CPU/device
publication contracts, not electrical timing measurements.

### Unsigned peripheral load

`unsigned_peripheral_load` owns 1,536 cases/replays:
all 64 synthetic endpoints, direct/absolute/indirect addressing, four words
and both accumulators. Read6 (absolute Read7), zero extension despite SXM,
preserved flags/other accumulator and retained poisoned literal/pointer are
checked with exactly one Read and no Writes. Pages/checkpoints are distributed
instead of multiplying to 9,216 executions. Endpoint substitution tests CPU
bus semantics only, not actual peripheral register values. Its legacy owner
has been removed.

### Full-status interrupt composition

`status_write_interrupt` replaces both MVMD/STLM status IRQ profiles with
192 independent cases/replays. It crosses old/requested INTM, IMR, IRQ/NMI,
three arrivals and one/two-word followers. Pages/checkpoints are distributed
rather than multiplied into 1,152 executions. Source Read5 and ST1 Write6
remain exact; the foreground marker executes once, and accepted service owns
one frame whose push/pop words equal RTN within the bounded continuation.
Rejected IRQ remains pending. Memory-write INTM clearing is still an
implementation-limit contract, not validated silicon behavior. Neither exact
IACK latency nor exact marker timing is claimed. A masked pending input can
wake successive IDLE instructions, so terminal IDLE is not required.

### Active repeat context and stack bypass

The original wrapper uses `POPD *AR7` at logical `03:1f02`, with AR7
`001d`, SP `5cd3` and saved/current PMST `202c`; RSA/REA remain
`db51/db56`, outside the restore routine. OVLY maps this low-half code
to physical program `001f02`. A raw read at `031f02` returns zero and
must not be interpreted as missing program content.

`pmst_restore` owns 368 independent cases/replays: 64 STM/POPM/POPD/prefetch
mapping cases, 48 BRC/RSA/REA pop cases and 256 interrupt-vector cases. It
retains four values, both source pages, lower/upper target regions, all sixteen
maskable sources and both pending-latch timings. Four checkpoints are distributed
instead of multiplying the grid into 1,472 executions. Exact source Read and
control Write clocks, source poisoning, program mapping, retained prefetch,
repeat-register preservation, IRQ stack words and selected vector markers are
checked through IDLE/replay. The IPTR fixture supplies IFR directly: it is a
CPU-latch conformance test, not physical-pin timing or ISR-return validation.
The prefetch case deliberately changes PMST through debugger state after the
first Fetch; an invalid opcode in the other bank detects resampling. A wrong
vector marker fails. All six old profiles and their shared phase machinery go.
`active_repeat_pmst` owns32 cases/restores using the same/replaced loop-context
inputs below, with an actual PSHM/POPD PMST pair outside the loop body,
ordered stack transactions and independently checked iteration counts.
The active PMST case restores unchanged PMST only. `active_repeat_popd` owns
32 cases/restores for indirect REA/RSA/BRC restoration outside the loop
body, using the same/replaced-context matrix and checking retained source
words, ordered publications and actual loop iterations. Its PSHM and
POPD sequences are separated; adjacent PSHM-to-POPD bypass is not claimed.
Active PMST mapping changes and in-body control writes remain excluded;
these tests do not establish interrupt composition or physical
bus-phase timing. These active-loop compositions are separate from the isolated
`pmst_restore` publication/mapping tests. Historical default checkpoints below
remain revision-bound, not renewed by section-only migration runs.

Separated POPM REA/RSA/BRC writes outside the active loop body are admitted;
in-body writes remain excluded. A near return into the restored bounds,
or immediately after their endpoint, rebinds the retained loop bounds and
iteration count from RSA/REA/BRC while preserving BRAF. `active_repeat_context`
owns32 independent cases/restores: same and replaced contexts, four counts,
two layouts and both pages. Stack alignment and decode/save/pop/return
checkpoints are assigned across inputs rather than multiplied. The same
reduction applies to its PMST/POPD companions; full stack/snapshot correlation
with every input is deliberately not retained. Ordered copies
and actual iteration counts are checked, not just register readback. Six
NOPs separate BRC restoration from return. Inactive-to-active BRAF changes,
general nested/interrupt contexts and other reentry routes are unvalidated.

Adjacent direct PSHM/POPM on a RAM stack requires same-clock write/read
bypass: the younger Read samples the older Execute publication, retaining
its already allocated stack address. SPRU131G section 7.3.2 documents the
on-chip pending-write bypass. `push-pop-same` passes 2,048 cases/restores
across eight sources, T/AR3/BK/SP destinations, four patterns, two pages,
two stack alignments and four snapshots. Source and stack mutation after
capture detect rereads. This does not establish PSHD/POPD composition,
peripheral stacks or physical bus-phase timing.

In the original context wrapper, a Decode-time stop sees SP before two
older pending pops have read their operands: REA/RSA/BRC are then at
SP+2..4, not SP..2. That observation must not be diagnosed as a corrupt
saved frame without accounting for the older pipeline slots.

### Physical BIO predicate ownership

BIO/NBIO predicates are also admitted for conditional calls, returns and XC.
Calls/returns sample the physical line once in Read; XC samples once in Access,
matching the existing owners for their other predicates. `bio_conditions` retains
all 28 opcode/level contrasts and its physical polling case on staged. The
polling test changes the pin at clock 13, checks Read samples at 7/12/17,
and observes two taken logical BC executions followed by the untaken BC,
the real load and IDLE. This replaces an atomic in-flight PC convention;
final PC, stack, selected values, delay effects and replay remain checked.
These stage clocks are model contracts, not measured silicon timing.

### Delayed conditional calls

CCD admits AGEQ/ALT/ANEQ/AEQ/AGT/ALEQ and their B equivalents. SPRU172C
4-29 specifies the two-word delay span, a taken frame containing `PC+4`,
and no stack transaction for an untaken call. The retained Read-stage
predicate is evaluated after older Execute publications and before stack
reservation. Only a taken call publishes its one-word frame in Execute;
the existing delayed-branch flow executes the two delay words on either
path without adding the nondelayed CC model's held clock. No new CPU save
fields are introduced. Physical phase timing is not independently measured.

`delayed_conditional_call` owns 384 independent cases/restores across 12 signed accumulator
conditions, eight 40-bit patterns, two delay layouts (two single-word
instructions or one two-word instruction) and two pages; five snapshots
are distributed. Exact target/delay fetches and the taken frame Write7
are checked against the model, not a silicon timing measurement.
Delay instructions change the tested accumulator without changing the
retained decision. Fetched target/delay words and sampled return frames
are replaced afterward to detect rereads. Both paths execute delay work
once; only the taken path visits the callee, balances a frame and returns
to the shared continuation. Other condition encodings remain outside this
qualification; prototype rejection is not a silicon trap contract.

An in-block CCD may target an outside callee when its complete header and
two delay words fit within the block, including a delay span ending at REA.
The frame remains `PC+4`, even when that equals the address after REA.
On a near return from outside the block, remaining Decode-owned BRC state
rebuilds discarded fetch predictions. Returning after REA wraps to RSA only
while the outer block remains active; the final iteration falls through.
Neither the stack frame nor firmware RAM is patched to select that path.

`block_conditional_call` owns 256 independent cases/restores across counts 0/1/2/7, AEQ/ANEQ/
BEQ/BNEQ, both outcomes, middle/endpoint placement, both delay layouts,
two pages; four snapshots are distributed. Every iteration performs one ordered RAM
copy and the expected delay/callee/suffix work, with balanced `PC+4` frames
and final BRC/BRAF/RSA/REA checks. The source sequence is now an arithmetic
progression rather than the old XOR pattern. Every return pop poisons real
stack RAM; an untouched destination tail guards extra stores. Interrupt composition, nested loop-control
changes in callees, other conditional-call families and physical timing
remain unvalidated. Nondelayed CC/RC, RPTB, RPTBD IRQ/NMI, in-block far
calls, backward branches and inner single repeats retain their focused
acceptance; this does not renew every isolated CPU profile.

### Mapped MVDK sources

MVDK admits sources `0008..0019` (accumulator words, T/TRN, AR0..AR7,
SP and BK). Access resolves and retains the source address and word before
source-pointer modification or destination publication. Source/destination
aliases therefore use the old sampled word. The existing destination timing
is preserved: DAGEN registers publish in Access, RAM and T/TRN in Read.
No new CPU save fields or atomic fallback are introduced.

SPRU172C 4-108 specifies the move and addressing forms; SPRU131G 7.5.10
distinguishes direct accumulator-producer latency from mapped-write/read
no-conflict ordering. A same-clock mapped STL/POPM accumulator publication
is sampled after Execute and can still publish a DAGEN destination in the
move's Access clock. A direct ALU producer is not forwarded by that rule.

`mapped_mvdk` passes 2,376 independently compiled cases/restores: 18 sources,
direct/postincrement/absolute forms, 11 destinations (eight ARs, T/TRN and
RAM), and four patterns. Pages and three snapshots are distributed rather
than multiplied. `accumulator_mvdk` adds 1,368 cases for six accumulator
destinations and a nineteenth RAM source; `clock_mvdk` adds 12 cases for
CLKMD's extra publication clock and readback normalization, without a
configured PLL reference or physical clock claim. Guard words use eight bits; the indirect
AR6-source self-alias uses its actual address value. Replacing fetched
literals and changing the source after Access cannot change the retained
transfer. `mapped_mvdk_producer` passes 864 independent cases/restores across
six accumulator words, three forms, STL/POPM/direct LD, gaps 0/1, AR3/RAM
and four input patterns. `accumulator_mvdk_producer` adds 2,592 cases with
all six accumulator destinations. Pages and three snapshots are distributed,
not multiplied. Exact ordered bus events include POPM's stack read and the
producer's mapped publication before MVDK capture. Post-capture source and
literal poisoning must not change the result. A changed expected capture
word is rejected. The legacy factory/setup/checker/routes are deleted.
LD's immediate is eight bits.
The unseparated short-form direct-LD case checks an Access-owned pipeline
projection, not a guaranteed silicon result for violating the latency rule.
STLM-produced accumulator inputs to MVDK, general producer/interrupt/repeat compositions
and measured physical timing remain unvalidated.

Actual read-identity acceptance is separate from that timing projection.
The mapped, accumulator-destination and CLKMD MVDK sections require
the captured Address-slot instruction address/opcode inside every source
bus callback. They reject another address and debugger scope, and require
read identity to be unavailable at destination publication. The legacy
between-tick observer polling is not retained. All 3,756 cases/restores pass
with exact one-read/one-write counts and ordered literal fetches. A negative
control expecting a changed publication word fails at the first CLKMD write.
Legacy factories, phase handlers and routes are removed. No CPU emulation
change is retained.
This tests the bounded diagnostic API, not every operand family, physical
silicon phases or a renewed complete default-core checkpoint.

Unadmitted status, repeat-control and peripheral sources remain outside
the mapped-source contract; prototype exclusions are not silicon traps.
Existing MVMD accumulator,
MVMD ST1, T/TRN move, mapped multiply and RPTBD interrupt controls pass.
Passive Execute hooks verify the original recorder's MVDK and following
instruction, with zero observer writes; current execution counts and the
stop are stated above. Resident-memory and low-page-copy controls remain
unchanged; this is not complete recording or validated audio.

### Delayed block-repeat boundary

A passive program-space read at the stopped original recorder supplies this
instruction sequence; it is not an injected test program:

```text
03:d9c2  891a       STLM B,BRC
03:d9c3  8812       STLM A,AR2
03:d9c4  f272 d9c9  RPTBD 0xd9c9
03:d9c6  6deb 0a40  MAR *+AR3(0x0a40)
03:d9c8  e5b8       repeated body, first word
03:d9c9  e5b9       repeated body, inclusive endpoint
03:d9ca  f6e4       following delayed far return
```

SPRU131G section 6.8 specifies that RPTBD executes either one two-word
instruction or two one-word instructions in its delay span. Thus this
sequence requires RSA `0xd9c8`, REA `0xd9c9`, one MAR update before the
loop, and `BRC+1` body iterations. Treating the delay span as two decoded
instructions, repeating MAR with the body, or starting RSA immediately after
the header would be incorrect. Section 7.5.8.1 separately prohibits BRC
modification in RPTBD delay slots and describes producer latency before
RPTB[D]; the preceding STLM-to-BRC sequence needs that ownership checked,
not a blanket immediate forwarding rule.

The staged implementation shares RPTB's saved loop state and the existing
control-delay machinery. Complete Decode establishes the loop; the Access
redirect preserves both prefetched delay words. No new scalar CPU save fields
are needed. The `rptbd` profile passes 512 cases/restores across counts
0/1/2/7, four delay layouts (two MAR words, STM T, offset LD and the original
two-word MAR), four data patterns, two pages and four snapshots. Replacing
the endpoint and delay words after Fetch, and the delay-load source after
Read, cannot change retained work. Ordered body reads/writes, final memory,
RSA/REA/BRC/BRAF, once-only delay effects and unrelated registers are checked.

`block_delay_shift_store` owns 1,340 independent cases/restores. Its 960-case
numeric bank crosses five destination forms, four source/high-low selectors,
shifts -16/-1/15, SST/SXM, two signed source patterns and CPL at outer count2.
The 380-case composition bank covers outer counts0/1/2/7 and none/IRQ2/NMI
at delay/body arrival windows with representative numeric inputs. Page and
three checkpoints are distributed. The former full numeric/count/IRQ cross
product is deliberately retired. Endpoint/extension poisoning, source
replacement at tick8, one exact store at clock9, no destination read,
RTN-bound hardware frame and final body/controller effects are checked.
These are model timing checks, not measured silicon phases.

`composition_limits` retains 28 delayed-block cases/restores across seven exclusions, two pages and
snapshots before/after the guard: STM/STLM/POPM BRC in the delay span, a
branch, a two-word spill into one remaining delay word, and nested RPTBD
in the body or delay span. They stop before data transactions, pointer
updates or count modification. These are bounded prototype exclusions,
not silicon traps or exhaustive coverage of indirect control-register writes.
The existing RPTB edge fixture now tests nested RPTBD rather than rejecting
all RPTBD instructions.

The separate original-firmware run records one RPTBD Execute entry, one
delay-MAR entry and 1,024 first-body entries with zero observer writes. Its
resident-memory comparison and first 128 ordered low-page reads still pass.
RPTB (6,168 cases/restores plus 16 edge cases), backward in-block branches,
in-block far calls, delayed LD (`delayed_load`) and the native 5110
save-state control pass on this implementation. These focused profiles retain
their own scope; current default-suite coverage is recorded in the
[core checkpoint](#retained-suite-acceptance).

`rptbd-irq` passes 768 cases/restores across four counts, all four delay
layouts, two pages, IRQ2/NMI, arrivals during the delay span or at the first
body write, and six snapshots. Interrupt acknowledgement follows complete
delay work; each case balances one retained continuation frame and returns
to ordered copies without repeating delay side effects. The terminal IDLE
is separated from the body by eight NOPs to distinguish loop continuation
from the separately tested IDLE-wake path. Header literals, delay words,
delay-load samples, CPU loop state and host observation counters survive
replay. Other arrival clocks, nested interrupts, instruction-produced BRC
latency matrices and measured physical timing remain unvalidated. No CPU
behavior change was needed for this additional interrupt acceptance.

The [core regression checkpoint](#retained-suite-acceptance) distinguishes
default-suite coverage from isolated-profile coverage. Instruction contracts
below retain their focused acceptance and explicit limits; none establishes
complete native audio or every composition of supported instructions.
The tool suite passes 2,861 tests with one skip; the native 5110
save-state round trip and exact original recorder resident-memory comparison
also pass. The first 128 ordered low-page reads remain unchanged.

Plain LD through `*ARx-0` or `*ARx+0` captures its old source address and
AR0-sized postmodify in Access. Read samples the retained RAM address;
Execute sign-extends or zero-extends according to SXM without rewriting the
pointer or changing carry/overflow. CMPT selects the indirect AR and may
publish ARP as for existing basic indirect consumers. AR0 selecting itself
uses the same pre-update value as address and index.

`ld-ar0-index` passes 98,304 cases/restores: 96,000 RAM cases and 2,304 low
mapped/peripheral exclusions before Read or pointer publication. The matrix
crosses eight AR fields, plus/minus, CMPT, both destinations, eight index
values, four RAM words, SXM/SST/OVM, two pages and three snapshots. SST and
OVM do not alter the word load. Pointer/index replacement after Decode checks
Access ownership; pointer replacement after Access and RAM replacement after
Read cannot affect the retained operand or duplicate updates. The exclusions
are prototype limits, not silicon faults. General producer latency,
interrupt/delay composition, other unadmitted address modifiers and measured
physical phase timing remain unvalidated.

Plain LD Smem,A/B with a long-offset address can occupy a complete two-word
control-delay span. Decode counts both words before admission; existing
Access/Read/Execute ownership retains the literal, address, auxiliary update
and RAM word through the control transfer. This follows SPRU131G 6.3's
one-two-word/two-one-word delay rule; the load is not a PC discontinuity.

`delayed_load` owns 1,200 independent cases/restores across BD, BCD taken/untaken,
CALLD, FCALLD and RETD; offset/preincrement/circular/absolute forms; eight
AR fields, offsets +/-1 (two RAM addresses for absolute), A/B and CMPT.
RAM values, SXM/OVM, page and three snapshots are assigned rather than fully
crossed. Absolute no longer duplicates an ignored AR field. The standalone
scalar/absolute-load sections retain numeric contracts; this is a deliberately
bounded control-interaction bank. Replacing literals after Fetch, pointers after Access
and RAM after Read cannot change the retained load or duplicate updates.
The selected target observes the result once; BCD decisions survive delay
loads that change A. Call frames and RETD's separate stack read are checked;
the CALLD/FCALLD targets stop after observation, not a return round trip.
Family exclusions are prototype limits, not silicon traps. Mapped sources,
general interrupt composition and measured physical phases remain unvalidated.

Near B and non-delayed BC admit backward as well as forward targets inside
the current block, provided the complete branch precedes REA. A taken branch
rebuilds discarded endpoint predictions from Decode-owned BRC; an untaken BC
retains the held word's block metadata. Back edges do not consume outer
iterations. Conditional calls are explicitly excluded from this branch class,
despite sharing the generic conditional-control predicate.

`block_repeat_backward` owns384 independent cases/restores across outer counts0/1/2/7, inner
counts 1/2/4/8, conditional GE back edges or LT exits plus unconditional back
edges, both accumulators, two pages and none/IRQ/NMI. Four snapshots are
distributed instead of crossed with every input. Each
outer iteration initializes its inner accumulator counter; the inner loop
copies ordered RAM words until negative. The test checks exactly
`(outer+1)*inner` copies, one outer suffix, final counters/BRAF, continuation,
balanced interrupt frames and restored held words. Interrupts arrive after
the first copy, not at every clock. On the unconditional route with another
inner copy pending, its Read precedes IACK and its Write drains before the
hardware frame; the complete ordered trace checks that model interaction.
An off-by-one inner count fails on an extra source Read. The 14-case `block_branch_limits` control
tests below/above-block targets and a conditional call instead of treating
ordinary backward branches as unsupported. Forward exits have their separate
`block_exit`/`block_delayed_exit` coverage; delayed back edges and measured
physical phase timing remain unvalidated here.

FCALL is admitted from inside a block when the complete call precedes the
endpoint; FCALLD additionally requires both delay words inside the block.
Its target must be outside the caller's logical block. Existing retained
frames and Access-time redirection preserve the caller's Decode-owned loop
state and rebuild discarded endpoint predictions; no separate call/loop state
or save fields are introduced. Code outside the low 16-bit interval does not
acquire its body tags, but a foreign page's matching low PC can consume the
loop. See the shared [repeat-endpoint contract](rom4_dsp_execution.md#repeat-endpoint-and-page-hazard).

`block_far_call` owns192 independent cases/restores across counts0/1/2/7, early/near-end
placement, two caller pages, same/cross-page callees, one/two nested far-call
frames and none/IRQ/NMI. Four snapshots are distributed rather than crossed
with every input. Interrupts arrive on the first callee
RAM read, not at every possible clock. The test checks one ordered copy per
outer iteration, balanced frame/interrupt transactions, caller-page return,
prefix/suffix counts, BRC, RSA/REA and final BRAF. The original recorder reaches
its `FCALL` Execute hook once before the next exclusion. `block_delayed_far_call`
owns576 replayed cases with the same counts/pages/depths/interrupts, middle and
endpoint delay placement, and two single-word, one double-word or original
LD/NOP delay layouts. It checks once-only delay effects and paired page/PC
frames returning to call+4. The complete bus trace distinguishes IRQ entry
after an outer FRET from entry with both nested frames still active. Read-owned
STM delay publication precedes the call's Execute-owned page Write. These are
model-ordering assertions, not physical bus timing. Rejection of unsupported internal targets and
incomplete call spans is an implementation limit, not a silicon-trap claim.
Callee loop-control writes, arbitrary control mixtures and
physical phase timing remain unvalidated.

Acceptance for this composition includes all 288 required checks in the
default isolated CPU suite, 16 focused repeat/control profiles, the native
5110 save-state round trip and 2,891 tool tests (one skipped). The original
REL run is a separate failed coverage diagnostic at the mapped shifted-store
boundary, not an audio acceptance test.

MACSU Xmem,Ymem,A/B retains both RAM words in Read, treating X as unsigned
and Y as signed independently of SXM. X is also published to T in Read, after
older Execute writes; retirement does not overwrite a younger T. Execute
uses the current destination accumulator, FRCT and SMUL product handling,
OVM and sticky destination overflow. Carry and other status bits are preserved.
This follows SPRU172C 4-91 and the shared multiply-product/result helpers.

`dual_unsigned_mac` owns 51,200 cases/restores in factored matrices: 18,432 address cases
cover all 512 encodings, three alias/address layouts, linear/circular BK,
two pages and three snapshots; 32,768 arithmetic inputs retain 64 value pairs,
both destinations, eight accumulator patterns and 32 status profiles, with
pages and checkpoints3/4/5 distributed. All status-varying address cohorts
remain. This is not the address/arithmetic full cross-product. Setup, arithmetic
expectations and Execute feedback are explicit; no legacy variant decoder or
custom snapshot machinery supplies acceptance.
Post-Access pointer replacement and post-Read RAM/T replacement cannot change
retained inputs; replacing the accumulator after Read checks Execute ownership.
Paired reads are ordered and occur twice even when logical addresses alias;
only Xmod updates a shared AR. Low-page rejection shares the dual-read guard
represented by `dual_admission`; this is prototype admission, not a silicon trap.
Physical bank arbitration
and general interrupt/repeat composition remain unvalidated.

Short-immediate RPT is admitted within an active block; tested bodies remain
wholly inside the block. RPTC owns the inner repetition; BRC and endpoint metadata
still belong to the outer block's Decode. Inner completion rebuilds discarded
prefetch predictions from BRC and resumes either after the body or at RSA
when the repeated body ends the outer block. Cloned MVDD pipeline slots do
not decrement the outer count. No new scalar save-state fields are required.
SPRU131G sections 6.7/6.8 describe the separate single/block controls; the
block-count update is Decode-owned. This is not nested RPTB support.

`block_single_repeat` owns 192 independent cases/restores over outer and inner counts 0/1/2/7,
MVDD, immediate ST and NOP bodies, middle/end placement and two pages; four
snapshots are distributed. Ordered RAM copies/stores, one prefix/suffix per outer iteration,
final BRC/RPTC, BRAF, RSA/REA and continuation are checked. NOP has no memory
effects; it is a control-state case, not an independently measured cycle count.
Boundary-straddling body encodings, combined inner-RPT/block interrupt behavior
and measured physical phases remain unvalidated, independently of the existing
standalone-repeat and RPTB IRQ/NMI controls.

Extended STL/STH A/B stores admit mapped T (`000e`) alongside the existing
AR destinations. The destination and any auxiliary update belong to Read;
the accumulator value and SST conversion belong to Execute, which publishes
one T write without reading the destination. `st-t` passes 49,152 cases and
restores across four stores, absolute/offset/preincrement/circular addressing,
eight AR fields, eight accumulator patterns, SST/SXM, CMPT, two pages and
three snapshots. Literal, pointer and accumulator replacement at their stage
boundaries checks that ownership without duplicated transactions.

SPRU131G section 7.5.6, table 7-13 requires separation after a store to T for
Category I consumers, including short-address TS loads, but excludes long-offset
consumers from that category. `st-t-consumer` passes 15,360 cases/restores with
short and long TS consumers at gaps 0/1, four stores, eight patterns, SST/SXM,
four RAM values, two pages and three snapshots. Short gap 0 checks only the
prototype's old-T projection, not guaranteed silicon behavior; long gap 0 uses
the new T. Its 3,072 overlapping younger LD T cases preserve Read-owned
publication after the older store's Execute, with no retirement rewrite.
Other low mapped/peripheral destinations remain prototype limits, not
silicon traps. General interrupt/control-delay composition and measured
physical phases remain unvalidated.

ADD/SUB Smem,TS,A/B retains both the RAM word and T in Read before older
Execute publications, then shifts and adds/subtracts in Execute. Only T bits
5..0 select the shift. SPRU172C defines the supported domain as -16..31;
raw codes 32..47 are excluded by the staged prototype, not claimed silicon
traps. Arithmetic uses SXM, 32-bit carry, sticky destination overflow and OVM
saturation through the existing shared helper. `ts-alu` passes 153,984
cases/restores in two factored matrices: 6,528 address cases over 17 read-capable
address forms, eight AR fields, both CMPT states, all four selectors, two pages
and three snapshots; and 147,456 arithmetic cases crossing four memory values,
eight accumulator patterns, 48 supported counts, SXM/OVM, selectors, pages
and snapshots. Address and arithmetic matrices are not their full cross-product.
Post-capture T, RAM, literal and pointer replacement cannot change retained
inputs or duplicate auxiliary updates.

`ts-alu-producer` passes 18,432 cases/restores with STM T, LD T and STLM T,
gaps 0/1, eight supported counts, four memory values, SXM/OVM, A/B and ADD/SUB,
two pages and three snapshots. STLM's forbidden zero-gap sequence checks the
prototype's old-T projection, not a guaranteed hardware outcome.
Mapped sources and general interrupt/control-delay composition remain
unvalidated. The neighboring legacy TS-load producer control passes without
promoting its raw-64-code projection to an architectural oracle.

`LD Smem,ASM` retains its RAM word in Read and updates only ST1 bits 4..0 in
Execute, leaving T unchanged. Its shared Smem-special path also serves T loads
and memory multiplies, whose distinct T publication remains in Read. The
`asm-load` fixture passes 73,728 cases/restores over DP, SP, postincrement,
offset, preincrement and absolute addressing, all eight AR fields, all 32 ASM
encodings, four status/payload profiles, both CMPT states, two pages and three
snapshots. Replacing the literal, source word and ARs after their owning stage
cannot cause resampling or duplicate updates. Control/SP/peripheral sources
outside the mapped admission below remain prototype limits. Incrementing
excluded forms and mapped ASM-source producer latency remain unvalidated.

The separate `asm-mapped` fixture admits the existing register window
`0008..0017` (AL/AH/AG, BL/BH/BG, T, TRN and AR0..AR7). It passes 49,152
cases/restores across all 16 sources, DP/postincrement/offset/absolute forms,
32 ASM counts, four payload/status profiles, two pages and three snapshots.
The Read-owned source is retained after all source registers and address
pointers are replaced; Execute changes only ASM, with no data writes or
duplicate source/literal reads. Guard-register sources use their eight-bit
width. Pointer registers are selected separately from an AR source, so
source/address alias behavior is not claimed. This fixture does not establish
general producer latency, CMPT, interrupt or control-delay composition.
SPRU131G table 7-28 and example 7-69 explicitly cover mapped accumulator
consumers, including long-address `LD *(AL),ASM`; the original recorder uses
the equivalent AH source.

SPRU131G table 7-3 and example 7-52 require zero consumer latency for this
load. `asm-consumer` passes 9,216 cases/restores with immediately following
LD/ADD/SUB ASM operations, all 32 signed shift encodings, positive/negative
accumulators, SXM/OVM combinations, gaps 0/1, two pages and three snapshots.
Upper bits of the source word do not overwrite other ST1 flags. The isolated
`memory_load_t` owns 2,176 independent RAM/mapped cases protecting Read-owned
T publication without a retirement rewrite. Multiply-pair composition is a
separate contract, not part of that section's acceptance. The `memory_mac` and delayed-comparison controls
also pass after the shared-path rename. General ASM-load interrupt/control-delay
composition, mapped sources and measured physical phase timing remain unvalidated.

Short two-word CMPM/BITF is admitted in a complete BD/BCD delay span. The
owner is recovered from retained pipeline slots, not reread from program
memory; calls and returns remain excluded. SPRU131G section 6.3.2 permits one
two-word instruction after a delayed conditional branch and requires that its
effects do not change the condition already tested. `delayed_compare` passes
3,456 cases/restores across BD/BCD TC/BCD NTC, CMPM/BITF, DP/SP/postincrement
RAM sources, 16 value pairs, both initial TC states, an optional instruction-
driven BITF prefix and none/IRQ/NMI. Pages/four checkpoints are distributed,
not multiplied into 27,648 executions. The legacy factory/setup/result/IRQ
hook/profile are removed. The comparison
may overwrite TC without retargeting the branch. Replaced literals, RAM and
postincrement pointers cannot change retained inputs; IRQ/NMI acknowledgement
follows TC publication. Interrupt return is checked by the selected continuation,
balanced SP and restored status, not an IDLE bit that the interrupt may wake.
MVDM-delay, standalone CMPM-branch and delayed-store
controls pass. These tests do not establish measured physical phase timing.

AR0-indexed STL/STH now admits low-byte modes `a8..af` and `b0..b7` for
opcode upper bytes `80..83`. The existing store path retains the address and
performs auxiliary modification in Read, then samples the selected accumulator
and writes once in Execute (SPRU131G section 7.1, EAB/EB store phases).
`store-index` passes 98,304 cases/restores: 96,000 RAM cases and 2,304 low-page
exclusions across all four selectors, both directions, eight AR fields, both
CMPT states, eight index values, four accumulator patterns, SST/SXM combinations,
two pages and three snapshots. Changing every AR after Access proves Read-owned
address/index selection; changing the selected accumulator after Read proves
Execute-owned data. A and B differ, and no destination read occurs. Replacing
ARs after Read also detects duplicate pointer updates. Low-page CPU/peripheral
destinations fail closed before update/write; this is not a silicon trap claim.
Instruction-driven producer and delayed-control composition, and physical bus
phase timing, remain unvalidated. Store-bypass and indexed-MAR controls pass.

MAX/MIN (`f486/f586/f487/f587`, SPRU172C 4-99/100) compares signed 40-bit
accumulators in Execute and replaces only the selected destination and carry.
Equality selects B and sets carry; SXM and OVM do not change comparison or
saturate the selected value. `minmax` retains 8,192 cases/restores:
all four selectors, 256 value pairs and eight status combinations. Pages/four
checkpoints are distributed, including equality tuples; their extra checkpoint
copies are retired. Sign-bit-biased unsigned ordering independently checks
signed 40-bit comparison. Guard-bit patterns distinguish signed 40-bit from signed 32-bit or
unsigned comparison. Replacing both accumulators after Read proves Execute
ownership; no data access occurs. This is not an instruction-driven adjacent
producer or interrupt-composition proof. Signed-XC and EXP/NORM controls pass.

MIN/MAX and accumulator SQUR use the shared clocked register-case runner.
Case data explicitly supplies the tick5 A/B replacement, tick6 observations,
bus-access prohibition and opcode address. The runner
rewinds the fetch observer separately from guest state; replay must not
duplicate a fetch. Arithmetic remains in the unchanged independent reference,
not the runner or a CPU implementation helper.

The dedicated atomic MIN/MAX and ROLTC 256-case sweeps are retired. The
shared literal reference bank retains the existing MIN/MAX interval examples
and adds four full-status MIN/MAX witnesses plus two ROLTC witnesses, both
destinations with opposed C/TC and guard-bearing inputs. These execute the
atomic backend with independent literal results and port-bracketed cost
checks; staged `minmax`/`rotate_pair` retain the broader numeric and replay coverage.
This deliberately removes the old atomic Cartesian products, not merely
their routing. Shared MIN/MAX/SQUR and SAT/rotate fixtures use ARP0 when
CMPT is clear; original ordered identities do not certify unchanged inputs.

Indexed MAR now admits the AR0 subtraction/addition forms (`6da8..6db7`).
SPRU131G table 5-4 defines ordinary 16-bit auxiliary arithmetic, not reverse
carry or circular indexing; SPRU172C MAR defines CMPT/ARP selection. Both
index and selected register are consumed by the existing Access-owned modifier.
`mar-index` passes 3,072 cases/restores: both directions, eight AR fields,
both compatibility states, eight indices, two starting layouts, two pages and
three snapshots. AR0 may also be the destination. Replacing every AR after
Access proves retirement and restoration do not reapply the update; no data
read, data write or extension fetch occurs. Adjacent AR0/CMPT producer latency
and physical phase timing are not established by this fixture.

`accumulator_branch_targets` owns BACC/FBACC target capture in 324 cases/replays:
both accumulators, three caller and upper-target pages, IRQ/NMI or DLD/READA
producers with gaps 0/1/2. The branch allocates no call frame; an interrupt
stacks only the selected target PC. Complete producer transactions, target
fetch/refetch counts, target execution, source poisoning and flushed sequential
TRN writes are checked. Four checkpoints are distributed instead of multiplied
into the old 1,296 executions. Observed pipeline clocks are model contracts,
not silicon timing measurements; phase-specific fixture/ACK routing is removed.

The firmware-only `nibble_composition` retains the relocated 23-word helper as
64 input/index/SXM combinations with distributed pages and independent nibble
selection expectations. It checks the original selected input/count bus ranges,
both count writes, IDLE, restored SP/AR1 and replay. It does not establish a
complete call-frame bus trace. No ROM image is needed; this composition remains
separate from generic CPU forwarding tests. The old 128-case profile is removed.

Immediate MAC (`f067/f167/f267/f367`, SPRU172C syntax 3) retains its literal
and Read-owned signed T operand through the existing Class-2 path. Execute
adds the signed product to the selected 40-bit source accumulator. FRCT shifts
the product by one; SPRU131G section 4.5.3 requires SMUL pre-accumulation
saturation only when SMUL, FRCT and OVM are all set. Final destination overflow
and saturation use the shared multiplier helper; T and carry are preserved.
The independent `immediate_product` section replaces both immediate-MPY/MAC
profiles with 4,224 cases/replays. Its 3,072 arithmetic cases cover all 64
signed operand pairs, FRCT/OVM/SMUL combinations, both destinations and MPY,
MAC-from-A and MAC-from-B. Its 1,152 transport cases cover eight STLM values,
both source/destination accumulators, SATD/SXM pairs and gaps 0/1/2 for all
three consumers. Arithmetic flags and literals are distributed in the transport
bank rather than multiplied across it; SXM is distributed in the arithmetic
bank because signed immediate multiplication does not use it.
Literal replacement and post-Read T mutation cannot alter retained operands.
Exact data traffic permits only the producer's T write at model clock 6;
no memory operand read or destination read/write is allowed. Per-clock state
predicates retain the publication boundary and status preservation through
IDLE/replay. Zero-gap producer behavior is a model contract, not a guaranteed
silicon schedule. Adjacent accumulator producer timing, repeat and interrupt
composition remain unvalidated. The 258,048-execution legacy machinery is gone.

`product_mapped_consumer` separately owns MPY→LDM interactions: 1,536 cases
cover all sixteen signed operand pairs, FRCT/OVM, producer and consumer A/B,
all six accumulator word fields, and gaps 0/1. A mapped Read at model clock
7+gap sees the published product field; AG/BG expose eight guard bits.
T is poisoned after its earlier sample and the producer accumulator after
the LDM Read. Exact one-Read/no-Write and one-literal-fetch checks survive
replay, and the destination retains the captured word. The two original
checkpoint cohorts are distributed rather than run as 3,072 cases. These
are model publication checks, not physical timing measurements; the legacy
profile, descriptor, setup and result phases are removed.

`immediate_mapped_consumer` covers 480 ADD/SUB/AND/OR/XOR → opposite-accumulator
LDM combinations: both source accumulators, all three fields, four value/literal
patterns, both SXM states and gaps 0/1. It checks the independently computed
40-bit producer result and captured field, exactly one mapped Read/no writes,
one retained literal, IDLE and replay. It does not claim an exact Read clock.
Program pages are distributed instead of duplicating the 480-case grid.
`rounded_byte_count` separately exercises a self-contained 13-word composition:
eight boundary bit counts, stack-relative destination pointer, mapped AL reloads
and a long store of `(bits+7)/8`. All seven data events are checked in order,
without asserting physical or model cycle timing. No private image or firmware
address is required. The old two-page duplication and shared phase chain are
removed, including the now-empty misleadingly named negate-result dispatcher.

Dual-memory ADD/SUB (`a000..a3ff`, SPRU172C syntax 6) uses the same retained
address/read machinery as dual MPY, but does not load T. Both operands are
extended according to SXM and shifted left by 16; their sum/difference replaces
the destination independently of its previous value. Execute publishes carry,
sticky destination overflow and OVM saturation through the shared arithmetic
helper. `dual_arithmetic` owns 45,056 cases/restores: 36,864 address cases and 8,192
arithmetic cases across ADD/SUB, all 1,024 encodings, three alias layouts,
linear/circular addressing, 64 value pairs and 32 status combinations. Numeric
pages/checkpoints are distributed; address cases retain the original page,
checkpoint and status cohorts. `dual_admission` separately owns the 32 ADD/SUB
low-page exclusions. Physical bank timing, repeat composition and general
producer latency remain unvalidated. MPY's signed product and T-publication
contract is independently executed rather than inferred from ADD/SUB.

Dual-memory MPY (`a400..a5ff`) captures both addresses before modifying either
auxiliary register. SPRU131G section 5.5.4 gives Xmod priority when both operands
use the same AR. Two logical reads supply signed 16-bit operands; X publishes T
in Read, while the product publishes in Execute with FRCT and destination
overflow/OVM handling. Execute does not rewrite a newer T value.
`dual_multiply` owns 22,528 cases/restores: an 18,432-case address matrix covers
all 512 encodings, three alias layouts, linear/circular addressing, two pages
and three snapshots; a separate 4,096-input arithmetic bank covers 64 value
pairs, both destinations and 32 status combinations, distributing pages and
checkpoints. These are factored matrices, not their full Cartesian product.
`dual_admission` retains 16 MPY exclusions for low-page CPU and
peripheral sources before reads or pointer updates. Physical bank arbitration
and repeat composition remain unvalidated; these exclusions are not silicon
trap claims. Both sections explicitly poison pointers/BK after address capture,
memory and T after Read, and require the two ordered reads at clock5 and result
at clock6. MPY publishes T while ADD/SUB preserves it. The replacement uses
fixture-owned integer expectations and the common replay runner, not the
legacy variant decoder or custom snapshot/tap machinery. No handset acceptance
is inferred from these focused runs. Parallel MAC and MACSU use the same independent section builder
as MPY/ADD/SUB. Its unsigned-X/signed-Y arithmetic and live feedback are checked
directly, not inferred from the other operations.

SPRU131G section 7.5.10 distinguishes direct accumulator consumers (no
pipeline latency) from memory-mapped accumulator reads. The zero-terminated
table walk uses adjacent `LDU *AR2+,A` / `RC AEQ` at `1c47..1c48`.
Its return predicate must consume the older load's Execute publication,
not the accumulator present at Access. The atomic zero-entry control
returns from `1c48` to `1c59` (SP `6f12`, AR2 `8002`, A zero), then
restarts the table walk through `1c54`. A synthetic nonzero terminator
would hide a producer/consumer timing error rather than repair the table.
`ret-load` passes 64 cases/restores: A/B, zero/nonzero results opposed to the old accumulator,
adjacent/one-NOP separation, two pages and four snapshots. Conditional return
now decides after older Execute publications and retains its stack sample.
Sequential prefetch preserves the existing taken/untaken cadence. The 768-case
isolated return fixture, conditional-call and stack/IRQ-return controls pass.
These checks do not establish physical bus-phase timing.
The following `BC ...,NC` (`f808 e6bc`) at `03:e6be` now executes through
the retained branch machinery. `carry-branch` passes 384 cases/restores:
C/NC, BC/BCD, both flag values, status-bit producers with gaps 0/1/2,
two pages, four snapshots and post-evaluation flag mutation. Branch target
and literal fetches remain exactly once. Arithmetic carry producers are not
covered by this fixture. TC, BIO and adjacent-load return controls also pass.
Long-offset BANZD `6eea ffff 949b` at `02:94c5` now shares the existing
retained offset-address and delayed-branch machinery. Offset `-1` and target
`949b` are separate retained extensions; the following `7006 0012` is one
two-word delay instruction, not another branch operand. `banzd-long` passes
4,608 cases/restores covering all eight registers, offset/preincrement/circular
forms, eight offset pairs, both CMPT states, two pages and both delay layouts.
Later literal/AR/BK changes cannot alter the captured decision or duplicate
the auxiliary update. No data-memory transaction is performed by the branch.
Physical stage timing remains unvalidated.

WRITA `7f00` at `02:8931` now executes as a retained data-to-program transfer.
Its wrapper loads A with `032e00`; common-window mapping projects that to
program RAM `2e00`. Source DAGEN belongs to Access, the data word is sampled
once in Read, and the program destination is retained after older Execute
publications. Four held clocks model Class 26's additional latency; their
bus-phase placement is provisional, not measured silicon timing. Later source,
AR, A or PMST mutations cannot redirect or resample the retained write.
The wait counter and mapped program destination are save-state registered.

`write_a` owns 280 independent cases/restores: DP, CPL, short postincrement,
absolute, offset, long preincrement and circular sources; all encoded
indirect registers, four value/destination patterns and both mapping states.
Pages and four checkpoints are distributed; unused direct/absolute AR
selectors are not crossed. It checks one source Read and one program Write,
no destination read, status preservation and physical program-RAM readback.
Source/pointer/A/OVLY changes after capture must not redirect the write.

`repeated_write_a` and `repeated_write_a16` each own 4,800 cases/restores.
All DP/SP/plain/decrement/increment sources, five count-source forms,
eight registers, counts 0/1/7/255, three target boundaries and both overlay
settings remain. Extended pages and four snapshots are distributed.
The 23-bit profile exercises ordinary, overlay-boundary and 23-bit-wrap
destinations; the 16-bit profile exercises ordinary, halfspace and 16-bit
wrap destinations, ignoring overlay in its address calculation.

Each count source is sampled once, then poisoned. The first source word and
logical PAR are retained before RAM/A poisoning. Every later source is
published only after the preceding program write; exact cross-space event
counts enforce Read(n) after Write(n-1) and before Write(n). Every actual
program-RAM destination starts poisoned and must contain the expected word
after count+1 writes and replay. RC, pointer progression and unrelated CPU
state settle independently. These are CPU-model ownership/cadence tests,
not physical program-bus timing measurements.

Compact Xmem arithmetic (`9000..93ff`) retains its RAM address and performs
one pointer update at Access, samples the word at Read and publishes ADD/SUB
at Execute through the shared arithmetic helper. It preserves the other
accumulator and updates carry/sticky destination overflow with SXM/OVM
semantics. `xmem-alu` passes 393,216 cases/restores over all 1,024 encodings:
16 Xmem modes, shifts 0..15, eight source/accumulator patterns, both
accumulators, eight status combinations, two pages and three snapshots.
Source-memory and AR changes after their sampling boundaries cannot change
the result. BK-zero linear and circular-wrap cases are included; mapped
sources and repeat composition are not established by this isolated fixture.
The original `9008` at `03:838b` executes inside the firmware's block-repeat
loop and advances to the current unsupported form; this is a bounded original
composition observation, not general block-repeat arithmetic acceptance.

Nonmodifying Xmem BIT can read the mapped AL/AH/AG/BL/BH/BG fields
(`08..0d`) through the same Access-address/Read-operand/Execute-TC path as
RAM. Bit index zero names the source word's MSB. AG/BG expose eight bits
with the unused upper word bits clear; they are not sign-extended fields.
`xmem_accumulator_bit` has 72 positive cases/replays across six fields,
indices 0/8/15 and four patterns. Poisoning both accumulators and the pointer
after the single Read must not change TC; unrelated status remains intact.
Six negative cases preserve rejection without a read or pointer change for
other mapped endpoints, modifying operands, non-BIT Xmem families and single
repeat. Four save points are distributed. The RAM `xmem_bit` section retains
the wider mode/index/TC-visibility contract; this addition does not qualify
new interrupt/control compositions. Disassembly now names the one-word BIT
encoding and preserves its operand mode and MSB-first index. The NSM-3
witness is `9630` at `76be`, reading AL through AR5=`0008`.

Compact shifted Xmem stores (`9800..9bff`) now present the retained write
address on EAB in Read and write EB in Execute, following SPRU131G section
7.1 and SPRU172C Class 10A. Source shifting and SST saturation use the
Execute-visible accumulator; this is distinct from a memory operand sample.
AR modification occurs once with the write address, before younger Access
uses the pointer. Mapped endpoints are restricted to the nonmodifying AR subset
described below; other low-page register/peripheral destinations remain excluded.

The current `xmem_store` section has 32,768 cases/restores across all 1,024
STL/STH encodings: 16 Xmem modes, shifts 0..15, eight 40-bit source patterns,
both accumulators and SXM/SST. OVM, compatibility/page and checkpoint choices
are distributed rather than fully crossed. The fixture changes
AR after EAB sampling and the source accumulator before Execute, then checks
the actual RAM word. SST, SXM and OVM vary independently; neither the source
accumulator nor status is changed by the store. There are no destination reads.
The Xmem subset of `store_forwarding` covers same-location store-to-load bypass
with zero/one gap, alias/nonalias addresses, six producers and both load
destinations. A bypass suppresses only the stale backing-memory read, not the
store's real Execute write. Physical bank arbitration remains unvalidated.

`xmem_ar_store` adds 96 positive cases/restores for nonmodifying stores to
AR0..AR7: both sources, low/high halves and shifts 0/1/15. Self-aliasing
pointer cases retain EAB before the pointer is poisoned after Read, while
the accumulator source is supplied only before Execute. A one-gap LDM
consumer sees the new register value. Exact write/read clocks, untouched
registers/status and four distributed save points are checked. Five negative
cases retain rejection before publication for SP/BK/accumulator fields,
modifying pointers and single repeat. These vectors use SST=0; broader
arithmetic policy remains in the RAM section. They do not establish a
zero-gap mapped consumer or physical bus phase. The native NSM-3 witness
is `9911` at uploaded `1ce7`, writing AR7 through AR3=`0017`.

DRSUB (`5800/5900`) uses the same retained paired-read path as DADD/DSUB,
but computes memory minus the selected accumulator. At original `02:8d23`,
`58f8 efb8` consumes the RAM pair `efb8/efb9` after a DLD from `efbe/efbf`.
The two words are sampled once in parity-defined MSW/LSW order, then the
shared long-word arithmetic helper publishes at Execute. C16 mode subtracts
the two lanes independently and does not saturate with OVM; full-width mode
uses SXM, carry, sticky destination overflow and OVM saturation.
`long_pair_arithmetic` owns 8,192 cases/replays: 2,048 each DSUB/DRSUB and
4,096 DADD, across eight read-capable forms, all accumulator selectors, eight
paired patterns, C16/SXM/OVM and both parities. Pages retain their original
parity association; three checkpoints are distributed instead of multiplying
to 24,576 executions. Exact paired Reads, Execute-only results/flags, extension
poison and later AR/SP/RAM changes are checked. The two legacy profiles,
factories, setup and result handler are removed. Short MOD=3 and low-page
admission remain separate from this positive section. Physical phase timing and
general producer/interrupt composition are not established by these fixtures.

NORM (`f48f/f58f/f68f/f78f`) retains T in Read before older Execute publishes
and decodes that retained count at Execute. SPRU172C 4-122 defines the
documented shift domain as -16..31. ROM4's Pow2 helper additionally uses raw
TS codes 32..47 as aliases of right shifts -16..-1, followed by conditional
SFTA -16 for larger shifts. Atomic and staged NORM share this decode; see
[native EFR](native_efr_codec.md#native-efr-decoder-acceptance) for the
Pow2 `d54e`/T=`ffe5` witness, independent reference result and limits.
The shared shift helper supplies SXM right shifts, sticky destination overflow
and OVM saturation while preserving carry and the unselected accumulator.
`normalization` owns 12,288 cases/restores over all selectors, 48 supported
shifts, eight 40-bit patterns and eight status combinations. Pages and four
checkpoints are distributed instead of crossed. Later T mutation cannot
change the captured count. `normalization_aliases` owns 4,096 cases/restores
over all selectors, 16 alias codes, eight patterns and eight status combinations,
with pages and two checkpoints distributed. Aliases are an inferred-model
regression, not a documented-ISA or illegal-operand test. Both use the common
clocked runner and a fixture-owned bit-position oracle, with no data traffic,
one NORM opcode fetch and Execute-only accumulator/status publication.

SPRU131G table 7-13 and example 7-40 require one separating word after EXP.
`exponent_normalization` owns 768 inputs/restores over all selectors, 16
patterns, gaps 0/1/2 and SXM/OVM. Pages and four checkpoints are distributed;
sticky-overflow seed parity remains pattern-owned. Gap zero checks the
prototype's stale-T projection, not a guaranteed hardware result for an
invalid-latency sequence. The original ABS/EXP/two-word move/NORM prefix at
`03:de97..de9b` has the required separation and now executes.

`exponent_forwarding` independently owns EXP/T forwarding: 1,536 cases and
restores retain all 16 values, both accumulators, LDU/no-producer, gaps 0/1/2
and eight status combinations. Pages and four checkpoints are distributed,
not fully crossed. It checks Execute T publication, exact producer and T
reads, preserved source/flags and a separately retained LDM sample after T
mutation. Adjacent LDM reads old T; separated LDM reads the new exponent.
The leading-bit expectation is fixture-owned, with no CPU arithmetic helper
or Nokia/private-ROM dependency. This is not NORM acceptance or measured
silicon timing. The custom EXP/NORM snapshot/tap harness and its descriptor-only
schedule census are removed; execution and restore checks now live in the
independently compiled sections.

Staged XC accepts the twelve single signed accumulator predicates GE/LT/NE/EQ/
GT/LE for A/B with XC1/XC2 protected spans. XC itself is a one-word opcode.
It uses the shared signed
40-bit comparator at Access, preserving the existing skip-word and two-word
interrupt-protection mechanisms. Carry/BIO/overflow and combined conditions
outside the previously supported TC/NTC/UNC forms remain unimplemented.
`xc-signed` passes 2,304 cases/restores: six comparisons, A/B, eight patterns,
three legal slot layouts, two pages and four snapshots. Predicate replacement
before and after Access distinguishes the owned sample from Decode/Execute;
guard-only nonzero and positive-bit-31 values prevent a 32-bit comparison.
The two-word layout includes an opcode-shaped literal and verifies that
rejected words never become a nested instruction or data write.

`xc-controls` retains 164 cases/restores covering unconditional IRQ
protection, arithmetic/IRQ composition and TC/NTC. The separate 48-case
EQ/NE subset is retired: `xc-signed` owns accumulator equality sampling,
including zero versus guard-only nonzero, source replacement around Access,
rejected stores and snapshot replay. This deliberately drops the subset's
accumulator-source store combination, snapshot6 and cumulative per-clock
write-count checks; it is not a claim of identical case coverage.
Signed-XC inputs now use legal ARP0 with CMPT clear, retaining low status bits.
This
guards the common mechanism; signed-predicate/IRQ composition is not separately
established. The original `XC 1,ALT` at `03:dea4` executes without a firmware
branch override. Aggregate test counts and the current execution boundary
are maintained above, not repeated for this instruction family.

The unchanged `aa88` upload contains two pertinent signed in-block forms:
`ff4a` at `03:da79` is `XC 2,BGEQ`, protecting `32f8 0009` (a two-word
ASM load); `fd4b` at `03:da85` is `XC 1,BLT`, protecting `ed11`
(`LD #-15,ASM`). Both spans finish inside `03:da71..03:da93`.
SPRU131G example 7-57 explicitly uses XC within RPTB. The frontend admits
its supported predicates when both interrupt-protected words remain inside
the block. Selected instructions and substituted NOPs preserve their original
block/end tags, including a two-word instruction ending at REA.

`block_xc` owns the supported block-slot execution contracts, including
the endpoint extension described below; the old revision-specific subset
counts are not separate execution owners.
`composition_limits` retains 18 XC cases/restores rejecting spans crossing the block
end before data access or pointer effects. This exclusion is a bounded model
boundary, not a claimed silicon trap.

A younger decoded XC extends interrupt protection: older retirement at the
same PC in a previous iteration must not release that younger span. Release
therefore checks the live Decode/Access/Read owners, using already-saved
pipeline state rather than a new timing counter. PC-only release can admit a
pending IRQ before the complete protected group retires.
The general bank in `block_xc_interrupt` passes 48 cases/restores for XC1/XC2,
three-/four-/five-word loops, counts 0/1/3/7 and IRQ/NMI. Pages and four
snapshot positions are distributed rather than multiplying the grid to 384.
The source is
asserted by the first protected RAM write; speculative opcode fetches are not
treated as Execute entries. The pipeline's overlapping groups finish before
one acknowledgement, vector fetch and stack write. The stacked continuation
keeps the original inclusive bound `(end+1)..(end+9)` rather than inventing an
exact physical pipeline PC. This is a declared pipeline-ownership
contract, not measured interrupt latency for physical NOP gaps. Restoring
IFR/NMI state does not resubmit an external line before scheduler settlement.

`xc_interrupt` separately retains all eight atomic XC/follower and
loop-wrap controls. A physical IRQ/NMI is asserted on the actual trigger-word
Fetch; a zero-valued normal RAM read in the ISR observes A at that bus boundary.
Frame words, two-word protection, loop redirection and RETE where applicable
are checked through save/replay. This replaces custom observer RAM callbacks
and is distinct from the pipeline contracts above.

Other crossing spans, predicated control transfers and silicon phase timing
remain outside this composition scope. Original-firmware execution is separately counted
in [Current boundary](5510_bringup.md#current-boundary), not inferred from static bytes.

Original recorder NAND data writes are observed at `02:891c`. NAND traffic alone
does not establish a valid recording, completed file, PCM or physical audio.
The atomic control is not a physical oracle.
The load at `02:9edc` (`1281`, source `002a`) uses a bench RAM-backed
gap whose physical ownership remains unknown. Output words
`0580,0a09` are not decoded audio. No complete staged boot, service
transaction or physical DA150 mapping is established.


### Absolute-alias frontend consistency

Memory AND/OR/XOR, scalar arithmetic and the `5xxx` long-memory arithmetic
family reject unadmitted `f9..ff` before operand effects in both frontends.
Negative controls exposed operand transactions and destination/status changes;
long-memory arithmetic changed ST0 from `1804` to `1004` before the guard.
These are fail-closed model contracts, not silicon-trap claims or positive
staged support for T-dependent arithmetic.

The CPU-only `source_admission` census owns the rejection coverage. Separate
legal controls remain in `logic-absolute`, `add-absolute`, `arith-ar` and
`long_pair_arithmetic`, which includes the positive DADD/DSUB/DRSUB contracts.
Repeat/interrupt composition and physical timing remain separate scopes.

## Retained-suite acceptance

`make check-c54x-core` executes the complete generic section catalogue,
sequentially. `tmp/c54x-section-trial/cpu-summary.json` records only successful
children and leaves `complete=false` on failure/interruption. The retired
default-chain captures do not qualify the replacement suite.

The [execution-model checkpoint](c54x_frontend_retirement.md#acceptance)
owns current full-suite counts and native integration results. The manifest,
not an old migration table, owns registration. Firmware code and copied programs
remain separate from generic CPU acceptance, including codec, RF and handset
claims. Mocked runners and helper tests do not renew native acceptance.

`instruction_observer` has twelve independent cases and save/replays covering
repeat/XC1/XC2, two logical pages and OVLY clear/set.
Four checkpoints are distributed. Repeat cases observe seven ordered
instruction entries, including three real repeat iterations; XC cases observe
only the selected instruction identities. Ordinary
program reads, extension words and annulled fetches produce no extra events.
Overwriting program bytes after fetch does not change the captured opcode.
The staged accelerated-repeat execution path is observed separately from
the normal Execute slot. This optional diagnostic callback is configuration,
not architectural state or a physical pin; it does not assert retirement
or an atomic snapshot of other registers. The section uses a bounded expected
sequence and replay cursor rather than saving a growing observation history.
The legacy observer module and phase routing are removed. A wrong expected
opcode fails; shared-fixture controls retain 1,730 memory-count and five
atomic repeat cases/restores. This migration does not renew handset/NAND
acceptance or the complete CPU suite.

The mapped-SP ADD checkpoint passes `add-sp` (4,096 arithmetic cases/restores,
576 stack-update contexts/restores and twenty control/peripheral exclusions),
plus the unchanged `add-absolute` and `add-bk` controls. The native 5110
save-state round trip and exact original MU4 resident-memory comparison pass.
Tooling acceptance at that checkpoint was 2,878 tests (one skip). These
isolated scopes are distinct from the current full default population above.

The direct-ISR-return checkpoint at `59f4285f` renewed the then-default population: all
288 required markers exactly once, expected completion exit and terminal
completion check. At that revision, `repeat-body-isr` passed768 cases and save/replays.
Controls `repeat-isr`, `repeat-context`, `repeat-pmst`, `repeat-popd`,
RPTBD IRQ, far-call and internal block-branch contracts have separate sections.
Tooling acceptance is 2,877 tests (one skip), and the native 5110
save-state round trip and exact MU4 resident-memory comparison (48,261
program and 3,656 data words) pass. These scopes do not establish complete
recording, physical loop timing or handset speech.

The earlier ASM-delay checkpoint at `930c486b` covers 30,720 cases and save/replays plus sixteen
partial-slot exclusions. Controls `asm-load`, `asm-mapped`, `asm-consumer`,
The retained delay-store exclusions now belong to `composition_limits`;
the sibling exclusion matrices are retired.
It also renewed exact MU4 resident-memory comparison (48,261 program and
3,656 data words); those additional isolated profiles are revision-bound.

The constant-DAGEN checkpoint at `026b6670` renewed the full default run: all 288 required
markers exactly once, expected completion exit and terminal completion check.
The separate `st-dagen` profile covers 3,840 positive cases, 80 exclusions
and 3,920 save/replays. Controls `imm-forward`, `st-extended-ar`,
`st-ar-consumer`, `st-t` and `stm-brc` also passed. None of these scopes
establishes complete recording or speech.

Earlier isolated renewals remain revision-bound, not implicitly rerun by
the renewed default population:

- `fd9e402c`: `dld-delay` (147,456), `dld-delay-edge` (sixteen),
  `load-delay`, `ld-delay-edge`, `add-delay`, `mac-delay-edge`,
  the delay-store bank in `composition_limits` and `rptb-back`.
- `40996160`: `mac-delay` (147,456), `mac-delay-edge` (sixteen),
  `mpy-delay`, the delay-store bank in `composition_limits`, `add-delay` and `fcala-delay`.
- `d1ea11b3`: `mac-bk` (98,304), `mac-smem` (73,728), `arith-bk`,
  `arith-ar`, `add-bk`, `add-delay`, `fcala-delay`, `mpy-mapped` and `mac-t-imm`.
- `5ad89352`: `fcala-delay` (648), the eight accumulator-call/branch
  profiles and producer controls, `far-call-delay`, `far-atomic`,
  `stack-irq-return` and `repeat-isr`.
- `1fe0df59`: full default run, all 93 named isolated profiles registered at
  that revision, and the seven additional scopes `--bus-registers`, `--bus-io`,
  `--atomic-io`, `--compose`, `--bus-stm`, `--mapped-read-a`, `--dp-absolute`.
  The separate atomic `bootreplay` control also passed bootstrap loading,
  pin-level status service and mid-byte save/replay with selected-state
  equality and unchanged NAND; this is not staged transaction completion.

Current isolated profiles are enumerated by `EXTRA_PROFILES` in
`tools/run_c54x_core.py`; the older 93-profile population is revision-bound.
A renewed default run does not imply a renewed run of every isolated profile
or of the seven additional scopes.
Each fixture retains its own opcode, composition, physical timing and bus
ownership limits. Latest tool counts, native 5110 restoration and original
resident-memory checks are recorded above; the recorder execution bound is in
[Current boundary](5510_bringup.md#current-boundary). SIP lifecycle unit tests require local
loopback socket permission. No aggregate scope establishes native speech.

Default-scope reproduction (serialize with other native runs):

```sh
mkdir -p tmp/native-gates
env TMPDIR=$PWD/tmp/native-gates python3 tools/run_c54x_core.py \
  --mame mame/mame --rompath roms --output-root tmp
```

## Recorder low-page copy

The first zero MVDD source has an explicit firmware path. Startup initializes
the object at `fe76`, including zero field `fe78`. The common-window callback
dispatcher passes that object's address to `03:8e91`; `71e1 0002 0013` at
`03:8e9d` reads `[AR1+2]` into AR3. Selector `05b8` reads zero, so the
comparison with two at `03:8ed2` does not take the TC branch. The firmware
sets AR2=`0e35`, executes `RPT #127` and reaches `e598` at `03:8eda` with
AR3 still zero. Do not infer a missing initializer from this zero alone.

The paired `bootrecstage` and atomic `bootrecsave` probes use identical
retained NAND bytes, the same acquired R060 image and the same pin-command
sequence. With a one-second stop bound:

- the pre-admission staged CPU at `046271df` rejected the first low-page
  MVDD before any source read; the mapped-source implementation now performs
  128 ordered reads and observes the subsequent `1800`, `1880` publications;
- atomic execution also starts that copy at zero, performs 128 ordered
  reads of `0000..007f`, then publishes `1800` followed by `1880` to `fe78`
  through the existing firmware buffer path (write context `02:1c96`);
- both observe selector `05b8` as zero on the first callback.

`tools/mu4_recorder_source_observe.lua` observes field, selector and low-page
activity without debugger support, memory reads of its own or DUT writes.
Attach it with `-autoboot_script /absolute/path/to/tools/mu4_recorder_source_observe.lua
-autoboot_delay 0` to the existing recorder commands, retaining
`-nonvram_save`. Compare the two NVRAM inputs byte-for-byte before the run;
the BIOS-specific NAND tags are `mu4nand_25` and `mu4nand_24`, respectively.
The paired one-second witness recorded `first_copy_reads=128`, `order_errors=0`
and `first_copy_before_nonzero=1`. The legacy observer's arm also qualifies
the selector read with a live-PC range, however, and its subsequent low-page
tap does not identify an instruction owner. Broader staged runs can mix other
MMR reads into that summary; it is not a copy-acceptance gate. Revalidation
needs retained MVDD Read identity, not a tighter guessed live-PC range.
Field-access totals likewise describe the observation window, not cadence
or acceptance constants.

This falsifies a staged-only source-pointer explanation. It does not prove
that a physical DA150 deliberately reads this page before its first buffer
is published: both frontends share board models and provisional mappings.
Do not seed `fe78` or suppress the callback. Mapped-source MVDD now has
49,152 scalar and 400 repeated save/replay cases, including CPU aliases,
read-clear peripheral endpoints and crossing the peripheral-window boundary.
Twelve exclusion cases reject CPU-register destinations, dual-peripheral
transfers and repeated peripheral writes before side effects. These use a
plain fixture RAM map: the older test map's `0060..0061` IRQ probes are not
ordinary low-page RAM. Focused MVDD acceptance and aggregate regression
scopes are recorded separately. Unknown physical register identities,
especially data `002a`, remain separate from transaction conformance.

For execution tracing, startup's instruction address is `0073f8`; the legacy
`pc=0073f9` was an advanced live PC, not the executed address. Debugger
disassembly of common-window PCs on nonzero pages
is not reliable: use the explicit loader projection of program records
(for example `02:1f83` to physical `001f83`), not a merged data/program
dictionary or a missing-word-as-zero fallback.


## NSK-1 zero-word execution boundary

The pre-correction own-input NSK-1 listed-FDN run observes 629 distinct first opcode words;
this is not the NSE-1 cold-boot inventory. `63f8` and `f166` require immediate
multiply disassembly (three and two words respectively). The decoder now
formats these existing execution forms, with the address extension before
the immediate. Focused disassembler/conformance tests pass; this is not a new
multiply-execution or native-radio qualification.

An execution-breakpoint observer across all 128 XPC pages confirms execution
at page-zero `29d3`, not merely a data read or a truncated nonzero-page PC.
Observed PMST is `ffac`; program words `29d3/29d4` and data word `29d3` are
all zero under the old `2800` overlay extent. First captured SP values are `1eab` and `1ea7`, with stack heads
`3798/00fa/3073/3073` and `38ca/3976/3923/30bc`. Zero data reads do not
exclude discarded uploads: `program_w` ignores writes outside the overlay.
Live disassembly identifies `3798` and `38ca` as return addresses immediately
after calls to ROM routine `a51b`. A bounded instruction trace follows
`a539 -> 24cf -> 26c3`, the return-address comparison against `a53b`, and
`26d0 B 29d3`. A program-write observer sees 811 writes in `2800..2b2a`,
including `29d3=7714,29d4=085e,29d5=7194,29d6=1f2b,29d7=1284` at
approximately 0.150281 s. The old mapping discards this own-firmware code.
The NSK-1 research configuration selects a `3000` overlay extent; this is
an upload-derived minimum bank model, not fitted-mask
identification or proof of the full physical RAM size. Other products are
not changed by this observation.
The corrected 65-second run passes `nsk1_native_upload_check.check_upload`,
the existing native boot/storage predicates, physical listed-FDN dial/SIM
checks, unchanged whole-card comparison and the previous exact digit/progress
frame hashes. The zero-entry probe no longer fires. No registration, connected
call, speech or full physical-memory qualification follows from this fix.
The corrected run observes 636 distinct first opcode words, all with decoded
boundaries (including observed extension variants); opcode `0000` is absent.
This is decoding coverage, not independent semantic proof for every instruction.

Reproduction uses `make_c54x_entry_script.py 0x29d3 OUTPUT`, copied as
`native_entry.cmd` into an isolated qualified NSK-1 FDN-enabled cold run, and
`nsk1_native_entry_observe.lua`. Its 65-second run retains the qualified
boot/storage predicates, physical listed-number input, SIM conversation,
unchanged whole card and exact digit/progress frames. Debugger `focus` alone
does not select the command expression CPU: the observer explicitly sets
`visible_cpu`, then verifies installation of all 128 DSP breakpoints.
Queued headless startup scripts or failed breakpoint installation supply no
negative execution evidence. The bounded counter is debugger-host state only.

## Native MAR assertion attribution

Missing exact-word tags in the default fixture do not imply missing semantic
coverage across isolated profiles. `mar-index` already checks 3,072 cases
plus save/restore replays against independently calculated 16-bit AR updates;
`mar-reverse` checks another 3,072 cases/replays using a bit-serial oracle.
The architectural Access update now emits exact-word assertion attribution:
`6da8..6db7` for indexed MAR, `6da0..6da7` and `6db8..6dbf` for reverse carry.
Fresh verbose profile runs require both the existing completion gate and
execution evidence for each attributed word. CPU execution is unchanged.

The corrected NSK-1 trace contains indexed words `6db1/6db2/6db3/6db7`,
with counts 5/69751/69751/2244: 141,751 executions now link directly to the
isolated semantic fixture. This is not a new all-opcode closure, physical
pipeline timing proof or evidence about arbitrary producer compositions.
Use `c54x_opcode_coverage.py --additional-fixture-log` to combine separately
completed profile evidence; never manufacture execution credit by combining
an assertion-only run with a handset's execution trace.

## Native dual-memory subtraction

The corrected NSK-3 long trace executes `a245` 69,751 times. This exact
encoding is `SUB *AR2-,*AR3-,A`: both pointers post-decrement, unlike
the separately tested mixed-direction `a249` form. The isolated
`dual-sub-native` profile checks eight source patterns in each operand,
with SXM and OVM independently clear/set (256 cases, each replayed from
save state). It checks the shifted signed/unsigned difference, guard bits,
32-bit overflow/saturation, no-borrow carry, unchanged B/T/status controls,
unchanged source RAM and both pointer updates. Initial overflow flags are
clear; sticky overflow, other registers/encodings, staged pipeline ownership
and physical cycle timing are outside this fixture's claim.

```sh
.venv/bin/python tools/run_c54x_core.py --mame mame/mame \
  --rompath mame/roms --profile dual-sub-native --verbose \
  --output-root tmp/native-dual-sub
```

The qualified run contains the `a245` assertion and 512 actual executions.
No CPU arithmetic implementation change is needed. The default core runner
separately requires every marker in `run_c54x_core.REQUIRED`; its assertions
alone must not be promoted to exact execution attribution without an opcode
inventory.

The runner owns a separate process group and reaps it on SIGTERM,
KeyboardInterrupt or wait failure; interruption is never a conformance pass.
`exit_status.json` retains the actual child return code, command and
`interrupted` flag, including interrupted runs that reach cleanup. No manifest
or partial PASS prefix establishes completion. An uncatchable launcher kill
still requires external orphan cleanup. Profile runs qualify only their named
matrix, not the default full-core set or native handset speech.

## Paired-memory assertion attribution

Paired CPU ownership is now the independent DLD/DST sections described under
Circular Fixture Domains. Historical `indirect-dld` profile assertion counts
are not acceptance for these replacements or for native handset execution.
The sections check exact opcodes, bus words/order, retained operands and CPU
state through replay. Private-ROM attribution remains a separate contract.

`paired_store_forwarding` owns 1,152 cases/restores: DST from A/B, high-word,
low-word and DLD consumers, direct/short/absolute producer addressing, four
source patterns, SST/SXM, zero/one NOP gaps and same/unrelated source pairs.
Pages and pre/post-publication checkpoint choices are distributed. At zero
gap, an aliased consumer uses the internal bypass with no RAM reads; other
consumers perform exactly one/two reads in the complete ordered bus trace.
Both physical writes remain mandatory, and final RAM and accumulators must
match the independent saturation/sign-extension oracle. This contract checks
transaction order, not exact bus-clock placement or physical contention.

## Mapped move and younger repeat ownership

Decode can arm an RPT header while an older MVDK remains in Access. A global
repeat-active flag does not make that older instruction a repeated body.
The mapped-source/destination MVDK exclusions apply only when the body is
latched and the instruction's full program address matches that body.
They still reject genuinely repeated mapped moves before source/pointer or
destination effects. No new mapped-repeat execution semantics are admitted.

`mapped_move_repeat_owner` owns 32 independent cases and cleared-pipeline
save/restores: `MVDK TRN,AR2` before short/long RPT, the same move as the rejected
body, counts 0/3 and two values/pages; two checkpoints are distributed.
Exact mapped Read/Write transactions are required for the older move and
forbidden for the rejected body. The pre-fix core rejects
the ordinary predecessor before any body is latched. The fixture establishes
instruction ownership, not physical pipeline-phase accuracy.

## Mapped source in parallel Store/Load

ST||LD may read X addresses `0000..005f` while Y remains ordinary RAM.
Owned registers use their existing read interfaces; gaps `0002..0005` and
`001f` delegate to the board map. X is retained in Read, whereas the store
uses the Execute-live old accumulator before load replacement. Mapped AR
sources observe the existing Access-stage modifier model. Loading T retains
its Read publication and cannot overwrite a newer T in Execute.

`mapped_parallel_load` owns 24,576 cases with cleared-pipeline replay: all six A/B/T forms, all 32
source addresses, eight nonaliasing selector pairs, two patterns, SXM/SST/OVM,
ASM=0/-1, with pages and three checkpoints distributed rather than crossed.
Source words are explicit pre-Read state, including modified ARs; no CPU peek
supplies an expected value. It checks one read and one RAM
write, exact pointer updates, late accumulator/ASM/T changes and preserved
carry under the existing explicitly unvalidated policy. IMR/IFR reads do not
acknowledge interrupt state. Same-pointer aliasing, other ASM values and RAM
numeric boundaries remain in `parallel_load`. Mapped Y writes and
mapped single-repeat bodies remain excluded. Physical
timing and arbitrary block/interrupt composition are not proved here.

The acquired EFR loop uses AR5 as a postincremented counter while `c8ba`
also performs its X read; atomic and staged traces agree on that deliberate
low-address sequence. Qualified peripheral reads let that loop complete.
The EFR encoder passes all 163 complete reference frames:
9,291 parameters, 326 VAD/SP flags and 2,771 packed output words, including
all 65,440 ordinary RAM words on every frame's save/replay. The repeatable
gate is `make check-c54x-rom4-efr-encoder-staged`; it verifies the independent
ETSI fixture and retains strict comparison requirements. FR
encoder and decoder retain their separate 32-frame controls. These isolated
DTX-disabled algorithm results do not prove handset speech or physical bus
timing; DTX has its own acceptance below. The staged decoder separately
passes 163 clean speech frames and all 26,080 PCM samples, with full-RAM
replays, through `make check-c54x-rom4-efr-decoder-staged`. Its existing
ten-case delayed-repeat preflight, decoder reset and every speech frame use
the same stage-owned engine, with an explicit execution marker.

Both staged EFR homing suites also pass their independent 46-frame fixtures
with full-RAM replays: encoder 2,622 parameters, 92 flags and 782 packed
words; decoder 7,360 PCM samples. Their reference includes six homing frames
and two matching twenty-frame speech segments. The decoder additionally checks four
steady encoder-homing outputs. Acceptance targets are
`check-c54x-rom4-efr-homing-encoder-staged` and
`check-c54x-rom4-efr-homing-decoder-staged`.

Staged DTX encoding and decoding pass all 518 frames: 29,526 parameters,
1,036 flags and 8,806 packed words match on encoding; 82,880 PCM samples,
eight SID frames, 135 erased frames and 46 nonzero comfort-noise frames match
on decoding. Both check full 65,440-word RAM replay for every frame.
Acceptance targets: `check-c54x-rom4-efr-dtx-encoder-staged` and
`check-c54x-rom4-efr-dtx-decoder-staged`. Atomic runs remain controls.

The independent invalid/bad-SID conversation also passes all 518 frames on
the staged frontend: 82,880 PCM samples, four invalid codewords, one bad valid
codeword, per-frame invalid-SID control checks and full-RAM replay.
`check-c54x-rom4-efr-invalid-sid-staged` uses the same hashed inputs, reference,
program and cold data as the atomic control. Its dedicated frontend marker
is mandatory; this is decoder-entry coverage, not RF SID classification.

The mapped AH
multiply-accumulate at `fc00` (`6709`, literal `f38b`) is now covered by
the mapped memory/immediate MAC contract below.
Short staged BIOS aliases preserve MAME's sixteen-character limit. Fixture
parity tests require identical hashes in paired atomic/staged regions, and
the runner rejects MAME driver-validity errors even if a completion marker
is printed afterward.

Both EFR frontend BIOSes must load the same hashed PCM and reference-vector
files; program/data ROM coverage alone does not establish codec-fixture
coverage. A fixture-declaration regression enforces this pairing. With these
inputs loaded, verbose first-frame-entry snapshots agree on all 65,440
ordinary RAM words after reset and PCM publication. Younger Access/Read
publications make raw pre-Execute register equality an invalid pipeline
equivalence test. Verbose tracing retains the independent vector
acceptance requirement and does not alter the ordinary gate.

MVDD's retained Read sample bypasses a pending same-location RAM write to
an ordinary following load. The acquired example is CALLD `d392`, final
delay-word MVDD `d395` (`e5a1`, `ed8a` to `006e`), then Xmem load `d40d`.
Without bypass the load retains previous `006e=7ff8` despite MVDD having
sampled `7fd3`; the resulting low-word store to `0071` is `e000` instead of
`4c00`. The earlier read of `006e` belongs to the first delay-word DELAY,
not the callee: PC context alone is not an operand-ownership annotation.

`mvdd-follow` passes 1,536 cases and cleared-pipeline replays: adjacent and
both CALLD delay slots, Smem/Xmem loads, all four source modifiers, four
boundary values, zero/one gap, two program pages and four snapshots. It
checks retained source after poisoning, new-value consumption, exactly one
source read and copy write, balanced return and preserved sibling state.
Disabling only this bypass makes the fixture fail on stale RAM. Physical
pipeline phases and arbitrary peripheral/interrupt compositions are not
claimed. Repeated MVDD deliberately remains outside this bypass: its
overlapping RAM matrix qualifies Read-before-older-Write ordering separately.

Ordinary mapped ADD/SUB must likewise retain the older AR operand before a
younger Access update. In the acquired case ADD `b9b4` (`0013`) precedes
parallel store/load `b9b5` (`c8d8`): consuming the incremented AR3 wrongly
stores `0fb8=1` instead of zero. `addsub-ar-age` passes 18,432 cases and
cleared-pipeline replays: ADD/SUB, DP/indirect/absolute/offset forms, all
eight ARs, both accumulators, four boundary words, MAR increment/decrement
and MVMM writers, both SXM states, two pages and three snapshots. A zero
initial destination isolates operand age and extension from unrelated ALU
producer hazards. Absolute ADD's separate decode path is included.
Carry/TS/shift-16 arithmetic, RMW, logic and dual-read age compositions
retain their separate contracts; this does not admit them implicitly.

`shifted_peripheral` owns 2,520 cases/restores: 1,280 numeric boundary cases
at `0020/005f` and 1,240 interior address/form/shift cases. It retains the
previous four shift boundaries, both numeric destinations, all flags and word
patterns at the boundaries, and unsigned `8000` interior controls. Pages and
three checkpoints are distributed instead of 11,400 executions. Synthetic
source words change on Read, and retained literals/pointers are poisoned;
these are bus-class contracts, not endpoint identities or physical clock claims.

At `0020` the board endpoint is BSP receive data, whose ordinary read clears
receive-ready and overrun; debugger reads must not acknowledge either.
`shifted_bsp` owns 5,120 cases/restores: all 32 shifts, five addressing forms,
SXM/OVM, both accumulators and four words. Pages and four checkpoints are
distributed instead of 40,960 crossed executions. It poisons retained literals,
replaces the postincrement pointer before Read, checks peeks before/after
acknowledgement and uses the same BSP word-state attachment as the parallel bank.
`bsp_parallel_load` owns 768 cases with cleared-pipeline replay, replacing
6,144 page/checkpoint-crossed executions:
six forms, eight distinct X/Y selector pairs, two signed-boundary patterns,
SXM/SST/OVM, with pages and four checkpoints distributed across Access, its extra wait,
Read and Execute. It checks one acknowledged source read, one RAM store,
retained ASM, Execute-live accumulator data, Read-owned T publication and a
second frame left pending. The standalone attachment uses the shared BSP
completed-word model, not a constant read stub; control, received word, ready
and overrun participate in the actual machine snapshot. Omitting acknowledgement
or saved overrun state fails the native run. Serial-pin state is not exercised
by this word-level attachment. The old descriptor, setup, result phases and
profile routing are deleted. The staged wait classifier includes this
parallel source. Destination MMIO, mapped single-repeat bodies and simultaneous
peripheral arbitration remain separate exclusions. The extra clock is a
documented instruction cost, not a measured silicon pipeline phase; the
parallel carry-preservation policy remains unvalidated.

`peripheral_parallel_load` owns 49,152 cases with cleared-pipeline replay,
retaining all six forms, eight selectors, two patterns and status modes across
all64 addresses `0020..005f`. Pages and four checkpoints are distributed,
not crossed into393,216 executions. A stateful synthetic endpoint checks one
sample at Read with the extra peripheral clock, one RAM store at Execute,
and no resampling after the endpoint changes. Its word toggles on an actual
read, debugger peeks are nondestructive, and the endpoint word is restored
alongside the test's bus-observation state. Disabling the toggle or restoring
CPU state without the endpoint state must fail. This is bus-delivery and
instruction-ordering evidence, not an endpoint-identity claim: the fixture
supplies the read value even at CPU-owned timer/clock addresses. Actual
register semantics retain their separate gates. On the handset board,
`0021` reads BSP transmit data without acknowledging it, `0022` reads BSP
control, `0024..26` are CPU-owned timer registers, and unowned gaps delegate
to the board map. The BSP receive side effect is qualified separately above.

## Short T loads in ordinary delayed controls

Short RAM `LD Smem,T` may occupy either delay word of BD, BCD, CALLD,
FCALLD or RETD, in addition to the separate RPTBD/BANZD contracts. Access
owns the address and pointer modification; Read publishes the sampled word
to T. Execute must not overwrite a newer T, and the selected continuation
must consume the resulting T through the ordinary multiply path.

`control_delay_t_load` passes 1,152 independent cases and cleared-pipeline replays: six control
outcomes, both slots, direct/plain/postincrement/postdecrement sources,
four patterns and both multiply destinations. DP/CPL, flag combinations,
pages and checkpoints are distributed. Three cohorts retain the loaded T or
replace it at Read with its complement or `5aa5`. Source/opcode/target
poisoning, one source read, target products, pointer effects and owner-only
stack transactions are checked. CPL addresses account for each control's
SP change; BCD's second delay position uses the existing prototype control
timing helper, not a newly measured silicon phase. The mapped/peripheral,
extended-address and arbitrary interrupt/producer compositions are not
qualified by this RAM fixture. The shared legacy fixture/setup/checker/routes
are removed. Replacing the newer-T mutation with the old value fails the
replacement's T-publication assertion; no CPU change is retained.

## Mapped memory/immediate MAC

Memory/immediate MAC admits mapped accumulator fields `0008..000d`,
T/TRN and AR0..AR7. Read retains the mapped word and publishes it to T;
Execute combines that retained word and literal with the live selected
accumulator. Source and destination accumulator selectors remain independent.
SMUL clamps the fractional product before accumulation only in its existing
fractional-overflow mode; result saturation and sticky OVA/OVB remain
destination-owned. No data-memory write is synthesized.

`mapped_immediate_mac` uses the common CPU-only section runner for 18,432
cases/replays. A numerical group crosses four words, eight literals, all
FRCT/OVM/SXM/SMUL combinations and all accumulator selectors on AL. Routing
crosses all sixteen mapped addresses and four addressing forms with four
patterns, all flags/selectors and distributed literals/pages/checkpoints.
Guard fields retain eight-bit width. The full arithmetic/address/literal
Cartesian product is deliberately not retained.

A/B change after Read to distinguish retained mapped data from the
Execute-live addend, including same-accumulator aliases. T, TRN, every pointer
and literal words are poisoned after capture. Exact source Read, Read-owned
T, Execute-only arithmetic/sticky overflow, no Writes, extension retention and
cleared-future-pipeline replay are preserved. This is implementation ordering,
not measured silicon timing. The old 262,144-input owner, independent timer,
snapshot/counter machinery and enumeration-only schedule test are deleted.
Focused build/run: 6.051/7.023 seconds; negative expectations are rejected.

The `memory_immediate_product` RAM/MPY section separates numerical and routing
coverage as described above. The implementation-limit controls for the
remaining eighty low/control/peripheral addresses and eight write-only MOD=3
forms run separately in `source_admission`: 704 cases/replays retain all selectors
and pages, distributing the original checkpoints instead of duplicating them. Producer, control, repeat and
interrupt compositions are not newly qualified, nor are physical bus phases.

## Parallel Store/Multiply

The staged frontend admits `ccxx..cfxx`: either accumulator as store
source and either accumulator as multiply destination. X/Y capture and
modification share the parallel-store path. Read retains X, T and ASM;
Execute stores the old live accumulator before replacing the selected
destination with signed X times T. FRCT doubles the product, OVM applies
to the result, and SMUL does not apply the MAC-family product clamp.
The atomic model's carry-preservation policy remains silicon-unvalidated.

`parallel_multiply` owns 14,336 cases with cleared-pipeline replay: 6,144
encoding/layout/BK cases, 4,096 ASM/SXM/SST store cases and 4,096 numeric
word/T/FRCT/OVM/SMUL/sticky cases. Pages/checkpoints are distributed; full
address/status/seed and arithmetic/store correlations are deliberately reduced.
All 1,024 encodings and 32 ASM counts remain. Setting SMUL in the numeric
bank must not add a product clamp to this instruction.
T and source memory change after Read;
the accumulator changes before Execute and ASM changes after capture.
Mapped/peripheral operands, arbitrary producer/
interrupt/repeat composition and physical bus phases are not qualified.

## Memory SQUR in delayed controls

Short RAM SQUR Smem,A/B (`26xx/27xx`) may occupy ordinary control and
RPTBD delay words. Read retains Smem and publishes it to T; Execute replaces
the selected accumulator with the signed square. FRCT doubles the product,
OVM clamps the result with sticky overflow, and SMUL does not clamp the
product. This differs from SQURA/SQURS, which accumulate into the old value.

`delayed_memory_square` owns 288 independent cases/restores across BD,
both BCD outcomes, CALLD, FCALLD and RETD; both destinations; DP/plain/
postincrement RAM and eight patterns. FRCT/OVM/SMUL/SXM, page and three
checkpoints are assigned, not fully crossed. `rptbd-squr` passes 92,160 cases and cleared-pipeline replays:
first/second/both-same/both-split layouts plus SQUR followed by MPYA,
with the same addressing/pattern/mode dimensions, outer counts 0/1/7 and
four checkpoints. The mixed case consumes the correct signed 17-bit A
high word at MPYA Execute. Per-clock results/status, one read per source,
pointer/opcode retention, Read-owned T followed by a newer T, and completed
results observed by every outer-body copy are checked. Extended/mapped
sources, arbitrary interrupts and physical bank phases remain unqualified.

## Square accumulation in ordinary delayed controls

Short RAM SQURA/SQURS forms may occupy ordinary delayed branch/call/return
spans as well as the independently qualified RPTBD delays. Read retains
the signed memory operand and publishes it to T; Execute adds or subtracts
its square from the selected live accumulator. FRCT doubles the square,
OVM applies to the result, and SMUL does not clamp the product.

`delayed_square_accumulate` owns 576 independent cases/restores: BD, both BCD
outcomes, CALLD, FCALLD and RETD; direct, indirect and postincrement RAM;
both operations and destinations and eight patterns. FRCT/OVM/SMUL/SXM,
page and three checkpoints are assigned. It checks one retained read, pointer
updates, T publication followed by a newer T value, per-clock accumulator
publication, result/status/stack preservation and target observation.
BCD fixes A to satisfy its branch condition, so those cases do not vary
A independently. Extended/mapped sources and arbitrary interrupt composition
remain unqualified. The existing RPTBD square matrix and mapped-repeat
exclusions remain separate controls; physical silicon phases are not proved.

## Rounded memory multiply in delayed controls

Complete MPYR Smem forms may occupy ordinary delayed branch/call/return
spans. The signed memory and T operands are retained in Read; Execute rounds
the product by adding `8000` and clearing its low word, then applies the
existing result-overflow/OVM rules. FRCT doubles the product before rounding.
MPYR does not use the MAC-family SMUL product clamp and does not publish T.

`delayed_rounded_multiply` owns 768 independent cases/restores over BD, both BCD
outcomes, CALLD, FCALLD and RETD; postincrement, absolute RAM/AR7 and offset;
both destinations; sixteen signed operand pairs including positive/negative
rounding ties. FRCT/OVM/SMUL, page and three checkpoints are assigned, not
fully crossed. Memory, T,
pointer and literal poisoning reject resampling, while the target observes
the completed result and stack effects remain exact. Other
addressing, interrupt composition and physical pipeline phases remain
unqualified; ordinary MPY/MAC fixtures are separate controls.

## Low-address dual ADD/SUB operands

Dual ADD/SUB admit IMR/IFR (`0000/0001`) as Read-owned, non-acknowledging
sources, and delegate `0002..0005` to the existing board map. No read constant
or peripheral identity is introduced for those unowned words. ST0/ST1 and
other low control/peripheral sources remain guarded, as do low sources for
multiply/MAC/SQDST.

`interrupt_dual_alu` passes 384 independent cases and cleared-pipeline replays over IMR/IFR,
X/Y/both placements, eight selector pairs, ADD/SUB, both accumulators,
and BK=0/32. SXM/OVM, two payloads, pages and checkpoints are distributed
rather than multiplied across every address/selector case. Reads leave
IMR/IFR intact. Post-Read poisoning changes both registers to a disjoint
mask/pending pair, avoiding an unrelated IDLE wake, while Execute must use
the retained operands. This does not qualify concurrent interrupt arrival.
`low_bus_dual_alu` passes 768 corresponding cases over all four unowned words,
with mutable board-map values and post-Read poisoning. Address modifiers and
alias priority retain the existing dual-operand model; physical bus phases
and producer/control hazards remain unvalidated.

Both acquired-ROM frontends deliberately set AR5=0 via `8815` at `d43b`
after clearing A at `d436`. `a0b8` at `d45b` uses it as a postincremented
operand address/counter. The staged stops at values 0 and then 2 therefore
identified missing operand domains, not an unexplained zero pointer. The
frontends already differ in other data values here; this observation is not
state or EFR-result parity.

## Square arithmetic in RPTBD delay words

Short SQURA/SQURS RAM forms may occupy either or both RPTBD delay words.
Each source address and pointer modifier is owned by Access, memory/T by
Read, and the selected accumulator with sticky overflow by Execute. The
second square sees the first square's result when they share a destination.
FRCT/OVM apply; square products do not use the MAC-family SMUL product clamp.
Mapped/peripheral sources, extended addressing, other control owners and
interrupt composition remain outside this qualification.

`rptbd-square` passes 147,456 cases and cleared-pipeline replays: SQURA/SQURS,
A/B destinations, first-only/second-only/both-same/both-split layouts,
DP/indirect/postincrement, eight patterns, FRCT/OVM/SMUL/SXM, counts 0/1/7,
two pages and four checkpoints. Per-clock accumulator/flag checks cover
sequential saturation. Retained opcode/source/address checks and a newer T
value reject resampling or T republication. Both accumulator results are
stored by every count+1 body iteration, while the delay work occurs once.
This qualifies the declared staged ownership model, not physical bus phases.

## BRC memory moves in ordinary delayed controls

MVDM may also read mapped BRC into AR0..AR7 through the ordinary retained
Read operand and early AR-publication path. This is a read of BRC, not an
assignment to the block owner's counter. `brc_to_ar` checks 32 cases/restores
across all AR destinations and four words, changing BRC immediately before
and after the prototype Read clock. It requires the captured word at the
destination, unchanged other ARs, RSA/REA/RC, accumulators and status, and
exact mapped bus transactions. Five negative cases preserve exclusions for
RSA/REA/PMST sources, a genuinely repeated mapped copy and the unqualified
BRC-to-T form, without claiming a hardware trap. Four save points are
distributed. This does not newly qualify active-block or interrupt timing.

Complete two-word MVMD/MVDK-to-BRC forms may occupy ordinary delayed
branch/call/return spans. Their existing source sample and zero-latency BRC
publication remain owned by Access. RPTBD's delay span and active block-body
BRC writes remain excluded; allowing an ordinary control delay does not
license modification of a block owner's count.

`move-brc-delay` passes 1,800 cases and save/load replays: BD, taken/untaken
BCD, CALLD, FCALLD and RETD; DP/postincrement and mapped AR/AL source forms;
counts 0/1/2/7/127; subsequent RPTB/RPTBD; two pages and three checkpoints.
Source poisoning cannot change the retained count. The resulting loops copy
exactly count+1 words, and call/return frames remain separately owned.
SP-relative sources, extended addressing, interrupt composition and silicon
pipeline timing are not qualified by this matrix. Existing ordinary-source
and block-body/RPTBD-delay exclusion fixtures remain separate controls.

## XC protection across a selected near return

TI [SPRU172C, XC description](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)
states that XC and the two following instruction words are uninterruptible,
including for XC 1. In the staged model, a selected plain RET in the first
following word redirects the remaining protection endpoint to its return
target. Leaving that endpoint at the abandoned sequential address would
retain protection indefinitely. A later ordinary status write must not be
misclassified as an XC-controlled instruction.

`xc_return` passes sixteen cases and cleared-pipeline replays: XC 1 BEQ/BNEQ,
selected/annulled returns and one-/two-word resumed instructions, with
none/IRQ/NMI on selected paths. Two pages and four checkpoints are distributed;
unasserted interrupt variants on annulled paths are removed. Interrupts arrive
only on selected
return paths, at fixture clock 7. The resumed write precedes interrupt
stack/vector effects; the original stack target is sampled once and poisoned
after Read. No-interrupt runs also execute a later ordinary STM ST1, beyond
the protected word's retirement. Adjacent-status admission, other return or
control-transfer forms and silicon interrupt timing remain unqualified.
This validates the declared resumed-word protection model, not an independent
measurement of the physical return pipeline. Exact ordered bus events include
the model-selected IRQ continuation and vector fetch; IRQ acceptance requires
the return Read and protected Write to have completed. A wrong bus-progress
expectation fails. The old 192-case profile, handlers and now-empty control
source file are removed.

Save/load fixtures must not queue synchronized input changes immediately
before `read_stream`: restoring scheduler timers does not restore the pending
`device_input` event queue. Saved IFR/NMI/entry state must not be followed by
a duplicate assertion of the original physical event.

## Staged successor memory copy

Short `DELAY Smem` (`4dxx`, low byte below `98`) retains its source address
and applies its pointer modifier in Access, samples one RAM word in Read,
and writes that retained word to source+1 in Execute. It does not modify
A/B, T/TRN or arithmetic flags. Compiler-mode DP/SP and compatibility ARP
selection use the existing address-age model. Mapped operands and a source
at `ffff` fail before reads, writes or pointer updates.

The same staged path now also handles LTD (`4cxx`), with T published at Read,
and absolute/long-offset LTD/DELAY. The retained literal sections qualify
stationary, postincrement, compiler-SP, absolute and long-offset successor
copies with independent T, pointer and source/destination results. Their
short matrix is not a full long-address matrix. Repeat/block/XC composition
and physical bus arbitration remain outside this qualification.

LTD's T publication follows older Execute, like other Read-owned T producers;
the source bus Read itself remains earlier in that same clock. An adjacent
STLM-to-T/LTD case in `memory_literals` fails on the former early
publication, checks T at clock 7, and restores there before the retained
successor copy writes at clock 8. The 24 original cases remain (25 current).

ABDST now shares SQDST's retained dual-source path. It adds the magnitude
of old A's signed high-17 field to B (doubled under FRCT), then replaces A
with `(X-Y)<<16`, with the documented status effects (SPRU172C page 4-2).
Two retained literal cases include the TI example and its fractional contrast.
SUBB retains unsigned Smem, then subtracts it plus inverted carry at Execute.
DADST/DSADT/DSUBT use retained paired Reads and a separately saved T latch;
all 33 long-word literals remain, plus one after-Read T-poison/replay check.

`delay-copy` passes 15,360 cases and replays over DP/SP, indirect,
postdecrement and postincrement, all eight AR selectors, eight patterns,
SXM/OVM, compatibility, two pages and three checkpoints. Post-Access pointer
poisoning and post-Read source poisoning cannot redirect or change the copy.
`delay-copy-ctrl` passes 73,728 cases and replays over BD, both BCD outcomes,
CALLD, FCALLD and RETD. Its four address forms exclude SP-relative data;
the control owner retains its independent stack transactions. This is the
declared staged operand-age model, not a physical pipeline timing claim.

## Accumulator-source multiply in control-delay words

Short MPYA/MASA/MACA/MACAR may run in BD/BCD/CALLD/FCALLD/RETD delay
words. They use the ordinary signed 17-bit A-high and signed Smem arithmetic;
memory publishes to T in Read and B publishes in Execute. The return/call
frame remains independently owned. Extended address forms remain guarded.

`delayed_accumulator_multiply` owns 576 independent cases/restores: six control paths,
four operations, DP/indirect/postincrement, eight input patterns,
with FRCT/OVM/SMUL/SXM, page and three checkpoints assigned across tuples.
These three short-family sections deliberately drop the old numeric-mode/control
cross-products; standalone accumulator/square sections own broader numeric
contracts. The two BCD paths seed
A=0/1 to select their outcomes; they do not independently vary A-high.
Checks cover A preservation, B publication, one memory sample, pointer update,
T poisoning after Read, balanced frames and target observation. This does
not qualify interrupt composition, accumulator-producer hazards or physical
pipeline phases.

## Shifted arithmetic in control-delay words

Two-word shifted Smem ADD/SUB may occupy the complete delay span of BD,
BCD, CALLD, FCALLD or RETD. Access owns the address and pointer update;
Read retains one memory operand; Execute consumes the selected accumulator
and publishes the result, carry and sticky overflow. Source and destination
accumulators are independently selected. The existing SXM/OVM rules apply.

`delayed_shift_alu` owns 4,608 independent cases/restores: six control
paths including both BCD outcomes, DP/indirect/postincrement addressing,
ADD/SUB, four source/destination pairs and all 32 signed shifts. Memory
patterns, SXM/OVM, pages and three checkpoints are assigned rather than fully
crossed. Standalone shifted arithmetic owns broader numeric coverage;
this drops the old numeric/control Cartesian correlations. Extension and memory
poisoning check retained ownership; the target observes the result and
stack/pointer counts remain exact.
Interrupt composition and physical pipeline phases remain unvalidated.

## Overflow-only conditional branches

Staged BC/BCD admit AOV, ANOV, BOV and BNOV. Their predicate tests the
selected accumulator's sticky overflow flag and consumes that flag whether
or not the branch is taken (SPRU131G table 4-1). The other overflow flag,
carry, TC and DP are preserved. Delayed execution retains the decision;
subsequent flag writes must not cause another test or clear.
SPRU172C's BOV example retains the flag, contrary to the CPU guide's explicit
rule. This model follows the latter; these fixtures are not an independent
silicon resolution of that documentation conflict.

`ov-branch-pipe` passes 4,608 cases plus cleared-pipeline replay: both branch
forms, four predicates, all initial overflow pairs, seeded/SSBX/RSBX producers,
three gaps, two pages and four checkpoints. Post-Evaluate flag mutation and
delay-word updates cannot change the selected path. Literal and target-fetch
counters reject duplicate consumption, and no data writes or stack changes
occur. Arithmetic-producer hazards, interrupt composition and physical
pipeline phases remain unqualified. The existing atomic overflow, TC/carry,
signed-branch/call and delayed-arithmetic controls remain separate gates.

## Mapped dual-memory arithmetic operands

Dual ADD/SUB admit mapped sources `0008..0017` on either or both operand
lanes. Access retains both addresses and applies the existing X-priority
dual modifier; Read samples the resulting mapped-register values once.
Execute uses those retained words with the existing signedness, saturation,
carry and sticky-overflow arithmetic. A source that is also an updated AR
therefore observes that update in this model. Multiply/MAC and SQDST mapped
operands remain separately guarded.

`mapped_dual_alu` passes 3,072 independent cases plus cleared-pipeline replay: sixteen mapped
addresses, X/Y/both lane placements, eight selector pairs (including aliases),
ADD/SUB, both accumulators and BK=0/32. SXM/OVM, two value patterns, pages and
three checkpoints are distributed. The BK=32 fixtures use valid circular pointer geometry and
include wrap; BK=0 retains the declared linear behavior. Post-Read poisoning
of mapped or RAM sources cannot change the result or cause another pointer
update. Carry/overflow, unselected accumulator lanes, T/TRN and all ARs are
checked independently. `mapped_dual_rejections` passes 888 cases plus replay over
74 unqualified low/control/peripheral addresses, with no reads, writes
or address updates, checking before and at Access rejection. The legacy
factory/setup/checker/routes are removed. A wrong output bit fails the
replacement's first case. The prior arithmetic negative fixture uses unowned
ST0 rather than newly admitted T; multiply/square negatives are unchanged.

This qualifies the declared operand-age/register/arithmetic model, not
physical bus arbitration, producer/control hazards or interrupt composition.
The acquired EFR encoder uses this contract for `a20a` mapped-AR3 subtraction.
Its complete vector acceptance is established by the separate codec gate,
not by this instruction matrix alone.

## READA in unconditional BD delay words

Short RAM-destination READA may occupy either BD delay word. Its existing
four-clock program-bus hold retains the program source after older ALU
publication, then retains the sampled word for the real RAM write. The
acquired decoder case is BD `ca2e`, ADD `ca30`, READA `ca31` (`7e40`).

`bd-read-a` passes 512 cases and cleared-pipeline replays: both delay slots,
DP/plain/postincrement/SP destinations, four boundary words, two caller
pages, two program-source pages and four snapshots spanning the held
transfer. The second-slot cases use an ordinary first-slot ADD to produce
the program address. Changing A after capture, changing AR3 after Read and
poisoning the program word after its sample cannot retarget or duplicate
the transfer. The destination update occurs once at Read; program sampling
and the RAM write retain their existing held-clock placement. Disabling
the BD admission fails the fixture. Physical timing, mapped destinations,
absolute/extended forms, interrupts and other control owners are not
qualified by this matrix and retain guards.

## Short repeat in accumulator-conditional BCD delay words

A short immediate RPT in the first BCD delay word owns its following
one-word body on either branch outcome. Its continuation is selected from
the branch's retained Read predicate: the encoded target when taken, the
word after the full delay span otherwise. Repeat Decode must not sample
the predicate early. BCD's empty prefetch slot also places the count header
in Read when a one-word body reaches Decode; that exact delay relationship
admits the Read-owned count, not arbitrary older repeat headers.

`conditional_delay_repeat` owns 1,440 independent atomic/staged cases/restores:
twelve signed A/B predicates, zero/+1/-1/positive-guard patterns, STL/MVDD/NOP
bodies and counts 0/1/8/39/255. Path counts 0/2/7, direction, page and four
checkpoints are distributed rather than fully crossed. Staged A/B changes after predicate Read
reverse the condition without changing the selected path. Target/body
poisoning, exact body retirement counts, ordered copies, exact buffers and
preserved counters/status check that the delay repeat drains before a fresh
target or fallthrough repeat. The conditional bank in `delay_repeat_limits` retains ten cases/restores
of late headers, long/memory counts and spilling bodies without data,
pointer or counter effects. TC/carry/overflow/BIO predicates, conditional
calls, active outer blocks, arbitrary interrupts and silicon phases remain
unqualified for this repeat composition.

## Short repeat in CALLD delay words

Short immediate RPT in near CALLD's first delay word owns the following
one-word body before the retained call target runs. CALLD writes its normal
continuation frame once; repeating the delay body must not repeat that push.
The shared saved redirect mechanism resumes the target after the delay
repeat drains, and a target-side repeat has an independent count.

`call_delay_repeat` owns 120 independent cases/restores: atomic/staged,
STL/MVDD/NOP bodies, counts 0/1/8/39/255 and four patterns. Target counts
0/2/7, direction, page and four checkpoints are distributed. The target returns
with RET to the original caller; one frame write and one return read restore
SP. Target-literal poisoning, staged body-opcode poisoning, ordered accesses,
exact body retirement counts and buffer guards check continuation ownership.
Far calls, late-header placement, active outer blocks, arbitrary body/interrupt
composition and physical pipeline phases remain unqualified. The shared
exclusion fixture retains CALLD with RPT in the last delay word.

## Short repeat in RETD delay words

A short immediate RPT in RETD's first delay word retains the return target
already sampled by the return's stack Read. Its body drains before target
execution. Repeating the body must neither pop another return word nor
re-sample the stack location after it has changed. Existing return-prefetch
ownership is retained; the repeat uses the same saved continuation mechanism
as BD, not a new stack or firmware-specific target.

`return_delay_repeat` owns 120 independent cases/restores across atomic/staged
frontends, STL/MVDD/NOP bodies, counts 0/1/8/39/255 and four patterns. Target
counts 0/2/7, return direction, page and four checkpoints are distributed.
The original return word is read once, then poisoned; the target starts its
own fresh repeat only after delay writes finish. Tests check balanced SP,
ordered accesses, pointers, unrelated loop state, status and retained staged
body opcodes. The shared delay exclusion test retains a negative RETD case
with RPT in the second delay word. Near CALLD has its separate qualification
above; other unqualified owners remain guarded. Interrupt and physical
pipeline timing are not qualified here.

## Short repeat in RPTBD delay words

The staged model admits a short immediate RPT in the first RPTBD delay word,
followed by an already-qualified one-word repeat body. Its count-plus-one
copies complete once before the outer block runs. This does not install a
branch redirect: RPTBD retains its block controller rather than BD's target.
Discarding speculative outer-body prefetch rebuilds its remaining-iteration
prediction from BRC, including a single-iteration outer block. Otherwise a
correct store count can conceal an incorrectly retained BRAF.

`block_delay_repeat` owns 120 independent cases/restores: atomic/staged,
STL/MVDD/NOP delay bodies, inner counts 0/1/8/39/255 and four patterns. Outer
counts 0/2/7, one/two-word block layouts, pages and four checkpoints are distributed.
Ordered read/write observations require the delay body to drain before outer
publication; counters, pointer updates, stack and final status are checked.
The RPTBD bank in `delay_repeat_limits` retains ten cases/restores, rejecting late placement,
long/memory count forms and spilling store/copy bodies before data access or
short-repeat pointer/counter updates. RPTBD's preceding setup is retained.

TI SPRU131G sections 6.7/6.8 describe the two repeat controllers and RPTBD's
two delay words. The acquired EFR reset supplies the concrete `f272 c9c9
ec05 8392` composition. These bounded algorithm and restoration tests do not
establish silicon pipeline timing or interrupt composition. RETD has the
separate qualification above, as do near CALLD and accumulator-conditional
BCD. Other delayed owners remain guarded.

## Mapped memory logic sources

Basic and absolute Smem AND/OR/XOR admit owned mapped sources `0008..0017`.
Read retains the raw sixteen-bit word; Execute combines its zero extension
with the live destination accumulator. SXM/OVM do not change the logical
operand or result, and no status bits are written. Existing peripheral-logic
contracts are unchanged. Long-offset mapped sources and the remaining low
register/control addresses stay guarded.

`logic_mapped_alu` passes 9,216 cases plus replay: AND/OR/XOR, sixteen mapped
sources, DP/absolute/plain-indirect forms, both accumulators, eight flag
states and four patterns. Early/late sampling, pages and checkpoints are
distributed rather than multiplied into 110,592 executions.
The independent bitwise reference checks high accumulator bits, zero
extension, source/destination lane aliases and preservation of Execute-live
status. Mapped sources change after Read without retargeting or resampling.
`logic-map-edge` passes 1,152 cases plus replay over sixteen unqualified low
addresses, with no reads, writes or pointer changes. Both profiles clear the
live pipeline before load. Producer forwarding, wider control compositions
and physical pipeline timing remain outside this qualification.

The isolated ROM4 FR encoder and decoder now pass their complete bounded
32-frame comparisons and full-RAM replay on both frontends. The acceptance
scope and commands live in the [COBBA control boundary](cobba_control_boundary.md);
this does not establish a handset speech stream or native RF lifecycle.

## Mapped shift-16 ADD/SUB sources

Shift-16 Smem ADD/SUB admits the sixteen owned mapped sources `0008..0017`:
A/B lanes, T/TRN and AR0..7. The mapped word is retained in Read while the
independently selected input accumulator and carry state remain Execute-owned.
The existing arithmetic preserves partial carry: ADD only sets C on a carry,
and SUB only clears it on a borrow. Overflow remains sticky on the selected
destination, with OVM clamping. Memory sampling and arithmetic do not republish
the mapped source after it changes.

`shift16_mapped_alu` passes 12,288 cases plus replay: eight ADD/SUB/input/output
forms, sixteen mapped sources, DP/absolute/plain-indirect addressing, eight
carry/SXM/OVM flag states and four input patterns, with distributed early/late
input sampling, pages and checkpoints instead of 147,456 executions.
An independent integer oracle checks the
shifted result and flags. The fixture changes the mapped source after Read,
including aliases with either accumulator, then checks the retained word
against the later live accumulator and carry. Pipeline state is cleared before
restoring each snapshot.

`shift16-map-edge` passes 7,680 cases plus replay over eighty unqualified
control/peripheral addresses, with no reads, writes or pointer updates. SP/BK
and other control registers remain outside this extension. These tests do not
establish arbitrary producer forwarding, repeat/interrupt compositions or
physical pipeline timing; in particular they do not inherit the separate
LDM/LD AR read-age exception.

## Conditional register and accumulator stores

The staged frontend admits STRCD, SRCCD and SACCD A/B (`9cxx..9fxx`, valid
signed accumulator conditions). Every instruction captures one Xmem address,
updates its pointer regardless of the predicate, reads RAM or a mapped
auxiliary register and writes once.
A false predicate writes back the retained Read sample, not the later live
memory value (SPRU131G 6.6.3; SPRU172C 4-152). SACCD obeys ASM/SXM but does
not use SST saturation.

The partial staged model captures address/DAGEN in Access and memory/ASM in
Read, then samples the predicate and source in Execute. SRCCD's Execute-owned
BRC is explicitly documented in SPRU131G 7-73; this qualification does not
establish silicon timing for every other predicate/source or producer pairing.
The shared store-word calculation is also used for an adjacent RAM bypass,
without replacing the actual write transaction.

`staged_conditional_store` passes 1,152 independent cases plus replay:
four source forms, twelve A/B predicates, six zero/sign/guard patterns and
four ASM shifts. Xmem modes, flags, pages and checkpoints are distributed;
BK remains coupled to source patterns. Late pointer and ASM changes check
retention; changing A/B/T/BRC after Read checks the declared Execute sampling
policy. Poisoning RAM after Read checks the false write-back path. Source
registers and status are preserved. `conditional_store_rejections` passes 256 cases plus
replay, rejecting accumulator, BRC and peripheral destinations before pointer
or bus effects.

`mapped_conditional_store` passes 1,024 independent cases and cleared-pipeline
replays: four forms, sixteen Xmem encodings, all eight mapped AR destinations,
and taken/not-taken EQ cases. Other predicates and numeric boundaries belong
to the numeric section above, not an address cross-product. Pages/checkpoints
are distributed. BK is zero; ASM=0/-1 and flags are coupled to the two tested
values. The target may alias the addressing pointer: its Read sample then
contains the Access-stage modification, and its Execute write replaces that
register once. A false predicate writes the retained sample, not a newer
poisoned AR value. AR0 destinations also test signed AR0-based address steps.
Source/predicate values change before Execute, and non-target registers and
status remain unchanged except for the declared pointer update. Legacy
factories/setup/checkers/routes are removed. A wrong output bit fails at
the first replacement write; no CPU change is retained.

The acquired EFR block deliberately sets AR5 to `0016` at `d1a6` and uses
conditional store `9d3a` at `d1ad` to target mapped AR6. Atomic and staged
traces agree on that initialization, not on all preceding data values.
Control/peripheral register writes, mapped circular and arbitrary short-repeat/
interrupt/control compositions and physical bank contention remain unqualified.

## AR0-indexed MVDK sources

MVDK admits short `*ARx-0` / `*ARx+0` source modes (`71a8..71b7`).
The old source address is sampled before its linear AR0-indexed update and
before a potentially aliasing mapped destination is published. CMPT/ARP
selection and destination-specific publication use the established move
path. BK does not impose circular wrapping on these modes.

`indexed_mvdk` retains 13,504 independently compiled cases plus replay.
It shares move construction with circular cases but uses independently computed
linear pointer results: sixteen unique encodings, twenty-one destination
kinds, eighteen mapped sources and representative signed indices. Pages,
flags and checkpoints are distributed. Each accepted case samples one raw word, fetches one
destination literal and publishes once; accumulator lanes, T/TRN, DAGEN,
BRC and RAM destinations retain their separate ownership. The profile
clears the live pipeline before restoring its snapshots.

Short RPT has a separate `repeated_indexed_mvdk` qualification: 1,488 independent
cases/restores, all sixteen encodings, counts 0/1/5/15, six separated/overlapping RAM
placements and CMPT/CPL combinations; pages and three checkpoints are distributed. Source
samples and pointer updates are fresh per copy; EAR advances from the one
retained destination literal. The reference preserves the existing staged
ordering in which a copy's Access sample precedes the previous copy's Read
publication. This is not independent silicon validation of overlapping-bus
ordering. Forty-eight self-indexed source geometries that enter mapped space are
excluded from positive acceptance. Mapped repeated source/destination
accesses, interrupts and wider control compositions remain
unqualified; no firmware-specific address participates in the implementation.

## Parallel store with ADD/SUB

The staged frontend admits `ST src,Ymem || ADD/SUB Xmem,dst` (`c000..c7ff`).
The two pointers resolve in Access, with X modification winning on aliases.
Read retains X and the store's ASM; Execute samples both accumulators, stores
the old selected source, then replaces the destination. ADD uses the opposite
accumulator plus the shifted memory operand. SUB reverses that order:
`(Xmem << 16) - opposite_accumulator` (SPRU172C 4-177/185). Fractional
accumulator bits must survive the subtraction; subtracting only high words
is not equivalent.

`parallel_arithmetic` owns 24,576 cases/restores: eight forms, 12,288
encoding/layout/BK cases, 8,192 ASM/SXM/SST store cases and 4,096 numeric
memory/opposite-accumulator/SXM/OVM/sticky cases. Pages/checkpoints and
address/status/seed interactions are representative rather than full products.
Independent integer expectations check bit-32 carry, sticky
destination overflow, OVM, shifted/saturated stores and preserved sibling
state. The fixture changes accumulators after Read and changes ASM/pointers
after their capture, poisons the sampled memory word, and checks one read and
one ordered store. Aliased source/destination stores must use the old value.
Clearing the live pipeline before each load verifies restored retained state.

These single-instruction tests do not qualify mapped/peripheral operands,
arbitrary inter-copy alias forwarding, physical bank contention or wider
interrupt/control compositions. Repeated RAM arithmetic is qualified separately.
The stage ownership is a partial model, not measured silicon timing.

## Circular ASM-shifted accumulator stores

ASM-shifted STL/STH (`84xx..87xx`) admit all four short circular modes
`c0..df`. They use the existing circular address generator with Decode-owned
BK, Read-owned pointer/AR0 and ASM, and Execute-live accumulator data. The
destination is retained after one pointer update. Register/peripheral
destinations remain rejected before circular or compatibility-ARP changes.

`asm-circular` passes 393,216 cases plus replay: low/high A/B selectors,
four circular modes, all eight encoded ARs, both CMPT states, four BK inputs,
four index cases, shifts -16/-1/0/15, SXM/SAT combinations, two pages and three
snapshots. Source patterns are coupled to the AR selection; this is not a
complete arithmetic-pattern cross-product. Independent modulo and shifted
store expectations check the one RAM publication and no destination read.
Live BK changes after Decode, pointers/ASM change after Read, and accumulator
data changes immediately before Execute. The saved pipeline must restore the
retained values after the live pipeline is cleared.

BK=0 retains the pre-existing linear behavior required by the atomic ROM4
control; the physical-silicon discrepancy remains unresolved. The fixtures
respect the documented nonzero-BK index bounds, including a large block for
self-indexed AR0. Wider producer, control and repeat compositions are not
qualified here. The stage ownership is a partial model, not measured bus
timing.

## XC 1 selecting the block-repeat endpoint

Conditional execution and interrupt protection have different extents:
XC 1 selects one following word, but XC and both following instruction words
are uninterruptible (SPRU172C 4-199). An XC 1 in the penultimate block word
therefore has an in-block conditional slot while its protection crosses the
repeat back-edge. The staged frontend retains the next iteration's first word
as the protection endpoint when BRC is nonzero, or the ordinary fall-through
word on the final iteration. It does not conditionally annul that extra word.
The existing saved XC endpoint field owns this continuation; no firmware PC
or new synthetic event participates.

`block_xc` owns 2,176 independent cases/restores: seventeen predicates (including
OV/NOV), four layouts including the penultimate XC 1, counts 0/1/2/7, interior
and endpoint positions, two pages and two source rotations. Four checkpoints
are distributed instead of multiplying every semantic tuple by four.
Ordered reads, selected writes, per-iteration conditions, pointer effects,
BRC/BRAF and final fall-through remain checked. Exact interleaved data traces
include next-iteration Read before the previous endpoint Write, and untouched
output tails detect rejected-slot writes or wrong destinations. The eighteen XC `composition_limits`
cases plus replay retain guards when the *conditionally selected* span crosses
the endpoint: XC 1 at the endpoint, or XC 2 at/just before it. These are
model-scope exclusions, not silicon traps.

The endpoint bank in `block_xc_interrupt` passes eight cases plus replay: a
two-word XC 1/store loop, counts 0/1/3/7 and pending IRQ/NMI. Pages and four
snapshots are distributed. Every repeated
store and the final protected continuation store must precede the one vector
fetch and stack write. The live pipeline is cleared before restoration so
protection cannot survive accidentally from the completed run. The combined
section replaces both old profiles, their private taps and phase handlers;
all 56 cases replay. These establish the bounded
staged protection contract, not measured physical pipeline phases.

## Repeated parallel store/ADD/SUB

Short RPT admits the RAM parallel ADD/SUB families using fresh address and
Read stages for each copy. Execute stores the old live source accumulator,
then computes the selected result using the opposite accumulator and the
retained X sample. SUB remains memory-minus-opposite-accumulator. Carry
uses the 32-bit boundary; overflow is sticky and OVM clamps the result.
Store-side ASM/SXM/SST behavior is shared with the isolated instruction.

`repeated_parallel_alu` owns1400 independent cases and cleared-pipeline replays:
all eight forms, five layouts, four shifts and SXM/SST/OVM combinations with
eight copies; a separate count bank covers1/2/256 copies for every form/layout
using distributed flag/shift boundaries. Pages and three checkpoints are assigned
rather than multiplied; full long-count × numeric-input correlation is not retained.
An independent integer reference checks per-clock A/B/carry/overflow
feedback, unchanged T, every old-source store, source/opcode retention and
exact pointer updates. The strided layout uses AR0=3 with linear BK=0;
arbitrary circular strides and mapped/peripheral operands are not qualified here.
Interrupt/control composition, inter-copy same-address forwarding and
physical bank phases remain unqualified.

## Repeated parallel store/load

Short RPT can retain the one-word `ST src,Ymem || LD Xmem,dst/T` body and
issue fresh Access/Read/Execute work for every copy. X and Y addresses are
captured before either pointer update; X modification wins when ARs alias.
Each copy samples X in Read, then stores the Execute-live old accumulator
before replacing an aliased destination accumulator. T loads publish in Read
and are not repeated at Execute. Store shift retains the existing Read-owned
ASM contract. Carry remains the single-instruction model's unvalidated
preserve policy, not a new silicon claim (SPRU172C 4-178/179).

`repeated_stld` owns456 independent cases plus replay: all six A/B/T forms; fixed,
native `b9` postincrement, decrement/increment and same-AR X-priority layouts;
four shifts and SXM/SAT combinations with eight copies. A separate count bank
covers1/2/256 copies per form/layout with assigned flag/shift boundaries. Two pages
and three checkpoints are distributed across pending reads, stores and drain;
their full correlation with every numeric input is deliberately not retained. An integer oracle
checks every accumulator/T transition and each RAM publication. Source and
opcode poisoning check retention; fixed-source samples change between copies.
The live pipeline is cleared before loading each snapshot. Samples use a16-bit
arithmetic progression; poisoning occurs in real RAM after each Read, preserving
an independently observable final store even when X and Y addresses alias.

Mapped/peripheral operands, memory-bank contention, circular repeated addressing and
same-address inter-copy forwarding are not established by this qualification.

## Direct STM to absolute low RAM

`7760..777f` stores the retained immediate to the encoded absolute address
`0060..007f`, independent of DP, CPL, CMPT or SP. Access retains the address;
Execute writes once without a destination read or DAGEN early publication.
The pending literal participates in the existing RAM forwarding path, while
the actual write is preserved. These RAM forms share retained-literal
handling with plain indirect STM, not the early register-write path.

`stm-low-ram` passes 65,536 cases and cleared-pipeline replays: all 32
addresses, eight literals including opcode values, four DP settings,
four CPL/CMPT combinations, two pages and four checkpoints. An adjacent
LDM consumes the forwarded word without a physical read; with one NOP gap
it reads committed RAM once. Literal, pointer, DP/CPL and SP poisoning
checks retention and absolute addressing; exact consumer results, one
write, neighboring RAM, unrelated registers and continuation PC are checked.
This does not qualify arbitrary repeat/control/interrupt composition or
physical bank arbitration. The EFR `7767` at `b955` is this absolute-RAM
form, not a DP-relative write or an identified peripheral.

## Plain indirect STM to page-zero RAM

`STM #lk,*ARx` (`7780..7787`) resolves a page-zero address in Access,
independent of DP. CMPT with selector zero uses ARP; other selectors name
their own AR. The selected pointer's upper nine bits are cleared, as in
SPRU172C 4-176. The literal and destination are retained for one ordinary
RAM write at Execute, with no destination read. This is a partial staged
publication model, not measured silicon timing.

For example, the ROM4 encoder's `7782 b3a6` at `879c` uses AR2=`1268`
(ST0=`0024`, ST1=`0340`), writes `b3a6` to RAM `0068`, and leaves AR2=`0068`.
Atomic and staged entry registers agree there. Treating this as a write to
`1268` loses the MMR-addressing contract.

`stm-ind` passes 1,536 cases plus replay: eight selectors, both CMPT modes,
four literals, four RAM destinations spanning `0060..007f`, two program
pages, and checkpoints before/after address resolution and publication.
Literal poisoning and a subsequent pointer change verify retained operands;
neighboring RAM, unrelated registers and flags remain unchanged. The live
pipeline is cleared before loading each saved state.

`stm-ind-edge` passes the same case count/replay over destinations `0000`,
`0010`, `0028` and `005f`: these unqualified register/peripheral targets are
rejected before pointer changes or data transactions. This is a model-scope
guard, not a silicon trap. Modifying indirect modes, mapped-register timing
and control/repeat compositions are not qualified by these tests. These
profiles use plain RAM instead of the generic test map's synthetic endpoints
at `0060..0061`.

## Short repeat in BD's delay span

Short RPT in BD's first delay word owns the following one-word body before
control reaches the branch target. Starting a staged repeat discards target
prefetch, so the CPU retains the decoded target separately and resumes it only
after all repeated delay copies drain. Both continuation fields are reset and
saved. This is instruction-derived continuation state, not a firmware hook.

`branch_delay_repeat` owns 120 independent cases/restores on atomic and staged frontends:
STL/MVDD/NOP bodies, counts 0/1/8/39/255 and four data/status patterns.
Target counts 0/2/7, direction, page and four checkpoints are distributed;
their previous full cross-product is deliberately removed. The selected
frontend's live pipeline is cleared before each load, so retained targets and
repeat metadata must actually be restored rather than survive from the future
run. It checks exact
buffer contents/guards, ordered transactions, pointers, preserved registers,
execution-owned body counts and final PC. Separate branch/header retirement
counts are no longer observed: ordered path writes and final state qualify
the continuation. The target literal is poisoned
after capture; body-opcode poisoning is staged-only because the atomic frontend
refetches repeat opcodes. Equal architectural outcomes are not a clock/bus
equivalence claim. The existing acquired-ROM atomic delayed-repeat fixture
provides separate BD/RETD corroboration.

The BD bank in `delay_repeat_limits` retains sixteen cases/restores: late header, BANZD,
late CALLD/RETD headers, long/memory counts and spilling store/copy bodies on two pages;
two checkpoints are distributed. No rejected-repeat transaction or address-register/counter
change occurs. The enclosing CALLD's Decode stack reservation and RETD's one
return-word Read/SP increment legitimately precede rejection and are retained,
not undone. These are bounded model exclusions, not silicon traps. Other
control owners, active blocks, interrupt composition and physical bus phases
are not newly qualified.

## Repeated dual-memory MAC

MAC/MACR/MAS/MASR (`bxxx`) can use the existing per-copy pipeline issue path.
Each copy retains two RAM reads, publishes X to T in Read, and reads its
feedback accumulator in Execute. MPY/MACSU and parallel-store repeat families
are not admitted by this change.

`repeated_dual_mac` owns4288 independent cases/restores: four operations, all four source/
destination accumulator pairings, four operand/accumulator patterns, sixteen
FRCT/OVM/SMUL/SXM combinations at eight copies. A separate bank covers1/2/256
copies per form/op/selector with assigned numeric boundaries. Pages and snapshots
are distributed, not multiplied; full long-count × numeric correlation is lost.
The four address layouts cover fixed pointers, the ROM's exact `b09a`
post-increment form, decrement/increment, and conflicting modifiers on an
aliased AR. Both aliased reads retain the original address and X's update wins
(SPRU131G 5.5.4). This is representative repeat addressing, not a fresh census
of all 4,096 non-repeated encodings.

The independent arithmetic expectation checks every accumulator/ST0 clock,
rounding, product clamp and destination saturation. Source words change after
each paired read, the opcode is poisoned after capture, and a newer T follows
the final Read. The gate requires exact paired reads, one opcode fetch, no CPU
data writes, exact final pointers/status and one fresh continuation. Initial
carry/overflow states vary with the four patterns, not independently.
Low-page operands, memory-count headers, nested blocks,
interrupt composition, physical RAM banks and bus phases are not newly
qualified by these gates.

## Repeated RAM square-accumulate

Short RAM-source SQURA/SQURS use the existing per-iteration pipeline issue
path. Every copy owns Access/Read/Execute: the memory sample publishes T in
Read and the retained sample is squared against the live Execute accumulator
(SPRU172C 4-163/4-164). FRCT scales the square, OVM controls saturation,
SMUL does not clamp it, and carry is unchanged.

`repeated_square` owns1572 independent cases/restores: add/subtract, A/B destinations,
DP/plain/post-increment RAM sources, eight input/accumulator patterns, sixteen
FRCT/SXM/OVM/C16 combinations at eight copies, plus assigned numeric boundaries
for1/2/256 copies per form/op. Pages and four snapshots are distributed rather
than multiplied. An integer expectation checks every accumulator/status clock.
Fixed-address inputs change after each read; post-increment inputs vary
by address and consumed words are poisoned. The body opcode is overwritten
after capture, and a newer T is installed after the final Read. It checks
one opcode fetch, exact reads, no CPU data writes, and final continuation.
Initial carry/overflow states vary with the eight patterns, not as independent
full-cross-product axes. Extended or mapped/peripheral repeats, interrupt and
nested-block composition are not newly qualified; physical bus phases remain
unvalidated. Non-repeated square arithmetic retains its separate matrix.

## BANZD delay-owned T loads

One-word `LD Smem,T` can occupy either delay position of BANZD, using the
existing Read-owned T publication. `banz_delay_t_load` passes 192 independent
cases plus replay: counters 0/1/7, four source patterns, direct/plain/
post-increment/post-decrement addressing, both delay positions and A/B
multiply consumers. DP/SP, SXM/OVM, pages and checkpoints are distributed.

Each selected target or fallthrough multiplies the loaded T by an independent
RAM operand. The fixture checks exactly one delay read, one multiply/store,
one source-pointer modification, the original-condition AR7 decrement,
unchanged BRC/status/stack and final PC. Source data, the load opcode and
the branch-target literal are poisoned after capture. Forward/backward target
layouts are paired with A/B destinations, not independently crossed.
The original RPTBD delay-load matrix retains its separate result. Other
control owners, extended loads, mapped/peripheral delay sources and interrupt
composition are not newly qualified; physical bus phases remain unvalidated.

## Memory-counted single repeat inside a block

RPT with a memory count can occur inside an active block when its complete
header and following body start remain inside the block endpoint. It retains
RPTC independently of BRC and uses the existing block-resume prediction after
the final repeated body. Endpoint-overlapping headers remain guarded;
long-immediate and nested-block setup exclusions are unchanged.

`block_memory_repeat` owns 960 independent cases/restores, extending the short-RPT
fixture across DP, SP, plain indirect, post-increment and absolute sources.
It covers outer/base-inner counts 0/1/2/7, MVDD/ST/NOP bodies at the middle
or end and two program pages; four checkpoints are distributed. Post-increment sources
alternate base and base+1 counts across outer iterations; consumed words
are inverted after capture by the volatile-read fixture. This preserves
retained-count poisoning, not the historical exact `ffff` poison value.
It checks one fresh count read and pointer
update per outer iteration, exact ordered body transactions, independent
outer completion, untouched output tails and one final fallthrough. The whole
data-bus trace includes the final T write and distinguishes memory-count
first-copy draining from subsequent pipelined copy issues. IRQ/NMI composition and physical
bus phases are not newly qualified. SPRU172C's RPTB description explicitly
permits single-instruction repeat loops inside block repeats.

## ADDM publication to T

Direct-DP and absolute `ADDM` can write mapped T (`000e`); unrelated low
registers retain their guards. `addm-t` passes 6,144 cases plus save/load
replay using the existing independent mapped-ADDM arithmetic matrix:
four source words, four immediates, sixteen SXM/OVM/carry/OVA combinations,
both CMPT modes, two program pages and three checkpoints. It checks one
retained Read, one Execute publication, unchanged A/B and address registers,
literal poisoning and a changed live T between Read and Execute.
Direct forms read/write at clocks 6/7, absolute forms at 7/8.

SPRU172C 4-9 specifies a non-repeatable instruction affecting C/OVA under
SXM/OVM. SPRU131G 7.5.6 classifies it as a store-type T writer: short
Category-I shift/bit-test consumers require a separating instruction;
other consumers have zero latency. This gate establishes publication, not
those consumer compositions, IRQ/delay interaction or physical bus phases.

## Repeated accumulator loads

`repeated_accumulator_load` owns 8,192 numeric cases/restores over all four
source/destination selectors, shifts -16..15, eight guard/sign patterns,
SXM/OVM and opcode/ASM shift sources. Counts0/1/7/255 and pages/checkpoints
are distributed instead of crossed with every numeric input; full
count-by-value correlations are not claimed.

`repeated_accumulator_counts` owns 4,608 cases/restores over all six count
sources (short/long immediate, DP/SP, absolute RAM/AR), four counts, selectors,
SXM/OVM, two numeric boundaries, opcode/ASM and none/IRQ/NMI. One of shifts
-16/-1/15 is assigned to each tuple. The retired composition range contained
two identical 82,944-tuple cycles; the replacement removes that duplicate,
page/checkpoint copies and the full count-source/shift cross-product.

`repeated_accumulator_block` retains all512 inner/outer arithmetic tuples:
outer counts0/2, inner counts0/1/7/255, four selectors, shifts -1/15,
SXM/OVM and two patterns. Pages/checkpoints are distributed. The discarded
block-setup prefetch has a deliberately different opcode; it must never
retire. Every outer iteration fetches one fresh inner body and executes one
continuation, proven by body counts and AR4 progression.

All sections seed CPU/count state explicitly, poison captured body/count
storage, and compare A/B/ST0 after every retirement and between retirements
against a once-built independent bit-position/saturation sequence. Non-block
cases retain exact one-clock retirement and RC cadence. The saved scalar
retirement count also gates IRQ/NMI acknowledgement; RTN frame writes/reads
balance after arithmetic completes. A stale per-iteration expectation fails
immediately. These are model ownership/cadence contracts, not measured timing.

## Repeated arithmetic accumulator shifts

Single-repeat SFTA reuses the ordinary instruction's arithmetic on each
Execute iteration, with a live source accumulator and retained opcode.
Unlike shifted LD, it updates carry for nonzero shifts and does not raise
new overflow for a zero shift (SPRU172C 4-155). The existing left-carry
formula/zero-shift policy remains explicit: the manual's left-shift example
and formula disagree, so these tests do not establish silicon behavior.

`repeated_arithmetic_shift` owns 4,096 cases plus save/load replay: all four
source/destination selections, shifts -16 through15, eight signed/guard-bit
patterns and SXM/OVM. Counts0/1/7/255 and pages/checkpoints are distributed
instead of crossing every numeric input with all counts. The independent
expectation checks every arithmetic publication, carry and sticky overflow,
one accepted body fetch despite opcode poisoning, and one fresh continuation.
Both initial carry values and program pages are represented, but not a full
cross-product of those axes. Memory count sources, IRQ/NMI and nested-block
composition are not newly qualified for SFTA.

## Repeated logical accumulator shifts

Single-repeat SFTL uses the ordinary 32-bit logical-shift effect on each
Execute publication, with a live source accumulator and one retained opcode.
It participates in the existing pipelined repeat issue/retirement path;
no alternate arithmetic implementation or saved state is added.

`repeated_logical_shift` owns 180 independent cases/restores: short-immediate,
extended-immediate and RAM-count headers, all four source/destination choices,
shifts -16/-1/0/1/15 and counts 0/3/17. A bit-copy reference validates every
intermediate accumulator/carry publication, 32-bit truncation of guard bits,
preserved unrelated flags, one body fetch despite opcode poisoning, a single
count read where applicable and the completed continuation. SXM/OVM and four
save points are distributed. Interrupt and nested-control composition are not
newly qualified. The native ROM6 witness is `RPT *AR2` at `43b2` followed by
`SFTL A,-1,A` at `43b3`.

## Compare, select and store maximum

CMPS compares the signed sixteen-bit high and low accumulator halves in
Read, as explicitly specified by TI SPRU172C 4-35. The selected word and
low-wins-ties decision are retained in existing pipeline operands. Execute
stores that word, shifts the current TRN left with the decision bit, and
updates TC; neither accumulator changes. Short direct, ordinary indirect and
circular RAM destinations use existing Decode DP/CPL/BK and Access SP/AR samples.
Mapped destinations, extended addressing and single-repeat bodies remain
excluded by this admission, not modeled as silicon traps.

`compare_select_store` checks 120 positive cases/restores: both accumulators,
signed extrema and ties, DP/SP/plain/postincrement/postdecrement addressing,
and two initial TRN patterns. It checks pointer updates at Access, no early
TC/TRN publication, and exact Execute stores after poisoning both accumulator
values, TRN and pointer following Read. Four save points are distributed.
Three negative cases check no side effects for mapped destinations,
extended addressing and repeated bodies. The disassembler identifies the
full CMPS memory syntax independently of this execution subset. Interrupt,
physical stage timing and block-endpoint composition are not newly qualified.

Another 72 cases/restores cover circular +/-1 and +/-AR0 modes, positive
and negative indices and positions at either end or inside an eight-word
buffer, for both source accumulators. An independent modulo reference checks
Access updates and the original store address. Bit-reversed and other
indexed forms are not newly admitted.

## Repeated paired-word addition

Short-address DADD participates in the existing pipelined single-repeat
path. Each iteration retains its own parity-ordered pair at Read and uses
the live source accumulator at Execute; no alternate arithmetic or saved
state is introduced. Mapped/peripheral pairs remain rejected by the ordinary
pair-address guard. Extended-address DADD and repeated DSUB/DRSUB are not
admitted by this change.

`repeated_double_add` checks 72 cases/restores across all four accumulator
selectors, short/extended-immediate and RAM-count headers, counts 0/4/15,
and positive/negative pairs. It checks every intermediate accumulator/carry
publication, paired bus reads, two-word pointer advancement, retained body
fetch despite opcode poisoning, continuation and distributed save points.
The existing `long_pair_arithmetic` section separately covers arithmetic
modes and address forms. Nested-block and interrupt compositions are not
newly qualified by these repeat tests.

## Memory SQUR

`SQUR Smem,A/B` (`26xx/27xx`) squares the signed sixteen-bit memory word,
publishes that original word into T at Read, and writes the retained square
and overflow state at Execute (SPRU172C 4-161). FRCT doubles the product;
OVM controls positive overflow saturation. It does not consume an unrelated
old T or overwrite a newer T at retirement. The accumulator-A form remains
a distinct seventeen-bit operation that leaves T unchanged.

`memory_square` passes 8,704 cases plus replay: RAM and CPU-mapped sources,
both destinations, eight patterns and eight FRCT/OVM/SMUL combinations;
SXM, pages and checkpoints are distributed. It checks one retained read,
address/literal ownership, later T poisoning, unselected accumulator and carry
state and no data writes. Program/memory are changed after capture.
Peripheral electrical timing, single-repeat and delay-slot SQUR composition
are not newly qualified. The original register-SQUR and multiply/T-source
gates retain their independent results.
Disassembly syntax coverage is separate from the independent execution
sections and does not establish arithmetic or pipeline correctness.

## Delayed forward block exits

BD and the twelve signed-accumulator BCD predicates can exit forward beyond
an active block when both delay words remain strictly inside its bounds.
The existing delayed-control engine retains the predicate before the delay
words and executes those words once on both outcomes. This admission does
not automatically clear architectural BRAF or consume the remaining BRC;
software can clear BRAF outside the block and revisit the old endpoint after
the documented spacing. Backward exits, endpoint-overlapping delay spans,
other predicate families and interrupt composition are not newly qualified.

`block_delayed_exit` owns 1,568 independent cases/restores: BD/twelve A/B predicates,
counts 0/1/2/7, first/middle/last/never exits, two endpoint layouts, two delay
load/store orders and two program pages. Six checkpoints are distributed;
unconditional branches retain one input pattern rather than four identical
decisions. A delay load
reverses the selected condition, so a re-evaluated rather than retained
decision fails. Ordered reads/writes, single delay effects, BRC/BRAF at exit,
explicit software clear and a spaced endpoint revisit are checked. Original
non-delayed forward exits are owned by `block_exit` (144 independent
cases/restores replacing 1,152 phase-chain executions), retaining B and
BC A/B equality, the same counts, exit positions, layouts and pages.
Both sections observe the whole data bus: endpoint layouts permit the next
iteration's Read before the previous tail Write. Delayed conditional and
unconditional forms also differ in delay-load/store bus ordering, which is
checked as implementation behavior, not a measured silicon phase claim.

## Conditional exit at the block endpoint

A nondelayed conditional forward branch may have its extension word at REA.
The endpoint retains its ordinary Decode-owned BRC decrement even when the
branch is taken; the existing branch redirect handles the transfer. No hidden
loop stack or unconditional BRAF clear is added. The admission remains limited
to conditional forward exits; unconditional/inside-target endpoint branches
and endpoint-overlapping delayed branches retain their existing exclusions.

`block_terminal_exit` checks 20 independent cases plus replay: counts 0/2,
first/last/never exits without duplicate zero-count patterns, A/B conditions
and two program pages. It checks exact read-pointer advancement, one selected
marker write, post-endpoint BRC, RSA/REA, explicit software BRAF clear and the
ordinary continuation. The test fails before this admission. The two former
conditional-endpoint exclusions move from `block_branch_limits` into this
positive coverage rather than being described as hardware-invalid forms.

[TI SPRU131G section 6.8](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
defines REA as the last instruction word and the Decode-time decrement.
The acquired ROM6 instruction at `6238` supplies a concrete consumer with
extension at `6239`; it is not a firmware-specific CPU exception.

## Conditional return inside a block

Nondelayed RC may return before REA. The existing conditional-return engine
retains its predicate and owns the ordinary stack pop; admitting it inside
the block does not synthesize a second return or silently clear BRAF/BRC.
An untaken RC reaches the ordinary endpoint; a taken RC preserves the
remaining controller state until software clears or replaces it.
Endpoint RC and delayed returns inside blocks remain outside this admission.

`block_conditional_return` checks 20 cases plus replay: zero/two repeat counts,
first/last/never returns, A/B predicates and two pages. It verifies exact
source-pointer advancement, one caller marker, balanced SP, BRC, RSA/REA,
explicit software BRAF clear and the caller continuation. The test fails
before admission. The ordinary conditional-return and block-boundary tests
remain independent controls. Acquired ROM6 uses the admitted form at `6298`
inside `6298..629a`; no firmware address is present in the CPU logic.

## RPTBD delay-owned T load

One-word `LD Smem,T` can occupy either RPTBD delay position while retaining
the ordinary Read-owned T publication. The admission is limited to direct
and short-indirect encodings below MOD 3; other control owners and extended
T-load delay forms are not newly admitted. The first body multiply must see
the loaded T, rather than an old value or a re-read of changed memory.

`block_delay_t_load` passes 192 independent cases plus replay: counts 0/1/7,
four input patterns, direct/plain/postincrement/postdecrement addressing,
both delay positions and A/B multiply destinations. DP/SP, SXM/OVM, program
pages and three checkpoints are distributed rather than multiplied. It verifies one delay read at
prototype clock 7 or 8, count+1 body products, single delay DAGEN update,
buffer sentinels and unselected state. Source memory and the fetched opcode
are poisoned after capture. The entire shared legacy T-load fixture,
setup/checker/declarations and routes are removed after BANZD/control migration.
A changed product bit fails at the first publication. Interrupt composition and physical stage timing
are not established.

## Repeated fixed-accumulator transfer

The ordinary block-interior immediate-repeat admission also includes RPTZ
for A or B, provided its complete header and following body fit inside the
block. Each block iteration executes the zeroing header anew, while the
single-repeat count remains separate from BRC. `block_zero_repeat` checks
18 cases/restores: outer counts 0/1/3, inner counts 0/2/9 and both cleared
accumulators. Independent dot products and one output store per outer
iteration check re-zeroing, MAC accumulation, both advancing input pointers,
outer suffix execution, preserved opposite accumulator/status and completed
continuation. Four save points are distributed. This does not qualify
interrupt composition or physical pipeline timing.

RPTZ's sequence-opening clear precedes the first MVMD read of the same
accumulator (SPRU172C 4-150). In the staged prototype, the matching AL/BL
source sample is retained after the older RPTZ Execute, not before it in
that same clock. Opposite-accumulator reads and ordinary direct-ALU producer
latency are unchanged. No result arithmetic or opcode timing is replaced
with a firmware-specific answer.

`zero_repeat_mvmd` passes 64 independently compiled cases plus replay:
AL/BL sources, A/B clears, counts 0/1/39/255 and four seeds. SXM/OVM,
pages and three checkpoints are distributed rather than multiplied.
Matching clears yield zero on the first sample; opposite
sources retain their value. A deliberate mid-repeat source change must
affect only later copies. Exact read/write cadence, final accumulator fields,
flags, SP/T and destination guards are checked. This section does not renew
FR decoder or handset acceptance.

`MVMD AL/BL, dmad` (`7308/730b`) can repeat into ordinary RAM. Each iteration
samples the same mapped source, retains that word, and writes the next EAR
destination. The original literal is fetched once, not incremented in program
memory. This uses the existing pipelined move stages (Access sample, Read
publication), not measured physical bus phases; other MMR sources and mapped
destinations retain their guards (SPRU172C 4-115).

`repeated_mvmd` passes 64 independently compiled cases plus replay: AL/BL,
counts 0/1/39/255, four source patterns and short/long RPT. Four SXM/OVM
combinations, pages and checkpoints are distributed. A deliberate source change halfway through the read sequence
must affect only later copies; destination and source guards, untouched ARs,
stack, T, other accumulator and status are checked. Post-fetch literal
poisoning and save/load must not cause re-fetch or duplicated accesses.

## Mapped-load AR source age

LDM and zero-shift LD/LDU from mapped AR0..AR7 retain their Read operand
before a younger instruction's same-clock Access update of that AR. Source
address selection, destination Execute and extension rules are unchanged:
LDM/LDU zero-extend, while LD sign-extends only under SXM (SPRU172C 4-73
for LDM; the retained LD/LDU contracts below apply to ordinary loads).
This is an age-order correction within the prototype clock, not a new
physical timing measurement. Other MMR sources retain their existing rules.

The ROM4 timer-descriptor producer uses the five-word block at
`a1af..a1b3`: LDM AR5,A; MAR *AR5+; STL B,*AR2+; READA *AR2+; LD #1,B.
The old ordering let MAR change the source of the older LDM, shifting the
program-table read by one word. Its first target became `0884` instead of
`0834`; the last target became `0299` instead of `0974`. Ordinary queue
consumption consequently lost a later `0000/0050` control. Retaining the
older AR sample restores both the target sequence and the shared RF-control
prefix, without firmware writes or transport changes. The initializer and
loop words are pinned by `rf_timer_table_generation` in the codec contract.

`ldm-ar-age` passes 2,304 cases plus replay: eight ARs, A/B destinations,
four values including sign/wrap boundaries, younger MAR increment/decrement
and MVMM writes, both SXM states, two pages and three checkpoints around
Read/Execute. It checks one Read at clock 5, retained zero-extended Execute
at 6, the younger update, other accumulator and status preservation, and no
duplicate bus accesses after restore. The unmodified core fails the Read
sample. General interrupt/stack/control-MMR compositions are not newly
qualified.

`mapped-load-age` adds 18,432 cases plus replay for LD/LDU through direct DP,
plain indirect, absolute and ordinary offset addressing. It covers the same
eight AR sources, A/B destinations, four values, three younger writers,
SXM states, pages and snapshots as the LDM matrix. One-word loads retain
Read/Execute at clocks 5/6; two-word forms retain 6/7. The unmodified ordinary
load path fails the old-register sample despite LDM already being corrected.

`shift16-load-age` adds 9,216 cases and cleared-pipeline replays for
`LD Smem,16,A/B`, covering the same four addressing forms, eight ARs,
four boundary values, younger MAR increment/decrement and MVMM, both SXM
states, two pages and three snapshots. It retains the older Read before
younger Access and applies sign extension/shift only to that retained word.
Disabling the shifted-load retention fails the source-read assertion.
The acquired example is `b99e` (`4413`) followed by parallel `e4d8`:
the load must retain AR3=2 before the younger instruction increments it,
otherwise the next parameter-array copy starts with 3. TS-based mapped
loads remain excluded; other ALU/dual-read families are not admitted by
this qualification.

A literal adjacency scan of the 65,535 acquired ROM4 program words found no
direct mapped LD/LDU followed immediately by MAR on the same AR. This is not
a control-flow or indirect-source census and does not establish that the
hazard is unreachable. The ordinary-load correction is ISA conformance, not
an independently observed additional handset blocker.

Apart from the separately qualified ordinary ADD/SUB matrix above, read-age
qualification is not inferred for RMW, other arithmetic/logic, multiply,
port-transfer or dual-read families. They retain their separate operand,
forwarding and publication rules; their same-clock AR-age compositions need
dedicated tests before a broader read-order change.

## ADDM BRC and interactive staged execution

Absolute ADDM admits BRC outside active block, single-repeat and delay
compositions. Read captures the source once and Execute publishes the
retained arithmetic result. This uses ADDM's existing SXM/OVM, carry and
overflow behavior (SPRU172C 4-9). Other low controls remain guarded.

`addm-brc` passes 3,072 cases plus save-state replay: four initial values,
four immediates, sixteen SXM/OVM/C/OVA combinations, two CMPT states,
two pages and three snapshots. The immediate and BRC are changed after
capture. Source Read at clock 7 and publication at 8 occur once; ARs and
accumulators remain unchanged. These are prototype stage assertions, not
measured physical phases.

SPRU131G 7.5.8.1/table 7-24 gives ADDM one intervening instruction before
RPTB[D], unlike the separately listed zero-latency STM/MVDK writers.
`addm-brc-loop` passes 320 cases plus replay: derived counts 0/1/3/7,
increments +1/-1, NOP/CALL separators, RPTB/RPTBD, two pages and five
snapshots. It checks one arithmetic publication, exactly count+1 body
iterations, final BRC/BRAF and balanced call frames. Zero-gap and active-loop
counter edits are not newly qualified. Existing mapped-AR ADDM/consumer,
TC-block, polling-interrupt, RCD and delayed-block exclusion profiles pass.

The native `0a40..0a5f` upload still binds uniquely to MCU offset `958e8`,
but its ADDM no longer stops execution. A ten-second clean cold checkpoint
has ILLEGAL clear, 22 host-consumer advances, 262 RF-control writes and 2,878
transfer expiries. Continuing CPU-issued control writes distinguish this from
the earlier frozen-counter polling failure; they do not establish valid RF
reception or registration. These counts replace the rejected reset-lifecycle
baseline, not an independently measured silicon timing contract.

`make verify-5110-menu NSE1_MACHINE=nse1pipe` performs a fresh own-model
EEPROM boot, presses/releases the physical Menu key at eight seconds and
requires the existing Phone book frame SHA-256
`d82cc6891fcf4efb0bd11ded583508f40826f58aa69463708a46897b76fdffb5`.
The staged wrapper also requires terminal ILLEGAL clear and no unsupported
form or reset. `make verify-5110-save-state NSE1_MACHINE=nse1pipe`
saves the running handset at seven seconds. It compares save-time
PC/ST0/ST1/SP, I/O `21`, BSP control `22` and emulated time exactly, then
requires an identical nonempty 160 ms stream of DSP I/O writes and timestamps
on the reference and restored branches. Trace overflow fails instead of
accepting a truncated comparison. It then opens the menu through physical
input. Its structural check also requires a clear DSP ILLEGAL flag after load. Default invocations
of both gates still test the atomic profile. This validates the running
idle/radio-service composition and subsequent menu input, not saves inside
native calls, SMS transactions or every application.
Both profiles use the native DSP backend, not `nokia_dsp_hle`. Both now pass
menu, late-input, exact save/replay and short/long physical power gates.
All enable reset tracing and explicitly fail on resets or DSP faults;
terminal ILLEGAL alone is insufficient. Native calls, registration and speech
remain unaccepted.

`NSE1_MACHINE` selects the frontend for menu, restoration, late-input and
power-lifecycle gates; the former `NSE1_MENU_MACHINE` remains a compatibility
alias.

Short postincrement immediate ST (`7690..7697`) now retains its old address
and updates AR in Access, presents the retained write address in Read, and
publishes its immediate in Execute. SPRU131G figure 7-1 separates those
events; SPRU172C 4-116 places MVMM's source read in Decode. Delaying the
store's AR update until Read made an adjacent `MVMM AR3,AR4` copy a pointer
one word short. Identical challenge buffers then produced a first bus write
at `1201` instead of `1202`, a wrong challenge response and MCU reason-4
reset. The corrected clean run continues with validator `r6=1` at 0.349379 s,
without that reset. This is a CPU addressing correction, not new provisioning.

`immediate_store_mvmm` passes 256 independently compiled cases plus replay:
one/two immediate stores, all eight AR sources and destinations (including
aliases), and zero/one NOP gaps. Pages and three snapshots around
Decode/Access are distributed rather than multiplied. It checks each original
store address/value and Execute clock, the one-time retained MVMM read/write,
and a following indirect load. Legacy setup and phase branches are removed.

An adjacent MVMM also consumes the pending short STL/STH postincrement
through an internal DAGEN bypass. The accumulator store retains its existing
Read-address/Execute-write placement; unrelated mapped loads and explicit
AR stores retain their separate latency contracts. The acquired case is
STL at `d444` followed by MVMM `d445` (`e734`): the copied AR3 is `1104`,
not `1103`. Reading the stale cell made a later copy start one word early.

`accumulator_store_mvmm` passes 4,096 cases and cleared-pipeline replays: STL/STH
from A/B, four boundary words, one/two stores, every AR source/destination
pair including aliases, and zero/one NOP gap. Pages and three snapshots
are distributed rather than multiplied.
It checks the original store addresses and Execute write clocks, one
retained MVMM publication and the following indirect load. The adjacent
case bypasses the stale cell; a gapped case performs the ordinary mapped
read. A negative control expecting a stale pointer fails at the first
publication. Legacy setup and phase branches are removed. Other modifiers,
mapped destinations, interrupts and physical pipeline timing are outside
this qualification.
These are prototype phase assertions, not measured silicon clocks; other
immediate-store forms and general interrupt/repeat composition are not newly
qualified.

The read-only `tools/c54x_rom4_challenge_observe.lua` records buffer writes
and context snapshots; fetch snapshots are not operand-capture proof. Use
`-autoboot_delay 0 -autoboot_script <absolute script path> -seconds_to_run 1`
on each frontend with separate freshly provisioned NVRAM directories.
It modifies no memory, retains tap handles, and reports write-trace truncation.

## Conditional delayed returns

The staged frontend admits the twelve signed A/B predicates and TC/NTC for
RCD outside active block bodies and other delay spans. The predicate is
retained at Read after older Execute publications. A taken return reads and
pops the stack once; an untaken return neither reads nor changes it. Both
paths execute the two retained delay words, with false conditions resuming
at PC+3. Delay-word values cannot change the captured decision. Carry,
BIO and overflow predicates are not admitted by this qualification.
This follows SPRU172C 4-133/4-134, reusing the retained RETD delay-word path.

`rcd` passes 4,480 cases plus save-state replay: fourteen predicates, eight
patterns including signed 32-/40-bit boundaries, two delay layouts, two
stack alignments, two pages and five snapshots. One layout changes the
tested accumulator or TC after the decision; the other contains a two-word
STM with an opcode-shaped literal. Stack memory is changed after its one
read to detect resampling. Ordered continuation stores observe the delay
results, and the fixture checks selected target, final registers/status,
balanced conditional pop, no untaken stack read and no repeated delay fetch.
The unchanged core rejects `fe42` at `0600` in the fixture. Existing
conditional/TC returns, BITT/RETD, load-return, interrupted STLM, polling-loop
and delayed TC-block profiles pass. General interrupt/block compositions
and physical bus phases remain unvalidated.

Native execution proceeds through `90eb` and another RF-control burst,
then reaches `6bf8 001a 0001` in uploaded program RAM at `0a4c`.
The 32-word live window `0a40..0a5f` binds uniquely to MCU offset `958e8`;
the following helper begins with RPTB at `0a52`. The canonical observer
captures this window at stop and the checker verifies its MCU binding.
Mapped ADDM to the block-repeat counter is now qualified above. The source
binding protects against mistaking corrupt bytes or another image's code
for a missing instruction.

## Delayed TC branches inside a block

TC/NTC delayed branches use the existing within-block target and complete
delay-span checks. Their predicate is retained at Read as for standalone
TC branches; a taken internal branch resets speculative fetch iteration
state without changing Decode-owned BRC. Calls and out-of-block delayed
targets are not newly admitted by this qualification.

`internal_tc_branch` owns 128 independent cases/restores: TC/NTC,
counts 0/1/2/7, four source patterns, two delay layouts, two pages and
four distributed snapshots. BITT produces TC from ordered, post-incremented RAM reads;
each source is changed after Read. One delay layout overwrites TC with
SSBX/RSBX and advances a pointer, while the other performs a two-word
accumulator addition. Selected-arm writes, retained branch decisions,
target-literal fetches, pointer updates, final status and exact loop counts
are checked. The old core rejects `fa20` at `0605` in this fixture.
Accumulator-condition, unconditional and BANZ block profiles, block-repeat
exclusions/interrupts, and the polling-loop interrupt profile remain green.
General TC-branch/block interrupt composition remains unvalidated.

The native three-second capture after this qualification has ILLEGAL clear and continues through
28 consumer advances. A longer cold run finds the next unsupported staged
form at `90eb`, `fe44` (`RCD ANEQ`), now separately qualified above.
The intermediate five-second capture is not the current acceptance endpoint.

## Delayed-branch polling interrupts

The staged CPU must not wait for every delay instruction to reach Execute
before recognizing a pending interrupt. In a short polling loop, the only
clock with the old control fully retired can coincide with the next branch's
partial Decode, starving interrupt recognition indefinitely. A synthetic
polling loop reproduces this with an enabled pending IRQ and no handler entry.

The qualified exception covers a retired delayed near branch with a complete
two-word absolute LD in Access, after discarded fall-through prefetch has
cleared. The decoded load drains ahead of INTR, following SPRU131G 6.10.7
and 7.2. Prefetch/Fetch instances at the same PC are not decoded control
owners. The retained control opcode is saved with the rest of the pipeline;
calls, returns and flag-writing delay instructions keep their existing guards.
This is a bounded composition, not a general interrupt-latency claim.

`poll_interrupt` passes 64 cases and save-state replays: IRQ/NMI and all 32
arrival phases. Pages and three snapshot positions before assertion, after
the pin event settles and during entry are distributed rather than Cartesian
dimensions. The handler stores the foreground
accumulator to prove that the delay load completed, updates the polled RAM
once, then returns. The test requires the branch target as the stacked
continuation, one acknowledgement, balanced SP and eventual loop exit.
Exact ordered writes cover the interrupt frame, handler sample and release.
Polling reads were not count-constrained by the old fixture and remain
unconstrained; a write-only data-event policy avoids inventing a cycle-by-cycle
read oracle. Checkpoints avoid the callback that queues the physical input.
The old 384-case profile, custom taps/counters and phase-keyed IRQ hook are
removed. This section-only run does not renew neighboring legacy profiles.
A broader guard relaxation failed comparison-publication and far-call-frame
tests and is not part of this contract.

In the native run, this releases the `4717` polling loop without changing
RAM, firmware inputs or peripheral timing. Execution reaches the delayed
TC branch inside block `473c..4777`, qualified above. This is not native
handset boot or RF acquisition.

## CMPM mapped-register source qualification

CMPM now admits T, TRN and AR0..AR7 (`000e..0017`) in addition to
the qualified accumulator slices. These are CPU-owned mapped registers,
not board RAM. The operand is captured at Read; TC is published at
Execute, and the source is not written back. Immediate and absolute-address
extensions are retained rather than fetched again after capture.

`cmpm-map` passes 69,120 cases and save-state replays across direct,
indirect and absolute addressing, both CPL settings, two pages, three
snapshots, eight source values, six literals, initial TC clear/set and
both TC/NTC branches. Sources and instruction extensions are changed
after capture to detect re-reads. The indirect pointer is distinct from
the source register. `cmpm-map-edge` passes 3,840 exclusion cases and
replays for controls `0000..0007` and `0018..005f`; those guards are an
implementation boundary, not a claim that silicon traps on those addresses.
The original accumulator, direct-memory and TC-call profiles also pass.

Fresh three- and thirty-second native captures after this change reach
PC `471c` and `471a`, respectively, with ILLEGAL clear. Both retain twelve
RF-control writes, 32 transfer expiries, 357 completion strobes, 74 mailbox
writes, 329 slot expiries, 10,400 RF reads and twenty-one consumer advances.
Frame expiries advance from 573 to 6,423. The terminal T=`5555`, BK=`0048`,
ST0/ST1=`081f/6103` and AR0..AR3=`126d/06b8/067a/0008` agree; A differs
between `1a00` and zero. Artifacts are
`tmp/cmpm-map-native{,-long}/error.log`.
The surrounding mask-ROM loop reads `06b7`, subtracts `*AR1` (`06b8`)
and branches to `4717` while equal. The delayed branch's two-word LD
reloads `06b7`. A passive read/write probe observes both words initialized
to `1a00` at PC contexts `4442/4443`, first at 1.853057865 s and again
at 1.993330865 s. Between those initializations, the frame-ISR path's
store at PC context `3363` advances `06b7` through `1a04/1a08/1a0c/1a10`
on successive frame periods. After the second initialization both words
remain `1a00` through three seconds, with no further writes. The loop is
actively reading equal words, not CPU IDLE or an unsupported-opcode halt.
Artifact: `tmp/cmpm-wait-native/error.log`, produced by the read-only
`tmp/cmpm_wait_probe.lua`.

The pre-correction terminal IFR=`0225` and IMR=`035f` include enabled pending frame
source 0; ST1=`6103` has INTM clear. Thus the next question is delivery
through the short delayed-branch loop, not a fabricated peripheral reply.
The delayed-control guard was responsible, as qualified above. This is an
important counterexample: timer advancement alone is not evidence of ongoing
firmware service or a healthy wait. These pre-correction captures are not
the current acceptance endpoint.
It does not establish upload completion, RF acquisition or handset boot.

## Extended I/O and program transfer encoding

Short repeated PORTW/PORTR retain their address, port, sampled value and wait
state without using atomic continuation fields. Each later transfer has a RAM
and I/O phase; seven software waits yield nine clocks between port accesses.
READY can extend the held transfer without rereading its source or modifying
AR twice. The twenty waited-control cases retain repeat counts 0..3, the
two-word BCD delay transfer and RSBX-INTM follower; six added cases restore
inside repeated software/READY holds or after operand capture. Distinct words,
post-capture poison and complete RAM/I/O/frame traces protect the held value
and once-only pointer/bus effects. Two more protect older mapped ports while
a younger RPT enters Decode, a genuine native-boot regression caught by the
5110 menu gate: repeat guards must match the body's address, not just a global
active bit. IRQ is accepted after all transfers, with
exact frames and repeated-access intervals. Nested control and mapped repeat
operands remain guarded before the first side effect.

External I/O helpers now vary forms, not execution models. Both bus profiles
retain 1,440 staged cases; the 288 duplicate short-form atomic executions are
removed. Extended words use assembler-checked offset/address ordering, and the
source poison now targets the actual RAM/MMR operand. A self-aliasing mapped AR
follows the declared Access-before-Read/Execute publication ordering, rather
than the old atomic read/update sequence. Board-owned `0030` and AR destinations
are qualified; no peripheral semantics are invented for board storage.

CPU input-line delivery is queued by the scheduler. A checkpoint in the same
observer callback as `set_input_line` can precede delivery and omit that queued
edge on replay. The waited BCD fixture settles its protected IRQ edge one clock
before capture; it does not compensate by forcing IFR or weakening replay.

Long-offset PORTW/PORTR and MVDP/MVPD retain offset in the first extension
and explicit port/program address in the second. GNU tic54x assembler output
independently establishes the qualified encoding; SPRU131G's generic
last-word-offset note conflicts and is not hardware confirmation. Execution,
disassembly, and execution fixtures use the same corrected words. Existing
832 program-transfer and 41 MVDP cases retain their pointer/mapping poison,
bus values, exact stage checkpoints and restores after re-encoding.

`operand_order` retains all 46 independent data/I/O/program sequences
and extension-before-operand checks. The route table in `memory_literals` retains
26 result/marker cases, including long preupdated READA. The six transport
cases retain pointers, flags, unrelated accumulators and actual transfers;
PORTR's input Read is two clocks after the preceding PORTW Write and four
before the successor Write. These are model-clock intervals, not standalone
instruction latency. Offset READA repeat/control compositions remain excluded.

## Mapped TRN publication and protected interrupt drain

`peripheral_cost_classes` retains its twenty two-access STM/ANDM/ORM/ST/LD
contrasts at AR2, PRD, board-owned `005f` and scratch `0060`. Direct STM to
`005f` reaches the board mapping and pays the peripheral-window clock; no CPU
register is invented there. RMW reads and writes are separate staged clocks,
preserving the original total between successive reads instead of asserting
atomic same-clock Read/Write. All stored values and unrelated state remain
checked with save/restore.

Adjacent STM CLKMD / LDM CLKMD is qualified without inserting a separating
instruction. Both peripheral wait clocks are paid, the older Write completes,
and a saved pending Read holds its slot for the next bus clock. IRQ acceptance
is blocked until that Read completes. The original register-copy fixture retains
CLKMD=3 readback and adds snapshots immediately before/after the deferred Read.
Other simultaneous peripheral conflicts retain their explicit guards; this is
a partial arbiter, not a measured silicon phase claim. MVMD BRC uses an ordinary
Read sample: its adjacent STLM sees the old BRC, while a separating word sees
the new one. Both copies and their exact bus sequences remain tested.

`interrupt_entry` retains twelve entry-stage/save/vector-poison/cancellation
cases and checks the handler's logical Execute words. Its IMR poison deliberately
enables another pending source during entry; that unrelated source is masked
after the five captured-entry checks so the terminal IDLE can remain asleep.
Leaving it enabled correctly wakes staged IDLE despite INTM, and is not evidence
of failed interrupt restoration. `xc_interrupt` checks both IRQ and NMI,
accepted/annulled words, two-word spans and loop wrap. Speculative second-loop
Prefetch is recorded separately from exactly two body executions. Entry drains
the already-decoded terminal IDLE; the non-loop return has a second IDLE as its
valid continuation. The delayed-branch table in `port_marked_operations` retains all six condition/slot
contrasts and actual marker values; both continuations are nine model clocks
after the initial marker.

The `transition_register` fixture distinguishes adjacent ST-to-TRN / LDM
from a separated readback: staged Read sees the old TRN before the older
Execute store in the shared clock; a separating word reads the stored value.
Both variants retain the final TRN store, circular pointer, flags and unrelated
register assertions. Short indirect PSHM is qualified for TRN only, with
page-zero addressing independent of DP/CPL and retained AR modification.
Unqualified mapped sources and repeat/control compositions fail closed.

`bus_timing` retains all operand intervals and checks that the target
IDLE already decoded behind a delayed branch drains before IRQ entry. A second
IDLE supplies its valid wake continuation. `xc_word_boundary` retains
both selected words and the full data/I/O trace: the decoded successor PORTW's
Read precedes IACK, its Write drains before the interrupt frame, and RTN points
past that instruction. These are staged ordering assertions, not atomic
instruction-boundary timing or measured silicon latency.

## STLM suspension of an interrupted loop

Outside-body STLM to ST1 is admitted without delay/RPT/XC composition.
Software BRAF clears retire future loop prediction without flushing words
already fetched. Architectural BRC/RSA/REA are preserved for software
save/restore. `block_repeat_control` qualifies BRAF-only STM/POPM ST1 clears
at least six words before REA (SPRU131G 7.5.8.2); other in-body status and
loop-register writes remain guarded. STM publishes at Read, POPM and RSBX at
their owning stages, rather than waiting for an unrelated Execute edge.
Manual activation is qualified only with sufficient Prefetch lead before RSA;
return-based context reconstruction remains a separate boundary.

`stlm_repeat_restore` owns320 independent cases/restores: outer counts
0/1/2/7/56, inner counts 0/1/3/63, IRQ/NMI, two pages, two stack alignments,
FRETE/FRETED, six distributed bus-prefix snapshots and callee returns inside
the loop or at REA+1. Stack and snapshot correlations with every input are
deliberately not retained. It shares the complete stack/MMR trace builder with
the ordinary leaf and direct-body return cases, not implicit phase state.
The program saves real interrupt state, computes the BRAF-clearing word
with LDM/AND/OR/STLM, runs its own loop, restores REA/RSA/BRC before ST1,
then returns through the interrupted callee. Ordered reads/writes,
one interrupt delivery, actual iteration counts and balanced frames are
checked. Poisoning each popped word checks retained operands; a wrong BRAF
clear fails at the guest STLM's controller write.
The fixture separates STLM from the following STM because BRC's Read-owned
publication can precede the older status store's Execute in the same clock.
Primary constraints remain SPRU131G 6.10.6 and 7.5.7.1; physical phases and
arbitrary overlapping loop-controller writes remain unvalidated.

A read-only live-vector/stack probe identifies this prologue as the frame
ISR: vector `ffc0` contains delayed branch `f273 3204`, with ST0/ST1 saves
in its delay slots. The transfer interrupt uses a different vector (`ffc8`
to `3598`); RF activity alone does not identify the interrupt source.
At the pre-admission stop, SP=`1eaf` holds BK/REA/RSA/BRC=`0052/4dee/4deb/0003`.
Saved ST1/ST0 at `1ec1..1ec2` are `c903/081f`; the next stack words are
ISR return `408d`, callee return `4def`, and outer return `30d8`.
The interrupted callee was entered by CALLD at `4deb`, whose two-word
delay instruction ends at the loop endpoint `4dee`.
Thus the qualified composition is interrupt entry during an outside-body
callee, software suspension of the saved loop, and eventual restoration
through that callee's return. Merely admitting STLM is not sufficient.
The handler's `AND #2000,A; OR #4900,A` computes `4900` from saved `c903`,
clearing BRAF. The value now retires organically in the native run.

The same probe logs every changed write at consumer index `0853`, rather
than the canonical observer's first eight. All twenty-one observed writes
are nonzero, including ring wraps; the second initialization resets the
index before advance eight. This confirms the aggregate for this capture,
not twenty-one packets in one session. Probe evidence is local artifact
`tmp/st1-context-native/error.log`; neither probe changes firmware state.
The live checker retains the 0.5-second register/upload checkpoint and
the later service evidence; physical menu access is verified separately.

## MVDM accumulator slices

The staged Class-19A path admits MVDM destinations AL/AH/AG/BL/BH/BG.
The absolute source is retained once and the existing mapped-register write
merges only that slice; AG/BG retain eight bits, and SXM/OVM do not extend
or saturate a register copy (SPRU172C 4-110). The two-word slot publishes
at the existing Class-19A clock; this is not a measured bus-phase claim.

`mvdm-acc` passes 110,592 cases and save-state replays across six slices,
RAM/AR/BK sources, eight words, eight accumulator patterns, sixteen
SXM/OVM/CPL/FRCT combinations, two pages and three snapshots. The source
literal and data change after capture; an immediately following logical
consumer must preserve the merged accumulator. All neighboring slices,
status and unrelated accumulator remain checked. `mvdm-acc-edge` adds
2,064 replayed exclusions covering all 86 currently excluded mapped
sources in `0000..005f`, two pages and two snapshots, without data reads,
writes or accumulator/status changes. MVDM-to-accumulator repeats remain
guarded; arbitrary interrupt/repeat compositions are not newly qualified.
The unchanged core rejects the first accumulator case before source access.

## Direct Page-Zero Stack

PSHM/POPM use seven-bit direct MMR addresses independent of DP and CPL
(SPRU172C 4-132 and the existing Class-17A pop contract). The staged path
admits `0060..007f` through the existing data map, not synthesized values.
On the current NSE-1 board map these addresses use the data backing store;
this does not identify the physical function of every MAD2 register there.
Peripheral-page `0020..005f` and indirect encodings remain excluded.

`stack-page0` passes 73,728 cases and save-state replays: all 32 addresses,
eight words, four stack placements, two CPL values, two pages and three
snapshots. Push, pop, zero/one/three-gap pairs and consecutive pushes check
retained volatile sources, exactly-once writes and SP updates, unchanged
status/accumulators and source/stack overlap. The fixture's `0060/0061`
provider uses saved storage in this profile, rather than the separate
interrupt-trigger provider used by older profiles. `stack-page0-edge`
adds 1,536 replayed exclusions for 64 peripheral and 128 indirect encodings,
both instructions, two pages and two snapshots, with no data access or
stack allocation.

SPRU131G 7.3.2 requires same-location on-chip reads to consume a pending
write rather than stale memory. The consecutive-push overlap case fails
without forwarding (case 61952, second source `0061`, stale `5aa5` instead
of pending `0000`). The bounded implementation preserves the older stack
write and samples the younger source once after that same-clock publication,
as for the existing adjacent PSHM/POPM contract. This does not qualify all
memory arbitration or measured bus phases; indirect and arbitrary interrupt/
repeat compositions remain outside this gate.

## Parallel Store/Load

The staged frontend admits all six ST||LD forms: either accumulator as
store source, with A/B or T as load destination (1,536 encodings).
X/Y addresses and updates are Access-owned, using original pointers and
Xmod priority when the auxiliary registers alias. The X sample and ASM
are retained in Read. The store samples the old accumulator at Execute,
before replacing an aliased accumulator destination. Loading T publishes
in Read after older Execute writes; it is not republished at Execute.
The [mapped-source qualification](#mapped-source-in-parallel-storeload)
extends X reads below `0060`. Mapped/peripheral Y remains
excluded before pointer changes; single-repeat mapped sources are separately
excluded.

`parallel_load` owns 15,552 cases/restores: 9,216 all-encoding/layout/BK
cases, 6,144 ASM/SXM/SST store cases and 192 memory/SXM/carry load cases.
It separates store conversion from load numeric dependencies instead of
crossing every dimension. Eight memory words and accumulator patterns,
all32 ASM counts, both SXM/SST states and three checkpoints remain. Pages,
address/status/seed cohorts and store/load numeric interactions are deliberately
distributed. The RAM load, arithmetic and multiply sections together replace
626,688 executions with 54,464. The old factories, variant schedule and mirrored
host census are deleted; mapped/volatile/BSP fixtures remain separate.
Source memory,
ASM, accumulators, pointers and T change after their respective capture
points. Mapped sources have a separate positive fixture; destination-MMIO
and mapped repeated operands are outside this bank's qualification.
The existing parallel-MAC store-shift path and ordinary-RAM forwarding
are shared; no DSP reply or firmware state is supplied by this qualification.

Primary data semantics are TI SPRU172C 4-178/4-179; PMST.SST saturation is
specified by SPRU131G table 4-3, rather than assuming that OVM controls
store saturation. A remaining status ambiguity is explicit: SPRU172C lists
a C effect without an equation, while SPRU131G table 4-1's carry summary
does not name this form. The atomic model's carry-preservation policy is
retained and regression-tested, **not** promoted to a silicon contract.
Gate output requires `carry_policy=preserve_unvalidated`; native acceptance
reports `parallel_store_carry_claim=false`. Physical bus phases and arbitrary
interrupt/repeat compositions remain outside the standalone fixture scope.

## ADDC ownership

ADDC retains unsigned Smem in Read and samples its accumulator and carry
in Execute. Sign extension is suppressed regardless of SXM; OVM and
sticky OVA/OVB follow the arithmetic result (TI SPRU172C page 4-8).
Carry is defined at bit 32, not at the 40-bit guard boundary (SPRU131G
table 4-1). An input word of `ffff` plus carry must remain a 17-bit
increment, not wrap to zero before addition.

`unsigned_mapped_alu` covers ADDC/ADDS/SUBS in 16,512 cases/replays. Each
operation has 2,048 full word/accumulator/flag/destination/late-sampling
numerical inputs on one RAM form, 384 six-form RAM routing inputs and 3,072
mapped-source routing inputs. All sixteen mapped sources and three forms
remain, with distributed early/late sampling, pages/checkpoints. Source words
and auxiliary registers change after Read; selected cases change the live
accumulator/carry before Execute. `addc_chain` separately covers adjacent
ADDC instructions in AA/BA/AB/BB order, checking the preceding ALU's carry
and accumulator updates without a gap. `addc-edge` adds 1,920 no-read,
no-write exclusions for control/unowned mapped sources. Interrupt,
repeat/control composition and physical phase timing remain unqualified.
The native `07f8 000e` (ADDC T,B) at `4914` executes; no arithmetic result
is supplied by the observer.

## Unsigned Mapped Arithmetic

ADDS (`02xx/03xx`) adds unsigned Smem; SUBS (`0axx/0bxx`) subtracts
unsigned Smem. In particular, `0af8 000b` is SUBS from absolute BL, not
ADDS. Both suppress sign extension regardless of SXM and ignore input C;
output carry/no-borrow is taken at bit 32, with sticky OVA/OVB and OVM
saturation (SPRU172C 4-10/4-194; SPRU131G table 4-1).

The staged path admits mapped sources `0008..0017`: six accumulator
slices, T/TRN and eight auxiliary registers. Sources are actual mapped
reads retained in Read; the accumulator and arithmetic flags are
Execute-owned. SP/BK, control and peripheral sources remain excluded.
ADDS/SUBS now share the `unsigned_mapped_alu` section described above,
with numerical coverage separated from address routing. Source memory,
mapped words and pointers change after Read, including self-aliasing
accumulator sources. `unsigned-edge` adds 3,840 replayed exclusions for
80 addresses and DP/absolute/indirect forms without data access, writes
or pointer changes. Owned AL (`0208`) is a positive mapped-source case;
it must not be confused with the separately guarded low/control sources.
The fixture shares address/sample ownership with ADDC but uses operation-specific
add/subtract expectations. Native `0af8 000b` at `403e` executes
without an observer-supplied source/result; the RF/host counters do not
increase at this new endpoint. This does not qualify arbitrary
interrupt/repeat composition or measured bus phases.

## Circular MVDK qualification

Short circular sources `71c0..71df` retain one raw source word, update the
source pointer using the old BK/index, then publish the destination. Native
`71d2` uses a +1 circular step, not AR0. `circular_mvdk` passes
26,840 independently compiled cases and save-state replays, including rejected sources,
all 32 encodings, 21 destination kinds, 18 owned mapped sources and native
BK=82. Destination admission is register-specific: CLKMD at `0058` has a
separate move path, so the peripheral range is not a blanket rejection
domain. Circular/indexed CLKMD writes are not covered by the dedicated
clock-move gate. Physical phase timing and interrupt/repeat composition
remain unvalidated.

The replacements separate legal pointer geometry, every mapped-source/
destination pairing, and eight raw payloads per destination kind. BK includes
0/1/7/16/82 with explicit legal self-indexed AR0 geometry. Page/flag/checkpoint
and payload/geometry cross-products are distributed, not retained as exhaustive
ordinal coverage. The old scheduler, source-extraction host audit, digest and
legacy addressing source are deleted. Exact source Read, pointer update,
destination publication, literal/source poison and all unselected fields are
checked with replay. A wrong indexed update fails the independent section.
Run `python3 tools/run_c54x_section.py circular_mvdk` or `indexed_mvdk`.
This does not establish physical bus phases or RF sample encoding/acquisition.

The resident sequence `ec05 7192 13d0` at `7f8d..7f8f` is qualified:
`RPT #5; MVDK *AR2+,13d0`. TI SPRU172C page 4-108 specifies six
copies, incrementing the destination EAR as well as the source's explicit
postincrement. It becomes single-cycle after the repeat pipeline starts.
`repeated_mvdk` owns 17,616 independent cases/restores across
184 direct/short/circular source encodings, four counts, six disjoint/overlap
placements and both CMPT/CPL states; pages and three snapshots are distributed.
Its address oracle skips 48 self-indexed geometries that enter mapped
memory, rather than claiming them as RAM cases. Mapped
repeat accesses, physical memory-bank timing and interrupt composition
remain unqualified. The overlap tests pin the existing staged order:
Access(n) samples before Read(n-1) publishes in the same clock; this is not
a silicon timing measurement.

The native sequence sets BRC=5 at `7ff3`, establishes the block with
`f072 7ff8` at `7ff5`, then calls `7ffa` with `f074 7ffa` at `7ff7`.
The call spans the entire two-word block. Its call/return and repeat-end
bookkeeping is qualified by the near-call profiles below.

## Near calls within block repeats

CALL/CALLD to an outside target is admitted only when the complete call and
any delay words fit inside the block. RET/RETD restore the caller's repeat
prediction without changing the ordinary one-word return frame: CALL pushes
PC+2, CALLD PC+4 (TI SPRU172C page 4-27). The existing 16-bit repeat-end
comparator, including its cross-page hazard, is unchanged.

`block_near_call` owns 864 independent cases and save-state replays: BRC 0/1/5/15,
middle/endpoint/whole-block calls, pages 0/2/127, one/two call depths,
IRQ/NMI controls, CALL/CALLD and RET/RETD. Four save points are distributed
instead of multiplying every semantic case by four. Ordered data
copies, exact interleaved stack transactions and ordinary return addresses, delay effects,
final BRC/BRAF and a once-only post-loop sentinel are checked. The final
parking IDLE follows interrupt-drain space: a pending IRQ legitimately
wakes an adjacent IDLE (SPRU131G section 6.11.1), so that cannot itself
prove a failed return. Physical phase timing is not a silicon measurement.
The staged implementation's one-depth whole-block RETD defers pending
interrupt entry: CALL reaches the next iteration's callee Read, while CALLD
drains the block first. IACK and the pending copy Write are checked separately;
the interrupt frame is bounded and must equal live RTN on both write and pop.

## Memory-sourced repeat counts

`RPT Smem` captures RC once in Read, rather than interpreting its address
literal as a count. That retained value supplies the pipelined body's
remaining issues. The admitted mapped sources are `0008..0019`: A/B lanes,
T, TRN, AR0..AR7, SP and BK. Control/peripheral reads remain excluded.
Every new body replaces the previous opcode before Read can seed an issue
count, including non-pipelined bodies such as NOP. Otherwise a following
NOP repeat can issue stale copies and update their auxiliary registers.
The count-owner address comparison wraps the low 16-bit PC within XPC.

`memory_count_repeat` owns 1,730 independent cases/restores: all36 count
address forms, counts0/1/7/255, CPL/compatibility states and none/IRQ/NMI,
plus one case each for counts8000/ffff. Pages and three checkpoints are
distributed; duplicate high-count page/checkpoint copies are removed.
Every case seeds its own CPU/RAM. The header reads the count once and
modifies its pointer once; the NOP body is fetched once and must retire
count+1 times despite poisoning its backing opcode. RAM count sources are
poisoned after Read. Mapped count-register values remain intact.

A saved scalar retirement counter replaces the legacy instruction-history
vector. It must reach the expected total before IRQ/NMI acknowledgement and
again at case completion. A pre-count input pulse is latched before the
earliest checkpoint, avoiding queued-line ambiguity on restore. The optional
interrupt owns one bounded continuation write/read pair, both equal to RTN;
RETE restores SP/status. Omitting the retirement counter from the snapshot
fails during replay. Exact physical interrupt latency remains unclaimed.

`memory_count_copy` owns 864 independent cases/restores: all 24 addressing/source
forms, 18 owned mapped sources, counts 0/1/7/31, MVDD/MVKD/MVDK bodies and
none/IRQ2/NMI. Flag/raw-word patterns, pages and four snapshots are assigned
rather than fully crossed; those correlations are no longer coverage claims.
Each source is mutated after Read; body fetches and count reads
remain once-only, copies total RC+1, and interrupts follow the final copy.
`memory_count_wrap` owns 24 cases/restores with an absolute header at `fffe..ffff`
and the body at `0000`. `memory_count_chain` owns 24 copy→memory-count-NOP
sequences and replays, checking four NOP retirements and no stale pointer
updates. Both retain AL/RAM counts, all counts and three body kinds, with
pages/flags/patterns/snapshots assigned. Whole-data-space traces distinguish
MVDD's first-copy drain from the two-word body's overlapping copy sequence.
The chained fixture fails without replacing non-pipelined body
ownership. Class-5A/5B's existing two held clocks remain partial stage
placement, not a measured physical timing claim.

The native `47f8 0008; e59c` sequence at `37f9..37fb` executes with the
firmware's own AL count. No count or packet is supplied by the observer.

## Staged dual-memory MAC family

MAC, MACR, MAS and MASR use the complete `Bxxx` encoding family, with
independent source/destination accumulators. TI SPRU172C pages 4-82 and
4-94 specify signed X/Y multiplication, Read-phase publication of X into T,
and optional rounding by adding `0x8000` and clearing the low sixteen bits.
The accumulator is sampled at Execute; SMUL product saturation precedes
addition/subtraction, and rounding precedes destination overflow handling.

`dual_mac`, `dual_mac_round`, `dual_mas` and `dual_mas_round` each own
38,912 cases/restores: 6,144 address cases and 32,768 numerical cases. Together
they replace 671,744 executions with 155,648. Every encoding, source/destination
pair, alias layout and linear/circular BK mode remains, but addressing no longer
crosses all old page/checkpoint/status/seed cohorts. Those interactions are
deliberately reduced to representative contexts, not claimed equivalent.
Numerical cases retain all 64 operand pairs, eight feedback seeds, four
source/destination pairs and every FRCT/OVM/SMUL/sticky-overflow combination.
SXM is distributed rather than doubled because this instruction's operands
are signed independently of SXM; both contexts remain. Pages/checkpoints are
also distributed. Rounding uses signed floor division, not the CPU bit-mask
implementation. Explicit memory/T/pointer/BK poisoning and feedback changes
preserve Read ownership and Execute-live accumulation. The common clocked
runner owns actual snapshot restores; the old profile and runner branch are gone.
Low-page rejection uses the shared dual-read admission controls rather than
a separate per-operation matrix. Physical memory-bank timing and interrupt/repeat
composition remain unvalidated; this is not native handset acceptance.

## Staged parallel store/MAC

The next staged family is `Dxxx`: `ST src,Ymem || MAC[R]/MAS[R] Xmem,dst`.
SPRU172C pages 4-180/4-182 and TI's
[SPRU179B, pages 4-184/4-186](https://speech.di.uoa.gr/dsp/manuals/E18.pdf)
agree on separate store-source and arithmetic-destination selectors,
signed `T * Xmem`, optional rounding after accumulation, and storage of
the old source accumulator when source equals destination. SPRU131G
5.5.4 requires X to be read before an aliased Y write and gives X's
modifier priority when both operands select the same auxiliary register.
PMST.SST applies to the shifted store value (SPRU131G 4.1.2).

`d6e1` is specifically `ST B,*AR3 || MACR *AR4+0%,A`. Pipeline observations
are not retirement records: rejection in Decode can freeze older pending
slots. Admission alone does not prove retirement; isolated instruction
fixtures qualify results separately from bounded native execution.

The atomic implementation is not a complete status oracle: both TI
instruction manuals list C as affected for parallel stores, whereas the
current helper preserves it for ST/MAC. Their instruction descriptions do
not define that carry update. Resolve this discrepancy before claiming
full status fidelity; neither an atomic comparison nor a successful boot
can establish the missing rule. The staged prototype retains the atomic
model's carry-preservation hypothesis explicitly, not as a validated rule.
It captures the two addresses before X-priority pointer modification,
retains X and T in Read, retains ASM separately, and samples both
accumulators at Execute before writing the old source and replacing dst.
The store shares the ordinary pending-store forwarding mechanism, whose
adjacent-consumer composition is not qualified by the isolated fixture.
Adjacent ASM/SXM/FRCT/OVM writers, physical memory-bank timing and
interrupt/single-repeat composition remain unvalidated.

`parallel_mac`, `parallel_mac_round`, `parallel_mas` and `parallel_mas_round`
each own 43,008 cases/restores: 6,144 encoding/address cases, 32,768 numeric
cases and 4,096 store-conversion cases. Together they replace 671,744 executions
with 172,032. All encodings, selector pairs, alias layouts and BK modes remain;
address/status/seed interactions and numerical page/checkpoint/SXM combinations
are deliberately distributed, not preserved as full cross-products. The separate
store bank covers all 32 ASM encodings, SXM/SST combinations, feedback seeds
and source/destination pairs. Cross-accumulator store sources remain the explicit
sentinels; same-accumulator stores cover all eight feedback values.

Read owns one X transaction and retained T/ASM; Execute owns one Y write
of the old shifted source before accumulator replacement. Bit-position store
expectations independently apply SXM and signed/unsigned SST saturation.
Pointer/BK, memory, T, ASM and feedback mutations exercise the retained/live
boundaries. Exact Read5/Write6, publication, IDLE and replay are checked.
The custom dual-operation snapshot/tap runner and raw-key schedule are deleted.
`dual_admission` separately retains sixteen parallel source limits. Carry and
physical-bank timing remain unvalidated silicon claims; these are CPU-only
model contracts, not handset acceptance.

### Representative dual admission controls

The MPY/MACSU/MAC/SQDST/LMS Access paths share one low-page operand guard.
Dual ALU uses the same guard with a mapped-register exception; parallel
ST/MAC uses a separate source/destination guard. `dual_admission` retains MPY16,
ADD/SUB32 and parallel MAC16: four X/Y low-page placements, both
program pages and before/after-guard snapshots, with ADD/SUB separate for ALU.
These explicit cases replace the old raw-key profile digests. They assert no operand effects or pointer
updates and retained live/replay state. These model-limit tests do not overlap
the Smem admission grammar or establish a hardware exception policy.

Per-operation MAC-family/MACSU/SQDST/LMS profiles and seven further parallel
operation/selector blocks are retired. The 288 removed executions no longer
qualify those individual opcode/feedback combinations' rejection behavior.
Representative guard coverage is not equivalent to the old per-opcode matrix;
successful arithmetic and address coverage is unchanged.

## Staged square distance

SQDST uses all 256 `E2xx` encodings. TI SPRU172C page 4-160 specifies
that B accumulates the square of old A's signed bits 32..16, before A is
replaced by the X/Y difference shifted left sixteen bits. The implementation
retains both memory words in Read and samples old A/B at Execute. T is
neither an input nor a publication. The generic SMUL product clamp remains
the existing arithmetic model, not new evidence about the fitted MAD2 revision.

`square_distance` owns 5,632 cases/restores: 1,536 encoding/layout/BK cases,
2,048 memory-pair/status cases and 2,048 old-A/old-B/status cases. Memory
difference and square accumulation have separate numeric dependencies, so
their former full cross-product and checkpoint copies are removed. Address/
status/seed interactions and pages/checkpoints are representative, not equivalent
to all old cohorts. All 64 memory pairs, eight signed-17-bit A values, eight B
values and 32 status configurations remain. Literal witnesses require the
17-bit square and its SMUL-clamped boundary. Integer expectations check carry,
separate sticky A/B overflow, OVM clamping, FRCT, and the existing SMUL model.
Post-Read accumulator mutation distinguishes Execute-owned feedback from
retained X/Y words; it also detects squaring A after replacing it. Low-page
admission is covered by the representative shared dual-read guard controls.
Physical memory-bank timing and interrupt/single-repeat composition remain
unvalidated.

## Staged accumulator saturation

SAT A/B is admitted as a register-only Execute operation. TI SPRU172C,
page 4-154, specifies signed 32-bit saturation independently of OVM and
replacement of the selected accumulator's overflow flag. `saturation_pair`
verifies 512 cases and save-state replays: 16 signed 40-bit
patterns around both clamp boundaries, both accumulators, all initial
overflow-bit combinations and SXM/OVM combinations. Pages and three
snapshots are distributed around adjacent Execute publications. A second adjacent
SAT clears the overflow from a preceding clamp. The other accumulator,
unrelated status bits and modes remain unchanged. This qualifies the
declared staged schedule, not measured silicon pipeline phases;
single-instruction repeat remains rejected by the existing repeat guard.

`rotate_pair` retains 768 cases/restores: ROR/ROL/ROLTC on A/B, eight values,
all four C/TC combinations and all four SXM/OVM modes, with distributed pages
and checkpoints 5/6/7. Two consecutive ROR/ROL instructions forward carry;
ROLTC instead continues to use TC. The result clears guards and preserves
the other accumulator and unrelated full status. These are staged pair
interactions, not duplicates of the atomic single-instruction examples.

Together these retire 6,400 duplicated page/checkpoint executions. Six
independent literal examples are native expected results, not a layout-only
host census. Legacy factories/profile plumbing and that census are removed.
Their case data
explicitly specifies the program/setup and register observations at ticks
0/6/7; the reusable runner observes each CPU clock, restores at the declared
checkpoint and requires real IDLE at tick 16. It neither computes results
from CPU helpers. Every case starts independently; native execution checks
1,280 successful restores, exactly two opcode fetches and no data-bus
accesses. No Nokia devices, GSM, private ROM or physical phase measurements
are involved.

## Staged circular accumulator stores

Short STL/STH modes 8 through 11 use the existing circular address generator:
the old auxiliary-register address is retained at Read, its pointer is updated
once using the retained BK, and accumulator data is sampled at Execute.
TI SPRU131G section 5.5.3.4 defines the nonzero-block alignment, initial-index
and step-size constraints. The `store-circular` fixture reuses the independently
calculated circular-load address cases and the indexed-store transaction checks.
Its 786,432 cases and replays cover all four store selectors, eight auxiliary
registers, compatibility selection,
block sizes, boundary indices, signed steps, saturation modes, program pages
and snapshots before/after address and data publication. Low-page destinations
are rejected before pointer or bus effects; destinations are never read.

BK=0 retains the pre-existing linear-update behavior as an explicit legacy
exception. The manual describes suppressed circular modification instead;
this store extension does not resolve that fitted-silicon discrepancy or claim
measured physical pipeline timing. Single-instruction repeat is still excluded.

## Short mapped-AR ADDM

DP and plain indirect `ADDM #lk,Smem` may target AR0..AR7, in addition to
the separately qualified absolute mapped-AR path. The short forms retain
the source at Read and publish one arithmetic result at Execute; a newer
write to the destination cannot replace the retained operand. Compatibility
mode's ordinary ARP update remains visible for indirect addressing.

`addm-ar-short` passes 49,152 cases and cleared-pipeline replays: two forms,
eight registers, four values, four immediates, sixteen SXM/OVM/carry/overflow
configurations, both compatibility modes, two pages and three checkpoints.
It checks Read at clock 6, Write at clock 7, retained literal/source values,
one read/write and unchanged accumulators and unrelated ARs. The indirect
fixtures use a separate addressing pointer. Short modifying forms and other
mapped/peripheral destinations are not newly admitted; physical pipeline
timing and additional compositions are not inferred from this matrix.

The active-speech erasure bench organically reaches `6b84 0001` at `c38c`,
with AR4=`0010` selecting AR0 in both frontends. Its staged continuation
completes all 163 frames with the same PCM hash as atomic execution. The
recovery-policy reference differences remain characterized, not accepted as
conformance; the strict staged erasure comparison still rejects them.
`efr-loss-observe` and `efr-erasure` load the same hashed inputs as their
atomic controls. Observation requires its own staged marker, full frame/RAM
replay and a valid driver; a completed characterization cannot satisfy the
strict conformance checker.

## BD long accumulator multiply

`delayed_long_accumulator` qualifies stationary long-offset and absolute
MPYA/MASA/MACA/MACAR to B within BD's two delay words: 768 independent cases/restores
cover eight addressing ARs, three address forms, four operations and eight
numeric patterns. Arithmetic modes, page and four checkpoints are assigned,
not fully crossed. Initial A/B are poisoned and replaced after Read, so
Smem and T are Read-owned while accumulator inputs
remain Execute-owned. Other long addressing modes and control owners are
not newly admitted.

## Pending stack allocation and MVMM

A directly preceding PSHM's pending stack decrement participates in MVMM's
Decode view of SP when the destination is AR0..AR7. Actual stack allocation
remains at Read and the stack write remains at Execute; forwarding must not
allocate twice. `push-mvmm-sp` checks 2,304 cases and cleared-pipeline replays:
one/two pushes, AR2/ST0/ST1/SP source families, three gaps, eight destinations,
normal/wrapping stacks, two pages and three checkpoints, with exact bus events.
This does not qualify SP-to-SP composition or other push families.

The original EFR decoder wrapper reads its return address using MVMM SP,AR2
after PSHM AR2 and PSHM ST0. A stale SP selects a different wrapper despite
identical entry registers and 65,440 ordinary RAM words. Frame completion alone
therefore does not establish correct decoder execution.

## Absolute XORM

Staged `6af8` retains its absolute address, immediate and Read sample, publishes
one XOR result at Execute and leaves status unchanged. The same XOR result
feeds pending-write forwarding. The XORM bank in `long_memory_rmw` checks 512
cases/replays: four addresses, 64 value/status combinations and both compatibility
modes, with pages/four checkpoints distributed. An adjacent load must consume the XOR result without a
second bus read; later literal, pointer and RAM changes cannot replace retained
operands. Other XORM addressing forms remain unqualified. These are emulator
phase contracts, not measured silicon timing.

## MVDM to T and TRN

Staged `720e/720f` use the existing Class-19A absolute-source transfer:
retain one address literal, read the source once and publish the encoded
mapped register at clock 6. This follows the MVDM operation in SPRU172C
4-110; the clock placement remains an emulator pipeline contract, not a
measured Lead bus phase. Source ownership and repeat exclusions are unchanged.

`mvdm-t` checks 12,288 cases and cleared-pipeline replays: T/TRN destinations,
RAM/AR4/AR1 sources, eight values, sixteen status combinations, zero/one-gap
mapped LDM consumers, two pages and four checkpoints. Literal changes after
Decode and source changes after Read cannot replace the retained transfer.
The consumer reads the new unsigned word; status, the opposite T/TRN register
and B remain unchanged. No additional mapped/peripheral sources, repeat or
interrupt compositions are qualified by this fixture.

## Staged LMS

`LMS Xmem,Ymem` (`e1xx`, TI SPRU172C page 4-80) retains both RAM samples
at Read and uses the live A/B accumulators at Execute. A adds SXM-extended
X shifted sixteen bits plus `8000`; B accumulates the signed X/Y product,
with FRCT and PMST.SMUL product saturation. A owns carry, and overflow flags
remain sticky and independently destination-owned. Neither calculation
publishes T. Address capture and X-priority alias modification use the
existing dual-operand path; low-page sources reject before either read or
pointer update.

`least_mean_square` owns 22,016 cases with cleared-pipeline replay: 1,536
encoding/layout/BK cases, 2,048 A/X/status cases, 16,384 B/X/Y/status cases
and 2,048 combined-A/B/status cases at one memory pair. Each output's complete
numeric dependency domain remains; the full A/B/memory correlation is deliberately
reduced. All 256 encodings, three layouts, linear/circular updates, 64 memory
pairs, eight A and B patterns and 32 flag configurations remain, with pages
and three checkpoints distributed. Literal witnesses distinguish signed and
unsigned LMS addition independently of the general formula.
It changes A/B after Read to distinguish Execute-owned feedback, poisons
source words and pointers after capture, and checks exact paired reads,
carry/overflow, no writes and unchanged newer T. Low-page admission is covered
by the representative shared dual-read guard controls. Physical bank
conflicts, measured silicon phases and repeat/interrupt compositions are
not established by these fixtures.

## Staged repeated T-based MAC

Short RAM `MAC/MACR/MAS/MASR Smem,A/B` bodies (`28xx..2fxx`, short
address byte below `98`) use the existing pipelined repeat issue/retirement
path. Each iteration retains its memory and T operands at Read, uses the
live destination accumulator at Execute, and does not publish T. Rounding,
fractional multiplication, PMST.SMUL product saturation, accumulator
saturation and sticky destination overflow retain ordinary-instruction semantics.

`repeated_tmac` owns3144 independent cases and cleared-pipeline replays: both
destinations, four operations, DP/plain/postincrement RAM forms, eight
operand/accumulator patterns and sixteen flag configurations at eight copies.
The count bank covers1/2/256 copies per form/op with assigned numeric boundaries;
the retained C16/SMUL setting remains coupled as in its input contract. Two pages
and four snapshots are distributed, not multiplied. It checks every accumulator publication,
one source read per iteration, a single retained opcode, source/opcode poisoning,
and preservation of a newer T value after the last Read.
This is bounded instruction qualification, not measured silicon timing or
blanket acceptance of mapped, extended, interrupt or block-repeat compositions.

## Staged TC conditional calls

Nondelayed CC with TC/NTC uses the existing retained call transaction.
SPRU172C page 4-29 assigns condition byte `30` to TC and `20` to NTC:
`f930` is TC and `f920` is NTC. `tc-call` verifies 64 cases
and replays across both predicates, both initial TC values, two program
pages and four snapshots. After the decision, TC and accumulator values
are changed and the target literal is overwritten: only the retained
decision may allocate a frame, the target must stay retained and RET
must balance the single taken-call write. Untaken calls touch neither
SP nor stack memory. This fixture does not establish measured silicon
pipeline phases.

Delayed CCD TC/NTC (`fb30`/`fb20`) uses the same retained Read decision and
the existing delayed-call PC+4 frame. `tc_delayed_call` owns 256 independent cases
and cleared-pipeline replays: both predicates and initial TC values, eight
unrelated accumulator patterns in either accumulator, two delay layouts,
two pages; five snapshots are distributed across decision/frame/delay/return state.
An organic SSBX/RSBX in either delay slot flips TC after evaluation; it
cannot change whether the call owns a frame or reaches its retained target.
Target/delay words and the return frame are poisoned after their samples.
The matrix checks both delay words once on both paths, no untaken stack
transaction, and one balanced taken call. Disabling the TC admission fails
the fixture. The acquired encoder uses `fb30` at `cd7f` during its first
DTX SID interval. Block-repeat compositions remain guarded and interrupt
compositions remain unqualified;
the former edge controls now use unadmitted compound bytes `24`/`34`
instead of the qualified `20`/`30`.

## Staged accumulator-lane comparisons

CMPM admits accumulator lanes `0008..000d` through the existing memory-read
path. SPRU172C page 4-33 defines a 16-bit equality test that replaces TC;
the staged implementation captures the operand at Read and publishes TC at
Execute. `cmpm-acc` verifies 6,912 cases and replays across all six lanes,
direct/indirect/absolute forms, DP/SP modes, four values, two literals, both
TC branch senses, both initial TC values, two program pages and three
snapshots. Accumulators are poisoned before Read and changed after it;
the literal is also overwritten after capture. Only one operand read occurs,
with no data write, and the branch consumes the retained comparison result.
Other mapped registers and peripherals remain outside this qualification;
the fixture does not claim measured physical pipeline phases.

## Repeated data-memory moves

Short MVKD destinations use real repeated pipeline slots, not the atomic
interpreter or a bulk memory copy. SPRU172C page 4-113 and SPRU131G section
6.7/table 6-17 specify count+1 transfers, an incrementing absolute source
address and single-cycle issue after setup. The fetched source literal is
retained; each injected slot owns its incremented 16-bit source, DAGEN
destination, Read value and Execute write. Resume skips both words of MVKD,
unlike the pre-existing one-word pipelined repeat bodies.

`rpt-mvkd` verifies 9,216 cases and replays across DP/SP and four short
indirect modes, eight auxiliary registers, counts 0/1/2/7, both immediate
count forms, distinct/equal/overlapping layouts, two program pages and four
snapshots. It checks consecutive writes, source progression, pointer updates,
one opcode/literal fetch and retention after source/literal poisoning.
Same-address reads inherit the existing pending-write bypass documented by
SPRU131G 7.3.2: forwarded iterations do not issue a second RAM read.
This differs from assuming that physical Read-before-Write ordering must
return the old memory word. The source and issue state survive restoration.

Mapped sources/destinations and long-offset/absolute Smem repeat forms
remain excluded. Interrupt composition and physical pipeline phases remain
unqualified for this new repeated form.

## Immediate accumulator-field publication

STM to AL/AH/AG/BL/BH/BG retains its literal through Decode and writes
the selected field at Execute, not on DAGEN's early-write path. The other
fields are preserved from the live accumulator at publication; AG/BG retain
only eight bits. The mapped-producer ordering rule from SPRU131G section
7.5.10 also covers this writer, so an adjacent MVDK samples after its mapped
write rather than treating it as a direct-ALU producer with setup latency.

`stm-acc` verifies 3,456 cases and replays across six fields, six literals,
four accumulator patterns, SXM/OVM combinations, two program pages and
three snapshots. It poisons the fetched literal, changes sibling fields
before Execute, checks every clock's accumulator state and follows the
write with a mapped MVDK read and RAM publication. Both writes and the
single read must occur once at their declared stages; flags and modes
are unchanged. The schedule remains a staged-core model, not a measured
silicon-phase claim.

## Long-address T/TRN stores

ST T/TRN admits absolute, signed-offset, premodified and circular long
Smem forms using the shared extended-store address path. SPRU172C page
4-167 specifies the source and additional address word; the modeled
destination is retained at Read while T/TRN remains Execute-live.
`extended_t_store` verifies 1,024 accepted cases and 128 rejected cases,
each with replay, across both sources, four values, all eight auxiliary
registers, compatibility selection, and RAM/peripheral endpoints. Program
pages and checkpoints are distributed rather than crossed. It poisons
the literal and changes auxiliary registers after address capture, and
changes T/TRN immediately before Execute. No destination read occurs;
peripheral writes retain the existing one-extra-clock model.

Rejected mapped destinations have no data effects or pointer/ARP updates.
Rejection does not consume value or endpoint dimensions. Standard mode now
seeds ARP0: the legacy fixture's nonzero ARP with CMPT clear was invalid
setup, not useful standard-mode coverage. Compatibility mode retains ARP4
and all explicit pointer selections. Accepted paths must reach IDLE; rejected
paths must latch the implementation guard without reaching IDLE.
`st-t-branch` verifies 32 cases and replays of
ordinary stores after both outcomes of nondelayed TC/NTC branches.
A draining conditional-branch control counter is not evidence that the
following instruction occupies a delay span. Actual delay spans, repeats,
XC and block-body compositions remain unqualified and explicitly excluded
for this new form. The schedule is not a measured silicon-phase claim.

## Unowned low-address bus reads

The CPU's internal map does not own data words `0002..0005`; accesses
delegate to the board data map. Shifted LD now preserves that dispatch
instead of rejecting every low-page address as an unsupported CPU register.
No register, return constant or peripheral response has been added.
`shifted_unowned` owns 20,480 cases/restores: all four addresses,
all 32 shifts, DP/SP/absolute/post-increment/offset forms, SXM/OVM states,
both accumulators and four volatile words, with pages and three checkpoints
distributed instead of 122,880 crossed executions. It checks a single bus read,
retained operands/literals/pointers
and Execute publication without the peripheral-region extra clock. This
qualifies bus delegation in the current model, not the physical identity,
electrical behavior or fitted-silicon timing of reserved register-map gaps.

The native `a5e8` chain loads data word `00f1`, adds word `00f2`, and
publishes AR3 through STLM `8813` at `a5ec`. Passive existing-access taps
show boot clearing both fields, then firmware storing `00f2=3` while
`00f1` remains zero. The bounded native controls take this
initial path and make six early reads of board word `0003`, returning zero
under the existing board model. The atomic run later publishes `00f1=b736`
and `b71a`. Thus the zero base is not a staged-only AR3 derivation error;
it is not independent proof of real reserved-gap hardware behavior either.
The two profiles use different SWWSR policies, so this comparison is not
a timing oracle. Logged `pc_context` values are observation-time PCs, not
asserted instruction-retirement addresses.

The staged checker retains the ordered field writes/reads, six early gap reads
and their counts as regression evidence. Its result explicitly leaves
`physical_low_gap_identity_claim` false. The Lua taps issue no extra reads
of these fields or endpoints and never replace values.

## Indexed McBSP control

`mcbsp_control` retains sixteen cases/restores across two program pages,
both SXM states and four semantic checkpoints. It attaches the real indexed
McBSP device at test-map addresses 38/39, initially enabled/ready with its
selector on another register. Firmware writes selector 1, reads status 3,
and clears XRST using the retained status word. The read observer queues
a real DXR word, making the live status 1 before the masked write publishes
2. The device must finish with selector 1/status 0 and transmit no word.
CPU AR1, accumulators and status remain unchanged.

The ordered model-regression events occur at clocks 9/12/13. Snapshots at
4/9/12/13 retain CPU, device and observer state before selection, after
selection, after the now-stale read and after reset. Omitting the DXR write
fails the busy-state check. This is indexed-device/CPU composition, not a
physical address-map, serial-pin or silicon-timing claim.

## Extended accumulator stores to auxiliary registers

Independent sections `extended_ar_store` (3,584), `extended_ar_consumer`
(1,024), `shifted_ar_store` (17,920) and `shifted_ar_consumer` (5,120)
replace the legacy `st-extended-ar`, `st-ar-consumer`, `st-shift-ar` and
`st-shift-ar-use` profiles. Each case seeds its own CPU/program/data state.
All eight AR destinations, STL/STH A/B selectors, four accumulator patterns,
SST/SXM combinations and shifts -16/-1/0/1/15 remain. Address-only cases
cover absolute, offset, preincrement and circular forms with self/other
pointer aliases; absolute addressing has no alias axis. Consumer cases use
absolute stores followed by indirect LDU at both legal gaps 2/3. Page and
checkpoint choices are distributed, not crossed with every input.

The address is retained before source publication: ARs are checked then
poisoned, and the source accumulator changes immediately before Execute.
A bit-position shift oracle and signed/unsigned saturation determine the
expected word independently of CPU helpers. One write occurs at clock 7
(8 for the shifted extension), with no destination read. Address/shift
literals are poisoned after capture. Consumer reads occur at Write+gap;
subsequent RAM poisoning must not change the loaded accumulator. Replay
checks unchanged arithmetic flags, unrelated ARs, source, SP and BK.
These are model timing/ownership checks, not measured silicon timings.

## Circular PORTR and block composition

PORTR admits short circular modes 8/9/10/11 through the shared address
generator (SPRU131G 5.5.3.4). Access retains the old destination and updates
its selected pointer once; Read samples the retained port and Execute
publishes the sampled word. No data-destination read occurs. Unsupported
mapped destinations stop before pointer or I/O effects.

The independent `circular_portr` section verifies 14,784 cases and replays
across all four modes, eight registers, compatibility selection, block sizes
and index boundaries, four accepted samples, two full-width port literals
and both flag patterns. Pages and three checkpoint positions are distributed
rather than crossed. Duplicate self-indexed AR0 setups and sample variants
rejected before I/O are removed. Literal and pointer poisoning and a volatile
I/O endpoint expose re-sampling and duplicate effects; debugger peeks do not
consume the endpoint. Access updates the pointer at clock 5, I/O Read occurs
at 6, and RAM Write at 7, with no destination read.
BK=0 retains the declared legacy linear behavior, not a new silicon claim.

The independent `circular_portr_block` section verifies 288 cases and replays
of circular post-increment inside RPTBD: counts 2/7/17, BK 1/7/16, every
auxiliary register, compatibility modes and both initial ring positions.
Program pages and snapshots at first Read, first Write and mid-block Read
are distributed. An increment-on-read test endpoint supplies distinct words;
the fixture checks two-clock issue cadence, retained sample publication,
ring wrap, final RAM/BRC, unchanged unrelated registers and observational
Read/Execute PCs. Omitting endpoint restoration fails on the replayed read.
It does not supply GSM packets or assert measured physical bus timing.

The current native run logs the first sixteen RF reads per initialization
pass, at modeled frames 30 and 28. Observation-time PCs
`324e/3254/325a/3260` repeat; these are not retirement addresses. Its final
provider counter is 10,336 reads, not an uninterrupted-run total: the logged
sample index restarts at one between passes. RF acquisition and handset
boot remain unclaimed.

## PORTR consumer forwarding

SPRU131G 7.3.2's same-location pending-write bypass applies to an ordinary
RAM destination populated by PORTR, not only to accumulator stores and memory
copies. The staged Read/Execute model retains the I/O word and forwards it
to an adjacent same-address consumer while preserving the real EB write.
It must not reread the port, forward to a different address, or extend this
rule to mapped/peripheral destinations without separate qualification.

The ROM's request dispatcher makes this load-use contract observable:
`35b9` reads the in-service mask into RAM, `35bb/35bc` loads and complements
it, and `35bd/35bf` reads and ANDs the incoming request. A stale RAM read can
cancel the request against its own mask and silently skip host commands.
Consequently, a live RF loop without an illegal instruction is not sufficient
evidence of functional boot or host delivery.

The `portr-bypass` fixture failed on stale RAM (`a55a` instead of an I/O
sample of zero) without forwarding. It passes 6,656 cases and replays:
6,144 LD/AND/OR/XOR consumer cases covering both accumulators, SXM states,
eight samples, zero/one/two gaps, aliased/non-aliased addresses, two program
pages and four snapshots; plus 512 versions of the native request-mask
sequence. The volatile endpoint is sampled once and the real memory write
still occurs once per PORTR. This qualifies the logical on-chip RAM contract,
not measured bank contention or peripheral forwarding.

Ordinary `noki5110` and its standard-bus observation fixture use stage-owned
execution and standard SWWSR decoding. The legacy-dialect negative control
`nse1plg` still stops on the original `7728` initializer; it is a bus-profile
contrast, not an atomic engine or a hardware verdict. The standard composition
accepts the challenge, advances host consumers and passes separate physical
Menu, power and save-state tests. Matching bounded RF/control prefixes do not
establish physical timing, acquisition or whole-session parity.
`make check-c54x-rom4-staged` generates fresh own-model EEPROM/NVRAM and
checks the reset-free ten-second capture against the reviewed structured
fixture `fixtures/c54x_rom4_staged_checkpoint.json`. The fixture is a
regression fingerprint, not a physical timing specification. The checker
requires all 390 records in order, unchanged non-time fields, ordered bounded
timestamps, the same half-second ownership epochs, MCU validator `r6=1`, no
reset/fault, and unique binding of all four observed uploads to their MCU
source offsets. It does not pin absolute sub-microsecond timestamps. The
machine identity, clear ILLEGAL flags and accepted MCU challenge are enforced
independently of the fingerprint, so re-banking data cannot promote a fault.
The four handset gates independently test physical input and restoration.
The first RF-control writes include two six-write bursts, `2813/0030` at
`4026`, then two more six-write bursts. Their transfer targets begin
`0834/0884/08d4/0924/0974`. Later controls include ascending
`2727..272d` writes at `4026` in the logged prefix. The observer
logs only the first 32 controls and first 256 transfer expiries; summaries
count all 262 controls and 2,878 expiries in this ten-second capture.
CPU ILLEGAL must remain clear. In stopped runs, device timer edges alone
are not further execution. RF-control
and transfer-expiry parity remain open; these are calibrated bench-device
events, not RF acquisition.

The four-second native coherent gate requires two full six-write descriptor
bursts followed by `2813/0030` at PC context `4028` (thirteen controls total),
plus the firmware queue producer. The five-write partial fingerprint is
rejected, not retained as another accepted profile. Its minimum-frame bound
is 800; the separate thirty-second scan check retains its exact longer-run
counts and reload/selector requirements.

