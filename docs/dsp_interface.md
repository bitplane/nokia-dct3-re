# The MAD2 MCU↔DSP interface (DCT3 / Nokia 3210)

A map of how the ARM7 MCU talks to the on-chip **DSP** (which runs the GSM Layer-1
baseband + audio codec) in the Nokia 3210 firmware. It combines static
disassembly with the reviewed shared-ring and service-boundary traces. Use the
focused DSP-boundary MAME log category (`RUN_VERBOSE=1`) for live observation at this boundary.

**TL;DR for emulation:** at boot the MCU treats the DSP interface as (a) a RAM
self-test it passes by echo, (b) a handful of "DSP ready" status flags, (c) a
download target for coefficient/program blobs, and (d) bidirectional lower-service
message rings. The aggregate device carries organic D0 discovery,
type-`0x70` DSP completion, the separate external-service session, and an
opt-in deterministic radio peer through FIQ 0. The radio peer follows recovered
SEARCH_LIST, measurement, channel-change, BCCH, random-access, LAPDm, Location
Updating, RR release, and operator-presentation contracts for one deterministic
laboratory cell. This proves that the transport composes in the normal
scheduler, but not that the external class-`0x40` peer is DSP-owned or that
unimplemented mobility, paging, authentication, calls, or SMS are implied.

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
| `[0x00–0x04]` | bootstrap-ready words → `0x01` | the peer publishes all three after the product-configured download handshake: 64 exchanges on both 3210 ROMs and 58 on 3310 v6.39. Firmware reads ordinary shared RAM. |
| `[0xe0]` → `0x00`, `[0xfe]`/`[0x100]` → `0x01` | **DSP ready/busy flags** | MCU zero-writes to `0xfe`/`0x100` request peer acknowledgements. DSPIF command 4 clears peer-owned busy word `0xe0`; `0xe4` is the lower-service pending counter. |
| `[0xa8]` / `[0xaa]` | **adjacent MCU-written DSP control words** | Both supported 3210 ROMs update `0xa8` through shared-control command `0x08`. In both answered-call lifecycles, command `0x1c` independently writes `0xaa=ffff` during the active interval and restores zero at clearing. A separate 5110 ROM4 analysis calls these request/acknowledgement, but the organic paired-ROM 3210 writes contradict transferring that semantic unchanged. The HLE therefore latches only raw `0xa8`; neither word is treated as PCM. |
| `[0xf6–0x102]` | **config words** (`0x0100 0x0300 0x0001 0x0000 0x0001 0x0001 0x0200`) | 7 individual writes at `0x290a44–0x290a64`. |
| `[0x200–0x600]` | **coefficient/parameter table** (512 halfwords) | strided copy at `0x290a94`: reads one halfword per 0x20-byte record from flash `0x200040`, packs into `[0x200+]`. |
| `[0xe00+]` | **second blob** (~240+ halfwords) | ARM block-copy (`stmia`) at `0x2b5bd0` (reached via `bx pc` ARM-mode switch). |

So the MCU **stages DSP tables/program from flash into shared RAM** at boot — the DSP's
data *is* in the image. On a real phone the DSP would execute or consume it;
the current HLE peer stores the blobs but does not interpret them.

DSPIF has no product-specific bootstrap switches and never fabricates a value
on an MCU read. It exposes backing RAM plus address-level access notifications;
the selected DSP backend owns acknowledgement consumption and publication. For
the current HLE this includes NHM-2's alternating `0x0fe`/`0x100` tokens, its
parked-loader response at `0x002`, and the service code-block publication. A
future DSP core therefore replaces the HLE at this seam without changing ring,
doorbell or interrupt transport.

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
state is unresolved. Use the DSP-boundary MAME log category (`RUN_VERBOSE=1`) for this boundary.

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
Both ROMs use the same set and structurally matching consumers. In the paired
3210 traces, bootstrap word `0x004` is published after the 64-exchange sequence
but is not subsequently read in either measured lifecycle.

The service submodel clears the pending count and raises IRQ4. The separate
request-driven external-service submodel consumes complete TX packets and returns
correlated service and transport responses through the inbound ring and FIQ0.
Neither is a complete DSP implementation. No derived lower-radio response has connected this traffic
to `0x05ea`, task-15 `0x07dd`, or the SIM registration result, so treating the D0 packet
as that request would be speculation.

Counter drain and IRQ 4 are jointly minimal: either signal alone leaves
`service_ready=0` (single home: `service_bootstrap.md`). The DSP HLE therefore
models one coherent peer transaction, not two independent conveniences.

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

### Cipher-control packet

An acknowledgement-gated GSM Cipher Mode Command now exercises the firmware's
real cipher-control boundary before each incoming laboratory service. The
retained command is `06 35 00`: its SC bit is zero, so the connection explicitly
remains unciphered. The v6.00 ROM publishes TX type `0x14`, payload
`00 f4 ff ff ff ff ff ff ff ff 00 00`, requires no DSP-ring response, and then
organically sends RR Cipher Mode Complete (`06 32`). Calls, ordinary SMS and the
first multipart Smart Message part all continue through their existing
checkpoints.

Two quarantined probes delimit the stronger interpretation. Command
`06 35 01` (start A5/1) changed the type-`0x14` prefix to `02 00` and still
produced Cipher Mode Complete without a DSP reply. Replacing the SIM's eight
`EF_Kc` bytes with `01 23 45 67 89 ab cd ef` changed the packet to
`02 00 cd ef 89 ab 45 67 01 23 00 00`, confirming that the middle eight bytes
carry Kc in the firmware/DSP word order. Both probes were removed: the radio
peer still exchanges clear decoded blocks, so retaining SC=1 would falsely
claim A5 bitstream processing. Type `0x14` is therefore a recovered one-way
cipher-control publication, not evidence of a working cipher backend.

### Answered-call boundary

The named answered-call fixture supplies the erased identity's matching
security verifier, enters `12345` through the physical keypad, observes the
organic MAD2 PUP ringtone gate and presses physical Answer. The v6.00 ROM emits
CC Connect, accepts Connect Acknowledge and remains on the assigned TCH/F.
After that acknowledgement, the retained MCU-to-DSP trace contains one LAPDm
RR and periodic empty type-`0x1b` blocks
`00 f0 01 03 01 2b...2b 00`. The only other packet is type `0x05`, class
`0x00`, command `0x5f`, already classified independently as the periodic
external-service poll and present at the same absolute time in unanswered and
idle controls. No new packet family or payload carries speech or codec
configuration.

A complete changed-write trace of the shared window does expose a lower
boundary outside the packet ring. Immediately after organic CC Connect, task 5
calls shared-control helper `0x290cf4` with command `0x08`, value `0x060b` and
commit flag 1; the helper publishes encoded word `0x860b` at shared offset
`0x0a8` and rings DSPIF command 4. This occurs before Connect Acknowledge and
does not occur in a matched unlocked-but-unanswered control. It is the first
proved answer-only DSP control entrance.

About 22 ms later, task 9 emits a separate bounded command group: commands
`0x09`, `0x26`, `0x21`, `0x25`, `0x29`, and `0x2f` program oscillator word
`0x0e10` (900 Hz), amplitude `0x0041`, and a route bit, then clear the
oscillator after 120.8 ms. The same helper/caller family produces keypad tones,
and no further shared-control command follows during the retained call. This
is an answer acknowledgement/UI tone, not speech framing.

A second physical press of the same context-sensitive Navi key organically
ends the retained call. The phone emits CC Disconnect, accepts network CC
Release, emits Release Complete, and requests the proved release channel
change. The command-`0x08` desired word follows the complete observed
lifecycle `0x0002 -> 0x060b -> 0x040a -> 0x0002`.

The answer transition is built by firmware stores at `0x28d9a8`
(`0x0002 -> 0x0203`) and `0x28dd1c` (`0x0203 -> 0x060b`). After release,
stores at `0x28d97e` and `0x28d986` produce `0x060a` and then `0x040a`;
`0x28dcf6` later restores `0x0002`. The desired table is at `0x11206c`
and its applied mirror at `0x112054`.

The compiler tables now separate two fields. The answer-only add table contains
`0x0201`; its release keep-mask is `0xfdfe`, the exact complement of that
field. The independent dedicated-channel add table contains `0x0408`.
Non-speech registration and paging channels repeatedly publish `0x040a`
(`0x0002 | 0x0408`), while Answer publishes `0x060b`
(`0x040a | 0x0201`). On clearing, firmware removes `0x0201` before it removes
the dedicated-channel field. This proves `0x0201` as the speech-path field
without assigning unsupported meanings to its two constituent bits.

The v5.01 ROM independently relocates the compiler to `0x28eae0`, the shared
helper to `0x2906a8`, the desired table to `0x111e6c`, and the applied mirror
to `0x111e54`. Nevertheless, a complete physical Answer-to-End call publishes
the identical DSP wire words at shared offset `0x0a8`: idle `0x8002`, answered
`0x860b`, post-release `0x840a`, and restored idle `0x8002`. This proves a
cross-ROM MCU-to-DSP protocol contract while keeping the relocated firmware
addresses in trace-only quarantine.

The v5.01 compiler tables independently contain the same `0x0201` add,
`0xfdfe` keep-mask and `0x0408` channel field. The checksum-independent
`verify-dsp-speech-control-static` gate checks both ROMs.

Shared offset `0x0a8` is a multiplexed command wire, not persistent command-8
storage. Bits 15..12 identify commands `0x08`/`0x09`, and bits 11..0 carry the
value. Thus the answer word `0x860b` is legitimately overwritten about 22 ms
later by tone-control wires `0x98af` and `0x99a0`. The HLE now decodes command
`0x08` into independent applied state, mirroring the firmware's per-command
table, rather than mistaking the last raw wire value for the speech state.
DSPIF remains only the shared-RAM and doorbell transport.

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

Type `0x99` is a scalar measurement sampler rather than a PLMN-result
transport. It converts and averages the incoming sample according to the
controller state, constructs a six-byte type-2/command-`0x4a` object and posts
it to task 3. It does not post to task 10 or 11, set the task-11 measurement
mode, or populate a network identity. This closes the last first-level RX
family as a direct source of the missing PLMN-selection transition.

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
  passes it to general controller-event decoder `0x217cac`. The `0x1394` arm
  reads object byte `+4` as a flag/mask and bytes `+5..+7` as a big-endian
  24-bit timing value, then derives an eight-byte `0x0400` object for task 16.
  It is a structured radio-access timing input, not a payload-free completion
  or a PLMN-identity transport;
- type `0x89` is rejected when its acceptance byte at `0x10dbd7` is one.
  Otherwise it runs state handler `0x216830` and posts `0x1393`. The state
  handler dispatches on the separate radio-controller state at `0x10dc93` and
  does not inspect the packet body on the state-1 path. Task 10 consumes
  `0x1393` at `0x21c340` and conditionally reaches fan-out `0x21bb5c`.
  Helper `0x21b468` is a separate replay/event path, not the normal `0x1393`
  consumer; joining the two paths previously made the channel confirmation
  appear to publish task-17 home-PLMN status `0x0433` directly. It does not.
  The fan-out also does not directly call `0x219e30`.

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
  owned controller state starts at 1 after the outbound `SEARCH_LIST` builder.
  For assigned channel `0x80`, helper `0x2167c0` deliberately enters state 6;
  state 7 is the alternate branch for other channel identities;
- Nokia's recovered MDI vocabulary identifies type `0x86` as `BLOCK_REQUEST`.
  In state 6, the decoder's accepted subtype `0x80` releases a firmware-queued
  dedicated-channel block. The internal state-7 replay descriptors at
  `0x2e1128` and `0x2e1130` instead carry subtypes `0xb0` and `0xb1`;
- every accepted direct packet eventually runs `0x2841e4`, the independent
  ten-sample measurement publication to task 8, and then frees the broker
  object.

The entire type-`0x86` implementation and its two internal callers use the
controller context rooted at `0x10dbd0` and queued channel objects near
`0x11281f`/`0x11381f`. They contain no load from, store to, or call into the
framed-session phase byte `0x11fedb` or its state machine at `0x2824e4`.
Consequently a valid type-`0x86` exchange cannot directly bootstrap acceptance
of type-`0x8e` class-`0x47` traffic. Any indirect relationship would have to be
a new, independently evidenced task-level contract; none is present in this
closed decoder.

The controller root was previously recorded as `0x10d0db`. That address was a
byte-lane transcription error: the swap16 literal `dbd00010` resolves to the
aligned firmware address `0x10dbd0`, as confirmed by the live state writes at
`0x10dbd6`.

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
image remains unknown, so it is not labelled as speech code or coefficients.
Static placement nevertheless narrows its ownership: `0x28d710` is the same
firmware audio manager that initializes the desired shared-control table at
`0x11206c`, and the two extracted images have an 85.2% sequence-match ratio.
The HLE now applies each big-endian address/data fragment to DSP-owned 16-bit
data memory rather than merely discarding it. `make verify-dsp-memory-upload`
proves seven contiguous fragments populate `0x2206–0x22fc` (247 words) on v6.00
and `0x2286–0x2376` (241 words) on v5.01. No fixed address or length is built
into the consumer and no reply is fabricated.

### Organic radio and registration transport

Task 3 serializes firmware-owned L1 requests into the DSP TX ring. The HLE peer
consumes those packets and returns request-correlated reports through the DSP RX
ring; advancing its producer pointer and asserting FIQ0 wakes task 4, which
dispatches the packets naturally. No task message, callback or firmware RAM
state is injected.

The complete search, cell selection, random-access, LAPDm, Mobility Management,
release and steady-camp sequence is single-homed in `network_scouting.md`.
This document retains only the DSPIF transport contract. In particular,
SEARCH_LIST type `0x1a` is asynchronous, type `0x80` is a received-block
envelope rather than a generic completion, and lower task statuses such as
`0x0434`, `0x1391` and `0x13e2` must remain firmware-owned.
## Network-emulation boundary

The message-boundary peer now implements the organic ROM-4 search, channel,
random-access and signalling transactions through accepted Location Updating,
RR release and steady camp. `nokia_gsm_network_device` owns GSM data while
`nokia_dsp_hle_device` owns Nokia packet translation and request correlation;
the complete contract and gates are in `network_scouting.md`.

Operator presentation is not a peer string. After MM acceptance, task 11 passes
the firmware-owned PLMN record to resource `0x5031` and posts its own render
event. The retained peer supplies neither task status `0x13e2` nor display
primitive `0x25`. Those remain separately decoded protocol families, not
shortcuts for registered-state presentation.

The older type-`0x80`/primitive-`0x70`, direct `0x0434`, identity and guessed
packet diagnostics are disproven and removed. A header-only type-`0x03` packet
remains as the DSP idle/liveness indication: experiments disproved it as a
registration or UI completion, but proved that the firmware consumes it as
MDI activity. Without periodic DSP activity the firmware enters its independent
reason-`0x68` DSP-watchdog reset path after roughly 32 seconds. Full DSP-core emulation
would still require a synthetic air interface and is not needed for the current
registered/camped boundary.

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

## Speech media boundary

Speech is being added as a separate data plane, not as another DSPIF
control-packet special case:

```
GSM TCH/F speech frame (33 octets / 20 ms)
        <-> DSP GSM-FR transcoder
        <-> 160 signed PCM samples at 8 kHz
        <-> MAD2/COBBA PCM and analogue endpoints
```

`nokia_radio_peer_device` exposes bounded uplink and downlink speech-frame
queues when RR has assigned the physical speech-mode TCH/F. The TCH exists
while the phone rings and briefly during FACCH release, independently of
call-control and handset audio routing.

The downlink queue boundary is explicitly tri-state. A good decoded block
carries a frame, a failed protected block or FACCH-stolen interval carries a
BFI, and an empty queue carries no delivery. Only the explicit BFI advances
GSM 06.11 substitution state; a temporarily empty producer/consumer boundary
leaves that history unchanged instead of inventing radio loss.

`nokia_gsm_fr_codec` is the DSP-side standardized codec boundary. It wraps the
official GSM 06.10 RPE-LTP implementation and converts exactly one 160-sample
PCM block to or from one 33-octet frame. It does not own radio scheduling,
channel coding, PCM routing, or analogue gain. `make verify-gsm-fr-codec`
checks framing, bidirectional conversion, malformed-frame rejection, and
reset independently of phone firmware. Encoder and decoder RPE-LTP histories
are separately snapshotable as fixed-width algorithm state. The verifier
branches after four non-silent frames and proves that restoring the snapshot
produces bit-identical future encoded frames and sample-identical decoded
PCM; it also rejects invalid decoder-lag state.

`nokia_gsm_fr_receiver` is a separate ETSI GSM 06.11 receive-side boundary.
Layer 1 supplies either a valid frame or BFI; a received bad payload is never
passed to the speech decoder. Before any valid history it emits silence.
Thereafter it substitutes the previous valid encoded frame at the decoder
input, leaves the first lost interval at full level, and progressively
attenuates subsequent output to silence on the sixteenth 20 ms loss (320 ms).
A valid frame resets the loss sequence. The substitution frame and loss
position are save-state data alongside, but distinct from, the GSM 06.10
predictor. The standalone verifier covers loss before first speech, first-loss
substitution, the 320 ms mute bound, clean recovery, invalid state rejection
and sample-identical save/load branching in mid-loss.

The DSP HLE clocks this boundary every 20 ms only when both the TCH/F and the
product-configured command-`0x08` speech field are active. It reads one microphone block
through `nokia_mad2_pcm_device`, encodes and submits the uplink frame, then
decodes any downlink frame and transfers one earpiece block through the same
full-duplex boundary. The PCM device owns the product-configured 520 kHz
`PCMDClk` and 8 kHz `PCMSClk` contract. Their integral ratio gives 65 serial
data-clock periods per full-duplex frame. NSE-3 MAD2/COBBA-GJ documentation
establishes a 16-bit word containing sign-extended 13-bit linear PCM (bits
15--13 repeat the sign and bits 12--0 carry the converter value).
Same-ASIC Nokia troubleshooting material establishes the one-clock,
active-high sync pulse at 520/8 kHz; Nokia's separately documented
sign-extended PCM contract supplies MSB-first falling-edge transfer. NSE-8
product configuration combines that family evidence, leaving 48 idle clocks,
but retains every property as configuration pending a direct NSE-8 DSP or
logic trace. Generic MAD2/COBBA defaults remain inert. The runtime gate
validates the configured shape and serializes every sample bit before
reconstructing the converter value. MAD2 converts between that value and the
left-scaled 16-bit domain consumed by GSM 06.10; COBBA alone converts between
the serial value and normalized analogue samples.

The command-`0x08` request and physical TCH are necessary but not sufficient
to start the HLE data plane. The product's PCM profile must also describe the
supported 8 kHz, integral-clock, one-sync-clock, 16-bit MSB-first
falling-edge link. Default or unknown product profiles therefore remain inert
instead of emitting synthetic silent GSM frames. If COBBA rejects a live
transfer, that 20 ms uplink frame is not encoded or submitted; the failure is
counted and the organic media verifier treats either condition as an error.
`make verify-radio-pcm-missing` proves this negative contract under both
v6.00 and independently relocated v5.01. Each firmware organically requests
its speech route and later completes call teardown, but the disabled PCM
component permits no DSP codec tick and no good uplink delivery. The
independent network transmitter continues producing downlink frames; missing
handset hardware does not incorrectly stop the remote endpoint.

MAME saves those two codec histories alongside the DSP HLE, rather than
serializing libgsm pointers or allocator padding. Combined with the complete
Layer-1 endpoint state, `make verify-radio-call-state-roundtrip` saves and
loads during organically answered v6.00 and v5.01 calls. Each run first
advances a two-second reference branch, restores the snapshot, and advances
the same emulated interval again. The gate requires the ordered DSP and
independent network speech checkpoints to match exactly, then evaluates
continuing bidirectional media and normal physical-End teardown only on the
canonical restored timeline. The v6.00 run additionally checks its decoded
speech-control lifecycle; the independently relocated v5.01 run checks its
older MCU/DSP audio-control wire lifecycle. Both replay four checkpoints
exactly before clean teardown, demonstrating that codec, Layer-1 and peer
state ownership is not tied to one firmware revision.
See the
[NSE-3 system-module description](https://electronicsandbooks.com/edt/manual/Hardware/N/Nokia/Phone/6110/03SYS%20%5B73%5D.pdf).
The edge convention is corroborated by Nokia's
[sign-extended PCM contract](https://fcc.report/FCC-ID/LJPRX-9/393500.pdf).

COBBA owns the 8 kHz converter stream and bounded queues on both sides. Its
sound interface exposes the documented three differential microphone pins
MIC1/MIC2/MIC3 and the separate EAR and HF outputs. NSE-8 board composition
wires MAME's host-facing physical microphone endpoint specifically to MIC2
and its receiver speaker specifically to EAR; other products do not inherit
those connections.
Only COBBA converts those samples into uplink PCM. With no host capture source
the pin naturally yields silence; no DSP or call fixture injects samples.
NSE-8 product configuration supplies the HLE internal-call MIC2/EAR path and
applies the service manual's nominal +18 dB/-10 dB transfer. The API is named
`set_hle_voice_profile` to make its status explicit: it describes the
product's internal handset path and nominal gains while the real DSP backend
is absent, not a decoded power-on register value or firmware-selected mux
transaction. The
still unknown DSP control-register encoding is not synthesized, and MCU call
state never writes this route. Accessory-driven runtime changes remain pending
recovery of COBBA's control bits.

The physical-uplink acceptance run uses MAME's ordinary microphone endpoint
with an external host-side 1 kHz source. The pinned MAME PulseAudio module
advertised capture sources but lacked its record-stream implementation, so
`mame-pulseaudio-input.patch` supplies the generic source-open, buffered-read
and source-close contract; no Nokia device receives a test-source option.
With the external source attenuated to retain headroom after NSE-8's +18 dB
MIC2 gain, the post-TDMA-scheduler v6.00 run produced 850/850 non-silent
COBBA microphone blocks and the v5.01 run produced 1200/1200. The separate
network peer's GSM-FR decoder observed peaks 2264 and 2032 respectively.
`radio_physical_uplink_trace_check.py`
requires at least 100 non-silent microphone blocks, non-zero decoded uplink
energy, and rejects clipping. `make verify-radio-physical-uplink` creates a
temporary isolated PulseAudio sink, drives its monitor with an attenuated
host-side 1 kHz stream, selects that monitor only for MAME, and removes it on
exit.

COBBA's DSP control plane is represented separately from those samples.
`nokia_cobba_device` exposes an opaque 16-register, 12-bit serial transport
matching the reviewed ROM4 port protocol: data is latched first, then a
low-nibble register select commits a write; select bit 4 requests a read, whose
data port reports the addressed 12-bit value with the busy flag clear after
the immediate modelled transaction. Control register `0xD` resets to the
recovered idle handshake value `0x00c` (accept bits clear, completion bits
set). Other register meanings are deliberately not
attached to routes or gains yet. In particular, command `0x08` does not
synthetically write a COBBA register.

The laboratory network has two separately configured speech endpoints.
`NETCFG` bit `0x10` selects raw frame loopback, which consumes an uplink frame
at the network boundary and returns that frame as downlink. Bit `0x20` instead
selects `nokia_gsm_voice_peer_device`, a network-side transcoder with codec
state independent of the handset DSP. Its explicit laboratory source is a
1 kHz signed-PCM signal, matching the service-manual audio-level test
frequency. The source enters only through network GSM-FR encoding and the
radio downlink queue; it does not alter handset state or inject samples into
MAD2, COBBA, or the UI.

The transcoder is clocked by every independently decoded uplink traffic
interval, not only by successful speech payloads. A good frame advances its
network-side GSM 06.10 decoder normally; an uplink BFI or FACCH-stolen
interval enters a separate GSM 06.11 receiver substitutor. In either case the
remote microphone is encoded into the next downlink frame, so damage in one
radio direction cannot pause the other direction. Predictor and substitution
state reset at each new traffic-channel activation and all survive emulator
save/load as fixed-width state.

Between the DSP speech queues and either network endpoint,
`gsm_tch_f_l1` now owns a handset-independent TS 45.003/TS 46.010
boundary. It converts the conventional 33-octet codec representation to the
260 serial codec bits, applies table-2 subjective-importance ordering, the
class-1a parity code, the rate-1/2 convolutional code and the unprotected
class-2 tail to form 456 coded bits. The receive side is a separate hard
decision Viterbi decoder with parity and tail validation; a bad protected
frame is not delivered as good speech.

The same component maps each block through the eight-burst diagonal
interleaver, combines old and new block halves at the four-burst cadence, and
packs the resulting 114 data bits and `hl`/`hu` flags into the 148-bit GMSK
normal-burst shape. Training sequence 2 comes from the cell's assigned BCC,
not from a media fixture. The 114-bit field is the explicit future A5
cipher/decipher seam; training, tail and stealing bits remain outside it.
SACCH/FACCH's shared 184-bit FIRE and convolutional coding is implemented
independently, and FACCH stealing marks the first four `hu` and last four
`hl` flags as TS 45.003 requires.

`make verify-gsm-tch-f-l1` checks a fixed all-zero channel-code result,
codec-bit permutation, clean and degraded speech decoding, unprotected
class-2 corruption, control-channel error correction, diagonal placement,
normal-burst fields, training bits and stealing flags. The current live link
crosses those bit and burst representations through independent transmitter
and receiver state in each direction. A separate exact 60/13 ms timer advances
one assigned timeslot per TDMA frame. For the laboratory assignment on
timeslot 1, FN modulo 26 positions 0--11 and 13--24 advance TCH/F, position 12
is idle, and position 25 is reserved for SACCH/TF. Thus 24 traffic bursts per
120 ms carry exactly six speech blocks while one four-burst SACCH block spans
104 TDMA frames.

Save states preserve the complete generic Layer-1 endpoint state: queued
speech/FACCH blocks and their kinds, both halves of every diagonal
interleaver, burst phase, receiver accumulation, stealing flags, and partial
SACCH blocks. Restoring at a non-block boundary therefore continues with the
same subsequent air bits and decode results; it does not manufacture an
erasure, lose a pending FACCH replacement, or restart the 480 ms SACCH
assembly.

Organic LAPDm blocks carried on channel selector `0xb0` are also copied into
the corresponding uplink or downlink FACCH coder. FACCH has priority over the
next queued speech block, its eight stealing flags identify the replacement
to the independent receiver, and the displaced speech interval is delivered
as an explicit BFI rather than delayed or confused with queue starvation.
The same typed delivery carries protected speech decode failures. Layer 3
still consumes the already recovered Nokia block transaction directly; the
parallel burst path models its Layer-1 consequence without making call-control
timing depend on the HLE air-link decoder.

The paired-ROM `make verify-radio-call-state-roundtrip` gate now checks that
consequence explicitly. After each firmware enables its speech route,
organically produced control traffic decodes as four uplink/four downlink
FACCH blocks on v6.00 and four/three on v5.01. The next handset and network
codec boundaries increase their independent concealment histories, proving
that the stolen intervals became BFIs. Both sides then resume good speech for
more than 100 frames before the firmware-controlled teardown. This is a
causal interruption-and-recovery check, not merely a search for a FACCH log
line.

The air boundary also accepts explicit, generic hard-error profiles. They
operate only on the 114 TCH data bits after diagonal interleaving and before
normal-burst packing, so they cannot alter firmware, codec payloads, training,
tails or stealing flags. `NETCFG` bits `0x40` and `0x80` independently select
laboratory four-burst downlink and uplink fades every six traffic
multiframes. This spacing is intentionally specified in radio units rather
than call/frame contents. FACCH bursts are preserved so the profiles test
speech degradation rather than forcing call control.

`make verify-radio-degraded-speech` proves the complete consequence under both
v6.00 and independently relocated v5.01: the configured fades reach both air
seams, protected speech fails parity after Viterbi decoding, and each BFI
triggers the direction's independent GSM 06.11 substitutor rather than
decoding damaged payload. The network continues encoding downlink audio
across uplink losses, later clean blocks reset both loss sequences, and call
control remains organic. The gate now saves during that active degraded
interval, advances a two-second reference branch, restores, and requires all
34 ordered impairment, Layer-1, codec and peer records to replay exactly on
each ROM. On the canonical restored timelines, v6.00 inverted 36 bursts per
direction, observed 16 impairment-induced downlink bad blocks and retained
295 non-silent earpiece blocks; v5.01 inverted 76, observed 35 and retained
595 respectively. Handset/network concealment totals were 20/5 and 35/5.
Both decoded clean frames after earlier losses and completed organic teardown;
a final fade may legitimately overlap release.

The receive-side requirements and 320 ms maximum muting interval are derived
from [ETSI GSM 06.11 version 3.0.1](https://www.etsi.org/deliver/etsi_gts/06/0611/03.00.01_60/gsmts_0611sv030001p.pdf),
“Full rate speech; Substitution and muting of lost frames for full rate speech
traffic channels”.

The answered-call fixture selects that voice peer. Across both v6.00 and
v5.01, `radio_speech_media_trace_check.py` proves fresh codec state, 20 ms
cadence, at least 100 continuing encoded-uplink and decoded-downlink frames,
the peer's fixed source peak, and non-zero samples in the COBBA receiver
stream. The current paired runs each carried 150 microphone-side frames and
145 network-to-earpiece frames, with all 145 received COBBA blocks non-silent.

This proves a standards-based codec and initial channel-coded media
*transport* slice, a non-silent
decoded downlink to COBBA, and non-silent physical-microphone uplink decoded
at the network peer. It does not yet prove the constituent meanings of
command-`0x08` bits 0 and 9, the MAD2 serial-port edge/idle-clock and register
contract or firmware-selected COBBA analogue routing. SACCH coding,
rectangular interleaving and timeslot coexistence are present. Independent
stateful SACCH transmitters and receivers now wait for the correct
timeslot-dependent phase in the 104-frame cycle; all eight timeslots are
covered, including the wrapped phase ordering for timeslots 2--7. An empty
endpoint transmits nothing rather than manufacturing a measurement report.
Meaningful measurement-report/system-information payload production and
consumption remain to be connected to their Layer-3 owners.

## Unresolved contracts
- Decode the other recv handlers' primitives (`0x23c4fc/55c/9e8/be8/d158/d2fe/d430`) and the
  full `0x2b66b0` routing table (same caveat: dormant, no runtime to validate).
- Static RE of the `0x2b7xxx–0x2c9xxx` L1 driver *send* side: enumerate the 287 DSPIF
  command stubs and their command encodings (large, code never runs on our boot).
- Identify the two downloaded blobs (`[0x200+]` from flash `0x200040`; `[0xe00+]`): DSP
  program vs coefficient tables vs audio codec params.
- The MCUIF (`0x40000`) memory-range configuration.
