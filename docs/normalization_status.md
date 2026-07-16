# Normalization status

This document identifies which conclusions have entered the repeatable evidence
regime and which implementation areas remain provisional. It is not a claim
that the driver is ready for upstream submission.

## Normalized acceptance profiles

- `make verify` protects the default fault-screen frame and structural
  summary.
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
  oracle matches v6.00 exactly outside the shared animated-icon mask. The
  retained service status `0x00c9` versus `0x0049` is not UI-blocking.

## Device and model debt

| Area | Current role | Remaining work |
| --- | --- | --- |
| SIM | `nokia_simi_device` owns MAD2 registers/FIFOs/IIR, the `TA1=0x05` character timing advertised by the synthetic card, and FIQ6, connected by byte/reset callbacks to `nokia_sim_card_device`, which owns T=0 plus synthetic contents. | Derive timing from negotiated card parameters rather than the current fixed ATR profile; recover ATR/turnaround and error/removal contracts; separate subscriber provisioning from card protocol and extend files only for organic requests. |
| CCONT/GENSIO | `nokia_ccont_device` owns selected-device registers and outputs, a deterministic binary RTC, recovered periodic IRQ sources, the documented watchdog counter and its WDDISX pin; `nokia_gensio_device` owns serial selection/status, LCD pins and SELECT latches. | Obtain board-level selector names, analog units, physical timing and SELECT peer identities; recover the missing periodic firmware watchdog-service lifecycle. |
| EEPROM | Native MAME `I2C_24C128` on mapped GenIO pins plus a v6.00-oriented generated provisioning fixture; the parallel window is unproved. | Decode remaining fields, validate writes/timing and another product's storage placement, and make fallback-record extraction ROM-aware. |
| DSP/external-service | `nokia_dspif_device` owns shared RAM/DSPIF, rings and interrupt-facing completion; `nokia_dsp_hle_device` owns boot-subset DSP behavior; `nokia_external_service_peer_device` owns the separate request-driven service/test session. | Recover bootstrap transition timing and add wrap/full/fault transport cases; extend peer vocabulary only for organic requests. |
| MAD2 | `nokia_mad2_device` owns CTSI offsets `0x00..0x16`, timer-0/FIQ4, free-running timer-1/FIQ5, pending/masks, IRQ/FIQ aggregation and save-state restoration. SIMI, MBUS and GENSIO are separate devices; other peripheral windows remain phone-owned. | Establish extended IRQ ownership, Timer-0's physical input, Timer-1 destination compare behavior and reset domains. |
| MBUS | `nokia_mbus_device` owns PUP offsets `0x18..0x1a`, receive/transmit holding state, 9,600-baud character timing and FIQ2/FIQ3 callbacks; ordinary v5.01/v6.00 boot initializes receive mode but transmits nothing. | Recover FIQ3 phase/source and collision/error behavior; attach a counterparty only for organic frames. |
| Display/input | Native PCD8544, a cross-ROM LCD transport check, and a cross-ROM-derived MAD2 IRQ0 matrix contract; the Lua mirror and key timing are acceptance fixtures. The generated EEPROM supplies the one recovered descriptor-`0x0749` field through normal NV loading. | Obtain a complete authentic `0x0749` profile to replace the minimal synthetic field; recover reset timing and mask/debounce edge cases separately. |
| Buzzer | MAD2 PUP bit 5 gates a MAME beeper; the two-byte divider produces `13 MHz / divider` and register `0x1e` remains stored. | Exercise an organic ringtone/key-tone path and recover volume/acoustic response; vibrator and backlight remain separate gaps. |

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
| Hardware scenarios | `ADC_PROFILE`, `ADC0..7`, `SIM_ATR_HEX`, `SIM_CPHS_AOC` | Deterministic analog/card inputs, not inferred physical defaults. |
| Timing calibration | `TIMER0_HZ`, `TIMER1_HZ`, `FIQ8_HZ`, `TIMER0_CATCHUP`, `MODEL_DSP_SERVICE_DELAY_MS`, `MODEL_DSP_SERVICE_TICK_MS` | Timer-0 overrides remain available to unfinished non-3210 profiles; the 3210 uses its documented 13 MHz clock and exact compare. Retain the others while visible as calibration debt. |
| Provisional hardware input | `CCONT_WDDISX_GROUNDED` | Grounds CCONT's documented physical watchdog-disable pin. The 3210 profile currently enables it because the emulation lacks the steady-state firmware service lifecycle; this is not claimed as the production board strap. |
| Device-boundary prototypes | `MODEL_CCONT_PRESENT`, `MODEL_DSP_SERVICE`, `MODEL_EXTERNAL_SERVICE_PEER`, `MODEL_SIM_DEVICE` | Enabled by the 3210 product profile; overrides remain for negative tests. Organic interfaces, incomplete wider contracts. |
| Read-only diagnostics | `TRACE_DISPLAY`, `TRACE_DISPLAY_PROFILE`, `TRACE_DISPLAY_IO`, `TRACE_TASKS`, `TRACE_SERVICE_COMMAND`, `TRACE_SIM_RX`, `TRACE_GSM_SERVICE`, `TRACE_DSP_BOUNDARY`, `TRACE_GENSIO`, `TRACE_CCONT_WATCHDOG`, `TRACE_MAD2_LEDGER`, `TRACE_MAD2_TIMERS`, `TRACE_MAD2_INTERRUPTS`, `TRACE_MAD2_CLOCKS`, `TRACE_MBUS` | Log-only and bounded or scoped to a named investigation. |
| Harness/output controls | `SNAPSHOT_DIR`, `BOOT_SUMMARY`, `LUA_QUIET`, `POST_READY_KEY`, `POST_READY_KEYS`, `POST_READY_KEY_DELAY_MS`, `POST_READY_KEY_DURATION_MS`, `POST_READY_KEY_GAP_MS`, `POST_READY_KEY_PERIOD_MS`, `POST_READY_CAPTURE_DELAY_MS`, `CCONT_CHARGER_PULSE_AT`, `CCONT_CHARGER_PULSE_DURATION`, `MAD2_IRQ_OVERLAP_AT`, `MAD2_IRQ_MASK_FIXTURE_AT`, `MAD2_FIQ8_FIXTURE_AT`, `DSPIF_CONFORMANCE`, `MBUS_RX_FIXTURE`, `MBUS_RX_FIXTURE_AT_MS`, `STATE_ROUNDTRIP_AT` | Frame capture, summaries, save-state checks, and deterministic physical-input/MMIO conformance fixtures outside the emulated hardware contract. |

There are no retained firmware-result, callback-key, task-message, or direct
registration-state forcing controls.

## Instrumentation debt

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
| `TRACE_GENSIO` | serial register transactions |
| `TRACE_CCONT_WATCHDOG` | combined firmware service-helper calls and logical-descriptor writes to physical CCONT watchdog register 5 |
| `TRACE_MAD2_LEDGER` | first-access MAD2 register census |
| `TRACE_MAD2_TIMERS` | timer-0 divider/counter/compare and FIQ assertion/acknowledgement lifecycle |
| `TRACE_MAD2_INTERRUPTS` | MAD2 source, pending, mask, acknowledgement and CPU-line routing transitions |
| `TRACE_MAD2_CLOCKS` | reset-cause, watchdog, clock-control and timer-1 boot accesses |
| `TRACE_MBUS` | MBUS register, byte, status and FIQ-facing controller lifecycle |

Firmware-address-specific implementations of retained traces live in
`driver/nokia_3310_trace.inc`; ordinary MAD2 register taps remain beside their
hardware handlers.

## Topology gaps

- Most mechanically recovered ROM edges are not yet promoted to reviewed
  semantic identities.
- Dynamic generic-service descriptors are mapped far enough to exclude the
  excluded registration paths, but the wider steady-state service
  topology remains incomplete.
- The DSP/external-service peer contract is proved only for the requests exercised by
  the current boot.
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
