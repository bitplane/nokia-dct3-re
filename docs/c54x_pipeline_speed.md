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
MAME's generic state table (`state_int(STATE_PMST)`). It now reads the
register with `tms320c54x_device::pmst()`.

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

The last two CPU figures are each the mean of two back-to-back runs, an A/B
of the same tree with and without the change. Single runs vary by about
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
