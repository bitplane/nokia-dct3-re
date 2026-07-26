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
validated CCONT, DSP-service, external-service and SIM devices are enabled by the 3210 machine
profile rather than an environment-variable stack. An explicit missing-hardware negative profile
is protected by semantic boot predicates without requiring a particular failure screen.

The coherent modeled profile reaches a verified interactive desktop. The SIM
completes ordinary non-CPHS initialization, provisioned identity data removes
the phone-lock prompt, and a physical left-softkey press opens the firmware's
`Phone book` menu. Task 1 remains in mode `0x0004`; that mode records a later
power/shutdown continuation and does not block the UI. Correct MAD2 IRQ0 keypad
wiring reaches the firmware matrix scanner and decoded key resources. The
unprovisioned Security-code editor also accepts a physical `12345` sequence.

Report code 7 is a later power/shutdown report. Mapped callback, descriptor,
timer, and task-6 selector paths are conditional firmware lifecycles rather
than missing hardware acknowledgements. The detailed contract lives in
[`docs/mmi_layer.md`](docs/mmi_layer.md).

`make verify-frontier RUN_DIR=run_frontier` is the authoritative 3210 hardware
baseline. It uses the request-driven external-service peer
and ordinary SIMI/FIQ6 card model, ending in mode `0x0004` with service-session status
`0x49`, no-SIM clear, and SIM enable set. Keypad interaction is independently
covered through MAD2 IRQ0 and the firmware matrix scanner. `make verify-mmi-menu`
adds provisioned identity and protects the interactive menu transaction.
For direct use, `make run-interactive` opens a normal MAME window with standard
remappable inputs and persistent NVRAM; it does not drive scripted keys.

The DSP, radio, GSM-network, and external-service HLE devices react at explicit
transport boundaries. They are executable protocol models, not finished silicon
emulation, but do not call firmware handlers or write registration state.
Firmware-PC conditions are confined to diagnostic traces; product provisioning
enters through EEPROM and other ordinary device boundaries.

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
| MAD2 timers, sleep and IRQ/FIQ | Partial hardware | Boot-critical paths work; paired ROMs and focused tests cover ARM clock-stop, timer/key wake and save-state restoration, while exact divider and transition timing remain open. |
| PCD8544 LCD and keypad | Partial hardware | Firmware renders authentic frames; MAD2 IRQ0 reaches the matrix scanner and decoded input resources. |
| 24C128 EEPROM | Partial hardware | MAME's native I2C device is wired through GenIO and passes the oracle; provisioning and the parallel alias need validation. |
| CCONT power/ADC/RTC | Partial hardware | Extracted MAME device passes the oracle; physical ADC latency, RTC encoding, watchdog clock and board-level analog signals remain open. |
| GENSIO serial/SELECT | Partial hardware | Extracted endpoint/status/LCD/CCONT transport passes two 3210 ROMs; physical timing and SELECT-attached peers remain open. |
| SIMI controller and SIM card | Partial hardware | Separate devices compose through byte/reset callbacks; organic SIMI/FIQ6 and T=0 initialization work, and the GSM 11.11 linear-record path persistently stores firmware-written ADN contacts. Timing, errors, security and broader file coverage remain incomplete. |
| DSP mailbox/service corner | Prototype | Bootstrap, service timing, packet rings, two firmware-programmed tone voices and an isolated GSM-FR speech backend work through HLE; a DSP core and DSP-local COBBA control interpretation remain unemulated. |
| Startup/service/radio peers | Prototype | Separate service, Nokia L1 and standards-shaped GSM devices compose through DSPIF; one laboratory cell supports camp, Location Updating, release and operator presentation. Wider peer contracts remain incomplete. |
| Interactive startup | Mapped | Provisioned boot reaches the idle screen and opens `Phone book` through a protected physical-keypad oracle. Mode 4 is compatible with the UI; cross-ROM presentation parity remains open. |
| MMI/RTOS internals | Mapped | Firmware owns these; observe them rather than emulate them. |
| Audio, RF, network | Partial HLE | The laboratory cell supports organic registration, paging, MT call/SMS control, bidirectional GSM-FR TCH/F media, FACCH/SACCH coexistence, degraded frames and teardown across both 3210 ROMs. Physical RF, mobility, MO calls, wider services and DSP-local COBBA mux/control decoding remain open. |

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

1. Recover MAD2's remaining clock-divider, transition-latency, extended-interrupt,
   and rail-sequencing contracts.
2. Replace calibrated CCONT ADC/serial timing and board-level analog values only
   when an exercised firmware consumer or hardware measurement supplies units.
3. Extend SIMI/card timing, error, removal, CHV, and file behavior from organic
   firmware requests or focused protocol conformance tests.
4. Keep DSPIF as a transport-only attachment point while expanding the HLE only
   from evidenced traffic; a future C54x backend replaces that HLE at the same seam.
5. Use the validated 3210, 3310, 3330, and 3410 profiles to distinguish shared
   DCT3 behavior from product-specific display, flash, storage, and peer contracts.

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
- [`docs/mmi_layer.md`](docs/mmi_layer.md) for the validated keypad, idle/menu, security-editor, and power lifecycle.
- [`docs/sim_registration.md`](docs/sim_registration.md) for SIM initialization and adjacent session firmware maps.
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

`make verify` boots the explicit missing-hardware profile and checks its semantic predicates.
Normal product composition is typed machine configuration. Named `HWCFG` and
`DIAGCFG` fixtures select negative-composition and controller-conformance tests;
passive diagnostics use MAME logging. `NOKIA_DCT3_*` variables are confined to
the external Lua harness for scripted input, capture, and test output. The former
`NOKI3210_*` prefix is not retained as a compatibility alias.

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
