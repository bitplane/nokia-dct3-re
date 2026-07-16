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
| CCONT/GENSIO | Separate `nokia_ccont_device` and `nokia_gensio_device`; organic two-ROM phase/status/SELECT regression | Establish physical GENSIO/ADC latency, RTC encoding, watchdog clock, board-level ADC signals and SELECT peers. Do not assume a conversion-complete IRQ absent hardware evidence. |
| MAD2 | `nokia_mad2_device` owns the CTSI core, timers, interrupt controller and CPU routing; board/peripheral windows remain phone-owned or extracted separately | Recover physical clocks and reset domains; move further windows only after their individual contracts pass the same gate. |
| MBUS | Extracted `nokia_mbus_device` with RX/TX byte attachment and FIQ2/FIQ3 callbacks; no default peer | Recover physical baud/FIQ3 timing and attach a peer only when firmware organically transmits a supported frame. |
| DSP/DSPIF | `nokia_dspif_device` owns shared RAM, DSPIF, packet rings and interrupt-facing completion; `nokia_dsp_hle_device` owns the boot-subset DSP responses | Recover bootstrap publication transitions, physical timing and wider DSP vocabulary only from organic traffic. |
| SIM | Separate `nokia_simi_device` controller and `nokia_sim_card_device` protocol/profile connected by reset/byte callbacks | Stabilize timing/error/removal behavior, then extract reusable provisioning profiles; extend card behavior only for organic requests. |
| Startup/service/GSM peers | `nokia_external_service_peer_device` owns the request-driven class-`0x40` session carried through the DSP transport | Validate the logical peer against another ROM family without treating its transport as peer ownership. |

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
| Machine/scenario configuration | display profile, clock rates, battery state, EEPROM image | Keep, but prefer typed machine configuration or input data over individual register values. |
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

1. Improve the extracted CCONT and GENSIO devices only from observed transactions.
2. Extend the extracted MAD2 core only from observed reset, clock and peripheral
   contracts; timer 1 remains an explicitly unexercised placeholder.
3. Resume the ordinary unattended UI/idle-window entrance investigation without
   presuming that the missing transition is hardware-owned.
4. Extend focused DSPIF transport tests to wrap/full/fault cases and replace
   calibrated HLE scheduling only when peer timing is recovered.
5. Use the 3330 as the first portability probe before treating MAD2 behavior as
   common DCT3 hardware.

The CTSI core met the extraction gate without moving unresolved peripheral
windows. Each additional MAD2 block still needs reset semantics, callbacks,
focused tests and freedom from firmware-address conditions first.

## Engineering rules

- Model hardware and nonvolatile data; do not model desired firmware results.
- Product differences belong in machine configuration or input data.
- Hardware components emit signals; firmware owns RTOS and application state.
- Keep the default oracle byte-exact through refactors.
- Record useful negative conclusions, but remove chronological experiment logs.
- Require a second-ROM confidence pass before calling shared behavior validated.
