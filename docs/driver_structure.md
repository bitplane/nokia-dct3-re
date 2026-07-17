# Driver structure convention

Goal: the Nokia driver should describe machine composition while reusable hardware behavior lives
in explicit MAME devices. Firmware-research execution tracing remains quarantined so it can be
deleted incrementally as observed contracts become components.

## Entry points vs. quarantine

The memory-map-registered handlers are thin. Diagnostic execution observations
are isolated in `driver/nokia_3310_trace.inc`; no memory read path overrides a
firmware result:

| hardware entry point | quarantined research helper |
|---|---|
| `flash_r` (≈6 lines)  | `nokia_3310_trace.inc::flash_firmware_traces` (observation only; cannot override instructions) |
| `ram_w`   (≈10 lines) | `nokia_3310_trace.inc::ram_w_firmware_traces` (write-side research observations) |
| `ram_r`   (≈2 lines)  | none; display provisioning now arrives through EEPROM/NV |
| `mad2_io_r/w` | functional register routing and board-output helpers, followed by observation-only MAD2 trace helpers |

The PCD8544 LCD and MAME `I2C_24C128` model the display and external
EEPROM. Headless LCD capture and scripted keypad input belong to the Lua
acceptance harness; the production driver contains neither a second LCD parser
nor synthetic key state. CCONT is an explicit local `nokia_ccont_device` owning its serial
registers, ADC results, RTC, interrupt state and watchdog. Task 7 remains the
firmware adapter to the external service/test peer; the request-driven
DSP behavior is split at its evidenced boundaries: `nokia_dspif_device` owns
shared RAM, DSPIF, packet rings and FIQ0/IRQ4 signaling;
`nokia_dsp_hle_device` owns the boot-subset DSP behavior; and
`nokia_external_service_peer_device` owns the separate class-`0x40` service
session. The phone state only wires their callbacks to MAD2.
`nokia_simi_device` owns the MAD2 register/FIFO/IIR/FIQ-facing controller and
connects by reset/byte callbacks to `nokia_sim_card_device`, which owns T=0,
declared file metadata, persistent mutable card records and
the synthetic GSM 11.11 contents. `nokia_mad2_device` owns the CTSI core at
offsets `0x00..0x16`: reset/clock/watchdog latches, timer state, interrupt
pending/masks, and ARM IRQ/FIQ routing. Attached devices signal it through
callbacks. `nokia_mbus_device` owns PUP offsets `0x18..0x1a`, RX/TX holding
state, byte callbacks and FIQ2/FIQ3 outputs without supplying a peer. The phone
state retains board wiring, physical-input latches and less-established
peripheral windows. Its MAD2 memory-map handlers delegate register ownership,
board outputs and diagnostics to separate helpers so traces do not obscure the
functional dispatch. `nokia_gensio_device` owns its sparse serial/status/SELECT
registers and connects CCONT and the PCD8544 through callbacks. Product differences use
explicit configurations rather than driver-name parsing.

## Rules

- New *hardware* behaviour goes in a device model or the owning MAD2 register block.
- New diagnostic traces may go in the `*_firmware_*` helpers when no component
  boundary exists yet. Their firmware-address-specific implementation belongs
  in `nokia_3310_trace.inc`. Do not add result forcing or task-message injection.
- The research helpers should **shrink over time**. Delete a trace after its
  conclusion is normalized; replace each RAM-read shortcut with its real
  hardware or nonvolatile-data owner.
- All firmware-result forces, registration-message injections and firmware RAM
  read overrides are removed. The synthetic EEPROM provisions only the
  recovered descriptor-`0x0749` display-selection field; firmware loads and
  copies it through its ordinary NV path. See `evidence_regime.md`.
- The direct firmware allocation trampoline,
  service-status RAM completion, and synthetic startup-report feed are removed.
  The current external-service peer consumes organic DSP-ring requests and returns
  request-derived transport/contact responses without writing firmware state.

## Component completion gate

A subsystem leaves this driver only when its external interface is explicit, save-state fields are
registered, configuration contains no firmware addresses, and both the 3210 oracle and the current
cross-ROM smoke pass. Phone-specific constants remain in machine configuration.

The local 3330 PPM E Wintesla records are normalized reproducibly by
`make normalize-3330` and exercised by `make smoke-3330e`. This is a labelled
cross-ROM execution baseline, not the canonical PPM C audit declared by
upstream MAME. Component changes must preserve the default and coherent 3210
oracles and avoid
regressing this smoke run.

## Regression oracle

Changes must preserve the exact LCD frame and the semantic structural
predicates exercised by `make verify` and `make verify-frontier`. Raw counters,
timestamps, scheduler order, and `error.log` text are diagnostic observations,
not acceptance criteria.
