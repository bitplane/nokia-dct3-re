# C54x test suite

## Authority

`fixtures/c54x_sections.json` owns section names and expected case counts.
`tests/c54x/sections/` owns generic processor cases; `tests/firmware/c54x_*.h`
owns copied-firmware cases. Each registration must resolve to exactly one
source. Missing, ambiguous, duplicate or unregistered sections fail discovery.
There is one execution engine; cases do not select a frontend.

[Execution contracts](c54x_execution_contracts.md) records instruction-level
qualification and unresolved silicon behavior. [The execution model](c54x_frontend_retirement.md)
records integration acceptance. Neither a host-parser test nor a source-string
invariant establishes processor behavior.

## Running

```sh
python3 tools/run_c54x_section.py --list --scope all
python3 tools/run_c54x_section.py SECTION
python3 tools/run_c54x_section.py SECTION --negative-control
python3 tools/run_c54x_section.py all --scope all --jobs 8
```

`all` defaults to generic CPU scope; copied firmware requires `--scope firmware`
or `--scope all`. `make build-c54x-isa C54X_SECTION=NAME` builds one section
without executing it. It defaults to `instruction_examples`.

The runner refreshes changed C54x CPU sources and replaces the isolated
`c54xisa` fixture before building; unchanged files retain their cached objects.
Sections share one object tree:
build sequentially, including relative to the main MAME and benchmark builds.
Do not retain an emulator binary per section.

Complete runs batch up to eight small, self-contained base-machine sections per
build. Each section still runs in a fresh emulator process and must pass its own
case count, restore count and completion marker. Device-specific, observer-hook,
copied-firmware and large sections retain isolated builds. `--batch-size 1`
disables batching; `--batch NAME ...` runs an explicit compatible group.
`C54X_TEST_SECTION` is generated-fixture dispatch only, not a CPU execution mode.
Batch build times are recorded once under the batch directory; each section's
timing links to that build group. Suite per-section wall times amortize shared
build overhead and must not be interpreted as instruction throughput.

Each section's logs and build/run times are under ignored
`tmp/c54x-section-trial/SECTION/`. Complete runs write
`tmp/c54x-section-trial/SCOPE-summary.json`, including completed sections,
case totals and an explicit completion flag. A failure/interruption leaves the
run incomplete; cached passes are not credited as new execution.

Use focused sections between edits. Run the complete catalogue once at an
integration checkpoint, not after every change. A complete build/run takes
about an hour; cached focused builds usually take seconds.

## Acceptance

The complete catalogue passes with no coverage reductions. Consolidation keeps
independent literal expectations alongside generated timing/ownership tests;
opcode overlap alone did not justify deleting either contract.

| Measure | Before consolidation | Consolidated Checkpoint (`f4cf9496`) |
| --- | ---: | ---: |
| Registered sections | 520 | 487 |
| Cases, each with restore execution | 1,461,937 | 1,461,937 |
| Builds per complete run | 520 | 381 |
| C++ fixture lines | 24,333 | 24,195 |
| C++ fixture bytes | 1,507,089 | 1,502,600 |
| Recorded complete-run wall time | 3,860.357 s | 2,982.044 s |

C++ checkpoint sizing includes `tests/c54x/**/*.h` and the section runner, not Python tools
or copied-firmware headers. The added batch planning/validation code is outside
that sizing. Consolidation-checkpoint build time totals 2,395.196 s and emulator runs
570.491 s; remaining wall time is orchestration. The predecessor wall run
included a pause, so this is an observed comparison, not a controlled benchmark.
The four-section pilot built in 6.696 s and ran in separate processes.
Full-run reports and per-section/batch timing records remain ignored artifacts;
the current manifest and integration qualification live in
[the execution model](c54x_frontend_retirement.md#acceptance).

## Fixture contracts

Small independent literal tables are grouped into `register_literals`,
`product_literals`, `parallel_literals`, `memory_literals`, `address_literals`,
`logic_literals` and `control_literals`. Each table keeps its own inputs,
expected values, checkpoint selection and bus events; `literal_sequence` only
constructs the shared before/running/final snapshot cadence. Family grouping
does not replace these literals with expectations computed by the CPU model.
Port-marker tables live in `port_marked_operations`, `port_marked_memory` and
`port_timing`. Their shared helpers construct the program and snapshot cadence;
ordered I/O values, absolute clocks and inter-marker intervals stay in the rows.

- Expectations belong to the case, not opcode-specific branches in the runner.
  Keep independent arithmetic, flags, addresses, guards and rejection limits.
- Observe ordered data/program/I/O events, widths and source identity where
  relevant. A speculative fetch is not Execute or an operand owner. Exact bus
  callbacks establish once-only side effects more reliably than polling PC.
- Poison opcodes, extension words, sources and pointers after their capture
  when the test claims retained ownership. Check both intermediate publication
  and final state where their distinction matters.
- Check IRQ/NMI source selection, whole delayed/repeated-operation drain,
  return frames and pending sources. Waking from IDLE is not necessarily
  accepting an interrupt; INTM-blocked wake can resume foreground code.
- Every registered case includes restore execution. Cover meaningful pipeline,
  pending peripheral and control boundaries; selected cases clear the live
  pipeline before restoring to prove the saved state is sufficient.
- `reset_execution_pipeline()` discards in-flight execution/interrupt-entry
  work without resetting architectural registers or peripherals. Preserve
  this operation when restructuring fixture setup or restore paths.
- Rejection assertions describe supported implementation boundaries, not
  necessarily silicon traps. Cycle assertions describe the software model,
  not measured physical pipeline or external-bank timing.

## Coverage budget

The suite deliberately distributes pages, checkpoints and other orthogonal
dimensions instead of repeating every Cartesian combination. This does not
claim all correlations are covered. Preserve non-orthogonal contrasts: mapped
versus RAM operands, aliases, addressing forms, flag/carry boundaries, repeat
counts, delay-slot ownership, source poisoning and interrupt/replay order.

Deleting a duplicate requires a named retained contract covering its inputs,
observable effects and restore behavior. A similar filename or a green final
register snapshot is insufficient. Do not reintroduce duplicated execution-mode
dimensions or dense per-tick polling where ordered bus events establish the
contract. Deliberate redistribution must leave a concise local explanation;
closed migration chronology belongs in Git, not a parallel progress table.

## Opcode assertion inventory

The retired default fixture printed an explicit `[opassert]` marker per
asserted opcode. Its replacement is the set of opcodes the core admits while
a passing section executes: every case loads its own program and asserts the
complete final state, so those opcodes are executed under assertion. This is
weaker than a per-instruction marker; it does not show that each
instruction's own effect is distinguishable from the rest of its case.
Rejection, bus and clock sections may pass without admitting any opcode.

```sh
python3 tools/run_c54x_section.py all --scope all --jobs 8 --opcode-coverage
python3 tools/c54x_section_opcode_inventory.py tmp/c54x-sections.log --scope cpu
python3 tools/c54x_opcode_coverage.py FIRMWARE/error.log \
    --fixture-log tmp/c54x-sections.log [--additional-fixture-log BSP/error.log] \
    --require-all-asserted
```

The inventory accepts only a complete suite summary, each section's exact
PASS line and its own coverage log. Firmware traces come from
`run_c54x_rom4_pcm_clock.py --coverage` (the `pin_boot` case) and
`run_nse2_word_contract.py` (`service/error.log`). The backend BSP fixture
(`run_c54x_backend_bsp.py --coverage`) still prints its explicit
`[opassert] op=7721`, the clocked boot's `STM #0aaa, BDXR` retry store.

At the 2026-11-11 run (528 sections, 31,588 CPU-scope opcodes) the NSE-2
service inventory (564 forms) is fully covered by CPU sections, and the
clocked NSE-1 boot (643 forms) by CPU sections plus that BSP assertion.

## Firmware boundary

Private ROMs and application ABI assumptions remain outside generic ISA tests.
`tools/run_c54x_core.py` selects the linked `rom4-context` or acquired-ROM `cold`
fixture; codec and handset runners have separate gates. The linked ROM4 context
digest, bit-exact codec results, serial/pin replay and handset menu tests are
integration evidence, not substitutes for CPU sections or physical validation.
