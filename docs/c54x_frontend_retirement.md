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

The 520-section catalogue has 507 generic CPU sections and thirteen copied
firmware sections. Ninety-one former atomic-named sections now use operation
names; no cases or independent assertions were removed by that rename.
See [the test suite](c54x_test_suite.md) for execution and coverage policy and
[instruction contracts](c54x_execution_contracts.md) for bounded qualifications.

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

The stage-4 single-engine checkpoint passes all 520 sections: 1,461,937 cases
and restores. It also passes ordinary 5110 menu/input/power/save-state tests,
358 linked ROM4 context cases, CRC/packing, 32-frame FR and 163-frame bit-exact
EFR codecs, serial/pin audio and replay, four uploaded-program verifier families,
MU4 provisioning/retained-storage/bootstrap/replay/reset, and backend BSP/slot
requests. The CPU was 978 net lines smaller than the pre-removal checkpoint.

All five retirement stages are complete. The normalized catalogue independently
passes all 520 sections and 1,461,937 cases and restores (3,860.357 seconds).
Its manifest SHA-256 is
`20173d1de10d112c5a1f78e9802a7901fe4e5a191a140128315042af3f553e81`.
Renewed acceptance also passes canonical ROM4 cold/context, FR/EFR and RF
algorithms; 5110 menu, late input, power and save-state; clocked serial audio,
backend BSP/requests; and MU4 original bootstrap with exact 1,339-item replay.
The host checks pass 354 C54x, 81 runner and 34 patch/keypad tests. These results
validate the normalized interfaces without claiming broader physical coverage.

## Limits

This is not an all-ISA, silicon timing, fitted DSP-mask, RF-acquisition or
end-to-end speech certificate. Generic tests, copied firmware, linked private
ROM algorithms and handset acceptance remain separate scopes. Physical bank/
READY behavior, unsupported instruction compositions, retained carry-policy
hypotheses and MU4 board/mask-ROM reset remain explicitly unvalidated in their
own contracts. No snapshot or generated peer response may bypass them.
