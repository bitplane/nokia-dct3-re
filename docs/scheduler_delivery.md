# Scheduler event delivery — the `0x15`/`000d` mechanism, from clean disassembly

Ground-truth Thumb disassembly of the RTOS event/message delivery path, cutting through the
Thumb-**mis**decoded C bodies that misled the earlier `000d` investigation. Method: `tools/disrom.py`
on the swap16 image (`roms/*_swap16.bin`), `offset = addr − 0x200000`. **Instruction bytes decode
directly; 32-bit pool literals are halfword-swapped** — the real value is `(w<<16)|(w>>16)` (verified:
dispatcher pool `0x2711e8` reads `23990011` = real `0x00112399` = the `000d` flag byte). Everything below
is **read**, not inferred, unless tagged 🟡.

## Two delivery channels — and they encode events differently

The producers post a startup event via one of two primitives (both take `r0 = event id`):

| primitive | how it delivers | code the receiver sees |
|---|---|---|
| **immediate** `0x2695f4` | walks the **ECB waiter list** (`ECB = 0x100140 + event*0xc`; state at `+8`, flags `+7`), delivers to the waiting task's ring | the **raw** event id (`0x14`, `0x17`, …) |
| **delayed** `0x2697aa` | inserts a node into the per-task **timer wheel** (`0x1093bc + task*0x10`); on maturity `0x26a458` delivers it through a **recode table** | **`0xc0 + event`** (see below) |

`0x14`/`0x17` reach mode-`000d` as raw codes because their producers call the **immediate** primitive.
`0x15`/`0x16` are posted **delayed-only** (no `bl 0x2695f4` with `r0=0x15/0x16` exists anywhere in the
image), so they always go through the recode table.

## The recode table `0x002d71a8` — `event k → 0xc0 + k`

The recv primitive `0x26a458`, on the ring-delivery path (`0x26a640`), sets its return value from:

```
0026a650  ldr  r1,[pc,#0x2fc]   ; [0026a950] = real 0x002d71a8   (table base)
0026a652  ldrb r2,[r0,#9]       ; node[+9] = message class
0026a654  lsls r2,r2,#3         ; *8 (stride 8)
0026a656  ldr  r1,[r1,r2]       ; code = table[class].word0
0026a658  str  r1,[sp,#4]       ; -> return value
```

The table (decoded from the image) is simply **`table[k].word0 = 0xc0 + k`** for `k = 0x00..0x1b+`:

```
class 0x00 -> 0xc0   class 0x14 -> 0xd4   class 0x15 -> 0xd5   class 0x16 -> 0xd6   class 0x17 -> 0xd7
```

So a **timer/delayed-delivered event `0x15` surfaces as `0xd5`** (`0xc0+0x15`) — never as raw `0x15`.
This is the exact, mechanical reason the earlier experiments failed: shrinking the delay (`delay=1`)
changes *when* the node matures, not *what code it surfaces as* — it is still `0xd5`.

## The recv wrapper `0x26ff14` handles the `0xc0–0xdf` range — and loops on `0xd5`

`0x26ff14` calls `0x26a458`, then translates the returned code. It special-cases the recoded
timer events `0xc0/0xc1/0xc4/0xc6` (deref firmware RAM words), `0xd2/0xd3/0xd4`, `0x72/0x71/0xc8/0xc9/0xca`
(pass through), and **`0xd5`**:

```
0026ff6a  cmp r4,#0xd5 ; bne ...
0026ff6e  bl  0x2b08c6           ; re-run the CCONT IRQ-status dispatch
0026ff72  movs r0,#0x15
0026ff74  ldr  r1,[0x270168]     ; = real 0x000020a1 = 8353
0026ff76  bl  0x2697aa           ; re-post event 0x15 DELAYED again
0026ff7a  movs r0,#0xd5 ; pop    ; returns 0xd5 to mode-000d (which ignores it)
```

Note `0x14`/`0x15`/`0x16`/`0x17` are **absent** from the ladder — a *raw* one would pass through to
mode-`000d` unchanged (that's how `0x14`/`0x17` set their bits). But a *timer* `0x15` arrives as `0xd5`,
and the wrapper's response is to **re-post `0x15` delayed** → recoded to `0xd5` → received again → a
closed loop that never yields a raw `0x15`.

## What this means for `000d`

Mode-`000d` advances only when flag `[0x112399]` reaches `0x0f` by *receiving* raw codes
`0x14/0x16/0x15/0x17` (bits `0x01/0x02/0x04/0x08`; dispatcher `0x270e22`). Given the above:

- Raw `0x14`/`0x17` arrive (immediate producers) → bits `0x01`/`0x08` set.
- `0x15`/`0x16` have **no immediate producer**, and the delayed path recodes them to `0xd5`/`0xd6` →
  bits `0x04`/`0x02` are **never** set by event delivery, on **any** phone running this firmware.

So the gate's bit-`0x04` cannot come from *receiving* event `0x15`.

**Lead checked and REFUTED (`0x2b08c6` disassembled):** the CCONT IRQ-status dispatch computes
`r4 = (ccont_read(0x90ff) & ~ccont_read(0x11ff)) & 0xf8`, posts `0x15`/`0x16` **delayed**, sends `0x77xx`
PMM messages, and writes battery/charger state (`1`/`2`/`4`) to **`0x1121d2`** — it does **not** write the
`000d` flag `0x112399`. So the `0xd5` handler does not set the flag; that lead is dead.

**The sharpened target — which of `0x26a458`'s three rings a node lands in.** `0x26a458` returns `[sp+4]`,
set on one of three ring paths checked in priority order per task (`fp = 0x101484 + task*0x1c`):

| path | source of return value | gate | raw or recoded? |
|---|---|---|---|
| **A** `0x26a4ee` | `*(fp[+0xc] + idx*4)` (indexed msg buffer) | wheel-slot `[+0xf]` bit0; `fp+0x10/+0x11` head/tail | **raw** |
| **C** `0x26a656` | `recode_table[node[+9]]` (`0x2d71a8`) on the `fp+8` **linked list** | `fp+8 != ~0` (**priority**) | **recoded** `0xc0+class` |
| **B** `0x26a5ec` | `*(fp[+0x14] + idx*4)` (indexed msg buffer) | `fp+8 == ~0`; `fp+0x18/+0x19` head/tail | **raw** |

So **only the `fp+8` linked-list ring recodes**, and it has priority. The raw deliveries we observe
(`0x14`/`0x17`, and the one-off raw `0x16`) come through the **indexed message rings** (A/B) — consistent
with `0x17` being posted via `sched_context_post_message 0x26a354` (a *message* post) rather than an event.
The delayed CCONT events (`0x15`/`0x16` via `0x2697aa` → timer wheel) are routed on maturity into the
**`fp+8` recode ring**, so they come out as `0xd5`/`0xd6`. **Next function to trace: `0x269acc`** (the
wheel service) — specifically how a matured node is routed into `fp+8` vs a raw message ring, i.e. what
would make a matured `0x15` land in a raw ring (→ raw `0x15` → `000d` clears). That is the remaining
ground-truth thread; fully digital, no hardware required.

## The `000d` gate strictly needs bit `0x04` (premise confirmed)

Disassembled the full advance gate (`0x270ec6`): it loops until **both** `CCONT_STATE[0x11ff6c] & 0xf == 6`
**and** `flag[0x112399] & 0xf == 0xf`, then `bl 0x2a6942` advances. No looser path — bit `0x04` (from a raw
`0x15`) is genuinely required. The only writer of the flag's low nibble is the dispatcher itself, OR-ing a
bit on receipt of a **raw** `0x14/0x16/0x15/0x17` (`0x270e3e`); `0x2b08c6` does not touch it. So `000d` can
only clear if a raw `0x15` is delivered — yet `0x15` is delayed-only (→ recode → `0xd5`).

**The reconciling fact: raw delivery is conditional, not impossible** — `limp2_deq` caught a raw `0x16`
once, so `0x26a458` *does* emit these raw under some state. `0x269acc` matures a node and delegates the
ring-enqueue to **`0x2aca40(wheelslot+8, node+8)`**; that routing (raw ring vs `fp+8` recode ring) is the
last unknown. Whether it's timing (short-delay nodes caught before classification) or a node-field
condition is next to pin — via `0x2aca40`, or faster, a **runtime probe at `0x26a458`'s three return paths**
(`0x26a4ee`/`0x26a656`/`0x26a5ec`, all address-known) to catch the raw-`0x16` case live and read the node
state that routed it raw.

## Runtime confirmation (`TRACE_DELIV`) + a tooling caveat

Probing the recode delivery live (branch target `0x26a640`, `r0` = the delivered node) confirms the
mechanism: the delivered "node" is the **ECB entry itself** (`0x100140 + event*0xc`), and its class byte
`[+9]` drives the recode. Observed at mode `000d`: `ecb=10023c class=15 → surfaced=d5` and
`ecb=100248 class=16 → surfaced=d6` — i.e. the delayed `0x15`/`0x16` events **are** recoded to `0xd5`/`0xd6`,
exactly as the static table predicts. The raw sweep deliveries (`0x14`/`0x17`, and the one-off `0x16`)
arrive through the buffer paths, not here.

**Tooling caveat (important for any runtime probe of these functions):** the driver's instruction-fetch
hooks fire only when `m_maincpu->pc()` equals the fetched address, which — because the ARM7 pipeline runs
the fetch ahead of the architectural PC — happens **only at branch/call targets and return addresses**, not
mid-straight-line instructions. So probe at a `bne`/`bl` target near the code of interest (e.g. `0x26a640`),
never at an arbitrary mid-function store. This is why the first `TRACE_DELIV` attempt (hooking the mid-line
`str [sp,#4]` sites) saw nothing.

## Resolution: the raw-`0x15` producer IS the contact-service command loop (`0x236bac`)

The whole chain now closes — and my first pass ("`0x15` has no producer") was **wrong**: a `[+2]`-only scan
missed it, because `0x15` is produced with the code at **`[msg+0]`**, by the contact-service command
processor. The full trace:

- **`0x236bac`** — a service **status-word → startup-event translator**. Arg `r0` = a status byte; it emits
  code **`0x15` if bit 2** is set (`0x17` on bit 3, `0x19` on bit 5), gated by `0x2a674c()` and the slot
  (`0x11d3fe`) being empty:
  `0x236bd4 lsrs r0,r4,#2; blo … / 0x236bec movs r0,#0x15; strb r0,[r5]`.
- **Caller `0x237844`**, inside the **contact-service command dispatch `0x237816`** (the same jump table
  that reaches the channel-map handler `0x23670c`): `ldrb r0,[r5,#9]` → the status word is
  **`message[+9]`** of a service command; `bl 0x236bac`.

So the mode-`000d` sweep events `0x15`/`0x17`/`0x19` are **produced by the contact-service command loop from
real service messages** — specifically the status bits in `message[+9]`. (`0x14`/`0x16` have their own
service producers — `0x14`: 8 event/message sites incl. immediate `0x2695f4`; `0x16`: `0x264fc0`. `0x15`'s
*event-channel* posts, by contrast, are all delayed `0x2697aa` → recoded to `0xd5` — which is why the delayed
path never helps; the **raw** `0x15` comes only from `0x236bac`.)

**The command index (decoded):** the contact-service command loop dispatches on **`message[+8]`**
(`0x23741a ldrb r4,[r5,#8]`, then a subtract-cascade). **Command `0x65`** routes (via `0x237492 b 0x237840`)
to the case that calls `0x236bac` — so command `0x65`, with `message[+9]` bit 2 set, is what emits the raw
`0x15`. Tell-tale: entry special-cases `cmp r4,#0x64` — and **`0x64` is exactly the command our
`MODEL_SVC_RESPONDER` injects** (`SVC_RESPONDER_B8=0x64`, the completion that leaves CONTACT SERVICE). So we
deliver command `0x64` (completion) but **not `0x65`** (the sweep-event trigger), a sibling in the same
command family.

**Why our boot stalls, exactly:** we clear CONTACT SERVICE by faking one node-`0x18` response (command
`0x64`) instead of driving the real command loop, so command `0x65` (with `message[+9]` bit 2) is never
delivered → `0x236bac` never emits `0x15` → bit `0x04` never sets → the gate never closes. Same boundary as
genuinely clearing CONTACT SERVICE, now pinned at the instruction level as the `000d` gate too.

**Build attempted (`MODEL_CMD65_RESPONDER`) — and it proved the producer is DEAD-GATED for `0x15`.** We
built a `MODEL_SVC_RESPONDER`-style injection of a command-`0x65` message and verified the mechanism step by
step: the service loop keeps polling for non-completion commands (unlike `0x64`, which stops it), `0x65`
dispatches, and — once the service-ready gate `[0x11fed1]` bit 0 is set (skipped otherwise at `0x23742a`) —
it reaches `0x236bac(status)`. **But the `0x15` emit never fires.** Inside `0x236bac`:
- `0x15` is gated on **status bit 1** (`0x236bd4 lsrs r0,r4,#2` puts *bit 1* in carry, not bit 2), then on
  **`0x2a674c(2) != 1`**. And `0x2a674c` returns `1` for **any even argument** (`0x2a6750 lsrs r0,#1; bhs`
  → even → `r4=1`); the arg for `0x15` is `2` (even). So `0x2a674c(2)` is **always `1`**, and the `0x15`
  emit at `0x236bec` is **skipped on every phone** — structurally dead code.
- (`0x17` uses arg `1` (odd) → `0x2a674c` runs its real path → `0x17` *is* emitted. Consistent with runtime:
  `0x17` delivers, `0x15` never.)

Even overriding `0x2a674c(2)→0` (pure force, not a model), `000d` still did not advance — there is a
further delivery step beyond the emit. **Conclusion:** `0x236bac` is a faithful producer for `0x17`/`0x19`
but **not** for `0x15` — the `0x15` path is dead-gated. So there is **no reachable faithful producer of a raw
`0x15`** in this firmware image: mode-`000d`'s bit `0x04` cannot be set by any real code path here. On a real
phone the gate must be satisfied by state we don't reproduce (a seeded flag, a different provisioned/firmware
configuration, or a mechanism still unidentified). The command-`0x65` responder + gate/`0x2a674c` forces were
reverted (non-working forcing chain); the dispatch decode and the dead-gate finding stand.

**Cross-firmware confirmation — the dead-gate is shared DCT3 design (Nokia 3310 NHM-5 v06.39).** To rule
out a 3210-v06.00 quirk, disassembled the sibling 3310 firmware (BYO, analysed locally, never committed).
The status→event translator is present **byte-for-byte identical in structure** (same `lsrs r0,r4,#2` bit
test, same `movs r0,#2; bl <gate>`, same `movs r0,#0x15; strb r0,[r5]`, then `0x17`/`0x19`). Its gate
function (found by exact entry-signature search) is byte-identical on the deciding path: `push {r4-r7,lr};
adds r5,r0,#0; lsrs r0,r5,#1; bhs +2; movs r4,#1; b <ret>` — i.e. **even arg → return 1**, the same
dead-gate. So on the 3310 too, the `0x15` emit (arg `2`, even) is dead code. This is **structural DCT3
firmware design, not a 3210 bug**, confirmed against an independent image.

**Implication (the real closure).** Real 3310s boot to idle, yet their `0x15` emit is *also* dead — so a
normally-booting DCT3 phone does **not** depend on this translator emitting `0x15`. That strongly implies
our mode-`000d` "limp" (waiting for a raw `0x15`) is an **artifact of the blank + faked boot**, not the path
a real phone takes: the real boot satisfies the `000d` flag by a route our reconstructed/unprovisioned state
never exercises (a seeded flag, a different state on entry to `000d`, or a producer outside the reachable
set). The `000d` code is fully and correctly RE'd; the *stall* is a property of how we reach it, not a
missing model. This is the honest end of the `000d` thread on the data we can obtain.

## The whole post-CONTACT-SERVICE mode chain is service-session-gated (not EEPROM-gated)

Probed whether a synthesized provisioned **EEPROM** could carry the boot to idle. It cannot — the boundary is
the service *session*, not calibration data:

- **EEPROM read pattern:** the boot reads the 24C128 in one early bulk sweep (~t=0.008–0.35s; 637 distinct
  addresses, 604 blank on a virgin unit — the RF/ADC calibration tables). These load into RAM once and are
  **not re-read**; they do not gate the mode chain.
- **Each mode waits on a service event, `000d`-style.** Mode `0007` (`0x271392`): `bl 0x26ff14; cmp r0,#0x74;
  bne <recv>` — spins until it receives event `0x74`. Same recv-wrapper wait as `000d`/`0x15`. Event `0x74`
  *does* have live producers (immediate `0x2695f4` posts at `0x213fcc`/`0x214836`), but they sit in the
  **contact-service command handlers** (the `0x213–0x219` region that also produces `0x14`) — i.e. they fire
  when the **real service-box session processes commands**, which our single faked completion (`0x64`) never
  drives.

**So reaching a genuine idle needs the whole service-box *session* modelled** (the command sequence that
produces `0x14`/`0x74`/… and advances each mode), not a synthesized EEPROM. That is the same boundary as
`000d` (a real service session vs a faked completion), and it needs the real service protocol + provisioned
exchange we can't confidently synthesize. The EEPROM-synthesis avenue is therefore **closed for boot-to-idle**;
the calibration would only matter *after* the mode chain, which we never reach faithfully. This refines the
earlier "reaching idle needs provisioned data" note: the operative missing piece is the **service session**,
not the EEPROM.

## "Bullshit the boot" — marching the startup mode chain (EXPERIMENT_MARCH)

Tested how far a *hollow* boot can be driven by faking each mode's ready-signal (all the mode-advancing
event producers are in-ROM; our stubbed subsystems just don't fire them). The startup is a **shared-code
event-driven state machine**: a master dispatcher (`0x270c8e` recv → mode jump table `0x270ca8`, modes
`0..0xd` → handlers) where each mode is a flag-accumulator or router. `TRACE_MODEWAIT` maps mode→wait-loop;
the jump table gives every handler.

`EXPERIMENT_MARCH` injects each mode's advance event at the recv (`0x26ff1a`). Result — **real forward
motion**: `000d → 0004 → (event 7) → the flag-accumulator phase`, and it **renders the battery-idle frame
`4235fa`**. But it dead-ends there, and the reason is structural: mode 4's event-7 handler runs a big
**subsystem-init sequence** (`0x2af058`/`0x2a26d4`/`0x2794d2`/`0x29797c`/`0x29af90`/`0x2a102c`…) then waits
in a flag-accumulator (`0x2712c0`, flags `0x112390-0x112395`) for events those subsystems are meant to
produce. Faking the events partly fills the flags, but the machine has **consistency/terminal paths** —
e.g. mode `000c`'s "exit" `0x2714fc` is `movs r0,#0xc; bl 0x2b4dda` (the **terminal commit**) + an infinite
recv loop — that fire when the faked signals don't match a coherent subsystem state.

**Takeaway:** boot-to-idle is *drivable* but not cheaply bullshittable — each phase kicks off real subsystem
init (DSP/RF/battery) and then waits for those subsystems to report a *consistent* ready state. Reaching a
clean idle means faking a coherent set of subsystem states + event sequences per phase — a real,
phase-by-phase modelling grind (tractable, but with dead-end risk), not a single injection. The march
(opt-in) is the tool for that grind, and it already advances further than any prior lever.

## Tested: "backward-chain to the startup outcome" is NOT a shortcut

The mode chain ends by committing a one-byte **outcome** to `[0x1150ff]` via `0x2b4dda` (which also parks the
startup task in a `b self` loop — its natural retirement, not a crash). It looked like steering that outcome
to a "success" value might shortcut the per-phase grind. Chased it end to end; it doesn't:

- **The outcome is a power supervisor, not an idle selector.** `0x2a924c` reads `[0x1150ff]` and treats
  `{1,5,6}` as **retry** (bumps `[0x1140ff]`, gives up via `0x2b4e16` after 6), else stores to `[0x1141ff]`.
  Readers of `[0x1141ff]` (`0x29bc96`, `0x2a1140`, …) compare against `0xa`/`0xf`/`0x66` — CCONT/charger
  reason codes. The sibling `0x2a92fc` "readiness loop" is a boolean **ready?** check (fail → re-loop;
  success → returns 1), not a transition to idle UI. The whole cluster is the CCONT power-on/readiness
  arbiter — reaching a given outcome doesn't render idle.
- **Gate1 `0x2a6942`** just classifies charger state (`0x27d654` = `byte[0x113604]`); **the commit
  re-validates** outcomes 2/3/4 via `0x2af9c6` (→ `0x2af858`) and downgrades to `1`/retry on failure. So
  even a forced outcome stays entangled with coherent subsystem state.
- **Event injection is too blunt.** `EXPERIMENT_MARCH` (inject each mode's advance event at recv) drives
  `000d→0004→000c` but the injected events have side effects; adding the `000c` completion event *regressed*
  the render (`4235fa` → limp). Faking events ≠ faking the conditions the real producers check.

**Conclusion:** there is no clean shortcut around coherence. The events the mode chain waits on are produced
by MCU/CCONT subsystem code gated on data conditions; a clean boot-to-idle needs those conditions made true
so the *real* producers fire (side effects intact), phase by phase — confirming the earlier "coherent
per-phase state faking" assessment rather than bypassing it. `FORCE_OUTCOME` (commit override) and
`TRACE_MODEWAIT` remain as opt-in diagnostics for that work.

## Option C (hollow-idle bypass): mapped the normal-boot path to one blocking recv

Rendering the captured frames was the unlock: `4235fa` is a **battery/charger indicator**, not idle —
`94a2dc` blank, `4aab13` all-black (LCD init). The MMI main loop (`0x297fc4`) *runs* (recv's, dispatches,
draws idle when `[0x11f81b]==1`) but sits in the charger/battery-display state, so forcing the idle flag
(`FORCE_IDLE`) does nothing — it's not on the idle path.

The Ghidra symbol names then cracked the mode chain: `0x2711f6`/`0x27124e`/`0x271266`/`0x2712cc` are all
`startup_*charger*`, and **`startup_post74_boot_decision_271364`** forks the boot: `bl 0x2b084c`
(`charger_present_check`, = ADC **channel 5** debounced `> 0x64`) → charger present → `0x271422`
(battery-display); **absent → normal boot** → wait `0x74` → gate `0x2b2f90==0x80` + `[0x11239d]==1` →
commit **outcome 3**. Our ADC model already returns channel 5 = 0 (`sane` profile), so the fork *takes the
normal branch*.

`EXPERIMENT_BOOTPATH` threads it PC-specifically (no side-effecting event injection): fill the `000c`
accumulator flags in RAM + feed event `0xd` → flag-check → `0x271354` (`[0x112398]==0`) → **`0x271364`
reached, charger-absent branch confirmed** (`0x271422` never hit) → `0x27138e` (the `0x74` wait). That's the
deepest the boot has gone. **It stalls there on a *blocking* recv**: `0x26ff14 → bl 0x26a458` suspends the
task on an empty ring, and event `0x74`'s only producers are the service-command handlers `0x213fcc`/
`0x214836`, which don't run — so `0x74` is never posted, `0x26ff1a` is never reached, and MARCH-style feeding
(which needs the recv to *return*) can't reach it. Feeding it needs a real post to the ring (a responder-style
trampoline) or the real producer running.

**Net:** option C mapped the entire normal-boot path down to a single missing event (`0x74`), the same
subsystem-produced-readiness wall as everywhere else — and even past it, outcome 3 commits to the power
supervisor, so idle isn't guaranteed without tracing the supervisor's proceed path. `EXPERIMENT_BOOTPATH`,
`FORCE_OUTCOME`, `FORCE_IDLE`, `TRACE_MMI`, `TRACE_MODEWAIT` are the opt-in tools for this.

## Reusable disassembly

`.venv/bin/python tools/disrom.py <addr>:<len>` (or `NOKI_BIN=roms/<img>_swap16.bin`). Key functions
mapped here: recv wrapper `0x26ff14`, recv primitive `0x26a458`, immediate post `0x2695f4`, delayed post
`0x2697aa`, wheel insert `0x2699be` / service `0x269acc`, recode table `0x2d71a8`, `000d` dispatcher
`0x270e1c`. Structures: TCB base `0x100020` (running-task idx `+2`, mask `+4`), ring array `0x101484`
(`0x1c`/task), timer wheel `0x1093bc` (`0x10`/slot), ECB table `0x100140` (`0xc`/event), trace port
`0x600100`.
