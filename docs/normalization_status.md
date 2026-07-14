# Normalization status

This document identifies which conclusions have entered the repeatable evidence
regime and which implementation areas remain provisional. It is not a claim
that the driver is ready for upstream submission.

## Normalized acceptance profiles

- `make verify` protects the default CONTACT SERVICE frame and structural
  summary.
- `make verify-frontier` protects the request-driven contact peer plus ordinary
  SIMI/FIQ6 card path. It ends in task-1 mode `0x0004`, flags `0x0f`, contact
  status `0x49`, no-SIM clear, and SIM enable set.
- `run-manifest-contact` records the contact-service command directions.
- `run-manifest-deep-gsm` records the coherent generic-service/SIM frontier.
- `smoke-3330e` is the first cross-ROM portability probe.

The historical responder/drain and startup-report bridge profile has been
retired. Its direct message trampoline, service-state RAM completion, synthetic
report sequence, Make targets, and structural oracle are no longer supported.
The undocumented contact-ready write filter and pre-device `SIM_PROFILE`
register harness are also removed.

## Current frontier

The ordinary SIM initialization contract is satisfied. With provisioned phone
identity data, firmware paints an idle-like `Menu` frame, but task 1 remains in
startup mode `0x0004`.

The keypad lifecycle is bounded:

```text
IRQ6 -> ISR 0x2b5da0 -> event 0x72 -> mailbox/task 1
     -> mode-4 fallback 0x2701b0 -> no matrix decode
```

Report code `0x07` is an actual prerequisite for this 3210 v6.00 branch. Its
four reporter callers are classified. No ordinary external condition selecting
one of them has yet been proved. The next static control is the same-product
3210 v5.01 task-1 branch and caller alignment.

## Evidence coverage

- Hardware contracts, state predicates, falsifications, topology nodes, and
  runtime-pattern scope are machine validated by `make evidence-check`.
- The message census includes reviewed producers/consumers, descriptor-derived
  edges, positive fixtures, and negative fixtures for known decode traps.
- Contact-service, SIM, DSP-ring, and task-mailbox conclusions have named
  runtime manifests.
- The 3330 remains a smoke input rather than a second supported semantic
  topology.
- The 3210 v5.01 full flash is a local static control; its code-7 wrapper and
  four callers are mapped, but its runtime profile is not yet normalized.

## Device and model debt

| Area | Current role | Remaining work |
| --- | --- | --- |
| SIM | `nokia_sim_card_device` owns stateful SIMI/FIQ6 transport and requested GSM 11.11 files. | Extend only for organically requested commands; validate FIFO flags. |
| CCONT | `nokia_ccont_device` owns serial registers, ADC values, RTC, watchdog, power and IRQ output. | Complete GENSIO transaction/reset semantics and validate ADC IRQ behavior. |
| EEPROM | Native MAME `I2C_24C128` plus generated profile. | Decode remaining security/product fields and parallel alias behavior. |
| DSP/contact | `nokia_dsp_peer_device` owns shared RAM, ring state, service timing, and the request-driven contact peer. | Stabilize and extend the wider mailbox contract from observed requests; validate it on a sibling ROM family. |
| MAD2 | Phone-owned timers, interrupt aggregation, GENSIO, SIMI window and DSP control registers. | Improve reset/decode fidelity before extracting shared blocks. |
| Display | Native PCD8544 plus firmware-selected display-type read shortcut. | Recover the real product-data source and remove the shortcut. |

The headless LCD mirror and delayed-key fixture are acceptance tooling in
`mame_noki3210_input_exerciser.lua`. They do not add device state or firmware
shortcuts to the phone driver.

Two RAM-read shortcuts remain explicitly quarantined: display-type selection and
the startup event-14 latch. Neither is presented as hardware behavior.

## Instrumentation debt

The retained trace switches are scoped as follows:

| Trace | Purpose |
| --- | --- |
| `TRACE_HANDOFF` | task-1 mode, report-7 surface, event-`0x72`, scan/decode seam |
| `TRACE_TASKS` | generic task liveness and mailbox edges |
| `TRACE_CSCMD` | contact-service command direction and state |
| `TRACE_SIM_RX` | SIMI/FIQ/APDU lifecycle |
| `TRACE_GSM_SERVICE` | manifest-backed generic-service registrations/callbacks |
| `TRACE_DSP_BOUNDARY` | shared-ring requests and request-derived peer responses |
| `TRACE_GENSIO` | serial register transactions |
| `TRACE_MAD2_LEDGER` | first-access MAD2 register census |

The 60-block `TRACE_GSM_LOWER`, `TRACE_MMIVM`, and `TRACE_TASK5_REG` research
weaves have been removed. Their durable conclusions remain in subsystem docs
and evidence ledgers.

## Topology gaps

- Most mechanically recovered ROM edges are not yet promoted to reviewed
  semantic identities.
- Dynamic generic-service descriptors are mapped far enough to exclude the
  previously suspected registration paths, but the wider steady-state service
  topology remains incomplete.
- The DSP/contact peer contract is proved only for the requests exercised by
  the current boot.
- The organic owner of the 3210 report-7 completion remains unresolved.

## Completion gate

A subsystem is boxed off only when it has:

1. an explicit hardware or transport boundary;
2. symbolic topology identities rather than address-only prose;
3. a repeatable structural or behavioral acceptance profile;
4. no firmware-result forcing, task-message injection, or direct state writes;
5. stale probes and contradicted narratives removed; and
6. a second-ROM confidence result where the behavior is claimed as shared.
