# The interactive-boot handoff — task-1 master sequencer, traced

Every thread of this project (SIM acceptance, MMI state, keypad input, the display
resource-enable, the idle repaint) ends at the same place: some post-startup transition
into the interactive/idle UI never fires, so the screen holds at "Insert SIM card". This
doc identifies that transition concretely and traces, on our full-SIM boot, exactly how
far the boot sequencer gets and where it stops.

Probed with the opt-in `NOKI3210_TRACE_HANDOFF` tap (three PC hooks: task-1 dispatcher
`0x270c8e`, mode-0 interactive-init burst `0x270d1c`, display-manager idle call site
`0x298000`). Oracle unchanged (`d8a9a7a58e587be8`); the tap is default-off.

## The master sequencer is task 1

Task 1 (`0x270170`) is the boot state machine. Its state struct is at **`0x1123ec`**; the
current **mode** is a signed halfword at **`0x1123f0`** (`[state+4]`). Dispatcher
`0x270c8e` reads the mode, bounds-checks `≤ 0xd`, and jumps through a **14-entry table
`0x270ca8`** (modes 0..13). Mode handlers self-advance by writing the next mode into
`[state+4]` in their epilogue (`movs r0,#N; strh r0,[r4,#4]`). Modes are *states*, not a
linear count — mode 1 is early init, mode `0xd` is the CONTACT-SERVICE/limp supervisor,
mode 0 is the interactive path (it holds the UI-subsystem init burst `0x270d1c`).

## What our boot actually does (traced, full 20 s)

```
mode 0001  chk[112399]=00 ccont[11ff6c]=00   t=0.25   early init
mode 000d  chk=00         ccont=06           t=0.33   enter CONTACT-SERVICE limp
mode 000d  chk=08 → 09 → 0b                  t=0.33–0.83   startup events arrive
mode 0004  chk=0f         ccont=03           t=0.91   ADVANCE out of the limp to mode 4
(stays mode 0004 for the remaining ~19 s)
```

Two corrections to earlier notes fall out of this:

1. **The mode-`0xd` limp is passed, not stuck.** The old "stalls in the 000d limp because
   CCONT battery-measurement events 0x15/0x16 are never delivered" was true of an earlier,
   less-complete boot. With the full model stack the checklist byte `[0x112399]` climbs
   `08→09→0b→0f` (all four startup events `0x14/0x17/0x16/0x15` → bits `0x1/0x8/0x2/0x4`),
   the CCONT-state nibble `[0x11ff6c]` is `6`, and at t=0.91 task 1 **advances out of mode
   `0xd`** into mode `0x4`. (A first pass with a 48-line trace cap stopped at t=0.6 and
   missed this — hence a momentary "parked at 0x0d" misread, corrected here.)
2. **The wall is mode `0x4`, one step further than any prior note placed it.**

### The mode-`0xd` exit gate (for reference)

Handler `0x270e22` accumulates the checklist: it dispatches on the message code and OR's a
bit into `[0x112399]` — `0x14→0x01, 0x16→0x02, 0x15→0x04, 0x17→0x08`. The exit test at
`0x270ec6` requires **all** of: `[0x11ff6c]&0xf == 6`, `[0x112399]&0xf == 0xf`,
`0x2a6942() != 0`, `0x2b084c() == 0`; else it loops in mode `0xd`. On our boot all four
hold by t=0.91, so the limp is cleared.

## The current wall: task 1 parks in mode `0x4`

Mode `0x4` handler `0x271254` is **not** a dead park — on entry it runs a real
subsystem-init burst (`0x25e4be`×3 with args 1/2/0, `0x25ff72`, `0x25f778`, `0x2601e8`,
`0x2794d2`, `0x2a6646`, `0x29797c`, `0x29af90`, `0x2a102c`), clears the startup latch
`[0x112398]=0`, sets sub-state `[0x11239a]=4`, posts a delayed event
`0x2697aa(6, 0x0303)`, then enters an **inner message loop** (`0x2712cc`, dispatching on
`[state+2]` message codes `0xd`, `6`, …) waiting for a trigger to advance. The mode value
stays `4` for ~19 s while it services that loop — so the boot is **blocked on whatever
message/event mode 4's inner loop is waiting for** to advance toward the interactive mode.

Downstream confirmation: the mode-0 interactive-init burst `0x270d1c` **never runs** (task
1 never enters mode 0), and `display_idle` (`0x298000`) **never fires** — consistent with
both being gated behind the mode-`0x4` → interactive transition.

## Why this unifies every other thread

The resource-enable (contact-service cmd `0x70` → bitmap; `resource_providers.md`), the
idle repaint (`[0x1116fd]=1` in the display manager; this doc's `0x298000` hook), the
input-to-MMI translation (`mmi_layer.md`), and the post-SIM screen advance
(`sim_emulator_scope.md`) are all **downstream of task 1 reaching the interactive mode**.
The sequencer is the single junction; mode `0x4`'s inner-loop wait is the precise, current
gate. This is the same *shape* as CONTACT SERVICE / `000d` / service-ready — a coherent
boot-state handoff waiting on a message our reconstructed boot doesn't deliver — now
localised one mode further along than before.

## Follow-up: the mode-`0x4` gate is a single trigger — message code `7`, never delivered

Traced mode `0x4` in detail (extended `TRACE_HANDOFF`: hooks at handler entry `0x271254`,
mode-write `0x270184`, and the task-1 mailbox posts `0x26a204`/`0x26a354`). The result
**re-frames the "6-message checklist" target**: that inner checklist (`0x112390..0x112395`,
loop `0x2712cc`) is *downstream and never reached*. The actual gate is one step earlier.

**Mode-`0x4` handler `0x271254` dispatches on the entry message code:**
`3 → 0x2711f6`, `0xe → 0x271230`, `7 → the init burst 0x271266`, **else → `0x2701b0`**,
which is a mode-set stub (`movs r0,#4; b 0x270184` = "stay in mode 4, yield"). So the init
burst (and thus the inner 6-message checklist behind it) runs **only on entry message code
`7`** (or 3/0xe).

**What task 1 actually receives in mode 4:** codes `d5, 75, 33, c3` (c3 = the periodic
tick) — **never `7`/`3`/`0xe`**. Every message hits the else-path → `0x2701b0` → re-arms
mode 4. Confirmed by the mode-transition trace:

```
0001 -> 000d  via lr=0x270e3c   (mode-0d handler)
000d -> 0004  via lr=0x271266   t=0.9127   (= return from bl 0x2701b0 at 0x271264)
0004 -> 0004  via lr=0x271266             (stable park; re-armed every message)
```

**Why code 7 never comes:** inventory of every post to task 1's mailbox
(`0x26a204`/`0x26a354`, `r0`=task, `r1`=code) over the whole boot yields exactly:
`0x14 0x15 0x16 0x17` (the mode-0d checklist events — all arrive, which is why the limp
clears) plus `0x32 0x33 0x37 0x75`. **Code `7` is never posted.** The event posters are
small stubs at `0x2af074+` (`movs r0,#1; movs r1,#<code>; bl 0x26a354`; e.g. `0x2af086`→
`0x17`, `0x2af09a`→`0x11`); no reachable stub posts `7` on our boot.

Correction to the section above: the mode-`0x0d` exit gate `0x270ec6`→advance `0x270ee6`
(and its *inline* copy of the burst) is **not** the path our boot takes — those hooks never
fire; the `0x0d→0x4` transition happens via `lr=0x271266` (the mode-4 handler's own
else-path), and `ccont[11ff6c]` moves 6→3 across it. So the gate documented earlier is one
of several mode-0d branches, not the one exercised.

**Net gate:** the interactive handoff is blocked because **message code `7` — the mode-`0x4`
init-burst trigger — is never delivered to task 1.** Everything downstream (the 6-message
subsystem-ready checklist, the mode-0 interactive-init burst `0x270d1c`, the idle repaint)
waits behind it.

## Next

Two open threads for the code-`7` trigger:
1. **Not yet traced:** the other post paths — delayed `0x2697aa`, immediate `0x2695f4`, ring
   `0x26aac0` — and the getter `0x26ff14`'s *translation* (raw `0xc0/0xc1/0xc4/0xc6` return
   RAM cells `[0x1123a0]/[0x112408]/[0x11245e]/[0x11239a]`, which could evaluate to 7).
   Confirm code 7 is absent by *every* delivery mechanism, or find the one that carries it.
2. **Who should emit it:** identify the subsystem whose "ready" report is code 7 (by analogy
   with the `0x2af074+` posters for `0x14–0x17`) and why its path isn't taken on our boot.

Knob: `NOKI3210_TRACE_HANDOFF` (opt-in; hooks `0x270c8e`, `0x270d1c`, `0x298000`,
`0x271254`, `0x270184`, `0x26a204`/`0x26a354`).

Symbols: `task1_sequencer 0x270170`, `task1_dispatch 0x270c8e`, `task1_mode_table 0x270ca8`,
`task1_mode0d_limp 0x270e22`, `task1_mode0d_exit_gate 0x270ec6`, `task1_mode04 0x271254`,
`task1_mode0_interactive_init 0x270d1c`, `display_mgr_idle_call 0x298000`. State:
mode `0x1123f0`, checklist `0x112399`, CCONT-state `0x11ff6c`.
