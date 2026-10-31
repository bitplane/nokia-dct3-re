# C++ test contracts

Tests are separate from shipped device implementations.

| Directory | Contract | Limits |
| --- | --- | --- |
| `unit/` | Standalone algorithms, codecs and protocol data | No scheduler or handset acceptance. |
| `c54x/` | Synthetic processor execution, interrupts, bus ordering, peripheral composition and restoration | Expectations must not call CPU implementation helpers. Timing projections are not measured silicon behavior. |
| `devices/` | MAME device benches using registers, callbacks and saved state | Self-conformance is not independently measured hardware behavior. |
| `firmware/` | Copied instruction sequences and acquired-ROM execution | Input identity and bounded integration contracts, not generic CPU or full-handset acceptance. |

## CPU sections

`fixtures/c54x_sections.json` owns section names and expected case counts.
Each entry resolves to exactly one source in `c54x/sections/` or
`firmware/c54x_*.h`. Missing, ambiguous and unregistered sources fail closed.

- `python3 tools/run_c54x_section.py --list`: list generic CPU sections.
- `python3 tools/run_c54x_section.py NAME`: build and execute only that section.
- `make build-c54x-isa C54X_SECTION=NAME`: build that section without executing.
- `make check-c54x-core`: complete retained generic CPU scope.
- `python3 tools/run_c54x_section.py all --scope firmware`: explicitly opt into copied-firmware sections.

The selected bench produces `mame/c54xisa`, independently of the handset
binary. Builds share the overlay/object tree: never run builds concurrently.
Selected execution remains the development workflow; complete regression is
a final integration checkpoint, not a prerequisite for each edit.

The catalogue checks registration and counts, not instruction correctness.
Executable sections must establish their own stimulus, bus/state expectations,
replay and meaningful rejection controls. See
[`c54x_section_trial.md`](../docs/c54x_section_trial.md) for the canonical
disposition table, deliberate coverage reductions and unresolved integration work.

## Firmware and handset acceptance

`tools/run_c54x_core.py` now runs only the retained `rom4-context` and
`cold-pipeline` firmware integrations. The historical generic BIOS profiles,
default phase chain, review manifests and frozen transcript inventories are
retired. Their old command lines are not current acceptance interfaces.

`make check-c54x-rom4-context` selects the copied context integration.
Acquired-ROM codec fixtures retain separate runners and input hashes.
ROM4 `cold`/`cold-pipeline` omit the MCU loader deliberately: a cold-frontier
result is not a statement that normal handset boot stops there.
Verifier-sensitivity machines are research instruments, not handset verdicts.

`make build-conformance` includes device/firmware benches;
`make build-handsets` excludes synthetic test drivers and firmware-PC probes.
Handset acceptance remains product/workflow-specific in Makefile and gates.json.
Host tooling tests and mocked runner tests never substitute for native execution.

## Retention rules

Retain distinct operand contrasts, arithmetic boundaries, bus ordering,
side-effect counts, output-pin behavior and restore points. An old bug
regression is not obsolete merely because the implementation passes it.

Replace redundant Cartesian products with justified boundaries, recording
lost coverage explicitly. Delete unjustifiable fixtures after bounded review;
existence and old passing counts are not retention arguments.
A final-frame oracle cannot replace instruction-level checks.

Keep setup, expectations and replay explicit. Helpers may load memory,
compare state and serialize snapshots; they must not calculate expected
results through the CPU decoder, arithmetic, addressing or timing code.
No implicit cross-family phase chain belongs in the replacement suite.

Final integration validation is still outstanding during the replacement
campaign. A selected section's passing result must not be reported as proof
that the full CPU suite, moved firmware adapter or handset gates pass.
