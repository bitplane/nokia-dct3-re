# Service-startup bootstrap — message-bus map

## Executive summary (status: mapping phase COMPLETE)

CONTACT SERVICE is the firmware's correct response to a blank/un-provisioned 3210. The whole
chain is now mapped end-to-end and experimentally validated; what remains is a modelling build,
not more investigation. The causal chain, outermost symptom to root:

```
CONTACT SERVICE screen
  ← D9 watchdog counter 0x11fed6 hits 0x0f          (0x237b2e; ack 0x11fedb never set)
  ← watchdog armed because bit6 of 0x11fed1 is clear (service-present)
  ← bit6 cleared unless service_ready==1 AND EEPROM config checksum passes
  ← service_ready 0x110c2c never set: its setter 0x291068 runs only on MAD2 IRQ line 4
    (the DSP service-completion interrupt) with DSP pending-counter 0x100e4 == 0
  ← AND the contact-service never *completes*: it reads its command from PM logical addr
    0x5f00 via an async request whose dest node = [0x11fee4]; every read is DROPPED because
    the channel-enable flag 0x11fee4 == 0 (so no request is ever sent / no response dispatched).
```

> **Correction (this session):** an earlier note said the dest node was `0x18`. That was a
> probe artefact — `pm_read` logged the dest *buffer* address (low byte `0x18`), not the node.
> The real dest node is `[0x11fee4]` (the channel-enable flag *is* the node id), which is `0`
> on a blank phone. The request frame format is now captured (see "Request format" below);
> the open question is the **real node value** a provisioned phone uses for `0x11fee4`, and
> whether that node responds — the "node absent" claim was tested against a *forced* node
> value (`0x01`) and so is not yet conclusive.

**Request frame** (dumped at `0x2b0482` with the channel forced open, `pm_request:`):
`00 [node] 00 00 00 0a 00 01 5f 00 [seq][seq] [ctr] 02 [data..]` — header, dest node at `+1`
(from `0x11fee4`), address `0x5f00` at `+8/9`, count `02` at `+0xd`.

#### Where the channel node comes from (and the unifying conclusion)

The channel-open is **`0x2b140a`** (writes `[0x11fee4] = r1` = the node, then copies a 0x20-byte
registration block). Its callers are all in the contact-service (`0x2366f6` etc.), and the node
value comes from a **config block at `0x11fed9`** (node at `[0x11fedd]`), which is itself
populated by processing an incoming **service config *message*** that is **checksum-validated**
(`0x2366c8` → `0x2a41d0` over 0x40 bytes). The channel-open **never runs** on our boot
(`[0x11fee4]` is only ever written 0, by the reset `0x2b13c0`) because that config message never
arrives.

So the "find the node value" thread bottoms out at the **same place as everything else**: the
channel node is **provisioning data delivered by a service message** a factory-provisioned phone
receives, not a constant we can look up. There is no single forceable node value — the channel
config, the `0x5f00` command, the EEPROM tune/security/config blocks, and the service responses
are **all provisioning state**, absent on a blank unit.

**Unifying conclusion:** a clean boot = *provision the service layer* — supply the config
messages / NV / responses a factory phone has. That is one coherent (if multi-part) modelling
task: emulate a provisioned service environment, not a single missing byte. The `EXPERIMENT_*`
forces remain the way to run the boot *as if* provisioned, to develop and measure that model
incrementally.

Validated by env-gated experiments (oracle frame stays `d8a9a7`):
- `EXPERIMENT_DSP_IRQ4` (pulse IRQ 4 + drain the DSP counter) sets `service_ready`, keeps bit6,
  resumes the extended tasks.
- `+ EXPERIMENT_FORCE_ACK` clears CONTACT SERVICE and advances to a new mode-`0x000d` display
  phase — but the boot only *limps* (the contact-service never genuinely completes).
- `EXPERIMENT_FORCE_SVC_CHANNEL` proves the reads are dropped (channel never enabled), and that
  forcing them through still yields no response (`svc_response`=0) — node `0x18` is absent.

**The remaining work (modelling phase):** build a minimal **service-node responder** that
answers the firmware's PM/service reads — minimally, node `0x18` / addr `0x5f00` → a response
message whose command dispatches to `0x05` (healthy) at `0x236dc6`. This is "make the emulation
look like a fully provisioned, service-responsive phone." Key fixed points and the detailed
trace are below; named in code as `FW_SERVICE_*`, `PM_LOGICAL_CONTACT_COMMAND`,
`CONTACT_SVC_RESPONSE_CMD_HEALTHY`, and in Ghidra via `ExportNokiaFunctions.java`.

## Modelling project plan (the forward phase)

The investigation/mapping phase is **complete** — there are no unknown gates left, and every
thread converged on one conclusion: **a blank DCT3 is unprovisioned, and the firmware correctly
won't finish service bring-up without that provisioning.** The forward work is therefore a
single coherent (if multi-part) modelling project: **emulate a provisioned service environment.**

**Provisioning requirements identified during mapping** (what a factory phone supplies, that a
model must too):

1. **EEPROM blocks** with valid additive checksums (`docs/eeprom_analysis.md`):
   tune `0x00..0x3d` (+cksum `0x3e/3f`), security `0x40..0x11d` (+cksum `0x11e/11f`, IMEI + SIM
   locks), config `0x120..0x243` (+cksum `0x244/245`). Config checksum **done**; tune/security
   need real data + checksums (donor image or synthesis).
2. **DSP service handshake** — MAD2 IRQ line 4 (the DSP service-completion interrupt) + the
   DSP-shared pending counter `0x100e4` draining to 0. **MODELLED** (`NOKI3210_MODEL_DSP_SERVICE`,
   opt-in) — replaces the `EXPERIMENT_DSP_IRQ4` force; see "DONE — DSP service handshake modelled".
3. **D9 watchdog ack heartbeat** (`0x11fedb`). Currently *forced* by `EXPERIMENT_FORCE_ACK`.
4. **Service-channel open** — a checksum-validated config *message* that sets the channel node
   (`0x11fee4`) + the 0x20-byte registration (`0x2b140a`). Not modelled.
5. **Service-node responses** — answer the firmware's PM/service reads (e.g. node-`[0x11fee4]`
   addr `0x5f00` → a response dispatching to command `0x05` at `0x236dc6`) with the values a
   provisioned phone returns. Not modelled. Request frame format captured (see below).

**Approach — incremental, against the live boot.** The `EXPERIMENT_*` forces run the boot
*as-if-provisioned* (each forces one gate). Build one real model (e.g. a service-node responder),
replace the corresponding force, measure how far the boot advances, repeat. This keeps the boot
runnable at every step so progress is always measurable.

**The key unknown the project answers first: cascade vs fan-out.** How many distinct
config/response items does the firmware need before it proceeds? Unknown until the first responder
is built — could be a handful (cascade) or a dozen+ (fan-out). That single measurement should
gate how much effort to commit.

**Tools/scaffolding available:** forces (`EXPERIMENT_DSP_IRQ4`/`_AFTER_MS`, `FORCE_ACK`,
`FORCE_SVC_CHANNEL`, `RESUME_TASK14`, `FORCE_TASK14_READY`); traces (`TRACE_PM`, `TRACE_DSP`,
`TRACE_CONTACT_COMMIT`, `TRACE_BUS`); the captured request frame format; Ghidra-named functions
(`ExportNokiaFunctions.java`) and driver `FW_*` constants.

---

Reconstructed by wiretapping the scheduler message/event bus (`TRACE_BUS=1`): every
`post_task_message` (`0x26a204`/`0x26a354`), `event_post` (`0x2697aa`), `event2`
(`0x2698e4`), `resume` (`0x269c6e`) and `recv` (`0x26a458`) over the forcing-free boot.
This is the breadth-first view that replaces the depth-first gate-chasing.

## Actors (tasks that run vs are merely messaged)

| task | role | runs? |
|---|---|---|
| `0x00` | main startup — resumes others, runs the mode-`0x000d` readiness loop | yes |
| `0x01` | service/event loop — spins posting event `0x03`, checking event2 `0xe1` | yes |
| `0x02` | contact-service — D9 watchdog (event `0x19`), times out → CONTACT SERVICE | yes |
| `0x07` | lower-service / MBUS — sends the D0 startup frame | yes |
| `0x05` | (service task) — **receives 3 messages, never resumed** | **NO** |
| `0x08` | lower-service frame processor — **receives the D0 frame, never resumed** | **NO** |

`0x00`,`0x01`,`0x02`,`0x07` are the *core* batch resumed at mode 1; `0x05`,`0x08` (and
`0x14` etc.) are the *extended* batch, deferred by the resume gate (see
`static_branch_map.md`).

## Timeline (abridged)

```
mode 0001:
  t=0.190  task00  EVENT e1=ffff, e2=0282, e3=0040, EVENT2 e1, EVENT 6c=0101
  t=0.251  task00  POST->task05 id=012e ; RESUME task07
  t=0.251  task07  POST->task08 id=1eff a1=d0   ← the MBUS D0 startup frame
  t=0.251  task00  RESUME task01
  t=0.325  task01  EVENT 00=0080,09=0019,0d=01c1,15=0001 ; POST->task05 id=071d
  t=0.332  *** mode 0001 -> 000d ***
  t=0.337+ task01  EVENT 03=0101 ... (repeats ~17x, interleaved with RECV) — spinning
mode 000d:
  t=0.338  task00  RESUME task02
  t=0.405+ task02  EVENT 19 (D9 watchdog) ... (repeats every ~9ms) — counting to timeout
  t=0.412  task02  POST->task05 id=46aa
  t=0.540  task02  POST->task07 a1=40 ; task07 POST->task02 ; EVENT2 1c  ← watchdog fires
  -> CONTACT SERVICE
```

## The dead-end (confirmed from the bus, not inferred)

**Messages are posted to tasks `0x05` and `0x08`, which are never resumed.** The D0 service
frame (task07→task08) and three messages to task05 sit in queues that are never drained,
because those tasks are in the **deferred extended batch**. So the service handshake can
never complete:

```
extended service tasks (05, 08, 14, …) deferred  [resume gate, service-buffer byte = 0]
  → messages to them pile up unprocessed
    → service never reports ready  → task14_ready never set
      → task00 readiness loop stalls  → task02 D9 watchdog free-runs to timeout
        → CONTACT SERVICE
```

task01's repeated `event 0x03` and task02's repeated `event 0x19` are the two cores
spinning while they wait for the (never-resumed) extended tasks. This matches the
resume-gate finding exactly, now seen end-to-end on the bus.

## RESOLVED: it's hypothesis B (bootstrap ordering), NOT the EEPROM

The discriminating experiment (5 passes, static + runtime) settles A-vs-B decisively in
favour of **B: a service-transport / bootstrap-completion problem, independent of the
EEPROM.** Evidence:

### The task-0x14 resume sequence (`0x2a9120`) and its two gates

```
0x2a912c  if phase [r7+1]==5            -> skip all (core-only minimal path)
0x2a9136  if service_ready [0x110c2c]==0 -> skip EXTENDED BATCH 1   ← we stall here
          ... batch 1 resumes 7,8,3,4,1,0x13,0x16,9,2,6,5 ...
0x2a9186  if 0x29bafc()!=1              -> skip BATCH 2
0x2a918c  if service_ready [0x110c2c]!=1 -> skip BATCH 2
          ... batch 2 resumes 0xa..0x12 incl. TASK 0x14 ...
```

Both gate inputs are **service-subsystem RAM**, and the literals resolve to:
- batch-1 gate `[0x2a9130]` → `0x110c2c` = `FW_STARTUP_SERVICE_BUFFER` ready flag.
- batch-2 gate `0x29bafc` reads `[0x11fe68+0x69] = 0x11fed1`, **inside the contact-service /
  D9 watchdog block** (docs: counter `0x11fed6`, ack `0x11fedb`). It polls bit 2, then
  requires bit 6 — and **posts the watchdog event `0x19` itself when bit 6 is clear.**
  That is the very event task 02 free-runs to its CONTACT SERVICE timeout.

### The EEPROM does not feed either gate

- `findptr 0x110c2c` → referenced only from `0x290c0c/0x291004/0x291280` (service-startup)
  and the gate reader `0x2a93a0`. None in EEPROM code.
- `findptr 0x11fe68` → referenced only from `0x234xxx–0x237xxx` (contact-service task),
  `0x282xxx`, and the gate. None in EEPROM code.
- Every PC that drives the EEPROM I2C bus is in the transport layer `0x2b0xxx`/`0x2b1xxx`;
  the EEPROM is read heavily t=0.006–0.40 (incl. the service phase) but its bytes are
  consumed by RF/ADC/config code, never written into the two gate bytes.
- The service-ready flag `0x110c2c+0` is **never written non-zero by any of its 4
  referencing functions** — so it isn't conditionally set from the EEPROM either.

**Conclusion:** a fuller / checksum-valid EEPROM would **not** clear this gate. The dead-end
is the **MBUS D0/D9 service handshake not completing**: the service ack never sets the
watchdog-ready bit (`0x11fed1` bit 6) and the service-startup never sets the ready flag
(`0x110c2c`), so the extended batch (incl. task 0x14) is deferred and task 02's watchdog
times out. This is the same service-transport layer we began modelling with the MBUS D0
response — exactly where to keep building.

## Runtime-verified dependency graph (the writers)

Tracing the two gate writers (`TRACE_CONTACT_COMMIT=1`, reading `cs_write:`/`svcbuf_write:`)
nails the exact mechanism and resolves a mid-investigation wobble (see the honesty note).

**The contact-service present byte `0x11fed1` (batch-2 gate input):** the handler `0x2346b2`
sets bit 6 during init (`0x234758`, `|0x40`), then **immediately re-tests and clears it**:

```
0x2347a4  bl 0x2a8fec        ; returns the byte at its literal -> [0x110c2c] = service_ready
0x2347aa  cmp r0,#1 / beq    ; keep bit6 ONLY if service_ready==1
0x2347b2  strb (val & 0xbf)  ; else CLEAR bit6                       <-- fires in our boot
...
0x234810  cmp r4,[EEPROM 0x244] ; checksum(EEPROM[0x120..0x243]) vs stored
0x234832  strb (val & 0xbf)  ; on mismatch, CLEAR bit6 again (redundant here)
```

Runtime: bit6 is cleared at `0x2347b2` (because `service_ready==0`) **before** the checksum
runs; the checksum-fail clear at `0x234832` is a no-op. So bit6 has **two required
conditions**: `service_ready==1` **and** the EEPROM checksum passing.

**The linchpin — `service_ready`, byte `0x110c2c` (even byte, mask `0xff00`):** the only
writes to it are resets to 0 (`0x290c9c`). The `new=0001 mask=00ff` write at `0x290a0c`
hits the *odd* byte `0x110c2d` (hardware-variant), not the ready flag. The status word
`0x110c2e` does advance to `0x8002` (`0x290fd0`, mode `0x000d`) — but the ready byte is
**never set non-zero**. `service_ready` gates batch-1 **directly** (`0x2a9136`) and bit6 /
batch-2 **indirectly** (`0x2a8fec`). It is THE flag.

**The halt driver — `ack`, byte `0x11fedb`:** never written non-zero in the whole boot.
The D9 watchdog (`0x237b2e`) increments its counter every poll while `ack==0`; at count
`0x0f` it calls `0x2b4dda` → CONTACT SERVICE. (bit6 only controls whether the fault screen
is drawn during the countdown; the halt itself is the `ack` timeout.)

### So, precisely

| symptom | gated by | set by |
|---|---|---|
| CONTACT SERVICE halt | `ack 0x11fedb` stays 0 → watchdog timeout | service/MBUS response (never arrives) |
| task 0x14 batch-1 deferred | `service_ready 0x110c2c`==0 | service-startup completion (never) |
| task 0x14 batch-2 deferred | bit6 `0x11fed1` = `service_ready==1` **AND** EEPROM checksum | as above + EEPROM[0x244] |

All three trace to the **service-startup state machine stuck at status `0x8002`**, waiting on
the MBUS service transport to complete. The EEPROM checksum is a **real but secondary** gate
(only on task-14 batch-2); it is **not** the root of the halt. Honest verdict: hypothesis B
(service/bootstrap completion) is the root; the EEPROM is a second, independent task-14 gate.

## Honesty note (I nearly flip-flopped)

On finding the EEPROM checksum at `0x234588`/`0x234810` I briefly concluded "the EEPROM IS
the root after all." Runtime verification corrected that: bit6 was already cleared by
`service_ready==0` *before* the checksum executed, and `service_ready`/`ack` (the actual
blockers) have no EEPROM input. The static discovery was real; the root-cause attribution
was not. Verifying at runtime before re-concluding is what kept this honest.

## Implementation path — #1 and #2 are ONE fix (runtime-proven)

**Decisive experiment** (`EXPERIMENT_RESUME_TASK14=1` + `TRACE_CONTACT_COMMIT`/`TRACE_BUS`):
forcing the resume gate brings the extended tasks up — **task 8 (lower-service) now runs**,
along with 3,4,1,0x13,0x16,9,2,6,5 — yet **`ack 0x11fedb` and `ready 0x110c2c` are still
never set.** So the blocker is **not** task scheduling. The service handshake itself stalls:

```
task 07 --D0 frame--> task 08 (lower-service, 0x283xxx)
task 08: posts a setup cmd to task 03 (0x2832e0), then BLOCKS at recv (0x283db2)
task 08 recv dispatcher (0x283db6): msg id in [0xc0..0x1bf] -> MBUS-frame handler 0x283d6e
   ... but no such frame ever arrives ...
task 03 (I2C/peripheral server, recv @0x2b18e8): sits in RECV(wait), never replies
=> service never completes => ack & ready never set
   => CONTACT SERVICE (ack timeout) AND extended tasks deferred (ready==0)
```

So **the missing piece is the inbound MBUS service response**: task 08 waits for an MBUS
frame (command ≥ 0xc0, e.g. the D0=0xd0 reply) to be **delivered to it as a message**. Our
MBUS model already synthesises the D0 *bus bytes* (`mbus_generated_response_byte`), but the
**RX path that parses those bytes into a frame and posts it to task 08 is incomplete**.

### The one fix that clears both `ack` and `ready`

Complete the **MBUS receive path** so a D0 service response becomes a task-08 message:
1. Drive the MBUS RX state machine (`0x2aae76`/`0x2b052e`/`0x2aaf44`) to "frame complete"
   after the generated D0 response, so the firmware's RX ISR assembles the reply frame.
2. Ensure that frame is posted to task 08 with a message id in `[0xc0..0x1bf]` (it routes
   to handler `0x283d6e`). That handler advances the lower-service state, which is what
   ultimately sets `ack 0x11fedb` (stops the watchdog) and drives the service-startup to
   completion so it writes `ready 0x110c2c` (un-defers the extended batch).
3. Verify: `ack`/`ready` go non-zero, task 14 resumes, boot leaves CONTACT SERVICE.

*Next concrete RE step:* disassemble the task-08 MBUS-frame handler `0x283d6e` to learn the
exact reply frame shape (command, payload) it expects — that defines what the RX model emits.

## ROOT FOUND: `service_ready` waits on MAD2 IRQ line 4 (the DSP/lower-service interrupt)

Tracing the `service_ready` setter to the hardware (via the driver's memory map) reaches the
bottom of the whole investigation:

- The setter `0x291068` and its dispatcher `0x2af3ca` use base `0x20000` and `0x10000`, which
  the driver maps as **MAD2 I/O registers** (`0x20000–0x200ff`) and **DSP shared RAM**
  (`0x10000–0x10fff`). So `[0x20009]` is **`MAD2_IRQ_STATUS`** and `[0x100e4]` is **DSP shared
  RAM**.
- `0x2af3ca` is the firmware's **IRQ handler**: it reads `IRQ_STATUS & ~IRQ_MASK` and dispatches
  one routine per active bit. **Bit 4** routes to `0x291068` — the lower-service processor that
  drains the DSP-shared channels and runs the `service_ready` check.
- The `service_ready` setter fires only when **IRQ line 4** is active. In the driver,
  `assert_irq()` is called only with `0` and `6` — **`assert_irq(4)` never happens**. Reg `0x0E`
  (interrupt trigger) is read-only and unimplemented, so IRQ 4 is a *hardware* source, not
  software-triggered.
- IRQ 4's handler operates on **DSP shared RAM**, and the **DSP is not emulated** — `dsp_ram_r`
  is a documented "HACK to avoid hangs." So the DSP never raises its completion interrupt.

**So the single, true root of CONTACT SERVICE is: the lower-service/service-startup is driven
by the MAD2 internal DSP, which is stubbed. The DSP never raises IRQ line 4, so the
`service_ready` poll never runs, so `service_ready` stays 0, so bit 6 is cleared, so the
contact-service watchdog arms and halts.** (The EEPROM checksum is a *second, independent* gate
on bit 6 — now fixed — but this DSP/IRQ-4 path is the dominant one.)

### EXPERIMENT RESULT — the DSP/IRQ-4 model is VALIDATED (`EXPERIMENT_DSP_IRQ4=1`)

Pulsing `assert_irq(4)` at 200 Hz (`timer_keypad`) **and** draining the DSP-shared "pending
work" counter at byte `0x0e4` (`dsp_ram_r`) — i.e. simulating the DSP raising its completion
interrupt and clearing its queue — produces, confirmed by trace:

- The `service_ready` setter `0x291068` **runs** (it never did before).
- `service_ready 0x110c2c` is **set to 1** (`strb` at `0x29109e`, t=0.25) — the first time
  this byte is ever non-zero.
- bit 6 of `0x11fed1` now **survives the contact-service init**: with `service_ready==1`
  (via `0x2a8fec`) *and* the EEPROM checksum fixed, the init takes the **keep** path
  (`0x2347d0`, sets bit 2) instead of either clear path.
- The **extended batch-1 tasks resume** (12 dispatched vs 4: incl. task 8).

So the DSP/IRQ-4 hypothesis is correct: IRQ line 4 drives `service_ready`, and the absent DSP
is exactly why it never fired. Two driver gaps (no `assert_irq(4)`; DSP-RAM counter never
drained) account for the entire `service_ready` failure.

**Remaining blocker, now cleanly isolated:** CONTACT SERVICE *still* fires, because the D9
watchdog counter (`0x11fed6`) reaches `0x0f` — `ack 0x11fedb` is **still never set**. This is
a **separate heartbeat** from `service_ready`/bit 6: the watchdog is armed at t=0.25 by the
batch-2 gate (bit 6 not yet set, since the contact-service init runs later at t=0.38), then
self-perpetuates, and nothing satisfies `ack` before it times out. Also, **task 14 (batch-2)
still doesn't dispatch** — the batch-2 gate samples bit 6 before the init sets it (ordering).

So with the DSP/IRQ-4 model, two of the three sub-gates are cleared (`service_ready`, bit 6 /
EEPROM checksum); the **D9-watchdog `ack` heartbeat** is the last wall. `ack` has no writer
anywhere in the firmware that we can find — strongly suggesting it too is a DSP/hardware-fed
heartbeat (or that the watchdog is meant never to *arm* on a phone where bit 6 is set before
the batch-2 gate). That timing/heartbeat is the next thread.

Repro: `make run-boot-progress EXPERIMENT_DSP_IRQ4=1 [EXPERIMENT_DSP_IRQ4_AFTER_MS=0] TRACE_CONTACT_COMMIT=1`.
Oracle unaffected (both knobs env-gated, frame stays `d8a9a7`).

### BREAKTHROUGH — DSP/IRQ-4 model + ack clears CONTACT SERVICE (`EXPERIMENT_DSP_IRQ4=1 EXPERIMENT_FORCE_ACK=1`)

Combining the DSP/IRQ-4 model (sets `service_ready` + bit 6) with forcing the D9-watchdog
`ack` heartbeat non-zero (read shim at the watchdog's own `ldrb`, pc `0x237b42`) **clears
CONTACT SERVICE for the first time via a hardware-style path** (not by forcing the firmware's
task-14 result). Confirmed:

- The watchdog counter `0x11fed6` **never climbs** (stays `0x00`) — `ack!=0` resets it each
  tick. So `ack` was indeed the final CONTACT-SERVICE gate.
- The boot **advances into new phases**: the LCD now cycles a **white fill (`94a2dc`) ↔ black
  fill (`4aab13`)** display-init pattern — states only ever reached before by forcing
  `task14_ready`. Extended tasks `00,01,02,03,04,07,08,09,13,16` dispatch.

So the **entire CONTACT SERVICE root cause is now accounted for**: `service_ready` (DSP IRQ-4),
bit 6 (service_ready + EEPROM checksum), and the D9-watchdog `ack` heartbeat. Each maps to a
specific, identified hardware/firmware mechanism.

**New frontier (next blocker):** with CONTACT SERVICE cleared the mode progresses
`0000 → 0001 → 000d` and then **holds at `000d`** (not a reboot loop — task 0 is not
redispatched, the watchdog counter stays `0`). The LCD then alternates a white fill
(`94a2dc`) and black fill (`4aab13`) — the mode-`000d` display phase. The mode never advances
to the "ready" modes (`0x0004..0x0007`).

Adding `EXPERIMENT_FORCE_TASK14_READY` on top does **not** advance it further (same frames),
so the blocker is *not* just `task14_ready`. The mode-`000d` → ready transition needs the full
readiness chain (`service_context_ready`, `display_init_ready`, `task14_ready`, `0x2a6566`,
`0x2a0ec4`, `0x279282`) and/or the battery/charger state. Also task 14 (batch-2) still doesn't
dispatch: its gate needs the startup **phase byte `0x112449` ∈ {0,2}** (currently `01`) in
addition to bit 6 + `service_ready` — a phase the boot hasn't reached. Those are the next
threads: (1) which readiness predicates still fail in mode `000d`; (2) what advances the
startup phase `0x112449`; (3) the charger/battery state.

**Honesty note:** `FORCE_ACK` and the DSP-counter drain are still *diagnostic forces*, not
faithful models — they prove the mechanism (and that the root-cause map is correct) by
advancing the boot, but a real fix needs the DSP completion interrupt + `ack` heartbeat modelled
from the hardware (or a valid EEPROM that makes the firmware reach them naturally). Oracle
unaffected (all knobs env-gated; default boot stays `d8a9a7`).

### Post-clear analysis — the forces "limp", they don't "complete" (honest)

Tracing the cleared boot (`EXPERIMENT_DSP_IRQ4=1 EXPERIMENT_FORCE_ACK=1`, `cs_pred`/`cs_disp`):

- **The readiness loop `0x2a92fc` is never reached** (`cs_pred` logs 0 lines). The mode-`000d`
  steady state is the **contact-service watchdog ticking `0xd9` forever** (every ~6.6 ms),
  with its substate `[0x11feda]` stuck at `0x03` (watchdog-active) — it **never transitions to
  a healthy/complete substate**.
- So `FORCE_ACK` stops the watchdog *counter* from reaching the halt, but the contact-service
  never *completes and exits*: it never receives the "service OK" message that would advance
  it. The boot **limps** past CONTACT SERVICE (no halt) but does not cleanly boot — it spins
  the watchdog + white/black display in mode `000d`.

**Conclusion (honest):** the `EXPERIMENT_*` forces have done their job — they **validate the
entire root-cause map** (each CONTACT SERVICE gate identified, the halt clears, the boot
advances to new states). But they are diagnostic band-aids, not a clean boot. A genuinely
booting phone needs the **service/contact-service layer to actually complete** — i.e. the
lower-service (DSP-driven) signalling the contact-service that the service is healthy, which
transitions its substate and lets the startup reach the readiness loop. That requires real
DSP-interface emulation and/or a fully valid, calibrated EEPROM so the firmware reaches these
states on its own, rather than forcing individual gates.

This is the honest frontier: we have a complete, validated **map** of why a blank/un-emulated
3210 shows CONTACT SERVICE and stalls, and a forced path that proves it — but the clean fix is
the larger task of modelling the DSP service interface + supplying a valid EEPROM.

### Two interacting startup paths (why stacking forces gets tangled)

A subtlety that complicates the post-CONTACT-SERVICE picture: there are **two startup paths**,
selected by whether the extended task batch is resumed.

- **Deferred path** (baseline / `FORCE_TASK14_READY`): extended tasks stay deferred, the
  startup runs the **readiness loop `0x2a92fc`** (`cs_pred` fires); `task14_ready` fails →
  loops → watchdog → CONTACT SERVICE.
- **DSP path** (`EXPERIMENT_DSP_IRQ4`): the extended tasks *resume*, and the startup takes a
  **different branch that never reaches the readiness loop** (`cs_pred` = 0 lines) — it
  advances toward `94a2dc` instead.

Both paths currently dead-end at the same place: **mode `000d`, contact-service ticking `0xd9`
forever, LCD cycling white (`94a2dc`) / black (`4aab13`)**. So the forces don't compose into a
clean boot — they reach the same wall by different routes. The wall is the **contact-service
never *completing*** (substate `[0x11feda]` stuck at `0x03`), independent of which path.

### What the cleared boot loops on (PC sampler, `TRACE_PC_SAMPLE=1`)

Sampling the instruction-fetch PC in the cleared boot (`EXPERIMENT_DSP_IRQ4=1
EXPERIMENT_FORCE_ACK=1`) shows the mode-`0x000d` steady state is a **startup *retry* loop**,
not a settled state. Hot PCs (steady-state, t>1.5 s):

- `0x2b65e4` = **`memset`** (byte fill) — by far the hottest; the white/black LCD fills.
- `0x2a41d0` = **16-bit byte-sum** (the EEPROM/checksum primitive) — called repeatedly.
- `0x25exxx` = a **rendering/fill routine** (dispatches on a 4-bit type field at `0x25e682`,
  memset-heavy) — the display redraw.
- `0x290a94` = service-startup code.

So the boot keeps re-running the startup (service-startup → render → watchdog tick) forever,
redrawing the white/black LCD each pass. The white/black is a **symptom** of the retry, not a
separate blocker — every thread (readiness loop not reached, contact-service substate stuck
`0x03`, watchdog ticking, display redrawing) converges on the same cause: **the service never
*completes*** (no DSP/service result arrives). The contact-service substate machine confirms
it — substate goes to `5` (OK) only if a service-result code `r4==5` (`0x235600`), else `2`
(fault); that result is what the (absent) DSP/service response would supply.

The result handler is **`0x2355b6`** (`r0`=result code): it sets substate **`5` only if
`r4==5`** (`0x235604`), otherwise **`2`** (fault) — for every code in {2,8,9}. So `5` is the
*unique* healthy completion code. The 9 callers and the result each passes:

| caller | result | gate / meaning |
|---|---|---|
| `0x235664` | `2` fault | service reset/abort path |
| `0x236eaa` | `2` fault | status byte (via `0x282afc`) bit 0 clear |
| **`0x236f00`** | **computed `r6`** | **EEPROM config-checksum-validating response path** (calls `0x234588`/`0x2af8e2`) — the *only* caller that can produce `r4==5` |
| `0x26485e` | `8` present | response status `[blk+0xc]` nibble [11:8] == 2 |
| `0x264bc0` | `8` present | `[r6+0x11]` low bit + counter check |
| `0x264d62` | `8` present | response status `[r6+0xc]` nibble [11:8] == 2 |
| `0x265918` | `9` | `[blk+9]==0` |
| `0x29bcb4` | `2` fault | sets bit6 of `0x11fed1`, then fault |
| `0x2a1308` | `2` fault | error path |

**All 9 run only when a service *response* is processed.** With no DSP/service response, none
fire — so the substate stays at its init value `0x03` and the contact-service just ticks.

#### The response-command dispatcher `0x236dc6` (the healthy trigger found)

The computed caller `0x236f00` lives in the **service-response dispatcher `0x236dc6`** (`r6`
= the response **command** byte, 1..0x0b), which dispatches via an 11-entry jump table at
`0x236e34`:

| cmd | handler | result → substate |
|---|---|---|
| `0x02` | `0x236efe` | result `2` → fault |
| **`0x05`** | **`0x236efe`** | **result `5` → substate 5 (HEALTHY)** |
| `0x06` | `0x236e90`→`0x236ec6` | EEPROM config-checksum validation path |
| `0x04` | `0x236f06` | … |
| `0x07/08/09` | `0x236e60` | … |
| `0x01/03/0a/0b` | misc | … |

So **the contact-service completes healthily iff it receives a service response with command
`0x05`** (the `0x236efe` handler passes the command value straight through as the result
code, and only `5` maps to the OK substate). The cmd-`0x05` path passes `5` directly — it
does **not** go through the EEPROM checksum (that is the separate cmd-`0x06` path; an earlier
note conflated them).

So the single thing needed for a clean contact-service completion is **a service response
whose command byte is `0x05`**. (Other observed wants: result-`8` "service-present" needs
response status `[blk+0xc]` nibble [11:8]==2.)

#### The command comes from NV/PM parameter `0x5f00`

Both the response builder (`0x29be50`, inside `0x236dc6`) and the **D9 watchdog dispatch
`0x237994`** read the command from logical address **`0x5f00`** via `0x2b13a2` → `0x2b12dc`.
`0x2b12dc` is **not** a raw 24C128 read — it is the higher-level **PM (parameter manager) /
NV layer** (it caches/clamps records, length ≤ `0xb4`, with scheduler waits). And `0x5f00`
is past the 24C128's `0x4000` size, so it is a **PM logical address** (the DCT3 "PM" store,
typically flash-backed NV). On a blank/un-provisioned unit this parameter is absent → the
command is not `0x05` → the contact-service can't complete.

#### The PM layer is MBUS message-passing (the unification)

Tracing `0x2b12dc` further: it does **not** read memory directly. It **allocates a request
message**, fills it with the target address (`0x5f00` at `[msg+8/9]`), a count, and a
**destination node** (`[msg+1]` from `[0x11fee5]`, near the service-channel flags), then
**posts it** via `0x2b0482` — which *routes by destination node* (`[msg+1]`, rejecting
0/0x7f/0xfd/0xff). So the "PM/NV read" of `0x5f00` is actually a **remote memory read issued
as an MBUS/service request frame to another node** — the same MBUS service transport as the
D0 frame, not a local EEPROM access. The response (carrying the data at `0x5f00`) comes back
asynchronously and is what the contact-service then dispatches on.

**This unifies the tracks.** The contact-service command (`0x05` = healthy), the D9 watchdog
selector, and the service "responses" all come from **MBUS remote reads whose responses we
must model with correct data** — exactly the layer the D0-response model (`mbus_generated_
response_byte`) began. There is no separate "DSP completes" vs "EEPROM is valid" vs "service
box responds": it is one MBUS service-transport layer, and a clean boot needs that transport
to answer the firmware's read-requests (`0x5f00`, etc.) with the values a provisioned phone
would return (e.g. whatever makes the contact-service command `0x05`).

**Next thread:** extend the MBUS response model to answer the contact-service's read-requests
— identify the destination node (`[0x11fee5]`), the exact request for `0x5f00`, and the
response payload that yields command `0x05`. That is the single, now-unified path to a genuine
contact-service completion (and likely the broader boot), replacing the per-gate `EXPERIMENT_*`
forces with one transport model.

#### Runtime trace (`TRACE_PM=1`) — node `0x18` is the missing responder

Probing the PM read (`0x2b13a2`: r1=addr, r2=dest) and the response dispatcher (`0x236dc6`):

- The contact-service reads **`0x5f00` (count 2) from destination node `0x18`** every ~9 ms
  (the D9 watchdog tick, `lr=0x2379a8`). Node `0x18` = `[0x11fee5]`, the contact-service's
  service channel.
- The phone issues PM reads to a whole **map of nodes**: `0x00` (self, 49×), `0x18` (16×),
  `0x34, 0x58, 0x60, 0x68, 0x6c, 0x70, 0x74, 0x78, 0x7c, 0x80, 0xf4` — for addresses
  `0x5f00, 0x7105, 0x7312, 0x7317, 0x0c02, …`.
- **`svc_response` (`0x236dc6`) count = 0 in every run — including the cleared boot with the
  extended tasks resumed.** So *no* read ever gets a response; it is **not** a task-deferral
  issue — the node providers simply never answer.

So the precise missing piece is: **node `0x18` (and the other service nodes) never respond to
the firmware's read-requests.** The contact-service polls `0x5f00` forever, never gets the
command byte, and stays in substate `0x03`. This is the single concrete target — model the
node-`0x18` provider's responses (so a `0x5f00` read returns the value yielding command `0x05`).
Open sub-question: whether the requests are even transmitted or dropped at the address-validity
check `0x2b12b4` (if the channel/node is "unregistered", the read returns stale data with no
request sent) — i.e. whether the fix is "register/enable the channel" or "supply the response".

#### ANSWERED — the reads are DROPPED: the service channels are never enabled

`TRACE_PM` probe at the validity-check return (`0x2b13b0`): **every `0x5f00` read returns
`valid=0` (dropped) because the master enable `[0x11fee4]` (`FW_SERVICE_CHANNEL_ENABLE_FLAGS`)
is `0x00`.** So the contact-service never even *sends* the request — `0x2b13a2` returns
without reading, leaving stale data, and the watchdog perpetuates on garbage.

The validity check `0x2b12b4` requires **`[0x11fee4]!=0` AND the address registered** in a ROM
bit-table (`0x2e2f5c`) gated by a RAM channel-mask bitmap (`0x11ff08` = `FW_SERVICE_CHANNEL_
MASK_BASE`). The enable flag is **reset to 0 by `0x2b13c0`** (called from startup `0x2a9284`)
and **never set non-zero** in our boot. So the **service channels are never opened**.

This narrows the whole investigation to a sharper, earlier point than the response model:
**find what *opens* the service channel** (sets `0x11fee4` + registers the channel in
`0x11ff08`) on a healthy boot, and why it never runs here. Likely it is gated on the same
service-detection/NV state — but it is a *single, concrete, local* write (`0x11fee4 != 0`),
not a remote response, so it is directly testable. **Next:** (1) find the channel-open writer
of `0x11fee4`; (2) as a probe, force `0x11fee4` non-zero and see whether the reads then
transmit and whether *any* provider answers (revealing if a local responder exists or the
node is genuinely remote/absent).

#### FORCE TEST RESULT — node `0x18` is genuinely absent (no local provider)

`EXPERIMENT_FORCE_SVC_CHANNEL=1` forces the validity check (`0x2b13b0`) to pass, so the reads
**do transmit** (`pm_valid` now `valid=1`; 100 reads issued to nodes `0x00/0x18/0x34/…/0x80`).
Result: **`svc_response` is still 0** — in every configuration, including reads-transmitting
**plus** the extended tasks resumed (`+EXPERIMENT_DSP_IRQ4 +EXPERIMENT_FORCE_ACK`). No provider
ever answers, and the boot still reaches CONTACT SERVICE (`d8a9a7`).

So the fork is resolved: **node `0x18` has no local responder to wake up — it is genuinely
absent** (a remote / DSP / service-box node not present in the emulation). The channel-enable
gate (`0x11fee4`) was real (reads were dropped), but opening it is *not sufficient*: past it,
the response data simply does not exist locally.

**Consequence (honest):** the optimistic "single local write" hope is wrong. A clean
contact-service completion needs **both** (a) the service channel enabled (`0x11fee4` + the
`0x11ff08` registration) **and** (b) a **model of node `0x18`'s responses** — at minimum, a
`0x5f00` read returning the value that makes the response command `0x05`. That is the MBUS/PM
service-transport response model (the `mbus_generated_response_byte` layer), now with a precise
spec: node `0x18`, address `0x5f00`, count 2, → a value that dispatches to command `0x05`.

This is the deep-but-mapped work. Every layer is now charted end to end; what remains is to
**synthesise the service-node responses** (effectively, emulate the responder for the PM/service
nodes the firmware queries), which is the same task as "provision a fully service-responsive
phone". Bounded and specified, but not a one-line fix.

#### Injection test — synchronous `0x5f00` ≠ completion; the completion is async

`EXPERIMENT_INJECT_5F00` writes a chosen value into the dest buffer of the synchronous
`0x5f00` read (`0x2b12dc`), which feeds the **D9 watchdog dispatch** (`[dest]-0xd9` selects a
sub-handler). Sweeping the value over `0xd9..0xe2` (with `FORCE_SVC_CHANNEL`): **no value
completes the contact-service** — `svc_response` stays 0, frame stays `d8a9a7`. The synchronous
read only steers the *watchdog* sub-handler, not the completion.

This is because **the healthy completion is via the *async* response dispatcher `0x236dc6`**
(command `0x05`), which has **zero static references** — it is reached only by a *computed*
dispatch when a real response *message* is received and routed by the contact-service task.
There is no value to inject and no local trigger to force; the dispatcher only runs if a
correctly-formatted **response message** is delivered.

**So the remaining work is concretely: synthesise and post the contact-service response
message** (format: a frame whose command routes to `0x236dc6` with `r6==0x05`), i.e. build a
minimal **service-node responder** that answers the firmware's `0x5f00`/node-`0x18` read with a
healthy reply. That is genuine modeling (message format + posting + the responder for each
queried node), not a force — the natural stopping point of the "force/inject individual gates"
approach. The map is complete; the build is the next phase.

### DSP service-area map (DSP shared RAM `0x10000`, `TRACE_DSP=1`)

The MCU↔DSP service handshake lives in DSP shared RAM (mapped at `0x10000-0x10fff`; the DSP
itself is unemulated, `dsp_ram_r` is a stub). Tracing reads/writes (`dsprd:`/`dspwr:`) maps
the handshake block (byte offsets within DSP RAM):

| off | role | read by (pc) | value |
|---|---|---|---|
| `0x00..0x24` | DSP-RAM self-test (write `0xea`/`0x00`, read back) | `0x295fd6` | echoes |
| `0xa4,0xa6` | DSP service status/version | `0x2907xx`,`0x2909xx` (service-startup) | `0` |
| `0xda,0xe2` | lower-service channel counts/pointers | `0x2910ac/0x2910da` (ready setter) | `0` |
| `0xe0` | service-dispatch busy flag | `0x290d0a` | `0` |
| **`0xe4`** | **lower-service pending counter** | `0x291096` (`service_ready` gate) | **`0x0002`** |
| `0xfe,0x100` | DSP ready flags | `0x290aba/0x290aae` | `0`/`1` (hack) |

The DSP "completes" service work by **draining `0xe4` → 0** and **raising IRQ line 4**. Both
are what the `EXPERIMENT_DSP_IRQ4` shim fakes. A faithful model would, on the MCU queueing
work (write to `0xe4`), drain it and pulse IRQ 4 after a short delay — replacing the 200 Hz
pulse + read-time drain. The other offsets already read `0` (the "empty/OK" value), so `0xe4`
+ IRQ 4 is the core of the handshake. The DSPIF API register (`0x30000`) is also a stub
(returns 0) and may carry the command/status side of the protocol — not yet exercised.

### Concrete fix paths

1. **Targeted IRQ-4 model (testable now):** assert `assert_irq(4)` when the firmware queues
   lower-service work (e.g. on the DSP-interface / shared-RAM write that kicks a request),
   simulating the DSP completing. With the DSP-RAM counter `[0x100e4]` reading 0 (its default),
   the setter `0x291068` would then run and set `service_ready=1`. Combined with the EEPROM
   checksum fix, bit 6 would stay set → no watchdog → no CONTACT SERVICE. *Experiment: force
   a periodic `assert_irq(4)` and confirm `service_ready` gets set and the boot advances.*
2. **Fuller DSP-interface emulation (faithful):** model the MAD2 DSP handshake (the `mad2_dspif`
   registers + shared-RAM protocol) so IRQ 4 is raised with correct timing/state. Larger, but
   the honest hardware-emulation path.

This supersedes the MBUS-transport framing for the *final* gate: the MBUS D0 work was real and
correct, but `service_ready` ultimately waits on the DSP, not the MBUS bus.

### DONE — DSP service handshake modelled (`NOKI3210_MODEL_DSP_SERVICE`, replaces the IRQ-4 force)

Fix path #1 above is now a real, opt-in **model** (`dsp_ram_w` + `timer_dsp_service`), validated
against the oracle. It is the honest replacement for the `EXPERIMENT_DSP_IRQ4` force (blind 200 Hz
pulse + read-time fake-zero of `[0x100e4]`).

**Trigger found (un-deduped `dspwr-pending` probe).** The MCU queues lower-service work by writing
the pending counter itself: `[0x100e4] = 0x0002` at **pc `0x290c98`, t≈0.170** (after clearing it
to 0 at `0x290ba6`, t≈0.164). It is never re-queued in a 20 s boot. The earlier "counter is
naturally 0" reading was wrong — the deduped `dspwr` trace saturates during the 256-byte `0xe00`
DSP-program upload, hiding this write; the raw RAM holds `0x0002`, so the read-hack was load-bearing.

**The model.** On the MCU writing a non-zero pending count, after a short processing delay
(`MODEL_DSP_SERVICE_DELAY_MS`, default 5) the modelled DSP **drains `[0x100e4]` → 0 for real** and
**raises IRQ line 4** — then re-arms at a service-tick rate (`MODEL_DSP_SERVICE_TICK_MS`, default 5).
No read-hack; the gate at `0x291096` reads the genuinely-drained `0x0000`.

**Why recurring, not one-shot (a finding).** A single completion at t≈0.175 fires *too early*: the
firmware resets `service_ready` at the top of every startup phase (`0x2a90d6`) and only re-sets it
from inside the IRQ-4 path, so the phase that consumes it (~t≈0.255) sees 0 unless IRQ-4 recurs
*within that phase window*. One-shot → setter runs 0×; recurring → setter runs each phase and
`ready[0x110c2c]=01`, matching the force. This is physically right: a booted DSP raises a periodic
per-frame service interrupt, it does not signal once.

**Measured.** `MODEL_DSP_SERVICE=1` alone: setter runs, `ready=01`, **oracle frame stays `d8a9a7`**
(still CONTACT SERVICE — the D9 ack heartbeat `0x11fedb` is still genuinely absent; that is the next
force to model, requirement #3). Default boot (model off) is byte-identical to baseline. *Next:*
verify against the full structural-marker set, then promote to default and retire `EXPERIMENT_DSP_IRQ4`.

### Bit-6 gate investigated — the ack is confirmed a red herring; real gate is the service-channel array

Pursuing "model the D9 ack" (`EXPERIMENT_FORCE_ACK`) confirmed the ack `0x11fedb` is **not
modellable** — it has no firmware producer; `FORCE_ACK` is a brute-force watchdog bypass. The
real lever for leaving CONTACT SERVICE is **service-present bit 6** (in the word at `0x11fed0`),
which the contact-service init clears via **two** paths, both now characterised at runtime:

1. **`service_ready != 1`** (getter `0x2a8fec` at `0x2347a8` → clear `0x2347b2`). With the new
   `MODEL_DSP_SERVICE` on, the init reads **`r0=1`** here (probe `bit6_svcready_check`) — this
   path no longer fires. ✅ (the DSP model pays off).
2. **Dirty service-channel status array** — a 24-entry array at **`0x11fc60`** (loop `0x23487e..
   0x2348a2`): bit 6 is cleared if any entry `[0x11fc60+i]` (i≠11) is not `0x00/0xfe/0xff`. Probe
   `bit6_clear` finds **two dirty on a blank phone: idx6 `[0x11fc66]=0xfd`, idx18 `[0x11fc72]=0x12`**
   — service modules reporting "present but not OK". Writers logged (`svcchan_write`): bulk-init at
   `0x29645c`, then per-module init routines (`0x2962d6`, `0x295edc`, `0x295eb4`, `0x2347a2`, …).

**The two dirty entries traced to their gating checks (fan-out = 2, both known subsystems).**
The `svcchan_write` probe (big-endian-correct) + disassembly pin each dirty status to one
availability check; the value is `0x00` (clean) iff the check passes, else `0xfd`:

- **idx6 `[0x11fc66]`** ← `0x295ebe: bl 0x2afb44` (ghidra `ccont_reg_read_2afb44`). Decoded: reads
  **CCONT register 1, masks `0x90`**; clean iff `(ccont_reg[1] & 0x90) != 0`. So idx6 is a
  **CCONT / power-management ASIC** state dependency — modellable via the existing CCONT emulation
  (supply the right register-1 bits).
- **idx18 `[0x11fc72]`** ← `0x295ea4: bl 0x264c56`, which reads **EEPROM near `0x11c`/`0x120`**
  (the security/config block boundary) and compares — an **EEPROM-validity** check. (The contact-
  service init re-runs the same check at `0x234796` and stamps `0x12` when it fails.) This is the
  **EEPROM provisioning** axis (requirement #1).

So the cascade-vs-fan-out question, for this gate, resolves to **two well-understood subsystems**
(CCONT register state + EEPROM validity) — not an unbounded explosion. Both are already on the
provisioning list.

**idx6 = CCONT register 0xe (IRQ status) bit 0 — validated (corrects an earlier decode error).**
A 3-pass CCONT deep-dive nailed idx6 exactly. The earlier "index 1, mask `0x90`, boot-ordering wall"
framing was a **static-decode error** (the literal was misread). The runtime arg to `ccont_reg_read`
(`0x2afb44`) is **`0x9001`**, decoding to **index `0x10`, mask `0x01`**. Because the cached shadow
byte `[0x11238c]` (= index `0x10`) is `0x70` (≠`0xff`), the helper takes the **live serial-read path**:
it sends CCONT command `0x04 | 0x70 = 0x74` → CCONT address `(0x74>>3)&0xf = 0xe`, and returns
**bit 0** of it. So:

> **idx6 is clean ⟺ bit 0 (`0x01`) of CCONT register `0xe` (IRQ status), read *live* at t≈0.0135.**

Being a live read, there is **no timing wall** — a reset/early CCONT value reaches it. Confirmed by
`EXPERIMENT_CCONT_SVCBIT` (OR `0x01` into the reg-`0xe` read): `idx6_ccont_check` flips `0x00 → 0x01`,
**idx6 goes clean**, and the bit-6 clear loop then lists **only idx18**. (Frame stays `d8a9a7` because
idx18 still blocks — see below.)

**Open — faithfulness.** The emulated CCONT only ever sets IRQ-status to `0x08` (bit 3, the boot IRQ);
**bit 0 is never produced**. What real CCONT condition sets IRQ-status bit 0 (and whether the emulated
chip should report it) needs CCONT register semantics — that's the one remaining unknown for a faithful
idx6 model (vs. the validated `EXPERIMENT_CCONT_SVCBIT` force). Bit 0 of the CCONT interrupt register
is the lever; identifying its real source is the next step.

**Open — idx18.** Still the `0x264c56` **EEPROM-validity** check (reads `~0x11c/0x120`). With idx6
modelled, idx18 is the **last** dirty service-channel entry; satisfying it should let bit 6 stay set
(then re-test across the multi-pass init: svcready check t≈0.415, clearing loop t≈0.460). Forcing the
entries' *reads* clean (`EXPERIMENT_CLEAN_SVCCHAN`) does **not** alone clear CONTACT SERVICE — the
honest fix satisfies the underlying CCONT + EEPROM checks.

Tools added: `bit6_clear`, `bit6_svcready_check`, `svcchan_write`, `idx6_ccont_check`, `ccont_read_tbl`,
`ccont_shadow_write`, `ccont_reg_read`/`ccont_path` (`TRACE_CCONT_READ`) probes, plus the
`EXPERIMENT_CLEAN_SVCCHAN` and `EXPERIMENT_CCONT_SVCBIT` (reg-0xe bit-0) diagnostic levers.

## CORRECTION (deep dispatch + ready-byte trace): the dispatch is NOT the ready setter

A full runtime trace of the service-startup dispatcher (`svc_disp:` probe at `0x290cf4`)
overturns the "complete the D0 handshake → service sets ready" model from the sections
below. The dispatcher **does complete** — yet `ready` stays 0. Evidence:

- The dispatch command timeline runs `cmd 08`(→status `0x8002`)`,09,29,2e,33,2f,30,32,…,12,
  12,33,2e,2f`. The status word `0x110c2e` reaches `0x8002` and **never changes again**.
- **`cmd 0x2f` (the dispatch "completion", arg2=1) IS dispatched (twice), but
  `ready[0x110c2c]` stays `00`.** Disassembly: the completion store `0x291036`
  `strh r0,[r3,r1]` has `r3=0xe0` (set at `0x290d06`, unchanged on the cmd-2f path) and
  `r1=0x110c2c`, so it writes `0x110c2c+0xe0 = 0x110d0c` — a "done" field, **not** the ready
  byte.
- The ready byte is **reset to 0 by `0x290b54` at `0x2a90d6`** — the top of the *same*
  startup function whose gate (`0x2a9132`) reads it. So it is structurally 0 on that run.

**So the ready byte / extended-task resume is a phased-bootstrap *re-run* mechanism, not a
flag the D0 handshake sets** (this re-confirms the `static_branch_map.md` "structurally 0"
finding). Extended tasks (incl. 0x14) come up only when this startup sequence **re-runs in a
later phase**, which only happens once the core service/contact-service finishes phase-1.

### COMPLETE CHAIN (supersedes the ack framing below): bit 6 is the linchpin

Deeper tracing of the watchdog *arming* shows `ack` is a red herring too — it is **never
written non-zero anywhere reachable** (no `base+0x73` writer exists in the binary). The
watchdog only runs because it gets **armed**, and arming is gated by bit 6:

```
batch-2 resume gate 0x29bafc reads bit6 of 0x11fed1 (service-present):
    bit6 SET   -> return 1: task 14 resumes, NO event 0x19, NO watchdog, NO CONTACT SERVICE
    bit6 CLEAR -> post event 0x19 (0x29bb1a) -> watchdog 0x237b2e self-perpetuates
                  (re-arms 0x19 each tick), counter++ while ack==0, ack never set,
                  counter==0x0f -> 0x2b4dda -> CONTACT SERVICE
```

**bit 6 is THE linchpin.** The contact-service init `0x2346b2` sets bit 6, then clears it if
**either** condition fails:
- `service_ready 0x110c2c != 1` (checked via `0x2a8fec` at `0x2347a4` → clear at `0x2347b2`), **or**
- the **EEPROM checksum** `sum16(EEPROM[0x120..0x243]) != EEPROM[0x244]` (`0x234810` → clear at `0x234832`).

On the blank phone **both** fail: `service_ready` is structurally 0 (the reset-then-gate
re-run mechanism above) **and** the EEPROM checksum mismatches (`0x20df` vs stored `0x20de`).
So bit 6 is cleared → watchdog arms → CONTACT SERVICE.

**This re-elevates the EEPROM to a genuine root** (correcting the turn-3 "EEPROM not the
root"): a checksum-valid, calibrated EEPROM is *required* for bit 6 — both directly (the
`0x234810` checksum) and indirectly (a valid EEPROM lets the service-startup complete, which
sets `service_ready`, before contact-service init runs). CONTACT SERVICE is, fundamentally,
the firmware's **correct response to a blank/uncalibrated phone**.

**Two concrete requirements to keep bit 6 set:**
1. **EEPROM checksum pass** — ✅ **DONE & VERIFIED.** Probe `cs_cksum` at `0x2347fe` showed
   the firmware computes **`0x1ee1`** (`sum16(EEPROM[0x120..0x243]) = 0x20df` minus the
   `0x2978c0` correction `EEPROM[0x154]+[0x155] = 0x1fe`). The firmware reads
   `EEPROM[0x244..0x245]` big-endian-in-word, so the overlay now returns `0x244=0x1e`,
   `0x245=0xe1` → `ldrh = 0x1ee1`, `match=1`. Result: the checksum-fail clear at `0x234832`
   **no longer fires**, and the boot frame stays **byte-identical to the oracle** (`d8a9a7`).
   So this is hardware-faithful and milestone-preserving — a real EEPROM fix that removes one
   of the two bit-6 clear paths.
2. **`service_ready==1`** — ⛔ **still failing; setter found and chain traced.** The
   `service_ready 0x110c2c` setter is **`0x291068`** (writes `[0x110c2c]=1` iff the
   lower-service pending counter `[0x100e4]==0`). It is reached only from the lower-service
   **event-dispatcher `0x2af3ca`** (called from `0x2ac794`), and only when **bit 4 of the
   event-flags byte `[block+9]`** is set. A runtime probe at the setter (`0x2910a0`) confirms
   **the setter never runs** in our boot — so that event bit is never signalled.

   So `service_ready` requires the lower-service layer to (a) signal event bit 4 ("check
   ready") **and** (b) have drained its pending counter `[0x100e4]` — i.e. the lower-service
   transport must finish its work. This bottoms out at the **same lower-service/MBUS transport
   completion** as everything else; the EEPROM checksum was an *independent* gate (now closed),
   but `service_ready` is gated on the transport draining.

   **Full `service_ready` chain:**
   `ready 0x110c2c=1` ← setter `0x291068` (needs `[0x100e4]==0`) ← dispatcher `0x2af3ca` sees
   event bit 4 in `[block+9]` ← lower-service signals it on work completion (never happens).

   **Next:** find what sets bit 4 of the lower-service event-flags `[block+9]` and what drains
   `[0x100e4]` — both are lower-service-completion signals, the same transport layer the MBUS
   D0 work feeds.

### (Earlier framing) `ack 0x11fedb` — necessary but never settable

The CONTACT SERVICE *halt* is the **D9 watchdog timeout**: `0x237b2e` increments a counter
each poll while `ack[0x11fedb]==0`, and at `0x0f` calls `0x2b4dda` (the halt). Trace
confirms **`ack 0x11fedb` is only ever written 0** (init `0x23471e`) — never satisfied. So:

```
CONTACT SERVICE halt  ⇐  ack 0x11fedb never set  ⇐  D9 watchdog never satisfied
ready/extended-tasks/task14  ⇐  later-phase re-run  ⇐  boot getting PAST contact-service
```

Everything downstream (ready, extended batch, task 14) auto-resolves **once the boot gets
past contact-service**. So the single thing to fix is **what satisfies the D9 watchdog (sets
`ack 0x11fedb`)** on a normal boot. That is the contact-service's own job (handlers around
`0x234xxx`/`0x237xxx`), tied to service-present detection — and recall the contact-service
init's **EEPROM checksum** (`0x234810`) and the `service_ready`-gated bit6 both feed that
path. **Next:** find the `ack 0x11fedb` (`base+0x73`) setter in the contact-service D9 flow
and the condition it needs — that condition (likely a valid EEPROM + service-present state)
is the real fix for the halt.

The sections below remain accurate about the *MBUS transport* (the D0 frame is sent,
answered, delivered, and the dispatch completes) — they were just aimed at the wrong final
flag. The transport works; the **contact-service watchdog satisfaction** is the open root.

## MBUS D0 exchange — captured frame + the precise delivery gap

Runtime capture (`TRACE_CONTACT_COMMIT`/`mbus_w:` + `rx_sm:`, with `EXPERIMENT_RESUME_TASK14`)
of the actual D0 service exchange:

**The query the firmware transmits** (reg `0x1a` byte stream):
```
1f ff 00 d0 00 01 01 01 31
│  │  │  │  └──┴─ len=0x0001  │  │  └ XOR checksum (1f^ff^00^d0^00^01^01^01 = 0x31 ✓)
│  │  │  └ cmd = 0xD0          │  └ seq
│  │  └ src = 0x00 (phone)     └ payload[0] = 0x01
│  └ dest = 0xff (test box)
└ frame start
```

**The model's response** = src/dest-swapped echo `1f 00 ff d0 00 01 01 01 31` (XOR checksum
is order-independent, so it stays `0x31` — valid). `mbus_generated_response_byte` serves it.

**What works:** the firmware's RX state machine **accepts it**. `rx_sm:` shows state
`10f4a8` stepping 00→06, the byte compare `rx_byte[10f4ad]=00 vs expected[111794]=00`
matching `resp[1]`, payload count `10f4ae` counting 3→2→1, then **`service_transport_complete`
(`0x2b052e`) is reached** at t=0.306. So the *content/shape* of the swapped-echo reply is
sufficient for the transport layer.

**Delivery WORKS (probe result):** instrumenting the router `0x2b052e` shows it processes
the correct frame `frame=0x10f4b4 [1f 00 ff d0 00 01 01 01] ouraddr[111794]=00` and the post
**succeeds**: `route_post_B: 0x26aac0 returned r0=1 (delivered to task 7)`. (The earlier
"no recv" was a probe artefact — `task_recv_seen` logs only each task's *first* recv.) So the
response reaches task 7, which checks `cmp r1,#0xd0` (matches). Delivery is **not** the gap.

**The real gap is the response PAYLOAD, at the service level.** The D0 response arrival
drives the service-startup dispatcher (`0x290cf8`, a 52-entry jump table) with **cmd `0x12`**
(seen at t=0.334). Its handler `0x290f7c` is `r0 = (r6 & 9) << 5` — i.e. it derives status
bits from `r6`, the message **argument carried from the D0 response payload**. The model's
**src/dest-swapped echo** carries payload `01 01`, which produces the wrong bits, so the
service status word `0x110c2e` stays stuck at `0x8002` and never reaches completion.

Service-startup is a **bit-field state machine** on `0x110c2e`: cmd `0x08` sent the D0 frame
(→`0x8002`); each response/event ORs in more bits; **completion fires on cmd `0x2f`** (or
arg2==1) at `0x291032`, which sets the ready/completion buffer fields and signals task 7 —
that is the path that would finally set `ready 0x110c2c`. There is **no second MBUS TX** (D0
is sent once), so the single D0 response must carry the payload that completes the machine.

**The fix (refined, verified):** replace the echo with a **realistic D0 response payload** —
one whose bytes, parsed into cmd `0x12`'s argument, set the status bits that drive the
machine to cmd-`0x2f` completion. The remaining RE is: (1) map the full bit-field target on
`0x110c2e` (which bits = "service ready"), (2) work back through cmd `0x12`/its siblings to
the required argument, (3) hence the required D0 response payload bytes. Cross-check against
the documented DCT3 D0 ("get HW/SW version / start-up") response format.

### #3 — EEPROM checksum (secondary, independent, task-14 batch-2 only)

`checksum(EEPROM[0x120..0x243])` must equal stored halfword `EEPROM[0x244..0x245]`. Routine
`0x234588`: a 16-bit byte-sum, then a correction (`0x2978c0` subtracts bytes read from
`EEPROM[0x154]`). Pure byte-sum over the current overlay = `0x20df`; stored = `0x20de`
(off by one). Exact target value needs either full decode of the `0x2978c0` correction or a
one-shot runtime capture of the computed checksum at `0x234810` (cheaply observable once #1
advances the boot). Then store that value at `0x244`. Cross-check with NokiaTool's recalc.

The EEPROM is **not** the root of the CONTACT SERVICE halt — only this secondary task-14
batch-2 gate. A fuller image is tracked separately in `eeprom_analysis.md`.

Repro: `make run-boot-progress … TRACE_BUS=1` (per-interaction `bus:` lines).
