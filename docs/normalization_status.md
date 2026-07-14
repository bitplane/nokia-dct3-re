# Normalization status

This is the boundary between conclusions that have entered the normalized
evidence regime and research scaffolding that still needs ownership, validation
or removal. It is an inventory, not a claim that the driver is normalized end
to end.

## Covered now

- Four named runtime manifests define the default, deep-GSM, contact-service and
  3330 smoke profiles.
- `make verify-frontier` is the canonical current research profile. It composes
  `MODEL_DSP_SERVICE`, `MODEL_CCONT_PRESENT`, `MODEL_DSP_CONTACT_PEER`, and
  `MODEL_SIM_DEVICE`; the request-driven peer replaces the legacy service
  responder/drain pair. Its exact frame plus semantic structural predicates
  end in mode `0x0004` with
  contact status `0x49`, no-SIM clear, and SIM enable set.
- The 3210 v6.00 topology profile has stable symbolic identities for the
  reviewed nodes and semantic edges across the active boot frontier.
- Hardware contracts, state predicates and falsifications have machine-checked
  ledgers. `tools/validate_evidence.py` reports the authoritative current counts
  rather than this document duplicating values that drift as findings land.
- Runtime observations are subsystem-scoped. A trace collected for contact
  service can no longer silently establish absence in generic service or GSM.
- Current network and resource-provider conclusions are separated from the
  address-heavy historical investigation documents.
- The evidence validator checks ledger schemas, node and edge identities,
  manifest references and runtime-pattern scope before census generation.

This coverage is deliberately narrower than the whole ROM. It normalizes the
current frontier and the contracts already strong enough to state precisely.

## Topology gaps

- The census contains 37 RAM-built or unresolved descriptor registrations. Of
  these, 30 statically target services other than service 5 and seven select the
  service dynamically. Six dynamic sites are excluded by their event/callback
  fields; the remaining indirect resident site is dormant. The named deep run
  observes only service-`0x0a` transient registrations and no resident
  registrations, so none populates service 5 in the current boot.
- Only the active 3210 frontier has semantic node and edge annotations. Most of
  the mechanically recovered call sites and consumers are not yet promoted to
  reviewed topology.
- The 3330 is a smoke-test and comparative input. Its equivalent node addresses
  and semantic edges have not been mapped, so it is not yet a second supported
  topology profile.
- The contact-service producer boundary is resolved: the DSP-facing D0 and
  type-`0x70` transactions and the task-7 external service session compose in a
  single boot. Ordinary non-CPHS SIM initialization also completes. The active
  topology gap is now the ordinary owner of global status `0x05eb`. The mapped
  display-status route beginning with `0x0280`-`0x0282` is service/test-owned
  and is no longer the active ordinary-boot hypothesis. Callback `0x28`'s
  service-5 `0x05dc` initializer is also excluded: the apparently comparable
  callback-`0x2f` initialization is deliberately selected by terminal status
  `0x012f`, and no ordinary selector for callback `0x28` is mapped. The
  transient service framework is also excluded as the immediate owner.
  Ordinary boot populates service `0x0a`, but the exhaustive literal/callsite
  census finds `0x0394` constructors only for services `0x08`, `0x19`, and
  `0x1a`; the mapped dynamic activation branch therefore does not activate
  service `0x0a` in ordinary boot.

## Model-boundary debt

The following opt-in paths remain useful, but are not final hardware models:

| Area | Current role | Remaining normalization work |
| --- | --- | --- |
| SIM | `MODEL_SIM_DEVICE` is the sole card model and owns the stateful register/FIQ path. | Validate additional SIMI FIFO flags and card commands as firmware reaches them. |
| Contact service | `MODEL_DSP_CONTACT_PEER` answers observed DSP requests and runs the external class-`0x40` session through task 7. | Extract the peer from the driver once the surrounding DSP mailbox ownership is stable. |
| Service channel | Compact class-`0x7f` acknowledgements complete firmware-originated transactions, including `0x622a`, at the transport boundary. | Expand only when a new organic request demonstrates additional framing or status semantics. |
| DSP | `MODEL_DSP_SERVICE`, `MODEL_DSP_RING_DRAIN`, and `MODEL_DSP_CONTACT_PEER` cover distinct observed portions of the missing DSP-side machine. | Consolidate them into one request-driven peer without losing the verified ordering between D0, type `0x70/0x74`, and contact startup. |
| Startup | `MODEL_STARTUP_REPORTS` supplies firmware-visible subsystem reports. | Replace the firmware-call bridge with the devices or transport peers that own each report. |
| Display | `MODEL_LCD_TRANSFER_FIQ` supplies a provisional transfer interrupt. | Validate the precise controller interrupt contract and move it into the display device. |

`MODEL_CCONT_PRESENT` remains a scenario selection around a real extracted
device, rather than direct proof that every CCONT register behavior is faithful.

The historical `make verify-deep` profile intentionally retains
`MODEL_SVC_RESPONDER` and `MODEL_SVC_CHANNEL_DRAIN` only to protect its older
Insert SIM oracle. It is not a source of new frontier evidence.

## Instrumentation debt

Eleven `TRACE_*` environment switches remain. The completed slot-45 watcher and
chooser plus the historical `TRACE_LIMP`, `TRACE_LIMP2`, and
`TRACE_CCONT_READ` blocks have been retired. The active GSM, DSP, contact, SIM,
task and UI traces currently earn their keep. Future investigations
should extend a named manifest or add a narrowly scoped trace, then remove it or
record why it remains part of a repeatable acceptance profile.

The default frame and structural oracle reproduce exactly. The historical deep
profile reproduces its exact frame and stable semantic/CCONT predicates after
the recursive Make wrapper was corrected to export its model variables.
The deep and frontier profiles intentionally do not oracle raw LCD-command or FIQ counts: two
equivalent runs produced the same exact frame and semantic state with different
counts. Those counters remain timing diagnostics rather than correctness gates.

## Exit criteria for future normalization

A subsystem is boxed off only when it has:

1. a named hardware or transport boundary;
2. symbolic topology identities instead of address-only prose;
3. an observable state predicate and a repeatable acceptance profile;
4. no direct firmware-state forcing in its supported path; and
5. stale probes and contradictory historical claims removed or clearly marked.

The ordinary GSM 11.11 SIM initialization sequence is now a validated supporting
contract rather than the immediate frontier. Correct directory metadata and the
requested `EF_LP`/`EF_SST` files let SIM enable rise organically and complete the
non-CPHS initialization pass; task 20 then enters its timed card-presence monitor.

The task-1 report-code-`0x07` frontier remains an ordinary-boot contract, but
the power/analog candidate is excluded. Task 1 explicitly waits for the same
report in both branches out of mode `0x000d`: SIM-ready/no-charger selects mode
`0x0007`, while the fallback selects mode `0x0004`; both run the shared init
burst only after receiving code `0x07`. Exhaustive caller review leaves four
reporter sites. Runtime state tracing places the power task in its normal
no-charger state `0x04`; `0x21e40c` is a shutdown path and `0x21f8de` is reached
only through firmware blocks labelled maintenance and cold charging. Modeling
an analog-ready event solely to reach either site would select the wrong
lifecycle. Selector `0` is a CPHS/SST provisioning check whose complete
contract includes the EEPROM product-feature gate. Its following `0x27f150`
path is a conditional Advice-of-Charge limit controller. Coherent
exhausted-account evidence reaches its completion and proves it publishes
`0x019a`, not `0x0795`. The actual non-display `0x0795` producer is downstream
of catalogue sequences for packed `0x213a`/`0x613a` and
`0x213b`/`0x613b` (status indexes `0x013a`/`0x013b`), both of which emit
`0x08b0`; their callback paths are established only in later framework mode
11. Mapped service/test, SAT, power,
and this later controller lifecycle remain excluded from ordinary mode-4
startup; the frontier is again unresolved callback-`0x5d` completion
ownership.

Status `0x0394` remains excluded from activating ordinary service `0x0a`: its
three recovered constructors are tied to services `0x08`, `0x19`, and `0x1a`.
