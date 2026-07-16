# Structural boot regression

The LCD frame oracle proves the final visible state. Under `-video none`, the
Lua harness mirrors the PCD8544 command/data stream and snapshots completed and
frame-visible states; the phone driver has no parallel capture implementation.
The structural oracle
guards stable mid-boot behavior that can regress while still converging on the
same frame.

`mame_noki3210_input_exerciser.lua` writes a deterministic summary to
`NOKI3210_BOOT_SUMMARY`. `make run` places it at
`RUN_DIR/boot_summary.txt`; `make verify` checks the semantic predicates in
`oracles/noki3210-default.struct` after checking the exact frame hash.
`make verify-frontier` performs the corresponding semantic check for the current
request-driven external-service/SIM profile against `oracles/noki3210-frontier.struct`.
`make verify-3210-v501` runs the same-product v5.01 control with a BIOS-specific
EEPROM profile and checks `oracles/noki3210-v501-smoke.struct`.
`make verify-mad2-interrupts` runs three non-oracle controller conformance
fixtures: overlapping physical keypad/charger sources, an IRQ held pending
behind its masks, and register-enabled extended FIQ8 routing. The fixtures use
only input ports and mapped MAD2 registers; their bounded trace checker does
not treat timing-sensitive interrupt totals as structural-oracle fields.
`make verify-mad2-clocks` checks both 3210 ROMs against the observed reset,
watchdog and clock-control boot sequence and asserts that timer 1 remains
unexercised. This is a negative coverage contract, not validation of timer-1
semantics.
`make verify-mbus` checks the identical v6.00/v5.01 receive-mode
initialization, asserts that ordinary boot transmits no bytes, and uses one
external byte to verify RX-ready, firmware consumption and FIQ2 acknowledgement.
The arbitrary byte's later parser outcome is not an acceptance predicate.
The final startup-event field is deliberately excluded from both subsets: the
dispatcher continues receiving events after reaching the same accepted mode,
flags, contact state, SIM state, and exact frame.

`make verify-frontier` checks the current request-driven external-service/SIM profile's
stable semantic predicates. `make verify-frontier-stability` repeats that check
with freshly seeded NVRAM. It reports full-summary
hash drift without failing because LCD-command, CCONT-byte/read, and similar
raw counters vary with harmless scheduling. Set `FRONTIER_STABILITY_STRICT=1`
when investigating those counters specifically. Use the repeatability target
before banking a new frontier; ordinary RE iterations should use the faster
single-run target.

The default runner reseeds an isolated per-run NVRAM directory from the
generated EEPROM profile. This prevents an old shared `mame/nvram` file from
silently changing product data. Introducing that deterministic fixture added
four stable EEPROM transactions and corresponding FIQ/CCONT accounting while
preserving the exact LCD hash; the structural oracle records the profile-backed
baseline rather than the former stale-NVRAM run.

## Summary fields

The generated summary records:

- emulated frames and LCD command/data/full-transfer counts;
- MAD2 soft reset count, IRQ/FIQ lines observed at frame boundaries, final
  pending status and optional save-state round-trip result;
- GENSIO control values;
- CCONT byte/read counts and command-byte set;
- EEPROM START conditions and GenIO signal-write count;
- startup modes observed; and
- final task, startup, service-session and SIM gate values.

The committed oracle intentionally selects terminal state and lifecycle
predicates from that larger summary. Raw counters, timestamps, scheduler order,
and full traces are too sensitive to harmless timing changes. They remain
available for review but are not pass/fail criteria.

The Lua MMIO observer decodes ARM big-endian byte lanes explicitly. Summary
files are refreshed every 30 frames and from a display-independent periodic
callback through an atomic rename. This keeps the terminal state current while
firmware gates the LCD clock and lets a partial result survive a livelock or
externally terminated research run.

## Default profile

The exact latest nonblank LCD frame retains SHA-256 prefix
`d8a9a7a58e587be8`. Its semantic subset requires the expected reset count,
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

Non-oracle research evidence includes a Security-code frame with SHA-256 prefix
`6471d1a5803619c2`. It is not part of `make verify-frontier` because the current
hardware-boundary profile does not reproduce the additional display transfer.

A separately generated provisioned EEPROM profile matched the synthetic phone
identity and removed that prompt in the same historical display setup. It painted an idle-like `Menu` frame with
SHA-256 prefix `dbf2704cb945d56b`, while the structural state remains in mode
`0x0004`. The corrected IRQ0 keypad source reaches the real matrix scanner and
publishes decoded keys while that mode remains selected. A deterministic
`12345` plus softkey sequence completes the editor through `0x0578`; its
`0x05e6` callback result is the statically proved accepted-code branch, though
the interactive sequence is not yet part of the structural oracle.

## Nokia 3210 v5.01 control

`make verify-3210-v501` runs the independent NSE-8 v5.01 full flash with its
own generated EEPROM profile. It is a same-product structural control, not a
second supported frontier: the current model leaves its LCD blank and reaches
less external-service/SIM progress than v6.00.

The useful invariants are the task-1 and SIM results. The run organically observes modes
`0x0001`, `0x000d`, and `0x0004`, then remains in mode `0x0004` with readiness
flags `0x0f`. At its relocated state block it also clears
no-SIM and sets SIM ENABLE organically. This independently reproduces the v6.00
task-1 terminal mode and validates the shared SIM device contract. Service-session status remains
`0x00c9` and the LCD remains blank, so the oracle records those narrower v5.01
terminal semantics without claiming presentation parity.

`tools/find_literal_loads.py` scans Thumb-1 PC-relative literal loads while
normalizing the swapped image's 32-bit halfword order. It is intended for static
producer censuses; use `--raw` only when searching for the on-disk literal value.
