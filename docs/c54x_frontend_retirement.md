# Atomic frontend retirement inventory

## Decision and scope

Steps 1-4 are complete: one stage-owned execution model, with the atomic
scheduler/state and live comparison machines removed after fixture and native
consumer migration. Step 5 is compatibility naming/API and documentation
cleanup. This is not an all-ISA or physical-clock-equivalence certificate.
No section is approved for
deletion solely because of its name or similarity to another section.

## Step 4 acceptance

All 520 registered sections pass on the single-engine composition: 507 generic
CPU sections (1,460,151 cases) and thirteen copied-firmware sections (1,786),
totaling 1,461,937 cases and restores. Registration-manifest SHA-256:
`9ddb5649e17b77ab0b3909f40c52fd5fadf6bd3c79803fa22b9095d2ba182fcc`.
The complete sequential build/run took 4,103.172 seconds, including pauses
for independent native-fixture rebuilds. It is final acceptance, not the
per-edit workflow.

Renewed acceptance also passes:

- Ordinary 5110 menu, late input, short/long power and save-state/serial replay.
- ROM4 entry/cold boot and 358 linked context cases/restores; CRC, FR/EFR
  packing, 32-frame FR codecs and 163-frame bit-exact EFR codecs with RAM replay.
- Serial/pin audio, clocked boot, wire replay and opcode fingerprints.
- 3310/6210/8210/7110 uploaded-program verifier fixtures.
- MU4 fresh provisioning, six complete-file comparisons, reset/reload,
  fresh-process retained media, original bootstrap, mid-byte replay and full
  uploaded-software restart. The stored NAND hash remains unchanged.
- Native backend BSP/pins, simultaneous-source priority, save/replay, reset
  and sixteen slot-request cases.
- 354 C54x host checks and 85 runner checks, plus focused MU4/backend checks.

The CPU implementation is 978 net lines smaller than the step-3 checkpoint,
including the added fidelity fixes. Shared instruction effects remain; there
is no atomic execution fallback. Compatibility `-pipe` names and the true-only
pipeline-reset API are step-5 work, not alternate engines.

## Retirement baseline

The retirement baseline has 518 registered sections, 18,148 source lines and
1,477,911 registered cases. Its 137 sections explicitly assigning
`pipeline_frontend = false` contain 5,991 source
lines and 11,468 registered cases. Seven directly paired sections contain
241 baseline lines and 39,048 cases. Five more use the shared delay/repeat
helper and contain 1,920 baseline cases: twelve paired families and 40,968
cases in total. Counts describe entire
sections, not atomic-only case counts. Helpers and firmware integration sources
are additional migration work. The common case defaults to staged.

Completed migration: all 137 original explicitly atomic sections migrated
(11,502 current cases plus restores, including 34 added regression contrasts).
The baseline counts
describe the retirement scope, not the current number of atomic assignments.
The twelve originally paired families are now staged-only: 25,000 cases remain
from their 40,968-case baseline. Their original staged stimuli, assertions and
checkpoints are unchanged; 15,968 atomic-side executions/replays are removed.
Source admission retains all 23,748 staged cases, not an assumed half split.
The extended I/O helper now selects operand forms, not a frontend: 1,440 staged
cases cover both bus profiles, with 288 duplicate short-form executions removed.
Its unique 1,152 extended/mapped cases retain waits, READY, IRQ/NMI, poisoning and
replay. The additional helper-owned physical-IACK-disabled IRQ case is staged.

## Step 3 acceptance snapshot

All 518 registered sections pass: 506 generic CPU sections (1,459,919 cases)
and twelve copied-firmware sections (1,770 cases), totaling 1,461,689 cases and
restores. The runner rejects any atomic-selected case before execution.
Catalogue SHA-256: `5974553ecdac8e67ee6a9383113dbc5bbd1f400d8966cc8f776c3dffc4980cda`.

Acceptance covers a 445-section prefix and disjoint 73-section tail. The guard
found the shared delay/repeat comparison loop between them; only that tail's
helper and counts changed, with no CPU or accepted-prefix dependency change.
Total build/run wall time is 3,773.430 seconds (62.89 minutes). This is final
catalogue acceptance, not the per-edit workflow.

Reproduce the current complete catalogue with:

```sh
python3 tools/run_c54x_section.py all --scope all --jobs 8
```

The linked ROM4 context gate passes 358 cases/restores with digest
`ce54ab5aff19dbc0`. Ordinary native 5110 menu, save-state/serial replay, late Menu
input and short/long power lifecycles pass. Four EFR comparison profiles retain
163 bit-exact frames apiece and RAM replay. Host checks pass 354 C54x and 84
runner tests. No atomic scheduling/state or comparison alias is deleted here;
that is stage 4, followed by naming/alias cleanup in stage 5.
The paired serial-codec and clocked pin-boot/replay gates also pass. Their
explicit laboratory clock wiring is not a physical timing-validation claim.

## Disposition rules

- **Migrate assertions:** arithmetic, addressing, flags, rejection boundaries and
  literal instruction examples. Keep independent expectations; change schedules
  only when justified by the staged execution contract.
- **Re-derive timing:** interrupt/return/repeat ordering, bus events, idle, DMA,
  timers and clock transitions. Atomic observation clocks are not staged clocks.
- **Trim paired dimension:** variable-frontend matrices retain their staged
  operand/state contrasts; delete only the atomic dimension after validation.
- **Retire mechanism:** atomic pending-I/O continuations, atomic retirement/XC
  history and mode-switch plumbing, once no live consumer needs them.
- **Delete redundant test:** requires a named retained test covering the same
  inputs, observable effects and restore contract. None is established by this
  scan; this inventory deliberately does not equate similar names with parity.

## Execution boundary

Execution is stage-owned only. The atomic scheduler, pending-I/O continuation,
XC history, delayed-control/repeat scheduler, stack-address history and their
reset/save registrations are removed. Control flow, bus transactions and clock
accounting belong to the pipeline. `execute_one` retains shared instruction
effects, without the duplicate control handlers or instruction-level timing
debits. `set_pipeline_frontend(true)` remains a compatibility reset entry point;
false is rejected, not an alternate execution mode. Naming/API cleanup is step 5.

Ordinary `noki5110` now explicitly selects staged and standard SWWSR decoding.
The frontend-only trial rejected firmware STM `7728`; standard decoding is
therefore a necessary part of this composition, not an unrelated research
override. The `nse1astd` and `nse1aclk` atomic comparison machines are retired.
Validate ordinary boot/menu, codecs, peripheral/audio integration, power/reset
and save/restore without inheriting unrelated research overrides.
Other HLE-backed products do not establish native DSP parity.

MU4 laboratory defaults now select staged with the C5416-compatible bus profile.
Isolated routine wrappers explicitly initialize zero I/O waits because they
bypass the original memory-interface setup; native startup retains its own
SWWSR/SWCR initializer. This does not establish physical bank or READY wiring.
ROM4
integration now selects staged at reset for ordinary
profile names as well as the retained `-pipe` aliases. Both cold names use the
same mask-only Prefetch assertion; the unreachable atomic cold phase is removed.
Generic-section migration is separate from full firmware-application acceptance;
the qualification results below establish the exercised application contracts.

Production promotion acceptance passes menu hash/input, late input, short/long
power presses and idle save/load replay. The audio harness uses `nse1obs`, an
ordinary-5110 composition with only a passive instruction observer, because
program-fetch taps cannot identify staged execution-time helper arguments.
External-clock fixtures remain explicitly laboratory-only. This promotion does
not retire atomic tests or claim physical speech/network timing.
Both the paired atomic/staged codec gate and clocked-boot gate pass, including
serial/wire restoration. All seven paired-codec opcode fingerprints reproduce
the retained drift checks. Related host validation passed 354 C54x tests,
84 runner tests and 80 product/profile/PCM checks (overlapping selections).

ROM4 fixtures in `tests/firmware/tms320c54x_test_rom4.cpp` select staged for paired
codec profiles. The eight former false-mode assignments in
`tms320c54x_test_rom4_context.cpp` were cleanup between staged cases, not atomic
execution continuations; each successor reinitializes staged before scheduling
CPU time. They are removed rather than counted as eight migrated code paths.
Remaining `-pipe` BIOS names are compatibility aliases, not alternate engines.
The codec benchmark defaults to one staged decoder and one staged encoder.

### Stage 4 firmware qualification

Ordinary ROM4 entry and cold boot, CRC helper (512 cases), packed CRC (516),
FR packing (293), EFR CRC (554), EFR packing (277 vectors with round-trip and
corruption checks), FR decoder (32 frames plus replay) and FR encoder (32 frames
plus replay and eight PCM-mute cases) pass on staged. EFR unpacking required
admitting delayed C/NC branches whose targets and complete delay span remain
inside the active block. `block_branch` retains eight new carry/polarity/page
contrasts (392 current cases).

A backward branch can retire beside its target IDLE's Decode. It must not
consume that younger one-word control guard merely because its numeric address
is nearby. Control-word subtraction is bounded, and IDLE's guard belongs only
to IDLE itself. `idle_wake` adds two receive interrupts through the backward
IDLE loop with four replay checkpoints (ten current cases). MU4's original
two-word serial receive/ring assertion passes this boundary; full MU4
application qualification remains separate from that focused result.

The uploaded-program research device and standalone verifier now select staged
with standard SWWSR decoding, as required by the original `STM 7728` initializer.
The 3310/8210 (116 blocks), 6210 (232) and 7110 (228, supplied ROM4) verifier
gates pass, including all existing peripheral/version sensitivity controls and
stock-input fingerprints. The 6210 bootstrap gate also passes. Executed-image
bounds use Execute observation, not speculative Prefetch; the fail-closed I/O
message reports the actual Read owner (`0f9c` for the NSM-3-family fixture).
Publication is checked only after its three-word copy reaches terminal IDLE,
retaining the existing result values and PC assertion. Host input providers are
disabled in these headless fixtures. This is not full application qualification
for every uploaded-program product composition.

The added cases change the catalogue from the step-3 snapshot. Current complete
acceptance is recorded under step 4 above; the old snapshot is not the current
registration count.

### MU4 migration contracts

The fresh InitDisk corruption is resolved by the existing on-chip RAM bypass
contract (SPRU131G section 7.3.2). MVDK captures its source in Access, not Read;
it must use the same pending-write forwarding as Read-stage consumers. An
adjacent ADDM/MVDK pair previously captured the old metadata offset. The new
`rmw_move_bypass` section covers 160 alias/nonalias, addressing, logical/additive,
gap and restore contrasts; reverting only the MVDK forwarding call fails its
first case. Eleven affected move/forwarding sections pass, and fresh MU4 setup
now passes all six complete-file comparisons and the native entry assertions.
The original loader transfers 3,254 records/51,921 words. The retained consumer
uses the C5416-compatible bus-register profile (`7728/7729`). Repeated short RAM
logic has sixty additional generic cases, including the XOR form (`1c91` at
`03:928d`). Fresh setup media has no settings files; the stronger reset/retained
gate must create and preserve those four files through original firmware.
It is not interchangeable with the setup-only gate. Diagnostic write taps and
temporary media-dump probes are removed.

The original InitDisk name-copy helper `491e..492a` passes sixteen isolated
copied-firmware cases with empty/short/eight/twelve-character inputs and four
restore points. Its unchanged delayed BANZ loop is not, by itself, the fresh
directory corruption. This is an added firmware section, not generic ISA scope.

The new IDLE-loop regression also fails with the old control-span arithmetic
and passes after restoring the correction, independently of the full MU4 run.
AAC's ABI fixture already selects staged unconditionally in `machine_reset`
before any CPU time; its constructor default is not an atomic execution path.
The native Nokia DSP backend also selects staged/standard directly, rather
than depending on handset wrappers. The isolated runner's former false-mode
teardown call is removed; it scheduled no atomic instructions between cases.

The serial/pin audio runner no longer executes the atomic comparison dimension.
Its staged pin attachment compares the screen against the retained serial
control, and keeps the same boot, demand, bus, opcode and replay assertions.
RF/call command-line runners cannot select a retired atomic machine; archived
capture metadata remains parseable for existing checker fixtures only.

Native backend BSP and slot-request gates pass. The simultaneous RX/TX fixture
holds the RX vector in a synthetic branch sink: its pending TX source can wake
IDLE even with INTM set, as independently covered by `idle_wake`. It retains
RX-first selection, exactly one saved frame, the return word and pending TX
bit across replay; it must not mistake an invalid terminal IDLE assumption for
lost interrupt priority. Pin, joined-clock, pending-I/O and reset checks remain.

Fresh MU4 reset/retained qualification passes on the single-engine composition.
Both legs retain all six uploads and four original settings files and complete
the exact 11-word receive/14-word transmit status exchange. Full bootstrap
uses Execute-owned entry counts and ACKs four additional validated selector-02
notifications: 23 received words and 78 transmitted words. Both mid-byte replay
legs compare 1,339 registered emulation-state items exactly. The full uploaded
startup/reset fixture also passes. These do not establish physical board reset,
mask-ROM behavior or music decoding.

## Section work queue

### Validated migrations

`atomic_far_edges` retains all 23 transfer/frame/delay/page-wrap cases on staged.
FB[D] and BACC[D]/FBACC[D] share captured target/redirect handling, with only
calls owning frame words. The former mapped-XPC-write page-switch expectation
is corrected: SPRU131G table 6-3 explicitly limits XPC loads to reset/far control
operations. The replacement checks the attempted bus write, unchanged page
and XPC readback; debugger XPC redirects remain separate. Other independent
results and stack/frame assertions are unchanged.

`indirect_popm` retains all four masked/preincrement/postincrement/CMPT/SP
cases on staged, including exact stack reads, resolved MMR writes and final
pointer/stack/status values. Access retains the resolved AR/SP destination;
Read performs stack increment/publication once, with Execute using that
destination rather than reinterpreting the encoded mode byte.

`atomic_mapped_moves` keeps all four MVMD/MVDM progression cases plus a DP=0,
mid-repeat restore regression (five current). Short direct AR-source MVMD and
AR-destination MVDM repeats advance EAR while retaining the fixed MMR.
`repeat_endpoints` retains all twelve page-wrap/IRQ/NMI/final-copy/frame cases;
exact Execute events replace atomic opcode refetch assertions. Its transfer
values/counts, late-IRQ trigger and continuation frame remain unchanged.
`atomic_port_marked_memory` retains all eleven values/address/flag cases; the
six-ADD repeat marker interval is 13 staged clocks, explicitly checked at
clocks 8 and 21 across the declared Prefetch refill.

`coefficient_mac_literals` keeps all six MACP/MACD source, successor, T and
coefficient results on staged; GNU tic54x-as independently corrects long-offset
encoding to offset-before-coefficient. `atomic_coefficient_fir_literals` keeps
all five baseline standalone/streamed MACP/FIRS and MACD cases plus two held
request/pending successor poison-and-replay checks (seven current). A running
repeat retains its opcode and advances PAR without refetching literals; the
first program-bus hold is followed by one coefficient read per iteration.
Repeated MACD, extended MACP repeats and block/XC/control composition remain
explicitly unqualified, not silently routed through atomic execution.

`firs_sum` retains all 32 SXM/OVM/carry/sticky-overflow cases on staged, plus
two coefficient-request/capture poison-and-replay cases (34 current). The
Class-8 program-bus hold retains physical coefficient selection and X/Y words;
the separately qualified stream cases cover single-instruction repeats, not
block/XC/control composition. `repeat_status` now checks the
documented Decode-owned BRAF clearance during the final iteration (SPRU131G
7.5.8.2), with exact two-iteration Execute events and final CPL/BRC state.
Its old `c800` accumulator expectation depended on atomic clearing BRAF after
the endpoint; the correctly observed value is `4800`.

`atomic_mmr_burst` retains both 12-register initialize/overwrite/pop/push
cases. Staged STM now admits inactive RSA/REA setup alongside BRC, with
in-body loop-control mutation still rejected. PSHM T consumes older POPM/STLM
T publication with zero latency (SPRU131G table 7-13); the burst checks the
restored T and every stack/register word. `initial_control_sequences` retains
all three call/repeat/copy results. Both fixtures now assert interleaved bus
events by Read/Execute ownership rather than atomic instruction serialization.

`zero_bk` retains all 208 linear-wrap pointer cases on staged. Its dual-MPY
rows now also assert explicitly initialized source words, both Read events,
the product, T and preserved IMR/IFR. MPY/MACSU admission includes IMR/IFR
sources through the ordinary register map; other newly unowned low-page
multiply operands remain rejected before reads or pointer updates.

`clock_control` (17 cases) and `stopped_timer_reload` (48) retain all protected
clock-field, reload, read-only PSC, bus-value and restore assertions on staged.
NOP separation makes readback observe a completed peripheral write; it does
not qualify simultaneous peripheral arbitration. The stopped-timer settled
observation moves from clock 20 to 24. `atomic_intm_follower` retains all three
protected complete-instruction cases. Frame continuations now follow the
documented decoded-slot drain rather than atomic's immediate boundary;
logical Execute events explicitly prove each drained slot before vector entry.

`parallel_load_multiply` retains all 64 LD||MAC[R]/MAS[R] combinations on staged:
both accumulators, rounding, subtract, SXM, FRCT and pointer updates. One added
Read-T poison/checkpoint case checks that Execute and restore consume captured
T rather than a later register value (65 current cases). Dual operands remain
RAM-only, and repeat/control composition is not admitted by this migration.

`atomic_absolute_literals` retains all 30 results/events on staged, adding
indirect LDM and AR-destination STLM with page-zero addressing and pointer
high-bit clearing. Its TCR read follows the completed write across a NOP;
simultaneous peripheral read/write arbitration is not qualified by this literal
test. `atomic_multiply_move_literals` retains all seven results. Its STLM/MVMM
sequence now obeys the two-word AR latency in SPRU131G table 7-3 (one inserted
NOP), with explicit interleaved Read/Execute/Decode/Access bus clocks.

`atomic_delayed_call` retains all nine page/stack/predicate-before-delay cases.
Staged CC[D] additionally admits the combined AGT+AOV condition (`0x76`),
samples both predicates before the delay words, and consumes OVA as required
by SPRU172C 4-29..31. The fixture now explicitly checks OVA clearance too.

`software_interrupt` retains all 128 vector/INTM/IPTR/TRAP/RETF cases on staged
execution. TRAP shares vector and frame delivery with INTR but preserves INTM
and IFR. `atomic_memory_address_literals` retains its 28 independent address,
data and arithmetic cases; only its cold first-clock PC observation changes.
Short RAM-source ADD repeats now issue through the pipeline, also migrating
both `atomic_repeat_irq` cases (including 65,536 body reads before IRQ entry).
Mapped/peripheral repeated ADD sources remain rejected explicitly.
`atomic_fast_return` retains its CALL-owned RTN, poisoned-stack, second-CALL
overwrite and restore assertions. RETF is installed before Prefetch rather than
patched into an in-flight stream; its settled observation and second debugger
redirect move from clock 20 to 28 to allow the staged refill/return/IDLE tail.

`atomic_register_logic`, `atomic_register_shift`, `atomic_high_store_literals`
and `atomic_alu_store_literals` now select staged. Literal results, flag masks,
bus events and final observations are retained. The two store sections' first
clock observes the cold pipeline's initial PC rather than atomic fetch's next
PC. Existing section names remain stable until the final naming cleanup.

Also validated on staged: `atomic_subtract`, `atomic_accumulator_add_literals`,
`atomic_short_immediate`, `atomic_mac_saturation`,
`atomic_immediate_product_literals` and `atomic_single_slot_conditions`.
Only cold first-clock PC expectations change; arithmetic, carry/saturation,
annulment, bus events and replay checks retain their independent expectations.

Also validated: `atomic_guard_conditions`, `atomic_status_xf`,
`atomic_logic_pointer_literals` and `atomic_mac_literals`. Status/XF publication,
guard conditions, pointer changes, literal MAC results and replay remain checked.

The staged set also includes `atomic_memory_tests`, `atomic_condition_edges`,
`atomic_parallel_load_literals`, `atomic_move_modify_literals`,
`atomic_logic_program_literals`, `atomic_memory_alu_literals`,
`atomic_dual_arithmetic_literals`, `atomic_dual_add_sub_literals` and
`atomic_asm_arithmetic`. Their semantic and bus assertions are unchanged;
only cold first-clock PC expectations are retimed where present.

Also migrated with unchanged literal results and bus events:
`atomic_parallel_literals`, `atomic_dual_address`, `atomic_double_stores`,
`atomic_address_literals` and `atomic_shifted_loads`.

Control-flow migrations validated with unchanged final results, stack/data bus
assertions and restore checks: `atomic_conditional_return`,
`atomic_conditional_control` and `atomic_program_mapping`. Cold initial-PC
observations in the mapping section retain the pre-Prefetch logical address,
including nonzero XPC pages; final page/overlay/ARP/DP expectations are unchanged.

Also migrated without semantic assertion changes: `signed_conditions`,
`carry_branch`, `conditional_register_store`, `accumulator_call`,
`instruction_examples`, `shifted_arithmetic`, `square_difference`,
`conditional_accumulator_store`, `rounded_multiply`, `store_shift`,
`smul_control`, `bit_immediate`, `shifted_high_store` and `offset_memory_test`.

The overflow-branch section now uses staged Read-clock observations (clock 6),
not atomic clock 1. Its original flag results, including delay-slot flag changes,
pass unchanged. See TI SPRU131G, ST0 overflow flags and chapter 7 pipeline rules.

### Idle fixture endpoints

Idle wake, NMI, deep-clock, DMA and BSP fixtures now observe their original
wake masks/pending flags at entry or continuation, then execute real IFR
acknowledgement or IMR masking before terminal IDLE. They no longer depend on
attempting to sleep again with an enabled wake flag still pending. These
revised inputs pass on both frontends; this does not establish real-silicon
IDLE re-entry behavior with an already-latched flag. Temporary CPU tracing
was removed; CPU idle behavior is unchanged.

The PLL fixture preserves its cold lock interval but releases wake after cold
staged startup has drained. Sixteenth-clock observation timestamps are derived
from 32 samples per divided CPU clock; the lock-edge check moves with the
software-reentry stimulus, not with observed CPU results. Both frontends pass.

`atomic_memory_literals`, `atomic_memory_arithmetic` and `long_literals` now
run staged with their independent results and ordered memory events retained.
ABDST shares the dual-read address ownership with SQDST; LTD shares DELAY's
copy path and additionally publishes T at Read. SUBB retains an unsigned memory
operand and consumes inverted carry at Execute. DADST/DSADT/DSUBT now use the
shared long-word arithmetic semantics with a saved Read-owned T latch. The
33 original long-word cases remain, with one additional T-poison/restore case.
`arithmetic_boundaries` also retains all 80 ABDST/POLY boundary cases on staged,
including fractional, saturation, overflow, sticky flags and old-source use.
The four migrated sections pass 183 cases/restores and eight focused controls
pass another 39,626. Host checks pass 354 CPU-tool and 84 runner tests.

`atomic_product_literals`, `atomic_long_offset_literals` and `atomic_xorm`
now select staged. Their long-offset word streams are corrected against an
independent assembler, not against observed CPU output: the offset precedes the
literal. All expected arithmetic, resolved addresses, flags, pointer effects
and ordered bus events remain. The shared atomic reader and disassembler are
corrected too; their former agreement was not independent evidence. Staged
XORM now admits the tested RAM forms, including circular and long-offset modes;
DELAY now retains its absolute/long-offset source in Access, samples in Read
and writes source+1 in Execute. Repeat/block/XC restrictions remain.
The nine focused sections pass 43,584 cases plus restores; the 354 host tests,
four atomic/staged EFR codec/replay profiles and ordinary 5110 menu/save gates
also pass. This is selected regression evidence, not a full-suite certificate.

`rounded_load` and `reverse_carry` now retain all 64 and 2,690 cases on staged.
LDR uses retained SXM for its shifted, low-half-8000 result; the shared DAGEN
already implements reversed-bit carry/borrow, now admitted for the tested LD/ST
forms. Mapped destinations/sources remain excluded for those newly admitted
reverse-address forms. `atomic_memory_pairs` and `atomic_mapped_load_sequences`
retain all 17 cases. Their same-clock younger Reads precede older Execute
writes in the observer stream; exact clocks replace atomic serialization for
the adjacent MVDD and LDM/STL examples. All values, final registers and memory
predicates remain unchanged.
These four sections plus five focused load/store/admission controls pass
43,529 cases/restores. The 354 CPU-tool tests, four EFR codec/replay profiles
and ordinary 5110 menu/save gates also pass.

Six further sections run staged: stack literals, mixed MMR transfers, near
control, accumulator calls, pending far transfers and extended interactions.
All 31 cases retain their values, frame/stack ordering, page, delay-slot and
replay predicates. Bus interleaving follows the existing Read/Execute owners;
an aliasing PSHM/POPM pair retains its deferred same-address read. CALAD now
shares the retained delayed accumulator-call path, captures its target before
delay-slot overwrites and stacks PC+3 without a far-page word (SPRU172C 4-25).
The pending snapshots move to the first delay Execute: clock 8 for one-word
FRETD, 9 for two-word FCALLD. The extended IRQ fixture now asserts its physical
input after cold startup reaches IDLE, not during Prefetch. Its expected frame
and ISR behavior are unchanged. Thirteen focused sections pass 24,171
cases/restores; host CPU-tool checks pass 354 tests.

`bio_atomic` now runs all 29 cases on staged. BIO/NBIO CC, RC/RCD and XC
use their existing predicate owners (Read for calls/returns, Access for XC),
with one physical sample per decision. Its polling case retains the original
pin change at clock 13 and final state, but replaces an atomic transient PC
assertion with the logical Execute sequence and Read samples at 7/12/17.
`atomic_transfer_literals` retains all 34 cases, including three-copy MVKD/MVDK
streams with one-clock cadence and younger-read/older-write overlap explicit.
Seven BIO/control/admission sections pass 24,709 cases/restores, and the
transfer section passes another 34; host CPU-tool checks pass 354 tests.

`atomic_transfer_repeat_literals` and `atomic_conditional_repeat` retain all
269 baseline cases on staged. Short indirect MAR now uses the existing pipelined repeat
engine and its Access-owned pointer update; no data transaction or second
Execute update is added. All 256 eight-bit counts retain count+1 effects,
terminal RC and continuation, alongside indirect/absolute memory-count sampling
and XC carry contrasts. Long-offset and direct MAR forms are not added here.

`atomic_zero_repeat` retains its three zero-count RPTZ/RPTBD/RPTB cases on
staged. Logical Execute sequences prove one body execution and the original
continuations/register results. The former single-fetch assertion is retired:
speculative Prefetch may revisit the block body without executing it twice.
These three sections and eight repeat/admission controls pass 26,428
cases/restores. The 354 CPU-tool checks, four EFR codec/replay profiles and
ordinary 5110 menu/save gates also pass.

`atomic_basic_repeat_sequences`, `atomic_compatibility_memory` and
`atomic_port_register_examples` now run staged (20 cases/restores). The first
and third correct intended `MAR *AR0+` words from `6d10` to assembler-confirmed
`6d90`, preserving their pointer/count results. One basic-sequence assertion
is deliberately not preserved: repeated ADDM's expected success was undefined
under SPRU172C 4-9 (also diagnosed by GNU as). It becomes a fail-closed emulator
guard check, retaining the count-source Read and proving no ADDM data effects;
this does not claim a hardware illegal-instruction trap. Positive memory-count
repeat behavior remains covered by `atomic_conditional_repeat`. The marked MAR
stream's interval accounts for the modeled repeat-tail Prefetch refill, not
atomic scheduling. Compatibility arithmetic/flags and remaining values stay
unchanged. No CPU behavior changes accompany these three fixture corrections.

`atomic_port_control_examples` now retains all six cases on staged. The last
intended AR0 postincrement words are corrected to `6d90`; extended RPT count
sources now retain and consume their offset once in Read, without treating the
offset as a count or a second short AR update. The extended count header also
has a two-word retirement span. Three additional `atomic_conditional_repeat`
cases cover stationary, preupdate and circular count sources, with a clock-7
Read followed by source poison and save/replay. All original four cases remain
(seven current cases). Marked repeat intervals follow the declared count-Read
hold and Prefetch refill, not the atomic schedule. Ten focused sections pass
25,089 cases/restores; host checks pass 354 CPU-tool and 84 runner tests.

One additional `atomic_memory_literals` case protects LTD's Read-owned T
publication against an older STLM-to-T Execute write. It fails on the former
early publication, checks T immediately after the shared clock and restores
before the successor RAM write. All 24 baseline cases remain (25 current).

### Resolved migration boundaries

`atomic_register_copy_sequences` now uses the standard SWWSR profile and keeps
its adjacent CLKMD store/load intact. A saved pending Read serializes that
specific peripheral conflict after the older Write. Two added replay prefixes
cover either side of the Read; an added separated BRC-copy contrast checks the
new word while the original adjacent copy explicitly reads the old word.
The seven current cases preserve the original values outside that documented
Read-before-Execute distinction. Waited I/O/control and the helper-selected
extended I/O matrix are now migrated and covered by final acceptance.

These corrections are not established silicon defects. An atomic
expectation is not automatically correct where implementations disagree.

`block_repeat_control` retains all eight cases on staged. Software BRAF clears
now update the predictor at the owning register-publication stage; its programs
observe TI's six-word lead before the endpoint. Manual activation leaves enough
Prefetch lead before RSA. Nested short/long RPT counts and loop counters remain
independently checked.

Sizing columns below record the retirement baseline; current case counts live
in `fixtures/c54x_sections.json`. Dispositions are covered by the acceptance
above. For future edits, validate affected sections independently; do not run
the complete suite after every edit.

| Section | Cases | Lines | Disposition |
| --- | ---: | ---: | --- |
| `signed_conditions` | 389 | 56 | Staged validated; independent assertions retained. |
| `return_barrier` | 12 | 29 | Validated staged subset; atomic dimension removed |
| `xc_status` | 64 | 30 | Validated staged subset; atomic dimension removed |
| `xc_latency` | 384 | 46 | Validated staged subset; atomic dimension removed |
| `atomic_conditional_return` | 96 | 33 | Staged validated; ordering re-derived, assertions retained. |
| `atomic_overflow_branch` | 32 | 31 | Validated staged migration; assertions retained |
| `carry_branch` | 8 | 26 | Staged validated; independent assertions retained. |
| `repeat_status` | 1 | 14 | Migrated; TI Decode-owned BRAF timing replaces atomic endpoint timing, exact loop sequence checked. |
| `atomic_delayed_call` | 9 | 32 | Migrated; page/frame/predicate assertions retained, OVA clearance checked. |
| `conditional_register_store` | 2,304 | 42 | Staged validated; independent assertions retained. |
| `zero_bk` | 208 | 27 | Migrated; pointer wrap preserved, MPY reads/result/T/IMR/IFR explicitly checked. |
| `indirect_popm` | 4 | 27 | Migrated; all masked/pre/post/CMPT/SP results, bus events and restores retained. |
| `block_repeat_control` | 8 | 46 | Migrated; qualified early BRAF changes and nested RPT preserve loop-count assertions. |
| `clock_control` | 17 | 36 | Migrated; clock field/readback/reload assertions retained with separated peripheral reads. |
| `accumulator_call` | 2 | 23 | Staged validated; ordering re-derived, assertions retained. |
| `instruction_examples` | 50 | 21 | Staged validated; independent assertions retained. |
| `shifted_arithmetic` | 104 | 34 | Staged validated; independent assertions retained. |
| `rounding` | 64 | 31 | Validated staged migration after RND/circular PORTW admission; assertions retained |
| `square_difference` | 40 | 46 | Staged validated; independent assertions retained. |
| `firs_sum` | 32 | 35 | Migrated; all baseline cases plus two coefficient hold/capture poison/replays (34 current). |
| `arithmetic_boundaries` | 80 | 47 | Staged validated; ABDST/POLY boundaries, flags and old-source assertions retained |
| `conditional_accumulator_store` | 168 | 50 | Staged validated; independent assertions retained. |
| `rounded_multiply` | 104 | 67 | Staged validated; independent assertions retained. |
| `store_shift` | 120 | 41 | Staged validated; independent assertions retained. |
| `smul_control` | 192 | 42 | Staged validated; ordering re-derived, assertions retained. |
| `bit_immediate` | 512 | 26 | Staged validated; independent assertions retained. |
| `parallel_load_multiply` | 64 | 30 | Migrated; baseline cases plus one retained-T poison/replay check (65 current). |
| `rounded_load` | 64 | 29 | Staged validated; SXM/result/flags/addressing/read/replay assertions retained |
| `shifted_high_store` | 128 | 27 | Staged validated; independent assertions retained. |
| `offset_memory_test` | 8 | 24 | Staged validated; independent assertions retained. |
| `software_interrupt` | 128 | 35 | Migrated; TRAP preserves INTM/IFR while sharing vector/frame delivery. |
| `compact_load` | 28 | 32 | Validated staged migration; assertions retained |
| `stack_address_dependency` | 24 | 33 | Validated staged migration; assertions retained |
| `transition_register` | 20 | 40 | Migrated; twelve added separated-read contrasts (32 current) and indirect TRN PSHM preserve results. |
| `compact_store` | 640 | 43 | Validated staged migration; assertions retained |
| `reverse_carry` | 2,690 | 46 | Staged validated; independent reverse-carry addresses, RAM and full traversal assertions retained |
| `interrupt_entry` | 12 | 53 | Migrated; exact entry stages, captured vector/stack and cancellation retained; unrelated enabled pending source masked before terminal IDLE. |
| `atomic_xorm` | 256 | 33 | Staged validated; assembler-confirmed long-offset encoding; result/status/pointer/bus/replay retained |
| `scalar_boundaries` | 34 | 66 | Validated staged migration; assertions retained |
| `atomic_repeat_irq` | 2 | 34 | Migrated; body reads/count and deferred IRQ frame/return retained. |
| `atomic_subtract` | 384 | 53 | Staged validated; independent assertions retained. |
| `square_literals` | 6 | 38 | Validated staged migration; assertions retained |
| `atomic_near_control` | 10 | 50 | Staged validated; original stack stimulus/results, delay decisions and explicit bus clocks |
| `long_literals` | 33 | 68 | Staged validated; all baseline cases plus one T-poison/replay check (34 current cases) |
| `atomic_mac_literals` | 39 | 47 | Staged validated; independent assertions retained. |
| `atomic_dual_address` | 11 | 46 | Staged validated; independent assertions retained. |
| `atomic_parallel_literals` | 10 | 54 | Staged validated; independent assertions retained. |
| `atomic_shift_literals` | 25 | 54 | Validated staged migration after RND/circular PORTW admission; assertions retained |
| `atomic_register_shift` | 36 | 61 | Staged validated; independent assertions retained. |
| `atomic_register_logic` | 20 | 45 | Staged validated; independent assertions retained. |
| `atomic_product_literals` | 21 | 63 | Staged validated; corrected MAC encoding and extended DELAY admission; independent results retained |
| `atomic_conditional_control` | 12 | 39 | Staged validated; ordering re-derived, assertions retained. |
| `atomic_far_edges` | 23 | 66 | Migrated; all transfers/frames retained, XPC write expectation corrected to TI table 6-3. |
| `atomic_extended_interactions` | 3 | 55 | Staged validated; physical IRQ moved after real IDLE; page/frame/transfer predicates retained |
| `atomic_fast_return` | 1 | 26 | Migrated; RTN and stack poison retained, settled continuation retimed. |
| `atomic_pending_far` | 2 | 38 | Staged validated; first-delay retirement checkpoint derived; pending transfer replay retained |
| `atomic_program_mapping` | 13 | 68 | Staged validated; independent assertions retained. |
| `coefficient_mac_literals` | 6 | 38 | Migrated; canonical assembler-confirmed offset order and all independent results retained. |
| `atomic_memory_literals` | 24 | 91 | Staged validated; baseline assertions plus LTD/STLM T-publication replay regression (25 current cases) |
| `atomic_double_stores` | 8 | 40 | Staged validated; independent assertions retained. |
| `atomic_address_literals` | 4 | 33 | Staged validated; independent assertions retained. |
| `atomic_shifted_loads` | 7 | 36 | Staged validated; independent assertions retained. |
| `atomic_stack_literals` | 9 | 53 | Staged validated; original stack/accumulator fields retained with Read/Write clocks |
| `atomic_mapped_moves` | 4 | 36 | Migrated; all baseline cases plus DP=0/mid-repeat restore (five current). |
| `atomic_memory_tests` | 10 | 44 | Staged validated; independent assertions retained. |
| `atomic_zero_repeat` | 3 | 26 | Staged validated; once-only logical body execution replaces atomic fetch-count convention |
| `atomic_program_transfers` | 8 | 46 | Validated staged migration; assertions retained |
| `atomic_conditional_repeat` | 4 | 32 | Staged validated; baseline cases plus three extended count-source poison/replay checks (7 current cases) |
| `atomic_transfer_literals` | 34 | 85 | Staged validated; repeated-copy cadence/interleaving explicit; values and counts retained |
| `atomic_mmr_burst` | 2 | 43 | Migrated; RSA/REA setup, zero-latency T push and all register/stack words retained. |
| `atomic_memory_sequences` | 3 | 40 | Validated staged migration; assertions retained |
| `atomic_absolute_literals` | 30 | 78 | Migrated; indirect MMR addressing and completed TCR readback qualified. |
| `atomic_condition_edges` | 6 | 30 | Staged validated; independent assertions retained. |
| `atomic_alu_store_literals` | 8 | 37 | Staged validated; independent assertions retained. |
| `atomic_high_store_literals` | 5 | 28 | Staged validated; independent assertions retained. |
| `atomic_parallel_load_literals` | 8 | 46 | Staged validated; independent assertions retained. |
| `atomic_move_modify_literals` | 9 | 60 | Staged validated; independent assertions retained. |
| `atomic_logic_pointer_literals` | 18 | 57 | Staged validated; independent assertions retained. |
| `atomic_logic_program_literals` | 5 | 40 | Staged validated; independent assertions retained. |
| `atomic_accumulator_add_literals` | 7 | 31 | Staged validated; independent assertions retained. |
| `atomic_multiply_move_literals` | 7 | 59 | Migrated; results retained, AR latency corrected to TI contract. |
| `atomic_conditional_slot_literals` | 10 | 55 | Validated staged migration; assertions retained |
| `atomic_mapped_load_sequences` | 4 | 53 | Staged validated; same-clock Read/Write ordering made explicit; values retained |
| `atomic_register_copy_sequences` | 4 | 43 | Migrated; seven current cases cover serialized CLKMD Read, pending replay and adjacent/separated BRC copies. |
| `atomic_guard_conditions` | 18 | 41 | Staged validated; independent assertions retained. |
| `atomic_memory_alu_literals` | 9 | 40 | Staged validated; independent assertions retained. |
| `atomic_mmr_transfer_literals` | 4 | 45 | Staged validated; same-clock peripheral Read/older Write order explicit; results retained |
| `atomic_transfer_repeat_literals` | 265 | 55 | Staged validated; all eight-bit MAR repeat counts and original transfer/literal predicates retained |
| `atomic_status_xf` | 8 | 29 | Staged validated; independent assertions retained. |
| `atomic_single_slot_conditions` | 8 | 27 | Staged validated; independent assertions retained. |
| `atomic_memory_pairs` | 13 | 70 | Staged validated; adjacent MVDD Read/Write clocks explicit; results retained |
| `atomic_memory_address_literals` | 28 | 84 | Migrated; independent results/events retained, cold PC retimed. |
| `atomic_memory_arithmetic` | 45 | 97 | Staged validated; T-based long-word arithmetic and original literal/bus assertions retained |
| `atomic_dual_arithmetic_literals` | 3 | 29 | Staged validated; independent assertions retained. |
| `atomic_coefficient_fir_literals` | 5 | 45 | Migrated; baseline cases plus two coefficient/successor poison/replays (seven current). |
| `atomic_dual_add_sub_literals` | 8 | 38 | Staged validated; independent assertions retained. |
| `atomic_immediate_product_literals` | 11 | 41 | Staged validated; independent assertions retained. |
| `atomic_asm_arithmetic` | 523 | 51 | Staged validated; independent assertions retained. |
| `atomic_basic_repeat_sequences` | 5 | 54 | Staged validated; MAR encoding corrected; undefined repeated-ADDM success replaced by explicit fail-closed check |
| `atomic_basic_control_sequences` | 13 | 53 | Validated staged migration; assertions retained |
| `atomic_basic_alu_control` | 10 | 49 | Validated staged migration; assertions retained |
| `atomic_basic_parallel_block` | 9 | 51 | Validated staged migration; assertions retained |
| `atomic_accumulator_calls` | 3 | 31 | Staged validated; CALAD admission, frame/target/delay predicates and same-clock RET/ST order |
| `atomic_bus_timing` | 3 | 37 | Migrated; exact operand intervals retained, decoded target IDLE drains before IRQ entry. |
| `atomic_port_timing` | 15 | 65 | Validated staged migration after RND/circular PORTW admission; assertions retained |
| `atomic_mac_saturation` | 7 | 44 | Staged validated; independent assertions retained. |
| `atomic_port_marked_operations` | 22 | 83 | Validated staged migration after RND/circular PORTW admission; assertions retained |
| `atomic_compatibility_memory` | 9 | 55 | Staged validated; compatibility pointers, arithmetic/flags, RAM and marker intervals retained |
| `atomic_port_marked_memory` | 11 | 48 | Migrated; all results retained, repeated ADD markers explicitly retimed. |
| `atomic_port_loads` | 6 | 40 | Validated staged migration after RND/circular PORTW admission; assertions retained |
| `atomic_port_marked_transfers` | 9 | 42 | Validated staged migration after RND/circular PORTW admission; assertions retained |
| `atomic_port_marked_counter` | 8 | 40 | Validated staged migration; assertions retained |
| `atomic_absolute_arithmetic` | 16 | 49 | Validated staged migration; assertions retained |
| `atomic_long_offset_literals` | 14 | 60 | Staged validated; corrected ANDM/ORM encoding; resolved addresses, results and marker intervals retained |
| `atomic_memory_route_literals` | 26 | 85 | Migrated; offset READA admitted and CMPM/MVPD encoding corrected, values and marker intervals retained. |
| `atomic_short_immediate` | 520 | 30 | Staged validated; independent assertions retained. |
| `atomic_port_delayed_branch` | 6 | 30 | Migrated; all conditions, slot effects and port values retained, taken marker interval is nine staged clocks. |
| `atomic_port_register_examples` | 6 | 37 | Staged validated; MAR encoding corrected and modeled repeat-tail interval derived; values retained |
| `atomic_port_low_store` | 6 | 37 | Validated staged migration; assertions retained |
| `atomic_port_indirect_output` | 5 | 30 | Validated staged migration; assertions retained |
| `atomic_port_control_examples` | 6 | 37 | Staged validated; extended RPT source, corrected MAR words and modeled marker clocks; values retained |
| `atomic_offset_transports` | 6 | 42 | Migrated; assembler-checked long transfer words, pointers and values retained; PORTR Read/Write markers re-derived. |
| `atomic_shift_peripheral_literals` | 10 | 52 | Validated staged migration; assertions retained |
| `atomic_xc_word_boundary` | 3 | 37 | Migrated; two-word selection and decoded PORTW drain checked before the IRQ frame. |
| `bio_atomic` | 29 | 50 | Staged validated; physical predicate samples, control results and logical polling loop retained |
| `peripheral_cost_classes` | 20 | 36 | Migrated; all values/accesses retained; separate Read/Write phases preserve total RMW interval. |
| `atomic_operand_order` | 46 | 63 | Migrated; all independent bus/program order assertions retained using assembler-checked extension order. |
| `atomic_intm_follower` | 3 | 33 | Migrated; exact decoded drain, frame and whole-follower completion checked. |
| `atomic_xc_interrupt` | 8 | 40 | Migrated; speculative loop prefetch distinguished from two actual body executions; IRQ/NMI, selection and exact drained frames retained. |
| `atomic_waited_io_control` | 12 | 36 | Migrated; twenty current cases retain transfers/IRQs and add software/READY/operand replay plus older-port/younger-RPT ownership. |
| `stopped_timer_reload` | 48 | 26 | Migrated; all divider/reload/PSC cases and restores retained with completed-RMW readback. |
| `source_admission` | 38,464 | 54 | Validated staged subset; atomic dimension removed |
| `idle_nmi` | 12 | 35 | Validated staged migration; terminal idle cleanup explicit where needed |
| `idle_wake` | 6 | 36 | Validated staged migration; terminal idle cleanup explicit where needed |
| `deep_idle_clocks` | 8 | 34 | Validated staged migration; terminal idle cleanup explicit where needed |
| `bsp_interrupt` | 9 | 42 | Validated staged migration; terminal idle cleanup explicit where needed |
| `idle_dma` | 10 | 38 | Validated staged migration; terminal idle cleanup explicit where needed |
| `idle3_pll` | 1 | 34 | Validated staged migration; terminal idle cleanup explicit where needed |
| `repeat_endpoints` | 12 | 44 | Migrated; exact IRQ/NMI/frame/copy assertions retained, logical repeats replace refetch convention. |
| `block_repeat_pages` | 96 | 37 | Validated staged subset; atomic dimension removed |
| `initial_control_sequences` | 3 | 32 | Migrated; call frames, loop counts and copy words retained with explicit bus overlap. |
| `receive_ring_copy` | 4 | 20 | Validated staged subset; atomic dimension removed |
| `disassembly` | 1 | 13 | Validated staged migration; terminal idle cleanup explicit where needed |
| `instruction_observer` | 24 | 25 | Validated staged subset; atomic dimension removed |

## Exit gates before deletion

1. All explicitly atomic and variable-frontend sections have a validated staged
   successor or an evidence-backed deletion disposition.
2. Linked ROM4 contexts and ordinary 5110 pass on staged without atomic fallback.
3. Shared semantics are separated from atomic scheduling; no live atomic state,
   mode setter or atomic-only fixture remains.
4. Run the full CPU suite and affected handset/firmware acceptance once on the
   final composition. Static parity and codec success alone are insufficient.

All four exit gates are satisfied by step-4 acceptance. Shared instruction
effects and independent assertions remain required; engine retirement is not
permission to delete them wholesale. The true-only reset API must retain its
pipeline-reset behavior when its compatibility name is removed in step 5.
