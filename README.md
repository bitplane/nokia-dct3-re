# Nokia DCT Platform Emulation

This project reverse-engineers and emulates Nokia's early digital cellular phone platforms in
[MAME](https://github.com/mamedev/mame). The reference machine is the Nokia 3210 (NSE-8/9), a
DCT3 phone built around the MAD2 baseband ASIC. The long-term aspiration is a reusable,
maintainable DCT-family architecture capable of running unmodified firmware from DCT1, DCT2,
DCT3, DCTL, and eventually DCT4 phones where the hardware is understood.

That is a research direction, not a compatibility promise. Nokia product generations differ in
ASICs, flash layout, displays, companion chips, RF, DSP firmware, product data, and application
software. The practical method is incremental:

1. Observe a real firmware/hardware boundary.
2. Map it statically and at runtime.
3. Prototype the absent peer without claiming the prototype is hardware.
4. Replace the prototype with a register, bus, mailbox, or device model.
5. Validate the component against another ROM before treating it as shared platform behavior.

The original milestone was to run Snake on a Nokia 3210. That remains a useful vertical test, but
the intended result is the platform implementation that makes Snake, T9, ringtones, the address
book, operator logos, settings, and the rest of the phone ordinary firmware behavior rather than
special cases.

## Current State

The 3210 firmware executes deeply enough to initialize the CPU, MAD2 peripherals, CCONT, display,
EEPROM paths, RTOS tasks, external-service machinery, and a substantial SIM conversation. The
default profile deterministically reaches the authentic **CONTACT SERVICE** frame and is protected
by a byte-exact LCD oracle. Opt-in peer prototypes can carry the firmware further and the ring-2
SIM responder completes natural ATR and ICCID/ECC/PHASE APDU traffic.

The phone does **not** yet reach a verified interactive desktop. The modeled
SIM completes the ordinary non-CPHS initialization pass and task 1 selects
startup mode `0x0004`. Instruction and runtime traces prove that both the
code-7 and non-code-7 branches execute the same interactive-initialization tail;
mode 4 records a later continuation rather than blocking the UI. Correct MAD2
IRQ0 keypad wiring reaches the firmware matrix scan and decoded key resources,
and the Security-code editor accepts a physical `12345` sequence.

The remaining application-level question is the ordinary unattended
UI/idle-window entrance. Report code 7 is a later power/shutdown report, and
the mapped callback, descriptor, timer, and second-softkey paths are classified
as conditional firmware lifecycles rather than missing
hardware acknowledgements. Detailed exclusions and addresses live in
[`docs/mmi_settlement.md`](docs/mmi_settlement.md) and
[`docs/mmi_layer.md`](docs/mmi_layer.md).

`make verify-frontier RUN_DIR=run_frontier SECONDS=8` is the authoritative
research baseline for this boundary. It uses the request-driven external-service peer
and ordinary SIMI/FIQ6 card model, ending in mode `0x0004` with service-session status
`0x49`, no-SIM clear, and SIM enable set. Keypad interaction is independently
covered through MAD2 IRQ0 and the firmware matrix scanner.

The surviving `MODEL_*` paths react at device or DSP-ring boundaries. They are
executable protocol hypotheses, not finished hardware emulation, but do not
call firmware handlers or write registration state. Firmware-PC conditions are
confined to diagnostic traces and the documented erased-NV display-profile
RAM-read shortcut.

## Maturity

The labels below have precise meanings:

- **Mapped**: firmware ownership and boundary are documented.
- **Prototype**: enough peer behavior exists to test the contract, usually through research hooks.
- **Partial hardware**: firmware uses ordinary registers or buses, but behavior is incomplete.
- **Component-ready**: the contract is stable enough to extract into a MAME device.
- **Validated**: the component passes the 3210 regression and a second-ROM confidence check.

| Subsystem | State | Evidence / next step |
|---|---|---|
| ARM7, flash, RAM mapping | Partial hardware | 3210 executes reliably; product layouts need cross-ROM validation. |
| MAD2 timers and IRQ/FIQ | Partial hardware | Boot-critical paths work; several timings remain calibrated assumptions. |
| PCD8544 LCD and keypad | Partial hardware | Firmware renders authentic frames; MAD2 IRQ0 reaches the matrix scanner and decoded input resources. |
| 24C128 EEPROM | Partial hardware | MAME's native I2C device is wired through GenIO and passes the oracle; provisioning and the parallel alias need validation. |
| CCONT power/ADC/RTC | Partial hardware | Extracted MAME device passes the oracle; physical ADC latency, RTC encoding, watchdog clock and board-level analog signals remain open. |
| GENSIO serial/SELECT | Partial hardware | Extracted endpoint/status/LCD/CCONT transport passes two 3210 ROMs; physical timing and SELECT-attached peers remain open. |
| SIMI controller and SIM card | Partial hardware | Separate devices compose through byte/reset callbacks; organic SIMI/FIQ6 and T=0 initialization work, while timing, errors and synthetic provisioning remain incomplete. |
| DSP mailbox/service corner | Prototype | Boot handshake works; GSM L1 and audio DSP remain unemulated. |
| Startup/external-service peers | Prototype | Request-driven behavior composes through the DSPIF, DSP HLE and external-service devices; the wider peer contract remains incomplete. |
| Interactive startup | Mapped | Keypad and editor completion work in mode 4. Callback `0x01`/`0x0367`, callback `0x10`/`0x05e7`, and task-6 selector `0x0732 -> 0x2b1e44` are conditional UI/power lifecycle paths, not cold-boot entrances. Task-5/MMI context settlement is the smallest unresolved boundary. |
| MMI/RTOS internals | Mapped | Firmware owns these; observe them rather than emulate them. |
| Audio, RF, network | Unmapped/partial | Defer until offline application boot is stable. |

A component is considered boxed off only when firmware reaches it through its ordinary interface,
it performs no firmware-state writes, timing is expressed as device behavior, save state is
supported, and it passes the reference and portability checks.

## Roadmap

### Reference 3210 milestones

1. Stable default and modeled boot regressions.
2. Organic SIM initialization and offline desktop.
3. Reliable keypad navigation and persistent settings.
4. Address book editing and text/T9 entry.
5. Ringtone playback and audio output.
6. Operator logos and other period content workflows.
7. Snake and the other built-in games.

### Platform work

1. Improve the extracted EEPROM, CCONT and GENSIO devices from observed transactions.
2. Use the Nokia 3330 (NHM-6) as the first cross-ROM confidence target once its service files are normalized reproducibly.
3. Recover MAD2 physical clocks/reset domains and identify GENSIO SELECT peers from hardware evidence.
4. Stabilize the extracted SIMI/card seam, then separate synthetic card provisioning into reusable profiles.
5. Separate DSP transport/HLE from the external-service peer, then add further DCT3 products as evidence.

New phone support is initially a portability probe. A ROM that fails early is still valuable when
it identifies a 3210-specific assumption.

## Repository Layout

```text
driver/                 MAME driver and extracted Nokia devices
tools/                  Thumb-1 disassembly and cross-ROM helpers
ghidra/scripts/         headless analysis and symbol-export scripts
ghidra/symbols/         firmware-specific symbol maps
docs/                   hardware atlas and RE reports
roms/                   ignored, user-supplied firmware plus verification instructions
```

Start with:

- [`docs/README.md`](docs/README.md) for the documentation map and authority rules.
- [`docs/hardware_atlas.md`](docs/hardware_atlas.md) for the firmware/hardware boundary.
- [`docs/driver_structure.md`](docs/driver_structure.md) for implementation rules.
- [`docs/driver_vision.md`](docs/driver_vision.md) for the component retirement path.
- [`docs/service_bootstrap.md`](docs/service_bootstrap.md) for service-session startup.
- [`docs/mmi_settlement.md`](docs/mmi_settlement.md) for the current MMI settlement boundary.
- [`docs/sim_registration.md`](docs/sim_registration.md) for the SIM and generic-service findings.
- [`docs/tooling.md`](docs/tooling.md) for the analysis tools.

## Firmware Policy

No Nokia firmware, EEPROM image, proprietary tool, or derived decompilation is distributed here.
Bring legally obtained images and place them under the ignored `roms/` directory. The repository
records filenames and hashes solely for reproducibility; see [`roms/README.md`](roms/README.md).

Absolute addresses and the exported 3210 symbol map are specific to NSE-8/9 v6.00. Other ROMs are
expected to move functions and data even when they implement the same hardware contract.

## Build and Reproduce

The Makefile pins the upstream MAME revision and overlays the Nokia sources into a local checkout.
With the required 3210 files installed:

```sh
make build
make verify
make swap16
```

`make verify` boots the default 3210 profile and checks the promoted LCD frame against oracle
`d8a9a7a58e587be8`. Peer models and diagnostic traces are opt-in through `NOKI3210_*` environment
variables. They must not be enabled when establishing a new ROM's hardware baseline.

## Engineering Rules

- Hardware behavior belongs in a device or ordinary memory-map handler.
- Product differences are machine configuration or data, not firmware-PC conditions.
- Research hooks are opt-in, explicitly labelled, and must shrink as contracts become devices.
- Failed probes are documented and removed rather than retained as compatibility paths.
- The 3210 oracle runs after every behavior change.
- A second ROM is used before declaring behavior shared across DCT3.

## License

Original project code is BSD-3-Clause; see [`LICENSE`](LICENSE). MAME is fetched separately and is
GPL-2.0-or-later as a whole. Copyrighted phone firmware is not part of this project.
