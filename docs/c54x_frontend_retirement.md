# C54x execution model

## Contract

There is one stage-owned execution engine. Atomic scheduling, pending-I/O
continuations, XC history, delayed/repeat scheduling and their saved state are
removed. `execute_register_effects` retains register-only instruction effects, not a second
scheduler or instruction-level clock accounting. Unsupported instruction forms
stop without an alternate engine.

`reset_execution_pipeline()` discards in-flight pipeline and interrupt-entry
work after fixture state changes. It does not reset architectural registers or
peripherals. The former boolean selection API and test-case mode field are
removed; fixture setup and clear-before-restore still perform the same reset.

The 487-section catalogue has 474 generic CPU sections and thirteen copied
firmware sections. Family tables retain independent numeric expectations
alongside generated timing, ownership and restoration contracts.
See [the test suite](c54x_test_suite.md) for execution and coverage policy and
[instruction contracts](c54x_execution_contracts.md) for bounded qualifications.

## Source ownership

- `tms320c54x.cpp/.h`: CPU state, memory/register interfaces, clock/interrupt
  behavior and pipeline scheduling. `pipeline_execute`,
  `pipeline_publish_read_results` and `pipeline_prefetch` run at their original
  points within `pipeline_step`; they are not independently reordered stages.
- `tms320c54x_decode.h`: opcode predicates and shared cached classification/
  admission metadata, with no guest state or firmware addresses.
- `tms320c54x_dasm.cpp/.h`: disassembly and instruction formatting, registered
  through MAME's disassembler build list.
- Separate BSP, DMA and McBSP components retain their peripheral contracts.

`execute_register_effects` owns no guest bus transactions or instruction timing.
An exhaustive opcode-only census gives the same 2,087-word caller superset in
base and extended modes: 12 mode-snapshotted words, 2,075 ordinary words and
128 accelerated-repeat words (the repeat set overlaps the ordinary set).
The reviewed ranges live in `fixtures/c54x_dispatch_routes.json`.
Admission/arbitration remains the largest scheduling block and a bounded
partial model; this census does not expand instruction or silicon coverage.

The manual instrument is:

```sh
python3 tools/c54x_dispatch_audit.py --check fixtures/c54x_dispatch_routes.json \
  --output tmp/c54x-dispatch.json
```

It requires libclang for raw C++ tokenization and `g++` with the local MAME
headers to evaluate guards. Aliases and bus-admission expressions come from
the actual sources. Unknown local/state `if` conditions fork both ways; unknown
live switch selectors and unsupported control syntax stop the audit. The proof
covers ordinary admitted execution, not arbitrary debugger state edits or
corrupted snapshots. The nine retained
local/state-dependent arithmetic conditions are conservative, not removal
proofs. No admitted caller word reaches the final unimplemented-opcode path.

Opcode diagnostics allocate and collect only with MAME `-verbose`. Counts and
full extended PCs are observations, not saved architectural state. Fetched
tuples have a 4,096-entry diagnostic budget; overflow emits `opwords_limit`
and coverage tools reject the truncated capture instead of claiming closure.

## Cycle ownership

`pipeline_step` is an ordered cycle, not six independently reorderable stage
functions. The regions below execute in order; a return skips every later region.
The cycle counter increments before any hold. Peripheral timers remain scheduled
in emulated time, so a pipeline hold is not a stopped device clock.

| Region | Ownership and return boundary | Executable coverage |
| --- | --- | --- |
| Deferred peripheral read, then external I/O wait/READY | Complete the retained read once, or pay/hold the existing I/O owner before its side effect; no slot advancement. Reject overlapping I/O owners. | `register_copy_sequences`, `standard_io_extended`, `c5416_io_extended`, `waited_io_control` |
| Accelerated repeat | Execute only the retained body/transfer and finish one iteration; ordinary slots stay held. | `repeated_immediate_store`, `repeated_subc`, `repeated_mvdp`, `repeated_accumulator_load`, `waited_io_control` |
| Pending transfer/latency | After repeat handling, decrement compiler-SP latency and service the first pending DP, long-store, return/call, repeat, program-move or coefficient owner. Return even on its completion clock. Long-store completion alone calls interrupt acceptance here. | `memory_dp`, `store_double_load`, `far_return`, `accumulator_call`, `conditional_control`, `write_a`, `reada_accumulator`, `coefficient_mac_literals` |
| Peripheral arbitration | Before advancing slots, reject unqualified conflicts or charge one access wait. The serial retry pair and CLKMD write/read are the two qualified overlaps. Carry the CLKMD read-deferral decision across advancement. | `peripheral_waits`, `peripheral_cost_classes`, `register_copy_sequences`, native clocked serial/replay |
| Slot advancement and front-end/control capture | Move all slots, including invalid ones, in descending order; issue retained repeat literals; advance interrupt entry and capture fetch/control predicates. Later rejection does not roll back completed older work. | `interrupt_entry`, `far_return`, `bio_pipeline`, `xc_condition`, `block_branch` |
| Decode admission and Access/Read operands | Reject unsupported encodings/compositions at their existing boundary; retain extensions, modes, addresses, early DAGEN updates and bus operands. | `conditional_store_rejections`, `mapped_dual_rejections`, `dagen`, `memory_pairs` |
| Execute and late publications | Capture Read-owned modes before older Execute; then apply qualified forwarding, late stack reads, control/stack allocations and Read-result publication. | `ram_forwarding`, `stack_interrupt_order`, `status_write_interrupt`, `register_copy_sequences` |
| Final control, prefetch and interrupt acceptance | Publish MMR moves and block-loop bookkeeping, prefetch once, then check interrupt acceptance. | `block_repeat_control`, `software_interrupt`, `interrupt_mask`, `waited_io_control` |

These are model contracts, not a proof of physical bus priority or all instruction
compositions. Exact access timestamps/counts and replay cover the named paths;
unsupported peripheral/I/O overlaps remain deliberate rejection boundaries.
The direct rejection fixture covers an ordinary adjacent peripheral read/write;
the CLKMD-move/peripheral and overlapping external-I/O rejection guards do not
yet have qualified reaching instruction sequences. Keep those guards; their
presence is not executable coverage or proof that they are unreachable.
Operand capture and forwarding remain interleaved in the coordinator because
moving them into textbook stage order would change what adjacent instructions see.

## Compositions and interfaces

Ordinary `noki5110` selects the native DSP backend and standard SWWSR decoding.
The uploaded-program backend/verifier also uses standard decoding. MU4 uses
the C5416-compatible SWWSR/BSCR/SWCR grammar; isolated routine wrappers explicitly
initialize their existing zero-wait ABI because they bypass original startup.
AAC's independent ABI fixture uses the same execution engine.

Bus dialects and peripheral/clock attachments are real composition differences,
not execution modes. In particular, serial-word, physical-pin and externally
clocked benches remain distinct. Do not remove those dimensions with the retired
atomic comparison machines (`nse1astd` and `nse1aclk`). RF parsers may recognize
their archived metadata, but live runners cannot select them.

ROM4 has 23 canonical BIOS profiles. The seventeen duplicate `-pipe`/cold aliases
and alias-specific ROM/vector loads are removed. Retained profile declarations
are unchanged apart from BIOS indices. Legacy codec Make target names ending in
`-staged` delegate to canonical targets; they do not run duplicate recipes.
The codec runner has no `--staged` selection and the benchmark defaults to one
encoder and one decoder. Old benchmark BIOS names are not live interfaces.

Read-only log checkers distinguish marked versus legacy-unmarked captures using
`require_frontend_marker`; this selects an input format, never emulation.
PC-context parsers similarly expose `legacy_capture` or `--capture-format`
for archived instruction-boundary observations; current runners use stage-owned
capture contexts.
Current codec runs require the stage-owned execution marker and retain complete
frame, reference, replay, exit-status and validity checks. An unmarked old log
alone is not proof of an atomic engine or renewed acceptance.

## Load-bearing details

- Post-load re-drives peripheral clock-stop as well as PLL-stop, XF and bus
  outputs. The deep-idle fixture poisons an unsaved external observer before
  restore, so a peripheral's independently saved latch cannot mask omission.

- MVDK owns its source in Access. It must use the same pending on-chip RAM
  forwarding as Read-stage consumers (SPRU131G section 7.3.2). The
  `rmw_move_bypass` section retains 160 alias/nonalias, addressing, operation,
  gap and replay contrasts. Reverting that forwarding fails the first case.
  This does not establish physical bank contention or external RAM timing.
- An IDLE Decode guard belongs to IDLE, not an older backward branch at a
  nearby numeric address. Guard subtraction is bounded. `idle_wake` checks
  two receive interrupts and four restore boundaries through that loop.
- Delayed C/NC branches are qualified inside a block only with their target
  and complete delay span inside it; `block_branch` retains the page/polarity
  contrasts. This is not blanket qualification of all control compositions.
- The backend simultaneous RX/TX test uses a branch sink for the RX vector:
  pending TX can wake IDLE with INTM set. RX-first selection, exactly one frame,
  its return word and pending TX survive replay. A terminal-IDLE assumption
  must not replace those assertions.
- Runtime RAM logs use the retained operand or Execute owner, not a speculative
  PC. MU4's `374d` gate is read at `02:90e8` and written at `02:90ec`.
- MU4 bootstrap peer notifications use the already validated class-0 selector-02
  grammar: sixteen words, checksum/next-token checks and three-word ACKs. This
  is serial input, not firmware-state forcing. Bootstrap entry counts are
  Execute observations, not program-bus fetch counts.

## Acceptance

All five retirement stages are complete. The consolidated catalogue independently
passes all 487 sections and 1,461,937 cases and restores (2,905.815 seconds).
Its manifest SHA-256 is
`031bff8c6519d5e07acf10d428c31483c3ea6b234496a98bc527d156b2e07dd6`.
Fixture consolidation removes no cases, expected results or restore execution.
Fresh integration checks pass 358 linked ROM4 context cases, cold execution,
FR/EFR codecs, RF bitmap, clocked serial/replay, backend BSP/requests,
MU4 original bootstrap with exact 1,339-item replay, and 5110 menu/save-state.
The host checks pass 366 C54x, 81 runner and 59 device-split/patch/keypad tests.
The residual-effects census preserves all three caller sets while removing
105 proven-shadowed conditions/arms and nine unused private helpers. CPU
component sources shrink from 8,177 to 6,963 lines; the register-effects
dispatcher shrinks from 1,357 to 226 lines. No saved fields, catalogue cases,
pipeline ordering or admission/arbitration rules change.
Every catalogue completion also passes
exact case/restore line validation, including rejection of count suffixes.

The uploaded-program verifier and broader handset qualification remain earlier
results, not renewed certificates from this structural pass. These
results do not expand physical coverage; suite sizing and build accounting
are recorded in [the suite guide](c54x_test_suite.md#acceptance).

## Limits

This is not an all-ISA, silicon timing, fitted DSP-mask, RF-acquisition or
end-to-end speech certificate. Generic tests, copied firmware, linked private
ROM algorithms and handset acceptance remain separate scopes. Physical bank/
READY behavior, unsupported instruction compositions, retained carry-policy
hypotheses and MU4 board/mask-ROM reset remain explicitly unvalidated in their
own contracts. No snapshot or generated peer response may bypass them.
