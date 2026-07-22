# Structural boot regression

The LCD frame oracle proves the final visible state. Under `-video none`, the
Lua harness mirrors the PCD8544 command/data stream and snapshots completed and
frame-visible states; the phone driver has no parallel capture implementation.
When firmware gates video frame notifications but continues partial LCD writes,
the periodic oracle publishes the dirty terminal mirror so `make frame` cannot
fall back to an early blank boot image.
The structural oracle
guards stable mid-boot behavior that can regress while still converging on the
same frame.

`mame_noki3210_input_exerciser.lua` writes a deterministic summary to
`NOKI3210_BOOT_SUMMARY`. `make run` places it at
`RUN_DIR/boot_summary.txt`; `make verify` checks the semantic predicates in
`oracles/noki3210-default.struct` without requiring an LCD frame. That target
explicitly disables the 3210 peer devices and is a negative failure baseline.
`make verify-frontier` checks the machine-default request-driven
external-service/SIM composition against `oracles/noki3210-frontier.struct`.
`make verify-radio-camp` adds the opt-in deterministic radio peer and requires
an organically usable ARFCN, accepted channel change, task-11 acquisition
action, matching SI3 identity, and complete SI1--SI4 bitmap.
`make verify-radio-registration` continues through one accepted Location
Updating exchange. It requires the exact contention-resolution UA payload, the
firmware's `UPDATE BINARY` of `EF_LOCI`, RR release, channel deconfiguration and
at least four channel-`0x50` BCCH blocks after release. This distinguishes a
completed registration from unrelated type-`0x80` traffic or a retry loop.
`make verify-radio-operator` adds the unobscured firmware-rendered test-PLMN
label. None alters either boot oracle.
`make verify-mmi-menu` adds provisioned identity data and one delayed physical
left-softkey press. It requires the same coherent structural predicates and an
exact hash of the stable post-input `Phone book` pixels. The animated 20x12 icon
region is excluded; the text, softkey, layout and remaining pixels are protected.
This is the interactive MMI oracle; the semantic missing-hardware profile remains
an explicit negative control.
`make verify-sim-phonebook` extends the interactive gate across a mutable card
transaction. It enters `ADA`/`123` through physical keypad input, requires the
firmware to issue an absolute 32-byte `UPDATE RECORD` for `EF_ADN`, validates
that only record 1 changed, then restarts with the same SIM NVRAM and matches
the stable pixels of the firmware-rendered `ADA` search result. It does not
inject an APDU or conflate card storage with the handset EEPROM.
The task running at the final emulation tick is deliberately excluded:
final-tick sampling is timing-sensitive, and a harmless schedule shift can
sample scheduler idle `0xff` instead of task 1 without any change in durable
state or the stable menu frame. Startup mode, service/SIM state, resets and
hardware activity remain protected.
`make verify-3210-v501` runs the same-product v5.01 control with a BIOS-specific
EEPROM profile and checks `oracles/noki3210-v501-smoke.struct`.
`make verify-mad2-interrupts` runs three non-oracle controller conformance
fixtures: overlapping physical keypad/charger sources, an IRQ held pending
behind its masks, and register-enabled extended FIQ8 routing. The fixtures use
only input ports and mapped MAD2 registers; their bounded trace checker does
not treat timing-sensitive interrupt totals as structural-oracle fields.
`make verify-mad2-clocks` checks both 3210 ROMs against the observed reset-cause
read, organic watchdog service and SIM peripheral-clock gate lifecycle, and asserts that
neither boot accesses the timer-1 register window. Its 12-second window reflects
physical Timer-0 pacing. This is a negative register-coverage contract, not
validation of the independently running timer-1 overflow source.
`make verify-charger-wake` powers the running phone off through its physical
power key, connects the charger while CCONT retains power, and requires cause
bit `0x04`, a complete digital-domain restart, a post-reset VCHAR sample and
acting-dead mode `0x0005`. The checker consumes only device traces and the
structural summary; it does not write firmware state.
`make verify-mbus` checks the identical v6.00/v5.01 receive-mode
initialization, asserts that ordinary boot transmits no bytes, and uses one
external byte to verify RX-ready, firmware consumption and FIQ2 acknowledgement.
The arbitrary byte's later parser outcome is not an acceptance predicate.
`make verify-display` checks the version-specific descriptor-`0x0749` EEPROM
locations, the current erased-data boundary, GENSIO LCD selection, command
prefix and a complete 504-byte RAM transfer in both 3210 ROMs. It records the
remaining product-provisioning shortcut rather than treating it as LCD state.
The final startup-event field is deliberately excluded from both subsets: the
dispatcher continues receiving events after reaching the same accepted mode,
flags, contact state, SIM state, and exact frame.

`make verify-frontier` checks the 3210 request-driven external-service/SIM composition's
stable semantic predicates. `make verify-frontier-stability` repeats that check
with freshly seeded NVRAM. It reports full-summary
hash drift without failing because LCD-command, CCONT-byte/read, and similar
raw counters vary with harmless scheduling. Set `FRONTIER_STABILITY_STRICT=1`
when investigating those counters specifically. Use the repeatability target
before banking a new frontier; ordinary RE iterations should use the faster
single-run target.

The default runner reseeds an isolated per-run NVRAM directory from the
generated EEPROM profile. This prevents an old shared `mame/nvram` file from
silently changing product data. The structural oracle records the
profile-backed baseline, including its four stable EEPROM transactions and
corresponding FIQ/CCONT accounting.

MAME names the alternate v5.01 BIOS NVRAM system `noki3210_1`; the Makefile
seeds that directory explicitly rather than the default `noki3210` directory.
This distinction is load-bearing: regenerating a v5.01 EEPROM while retaining
an older `noki3210_1/eeprom` silently runs the stale product state.

## Summary fields

The generated summary records:

- emulated frames and LCD command/data/full-transfer counts;
- MAD2 soft reset count, IRQ/FIQ lines observed at frame boundaries, final
  pending status and optional save-state round-trip result;
- GENSIO control values;
- CCONT byte/read counts and command-byte set;
- EEPROM START conditions and GenIO signal-write count;
- reads and writes above the 3210's physical 128 KiB SRAM boundary;
- startup modes observed; and
- final task, startup, service-session and SIM gate values.

The committed oracle intentionally selects terminal state and lifecycle
predicates from that larger summary. It also requires both upper-SRAM access
counts to remain zero while the provisional wider map exists. Other raw
counters, timestamps, scheduler order, and full traces are too sensitive to
harmless timing changes. They remain available for review but are not pass/fail
criteria.

The Lua MMIO observer decodes ARM big-endian byte lanes explicitly. Summary
files are refreshed every 30 frames and from a display-independent periodic
callback through an atomic rename. This keeps the terminal state current while
firmware gates the LCD clock and lets a partial result survive a livelock or
externally terminated research run.

## Default profile

The historical CONTACT SERVICE frame with SHA-256 prefix `d8a9a7a58e587be8`
is not an acceptance oracle; the missing-hardware profile stops before that
presentation state. Its semantic subset requires
GENSIO controls, startup modes, running task, mode/flags, service-session status, and
SIM gates. LCD, IRQ, CCONT, and EEPROM counts remain in the generated summary
for diagnosis without making timing-dependent totals part of acceptance.

## Coherent frontier profile

`make verify-frontier` enables the request-driven DSP/external-service peer and the
ordinary SIMI/FIQ6 card device. Its structural oracle records startup mode
`0x0004`, flags `0x0f`, service-session status `0x0049`, no-SIM clear, and SIM enable
set. This semantic state is the forcing-free frontier oracle.

`make verify-dsp-transport` protects the separated transport/HLE/peer
composition. It requires complete-packet TX consumption, RX publication before
FIQ0, shared-service completion through IRQ4, the established type-`0x70`
completion, and the request-correlated external session. Its v5.01 leg checks
only the common doorbell/service-completion mechanics. An active coherent run
also crosses a save/load boundary.

Fixture frames and their oracle status:

| Fixture | Frame hash (SHA-256) | Oracle status |
|---|---|---|
| Unprovisioned Security-code prompt | prefix `6471d1a5803619c2` | Research evidence only; outside `make verify-frontier` because the hardware-boundary profile does not reproduce the additional display transfer |
| Provisioned idle `Menu` | prefix `dbf2704cb945d56b` | Research evidence; structural state remains mode `0x0004` |
| Provisioned `Phone book` menu after left softkey | raw one-shot mirror `9b2ac7477b5be11aa6b4f178f781ff2799754b0b5ff6ce8f66221564d0f914d1` | Masked stable pixels (animated 20x12 icon region excluded) are protected by `make verify-mmi-menu`; the raw hash is not part of the canonical structural oracle |
| Registered test operator `01` | crop SHA-256 `59dd0d4f80f705c98be148c7f60f3171d2b66d7a434fba51feef7a0134ada9a8` | `make verify-radio-operator` couples the full registration trace to a 12x7 glyph crop, excluding unrelated animated indicators |

The provisioned EEPROM profile matches the synthetic phone identity and
suppresses the security prompt. The IRQ0 keypad source reaches the real matrix
scanner and publishes decoded keys while mode `0x0004` remains selected. The
separate unprovisioned `12345` fixture completes the security editor through
`0x0578`. The verifier returns one and the observed callback publication is
`0x05e1`; that status is callback-scoped and is not, by itself, an accept/reject
code.

## Nokia 3210 v5.01 control

`make verify-3210-v501` runs the independent NSE-8 v5.01 full flash with its
own generated EEPROM profile as a same-product structural control.
`make verify-mmi-menu-501` adds provisioned identity and physical left-softkey
input, opening the same `Phone book` menu as v6.00 with the same stable-pixel
oracle. Its input is delayed until seven seconds and the run lasts fourteen
seconds because periodic CCONT RTC work shifts the v5.01 editor-ready schedule;
the v6.00 fixture retains its five-second input.

The useful invariants are the task-1 and SIM results. The run organically observes modes
`0x0001`, `0x000d`, and `0x0004`, then remains in mode `0x0004` with readiness
flags `0x0f`. At its relocated state block it also clears
no-SIM and sets SIM ENABLE organically. This independently reproduces the v6.00
task-1 terminal mode and validates the shared SIM device contract.
Service-session status remains `0x00c9`, but presentation and interaction
match v6.00 under the provisioned fixture. The status difference remains a
transport/lifecycle observation, not evidence of a blocked UI.

`tools/find_literal_loads.py` scans Thumb-1 PC-relative literal loads while
normalizing the swapped image's 32-bit halfword order. It is intended for static
producer censuses; use `--raw` only when searching for the on-disk literal value.
