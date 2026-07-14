# Driver structure convention

Goal: the Nokia driver should describe machine composition while reusable hardware behavior lives
in explicit MAME devices. Firmware-research execution tracing remains quarantined so it can be
deleted incrementally as observed contracts become components.

## Entry points vs. quarantine

The memory-map-registered handlers are thin. Diagnostic execution observations
are isolated in clearly bannered trace helpers; the RAM read path also owns two
explicitly documented research shortcuts:

| hardware entry point | quarantined research helper |
|---|---|
| `flash_r` (≈6 lines)  | `flash_firmware_traces` (observation only; cannot override instructions) |
| `ram_w`   (≈10 lines) | `ram_w_firmware_traces` (write-side research observations) |
| `ram_r`   (≈10 lines) | `ram_r_firmware_overrides` (read-side research observations) |

The PCD8544 LCD and MAME `I2C_24C128` model the display and external
EEPROM. Headless LCD capture and scripted keypad input belong to the Lua
acceptance harness; the production driver contains neither a second LCD parser
nor synthetic key state. CCONT is an explicit local `nokia_ccont_device` owning its serial
registers, ADC results, RTC, interrupt state and watchdog. Task 7 remains the
firmware adapter to the external service/test peer; the request-driven
DSP/contact prototype is an explicit `nokia_dsp_peer_device`. It owns shared DSP
RAM, MCU/DSP ring indices, service cadence, and the observed contact-session
state, and returns peer transactions through FIQ0/IRQ4 callbacks.
The provisional
`nokia_sim_card_device` owns the verified SIMI register/FIQ transport and a synthetic GSM 11.11
card. The phone state owns MAD2 interrupt routing and supplies power-scenario ADC inputs. The
remaining MAD2 register block should wait for the
cross-ROM pass to identify the genuinely shared contract.

## Rules

- New *hardware* behaviour goes in a device model or the owning MAD2 register block.
- New diagnostic traces may go in the `*_firmware_*` helpers when no component
  boundary exists yet. Do not add result forcing or task-message injection.
- The research helpers should **shrink over time**. Delete a trace after its
  conclusion is normalized; replace each RAM-read shortcut with its real
  hardware or nonvolatile-data owner.
- All firmware-result forces and registration-message injections are removed.
  Two explicit RAM-read shortcuts remain: display-type sourcing and the startup
  event-14 latch. See `evidence_regime.md`.
- The direct firmware allocation trampoline,
  service-status RAM completion, and synthetic startup-report feed are removed.
  The current contact peer consumes organic DSP-ring requests and returns
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
