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
| 3 | `stack.h`:84 PSHM and57 ordinary POPM checkpoints; literal source expectations | PSHM start/phase handlers; ordinary branch removed from shared POPM reference | Final native141 cases/restores; build5.262s, run0.069s |
| 4 | `dagen.h`:240 constant-DAGEN,12 STM-arbitration and40 repeated-write rejection cases | Constant-DAGEN fixture/starts/results and STM start/results | Native292 cases/restores; build5.398s, run0.126s |

The pilot retains opcode-shaped literals, pointer updates, exact data writes
and before/after-write replay. Clock assertions describe the current pipeline
model, not measured silicon timing. Final layout is undecided; these files
are independently executable migration sections, not a mandatory file count.

Stack coverage keeps three replay boundaries and both XPC pages. It retires
redundant ordinary-page replication and PSHM reserved-gap cases;
special IMR/IFR POPM remains in the unmigrated reference. Ordinary POPM also
checks XF output against its architectural ST1 bit on every observation.

DAGEN retains every target, value pattern, address form and older-store class
but distributes replay/gap choices instead of multiplying them by pages and
all snapshot points. It retains literal poisoning and complete bus events.
Standard-mode ARP is corrected to zero. Same-clock DAGEN/RAM event order is
an observed model regression, not a silicon ordering claim. Repeated-write
rejection is an implementation limit. Its40 cases retire duplicate page runs.

## Pass 5: trial result

`python3 tools/run_c54x_section.py all` rebuilds/runs each replacement in
sequence. Final measurements: immediate-store5.214/0.019s,
basic-memory5.423/0.020s, stack5.262/0.069s, DAGEN5.406/0.126s (build/run).
All459 cases and459 restores pass. Total build21.305s; execution0.234s.
An intentionally wrong immediate-store expectation is rejected, with no PASS
record (negative-control build5.065s/run0.015s). No legacy/default, handset
or full tool-suite execution was used for this trial.

Elapsed hands-on trial time was approximately16 minutes including tooling,
failed incremental attempts, review and commits. Each rebuild compiled one
test-driver source, reusing the CPU/framework objects. This is a warm-cache
measurement, not a clean-machine build claim.

Compared with `0ab50f98`, the largest file shrank10,551 ->10,046 lines.
Combined CPU and relocated C++ sources, including the new section files and
runner, shrank52,063 ->51,888 lines (175 net), and3,388,653 ->3,371,823 bytes.
The new Python runner adds80 lines: net reduction across C++ plus that tool
is95 lines. Four selected legacy populations totalled4,270 executions;
replacements retain459, removing3,811 executions, not distinct contracts.
The legacy suite is intentionally not executable as a whole during migration;
its missing continuations and old selection/oracle manifests remain reference
debt until integration. The parallel section runner is the executable authority
for these four replacements.

The observed retirement rate is roughly500 legacy lines per quarter-hour,
including one-time runner setup. About40k lines of legacy C++ remain across
modules, with varying amounts that can be retained unchanged or deleted.
Linear extrapolation is roughly20 hours, not a promise; budget15-30 hours for
the remaining migration/integration and final validation, revising after a
larger batch. Already structured independent cases should not be rewritten
merely to satisfy that estimate. Full retained-suite timing is still unknown.
