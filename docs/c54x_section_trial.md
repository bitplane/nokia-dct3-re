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
| 2 | `basic_memory.h`: twelve stores and six indirect loads, captured address, IRQ drain and store observation API | Two starts and phases251070..251100 | Native18 cases/restores; build5.246s, run0.021s |
| 3 | `stack.h`:66 PSHM and57 ordinary POPM checkpoints; literal source expectations | PSHM start/phase handlers; ordinary branch removed from shared POPM reference | Native123 cases/restores; build5.235s, run0.061s |

The pilot retains opcode-shaped literals, pointer updates, exact data writes
and before/after-write replay. Clock assertions describe the current pipeline
model, not measured silicon timing. Final layout is undecided; these files
are independently executable migration sections, not a mandatory file count.

Stack coverage keeps three replay boundaries and both XPC pages. It retires
redundant ordinary-page replication and PSHM reserved/zero-source cases;
special IMR/IFR POPM remains in the unmigrated reference. Ordinary POPM also
checks XF output against its architectural ST1 bit on every observation.
