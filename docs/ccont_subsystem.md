# CCONT subsystem — protocol map, hardware meaning, and component design

The living map of the Nokia 3210 **CCONT** power-management subsystem: what the chip is in
real hardware, the firmware↔chip protocol (serial register access + the interrupt→event/message
fan-out), confidence-tagged names for every signal, and the target C++ component. This is the
artifact the post-boot modelling work maps against — the boundaries and names here are meant to
outlive the boot effort, because CCONT keeps talking during normal operation (charger plug/unplug,
battery polling, RTC alarms).

See also: `hardware_atlas.md` (the whole MMIO boundary), `service_bootstrap.md` "Beyond the gate"
(the startup mode machine that consumes these signals), `driver_vision.md` (the knob→model map;
CCONT is retirement row #1).

## Confidence tiers

Every fact below is tagged. The tiers are the whole point — post-boot work needs to know what to trust.

- 🟢 **Proven** — firmware behaviour **and** an external source agree.
- 🟡 **Inferred** — consistent firmware-internal evidence only (disasm + runtime), no external confirmation.
- 🔴 **Guessed** — plausible placeholder, not yet evidenced.

## The chip (real hardware)

🟢 **CCONT = "Current Controller,"** the DCT3 power-management ASIC. The DCT3 baseband is four chips:
**MAD2** (TI, ARM7TDMI MCU + DSP), **CCONT** (power / ADC / RTC / charge-monitor / watchdog),
**CHAPS** (the charging switch/pump — does the actual charging), **COBBA-GJ** (audio + RX/TX I/Q
codec). CCONT identifies the battery by reading the **BSI** (Battery Size Indicator) line through its
ADC, monitors charger voltage, and its **watchdog block controls the power-on/off sequence**.
Sources: [DCT3 platform — LPC wiki](https://lpcwiki.miraheze.org/wiki/DCT3_platform),
[Nokia NSE-3 service manual](https://www.manualslib.com/manual/1226334/Nokia-Nse-3-Series.html).

🟡 Boundary with neighbours: **CHAPS** does the charging (CCONT only *monitors* charger voltage and
raises the charger-detect interrupt); **COBBA** does audio/RF codec (not CCONT); the **watchdog** that
DISABLE_CCONT_WATCHDOG suppresses lives in CCONT. So "battery/charger/RTC/power-rail" = CCONT;
"actually pushing charge current" = CHAPS.

## Transport — serial register access (GENSIO)

🟢 CCONT is a serial peripheral on the MAD2 **GENSIO** block: write at MMIO `0x2c`, read at `0x6c`,
transaction start/status at `0x2d`/`0x6d` (see `hardware_atlas.md`). The driver already models this
(`nokia_ccont_w` / `nokia_ccont_r`). 🟡 A command word selects a register `addr = (cmd>>3)&0xf` and
read/write direction; the firmware's `ccont_reg_read` helper is `0x2afb44`, write setup `0x2b5ae4`.

## Register file (reg 0x0–0xf)

From `nokia_ccont_r/w` + access sites. 🟡 unless tagged.

| reg | role | notes |
|---|---|---|
| `0x0` | control | |
| `0x1` | PWM / charger control (CHAPS drive) — **write-only** | 🟢 charger-control register |
| `0x2`/`0x3` | ADC result LSB / MSB | 🟢 ADC readout |
| `0x5` | watchdog (WDReg) | 🟢 the CCONT watchdog; gated by `DISABLE_CCONT_WATCHDOG` |
| `0x6` | RTC enable | |
| `0x7`–`0xa` | RTC sec/min/hour/day | 🟢 RTC, served from host clock |
| `0xb`–`0xd` | RTC alarm / calibration | |
| `0xe` | **interrupt status** (lines) | 🟢 the IRQ source the ISR reads (arg `0x90ff`); bit 0 = present-status (idx6, `MODEL_CCONT_PRESENT`) |
| `0xf` | **interrupt mask** | 🟡 read by the ISR (arg `0x11ff`) as `status & ~mask` |

## ADC channels (read via reg 0x0/0x2/0x3)

🟢 channel→signal (firmware reads + external BSI/charger confirmation); 🟡 exact scaling.

| ch | signal | hardware meaning |
|---|---|---|
| 0 | accessory detect | headset/accessory line |
| 1 | RSSI | RF signal strength (from COBBA path) |
| 2 | **battery voltage** | main Vbat |
| 3 | **battery type / BSI** | Battery Size Indicator line — 🟢 identifies the battery |
| 4 | **battery temperature** | BTEMP thermistor |
| 5 | **charger voltage** | =0 → "no charger" (the mode-`000d` cause) |
| 6 | VCXO temperature | oscillator temp |
| 7 | charging current | CHAPS current sense |

## The interrupt → event/message fan-out (the core protocol)

🟢 **`0x2b08c6` is the CCONT interrupt service routine** — the whole chip→firmware signal funnel.
It reads CCONT **interrupt-status reg `0xe`** (`& ~`mask reg `0xf`), keeps the **top 5 bits** (`0xf8`),
and per active bit posts a **startup event** (to the mode machine) and/or sends a **`0x77xx` PMM
message** (to subscriber tasks), plus sets a result-selector byte:

| int bit | → startup event | → PMM msg | handler / meaning |
|---|---|---|---|
| any set | `0x15` (🟡 "CCONT measurement IRQ") | `0x7701` (status report) | generic "a CCONT line fired" |
| bit 3 | `0x16` (🟡 charger) | `0x7706` | 🟢 **charger detect** → `charger_present_check 0x2b084c` |
| bit 4 | — | `0x7704` | 🟡 measurement result, selector=1 |
| bit 5 | — | `0x7703` | 🟡 measurement result, selector=2 |
| bit 6 | — | `0x7702` | 🟡 measurement result, selector=4 |
| bit 7 | — | `0x7705` | 🟡 → `0x29b71e` |

🟢 The `0x77xx` messages go through a generic IPC send primitive **`0x2b13a2(type, code, payload)`**
(`type` 0/1/2 = payload size; `0x2b5b24`/`0x2b5b3c` are the 1-byte / 2-byte wrappers; `0x2b12b4`
checks the channel is enabled via the flags at `0x11fee4`/`0x11ff08`, `0x2b12dc` delivers). So
**`0x77` is the power-management message family** and delivery is gated by channel-enable state — which
is exactly why some results don't reach a given subscriber on a blank phone.

🟡 **The startup-machine meaning of the gate events.** Mode `000d`'s four sub-events `0x14/0x15/0x16/0x17`
(flag byte `0x112399`, see `service_bootstrap.md`) are believed to be **CCONT measurement reports**, but
their exact source is **open** — see the Correction section below (an earlier "they come from emitter
`0x264f30`" conclusion was falsified; they appear to be scheduler-internal events from `0x26a458`).

## ⚠️ Correction — the emitter `0x264f30` is NOT the source of the surfaced sweep ids (falsified)

An earlier pass concluded the `000d` sweep events came from emitter `0x264f30` (called from
contact-service init `0x2347c6`, reading CCONT, posting task-3 messages), with dispatched id `[msg+4]`
class-gated by `[msg+2]` (classes `0x0e`/`0x1a` pass, `0x06`/`0x16` drop). **Two experiments + timing
falsified all of that — recorded here so it isn't a future bum steer:**

- 🟢 **Timing disproves the emitter as source.** The dispatched ids `0x17` (t≈0.337) and `0x14`
  (t≈0.373) are dequeued **before** emitter `0x264f30` posts its messages (t≈0.41–0.42). The emitter's
  own messages (which happen to *contain* the bytes `0x14`/`0x16`) are never dequeued as those ids — the
  `[msg+4]` match was coincidental. `0x264f30` is real (it does post CCONT-filled task-3 messages) but
  it is **not** what feeds the `000d` gate.
- 🟢 **Class-rewrite disproves the class-gating model.** Rewriting the trapped `0x15`-message's class
  `0x16 → 0x1e` (bit 3 set) *and* `0x16 → 0x1a` (a "known pass-through" class) at post time both left
  `0x15` un-surfaced and the flag at `0x08`. So neither "bit 3 of class" nor "class membership" gates
  surfacing. The whole `[msg+4]`/class model is withdrawn.
- 🟢 **The surfaced ids aren't task-3 message posts at all.** With the mode filter removed, the task-3
  posts before t≈0.38 carry classes/events like `0a 05 1e` and `50 51 22` — **none** with `[+4]=0x14`
  or `0x17`. Yet `0x14`/`0x17` *are* dequeued. So `0x26a458` surfaces them from an **internal scheduler
  source** (timer / system events), not from `0x26a204` message posts.

## What still stands (solid)

- 🟢 The `000d` flag (`0x112399`) stalls at `0x08`–`0x09`; event `0x15` **never** surfaces as a
  dispatched id → bit 2 never sets. (Reproduced every run.)
- 🟢 `0x17` reliably surfaces (bit 3); `0x14` surfaces intermittently (bit 0) — both from the scheduler
  `0x26a458`, **before** t≈0.38, not via task-3 posts.
- 🟢 The CCONT ISR `0x2b08c6` posts `0x15`/`0x16` only as `0x2697aa` *events* (a different channel from
  the mailbox the `000d` handler reads) plus `0x77xx` PMM messages. (Unchanged.)

## Corrected open question (the real lead)

**The dispatcher `0x26a458` — correct structure (Ghidra, after capstone desynced).** Decompiled locally
(`ghidra_out/.../sched_recv_26a458.c`; Ghidra output is git-ignored — read, never commit). It is the
per-task receive that returns the next event id, from **three queues** on the task record (`task*0x1c`):

1. 🟢 **Timer/delay queue** `[task+8]`: pops a delay node, returns `table[0x2d71a8 + [node+9]*8]`. That
   table maps index→id **`0xc0…0xd7`** — the timer/system family (the `0xc3` tick, the `0xd5` CCONT
   event). (My earlier `0x2d71a8` guess was right; the desync didn't reach it.)
2. 🟢 **Ring A** `[task+0x14]` (head `[+0x19]`, tail `[+0x18]`): returns `ringA[idx]`.
3. 🟢 **Ring B** `[task+0xc]` (head `[+0x11]`, tail `[+0x10]`), gated by `([task2+0xf] & 1)==0`: returns `ringB[idx]`.

The translator `0x26ff14` is a big near-identity switch on that id; the **`0xd5` case** is special — it
calls the **CCONT ISR `0x2b08c6`** then `sched_post_2697aa(0x15, …)` (so a dequeued `0xd5` re-emits
event `0x15` on the `0x2697aa` channel — the wrong one for `000d`).

## The four `000d` sweep producers — RESOLVED (Ghidra named corpus)

The ring-ref hunt found all four, already named by prior analysis, **all CCONT/charger** — which finally
nails the `000d` gate's meaning:

| event | producer (named) | addr | what it is |
|---|---|---|---|
| `0x14` | `startup_event14_source7_{absent,present}_producer` (+ `..latch_and_schedule_2a0fae`) | `0x2abdc0`/`0x2abde4` | 🟢 the **charger "source 7" present/absent** event (cf. the `7=absent->go` limp note) |
| `0x15` | `ccont_battery_init_post_event15` | `0x2b09f2` | 🟢 CCONT **battery-init** (also writes the charger latch `0x1124c9`) |
| `0x16` | `ccont_irq_charger_event16_payload6` | `0x2b0958` | 🟢 the CCONT **ISR charger** (bit-3) path |
| `0x17` | `ccont_init_post_startup_event17` | `0x2af086` | 🟢 CCONT **init** startup event |

So the `000d` gate **= "wait for the CCONT power-on/charger sweep"** (charger-source7 + battery-init +
charger-IRQ + ccont-init). That's the faithful name, and it makes the `ccont_device`'s role concrete:
own these signals.

🟢 **The surfacing asymmetry — confirmed at the producer level (per-event post channel).** The `000d`
handler *entry* calls **both** `0x2b09f2` (→`0x15`) and `0x2af086` (→`0x17`) right before the dispatch
loop (clean disasm `0x270e0e`/`0x270e18`), yet `0x17` surfaces (bit 3) and `0x15` does not (bit 2). The
difference is the **post channel**, now read directly from each producer:

| event | producer | posts via | channel | surfaces to `000d`? |
|---|---|---|---|---|
| `0x14` | `0x2abdc0`/`0x2abde4` | `startup_event14_latch_and_schedule 0x2a0fae` | direct schedule | ✅ |
| `0x15` | `0x2b09f2` | `sched_post_2697aa` (`0x2b0a12`) | **delayed/timer** | ❌ |
| `0x16` | `0x2b0958` | `sched_post_2697aa` | **delayed/timer** | ❌ |
| `0x17` | `0x2af086` | `sched_context_post_message 0x26a354` | direct message | ✅ |

So the two events that **never surface** (`0x15`/`0x16`) are exactly the two on the **delayed `0x2697aa`
channel**; the two that surface (`0x14`/`0x17`) use **direct** channels (`0x26a354` / `0x2a0fae`). Runtime
matches: the `limp2_evpost` trace shows `0x2697aa` called with `0x15`(`ev=21`)/`0x16`(`ev=22`) but never
`0x14`/`0x17`. The `0x2697aa` posts schedule through the timer mechanism and reflect as the `0xd5` timer
event (whose `0x26ff14` case re-posts `0x15` via `0x2697aa` again). **That the reflection never lands a
standalone `0x15` in the ring is now 🟢 confirmed** — see "The `000d` wall" below; it is no longer an
inferred step.

**So the faithful question is now precise:** does a *provisioned* phone close the `000d` gate at all via
`0x15`/`0x16` (delayed) — or does the gate only need `0x14`/`0x17` plus the `CCONT_STATE==6` condition,
with `0x15`/`0x16` being a red-herring requirement on a blank unit? That is the next thing the
`ccont_device` work must answer (likely by feeding real CCONT measurement values and watching the flag),
rather than forcing events.

⚠️ Several of these producer **bodies don't decompile cleanly** even in Ghidra ("bad instruction data" —
a Thumb-decode issue), so we rely on the **names** + the clean caller-side disasm.

## Is `0x15`/`0x16` delivery provisioning-gated? — investigated: NO (but a bigger finding)

Dumped, at `000d`, the per-event router records (`0x100140 + ev*0xc`) for all four sweep events and the
channel-enable/provisioning flags. Result:

```
chan_enable[0x11fee4]=0000   mask[0x11ff08]=00000000
ev=14 rec@100230: +6=01 +7=01 +8=01 +9=14 +a=00
ev=15 rec@10023c: +6=01 +7=01 +8=01 +9=15 +a=00     ← byte-identical to 0x14/0x17
ev=16 rec@100248: +6=01 +7=01 +8=01 +9=16 +a=00
ev=17 rec@100254: +6=01 +7=01 +8=01 +9=17 +a=00
```

🟢 **Not provisioning-gated at the event level.** The records for `0x14/0x15/0x16/0x17` are byte-identical
— nothing singles out `0x15`/`0x16`. The delivery asymmetry is purely **structural**: the producers use
the *direct* primitives (`0x26a354`/`0x2a0fae`) for `0x14`/`0x17` and the *delayed* `0x2697aa` for
`0x15`/`0x16`. Same firmware code on any phone — so a provisioned unit wouldn't deliver `0x15`/`0x16`
differently *at this level*. (Probe: `limp2_prov`, opt-in under `TRACE_LIMP2`.)

🟡 **Observation:** the **channel-enable flags are still `0`** at `000d` — we cleared CONTACT SERVICE via
the *responder trampoline* (faking node-0x18), not by provisioning those flags. This *looked* like it
implied "provision, don't force" (clear CS via real provisioning state so later gates inherit
consistency).

⚠️ **That implication was TESTED and REFUTED (commit follows).** Ran two experiments:
- **Provision the enable flag, drop the responder.** `0x11fee4` is **read-only from the firmware**
  (it never *writes* it), so the existing `FORCE_SVC_CHANNEL` write-hook is **broken** — it never fires.
  Forcing the *read* of `0x11fee4` non-zero (`EXPERIMENT_PROV_READ=0x0100`) + `CLEAN_SVCCHAN`, no
  responder → **CONTACT SERVICE does NOT clear** (frame stays `d8a9a7`). So provisioning the enable flag
  *alone* is insufficient; the service-node **response** is still required.
- **Mode vs display are decoupled.** The startup mode machine reaches `000d` **regardless** of the
  responder/CONTACT-SERVICE state — the responder only toggles the bit-6 *display* (CONTACT SERVICE
  screen vs the limp), not the mode machine.

**Corrected conclusion:** "provision instead of force" is a **false dichotomy**. (1) Clearing CONTACT
SERVICE fundamentally requires *modelling the service-node response* — a runtime handshake even a
provisioned phone performs — so the responder trampoline is **modelling a real response, not faking
provisioning data**. (2) The `000d` `0x15`/`0x16` events are **startup events** (`sched_post_2697aa`), a
**different subsystem** than the service-channel enable flags — so provisioning those flags **cannot**
affect the `000d` gate. The `000d` blocker is **structural** (the delayed-channel `0x15`/`0x16`),
**independent of provisioning**. The "reached in an artificial state" observation is true but does *not*
gate `000d`. (Cleanup note: `EXPERIMENT_FORCE_SVC_CHANNEL` was a broken write-hook — since removed.)

## Trying to set the enable flags via a `0x70` channel-map response — traced, dead-end (probe `svc70`)

Idea: make the firmware set `0x11fee4` itself by delivering a command-`0x70` (channel-map) response through
the responder, instead of forcing the flag. Traced end-to-end (with `SVC_RESPONDER_B9=0x70`):

- The responder's injected message routes **task 02 → `0x237400` (dispatch) → `0x236dc4` (response
  handler)**, with `r0 = command`. Command `0x05` (the completion) is in `0x236dc4`'s **jump table for
  commands `0..0xa`**; the responder works because `0x05 ≤ 0xa`.
- 🟢 Command `0x70` (`> 0xa`) takes `0x236dc4`'s **generic high-command path `0x236e60`**, which calls the
  config writer `0x2b140a` with **all-zero args (`r0=r1=r2=r3=0`)** — that's a **reset/clear**, not an
  apply. So `0x70` via the responder *clears* the enable flags (they stay `0`), confirmed by the trace.
- 🟢 The **real channel-map apply `0x2366c8`** (which reads the map from a message and sets `0x11fee4`) has
  one caller, `0x23674a`, inside the **specific `0x70/0x71` handler `0x23670c`** — which is dispatched from
  **`0x237816`** (the contact-service task's own command loop), a **different path** the responder never
  uses.

**Conclusion:** the responder can deliver the *completion* (`0x05`, jump-table) but **cannot** deliver the
*channel-map* (`0x70`) — they go through different dispatches, and the responder's path routes `0x70` to a
reset. Setting the enable flags faithfully would need injecting into the contact-service command path
(`0x237816`/`0x23670c`) with the real `0x70` message format **and** the channel-map data (still unknown for
a 3210 — the `0x2366c8` apply reads it from the message payload). So "model the `0x70` response" needs both
a different injection point and the provisioned map data; the simple responder-extension does not reach it.

## The `000d` wall — a missing peer response, below corpus resolution (ROM disasm + runtime traces, 2026-07)

The `000d` advance is **blocked, but not by hardware**: it waits on a request/response handshake
(`task_285`) whose peer never answers on our blank+faked boot — architecturally like CONTACT SERVICE, so
modellable *in principle*, but the exact peer/response/context sits below what the (garbage-decompiled)
corpus can resolve, and every forced stand-in failed (details at the end of this section). The confirmed
ROM mechanism, settled several ways:

🟢 **The gate is a literal compare on the received code.** Mode-`000d` (`0x270e1c` loop → dispatch
`0x270e22`) `cmp`s the code returned by the recv wrapper `0x26ff14` and ORs a bit into flag `[0x112399]`:
`0x14→0x01, 0x16→0x02, 0x15→0x04, 0x17→0x08`; it advances (`0x270edc`) only when `[0x112399]&0xf==0xf`.
Nothing else sets those low bits, and the recv wrapper passes `0x14–0x17` through **unchanged** (they're
absent from its translate ladder). So the gate genuinely needs the startup task to **receive standalone
`0x15` and `0x16`** — it is not satisfiable by CCONT state, a timer, or provisioning flags (all falsified;
`c` and `b` below).

🟢 **The firmware only ever posts `0x15`/`0x16` on the *delayed* primitive.** There are two post
primitives: `0x2695f4` (immediate → pushes `{task,event,arg}` into the running task's mailbox, visible at
`0x26a458`) and `0x2697aa` (delayed → inserts a per-TCB timer-wheel node with a delay: `0x15`→`0x20a1`=8353
ticks, `0x16`→6). Event `0x14` (`0x2a0fae`) calls **both**; `0x17` (`0x2af086`) uses a direct message path
and never touches `0x2697aa`. But `0x15` (`0x2b09f2`@`0x2b0a12`, `0x2b08c6`@`0x2b0900`) and `0x16`
(`0x2b08c6`@`0x2b095c`) are **hardcoded delayed-only** — no CCONT register value routes them through the
immediate primitive (option **(b)** refuted by scanning every BL in `0x2b0840–0x2b0a20`).

🟢 **The delay-queue drain never re-injects the raw code.** `sched_delay_queue_service 0x269acc` (timer-tick
driven, 17 scheduler-internal call sites) matures a wheel node and surfaces it as the generic `0xd5` poll;
the `0xd5` case in `0x26ff14` (`0x26ff6a`) re-runs the CCONT dispatch and **re-posts `0x15` via delayed
`0x2697aa` again** — a closed loop. So "just let it mature" (option **(a)**) does not turn `0x15`/`0x16`
into raw ring codes. The delayed post *can* also wake a waiter immediately, but only via a gated branch
(`0x2697f2: ands r2,r0; lsrs r2,#2; blo`) that needs the startup task **registered as a waiter** on the ECB
**and** `(TCB.mask 0x100024 & ECB.flags +7) != 0`.

🟢 **Runtime proof (`limp2_ecb`, deep boot, no forcing, 24s).** At every delayed post of `0x15`/`0x16`:
`ECB.flags[+7]=0x01`, `TCB.mask=0x00000100` → `mask&flags = 0x100 & 0x01 = 0` (waiter branch never taken),
and `waithead[+0]=ffffffff` (no task waiting). Dequeue trace: `0x14` (t=0.37), `0x17` (t=0.33) and even
`0x16` (once, t=0.83) arrive as raw codes, but **`0x15` never dequeues — not once** — so `[0x112399]` never
gains bit `0x04` and never reaches `0x0f`. This holds **even with `CCONT_EVENT15_DELAY=1`** shrinking the
`0x15` delay to 1 tick: maturity still only reflects as `0xd5`, confirming the drain never re-injects.
A 90s run likewise produced **no `4235fa`** (post-gate) frame — only the display-init limp `94a2dc`/`4aab13`.

**What the `0xd5` poll actually is (deeper trace).** The `0xd5` is **not** a CCONT-IRQ poll — the CCONT
status settles fast (reg `0xe` reads `0x08` once then `0x00`; `r4 = (status & ~mask) & 0xf8 = 0` almost
immediately, measured via `TRACE_CCONT_READ`). It is a **request/response state machine** ("task_285",
driver loop `0x285df4`, handler `0x2a9964`, control block `0x11228c`: `+1` retry, `+2` sub-state, `+4`
retry-limit, `+0x16` done-flag). It fires a request, arms a `0xd5` timeout, retries on `0xd5`, and completes
only when a **peer sends a terminal response opcode** (`0x0db3`/`0x0dc2`/`0x0dc3`/`0x0dc4`/`0x0daf`/`0x09ca`)
into its mailbox — which sets `[0x11228c+0x16]` and stops re-arming. The `0xd5` handler (`0x26ff6a`)
re-runs the CCONT dispatch and re-posts `0x15` at delay `[0x270168]=8353` **unconditionally** on every
`0xd5`. So on our **peerless** boot the machine retries forever, flooding `0xd5` (~every 0.11 s) and
re-arming `0x15` long. This is architecturally the **same shape as CONTACT SERVICE** — a missing peer
response — i.e. modellable *in principle* (cf. the node-`0x18` responder), not a raw hardware limit.

**But every lever that "should" advance it was tested and FAILED (2026-07).** The blocker is deeper than
timing or the poll:
- **Stop the `0xd5` re-post** (`EXPERIMENT_SUPPRESS_D5_REPOST`, since removed): `0x15` still never delivered.
- **Force *every* `0x15` post to delay 1** (so a node matures instantly, defeating starvation entirely):
  `0x15` **still never dequeues as a raw code**; boot stays at `000d`. So it is **conclusively not
  starvation** — a maturing `0x15` node does not reach the startup task's mailbox regardless of delay.
- **Force `task_285`'s done-flag** `[0x11228c+0x16]=1` during `000d`: no effect (the `0xd5` stream and stall
  were unchanged), so either that control-block layout is wrong (bodies are Thumb-decode garbage) or the
  flag is not the real gate.

**Honest classification.** The delivery of a matured/pending `0x15` to the startup task depends on scheduler
**context** (the owning task current with a matching mask, or a registered waiter — `mask&flags` was `0` and
the waiter list empty at every observed post) that our blank+faked boot never reaches; the upstream cause is
`task_285` never getting its peer response. **This is NOT proven to require live hardware** (that earlier
claim is retracted). But the exact peer, response, and context sit **below the reliable resolution of the
available corpus** — the scheduler-delivery and `task_285` bodies decompile to garbage, so we identified the
*machine* but not the sender of the terminal `0x0dxx` response, and no forced stand-in advanced the boot.
Closing this faithfully would need **cleaner decompilation of the scheduler/`task_285` path or a reference
boot/RAM trace from a working, provisioned 3210** (to observe the real peer response and task context) — a
*better reference*, not necessarily live silicon. `EXPERIMENT_FORCE_000D_EVENTS` injects codes the firmware
**never** injects on this path; it is a **diagnostic preview** of post-gate boot, explicitly not faithful.
Reproduce the evidence with `TRACE_LIMP2=1` / `TRACE_CCONT_READ=1` (probes `limp2_ecb`/`limp2_deq`/`ccont_r`).

## Open questions (the mapping backlog)

- 🔴 ~~emitter `0x264f30` / `[msg+4]` offset / class-gating~~ **Falsified** (see the Correction section)
  — the surfaced sweep ids come from an internal scheduler source in `0x26a458`, not task-3 posts.
- 🟢 ~~What internal source in `0x26a458` emits the surfaced `0x14`/`0x17`, and why no `0x15`?~~ **Answered**
  (see "The `000d` wall"): the immediate primitive `0x2695f4` pushes `0x14`/`0x17` into the mailbox; `0x15`
  is delayed-only and its waiter branch is gated off (`mask&flags=0`, empty waiter list) → wheel-only `0xd5`.
- 🟡 Exact CCONT command-word encoding (the `0x90ff`/`0x11ff`/`0x9001` arg format → reg + mask).
- 🟡 The result-selector byte at `0x1124d2`(?) and how `0x77xx` results map back to ADC channels.
- 🟡 Which task subscribes to `0x77xx`, and why `0x15`/`0x16` reach (or don't reach) the startup task (the routing records `≈0x100140`).
- 🟡 ADC conversion timing — how long after a request the completion interrupt should fire.
- 🟢→ confirm reg `0xf` is the mask (vs another role) by a runtime read at the ISR.

## Target C++ component

A MAME `ccont_device` (the PCD8544 LCD is already a real device, so this fits the grain). Staged:
a cohesive driver-internal `ccont` class first, promoted to a `device_t` once stable.

```cpp
class ccont_device : public device_t {
public:
    // GENSIO serial transport (MCU clocks command+data) — replaces nokia_ccont_r/w
    u8   reg_r(u8 addr);  void reg_w(u8 addr, u8 data);
    auto irq_cb() { return m_irq.bind(); }      // IRQ line out -> MAD2 CTSI
    void set_scenario(power_scenario s);         // no-charger / charging / charged (data, not knobs)
protected:
    void device_start() override;  void device_reset() override;
private:
    u8  m_reg[16];                                // register file 0x0-0xf
    u8  m_int_status, m_int_mask;                 // regs 0xe / 0xf  <- the IRQ the ISR reads
    struct power { u16 batt_mv, charger_mv, batt_temp, bsi, vcxo_temp, chg_current, rssi, accessory; }
        m_power;                                  // ADC sources, set by the scenario
    // THE missing piece: an ADC measurement state machine.
    //   MCU requests measurement (reg write / 0x77xx) -> schedule conversion on m_adc_timer
    //   -> on completion: write result reg 0x2/0x3, set the matching int-status bit (0xe),
    //      assert IRQ. This *produces* the 0x14-0x17 / 0x15 / 0x16 stream the startup machine waits on.
    emu_timer *m_adc_timer;
    devcb_write_line m_irq;
    // RTC from host clock (regs 0x7-0xd); watchdog (reg 0x5).
};
```

**It retires** the whole CCONT knob cluster — `ADC_PROFILE`, `ADC0/5*`, `BATTERY_PROFILE`,
`CCONT_EVENT15_DELAY`, `STARTUP_EVENT15_DELAY_CLAMP`, `CCONT_BOOT_STATUS`, `MODEL_CCONT_PRESENT`,
`CCONT_IRQ_*` — collapsing them into modelled state + the measurement timer. **Boundary:** the device
owns power/ADC/RTC/charger-monitor/watchdog and the interrupt it raises; it does **not** own the
startup mode machine (firmware), CHAPS charging current, or message routing (that's the RTOS) — it
just produces faithful interrupts and register values, and lets the firmware react.

## Why this matters past boot

The same interrupt model serves normal operation: plug a charger → CCONT charger-detect interrupt
(bit 3) → event `0x16` + `0x7706`; periodic battery poll → ADC completion interrupts; RTC alarm →
RTC interrupt. Getting the component's boundaries and signal names right now means post-boot features
(charging UI, battery meter, clock/alarm) plug into a real model instead of more forcing.
