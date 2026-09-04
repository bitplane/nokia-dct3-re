# Driver vision

The project aims to turn the Nokia phone driver into a composition of reusable
hardware devices. Firmware-specific research hooks are temporary instruments,
not compatibility mechanisms.

The target upstream composition and the non-negotiable DSP transport/backend
boundary are defined in `upstream_architecture.md`. In particular, the current
DSP HLE is a compatibility backend, not the permanent owner of DSP silicon.

## Current architecture

| Subsystem | Current shape | Next architectural step |
| --- | --- | --- |
| ARM7, flash and RAM | MAME CPU/flash devices plus product-owned maps. An explicit reset-vector HLE represents the undumped MAD2 mask ROM's branch to flash without forcing CPU state; reset and flash behavior are exercised across four products. | Obtain and identify real mask ROMs or recover their assumed exit state; validate further address-decode and retention behavior when another product diverges. |
| PCD8544-family display | MAME device with one validated product contract applying controller geometry and viewport configuration atomically | Add a separate controller only when a recovered command set falls outside this family. |
| External EEPROM | Product-owned MAME I2C parts on mapped MAD2 PUP pins, with opt-in self-timed write-cycle behavior and mapped-pin page/persistence coverage; no parallel EEPROMSelX alias | Validate exact board timing and legitimate provisioning, and make fallback extraction ROM-aware. |
| CCONT/GENSIO | Separate `nokia_ccont_device` and `nokia_gensio_device`; organic two-ROM phase/status/SELECT regression; deterministic binary RTC and enabled documented watchdog/WDDISX boundary. Physical short/long power-key gates distinguish retained frame-sampled RAM from CCONT's digital-baseband rail state and survive pre-key save/load. | Establish physical GENSIO/ADC latency, board-level ADC signals and SELECT peers. Do not assume a conversion-complete IRQ absent hardware evidence. |
| MAD2 | `nokia_mad2_device` owns the CTSI core, timers, 100 Hz FIQ8 centisecond source, interrupt controller, CPU routing, read-only external-status inputs, one-shot ARM clock-stop/routed-wake behavior, the established digital-baseband reset domain and an atomic product DSP-reset wiring contract; board/peripheral windows remain phone-owned or extracted separately | Recover the exact sleep-clock divider tree, external pin ownership, transition latency and rail sequencing; observe FIQ8 across an organic low-power interval before generalizing its sleep-domain behavior. |
| KBGPIO/PUP/UIF | Separate devices own the sparse keypad, output/GenIO and UIF register families. KBGPIO raises IRQ0 for any unmasked column change and firmware masks row-drive transitions. PUP decodes the vibrator mode and parameter fields. UIF remains a neutral saved latch bank. | Confirm keypad debounce, vibrator electrical waveforms, physical buzzer volume response and UIF pin nets without assigning semantics to retained neighboring latches. |
| MBUS | Extracted `nokia_mbus_device` with 9,600-baud RX/TX byte attachment and FIQ2/FIQ3 callbacks; no default peer | Recover FIQ3 phase/source and attach a peer only when firmware organically transmits a supported frame. |
| DSP/DSPIF/audio | `nokia_dspif_device` owns shared RAM, packet rings and interrupt-facing completion. NSE-8, NHM-5, NHM-6 and NHM-2 independently prove command-`0x08` speech (`0x0201`) through organic calls; separately named product contracts retain those evidence identities even where values match. NSE-3 retains only its evidenced decoder without an invented media-enable predicate. GSM-FR frames cross a separate 20 ms codec boundary, then a distinct MAD2 device carries full-duplex PCM over product-configured COBBA-clocked four-wire interfaces. NSE-8 uses the documented 520 kHz/8 kHz, 65-clock frame; NHM-5, NHM-6 and NHM-2 independently select the documented COBBA-GJP 1 MHz/8 kHz, 125-clock shape. All use one sync clock and a 16-bit MSB-first sign-extended 13-bit converter word sampled on falling edges. COBBA owns analogue conversion. Internal-media gates prove non-silent bidirectional GSM-FR on all four products; isolated host microphone/EAR acceptance exists only where board routing is evidenced. | Recover COBBA's DSP-port register meanings and firmware-controlled analogue-mux/gain contract. Identify NHM-6/NHM-2 fitted analogue routes before physical-duplex promotion. Implement A5 only at a real ciphered-bitstream boundary. |
| SIM | Separate `nokia_simi_device` controller using the default rate retained by firmware PPS `ff 00 ff`, and a callback-connected `nokia_sim_card_device` with declared files plus persistent ADN, SMS and SMS-parameter records | Generalize timing beyond the fixed ATR/PPS profile; recover ATR-start/turnaround and error/removal behavior, then extract reusable provisioning profiles and broaden GSM 11.11 conformance. |
| Buzzer | MZT-03C represented by a MAME beeper driven through the extracted PUP enable and 13 MHz divider; mapped-MMIO and organic user-alarm lifecycles are regression-tested | Recover volume/acoustic response. DSP/COBBA tones are modeled separately. |
| Vibrator | Optional vibra battery pack exposed as a named MAME output from the extracted PUP enable gate; the separate control latch is retained | Exercise an organic alert/call path and recover the frequency/mode encoding. |
| Backlight | Unmodeled; the service manual places separate LCD/key-light outputs behind COBBA and the UI-Switch, while paired-ROM traces falsify a direct GenIO-bit-6 model | Recover the firmware command at the MAD2-to-COBBA boundary before adding illumination outputs. |
| Startup/service/GSM peers | `nokia_external_service_peer_device` owns request-driven class-`0x40` service sessions; `nokia_radio_peer_device` owns Nokia L1 correlation, TDMA timing, per-cell measurement/acquisition state and the directional A5 burst seam behind one immutable protocol contract. Generic `gsm_xcch_l1` carries LAPDm through 184/456-bit coding and four SDCCH bursts. `nokia_lapdm_link_device` owns decoded bidirectional SAPI-0/SAPI-3 establishment, sequence and release state; `gsm_sms_transport` validates GSM 04.11 CP/RP messages against transaction data actually sent; `nokia_gsm_session_device` owns registration, paging, saved authentication/cipher activation, deterministic mobile-terminated/mobile-originated calls and bounded SMS Layer-3 state. `nokia_gsm_voice_peer_device` and the optional WebSocket adapter remain outside CC/RR and air L1; a standalone endpoint proves organic call acceptance, bidirectional GSM-FR and local/remote release without owning the MAME process. `nokia_gsm_network_device` owns standards-shaped multi-cell topology, SI1--SI4/control-channel data, RF availability, typed A5/0/A5/1, SI3 T3212 policy and typed SMS/Smart Message input compositions. Ordinary MT text now proves pre-page envelope validation, complete CP/RP/RR closure, firmware/SIM inbox ownership, physical read/delete, capacity, replay and two-message isolation on NSE-8, with independent NHM-5/NHM-2 application gates and an honest NHM-6 transport frontier. Organic same-/different-LAC reselection, loss/recovery, replacement-cell paging and independently closed two-part Smart Message transport pass across four products while firmware retains ranking, selection, MM, multipart retention, application dispatch and RTPL parsing ownership. NSE-8 proves RAM-owned reassembly and physical playback; NHM-5/NHM-2 independently corroborate playback and durable received-tone storage. A5/2 is rejected pending evidence. Shared behavior consumes typed contracts rather than handset branches. | Add an external incoming-call submission contract, then implement a SIP/RTP backend outside the handset and GSM state devices. Recover mobile-originated SMS, service-centre configuration and delivery reports; keep dedicated-mode handover separate until independently evidenced. |

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
3. Recover MAD2's remaining divider, transition, ninth-IRQ ownership, and rail
   contracts before extracting additional uncertain register windows.
4. Replace calibrated HLE timing only with measured hardware behavior or
   cross-ROM protocol evidence; declared calibration is preferable to invented
   precision.
5. Extend product coverage through typed profiles and device data, never
   firmware-PC compatibility branches.

The radio protocol is configured atomically. A product selects one complete
contract containing its acquisition strategy, channel-confirmation policy,
traffic-release parameter, assigned-link continuation and MM pacing. Partial
wire/acquisition combinations are not representable; a product with only a
static wire observation remains disabled until its acquisition lifecycle is
proved.

## Engineering rules

- Model hardware and nonvolatile data; do not model desired firmware results.
- Product differences belong in machine configuration or input data.
- Hardware components emit signals; firmware owns RTOS and application state.
- Keep both the machine-default semantic oracle and missing-hardware fault oracle stable through refactors.
- Record useful negative conclusions, but remove chronological experiment logs.
- Require a second-ROM confidence pass before calling shared behavior validated.
