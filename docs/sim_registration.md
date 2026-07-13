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
`0x254bb4` (older notes used `0x254b40`). Swap16-aware decoding establishes
that it belongs to router case `0x1770`, not `0x177b`,
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
generic service callback receives numeric status 0x05e8
  -> returns 0x05ea
  -> 0x28d29c / 0x28d194 transfers ownership as task-15 0x07dd
  -> parser 0x209978 validates class 5, primitive 0x0b
  -> maps primitive to registration discriminator 0x20
  -> successful task-15 parse result 0x09f3
  -> state-machine branches 0x20be0c or 0x20f324 publish internal result 0x0a08
  -> task-15 translator 0x208ee0 constructs task-14 0x09d8
  -> task-14 decoder 0x267258
```

No `0x05e8`, `0x05ea`, or `0x07dd` occurs in the coherent run
(**S/R**). The two `0x0a08` producer sites are also absent while task 15's receive
loop runs three times. Both are selected only when the handler returns `0x09f3`,
which ties the missing task-14 `0x09d8` directly to successful parsing of the
`0x07dd` object rather than to an independent unsolicited task-14 message.
Delivering the original object remains an external-peer responsibility; its exact
hardware transport is not yet proved.

The generic framework boundary is now separated from that external transport.
`0x2638e4` is called only by the task-5 dispatcher at `0x2af652`, and callback
`0x2618e8` is the organic **service-5** callback selected by the callback table
(`0x002db860`). In an unforced deep run it already receives `0x05f3` and `0x05e2`
through the normal dispatcher. Re-reviewed control flow shows that its `0x05e8`
branch does not read an object argument from dispatcher scratch `0x110f1c`; it
returns `0x05ea` after checking downstream SIM/profile readiness. The absent
condition is therefore an organic `0x05e8` publisher path with the required
readiness state, not queued-object population. The former `MODEL_GSM_SERVICE` prototype tracked
service `0x0b` and force-called `0x263154`/`0x2635ac`; it targeted a neighboring
transaction and was deleted.

An exhaustive Thumb literal-load scan adds an important qualification: executable
ROM contains loads of `0x05e2`, `0x05e7`, and `0x05ea`, but no instruction loads
`0x05e8` as an immediate literal. `0x05e8` instead appears in firmware registration
descriptors as a callback/event field. For example callback-table index `0x24`
(`0x28b844`) handles the global `0x05e8` transition and only then registers a
resident descriptor through `0x28ba9a -> 0x263d30`; that call is absent from the
coherent boot and is downstream, not the missing producer. Future census tooling
must therefore recover descriptor fields and indirect event generation in addition
to direct `0x2af798` call sites.

The message-topology census refines that statement without contradicting the
literal result. It recovers 16 in-ROM calls which construct global `0x05e8` as
`0xbd << 3` and pass it to `0x2af798`. In that ABI the high bits encode the
argument count; plain `0x05e8` carries **zero argument words**. These sites are
valid argumentless publishers: packed-event expansion converts the low 13-bit
status into the callback input without requiring argument words. The unresolved
predecessor is which sibling callback path can publish `0x05e8` organically in
the coherent boot, and which ingress/state transition activates it while entry
`0x28`'s downstream readiness checks succeed.

The dispatcher contract behind that correction is now pinned. Task 5 expands
packed events at `0x2aee20` before dispatching the current callback through
`0x2ac3f2`. `0x110f1c` is a scratch vector: its first word is the callback
selector/index, `0x110f20` is argument 1 (an object pointer for object-bearing
constructors), and `0x110f27` is a behavior/descriptor flag. The stack at
`0x111930` stores callback indices during switching; it is not an object queue.
Entry `0x28` shares its `0x05e8` branch with `0x05de` at `0x261b60`, checks
SIM/profile state through `0x2608f8`, `0x260df4`, and `0x260954`, and does not read
an argument from the scratch vector on that branch. The generated census report
lists the complete recovered direct publisher callsites; helper-mediated and
RAM-built event sources remain explicit coverage limits for the next pass.

All 13 callback owners containing the 16 recovered publishers are registered in
the coherent boot and each receives the global `0x05e2` sweep once. None publishes
`0x05e8`. Their relevant non-sweep inputs are statically classified in the census:
`0x0c2c`, `0x0580`, `0x0388`, `0x05e7`, `0x0578`, `0x1778`, `0x0598/0x0599`,
object-bearing lifecycle `0x05dc`, `0x05a5`, `0x05e1`, and `0x035c`, plus two
multi-status cleanup/error callbacks. Registration is therefore not the missing
step; an organic input and its associated state/object must reach one of these
owners.

The strongest mapped predecessor is the firmware-owned object classifier at
`0x267e68`. It reads the current lower-radio object from controller state, selects
callbacks `0x36`, `0x37`, `0x38`, `0x39`, or `0x3a`, and publishes packed event
`0x85e0` with selector and object arguments. Normal dispatcher expansion then
places the selector at `0x110f1c`, the object at `0x110f20`, and supplies lifecycle
input `0x05dc`. Callers at `0x254aac` and `0x26955c` belong to the same controller
dispatch region; `0x26955c` is reached through the sole call to helper `0x269524`
at `0x254b6a`. This route is dormant in the coherent run.

### Organic SIM-task ingress to the classifier

The classifier's upstream contract is now closed. It is not populated by a DSP
RX packet. Task 21 constructs task-20 status `0x120c` at `0x27e3cc` or
`0x27e8fe` and includes a selector byte. Task 20 handles it at `0x2085ce` and
calls `0x203d2c`, which reaches `0x2938b0`. That routine builds an internal
SIM-server request beginning `03 a0 12 00 00 <selector>` and uses synchronous
task-21 helper `0x293522`. A successful response begins with halfword `0x006a`;
its D0 object at reply+4 is parsed by `0x2726e0`.

The D0 parser publishes one of the `0x1770..0x1788` task-5 events. Router
`0x253e20` then reaches `0x269524`/`0x267e68`, which publishes packed `0x85e0`
with a callback selector and firmware-owned object:

```text
task 21 status 0x120c -> task 20 / 0x203d2c
  -> synchronous task-21 A0/12 request -> 0x006a + D0 response
  -> 0x2726e0 -> 0x177x -> 0x269524 / 0x267e68
  -> packed 0x85e0(selector, object) -> callback lifecycle 0x05dc
```

Validated DSP RX families (type `0x70`, type `0x80`, types `0x83..0x99`, and
translator classes 3/5/17/47) have no edge into status `0x120c`, `0x2938b0`,
or this task-21 request. The DSP-ingress hypothesis is therefore disproven for
this object path, rather than merely unobserved.

In the retained coherent deep run, normal SIM APDU requests prove task 21 works,
but no task message `0x120c` occurs and none of `0x203d2c`, `0x2938b0`,
`0x2726e0`, `0x267e68`, or `0x05e8` is reached. The strongest organic
predecessor is the dormant task-21 `0x120c` notification. Both producer sites
require SIM-state notification byte `0x10dcb7` to be nonzero and clear it after
use; the coherent run records no write which sets that byte. Its
initialization/subscription contract remains unresolved.

The dispatcher scratch region `0x110f1c..0x110f2b` is transient callback ABI,
not persistent session storage. Runtime write watching identifies startup
clearing at `0x200168`/`0x20016a`, task-5 receive copies at
`0x2af594`/`0x2af696`, and internal queue bookkeeping at `0x2af75e`,
`0x2af770`, and `0x2af78e`. The former mid-line `0x2aee84` trace was not a
valid writer observation.

The coherent runtime instead completes a separate acknowledgement side path:

```text
task 17 0x09ec -> task 15 internal case 6 -> task 16 0x07d6
  -> task 10 0x03e9 -> task 17 0x043c (immediate acknowledgement)
  -> task 10 builds 72-byte work object -> task 3 -> MCU-to-DSP TX ring
```

Task 17 consumes `0x043c` as bookkeeping and remains in its phase loop. The
completion it accepts is `0x0434` or `0x0a22` (with later `0x1583`/`0x1584`
phase events); it does not construct the session object and is not a substitute
for the absent `0x09ee` result. Task 10 produces the real `0x0434` asynchronously
from `0x219e30`, after its task-3/DSP work completes.

The coherent run also pins a separate radio-completion path at the DSP boundary. Task 10 creates
`{00 02 44 1a 00 81 98 ...}` and posts it to task 3. Task 3 serializes queued
work through `0x290840` into the MCU-owned DSP TX ring. With the DSP peer cadence
decoupled from the old 5 ms phase-lock (the opt-in model now defaults to 4 ms),
the earlier type-`0x51`/`0x70` backlog drains and this object appears organically
as `type 0x1a, payload 68`, beginning `441a 0081 9800`.

DSP-to-MCU RX delivery is also pinned to FIQ 0. A correlated type-`0x80`,
primitive-`0x70` candidate traverses the real RX ring and task-4 dispatcher, but
task 13 rejects it for this registration context even when delayed by up to
306 ms. That response hypothesis is falsified and its probe was removed.

The relevant task-10 transition is status `0x1392`, not `0x138f`. Task 17 enters
its long-lived event loop through `0x223964 -> 0x2271c6`; `0x2222fc` and the
`0x138f` callback family occur only after that loop returns, so they cannot
produce the `0x0434` the loop is awaiting. The similarly numbered DSP packet
type `0x89` is also an initialization-only route (`[0x112501] == 0`); a real
FIQ-0 probe at the current frontier was rejected and removed.

Status `0x1392` dispatches through `0x21b9b4 -> 0x21b198` and can reach the
`0x0434` producer at `0x219e30`. Callback `0x2b60f6` publishes it after
unregistering lower-radio key `0x4c00`. The organic callback path is:

```text
lower-radio result dispatcher 0x245a84, case 0x0fbf (0x245c76)
  -> callback-object wrapper 0x2525a8
  -> callback 0x2b60f6
  -> task-10 status 0x1392
  -> 0x21b198 -> 0x219e30 -> task-17 0x0434
```

The dispatcher input is encoded as `0x0fbf0000` and is loaded from a ROM literal
at `0x24788e` on the lower-radio controller's event-`0x102f` branch. The earlier
`0x0fc3` value was a jump-table arithmetic error. Decoder `0x267258` returns
event `0x102f` for an incoming object whose opcode byte is `0x2a`; task 14 calls
that decoder for statuses `0x09d8`/`0x09de`. Status `0x09d8` is constructed by
task 15 itself after its `0x07dd` object parser succeeds and returns internal result
`0x09f3`; it is not a raw peer status. The exact transport which supplies the
earlier generic-service object remains unresolved. The prior task-8 `0x8d`/`0x8e`
hypothesis is not linked to this path and is not a valid peer model yet.

## Current boundary and next acceptance point

The card-side preliminary transaction is functional. The newly closed ingress
chain moves the immediate boundary to the SIM manager's dormant task-20
notification: establish why task 21 does not set its notification latch and
post `0x120c`. The physical SIM device must continue to answer only register/FIQ
and APDU traffic; it must not post `0x120c` or construct the internal D0 object.

An eight-second coherent deep boot proves ordinary task-21 requests work, but
records neither a setting write to `0x10dcb7` nor `0x120c`. This is the next
bounded acceptance point. The previously mapped task-14/DSP radio-completion
path remains valid adjacent work, but is not ingress to this SIM-server object.

The post-completion session transition is now mapped. Task 17 owns the natural producer
of `0x13e2`; it is not an absent transport message. Once task 17 accepts `0x0434` (or the
parallel `0x0a22` completion), its phase handler can call `0x2b3f60` from `0x225b8c`.
The same publisher is called at `0x223a28` after the long-lived phase loop returns.
`0x2b3f60` publishes packed status `0x53e2`, carrying one firmware-owned pointer, and the
normal task-5 path continues as follows:

```text
task-17 completion branch -> 0x2b3f60(packed 0x53e2, firmware pointer)
  -> task-5 status 0x13e2 -> handler 0x255124
  -> transient descriptor bytes at 0x110f1f / object pointer at 0x110f20
  -> 0x28a4a8 -> request builder 0x238a24
  -> status 0x1776 object -> serializer 0x23879e
  -> post to decimal task 14 (ID 0x0e) at 0x2389fe
```

`0x13e2` is a real three-byte entry in the ROM message catalogue at `0x2cb810`, and its
packed one-argument producer literal `0x53e2` is at `0x2b3f64`. It remains absent in the
current runtime because task 17 never receives the preceding `0x0434`; the task-5
dispatcher and the `0x28aXXX` consumer are not the broken boundary. A scheduler-only trace
briefly misattributed live `0x13b5` to task 20 because `0x2af6ea` allocates and can reschedule
before posting. Tracing publisher entry preserves the caller and confirms why task identity
must be sampled before allocation.

The next investigation must therefore trace every initialization, subscription,
and write path for `0x10dcb7` and its containing task-21 state, compare the two
`0x120c` producer preconditions with the working APDU path, and make only the
smallest faithful hardware/boot-order correction needed to let firmware post
`0x120c` itself. It must then verify the mapped
`A0/12 -> 0x006a/D0 -> 0x177x -> 0x267e68 -> 0x05dc` chain in one coherent run.

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
