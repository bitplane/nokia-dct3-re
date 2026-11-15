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

| Change | Host-call run CPU (s), no PGO |
| --- | ---: |
| Before | 38.6 |
| Decode facts | 33.6 |
| Stage gates (24 gates, 82 blocks) | 29.6 |
| Execute dispatch (76 cases) | 27.2 |
