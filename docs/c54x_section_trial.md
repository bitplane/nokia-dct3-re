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
| `repeat_program.h` | Repeat counts0/1/3/15, fixed/advancing destinations and mapped/unmapped sources; IRQ/NMI acknowledged only after all writes, with RTN9004 and restored SP. Verify source-read/write order, first-source poisoning and no early continuation result, then fresh66 continuation. Distribute page/replay choices rather than multiply them. Independently vary IRQ source and mapping (the old bank coupled them). Retire exact speculative-prefetch counts, not continuation behavior. Data trace is explicitly scoped to the transfer buffer; return/stack behavior has separate assertions. Standard-mode ARP corrected to0. | Native48 cases/restores; build5.539s/run0.028s. Replaces144 legacy executions and their start, IRQ callback and phases252310/252320. IRQ source/timing/RTN/transfer-count expectations now belong to each case rather than a basic-memory runner macro. |
| `memory_repeat.h` | Memory-counted RPT/READA with AR/RAM count, counts0/1/3/15, OVLY and7fff boundary, distributed source pages0/2/7f, IRQ/NMI/ordinary execution. Keep count, accumulator-pointer and first-sample poisoning; ordered23-bit reads and count+1 writes; no early continuation and fresh66 result; IRQ deferral, RTN9003 and restored SP. Distribute pages/replay points instead of multiplying them. Retire speculative-prefetch-count checks, not continuation behavior. Sparse high program RAM avoids allocating an entire23-bit space. Data ranges explicitly cover count and destination only, matching the old taps. Standard-mode ARP corrected to0. | Native96 cases/restores; build5.493s/run0.058s. Replaces400 legacy executions and their start, IRQ callback and phase252410/252420 handlers. Repeat-program48 and basic-memory18 controls also pass. |
| `absolute_add.h` | Unify AR1/BK/SP/RAM source banks, removing duplicated RAM matrices. Keep all four operand and accumulator boundaries, SXM/OVM, both destinations, independent integer result/carry/overflow/saturation reference and poisoned literal/operand. Distribute SP/CPL and replay points; retire page replication. Correct standard-mode ARP. Separate SP-context/admission tests are not subsumed. | Native512 cases/restores; build5.430s/run0.210s. Replaces8,192 executions across three banks and their start/phase252430/252440 code. |
| `immediate_context.h` | All four literal values, two offsets and DP/CPL contexts; poison literal/CPL then DP or SP after capture. Exact write/no data reads and unchanged later context. Distribute replay points; retire page copies. Correct standard-mode ARP. | Native16 cases/restores; build5.413s/run0.023s. Replaces128 executions and phases252510/252520; surviving mapped-MVDD handler renamed for its actual owner. |
| `stlm_address.h` | STLM to every AR, both accumulator sources, four saturation boundaries and independently varied SST/SXM flags. Use two NOPs, sufficient for the stricter Category-I ARx separation in SPRU131G table7-7 (not the distinct ARP/CMPT table). Retire short-gap old-pointer projections and redundant longer-gap replay copies; do not assert that every shorter gap is universally invalid. Old flags were coupled to gap length; new safe numeric coverage decouples them. Preserve ordered publication/consumer reads, source/status/all-AR checks. Correct standard-mode ARP. Add final IDLE after the consumer instead of unobserved NOP padding. | Native256 cases/restores; build5.362s/run0.111s. Replaces768 executions and its helper/start/phases252530/252540. |
| `stlm_peripheral.h` | STLM to peripheral21: both accumulator sources, all eight numeric boundaries, independent SST/SXM; distribute status sentinels and replay before/around/after publication. Preserve Execute-live source sampling, no reads, exactly one write at model clock7, unchanged source/status and destination value. Retire duplicate program pages and Cartesian CPL/OVM/FRCT/snapshot copies; no hardware identity or silicon-cycle claim for this destination. | Native64 cases/restores; build5.415s/run0.039s. Replaces3072 executions; delete old setup and phases62060/62061. |
| `stlm_register.h` | T-to-MPY/MPYR/MPYU: both accumulators, eight values, independent SST/SXM/OVM/FRCT; distribute zero/one NOP gaps and replay points. Table7-13 supports zero-latency T-to-multiply. TRN/BRC-to-LDM: retain all eight values and SST/SXM with separated consumers; discard short-gap old-value projections. Preserve Execute-live source, publication, retained operand poisoning, status/untouched registers and exact ordered bus events. Discard duplicate pages, longer-gap and Cartesian snapshot copies; same-clock order is model-only. No block-repeat claim for BRC. | Native896 cases/restores; build5.464s/run0.346s. Replaces15360 executions; delete shared decoder, setup and phases253270/253280. |
| Firmware `c54x_byte_swap.h` | Single and adjacent byte-swap sequences, all16 input patterns and16 status combinations. Preserve intermediate dependent accumulator/flag states, AR6 postincrement, retained input poisoning, same-clock second Read/first Write ordering, IDLE and replay. Distribute snapshots instead of crossing them with duplicate program pages. Keep firmware sequence outside generic instruction sections. | Native512 cases/restores; build5.517s/run0.201s. Replaces3072 executions; delete body/join setup and phases253310..253340. |
| Firmware `c54x_block_swap.h` | RPTB around the byte-swap body: count0/1/2/7, all16 values and16 status combinations, Decode-owned count/BRAF changes, exact zero-overhead bus clocks, final buffer/pointer/IDLE. Distribute replay points; discard duplicated pages in ordinary runs. Retain24 IRQ/NMI cases crossing two pages and six replay positions, ordered bus work, single ACK, cleared BRAF, restored SP and final buffer. Interrupt cases intentionally do not assert ISR-duration clocks (as before). Replay point14 becomes15: snapshots cannot retain an unprocessed scheduler host-pin action issued in the same tick. | Native1048 cases/restores. Replaces6168 executions; delete old setup/helpers/phases253350..253361. RPTBD remains a separate pending family. |
| `block_edges.h` | Both one-word and two-word valid block endpoints, plus six distinct staged-model exclusions: nested delayed branch/repeat, count mutation, malformed two-word endpoint, oversized endpoint and indirect count store. Both program pages, no data work, early replay, correct count/BRAF/AR/result for accepted cases. Retain all16 executions; unsupported cases are implementation limits, not silicon claims. | Native16 cases/restores; build5.344s/run0.022s. Delete old setup and phases253370/253380; the surviving RAM-forwarding checker is renamed for its actual contents. |
| `delayed_block.h` | Four RPTBD delay layouts: two single-word pointer updates, immediate T write, retained long-offset load, extended pointer modification. Count0/1/2/7 and four ordinary data patterns. Retain IRQ/NMI arriving in the delay span or after first body write; observe completed delay effects before ACK, one exact stack write/read, ordered copies, all pointers/status/repeat boundaries/IDLE and final buffer. Poison endpoint/delay literal/loaded RAM after sampling. Distribute six interrupt/four ordinary replay positions and program pages instead of Cartesian repetition; ordinary and interrupt arrivals remain distinct. Exact continuation words are model regressions, not silicon latency claims. | Native128 cases/restores; build5.708s/run0.065s. Replaces1280 executions. Delete legacy block-repeat file, typed fixture, private ACK helper, shared decoders and routing; remove its obsolete source-ownership assertion. |
| `delayed_square.h` | SQURA/SQURS/SQUR delay slots, both destinations, eight numeric boundaries, all FRCT/OVM/SMUL combinations, direct/indirect/postincrement; first/second/both-same/both-split slots and SQUR-to-MPYA dependency. Arithmetic oracle follows SPRU172C square operations and signed A[32:16] multiply; no CPU helpers. Preserve intermediate A/B/sticky OV, T-at-Read/newer-T protection, pointer publication, poisoned opcode/source retention, exact source-read clocks, ordered body results, IDLE and frontend-cleared restore. Distribute count0/1/7 and four replay points; remove duplicate pages and SXM/count/snapshot Cartesian copies (SXM is a preserved sentinel, not a listed square modifier). Interrupt composition is still not claimed here. | Native4992 cases/restores; build5.707s/run1.898s. Replaces239616 executions. Delete fixture decoder/setup/phases62178/62179; rename the surviving checker for XC return. Source size is essentially unchanged in this family; the gain is independent ownership and execution reduction, not line-count compression. |
| `delayed_count_branch.h` | BANZD: all8 encoded ARs, no-change/decrement/increment, standard/compatibility selection, zero/one/ffff/8000 counts and single/double-word delay layouts. Preserve pre-modification predicate, all-AR publication, changed-count/literal retention, exact taken-target fetch, intermediate delay accumulator states, unchanged status/SP, no data accesses, IDLE and replay. Distribute pages/four replay points rather than crossing them. Drop speculative target-fetch absence on the not-taken path: continuation results establish branch semantics, while speculative fetch policy is not an architectural promise. | Native384 cases/restores; build5.478s/run0.162s. Replaces3072 executions; delete decoder/setup/phases253250/253260 and declarations/routing. |
| Firmware `c54x_signed_branch.h` | Retain the12 LDM/long-SUB/AGT producer cases: count0/1/2/ffff and replay around arithmetic retirement/branch evaluation. Ordered mapped-register/RAM reads, signed40-bit result and carry, taken-target fetch clock, selected continuation, unchanged AR6, no writes and IDLE. Drop speculative not-taken target-fetch absence as in the generic delayed-count section; final continuation still proves the predicate outcome. No private ROM loaded. | Native12 cases/restores; build5.373s/run0.019s. Delete old setup and phases252990/253000; ordinary signed-branch coverage remains pending separately. |
| `offset_count_branch.h` | Long-offset BANZ plus both BANZD delay layouts: all8 encoded registers, offset/preincrement/circular forms, eight literal input/offset boundaries, standard/compatibility mode. Independent literal results distinguish 16-bit wrap from the legal0x8000-word circular domain. Preserve pointer publication, retained offset/target, changed AR/BK immunity, delay effects, final accumulator/status and exact taken-target fetch; no data accesses. Distribute pages/three replay points, remove Cartesian copies. As before this observes a bounded NOP continuation rather than requiring IDLE; speculative not-taken target-fetch absence is not retained. | Native1152 cases/restores; build5.420s/run0.449s. Replaces6912 executions; delete staged setup/phases253090/253100 and routing. Shared old decoder remains used by the unmigrated offset-loop family. |

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

After the case-owned IRQ expectation change, all15 migrated sections pass:
2,842 cases/restores, including the firmware descriptor-copy section. This
is a migrated-section checkpoint, not full CPU or handset acceptance.
The basic store IRQ acknowledges in the same clock before its Write tap;
the basic load acknowledges after its earlier Read. Case-owned bus-progress
expectations preserve that distinction without putting opcode logic in the
IRQ runner. Snapshot registration restores the IRQ count/last-case observer.
