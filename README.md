# nokia-dct3-re

Reverse-engineering toolkit for **Nokia DCT3-era phones** (TI MAD2-based), starting
with the **Nokia 3210 (NSE-8/9)**. It pairs a [MAME](https://github.com/mamedev/mame)
driver with disassembly tools, headless Ghidra scripts, a symbol map, and detailed
analysis docs — enough to boot the firmware in emulation and reason about why it does
what it does.

The headline result: **CONTACT SERVICE is cleared.** A blank/un-provisioned 3210 halts at
the CONTACT SERVICE screen because it is unprovisioned and the service bring-up correctly
refuses to complete — and the whole chain, from the watchdog symptom down to the root, is now
not only reverse-engineered end-to-end but **emulated by five faithful, opt-in models** that
make the boot complete the service layer and leave the CONTACT SERVICE screen. See
`docs/service_bootstrap.md`.

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

**Phase 2 — boot → idle — is mapped end-to-end, and the `000d` boundary is diagnosed to the subsystem.**
Past CONTACT SERVICE the boot runs a chain of startup modes (`000d → 0004 → … → 0007 → readiness loop`)
and holds at the **mode-`000d` limp**. The gate advances only when the startup task *receives* four
standalone events `0x14/0x15/0x16/0x17`; `0x15` never arrives as a raw code. Chasing that to the root
(raw disassembly + a battery of emulator experiments) landed on a precise diagnosis: `000d` waits on a
**service-transport peer reply** (`task_285` / remote node `0x18`) — the **same subsystem CONTACT SERVICE
needed** — which never answers on our blank/peerless boot, so a timer keeps re-arming `0x15` long and it
starves. Forcing the events (diagnostic, *not* faithful) advances `000d → 0004` and renders the first
**battery-present idle screen** (frame `4235fa`).

The **CCONT power-management subsystem is faithfully modelled** (`docs/ccont_subsystem.md`): an explicit
ADC-source model, the interrupt→event protocol decoded, the `0x77xx` PMM messages mapped, its env-knob
cluster retired into device state/constants. The measurement path was confirmed *already faithful*
(synchronous ADC + the firmware's own timer-poll), and separately ruled **out** as the `000d` cause
(the CCONT IRQ status settles cleanly).

**We built the faithful fix and it didn't land — the practical bottom for this dump.** A
`MODEL_SVC_RESPONDER`-class responder for `task_285` was built to the traced spec, but its trigger
address is never executed: the scheduler bodies decompile to Thumb-garbage, so the exact code addresses
don't survive even though the *subsystem* identification does. **Six faithful levers were tested; all six
failed** — only the unfaithful event-forcing advances the boot (full scorecard + post-mortem in
`docs/ccont_subsystem.md`). This is **not a hardware wall** (that framing was tested and retracted): the
peer is software, but closing `000d` faithfully needs a **cleaner reference** — a working-phone boot/RAM
trace to observe the real peer reply, or a better decompilation of the scheduler path — rather than
another lever on this degraded corpus. (Reaching a fully live idle beyond `000d` additionally needs
provisioned data the dump lacks, e.g. the service-channel map.)

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
