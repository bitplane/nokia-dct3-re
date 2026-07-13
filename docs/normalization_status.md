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

- The census still leaves 37 runtime-built descriptor registrations unresolved.
  These are explicit unknowns, not evidence of an external producer.
- Only the active 3210 frontier has semantic node and edge annotations. Most of
  the mechanically recovered call sites and consumers are not yet promoted to
  reviewed topology.
- The 3330 is a smoke-test and comparative input. Its equivalent node addresses
  and semantic edges have not been mapped, so it is not yet a second supported
  topology profile.
- The deep-GSM trace establishes the current object-population absence but does
  not identify the missing producer or prove that it is external to the ROM.

## Model-boundary debt

The following opt-in paths remain useful, but are not final hardware models:

| Area | Current role | Remaining normalization work |
| --- | --- | --- |
| SIM | `MODEL_SIM_DEVICE` is the stateful register/FIQ path; `MODEL_SIM_CARD` remains as a legacy message-layer comparison harness and `MODEL_SIM_ATR` as a register probe. | Retire the overlapping legacy paths once the stateful device covers their remaining observations. |
| Contact service | `MODEL_SVC_RESPONDER` supplies a known healthy completion through a firmware message bridge. | Move the behavior behind an explicit peer/transport contract and remove the firmware trampoline. |
| Service channel | `MODEL_SVC_CHANNEL_DRAIN` models provisional progress at the channel boundary. | Establish the real producer/consumer contract before treating this as device behavior. |
| DSP | `MODEL_DSP_SERVICE` and `MODEL_DSP_RING_DRAIN` model parts of the missing DSP-side peer. | Consolidate them at a transport boundary backed by request/response evidence. |
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

The immediate research frontier remains organic generic-service object
population and the lower-radio/DSP response path. Normalization should make that
work cheaper and harder to misinterpret; it should not substitute documentation
for resolving the missing machine behavior.
