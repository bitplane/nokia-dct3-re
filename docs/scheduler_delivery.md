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

So the gate's bit-`0x04` cannot come from *receiving* event `0x15`. The remaining ground-truth lead is
the `0xd5` handler's **`bl 0x2b08c6`** (the CCONT IRQ-status dispatch): 🟡 that is the plausible place a
real, correctly-conditioned CCONT state would set the flag bit directly (or drive an immediate re-post),
rather than the event ever arriving raw. That is the next function to disassemble and correlate with the
CCONT registers — a concrete, digital next step (no hardware required), now that the delivery mechanism
is nailed rather than inferred.

## Reusable disassembly

`.venv/bin/python tools/disrom.py <addr>:<len>` (or `NOKI_BIN=roms/<img>_swap16.bin`). Key functions
mapped here: recv wrapper `0x26ff14`, recv primitive `0x26a458`, immediate post `0x2695f4`, delayed post
`0x2697aa`, wheel insert `0x2699be` / service `0x269acc`, recode table `0x2d71a8`, `000d` dispatcher
`0x270e1c`. Structures: TCB base `0x100020` (running-task idx `+2`, mask `+4`), ring array `0x101484`
(`0x1c`/task), timer wheel `0x1093bc` (`0x10`/slot), ECB table `0x100140` (`0xc`/event), trace port
`0x600100`.
