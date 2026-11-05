# C54x execution model

## Contract

There is one stage-owned execution engine. Atomic scheduling, pending-I/O
continuations, XC history, delayed/repeat scheduling and their saved state are
removed. `execute_one` retains shared instruction effects, not a second
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

`execute_one` is a residual effects dispatcher, not an alternate engine.
Some retained bus-capable branches still require per-family reachability
proofs; removing the proven redundant XOR/LD-immediate/SQUR tails does not
establish that every remaining branch is necessary. Admission/arbitration is
still the largest scheduling block and remains a bounded partial model.

Opcode diagnostics allocate and collect only with MAME `-verbose`. Counts and
full extended PCs are observations, not saved architectural state. Fetched
tuples have a 4,096-entry diagnostic budget; overflow emits `opwords_limit`
and coverage tools reject the truncated capture instead of claiming closure.

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
passes all 487 sections and 1,461,937 cases and restores (2,929.482 seconds).
Its manifest SHA-256 is
`031bff8c6519d5e07acf10d428c31483c3ea6b234496a98bc527d156b2e07dd6`.
Fixture consolidation removes no cases, expected results or restore execution.
Fresh integration checks pass 358 linked ROM4 context cases, cold execution,
FR/EFR codecs, RF bitmap, clocked serial/replay, backend BSP/requests,
MU4 original bootstrap with exact 1,339-item replay, and 5110 menu/save-state.
The host checks pass 359 C54x, 81 runner and 59 device-split/patch/keypad tests.
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
