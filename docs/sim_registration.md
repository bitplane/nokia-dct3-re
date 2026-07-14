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
  -> SELECT/GET RESPONSE/READ 6F05 (LP)
  -> SELECT/READ 6FAD (AD)
  -> SELECT/GET RESPONSE/READ 6F38 (SST)
  -> SELECT/GET RESPONSE 6F7E (LOCI)
  -> SELECT/READ 6F07 (IMSI)
  -> SELECT/READ 6F78 (ACC)
  -> SELECT/READ 2FE6
  -> SELECT 6F14 -> 94 04 (optional CPHS Operator Name String absent)
  -> remaining GSM/vendor file pass
  -> SELECT 6F99 -> 94 04 (optional file absent)
  -> timed directory STATUS presence monitor
```

Correcting the GSM 11.11 MF/DF response layout and supplying requested LP and
SST files lets `[0x111c79]` become 1 organically at about 1.309 s and reaches
the extended file sequence above. `6F14` is optional CPHS Operator Name String;
parser `0x201876` accepts at most 24 transparent bytes. The faithful synthetic
card profile omits CPHS, so unsupported SELECT now returns `94 04` and preserves
the previous selection. Firmware accepts that result and completes the remaining
file pass (**S/R**).

The later repeated `A0 F2` commands come from presence-monitor function
`0x2028a4`, not the initialization sequencer. At its first entry the coherent
run has SIM enable 1, no-SIM 0, ready 1, and selected directory `7F40`.
Separately tracking current DF makes STATUS report `7F40`; four observed polls
take steady exit `0x20290a` and none takes changed-directory exit `0x2028f4`.
`0x2028fc` rearms timer `0xea` with delay `0x181`, explaining the observed
roughly 42 ms cadence. The repeated APDU remains because real firmware polls
the card; the initialization loop and false directory-change handling are gone (**R**).

In the coherent contact profile task 1 enters mode `0x0004` at about 1.435 s. Its next
firmware predicate is report code `0x07`, posted by `0x2af190`. Runtime reaches
the reporter's status-dispatch caller `0x27b370` with the normal global status
`0x05e2`; that dispatch does not report ready. Backward mapping of all four
callers identified this additional dormant route (**S/R**):

```text
callback-table index 0x1a receives status 0x1400
  + message type 0x85 + state [0x11fced] == 2
  -> controller status 0x08ac
  -> telephony-control initialization drains its four activity slots
  -> controller status 0x0795
  -> handler 0x255c30 -> reporter 0x2af190
```

The old trace hook at `0x255c2e` was two bytes before the branch target and
therefore could not fire; the concrete dispatcher census proves `0x0795`
selects `0x255c30`. Further backchaining corrected the route's priority:
`0x1400` is the event decoded from an argumentless task-5 mailbox message at
`0x2af57c`, not a callback return. All 188 direct calls to task-5's event helper
`0x2af6ea` have recoverable constants and none emits `0x1400`; the remaining
unresolved-destination send is a service router whose table does not target
task 5. Neither `0x1400`, `0x08ac`, nor the `0x0795` producer runs in the
coherent profile. This makes callback `0x1a` a mapped dormant/service-specific
contract, not the strongest ordinary-startup hypothesis (**R**).

Callback-table index `0x5d` at `0x27b370` is a mapped completion branch, not an
established ordinary-boot owner. Initialization invokes it with `0x05e2`, and
its state slot is already selected organically as `0x0b`. Direct inputs
`0x05eb` and `0x06c5` call the code-7 reporter. Inputs `0x05e1`, `0x05e7`, and
`0x05dc` instead schedule task-local class `0x52`; the task-5 recode table
returns that timeout as `0x06c5`, completing the same route. The earlier
`0x06dc` interpretation was a subtract-cascade arithmetic error. Callback-table
flags `0x01a00000` select initialization status `0x032d`, rather than automatic
`0x05dc`, so organic activation does not itself start the completion timer.
The later context-completion census narrows that owner further. Manager
`0x24d588` reaches chooser `0x24d716` with callback-state slot `0x45`
(`[0x11fcc5]`) still zero and emits `0x09d1`. For callback `0x5d` state `0x0b`,
that sibling descriptor is ineligible; `0x09d0` is eligible and carries action
`0x00dc` into the mapped completion. A coherent write-watch records no nonzero
slot-`0x45` write, and selector-`0x45` catalogue descriptors all specify
no-transition state `0x3f`. The missing fact is now the direct/indexed writer
or framework owner that establishes slot `0x45` during ordinary boot.
A valid task-13 segmented transfer produces `0x05eb` directly for task 16 via
`0x23e1a4`; it does not feed the global callback sweep or callback `0x5d`.
Correlating such a transfer with the organic DSP `{0x0a, 0x09}` transaction
completed the parser without code 7 or a mode advance. Code 7 and SIM
initialization remain parallel predicates. The display lifecycle documented in
`resource_providers.md` is a valid service/test route to code 7, but is excluded
as the ordinary predecessor. Its separate `0x06ca -> 0x0795` path requires
startup/display state 7; the ordinary mode-4 fallback establishes state 3.

The historical `MODEL_STARTUP_REPORTS` run does not contradict this ownership.
Under the coherent contact ordering its fixed 950 ms hook returns code 7 and all
later reports while task 1 is still in mode `0x000d`; the wrong phase consumes
them before mode 4 begins. It is a superseded timing bridge, not evidence that
the mode-4 predicate has been satisfied.

The corrected card now requests and reads `EF_IMSI (6F07)`. The older absence
claim described the malformed-directory run and is superseded (**R**).

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

## Callback 7: a later-session lifecycle

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

The post-operation loop is statically closed, but it is not a peer boundary.
Seven branches in task 15 load `0x09ee` and call its internal message builder
`0x208ee0(0x09ee, 0)` (`0x20cbbe`, `0x20ce70`, `0x20cfa8`, `0x20cffa`,
`0x20d956`, `0x20f262`, and `0x20f432`). The task-15 receive branch at
`0x209274` forwards that firmware-constructed message through `0x251cc8` to
task 17. Task 17 maps it at `0x22270c` to a newly constructed `0x0a2e`, which
returns to task 15 and selects the router branch above. None of those seven
completion sites, `0x09ee`, `0x0a2e`, or the factory calls occurs in the
coherent run. The missing predecessor is therefore the object-bearing input to
the task-15 operation, not either status in this local completion loop.

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

The callback's missing initialization lifecycle is now pinned more precisely.
Status `0x05dc` selects the branch at `0x261ba8`, which calls `0x2633d0`, installs
four transient service-5 descriptors through initializer `0x2616bc`, and calls
`0x263154(5, 1)` to start the transient session. The four static registrations
are:

| callsite | descriptor |
| --- | --- |
| `0x2616e2` | `0x2dfb1c` |
| `0x2616f4` | `0x2dfb38` |
| `0x261706` | `0x2dfb54` |
| `0x261710` | `0x2dfb70` |

Here `0x2632fc` is the transient-registration API, while `0x263154` is the
transient handler/completion routine. Its tail at `0x2632be` publishes global
`0x05eb` once `0x26265c` reports that no registrations remain. The resident
registration API `0x263d30` has a parallel completion tail at `0x263e64`.
This explains how the later service-5 chain is initialized, but it does not make
that initialization an ordinary-boot prerequisite.

This lifecycle is selected per callback; it is not a global `0x05dc` broadcast.
In a coherent trace callback `0x28` receives `0x05f3` followed by `0x05e2`, both
with state zero and descriptor flags `0x3ac00000`. Callback `0x2f`, by contrast,
reaches the dispatcher path at `0x2ac5cc` that synthesizes `0x05dc`. The observed
`0x05f3` is emitted by the registry-change tail `0x2ae47e` after ordinary
initializer `0x26084e` calls `0x2ae3e4(0x10, 4)`; it is not an external peer
message and does not directly reconfigure callback `0x28`. A callback input/
return trace resolves the apparent discrepancy: terminal status `0x012f`
deliberately selects normal application callback `0x2f` through a catalogue-
generated `0x05e0` switch; the switch walker then supplies that selected callback
with `0x05dc`, and callback `0x2f` returns `0x05dd`. There is no equivalent
ordinary selector for callback `0x28`, nor evidence that one is required.
Service-5 initialization is therefore excluded from the immediate code-7
frontier unless a separate ordinary selector is demonstrated.

The later service-`0x0a` activation hypothesis is also disproven. Although an
ordinary coherent boot installs service-`0x0a` registrations and performs its
configuration reads, it never calls registry predicate `0x26265c` or starts a
draining transient transaction. More decisively, exhaustive recovery of packed
`0x0394` constructors finds only services `0x08`, `0x19`, and `0x1a` at
`0x27703a`, `0x2774c0`, and `0x277c4e`. The dynamic
`0x0394 -> 0x28c64c -> 0x263154(service, 1)` branch therefore cannot be used as
evidence that ordinary service `0x0a` is missing an activation event (**S/R**).

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

The firmware-owned object classifier at `0x267e68` reads the current lower-radio
object from controller state, selects callbacks `0x35..0x3a`, and publishes packed
event `0x85e0` with selector and object arguments. Normal dispatcher expansion
then places the selector at `0x110f1c`, the object at `0x110f20`, and supplies
lifecycle input `0x05dc`. It is not, however, an independent non-SAT predecessor.
The task-5 event table maps `0x177f`, `0x1782`, and `0x1783` to controller modes
`0x21`, `0x22`, and `0x23`; those modes select callback `0x37`, `0x38`, and `0x38`
respectively. All three inputs belong to the proactive-SIM `0x1770..0x1788`
family described below. The direct call at `0x254aac` handles status `0x1978` and
only re-runs the classifier against controller mode and object state populated by
an earlier event. It does not construct an object. Both classifier call paths are
dormant in the coherent run, and callbacks `0x37`/`0x38` are excluded from the
ordinary-registration frontier until a non-SAT writer of those controller modes
or their object slot is demonstrated.

### SIM Toolkit path (excluded as the registration predecessor)

Task 21 constructs task-20 status `0x120c` at `0x27e3cc` or
`0x27e8fe` and includes a selector byte. Task 20 handles it at `0x2085ce` and
calls `0x203d2c`, which reaches `0x2938b0`. That routine builds an internal
SIM-server request beginning `03 a0 12 00 00 <selector>`. `A0 12` is the GSM
11.14 FETCH instruction. A successful response begins with halfword `0x006a`;
its proactive-command BER-TLV (tag D0) at reply+4 is parsed by `0x2726e0`.

The D0 parser publishes one of the `0x1770..0x1788` task-5 events. Router
`0x253e20` then reaches `0x269524`/`0x267e68`, which publishes packed `0x85e0`
with a callback selector and firmware-owned object for applicable proactive
command types:

```text
task 21 status 0x120c -> task 20 / 0x203d2c
  -> synchronous task-21 A0/12 request -> 0x006a + D0 response
  -> 0x2726e0 -> 0x177x -> 0x269524 / 0x267e68
  -> packed 0x85e0(selector, object) -> callback lifecycle 0x05dc
```

The latch contract is fully mapped. Dedicated function `0x27def4` sets
`0x10dcb7`; callers `0x2037e6` and `0x29383c` invoke it immediately before
TERMINAL PROFILE (`A0 10`) and TERMINAL RESPONSE (`A0 14`). A returned `91xx`
status denotes a pending proactive command; while the latch is set, firmware
posts `0x120c`, issues FETCH, and clears the latch. The FETCH itself travels
through the ordinary task-21/SIMI transport.

The emulated card reports EF_PHASE `0x02`, correctly declaring a Phase-2 card
without profile download. Firmware gate `0x203906` calls TERMINAL PROFILE only
for parsed phase >= 3. An isolation run made EF_PHASE return 3, but the current
boot never reached function `0x2038ec`: profile download is downstream of the
registration wall. Therefore no hardware or scheduling correction is missing
at this latch, and manufacturing `91xx`, `0x120c`, or a D0 object would bypass
the firmware lifecycle rather than repair it.

Validated DSP RX families (type `0x70`, type `0x80`, types `0x83..0x99`, and
translator classes 3/5/17/47) have no edge into status `0x120c`, `0x2938b0`,
or this task-21 request. The DSP-ingress hypothesis is therefore disproven for
this SAT object path, rather than for registration generally.

In the retained coherent deep run, normal SIM APDU requests prove task 21 works,
but no task message `0x120c` occurs and none of `0x203d2c`, `0x2938b0`,
`0x2726e0`, `0x267e68`, or `0x05e8` is reached. This absence is expected for
the advertised Phase-2 card and no longer identifies the registration frontier.

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

This is the normal route for the observed message, not a failed attempt to take
an adjacent `0x09ee` branch. Task 17 constructs `09 ec 00 ...`. Task 15's receive
dispatcher copies byte `+2` into protocol-mode byte `0x10fe49` at `0x209efe`.
The handler at `0x20e74a` takes its direct `0x09ec` fast path only for mode
`0x10`; mode zero deliberately publishes `0x07d6`. A bounded write-watch proved
the organic zero comes from the message and is not stale initialization state.

Task 17 consumes `0x043c` as bookkeeping and remains in its phase loop. The
completion it accepts is `0x0434` or `0x0a22` (with later `0x1583`/`0x1584`
phase events); it does not construct the session object and is not a substitute
for the later `0x09ee` result. Task 10 can produce `0x0434` from finalizer
`0x219e30`, but no direct task-3 completion edge into that finalizer is proved.

The coherent run also pins a separate radio state-publication path at the DSP boundary. Task 10 creates
`{00 02 44 1a 00 81 98 ...}` and posts it to task 3. Task 3 serializes queued
work through `0x290840` into the MCU-owned DSP TX ring. With the DSP peer cadence
decoupled from the old 5 ms phase-lock (the opt-in model now defaults to 4 ms),
the earlier type-`0x51`/`0x70` backlog drains and this object appears organically
as `type 0x1a, payload 68`, beginning `441a 0081 9800`.

This packet is not a proved request for the missing completion. Builder
`0x219f0c` is called only by the task-10 state dispatcher at `0x21ba54`. It
retains no transaction token, reply object, or task-3 completion callback after
posting the packet; it arms the global DSP cadence timer, clears the activity
counter, and returns. Task 17 has already received `0x043c`. Type `0x1a` is thus
classified as a fire-and-forget DSP state/control publication unless a future
consumer correlation proves otherwise.

The apparent type-`0x80` correlation at `0x284c74` is numeric coincidence. It
tests radio-controller state byte `0x10dbd2`, which is `0x00` at this organic
send, against later state `0x1a`. Its inner-command-`0x60` handler can publish
task-10 status `0x1395`, not completion status `0x1391`.

DSP-to-MCU RX delivery is also pinned to FIQ 0. A correlated type-`0x80`,
primitive-`0x70` candidate traverses the real RX ring and task-4 dispatcher, but
task 13 rejects it for this registration context even when delayed by up to
306 ms. That response hypothesis is falsified and its probe was removed.

The broader DSP-to-task-13 route does not recover the missing registration lifecycle.
Type-`0x80` and type-`0x83` packets can normalize into `0x040b`; accepted
segmented transactions are parsed at `0x23e324`/`0x23e378` by
the task at `0x23e62c` and produce task-16 status `0x05eb`. Callback `0x3c`
handles `0x05eb` at `0x25df18`, publishes
`0x057a`, and returns `0x05e6`. It does not reach that callback's `0x05e8`
publisher at `0x25df08`, nor construct an object-bearing `0x05dc`. This closes
the apparent numerical connection between this transaction and the
registration frontier. The task-13 subsystem owner remains unresolved.

The `0x2697aa(0x23, 0x0a0a)` call made after constructing type `0x1a` is not a
task-10 timeout message. `0x2697aa` indexes a global 12-byte timer control block;
runtime tracing shows timer `0x23` expiring roughly every 34 ms, waking task 4,
and being rearmed immediately. Task 10 receives no timer-derived status and
`0x219e30` remains dormant. The timer wheel and DSP service cadence are therefore
working; timer expiry is not the missing semantic completion.

The relevant task-10 completion transition is status `0x1391`, not `0x1392` or
`0x138f`. Task 17 enters
its long-lived event loop through `0x223964 -> 0x2271c6`; `0x2222fc` and the
`0x138f` callback family occur only after that loop returns, so they cannot
produce the `0x0434` the loop is awaiting. The similarly numbered DSP packet
type `0x89` is also an initialization-only route (`[0x112501] == 0`); a real
FIQ-0 probe at the current frontier was rejected and removed.

The task-10 jump table at `0x21b4a0` maps `0x1391` to `0x21b9b4`, which can
reach `0x21b198` and the `0x0434` producer at `0x219e30`. Its adjacent entry
`0x1392` maps to `0x21b790`, a radio-state/configuration update path. The large
lower-radio result table distinguishes their producers:

```text
result 0x0fc1 -> 0x245c8c -> wrapper 0x2525be -> 0x2b610a
  -> task-10 0x1391 -> completion path -> task-17 0x0434

result 0x0fc2 -> 0x245c76 -> wrapper 0x2525a8 -> 0x2b60f6
  -> task-10 0x1392 -> radio-state update 0x21b790
```

Task-14 object decoder branch `0x2674da` handles opcode byte `0x36`, parses its
payload through `0x266ffc`, and emits controller event `0x1033`. The controller
branch at `0x2476fa` maps that event to normalized result `0x0fc1`. Adjacent
opcode `0x37` instead emits event `0x1034` and result `0x0fc2`. The completion
object itself is constructed by task 15 after the still-absent service-5 object
chain; it is not a raw DSP ingress and must not be injected at task 14.

The dispatcher input `0x0fbf` is loaded from a ROM literal at `0x24788e` on the
lower-radio controller's event-`0x102f` branch. The branch enters `0x246ad6`
after the neighboring `0x0fca` literal load, preserving `r0 = 0x0fbf`; its table
case is `0x245cb2 -> 0x253610`, not either task-10 publisher. The earlier
`0x0fc3` value and the later `0x245c76` association were separate jump-table
indexing errors. Decoder `0x267258` returns event `0x102f` for an incoming object
whose opcode byte is `0x2a`; task 14 calls that decoder for statuses
`0x09d8`/`0x09de`. Status `0x09d8` is constructed by
task 15 itself after its `0x07dd` object parser succeeds and returns internal result
`0x09f3`; it is not a raw peer status. The exact transport which supplies the
earlier generic-service object remains unresolved. The prior task-8 `0x8d`/`0x8e`
hypothesis is not linked to this path and is not a valid peer model yet.

## Current boundary and next acceptance point

The card-side preliminary transaction is functional. The `0x120c` latch is not
the immediate boundary: it belongs to downstream SIM Toolkit operation and is
correctly dormant for the current Phase-2 card. The ordinary-registration
frontier therefore returns to the mapped lower-radio/session paths which can
construct callback lifecycle `0x05dc` without SAT.

An eight-second coherent deep boot proves ordinary task-21 requests work. Its
lack of `0x10dcb7`/`0x120c` activity is expected and must not be treated as a
failed hardware model. The previously mapped task-14/DSP radio-completion path
remains valid adjacent evidence and is again a candidate for ordinary
registration, subject to an evidenced producer/consumer edge.

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

One non-SAT object-bearing later-cycle predecessor is identified. The catalogue entries for
statuses `0x06cd` and `0x09cd` both dispatch to the large context handler at `0x24df74`.
Its branch at `0x24e74e` publishes packed event `0x85e0` with selector 7 and the real object
pointer from `[r4+4]`; the callback engine subsequently supplies lifecycle input `0x05dc`.
This is distinct from the SAT-owned callback family selected by the D0 classifier.

The only direct `0x09cd` producer is the context routine at `0x24d8e8`. When its input slot
matches active slot byte `0x10e89a` and callback-registration state byte `0x11fcdd` is 1 or 2, it emits the
ordered sequence `0x09c9`, `0x09cd`, `0x151c`. In an earlier eight-second registration run,
`0x11fcdd` was observed as zero after task-5 registration processing and never changed; none of
`0x24d8e8`, `0x24df74`, or `0x24e754` executed. This confirms that the context
route was dormant in that profile; it does not make that route the current
bootstrap boundary. The later code-7 sweep observes slot `0x5d == 0x0b`, which
still does not satisfy this context routine's required state 1 or 2.

`0x11fcdd` is not a pending selector. It is callback-state array slot `0x5d` at
`0x11fc80 + 0x5d`. Catalogue registration at `0x2aefba` selects descriptors from
`0x2cc7f0`; eligibility helper `0x2aeda0` checks the old state, and `0x2aefa2`
commits the descriptor's six-bit new state. The descriptors recovered for callback
`0x5d` contain transitions from states `1`, `2`, `3`, `4`, or `0x0b`. The
current code-7 run reaches state `0x0b`, while the later context producer still
requires state 1 or 2. Callback 7 is therefore a
configuration or later-session cycle, not the ordinary-registration bootstrap
frontier.

The external callers are now connected to one concrete context-initialization cycle. A task-14
`0x09d8` object with opcode byte `0x3a` selects decoder branch `0x267414`, which parses field
type `0x1c` and emits lower result `0x0fc8` at `0x26746e`. The lower-result dispatcher reaches
`0x252aac`; parser `0x2528cc` classifies its type-2 object and `0x2b5cf8` publishes task-5
event `0x0ac8` command `0x16`. Task 5 populates the context slots and enters the task-5/task-20
handshake which can ultimately call `0x24d8e8`, emit `0x09cd`, and construct callback 7.

This route is **not** the first bootstrap producer. The only proved `0x09d8` constructor is
task-15 translator `0x208ee0`, reached after the absent `0x05e8 -> 0x05ea -> 0x07dd` service
object has already parsed successfully. The opcode-`0x3a` route is therefore a valid later
session cycle but circular as a correction for the initial `0x05e8` wall. Status `0x09de`
is another output of the same task-15 translator: input `0x0a0c` reaches `0x209116`, where
task-15 mode `0x1f` selects `0x09de` (modes `0x20` and `0x21` select `0x09eb` and `0x09ca`).
No immediate, PC-relative ROM literal, or statically recovered call argument supplies
`0x0a0c` to `0x208ee0`. A dynamic caller remains possible, but `0x09de` therefore does not
establish an independent peer ingress or an ordinary-bootstrap producer.

The generated census now inventories every direct packed `0x05e0` constructor with
at least two argument words and requires a reviewed assessment for each callsite.
Intersecting that closed set with callbacks containing direct `0x05e8` publishers
leaves owners `0x21`, `0x22`, `0x26`, `0x3c`, and `0x54`, plus the mixed dynamic
`0x35..0x3a` classifier. The recovered constructors for `0x22`, `0x3c`, and `0x54`
carry constant or null object words, while the proved `0x37`/`0x38` dynamic modes
are SAT-derived.

Backward tracing excludes `0x21` and `0x26` from the coherent bootstrap as well.
Callback `0x24` at `0x28b844` builds a callback-`0x21` object through `0x228aa0`
only in generic-framework mode 9 after input `0x013d`. It is also the only recovered
direct producer of global `0x0388`, at `0x28b926`, `0x28b960`, and `0x28b9aa`,
and those branches require framework mode 11 and inputs `0x013c`, `0x013a`, or
`0x0136`. Active callback `0x26` maps `0x0388` to `0x05e8`. The other `0x21`
constructor is callback-`0x54` input `0x1a34`; the five pointer-bearing `0x26`
constructors belong to callbacks `0x1f`, `0x1e`, `0x51`, `0x29`, and `0x79`.

In the coherent eight-second run callback `0x24` receives only global sweep
`0x05e2`, with framework mode 0; none of the constructors or `0x0388` sites runs.
Callbacks `0x21` and `0x26` then receive the same sweep but no lifecycle object.
Their direct `0x05e8` publishers are status-specific fallback/cleanup branches
(`0x0580` and `0x0388`), not a demonstrated registration-success path. This
removes the last non-scalar intersection candidates rather than identifying a
missing peer response.

The scalar intersection is downstream too. Callback `0x21` constructs callback
`0x22` at `0x228a66`; callbacks `0x51` and `0x25` construct callback `0x3c` at
`0x25d25c` and `0x25d6d0`; callbacks `0x55` and `0x42` construct callback `0x54`
at `0x24a8f4` and `0x2a8868`. Their direct `0x05e8` sites converge later
failure/cleanup lifecycles. Consequently every direct object-bearing constructor
whose selected callback can itself publish `0x05e8` is now SAT-derived,
mode-gated later-cycle, or downstream failure convergence. The direct-constructor
census does **not** identify a missing ordinary-registration peer response.

The former runtime-built-event caveat is closed as well. The census requires a
reviewed bound for all 22 calls to `0x2af798` whose packed event is not recovered
as a static value. Only the five calls in parameterized helper `0x26b58c` carry
two argument words. Its only ROM callers bound their completion events to
`0x0594` or `0x0c2d`; the other dynamic calls are argumentless or one-word
events. None can construct object-bearing lifecycle event `0x05e0`. Together,
the direct and runtime-built inventories are a quantified absence proof: this
ROM contains no unclassified MCU-side packed-event constructor that can create
the missing non-SAT `0x05dc` session lifecycle.

The mixed-looking controller classifier is SAT-owned in this boot vocabulary.
Its mode byte is `0x110a64`; helper `0x269524` stores the mode and object before
calling `0x267e68`. Concrete execution of the unmodified dispatcher
`0x253e20` over every 16-bit input recovers the complete selector map:

| input event | controller mode | lifecycle callback |
| --- | --- | --- |
| `0x177c` | `0x10` | `0x39` |
| `0x1782` | `0x11` | `0x3a` |
| `0x1779` | `0x13` | `0x36` |
| `0x177a` | `0x20` | conditional control path / `0x37` |
| `0x177d` | `0x21` | `0x37` |
| `0x1780` | `0x22` | `0x38` |
| `0x1781` | `0x23` | `0x38` |
| `0x1777` | `0x24` | `0x35` |

All eight inputs are members of the proactive-SIM `0x1770..0x1788` family.
Callbacks `0x3a` and `0x36` do contain organic `0x0578` publishers, but their
only dispatcher predecessors are SAT events `0x1782` and `0x1779`. They are not
ordinary-registration activation candidates. This closes the final dynamic
classifier exception in the non-SAT constructor proof.

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
