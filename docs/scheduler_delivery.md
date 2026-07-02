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

**Why our boot stalls, exactly:** we clear CONTACT SERVICE by *faking* one node-`0x18` response
(`MODEL_SVC_RESPONDER`) instead of driving the real command loop. So `0x236bac` is never invoked with a
`message[+9]` whose bit 2 is set → `0x15` is never emitted → bit `0x04` never sets → the gate (`flag==0xf`)
never closes. This is the **same boundary as genuinely clearing CONTACT SERVICE** (processing real
service-bus messages vs faking a reply) — now pinned, at the instruction level, as the gate on `000d` too.
The narrow remaining question is what real service command/`message[+9]` a provisioned boot delivers to set
bit 2 — a provisioned-data / real-service-bus question, not a modelling gap in the driver.

## Reusable disassembly

`.venv/bin/python tools/disrom.py <addr>:<len>` (or `NOKI_BIN=roms/<img>_swap16.bin`). Key functions
mapped here: recv wrapper `0x26ff14`, recv primitive `0x26a458`, immediate post `0x2695f4`, delayed post
`0x2697aa`, wheel insert `0x2699be` / service `0x269acc`, recode table `0x2d71a8`, `000d` dispatcher
`0x270e1c`. Structures: TCB base `0x100020` (running-task idx `+2`, mask `+4`), ring array `0x101484`
(`0x1c`/task), timer wheel `0x1093bc` (`0x10`/slot), ECB table `0x100140` (`0xc`/event), trace port
`0x600100`.
