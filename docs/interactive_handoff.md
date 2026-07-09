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

## Next

Identify the message/event mode `0x4`'s inner loop (`0x2712cc`) is waiting for to advance
task 1 to the next mode (toward mode 0 / interactive), and what subsystem should emit it.
That is the immediate, well-scoped next gate. Knob: `NOKI3210_TRACE_HANDOFF` (opt-in;
hooks `0x270c8e` / `0x270d1c` / `0x298000`).

Symbols: `task1_sequencer 0x270170`, `task1_dispatch 0x270c8e`, `task1_mode_table 0x270ca8`,
`task1_mode0d_limp 0x270e22`, `task1_mode0d_exit_gate 0x270ec6`, `task1_mode04 0x271254`,
`task1_mode0_interactive_init 0x270d1c`, `display_mgr_idle_call 0x298000`. State:
mode `0x1123f0`, checklist `0x112399`, CCONT-state `0x11ff6c`.
