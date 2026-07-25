# Tooling and external references

## In-repo tools

- `tools/*.py` — small Thumb-disassembly / cross-reference helpers built on
  [capstone](https://www.capstone-engine.org/). They operate on the **swap16**
  firmware image (see `roms/README.md`). Set the image path via the `NOKI_BIN`
  environment variable (default points at `roms/3210f600a_swap16.bin`).
  - `disrom.py ADDR:LEN ...` — **the** disassembler. Thumb-1 (ARMv4T) correct:
    it rejects capstone's Thumb-2 mis-decodes and resolves pc-relative pool
    literals (halfword-swapped). It honors `NOKI_BIN`, including normalized
    second-ROM images. Prefer this over anything capstone-raw.
  - `findcalls.py ADDR...` — find `bl`/branch callers of an address
  - `find_thumb_signature.py REF TARGET ADDR:LEN` — locate a Thumb-1 region in
    another image while masking only relocated direct `BL`/`BLX` encodings;
    reports exact compared-byte coverage and every match.
  - `find_scalar_uses.py VALUE...` — enumerate exact Thumb-1 immediate,
    literal-pool and two-instruction scalar constructions with decode coverage.
  - `find_struct_access.py OFFSET...` — find and contextualize Thumb-1 loads and
    stores at selected structure offsets; `--clusters` requires related offsets
    within one bounded window.
  - `find_literal_loads.py VALUE` — scan PC-relative literal loads with the
    swap16 word-lane correction applied by default.
  - `findptr.py VALUE...` — find pointer literals (raw LE and halfword-swapped)
  - `dump.py ADDR [LEN]` — dump words / halfword-swapped pointers
  - `message_census.py` — profile-driven 3210 v6.00 producer, callback-table,
    generic-service descriptor, and runtime-edge census. `make census` writes
    machine-readable JSON and a concise report under `run_census/`. Named
    inputs under `tools/run_manifests/` scope runtime evidence by
    subsystem; use `CENSUS_LOG=mame/error.log` only for ad-hoc unscoped work. Records
    retain `extracted_static`, `reviewed_static`, or `observed_runtime`
    provenance rather than presenting reviewed control-flow semantics as
    mechanically recovered facts. Requested status inventories also scan the
    fixed variable-length sequence catalogue at `0x2cb968`, decoding packed
    event argument counts and the `0x00dc` terminator. Inputs `0x213a`/`0x613a`
    emit `0x089a, 0x08b0`, while `0x213b`/`0x613b` emit `0x08b0` directly.
  - `display_trace_check.py` — validates the version-specific NV descriptor
    `0x0749` mapping and the selected PCD8544 command/data stream captured by
    `make verify-display`.
  - `radio_answered_audio_boundary_trace_check.py` — validates the answer-only
    DSP shared-control command, its committed shared-RAM publication, and the
    bounded acknowledgement-tone start/stop sequence captured by
    `make verify-radio-incoming-call-answered`.
  - `radio_answered_call_lifecycle_trace_check.py` — validates physical
    Answer-to-End CC teardown, the release channel change, and the complete
    observed command-`0x08` lifecycle captured by
    `make verify-radio-incoming-call-lifecycle`.
  - `radio_call_audio_wire_trace_check.py` — validates the address-independent
    MCU/DSP shared-control wire lifecycle, including stable answered traffic
    and organic teardown. The v5.01 gate is
    `make verify-radio-incoming-call-lifecycle-v501`.
  - `radio_speech_media_trace_check.py` — validates fresh handset and
    network-peer codec state, 20 ms full-duplex cadence, sustained frame
    exchange, the configured network-side 1 kHz source, and non-zero COBBA
    receiver blocks. It therefore distinguishes an energy-bearing decoded
    downlink from a silent framing-only pass.
  - `radio_physical_uplink_trace_check.py` — validates that an external host
    capture source produced at least 100 non-silent COBBA microphone blocks
    and speech above the GSM-FR silence floor at the network peer's independent
    decoder. It evaluates whole-call peak maxima and rejects both silence and
    clipping at any checkpoint. `make verify-radio-physical-uplink` runs the
    v6.00 and v5.01 firmware gates in real time, including their appropriate
    control oracle, speech/FACCH checks and physical End-to-idle teardown. The
    pinned MAME PulseAudio record-stream support is supplied by
    `mame-pulseaudio-input.patch`; the Nokia machine itself has no laboratory
    microphone generator.
  - `dsp_rom_audit.py` — distinguishes real nonuniform DSP regions from
    checksum-valid uniform fill placeholders (`make audit-dsp-roms`).
  - `dsp_upload_extract.py` — reconstructs contiguous type-`0x51`
    DSP-addressed images or bounded shared-RAM staging snapshots from passive
    traces, with explicit word byte order.
  - `dsp_memory_upload_trace_check.py` — proves every firmware type-`0x51`
    fragment was applied at the same address and that the complete image is
    contiguous (`make verify-dsp-memory-upload` covers both 3210 ROMs).
  - `gensio_trace_check.py` — validates CCONT phase/status plus the shared
    v5.01/v6.00 SELECT-latch initialization and read-modify-write contract.
  - `test_message_census.py`, `test_find_thumb_signature.py`,
    `test_make_eeprom_profile.py`, and `test_mad2_access_census.py` cover byte
    lanes, signature relocation masks, runtime-manifest isolation, generated
    EEPROM checksums, and MAD2 trace parsing. Run all four with
    `make test-tools`.
  - `validate_evidence.py` — validates `evidence/*.json` and runtime-manifest
    structure. `make evidence-check` runs it directly.
  - `radio_camp_trace_check.py` — validates the ordered firmware-owned path from
    a usable RSSI candidate through channel change and SI1--SI4 acceptance.
    `make verify-radio-camp` runs the deterministic radio peer and this checker.
- `ghidra/scripts/*.java` — headless Ghidra scripts (run via `analyzeHeadless`).
  `ExportGensioAccesses.java` separates resolved direct/literal MAD2 accesses
  from scalar-only candidates and reports coverage totals for another ROM.

The 3210 flash is stored in 16-bit bus order. Import
`roms/3210f600a_swap16.bin` (generated by `make swap16`) for ARM/Thumb static
analysis, not the raw `.fls`. Importing the raw file can still yield syntactically
valid instructions at requested addresses, but their control flow and literals are
byte-paired incorrectly and must not be used as evidence. The Ghidra scripts name
functions and export analysis. The naming list is also exported as a
  portable symbol map at `ghidra/symbols/3210.csv` (address, kind, name) so you
  get the names without running Ghidra.
- `mame_nokia_dct3_input_exerciser.lua` — MAME Lua harness used by the run targets
  to capture structural/LCD evidence and drive keypad input. Delayed input uses
  a scheduler-backed `emu.wait()` coroutine because LCD frame callbacks stop
  when this firmware gates its display clock. Set
  `NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS` to queue one mirror capture after the
  delayed key sequence; this observes a post-input UI even when no later LCD
  frame callback occurs. `input_field:set_value(1)` is the logical pressed
  state; MAME applies the port's active-low polarity.
  Firmware editors retain their real completion rules: for example, the first
  clock setup requires four time digits and all eight date digits before `OK`
  completes. Harness waits do not fill untouched fields or bypass validation.
  The `waitbuzzer` sequence token polls only the mapped MAD2 PUP buzzer gate
  before delivering the next physical key; it does not inspect or change call
  or firmware state.
- `make verify-alarm` uses that organic input path to set the clock/date and a
  12:02 alarm, then requires the firmware to program CCONT, consume its alarm
  IRQ, clear the software deadline, and drive the MAD2 buzzer. It is distinct
  from the direct-MMIO `make verify-ccont-rtc` controller fixture.
- `make verify-power-lifecycle` applies only the physical power-key input. It
  checks that a short press remains an interactive UI action and that a
  two-second hold enters the firmware-owned shutdown/teardown continuation.

## NokTool 1.8 (external — EEPROM/NV format reference)

[NokTool 1.8](https://nokia-tuning.net/download/noktool18.zip) is a third-party
Nokia service utility (Borland Delphi 7, Win32). It was useful evidence for
generic EEPROM sub-block boundaries and the 16-bit additive checksum algorithm.
The 3210 v6.00 firmware itself validates a combined tune/security record at
`0x264c56`, not NokTool's two independent sub-blocks; see
`docs/eeprom_analysis.md`. Tool format evidence is kept distinct from a
firmware-validated contract.

The tool and any reconstruction of it are **proprietary and not included** in this
repo. Only the *findings* (block layout, checksum algorithm — facts) are recorded
in the docs.

**How it was analysed:** the unpacked binary was reconstructed with
[IDR — Interactive Delphi Reconstructor](https://github.com/crypto2011/IDR),
which correctly recovers Delphi forms/units and names. IDR's output (the `.pas`
reconstruction, `.idc`, `.lst`) is a derivative of NokTool's copyrighted binary
and is likewise **not redistributed** here.

## MAME

The driver `driver/nokia_dct3.cpp` is overlaid onto an upstream
[MAME](https://github.com/mamedev/mame) checkout (pinned commit in the Makefile);
MAME is not vendored. See `LICENSE` for the licensing split.
