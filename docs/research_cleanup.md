# Research cleanup protocol

Reverse engineering benefits from provisional vocabulary. A hypothesis gives
an investigation a shared shape, focuses experiments, and accumulates useful
reasoning context. The failure mode appears later: repeated hypotheses spread
into symbols, filenames, trace names, topology nodes, documentation, and
component boundaries. Repetition then makes a claim look established even
after its premise has been corrected.

Cleanup is therefore not general shortening. It is **epistemic compilation**:
convert investigation material into the smallest evidence-backed set of
contracts, cautions, tests, and unresolved questions needed to continue work
without losing expensive knowledge.

## Evidence classes

Every durable claim should fit one class:

| Class | Contents | Durable home |
| --- | --- | --- |
| Observation | Addresses, bytes, control flow, messages, register activity, timings | Firmware maps, traces summarized by a normalized artifact |
| Derived contract | A conclusion supported by reproducible observations | Concise subsystem document and normalized evidence |
| Working hypothesis | A useful but unproved interpretation | Current investigation notes or explicitly provisional symbols |
| Falsification | A plausible interpretation killed by decisive evidence | `evidence/falsifications.json`, when retaining it prevents an expensive repeat |
| Historical narrative | The order in which experiments and guesses occurred | Git history, not current documentation |
| Compatibility name | A stale identifier retained because tools or schemas depend on it | Local compatibility note; never treated as a semantic claim |

The main error to prevent is silently promoting a working hypothesis to a
derived contract.

## Naming discipline

Names are evidence claims. A durable name must describe no more than the
evidence establishes.

Prefer operation- and boundary-based names:

- `task13_segmented_object_consumer`
- `class40_service_command_dispatcher`
- `status_1392_handler`
- `selector4_threshold`
- `ram_11fed0_service_status`

Avoid subsystem ownership or lifecycle semantics until independently proved:

- `display_window_task`
- `radio_init`
- `battery_ready`
- `fault_screen_server`
- `normal_boot_completion`

Names should mature with the evidence:

1. address only, such as `sub_23e62c`;
2. observed input or operation, such as `task13_status_040b_handler`;
3. established functional contract, such as
   `task13_segmented_object_consumer`;
4. subsystem ownership only after the task context and wider responsibility are
   independently established.

The canonical RTOS identity is the ROM plus numeric task/mailbox id. Project
aliases follow `rtos_tasks.md`; they are not recovered Nokia names.

## Cleanup depth by subsystem state

Cleanup depends on the maturity of the affected subsystem.

### Active investigation

- Preserve useful provisional hypotheses and focused diagnostics.
- Mark speculative names visibly.
- Keep raw traces until their conclusions are normalized.
- Do not extract a component around a boundary whose contract remains unknown.

### Mapped but incomplete

- Delete disproven probes and forcing shims.
- Consolidate observations into normalized evidence.
- Rename speculative ownership claims to neutral functional aliases.
- Preserve concrete unresolved boundaries and high-value falsifications.

### Validated subsystem

- Retain contracts, focused acceptance tests, stable names, and hardware
  interfaces.
- Remove chronological investigation narrative and broad diagnostics.
- Introduce new hypotheses only in a clearly provisional investigation layer.

### Upstream candidate

- No research-force behavior or firmware-state shortcuts.
- Minimal observation-only tracing.
- Component and device names describe modeled hardware, not a historical boot
  symptom.
- Documentation explains the resulting architecture and verified contracts,
  not the discovery journey.

## Cleanup pass

Use this sequence for a deliberate cleanup.

1. **Freeze the frontier.** Record acceptance commands, hashes, semantic
   predicates, current frame, relevant ROMs, and worktree state. Cleanup must
   not redefine success accidentally.
2. **Locate authority.** For each affected subsystem, identify its hardware
   contracts, state predicates, topology edges, concise document, acceptance
   run, and supporting addresses.
3. **Audit vocabulary first.** Search source identifiers, Ghidra symbols,
   topology node IDs, evidence owner fields, trace names, filenames, headings,
   and prose. Repeated names propagate stale premises more strongly than
   paragraphs.
4. **List contradictions.** Compare old names and claims with current evidence.
   Correct contradictions before removing explanatory detail.
5. **Compile narrative into contracts.** Replace chronology with input, output,
   owner or boundary, confidence, acceptance condition, and one concrete
   unresolved question.
6. **Retain negative knowledge selectively.** Keep a falsification only when it
   blocks a plausible, costly repeated mistake. Preserve the disproven claim,
   decisive counter-evidence, and corrected interpretation, not every probe.
7. **Quarantine compatibility residue.** Keep an old filename, manifest,
   environment variable, or schema key when renaming cost exceeds its value,
   but label it explicitly as historical and non-semantic.
8. **Remove implementation sediment.** Delete dead probes, superseded knobs,
   unused trace sites, temporary fixtures, generated logs, and documentation of
   implementations that no longer exist.
9. **Validate structure and behavior.** Run evidence/schema validation,
   topology fixtures, default/frontier acceptance, affected cross-ROM checks,
   and searches for retired terminology.
10. **Review the diff as a knowledge change.** Confirm that every deletion is
    recoverable from Git history and every retained assertion has one
    authoritative home.

## Proof obligations

Some recurring mistakes need stronger checks than ordinary source review:

- A static "no producer" result closes only the source classes actually
  searched. Quantify coverage of literal call sites, computed arguments,
  descriptor or interpreter tables, context-derived events, and external
  inputs before claiming producer absence.
- A comparison in a wait-shaped block is not necessarily a blocking gate.
  Decode both successors and their side effects before deciding whether it
  waits, selects a continuation, records state, or falls through.
- An address hit does not establish dispatcher or subsystem ownership. Confirm
  the containing function boundary and exhaustively decode the relevant
  dispatch extent or caller set before assigning an input to a handler.

## Protect surprising facts

Compression is most likely to discard rare, counterintuitive facts. Preserve a
compact caution when a fact is easy to misread, corrects a common assumption,
or was expensive to discover. Current examples include:

- swap16 byte-lane handling for byte tables;
- PC hooks firing only at actual branch targets;
- constructors that acknowledge an incoming command rather than initiate it;
- report code 7 belonging to shutdown/power lifecycle rather than ordinary
  boot readiness; and
- a numeric task id not being portable across products or ROMs.

Keep the surprising conclusion and decisive evidence. Do not keep the entire
chronology that produced it.

## Retention test

For each retained statement or artifact, ask:

1. Is it observed, derived, provisional, disproven, or compatibility-only?
2. What lets another engineer verify it?
3. Does its name claim more ownership or semantics than the evidence?
4. Is it required to operate the driver, reproduce a conclusion, guide current
   work, or prevent a costly repeated mistake?
5. If deleted, is Git history sufficient?
6. If retained, where is its single authoritative home?

A cleanup is successful when the repository starts the next investigation with
less inherited bias while retaining the evidence and cautions that make future
reasoning cheaper.
