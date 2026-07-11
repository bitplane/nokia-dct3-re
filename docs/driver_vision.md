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
| DSP/DSPIF | Shared RAM with boot-oriented placeholders | Build a mailbox peer from observed commands; eventually emulate DSP behavior. |
| SIM | Ring-message/T=0 prototype plus SIMI registers | Consolidate only after organic initialization is understood. |
| Startup/contact/GSM peers | Quarantined firmware hooks | Replace each with a bus, mailbox or device contract, then delete the hook. |

See `mad2_fidelity.md` for register-level implementation status and
`driver_structure.md` for ownership rules.

## Boot profiles

| Profile | Purpose | Acceptance condition |
| --- | --- | --- |
| Default `make verify` | Stable hardware regression | Exact CONTACT SERVICE frame SHA prefix `d8a9a7a58e587be8`. |
| Deeper peer-model run | Exercise mapped startup/SIM contracts | Must be described by enabled models and structural markers; it is not the default oracle. |
| New-ROM baseline | Detect product-specific assumptions | No firmware-address hooks; record first divergence even when no frame renders. |

The default profile is deliberately conservative. A deeper experimental frame
does not supersede it until the responsible peers use ordinary hardware
interfaces.

## Configuration taxonomy

Environment controls must fit one of four classes:

| Class | Examples | Policy |
| --- | --- | --- |
| Machine/scenario configuration | display type, clock rates, battery state, EEPROM image | Keep, but prefer typed machine configuration or input data over individual register values. |
| Faithful peer model | DSP service worker, request-driven SIM card behavior | Keep only while it reacts to an organic request through the real interface. |
| Diagnostic trace/probe | `TRACE_*`, bounded MAD2 ledger | Opt-in, no state changes, and small enough to remove when no longer useful. |
| Shortcut/research hook | firmware-PC trampoline, RAM override, forced event | Quarantine, label explicitly, and delete after the contract is understood. Never use as portability behavior. |

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
