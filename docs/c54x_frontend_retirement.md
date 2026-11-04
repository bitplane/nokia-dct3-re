# Atomic frontend retirement inventory

## Decision and scope

Target one staged execution model, but do not delete atomic before migrating its
unique assertions and the ordinary native 5110 composition. This is a migration inventory with selected execution evidence, not a complete
staged parity certificate. No section is approved for
deletion solely because of its name or similarity to another section.

The retirement baseline has 518 registered sections, 18,148 source lines and
1,477,911 registered cases. Its 137 sections explicitly assigning
`pipeline_frontend = false` contain 5,991 source
lines and 11,468 registered cases. Seven additional sections select the frontend
through an expression: 241 lines and 39,048 cases. Counts describe entire
sections, not atomic-only case counts. Helpers and firmware integration sources
are additional migration work. The common case defaults to staged.

Current validated progress: 106 sections (10,819 cases plus restores) migrated;
31 of the original explicitly atomic sections remain, containing 654 cases.
The baseline counts
describe the retirement scope, not the current number of atomic assignments.
The seven originally paired sections are now staged-only: 24,040 cases remain
from their 39,048-case baseline. Their original staged stimuli, assertions and
checkpoints are unchanged; 15,008 atomic-side executions/replays are removed.
Source admission retains all 23,748 staged cases, not an assumed half split.

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

## Implementation and integration dependencies

`execute_one` is approximately 2,077 lines but is shared: staged Execute falls
back to it and accelerated staged repeats also call it. Retain shared instruction
semantics. The clear retirement surface is `retire_atomic_instruction`, the
atomic arm of `execute_run`, atomic pending-I/O/XC state and conditional branches
on `m_pipeline_frontend`; branch-local helpers require a call-site audit before
removal. Header state, save registration, reset and restore must change together.

Ordinary `noki5110` now explicitly selects staged and standard SWWSR decoding.
The frontend-only trial rejected firmware STM `7728`; standard decoding is
therefore a necessary part of this composition, not an unrelated research
override. `nse1astd` and `nse1aclk` explicitly retain atomic comparison behavior.
Validate ordinary boot/menu, codecs, peripheral/audio integration, power/reset
and save/restore without inheriting unrelated research overrides.
Other HLE-backed products do not establish native DSP parity.

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
codec profiles. `tms320c54x_test_rom4_context.cpp` switches back to atomic in
multiple continuations: those linked fixture sequences need a separate migration,
not just removal of codec BIOS aliases. Benchmark tools, machine registrations,
gates and documentation also expose frontend variants.

## Section work queue

### Validated migrations

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

### Unresolved migration contrasts

`atomic_register_copy_sequences` remains atomic. It writes `7728` under the
legacy default bus profile. A standard-SWWSR trial of the register-copy sequence
passes admission but rejects simultaneous staged peripheral arbitration:
`4858` reads CLKMD at PC `0607` while `7758` writes it at PC `0605`.
Resolve opcode admission and peripheral arbitration respectively, rather than
weakening the illegal-state check or inserting gaps into the original sequence.

Further preserved atomic sections after isolated staged trials:

| Section | First observed contrast | Follow-up |
| --- | --- | --- |
| `atomic_memory_address_literals` | Short RAM ADD repeats now issue through staged slots | Migrated; all 28 cases and restores pass |
| `atomic_multiply_move_literals` | Case 6 reads mapped `0015=0000` as event 0 at clock 6 | Resolve mapped-source sampling |
| `atomic_coefficient_fir_literals` | Case 0, MACP `788b`, sets ILLEGAL | Audit program-coefficient multiply admission |
| `atomic_mapped_moves` | Case 2, MVMD/DAR progression, sets ILLEGAL | Audit mapped-move destination admission |
| `atomic_delayed_call` | Combined AGT+AOV admitted with OVA consumption | Migrated; all nine cases and restores pass |
| `atomic_far_edges` | Case 2, FBACCD, sets ILLEGAL after cold-PC correction | Audit accumulator far-branch admission |
| `atomic_fast_return` | RETF checkpoint has PC `20702` rather than `20683` | Audit program replacement and retained-prefetch lifecycle |

These are migration blockers, not established silicon defects. An atomic
expectation is not automatically correct where implementations disagree.

Additional shared-contract queue (original atomic sections retained):

- `repeat_status`: first case B is `4800`, expected `c800`; inspect repeat/status timing.
- `block_repeat_control`: first case AR1 is 4, expected 1; inspect block endpoint/count behavior.
- `zero_bk` case 144 and `indirect_popm` case 0 hit staged admission limits.
- `clock_control`, `firs_sum`,
  `parallel_load_multiply` stop at ILLEGAL in their first case.
  Resolve instruction families/profile selection, not their literal results.

Sizing columns below record the retirement baseline; current case counts live
in `fixtures/c54x_sections.json`. Dispositions are updated by isolated execution
evidence; untested entries remain routing priorities, not equivalence claims. Validate migrated
sections independently; do not run the complete suite after every edit.

| Section | Cases | Lines | Disposition |
| --- | ---: | ---: | --- |
| `signed_conditions` | 389 | 56 | Migrate independent assertions; check staged admission |
| `return_barrier` | 12 | 29 | Validated staged subset; atomic dimension removed |
| `xc_status` | 64 | 30 | Validated staged subset; atomic dimension removed |
| `xc_latency` | 384 | 46 | Validated staged subset; atomic dimension removed |
| `atomic_conditional_return` | 96 | 33 | Re-derive timing/order; retain assertions |
| `atomic_overflow_branch` | 32 | 31 | Validated staged migration; assertions retained |
| `carry_branch` | 8 | 26 | Migrate independent assertions; check staged admission |
| `repeat_status` | 1 | 14 | Re-derive timing/order; retain assertions |
| `atomic_delayed_call` | 9 | 32 | Migrated; page/frame/predicate assertions retained, OVA clearance checked. |
| `conditional_register_store` | 2,304 | 42 | Migrate independent assertions; check staged admission |
| `zero_bk` | 208 | 27 | Migrate independent assertions; check staged admission |
| `indirect_popm` | 4 | 27 | Migrate independent assertions; check staged admission |
| `block_repeat_control` | 8 | 46 | Re-derive timing/order; retain assertions |
| `clock_control` | 17 | 36 | Re-derive timing/order; retain assertions |
| `accumulator_call` | 2 | 23 | Re-derive timing/order; retain assertions |
| `instruction_examples` | 50 | 21 | Migrate independent assertions; check staged admission |
| `shifted_arithmetic` | 104 | 34 | Migrate independent assertions; check staged admission |
| `rounding` | 64 | 31 | Validated staged migration after RND/circular PORTW admission; assertions retained |
| `square_difference` | 40 | 46 | Migrate independent assertions; check staged admission |
| `firs_sum` | 32 | 35 | Migrate independent assertions; check staged admission |
| `arithmetic_boundaries` | 80 | 47 | Staged validated; ABDST/POLY boundaries, flags and old-source assertions retained |
| `conditional_accumulator_store` | 168 | 50 | Migrate independent assertions; check staged admission |
| `rounded_multiply` | 104 | 67 | Migrate independent assertions; check staged admission |
| `store_shift` | 120 | 41 | Migrate independent assertions; check staged admission |
| `smul_control` | 192 | 42 | Re-derive timing/order; retain assertions |
| `bit_immediate` | 512 | 26 | Migrate independent assertions; check staged admission |
| `parallel_load_multiply` | 64 | 30 | Migrate independent assertions; check staged admission |
| `rounded_load` | 64 | 29 | Staged validated; SXM/result/flags/addressing/read/replay assertions retained |
| `shifted_high_store` | 128 | 27 | Migrate independent assertions; check staged admission |
| `offset_memory_test` | 8 | 24 | Migrate independent assertions; check staged admission |
| `software_interrupt` | 128 | 35 | Migrated; TRAP preserves INTM/IFR while sharing vector/frame delivery. |
| `compact_load` | 28 | 32 | Validated staged migration; assertions retained |
| `stack_address_dependency` | 24 | 33 | Validated staged migration; assertions retained |
| `transition_register` | 20 | 40 | Atomic retained; first staged contrast: C54x case=0 event=0 write=0 address=f value=abcd clock=7 |
| `compact_store` | 640 | 43 | Validated staged migration; assertions retained |
| `reverse_carry` | 2,690 | 46 | Staged validated; independent reverse-carry addresses, RAM and full traversal assertions retained |
| `interrupt_entry` | 12 | 53 | Atomic retained; first staged contrast: C54x case=0 event=1 write=0 address=700 value=0 clock=15 |
| `atomic_xorm` | 256 | 33 | Staged validated; assembler-confirmed long-offset encoding; result/status/pointer/bus/replay retained |
| `scalar_boundaries` | 34 | 66 | Validated staged migration; assertions retained |
| `atomic_repeat_irq` | 2 | 34 | Migrated; body reads/count and deferred IRQ frame/return retained. |
| `atomic_subtract` | 384 | 53 | Migrate independent assertions; check staged admission |
| `square_literals` | 6 | 38 | Validated staged migration; assertions retained |
| `atomic_near_control` | 10 | 50 | Staged validated; original stack stimulus/results, delay decisions and explicit bus clocks |
| `long_literals` | 33 | 68 | Staged validated; all baseline cases plus one T-poison/replay check (34 current cases) |
| `atomic_mac_literals` | 39 | 47 | Migrate independent assertions; check staged admission |
| `atomic_dual_address` | 11 | 46 | Migrate independent assertions; check staged admission |
| `atomic_parallel_literals` | 10 | 54 | Migrate independent assertions; check staged admission |
| `atomic_shift_literals` | 25 | 54 | Validated staged migration after RND/circular PORTW admission; assertions retained |
| `atomic_register_shift` | 36 | 61 | Migrate independent assertions; check staged admission |
| `atomic_register_logic` | 20 | 45 | Migrate independent assertions; check staged admission |
| `atomic_product_literals` | 21 | 63 | Staged validated; corrected MAC encoding and extended DELAY admission; independent results retained |
| `atomic_conditional_control` | 12 | 39 | Re-derive timing/order; retain assertions |
| `atomic_far_edges` | 23 | 66 | Migrate independent assertions; check staged admission |
| `atomic_extended_interactions` | 3 | 55 | Staged validated; physical IRQ moved after real IDLE; page/frame/transfer predicates retained |
| `atomic_fast_return` | 1 | 26 | Migrated; RTN and stack poison retained, settled continuation retimed. |
| `atomic_pending_far` | 2 | 38 | Staged validated; first-delay retirement checkpoint derived; pending transfer replay retained |
| `atomic_program_mapping` | 13 | 68 | Migrate independent assertions; check staged admission |
| `coefficient_mac_literals` | 6 | 38 | Atomic retained; first staged contrast: C54x case MACP-coefficient-no-store/0: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `atomic_memory_literals` | 24 | 91 | Staged validated; baseline assertions plus LTD/STLM T-publication replay regression (25 current cases) |
| `atomic_double_stores` | 8 | 40 | Migrate independent assertions; check staged admission |
| `atomic_address_literals` | 4 | 33 | Migrate independent assertions; check staged admission |
| `atomic_shifted_loads` | 7 | 36 | Migrate independent assertions; check staged admission |
| `atomic_stack_literals` | 9 | 53 | Staged validated; original stack/accumulator fields retained with Read/Write clocks |
| `atomic_mapped_moves` | 4 | 36 | Migrate independent assertions; check staged admission |
| `atomic_memory_tests` | 10 | 44 | Migrate independent assertions; check staged admission |
| `atomic_zero_repeat` | 3 | 26 | Staged validated; once-only logical body execution replaces atomic fetch-count convention |
| `atomic_program_transfers` | 8 | 46 | Validated staged migration; assertions retained |
| `atomic_conditional_repeat` | 4 | 32 | Staged validated; baseline cases plus three extended count-source poison/replay checks (7 current cases) |
| `atomic_transfer_literals` | 34 | 85 | Staged validated; repeated-copy cadence/interleaving explicit; values and counts retained |
| `atomic_mmr_burst` | 2 | 43 | Atomic retained; first staged contrast: C54x case MMR-burst-initialize-and-push/0: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `atomic_memory_sequences` | 3 | 40 | Validated staged migration; assertions retained |
| `atomic_absolute_literals` | 30 | 78 | Atomic retained; first staged contrast: C54x case TCR-store-readback/17: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `atomic_condition_edges` | 6 | 30 | Migrate independent assertions; check staged admission |
| `atomic_alu_store_literals` | 8 | 37 | Migrate independent assertions; check staged admission |
| `atomic_high_store_literals` | 5 | 28 | Migrate independent assertions; check staged admission |
| `atomic_parallel_load_literals` | 8 | 46 | Migrate independent assertions; check staged admission |
| `atomic_move_modify_literals` | 9 | 60 | Migrate independent assertions; check staged admission |
| `atomic_logic_pointer_literals` | 18 | 57 | Migrate independent assertions; check staged admission |
| `atomic_logic_program_literals` | 5 | 40 | Migrate independent assertions; check staged admission |
| `atomic_accumulator_add_literals` | 7 | 31 | Migrate independent assertions; check staged admission |
| `atomic_multiply_move_literals` | 7 | 59 | Migrate independent assertions; check staged admission |
| `atomic_conditional_slot_literals` | 10 | 55 | Validated staged migration; assertions retained |
| `atomic_mapped_load_sequences` | 4 | 53 | Staged validated; same-clock Read/Write ordering made explicit; values retained |
| `atomic_register_copy_sequences` | 4 | 43 | Migrate independent assertions; check staged admission |
| `atomic_guard_conditions` | 18 | 41 | Migrate independent assertions; check staged admission |
| `atomic_memory_alu_literals` | 9 | 40 | Migrate independent assertions; check staged admission |
| `atomic_mmr_transfer_literals` | 4 | 45 | Staged validated; same-clock peripheral Read/older Write order explicit; results retained |
| `atomic_transfer_repeat_literals` | 265 | 55 | Staged validated; all eight-bit MAR repeat counts and original transfer/literal predicates retained |
| `atomic_status_xf` | 8 | 29 | Migrate independent assertions; check staged admission |
| `atomic_single_slot_conditions` | 8 | 27 | Migrate independent assertions; check staged admission |
| `atomic_memory_pairs` | 13 | 70 | Staged validated; adjacent MVDD Read/Write clocks explicit; results retained |
| `atomic_memory_address_literals` | 28 | 84 | Migrated; independent results/events retained, cold PC retimed. |
| `atomic_memory_arithmetic` | 45 | 97 | Staged validated; T-based long-word arithmetic and original literal/bus assertions retained |
| `atomic_dual_arithmetic_literals` | 3 | 29 | Migrate independent assertions; check staged admission |
| `atomic_coefficient_fir_literals` | 5 | 45 | Migrate independent assertions; check staged admission |
| `atomic_dual_add_sub_literals` | 8 | 38 | Migrate independent assertions; check staged admission |
| `atomic_immediate_product_literals` | 11 | 41 | Migrate independent assertions; check staged admission |
| `atomic_asm_arithmetic` | 523 | 51 | Migrate independent assertions; check staged admission |
| `atomic_basic_repeat_sequences` | 5 | 54 | Staged validated; MAR encoding corrected; undefined repeated-ADDM success replaced by explicit fail-closed check |
| `atomic_basic_control_sequences` | 13 | 53 | Validated staged migration; assertions retained |
| `atomic_basic_alu_control` | 10 | 49 | Validated staged migration; assertions retained |
| `atomic_basic_parallel_block` | 9 | 51 | Validated staged migration; assertions retained |
| `atomic_accumulator_calls` | 3 | 31 | Staged validated; CALAD admission, frame/target/delay predicates and same-clock RET/ST order |
| `atomic_bus_timing` | 3 | 37 | Atomic retained; first staged contrast: Section IRQ case=0 line=2 clock=15 data_events=3 previous_case=0 return_pc=0701 |
| `atomic_port_timing` | 15 | 65 | Validated staged migration after RND/circular PORTW admission; assertions retained |
| `atomic_mac_saturation` | 7 | 44 | Migrate independent assertions; check staged admission |
| `atomic_port_marked_operations` | 22 | 83 | Validated staged migration after RND/circular PORTW admission; assertions retained |
| `atomic_compatibility_memory` | 9 | 55 | Staged validated; compatibility pointers, arithmetic/flags, RAM and marker intervals retained |
| `atomic_port_marked_memory` | 11 | 48 | Atomic retained; first staged contrast: C54x case atomic-port-marked-load-store-ALU-and-repeat/9: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `atomic_port_loads` | 6 | 40 | Validated staged migration after RND/circular PORTW admission; assertions retained |
| `atomic_port_marked_transfers` | 9 | 42 | Validated staged migration after RND/circular PORTW admission; assertions retained |
| `atomic_port_marked_counter` | 8 | 40 | Validated staged migration; assertions retained |
| `atomic_absolute_arithmetic` | 16 | 49 | Validated staged migration; assertions retained |
| `atomic_long_offset_literals` | 14 | 60 | Staged validated; corrected ANDM/ORM encoding; resolved addresses, results and marker intervals retained |
| `atomic_memory_route_literals` | 26 | 85 | Atomic retained; first staged contrast: C54x case reada-long-preupdated-destination/16: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `atomic_short_immediate` | 520 | 30 | Migrate independent assertions; check staged admission |
| `atomic_port_delayed_branch` | 6 | 30 | Atomic retained; first staged contrast: C54x register sequence case=1 tick=17 replay=0: relative data-bus clock interval |
| `atomic_port_register_examples` | 6 | 37 | Staged validated; MAR encoding corrected and modeled repeat-tail interval derived; values retained |
| `atomic_port_low_store` | 6 | 37 | Validated staged migration; assertions retained |
| `atomic_port_indirect_output` | 5 | 30 | Validated staged migration; assertions retained |
| `atomic_port_control_examples` | 6 | 37 | Staged validated; extended RPT source, corrected MAR words and modeled marker clocks; values retained |
| `atomic_offset_transports` | 6 | 42 | Atomic retained; first staged contrast: C54x case atomic-offset-transport-or-stationary-AR0-load/0: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `atomic_shift_peripheral_literals` | 10 | 52 | Validated staged migration; assertions retained |
| `atomic_xc_word_boundary` | 3 | 37 | Atomic retained; first staged contrast: C54x case=2 event=2 write=0 address=100 value=1111 clock=14 |
| `bio_atomic` | 29 | 50 | Staged validated; physical predicate samples, control results and logical polling loop retained |
| `peripheral_cost_classes` | 20 | 36 | Atomic retained; first staged contrast: C54x case atomic-CPU-peripheral-and-scratch-cost-classes/2: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `atomic_operand_order` | 46 | 63 | Atomic retained; first staged contrast: C54x case extensions-before-data-IO-and-program-operands/10: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `atomic_intm_follower` | 3 | 33 | Atomic retained; first staged contrast: Section IRQ case=0 line=0 clock=10 data_events=0 previous_case=0 return_pc=09d5 |
| `atomic_xc_interrupt` | 8 | 40 | Atomic retained; first staged contrast: Section IRQ case=0 line=2 clock=12 data_events=0 previous_case=0 return_pc=0804 |
| `atomic_waited_io_control` | 12 | 36 | Atomic retained; first staged contrast: C54x case waited-port-control-defers-pending-IRQ/0: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `stopped_timer_reload` | 48 | 26 | Atomic retained; first staged contrast: C54x case stopped-timer-TRB-reloads-TIM-and-readonly-PSC/16: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `source_admission` | 38,464 | 54 | Validated staged subset; atomic dimension removed |
| `idle_nmi` | 12 | 35 | Validated staged migration; terminal idle cleanup explicit where needed |
| `idle_wake` | 6 | 36 | Validated staged migration; terminal idle cleanup explicit where needed |
| `deep_idle_clocks` | 8 | 34 | Validated staged migration; terminal idle cleanup explicit where needed |
| `bsp_interrupt` | 9 | 42 | Validated staged migration; terminal idle cleanup explicit where needed |
| `idle_dma` | 10 | 38 | Validated staged migration; terminal idle cleanup explicit where needed |
| `idle3_pll` | 1 | 34 | Validated staged migration; terminal idle cleanup explicit where needed |
| `repeat_endpoints` | 12 | 44 | Atomic retained; first staged contrast: C54x case repeat-final-copy-protected-at-page-endpoints/0: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `block_repeat_pages` | 96 | 37 | Validated staged subset; atomic dimension removed |
| `initial_control_sequences` | 3 | 32 | Atomic retained; first staged contrast: C54x case=0 event=1 write=0 address=2fff value=602 clock=11 |
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

Net line savings remain unproven: the affected 6,232 section lines contain useful
assertions that must survive. Immediate code-removal scope is scheduling/state,
not the shared 2,077-line instruction executor. Resolve admission and timing gaps
before promising a deletion count or completion date.
