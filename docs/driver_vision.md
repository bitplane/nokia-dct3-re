# Driver vision — what each peripheral should look like, and how we get there

This is the north star for `driver/nokia_3310.cpp`: the *target shape* of each
peripheral model, an honest assessment of where the code is today, and the
**knob → model retirement map** that turns the current firmware-bring-up
scaffolding into a clean hardware-emulation driver.

It complements `driver_structure.md` (the "thin handler + quarantined research
helper" convention) and `hardware_atlas.md` (the firmware↔hardware boundary).
Where `driver_structure.md` says *how* to keep hacks quarantined, this says *what
each quarantined cluster should become*.

## The one-number health gauge

A clean driver should not need a wall of environment variables to reach its first
screen. Today `make run` bakes in **13 `NOKI3210_*` knobs** (down from 32: −8 MBUS,
then −11 more via a leave-one-out audit that pulled the baked-in traces and the
inert forcing shims). That count is the **hack-debt gauge** — each knob substitutes
for hardware/firmware state we haven't modelled yet. The goal is to drive it down
to a handful of genuine *configuration* (display variant, battery state, clock
rates), deleting the rest as the gates they stand in for are modelled.

Current rough split of the ~3,500-line driver:
- **~⅔ real hardware models** — CCONT serial protocol, MAD2 register file, I²C
  EEPROM, PCD8544 LCD, keypad matrix, timers, IRQ/FIQ controller. These read like
  a real MAME driver and stay.
- **~¼ quarantined research scaffolding** (~790 lines, down from ~1,200: −320 MBUS,
  −90 forcing shims): `flash_firmware_hooks` (~725), `ram_w/r_firmware_overrides`
  (~70), plus the forcing/experiment knobs. This is explicitly banner'd "NOT hardware
  behaviour" and is meant to **shrink to zero**.

The discipline already exists (all `FORCE_*` result-forcing was audited out; three
faithful `MODEL_*` replaced `EXPERIMENT_*` forces). This doc extends that
trajectory to every subsystem.

## Target shape, per peripheral

### CCONT — power / ADC / RTC / charger (`nokia_ccont_r/w`)
**Today:** a real serial-protocol device with a register file — *plus* a scatter of
external shims: `BATTERY_PROFILE`, `CCONT_EVENT15_DELAY`, `ADC_PROFILE`,
`ADC5_AFTER_READY*`, `CCONT_BOOT_STATUS`, `MODEL_CCONT_PRESENT`.

**Target:** CCONT owns a small **`charger/battery state` object** that:
- drives the ADC channels (0–7) from modelled values (battery V/temp/type, charger
  V, RSSI…) instead of `ADC*` env overrides — a profile selects the *scenario*
  (no-charger/charging/charged), not individual registers;
- runs the **measurement state machine** that posts the battery-measurement-complete
  events (the `0x14/0x15/0x16/0x17` startup sub-events — see `service_bootstrap.md`
  "Beyond the gate") on a timer / IRQ sequence, the way the real CCONT does;
- serves RTC from the host clock (already partly done).

**Retires:** `BATTERY_PROFILE`, `CCONT_EVENT15_DELAY`, `STARTUP_EVENT15_DELAY_CLAMP`,
`ADC5_AFTER_READY*`, `CCONT_BOOT_STATUS`, `MODEL_CCONT_PRESENT`, `CCONT_IRQ_*`.
This is the **next gate** (the limp) and the clearest "we now know the shape" case.

### MAD2 I/O — the on-chip peripheral register file (`mad2_io_r/w`)
**Today:** a real register file organized by block (CTSI/PUP/KBGPIO/GENSIO/SIMI/UIF)
with per-register descriptions. The cleanest large model in the driver.
**Target:** unchanged in shape — keep filling register behaviour in as each block is
exercised. The GENSIO SELECT lines (RF synth / codec control) are the main unknown,
and are gated behind the boot reaching RF/audio.
**Retires:** `MAD2_SOFT_RESET*` (research), `FIQ8_HZ` (→ proper timer).

### EEPROM — 24C128 (`serial_eeprom_*` + `eeprom_*`)
**Today:** a real I²C model with a checksum **profile** overlay (`EEPROM_PROFILE=selftest`).
**Target:** keep the profile-as-data abstraction — it's the right shape. A provisioned
phone is a *data file*, not code. Long-term: load a real (user-supplied) EEPROM image;
the `selftest` overlay is the synthetic stand-in.
**Retires:** nothing forcibly — the profile system is legitimate config.

### MBUS / service transport (in `ram_*`/`flash_firmware_hooks` + FIQ plumbing)
**Done — deleted, not modelled.** This was the biggest hack cluster (~15 `MBUS_D0_*`/
`MBUS_RX_*`/`COMPLETE_MBUS`/`TRACE_MBUS_FLOW` knobs, ~320 driver lines of hand-tuned D0
frame *injection* keyed on internal firmware RAM flags — including a "trace" knob with
firmware-RAM side-effects). An empirical audit proved it **inert**: with the entire
cluster removed the oracle stays `d8a9a7` **and** the deep boot (CONTACT SERVICE cleared)
reaches the mode-`000d` limp with a **byte-identical frame set**. It was dead because
`MODEL_DSP_SERVICE` supersedes the lower-service handshake and task 08 (the D0 frame
processor) is **never resumed** — so even a perfectly faithful bus peer's reply would
pile up undrained and change nothing. Retiring the knobs was therefore a **deletion**,
not a build (commit removes 8 `BOOT_ENV` knobs + ~320 lines; the real MBUS *controller* —
`schedule_mbus_fiq`/`complete_mbus_transfer`/reg 0x18–0x1a FIQ plumbing — stays).
**If ever needed:** a real MBUS peer only becomes meaningful once the resume-gate that
drains task 08 is cleared (itself data-blocked). At that point the faithful TX→RX echo
generator (`mbus_generated_response_byte`, preserved in git history) is the seed.

### Startup / charger / battery state machine (firmware-side, driven via `ram_*`)
**Today:** advanced via forcing (`BATTERY_PROFILE` event injection, `POST_READY_KEY*`,
`CONTACT_*` flags). This session mapped its structure: a 14-entry mode jump table keyed
on `FW_STARTUP_MODE`, with per-mode flag-accumulator gates.
**Target:** this is *firmware* state, not hardware — so it shouldn't be "modelled" at
all. It should simply **advance on its own once the hardware events it waits for are
real** (CCONT measurement events, MBUS frames). I.e. fixing CCONT + MBUS deletes the
need to force the startup machine.
**Retires:** `BATTERY_PROFILE` (shared with CCONT), `POST_READY_KEY*`,
`SERVICE72_RESPONSE_STATUS`, `SKIP_SERVICE_E2_REARM`, `MODEL_SVC_RESPONDER` (once node
0x18 answers via a real transport). Already **deleted** (leave-one-out audit, inert vs
oracle + deep boot): `CONTACT_STATUS65_FLAGS`, `CONTACT_CHANNEL_MAP_FLAGS`,
`CONTACT_D9_TIMEOUT_DELAY`, `SUPPRESS_SIM_CONTEXT_EVENTS`, `STARTUP_EVENT15_DELAY_CLAMP`.

### DSP — GSM L1 + audio (`dsp_ram_r/w`, `mad2_dspif_*`)
**Today:** a `return 0` stub with a faked mailbox corner (`MODEL_DSP_SERVICE` for the
boot service handshake).
**Target (interface known, internals not):** the MCU↔DSP boundary is a **shared-RAM
mailbox** (`0x10000`) + the **DSPIF control register** (`0x30000`). The model should
become a small mailbox responder that answers the boot service handshake faithfully,
then — *after the boot reaches code that exercises the real GSM/audio driver* — grow
into a protocol model. **Cannot be made "nice" yet**: the big driver layer
(`0x2b6xxx–0x2c8xxx`, ~287 DSPIF + ~444 shared-RAM refs) isn't run until past the limp.
**Retires (eventually):** `MODEL_DSP_SERVICE`, `EXPERIMENT_DSP_IRQ4*`.

### SIM (`SIMI` UART) and RF / synth
**Today:** stubs with config knobs (`SIM_*`), untouched at boot-to-limp.
**Target:** unknown — not reached yet. Modelling now would be guessing. RF likely sits
on a GENSIO SELECT line; the SIM is a UART peer. Both are post-idle frontiers.
**Retires:** n/a yet.

## Knob → model retirement map (the cleanup backlog, in dependency order)

| # | Model to build | Knobs it retires | Gate it clears | Status |
|---|---|---|---|---|
| 1 | ~~**CCONT battery-measurement events**~~ → **`000d` = a missing service-transport peer reply (`task_285` = node-`0x18`)** | `FORCE_000D_EVENTS`, `CCONT_EVENT15_DELAY` (diagnostics); would add a `MODEL_285_RESPONDER` | the **limp** (mode `000d`) | **peer identified — modellable, one link untested** (raw-disasm map). Gate needs standalone `0x15`; posted delayed-only, never reaches the mailbox as a raw code — **not** starvation (fails even at delay=1), not CCONT-IRQ (`r4=0`). The `0xd5` poll = `service_transport_build_d5_notify`; `task_285` (@`0x11228c`) is the **service-transport / node-`0x18`** layer — same subsystem as CONTACT SERVICE — retrying forever because `[0x11213f]==0` aborts the request. Fix: a `MODEL_SVC_RESPONDER`-class injection of the terminal PDU (`0x0dc3`/`0x0dc4`) into `task_285`'s mailbox. **Not hardware** (retracted). Open risk: whether completion supplies the context `0x15` delivery needs, and possible chain to node-`0x18` frame details. See `ccont_subsystem.md`. |
| 2 | ~~**MBUS service-bus peer**~~ → **deleted as dead injection** | ~~`MBUS_D0_*`/`MBUS_RX_*`/`COMPLETE_MBUS`/`TRACE_MBUS_FLOW`~~ (~15 knobs, 8 in `BOOT_ENV`; ~320 lines) | — | **done** — audited **inert** (oracle `d8a9a7` + deep-boot frame-set both byte-identical with it removed). A peer is moot until the resume-gate drains task 08 (data-blocked); real MBUS *controller* kept. |
| 3 | **CCONT device** (`adc_src[]` model + constants) | ~~`ADC_PROFILE`/`ADC0-7`~~ (now feed `adc_src[]`), ~~`CCONT_IRQ_LINE`~~, ~~`CCONT_BOOT_STATUS`~~, ~~`CCONT_IRQ_SEQUENCE`~~, ~~`CCONT_IRQ_STATUS`~~, ~~`ADC5_AFTER_READY*`~~, ~~`FORCE_SVC_CHANNEL`~~ (constants/removed); `MODEL_CCONT_PRESENT` (kept, opt-in) | — | **largely done** — 7 knobs retired (`3476eb0`/`c50f368`/`e8b63b3`/`1dea8c2`). Measurement path confirmed *already faithful* (sync ADC + firmware timer-poll); no measurement SM to build for the boot. Async charger/RTC IRQs are post-idle. |
| 4 | **Startup machine self-advance** | ~~`CONTACT_STATUS65_FLAGS`/`CONTACT_CHANNEL_MAP_FLAGS`~~ (were 0-ref orphans), ~~`CONTACT_D9_TIMEOUT_DELAY`~~, ~~`SUPPRESS_SIM_CONTEXT_EVENTS`~~, ~~`STARTUP_EVENT15_DELAY_CLAMP`~~ (deleted); `POST_READY_KEY*`, `MODEL_SVC_RESPONDER` (kept) | falls out of #1+#2 | **partly done** — 5 forcing shims deleted after a leave-one-out audit proved them inert vs both the oracle and the deep boot; the remaining knobs wait on the boot self-advancing. `CCONT_EVENT15_DELAY` **kept** (deep-load-bearing). |
| 5 | **DSP mailbox responder** | `MODEL_DSP_SERVICE`, `EXPERIMENT_DSP_IRQ4*` | DSP handshake → then GSM/audio | gated behind limp |
| — | (keep) `TRACE_*` (13) | — | diagnostics, not debt | keep, opt-in — **pulled from `BOOT_ENV`** so they're actually opt-in (were 6 baked in) |
| — | (keep) `DISPLAY_TYPE`, `TIMER*_HZ`, `POWER_IRQ_*`, `ADC_PROFILE`, `BATTERY_PROFILE`, `EEPROM_PROFILE`, `SNAPSHOT_DIR` | — | genuine config | keep |

**Reading the map:** the retirements are *ordered by the boot* — #1 (the limp) unblocks
the firmware path that makes #4 observable, which unblocks #5. "Make it nice" is not a
separate refactor; it is the trail of deleted knobs left behind as each gate is cleared
faithfully — or, as rows #2 and #4 showed, the trail left when a leave-one-out audit proves
knobs were never load-bearing. The gauge to watch is the `make run` knob count: **13 today**
(was 32 — −8 MBUS, −11 traces+shims), and it should keep falling as the remaining rows land.

## Principles (carried from `best-practices.md` + this project's grain)

- **Model the missing hardware/NV state; never re-introduce result forcing.** A shim is a
  debt marker, not a fix.
- **Config is data, behaviour is code.** Battery/SIM/EEPROM *scenarios* belong in profiles
  (data); the chip *behaviour* belongs in the device model.
- **The oracle is sacred.** Every refactor must keep `make verify` → `d8a9a7` (default boot
  byte-identical) and the structural boot markers unchanged.
- **Shrink the quarantine.** Progress is measured by lines deleted from
  `flash_firmware_hooks` / `ram_*_firmware_overrides` and knobs removed from `BOOT_ENV`,
  not just by frames advanced.
