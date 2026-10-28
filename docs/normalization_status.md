# Evidence normalization and implementation debt

This document owns the evidence regime's coverage, runtime-control classes and
instrumentation limits. It is not another handset capability catalogue or a
claim of upstream readiness.

## Authorities

| Question | Authoritative document |
| --- | --- |
| What works on a product, and under which composition? | [Model coverage](model_coverage.md), then the linked product's acceptance contract. |
| Which predicates, frames and profiles define acceptance? | [Structural regression](structural_regression.md), named manifests and reviewed evidence. |
| Which hardware contracts remain unknown? | [Fidelity roadmap](driver_vision.md), then the owning subsystem contract. |
| Which backend is selected, and where is HLE substituted? | [Upstream architecture](upstream_architecture.md) and product configuration. |
| How are observations admitted, falsified and retained? | [Evidence regime](evidence_regime.md) and [cleanup protocol](research_cleanup.md). |

## Evidence coverage

- `make evidence-check` validates hardware contracts, state predicates,
  falsifications, topology and runtime-pattern scope.
- The message census includes reviewed producer/consumer and descriptor-derived
  edges, positive controls and negative fixtures for known decode traps.
- Named manifests retain class-`0x40`, SIM, DSP-ring and mailbox observations;
  retired broad trace generators are not required to preserve their conclusions.
- Cross-ROM promotion requires a product-owned observation. NSE-8 RAM summaries
  are not portable acceptance oracles for another handset.
- PIN acceptance and registration must precede an authenticated save boundary;
  post-load events cannot retroactively satisfy those prerequisites.
- Native CPU, isolated codec, music-DSP, handset HLE and physical-interface
  qualifications remain distinct. A missing ROM or capture for one boundary
  does not block unrelated software work.

## Device and model debt

The component roadmap and subsystem contracts own device behavior and physical
unknowns. Keep the following implementation-policy distinctions when extending
those models:

- Raw ADC tuples and unmeasured serial/clock delays are declared inputs or
  calibrations, not inferred electrical units.
- Valid generated NV checksums do not establish factory provisioning.
- Physical MIC/EAR routing evidence does not recover COBBA mux/gain controls
  or establish native speech. NHM-6/NHM-2 routes and neutral-gain HLE duplex
  have independent acceptance; they are not unknown topologies.
- Headless LCD mirrors, delayed keys and diagnostic session outputs are harness
  observations, not firmware state or alternative device interfaces.
- Product-local native/mask compatibility and storage comparisons retain their
  explicit qualification; no sibling success silently promotes them.

## Runtime-control ledger

Normal machine composition no longer depends on `NOKIA_DCT3_*` controls.
Product ADC tuples, clocks, SIMI, DSP/service peers and the validated 3210
laboratory-network peer are typed `nokia_product_config` data. Negative
composition is selected through standard MAME configuration ports, passive
diagnostics use MAME logging, and scripted physical-input/MMIO fixtures remain
outside the production driver in Lua:

| Class | Controls | Status |
| --- | --- | --- |
| Product hardware | Typed `nokia_product_config` fields and device setters | ADC inputs, timer clocks, SIM profile, display geometry, flash capabilities and peer composition are fixed by the selected machine configuration. |
| Negative composition | `HWCFG` MAME configuration port, with named files under `fixtures/` | A gate may disable a product-owned CCONT, SIM, DSP-service, external-service, radio or MAD2/COBBA PCM component. It cannot enable hardware absent from the product profile. |
| Controller conformance | `DIAGCFG` MAME configuration port, selected by a named fixture | Requests an internal device invariant check without changing firmware state or normal machine composition. |
| Network event | `NETCFG` MAME configuration port, selected by `fixtures/radio_paging` or `fixtures/radio_incoming_call` | Queues exactly one incoming page or one bounded incoming-call attempt after organic registration; the default laboratory cell remains passive and emits only no-identity PCH fill. |
| Passive diagnostics | Standard MAME logging categories, enabled together by `RUN_VERBOSE=1` in the local harness | Read-only and bounded observations. No device or driver parses a diagnostic environment variable. |
| Harness/output controls | Lua-only `NOKIA_DCT3_*` controls for snapshots, summaries, scripted keys, charger/RTC events, MAD2 MMIO fixtures, MBUS input and save-state checks | External test orchestration. These variables are parsed only by `mame_nokia_dct3_input_exerciser.lua` and are not production configuration. |

There are no retained firmware-result, callback-key, task-message, or direct
registration-state forcing controls.

## Instrumentation debt

All driver/component sources now carry MAME license and holder headers; device
headers use conventional include guards and do not include the `emu.h` umbrella.
The save-state audit distinguishes emulated state from configuration and
diagnostics: registers, FIFOs, protocol phases, mutable card data and controller
line state are registered. GSM cipher algorithm, Kc validity/value, activation
boundary and TDMA frame context are serialized; keystream registers are not
needed because generation is per burst. Immutable product settings remain
configuration. `make verify-mad2` retains the executable
mid-boot save/load check.

The extracted `nokia_radio_peer_device` uses named phases for the recovered SEARCH_LIST,
channel-change, RA_INFO, BCCH, random-access, Location Updating and release
transactions. Decoded link establishment and sequence state belong to
`nokia_lapdm_link_device`; complete Layer-3 request and bounded MM/CC/SMS
transaction state belong to `nokia_gsm_session_device`. Radio-peer report counts remain explicit
because the firmware requests and consumes those reports incrementally. A future table-driven
representation is worthwhile only where it preserves request correlation and
makes the protocol easier to review.

The retained MAME log categories are scoped as follows. The production driver
uses one verbose-mode gate; categories keep each output stream named and make a
future per-mask migration mechanical rather than maintaining parallel boolean
configuration:

| Trace | Purpose |
| --- | --- |
| `TRACE_DISPLAY` | selected-operator resource presentation used by the registration gate |
| `TRACE_DISPLAY_PROFILE` | descriptor-`0x0749` load/update/copy and setup-message boundaries |
| `TRACE_DISPLAY_IO` | GENSIO LCD endpoint selection and command/data transfers |
| `TRACE_DSP_BOUNDARY` | shared-ring requests and request-derived peer responses |
| `TRACE_DSP_SHARED_READS` | first read and value transitions for each firmware PC/DSP-shared-memory offset pair |
| `TRACE_DSP_SHARED_TRANSITIONS` | every firmware observation of the small peer-owned scalar set, for request/completion correlation |
| `TRACE_GENSIO` | serial register transactions |
| `TRACE_CCONT_WATCHDOG` | combined firmware service-helper calls and logical-descriptor writes to physical CCONT watchdog register 5 |
| `TRACE_CCONT_ADC` | physical input IRQ latches and firmware-selected CCONT ADC channels/raw values |
| `TRACE_CCONT_RTC` | CCONT RTC traffic, IRQ policy, and filtered user-alarm `0x46bc` publication/routing |
| `TRACE_MAD2_LEDGER` | first-access MAD2 register census |
| `TRACE_MAD2_TIMERS` | timer-0 divider/counter/compare and FIQ assertion/acknowledgement lifecycle |
| `TRACE_MAD2_INTERRUPTS` | MAD2 source, pending, mask, acknowledgement and CPU-line routing transitions |
| `TRACE_MAD2_CLOCKS` | reset-cause, watchdog, peripheral clock-gate and timer-1 boot accesses |
| `TRACE_MBUS` | MBUS register, byte, status and FIQ-facing controller lifecycle |
| `TRACE_BUZZER` / `TRACE_VIBRATOR` | changed organic output state used by focused peripheral gates |

The small firmware-address-specific set retained by focused radio, display,
alarm, and watchdog gates lives in `driver/nokia_dct3_trace.inc`. A compliance
test keeps this quarantine below 400 lines and rejects state-changing APIs.
Hardware boundary traces remain beside their owning handlers. Closed task, service,
SIM-registration, and generic-display probes have been removed rather than
preserved as an investigation journal. The standalone overlay defaults to
firmware probes disabled; the research build can enable them with
`DCT3_RESEARCH_TRACES`. `make check-tracefree` protects the probe-free
composition independently of the research build.

## Topology gaps

- Mechanically recovered ROM edges are not automatically reviewed semantic
  identities. Quantify the covered producer classes before asserting absence.
- Dynamic generic-service descriptors and wider steady-state topology remain
  partially decoded; retained negative results are scoped to the searched
  descriptors and named runtime inputs.
- Peer behavior is admitted only for exercised requests and independently
  qualified products. Feature coverage belongs in the model matrix, not an
  expanding second list here.
- The exact internal cold-boot UI selector remains unnamed, but organic idle
  and menu input close the behavioral question. It is not a missing hardware
  handoff. The MMI contract owns conditional reinitialization and shutdown.

## Completion gate

A subsystem is boxed off only when it has:

1. an explicit hardware or transport boundary;
2. symbolic topology identities rather than address-only prose;
3. a repeatable structural or behavioral acceptance profile;
4. no firmware-result forcing, task-message injection, or direct state writes;
5. stale probes and contradicted narratives removed; and
6. a second-ROM confidence result where the behavior is claimed as shared.
