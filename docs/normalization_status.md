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
- Reviewed runtime manifests retain the class-`0x40` service and coherent
  generic-service/SIM observations that established those contracts. Their
  broad firmware-PC trace generators have been retired.
- `verify-3330-frontier` and `verify-3330-navigation` protect the v4.50
  virgin-PMM setup, idle and physical-key menu lifecycle.

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
- The 3330 is a validated third interactive product profile. Its acceptance is
  UI/device-boundary based; the 3210-specific structural RAM summary is not
  treated as cross-product evidence.
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
| Flash | Native Intel/ST flash parts plus an extracted transitional `nokia_b3_flash_device` owning the 3410's lock-command, partition-status, erase-suspend timer and save-state contract. | Move the adapter semantics into MAME's generic `intelfsh` core once partitioned read-while-write and independently observable erase status are supported. |
| DSP/network/external-service | `nokia_dspif_device` owns shared RAM/DSPIF, rings and interrupt-facing completion; `nokia_dsp_hle_device` owns the cross-ROM bootstrap and service transport; `nokia_radio_peer_device` owns Nokia L1 packet correlation; `nokia_gsm_network_device` owns standards-shaped laboratory-cell, RR and MM data; `nokia_external_service_peer_device` owns the request-driven service/test session. Firmware-programmed COBBA oscillator words `0x0ae/0x0b0/0x0b6` drive separate HLE audio voices. | Recover physical bootstrap timing and the DSP-internal PLMN-measurement lifecycle; extend the GSM peer only from an evidenced Nokia L1 entrance; replace square-wave tone HLE with codec-backed behavior when evidenced. |
| MAD2 | `nokia_mad2_device` owns CTSI offsets `0x00..0x16`, timer-0/FIQ4, 15-bit timer-1/destination/FIQ5, reset request/cause, pending/masks, IRQ/FIQ aggregation, one-shot ARM clock-stop/routed wake and save-state restoration. SIMI clock bit 5 has a controller side effect; SIMI, MBUS and GENSIO are separate devices. | Establish extended IRQ ownership, exact physical timer dividers and transition latency, remaining clock-gate consumers and rail timing. |
| MBUS | `nokia_mbus_device` owns PUP offsets `0x18..0x1a`, receive/transmit holding state, 9,600-baud character timing and FIQ2/FIQ3 callbacks; ordinary v5.01/v6.00 boot initializes receive mode but transmits nothing. | Recover FIQ3 phase/source and collision/error behavior; attach a counterparty only for organic frames. |
| Display/input | Native PCD8544, a cross-ROM LCD transport check, and a cross-ROM-derived MAD2 IRQ0 matrix contract; the Lua mirror and key timing are acceptance fixtures. The generated EEPROM supplies the fields explicitly authored by equivalent v6.00/v5.01 descriptor-`0x0749` reset constructors through normal NV loading; an erased-profile control leaves the panel blank. | Obtain an authentic configured `0x0749` profile to resolve constructor-unassigned bytes; recover reset timing and mask/debounce edge cases separately. |
| Buzzer | MAD2 PUP bit 5 gates a MAME beeper; a mapped-MMIO conformance fixture validates the two-byte `13 MHz / divider` clock, enable edge and disable edge. The organic user-alarm lifecycle programs CCONT, consumes its IRQ and drives changing divider/volume values through this output. | Recover the physical volume/acoustic transfer. Ringing-tone preview and DSP/COBBA audio remain separate paths. Vibrator and backlight remain separate gaps. |
| Vibrator/backlight | PUP bit 4 drives a named MAME `vibration` output while `0x1b` stores its independent control byte; a mapped-MMIO fixture validates the gate. Enabling `Vibrating alert` through the firmware UI and ringing an organic RTC alarm leaves both registers inactive, constraining that setting to another alert lifecycle rather than proving an output defect. The 3210 service manual establishes that COBBA drives separate LCD/key-light signals into the UI-Switch. Paired-ROM MAD2 traces and a complete changed-write census of MCU-visible DSP shared RAM find no key/timeout-specific output command. | Exercise vibra organically from an incoming-call lifecycle and decode `0x1b`. Recover the lower DSP/COBBA light-control surface before exposing backlight outputs. |

The headless LCD mirror and delayed-key fixture are acceptance tooling in
`mame_nokia_dct3_input_exerciser.lua`. They do not add device state or firmware
shortcuts to the phone driver.

The synthetic EEPROM provisions the three display profiles' ROM-authored reset
fields at descriptor `0x0749`; firmware loads and copies them organically.
Constructor-unassigned bytes remain erased because no configured factory record
is available.

## Runtime-control ledger

Normal machine composition no longer depends on `NOKIA_DCT3_*` controls.
Product ADC tuples, clocks, SIMI, DSP/service peers and the validated 3210
laboratory-network peer are typed `nokia_product_config` data. The remaining
controls belong exclusively to diagnostics or explicitly named negative and
conformance fixtures:

| Class | Controls | Status |
| --- | --- | --- |
| Card/analog fixtures | `CHARGER_ADC`, `SIM_ATR_HEX`, `SIM_CPHS_AOC`, `SIM_CACHED_LOCATION` | Focused external-input fixtures. Ordinary ADC values and card attachment come from the product profile. `SIM_CACHED_LOCATION` supplies persistent EF_LOCI state for registration scenarios. |
| Timing fixtures | `TIMER0_HZ`, `TIMER1_HZ`, `FIQ8_HZ`, `TIMER0_CATCHUP` | Conformance-only clock overrides. Normal products use fixed 33,055 Hz Timer 0, 1,057 Hz Timer 1 and 1 kHz FIQ8 configuration. |
| Negative composition fixtures | `CCONT_READY`, `MODEL_DSP_SERVICE`, `MODEL_EXTERNAL_SERVICE_PEER`, `MODEL_SIM_DEVICE`, `MODEL_RADIO_PEER` | May only disable a product-owned component or readiness input for a named negative gate. They cannot enable a component absent from that product profile. |
| Read-only diagnostics | `TRACE_DISPLAY`, `TRACE_DISPLAY_PROFILE`, `TRACE_DISPLAY_IO`, `TRACE_SIM_RX`, `TRACE_DSP_BOUNDARY`, `TRACE_DSP_SHARED_READS`, `TRACE_DSP_SHARED_TRANSITIONS`, `TRACE_GENSIO`, `TRACE_GENSIO_LIMIT`, `TRACE_CCONT_WATCHDOG`, `TRACE_CCONT_ADC`, `TRACE_CCONT_RTC`, `TRACE_KEYPAD`, `TRACE_MAD2_LEDGER`, `TRACE_MAD2_TIMERS`, `TRACE_MAD2_INTERRUPTS`, `TRACE_MAD2_CLOCKS`, `TRACE_MBUS`, `TRACE_BUZZER`, `TRACE_PUP_OUTPUTS` | Log-only and bounded or scoped to a named regression. `TRACE_GENSIO_LIMIT` changes only the default 20,000-line diagnostic ceiling. |
| Harness/output controls | `SNAPSHOT_DIR`, `BOOT_SUMMARY`, `LUA_QUIET`, `POST_READY_KEY`, `POST_READY_KEYS`, `POST_READY_KEY_DELAY_MS`, `POST_READY_KEY_DURATION_MS`, `POST_READY_KEY_GAP_MS`, `POST_READY_KEY_PERIOD_MS`, `POST_READY_CAPTURE_DELAY_MS`, `CCONT_CHARGER_INITIAL`, `CCONT_CHARGER_PULSE_AT`, `CCONT_CHARGER_PULSE_DURATION`, `CCONT_RTC_FIXTURE_AT`, `MAD2_IRQ_OVERLAP_AT`, `MAD2_IRQ_MASK_FIXTURE_AT`, `MAD2_FIQ8_FIXTURE_AT`, `MAD2_SLEEP_FIXTURE_AT`, `MAD2_SLEEP_FIXTURE_SOURCE`, `MAD2_RESET_FIXTURE_AT`, `MAD2_WATCHDOG_FIXTURE_AT`, `BUZZER_FIXTURE_AT`, `VIBRATOR_FIXTURE_AT`, `DSPIF_CONFORMANCE`, `MBUS_RX_FIXTURE`, `MBUS_RX_FIXTURE_AT_MS`, `STATE_ROUNDTRIP_AT` | Frame capture, summaries, save-state checks, and deterministic physical-input/MMIO conformance fixtures outside the emulated hardware contract. |

There are no retained firmware-result, callback-key, task-message, or direct
registration-state forcing controls.

## Instrumentation debt

All driver/component sources now carry MAME license and holder headers; device
headers use conventional include guards and do not include the `emu.h` umbrella.
The save-state audit distinguishes emulated state from configuration and
diagnostics: registers, FIFOs, protocol phases, mutable card data and controller
line state are registered, while immutable product settings and log counters
are deliberately not serialized. `make verify-mad2` retains the executable
mid-boot save/load check.

The extracted `nokia_radio_peer_device` uses named phases for the recovered SEARCH_LIST,
channel-change, RA_INFO, BCCH, random-access, LAPDm, Location Updating and
release transactions. Its report counts remain explicit because the firmware
requests and consumes those reports incrementally. A future table-driven
representation is worthwhile only where it preserves request correlation and
makes the protocol easier to review.

The retained trace switches are scoped as follows:

| Trace | Purpose |
| --- | --- |
| `TRACE_DISPLAY` | selected-operator resource presentation used by the registration gate |
| `TRACE_DISPLAY_PROFILE` | descriptor-`0x0749` load/update/copy and setup-message boundaries |
| `TRACE_DISPLAY_IO` | GENSIO LCD endpoint selection and command/data transfers |
| `TRACE_SIM_RX` | SIMI/FIQ/APDU lifecycle |
| `TRACE_DSP_BOUNDARY` | shared-ring requests and request-derived peer responses |
| `TRACE_DSP_SHARED_READS` | first read and value transitions for each firmware PC/DSP-shared-memory offset pair |
| `TRACE_DSP_SHARED_TRANSITIONS` | every firmware observation of the small peer-owned scalar set, for request/completion correlation |
| `TRACE_GENSIO` | serial register transactions |
| `TRACE_CCONT_WATCHDOG` | combined firmware service-helper calls and logical-descriptor writes to physical CCONT watchdog register 5 |
| `TRACE_CCONT_ADC` | physical input IRQ latches and firmware-selected CCONT ADC channels/raw values |
| `TRACE_CCONT_RTC` | CCONT RTC traffic, IRQ policy, and filtered user-alarm `0x46bc` publication/routing |
| `TRACE_KEYPAD` | physical switch edges plus MAD2 row, direction, interrupt-mask and column-read activity |
| `TRACE_MAD2_LEDGER` | first-access MAD2 register census |
| `TRACE_MAD2_TIMERS` | timer-0 divider/counter/compare and FIQ assertion/acknowledgement lifecycle |
| `TRACE_MAD2_INTERRUPTS` | MAD2 source, pending, mask, acknowledgement and CPU-line routing transitions |
| `TRACE_MAD2_CLOCKS` | reset-cause, watchdog, peripheral clock-gate and timer-1 boot accesses |
| `TRACE_MBUS` | MBUS register, byte, status and FIQ-facing controller lifecycle |
| `TRACE_PUP_OUTPUTS` | changed PUP control, vibrator, buzzer and GenIO output values during organic application use |

The small firmware-address-specific set retained by focused radio, display,
alarm, and watchdog gates lives in `driver/nokia_3310_trace.inc`. A compliance
test keeps this quarantine below 200 lines and rejects state-changing APIs.
Hardware
boundary traces remain beside their owning handlers. Closed task, service,
SIM-registration, and generic-display probes have been removed rather than
preserved as an investigation journal.

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
