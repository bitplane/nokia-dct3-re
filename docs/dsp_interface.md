# The MAD2 MCU↔DSP interface (DCT3 / Nokia 3210)

A map of how the ARM7 MCU talks to the on-chip **DSP** (which runs the GSM Layer-1
baseband + audio codec) in the Nokia 3210 firmware. It combines static
disassembly with the reviewed shared-ring and service-boundary traces. Use the
focused `NOKI3210_TRACE_DSP_BOUNDARY` for live observation at this boundary.

**TL;DR for emulation:** at boot the MCU treats the DSP interface as (a) a RAM
self-test it passes by echo, (b) a handful of "DSP ready" status flags, (c) a
download target for coefficient/program blobs, and (d) bidirectional lower-service
message rings. The aggregate device carries organic D0 discovery,
type-`0x70` DSP completion and the separate external-service session through
FIQ 0, proving that the transport composes in the normal scheduler. It does not
prove that the external class-`0x40` peer is DSP-owned. The lower-radio
command/reply vocabulary remains incomplete, but it is not the immediate
boot frontier: ordinary SIM initialization runs after service startup.

## The two hardware windows

| MMIO | region | driver | boot usage |
|---|---|---|---|
| `0x10000–0x10fff` | **DSP shared RAM** (0x800 halfwords) | `nokia_dspif_device::shared_r/w` through `dsp_ram_r/w` | extracted partial transport; DSP HLE publishes bootstrap state into the same backing RAM |
| `0x30000–0x30003` | **DSPIF** control register | retained by `nokia_dspif_device::dspif_r/w` | early initialization plus repeated command-4 doorbells from shared-control and L1 send paths |
| `0x40000–0x40003` | MCUIF (memory-range config) | retained by `mad2_mcuif_r/w` | early config value `6a 0f 61 20` |

The atlas counted ~444 references to the shared-RAM base and 42 pool-literal
references to DSPIF across the image. A coherent stateful-SIM run reaches
`0x290cf4` with service commands `0x30` and `0x32`. That function updates DSP
shared control words, writes command 4 to DSPIF at `0x29103c`, and rings
doorbell byte 2 at `0x20008`. DSPIF is therefore a live boundary, not a
static-only future path. Those service commands remain useful lower-radio
evidence; the distinct boot-critical service-session completion is mapped as
type `0x70` TX / type `0x74` RX.

The widened MAD2 access ledger records 27 DSPIF command writes in the coherent
boot: one zero-valued initialization and 26 command-4 writes, chiefly from
`0x29103c` with one from `0x290778`. The command halfword is followed by the
MAD2 doorbell write, so DSPIF is retained by the peer device. Command 4 is not
the sole scheduling edge: ring-producer and service-pending are distinct
triggers (single home: `dsp_service_transport_contract.md`; ledger
`single_timer_dspif_doorbell_replaces_shared_triggers`).

## Shared-RAM layout at boot (`0x10000` base; offsets are byte offsets)

| offset | what | evidence |
|---|---|---|
| `[0x00–0x24]` | **self-test / RAM echo region** | written with a walking pattern at `0x295f48`, read back + compared at `0x295fc0`/`0x295fd6`. A RAM test of the shared window; passes trivially against a real backing store. |
| `[0x00–0x04]` | bootstrap-ready words → `0x01` | the peer publishes all three after the observed 64-exchange download handshake; firmware reads ordinary shared RAM. |
| `[0xe0]` → `0x00`, `[0xfe]`/`[0x100]` → `0x01` | **DSP ready/busy flags** | MCU zero-writes to `0xfe`/`0x100` request peer acknowledgements. DSPIF command 4 clears peer-owned busy word `0xe0`; `0xe4` is the lower-service pending counter. |
| `[0xf6–0x102]` | **config words** (`0x0100 0x0300 0x0001 0x0000 0x0001 0x0001 0x0200`) | 7 individual writes at `0x290a44–0x290a64`. |
| `[0x200–0x600]` | **coefficient/parameter table** (512 halfwords) | strided copy at `0x290a94`: reads one halfword per 0x20-byte record from flash `0x200040`, packs into `[0x200+]`. |
| `[0xe00+]` | **second blob** (~240+ halfwords) | ARM block-copy (`stmia`) at `0x2b5bd0` (reached via `bx pc` ARM-mode switch). |

So the MCU **stages DSP tables/program from flash into shared RAM** at boot — the DSP's
data *is* in the image. On a real phone the DSP would execute or consume it;
the current HLE peer stores the blobs but does not interpret them.

## L1-to-DSP mailbox protocol

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
message ever arrives. However, the separate `0x290cf4` shared-control service path runs
and issues command-4 doorbells. The mailbox plumbing (task 22,
IRQ4 lower-service, dispatch, routing table) is fully present and mapped, but **no
GSM-L1 traffic flows** because nothing runs the L1 stack: there is no DSP executing the air
interface to emit measurement/sync/registration primitives, and the MCU-side L1 senders sit
behind the same coherent-boot/network-attach phase we never reach.

Task 22's housekeeping use keeps it alive. Whether the `0x30`/`0x32`
completion returns through task 22 or only through shared control
state is unresolved. Use `NOKI3210_TRACE_DSP_BOUNDARY` for this boundary.

## What the reachable boot currently proves

The two-ROM 20-second census in `dsp_shared_memory_inventory.md` records 272
distinct `(profile, PC, offset)` read observations covering the same 125 byte
offsets in v5.01 and v6.00. Every reachable read belongs to the RAM self-test,
bootstrap flags, shared-control words, packet-ring indices, or inbound packet
contents. No phone-side read override remains, and no unexplained computed-result
word appears in the reachable run. The MCU queues lower-service packets in the
shared transmit ring. The first captured pair is:

```text
00 02 0a 05 1e ff 00 d0 00 03 01 01 e0 00
08 05 1e 14 00 f4 00 01 03 00
```

The paired transition census narrows active peer-owned scalar publication to
nine offsets: bootstrap words `0x000/0x002/0x004/0x0fe/0x100`, TX consumer
`0x0a6`, shared busy/pending words `0x0e0/0x0e4`, and RX producer `0x1c8`.
Both ROMs use the same set and structurally matching consumers. Bootstrap word
`0x004` is published after the 64-exchange sequence but is not subsequently
read in either measured lifecycle.

The service submodel clears the pending count and raises IRQ4. The separate
request-driven external-service submodel consumes complete TX packets and returns
correlated service and transport responses through the inbound ring and FIQ0.
Neither is a complete DSP implementation. No derived lower-radio response has connected this traffic
to `0x05ea`, task-15 `0x07dd`, or the SIM registration result, so treating the D0 packet
as that request would be speculation.

Counter drain and IRQ 4 are jointly minimal: either signal alone leaves
`service_ready=0` (single home: `service_bootstrap.md`). `MODEL_DSP_SERVICE`
therefore models one coherent hardware transaction, not two independent
conveniences.

### COBBA tone control

The MCU also uses shared RAM as an audio-control surface without committing a
ring packet. Words `0x0ae` and `0x0b0` program two oscillator frequencies in
quarter-Hz units; `0x0b6` is the amplitude gate. Ordinary v5.01 and v6.00 boot
organically writes oscillator 1 as `0x0e10`, enables amplitude `0x65ac` for
about 121 ms, then clears both, proving a 900 Hz start/stop command.

With no DSP codec core, two low-fidelity MAME tone voices expose those
firmware-owned commands. They are separate from the MAD2 PUP piezo. Organic
navigation reaches the Ringing-tone selector but produces neither this
shared-word sequence nor a PUP transaction, so the missing ringtone preview
remains upstream in firmware/resource handling.

### Reachable shared-control commands

The stateful-SIM path calls `0x290cf4` with command `0x30`, then command `0x32`.
These are bitfield setters, not opaque DSP primitive IDs: their jump-table cases update
the MCU shadow halfword at `0x110c3a`, and the common tail copies the result to DSP shared
offset `0xa8`. The observed `0xfff5` argument produces the successive values `0x700c` and
`0x7004`. Only the second call has payload flag 1; it sets DSP-owned busy word `[0xe0]=1`,
writes command 4 to DSPIF, and rings doorbell byte 2 at `0x20008`.

IRQ 4 enters `0x291068`. That routine handles the shared service counts at `[0xda]`,
`[0xe2]`, and `[0xe4]`; it does not parse a command-`0x30`/`0x32` reply payload.
The adjacent `[0xdc]` word is selected alongside `[0xe0]` by the common
shared-control request helper: the MCU requires zero, writes the request, then
rings DSPIF command 4. No later read requiring its completion is reached in the
20-second census. The HLE peer acknowledges command 4 by publishing zero into
the backing word synchronously; exposing `[0xe0]=1` until the service tick is
disproved (ledger `dsp_shared_e0_completion_exposure_delay`). The physical DSP
latency remains unknown; no delay may be calibrated merely to reproduce an
oracle.

### Shared packet rings

The lower-service packet queue is independent of the startup/table-transfer
words at `0xda..0xe4`. Ring layout, index ownership, and the `LLTT` header rule
are single-homed in `dsp_service_transport_contract.md`. On the MCU side,
`0x29099a` computes TX free space with one slot reserved, `0x2907c4` appends a
header plus packed big-endian bytes and commits the producer, and `0x290904`
copies from the RX consumer and commits the new consumer index.

Inbound header parsing at `0x29088e` allocates a firmware message of `LL + 5`
bytes and constructs `{0x18, 0x02, LL, TT, payload...}`. The surrounding receive
task dispatches `TT` only through the `0x70`, `0x80`, and selected `0x83..0x99`
families (`0x29bc00`, `0x284ac4`, `0x28464c`, and `0x283fe6`). Other types take
the invalid-message/free path. Consequently a symmetrical type-`0x05` reply to
the outbound D0 packet is not a valid inbound generic-service object and cannot
be assumed to produce service-5 `0x05ea` or task-15 `0x07dd`.

The complete first-level type switch at task-4 loop `0x2b3fb8` is decoded.
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
RX type. Ledger `dsp_type80_primitive70_reply` covers only the fabricated
primitive-`0x70` payload used in that run; it does **not** cover the type-`0x80`
family, whose handler `0x284ac4` contains a broad nested command decoder.

Service 5 itself is not missing: callback `0x2618e8` is selected organically by
the generic callback dispatcher and receives the normal `0x05f3`/`0x05e2` sweep.
The framework is downstream of task 5 (`0x2af652 -> 0x2638e4`), so it is not the
hardware ingress. The object-ingress question is closed against the DSP
interface: task-21 status `0x120c` crosses task 20 into GSM 11.14 FETCH
(`A0/12`), whose proactive-command D0 response reaches the `0x177x` router.
None of the validated DSP RX families enters that SIM Toolkit chain. This
excludes DSP ingress for SAT only; it does not exclude a separate DSP-owned
ordinary-registration path.

For types `0x70..0x7f`, `0x29bc00` preserves the type as the firmware message
class and posts the message to task 2. Class `0x70` takes task 2's
unknown-response fallback; class `0x74` is explicitly dispatched to `0x234954`.
The type-`0x70`/`0x74` `0d 00` service-control completion contract is
single-homed in `service_firmware_map.md`. Numeric group labels from another
DCT3 firmware still need reconciliation at both decoder layers before being
generalized. The direct DSP translator `0x282d64` handles classes 3/5/17/47,
but its class-5 primitive set starts at `0x11` and does not include the task-15
registration primitive `0x0b`. These are distinct protocols despite sharing a
numeric class value.

The peer consumes only complete packets and advances TX consumer `0x0a6`; it
correlates responses separately at the protocol boundary. The complete measured
packet vocabulary, per-type counts, and per-packet dispositions are single-homed
in the generated `dsp_packet_semantics.md`; do not infer request/reply behavior
from packet type alone. Consumption alone produces none of `0x0588`, `0x05ea`,
or `0x07dd`. Every observed outbound packet has classified MCU-side semantics,
including the fire-and-forget type-`0x1a` ARFCN bitmap publication.

Types `0x0d` (indexed 64-byte block uploads) and `0x3c` (selector-keyed
lookup-table uploads) are one-way DSP configuration publications named for their
recovered wire structure only. Their ROM-4 DSP consumer and physical purpose
remain unidentified, so stronger subsystem names would be speculation. The
type-`0x05` family divides into request-derived exchanges, one-way publications,
and the `0x622a` one-way report whose transaction completes through DSPIF shared
control; `dsp_packet_semantics.md` classifies each packet.

Type `0x70` is closed at the MCU boundary. Task-2 initializer `0x2346b2`
calls the single constructor `0x264f30`, which publishes selectors `0x13`--`0x16`:
`0x13` carries one four-byte platform value selected from a ROM default or a
hardware-derived helper, while `0x14`, `0x15`, and `0x16` carry staged 12-, 20-,
and 24-byte bootstrap tables. These four objects retain no reply token. The
static object at `0x2db250` is different: task 2 posts its `0d 00` request at
`0x2347e4` and consumes the correlated type-`0x74` completion. Only after that
completion, handler `0x234954` posts static object `0x2db234`, payload `0a 09`,
as a one-way follow-up. `0a 09` expects no echo (ledger
`dsp_type70_0a09_task13_reply`).

Type `0x51` is one transaction rather than independent requests.
Task 9's `0x28d710` selects a ROM profile descriptor containing a DSP word
address, a word count, and the data. Its sole packet post at `0x28d880` emits
command `0x22`, the current big-endian DSP word address, and at most 39 data
words. Successive v6 packets address `0x2206, 0x222d, ... 0x22f0`, uploading
247 contiguous words; v5.01 analogously uploads 241 words from `0x2286` through
`0x2370`. The constructor advances through the descriptor and retains no
per-packet reply token. The physical purpose of this version-specific DSP memory
image remains unknown, so it is not labelled as audio, radio, or coefficients.

### Organic radio-init queue lifecycle

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
type-`0x51` segmented DSP memory upload and five type-`0x70` control packets.
Packet consumption remains polling-based prototype behavior. The retained 4 ms
cadence drains the preceding queue so the organic publication reaches the
DSP boundary as a 35-word packet:

```text
task-3 object: length 0x44, type 0x1a
wire payload 68: 0081 9800 0000 ...
```

No firmware message, callback, or RAM state is injected to produce it. The
driver's opt-in DSP service defaults use the empirically stable 4 ms cadence;
the eventual DSP peer should own TX consumption independently of the service-IRQ
timer so correctness does not depend on this scheduling phase.

Type `0x1a` is a fire-and-forget GSM ARFCN channel-set publication with no
request/reply contract. The builder decode (`0x219f0c`), its sole caller
`0x21ba54`, the density-guard rejection with flag `0x81`, the disproved
type-`0x80` state-byte correlation, and the timer-`0x23` cadence are
single-homed in `dsp_service_transport_contract.md` (ledger
`dsp_type1a_direct_registration_request`). Adjacent to the publication,
`0x2697aa(0x23, 0x0a0a)` arms the global DSP-service timer; its live
expiry/rearm through task 4 delivers no task-10 status and does not run
`0x219e30`, proving the timer wheel is live while leaving the semantic
completion unresolved.

RX-ring transport probing pins the return notification: advancing RX producer
`0x1c8` and asserting **FIQ 0** wakes task 4 with receive sentinel `4`, which
calls `0x290904` and dispatches the packet naturally. IRQ lines and the other
FIQ lines do not. The candidate type-`0x80`, primitive-`0x70` registration
reply is disproved (ledger `dsp_type80_primitive70_reply`).

The same boundary establishes the DSP-liveness contract independently of any
semantic reply. A header-only ring entry `0x0003` is an outer RX type-`0x03`
packet. Task 4 increments activity counter `0x112502` at `0x2b3fca` for every
non-sentinel packet before type dispatch; type `0x03` then reaches the generic
discard/diagnostic path and is freed. The counter is cleared at `0x21596e`,
`0x21a1ec`, and `0x2a0f9a`, matching a repeatedly rearmed liveness check.
Periodic liveness traffic is real DSP behavior, but it is not the missing
registration-semantic response (ledger
`dsp_type03_heartbeat_registration_predecessor`).

The wider type-`0x80`/type-`0x83` `0x040b` route is classified separately.
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

Task 17's initializer enters its long-lived event loop at
`0x223964 -> 0x2271c6`; code at `0x2222fc` and its `0x138f` callbacks is reached
only after that loop returns. It is therefore downstream of the awaited
`0x0434`, not its missing predecessor. DSP packet type `0x89` can produce
`0x138f` only while task 4's initialization flag `[0x112501]` is zero; once
that flag is one, the packet is rejected.

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

The controller's event-`0x102f` branch loads result `0x0fbf` at
`0x24788e` and enters the dispatcher call at `0x246ad6` with that value intact.
Its table case is `0x245cb2 -> 0x253610`, a context handler. Jump-table
indexing corrections are ledgered as `lower_result_0fc3` and
`lower_result_0fbf_completes_task17`. Event
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
cell/RSSI/registration — lives largely in the `0x2b7xxx–0x2c9xxx` driver and is not
observed carrying task-22 messages. The narrower shared-control path at `0x290cf4` is live,
so DSP work is not wholly static: its `0x30`/`0x32` request and IRQ-4 completion
contract can be traced in the current coherent profile.

## Later network-emulation dependencies

- **To reach where we are:** the current echo, ready flags, queue drain, and IRQ model are
  sufficient, but they are not a completed DSP contract.
- **The lower-radio session start is a bounded downstream lifecycle.** Organic initialization
  creates task 14 and its eight controller slots, but no task-14 input starts a resource-`0x35`
  operation. The concurrent type-`0x1a` ARFCN bitmap is not evidence of such an operation;
  the falsified type-`0x80`/`0x70` reply (ledger `dsp_type80_primitive70_reply`) must not
  be restored.
- **For the network (operator name + signal):** extend the message-boundary DSP peer (answer the L1
  commands with "camped on a fake cell, operator X, RSSI y") is feasible *in principle* and
  is the right MAME approach, but the L1 protocol is static-only until coherent
  execution reaches it and an organic application path requires it. Signal RSSI is separately
  injectable (CCONT ADC ch1, `network_scouting.md`).
- **Full DSP-core emulation** (a TI Lead core running the downloaded blobs) is a much larger
  project and would still need a faked air interface; not warranted given the above ordering.

The ARFCN publication is not the resource-`0x35` transaction. In the measured
boot task 14 receives no message and neither `0x282238` (resource-`0x35`
transmit) nor `0x267258` (object receive) runs.

Task-5 status `0x13e2` is downstream, not an absent radio transition. Its natural
producer is `0x2b3f60`, called by task 17 at `0x225b8c` after the phase handler accepts
`0x0434`/`0x0a22` (and at `0x223a28` after the phase loop returns). It publishes packed
`0x53e2` with one firmware-owned pointer; the consumer path
`0x255124 -> 0x28a4a8 -> 0x238a24` then constructs `0x1776` for decimal task 14
(ID `0x0e`). The runtime never reaches it because `0x0434` is absent. The
unresolved downstream predecessor is the organic producer of lower result
`0x0fc1`, which selects completion status `0x1391`. The `0x0fbf` context path
and `0x0fc2 -> 0x1392` radio-state path are separate and do not reach `0x0434`;
only after the `0x0fc1` contract is pinned should the proven bidirectional
contract move into a DSP peer device.

## Per-primitive payload semantics

The primitive dispatch of `0x23cde0` (`[msg+9]` = primitive; `r5` = `msg+9`) is
decoded. **The key finding: most primitives forward straight into the `0x2a2xxx`
idle-element render library — the DSP L1 layer *is* the idle content producer.**
Each `0x2a2xxx` target opens with a call to status classifier `0x28fa4c`, which
reads camped/service state `[0x11fce1]`, and then render-posts to task 5. The
full producer chain is:

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
confirm GSM network-byte-order framing. The content is produced by these DSP primitives
arriving and being rendered, not by the camped-state byte alone. In the measured coherent
boot no such primitive arrives; dispatch `0x23d62c` runs zero times.

**Byte-lane caution (swap16):** the network-registration data lives in the struct
at **`0x10d124`** (pool literal `d1280010` → `0x0010d128`) / **`0x10d37e`** — a
network/registration struct (field `+2` a status halfword, message chunks
memcpy'd to `+0x34`), accessed from `0x26f1b8`/`0x208110`/`0x26f608`. Byte-lane
misreads of the same literals as `[0x1028d1]`/`[0x107ed3]` are wrong; auxiliary
addresses must be decoded with the corrected swap16 byte lanes. The state byte
alone is insufficient to produce content.

### `0x2b2exx` data-block routers

The data-block primitives (`0x30/0x33/0x36`) route into `0x2b2exx` targets that do
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

The data path terminates in a dormant MMI-VM event rather than a statically decodable
processor. Further semantic claims require an organic packet or an external protocol source.

## Unresolved contracts
- Decode the other recv handlers' primitives (`0x23c4fc/55c/9e8/be8/d158/d2fe/d430`) and the
  full `0x2b66b0` routing table (same caveat: dormant, no runtime to validate).
- Static RE of the `0x2b7xxx–0x2c9xxx` L1 driver *send* side: enumerate the 287 DSPIF
  command stubs and their command encodings (large, code never runs on our boot).
- Identify the two downloaded blobs (`[0x200+]` from flash `0x200040`; `[0xe00+]`): DSP
  program vs coefficient tables vs audio codec params.
- The MCUIF (`0x40000`) memory-range configuration.
