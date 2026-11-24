# C54x pipeline speed

The staged C54x model executes about 26 million DSP clocks per second of a
native ROM4 call (the DSP's IDLE is already skipped, so every modelled clock
is an executed one). Real-time SIP media needs that rate on the host.

## Where the time went

On the native nse1ap5 traffic call the model cost about 2,100 host
instructions per DSP clock. The host ran it efficiently: 4.7 instructions
per cycle, with 0.18 % of branches mispredicted. So the cost was the
instruction count itself. Counting every guarded block of `pipeline_step`
and `pipeline_execute` over the call:

| Function | Condition evaluations per clock | Blocks taken per clock |
| --- | ---: | ---: |
| `pipeline_step` | 155.5 | 6.5 |
| `pipeline_execute` | 39.6 | 1.4 |

Almost every evaluation asked an opcode question whose answer could not
change for that instruction. Making each opcode test cheaper by table did
not help, because each was already a few instructions. Asking fewer of
them per clock did.

## Per-instruction plans

Each step precomputes, once per opcode, what the step would otherwise
re-derive every clock. All of it is behaviour-preserving by construction,
and `make check-c54x-state-trace` checks each change over the whole call
([the state-trace oracle](c54x_test_suite.md#state-trace-oracle)).

- **Decode facts** (`decode_facts` in `tms320c54x_decode.h`). About thirty
  booleans that the decode stage derived from the decoded word: branch
  forms, return forms and the direct-status sampling condition. Each is
  built with the decode block's own expression.
- **Stage gates** (`tms320c54x_gates.h`, generated). Most blocks act on an
  older instruction in Access, Read or Execute, and begin with an opcode
  test of that stage's word. Those words are fixed for the rest of the
  clock. Runs of such blocks are wrapped as `if (BIT(gates, N)) // stage
  gate N`, and one per-opcode lookup per stage tells the clock which gates
  can have a live block.
  `tools/c54x_stage_gates.py` regenerates the tables from the wrapped
  source. It refuses a gated block that lacks a word-only opcode test,
  has a side-effecting term before that test, or has an `else`. The tools
  tests fail when the committed header is stale.
- **Execute dispatch** (also in `tms320c54x_gates.h`). `pipeline_execute`
  chose its action through a 76-branch `if`/`else if` chain of pure tests
  of the Execute opcode, about 39 tests per clock. It is now a `switch` on
  a per-opcode table holding the first case whose test holds. Each case
  keeps its original test as a `case N: // test` comment, which the
  generator reads, and the generator refuses a test that reads anything
  but `m_op`. `--convert-chain LINE` performed the conversion, keeping
  every comment.
- **Stage slots.** Each clock moves every stage one older. The 24
  per-stage arrays (`m_pipeline_word[k]` and the rest) are now one
  `stage_slot` struct per stage (`m_slot[k].word`), moved by five
  whole-slot assignments instead of 120 element moves. (A fixed-size
  `memmove` was compiled as a library call and cost 1.25 %.) Save states register each
  field with `STRUCT_MEMBER`, so states saved before this change do not
  load. `make verify-5110-lab-traffic-state` round-trips the call three
  times.
- **Idle services skipped.** Twelve hold flags, each waiting on a pending
  transfer, are one `pending_holds` struct. One `any()` test (two loads)
  replaces a call to `pipeline_service_pending_transfer` on every clock.
  Read-result publication sits behind stage gate 24. Bus-wait and
  peripheral arbitration are inlined.
- **Decode gates.** The Decode stage's checks of the word being decoded
  are wrapped as `if (BIT(decode_gates, N)) // decode gate N`, keyed on that
  word. Locals that the checks share are computed before the gates. The
  generator expands those locals and `fact(...)` calls into word-only tests,
  and ANDs every word-only conjunct that comes before the first
  side-effecting one.
- **Decode-word gates.** Once Decode ends, no slot word changes for the
  rest of the clock. A `// Decode marker` line ORs in a stage-2 table, so
  later blocks that test the Decode slot's word can be gated as well. The
  generator refuses the marker if any slot word is written after it.
- **Block-body refusals.** For each word inside a block-repeat body,
  Decode built a dozen locals (branch targets against the block's end and
  so on) only to refuse unsupported control forms. Every alternative of
  that refusal needs the word to be one of a few control classes, which the
  `block_control` decode fact records. The check now tests that fact first,
  so ordinary ALU words in speech loops skip it.

## Polling loops

Over the 35 s lab call the DSP executes about 349 million clocks. (IDLE is
already skipped, so far fewer than the DSP clock rate implies.) About 11 % of
them sit in two polling loops. Each loads a word, compares it, and branches
back with a delayed `BCD` until another agent changes the word.

Other agents run only between the core's timeslices, so within one slice a
polling loop that has reached a fixed point stays there. `execute_run`
watches for the Execute stage jumping back to the same instruction. It
skips the slice's remaining whole iterations, advancing only
`m_pipeline_cycles`, when all of these hold:

- two successive iterations have the same period, at most 64 clocks;
- nothing with an outside effect happened in the iteration: any memory or
  I/O write, I/O read, BIO/READY sample, XF change, or data read below the
  board's pure-read boundary (MMRs, the timer, the BSP);
- the core state is byte-identical to the previous iteration's, from `m_pc`
  through the hold flags and all six stage slots.

A board opts in with `set_pure_reads(first)`. That promises data reads from
`first` up, and all program reads, have no side effects and change only
between timeslices. The DCT3 board passes 0x0060. Its only side-effecting
reads are the BSP registers, and the HPI window is shared RAM. Boards that
do not opt in never skip. Neither does a run with the debugger or an
instruction observer attached.

Skipping is exact. With the state trace on, each skipped clock replays the
last iteration's per-clock trace values, and the oracle still matches on
every clock. Opcode coverage (`-verbose`) adds each logged opcode once per
skipped iteration. The census is byte-identical to a run without skipping.
A loop that records a shifted-Smem extension tuple is not skipped.

The skip covers 29 million clocks (8.3 %) of the call. Slices limit it. The
lab RF fixture's receive-word timer fires about 121,000 times a second, so
most DSP slices are only 32-63 clocks long. Detection costs two iterations
per slice. Making that timer lazy would lengthen the slices. But the
fixture's samples depend on the current time and on lab state, and fewer
slice boundaries would coarsen ARM/DSP interleaving. That changes the
emulation, so it is not done.

The MCU side matters too. Every ARM fetch from flash went through MAME's
16-to-32-bit unit splitter into two 16-bit handler calls. A native 32-bit
`flash_r32` reads both halfwords in the splitter's own order. That order is
the low bits first, which is the higher-addressed halfword on this
big-endian bus, and each half is read only when the access selects it.
It saves about 1 %.
(An earlier note claimed 19 %; that run had a PGO-built core.)

On every program fetch, the DSP board map tested PMST's OVLY bit through
MAME's generic state table (`state_int(STATE_PMST)`), and later through
`tms320c54x_device::pmst()`. Program space is now the mask ROM installed
directly, with the DARAM overlay as a `memory_view` over 0080 to the overlay
end. The core drives a new `ovly_cb` line when OVLY changes, at reset, on a
debugger PMST write and after a state load, and the board selects or
disables the view. Fetches no longer call a handler.

Each figure below comes from a plain (non-PGO) rebuild of the core
followed by two host-call runs.

| Change | Host-call run CPU (s), no PGO |
| --- | ---: |
| Before | 38.6 |
| Decode facts | 33.6 |
| Stage gates (24 gates, 82 blocks) | 29.6 |
| Execute dispatch (76 cases) | 27.2 |
| Stage advance | 26.7 |
| ARM flash trace hooks | 26.0 |
| Stage slots | 25.7 |
| Inline slot moves; flash hook test at the call | 24.0 |
| 32-bit ARM flash reads (no unit splitter) | 23.8 |
| PMST read directly on DSP program fetches | 23.1 |

Later steps were measured by host instructions instead, which are steadier
than CPU time. Each figure is `perf stat` over 27 emulated seconds of the
traffic call (`SECS=27`), after a plain rebuild.

| Change | Host instructions | User CPU (s) |
| --- | ---: | ---: |
| Before | 427.4 G | 17.5 |
| Idle services skipped | 395.0 G | |
| Decode gates (3) | 365.5 G | 15.2 |
| Decode-word gates (22, 23, 25) | 358.4 G | 15.0 |
| Polling loops skipped | 340.0 G | 14.4 |
| Block-body refusals keyed on the word | 325.4 G | 13.7 |
| Program ROM mapped directly, OVLY as a view | 322.5 G | 13.7 |

The 15.0 and 14.4 s figures are each the mean of two back-to-back runs, an A/B
of the same tree with and without the change; 13.7 s is one run. Single runs vary by about
0.5 s.

## Where it stands

With `make build-pgo`, the native traffic call's connected segment (23-27 s
emulated) costs 4.1-4.4 CPU seconds per 4 emulated seconds, 91-97 % of real
time on this host (mean of five runs 4.27 s, about 94 %). The polling-loop
skip does not move this figure beyond run-to-run noise. In the connected
call the DSP is busy with speech and spends little time polling. Its 4 %
gain is in the rest of the run. The figure is the difference in user time between
runs stopped at 23 and at 27 emulated seconds. It was 40 % before this work:
56 % from PGO alone, 85 % after the steps in the first table, and the rest
from the later steps.

These tried and made no measurable difference:

- Bit tests from a per-opcode table for single opcode checks, without
  skipping any.
- `-march=native` for the core.
- PGO across the ARM core, the board devices and the scheduler as well.
- Native 32-bit ARM RAM handlers.
- A read-array fast path past the flash device's virtual read.
- Gating the decode block's guards as they stood, because state-dependent
  locals were interleaved with them. Moving those locals ahead of the
  checks made the decode gates possible.
- 16-byte alignment of the stage slots.

What remains is spread thinly. `pipeline_step` is still about half of the
process, but no line in it costs more than 2 %. The largest line is the
per-clock slot move. Next come `pipeline_execute`'s bodies, prefetch, the
MCU's own execution and flash reads, at 2-8 % each. Each further gate would
save one or two percent.

## The 6210 call

The 6210 (`npe3ap5`, ROM6 lab cell) ran its connected call at about 21 % of
real time, against the 5110's 94 %. Most of the time was not the C54x core
but the Lua observers that every lab call run loads for its research checks.
Figures are user CPU seconds for the lab-tone early incoming call
(`--page-at 18 --speech --end-call-at 30`), unthrottled, with the PGO core:
the run to 20 s (boot, registration, page, ringing) and to 30 s, whose
difference is the connected call.

| Change | To 20 s | To 30 s | Connected 20-30 s | Real time |
| --- | ---: | ---: | ---: | ---: |
| Before | 57.4 | 104.5 | 47.2 | 21 % |
| Unacquired-program check native | 24.4 | 55.3 | 30.9 | 32 % |
| State entries cached in the observers | 18.4 | 41.1 | 22.7 | 44 % |
| Windowed taps retired | 16.5 | 36.3 | 19.8 | 50 % |
| I/O census native | | 31.7 | | |
| (No research observers at all) | 10.6 | 25.8 | 15.2 | 66 % |

Every step leaves `error.log`, `rom6_radio_writes.tsv` and the captured
dumps byte-identical.

- **Unacquired-program check.** `npe3_native_observe.lua` tapped DSP program
  reads of 0080-1fff and 10080-11fff to count fetches of the captured pages'
  zero padding (OVLY clear), and everything above 1ffff. Internal-RAM code
  runs from that window, so the taps entered Lua on almost every fetch, and
  they also defeated the direct ROM mapping. The C54x backend now selects a
  second overlay view entry while OVLY is clear on a captured (two-page)
  image, whose handler counts reads, and maps a counting handler above page
  1. `c54x_program_coverage: unacquired_reads=0 escapes=0` replaces
  `npe3_native_coverage`. Healthy firmware never takes either path, so the
  check costs nothing.
- **Loader capture.** A MAME tap is installed only into the view entry
  selected at install time (`populate_passthrough` follows `m_u_dispatch`),
  so the one-word program tap at 0a00 that captured the uploaded loader had
  worked only because the wide coverage taps changed the dispatch. A
  breakpoint at 0a00 now saves the words with `saved` as it is hit; by the
  next frame the loader has overwritten its first words.
- **State entries.** Each `cpu.state.X.value` in a tap indexes the state
  container through sol: a string hash, a `dynamic_cast` and a metatable
  check per call. The observers in the 6210 chain look each entry up once.
- **Retired taps.** The SCH provenance and FCCH schedule taps can only
  return once the SCH aperture has been seen (about 3.4 s), and the ADC lab's
  history, filter and mixer taps once 1024 ADC reads have passed. They are
  now removed a millisecond after that, from a coroutine, since a tap must
  not remove itself while it runs.
- **I/O census.** `rom6_native_radio.lua` tapped every DSP I/O read and
  write (about 6 M in the run) to build the per-port summaries and the
  `rom6_radio_writes.tsv` write stream that `rom6_radio_census.py` checks.
  The C54x backend now keeps the census itself when an observer writes the
  stream cap into its `m_io_census_cap` save item; the Lua keeps only a
  one-shot port-31 tap for the first-output overlay capture. The summary and
  footer lines lose their `[luaengine]` prefix; the stream, dumps and line
  contents are otherwise identical.

What remains: the formatter-store tap over 1000-26ff costs about 2.4 s, and
the debugger's instruction hook, which the observers' breakpoints need,
about 5 %. The C54x core is now most of the process.

## The lean 6210 call

The lab gates load research observers and the debugger, which a live call
does not need. `make bench-6210-call` (`tools/bench_6210_call.py`) runs the
same early incoming call with only its inputs (`tools/npe3_call_bench.lua`:
page at 18 s, Send at 20 s, End at 30 s), unthrottled, to 20 s and to 30 s,
and reports the connected ten seconds as a fraction of real time. It fails
if the call does not connect. Run it after `make build-pgo` to measure what
a host can sustain.

In that call the DSP executes about 31 M clocks per emulated second (68 %
of the 45.5 MHz policy clock; IDLE is skipped), and none of it is a
skippable polling loop. Block-repeat bodies are 55 % of executed clocks
(the Viterbi add-compare-select loop at 9015 alone is 12 %) and
single-instruction repeats 9 %. It is signal processing, so the per-clock
cost is what matters.

Two exact changes to memory access:

- **DSP data RAM mapped directly.** The board's data map sent every access
  through one handler that tested for the BSP registers, the HPI window and
  the mask ROM. Reads from 0060 up, and writes to the plain RAM ranges
  (0060-07ff, 1000-afff, f000-ffff), are now installed memory; the HPI
  window's reads map onto the transport's array. The handler keeps the
  BSP and MMR-range words, HPI writes (packet notifications) and mask-ROM
  writes. The program overlay's reads of DARAM are mapped the same way.
- **Direct pages in the core.** Program fetches, program-operand reads and
  data reads and writes look up a 64-word page table of direct pointers
  before the address space. A page is direct only if all 64 words are
  consecutive plain memory, so a handler, tap, watchpoint or view boundary
  anywhere in it keeps the ordinary path. The space change notifiers
  (OVLY view switches, tap and handler installs) bump a generation that
  invalidates every page, so Lua taps and the debugger still see every
  access they cover.

Both leave the full observer run's `error.log`, write stream and dumps
byte-identical, the state-trace oracle identical on all 343 M clocks, and
all 528 conformance sections passing.

| Lean call, PGO core (two alternating runs) | Connected 10 s | Real time |
| --- | ---: | ---: |
| Before | 11.72, 11.95 CPU s | 0.84-0.85x |
| Direct data RAM and direct pages | 11.11, 10.97 CPU s | 0.90-0.91x |

A clean clone with only the 6210's inputs, on the same (not idle) host, ran
the call at 0.82x without a profile. After `make build-6210`, which trains
on this call instead of the 5110's, two runs gave 0.93x and 0.85x.

This host is a Ryzen 9 9950X3D (5.7 GHz). The core is still about 80 % of
the lean call at roughly 900 host instructions per DSP clock. Real time on
ordinary hosts needs the pipeline model itself to become several times
cheaper per clock.

### Outside the core (2026-11-24)

A 4 kHz sample of the PGO build put about 77 % of the call in the C54x core.
The rest was the ARM (with about 7 % in its flash reads), the scheduler, bus
dispatch and the log. Each flash fetch went through a `std::function`, the
dispatch, two `flash_r` calls and two virtual `read_full` calls. Now:

- `flash_r32` reads the four array bytes directly while the flash is in
  read-array mode (`intelfsh16_device::plain_array`). Other modes, and the
  ST part, which has its own `read_full`, keep the ordinary path.
- The ARM's prefetch reads its cache directly and wraps its index without
  dividing. ARM RAM reads are one 32-bit handler.

The 30 s call's log is byte-identical (320,528 lines). Pinned, six
interleaved runs, minimum of each:

| | Boot to answer (20 s) | Connected 10 s | Real time |
| --- | ---: | ---: | ---: |
| Before | 7.33 CPU s | 11.10 CPU s | 0.90x |
| After | 6.63 CPU s | 10.59 CPU s | 0.94x |

These did not help further: extending PGO to the ARM, memory dispatch,
scheduler and driver (under 1 %, within noise); the trace-free build (no
difference). Omitting `-log`, as the live phone does, saves about 1 %. What
is left outside the core is spread too thinly for another such change.

### The native add-compare-select loop (2026-11-24)

The equaliser's butterfly at 9015 is 17 one-word instructions in a block
repeat, one clock each, entered about 31,000 times a second with seven
iterations. When its first instruction finishes Execute, the pipeline holds
only the loop, and nothing is pending, the state at the same point of a later
iteration depends only on registers and RAM. `acs_loop` computes that state
directly, advances the cycle count and moves on. It covers six of the seven
iterations, about 10 % of the connected call's clocks.

It is exact, not an approximation:

- It never runs past the timeslice, and other agents and interrupt lines act
  only between timeslices. (In the equaliser the slices are long: the median
  had 382 clocks left.)
- It refuses a pending interrupt, hold, flush, repeat or delay slot, memory
  that is not plain RAM (so taps and watchpoints still see every access), an
  overlap the pipeline could order differently, and runs with a debugger,
  instruction observer, opcode census or state trace.
- The loop is recognised by its words, not its address.

`C54X_ACS=check` computes each span, then lets the model run the same clocks
and compares the whole core state and every word written.
`make check-c54x-acs` does that over the 6210 call (431,536 spans) and
requires the call's log to be the same with the loop checked, on and off.
`C54X_ACS=off` disables it. The state-trace oracle's taps disable it too, so
that oracle does not cover it.

PGO builds, pinned, six interleaved runs, minimum of each:

| | Boot to answer (20 s) | Connected 10 s | Real time |
| --- | ---: | ---: | ---: |
| Before | 6.67 CPU s | 10.67 CPU s | 0.94x |
| After | 6.42 CPU s | 10.00 CPU s | 1.00x |

The next largest block loops in the connected call, by share of clocks, are
4371 (6.2 %), 2592 (3.9 %, in RAM), 8e35 (2.6 %), 796f (2.6 %) and 77b0
(2.3 %). They could be handled the same way.

### Why the per-clock cost is hard to cut

Measured on the lean call (30 emulated seconds, about 500 M DSP clocks):

- **The checks run every clock; the gated work hardly ever does.** With
  gcov, `pipeline_step` executes about 140 source lines per clock, and
  about 100 of those run on every clock: the peripheral arbitration, the
  slot shift, the state-flag tests and the stage gates themselves. No
  stage gate's blocks are entered on more than 41 % of clocks, and most
  on under 5 %. The gate tables already skip the opcode-specific work.
  What remains is many cheap tests, roughly 140 host branches per clock.
- **The host core is front-end bound.** About 48 % of dispatch slots go
  unused because the front end supplies no instructions. There are about
  2 G L1 instruction-cache refills from L2 per 100 G cycles: 90 % of the
  samples fall in 25 KB of code, and the ARM emulator shares the same
  32 KB L1i. Branch misses (under one per clock) and L1 data misses
  matter far less. Packing the eight per-opcode tables into one 32-byte
  record cut L1 data misses by 78 % and saved no time.
- **Layout noise is about ±5 %.** Changing a driver function's code moved
  `pipeline_step`'s cost by 4-5 % with the core's own code byte-identical.
  The difference was entirely in front-end stalls. Compare builds by
  `perf stat -e instructions:u` (repeatable to five digits) as well as
  by time, and alternate runs.
- **Compiler options do not help.** `-march=x86-64-v3`, turning off
  Ubuntu's hardening defaults and `-falign-*` all stay within noise;
  `-O2` is slower and `-Os` is three times slower.
- **A fast path for quiet clocks would be a second core.** 74 % of clocks
  have no pipeline special state (no repeat stream, hold, wait, flush,
  delay slot, control guard or pending interrupt). Covering half of them
  would still need about 200 distinct opcode words with their full stage
  behaviour, because the hot code uses 1663.
- **Redundant interrupt-line writes are holding the ARM and DSP in step.**
  MAD2 rewrites the ARM's FIQ and IRQ lines on every update (about 90 k
  times per second, nearly all unchanged), and each `set_input_line`
  ends the scheduler's timeslice. Sending only real changes makes the ARM
  run further ahead of the DSP between timers. The 5110 traffic call then
  registers about 20 s late and fails its gate. Removing those writes
  needs an explicit scheduler quantum, so the coupling stops being
  incidental.

### Restructurings that did not pay (2026-11-23)

All on the 30 s lean 6210 call, total CPU, pinned to one core and interleaved
(repeats within about 1 % on an idle host; earlier unpinned comparisons were
not reliable to 5 %). None is committed. Where checked, the prototypes'
terminal DSP cycle, mailbox and slot totals matched each other; the
state-trace oracle was not run.

| Change | Effect |
| --- | --- |
| Step a window back one slot per clock instead of moving five slots | 6 % slower |
| 16-byte aligned slots | within 1 % |
| Per-opcode "plain Decode" bit skipping Decode's always-run checks | 1.2 % faster |
| A second `pipeline_step` instance for clocks with no control state | 3 % slower (code footprint) |
| One specialised instance per opcode window, no profile | 11 % faster at 400 windows, 8 % at 2,500 |
| The same with PGO on both sides | 4 % faster (18.6 to 17.8 s) |
| Specialised instances jumping straight to the next | 6 % slower than a shared dispatcher |

The window approach compiled `pipeline_step` with the Decode to Execute
opcodes as template constants, keyed on those four words, their validity and
the pending extension and delay counts. 1,000 windows cover 75 % of the
call's clocks and 2,000 cover 85 %. A specialised clock costs roughly a
quarter of a generic one, at about 2 KB of code each, so the windows compete
for the instruction cache and the dispatch must predict among them. With PGO
already laying out the generic step, most of the gain disappears.

Two earlier ideas are also ruled out. A clock's cost does not depend on its
position in the scheduler slice (first clock 276 TSC cycles, the rest about
155), so longer slices would not help the core. The core runs about 232,000
slices per emulated second, averaging 59 clocks.
