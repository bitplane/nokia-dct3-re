# Driver vision

The project aims to turn the Nokia phone driver into a composition of reusable
hardware devices. Firmware-specific research hooks are temporary instruments,
not compatibility mechanisms.

## Current architecture

| Subsystem | Current shape | Next architectural step |
| --- | --- | --- |
| ARM7, flash and RAM | MAME CPU/flash devices plus phone-owned maps | Validate decode and reset path with another ROM. |
| PCD8544 display | MAME device | Add other display controllers per product configuration. |
| External EEPROM | MAME `I2C_24C128` on mapped MAD2 GenIO pins plus generated provisioning input | Validate write/timing behavior, legitimate provisioning, ROM-aware fallback extraction, and parallel-window semantics. |
| CCONT | Local `nokia_ccont_device` | Establish physical GENSIO/ADC latency, ready-status timing, RTC encoding, watchdog clock, and board-level ADC signals. Do not assume a conversion-complete IRQ absent hardware evidence. |
| MAD2 | Monolithic phone-owned register handlers | Fill the fidelity ledger before extracting blocks. |
| MBUS | MAD2 register/FIQ approximation | Model a peer only when firmware organically drains the receive task. |
| DSP/DSPIF | `nokia_dsp_peer_device` aggregates shared RAM/DSPIF, rings, calibrated service timing, boot-subset DSP HLE and the separate external-service counterparty | Extend contracts only from organic requests; protect the transport with focused tests before separating the DSP and external peer roles. |
| SIM | Combined MAD2 SIMI controller and stateful card prototype in `nokia_sim_card_device` | Stabilize and test the controller/card seam, then split it; extend card behavior only for organic requests. |
| Startup/service/GSM peers | Request-driven external-service session carried through shared rings and interrupt callbacks, plus observation-only quarantined traces | Validate the transport and peer roles across the coherent 3210 profile and a sibling ROM family without treating their current co-location as hardware topology. |

See `mad2_fidelity.md` for register-level implementation status and
`driver_structure.md` for ownership rules.

## Boot profiles

| Profile | Purpose | Acceptance condition |
| --- | --- | --- |
| Default `make verify` | Stable hardware regression | Exact CONTACT SERVICE frame SHA prefix `d8a9a7a58e587be8`. |
| Coherent frontier | Request-driven external-service peer plus ordinary SIMI/FIQ6 card traffic | `make verify-frontier`; semantic predicates with SIM enabled and task 1 in mode `0x0004`. No deep-profile display frame is currently promoted as an oracle. |
| New-ROM baseline | Detect product-specific assumptions | No firmware-address hooks; record first divergence even when no frame renders. |

The default profile is deliberately conservative. A deeper experimental frame
does not supersede it until the responsible peers use ordinary hardware
interfaces.

## Configuration taxonomy

Environment controls must fit one of five classes:

| Class | Examples | Policy |
| --- | --- | --- |
| Machine/scenario configuration | display type, clock rates, battery state, EEPROM image | Keep, but prefer typed machine configuration or input data over individual register values. |
| Device-boundary model | CCONT, request-driven SIM card behavior, DSP ring ownership | Keep only while it reacts to organic traffic through the real interface. |
| Diagnostic trace/probe | `TRACE_*`, bounded MAD2 ledger | Opt-in, no state changes, and small enough to remove when no longer useful. |
| Provisional firmware bridge | none retained in the supported profiles | Do not reintroduce firmware calls, result substitution, or message injection. |
| Shortcut/force | RAM-result override, forced event or task message | Add no new supported behavior in this class. Existing named shortcuts remain quarantined debt; a new diagnostic force must be bounded to one concrete question and then deleted. |

The number of variables alone is not a sufficient debt metric. A display
variant and a firmware-state poke are not equivalent. The useful measures are:

- firmware-PC conditions remaining in execution paths;
- direct firmware RAM/register state rewrites;
- device behaviors lacking a documented hardware cause; and
- subsystem contracts tested against only one ROM.

## Modularization order

1. Improve CCONT and GENSIO from observed transactions.
2. Document and stabilize MAD2 timers, interrupt aggregation and serial select
   behavior.
3. Resume the ordinary unattended UI/idle-window entrance investigation without
   presuming that the missing transition is hardware-owned.
4. Add focused transport tests, then split the combined SIM and DSP/external
   peer implementations along their established ownership boundaries.
5. Use the 3330 as the first portability probe before treating MAD2 behavior as
   common DCT3 hardware.

MAD2 should not be extracted by copying the current switch statement into a
device. Each block needs reset semantics, callbacks, focused tests and freedom
from firmware-address conditions first.

## Engineering rules

- Model hardware and nonvolatile data; do not model desired firmware results.
- Product differences belong in machine configuration or input data.
- Hardware components emit signals; firmware owns RTOS and application state.
- Keep the default oracle byte-exact through refactors.
- Record useful negative conclusions, but remove chronological experiment logs.
- Require a second-ROM confidence pass before calling shared behavior validated.
