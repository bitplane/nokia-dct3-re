# SIM registration firmware map

This is the current, evidence-based map of Nokia 3210 SIM registration. It
records conclusions needed to continue the driver rather than the sequence of
experiments used to discover them. Superseded forcing probes and falsified
hypotheses remain available in Git history.

Addresses refer to the correctly byte-swapped 3210 v6.00 image. Use
`roms/3210f600a_swap16.bin` and `tools/disrom.py`; analysis of the raw `.fls`
produces plausible but incorrect Thumb control flow.

Evidence labels used below:

- **S** - static disassembly/decompilation;
- **R** - observed at runtime in a coherent boot;
- **I** - isolation experiment only, not an implementation contract.

## Ownership

| owner | task | role |
|---|---:|---|
| SIM hardware manager | 21 (`0x15`) | reset, ATR/PPS, T=0 exchange, status; loop at `0x27eae0` |
| SIM/configuration client | 20 (`0x14`) | parses replies, controls extended EF reads, owns ENABLE |
| registration coordinator | 17 (`0x11`) | preliminary phase and `0x1583/0x1584` lifecycle |
| generic result parser | 15 (`0x0f`) | converts lower object results into registration events |
| task-5 callback engine | 5 | owns radio/resource callback selection and packed events |

The physical SIM owns bytes and timing at the MAD2 SIM interface. Radio
sessions, callback construction, task events, commit state, and configuration
flags are phone/baseband state and must not be manufactured by the card.

## Principal state

### Task-21 SIM manager `0x10dca8`

Important fields established by static/runtime tracing:

| offset | meaning |
|---:|---|
| `+1` | active/reset state |
| `+5` | most recent receive classification |
| `+6` | control state |
| `+7` | completion/presence latch |
| `+8` | operation active |
| `+0a` | current high-level state |
| `+0c` | card/CHV access state |
| `+0e` | requested state |

Transport buffers are `0x10deec` (command/request descriptor) and `0x10dddc`
(received response). Task 21 receives card data through the SIM UART/FIQ path,
not through a card-specific firmware shortcut.

### Task-20 registration block `0x111c64`

| address | role |
|---|---|
| `0x111c64` | no-SIM/result state |
| `0x111c69` | SIM selected/present lifecycle latch |
| `0x111c6f` | successful initialization side state |
| `0x111c76` | read-enable gate condition |
| `0x111c79` | extended-read ENABLE, set by the valid `0x1196` reply path |
| `0x111c86` | optional teardown-side-effect gate; not the `0x1580` producer |
| `0x111c96/97` | additional extended-read gate conditions |

Direct writes to these addresses are not hardware emulation.

### Transient callback vector `0x110f1c`

The task-5 engine expands packed descriptors into a transient argument vector:

- selector at `+0`;
- argument 1/object at `+4` (`0x110f20`);
- descriptor type at `+0xb`.

`0x2aee20` expands packed sequences and `0x2ac3f2` performs callback switches.
It consumes an already-produced descriptor; it is not a hidden session creator.

## Organic card conversation

With `NOKI3210_MODEL_SIM_DEVICE=1`, the stateful device and firmware complete
this conversation through MAD2 registers and FIQ6 (**R**):

```text
ATR -> PPS
  -> SELECT 7F20 -> STATUS
  -> SELECT 3F00
  -> SELECT/READ 2FE2 (ICCID)
  -> SELECT 7F20
  -> SELECT/GET RESPONSE/READ 6FB7 (ECC)
  -> SELECT/READ 6FAE (PHASE)
  -> periodic STATUS A0 F2 00 00 16
```

The periodic STATUS is stable firmware-owned presence polling. Its 22-byte
DF_GSM data follows the normal code-`0x0b` path at `0x27ee94`; `0x27ef0a`
special-cases INS `0xf2`, calls `0x27ea20`, and returns to the receive loop at
`0x27efb0`. It does not imply a malformed FCP or card-reset loop (**S/R**).

The preliminary pass never requests `EF_IMSI (6F07)`. Consequently the generic
EF parser `0x200364` never dispatches IMSI to `0x20194c`, which would copy it to
the configuration block and set bit `0x02` in `0x10d126` (**S/R**).

## Task-20 reply contract

Task 20 receives registration messages in its loop around `0x20837a`. The
legitimate path of interest is:

```text
task-20 message 0x1196
  -> handler 0x207234
  -> 0x293f30(request, reply, mode=1)
  -> return value 2
  -> 0x20731a
  -> 0x20733c sets 0x111c79=1 and related success state
```

Return value 2 is derived from the coherent request/reply data parsed by
`0x293f30`; it is not encoded solely by message ID `0x1196`. Isolation runs
which force-called the real `0x1196` producer delivered the message but never
reached `0x20733c` (**I**). Therefore message delivery is necessary but not
sufficient.

Once ENABLE is set, gate `0x208218` also requires the mailbox drained and the
other gate bytes clear. It then starts the full EF walk, including IMSI, from
the table at `0x2e0c04` (**S**).

## Producer chain to `0x1196`

The mapped firmware-owned chain is:

```text
object-bearing callback-7 construction (0x05dc)
  -> callback 7 queues 0x0aa0
  -> context allocation/attachment
  -> packed event 0x5518 (task-5 event 0x1518 + object)
  -> 0x27a00c posts 0x1583 to task 17
  -> registration lifecycle/commit family
  -> terminal SIM-server commit
  -> 0x2902ac constructs task-20 message 0x1196
  -> 0x207234 / 0x293f30
```

The commit implementation is the fall-through tail at corrected address
`0x254bb4` (older notes used `0x254b40`). It belongs to router case `0x177b`
inside `0x253d30`, consumes a populated session, emits the `0x1199/0x1196`
family, and frees the session (**S**). Replaying commit keys or calling its
producers directly is a forcing probe, not organic registration.

## Callback 7: the immediate missing session

Callback-table entry 7 is handler `0x29992c` (**S**). During a coherent boot it
is visited once by the global `0x05e2` callback sweep, but it never receives
constructor lifecycle `0x05dc` (**R**).

For a real constructor call:

1. `0x299bc4` queues internal `0x0aa0`.
2. Fall-through `0x299bda` copies framework argument 1 from `0x110f20` into
   callback state slot `0x1120e4`.
3. The later `0x0aa0` branch allocates one of four contexts at
   `0x10ea20..0x10ea9f`, attaches the existing object, and queues packed
   `0x5518`.
4. `0x27a00c` posts `0x1583` when the object's mode byte permits it.

The helper `0x277cb4(0x192, 0, 0)` is scheduler bookkeeping, not an object
allocator. No other ROM literal addresses `0x1120e4` (**S**).

Callback 7 manages radio resources `0x42..0x45` and lower-layer contexts. Its
object is a GSM/radio-session object, not SIM-card state. The SIM device must
not select callback 7, publish `0x05dc`, populate `0x110f20/0x1120e4`, allocate
contexts, or emit any downstream event.

### Session-object factory

Helper `0x24f120` constructs the object consumed by callback 7 (**S**). It
allocates an `0x18`-byte descriptor and initializes the fields later read by
`0x24f25c`:

- owned payload pointers at `+0x00` and `+0x04`;
- status fields at `+0x10` and `+0x12`;
- context selector at `+0x13` (initially zero);
- request parameters at `+0x14`, `+0x15`, and `+0x16`.

The four factory call sites (`0x2996aa`, `0x2997dc`, `0x299860`, and
`0x2998a0`) are branches of one function, `0x299610`. Every successful branch
publishes its descriptor through `0x2af798(0xca8a, 0x1e, object)`. The function
is reached from the task-15 router at `0x255a34` when it receives status
`0x0a2e`.

The organic predecessor is now statically closed back to the lower-radio
result. Task 15 handles `0x09ee` at `0x209274` and forwards it through
`0x251cc8`; task 17 maps that status at `0x22270c` to a newly constructed
`0x0a2e`, which returns to task 15 and selects the router branch above. None of
`0x09ee`, `0x0a2e`, or the factory calls occurs in the coherent run.

## Adjacent paths and exclusions

These results constrain the missing boundary:

- The global callback sweep deliberately supplies `0x05e2`; it is not a
  constructor sweep. Terminal status `0x012f` selects the normal application
  callback `0x2f`, which then receives `0x05dc` (**S/R**).
- After preliminary SIM status `0x13b5`, the callback engine intentionally
  reconstructs callback `0x2f` with argument `0x0787`, then delivers `0x05e0`,
  `0x05e1`, `0x0b5b`, and `0x1581` to that callback (**R**). This is not a
  failed attempt to select callback 7 and is not the missing session trigger.
- Status `0x0518` with argument 7 is input to the already-current callback; it
  does not select callback 7 (**S/R**).
- Resource `0x67`, installed by callback `0x2f`, is local resource-manager
  state. Runtime lookups only replace/read that value and generate no peer
  transaction (**R**).
- `0x0588` is a next-item operation inside an existing selector session. It
  publishes `0x05e0(selector+1)` and cannot originate callback 7 (**S**).
- Context-arbiter event `0x1388` requires a context which callback 7 itself
  would allocate, so using it as the constructor predecessor is circular
  (**S/R**).
- Service-30 callbacks `0x79 -> 0x7a` form a valid NV/request sequence, but no
  descriptor-bearing constructor starts it. Its `0x0578` is a downstream
  resource completion, not an unsolicited ready signal (**S/R**).
- Native EEPROM record `0x0757`, variant 0 lives at offset `0x0db0`, length
  `0x190`. Supplying the ROM-derived default is useful fidelity but does not
  initiate the missing session (**S/R**).
- The DSP shared packet ring accepts only documented inbound packet families.
  A mirrored type-5 packet or type-70 wrapper does not become the required
  generic-service object (**S/I**).
- Task 17's observed `0x09ec`, task 15's `0x07d6`, and subsequent task-16/task-10
  initialization notifications contain no object pointer (**R**).

## Generic lower-object path

A related result route is statically established:

```text
generic service callback receives object-bearing 0x05e8
  -> returns 0x05ea preserving the object
  -> 0x28d29c / 0x28d194 transfers ownership as task-15 0x07dd
  -> parser 0x209978 validates class 5, primitive 0x0b
  -> maps primitive to registration discriminator 0x20
```

No object-bearing `0x05e8`, `0x05ea`, or `0x07dd` occurs in the coherent run
(**S/R**). Parser `0x209978` populates the task-15 lower state later consumed
while producing `0x09ee`, so this path is part of the missing radio-session
activation. Delivering its object remains an external-peer responsibility; its
exact hardware transport is not yet proved.

The coherent runtime instead completes a separate acknowledgement side path:

```text
task 17 0x09ec -> task 15 -> task 16 0x07d6
  -> task 10 0x03e9 -> task 17 0x043c
```

Task 17 consumes `0x043c` as bookkeeping. It does not construct the session
object and is not a substitute for the absent `0x09ee` result.

## Current boundary and next acceptance point

The card-side preliminary transaction is functional. The missing operation is
the firmware or external-peer transition that constructs callback 7 with a
real, firmware-owned object. Known resource APIs currently expose no pending
request that a peer can honestly complete.

The next investigation must therefore:

1. decode the reachable DSP service commands `0x30` and `0x32` issued through
   `0x290cf4`, including their DSP-owned completion words and IRQ-4 consumer;
2. determine whether the absent lower-radio result returns through DSP shared
   control words, the task-22 L1 mailbox, or another DSP-owned callback route;
3. model only the external side of that proven DSP/L1 contract;
4. let firmware generate `0x05dc -> 0x5518 -> 0x1583 -> 0x1196` itself;
5. observe `0x293f30` return 2 from the coherent reply data;
6. extend the SIM filesystem only when the resulting organic APDUs demand it.

Completion requires one coherent boot with no injected task messages, forced
callback events, forced return values, or writes to SIM/registration RAM.

## Instrumentation worth retaining

- `NOKI3210_TRACE_SIM_RX`: register/FIQ/APDU path and the `0x1196` parser
  milestones;
- `NOKI3210_TRACE_DSP_BOUNDARY`: shared-ring ownership and packets;
- focused callback-7 constructor/object and packed-`0x5518` traces;
- task-message traces around tasks 15, 17, 20, and 21;
- resource/service API traces which show real request construction.

Broad callback-sweep and zero-initialization dumps are historical noise and
should not be restored unless a new hypothesis specifically requires them.

## Acceptance checklist

- callback 7 receives organic `0x05dc` with a valid object;
- firmware emits `0x5518` and task-17 `0x1583` without injection;
- firmware reaches `0x2902ac` and task 20 receives organic `0x1196`;
- `0x293f30` returns 2 from coherent request/reply contents;
- `0x20733c` sets ENABLE without a RAM write shim;
- the extended EF pass begins and is served by `nokia_sim_card_device`;
- both 3210 oracles and the 3330-E smoke baseline remain healthy.
