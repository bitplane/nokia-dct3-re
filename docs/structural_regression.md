# Structural boot regression

The LCD frame oracle proves the final visible state. The structural oracle
guards stable mid-boot behavior that can regress while still converging on the
same frame.

`mame_noki3210_input_exerciser.lua` writes a deterministic summary to
`NOKI3210_BOOT_SUMMARY`. `make run` places it at
`RUN_DIR/boot_summary.txt`; `make verify` compares it with
`oracles/noki3210-default.struct` after checking the frame hash.
`make verify-frontier` performs the corresponding check for the current
request-driven contact/SIM profile against `oracles/noki3210-frontier.struct`.

The default runner reseeds an isolated per-run NVRAM directory from the
generated EEPROM profile. This prevents an old shared `mame/nvram` file from
silently changing product data. Introducing that deterministic fixture added
four stable EEPROM transactions and corresponding FIQ/CCONT accounting while
preserving the exact LCD hash; the structural oracle records the profile-backed
baseline rather than the former stale-NVRAM run.

## Summary fields

The schema records:

- emulated frames and LCD command/data/full-transfer counts;
- MAD2 soft reset count and IRQ/FIQ lines observed at frame boundaries;
- GENSIO control values;
- CCONT byte/read counts and command-byte set;
- EEPROM START conditions and GenIO signal-write count;
- startup modes observed; and
- final task, startup, contact-service and SIM gate values.

It intentionally excludes timestamps, raw scheduler order and full traces.
Those are too sensitive to harmless timing changes. A changed counter is a
review signal, not automatic proof of a bug: inspect the responsible subsystem
and update the oracle only when the new behavior is understood.

The Lua MMIO observer decodes ARM big-endian byte lanes explicitly. Summary
files are refreshed every 30 frames through an atomic rename, so a partial
result survives a livelock or externally terminated research run.

## Stability evidence

Two independent 20-second default-profile runs produced byte-identical schema-1
summaries. The corresponding latest nonblank LCD frame retained SHA-256 prefix
`d8a9a7a58e587be8`.

## Coherent frontier profile

`make verify-frontier` enables the request-driven DSP/contact peer and the
ordinary SIMI/FIQ6 card device. Its structural oracle records startup mode
`0x0004`, flags `0x0f`, contact status `0x0049`, no-SIM clear, and SIM enable
set. The default synthetic identity paints the Security-code frame with SHA-256
prefix `6471d1a5803619c2`.

A separately generated provisioned EEPROM profile matches the synthetic phone
identity and removes that prompt. It paints an idle-like `Menu` frame with
SHA-256 prefix `dbf2704cb945d56b`, while the structural state remains in mode
`0x0004`. A scripted key adds the expected IRQ6 activity but does not reach
matrix decode. This distinction prevents a visually plausible frame from being
mistaken for completed interactive startup.

The former `verify-deep` profile used direct firmware-call and RAM-completion
bridges. Its Insert-SIM frame was historically useful, but the request-driven
frontier supersedes it and covers a deeper coherent state. The bridge target and
its structural oracle have been removed rather than retained as a supported
compatibility path.

`tools/find_literal_loads.py` scans Thumb-1 PC-relative literal loads while
normalizing the swapped image's 32-bit halfword order. It is intended for static
producer censuses; use `--raw` only when searching for the on-disk literal value.
