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
request-driven contact/SIM profile against `oracles/noki3210-frontier.struct`.
The final startup-event field is deliberately excluded from both subsets: the
dispatcher continues receiving events after reaching the same accepted mode,
flags, contact state, SIM state, and exact frame.

`make verify-frontier` checks the current request-driven contact/SIM profile's
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
- MAD2 soft reset count and IRQ/FIQ lines observed at frame boundaries;
- GENSIO control values;
- CCONT byte/read counts and command-byte set;
- EEPROM START conditions and GenIO signal-write count;
- startup modes observed; and
- final task, startup, contact-service and SIM gate values.

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
GENSIO controls, startup modes, running task, mode/flags, contact status, and
SIM gates. LCD, IRQ, CCONT, and EEPROM counts remain in the generated summary
for diagnosis without making timing-dependent totals part of acceptance.

## Coherent frontier profile

`make verify-frontier` enables the request-driven DSP/contact peer and the
ordinary SIMI/FIQ6 card device. Its structural oracle records startup mode
`0x0004`, flags `0x0f`, contact status `0x0049`, no-SIM clear, and SIM enable
set. This semantic state is the forcing-free frontier oracle.

Historical research runs painted a Security-code frame with SHA-256 prefix
`6471d1a5803619c2`. The cleaned driver does not currently reproduce that frame:
the retired display-transfer experiment supplied additional presentation
progress, and the old frame artifacts were incorrectly retained as a supported
acceptance condition. The hash remains evidence about the later MMI state, not
part of `make verify-frontier` until the display/DSP transfer contract is
implemented at a hardware boundary.

A separately generated provisioned EEPROM profile matched the synthetic phone
identity and removed that prompt in the same historical display setup. It painted an idle-like `Menu` frame with
SHA-256 prefix `dbf2704cb945d56b`, while the structural state remains in mode
`0x0004`. The corrected IRQ0 keypad source reaches the real matrix scanner and
publishes decoded keys while that mode remains selected. A deterministic
`12345` plus softkey sequence completes the editor through `0x0578`; its
`0x05e6` callback result is the statically proved accepted-code branch, though
the interactive sequence is not yet part of the structural oracle.

The former `verify-deep` profile used direct firmware-call and RAM-completion
bridges. Its Insert-SIM frame was historically useful, but the request-driven
frontier supersedes it and covers a deeper coherent state. The bridge target and
its structural oracle have been removed rather than retained as a supported
compatibility path.

`tools/find_literal_loads.py` scans Thumb-1 PC-relative literal loads while
normalizing the swapped image's 32-bit halfword order. It is intended for static
producer censuses; use `--raw` only when searching for the on-disk literal value.
