# The MAD2 MCU↔DSP interface (DCT3 / Nokia 3210)

A map of how the ARM7 MCU talks to the on-chip **DSP** (which runs the GSM Layer-1
baseband + audio codec) in the Nokia 3210 firmware. Built from static disassembly
(`tools/disrom.py`) + live tracing (`NOKI3210_TRACE_DSPIO`, which logs first-touch of
every DSP shared-RAM offset and DSPIF register with direction/value/PC over the boot).

**TL;DR for emulation:** at boot the MCU treats the DSP interface as (a) a RAM
self-test it passes by echo, (b) a handful of "DSP ready" status flags, (c) a
download target for coefficient/program blobs, and (d) a lower-service transmit
queue. The current model drains that queue and raises IRQ 4 but does not construct
reply payloads. A live D0-bearing packet proves MCU-to-DSP traffic is reachable;
it does not yet prove that a DSP reply is the missing SIM-registration predecessor.
With stateful SIM initialization, radio control reaches service commands `0x30` and
`0x32`; the reply contract for those commands is the current frontier.

## The two hardware windows

| MMIO | region | driver | boot usage |
|---|---|---|---|
| `0x10000–0x10fff` | **DSP shared RAM** (0x800 halfwords) | `dsp_ram_r/w` (stub, real backing store `m_dsp_ram`) | heavy — self-test, config, blob download |
| `0x30000–0x30003` | **DSPIF** control register | `mad2_dspif_r/w` (stub: reads 0, writes no-op) | early initialization plus reachable command-4 doorbells from `0x290cf4`; wider L1 use remains unmapped at runtime |
| `0x40000` | MCUIF (memory-range config) | `mad2_mcuif_r/w` (stub) | early config |

The atlas counted ~444 references to the shared-RAM base and 42 pool-literal references
to DSPIF across the image. Earlier boot-only traces saw just the initialization writes,
but a coherent stateful-SIM run reaches `0x290cf4` with service commands `0x30` and
`0x32`. That function updates DSP shared control words, writes command 4 to DSPIF at
`0x29103c`, and rings doorbell byte 2 at `0x20008`. DSPIF is therefore a live boundary,
not a static-only future path. The DSP-owned completion data and firmware IRQ-4 parser
still need to be mapped before implementing a reply model.

## Shared-RAM layout at boot (`0x10000` base; offsets are byte offsets)

| offset | what | evidence |
|---|---|---|
| `[0x00–0x24]` | **self-test / RAM echo region** | written with a walking pattern at `0x295f48`, read back + compared at `0x295fc0`/`0x295fd6`. A RAM test of the shared window; passes trivially against a real backing store. |
| `[0x00–0x04]` | **DSP status fakes** → `0x01` | `dsp_ram_r` HACK; the firmware reads these as "DSP alive". |
| `[0xe0]` → `0x00`, `[0xfe]`/`[0x100]` → `0x01` | **DSP ready/busy flags** | HACK fakes; `0xe4` = lower-service pending counter (MCU writes `0x0002` at `0x290c98`; `MODEL_DSP_SERVICE` drains it + raises IRQ 4). |
| `[0xf6–0x102]` | **config words** (`0x0100 0x0300 0x0001 0x0000 0x0001 0x0001 0x0200`) | 7 individual writes at `0x290a44–0x290a64`. |
| `[0x200–0x600]` | **coefficient/parameter table** (512 halfwords) | strided copy at `0x290a94`: reads one halfword per 0x20-byte record from flash `0x200040`, packs into `[0x200+]`. |
| `[0xe00+]` | **second blob** (~240+ halfwords) | ARM block-copy (`stmia`) at `0x2b5bd0` (reached via `bx pc` ARM-mode switch). |

So the MCU **stages DSP tables/program from flash into shared RAM** at boot — the DSP's
data *is* in the image. On a real phone the DSP would then execute/consume it; with the
DSP stubbed the blobs just sit in `m_dsp_ram`, harmless.

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

This task's *housekeeping* use (SIM/CCONT/scheduler mailbox) is what keeps it alive; the
Whether the `0x30`/`0x32` completion returns through task 22 or only through shared control
state is unresolved. Use `NOKI3210_TRACE_DSP_BOUNDARY` for this boundary; the retired broad
`TRACE_DSPMSG` history should not be restored without a specific mailbox hypothesis.

## What the reachable boot currently proves

`TRACE_DSPIO` shows **no MCU reads of DSP-computed result words** beyond the self-test
region and hardcoded ready flags. The MCU nevertheless queues lower-service packets in
the shared transmit ring. The first captured pair is:

```text
00 02 0a 05 1e ff 00 d0 00 03 01 01 e0 00
08 05 1e 14 00 f4 00 01 03 00
```

The model currently acknowledges these only by zeroing the pending count and raising
IRQ 4. It is enough to reach "Insert SIM card", but it is not a complete DSP peer.
No derived response format or receive-ring transition has yet connected this traffic
to `0x05ea`, task-15 `0x07dd`, or the SIM registration result, so treating the D0 packet
as that request would be speculation.

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

For type `0x70`, `0x29bc00` preserves the type as the firmware message class
and posts the message to task 2. Task 2 has no direct class-`0x70` case: its
fallback passes the first payload byte to `0x237960`, which records an
unknown-response notification. It does not unwrap a nested generic-service
object. The direct DSP translator `0x282d64` handles classes 3/5/17/47, but its
class-5 primitive set starts at `0x11` and does not include the task-15
registration primitive `0x0b`. These are distinct protocols despite sharing a
numeric class value.

The old DSP model leaves TX consumer `0x0a6` at zero. The producer reaches `0x34`,
free space collapses, and later firmware packets cannot be queued. The opt-in
`MODEL_DSP_RING_DRAIN` candidate advances the DSP-owned consumer across complete
packets on each service tick. It models consumption only: it writes no inbound packet
or firmware state. The newly visible coherent stream is:

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

The wider **bidirectional L1 protocol** — MCU sends "search/sync/measure/attach", DSP returns
cell/RSSI/registration — lives largely in the `0x2b7xxx–0x2c9xxx` driver and is not yet
observed carrying task-22 messages. The narrower shared-control path at `0x290cf4` is live,
however, so DSP work is no longer wholly static: its `0x30`/`0x32` request and IRQ-4 completion
contract can be traced at the current boot frontier.

## Emulation feasibility & the dependency re-ordering

- **To reach where we are:** the current echo, ready flags, queue drain, and IRQ model are
  sufficient, but they are not a completed DSP contract.
- **The DSP is now the leading bounded frontier, not yet a proven keystone.** Organic radio
  initialization issues DSP service requests immediately before the absent lower-radio result.
  Mapping their completion contract is justified; fabricating an L1 reply before that mapping is not.
- **For the network (operator name + signal):** a message-boundary DSP stub (answer the L1
  commands with "camped on a fake cell, operator X, RSSI y") is feasible *in principle* and
  is the right MAME approach — but it is **doubly blocked**: (1) the L1 protocol is
  static-only until coherent boot reaches it, so we can't observe the exact handshake to
  stub it; (2) it is behind the same MMI/coherent-boot wall. Signal RSSI itself is separately
  injectable (CCONT ADC ch1, `network_scouting.md`).
- **Full DSP-core emulation** (a TI Lead core running the downloaded blobs) is a much larger
  project and would still need a faked air interface; not warranted given the above ordering.

**Net:** the reachable MCU↔DSP interface exposes both an incomplete queue-acknowledgement model
and live shared-control commands. Neither has yet proved the exact reply that blocks SIM
registration. The next pass should map command `0x30`/`0x32` completion through IRQ 4 using
`NOKI3210_TRACE_DSP_BOUNDARY`, then decide whether a DSP peer device is warranted.

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
