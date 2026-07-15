# The MAD2 MCU↔DSP interface (DCT3 / Nokia 3210)

A map of how the ARM7 MCU talks to the on-chip **DSP** (which runs the GSM Layer-1
baseband + audio codec) in the Nokia 3210 firmware. It combines static
disassembly with the reviewed shared-ring and service-boundary traces. Use the
current focused `NOKI3210_TRACE_DSP_BOUNDARY`; the former broad DSP-I/O probe is
removed.

**TL;DR for emulation:** at boot the MCU treats the DSP interface as (a) a RAM
self-test it passes by echo, (b) a handful of "DSP ready" status flags, (c) a
download target for coefficient/program blobs, and (d) bidirectional lower-service
message rings. The request-driven contact peer now answers the organic D0 discovery
and type-`0x70` contact request through FIQ 0, proving that MCU-to-DSP requests and
DSP-to-MCU replies compose in the normal scheduler. The later lower-radio
command/reply vocabulary remains incomplete, but it is no longer the immediate
boot frontier: ordinary SIM initialization now runs after contact startup.

## The two hardware windows

| MMIO | region | driver | boot usage |
|---|---|---|---|
| `0x10000–0x10fff` | **DSP shared RAM** (0x800 halfwords) | `nokia_dsp_peer_device::shared_r/w` through `dsp_ram_r/w` | partial HLE — backing store, packet rings, service timing and request-derived contact replies |
| `0x30000–0x30003` | **DSPIF** control register | `mad2_dspif_r/w` (stub: reads 0, writes no-op) | early initialization plus reachable command-4 doorbells from `0x290cf4`; wider L1 use remains unmapped at runtime |
| `0x40000` | MCUIF (memory-range config) | `mad2_mcuif_r/w` (stub) | early config |

The atlas counted ~444 references to the shared-RAM base and 42 pool-literal references
to DSPIF across the image. Earlier boot-only traces saw just the initialization writes,
but a coherent stateful-SIM run reaches `0x290cf4` with service commands `0x30` and
`0x32`. That function updates DSP shared control words, writes command 4 to DSPIF at
`0x29103c`, and rings doorbell byte 2 at `0x20008`. DSPIF is therefore a live boundary,
not a static-only future path. Those service commands remain useful lower-radio
evidence; the distinct boot-critical contact completion is now mapped as type
`0x70` TX / type `0x74` RX.

## Shared-RAM layout at boot (`0x10000` base; offsets are byte offsets)

| offset | what | evidence |
|---|---|---|
| `[0x00–0x24]` | **self-test / RAM echo region** | written with a walking pattern at `0x295f48`, read back + compared at `0x295fc0`/`0x295fd6`. A RAM test of the shared window; passes trivially against a real backing store. |
| `[0x00–0x04]` | bootstrap-ready reads → `0x01` | explicit peer-device HLE; the firmware reads these as DSP-alive status. |
| `[0xe0]` → `0x00`, `[0xfe]`/`[0x100]` → `0x01` | **DSP ready/busy flags** | `[0xe0]` is still a read override: firmware sets the backing word before DSPIF command 4, but the peer-clear timing is unresolved. `0xe4` is the lower-service pending counter. |
| `[0xf6–0x102]` | **config words** (`0x0100 0x0300 0x0001 0x0000 0x0001 0x0001 0x0200`) | 7 individual writes at `0x290a44–0x290a64`. |
| `[0x200–0x600]` | **coefficient/parameter table** (512 halfwords) | strided copy at `0x290a94`: reads one halfword per 0x20-byte record from flash `0x200040`, packs into `[0x200+]`. |
| `[0xe00+]` | **second blob** (~240+ halfwords) | ARM block-copy (`stmia`) at `0x2b5bd0` (reached via `bx pc` ARM-mode switch). |

So the MCU **stages DSP tables/program from flash into shared RAM** at boot — the DSP's
data *is* in the image. On a real phone the DSP would execute or consume it;
the current HLE peer stores the blobs but does not interpret them.

## The L1↔DSP mailbox protocol (2026-07 dig — both directions mapped)

Task **22** (`dsp_if_task_2b6548`) is the DSP-interface RTOS task. Its loop: `recv`
(`0x26a458`) → if the code < `0xc0` call the dispatch `0x23d62c` (`0x2b6564`); codes
`0xc0–0x1bf` are delayed-message markers, freed and ignored.

**Recv side (DSP→MCU downlink) — dispatch `0x23d62c`:**
- copies message header fields into a state struct at `[0x11251c]` (`[msg+2]→+0`,
  `[msg+7]→+1`, `[msg+0]→+3`),
- **dispatches on `[msg+3]` = message class**, a subtract-cascade over
  `{2, 3, 5, 6, 7, 8, 0xa, 0x11, 0x13, 0x14, 0x47, 0x64, 0xd5}` → handlers
  `0x23c4fc / 0x23c55c / 0x23c9e8 / 0x23cbe8 / 0x23cde0 / 0x23d158 / 0x23d2fe / 0x23d430`
  (payload pointer = `msg+9`). Class `0xd5` is a special inline (wheel-only: sets state
  `{0xc,1,0}`, forwards id `0x3957`).
- **Two-level protocol**: each handler sub-dispatches on `[msg+9]` = primitive (e.g.
  `0x23cde0` branches on `0x10/0x13/0x16/0x18/0x1a/0x25/…`). So a message is
  `{class:[+3], primitive:[+9], payload:[+10…]}`.
- **Routing to the upper L1 layers uses the resource-message bus**: the "forward" helper
  `0x2b5f88(id, msg)` is actually `resource_available(id)` + `resource_acquire(id, msg)`
  (the same `0x2b12b4`/`0x2b12dc` pair as the display) — i.e. L1 messages are delivered to
  whichever task registered for message-id `id` (`0x82xx`/`0x03xx`). A routing table at
  `0x2b66b0` (0xc-byte records) maps DSP primitives `0x10–0x29` ↔ upper-layer message ids
  `0x035c–0x037b`.

**Send side (MCU→DSP uplink):** write a command halfword to the **DSPIF register
`0x30000`**, then poke the doorbell interrupt at `0x20008` (pattern seen at `0x291038`:
`strh #4→[0x30000]; strb #2→[0x20008]`). The DSPIF has **287 write sites, almost all in the
L1 driver `0x2b7xxx–0x2c9xxx`** — the per-command send stubs.

**Runtime status.** The task-22 downlink protocol remains dormant: on the measured boot the
dispatch `0x23d62c` is reached **0 times**: task 22's handler is entered only ~twice
(t≈0.37, 3.76) for the low-level echo/handshake and stays `recv`-blocked — no DSP→MCU L1
message ever arrives. However, the separate `0x290cf4` shared-control service path now runs
and issues command-4 doorbells. The mailbox plumbing (task 22,
IRQ4 lower-service, dispatch, routing table) is fully present and now mapped, but **no
GSM-L1 traffic flows** because nothing runs the L1 stack: there is no DSP executing the air
interface to emit measurement/sync/registration primitives, and the MCU-side L1 senders sit
behind the same coherent-boot/network-attach phase we never reach.

Task 22's housekeeping use keeps it alive. Whether the `0x30`/`0x32`
completion returns through task 22 or only through shared control
state is unresolved. Use `NOKI3210_TRACE_DSP_BOUNDARY` for this boundary; the retired broad
`TRACE_DSPMSG` history should not be restored without a specific mailbox hypothesis.

## What the reachable boot currently proves

Reviewed traces show no MCU reads of additional DSP-computed result words beyond
the bootstrap/self-test region. The MCU nevertheless queues lower-service
packets in the shared transmit ring. The first captured pair is:

```text
00 02 0a 05 1e ff 00 d0 00 03 01 01 e0 00
08 05 1e 14 00 f4 00 01 03 00
```

The service submodel clears the pending count and raises IRQ4. The separate
request-driven contact submodel consumes complete TX packets and returns
correlated contact and transport responses through the inbound ring and FIQ0.
Neither is a complete DSP implementation. No derived lower-radio response has connected this traffic
to `0x05ea`, task-15 `0x07dd`, or the SIM registration result, so treating the D0 packet
as that request would be speculation.

The two acknowledgement actions are jointly minimal. In matched one-second runs,
counter drain without IRQ and IRQ without counter drain both leave
`service_ready=0`, take the same soft reset, and finish at startup event `0x32`.
Together they set `service_ready=1`, prevent the reset, and reach event `0xc3`.
The temporary isolation switches used for that experiment were removed; the result
narrows `MODEL_DSP_SERVICE` to one coherent hardware transaction rather than two
independent conveniences.

### Reachable shared-control commands

The stateful-SIM path calls `0x290cf4` with command `0x30`, then command `0x32`.
These are bitfield setters, not opaque DSP primitive IDs: their jump-table cases update
the MCU shadow halfword at `0x110c3a`, and the common tail copies the result to DSP shared
offset `0xa8`. The observed `0xfff5` argument produces the successive values `0x700c` and
`0x7004`. Only the second call has payload flag 1; it sets DSP-owned busy word `[0xe0]=1`,
writes command 4 to DSPIF, and rings doorbell byte 2 at `0x20008`.

IRQ 4 enters `0x291068`. That routine handles the shared service counts at `[0xda]`,
`[0xe2]`, and `[0xe4]`; it does not parse a command-`0x30`/`0x32` reply payload. A trial which
exposed `[0xe0]=1` until the existing 5 ms service tick regressed the deep boot, so that delay
is disproved. The historical idle read override remains until the actual doorbell-completion
timing is recovered; do not tune a delay merely to reproduce the oracle.

### Shared packet rings

The lower-service packet queue is independent of the startup/table-transfer words at
`0xda..0xe4`. Static recovery plus ownership tracing gives its exact layout:

| DSP RAM offsets | owner | role |
|---|---|---|
| `0x000..0x0a2` | MCU writes, DSP reads | MCU-to-DSP circular packet ring |
| `0x0a4` | MCU | TX producer index, in halfwords (`0..0x51`) |
| `0x0a6` | DSP | TX consumer index, in halfwords |
| `0x100..0x1c6` | DSP writes, MCU reads | DSP-to-MCU circular packet ring |
| `0x1c8` | DSP | RX producer index, in halfwords (`0x80..0xe3`) |
| `0x1ca` | MCU | RX consumer index, in halfwords |

`0x29099a` computes TX free space from `0x0a4`/`0x0a6` with one slot reserved.
`0x2907c4` appends a header plus packed big-endian bytes and commits the producer.
For a header halfword `LLTT`, `LL` is the payload byte count, `TT` is packet type,
and total ring occupancy is `(LL + 3) / 2` halfwords. On the inbound side,
`0x290904` copies from the RX consumer, wraps at `0x1c8` to `0x100`, and commits
the new consumer at `0x1ca`.

Inbound header parsing at `0x29088e` allocates a firmware message of `LL + 5`
bytes and constructs `{0x18, 0x02, LL, TT, payload...}`. The surrounding receive
task dispatches `TT` only through the `0x70`, `0x80`, and selected `0x83..0x99`
families (`0x29bc00`, `0x284ac4`, `0x28464c`, and `0x283fe6`). Other types take
the invalid-message/free path. Consequently a symmetrical type-`0x05` reply to
the outbound D0 packet is not a valid inbound generic-service object and cannot
be assumed to produce service-5 `0x05ea` or task-15 `0x07dd`.

The complete first-level type switch at task-4 loop `0x2b3fb8` is now decoded.
The jump table at `0x2b3ffc` covers `0x83..0x8f`; types outside it pass through
the explicit tests below. "Post" means that the handler rewrites the broker
object's leading status and delivers it through the ordinary task mailbox.

| RX type | handler | first-level result |
| --- | --- | --- |
| `0x70..0x7f` | `0x29bc00` | preserve class and post to task 2 |
| `0x80` | `0x284ac4` | structured multi-command decoder |
| `0x83` | `0x284734` | structured state/report decoder |
| `0x84` | `0x284e26` | post `0x1394` to task 10 |
| `0x85` | `0x283fe6` | post broker object to task 8 |
| `0x86` | `0x284316` | structured controller decoder |
| `0x87` | `0x284ebc` | post `0x138f` to task 10 |
| `0x88` | `0x284ee8` | construct `0x13ac`, post to task 11 |
| `0x89` | `0x284f74` | post `0x1393` to task 10 after state handling |
| `0x8a` | `0x284e88` | post `0x1390` to task 10 |
| `0x8b` | `0x284fd8` | post `0x13b8` to task 11 |
| `0x8c` | `0x284f48` | consume/free; may publish separate static task-10 work from controller state |
| `0x8d` | `0x283fe6` | post broker object to task 8 |
| `0x8e` | fallback | generic invalid/diagnostic path, then free |
| `0x8f` | `0x284e50` | post `0x13b7` to task 11 |
| `0x99` | `0x28464c` | structured measurement/report decoder |
| other | `0x2b3730` | generic invalid/diagnostic path, then free |

No simple fixed-status handler produces the required `0x1391`. Thus the organic
`0x1391 -> 0x0434` completion must be derived by one of the structured decoders
or by a later controller transition, rather than encoded as a bare first-level
RX type. The previous type-`0x80` experiment disproved only the fabricated
primitive-`0x70` payload used in that run. It did **not** disprove the type-`0x80`
family, whose handler `0x284ac4` contains a broad nested command decoder.

Service 5 itself is not missing: callback `0x2618e8` is selected organically by
the generic callback dispatcher and receives the normal `0x05f3`/`0x05e2` sweep.
The framework is downstream of task 5 (`0x2af652 -> 0x2638e4`), so it is not the
	hardware ingress. The object-ingress question is now closed against the DSP
	interface: task-21 status `0x120c` crosses task 20 into GSM 11.14 FETCH
	(`A0/12`), whose proactive-command D0 response reaches the `0x177x` router.
	None of the validated DSP RX families enters that SIM Toolkit chain. This
	excludes DSP ingress for SAT only; it does not exclude a separate DSP-owned
	ordinary-registration path.

For types `0x70..0x7f`, `0x29bc00` preserves the type as the firmware message class
and posts the message to task 2. Class `0x70` takes task 2's unknown-response
fallback, but class `0x74` is explicitly dispatched to `0x234954`. This is the
contact/self-test result route: the organic outbound type-`0x70` request
`0d 00` is completed by inbound type `0x74` payload `0d 00`, which clears the
contact busy flag through firmware at `0x2349dc`. Numeric group labels from
another DCT3 firmware still need reconciliation at both decoder layers before
being generalized. The direct DSP translator `0x282d64`
handles classes 3/5/17/47, but its
class-5 primitive set starts at `0x11` and does not include the task-15
registration primitive `0x0b`. These are distinct protocols despite sharing a
numeric class value.

The original DSP model left TX consumer `0x0a6` at zero. The producer reached
`0x34`, free space collapsed, and later firmware packets could not be queued.
An isolated ring-drain experiment proved that complete packets must be consumed,
but that standalone model has been removed. The current request-driven contact
peer owns consumption and any correlated response at the same boundary. The
newly visible coherent stream is:

```text
type 05: 0a05 1eff 00d0 0003 0101 e000
type 05: 0805 1e14 00f4 0001 0300
type 51: seven configuration/coefficient packets (six 80-byte, one 28-byte)
type 70: three control packets (6, 14, and 22 payload bytes)
```

No later type-`0x05` transaction appears, and consumption alone produces none of
`0x0588`, `0x05ea`, or `0x07dd`. This makes the ring drain a fidelity correction,
not evidence for a D0 response or a SIM-registration fix. It remains separate from
the established deep profile because enabling it preserves the final LCD hash but
changes the structural oracle's repeated-work counters.

### Organic radio-init queue frontier

The radio-initialization chain reaches this ring without an isolated producer probe.
Task 17 sends `0x09ec`; task 15 case 6 sends `0x07d6`; task 16 sends `0x03e9` to
task 10. Task 10 immediately acknowledges task 17 with `0x043c`, then constructs
a 72-byte task-3 object headed `{00 02 44 1a 00 81 98 ...}`. Finalizer
`0x219e30` is an organic producer of `0x0434`, but no direct task-3 completion
edge into it is proved.

The `0x07d6` leg is intentional for this transaction. The organic task-17
message is `09 ec 00 ...`; task 15 copies byte `+2` to protocol mode
`0x10fe49`, then selects `0x07d6` for mode zero. Its adjacent direct `0x09ec`
path requires mode `0x10` and does not describe this boot transaction.

Task 3 does not immediately serialize that object. Its FIFO first releases the
type-`0x51` coefficient/configuration stream and five type-`0x70` control packets.
The old 5 ms peer cadence can phase-lock against task 3's immediate free-space
recheck and strand the FIFO after sequence `0x22a2`. A 4 ms cadence avoids that
model artifact: all earlier packets drain and the organic request reaches the
DSP boundary as a 35-word packet:

```text
type 0x1a, payload 68: 441a 0081 9800 0000 ...
```

No firmware message, callback, or RAM state is injected to produce it. The
driver's opt-in DSP service defaults now use the empirically stable 4 ms cadence;
the eventual DSP peer should own TX consumption independently of the service-IRQ
timer so correctness does not depend on this scheduling phase.

Static control flow does not establish a request/reply contract for this packet.
The only call to builder `0x219f0c` is task-10 state dispatcher `0x21ba54`.
After posting the packet, the builder retains no transaction token, reply object,
or task-3 completion callback. It schedules the global DSP-service timer, clears
the DSP-activity counter, and returns. Task 10 has already sent task 17 the
immediate `0x043c` acknowledgement. Type `0x1a` is therefore best classified as
a fire-and-forget DSP state/control publication; it is not evidence that an
inbound packet should directly complete task 17.

The type-`0x80` structured decoder contains a superficially similar test at
`0x284c74`, but it compares radio-controller state byte `0x10dbd2` with `0x1a`,
not an outstanding packet type. At the organic type-`0x1a` send that byte is
`0x00`. If the later state is `0x1a`, inner command `0x60` can call `0x2849ac`
and eventually post `0x1395`; it does not produce completion status `0x1391`.
The shared number is not a request/reply correlation.

The adjacent `0x2697aa(0x23, 0x0a0a)` call schedules a global DSP-service timer,
not a task-10 timeout event. In a coherent 12-second run timer `0x23` expired at
roughly 34 ms cadence, woke task 4, and was rearmed; task 10 received no resulting
status and `0x219e30` did not run. This proves the timer wheel is live while
leaving the semantic completion unresolved.

An RX-ring transport probe also pins the return notification: advancing RX
producer `0x1c8` and asserting **FIQ 0** wakes task 4 with receive sentinel `4`,
which calls `0x290904` and dispatches the packet naturally. IRQ lines and the
other FIQ lines do not. A candidate type-`0x80`, primitive-`0x70` response was
decoded and forwarded to task 13 as `0x040b`, but its subscriber/parser rejected
it; delaying it by 3-306 ms did not change that result. It is therefore not a
proven registration completion, and the probe was removed.

The same boundary establishes the DSP-liveness contract independently of that
semantic reply. A header-only ring entry `0x0003` is an outer RX type-`0x03`
packet. Task 4 increments activity counter `0x112502` at `0x2b3fca` for every
non-sentinel packet before type dispatch; type `0x03` then reaches the generic
discard/diagnostic path and is freed. The counter is cleared at `0x21596e`,
`0x21a1ec`, and `0x2a0f9a`, matching a repeatedly rearmed liveness check.

An opt-in empty-ring-gated model wrote `0x0003`, advanced DSP-owned producer
`0x1c8` with the recovered wrap rule, and asserted FIQ0. The firmware consumed
230 such entries and updated `0x112502`, proving the transport model, but emitted
none of `0x05e8`, `0x05ea`, `0x07dd`, `0x09d8`, or `0x0434`. Repetition also
reduced coherent LCD/EEPROM work counts. The experiment and model were removed:
periodic liveness traffic is real DSP behavior, but it is not the missing
registration-semantic response.

The wider type-`0x80`/type-`0x83` `0x040b` route is now classified separately.
Task 13's receive loop at `0x23e62c` and command handler `0x23e7ac` parse a
segmented transfer at `0x23e324`/`0x23e378`. A valid completed transfer emits
`0x05eb` to task 16 through `0x23e1a4`. Task-16
callback `0x3c` takes a dedicated `0x05eb` branch at `0x25df18`, publishes
`0x057a`, and returns `0x05e6`. That branch bypasses the callback's argumentless
`0x05e8` fallback at `0x25df08`. Therefore a legitimate `0x040b` transfer is not
the missing ordinary-registration `0x05dc` predecessor. The transfer's owning
subsystem is not yet established. In particular, the UI window stack is task 6
around `0x297fc4` and RAM `0x1116f8`/`0x111724`; a Ghidra function name is not
evidence that task 13 is the display-window subsystem.

The task-10 completion route is now pinned more tightly, and corrects an earlier
false lead. Task 17's initializer enters its long-lived event loop at
`0x223964 -> 0x2271c6`; code at `0x2222fc` and its `0x138f` callbacks is reached
only after that loop returns. It is therefore downstream of the awaited
`0x0434`, not its missing predecessor. DSP packet type `0x89` can also produce
`0x138f`, but only while task 4's initialization flag `[0x112501]` is zero. A
real FIQ-0 type-`0x89` probe at the current frontier was rejected after that flag
became one, so the probe was removed.

The relevant task-10 completion is status `0x1391`. The dispatcher jump table
maps it to `0x21b9b4 -> 0x21b198`; when the firmware-owned work state is ready
this reaches `0x219e30`, the producer of `0x0434`. Status `0x1392` is the
adjacent table entry and instead maps to the radio-state/configuration path at
`0x21b790`. Callback `0x2b60f6` unregisters lower-radio key `0x4c00` with
`0x2b257e` and posts that non-completing `0x1392` update through `0x2af6ea`.

The large lower-radio result dispatcher `0x245a84` maps result `0x0fc1` through
`0x245c8c -> 0x2525be -> 0x2b610a` to completion status `0x1391`. Result
`0x0fc2` maps through `0x245c76 -> 0x2525a8 -> 0x2b60f6` to the separate
radio-state status `0x1392`. Both cases copy firmware-owned result fields into
callback objects before invoking their wrappers; neither is a bare status
notification.

The controller's event-`0x102f` branch instead loads result `0x0fbf` at
`0x24788e` and enters the dispatcher call at `0x246ad6` with that value intact.
Its table case is `0x245cb2 -> 0x253610`, a context handler. The earlier
`0x0fc3` value and association of `0x0fbf` with `0x245c76` were separate
jump-table indexing errors. Event
`0x102f` is returned by object decoder `0x267258` for opcode byte `0x2a`; that
decoder is reached from task 14 for statuses `0x09d8`/`0x09de`. Both statuses are
task-15 translator outputs rather than raw peer messages. `0x09de` is selected by
translator input `0x0a0c` and task-15 mode `0x1f`; no static caller supplying
`0x0a0c` has been recovered. Neither status currently establishes an independent
peer transport into the decoder.

The producer side is pinned one layer further back. Task 15 constructs `0x09d8`
through translator `0x208ee0` after an object-bearing `0x07dd` has passed parser
`0x209978`, returned internal success `0x09f3`, and selected one of the two `0x0a08`
state-machine branches (`0x20be0c`/`0x20f324`). Neither branch executes in the
coherent run. Consequently a DSP peer must not inject task-14 `0x09d8`; the open
hardware boundary precedes the generic-service object delivered to task 15.

For the completion variant, task-14 object opcode `0x36` reaches decoder branch
`0x2674da`, parses through `0x266ffc`, and emits controller event `0x1033`.
The controller branch at `0x2476fa` maps it to lower result `0x0fc1`, completing
the already-mapped `0x1391 -> 0x0434` route. Adjacent opcode `0x37` emits event
`0x1034` and result `0x0fc2`, which is the non-completing `0x1392` radio update.

The wider **bidirectional L1 protocol** — MCU sends "search/sync/measure/attach", DSP returns
cell/RSSI/registration — lives largely in the `0x2b7xxx–0x2c9xxx` driver and is not yet
observed carrying task-22 messages. The narrower shared-control path at `0x290cf4` is live,
however, so DSP work is no longer wholly static: its `0x30`/`0x32` request and IRQ-4 completion
contract can be traced at the current boot frontier.

## Emulation feasibility & the dependency re-ordering

- **To reach where we are:** the current echo, ready flags, queue drain, and IRQ model are
  sufficient, but they are not a completed DSP contract.
- **The lower-radio session start is the proven bounded frontier.** Organic initialization
  creates task 14 and its eight controller slots, but no task-14 input starts a resource-`0x35`
  operation. The concurrent type-`0x1a` DSP state block is not evidence of such an operation;
  the falsified type-`0x80`/`0x70` reply must not be restored.
- **For the network (operator name + signal):** extend the message-boundary DSP peer (answer the L1
  commands with "camped on a fake cell, operator X, RSSI y") is feasible *in principle* and
  is the right MAME approach — but it is **doubly blocked**: (1) the L1 protocol is
  static-only until coherent boot reaches it, so we can't observe the exact handshake to
  stub it; (2) it is behind the same MMI/coherent-boot wall. Signal RSSI itself is separately
  injectable (CCONT ADC ch1, `network_scouting.md`).
- **Full DSP-core emulation** (a TI Lead core running the downloaded blobs) is a much larger
  project and would still need a faked air interface; not warranted given the above ordering.

An eight-second coherent deep run with DSP-ring draining established that the organic
type-`0x1a` packet is **not** the resource-`0x35` transaction. Its complete contents are
`44 1a 00 81 98 00` followed by zeroes (35 ring words total), consistent with a fixed-size
DSP state/control block. The lower-radio controller initializes at about 0.814 s, but task
14 receives no message and neither `0x282238` (resource-`0x35` transmit) nor `0x267258`
(object receive) runs.

**Net:** task-5 status `0x13e2` is downstream, not the missing radio transition. Its natural
producer is `0x2b3f60`, called by task 17 at `0x225b8c` after the phase handler accepts
`0x0434`/`0x0a22` (and at `0x223a28` after the phase loop returns). It publishes packed
`0x53e2` with one firmware-owned pointer; the consumer path
`0x255124 -> 0x28a4a8 -> 0x238a24` then constructs `0x1776` for decimal task 14
(ID `0x0e`). The runtime never reaches it because `0x0434` is still absent. The leading
frontier therefore remains the missing organic producer of lower result
`0x0fc1`, which selects completion status `0x1391`. The `0x0fbf` context path
and `0x0fc2 -> 0x1392` radio-state path are separate and do not reach `0x0434`;
only after the `0x0fc1` contract is pinned should the proven bidirectional
contract move into a DSP peer device.

## Per-primitive payload semantics (2026-07 — handler `0x23cde0`, classes 5/7/0xa)

Decoded the primitive dispatch of `0x23cde0` (`[msg+9]` = primitive; `r5` = `msg+9`). **The
key finding: most primitives forward straight into the `0x2a2xxx` idle-element render
library — the DSP L1 layer *is* the idle content producer.** Each `0x2a2xxx` target opens
with a call to the status classifier `0x28fa4c` (the same one that reads the camped/service
state `[0x11fce1]`, from the camped-state dig) and then render-posts to task 5. So the full
producer chain is:

```
DSP L1 status primitive → 0x23cde0 → 0x2a2xxx render fn → 0x28fa4c (camped-state gate)
    → render-post 0x2af6ea → task 5 → idle content (signal bars / indicators / operator)
```

Primitive table (payload = byte fields at fixed offsets, big-endian 16-bit lengths, blobs):

| prim | payload | target | role (inferred) |
|------|---------|--------|-----------------|
| `0x10` | `{b[2],b[3],b[4]}` | `0x2a23f2` | 3-field display indicator update |
| `0x13` | `{b[2],b[3]}` | `0x2a24ae` (posts render id `0xd394`) | 2-field indicator update |
| `0x16` | — | `0x2a237a` | indicator refresh/trigger |
| `0x18` | `{b[1],…}` → 0x18-byte struct | (0x23ad40 err if b[1]==0) | structured element |
| `0x1a` | `{b[1]}` | `0x2a2518` | 1-field element |
| `0x1c` | `{b[1]!=0, b[2]==3, …}` parsed | `0x2a24f2` | structured element |
| `0x22` | `{b[1]}` validate 1..3 | err `0x23cd96` | validation |
| `0x25` | **null-terminated string** at `msg+5` (`strlen 0x2b6680`) | `0x23c664` parse | **text element (operator name?)** |
| `0x30` | `{4 bytes, BE16 len, blob ≤0xaa}` | `0x2b2ec8` | data block |
| `0x33` | `{b[1]}` | `0x2b2ed4` | short command |
| `0x36` | `{type, count≤0xc, items, BE16 len, blob ≤0x12c}` | `0x2b2ebc` | list/data block |
| `0x70` | — | `0x2b3ea2` | notification |

So the primitives split into **display-update** (`0x10–0x1c`, `0x25` → `0x2a2xxx` render) and
**L1 data-block** (`0x30/0x33/0x36` → `0x2b2exx`, carrying big-endian length-prefixed blobs
up to 300 bytes — measurement/frame data). Big-endian multi-byte fields (`b[n]<<8|b[n+1]`)
confirm GSM network-byte-order framing. This mechanizes *why* forcing the camped-state byte
did nothing (previous dig): the content is produced by these DSP primitives arriving and
being rendered — not by the state byte — and **no primitive ever arrives** (`TRACE_DSPMSG`:
`0x23d62c` runs 0 times).

⚠️ **swap16 correction to the camped-state dig:** the network-registration data it read as
`[0x1028d1]`/`[0x107ed3]` is really the struct at **`0x10d124`** (literal `d1280010` →
`0x0010d128`) / **`0x10d37e`** — a network/registration struct (field `+2` a status halfword,
message chunks memcpy'd to `+0x34`), accessed from `0x26f1b8`/`0x208110`/`0x26f608`. The
camped-dig *conclusion* (forcing `[0x11fce1]` changes nothing) stands; only the auxiliary
net-data address was mis-swapped.

### The `0x2b2exx` "data-block processors" are thin routers (2026-07)

Followed the data-block primitives (`0x30/0x33/0x36`) into their `0x2b2exx` targets. They do
**no processing** — each is a 3-instruction thunk that render-posts the parsed descriptor to
**task 5 (the MMI VM)** with a unique type id:

| target | render id | from primitive |
|--------|-----------|----------------|
| `0x2b2ebc` | `0x7a59` | `0x36` (list/blob ≤300B) |
| `0x2b2ec8` | `0x7859` | `0x30` (blob ≤170B) |
| `0x2b2ed4` | `0x7959` | `0x33` (short) |
| `0x2b2ee0` | `0x8c99` | — |
| `0x2b2eee` | `0x8e19` | — |
| `0x2b2ef8` | `0x8d59` | — |

`render-post 0x2af6ea(id, descriptor)` (big-endian: `msg[0]=id`, params follow) allocates a
0x10-byte message and posts it to **mailbox 5 = task 5** via `0x26a354`. So the descriptor
becomes task-5 event `id & 0x1fff` (`0x1859/0x1959/0x1a59/…`) with the descriptor pointer as
param 0.

Two consequences: (1) combined with the display-update primitives (which also render-post to
task 5, via the `0x2a2xxx` functions), the **entire** class-5/7/0xa handler `0x23cde0` is a
uniform *parse-and-forward-to-MMI* layer — there is no separate "L1 data processing", the MMI
VM (task 5) is the universal consumer. (2) These data-block events are **not** in the MMI-VM
rewrite table (`0x2cb218`, max key `0x1b5d`) and have **no other producers** in the image, so
they fall through to the VM's general action pipeline. Their blobs' exact semantics (what the
≤300-byte payloads *are* — cell-broadcast text? measurement lists?) cannot be pinned
statically without a spec or a live trace; the code never runs on our boot.

**This thread bottoms out here:** the data path is thin routing into a dormant MMI-VM event,
not a decodable processor. Further decode would be guessing at never-executed code.

## Open items (future deep-dives)
- Decode the other recv handlers' primitives (`0x23c4fc/55c/9e8/be8/d158/d2fe/d430`) and the
  full `0x2b66b0` routing table (same caveat: dormant, no runtime to validate).
- Static RE of the `0x2b7xxx–0x2c9xxx` L1 driver *send* side: enumerate the 287 DSPIF
  command stubs and their command encodings (large, code never runs on our boot).
- Identify the two downloaded blobs (`[0x200+]` from flash `0x200040`; `[0xe00+]`): DSP
  program vs coefficient tables vs audio codec params.
- The MCUIF (`0x40000`) memory-range configuration.
