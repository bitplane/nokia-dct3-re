# SIM registration firmware map

This is the evidence-based map of Nokia 3210 SIM registration. It records the contracts needed
to continue the driver, not the sequence of experiments that discovered them. Falsified
hypotheses are indexed by id in `evidence/falsifications.json`; probes and chronology remain in
Git history.

Addresses refer to the correctly byte-swapped 3210 v6.00 image. Use `roms/3210f600a_swap16.bin`
and `tools/disrom.py`; analysis of the raw `.fls` produces plausible but incorrect Thumb
control flow.

Evidence labels: **S** static disassembly/decompilation; **R** observed at runtime in a
coherent boot; **I** isolation experiment only, not an implementation contract.

## Ownership

| owner | task | role |
|---|---:|---|
| SIM hardware manager | 21 (`0x15`) | reset, ATR/PPS, T=0 exchange, status; loop at `0x27eae0` |
| SIM/configuration client | 20 (`0x14`) | parses replies, controls extended EF reads, owns ENABLE |
| registration coordinator | 17 (`0x11`) | preliminary phase and `0x1583/0x1584` lifecycle |
| generic result parser | 15 (`0x0f`) | converts lower object results into registration events |
| task-5 callback engine | 5 | owns radio/resource callback selection and packed events |

The physical SIM owns bytes and timing at the MAD2 SIM interface. Radio sessions, callback
construction, task events, commit state, and configuration flags are phone/baseband state and
must not be manufactured by the card.

## Principal state

### Task-21 SIM manager `0x10dca8`

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

Transport buffers are `0x10deec` (command/request descriptor) and `0x10dddc` (received
response). Task 21 receives card data through the SIM UART/FIQ path, not through a
card-specific firmware shortcut.

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

Task 5 expands packed descriptors at `0x2aee20` and performs callback switches through
`0x2ac3f2`. `0x110f1c..0x110f2b` is a transient scratch vector, not persistent session
storage: callback selector/index at `+0`, argument 1/object at `+4` (`0x110f20`), descriptor
type at `+0xb`, behavior/descriptor flag at `0x110f27`. The stack at `0x111930` stores
callback indices during switching; it is not an object queue. Runtime write watching
identifies startup clearing at `0x200168`/`0x20016a`, task-5 receive copies at
`0x2af594`/`0x2af696`, and internal queue bookkeeping at `0x2af75e`, `0x2af770`, and
`0x2af78e` (**R**). The engine consumes an already-produced descriptor; it is not a hidden
session creator.

## Organic card conversation

With the 3210 machine profile's stateful SIM device enabled, the device and firmware complete
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

A GSM 11.11 MF/DF response layout with the requested LP and SST files lets `[0x111c79]` become
1 organically at about 1.309 s and reaches the extended file sequence above, including
`EF_IMSI (6F07)`; a malformed directory layout instead holds firmware in the preliminary pass
without ever requesting IMSI (ledger `sim_imsi_never_read_during_init`) (**R**). `6F14` and
`6F99` are optional files answered `94 04`; the CPHS/`94 04` card contract is authoritative in
`sim_emulator_scope.md`.

The later repeated `A0 F2` traffic is the firmware-owned card-presence monitor, not the
initialization sequencer; its contract (function `0x2028a4`, timer `0xea`) is authoritative in
`sim_subsystem.md` (**R**).

Task 1 enters mode `0x0004` after this conversation and remains interactive. Report code
`0x07`, callback `0x5d`, controller status `0x0795`, and the Advice-of-Charge lifecycle are not
SIM-registration prerequisites; their ownership and power/UI semantics are authoritative in
`mmi_settlement.md` and intentionally not duplicated here.

## Task-20 reply contract

Task 20 receives registration messages in its loop around `0x20837a`. The legitimate path is:

```text
task-20 message 0x1196
  -> handler 0x207234
  -> 0x293f30(request, reply, mode=1)
  -> return value 2
  -> 0x20731a
  -> 0x20733c sets 0x111c79=1 and related success state
```

Return value 2 is derived from the coherent request/reply data parsed by `0x293f30`; it is not
encoded solely by message ID `0x1196`. Isolation runs which force-called the real `0x1196`
producer delivered the message but never reached `0x20733c` (**I**); message delivery is
necessary but not sufficient. Once ENABLE is set, gate `0x208218` also requires the mailbox
drained and the other gate bytes clear, then starts the full EF walk, including IMSI, from the
table at `0x2e0c04` (**S**).

## Producer chain to `0x1196`

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

The commit implementation is the fall-through tail at `0x254bb4` inside `0x253d30`. Swap16-aware
decoding is required to place it: it belongs to router case `0x1770`, not `0x177b`. It consumes
a populated session, emits the `0x1199/0x1196` family, and frees the session (**S**). Router
case `0x1770` is outside the SAT classifier map tabulated in the frontier section. Replaying
commit keys or calling its producers directly is a forcing probe, not organic registration
(ledger `forced_registration_messages`).

## Callback 7: a network-session lifecycle

Callback-table entry 7 is handler `0x29992c` (**S**). In a coherent boot it is visited once by
the global `0x05e2` sweep and never receives constructor lifecycle `0x05dc` (**R**). A real
constructor call proceeds:

1. `0x299bc4` queues internal `0x0aa0`.
2. Fall-through `0x299bda` copies framework argument 1 from `0x110f20` into callback state slot
   `0x1120e4`.
3. The later `0x0aa0` branch allocates one of four contexts at `0x10ea20..0x10ea9f`, attaches
   the existing object, and queues packed `0x5518`.
4. `0x27a00c` posts `0x1583` when the object's mode byte permits it.

Helper `0x277cb4(0x192, 0, 0)` is scheduler bookkeeping, not an object allocator. No other ROM
literal addresses `0x1120e4` (**S**). Callback 7 manages radio resources `0x42..0x45` and
lower-layer contexts; its object is a GSM/radio-session object, not SIM-card state. The SIM
device must not select callback 7, publish `0x05dc`, populate `0x110f20/0x1120e4`, allocate
contexts, or emit any downstream event.

### Session-object factory

Helper `0x24f120` constructs the object consumed by callback 7 (**S**): an `0x18`-byte
descriptor whose fields are later read by `0x24f25c` — owned payload pointers at
`+0x00`/`+0x04`, status fields at `+0x10`/`+0x12`, context selector at `+0x13` (initially
zero), request parameters at `+0x14..+0x16`.

The four factory call sites (`0x2996aa`, `0x2997dc`, `0x299860`, `0x2998a0`) are branches of
one function, `0x299610`; every successful branch publishes its descriptor through
`0x2af798(0xca8a, 0x1e, object)`. The function is reached from the task-15 router at `0x255a34`
on status `0x0a2e`.

The post-operation loop is statically closed but is not a peer boundary. Seven task-15 branches
load `0x09ee` and call internal builder `0x208ee0(0x09ee, 0)` (`0x20cbbe`, `0x20ce70`,
`0x20cfa8`, `0x20cffa`, `0x20d956`, `0x20f262`, `0x20f432`). Receive branch `0x209274` forwards
that firmware-constructed message through `0x251cc8` to task 17, which maps it at `0x22270c` to
a new `0x0a2e`, returning to the router branch above. None of the seven completion sites,
`0x09ee`, `0x0a2e`, or the factory calls occurs in the coherent run. The missing predecessor is
the object-bearing input to the task-15 operation, not either status in this local loop
(ledger `task15_09ee_is_peer_ingress`).

## Adjacent paths and exclusions

- The global callback sweep deliberately supplies `0x05e2`; it is not a constructor sweep.
  Terminal status `0x012f` selects normal application callback `0x2f`, which then receives
  `0x05dc` (**S/R**).
- After preliminary SIM status `0x13b5`, the callback engine intentionally reconstructs
  callback `0x2f` with argument `0x0787`, then delivers `0x05e0`, `0x05e1`, `0x0b5b`, and
  `0x1581` to it (**R**). This is not a failed callback-7 selection.
- Status `0x0518` with argument 7 is input to the already-current callback; it does not select
  callback 7 (**S/R**).
- Resource `0x67`, installed by callback `0x2f`, is local resource-manager state; runtime
  lookups only replace/read it and generate no peer transaction (**R**).
- `0x0588` is a next-item operation inside an existing selector session; it publishes
  `0x05e0(selector+1)` and cannot originate callback 7 (**S**).
- Context-arbiter event `0x1388` requires a context which callback 7 itself would allocate;
  using it as the constructor predecessor is circular (**S/R**).
- Service-30 callbacks `0x79 -> 0x7a` form a valid NV/request sequence, but no
  descriptor-bearing constructor starts it; its `0x0578` is a downstream resource completion
  (**S/R**).
- Native EEPROM record `0x0757` variant 0 lives at offset `0x0db0`, length `0x190`. The
  ROM-derived default is useful fidelity but does not initiate the missing session (**S/R**).
- The DSP shared packet ring accepts only documented inbound families; a mirrored type-5
  packet or type-70 wrapper does not become the required generic-service object (**S/I**).
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

No `0x05e8`, `0x05ea`, or `0x07dd` occurs in the coherent run, and the two `0x0a08` producer
sites stay dormant while task 15's receive loop runs three times. Both are selected only when
the handler returns `0x09f3`, tying the missing task-14 `0x09d8` to successful parsing of the
`0x07dd` object rather than an independent unsolicited task-14 message (**S/R**). Delivering
the original object is an external-peer responsibility; its hardware transport is unproved.

`0x2638e4` is called only by the task-5 dispatcher at `0x2af652`, and callback `0x2618e8` is
the organic **service-5** callback selected by the callback table (`0x002db860`). In an
unforced deep run it receives `0x05f3` and `0x05e2` through the normal dispatcher. Its `0x05e8`
branch does not read an object argument from dispatcher scratch `0x110f1c`; it returns `0x05ea`
after checking downstream SIM/profile readiness. The absent condition is an organic `0x05e8`
publisher path with the required readiness state, not queued-object population (**S/R**).
Forcing `0x05e8` numerically does not demonstrate the organic predecessor (ledger
`direct_05e8_completion`).

The callback's initialization lifecycle is pinned. Status `0x05dc` selects the branch at
`0x261ba8`, which calls `0x2633d0`, installs four transient service-5 descriptors through
initializer `0x2616bc`, and calls `0x263154(5, 1)`:

| callsite | descriptor |
| --- | --- |
| `0x2616e2` | `0x2dfb1c` |
| `0x2616f4` | `0x2dfb38` |
| `0x261706` | `0x2dfb54` |
| `0x261710` | `0x2dfb70` |

`0x2632fc` is the transient-registration API; `0x263154` is the transient handler/completion
routine, whose tail at `0x2632be` publishes global `0x05eb` once `0x26265c` reports no
registrations remain. Resident registration API `0x263d30` has a parallel completion tail at
`0x263e64`. This explains how the later service-5 chain initializes; it is not an
ordinary-boot prerequisite.

The lifecycle is selected per callback, not broadcast. In a coherent trace callback `0x28`
receives `0x05f3` then `0x05e2`, both with state zero and descriptor flags `0x3ac00000`, while
callback `0x2f` reaches the dispatcher path at `0x2ac5cc` that synthesizes `0x05dc`. The
observed `0x05f3` is emitted by registry-change tail `0x2ae47e` after ordinary initializer
`0x26084e` calls `0x2ae3e4(0x10, 4)`; it is not an external peer message. A callback
input/return trace shows terminal status `0x012f` selecting callback `0x2f` through a
catalogue-generated `0x05e0` switch, after which the switch walker supplies `0x05dc` and
`0x2f` returns `0x05dd`. No equivalent ordinary selector exists for callback `0x28`. Service-5
initialization is therefore excluded from the ordinary non-SAT registration path unless a
separate selector is demonstrated (**S/R**).

The service-`0x0a` activation hypothesis is disproven: an ordinary coherent boot installs
service-`0x0a` registrations and performs its configuration reads but never calls registry
predicate `0x26265c` or starts a draining transient transaction, and exhaustive recovery of
packed `0x0394` constructors finds only services `0x08`, `0x19`, and `0x1a` at `0x27703a`,
`0x2774c0`, and `0x277c4e`. The dynamic `0x0394 -> 0x28c64c -> 0x263154(service, 1)` branch
cannot evidence a missing service-`0x0a` activation event (**S/R**).

An exhaustive Thumb literal-load scan qualifies the producer search: executable ROM loads
`0x05e2`, `0x05e7`, and `0x05ea` as immediates, but no instruction loads `0x05e8`. `0x05e8`
appears instead in firmware registration descriptors as a callback/event field — for example
callback-table index `0x24` (`0x28b844`) handles the global `0x05e8` transition and only then
registers a resident descriptor through `0x28ba9a -> 0x263d30`, absent from the coherent boot
and downstream, not initiating. Census tooling must therefore recover descriptor fields and
indirect event generation in addition to direct `0x2af798` call sites (**S**).

The message-topology census refines this without contradiction: 16 in-ROM calls construct
global `0x05e8` as `0xbd << 3` for `0x2af798`. In that ABI the high bits encode the argument
count; plain `0x05e8` carries **zero argument words**, and packed-event expansion converts the
low 13-bit status into the callback input without argument words. Entry `0x28` shares its
`0x05e8` branch with `0x05de` at `0x261b60`, checks SIM/profile state through `0x2608f8`,
`0x260df4`, and `0x260954`, and reads no scratch argument on that branch (**S**). The
unresolved predecessor is which sibling callback path can publish `0x05e8` organically while
entry `0x28`'s downstream readiness checks succeed.

All 13 callback owners containing the 16 recovered publishers are registered in the coherent
boot; each receives the global `0x05e2` sweep once and none publishes `0x05e8`. Their non-sweep
inputs are statically classified per owner in `message_topology_census.md`. Registration is not
the missing step; an organic input and its associated state/object must reach one of these
owners (**S/R**).

The firmware-owned object classifier at `0x267e68` reads the current lower-radio object from
controller state, selects callbacks `0x35..0x3a`, and publishes packed `0x85e0` with selector
and object arguments; dispatcher expansion then supplies lifecycle `0x05dc`. It is not an
independent non-SAT predecessor: its dispatcher predecessors are the SAT selector inputs
tabulated in the frontier section, and the direct call at `0x254aac` handles status `0x1978`,
only re-running the classifier against controller mode and object state populated by a
preceding event. Both call paths are dormant in the coherent run; callbacks `0x37`/`0x38` are
excluded from the ordinary-registration frontier until a non-SAT writer of those controller
modes or their object slot is demonstrated (**S/R**).

### SIM Toolkit path (excluded as the registration predecessor)

Task 21 constructs task-20 status `0x120c` at `0x27e3cc` or `0x27e8fe` with a selector byte.
Task 20 handles it at `0x2085ce` and calls `0x203d2c`, reaching `0x2938b0`, which builds an
internal SIM-server request beginning `03 a0 12 00 00 <selector>`. `A0 12` is the GSM 11.14
FETCH instruction; a successful response begins with halfword `0x006a`, and its
proactive-command BER-TLV (tag D0) at reply+4 is parsed by `0x2726e0`, which publishes one of
the `0x1770..0x1788` task-5 events:

```text
task 21 status 0x120c -> task 20 / 0x203d2c
  -> synchronous task-21 A0/12 request -> 0x006a + D0 response
  -> 0x2726e0 -> 0x177x -> 0x269524 / 0x267e68
  -> packed 0x85e0(selector, object) -> callback lifecycle 0x05dc
```

The latch contract is fully mapped. Dedicated function `0x27def4` sets `0x10dcb7`; callers
`0x2037e6` and `0x29383c` invoke it immediately before TERMINAL PROFILE (`A0 10`) and TERMINAL
RESPONSE (`A0 14`). A returned `91xx` status denotes a pending proactive command; while the
latch is set, firmware posts `0x120c`, issues FETCH, and clears the latch. The FETCH travels
through the ordinary task-21/SIMI transport.

The emulated card reports EF_PHASE `0x02`, a Phase-2 card without profile download. Firmware
gate `0x203906` calls TERMINAL PROFILE only for parsed phase >= 3; an isolation run with
EF_PHASE 3 never reached `0x2038ec`, so profile download belongs to a later session lifecycle.
Manufacturing `91xx`, `0x120c`, or a D0 object would bypass the firmware lifecycle rather
than repair it. Validated DSP RX families (type `0x70`, type `0x80`, types `0x83..0x99`,
translator classes 3/5/17/47) have no edge into `0x120c`, `0x2938b0`, or this task-21 request;
the DSP-ingress hypothesis is disproven for this SAT object path, not for registration
generally. In the retained coherent deep run no `0x120c` occurs and none of `0x203d2c`,
`0x2938b0`, `0x2726e0`, `0x267e68`, or `0x05e8` is reached; that absence is expected for a
Phase-2 card and does not identify the registration frontier (ledger
`sat_fetch_is_registration_predecessor`).

## Acknowledgement and DSP-boundary paths

The DSP-boundary transactions adjacent to registration are authoritative in
`dsp_interface.md`; this map retains their registration-facing conclusions:

- The coherent `task-17 0x09ec -> task-15 case 6 -> task-16 0x07d6 -> task-10 0x03e9 ->
  task-17 0x043c` route is the normal acknowledgement for that message: handler `0x20e74a`
  takes its direct `0x09ec` fast path only for protocol mode `0x10`, and the organic mode
  zero (byte `0x10fe49`, copied from the message at `0x209efe`) deliberately publishes
  `0x07d6` (**R**). It is not a failed `0x09ee` branch. Task 17 consumes `0x043c` as
  bookkeeping and keeps awaiting completion `0x0434` or
  `0x0a22` (with later `0x1583`/`0x1584` phase events); task 10 can produce `0x0434` from
  finalizer `0x219e30`, but no direct task-3 completion edge into it is proved.
- Outbound type `0x1a` is a fire-and-forget GSM ARFCN bitmap publication from builder
  `0x219f0c` (sole caller: task-10 state dispatcher `0x21ba54`), which retains no transaction
  token, reply object, or completion callback. Task 3 serializes it through `0x290840` into
  the MCU-owned DSP TX ring, and it appears organically once the DSP service
  cadence runs at 4 ms (the external-service peer profile default in `driver/nokia_3310.cpp`)
  (**R**). Ledger `dsp_type1a_direct_registration_request` also covers the numeric
  type-`0x80` coincidence at `0x284c74` (`0x1395`, not completion `0x1391`).
- A fabricated type-`0x80` primitive-`0x70` reply traverses the real FIQ0 RX ring and task-4
  dispatcher but task 13 rejects it even delayed up to 306 ms (ledger
  `dsp_type80_primitive70_reply`).
- The task-13 segmented route (`0x040b -> task-16 0x05eb -> callback 0x3c` at `0x25df18`)
  publishes `0x057a` and returns `0x05e6`; it reaches neither the callback's `0x05e8`
  publisher at `0x25df08` nor an object-bearing `0x05dc` (ledger `task13_05eb_is_code7_owner`).
  The task-13 subsystem owner remains unresolved.
- Timer `0x23` (`0x2697aa`) is a live roughly-34 ms service-cadence timer, not the missing
  semantic completion; `0x219e30` stays dormant (**R**).

The explicit lower-result completion transition is `0x1391 -> ... -> 0x0434` through jump
table `0x21b4a0`; adjacent `0x1392` is a non-completing radio-state update. A later full
decode corrects the former fixed-status exclusion: DSP type `0x87` becomes task-10 status
`0x138f` and can call finalizer `0x219e30` immediately when its two outstanding-work pointers
are null; type `0x8a` becomes `0x1390` and can call the same finalizer after a configured
report-count limit. Type `0x89` instead posts `0x1393` and advances controller bookkeeping.
The RF conditions that legitimately produce `0x87`/`0x8a` remain unknown, so this is a
mapped peer-contract family rather than permission to synthesize either packet. Lower results `0x0fc1`/`0x0fc2`
select `0x1391`/`0x1392` (produced from task-14 object opcodes `0x36`/`0x37`), while result
`0x0fbf` (ROM literal at `0x24788e`, loaded beside the neighboring `0x0fca` literal on the
event-`0x102f` branch) dispatches to context handler `0x253610`, not either task-10
publisher; two jump-table misreadings of this extent are ledgered as `lower_result_0fc3` and
`lower_result_0fbf_completes_task17`. The full chains are documented in `dsp_interface.md`.
The binding registration constraint is upstream: decoder `0x267258` is called for statuses
`0x09d8`/`0x09de`, and `0x09d8` is constructed by task 15 itself only after its `0x07dd`
parser succeeds and returns `0x09f3`. The completion object is therefore not raw DSP ingress
and must not be injected at task 14; the transport supplying the upstream generic-service
object remains unresolved, and the task-8 `0x8d`/`0x8e` hypothesis is not linked to this path
(ledger `task8_8d_8e_peer`) (**S**).

## Mapped downstream session lifecycle

The card-side preliminary transaction is functional: ordinary task-21 requests work in a
coherent deep boot, and the dormant `0x120c` latch is expected for the Phase-2 card, not a
failed hardware model (**R**). The lower-radio/session paths below are retained for later
network work and do not block the validated offline UI; the mapped task-14/DSP
radio-completion path is valid adjacent evidence and a candidate for ordinary registration,
subject to an evidenced producer/consumer edge.

Task 17 owns the natural producer of `0x13e2`. Dispatcher input `0x09d6` selects handler
`0x225b82`, which calls `0x2b3f60` at `0x225b8c`; the same publisher is called at
`0x223a28` after the long-lived phase loop returns. `0x2b3f60` publishes packed `0x53e2` with
one firmware-owned pointer:

```text
task-17 completion branch -> 0x2b3f60(packed 0x53e2, firmware pointer)
  -> task-5 status 0x13e2 -> handler 0x255124
  -> transient descriptor bytes at 0x110f1f / object pointer at 0x110f20
  -> 0x28a4a8 -> request builder 0x238a24
  -> status 0x1776 object -> serializer 0x23879e
  -> post to decimal task 14 (ID 0x0e) at 0x2389fe
```

`0x13e2` is a real three-byte entry in the ROM message catalogue at `0x2cb810`; its packed
one-argument producer literal `0x53e2` is at `0x2b3f64`. An opt-in DSP-boundary diagnostic now
delivers type `0x87` through MDIRCV/FIQ0 and makes task 10 publish `0x0434` organically. In the
active startup phase task 17 accepts it at `0x225240`, runs handler `0x225b6c`, and continues
at `0x226348` without publishing `0x13e2`. The task-5 dispatcher and `0x28aXXX` consumer remain
valid dormant downstream code, but `0x0434 -> 0x13e2` is not an edge: it joined the
`0x0434` arm at `0x225b6c` to the adjacent `0x09d6` arm at `0x225b82` (**S/R**).
Caution: a scheduler-only trace misattributes live `0x13b5`
to task 20 because `0x2af6ea` allocates and can reschedule before posting; sample task
identity at publisher entry, not after allocation.

One non-SAT object-bearing later-cycle predecessor is identified. Catalogue entries for
statuses `0x06cd` and `0x09cd` both dispatch to the large context handler at `0x24df74`, whose
branch at `0x24e74e` publishes packed `0x85e0` with selector 7 and the real object pointer
from `[r4+4]`; the callback engine then supplies lifecycle `0x05dc`. This is distinct from the
SAT-owned callback family selected by the D0 classifier.

The only direct `0x09cd` producer is context routine `0x24d8e8`: when its input slot matches
active slot byte `0x10e89a` and callback-registration state byte `0x11fcdd` is 1 or 2, it
emits the ordered sequence `0x09c9`, `0x09cd`, `0x151c`. In coherent registration runs
`0x11fcdd` is zero after task-5 registration processing and none of `0x24d8e8`, `0x24df74`, or
`0x24e754` executes; a runtime control observing slot state `0x0b` still fails the required
state 1 or 2 (**R**).

`0x11fcdd` is not a pending selector; it is callback-state array slot `0x5d` at
`0x11fc80 + 0x5d`. Catalogue registration at `0x2aefba` selects descriptors from `0x2cc7f0`;
eligibility helper `0x2aeda0` checks the old state and `0x2aefa2` commits the descriptor's
six-bit new state. The descriptors recovered for callback `0x5d` contain transitions from
states `1`, `2`, `3`, `4`, or `0x0b`; the context producer requires state 1 or 2, which the
runtime never reaches. Callback 7 is therefore a configuration or later-session cycle, not an
ordinary-registration prerequisite (**S/R**).

One concrete context-initialization cycle connects the external callers. A task-14 `0x09d8`
object with opcode byte `0x3a` selects decoder branch `0x267414` (field type `0x1c`, lower
result `0x0fc8` at `0x26746e`); the lower-result dispatcher reaches `0x252aac`, parser
`0x2528cc` classifies its type-2 object, and `0x2b5cf8` publishes task-5 event `0x0ac8`
command `0x16`, after which the task-5/task-20 handshake can call `0x24d8e8`, emit `0x09cd`,
and construct callback 7. This route is **not** the first bootstrap producer: the only proved
`0x09d8` constructor is task-15 translator `0x208ee0`, reached after the absent
`0x05e8 -> 0x05ea -> 0x07dd` object has parsed, so the opcode-`0x3a` route is a valid later
session cycle but circular for the initial `0x05e8` transition. Status `0x09de` is another
translator output: input `0x0a0c` reaches `0x209116`, where task-15 mode `0x1f` selects
`0x09de` (modes `0x20`/`0x21` select `0x09eb`/`0x09ca`). No immediate, PC-relative ROM
literal, or statically recovered call argument supplies `0x0a0c` to `0x208ee0`; a dynamic
caller remains possible, but `0x09de` establishes neither an independent peer ingress nor an
ordinary-bootstrap producer (**S**).

## Frontier: quantified non-SAT constructor absence

The census-backed absence proof is the principal current result: this ROM contains no
unclassified MCU-side packed-event constructor that can create the missing non-SAT `0x05dc`
session lifecycle.

- The generated census inventories every direct packed `0x05e0` constructor with at least two
  argument words and requires a reviewed assessment per callsite. Intersecting that closed set
  with callbacks containing direct `0x05e8` publishers leaves owners `0x21`, `0x22`, `0x26`,
  `0x3c`, and `0x54`, plus the mixed dynamic `0x35..0x3a` classifier. The recovered
  constructors for `0x22`, `0x3c`, and `0x54` carry constant or null object words; the proved
  `0x37`/`0x38` dynamic modes are SAT-derived (**S**).
- Backward tracing excludes `0x21` and `0x26` from the coherent bootstrap. Callback `0x24`
  (`0x28b844`) builds a callback-`0x21` object through `0x228aa0` only in generic-framework
  mode 9 after input `0x013d`, and is the only recovered direct producer of global `0x0388`
  (`0x28b926`, `0x28b960`, `0x28b9aa`; mode 11, inputs `0x013c`/`0x013a`/`0x0136`), which
  active callback `0x26` maps to `0x05e8`. The other `0x21` constructor is callback-`0x54`
  input `0x1a34`; the five pointer-bearing `0x26` constructors belong to callbacks `0x1f`,
  `0x1e`, `0x51`, `0x29`, and `0x79`. In the coherent eight-second run callback `0x24`
  receives only sweep `0x05e2` with framework mode 0, and `0x21`/`0x26` receive the same
  sweep with no lifecycle object; their direct `0x05e8` publishers are status-specific
  fallback/cleanup branches (`0x0580` and `0x0388`), not a registration-success path
  (**S/R**).
- The scalar intersection is downstream too. Callback `0x21` constructs callback `0x22` at
  `0x228a66`; callbacks `0x51`/`0x25` construct `0x3c` at `0x25d25c`/`0x25d6d0`; callbacks
  `0x55`/`0x42` construct `0x54` at `0x24a8f4`/`0x2a8868`. Their direct `0x05e8` sites
  converge later failure/cleanup lifecycles. Every direct object-bearing constructor whose
  selected callback can itself publish `0x05e8` is SAT-derived, mode-gated later-cycle, or
  downstream failure convergence (**S**).
- Runtime-built events are bounded. The census requires a reviewed bound for all 22 calls to
  `0x2af798` whose packed event is not recovered as a static value. Only the five calls in
  parameterized helper `0x26b58c` carry two argument words, and its only ROM callers bound
  their completion events to `0x0594` or `0x0c2d`; the other dynamic calls are argumentless or
  one-word events. None can construct object-bearing lifecycle event `0x05e0` (**S**).

The dynamic controller classifier is SAT-owned in this boot vocabulary. Its mode byte is
`0x110a64`; helper `0x269524` stores the mode and object before calling `0x267e68`. Concrete
execution of the unmodified dispatcher `0x253e20` over every 16-bit input recovers the
complete selector map:

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

The eight classifier inputs lie within `0x1777..0x1782`. Router case `0x1770` itself carries
the `0x1196` registration commit at `0x254bb4` and is outside the SAT classifier. Callbacks
`0x3a` and `0x36` contain organic `0x0578` publishers, but their only dispatcher predecessors
are SAT events `0x1782` and `0x1779`; they are not ordinary-registration activation
candidates. This closes the final dynamic classifier exception in the non-SAT constructor
proof (**S**).

### Offline initialization versus network registration

Offline SIM initialization is complete and does not construct callback 7 (state slot `0x5d`):
the coherent boot raises SIM enable, starts the presence monitor, and enters interactive mode
without any `0x05dc` constructor. A network-registration session is the later lifecycle that
would construct callback 7 organically, and it must not be forced. Statements that callback 7
"blocks registration" are wrong without this offline/network qualifier; `sim_subsystem.md` and
`sim_emulator_scope.md` reference this distinction.

Completion requires one coherent boot with no injected task messages, forced callback events,
forced return values, or writes to SIM/registration RAM.
