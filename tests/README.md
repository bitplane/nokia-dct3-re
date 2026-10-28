# C++ test contracts

Tests are separate from shipped device implementations. Directory ownership
describes what a fixture can establish, not how recently it was written.

| Directory | Contract | Limits |
| --- | --- | --- |
| `unit/` | Standalone algorithms, codecs and protocol data | No scheduler, device timing or handset acceptance. |
| `c54x/` | Synthetic processor execution, addressing, interrupts, peripheral composition and restoration | Expected values must remain independent of CPU implementation helpers. |
| `devices/` | MAME device benches using real registers, callbacks and saved state | Self-conformance is not independently measured silicon behavior. |
| `firmware/` | Acquired original-ROM and music-DSP execution fixtures | Each owns its input hashes and bounded acceptance; no implicit full-handset promotion. |

The C54x machine-registration module also retains the small verifier-sensitivity
machines. Those are research instruments, not processor-conformance or fitted
handset verdicts. Their declared `boundary`, `cobba`, `cobba_alt` and `rom4`
profiles do not become hardware facts because the runner exits successfully.

Likewise, ROM4 `cold`/`cold-pipeline` are mask-only controls with the MCU loader
deliberately absent. Their retained `cold frontier` result names are runner
compatibility, not a claim that ordinary native handset boot still stops there.

## Build and run

- `make build` retains the complete research/conformance composition.
- `make build-conformance` explicitly selects those test drivers.
- `make build-handsets` excludes synthetic test drivers and firmware-PC probes.
  Research handset compositions remain available; this is not an upstream export.
- Builds share the MAME overlay/object tree: never run the two modes or their
  acceptance gates concurrently. Rebuild the required mode before its gates.

Source files are overlaid by basename into MAME's Nokia directory for the
existing local build. That compatibility mapping does not make a CPU test a
Nokia hardware device. BIOS declarations own profile selection;
`tools/c54x_test_oracles.py` separately owns expected results.

Use `make test-tools` for host-side algorithms and tooling, and
`make check-c54x-core` for the full default processor regression (including
its build). For an already-built conformance executable, run
`python3 tools/run_c54x_core.py --mame mame/mame --rompath mame/roms`;
`--profile` selects an isolated declared fixture rather than the default
sequence. Firmware and device gates retain their individual input and
acceptance requirements in `Makefile` and `gates.json`.

## Retention and consolidation

Keep a test when it protects a distinct contract: legal/illegal operand
contrast, ordering, side-effect count, arithmetic boundary or restore point.
An old bug's regression is not obsolete merely because the implementation
now passes it.

A fixture can be retired only when its replacement covers the same stimulus,
expected behavior, negative controls and restoration obligations. A broad
boot or final-frame oracle is not such a replacement for an instruction test.
Temporary sensitivity and frontier experiments may instead be removed after
their reusable conclusion is retained in reviewed evidence.

During behavior-preserving refactors, run `tools/c54x_harness_inventory.py`
without rewriting its baseline. Its lexical assertion count and result-text
inventory are structural guards, not executed coverage. A deliberate change
to coverage requires a separately reviewed baseline change and executable
acceptance, not an automatic `--write` to make the guard green.

Fixture helpers may centralize stimulus, CPU reset, memory loading and result
reporting. They must not calculate expected results through the CPU's own
decoder, addressing, arithmetic or timing implementation. Family-specific
state should stay with its fixture rather than accumulate in a global bench.

The default atomic regression remains an ordered sequence, partitioned into
basic, corpus, TI-reference and extended-control owners. Its router is checked
against every top-level guard, including uncovered gaps and the extended phase
identifiers. Splitting its source does not remove inherited state between
steps; further isolation requires retaining those cross-instruction contrasts.
Batch setup is limited to independent values: later values must not depend on
an earlier register or memory write. Live-state-dependent setup stays explicit.
