# Atomic frontend retirement inventory

## Decision and scope

Target one staged execution model, but do not delete atomic before migrating its
unique assertions and the ordinary native 5110 composition. This is a static
migration inventory, not a staged parity certificate. No section is approved for
deletion solely because of its name or similarity to another section.

The 518 registered sections total 18,148 source lines and 1,477,911 registered
cases. 137 sections explicitly assign `pipeline_frontend = false`: 5,991 source
lines and 11,468 registered cases. Seven additional sections select the frontend
through an expression: 241 lines and 39,048 cases. Counts describe entire
sections, not atomic-only case counts. Helpers and firmware integration sources
are additional migration work. The common case defaults to staged.

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

### Unresolved migration contrasts

`atomic_product_literals` remains atomic: after accounting for cold Prefetch,
case 14 (`MAC offset extension order`, `65e3 0004 0002`) reads `0204` on staged
where the retained atomic fixture expects `0202`. Do not change the expected
address or delete this case to obtain parity. Resolve the long-offset addressing
contract against independent evidence before migrating this section.

Classifications below are routing priorities based on explicit frontend settings
and contract families, not behavioral equivalence findings. Validate migrated
sections independently; do not run the complete suite after every edit.

| Section | Cases | Lines | Disposition |
| --- | ---: | ---: | --- |
| `signed_conditions` | 389 | 56 | Migrate independent assertions; check staged admission |
| `return_barrier` | 12 | 29 | Trim paired dimension; preserve staged contrasts |
| `xc_status` | 64 | 30 | Trim paired dimension; preserve staged contrasts |
| `xc_latency` | 384 | 46 | Trim paired dimension; preserve staged contrasts |
| `atomic_conditional_return` | 96 | 33 | Re-derive timing/order; retain assertions |
| `atomic_overflow_branch` | 32 | 31 | Migrate independent assertions; check staged admission |
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
| `rounding` | 64 | 31 | Migrate independent assertions; check staged admission |
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
| `software_interrupt` | 128 | 35 | Re-derive timing/order; retain assertions |
| `compact_load` | 28 | 32 | Migrate independent assertions; check staged admission |
| `stack_address_dependency` | 24 | 33 | Migrate independent assertions; check staged admission |
| `transition_register` | 20 | 40 | Migrate independent assertions; check staged admission |
| `compact_store` | 640 | 43 | Migrate independent assertions; check staged admission |
| `reverse_carry` | 2,690 | 46 | Migrate independent assertions; check staged admission |
| `interrupt_entry` | 12 | 53 | Re-derive timing/order; retain assertions |
| `atomic_xorm` | 256 | 33 | Migrate independent assertions; check staged admission |
| `scalar_boundaries` | 34 | 66 | Migrate independent assertions; check staged admission |
| `atomic_repeat_irq` | 2 | 34 | Re-derive timing/order; retain assertions |
| `atomic_subtract` | 384 | 53 | Migrate independent assertions; check staged admission |
| `square_literals` | 6 | 38 | Migrate independent assertions; check staged admission |
| `atomic_near_control` | 10 | 50 | Re-derive timing/order; retain assertions |
| `long_literals` | 33 | 68 | Migrate independent assertions; check staged admission |
| `atomic_mac_literals` | 39 | 47 | Migrate independent assertions; check staged admission |
| `atomic_dual_address` | 11 | 46 | Migrate independent assertions; check staged admission |
| `atomic_parallel_literals` | 10 | 54 | Migrate independent assertions; check staged admission |
| `atomic_shift_literals` | 25 | 54 | Migrate independent assertions; check staged admission |
| `atomic_register_shift` | 36 | 61 | Migrate independent assertions; check staged admission |
| `atomic_register_logic` | 20 | 45 | Migrate independent assertions; check staged admission |
| `atomic_product_literals` | 21 | 63 | Migrate independent assertions; check staged admission |
| `atomic_conditional_control` | 12 | 39 | Re-derive timing/order; retain assertions |
| `atomic_far_edges` | 23 | 66 | Migrate independent assertions; check staged admission |
| `atomic_extended_interactions` | 3 | 55 | Migrate independent assertions; check staged admission |
| `atomic_fast_return` | 1 | 26 | Re-derive timing/order; retain assertions |
| `atomic_pending_far` | 2 | 38 | Migrate independent assertions; check staged admission |
| `atomic_program_mapping` | 13 | 68 | Migrate independent assertions; check staged admission |
| `coefficient_mac_literals` | 6 | 38 | Migrate independent assertions; check staged admission |
| `atomic_memory_literals` | 24 | 91 | Migrate independent assertions; check staged admission |
| `atomic_double_stores` | 8 | 40 | Migrate independent assertions; check staged admission |
| `atomic_address_literals` | 4 | 33 | Migrate independent assertions; check staged admission |
| `atomic_shifted_loads` | 7 | 36 | Migrate independent assertions; check staged admission |
| `atomic_stack_literals` | 9 | 53 | Migrate independent assertions; check staged admission |
| `atomic_mapped_moves` | 4 | 36 | Migrate independent assertions; check staged admission |
| `atomic_memory_tests` | 10 | 44 | Migrate independent assertions; check staged admission |
| `atomic_zero_repeat` | 3 | 26 | Re-derive timing/order; retain assertions |
| `atomic_program_transfers` | 8 | 46 | Migrate independent assertions; check staged admission |
| `atomic_conditional_repeat` | 4 | 32 | Re-derive timing/order; retain assertions |
| `atomic_transfer_literals` | 34 | 85 | Migrate independent assertions; check staged admission |
| `atomic_mmr_burst` | 2 | 43 | Migrate independent assertions; check staged admission |
| `atomic_memory_sequences` | 3 | 40 | Migrate independent assertions; check staged admission |
| `atomic_absolute_literals` | 30 | 78 | Migrate independent assertions; check staged admission |
| `atomic_condition_edges` | 6 | 30 | Migrate independent assertions; check staged admission |
| `atomic_alu_store_literals` | 8 | 37 | Migrate independent assertions; check staged admission |
| `atomic_high_store_literals` | 5 | 28 | Migrate independent assertions; check staged admission |
| `atomic_parallel_load_literals` | 8 | 46 | Migrate independent assertions; check staged admission |
| `atomic_move_modify_literals` | 9 | 60 | Migrate independent assertions; check staged admission |
| `atomic_logic_pointer_literals` | 18 | 57 | Migrate independent assertions; check staged admission |
| `atomic_logic_program_literals` | 5 | 40 | Migrate independent assertions; check staged admission |
| `atomic_accumulator_add_literals` | 7 | 31 | Migrate independent assertions; check staged admission |
| `atomic_multiply_move_literals` | 7 | 59 | Migrate independent assertions; check staged admission |
| `atomic_conditional_slot_literals` | 10 | 55 | Migrate independent assertions; check staged admission |
| `atomic_mapped_load_sequences` | 4 | 53 | Migrate independent assertions; check staged admission |
| `atomic_register_copy_sequences` | 4 | 43 | Migrate independent assertions; check staged admission |
| `atomic_guard_conditions` | 18 | 41 | Migrate independent assertions; check staged admission |
| `atomic_memory_alu_literals` | 9 | 40 | Migrate independent assertions; check staged admission |
| `atomic_mmr_transfer_literals` | 4 | 45 | Migrate independent assertions; check staged admission |
| `atomic_transfer_repeat_literals` | 265 | 55 | Re-derive timing/order; retain assertions |
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
| `atomic_basic_repeat_sequences` | 5 | 54 | Re-derive timing/order; retain assertions |
| `atomic_basic_control_sequences` | 13 | 53 | Re-derive timing/order; retain assertions |
| `atomic_basic_alu_control` | 10 | 49 | Re-derive timing/order; retain assertions |
| `atomic_basic_parallel_block` | 9 | 51 | Migrate independent assertions; check staged admission |
| `atomic_accumulator_calls` | 3 | 31 | Re-derive timing/order; retain assertions |
| `atomic_bus_timing` | 3 | 37 | Re-derive timing/order; retain assertions |
| `atomic_port_timing` | 15 | 65 | Re-derive timing/order; retain assertions |
| `atomic_mac_saturation` | 7 | 44 | Migrate independent assertions; check staged admission |
| `atomic_port_marked_operations` | 22 | 83 | Re-derive timing/order; retain assertions |
| `atomic_compatibility_memory` | 9 | 55 | Migrate independent assertions; check staged admission |
| `atomic_port_marked_memory` | 11 | 48 | Re-derive timing/order; retain assertions |
| `atomic_port_loads` | 6 | 40 | Re-derive timing/order; retain assertions |
| `atomic_port_marked_transfers` | 9 | 42 | Re-derive timing/order; retain assertions |
| `atomic_port_marked_counter` | 8 | 40 | Re-derive timing/order; retain assertions |
| `atomic_absolute_arithmetic` | 16 | 49 | Migrate independent assertions; check staged admission |
| `atomic_long_offset_literals` | 14 | 60 | Migrate independent assertions; check staged admission |
| `atomic_memory_route_literals` | 26 | 85 | Migrate independent assertions; check staged admission |
| `atomic_short_immediate` | 520 | 30 | Migrate independent assertions; check staged admission |
| `atomic_port_delayed_branch` | 6 | 30 | Re-derive timing/order; retain assertions |
| `atomic_port_register_examples` | 6 | 37 | Re-derive timing/order; retain assertions |
| `atomic_port_low_store` | 6 | 37 | Re-derive timing/order; retain assertions |
| `atomic_port_indirect_output` | 5 | 30 | Re-derive timing/order; retain assertions |
| `atomic_port_control_examples` | 6 | 37 | Re-derive timing/order; retain assertions |
| `atomic_offset_transports` | 6 | 42 | Re-derive timing/order; retain assertions |
| `atomic_shift_peripheral_literals` | 10 | 52 | Migrate independent assertions; check staged admission |
| `atomic_xc_word_boundary` | 3 | 37 | Re-derive timing/order; retain assertions |
| `bio_atomic` | 29 | 50 | Migrate independent assertions; check staged admission |
| `peripheral_cost_classes` | 20 | 36 | Migrate independent assertions; check staged admission |
| `atomic_operand_order` | 46 | 63 | Migrate independent assertions; check staged admission |
| `atomic_intm_follower` | 3 | 33 | Migrate independent assertions; check staged admission |
| `atomic_xc_interrupt` | 8 | 40 | Re-derive timing/order; retain assertions |
| `atomic_waited_io_control` | 12 | 36 | Re-derive timing/order; retain assertions |
| `stopped_timer_reload` | 48 | 26 | Re-derive timing/order; retain assertions |
| `source_admission` | 38,464 | 54 | Trim paired dimension; preserve staged contrasts |
| `idle_nmi` | 12 | 35 | Re-derive timing/order; retain assertions |
| `idle_wake` | 6 | 36 | Re-derive timing/order; retain assertions |
| `deep_idle_clocks` | 8 | 34 | Re-derive timing/order; retain assertions |
| `bsp_interrupt` | 9 | 42 | Re-derive timing/order; retain assertions |
| `idle_dma` | 10 | 38 | Re-derive timing/order; retain assertions |
| `idle3_pll` | 1 | 34 | Re-derive timing/order; retain assertions |
| `repeat_endpoints` | 12 | 44 | Re-derive timing/order; retain assertions |
| `block_repeat_pages` | 96 | 37 | Trim paired dimension; preserve staged contrasts |
| `initial_control_sequences` | 3 | 32 | Re-derive timing/order; retain assertions |
| `receive_ring_copy` | 4 | 20 | Trim paired dimension; preserve staged contrasts |
| `disassembly` | 1 | 13 | Migrate independent assertions; check staged admission |
| `instruction_observer` | 24 | 25 | Trim paired dimension; preserve staged contrasts |

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
