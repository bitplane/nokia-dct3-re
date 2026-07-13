# Structural boot regression

The LCD frame oracle proves the final visible state. The structural oracle
guards stable mid-boot behavior that can regress while still converging on the
same frame.

`mame_noki3210_input_exerciser.lua` writes a deterministic summary to
`NOKI3210_BOOT_SUMMARY`. `make run` places it at
`RUN_DIR/boot_summary.txt`; `make verify` compares it with
`oracles/noki3210-default.struct` after checking the frame hash.
`make verify-deep` performs the corresponding check for the four-model Insert
SIM profile against `oracles/noki3210-deep.struct`.

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

## Deep-profile reference

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

As of 2026-07-13, repeated 20-second deep runs still reproduce the Insert SIM
frame hash `90eb19a5478483ca` and every non-CCONT structural field, but report
`ccont_bytes=5153` and `ccont_reads=4299`, ten above the recorded `5143`/`4289`.
Restoring the old 5 ms DSP service cadence produces the same counts, while the
default-profile structural oracle remains byte-identical. The drift therefore
predates and is independent of the ring-drain cadence used by the current radio
investigation. Do not update the deep oracle until the extra CCONT transaction
has been identified or accepted as a deliberate model change.

The service responder remains a firmware-boundary model: it expresses a real
request/completion contract but currently completes it through the shared
status byte rather than a separately emulated transport device.

`tools/find_literal_loads.py` scans Thumb-1 PC-relative literal loads while
normalizing the swapped image's 32-bit halfword order. It is intended for static
producer censuses; use `--raw` only when searching for the on-disk literal value.
