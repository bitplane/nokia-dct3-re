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

## Current frontier

The ordinary SIM initialization contract is satisfied. With provisioned phone
identity data, firmware paints an idle-like `Menu` frame, but task 1 remains in
startup mode `0x0004`.

The keypad lifecycle is bounded and its MAD2 register contract is aligned in
3210 v6.00 and v5.01:

```text
IRQ0 -> ISR 0x2b3084 -> task-1 event 0x41
     -> firmware 0x41/0x42/0x43 sequence -> matrix scan 0x2b2f90
     -> key decode 0x2b4628 -> resource 0x6e02
```

Mode 4 already runs keypad and security-editor interaction. Report code 7,
callbacks `0x5d`, `0x01`, and `0x10`, and task-6 selector `0x0732` belong to
conditional navigation, reinitialization, or shutdown lifecycles. The open
question is the ordinary unattended idle-window selection.

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
  and sets SIM ENABLE through relocated v5 state fields.
  Contact ready-bit consumption and display progress are not yet equivalent to v6.00.

## Device and model debt

| Area | Current role | Remaining work |
| --- | --- | --- |
| SIM | `nokia_simi_device` owns MAD2 registers/FIFOs/IIR/timing/FIQ6 and connects by byte/reset callbacks to `nokia_sim_card_device`, which owns T=0 plus synthetic contents. | Stabilize timing/error/removal contracts and separate subscriber provisioning from card protocol; extend files only for organic requests. |
| CCONT | `nokia_ccont_device` owns selected-device register behavior and output signals; MAD2 owns GENSIO status, while the phone supplies raw ADC scenarios and the provisional watchdog clock. v6.00/v5.01 share the observed register and IRQ contract. | Obtain board-level selector names, analog units, conversion timing, RTC encoding and watchdog clock from hardware evidence. |
| EEPROM | Native MAME `I2C_24C128` on mapped GenIO pins plus a v6.00-oriented generated provisioning fixture; the parallel window is unproved. | Decode remaining fields, validate writes/timing and another product's storage placement, and make fallback-record extraction ROM-aware. |
| DSP/external-service | `nokia_dsp_peer_device` currently aggregates shared RAM/DSPIF, DSP-owned ring and interrupt behavior, boot-subset DSP HLE, and a semantically separate request-driven service/test peer. | Add focused transport tests, validate another ROM family, then separate transport, DSP HLE and external peer without changing firmware-visible composition. |
| MAD2 | Phone-owned timers, interrupt aggregation, GENSIO, SIMI window and DSP control registers. | Improve reset/decode fidelity before extracting shared blocks. |
| Display/input | Native PCD8544 and a cross-ROM-derived MAD2 IRQ0 matrix contract; the Lua mirror and key timing are acceptance fixtures, while display type still comes from a RAM-read shortcut. | Add focused reset/command and mask/debounce tests, recover the real display-type source, then remove the shortcut. |

The headless LCD mirror and delayed-key fixture are acceptance tooling in
`mame_noki3210_input_exerciser.lua`. They do not add device state or firmware
shortcuts to the phone driver.

One RAM-read shortcut remains explicitly quarantined for display-type selection.
It is not presented as hardware behavior; a leave-one-out run confirms that it
still controls later LCD presentation even though the scheduler/SIM frontier
otherwise survives.

## Runtime-control ledger

Every live `NOKI3210_*` control belongs to one of these classes:

| Class | Controls | Status |
| --- | --- | --- |
| Hardware/product shortcut | `DISPLAY_TYPE` | Quarantined firmware RAM-read override; recover the product-data source. |
| Hardware scenarios | `ADC_PROFILE`, `ADC0..7`, `SIM_ATR_HEX`, `SIM_CPHS_AOC` | Deterministic analog/card inputs, not inferred physical defaults. |
| Timing calibration | `TIMER0_HZ`, `TIMER1_HZ`, `FIQ8_HZ`, `TIMER0_CATCHUP`, `MODEL_DSP_SERVICE_DELAY_MS`, `MODEL_DSP_SERVICE_TICK_MS` | Retain while visible as calibration debt; replace with recovered clocks/transactions. |
| Safety/acceptance guard | `DISABLE_CCONT_WATCHDOG` | Prevents an incompletely timed watchdog from hiding the investigated state; not hardware fidelity. |
| Device-boundary prototypes | `MODEL_CCONT_PRESENT`, `MODEL_DSP_SERVICE`, `MODEL_EXTERNAL_SERVICE_PEER`, `MODEL_SIM_DEVICE` | Supported deep profile only; organic interfaces, incomplete contracts. |
| Read-only diagnostics | `TRACE_HANDOFF`, `TRACE_DISPLAY`, `TRACE_TASKS`, `TRACE_SERVICE_COMMAND`, `TRACE_SIM_RX`, `TRACE_GSM_SERVICE`, `TRACE_DSP_BOUNDARY`, `TRACE_GENSIO`, `TRACE_MAD2_LEDGER` | Log-only and bounded or scoped to a named investigation. |
| Harness/output controls | `SNAPSHOT_DIR`, `BOOT_SUMMARY`, `LUA_QUIET`, `POST_READY_KEY`, `POST_READY_KEYS`, `POST_READY_KEY_DELAY_MS`, `POST_READY_KEY_DURATION_MS`, `POST_READY_KEY_GAP_MS`, `POST_READY_KEY_PERIOD_MS` | Frame capture, summaries, and deterministic input fixtures outside the emulated hardware contract. |

There are no retained firmware-result, callback-key, task-message, or direct
registration-state forcing controls.

## Instrumentation debt

The retained trace switches are scoped as follows:

| Trace | Purpose |
| --- | --- |
| `TRACE_HANDOFF` | task-1 modes/posts plus IRQ0 and keypad scan/decode seams |
| `TRACE_DISPLAY` | active MMI context, resource/render entry points, and LCD/DSP transfer boundaries |
| `TRACE_TASKS` | generic task liveness and mailbox edges |
| `TRACE_SERVICE_COMMAND` | class-`0x40` service command direction and state |
| `TRACE_SIM_RX` | SIMI/FIQ/APDU lifecycle |
| `TRACE_GSM_SERVICE` | manifest-backed generic-service registrations/callbacks |
| `TRACE_DSP_BOUNDARY` | shared-ring requests and request-derived peer responses |
| `TRACE_GENSIO` | serial register transactions |
| `TRACE_MAD2_LEDGER` | first-access MAD2 register census |

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
- The ordinary unattended UI/idle-window entrance is not yet identified. The
  accepted security transaction, periodic UI timers, and conditional
  reinitialization/shutdown selectors are excluded.

## Completion gate

A subsystem is boxed off only when it has:

1. an explicit hardware or transport boundary;
2. symbolic topology identities rather than address-only prose;
3. a repeatable structural or behavioral acceptance profile;
4. no firmware-result forcing, task-message injection, or direct state writes;
5. stale probes and contradicted narratives removed; and
6. a second-ROM confidence result where the behavior is claimed as shared.
