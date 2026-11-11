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

The 490-section catalogue has 477 generic CPU sections and thirteen copied
firmware sections. Family tables retain independent numeric expectations
alongside generated timing, ownership and restoration contracts.
See [the test suite](c54x_test_suite.md) for execution and coverage policy and
[instruction contracts](c54x_execution_contracts.md) for bounded qualifications.

## Source ownership

- `tms320c54x.cpp/.h`: CPU state, memory/register interfaces, clock/interrupt
  behavior and pipeline scheduling. The bus-wait, pending-transfer and peripheral
  arbitration helpers run before slot advancement, separated by the retained
  repeat path. Their results are transient cycle decisions, not saved state.
  `pipeline_execute`,
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

## Rejection scope

`tools/c54x_rejection_audit.py --output tmp/c54x-rejections.json` inventories
all 126 explicit `m_illegal = true` sites, retaining source guards and diagnostics.
It uses the raw-token reader above. Diagnostic-wording categories are 3 bus
arbitration, 29 control-composition, 7 encoding, 86 other (mostly operand-domain
or publication restrictions), and 1 unlogged retained-RETD-word invariant.
These are source-site counts, not unsupported instruction counts or reachability
proofs; a first opcode word alone cannot establish operands or control context.

The seven renewed ROM4 control/clock/wire/pin/replay captures contain 626--643
distinct opcode words, with matching decoded lengths and no rejection diagnostic.
A separate original MU4 bootstrap/replay capture has 862 distinct words and ends
at `02:98ea`, IDLE set and ILLEGAL clear. This bounds the observed workloads,
not all firmware paths. An absent error message alone is insufficient because
the retained-word invariant has no diagnostic.

The MU4 capture identifies a debugger gap rather than an execution exclusion:
ten executed CMPR encodings previously lacked disassembly. CMPR now decodes
all 32 CC/ARx combinations under [SPRU172C, page 4-34](https://www.ti.com/lit/ug/spru172c/spru172c.pdf).
The unchanged 862-word observed set has 856 correctly decoded words, versus 846
before this debugger-only correction; native 1,339-item replay still passes.
The isolated disassembly gate checks syntax, one-word reads and absence of
branch flags on two program pages; execution ownership remains with the existing
register-compare tests. Six other observed words (`9411`, `9421`, `9634`, `9690`,
`fd00`, `ff00`) remain debugger gaps, not observed CPU halts. Extension-controlled
`6f` words require their fetched-tuple decode results, not the incomplete first
word's result.

Do not widen an execution rejection using these opcode counts as justification.
No reached instruction exclusion was found in these workloads; a new admission
needs a blocked execution/context or an independently qualified composition.

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
- Peripheral arbitration captures CLKMD read deferral before slot advancement.
  Do not recompute it from advanced slots or resume advancement on the retained
  read's completion clock. `peripheral_waits` checks both accumulator destinations,
  the write/read bus order and spacing, and replay before/between/after accesses.
  Disabling deferral fails on the stale CLKMD read.
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

All five retirement stages are complete. The earlier execution-core checkpoint `9f979059`
passes all 487 sections and 1,461,943 cases and restores (3,055.702 seconds).
Its manifest SHA-256 is
`258cae1abf722739cff3e3e443e33f8592cb98fecf8c5e3b881c601fffd39bd0`.
Fixture consolidation removes no cases, expected results or restore execution.
Six additional CLKMD write/read cases cover both accumulator destinations and
three replay boundaries; disabling the retained-read deferral fails this gate.
Fresh integration checks pass 358 linked ROM4 context cases, cold execution,
FR/EFR codecs, RF bitmap, clocked serial/replay, backend BSP/requests,
MU4 original bootstrap with exact 1,339-item replay, and 5110 menu/save-state.
The debugger-only CMPR addition separately passes the full 45-family disassembler
gate, 140 register-compare and 3,456 delayed-compare cases with replay; that
debugger-only change did not modify execution or saved state.
The host checks pass 371 C54x tests and 66 layout, native-runner,
device-split/patch/keypad tests. Focused builds refresh changed CPU sources
automatically rather than relying on a separate overlay invocation.
The residual-effects census preserves all three caller sets. Its register-only
dispatcher remains 226 lines. Separating retained waits and peripheral arbitration
reduces the cycle coordinator from 2,267 to 2,106 lines; explicit helper results
add 30 lines across the CPU component overall. No saved fields, pipeline ordering
or admission/arbitration rules change. The remaining interleaved operand and
forwarding regions stay in the coordinator; the coverage limits above remain open.
Every catalogue completion also passes
exact case/restore line validation, including rejection of count suffixes.

The current native-ROM6 execution checkpoint additionally qualifies absolute
BRC comparison, retained RAM-to-SP copy and older READA destination-address
publication before younger Access-stage address updates. The complete current
catalogue passes **490 sections and 1,461,965 cases and restores**: 477 CPU
sections/1,460,179 cases in 2,853.515 seconds and 13 copied-firmware
sections/1,786 cases in 76.712 seconds (2,930.227 seconds total). Manifest SHA-256:
`6d330221ee7d57ecb9f55614bc6f334836c45e923b7099ecae430ad48f906e32`.
Fresh native 5110 menu and ROM4 EFR decoder/encoder gates also pass, each codec
covering 163 frames with zero reference mismatches and exact replay. Default
and coherent 3210 HLE acceptance remain green. These renewed results do not
extend the ISA, physical timing or native-radio claims beyond their fixtures.

The 2026-11-10 ledger revalidation at `93a6cb11` reran the complete catalogue:
**528 sections and 1,464,127 cases and restores** in 2,926.7 seconds
(`python3 tools/run_c54x_section.py all --scope all --jobs 8`). The same pass
reran the ROM4 context, FR/EFR, RF bitmap/pairs, backend BSP/request, clocked
boot, RF filter and MU4 storage/NAND gates. The strict EFR erasure, homing-error
and lost-SID comparisons still reproduce their documented divergences
unchanged. The RF filter observer had kept atomic `PC` ownership for
`noki5110` after its promotion to the stage-owned engine and captured no
input. It now uses `READPC`/`EXECPC` on every live machine.

The ROM4 uploaded-program verifier and handset qualifications not listed above
remain earlier results, not renewed certificates from this pass. Native 8210
acceptance has its own [ROM6 contract](rom6_dsp_bringup.md). These results do
not expand physical coverage; suite sizing and build accounting
are recorded in [the suite guide](c54x_test_suite.md#acceptance).

## Limits

This is not an all-ISA, silicon timing, fitted DSP-mask, RF-acquisition or
end-to-end speech certificate. Generic tests, copied firmware, linked private
ROM algorithms and handset acceptance remain separate scopes. Physical bank/
READY behavior, unsupported instruction compositions, retained carry-policy
hypotheses and MU4 board/mask-ROM reset remain explicitly unvalidated in their
own contracts. No snapshot or generated peer response may bypass them.
