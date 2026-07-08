# nokia-dct3-re

Reverse-engineering toolkit for **Nokia DCT3-era phones** (TI MAD2-based), starting
with the **Nokia 3210 (NSE-8/9)**. It pairs a [MAME](https://github.com/mamedev/mame)
driver with disassembly tools, headless Ghidra scripts, a symbol map, and detailed
analysis docs — enough to boot the firmware in emulation and reason about why it does
what it does.

The headline result: **a blank 3210 boots to "Insert SIM card"** — the correct home state
for a DCT3 phone with no SIM inserted. A blank/un-provisioned phone first halts at the
CONTACT SERVICE screen; that whole chain is reverse-engineered end-to-end and cleared by
**faithful, opt-in models** (`docs/service_bootstrap.md`). From there the boot was carried
the rest of the way: the `000d` startup wall, an 11-subsystem readiness barrier, and a
startup-supervisor **busy-wait on a service-channel-busy bit** — the single root cause that
gated everything — were traced and fixed with one more opt-in model, cascading all the way to
a rendered **"Insert SIM card"** MMI screen. See `docs/boot_to_insert_sim.md`. From there the SIM
**read conversation** was reverse-engineered end-to-end *and modelled faithfully*: `MODEL_SIM_CARD`
(`docs/sim_emulator_scope.md`) is a message-layer virtual SIM that delivers a genuine ATR, echoes
the ISO-7816 PPS, and answers the phone's own GSM 11.11 T=0 command stream
(`SELECT → GET_RESPONSE → READ`, with a synthetic EF file-control block + content) to completion —
so the firmware **accepts the SIM** (the no-SIM reject decision at `0x27ea88`/`[0x111c64]` is cleared
through the read, not forced). That is the whole SIM-conversation problem, solved.

Reaching the classic **operator-idle** home screen is a *separate* wall, now mapped in detail but
not broken: the interactive **MMI application layer is dormant** on the reconstructed boot — the idle
"dirty" flag (`0x1116fd`), the display-ready flag (`0x11fee4`), the window-state machine (`0x297ed8`),
and the display-resource path (`0x2b257e`/`0x224c`) are never activated because the phone's top-level
state never declares "ready for interactive UI" and never emits the MMI window-creation messages.
That is a coherent top-level-state problem (like CONTACT SERVICE and `000d` were), upstream of the
SIM. `docs/sim_subsystem.md` keeps the original UART-level Phase-1 map; the SIM build log and the
MMI/idle dig are in `docs/sim_emulator_scope.md`.

The default boot (no models) still reproduces the CONTACT SERVICE oracle frame byte-for-byte;
every model is opt-in.

## ⚠️ No firmware here — bring your own

This repo contains **no copyrighted firmware, dumps, or proprietary tools** — only
original tooling, annotations, and analysis. You supply your own legitimately-obtained
3210 flash image; see [`roms/README.md`](roms/README.md) for the source and the
SHA-256 to verify against. Without a matching dump, the absolute addresses in the docs
and symbol map won't line up.

## Layout

```
driver/nokia_3310.cpp     the MAME driver (BSD-3-Clause; overlaid onto a fresh MAME clone at build)
tools/                  Thumb-disasm / xref helpers (capstone); the MAME input-exerciser Lua
ghidra/scripts/         headless Ghidra naming/export scripts
ghidra/symbols/3210.csv address -> name -> kind, exported from the naming script (use without Ghidra)
docs/                   the reverse-engineering write-ups (see below)
roms/                   bring-your-own firmware (git-ignored) + how-to + verification SHAs
```

Build state (the MAME checkout, `.venv`, run outputs, frames) and anything
derived from the firmware (Ghidra decompiled listings) are git-ignored.

## Key docs

- [`docs/service_bootstrap.md`](docs/service_bootstrap.md) — the main result: the full
  CONTACT SERVICE chain, the validated experiments, the **DSP/PM/MBUS service layer**,
  and the **forward "provisioning model" plan**. Start with its executive summary.
- [`docs/eeprom_analysis.md`](docs/eeprom_analysis.md) — EEPROM block layout + the
  additive-checksum algorithm (cross-validated with NokTool).
- [`docs/driver_structure.md`](docs/driver_structure.md) — how the driver is organised
  (thin hardware handlers + quarantined research hooks/traces).
- [`docs/driver_vision.md`](docs/driver_vision.md) — the per-peripheral target shape and
  the knob→model retirement map (turning the bring-up scaffolding into a clean driver).
- [`docs/ccont_subsystem.md`](docs/ccont_subsystem.md) — the CCONT power-management subsystem:
  confidence-tagged protocol map (serial regs, ADC, the interrupt→event/message fan-out) and
  the target `ccont_device` component. The first subsystem to model faithfully.
- [`docs/scheduler_delivery.md`](docs/scheduler_delivery.md) — the RTOS event/message delivery path
  from **ground-truth disassembly**: the immediate-vs-delayed post split, the `0x2d71a8` recode table,
  the mode-`000d` gate, why the raw-`0x15` producer is dead-gated, and the **3310 cross-firmware**
  confirmation that closes the `000d` limp (it's a faked-boot artifact). Includes the reusable
  cross-firmware comparison method (byteswap + entry-signature search).
- [`docs/battery_classifier_analysis.md`](docs/battery_classifier_analysis.md),
  [`docs/static_branch_map.md`](docs/static_branch_map.md),
  [`docs/firmware_code_maps.md`](docs/firmware_code_maps.md) — supporting analysis.
- [`docs/hardware_atlas.md`](docs/hardware_atlas.md) — the firmware↔hardware boundary:
  every MMIO region the firmware touches, tagged emulated/partial/**stub**, and what the
  boot reaches vs not (the phase-2 map; the DSP interface is the next deep-dive).
- [`docs/tooling.md`](docs/tooling.md) — the in-repo tools and external references
  (NokTool, IDR).

## Status

**Phase 1 — CONTACT SERVICE — is complete.** The boot stall is fully understood *and* cleared
by five faithful, opt-in models (DSP service handshake, CCONT present-bit, EEPROM config +
tune/security checksums, and the node-`0x18` service responder). With them on, the boot completes
the contact-service and leaves the CONTACT SERVICE screen; with them off, the default boot still
reproduces the regression oracle (`make verify` → frame `d8a9a7`). The full chain and every model
are documented in `docs/service_bootstrap.md` (start at "Status & model stack").

**Phase 2 — boot → idle — is mapped end-to-end, and the mode-`000d` limp is fully reverse-engineered.**
Past CONTACT SERVICE the boot runs a chain of startup modes (`000d → 0004 → … → 0007 → readiness loop`)
and holds at the **mode-`000d` limp**, which advances only when the startup task *receives* the four raw
startup events `0x14/0x15/0x16/0x17` (flag `[0x112399]` reaches `0x0f`); `0x15` never arrives. Ground-truth
disassembly (`docs/scheduler_delivery.md`, via `tools/disrom.py` on the swap16 image — the Ghidra
*decompiler* output is Thumb-garbage for this path, but the *disassembly* is clean) traced the exact
delivery machinery: the scheduler has an **immediate** post path (raw code → the task's ring) and a
**delayed** path that **recodes** every event `k` to `0xc0+k` via a table at `0x2d71a8` — so a
delayed-posted `0x15` always surfaces as `0xd5`, never as raw `0x15`. `0x15` has **no** immediate producer;
the one candidate — the contact-service command handler `0x236bac`, reached by command `0x65` — has its
`0x15` emit **dead-gated** (`0x2a674c` returns 1 for its even argument, skipping the emit on every phone).

**Cross-firmware confirmed — and it reframes the limp.** Diffing the sibling **Nokia 3310 (NHM-5 v06.39)**
shows that dead-gate is **byte-identical** — shared DCT3 firmware design, not a 3210 bug. Since real 3310s
boot to idle with the *same* dead `0x15`-emit, a normal DCT3 boot does **not** depend on this producer — so
the mode-`000d` limp (waiting for a raw `0x15`) is an **artifact of the blank + faked boot**, not the path a
real phone takes. The `000d` code is completely and correctly reverse-engineered; the *stall* is a property
of how our unprovisioned/faked reconstruction reaches that state, not a missing model or a hardware gate.
The diagnostic `EXPERIMENT_FORCE_000D_EVENTS` (not faithful) advances `000d → 0004` and renders the first
**battery-present idle screen** (frame `4235fa`). The full mechanism, the refuted faithful levers, and the
reusable cross-firmware method are in `docs/scheduler_delivery.md`.

The **CCONT power-management subsystem is faithfully modelled** (`docs/ccont_subsystem.md`): an explicit
ADC-source model, the interrupt→event protocol decoded, the `0x77xx` PMM messages mapped, its env-knob
cluster retired into device state/constants. The measurement path was confirmed *already faithful*
(synchronous ADC + the firmware's own timer-poll), and separately ruled **out** as the `000d` cause
(the CCONT IRQ status settles cleanly).

## Reproducing

Three things pin reproducibility anywhere: the **MAME commit** (`MAME_COMMIT` in the
Makefile), the **firmware SHA-256** (`roms/README.md`), and the **oracle frame hash**
(`d8a9a7…`) the boot must reach. Once your dump is in `roms/` (see `roms/README.md`):

```
make build      # clone MAME at the pin, overlay driver/nokia_3310.cpp, build
make verify     # boot to CONTACT SERVICE, check the LCD frame SHA == the oracle
make swap16     # derive the halfword-swapped image the static tools/Ghidra use
```

Every `NOKI3210_*` knob the driver reads is overridable on the command line; the
canonical oracle profile is baked into `make run`. Two useful profiles beyond the oracle:

```
# Clear CONTACT SERVICE and reach the mode-000d limp (Phase 1's five models):
make run NOKI3210_MODEL_DSP_SERVICE=1 NOKI3210_MODEL_CCONT_PRESENT=1 NOKI3210_MODEL_SVC_RESPONDER=1

# Diagnostic preview of the first battery-idle screen past the 000d gate (frame 4235fa;
# forces events the firmware never emits on this path — NOT a faithful boot):
make run NOKI3210_MODEL_DSP_SERVICE=1 NOKI3210_MODEL_CCONT_PRESENT=1 \
         NOKI3210_MODEL_SVC_RESPONDER=1 NOKI3210_EXPERIMENT_FORCE_000D_EVENTS=1
```

## License

BSD-3-Clause (see [`LICENSE`](LICENSE)). The MAME driver keeps its upstream BSD-3-Clause
header; MAME-as-a-whole is GPL-2.0-or-later but is not redistributed here (fetched from
upstream and overlaid at build time).
