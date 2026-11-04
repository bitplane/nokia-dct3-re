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

Current validated progress: 75 sections (7,010 cases plus restores) migrated;
62 of the original explicitly atomic sections remain. The baseline counts
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

### Unresolved migration contrasts

`atomic_product_literals` remains atomic: after accounting for cold Prefetch,
case 14 (`MAC offset extension order`, `65e3 0004 0002`) reads `0204` on staged
where the retained atomic fixture expects `0202`. Do not change the expected
address or delete this case to obtain parity. Resolve the long-offset addressing
contract against independent evidence before migrating this section.

`atomic_xorm` and `atomic_register_copy_sequences` also remain atomic after
staged trials set STATE_ILLEGAL in case 0. The former includes `6a05`, beyond
the staged XOR admission of absolute `6af8`; the latter writes `7728` under the
legacy default bus profile. A standard-SWWSR trial of the register-copy sequence
passes admission but rejects simultaneous staged peripheral arbitration:
`4858` reads CLKMD at PC `0607` while `7758` writes it at PC `0605`.
Resolve opcode admission and peripheral arbitration respectively, rather than
weakening the illegal-state check or inserting gaps into the original sequence.

Further preserved atomic sections after isolated staged trials:

| Section | First observed contrast | Follow-up |
| --- | --- | --- |
| `atomic_memory_arithmetic` | Case 1, `5aea 0006`, sets ILLEGAL | Audit extended two-word arithmetic admission |
| `atomic_memory_address_literals` | Case 16, repeated ADD, sets ILLEGAL | Audit repeated-body admission and ownership |
| `atomic_accumulator_calls` | Case 0 observes stack read `02ff=0602` at clock 15 rather than its retained bus sequence | Decode staged call/return ordering before retiming |
| `atomic_memory_literals` | Case 0, ABDST, sets ILLEGAL after cold-Prefetch correction | Audit ABDST admission |
| `atomic_zero_repeat` | Case 2 fetches `0602=f495` at clock 7 outside the expected sequence | Derive staged repeat-prefetch expectations |
| `atomic_transfer_literals` | Case 13 reads `0d91=2222` as event 1 at clock 9 | Resolve transfer bus order |
| `atomic_multiply_move_literals` | Case 6 reads mapped `0015=0000` as event 0 at clock 6 | Resolve mapped-source sampling |
| `atomic_mapped_load_sequences` | Case 0 reads mapped `0013=8003` as event 1 at clock 8 | Resolve mapped-load bus order |
| `atomic_mmr_transfer_literals` | Case 3 reads peripheral `0020=9abc` as event 4 at clock 13 | Resolve MMR/peripheral transfer order |
| `atomic_memory_pairs` | Case 1 reads `0d03=2468` as event 1 at clock 7 | Resolve paired-memory operand order |
| `atomic_coefficient_fir_literals` | Case 0, MACP `788b`, sets ILLEGAL | Audit program-coefficient multiply admission |
| `atomic_stack_literals` | Case 2 reads `0061=abcd` as event 1 at clock 7 | Resolve stack operand sampling/order |
| `atomic_mapped_moves` | Case 2, MVMD/DAR progression, sets ILLEGAL | Audit mapped-move destination admission |
| `atomic_delayed_call` | Case 4 sets ILLEGAL | Audit delayed-call composition admission |
| `atomic_near_control` | Case 9 reads `0063=5678` as event 1 at clock 7 | Resolve near-control operand order |
| `atomic_far_edges` | Case 2, FBACCD, sets ILLEGAL after cold-PC correction | Audit accumulator far-branch admission |
| `atomic_extended_interactions` | Case 2 IRQ checkpoint has PC `10604` rather than `10605` | Derive staged interrupt checkpoint and input timing |
| `atomic_fast_return` | RETF checkpoint has PC `20702` rather than `20683` | Audit program replacement and retained-prefetch lifecycle |
| `atomic_pending_far` | Delay-word checkpoint has PC `10602` rather than `10603` | Derive the staged active-transfer snapshot boundary |

These are migration blockers, not established silicon defects. An atomic
expectation is not automatically correct where implementations disagree.

Additional shared-contract queue (original atomic sections retained):

- `repeat_status`: first case B is `4800`, expected `c800`; inspect repeat/status timing.
- `block_repeat_control`: first case AR1 is 4, expected 1; inspect block endpoint/count behavior.
- `zero_bk` case 144 and `indirect_popm` case 0 hit staged admission limits.
- `clock_control`, `firs_sum`, `arithmetic_boundaries` (ABDST),
  `parallel_load_multiply` and `rounded_load` stop at ILLEGAL in their first case.
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
| `atomic_delayed_call` | 9 | 32 | Re-derive timing/order; retain assertions |
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
| `arithmetic_boundaries` | 80 | 47 | Migrate independent assertions; check staged admission |
| `conditional_accumulator_store` | 168 | 50 | Migrate independent assertions; check staged admission |
| `rounded_multiply` | 104 | 67 | Migrate independent assertions; check staged admission |
| `store_shift` | 120 | 41 | Migrate independent assertions; check staged admission |
| `smul_control` | 192 | 42 | Re-derive timing/order; retain assertions |
| `bit_immediate` | 512 | 26 | Migrate independent assertions; check staged admission |
| `parallel_load_multiply` | 64 | 30 | Migrate independent assertions; check staged admission |
| `rounded_load` | 64 | 29 | Migrate independent assertions; check staged admission |
| `shifted_high_store` | 128 | 27 | Migrate independent assertions; check staged admission |
| `offset_memory_test` | 8 | 24 | Migrate independent assertions; check staged admission |
| `software_interrupt` | 128 | 35 | Atomic retained; first staged contrast: C54x case software-interrupt/64: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `compact_load` | 28 | 32 | Validated staged migration; assertions retained |
| `stack_address_dependency` | 24 | 33 | Validated staged migration; assertions retained |
| `transition_register` | 20 | 40 | Atomic retained; first staged contrast: C54x case=0 event=0 write=0 address=f value=abcd clock=7 |
| `compact_store` | 640 | 43 | Validated staged migration; assertions retained |
| `reverse_carry` | 2,690 | 46 | Atomic retained; first staged contrast: C54x case reverse-carry-address/1: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `interrupt_entry` | 12 | 53 | Atomic retained; first staged contrast: C54x case=0 event=1 write=0 address=700 value=0 clock=15 |
| `atomic_xorm` | 256 | 33 | Migrate independent assertions; check staged admission |
| `scalar_boundaries` | 34 | 66 | Validated staged migration; assertions retained |
| `atomic_repeat_irq` | 2 | 34 | Atomic retained; first staged contrast: C54x case atomic-repeat-pending-IRQ/0: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `atomic_subtract` | 384 | 53 | Migrate independent assertions; check staged admission |
| `square_literals` | 6 | 38 | Validated staged migration; assertions retained |
| `atomic_near_control` | 10 | 50 | Re-derive timing/order; retain assertions |
| `long_literals` | 33 | 68 | Atomic retained; first staged contrast: C54x case literal-long-word-arithmetic/11: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `atomic_mac_literals` | 39 | 47 | Migrate independent assertions; check staged admission |
| `atomic_dual_address` | 11 | 46 | Migrate independent assertions; check staged admission |
| `atomic_parallel_literals` | 10 | 54 | Migrate independent assertions; check staged admission |
| `atomic_shift_literals` | 25 | 54 | Validated staged migration after RND/circular PORTW admission; assertions retained |
| `atomic_register_shift` | 36 | 61 | Migrate independent assertions; check staged admission |
| `atomic_register_logic` | 20 | 45 | Migrate independent assertions; check staged admission |
| `atomic_product_literals` | 21 | 63 | Migrate independent assertions; check staged admission |
| `atomic_conditional_control` | 12 | 39 | Re-derive timing/order; retain assertions |
| `atomic_far_edges` | 23 | 66 | Migrate independent assertions; check staged admission |
| `atomic_extended_interactions` | 3 | 55 | Migrate independent assertions; check staged admission |
| `atomic_fast_return` | 1 | 26 | Re-derive timing/order; retain assertions |
| `atomic_pending_far` | 2 | 38 | Migrate independent assertions; check staged admission |
| `atomic_program_mapping` | 13 | 68 | Migrate independent assertions; check staged admission |
| `coefficient_mac_literals` | 6 | 38 | Atomic retained; first staged contrast: C54x case MACP-coefficient-no-store/0: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `atomic_memory_literals` | 24 | 91 | Migrate independent assertions; check staged admission |
| `atomic_double_stores` | 8 | 40 | Migrate independent assertions; check staged admission |
| `atomic_address_literals` | 4 | 33 | Migrate independent assertions; check staged admission |
| `atomic_shifted_loads` | 7 | 36 | Migrate independent assertions; check staged admission |
| `atomic_stack_literals` | 9 | 53 | Migrate independent assertions; check staged admission |
| `atomic_mapped_moves` | 4 | 36 | Migrate independent assertions; check staged admission |
| `atomic_memory_tests` | 10 | 44 | Migrate independent assertions; check staged admission |
| `atomic_zero_repeat` | 3 | 26 | Re-derive timing/order; retain assertions |
| `atomic_program_transfers` | 8 | 46 | Validated staged migration; assertions retained |
| `atomic_conditional_repeat` | 4 | 32 | Atomic retained; first staged contrast: C54x case RPT-memory-count-sampled-once/2: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `atomic_transfer_literals` | 34 | 85 | Migrate independent assertions; check staged admission |
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
| `atomic_mapped_load_sequences` | 4 | 53 | Migrate independent assertions; check staged admission |
| `atomic_register_copy_sequences` | 4 | 43 | Migrate independent assertions; check staged admission |
| `atomic_guard_conditions` | 18 | 41 | Migrate independent assertions; check staged admission |
| `atomic_memory_alu_literals` | 9 | 40 | Migrate independent assertions; check staged admission |
| `atomic_mmr_transfer_literals` | 4 | 45 | Migrate independent assertions; check staged admission |
| `atomic_transfer_repeat_literals` | 265 | 55 | Atomic retained; first staged contrast: C54x case RPT-every-eight-bit-count-with-MAR-body/9: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `atomic_status_xf` | 8 | 29 | Migrate independent assertions; check staged admission |
| `atomic_single_slot_conditions` | 8 | 27 | Migrate independent assertions; check staged admission |
| `atomic_memory_pairs` | 13 | 70 | Migrate independent assertions; check staged admission |
| `atomic_memory_address_literals` | 28 | 84 | Migrate independent assertions; check staged admission |
| `atomic_memory_arithmetic` | 45 | 97 | Migrate independent assertions; check staged admission |
| `atomic_dual_arithmetic_literals` | 3 | 29 | Migrate independent assertions; check staged admission |
| `atomic_coefficient_fir_literals` | 5 | 45 | Migrate independent assertions; check staged admission |
| `atomic_dual_add_sub_literals` | 8 | 38 | Migrate independent assertions; check staged admission |
| `atomic_immediate_product_literals` | 11 | 41 | Migrate independent assertions; check staged admission |
| `atomic_asm_arithmetic` | 523 | 51 | Migrate independent assertions; check staged admission |
| `atomic_basic_repeat_sequences` | 5 | 54 | Atomic retained; first staged contrast: C54x case basic-repeat-and-transfer-sequences/0: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `atomic_basic_control_sequences` | 13 | 53 | Validated staged migration; assertions retained |
| `atomic_basic_alu_control` | 10 | 49 | Validated staged migration; assertions retained |
| `atomic_basic_parallel_block` | 9 | 51 | Validated staged migration; assertions retained |
| `atomic_accumulator_calls` | 3 | 31 | Re-derive timing/order; retain assertions |
| `atomic_bus_timing` | 3 | 37 | Atomic retained; first staged contrast: Section IRQ case=0 line=2 clock=15 data_events=3 previous_case=0 return_pc=0701 |
| `atomic_port_timing` | 15 | 65 | Validated staged migration after RND/circular PORTW admission; assertions retained |
| `atomic_mac_saturation` | 7 | 44 | Migrate independent assertions; check staged admission |
| `atomic_port_marked_operations` | 22 | 83 | Validated staged migration after RND/circular PORTW admission; assertions retained |
| `atomic_compatibility_memory` | 9 | 55 | Atomic retained; first staged contrast: C54x case atomic-compatibility-memory-and-fixed-ADDM-boundaries/2: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `atomic_port_marked_memory` | 11 | 48 | Atomic retained; first staged contrast: C54x case atomic-port-marked-load-store-ALU-and-repeat/9: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `atomic_port_loads` | 6 | 40 | Validated staged migration after RND/circular PORTW admission; assertions retained |
| `atomic_port_marked_transfers` | 9 | 42 | Validated staged migration after RND/circular PORTW admission; assertions retained |
| `atomic_port_marked_counter` | 8 | 40 | Validated staged migration; assertions retained |
| `atomic_absolute_arithmetic` | 16 | 49 | Validated staged migration; assertions retained |
| `atomic_long_offset_literals` | 14 | 60 | Atomic retained; first staged contrast: C54x case andm-immediate-before-long-offset/5: data[f05]=ff00 expected=f00 |
| `atomic_memory_route_literals` | 26 | 85 | Atomic retained; first staged contrast: C54x case reada-long-preupdated-destination/16: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `atomic_short_immediate` | 520 | 30 | Migrate independent assertions; check staged admission |
| `atomic_port_delayed_branch` | 6 | 30 | Atomic retained; first staged contrast: C54x register sequence case=1 tick=17 replay=0: relative data-bus clock interval |
| `atomic_port_register_examples` | 6 | 37 | Atomic retained; first staged contrast: C54x case atomic-port-marked-register-store-repeat-and-LDU/4: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `atomic_port_low_store` | 6 | 37 | Validated staged migration; assertions retained |
| `atomic_port_indirect_output` | 5 | 30 | Validated staged migration; assertions retained |
| `atomic_port_control_examples` | 6 | 37 | Atomic retained; first staged contrast: C54x case atomic-absolute-marked-return-branch-and-repeat/2: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `atomic_offset_transports` | 6 | 42 | Atomic retained; first staged contrast: C54x case atomic-offset-transport-or-stationary-AR0-load/0: register 22 actual=1 expected=0 mask=ffffffffffffffff |
| `atomic_shift_peripheral_literals` | 10 | 52 | Validated staged migration; assertions retained |
| `atomic_xc_word_boundary` | 3 | 37 | Atomic retained; first staged contrast: C54x case=2 event=2 write=0 address=100 value=1111 clock=14 |
| `bio_atomic` | 29 | 50 | Atomic retained; first staged contrast: C54x case atomic-BIO-NBIO-control-forms-at-both-levels/0: register -1 actual=10900 expected=10901 mask=ffffffffffffffff |
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
