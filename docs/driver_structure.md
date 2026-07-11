# Driver structure convention

Goal: the Nokia driver should describe machine composition while reusable hardware behavior lives
in explicit MAME devices. Firmware-research execution tracing remains quarantined so it can be
deleted incrementally as observed contracts become components.

## Entry points vs. quarantine

The memory-map-registered handlers are thin and contain only real hardware
behaviour. Each forwards to a clearly-banner'd `*_firmware_*` helper that holds
the forcing shims + traces:

| hardware entry point | quarantined research helper |
|---|---|
| `flash_r` (≈9 lines)  | `flash_firmware_hooks` (returns an override fetch, or `nullopt`) |
| `ram_w`   (≈10 lines) | `ram_w_firmware_overrides` (forcing can rewrite the stored value) |
| `ram_r`   (≈10 lines) | `ram_r_firmware_overrides` (forcing can rewrite the returned value) |

The PCD8544 LCD is already a MAME device. EEPROM and CCONT are the first local implementations to
extract; SIM and the MAD2/DSP mailbox follow after their boundaries stabilize.

## Rules

- New *hardware* behaviour goes in a device model or the owning MAD2 register block.
- New *forcing/diagnostic* shims go in the `*_firmware_*` helpers, gated by their
  `NOKI3210_*` knob, with a comment naming the gate they stand in for.
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

## Component completion gate

A subsystem leaves this driver only when its external interface is explicit, save-state fields are
registered, configuration contains no firmware addresses, and both the 3210 oracle and the current
cross-ROM smoke pass. Phone-specific constants remain in machine configuration.

## Regression oracle

Any change to these handlers must preserve behaviour, checked by a fixed
`run-boot-progress` (20 s): the promoted LCD frame SHA and the structural boot
markers (max CCONT state, startup mode, task5 dispatch count, battery-state
distribution) must be unchanged. The raw `error.log` hash jitters ~1 line and is
not part of the oracle. (Helper scripts used during the Stage-1 cleanup lived in
the session scratchpad: `oracle.sh` / `cmp_oracle.sh`.)
