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
- The 3330 and 3410 fresh-PMM frontier and navigation gates pass again. Both
  firmware images organically emit compact DSP request `70/0d00`; their typed
  product contracts return `74/0d00` through FIQ0 before the UI is promoted.
- The 3410 registration profile is normalized through fresh/preserved PMM,
  two save-state boundaries and three negative network compositions. Its
  autonomous band scan and external-service sequences are explicit NHM-2
  contracts; standards-level cell, assignment, LAPDm and MM behavior remains
  shared.
- NHM-2 paging is normalized without a driver branch: fresh/preserved,
  pre-page/assigned save-state and wrong-group/unmatched/malformed/barred
  compositions all use the generic network, paging-group, session and LAPDm
  components.
- Deterministic mobile-originated speech calls are normalized across NSE-8,
  NHM-5, NHM-6 and NHM-2: physical dial, CM Service, called-number SETUP,
  one TCH assignment, Alerting/Connect, bidirectional media, product-specific
  release and restored PCH cadence. Backend decisions and failure outcomes
  remain a separate interface rather than fixture flags in the session.
- NHM-2 incoming calls and internal media are normalized through organic
  ringing, physical Send/End, one TCH assignment despite repeated Call
  Confirmed, command-`0x08` values `0x060b`/`0x040a`, timed 1 MHz/8 kHz
  COBBA-GJP PCM, bidirectional GSM-FR and clean return to PCH. Fitted analogue
  routing remains explicitly unknown.

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

The validated NSE-8/NHM-5 frontier includes deterministic menu traversal,
user-data persistence, authenticated registration, call control and
bidirectional physical speech. The active model frontier is NSE-3 bootstrap:
its external firmware and MCU-side protocol are bounded, but executable work
must wait for the physical DSP completion publication or matching internal
ROMs. Other application work should begin only when an organic path reaches an
unresolved hardware or protocol boundary.

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
| CCONT/GENSIO | `nokia_ccont_device` owns selected-device registers and outputs, a deterministic binary RTC, recovered periodic IRQ sources, the documented watchdog counter and its WDDISX pin, plus retained power state. Five ROMs share an identical 18-entry descriptor vocabulary and a 933-transaction organic census. NSE-8 board wiring is classified; 3330/3410 explicitly retain the 3310 raw tuple as boot calibration rather than asserted PCB identity. | Measure GENSIO/ADC latency, sibling board wiring, raw electrical units, rail timing, interrupt bit 6 and compatibility-register effects. |
| EEPROM | Native MAME `I2C_24C128` on mapped GenIO pins plus a v6.00-oriented generated provisioning fixture; the parallel window is unproved. | Decode remaining fields, validate writes/timing and another product's storage placement, and make fallback-record extraction ROM-aware. |
| Flash | Native Intel/ST flash parts plus an extracted transitional `nokia_b3_flash_device` owning the 3410's lock-command, partition-status, erase-suspend timer and save-state contract. | Move the adapter semantics into MAME's generic `intelfsh` core once partitioned read-while-write and independently observable erase status are supported. |
| DSP/network/external-service | DSPIF, DSP HLE, Nokia L1 correlation, LAPDm, GSM session/network data and external service are separate owners. NSE-8, NHM-5, NHM-6 and NHM-2 independently pass paging, physical Answer/End and bidirectional internal GSM-FR; the first three additionally pass FACCH/BFI/SACCH coexistence and active-call save-state replay, while NSE-8 and NHM-5 pass isolated host audio. Speech starts only when TCH, product-specific DSP control and an evidenced PCM profile agree; unknown products remain inert. | Obtain NSE-3's physical final DSP-bootstrap publication or matching internal ROMs. Recover real COBBA mux/gain behavior and NHM-6/NHM-2 fitted analogue routes before physical-duplex promotion. Extend NHM-2 through degradation/save-state gates before claiming equal resilience coverage. Add A5 only at the established burst-bit boundary. |
| MAD2 | `nokia_mad2_device` owns the CTSI registers within `0x00..0x16`, timer-0/FIQ4, timer-1 current/terminal-count/FIQ5, reset request/cause, pending/masks, IRQ/FIQ aggregation, cross-ROM ninth-IRQ status/ack semantics, one-shot ARM clock-stop/routed wake and save-state restoration. PUP `0x15`, KBGPIO, SIMI, MBUS and GENSIO are separate devices. | Establish the ninth IRQ's physical owner, exact physical timer dividers and transition latency, remaining clock-gate consumers and rail timing. |
| MBUS | `nokia_mbus_device` owns PUP offsets `0x18..0x1a`, receive/transmit holding state, 9,600-baud character timing and FIQ2/FIQ3 callbacks; ordinary v5.01/v6.00 boot initializes receive mode but transmits nothing. | Recover FIQ3 phase/source and collision/error behavior; attach a counterparty only for organic frames. |
| Display/input | Native PCD8544, a cross-ROM LCD transport check, and extracted `nokia_kbgpio_device` matrix/IRQ0 behavior; the Lua mirror, handset key layout and timing remain acceptance/board fixtures. The generated EEPROM supplies fields authored by equivalent v6.00/v5.01 descriptor-`0x0749` reset constructors through normal NV loading. | Obtain an authentic configured `0x0749` profile; recover keypad mask/debounce and display reset timing separately. |
| Buzzer | Extracted `nokia_pup_device` bit 5 gates a MAME beeper and derives its clock from the two-byte `13 MHz / divider`; mapped-MMIO, organic user-alarm and the focused unlocked incoming-call/physical-Answer verifier validate the boundary. | Recover the physical volume/acoustic transfer. Ringing-tone preview and DSP/COBBA audio remain separate paths. Vibrator and backlight remain separate gaps. |
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
laboratory-network peer are typed `nokia_product_config` data. Negative
composition is selected through standard MAME configuration ports, passive
diagnostics use MAME logging, and scripted physical-input/MMIO fixtures remain
outside the production driver in Lua:

| Class | Controls | Status |
| --- | --- | --- |
| Product hardware | Typed `nokia_product_config` fields and device setters | ADC inputs, timer clocks, SIM profile, display geometry, flash capabilities and peer composition are fixed by the selected machine configuration. |
| Negative composition | `HWCFG` MAME configuration port, with named files under `fixtures/` | A gate may disable a product-owned CCONT, SIM, DSP-service, external-service, radio or MAD2/COBBA PCM component. It cannot enable hardware absent from the product profile. |
| Controller conformance | `DIAGCFG` MAME configuration port, selected by a named fixture | Requests an internal device invariant check without changing firmware state or normal machine composition. |
| Network event | `NETCFG` MAME configuration port, selected by `fixtures/radio_paging` or `fixtures/radio_incoming_call` | Queues exactly one incoming page or one bounded incoming-call attempt after organic registration; the default laboratory cell remains passive and emits only no-identity PCH fill. |
| Passive diagnostics | Standard MAME logging categories, enabled together by `RUN_VERBOSE=1` in the local harness | Read-only and bounded observations. No device or driver parses a diagnostic environment variable. |
| Harness/output controls | Lua-only `NOKIA_DCT3_*` controls for snapshots, summaries, scripted keys, charger/RTC events, MAD2 MMIO fixtures, MBUS input and save-state checks | External test orchestration. These variables are parsed only by `mame_nokia_dct3_input_exerciser.lua` and are not production configuration. |

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
channel-change, RA_INFO, BCCH, random-access, Location Updating and release
transactions. Decoded link establishment and sequence state belong to
`nokia_lapdm_link_device`; complete Layer-3 request and bounded MM/CC/SMS
transaction state belong to `nokia_gsm_session_device`. Radio-peer report counts remain explicit
because the firmware requests and consumes those reports incrementally. A future table-driven
representation is worthwhile only where it preserves request correlation and
makes the protocol easier to review.

The retained MAME log categories are scoped as follows. The production driver
uses one verbose-mode gate; categories keep each output stream named and make a
future per-mask migration mechanical rather than maintaining parallel boolean
configuration:

| Trace | Purpose |
| --- | --- |
| `TRACE_DISPLAY` | selected-operator resource presentation used by the registration gate |
| `TRACE_DISPLAY_PROFILE` | descriptor-`0x0749` load/update/copy and setup-message boundaries |
| `TRACE_DISPLAY_IO` | GENSIO LCD endpoint selection and command/data transfers |
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
| `TRACE_BUZZER` / `TRACE_VIBRATOR` | changed organic output state used by focused peripheral gates |

The small firmware-address-specific set retained by focused radio, display,
alarm, and watchdog gates lives in `driver/nokia_dct3_trace.inc`. A compliance
test keeps this quarantine below 260 lines and rejects state-changing APIs.
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
  boot, authenticated registration, paging, answered MT call control and speech,
  and persistent ordinary MT SMS delivery. MO SMS, MT CP/RP closure, completed
  multipart Smart Messaging/ringtone UI, mobility and broader inbound radio/L1
  behavior remain unmapped. A bounded
  two-part long-ringtone codec and queue are present; part 1 is organically
  regression-tested through nine stop-and-wait SAPI-3 frames, while part 2
  correctly remains gated by the missing CP/RP close.
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
