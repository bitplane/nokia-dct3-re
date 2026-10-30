# Section replacement trial

Legacy C54x tests are reference-only during migration. They may temporarily
have unresolved continuations. Do not build or run them to qualify a section.

Run `python3 tools/run_c54x_section.py SECTION`. This replaces the **isolated
c54xisa overlay** with a one-source runner and the selected section only;
it does not change CPU implementation sources. `make build-c54x-isa` restores
the ordinary overlay when the migration is integrated. Logs and measured
build/run times are in ignored `tmp/c54x-section-trial/SECTION/`.

| Pass | Replacement | Retired source | Validation |
| --- | --- | --- | --- |
| 1 | `immediate_store.h`: eight direct/indirect/absolute/peripheral literal-store checkpoints | Legacy immediate-store start and phases251250/251260 | Native eight cases/restores; build5.083s, run0.016s |

The pilot retains opcode-shaped literals, pointer updates, exact data writes
and before/after-write replay. Clock assertions describe the current pipeline
model, not measured silicon timing. Final layout is undecided; these files
are independently executable migration sections, not a mandatory file count.
