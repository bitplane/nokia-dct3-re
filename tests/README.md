# C++ test contracts

Tests are separate from shipped device implementations.

| Directory | Contract | Limits |
| --- | --- | --- |
| `unit/` | Standalone algorithms, codecs and protocol data | No scheduler or handset acceptance. |
| `c54x/` | Synthetic processor execution, interrupts, bus ordering, peripheral composition and restoration | Expectations must not call CPU implementation helpers. Timing projections are not measured silicon behavior. |
| `devices/` | MAME device benches using registers, callbacks and saved state | Self-conformance is not independently measured hardware behavior. |
| `firmware/` | Copied instruction sequences and acquired-ROM execution | Input identity and bounded integration contracts, not generic CPU or full-handset acceptance. |

## CPU sections

`fixtures/c54x_sections.json` owns section names and expected case counts.
Each entry resolves to exactly one source in `c54x/sections/` or
`firmware/c54x_*.h`. Missing, ambiguous and unregistered sources fail closed.

- `python3 tools/run_c54x_section.py --list`: list generic CPU sections.
- `python3 tools/run_c54x_section.py NAME`: build and execute only that section.
- `make build-c54x-isa C54X_SECTION=NAME`: build that section without executing.
- `make check-c54x-core`: complete retained generic CPU scope.
- `python3 tools/run_c54x_section.py all --scope firmware`: explicitly opt into copied-firmware sections.

The selected bench produces `mame/c54xisa`, independently of the handset
binary. Builds share the overlay/object tree: never run builds concurrently.
Selected execution remains the development workflow; complete regression is
a final integration checkpoint, not a prerequisite for each edit.

The catalogue checks registration and counts, not instruction correctness.
Executable sections must establish their own stimulus, bus/state expectations,
replay and meaningful rejection controls. See
[`c54x_test_suite.md`](../docs/c54x_test_suite.md) for the canonical
coverage boundaries and scoped execution commands.

## Firmware and handset acceptance

`tools/run_c54x_core.py` now runs only the retained `rom4-context` and
`cold` firmware integrations. The historical generic BIOS profiles,
default phase chain, review manifests and frozen transcript inventories are
retired. Their old command lines are not current acceptance interfaces.

`make check-c54x-rom4-context` selects the copied context integration.
Acquired-ROM codec fixtures retain separate runners and input hashes.
ROM4 `cold` omits the MCU loader deliberately: a cold-frontier
result is not a statement that normal handset boot stops there.
Verifier-sensitivity machines are research instruments, not handset verdicts.

`make build-conformance` includes device/firmware benches;
`make build-handsets` excludes synthetic test drivers and firmware-PC probes.
Handset acceptance remains product/workflow-specific in Makefile and gates.json.
Host tooling tests and mocked runner tests never substitute for native execution.

## Codec performance

After building the conformance binary, run:

```
python3 tools/bench_c54x_codec.py --output tmp/c54x-speed/current
```

This runs the acquired ROM4 EFR encoder/decoder on both frontends, three times
each, unthrottled and without tracing. Every run must still pass bit-exact output
and full-frame save/replay checks. JSON records binary/workspace-source hashes,
commands and individual timings; it cannot infer source provenance for an
arbitrary `--binary`. Do not run builds or other gates concurrently with it.

Process throughput includes startup, preparation, output checks and two
executions of every frame. Separately reported first-execution spans exclude
reset, preparation, snapshots, replay and output comparison, but include normal
emulator scheduling. Both use 163 frames / 3.26 seconds of audio; neither is a
full-handset call benchmark or proof of physical DSP cycle accuracy. Older
binaries without span instrumentation report process throughput only.

Reference measurement (Ryzen 9 9950X3D, GCC 15.2, normal `-O3` build,
three-run medians):

| ROM4 EFR fixture | Before decode/copy optimization | Optimized process time | First-execution throughput |
| --- | --- | --- | --- |
| Atomic encoder | 1.79 s | 1.80 s | 394% realtime |
| Staged encoder | 16.84 s | 6.05 s | 110% realtime |
| Atomic decoder | 0.300 s | 0.305 s | 4015% realtime |
| Staged decoder | 1.83 s | 0.724 s | 1123% realtime |

These are workload/host results, not guaranteed rates. In particular, 110%
encoding alone does not establish headroom for realtime duplex calls plus the
handset. Small per-field `copy_backward` calls and
a larger 64-predicate cache regressed the measured workload and are not retained.

Source-line sampling identified repeated retirement-length and opcode-admission
classification in the staged pipeline. Those now share a compact descriptor;
XC and forwarding checks reject irrelevant cases before scanning/classifying.
The descriptor does not replace live state, extension or profile checks. Fixed
stage copies accounted for roughly 7--12% of two short sample sets, not a dominant
hotspot; rotating storage was deferred rather than changing save-state and
forwarding layout for that limited ceiling.

Validation of this optimization covered 53 CPU sections (29,244 cases and
restores), both native 5110 menu oracles, bit-exact EFR fixtures with replay on
both frontends, and 438 related host tests. It was not a full 518-section rerun.
The focused section batch took 336 seconds. A separate exhaustive transformation
check compared all 65,536 opcodes against the previous decoder: 121 predicates,
retirement spans, extension counts across four bus-profile combinations, and
admission across eight extended-program/bus-profile combinations. That establishes
optimization equivalence, not independent ISA or silicon validation.

The subsequent Access/Read classification cache uses the descriptor's two
remaining bits, without increasing table storage. CPU-0-pinned, no-trace
three-run medians measured staged EFR encoding at 5.94--6.01 s per validation
process (111--113% first-execution realtime), against 6.17--6.32 s
(106--108%) for the saved control. Decoding measured 0.725--0.745 s
(1111--1140%), against 0.748--0.761 s (1073--1099%). This is a small host-specific
gain, not a new realtime guarantee. An exhaustive comparison covers the SP
classification for every opcode and the Read decision for every opcode/extension
pair. Stage actions, SP sampling time and live extension checks are unchanged.
Focused validation covers 17 CPU sections (31,370 cases plus restores, 119 s),
the staged EFR bit-exact/replay checks, both native 5110 menu oracles and 442
related host tests. This is not a full-suite rerun.

Do not replace the entire Execute chain with a predecoded operation switch
without measuring it: the equivalent 72-handler switch regressed this workload
to 7.10 s encoding and 0.849 s decoding and was discarded.

The ASM-store predicate's constant mode mask preserves all 65,536 opcode
classifications without adding storage. Ordinary-build, CPU-0-pinned three-run
batches measured staged encoding at 5.84--5.87 s (114--115% first-execution
realtime), versus saved controls at 5.98--6.02 s (111--112%). Decoding measured
0.713--0.718 s (1150--1159%), versus 0.729--0.737 s (1120--1126%). This modest
gain was retained; it is not included in the older PGO measurements below.
Validation passed 10 focused CPU sections (18,214 cases plus restores, 68 s),
EFR bit-exact/replay checks on both frontends, both native 5110 menu oracles,
36 benchmark/codec-runner tests, and an exhaustive C++ predicate comparison.

### Frame Budgets and PGO

`--frame-timing --handset --cpu 0` adds first-execution frame distributions and
three fresh-NVRAM, 30-second staged 5110 boot/idle runs. The codec fixtures still
validate every output and replay every frame; timing records exclude that replay
and are emitted outside the measured interval. Do not enable `-verbose` for
performance runs: it also enables per-instruction DSP extension tracking.
Paired budgets sum corresponding encoder/decoder frame durations.
The handset estimate adds its average boot/idle cost per 20 ms, including its
idle DSP. Neither sum models concurrent cache/scheduler effects, nor is it a
native duplex call or a bound on active-call overhead. Native traffic-channel
activation remains unqualified; HLE call gates cannot substitute for this test.

A C54x-only GCC PGO trial trained on 32-frame FR encode/decode fixtures on both
frontends, holding out EFR and handset boot. On the host/compiler above, pinned
to CPU 0, alternating ordinary/PGO three-run batches without verbose tracing gave:

| Measurement | Ordinary release | FR-trained PGO |
| --- | --- | --- |
| Staged EFR encoder, first execution | 111--113% realtime | 134--136% realtime |
| Staged EFR decoder, first execution | 1110--1133% realtime | 1376--1379% realtime |
| Staged 5110 boot/idle | 671--674% realtime | 680--686% realtime |
| Paired EFR mean frame budget | 19.60--19.85 ms | 16.21--16.44 ms |
| Paired EFR plus boot/idle estimate | 22.57--22.83 ms | 19.12--19.38 ms |

PGO improves staged encoder throughput by about 19--22% in these batches.
Its paired-codec p99 is 16.38--17.58 ms, worst 17.89 ms. Adding the average
boot/idle estimate gives a worst frame of 20.84 ms: even this estimate is not a
deadline guarantee. Atomic EFR also passed, measuring 520% encoding and 5443%
decoding with PGO, versus 379% and 4070% in the ordinary initial batch.
These clean measurements supersede the earlier verbose-instrumented PGO trial.

This is an optional build experiment, not a source-level emulator mode. Start
with a current ordinary build and no concurrent builds/gates. Only the C54x
translation unit should be stale when invoking the incremental generated build:

```sh
profile="$PWD/tmp/c54x-speed/pgo-profile"
build=mame/build/projects/sdl/mame/gmake-linux
mkdir -p "$profile"
touch mame/src/devices/cpu/tms320c54x/tms320c54x.cpp
make -C "$build" config=release64 -j8 FORCE_INCLUDE= \
  CXXFLAGS="-fprofile-generate=$profile -fprofile-update=single" \
  LDFLAGS="-fprofile-generate=$profile"
for mode in --fr-encoder --fr-decoder; do
  python3 tools/run_c54x_rom4_crc.py --mame mame/mame --rompath mame/roms \
    "$mode" --staged --output-root tmp/c54x-speed/pgo-training
  python3 tools/run_c54x_rom4_crc.py --mame mame/mame --rompath mame/roms \
    "$mode" --output-root tmp/c54x-speed/pgo-training
done
touch mame/src/devices/cpu/tms320c54x/tms320c54x.cpp
make -C "$build" config=release64 -j8 FORCE_INCLUDE= \
  CXXFLAGS="-fprofile-use=$profile -fprofile-partial-training -g"
python3 tools/bench_c54x_codec.py --output tmp/c54x-speed/pgo-evaluation \
  --frame-timing --handset --cpu 0
```

Use a fresh profile directory for a new experiment. Do not suppress missing or
mismatched profile errors, or reuse profiles after source/compiler changes.
Partial training preserves normal optimization of functions absent from the
training workload. Both 5110 menu oracles must still pass. To return to the
ordinary build, touch that overlaid CPU source and run the generated make command
again **without** the profile flags; make does not track compiler flag changes.
The retained trial binary is `tmp/c54x-speed/mame-clean-pgo` (ignored, reproducible
from the recipe), not the normal `mame/mame` build.
The measured PGO build also includes `-g` on the C54x translation unit for
subsequent source-line sampling; optimization remains the normal `-O3` plus PGO.
Validation passed 12 focused CPU sections (9,716 cases plus restores, 78 s),
both native 5110 menu oracles, and FR/EFR bit-exact output and frame replay on
both frontends. This is not a full-suite run. The CPU source is `bc65de7e`;
the default release build was restored byte-identically after profiling.

### Host Hotspot Sampling

`tools/profile_c54x_gdb.py` is a fallback when host performance counters are
unavailable. Build only the C54x translation unit with `CXXFLAGS=-g` and
`FORCE_INCLUDE=` (retaining the generated build's normal `-O3`), then run:

```sh
DCT3_PROFILE_JSON="$PWD/tmp/c54x-speed/profile.json" \
gdb -batch -x tools/profile_c54x_gdb.py --args "$PWD/mame/mame" tms54rom4 \
  -bios efr-encoder -rompath "$PWD/mame/roms" \
  -video none -sound none -nothrottle -noreadconfig \
  -joystickprovider none -midiprovider none -seconds_to_run 10
```

The JSON counts interrupted main-thread stack/source locations at jittered
10--30 ms intervals. It includes replay, snapshots and validation; debugger
pauses invalidate throughput measurements. Optimized inline/source attribution
is approximate. Repeat samples to establish a candidate, then measure changes
with the uninstrumented benchmark. `complete` means the inferior exited, not
that its codec output passed: correctness must be checked separately.

Two ordinary-build samples (297/296 stops) and two PGO samples (245/246) of
staged EFR encoding at `bc65de7e` agree on the broad ranking:

- `pipeline_step` is the exclusive frame in 55--61% of samples. Its callees
  account for additional pipeline work; this is not the entire pipeline cost.
- Stage-copy lines account for approximately 6--10% of all samples. They do not
  justify a save-state/layout rewrite as the first optimization.
- The ASM-store predicate is a recurring inline location (approximately 4--7%
  of samples); dual-operand reads and MAME memory handlers also recur. Source
  attribution alone cannot establish how much a replacement would save.
- Retirement still updates diagnostic opcode counts/first-PC entries without
  verbose output and queries the verbose option for shifted forms. Trials that
  cached the option and gated collection, both inline and in a cold helper,
  regressed staged encoding to 6.29--6.36 s and decoding to 0.772 s. They were
  removed, despite preserving all 2,128 enabled staged diagnostic records in
  the inline trial. Less bookkeeping did not produce faster compiled code.

The isolated ASM-store mask above produced a small gain. Checking DST destination
addresses before opcode classification in the shared operand-read path did not
improve on it, so that trial was also removed. Do not infer that all memory
accesses can bypass MAME handlers: mapped registers, forwarding and bus side
effects remain part of the instruction contract. The rejected full Execute
switch is not reopened by these coarse samples.

## Native 5110 Frontend

The ordinary `noki5110` native DSP composition uses the single stage-owned
engine with standard SWWSR decoding. `verify-5110-menu`, late-input,
power-lifecycle and save-state gates exercise that default, including passive
health checks. The atomic comparison machines `nse1astd` and `nse1aclk` are
retired with the atomic engine; `nse1plg` retains the legacy SWWSR bus
comparison.
The native PCM runner's `nse1obs` control adds only a passive instruction
observer to ordinary 5110: no external clocks or firmware-state modification.
Its execution-owned helper observations replace fetch-time argument sampling.
Opcode fingerprints were rechecked on the new compositions before retaining
the existing drift gates; matching them is not independent CPU validation.

## Retention rules

Retain distinct operand contrasts, arithmetic boundaries, bus ordering,
side-effect counts, output-pin behavior and restore points. An old bug
regression is not obsolete merely because the implementation passes it.

Replace redundant Cartesian products with justified boundaries, recording
lost coverage explicitly. Delete unjustifiable fixtures after bounded review;
existence and old passing counts are not retention arguments.
A final-frame oracle cannot replace instruction-level checks.

Keep setup, expectations and replay explicit. Helpers may load memory,
compare state and serialize snapshots; they must not calculate expected
results through the CPU decoder, arithmetic, addressing or timing code.
No implicit cross-family phase chain belongs in the replacement suite.

The completed integration checkpoint is recorded in the disposition document:
all 518 CPU/copied-firmware sections, the linked firmware context/cold controls,
the full host suite and seven affected handset gates pass their named contracts.
A later selected section's result must not silently renew that broad checkpoint.
