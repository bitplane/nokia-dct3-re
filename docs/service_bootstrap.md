# Service-startup bootstrap

> **Address-heavy research reference.** Use `contact_service_topology.md` for
> current contact-command direction and `evidence/state_predicates.json` for
> current readiness gates. This file retains the underlying boot-chain detail.

CONTACT SERVICE is the firmware's response when the service layer cannot complete
startup and its watchdog expires. This document retains the branch-level map of
that failure and the experiments used to isolate it.

> This is a **current-knowledge** reference, not a journal. The full investigation history
> (including conclusions later corrected — the `ack` red herring, "NOT the EEPROM", etc.) lives
> in git history; a terse summary is in the Appendix. Where older commits/wording conflict with
> this file, this file wins.

## Current status

**The contact-service path now completes coherently without a firmware-message
bridge or firmware-state write.** The authoritative transaction and ordering map
is `contact_service_topology.md`. In one boot, the request-driven peer answers the
DSP D0 discovery, returns the type-`0x74` completion for the organic type-`0x70`
request, initiates the external class-`0x40` session through task 7, and
acknowledges the final `0x622a` transaction. Firmware retains service-present bit
6, clears busy bit 2 itself, resumes the extended tasks, and starts normal SIM
traffic.

The old `MODEL_SVC_RESPONDER` and `MODEL_SVC_CHANNEL_DRAIN` discussion below is
**historical branch-isolation evidence**, not the supported path. Its incomplete
header, self-route loop, direct task-2 delivery, and shared-status completion were
useful falsifications but are superseded by `MODEL_DSP_CONTACT_PEER`.

| model (env) | what it emulates | gate it clears |
|---|---|---|
| `NOKI3210_MODEL_DSP_SERVICE` | DSP lower-service handshake: drains pending counter `[0x100e4]`, raises IRQ 4 | `service_ready` |
| `NOKI3210_MODEL_CCONT_PRESENT` | CCONT reg `0xe` bit 0 = present/status | service-channel idx6 |
| `EEPROM_PROFILE=selftest` (overlay) | EEPROM config checksum (`0x244`) + tune/security checksum (`0x11c`) | EEPROM config gate + service-channel idx18 |
| `NOKI3210_MODEL_DSP_CONTACT_PEER` | request-driven DSP and external service counterparty | contact result, busy-bit completion, extended-task resume |

The first three establish the hardware prerequisites. The contact peer supplies
the missing counterpart transactions; firmware then owns all state transitions.

## How CONTACT SERVICE works (the bit-6 gate)

### The halt and its linchpin

CONTACT SERVICE is the **D9 watchdog timeout**: `0x237b2e` increments counter `0x11fed6` each
poll; at `0x0f` it calls `0x2b4dda` (the halt). The counter resets only while ack `0x11fedb != 0`
— but **the ack is a red herring**: it is *never written non-zero anywhere reachable* (only
zeroed at init `0x23471e`). The real control is whether the watchdog **arms** at all, gated by
**service-present bit 6** of the byte at `0x11fed0`:

```
batch-2 resume gate 0x29bafc reads bit 6 of 0x11fed0 (service-present):
  bit 6 SET   -> returns 1: task 0x14 resumes, NO event 0x19, NO watchdog, NO CONTACT SERVICE
  bit 6 CLEAR -> posts event 0x19 (0x29bb1a) -> watchdog 0x237b2e self-perpetuates -> CONTACT SERVICE
```

So **keeping bit 6 set** is the goal; then the watchdog never arms and the ack never matters.

### The four ways bit 6 gets cleared (and the model for each)

The contact-service init `0x2346b2` sets bit 6 (`|0x40` at `0x234758`), then clears it
(`&0xbf`) if **any** of these fail. Plus one later re-clear in the PM path:

**(a) `service_ready 0x110c2c != 1`** — checked via `0x2a8fec` at `0x2347a4`, clear at `0x2347b2`.
The setter `0x291068` writes `[0x110c2c]=1` iff the DSP-shared **pending counter `[0x100e4]==0`**;
it runs only from the IRQ handler `0x2af3ca` (dispatched from `0x2ac794`) on **MAD2 IRQ line 4**,
the DSP service-completion interrupt. The DSP is unemulated and `assert_irq(4)` never happens, so
the setter never runs. Nuance: the ready byte is also reset at the top of the startup function
(`0x290b54` at `0x2a90d6`) each phase, so it must be (re)set *within* the phase that reads it —
i.e. the DSP must signal continuously, not once.
→ **`MODEL_DSP_SERVICE`**: when the MCU writes a non-zero pending count to `[0x100e4]` (pc
`0x290c98` writes `0x0002`), after a short delay drain it to 0 and `assert_irq(4)`, then re-arm at
a service-tick rate (a running DSP raises a periodic per-frame interrupt).

A crossed isolation run proves both operations are necessary. With CCONT present and
the same EEPROM, draining `[0x100e4]` without IRQ 4 is identical to a silent DSP
(`service_ready=0`, soft reset, final event `0x32`); recurring IRQ 4 while leaving
the counter at 2 is also identical. Only drain plus IRQ produces
`service_ready=1`, avoids the reset, and advances the one-second boot to event
`0xc3`. This is the minimum firmware-observable shared-service completion
contract, not a semantic registration reply.

**(b) EEPROM config checksum** — `0x234810` compares `sum16(EEPROM[0x120..0x243])` against the
stored halfword at `EEPROM[0x244..0x245]`; mismatch clears bit 6 at `0x234832`. The firmware
computes `0x1ee1` = `sum16 (0x20df)` minus a correction `EEPROM[0x154]+[0x155]` (`0x2978c0`), and
reads `0x244/0x245` **big-endian-in-word**.
→ **`EEPROM_PROFILE=selftest`** overlay returns `0x244=0x1e`, `0x245=0xe1` → `ldrh=0x1ee1`, match.
The native I2C path now reaches that match organically. A former GenIO integration
bug ANDed released SDA with the stale output latch and yielded
`computed=0x00ff, stored=0x0000, guard=0x0000`; after correcting input/release
semantics, `0x234826` is not taken and bit 6 survives as status `0xcc` until the
separate PM-read clear below.

**(c) service-channel status array** — a loop `0x23487e..0x2348a2` walks 24 bytes at `0x11fc60`;
bit 6 is cleared (`0x234896`) if any entry `[0x11fc60+i]` (i≠11) is not `0x00/0xfe/0xff`. Two are
dirty on a blank phone, each gated by one availability check (clean = `0x00`, else `0xfd`):

- **idx6 `[0x11fc66]`** ← `0x295ebe: bl 0x2afb44` (`ccont_reg_read`, arg `0x9001` = index `0x10`,
  mask `0x01`). The cached shadow byte `[0x11238c]` is `0x70`, so the helper does a **live CCONT
  serial read** (cmd `0x74` → CCONT register `0xe`) and returns **bit 0**. The firmware's own CCONT
  IRQ dispatcher `0x2b08c6` masks bits 0–2 off (`and #0xf8`), so bit 0 is a **present/status bit,
  not an interrupt**. It is also a *fallback*: the primary service-6 detector `0x297540` (a config
  struct) is consulted first. A functional CCONT reports bit 0 set on any phone; the emulation
  never did.
  → **`MODEL_CCONT_PRESENT`**: report CCONT reg `0xe` bit 0 set (read-time only; the dispatcher
  ignores it, so it doesn't perturb the IRQ latch).

- **idx18 `[0x11fc72]`** ← `0x295ea4: bl 0x264c56`, which reads `0x120` bytes and checks
  `sum16(EEPROM_cache[0x000..0x11d]) == 32-bit big-endian word[0x11c]`
  (sum16 = `0x2a41d0`). The checksum field overlaps the final two summed bytes;
  those bytes are zero in the fixture. Computed `0x1ae4` (exactly `0x11c × 0xff`
  — the erased region) vs stored `0xffff` (virgin EEPROM).
  → **`EEPROM_PROFILE=selftest`** computes and supplies `0x11c..0x11f = 00 00 1a e4`.
  The init re-checks at `0x234796` and stamps `0x12` on failure.

**(d) the PM-read re-clear `0x237b04`** — even with (a)–(c) satisfied (bit 6 builds to `0xcc` and
survives the init), it is cleared again at `0x237b04` (`lr=0x2b13b0`, the PM service-read validity
return) when the `0x5f00` read is **dropped** (channel disabled). Enabling the service channel
removes this clear (and the watchdog timeout) — but completion still needs the response. This is
the remaining gate, below.

## Historical branch-isolation responder

> **Correction:** the responder reaches the firmware's result-5 branch and lets the boot leave the
> initial CONTACT SERVICE frame, but it does not complete a coherent service transaction.
> **How it worked:** at the contact-service loop top (`0x237bc6`), a flash
> trampoline drove the firmware's allocation and post primitives to deliver a
> synthetic response. The loop then
> `recv`s the posted message and dispatches it (`[3]=0x40`→`0x237400`, `[8]=0x64`→`0x236dc4([9]=0x05)`
> → substate 5, HEALTHY), which drives the downstream startup branch. Default boot
> (responder off) remains unchanged, but the model is an isolation bridge rather
> than a validated peer implementation.

The missing header fields are behaviorally significant. In a six-second trace the
injected allocation `0x101b60` was consumed once and freed. The firmware-generated
status frame then acquired route selector `1` through task 7 and returned to task 2,
where fresh allocations alternated between `0x101b60` and `0x101b88` while result
`5` re-entered `0x236dc4`. Crossed runs with the separate drain merely moved the
loop earlier. The responder must not be treated as proof that an ordinary standalone
boot receives this command from a service box.

The extended-task gate makes service report `0x622a` through the shared report
entry `0x2b13d4` (`lr=0x29bb05`). The transport recognizes that specific code,
waits one microsecond, and signals channel-empty/service-present through its
callback. Tracing all calls to `0x2b13d4` was essential: it carries many report
families, so treating every call or every earlier busy-bit edge as this request
is incorrect.

The contact-service reads logical address **`0x5f00`** (count 2) from destination **node `0x18`**
(`[0x11fee5]`) every ~9 ms (the D9 watchdog tick). Two things are missing, and both reduce to
the lower service transport returning a coherent response. The command census in
`contact_service_topology.md` shows that task 7 is the immediate MCU boundary and that contact
responses address the learned external service node. Node `0x18` is the PM-read destination;
it should not be conflated with every class-`0x40` command id or address.

**1. The channel is never opened.** The validity check `service_channel_lookup_2b12b4` requires the
master enable `0x11fee4 != 0` **and** the address registered (ROM bit-table `0x2e2f5c` gated by RAM
mask `0x11ff08`). `0x11fee4` is only ever *reset* (`service_channel_reset_2b13c0`), never set, so
reads are dropped (`0x2b13a2` returns without transmitting). The only writer of `0x11fee4` is the
channel-open `0x2b140a` (`strb r1,[0x11fee4]`; if `r1 != 0` it also copies the registration block).
Its four callers (`0x2366f6`, `0x23672c`, `0x236e6c`, `0x236f10`) are **all contact-service
message-command handlers** — cases in the dispatch switch at `0x2377fc` and inside the response
dispatcher `0x236dc6`. They process a *received message struct* (`0x2366c8` checksums its payload
`[r0+9..+0x40]`). So **channel-open is response-driven, not a local write**: a real phone opens the
channel after receiving a registration message from the external service peer through the lower
service transport.

**2. The contact-service never *completes*.** The healthy completion is the async response
dispatcher **`0x236dc6`** with command **`0x05`** (`0x236efe` maps result `5` → substate 5,
HEALTHY). `0x236dc6` has **no static references** (computed dispatch only) — it runs solely when a
correctly-formatted **response message** is received and routed. The request is represented on the
firmware's **internal message bus** (frame starts `0x00`, not the MBUS-serial `0x1f`). The old
investigation incorrectly inferred that the response therefore had to be injected there; the
coherent model shows task 7's lower transport produces that internal representation. The
*synchronous* `0x5f00` read (D9 watchdog dispatch `0x237994`,
`value-0xd9` jump table) only steers watchdog sub-handlers, never completion.

**Proven (with the models on):** forcing the channel enable (`EXPERIMENT_FORCE_SVC_CHANNEL`) makes
the reads transmit and **stops the D9 watchdog timing out** (`D9TIMEOUT` 1→0, the `0x237b04` clear
gone) — but the frame stays `d8a9a7`: watchdog-satisfied ≠ completed. No local provider answers
(`svc_response` count 0 in those configurations); the required lower-service behavior is absent
from the driver.

**Historical result:** the deleted responder supplied one header-incomplete result-5 message at
the task-message boundary. It neither modeled the lower-service session nor converged after the
firmware's status response. Only its branch evidence is retained.

**Response message spec (fully traced).** The contact-service task loop `0x237bb4` calls
`recv 0x26a458` → `r4` = message, then dispatches on **`[msg+3]`**; `[msg+3]==0x40` →
`0x237c70` → the sub-dispatcher **`0x237400`**, which dispatches on **`[msg+8]`**; `[msg+8]==0x64`
→ `0x23785e` → **`0x236dc4([msg+9])`**; `[msg+9]==0x05` → result 5 → **substate 5 (HEALTHY)**. So a
message `{[3]=0x40, [8]=0x64, [9]=0x05}` delivered to the contact-service task **completes it**. (The
registration message that opens the channel is the same mechanism — a different `[msg+8]` command
routing to `0x23670c`/`0x2366c8`, which checksums its payload `[msg+9..+0x40]`.)

**Injection mechanism (mapped).** Messages are per-task: a TCB (stride `0x1c`) holds a ring of
message **pointers** at `[TCB+0xc]` indexed by tail `[TCB+0x10]`. `alloc = 0x26afe0(size)`;
`post = sched_post_task_message_26a204(task, msg)` (ring write at `0x26a2a2` + wake); `recv =
0x26a458`. **Crucially, the contact-service frees the message after dispatch** (`0x26abf8(msg)` at
`0x237c8e`), so an injected message must be a *real pool buffer*, not scratch RAM.

Using a real pool allocation fixes buffer ownership but is not sufficient for faithful delivery.
A real model must observe the initiating boundary, preserve address/route/sequence metadata, and
let firmware-generated reports terminate at the peer rather than route back to task 2. Directly
posting the three discriminating bytes is an isolation probe, not the remaining faithful build step.

## Historical post-responder frontier

With the responder (and the historical model stack), the boot **left CONTACT SERVICE** and advanced
`0001 → 000d`, where that experiment **held**: the LCD cycled a white fill (`94a2dc`) / black fill
(`4aab13`) display-init pattern. (The diagnostic forces `EXPERIMENT_DSP_IRQ4 EXPERIMENT_FORCE_ACK`
reach the same limp by brute force; the responder reaches it *faithfully*.) The post-injection PC
trace runs `0x236dc4` → `0x2af3ca` → `0x291068` (service-completion) then grinds the **sum16 loop
`0x2a41de`** heavily — the service-startup re-running. PC sampler hot spots: `memset` `0x2b65e4`
(the fills), sum16 `0x2a41d0`/`0x2a41de`, a render routine `0x25exxx` (4-bit type field at
`0x25e682`), service-startup `0x290a94`. Task 0x14 (batch-2) also needs the startup **phase byte
`0x112449` ∈ {0,2}** (then observed as `01`). At that point the next historical frontier was the
display-init/readiness re-run. (Long runs with the responder were slow to
emulate because this re-run is checksum-heavy; short windows complete and show the `94a2dc`/`4aab13`
fills.)

**Mode-`000d` advance gate (resolved).** The boot is **not frozen**:
the startup/charger state machine runs a live loop every ~80 ms. The mode dispatcher is a **14-entry
jump table** keyed on `FW_STARTUP_MODE`: the struct base `r4 = FW_STARTUP_DISPATCH_STATE (0x1123ec)`,
so `[r4+2] = FW_STARTUP_EVENT (0x1123ee)` and `[r4+4] = FW_STARTUP_MODE (0x1123f0)`; mode `000d`
dispatches to **`0x270e22`**. That branch is a **flag-accumulator wait**: it reads `FW_STARTUP_EVENT`
(computed by the message→event translator `0x26ff14`, then written at `0x270e20`) and, for events
`0x14/0x16/0x15/0x17`, OR-s bits `1/2/4/8` into the
**flag byte `[0x112399]`**. Mode `000d` **advances** (`0x270edc`) only when **both** hold:

1. `FW_CCONT_STATE [0x11ff6c]` low nibble `== 6` — **satisfied** (always `06`).
2. flag byte `[0x112399]` low nibble `== 0xf` — i.e. all four sub-events `0x14/0x15/0x16/0x17`
   delivered as `FW_STARTUP_EVENT`.

The former `0x08`–`0x09` stall was an emulation artifact. CCONT register `0xe` had been reset to
upper interrupt bit `0x08`, and every status bit asserted the IRQ output. Correct cold-boot PWRONX
status `0x02` plus upper-source mask `0xf8` lets the ROM deliver all four events and advance to
mode `0x0004` organically. The former battery-event RAM rewrite and event-delay literal override
are removed. The checklist now progresses `0x08 -> 0x09 -> 0x0b -> 0x0f` without either shim.

**Gate confirmed (`EXPERIMENT_FORCE_000D_EVENTS`).** Injecting `0x16` then `0x15` at the dispatch
write (`0x270e20`) completes the flag byte to `0x0f` and **mode `000d` advances → `0004`
(POST_SELFTEST)** at t≈0.39 — the LCD leaves the white/black limp and renders the **battery-present
idle screen** (frame `4235fa`: battery indicator top-right, no network/SIM). So `000d` is cleared in
principle; **`0004` is the next gate.** (The injection is a *diagnostic scaffold*, not a faithful
model — opt-in, oracle-preserving.)

**Producer.** The events `0x15`/`0x16` are posted by the **CCONT interrupt
service routine `0x2b08c6`**: it reads the CCONT interrupt-status register (`ccont_reg_read 0x2afb44`),
masks the active upper bits, and posts `0x15` (any upper bit) / `0x16` (charger bit) via `0x2697aa`.
The translator `0x26ff14` has identity cases for all four sweep events. Earlier routing-gap claims
were observations of the incorrect CCONT reset lifecycle, not a missing RTOS subscription.

### The startup machine is a chain of event-gated modes (scaffold-march, `EXPERIMENT_SCAFFOLD_MARCH`)

The mode dispatcher (`0x270c84`→jump table) is a state machine keyed on `FW_STARTUP_MODE`. Each mode's
branch reads `FW_STARTUP_EVENT` and advances on a specific event:

| mode | advances on |
|---|---|
| `000d` CHARGER_WAIT | flag-accumulate `0x14`+`0x15`+`0x16`+`0x17` |
| `0004` POST_SELFTEST | `0x07` (BATTERY_READY) |
| `000b` POST_CHARGER | `0x07` |
| `0009` BATTERY_WAIT | `0x0e`/`0x02` |
| `000c` | `0x04`/`0x06`/`0x0b`/`0x0d` (sub-states) |
| `0005` READY_GATE | `0x06` |
| `0006` SERVICE_QUIESCE_GATE | `0x03`/`0x11` |
| `0007` BATTERY_READY_GATE | `0x07` to enter, then a **nested sub-loop** (`0x271392`) spins for `0x74` |

`EXPERIMENT_SCAFFOLD_MARCH` injects each mode's advancing event at the dispatch write
(`pc∈[0x270000,0x271600]`, `addr==FW_STARTUP_EVENT`). It marches the boot **`000d → 0004 → … → 0007`**
(through the event-wait modes) — but **stalls at `0007`**: past the mode-level dispatch the handlers
open **nested sub-state waits** for *specific* events at *specific* PCs (e.g. `0x74` at `0x271392`),
and beyond those the boot stops posting startup events entirely (≤17 posts, all by t≈0.36), sitting in
deeper subsystem-specific spins. The display falls back to the white/blank state (`94a2dc`).

**Conclusion (end-of-phase finding):** the *early* startup modes are pure event-delivery gaps and are
forceable; reaching idle is **not** cheaply scaffold-able — past `0007` it becomes an open-ended
sequence of subsystem-completion signals (CCONT measurements, display-init readiness, the `0x74`
completion, then the readiness predicates `0x2a92fc`). So "boots to idle" needs the **faithful
subsystem event models**, not more scaffolding — that is the re-plan. The visual high-water mark so far
is the **battery-present idle screen** (`4235fa`) reached at mode `0004`. `EXPERIMENT_SCAFFOLD_MARCH`
is a rough diagnostic (some injected events are best-guesses), opt-in, oracle-preserving.

## Reference

### Key addresses

| addr | role |
|---|---|
| `0x237b2e` | D9 watchdog (counter `0x11fed6`, halt at `0x2b4dda`) |
| `0x11fed0` | service-present byte; **bit 6** is the linchpin |
| `0x11fedb` | ack (red herring — never written non-zero) |
| `0x2346b2` | contact-service init: sets bit 6, then the clear checks |
| `0x2347a4`/`0x2347b2` | service_ready check / clear-bit-6 |
| `0x234810`/`0x234832` | EEPROM config checksum check / clear-bit-6 |
| `0x23487e..0x2348a2` (`0x234896`) | service-channel array loop / clear-bit-6 |
| `0x237b04` | PM-read re-clear of bit 6 (dropped `0x5f00` read) |
| `0x110c2c` | `service_ready` byte (setter `0x291068`, needs `[0x100e4]==0`) |
| `0x2af3ca` | IRQ handler; IRQ line 4 → `0x291068` |
| `0x100e4` | DSP-shared pending counter (MCU writes `0x0002` at pc `0x290c98`) |
| `0x11fc60` | 24-byte service-channel status array (idx6=`0x11fc66`, idx18=`0x11fc72`) |
| `0x2afb44` | `ccont_reg_read` (idx6: arg `0x9001` → CCONT reg `0xe` bit 0) |
| `0x2b08c6` | CCONT IRQ dispatcher (masks bits 0–2; proves bit 0 = status) |
| `0x264c56` / `0x2a41d0` | idx18 EEPROM checksum / the `sum16` primitive |
| `0x244..0x245`, `0x11c..0x11f` | EEPROM stored checksums (config / tune+security) |
| `0x5f00` | PM logical address the contact-service reads (cmd) |
| `0x11fee4` | service-channel master enable (writer `0x2b140a`, reset `0x2b13c0`) |
| `0x2b12b4`/`0x2b13a2` | channel validity / request-if-enabled |
| `0x236dc6` | async response dispatcher (cmd `0x05` → HEALTHY; computed-dispatch only) |
| `0x237994` | D9 watchdog sync `0x5f00` dispatch (`value-0xd9` jump table) |

### Frames

**MBUS D0 startup query** (task07→task08, reg `0x1a` byte stream), `1f`-framed serial:
```
1f ff 00 d0 00 01 01 01 31
│  │  │  │  └──┴─ len=0x0001 │  │  └ XOR checksum (1f^ff^00^d0^00^01^01^01 = 0x31)
│  │  │  └ cmd 0xD0          │  └ seq
│  │  └ src 0x00 (phone)     └ payload[0]=0x01
│  └ dest 0xff (test box)
└ frame start
```
Delivery path (now superseded by `MODEL_DSP_SERVICE`, kept for reference): the RX state machine
`0x2aae76`/`0x2b052e`/`0x2aaf44` assembles the reply and posts it to the task-08 frame handler
`0x283d6e`, which advances the lower-service state. That thread fed `service_ready`; the model
now supplies `service_ready` directly, so the D0 reply itself is no longer on the critical path.

**Transport correction (current 3210 v6.00 trace):** the live task-8 path does
not reach the MAD2 serial register for this D0 exchange.  Task 8 wraps the frame,
task 3 writes DSP shared TX-ring packet type `0x05`, and the inverse path is DSP
RX-ring packet type `0x8e` followed by FIQ0 -> task 4 -> task 8.  A
request-derived state-`1` D0 response now completes that path and reaches task 7
without an RTOS post or firmware-state write.  It establishes lower-transport
discovery only; the later contact-service header/session is learned separately
from an incoming class-`0x40` frame at `0x237c70`.  The serial-register account
below is retained as historical evidence from the earlier hook/model and must
not be used as the current transport specification.

The contact busy flag has a separate, now-recovered DSP transaction. After
contact initialization posts static task-3 object `0x2db250`, the MCU TX ring
contains type `0x70`, payload `0d 00`. The healthy peer response is RX type
`0x74`, payload `0d 00`; `0x29bc00` translates it into the class-`0x74` task-2
message consumed at `0x234954`. Its command-`0x0d` path clears
`[0x11fed1]` bit 2 at `0x2349dc` while retaining bit 6. This supersedes
the deleted direct-drain experiment for the coherent contact-peer profile: the firmware
performs the flag transition itself, and the later `0x622a` transaction needs
only its correlated transport acknowledgement.

**`0x5f00` service request** (internal message bus, captured at `0x2b0482`):
```
00 [node] 00 00 00 0a 00 01 5f 00 [seq][seq] [ctr] 02 00 [d9/da] ...
   node@+1 (from 0x11fee4)         addr 0x5f00@+8/9   count@+0xd   watchdog selector
```

**DSP shared RAM** (`0x10000–0x10fff`; DSP unemulated, `dsp_ram_r` stubbed):
`0xe4`=pending counter (`0x0002`), `0xa4/a6`=status/version, `0xda/e2`=channel counts/ptrs,
`0xe0`=busy flag, `0xfe/0x100`=ready flags, DSPIF API reg at `0x30000` (stub).

### Historical instrumentation

The force and broad trace controls used to establish this map have been
deleted. The supported coherent profile uses `MODEL_DSP_SERVICE`,
`MODEL_CCONT_PRESENT`, the generated EEPROM fixture, and
`MODEL_DSP_CONTACT_PEER`. Current contact observations use the bounded
`TRACE_CSCMD` and `TRACE_DSP_BOUNDARY` taps.

## Bus-level view (actors, timeline, dead-end)

Reconstructed by wiretapping the scheduler message/event bus (`TRACE_BUS=1`): `post_task_message`
(`0x26a204`/`0x26a354`), `event_post` (`0x2697aa`), `event2` (`0x2698e4`), `resume` (`0x269c6e`),
`recv` (`0x26a458`).

| task | role | runs? |
|---|---|---|
| `0x00` | main startup — resumes others, mode-`0x000d` readiness loop | yes |
| `0x01` | service/event loop — spins event `0x03` | yes |
| `0x02` | contact-service — D9 watchdog (event `0x19`) → CONTACT SERVICE | yes |
| `0x07` | lower-service / MBUS — sends the D0 startup frame | yes |
| `0x05` | service task — receives 3 messages, **never resumed** | no |
| `0x08` | lower-service frame processor — receives the D0 frame, **never resumed** | no |

`0x00/01/02/07` are the core batch (resumed at mode 1); `0x05/08/0x14`… are the extended batch,
deferred by the resume gate. **The dead-end:** messages to `0x05`/`0x08` pile up undrained (they're
deferred), so the service handshake never completes → service never reports ready → task 0 readiness
loop stalls → task 2's D9 watchdog free-runs to timeout → CONTACT SERVICE. Abridged timeline: mode
`0001` resumes 00/07/01, sends the D0 frame (t≈0.251); mode flips to `000d` (t≈0.332); task 02's D9
watchdog ticks every ~9 ms from t≈0.405 and fires ~t≈0.54.

See also `eeprom_analysis.md` (block/checksum layout), `static_branch_map.md` (the resume-gate
branches), `driver_structure.md` (how the models/probes live in the driver).
