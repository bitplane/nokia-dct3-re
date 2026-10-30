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

Subsequent replacements use the same section-only workflow:

| Section | Contract and deliberate reduction | Validation |
| --- | --- | --- |
| `paired_store.h` | DST high/low write order, odd/even address, DP/CPL, both accumulators, four saturation boundaries and poisoned source after Read. All64 numeric tuples retained with distributed replay points instead of192 executions. Standard-mode ARP corrected to0; literal saturation expectations replace the duplicated arithmetic formula. | Native64 cases/restores; build5.122s/run0.038s. Legacy DST start and phases251530/251540 removed. |
| `register_load.h` | LDM zero-extension, nine source classes, both accumulators and SXM/CPL states, peripheral wait and poisoned retained RAM value. Keep both XPC pages but retire other page replication; distribute replay points. Correct standard-mode ARP to0 and use literal source expectations instead of CPU readback. | Native80 cases/restores; build5.298s/run0.045s. Retires288 legacy executions and start/results phases251550/251560. |
| `memory_dp.h` | Direct/absolute memory DP, held-clock forwarding to adjacent consumer, nine-bit masking and poisoned data/address-extension retention. Eight representative page boundaries replace all512; all three replay points remain. Standard-mode ARP corrected to0. | Native48 cases/restores; build5.321s/run0.034s. Retires3,072 legacy executions and phase251590/251600 code. |
| `data_stack.h` | PSHD/POPD four addressing forms, retained/poisoned source, allocation, one stack access and replay at four boundaries. Keep separated compiler-mode consumers and ADDM-to-PSHD forwarding/wrap. Retire six zero-gap consumer predictions lacking an independent latency basis. Standard-mode ARP corrected to0. | Native76 cases/restores; build5.374s/run0.043s. Removes legacy generic data-stack starts/results. |
| `tests/firmware/c54x_descriptor_copy.h` | Firmware integration: public six-instruction ADDM/PSHD/MVDM/POPD sequence at original extended address, two values, both SXM states and three replay points. No private ROM. Complete bus trace includes pointer publication omitted by old narrow taps; same-clock ordering is a model regression, not silicon evidence. | Native12 cases/restores; build5.371s/run0.019s. All12 original copy tuples retained outside CPU conformance. |
| `register_store.h` | Six source registers and four address forms, captured address despite later AR mutation, unchanged unrelated registers. Keep the three-store sequence at all three replay points; ordinary tuples use distributed replay instead of triple repetition. Standard-mode ARP corrected to0. | Native27 cases/restores; build5.620s/run0.025s. Retires75 legacy executions and phase251950/251960 code. |
| `absolute_load.h` | Literal address capture, poisoned extension/data, signed/unsigned16-to40-bit results and unchanged context. All64 original tuples/checkpoints retained, with literal expected values and standard-mode ARP corrected to0. | Native64 cases/restores; build5.230s/run0.039s. Removes start/results phases252150/252160. |
| `stack_control.h` | IMR replacement/IFR W1C POPM without destination reads, poisoned retained stack value, before/after-write replay; DAGEN-bank conflict versus RAM control; active-repeat POPM rejection before effects. Remove duplicated pages, retain all register/value/pending combinations, and add replay to the three rejection controls. Same-clock publication order is model-only evidence. | Native108 cases/restores; build5.365s/run0.055s. Replaces192 IRQ-pop,12 conflict and3 rejection executions; removes their starts, decoder and phase handlers. |
| `stm_ram.h` |256 successful indirect and256 rejected-alias cases retain all selectors, four literal/destination choices and both compatibility states.512 direct cases retain every32 low-RAM destination, eight literals and adjacent/gapped LDM. Distribute status and snapshot choices; retire page/status/replay Cartesian replication. Preserve poisoned literal/pointer/status inputs, neighboring RAM, no destination reads, complete bus events and clearing the frontend before restoring a saved pending instruction. Correct nonzero standard-mode ARP. | Native1,024 cases/restores; build5.449s/run0.398s. Removes68,608 legacy executions and their fixture/starts/phases62134/62135. |
| `program_transfer.h` |32 short MVPD cases retain all destination modes, two source regions, overlay states and pages2/3.800 extended cases retain absolute plus all indirect selectors, four values, mapping/region/compatibility combinations, pointer/source poisoning and all-AR preservation. Distribute replay points; collapse redundant absolute-selector and page-zero replication. Correct standard-mode ARP. New program-event observations check literal order and held source-read clocks; unrelated instruction fetches are outside that trace. | Native832 cases/restores; build5.582s/run0.410s. Replaces6,272 legacy executions and their starts/phase handlers. Memory-DP48 and STM1,024 retained-literal controls also pass after the observer addition. |

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
