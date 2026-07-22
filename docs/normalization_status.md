# Normalization status

This document identifies which conclusions have entered the repeatable evidence
regime and which implementation areas remain provisional. It is not a claim
that the driver is ready for upstream submission.

## Normalized acceptance profiles

- `make verify` protects the explicit missing-hardware structural summary; it
  does not require a particular failure screen.
- `make verify-frontier` protects the request-driven external-service peer plus ordinary
  SIMI/FIQ6 card path. It ends in task-1 mode `0x0004`, flags `0x0f`, service-session
  status `0x49`, no-SIM clear, and SIM enable set.
- `run-manifest-service` records the class-`0x40` service command directions.
- `run-manifest-deep-gsm` records the coherent generic-service/SIM frontier.
- `smoke-3330e` is the first cross-ROM portability probe.

## Current application baseline

The ordinary SIM initialization contract is satisfied. With provisioned phone
identity data, firmware paints the idle `Menu` frame and a physical left-softkey
press opens the `Phone book` menu organically. Task 1 remains in mode `0x0004`,
which is now validated as compatible with the interactive UI rather than a
blocked startup state.

The keypad lifecycle is bounded and its MAD2 register contract is aligned in
3210 v6.00 and v5.01:

```text
IRQ0 -> ISR 0x2b3084 -> task-1 event 0x41
     -> firmware 0x41/0x42/0x43 sequence -> matrix scan 0x2b2f90
     -> key decode 0x2b4628 -> resource 0x6e02
```

Mode 4 already runs keypad and security-editor interaction. Report code 7,
callbacks `0x5d`, `0x01`, and `0x10`, and task-6 selector `0x0732` belong to
conditional navigation, reinitialization, or shutdown lifecycles. They are not
missing cold-boot entrances; MMI settlement is closed.

The active frontier is now application coverage: deterministic menu traversal,
user-data persistence, audio, and built-in applications. Missing behavior should
be investigated only when an organic application path reaches its boundary.

## Evidence coverage

- Hardware contracts, state predicates, falsifications, topology nodes, and
  runtime-pattern scope are machine validated by `make evidence-check`.
- The message census includes reviewed producers/consumers, descriptor-derived
  edges, positive fixtures, and negative fixtures for known decode traps.
- Class-`0x40` service, SIM, DSP-ring, and task-mailbox conclusions have named
  runtime manifests.
- The 3330 remains a smoke input rather than a second supported semantic
  topology.
- The 3210 v5.01 full flash is a same-product runtime control. Its structural
  oracle independently reaches mode `0x0004` with flags `0x0f`, clears no-SIM,
  and sets SIM ENABLE through relocated v5 state fields. Its provisioned menu
  oracle matches v6.00 exactly outside the shared animated-icon mask. Both
  revisions now retain coherent service status `0x0049`.

## Device and model debt

| Area | Current role | Remaining work |
| --- | --- | --- |
| SIM | `nokia_simi_device` owns MAD2 registers/FIFOs/IIR, the default character rate retained by firmware PPS `ff 00 ff`, and FIQ6. The callback-connected card owns T=0, declared file metadata and persistent mutable ADN records. | Generalize timing beyond the current fixed ATR/PPS profile; recover ATR-start/turnaround and error/removal contracts; extract reusable subscriber profiles and extend commands only for organic requests or focused conformance tests. |
| CCONT/GENSIO | `nokia_ccont_device` owns selected-device registers and outputs, a deterministic binary RTC, recovered periodic IRQ sources, the documented watchdog counter and its WDDISX pin, plus retained power state and charger-originated reset cause. `nokia_gensio_device` owns serial selection/status, LCD pins and SELECT latches. Both ROMs service the watchdog and reproduce powered-off charger restart. | Obtain board-level selector names, analog units, physical rail timing and SELECT peer identities; model battery/current evolution only from an exercised consumer. |
| EEPROM | Native MAME `I2C_24C128` on mapped GenIO pins plus a v6.00-oriented generated provisioning fixture; the parallel window is unproved. | Decode remaining fields, validate writes/timing and another product's storage placement, and make fallback-record extraction ROM-aware. |
| DSP/network/external-service | `nokia_dspif_device` owns shared RAM/DSPIF, rings and interrupt-facing completion; `nokia_dsp_hle_device` owns the cross-ROM bootstrap and service transport; `nokia_radio_peer_device` owns Nokia L1 packet correlation; `nokia_gsm_network_device` owns standards-shaped laboratory-cell, RR and MM data; `nokia_external_service_peer_device` owns the request-driven service/test session. Firmware-programmed COBBA oscillator words `0x0ae/0x0b0/0x0b6` drive separate HLE audio voices. | Recover physical bootstrap timing and the DSP-internal PLMN-measurement lifecycle; extend the GSM peer only from an evidenced Nokia L1 entrance; replace square-wave tone HLE with codec-backed behavior when evidenced. |
| MAD2 | `nokia_mad2_device` owns CTSI offsets `0x00..0x16`, timer-0/FIQ4, 15-bit timer-1/destination/FIQ5, reset request/cause, pending/masks, IRQ/FIQ aggregation, one-shot ARM clock-stop/routed wake and save-state restoration. SIMI clock bit 5 has a controller side effect; SIMI, MBUS and GENSIO are separate devices. | Establish extended IRQ ownership, exact physical timer dividers and transition latency, remaining clock-gate consumers and rail timing. |
| MBUS | `nokia_mbus_device` owns PUP offsets `0x18..0x1a`, receive/transmit holding state, 9,600-baud character timing and FIQ2/FIQ3 callbacks; ordinary v5.01/v6.00 boot initializes receive mode but transmits nothing. | Recover FIQ3 phase/source and collision/error behavior; attach a counterparty only for organic frames. |
| Display/input | Native PCD8544, a cross-ROM LCD transport check, and a cross-ROM-derived MAD2 IRQ0 matrix contract; the Lua mirror and key timing are acceptance fixtures. The generated EEPROM supplies the one recovered descriptor-`0x0749` field through normal NV loading; an erased-profile control proves firmware has no usable fallback. | Obtain a complete authentic `0x0749` profile to replace the minimal synthetic field; recover reset timing and mask/debounce edge cases separately. |
| Buzzer | MAD2 PUP bit 5 gates a MAME beeper; a mapped-MMIO conformance fixture validates the two-byte `13 MHz / divider` clock, enable edge and disable edge. The organic user-alarm lifecycle programs CCONT, consumes its IRQ and drives changing divider/volume values through this output. | Recover the physical volume/acoustic transfer. Ringing-tone preview and DSP/COBBA audio remain separate paths. Vibrator and backlight remain separate gaps. |
| Vibrator/backlight | PUP bit 4 drives a named MAME `vibration` output while `0x1b` stores its independent control byte; a mapped-MMIO fixture validates the gate. Enabling `Vibrating alert` through the firmware UI and ringing an organic RTC alarm leaves both registers inactive, constraining that setting to another alert lifecycle rather than proving an output defect. The 3210 service manual establishes that COBBA drives separate LCD/key-light signals into the UI-Switch. Paired-ROM MAD2 traces and a complete changed-write census of MCU-visible DSP shared RAM find no key/timeout-specific output command. | Exercise vibra organically from an incoming-call lifecycle and decode `0x1b`. Recover the lower DSP/COBBA light-control surface before exposing backlight outputs. |

The headless LCD mirror and delayed-key fixture are acceptance tooling in
`mame_noki3210_input_exerciser.lua`. They do not add device state or firmware
shortcuts to the phone driver.

The synthetic EEPROM provisions the single recovered display-profile field at
ROM-described descriptor `0x0749`; firmware loads and copies it organically.
Unknown bytes remain erased because no complete factory record is available.

## Runtime-control ledger

Every live `NOKI3210_*` control belongs to one of these classes:

| Class | Controls | Status |
| --- | --- | --- |
| Hardware scenarios | `ADC_PROFILE`, `ADC0..7`, `CHARGER_ADC`, `SIM_ATR_HEX`, `SIM_CPHS_AOC`, `SIM_CACHED_LOCATION` | Deterministic analog/card inputs, not inferred physical defaults. Charger attachment is a typed CCONT input; `CHARGER_ADC` remains its raw selector-5 VCHAR level for threshold fixtures. Raw `ADC0..7` controls remain laboratory provisioning, not a battery simulation. `SIM_CACHED_LOCATION` supplies persistent EF_LOCI state for registration scenarios. |
| Timing calibration | `TIMER0_HZ`, `TIMER1_HZ`, `FIQ8_HZ`, `TIMER0_CATCHUP`, `MODEL_DSP_SERVICE_DELAY_MS`, `MODEL_DSP_SERVICE_TICK_MS` | The 3210 defaults Timer 0 to 33,055 Hz and Timer 1 to 1,057 Hz. Paired ROMs prove the post-divider 8:1 conversion but not the exact physical divider tree. `MODEL_DSP_SERVICE_DELAY_MS` controls the one-shot shared-service completion delay; the legacy `TICK_MS` name controls peer packet polling only. |
| Hardware scenarios | `CCONT_READY`, `CCONT_WDDISX_GROUNDED` | Reset-time CCONT readiness and the documented physical watchdog-disable pin. The 3210 profile defaults to ready with WDDISX released. |
| Device-boundary prototypes | `MODEL_DSP_SERVICE`, `MODEL_EXTERNAL_SERVICE_PEER`, `MODEL_SIM_DEVICE`, `MODEL_RADIO_PEER` | The first three are enabled by the 3210 product profile; overrides remain for negative tests. `MODEL_RADIO_PEER` is an opt-in deterministic network whose search, camp, Location Updating, release and operator-presentation contract is verified through DSPIF/FIQ0 and organic firmware behavior. |
| Read-only diagnostics | `TRACE_DISPLAY`, `TRACE_DISPLAY_PROFILE`, `TRACE_DISPLAY_IO`, `TRACE_TASKS`, `TRACE_SERVICE_COMMAND`, `TRACE_SIM_RX`, `TRACE_GSM_SERVICE`, `TRACE_DSP_BOUNDARY`, `TRACE_DSP_SHARED_READS`, `TRACE_DSP_SHARED_TRANSITIONS`, `TRACE_GENSIO`, `TRACE_GENSIO_LIMIT`, `TRACE_CCONT_WATCHDOG`, `TRACE_CCONT_ADC`, `TRACE_CCONT_RTC`, `TRACE_MAD2_LEDGER`, `TRACE_MAD2_TIMERS`, `TRACE_MAD2_INTERRUPTS`, `TRACE_MAD2_CLOCKS`, `TRACE_MBUS`, `TRACE_BUZZER`, `TRACE_PUP_OUTPUTS` | Log-only and bounded or scoped to a named investigation. `TRACE_GENSIO_LIMIT` changes only the default 20,000-line diagnostic ceiling. |
| Harness/output controls | `SNAPSHOT_DIR`, `BOOT_SUMMARY`, `LUA_QUIET`, `POST_READY_KEY`, `POST_READY_KEYS`, `POST_READY_KEY_DELAY_MS`, `POST_READY_KEY_DURATION_MS`, `POST_READY_KEY_GAP_MS`, `POST_READY_KEY_PERIOD_MS`, `POST_READY_CAPTURE_DELAY_MS`, `CCONT_CHARGER_INITIAL`, `CCONT_CHARGER_PULSE_AT`, `CCONT_CHARGER_PULSE_DURATION`, `CCONT_RTC_FIXTURE_AT`, `MAD2_IRQ_OVERLAP_AT`, `MAD2_IRQ_MASK_FIXTURE_AT`, `MAD2_FIQ8_FIXTURE_AT`, `MAD2_SLEEP_FIXTURE_AT`, `MAD2_SLEEP_FIXTURE_SOURCE`, `MAD2_RESET_FIXTURE_AT`, `MAD2_WATCHDOG_FIXTURE_AT`, `BUZZER_FIXTURE_AT`, `VIBRATOR_FIXTURE_AT`, `DSPIF_CONFORMANCE`, `MBUS_RX_FIXTURE`, `MBUS_RX_FIXTURE_AT_MS`, `STATE_ROUNDTRIP_AT` | Frame capture, summaries, save-state checks, and deterministic physical-input/MMIO conformance fixtures outside the emulated hardware contract. |

There are no retained firmware-result, callback-key, task-message, or direct
registration-state forcing controls.

## Instrumentation debt

The extracted `nokia_radio_peer_device` uses named phases for the recovered SEARCH_LIST,
channel-change, RA_INFO, BCCH, random-access, LAPDm, Location Updating and
release transactions. Its report counts remain explicit because the firmware
requests and consumes those reports incrementally. A future table-driven
representation is worthwhile only where it preserves request correlation and
makes the protocol easier to review.

The retained trace switches are scoped as follows:

| Trace | Purpose |
| --- | --- |
| `TRACE_DISPLAY` | active MMI context, resource/render entry points, and LCD/DSP transfer boundaries |
| `TRACE_DISPLAY_PROFILE` | descriptor-`0x0749` load/update/copy and setup-message boundaries |
| `TRACE_DISPLAY_IO` | GENSIO LCD endpoint selection and command/data transfers |
| `TRACE_TASKS` | generic task liveness and mailbox edges |
| `TRACE_SERVICE_COMMAND` | class-`0x40` service command direction and state |
| `TRACE_SIM_RX` | SIMI/FIQ/APDU lifecycle |
| `TRACE_GSM_SERVICE` | manifest-backed generic-service registrations/callbacks |
| `TRACE_DSP_BOUNDARY` | shared-ring requests and request-derived peer responses |
| `TRACE_DSP_SHARED_READS` | first read and value transitions for each firmware PC/DSP-shared-memory offset pair |
| `TRACE_DSP_SHARED_TRANSITIONS` | every firmware observation of the small peer-owned scalar set, for request/completion correlation |
| `TRACE_GENSIO` | serial register transactions |
| `TRACE_CCONT_WATCHDOG` | combined firmware service-helper calls and logical-descriptor writes to physical CCONT watchdog register 5 |
| `TRACE_CCONT_ADC` | physical input IRQ latches and firmware-selected CCONT ADC channels/raw values |
| `TRACE_CCONT_RTC` | CCONT RTC traffic, IRQ policy, and filtered user-alarm `0x46bc` publication/routing |
| `TRACE_MAD2_LEDGER` | first-access MAD2 register census |
| `TRACE_MAD2_TIMERS` | timer-0 divider/counter/compare and FIQ assertion/acknowledgement lifecycle |
| `TRACE_MAD2_INTERRUPTS` | MAD2 source, pending, mask, acknowledgement and CPU-line routing transitions |
| `TRACE_MAD2_CLOCKS` | reset-cause, watchdog, peripheral clock-gate and timer-1 boot accesses |
| `TRACE_MBUS` | MBUS register, byte, status and FIQ-facing controller lifecycle |
| `TRACE_PUP_OUTPUTS` | changed PUP control, vibrator, buzzer and GenIO output values during organic application use |

Firmware-address-specific implementations of retained traces live in
`driver/nokia_3310_trace.inc`; ordinary MAD2 register taps remain beside their
hardware handlers.

## Topology gaps

- Most mechanically recovered ROM edges are not yet promoted to reviewed
  semantic identities.
- Dynamic generic-service descriptors are mapped far enough to exclude the
  excluded registration paths, but the wider steady-state service
  topology remains incomplete.
- The DSP/external-service peer contract is proved only for requests exercised by
  boot and the deterministic registration checkpoint. Authentication, paging,
  calls, SMS, mobility and broader inbound radio/L1 behavior remain unmapped.
- The exact internal cold-boot UI selector remains unnamed, but its behavior is
  no longer a topology gap: provisioned boot reaches an interactive idle screen
  and opens the menu organically. Accepted-security, periodic-timer, conditional
  reinitialization and shutdown paths remain separately classified.

## Completion gate

A subsystem is boxed off only when it has:

1. an explicit hardware or transport boundary;
2. symbolic topology identities rather than address-only prose;
3. a repeatable structural or behavioral acceptance profile;
4. no firmware-result forcing, task-message injection, or direct state writes;
5. stale probes and contradicted narratives removed; and
6. a second-ROM confidence result where the behavior is claimed as shared.
