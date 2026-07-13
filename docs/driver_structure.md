# Driver structure convention

Goal: the Nokia driver should describe machine composition while reusable hardware behavior lives
in explicit MAME devices. Firmware-research execution tracing remains quarantined so it can be
deleted incrementally as observed contracts become components.

## Entry points vs. quarantine

The memory-map-registered handlers are thin and contain only real hardware
behaviour. Each forwards to a clearly-banner'd `*_firmware_*` helper that holds
bounded traces and provisional firmware-call bridges:

| hardware entry point | quarantined research helper |
|---|---|
| `flash_r` (≈9 lines)  | `flash_firmware_hooks` (observes fetches; legacy optional return remains) |
| `ram_w`   (≈10 lines) | `ram_w_firmware_overrides` (write-side research observations) |
| `ram_r`   (≈10 lines) | `ram_r_firmware_overrides` (read-side research observations) |

The PCD8544 LCD and MAME `I2C_24C128` now model the display and external
EEPROM. CCONT is an explicit local `nokia_ccont_device` owning its serial
registers, ADC results, RTC, interrupt state and watchdog. The
`nokia_service_transport_device` owns the modeled healthy contact completion,
response readiness and channel-empty completion timing. Task 7 remains the
firmware adapter to the external service/test peer. The provisional
`nokia_sim_card_device` owns the verified SIMI register/FIQ transport and a synthetic GSM 11.11
card. The phone state owns MAD2 interrupt routing and supplies power-scenario ADC inputs. The
MAD2/DSP mailbox follows after its boundary stabilizes; MAD2 extraction should wait for the
cross-ROM pass to identify the genuinely shared contract.

## Rules

- New *hardware* behaviour goes in a device model or the owning MAD2 register block.
- New diagnostic traces may go in the `*_firmware_*` helpers when no component
  boundary exists yet. Do not add result forcing or task-message injection.
- The `*_firmware_*` helpers should **shrink over time**: when a shim's gate is
  understood and modeled as real hardware/scheduler state, delete the shim.
- **Status (2026-06-26):** all `NOKI3210_FORCE_*` firmware-result forcing has been
  removed (audited inert against the oracle — see `removed_forcing_knobs.md`). The
  helpers now hold only non-force research shims (NV/display-source stubs, trace
  taps). Do **not** re-introduce result forcing; model the missing hardware/NV state
  instead.
- **Update (2026-07-11):** the organic registration investigation is banked and its forcing lineage
  has been removed. Surviving `MODEL_*` firmware hooks are peer prototypes until their behavior is
  expressed at a real transport boundary. See `sim_registration.md` and
  `removed_forcing_knobs.md`.
- **Service boundary (2026-07-11):** service payload construction and
  service-status completion writes no longer live in `flash_firmware_hooks`.
  The remaining flash trampoline is a narrow firmware-call bridge: it asks the
  firmware to allocate and post a device-owned inbound message. The observed
  channel-empty request is report code `0x622a` at `0x2b13d4`; the device
  completes it asynchronously through a callback.

## Component completion gate

A subsystem leaves this driver only when its external interface is explicit, save-state fields are
registered, configuration contains no firmware addresses, and both the 3210 oracle and the current
cross-ROM smoke pass. Phone-specific constants remain in machine configuration.

The local 3330 PPM E Wintesla records are normalized reproducibly by
`make normalize-3330` and exercised by `make smoke-3330e`. This is a labelled
cross-ROM execution baseline, not the canonical PPM C audit declared by
upstream MAME. Component changes must preserve both 3210 oracles and avoid
regressing this smoke run.

## Regression oracle

Any change to these handlers must preserve behaviour, checked by a fixed
`run-boot-progress` (20 s): the promoted LCD frame SHA and the structural boot
markers (max CCONT state, startup mode, task5 dispatch count, battery-state
distribution) must be unchanged. The raw `error.log` hash jitters ~1 line and is
not part of the oracle. (Helper scripts used during the Stage-1 cleanup lived in
the session scratchpad: `oracle.sh` / `cmp_oracle.sh`.)
