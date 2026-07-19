# The MAD2 MCU↔DSP interface (DCT3 / Nokia 3210)

A map of how the ARM7 MCU talks to the on-chip **DSP** (which runs the GSM Layer-1
baseband + audio codec) in the Nokia 3210 firmware. It combines static
disassembly with the reviewed shared-ring and service-boundary traces. Use the
focused `NOKI3210_TRACE_DSP_BOUNDARY` for live observation at this boundary.

**TL;DR for emulation:** at boot the MCU treats the DSP interface as (a) a RAM
self-test it passes by echo, (b) a handful of "DSP ready" status flags, (c) a
download target for coefficient/program blobs, and (d) bidirectional lower-service
message rings. The aggregate device carries organic D0 discovery,
type-`0x70` DSP completion, the separate external-service session, and an
opt-in deterministic radio peer through FIQ 0. The radio peer follows recovered
SEARCH_LIST, measurement, channel-change and BCCH contracts through serving-cell
selection; it stops before Location Updating. This proves that the transport
composes in the normal scheduler, but not that the external class-`0x40` peer is
DSP-owned.

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

### Class-0x47 call-control candidate

Task-22 class `0x47` is a strong call-control candidate, but the name remains
provisional until an organic or external-peer scenario reaches it. Handler
`0x23c55c` accepts exactly primitives `1`, `3`, and `7`. Primitive `3` parses a
variable-length address-like object and forwards it through `0x2b2ee0`; primitives
`1` and `7` use the adjacent helpers `0x2b2ef8` and `0x2b2eee`. Those helpers publish
resource ids `0x8c99`, `0x8d59`, and `0x8e19` respectively. The complete handler body
has one signature match in the v5.01 image at `0x23bff0`, so this contract is stable
across the two supported 3210 ROMs.

Translator `0x282d64` is called by task 7's non-class-`0x40` framed-session loop
at `0x237c4e`; it does not run inside either task 22 or the physical DSP packet-ring
decoder. For class `0x47`, its primitive-`2` branch constructs and posts a normalized
class-`0x47`, primitive-`3` message to task 22. This establishes the firmware-side
envelope as DSP RX type `0x8e` -> task 7 -> translator -> task 22, but does not
establish primitive `2` as an over-the-air indication.

The existing `enqueue_rx_packet` interface models the separate MDIRCV ring consumed
by task 4. Arbitrary class-`0x47` data must not be put on that ring: its outer decoder
only accepts the mapped packet families `0x70..0x7f`, `0x80`, `0x83..0x8f`, and
`0x99`. An incoming-call fixture is admissible only after the task-22 L1 mailbox
publication mechanism is recovered, or after a captured peer transaction establishes
an evidenced translation from one of those outer packet families.

An RX type-`0x8e` experiment reached task 7 and sharpened the remaining gate. A
parser-safe class-`0x47`, primitive-`3` frame arrived while session phase byte
`0x11fedb` was zero, so the loop recorded its peer header fields without calling
`0x282d64` and returned type-`0x05` control frame
`1e 02 00 7f 00 02 47 80 70`. Advertising `0x47` in the external-service
command-`0x70` bitmap did not alter the result. The phase byte is read throughout
`0x2824e4`/`0x282d64` and is incremented indirectly at `0x282734`; it is a framed
protocol state, not a resource-availability bit. The experimental fixture was
removed rather than bypassing that state machine.

**Send side (MCU→DSP uplink):** write a command halfword to the **DSPIF register
`0x30000`**, then poke the doorbell interrupt at `0x20008` (pattern seen at `0x291038`:
`strh #4→[0x30000]; strb #2→[0x20008]`). The DSPIF has **287 write sites, almost all in the
L1 driver `0x2b7xxx–0x2c9xxx`** — the per-command send stubs.

**Runtime status.** Without the opt-in radio peer, task 22's class/primitive
downlink remains dormant in the measured boot. The separate shared-ring path is
active: MCU search and channel requests are consumed from the TX ring, and the
radio peer returns ROM-evidenced `0x80`/`0x84`/`0x89`/`0x8b`/`0x8c`/`0x8f`
families through MDIRCV/FIQ0. Firmware selects ARFCN 1 and accepts SI1--SI4.
These packet-ring families do not establish the task-22 publication mechanism
needed for arbitrary class-`0x47` call-control messages.

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
| `0x83` | `0x284734` | controller-gated scalar report; state 3 posts `0x139f` to task 10 |
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

Type `0x83` is not a search/camp completion. Controller states 1 and 2 discard
the packet; state 3 rewrites it to `0x139f` and posts it to task 10; other
states take the diagnostic path. Task 10's `0x139f` handler copies signed
payload byte `+6` to scalar state `0x10dc99` and returns. It emits no lower
result, registration event, or follow-on transaction.

The task-11 consumers distinguish packet completion from search-lifecycle
ownership. Status `0x13b8` from type `0x8b` selects handler `0x215bcc`, while
`0x13b7` from type `0x8f` selects `0x215c28`. Neither initializes task 11.
Task 11 explicitly selects lifecycle byte `3` during reset and retains it
through the initial power scan.  The implemented radio peer reaches a usable
ARFCN, accepted `NO_PSW_LEFT` and channel change, then firmware constructs
status `0x13a5` with action byte `1` for task 11.  That action, not a literal
write of lifecycle byte `1`, starts the selected-cell acquisition operation.
Candidate-list helper `0x2126e4` is separate from this active path. Neither
`0x043f` nor a task-11 RAM byte is synthesized by the peer.

The recovered Nokia trace-name table supplies protocol-family vocabulary that
has been checked against these ROM handlers: outbound `0x1a` is `SEARCH_LIST`;
inbound `0x80` is `RECEIVED_BLOCK`, `0x83` is `RSSI_RESULTS`, `0x84` is
`RA_INFO`, `0x87` is `NO_BCCH_LEFT`, `0x88` is
`NEIGHBOUR_TIMING_OFFSET`, `0x89` is `CHANNEL_CHANGED_CNF`, `0x8a` is
`NO_PSW_FOUND`, `0x8b` is `ALL_RSSI_RESULTS`, and `0x8f` is
`NO_PSW_LEFT`. These names classify packets; they do not establish that an
isolated packet is valid in every firmware controller state.

The four remaining direct task-10 status types are closed through their first
semantic consumer:

- type `0x87` clears controller flag bit 1 and becomes `0x138f`. Task 10 stores
  state byte `0x0d` at `0x10dbdd` and, when both outstanding work pointers at
  `0x10d938` and `0x10d928` are null, calls finalizer `0x219e30`. Its payload
  is not inspected on this path;
- type `0x8a` is discarded only in controller state 1. Otherwise it clears the
  same flag and becomes `0x1390`. Task 10 increments count word `0x10dc90`;
  after it exceeds the firmware-configured limit at `0x110156`, a controller
  bit gate can enter the same finalizer. Setter `0x29da02` owns that limit and
  getter `0x29dbbe` reads it under lock;
- type `0x84` is discarded in controller state 1 and otherwise becomes
  `0x1394`. Task 10 copies its eight-byte body into the working object and
  passes it to general controller-event decoder `0x217cac`; this is a
  structured event input, not a payload-free completion;
- type `0x89` is rejected when its acceptance byte at `0x10dbd7` is one.
  Otherwise it runs state handler `0x216830` and posts `0x1393`. The state
  handler dispatches on the separate radio-controller state at `0x10dc93` and
  does not inspect the packet body on the state-1 path. Task 10 reaches fan-out
  `0x21bb5c` through a separate event arm, not directly from the type-`0x89`
  receive handler. That fan-out publishes controller/status work but does not
  directly call `0x219e30`.

This corrects the earlier claim that no fixed-status DSP handler can produce
the task-17 completion. Status `0x1391` remains the explicit lower-result
completion, but it is not the only entrance to finalizer `0x219e30`: direct
DSP type `0x87` can enter it immediately under empty-work gates, and repeated
type `0x8a` reports can enter it after the configured count limit. When
controller flag `[0x10dbdb] == 1`, `0x219e30` constructs `0x0434`, transfers
the queued object and scalar state into it, and posts it through `0x251c3e`.
Otherwise it publishes a separate `0x1100` status to task 15 before resetting
the same controller/timer state. These paths identify the missing peer
contract family; they do not establish which RF condition should cause a
faithful peer to send `0x87` or `0x8a`, so neither packet is synthesized.

### Type `0x86` controller protocol

The structured type-`0x86` decoder is closed at its first protocol boundary.
It is a lower controller/transfer multiplexer, not the constructor for the
type-`0x8e` framed session:

- direct DSP ingress reaches `0x284316` from the task-4 type switch at
  `0x2b4070`; the other calls at `0x2845d4` and `0x284fb4` replay firmware ROM
  descriptors while handling task callbacks;
- byte `+4` selects exactly four accepted subtypes: `0x70`, `0x80`, `0xb0`,
  and `0xb1`; all other values are freed without a transaction;
- `0x70` owns the queued channel context and its task-14/task-15 completion
  notifications; `0x80`, `0xb0`, and `0xb1` select task-3 transaction
  descriptors according to the active controller/configuration state;
- type `0x89` handler `0x284f74`, not type `0x86`, posts status `0x1393` to
  task 10. It does not itself establish controller state 3. The independently
  owned controller state starts at 1 after the outbound `SEARCH_LIST` builder;
  the type-`0x86` callback paths can subsequently operate when firmware moves
  that controller to state 7;
- every accepted direct packet eventually runs `0x2841e4`, the independent
  ten-sample measurement publication to task 8, and then frees the broker
  object.

The entire type-`0x86` implementation and its two internal callers use the
controller context rooted near `0x10d0db` and queued channel objects near
`0x11281f`/`0x11381f`. They contain no load from, store to, or call into the
framed-session phase byte `0x11fedb` or its state machine at `0x2824e4`.
Consequently a valid type-`0x86` exchange cannot directly bootstrap acceptance
of type-`0x8e` class-`0x47` traffic. Any indirect relationship would have to be
a new, independently evidenced task-level contract; none is present in this
closed decoder.

### Framed call/session startup

The phase byte at `0x11fedb` is firmware-owned. Task 7 initialization clears it
at `0x23471e`; no DSP packet is required to manufacture phase 1. Callback
`0x0a` (`0x2a8ca4`) handles event `0x06a9` by entering validator `0x2831a8`.
When its firmware-side checks succeed, it calls `0x28316c`, which:

- marks the session/control state active and initializes context offset
  `+0x72`;
- posts task-`0x1a` event `0x0202`;
- clears the framed-session index at `+0x74` and sets phase `+0x73` to 1;
- queues descriptor `0x2e0023f0` through `0x2b0482`.

The callback-table pointer at `0x2db770` establishes ownership of dispatcher
`0x2a8ca4`; the starter is therefore application/firmware initiated rather
than a hidden direct DSP callback. The coherent eight-second offline boot does
not execute callback event `0x06a9`, the validator, the starter, or constructor
`0x2824e4`.

Once active, task 7 sends supported framed classes through the phase machine.
Class `0x42` is decoded by `0x2827f8`. Its command `0x64` branch accepts payload
byte `+0x0b == 0x45` while phase equals 1, increments the phase to 2, clears the
per-phase index and calls outbound constructor `0x2824e4`. A different phase
selects the phase-20 recovery path; other payload values reset/terminate the
session. Thus the first legal peer acknowledgement is known, but it must only
be produced in response to an organic session start. It is not a registration
or idle-boot stimulus.

No simple fixed-status handler produces `0x1391` itself. That status must be
derived by a structured decoder or later controller transition. It is no
longer correct, however, to infer that every route to `0x0434` requires
`0x1391`: types `0x87` and `0x8a` reach the common finalizer through statuses
`0x138f` and `0x1390`. Ledger `dsp_type80_primitive70_reply` covers only the fabricated
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
including type-`0x1a` SEARCH_LIST. That command retains no direct transaction
token; its results arrive asynchronously through the recovered radio-report
families.

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

Type `0x1a` is SEARCH_LIST, carrying a GSM ARFCN channel-set bitmap. It has no
direct request/reply token: the builder decode (`0x219f0c`), its sole caller
`0x21ba54`, the density-guard rejection with flag `0x81`, the disproved
type-`0x80` state-byte correlation, and the timer-`0x23` cadence are
single-homed in `dsp_service_transport_contract.md` (ledger
`dsp_type1a_direct_registration_request`). Adjacent to the publication,
`0x2697aa(0x23, 0x0a0a)` arms the global DSP-service timer; its live
expiry/rearm through task 4 delivers no task-10 status. The radio peer instead
returns asynchronous measurement and search-completion reports through the
MDIRCV ring.

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
`0x223964 -> 0x2271c6`; code at `0x2222fc` and its callbacks is reached only
after that loop returns. It is therefore downstream of the awaited `0x0434`,
not its missing predecessor. This task-17 callback family must not be confused
with task 10's distinct `0x138f` status, which is produced by DSP RX type
`0x87` and can enter `0x219e30` as described above.

The explicit lower-result completion is status `0x1391`. The dispatcher jump
table maps it to `0x21b9b4 -> 0x21b198`; when the firmware-owned work state is
ready this reaches `0x219e30`, the producer of `0x0434`. Status `0x1392` is
the adjacent table entry and instead maps to the radio-state/configuration path
at `0x21c36a`. Direct statuses `0x138f` and `0x1390` provide the separately
gated DSP-report entrances documented above. Callback `0x2b60f6` unregisters
lower-radio key `0x4c00` with
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

Parser `0x209978` is specifically a GSM Mobility Management parser: object byte
`+4` must carry protocol discriminator 5, and message types `0x02`, `0x04`,
`0x21`, `0x22`, and `0x29` map to Location Updating Accept/Reject and CM Service
Accept/Reject/Abort results. This contract is necessarily downstream of first
cell acquisition and signalling-channel establishment. It must not be used to
bootstrap those prerequisites by injecting `0x07dd` or one of its translated
statuses.

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
producer is `0x2b3f60`, called by task 17 at `0x225b8c` from the dispatcher arm for
input `0x09d6` (and at `0x223a28` after the phase loop returns). It publishes packed
`0x53e2` with one firmware-owned pointer; the consumer path
`0x255124 -> 0x28a4a8 -> 0x238a24` then constructs `0x1776` for decimal task 14
(ID `0x0e`). A boundary diagnostic now reaches `0x0434` through direct type-`0x87`
completion. Task 17 consumes it at `0x225240`, runs `0x225b6c`, and continues at
`0x226348` without reaching `0x2b3f60`. The missing predecessor of `0x13e2` is therefore
the organic producer of task-17 input `0x09d6`, not the arrival of `0x0434`. The
`0x0fbf` context path
and `0x0fc2 -> 0x1392` radio-state path are separate and do not reach `0x0434`;
only after the `0x0fc1` contract is pinned should the proven bidirectional
contract move into a DSP peer device.

## Per-primitive payload semantics

The primitive dispatch of `0x23cde0` (`[msg+9]` = primitive; `r5` = `msg+9`) is
decoded. **The key finding: most primitives forward straight into the `0x2a2xxx`
idle-element render library — the DSP L1 layer *is* the idle content producer.**
Each `0x2a2xxx` target opens with a call to status classifier `0x28fa4c`, which
calls firmware-owned UI/network-state classifier `0x28f0f2`, and then
render-posts to task 5. Earlier notes described this as a direct read of a
"camped byte" at `0x11fce1`; the complete decode disproves that simplification.
`0x28f0f2` derives one of seven presentation states from a multi-field firmware
context and an auxiliary status query. The full producer chain is:

```
DSP L1 status primitive → 0x23cde0 → 0x2a2xxx render fn → 0x28fa4c/0x28f0f2
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
arriving and being rendered, not by assigning one registration byte. In the measured coherent
boot no such primitive arrives; dispatch `0x23d62c` runs zero times.

**Ownership correction and byte-lane caution:** the large structure at
`0x10d124..0x10d37f` is not proved network-registration state. A coherent
write-watch attributes its active initialization and updates to running-task
IDs `0x14` and `0x15`, decimal tasks 20 and 21: the SIM application router and
SIM driver. The apparent lower-radio attribution came from nearby consumers
and is retired. Pool literal `d1280010` still decodes to `0x0010d128`; readings
such as `[0x1028d1]` are invalid swap16 byte-lane interpretations. Camping must
be located from an evidenced DSP parser/output, not inferred from this SIM-owned
storage.

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
