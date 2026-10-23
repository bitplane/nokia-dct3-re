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
- An encoded operand number is not an effective address. Recover its addressing
  mode and live page/frame controls before naming a mapped register. On C54x,
  a short direct Smem operand inherits DP/CPL, while a literal dmad/MMR
  address may not; identical numeric fields can name different storage.
  For register-based stores, follow the last dominating base reload and
  callee preservation: a routine can reuse the same register for MMIO and RAM.
- Zero-valued writes do not establish a clear operation. Decode the source
  and destination paths: a copy of zero-filled data can produce the same
  write trace. Keep copy source, copy destination and initializer length
  separate until their caller contract is established.
- Exact output matching can still be degenerate coverage. Require transitions
  and nondegenerate output specific to the claimed feature: zero PCM does not
  establish nonzero comfort-noise synthesis, and a blank frame does not
  establish an interactive display. Preserve those coverage checks alongside
  the byte-comparison oracle.
- Inspect inverse-video selections at pixel/row resolution before declaring
  a missing label. Separate the selected item from adjacent rows and recover
  their producer namespaces; unrelated stale user records are not evidence
  that the selected SIM name or LCD font is broken.
- Similar addressing names need not have the same legal operand domain.
  On C54x, short MOD=3 preincrement is write-only (SPRU131G table 5-4),
  while long-offset MOD=13 preincrement can read. A passing fixture for
  a mechanically decoded form does not establish that the form is legal.
  Classify the encoded operand's role, not the instruction's overall effect:
  a data-to-program transfer writes memory but its Smem is still a read source.
  Read/modify/write operands require their own admission analysis.
  When correcting admission, audit legacy default/atomic checks as well as
  isolated staged profiles; otherwise an obsolete positive expectation can
  survive the new rejection bank. Disassembler recognition is a separate claim.
- Widening an operation classifier requires auditing all of its consumers,
  not just the primary execution path. Forwarding, bypass, wait arbitration
  and repeat helpers can retain assumptions about the narrower family.
  Production and forwarded values should share one calculation where their
  contract is identical, with an executable producer/consumer test.
- An executable oracle can preserve a mistaken latency premise. Derive
  expectations from the instruction's consumer category, not only a conservative
  summary: SPRU131G table 7-13 distinguishes T used for shifting/bit tests from
  T used for multiplication. Include both an older producer and a younger writer
  so a live-register shortcut cannot pass the dependency test.
- Check fixture inputs against the documented domain before interpreting a
  mismatch. Circular addressing, for example, constrains the initial index
  and nonzero-buffer step size; an out-of-domain modulo oracle does not prove
  a CPU arithmetic defect. Keep empirical behavior outside that domain
  separately scoped rather than silently generalizing it.
- A repeated PC does not identify one pipeline instance. Lifetime release
  must respect the owning instruction's age/stage, so an older loop iteration
  cannot clear a younger instruction's protection. Include the shortest
  legal loops with pending interrupts and save/replay in composition tests.
- An unsupported word that decodes as a valid opcode may still be data reached
  by bad control flow. Establish upload/program provenance, physical mapping
  and the preceding executed transfer before extending the ISA. Debugger
  history may disassemble current raw memory rather than the bytes executed
  through an overlay; retain PCs and recover instruction bytes independently.
  Admission guards must also distinguish a completed opcode slot from retained
  extension words. Test opcode-shaped address/literal values inside legal
  multiword instructions and loops; matching raw bits alone is insufficient.

## Protect surprising facts

Compression is most likely to discard rare, counterintuitive facts. Preserve a
compact caution when a fact is easy to misread, corrects a common assumption,
or was expensive to discover. Current examples include:

- swap16 byte-lane handling for byte tables;
- keeping data/program spaces and logical/physical addresses distinct: an
  absent image word is not a zero word, and overlay projection must follow
  a proven mapping rather than a merged numeric-address dictionary;
- fetch-based PC taps missing sequential instructions between block-entry
  fetches; establish the hook's coverage rather than assuming it observes
  the same events as an Execute instruction hook;
- a program-bus read matching the live PC not establishing an execution:
  data reads and speculative fetches can match too; count retained Execute
  identities before assigning invocation or completion semantics. Match
  logical Execute addresses against translated physical bus addresses explicitly
  under overlay mappings; the same instruction need not have the same number;
- instruction observers omitting accelerated repeat or resumed-transfer paths;
  test execution coverage and save/replay, not only the main dispatch loop;
- an input harness logging the requested key name while an unrecognized research
  clone falls back to another product's matrix. Register the clone's physical
  input/profile alias and verify firmware consumption before attributing a
  resulting screen or waveform to that key;
- memory ranges serving as samples, control tables or stack in different
  lifecycle phases; scope observations to the consumer's actual footprint
  and active interval, and report partial masks, truncation and alias limits
  before making a producer/absence claim;
- treating a reused scratch buffer as persistent state; observe its value at
  the evidenced construction or consumption boundary, not an arbitrary endpoint;
- a staged CPU's unsupported Decode frontier not proving that preceding
  instructions reached Execute; validate retirement with a positive-controlled
  stage observation rather than the reported PC alone;
- Access/Read-owned register writes not inheriting the PC or loop membership
  of the different instruction currently occupying Execute; an Execute-entry
  breakpoint's SP can already include younger prologue stack effects, so it
  is not an atomic function-entry register snapshot;
- Lua tap subscriptions needing a live owner for the entire observation
  window; retain them in a callback/upvalue and validate positive-control
  detail records, not just counters, before interpreting absent events;
- Lua waits waking early on state load: external physical-input fixtures must
  recheck absolute emulated deadlines, not mistake an early key release for
  failed device restoration;
- repeated Lua field lookups returning different wrappers for one physical
  contact: restore input timelines by stable port/mask identity, not userdata
  identity or unordered wrapper iteration. Include repeated presses and a held
  key at save in replay controls before blaming a timing mismatch on hardware;
- debugger register exports exposing cached values: compare named PC with
  architectural GENPC before attributing a mismatch to CPU save-state loss;
- banked ARM7 state: MAME's `R8..R14` exports are the user bank, not an
  automatic view of the active exception bank. Record CPSR mode and select
  `FR8..FR14` in FIQ, or the corresponding IRQ/SVC/ABT/UND SP/LR exports,
  before attributing an interrupt-time caller or stack to firmware;
- shift/carry selectors: `LSRS #n` puts original bit `n-1` into carry.
  Decode the conditional branch and intervening flag writers explicitly;
  a later CPSR snapshot need not retain the carry used for that branch;
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
