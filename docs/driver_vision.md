# Driver vision

The project aims to turn the Nokia phone driver into a composition of reusable
hardware devices. Firmware-specific research hooks are temporary instruments,
not compatibility mechanisms.

## Current architecture

| Subsystem | Current shape | Next architectural step |
| --- | --- | --- |
| ARM7, flash and RAM | MAME CPU/flash devices plus phone-owned maps | Validate decode and reset path with another ROM. |
| PCD8544 display | MAME device | Add other display controllers per product configuration. |
| External EEPROM | MAME `I2C_24C128` on MAD2 GenIO | Validate real provisioning images and parallel-window semantics. |
| CCONT | Local `nokia_ccont_device` | Establish GENSIO transaction state and ADC completion IRQ behavior. |
| MAD2 | Monolithic phone-owned register handlers | Fill the fidelity ledger before extracting blocks. |
| MBUS | MAD2 register/FIQ approximation | Model a peer only when firmware organically drains the receive task. |
| DSP/DSPIF | `nokia_dsp_peer_device` owns shared RAM, rings, service timing and observed peer replies | Extend the mailbox contract only from organic requests; eventually emulate more DSP behavior. |
| SIM | Stateful `nokia_sim_card_device` on SIMI registers/FIQ6 | Extend the device only for organically requested card behavior. |
| Startup/contact/GSM peers | Request-driven contact session in the DSP peer plus quarantined traces | Validate the extracted contract across the coherent 3210 profile and a sibling ROM family. |

See `mad2_fidelity.md` for register-level implementation status and
`driver_structure.md` for ownership rules.

## Boot profiles

| Profile | Purpose | Acceptance condition |
| --- | --- | --- |
| Default `make verify` | Stable hardware regression | Exact CONTACT SERVICE frame SHA prefix `d8a9a7a58e587be8`. |
| Coherent frontier | Request-driven contact peer plus ordinary SIMI/FIQ6 card traffic | `make verify-frontier`; exact Security-code frame plus semantic structural predicates with SIM enabled; keypad delivery remains startup-owned in mode `0x0004`. |
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
3. Resume boot investigation at the first organic unanswered hardware request.
4. Consolidate SIM and DSP peers only when their ordinary transport boundaries
   are demonstrated in one coherent boot.
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
