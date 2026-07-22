# SIM initialization and session firmware map

This is the evidence-based map of Nokia 3210 SIM initialization and adjacent session machinery. It records the contracts needed
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
`mmi_layer.md` and intentionally not duplicated here.

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

## Callback 7: a separate lower-session lifecycle

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

## Separate lower-session paths

The callback-7 and generic lower-object paths above are not prerequisites for
ordinary SIM initialization or GSM Location Updating. Their recovered
constructors remain useful maps for later SAT, resource and call/session work,
but absence during boot is expected and is not a registration frontier.

The SIM Toolkit path is independently dormant for the current card profile.
The card reports EF_PHASE `0x02`; firmware therefore performs no TERMINAL
PROFILE/FETCH lifecycle and receives no proactive-command `91xx` status.
The card must not manufacture a proactive D0 object, task-21 `0x120c`, or any
task-5 callback event. A future SAT profile must begin with standards-shaped
card behavior through SIMI/FIQ6.

## Current conclusion

Offline SIM initialization and GSM network registration are both complete at
their respective boundaries:

- boot selects and reads the supported GSM 11.11 files through the physical
  SIMI/T=0 path, rejects unsupported files with `94 04`, raises SIM enable and
  enters the steady `A0 F2` presence-monitor lifecycle;
- the radio peer drives an organic Location Updating exchange through DSPIF and
  FIQ0;
- after Location Updating Accept, firmware selects `EF_LOCI` and persists the
  new location/status with two `UPDATE BINARY` operations through the same
  SIMI/T=0 path.

The authoritative radio sequence and acceptance gates live in
`network_scouting.md`. Callback 7, the `0x1196` commit family and SAT
constructors remain separate later-session contracts; they must not be used to
explain or bootstrap ordinary registration.
