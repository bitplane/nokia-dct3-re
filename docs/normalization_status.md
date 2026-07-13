# Normalization status

This is the boundary between conclusions that have entered the normalized
evidence regime and research scaffolding that still needs ownership, validation
or removal. It is an inventory, not a claim that the driver is normalized end
to end.

## Covered now

- Four named runtime manifests define the default, deep-GSM, contact-service and
  3330 smoke profiles.
- The 3210 v6.00 topology profile has stable symbolic identities for 13 nodes
  and nine reviewed semantic edges across the active boot frontier.
- Hardware contracts, state predicates and falsifications have machine-checked
  ledgers. The current ledgers contain 18 entries in total.
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
- The contact-service producer boundary is now resolved: the DSP-facing D0 and
  type-`0x70` transactions and the task-7 external service session compose in a
  single boot. The remaining topology gap starts after ordinary SIM traffic.

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

## Instrumentation debt

Fourteen `TRACE_*` environment switches remain. The active GSM and DSP boundary
traces currently earn their keep; several older boot and UI traces are light but
do not yet have an explicit owner or retirement condition. Future investigations
should extend a named manifest or add a narrowly scoped trace, then remove it or
record why it remains part of a repeatable acceptance profile.

The default frame and structural oracle reproduce exactly. The deeper CCONT
profile still has the documented counter delta and must not be described as an
exact oracle pass. That difference remains a state-predicate fidelity item.

## Exit criteria for future normalization

A subsystem is boxed off only when it has:

1. a named hardware or transport boundary;
2. symbolic topology identities instead of address-only prose;
3. an observable state predicate and a repeatable acceptance profile;
4. no direct firmware-state forcing in its supported path; and
5. stale probes and contradictory historical claims removed or clearly marked.

The immediate research frontier is the ordinary GSM 11.11 SIM initialization
sequence after `EF_PHASE`: SIM control reaches `0xe3`, APDUs are exchanged, and
the firmware remains in its periodic DF_GSM `A0 F2` presence cycle with SIM
enable clear. Normalization should make that work cheaper and harder to
misinterpret; it must not turn the historical service-5/SAT chain into a claimed
ordinary-registration dependency.
