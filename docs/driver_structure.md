# Driver structure convention

Goal: `nokia_3310.cpp` should read like a hardware-emulation driver, with all
firmware-research forcing and execution tracing quarantined away from the real
hardware models so it can be deleted incrementally as the emulation matures.

## Entry points vs. quarantine

The memory-map-registered handlers are thin and contain only real hardware
behaviour. Each forwards to a clearly-banner'd `*_firmware_*` helper that holds
the forcing shims + traces:

| hardware entry point | quarantined research helper |
|---|---|
| `flash_r` (≈9 lines)  | `flash_firmware_hooks` (returns an override fetch, or `nullopt`) |
| `ram_w`   (≈10 lines) | `ram_w_firmware_overrides` (forcing can rewrite the stored value) |
| `ram_r`   (≈10 lines) | `ram_r_firmware_overrides` (forcing can rewrite the returned value) |

The real device models — `nokia_ccont_*`, `mad2_io_*`, `eeprom_*`,
`serial_eeprom_*`, the `pcd8544` LCD — are already clean and stay as-is.

## Rules

- New *hardware* behaviour goes in the thin entry points or a device model.
- New *forcing/diagnostic* shims go in the `*_firmware_*` helpers, gated by their
  `NOKI3210_*` knob, with a comment naming the gate they stand in for.
- The `*_firmware_*` helpers should **shrink over time**: when a shim's gate is
  understood and modeled as real hardware/scheduler state, delete the shim.
- **Status (2026-06-26):** all `NOKI3210_FORCE_*` firmware-result forcing has been
  removed (audited inert against the oracle — see `removed_forcing_knobs.md`). The
  helpers now hold only non-force research shims (NV/display-source stubs, trace
  taps). Do **not** re-introduce result forcing; model the missing hardware/NV state
  instead.
- **Update (2026-07): the model work is complete and the scaffolding is retired.** The faithful
  opt-in models now carry the boot all the way to "Insert SIM card":
  `MODEL_DSP_SERVICE`, `MODEL_CCONT_PRESENT`, `EEPROM_PROFILE=selftest`, `MODEL_SVC_RESPONDER`,
  `MODEL_SVC_CHANNEL_DRAIN`, `MODEL_SIM_CARD`, and `MODEL_RES_ENABLE` (the display-resource
  registration groundwork). **All 19 `EXPERIMENT_*` forcing shims and the `FORCE_IDLE/OUTCOME`
  shims were removed** (they were dead-end diagnostics; recoverable from git). The `TRACE_*` taps
  were thinned to a curated few that document the working boot (`CCONT_READ`, `LIMP`, `LIMP2`,
  `CSCMD`, `RESAVAIL`). See `sim_emulator_scope.md` for the SIM + resource layers and
  `boot_to_insert_sim.md` for the service-channel-busy root cause.

## Regression oracle

Any change to these handlers must preserve behaviour, checked by a fixed
`run-boot-progress` (20 s): the promoted LCD frame SHA and the structural boot
markers (max CCONT state, startup mode, task5 dispatch count, battery-state
distribution) must be unchanged. The raw `error.log` hash jitters ~1 line and is
not part of the oracle. (Helper scripts used during the Stage-1 cleanup lived in
the session scratchpad: `oracle.sh` / `cmp_oracle.sh`.)
