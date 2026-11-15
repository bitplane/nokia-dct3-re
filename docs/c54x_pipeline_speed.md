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
