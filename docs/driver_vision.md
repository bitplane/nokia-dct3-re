# Driver vision

The project aims to turn the Nokia phone driver into a composition of reusable
hardware devices. Firmware-specific research hooks are temporary instruments,
not compatibility mechanisms.

## Current architecture

| Subsystem | Current shape | Next architectural step |
| --- | --- | --- |
| ARM7, flash and RAM | MAME CPU/flash devices plus product-owned maps; reset and flash behavior are exercised across four products | Validate further address-decode and retention behavior when another product diverges. |
| PCD8544-family display | MAME device with product geometry and viewport configuration | Add a separate controller only when a recovered command set falls outside this family. |
| External EEPROM | MAME `I2C_24C128` on mapped MAD2 GenIO pins plus generated provisioning input | Validate write/timing behavior, legitimate provisioning, ROM-aware fallback extraction, and parallel-window semantics. |
| CCONT/GENSIO | Separate `nokia_ccont_device` and `nokia_gensio_device`; organic two-ROM phase/status/SELECT regression; deterministic binary RTC and enabled documented watchdog/WDDISX boundary | Establish physical GENSIO/ADC latency, board-level ADC signals and SELECT peers. Do not assume a conversion-complete IRQ absent hardware evidence. |
| MAD2 | `nokia_mad2_device` owns the CTSI core, timers, interrupt controller, CPU routing, one-shot ARM clock-stop/routed-wake behavior and the established digital-baseband reset domain; board/peripheral windows remain phone-owned or extracted separately | Recover the exact sleep-clock divider tree, transition latency, FIQ8 source and rail sequencing; move further windows only after their individual contracts pass the same gate. |
| MBUS | Extracted `nokia_mbus_device` with 9,600-baud RX/TX byte attachment and FIQ2/FIQ3 callbacks; no default peer | Recover FIQ3 phase/source and attach a peer only when firmware organically transmits a supported frame. |
| DSP/DSPIF | `nokia_dspif_device` owns shared RAM, packet rings and interrupt-facing completion; `nokia_dsp_hle_device` owns cross-ROM bootstrap and service timing. Two HLE audio voices expose firmware-programmed COBBA oscillator words while no DSP codec backend exists. | Recover physical response timing, codec waveform/amplitude behavior and wider DSP vocabulary only from organic traffic. |
| SIM | Separate `nokia_simi_device` controller using the default rate retained by firmware PPS `ff 00 ff`, and a callback-connected `nokia_sim_card_device` with declared files and persistent mutable records | Generalize timing beyond the fixed ATR/PPS profile; recover ATR-start/turnaround and error/removal behavior, then extract reusable provisioning profiles and broaden GSM 11.11 conformance. |
| Buzzer | MZT-03C represented by a MAME beeper driven from MAD2 PUP enable and the 13 MHz divider; mapped-MMIO and organic user-alarm lifecycles are regression-tested | Recover volume/acoustic response. DSP/COBBA tones are modeled separately. |
| Vibrator | Optional vibra battery pack exposed as a named MAME output from the PUP enable gate; the separate control latch is retained | Exercise an organic alert/call path and recover the frequency/mode encoding. |
| Backlight | Unmodeled; the service manual places separate LCD/key-light outputs behind COBBA and the UI-Switch, while paired-ROM traces falsify a direct GenIO-bit-6 model | Recover the firmware command at the MAD2-to-COBBA boundary before adding illumination outputs. |
| Startup/service/GSM peers | `nokia_external_service_peer_device` owns request-driven class-`0x40` service sessions; `nokia_radio_peer_device` owns Nokia L1 transaction correlation; `nokia_gsm_network_device` owns standards-shaped cell, RR and MM data | Validate each logical peer against another ROM family without treating DSPIF transport as peer ownership. |

See `mad2_fidelity.md` for register-level implementation status and
`driver_structure.md` for ownership rules.

## Boot profiles

| Profile | Purpose | Acceptance condition |
| --- | --- | --- |
| 3210 machine default | Request-driven external-service peer plus ordinary SIMI/FIQ6 card traffic | `make verify-frontier`; semantic predicates with SIM enabled and task 1 in mode `0x0004`. |
| Missing-hardware negative baseline (`make verify`) | Preserve a known failure comparison | Semantic startup predicates; no required LCD frame. |
| New-ROM baseline | Detect product-specific assumptions | No firmware-address hooks; record first divergence even when no frame renders. |

The Nokia 3210 v6.00/v5.01, 3310 v6.39, 3330 v4.50, and 3410 v5.46 profiles
have forcing-free acceptance gates. Their shared composition is evidence for
reuse, not proof that every DCT3 product has identical peripherals or peer
contracts. Unvalidated products retain conservative defaults until their first
divergence is classified.

## Configuration taxonomy

Runtime controls fit one of five classes:

| Class | Examples | Policy |
| --- | --- | --- |
| Product configuration | display geometry, clock rates, ADC tuple, flash capabilities, peer composition | Typed `nokia_product_config` or device setters; no production environment lookup. |
| Device-boundary model | CCONT, request-driven SIM card behavior, DSP ring ownership | Keep only while it reacts to organic traffic through the real interface. |
| Diagnostic trace/probe | MAME log masks, bounded MAD2 ledger | Read-only, no state changes, and small enough to remove when no longer useful. |
| Provisional firmware bridge | none retained in the supported profiles | Do not reintroduce firmware calls, result substitution, or message injection. |
| External fixture | Lua key, charger, RTC, MBUS, capture, or MMIO-conformance action | Test orchestration only; never production device configuration or firmware-state mutation. |

The number of variables alone is not a sufficient debt metric. A display
variant and a firmware-state poke are not equivalent. The useful measures are:

- firmware-PC conditions remaining in execution paths;
- direct firmware RAM/register state rewrites;
- device behaviors lacking a documented hardware cause; and
- subsystem contracts tested against only one ROM.

## Remaining architecture work

1. Keep `nokia_dspif_device` transport-only and preserve the single swappable
   DSP-backend seam. Move no radio, service, bootstrap, or audio semantics down
   into the transport.
2. Move the 3410 B3 adapter's partitioned read-while-write and erase-suspend
   behavior into MAME's generic flash core when that core can express it.
3. Recover MAD2's remaining divider, transition, extended-interrupt, and rail
   contracts before extracting additional uncertain register windows.
4. Replace calibrated HLE timing only with measured hardware behavior or
   cross-ROM protocol evidence; declared calibration is preferable to invented
   precision.
5. Extend product coverage through typed profiles and device data, never
   firmware-PC compatibility branches.

## Engineering rules

- Model hardware and nonvolatile data; do not model desired firmware results.
- Product differences belong in machine configuration or input data.
- Hardware components emit signals; firmware owns RTOS and application state.
- Keep both the machine-default semantic oracle and missing-hardware fault oracle stable through refactors.
- Record useful negative conclusions, but remove chronological experiment logs.
- Require a second-ROM confidence pass before calling shared behavior validated.
