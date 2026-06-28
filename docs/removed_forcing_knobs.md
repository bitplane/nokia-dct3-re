# Removed forcing knobs (cleanup changelog)

In two waves, **all** `NOKI3210_FORCE_*` knobs were removed from the Makefile and the
driver — the boot is now forcing-free. The first wave (22 knobs, below) were dead
defaults; the second wave (21 knobs, see bottom) were the *active* boot-progress
forces, removed after an empirical off-one-at-a-time audit proved them inert.
Every removal is behaviour-preserving (uncapped 8s oracle byte-identical: `d8a9a7`).
Captured here so the knowledge isn't lost.

## Battery / VBAT cluster (9) — proven red herring
`FORCE_BATTERY_STATE`, `FORCE_BATTERY_CLASSIFIER_THRESHOLDS`, `FORCE_BATTERY_HW_MODE`,
`FORCE_BATTERY_ADC_PHASE_BYTE`, `FORCE_SERVICE72_BATTERY_PHASE`,
`FORCE_SERVICE72_FAST_VBAT_READS`, `FORCE_SERVICE72_STABLE_VBAT`,
`FORCE_MODE7_BATTERY_READY_EVENT`, `FORCE_POST74_BATTERY_READY`

Why removed: the VBAT measurement subsystem is fully decoded (see
`battery_classifier_analysis.md`). Band 1 is the normal state; the boot never
needed band 3. `FORCE_POST74_BATTERY_READY` was shown to be a literal no-op
(identical LCD frame with it off). All of these forced battery state/calibration
that we now understand and don't need.

## Superseded phase-5 / post-74 experiments (6)
`FORCE_PHASE5_SERVICE7601_COMPLETE` (README explicitly: "do not use"),
`FORCE_PHASE5_EVENT`, `FORCE_PHASE5_EVENT03`, `FORCE_PHASE5_TASK3_WAITING`,
`FORCE_POST74_EVENT`, `FORCE_POST74_QUEUE_ROOM`

Why removed: these targeted the old mode-7 / phase-5 scheduler blocker, which is
superseded by the current task5/display blocker. `FORCE_PHASE5_TASK3_WAITING` was
a known diagnostic shim, not a model. The corresponding scheduler facts (task3
state byte `0x1093f9`, `sched_post_task_message_26a204`) are in the README's
"Known Startup Facts".

## Abandoned mode-by-mode experiments (7)
`FORCE_MODE4_STARTUP_COMPLETIONS`, `FORCE_MODE5_STARTUP_EVENTS`,
`FORCE_MODE6_LOWER_IDLE`, `FORCE_MODE6_READY_GATE`, `FORCE_POST_CHARGER_READY_GATE`,
`FORCE_STARTUP_GATE_HI`, `FORCE_CCONT_STARTUP_NIBBLE`

Why removed: early boot-march scaffolding, superseded by the current active profile
(`FORCE_MODE5_CCONT_IRQ`, `FORCE_MODED_STARTUP_COMPLETE`, `FORCE_STARTUP_READY_GATE`,
etc.). `FORCE_STARTUP_GATE_HI` was OR'd into the kept `FORCE_STARTUP_SERVICE_READY`
path; only its term was removed.

## Second wave (2026-06-26): the remaining 21 runtime forces — all removed

The "still to do" bisection below was completed. Every `NOKI3210_FORCE_*` knob the
driver still read was audited against the uncapped 8-emulated-second oracle by
flipping it off one at a time:

| force | engaged in oracle? | off → frame | verdict |
|---|---|---|---|
| `SERVICE_LOWER_IDLE`, `EVENT15_COMPLETION`, `MODED_STARTUP_COMPLETE`, `POST_CHARGER_COMPLETIONS`, `MODE5_CCONT_IRQ`, `CONTACT_STARTUP_GATE_PULSE`, `STARTUP_SERVICE_READY`, `SERVICE_READY_BIT`, `STARTUP_READY_GATE` | yes (set non-zero) | `d8a9a7` (unchanged) | **inert** |
| `MODED_WAIT_LATCH_COMPLETE` | yes | `c7a060` (changed) | **"load-bearing"** (cosmetic only) |
| `TASK14_READY_FLAGS`, `MODE7_STARTUP_READY_GATE`, `MODE7_STARTUP_READY_RESULT`, `CONTACT_STARTUP_STATUS_PASS`, `TASK14_READY`, `5F_CONTACT_COMMAND`, `CCONT_CHARGER_ABSENT`, `CONTACT_D9_ACK`, `IDLE_CONTEXT_UNLINKED`, `STARTUP_SELFTEST_RESULT`, `STARTUP_ALT_SELFTEST_RESULT`, `DISPLAY_0199_READY` | no (default 0/empty) | — | **inert (never engaged)** |

Key finding: removing **all 21** forces together (including the one "load-bearing"
`MODED_WAIT_LATCH_COMPLETE`) lands on **`d8a9a7`** — byte-identical to the original
fully-forced baseline. The `c7a060` frame only appeared in the *partial* combination
(that one off, the rest on); it was an interaction artifact between forces, not a real
boot state. Both `d8a9a7` and `c7a060` render **CONTACT SERVICE** — the forces never
changed the boot *milestone*, only stray right-edge framebuffer pixels.

Conclusion: **the entire runtime forcing apparatus was a net no-op for the 8-second
boot.** None of it was getting us to CONTACT SERVICE (the untuned/blank-EEPROM
self-test failure does that, see `battery_classifier_analysis.md`), and none of it was
getting us past it. All 21 driver branches and their Makefile knobs were deleted;
the boot is now **forcing-free** and still validates to `d8a9a7`.

Removed alongside: the dead param knobs `MODE5_CCONT_IRQ_STATUS` and
`CONTACT_STARTUP_STATUS_PASS_MASK` (they only fed removed force branches), and the
orphaned `battery_startup_event_forced` bool.

## Kept (not FORCE_ knobs, left in place)
`NOKI3210_BATTERY_PROFILE` (opt-in "charged" battery test profile, inert unless set),
`DISPLAY_TYPE`, `CONTACT_DA_PRESERVE_READY_BIT`, `SERVICE72_RESPONSE_STATUS`,
`CONTACT_D9_TIMEOUT_DELAY`, `STARTUP_EVENT15_DELAY_CLAMP`, and the unconditional
`EVENT14_LATCH` read-mask shim. These are non-force research/test knobs or
named hardware-source shims, out of scope for this audit.

## Next real blocker
With forcing gone, the honest state is unobstructed: the boot reaches **CONTACT
SERVICE** in ~8 emulated seconds because the EEPROM is blank/virgin (all 0xFF), so the
firmware's power-on self-test / calibration check fails. That self-test path — not any
force — is what stands between here and the idle screen.
