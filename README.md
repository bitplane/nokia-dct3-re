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

**Phase 2 — boot → idle — is mapped end-to-end, and the boundary is found.** Past CONTACT SERVICE the
boot runs a chain of startup modes (`000d → 0004 → … → 0007 → readiness loop`) and holds at the
**mode-`000d` limp**. That limp is now **fully reverse-engineered** (see `docs/service_bootstrap.md`
"Beyond the gate"): it is the CCONT **power-on / charger measurement sweep** — mode `000d` advances only
when four sub-events `0x14/0x15/0x16/0x17` are delivered to the startup task, and `0x15`/`0x16` never
arrive because their producers post them on a **delayed scheduler channel** while `0x14`/`0x17` use a
direct one (a structural firmware fact, not a missing model). Injecting the missing events advances
`000d → 0004` and renders the first real **battery-present idle screen** (frame `4235fa`).

The **CCONT power-management subsystem is now faithfully modelled** (`docs/ccont_subsystem.md`): an
explicit ADC-source model, the interrupt→event protocol decoded, the `0x77xx` PMM messages mapped, and
its env-knob cluster retired into device state/constants. The measurement path was confirmed *already
faithful* (synchronous ADC + the firmware's own timer-poll), so there is no measurement state machine to
add for the boot.

**The remaining boundary is data, not mechanism.** Reaching idle is blocked on **provisioned
configuration** a real phone carries but the firmware dump does not — chiefly the service-response
**channel-map** (command `0x70`) that enables the service channels. We currently clear CONTACT SERVICE by
*faking* one node response (the responder trampoline), which leaves the post-CONTACT-SERVICE region in a
state the real firmware never occupies; faithful progress past it needs the real provisioned data,
realistically a **real-hardware service-bus capture** (the runtime analogue of "bring your own dump").
This is documented with the dead-ends and disproofs in `docs/ccont_subsystem.md`.

## Reproducing

Three things pin reproducibility anywhere: the **MAME commit** (`MAME_COMMIT` in the
Makefile), the **firmware SHA-256** (`roms/README.md`), and the **oracle frame hash**
(`d8a9a7…`) the boot must reach. Once your dump is in `roms/` (see `roms/README.md`):

```
make build      # clone MAME at the pin, overlay driver/nokia_3310.cpp, build
make verify     # boot to CONTACT SERVICE, check the LCD frame SHA == the oracle
make swap16     # derive the halfword-swapped image the static tools/Ghidra use
```

Every `NOKI3210_*` knob the driver reads is overridable on the command line
(`make run NOKI3210_TRACE_PM=1`); the canonical oracle profile is baked into `make run`.

## License

BSD-3-Clause (see [`LICENSE`](LICENSE)). The MAME driver keeps its upstream BSD-3-Clause
header; MAME-as-a-whole is GPL-2.0-or-later but is not redistributed here (fetched from
upstream and overlaid at build time).
