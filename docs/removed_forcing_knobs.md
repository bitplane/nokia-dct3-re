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

## 2026-07 tidy — RE scaffolding retired (the boot is now models + config + a few taps)

Once the boot reached "Insert SIM card" and the operator-idle wall was mapped end
to end, the research scaffolding was removed to make the driver read like a driver.
All removals are opt-in (inert in every boot config) — the oracle stays `d8a9a7`
and the full faithful boot still renders "Insert SIM card". Recoverable from git.

**All 19 `EXPERIMENT_*` forcing shims + the dead `FORCE_*` shims** (−392 lines):
`EXPERIMENT_{DSP_IRQ4,RESUME_TASK14,FORCE_ACK,PROV_READ,CLEAN_SVCCHAN,`
`FORCE_000D_EVENTS,SCAFFOLD_MARCH,FORCE_TASK14_READY,MARCH,MODE4_EVENTS,`
`FORCE_UIEVENT,FORCE_WINTYPE,MMI_IDLE(+_DREADY,_MS),SIM_PRESENT,BOOTPATH}`,
`FORCE_IDLE`, `FORCE_IDLE_JUMP(+_MS,_AFTER)`, `FORCE_OUTCOME`. These were all
dead-end levers from the 000d / startup-march / MMI-idle-force / bootpath digs —
every one refuted or superseded by a faithful model. Orphaned member vars removed
with them (`m_mode4_*`, `m_mmi_idle_forced`, dead startup-scaffold step counters).

**Superseded SIM models** (historically replaced by `MODEL_SIM_CARD`): `MODEL_SIM_FILE`,
`MODEL_SIM_LOOP` (+ its `SIM_LOOP_*` sub-knobs), `MODEL_SIM_RESPONDER`. The
register-level `MODEL_SIM_ATR` and `MODEL_SIM_CARD` were retained at this checkpoint.
Both were removed after `nokia_sim_card_device` reproduced their useful ATR, T=0,
selected-file, FCP and EF behavior through the register/FIQ boundary. `SIM_PROFILE`
remains hardware-scenario configuration for the disabled-device path.

**`TRACE_*` taps thinned 46 → 5.** Kept the ones that document the working boot
across its layers: `TRACE_CCONT_READ` (power/ADC), `TRACE_LIMP`/`TRACE_LIMP2` (the
startup limp), `TRACE_CSCMD` (contact-service command inventory), `TRACE_RESAVAIL`
(display-resource availability). The ~40 one-off investigation taps
(disp/mmi/render/sim-uart/scheduler/service traces) were removed — the findings
they produced live in the docs.

**Tools:** `tools/fwdis.py` (capstone-raw disassembler) removed; `tools/disrom.py`
(Thumb-1-correct, resolves swap16 pool literals) is the replacement.

Net: the surviving `NOKI3210_*` knobs are **hardware config** (display/ADC/battery/
timers/EEPROM/power-IRQ/…), the **faithful opt-in `MODEL_*` stack** that reaches
"Insert SIM card", and **five diagnostic taps**.

## 2026-07 tidy (post interactive-handoff arc)

The interactive-handoff investigation added a batch of forcing shims + one-off taps;
after the message-gated handoff was isolated behind a firmware bridge, the throwaway forces were
retired (git-recoverable):

- **`EXPERIMENT_VBAT_GATE_PASS`** — forced the VBAT gate `0x2a6942` read 1→0. Its
  diagnostic role was absorbed by `MODEL_STARTUP_REPORTS`, which bridges the
  voltage-confirmation result at `0x27139e`.
- **`EXPERIMENT_FORCE_CODE7`** — forced the code-7 getmsg return. Superseded by
  `MODEL_STARTUP_REPORTS` feeding code 7 and the whole report sequence through
  the firmware mailbox path. This remains a bridge, not an organic producer.
- **`VBAT_RAW`** — overrode the raw VBAT reading at `0x27cc80`; never reached the
  confirming classification (the classifier is a multi-comparison, not linear in the
  reading), and is moot now the gate is modelled directly.
- One-off TRACE_HANDOFF sub-taps (`handoff4:` mode-0x04 entry, `gate0d:` mode-0d gate)
  removed — their findings are in `docs/interactive_handoff.md`.

**Kept at that checkpoint:** `MODEL_STARTUP_REPORTS` (+ `STARTUP_REPORTS_MS`) as
a quarantined diagnostic bridge; the curated
`TRACE_HANDOFF` seam set (dispatcher/mode/checklist, mode transitions, mailbox-post
inventory, VBAT-gate byte, interactive-init/idle markers).

## 2026-07 SIM-lifecycle-pivot cleanup

After moves 1+2 (docs/sim_emulator_scope.md "SIM-init sequencer" section) reframed the
target from "operator-idle / RF" to **milestone 2 (offline no-service menu)**, the
DSP/network and resource-content threads were deprioritised and their forcing shims +
one-off diagnostic taps retired (all git-recoverable):

- **`MODEL_RES_ENABLE`** (+ `RES_ENABLE_VAL/MS/MSGSZ/FILL`) — the cmd-0x70 resource-enable
  trampoline. Proven necessary-but-insufficient for the operator-idle window (milestone 3):
  registering the idle window `0x4c22` lets `display_idle` acquire it but the render then
  blanks on unbacked content classes (docs/resource_providers.md). Not needed for milestone 2.
- **`EXPERIMENT_UIINIT_SKIP`** — no-op'd the mode-0xc display-init call. The mode-0xc /
  coherent-boot thread it probed is understood; superseded.
- **`TRACE_DSPMSG` / `TRACE_DSPDRV` / `TRACE_DSPIO`** — the L1↔DSP mailbox / DSP-interface
  taps. The DSP protocol is mapped and dormant (docs/dsp_interface.md); the thread is
  deprioritised until (if ever) a DSP-L1 model exists to exercise it.
- **`TRACE_RESAVAIL`** and the `TRACE_MMIVM` sub-hooks for the closed digs (resource-get
  `0x4c22` result, `resget` availability `0x2b2588`, the camped-state service snapshot) —
  the resource-content and camped-state threads are bottomed out (docs/interactive_handoff.md).

**Kept at that checkpoint:** `MODEL_SIM_CARD`'s `sim_apdu` trace (the SIM APDU stream — the
move-2 tool; now superseded by `TRACE_SIM_RX` on the stateful device), the `TRACE_MMIVM`
**t6cmd** tap (task-6 display-command stream = the
"first content-window push" oracle) + the event-stream / `display_idle`-entry taps,
`TRACE_HANDOFF` (task-1 mode / checklist), `TRACE_TASKS`, `TRACE_CCONT_READ` (incl. RTC),
`TRACE_CSCMD`, `TRACE_LIMP/LIMP2`, and `POST_READY_KEY` (the key-nav oracle).

`SIM_CARD_CLEAR_NOSIM` and the single-EF injection were retained temporarily at this point as a
known non-faithful boot reference. They were removed in the later cleanup below after the ring-2
multi-file responder had preserved the useful transport behavior.

## 2026-07-11 GSM/SIM research-checkpoint cleanup

The organic registration investigation was banked at commit `9ab5ef2`, including its negative
results and exact firmware boundary map. The following isolation probes were then deleted rather
than allowed to become implied hardware behavior:

- `MODEL_SIM_INIT_KICK`, `SIM_REG_BOOTSTRAP`, `SIM_REG_DEFER119A`, and `SIM_REG_REARM`;
- `MODEL_SIM_1196_HANDSHAKE`, `MODEL_SIM_REG_COMMIT`, and `MODEL_SIM_REG_ROUTE`;
- `SIM_CARD_CLEAR_NOSIM` and its direct write to the firmware no-SIM flag;
- `MODEL_DISPLAY_PEER_PROBE`, `PROBE_SIM_CONFIG_READY`, and
  `PROBE_SIM_CAPABILITY_MASK`, including their synthetic DSP frames and SIM/config RAM writes.

All private trampoline, scratch-session, route, and display-probe state was removed with those
knobs. No direct writes remain to the registration/configuration targets `0x111c64`, `0x111c69`,
`0x111c6f`, `0x111c76`, `0x111c79`, `0x111c96`, `0x111c97`, or `0x10d126`.

The dense `TRACE_SIMKICK`, `TRACE_SIM_SERVER`, and `TRACE_SIM_CONFIG` investigations were also
removed. Their conclusions are recorded in `sim_registration.md`. The retained
`TRACE_GSM_LOWER`/`TRACE_GSM_SERVICE` taps observe the current external service boundary, while
the compact startup/MMI traces continue to serve established boot milestones.

`MODEL_GSM_SERVICE` was subsequently removed as another disproven execution shim. It watched
service `0x0b`, saved the CPU context, and force-called generic-framework helpers `0x263154` and
`0x2635ac`. Static and runtime evidence instead places the required completion on
the already-organic service-5 callback `0x2618e8`. Later control-flow review corrected the queued-
object interpretation: its `0x05e8` branch accepts an argumentless status, while the organic
publisher path and downstream readiness remain unresolved. The observational framework traces remain.

The useful substrate remains: register/ring-2 SIM delivery, the multi-file APDU responder,
EEPROM I2C behavior, DSP service, CCONT behavior, and request-driven peer prototypes. The default
oracle remains `d8a9a7a58e587be8`; an integrated-model smoke run still reaches the natural
ICCID/ECC/PHASE transaction sequence without any deleted probe.
