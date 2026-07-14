# Structural boot regression

The LCD frame oracle proves the final visible state. The structural oracle
guards stable mid-boot behavior that can regress while still converging on the
same frame.

`mame_noki3210_input_exerciser.lua` writes a deterministic summary to
`NOKI3210_BOOT_SUMMARY`. `make run` places it at
`RUN_DIR/boot_summary.txt`; `make verify` compares it with
`oracles/noki3210-default.struct` after checking the frame hash.
`make verify-deep` performs the corresponding check for the historical
four-model Insert SIM profile against `oracles/noki3210-deep.struct`. That
profile remains a regression artifact; it is not the current coherent
contact-service acceptance profile.

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

## Legacy deep-profile reference

The historical four-model profile enables:

```text
MODEL_DSP_SERVICE
MODEL_CCONT_PRESENT
MODEL_SVC_RESPONDER
MODEL_SVC_CHANNEL_DRAIN
```

The cleaned driver initially stopped in mode `0x000d` with flags `0x0b`.
Tracing established that the service channel-empty request at `0x29bafc`
completed after the firmware's bounded poll at the canonical 20 MHz timer
profile. A one-microsecond asynchronous peer response makes `0x29bafc` return
success, organically resumes tasks 10 through 21, delivers event `0x15`, sets
flags `0x0f`, and advances startup to mode `0x0004`.

A 20-second confidence run reproduced the visible Insert SIM frame, ending in
mode `0x0004` with flags `0x0f`, contact status `0x0049`, no-SIM asserted and
SIM-enable clear. That result is now recorded as a separate deep oracle; it
does not replace or weaken the default CONTACT SERVICE acceptance test.

The former reported CCONT `+10` drift came from a recursive Make invocation
which passed model names as Make variables without exporting them to MAME.
After adding an explicit `RUN_ENV`, the 20-second historical deep profile
reproduces its committed `ccont_bytes=1753` and `ccont_reads=877` values. Its
exact frame also reproduces. Raw LCD-command and FIQ counts vary between
otherwise equivalent runs, so those two timing counters are intentionally
excluded from the semantic subset check.

The responder and drain are superseded research bridges. Current acceptance
uses `MODEL_DSP_CONTACT_PEER`, which answers observed DSP requests, delivers the
external class-`0x40` session through task 7, and acknowledges `0x622a` without
writing firmware state. Its last clean five-second run ended with startup mode
`0x0004`, flags `0x0f`, contact status `0x0049`, and normal SIM APDUs in progress.
That path is now banked as `make verify-frontier`: its exact frame and
eight-second semantic predicate set additionally record no-SIM clear and SIM
enable set. Raw LCD-command and FIQ counts are excluded because equivalent
frontier runs vary those counts without changing the frame or final state. The default
frame/structural oracle remains the conservative release regression, while the
historical `verify-deep` oracle continues to protect the superseded bridge
profile without lending it current evidentiary status.

`tools/find_literal_loads.py` scans Thumb-1 PC-relative literal loads while
normalizing the swapped image's 32-bit halfword order. It is intended for static
producer censuses; use `--raw` only when searching for the on-disk literal value.
