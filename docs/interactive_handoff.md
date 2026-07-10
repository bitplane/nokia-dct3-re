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

## Follow-up (#28): the code-`7` trigger is armed by an advance gated on a SIM byte

Traced *why* code 7 is never delivered. It is **not** an external subsystem post — it is
**self-armed** by the mode-`0x0d` *proper* advance path, which our boot never takes.

**The two mode-0d exits.** When the mode-0d checklist completes (`chk[112399]&f==f` and
`ccont[11ff6c]&f==6`, both true at t=0.85), the handler reaches the gate `0x270ec6`, which
then calls **`0x2a6942`**:
- `0x2a6942 != 0` → **proper advance `0x270eee`** → runs the burst inline and, at `0x270f56`,
  sets mode to **7** with code 7 armed;
- `0x2a6942 == 0` → **fallback `0x270fa4`** → sets `ccont` state to 3 (`0x2af058(3)`, explains
  the 6→3 move) and lands in the **dead-park mode 4** that waits forever for a code 7 nobody
  sends.

**The gate is a single SIM byte.** `0x2a6942` reads `0x27d654`, which returns byte
**`[0x110436]`** (= field `+2` of the SIM struct at `0x110434`). It returns 0 (block) while
`[0x110436] ∈ {1,2}`. Trace `simgate`: `[0x110436] == 1` for the entire boot, never changes.
So the proper advance is blocked, mode falls to the dead park, and code 7 is never armed.
This gate is checked at *two* task-1 transitions (`0x270edc` in mode-0d, `0x27139a` in
mode-04) — it is the general "interactive-ready" predicate.

**Confirmed by experiment.** `EXPERIMENT_SIMGATE_PASS` overrides `0x2a6942`'s read of
`[0x110436]` from 1→0 at `0x2a6948`. Result: the advance fires and **mode goes `000d → 0007`
via `lr=0x270f56`** (the code-7-armed path) instead of `→ 0004`. So `[0x110436]` is decisively
*the* first blocker of the interactive handoff. (Mode 7 then parks — a further gate — so the
chain is 0d→7→…, but the first, causal gate is this SIM byte.)

**What `[0x110436]` is / the faithful fix.** It is a SIM/interactive-readiness enum (1 = "not
ready"); the gate wants 0 or ≥3. Our synthetic SIM does ATR + a couple of EF reads only, so
the firmware's SIM-init never advances this byte past 1. The faithful path is to complete more
of the SIM initialisation conversation (the files/status the firmware reads to declare the SIM
ready) so `[0x110436]` reaches its ready value naturally — the same shape as every prior gate.
Knobs: `EXPERIMENT_SIMGATE_PASS` (diagnostic force), `TRACE_HANDOFF` (`simgate` tap on
`0x27d654`).

## Follow-up (#30): CORRECTION — the gate is a BATTERY/VBAT byte, not SIM

Investigating how to make `[0x110436]` reach "ready" overturned its labelling. **It is not a
SIM state.** The subsystem that owns struct `0x110434` and drives `[0x110436]` is the
**battery / VBAT voltage monitor** at `0x21exxx` — its debug strings are "BATTERY VOLTAGE
CHECK", "REGULAR COLD CHARGE CHECKS", "Initialise VBAT filter", "Start limited fast VBAT
reads", "Check voltage level for shutdown". So the mode-0d→interactive advance is gated on a
**battery-voltage confirmation**: the firmware won't proceed to full operational/interactive
mode until the VBAT classification is out of the "checking/marginal" band {1,2}. (This
corrects the `#28` section above and the earlier `EXPERIMENT_SIMGATE_PASS` name — both said
"SIM"; it is VBAT. The knob is now `EXPERIMENT_VBAT_GATE_PASS`.)

**How `[0x110436]` is computed.** Writer `0x27dcfa` stores `[0x110436] = 0x27cbec()`. The
classifier `0x27cbec` compares a filtered voltage (`r1 = [0x110486] = 0x0995`) and a filter
output `ip` (from `0x27d500`, a **15-tap moving-average**: ring index `[0x110434+0xb] mod 15`,
accumulator `[+0x44]`, sample buffer `0x1104b4`) against **fixed thresholds**
`[0x110494]=0x076c`, `[0x110496]=0x0910`, `[0x110498]=0x08b6`. On our boot `ip ≈ 0x09ac ≥
0x0910`, so it structurally returns **1** and never changes over 20 s. Values 1/2 block the
gate; 0 and 3 pass.

**Not the battery ADC.** Sweeping the battery-voltage ADC (`NOKI3210_ADC2`) across its full
range `0x120…0x3ff` leaves `[0x110436]=1` and `r1=0x0995` unchanged — the classifier's inputs
are internal/calibration values, not the live ADC channel-2 reading. So **extending the SIM
init (the original #30 framing) cannot affect it, and neither can a battery-voltage tweak.**

**Verdict.** The faithful unblock of `[0x110436]` is a **battery/VBAT-subsystem** matter (the
filter/classification reaching "confirmed"), not a SIM-model extension. It was partially
explored before (`SKIP_SERVICE_E2_REARM`, the VBAT-pipeline probe) and is not resolved by any
single value. The digital lever to explore *past* this gate remains the forced pass
(`EXPERIMENT_VBAT_GATE_PASS`), which advances mode `0d→7` (confirmed) for use by `#31`.

## Follow-up (#31): the mode-7+ chain — enumerated, and it does not reach pixels

Used the two diagnostic force levers (`EXPERIMENT_VBAT_GATE_PASS` + `EXPERIMENT_FORCE_CODE7`,
the latter forcing the advance's getmsg return `r0:=7` at the reliably-hooked post-`bl`
point `0x270f4a`) to push the sequencer as far as it will go and map the gate chain:

| # | gate | our boot | lever |
|---|---|---|---|
| 1 | mode-0d startup checklist (`[0x112399]` ← events `0x14/15/16/17`) | **satisfied** (limp clears) | — |
| 2 | **VBAT voltage-confirmation** `[0x110436]` (via `0x2a6942`) | blocks (structurally 1) | `VBAT_GATE_PASS` |
| 3 | **code-7 trigger** (msg code 7 to task 1 at the advance getmsg) | never posted (unrun subsystem) | `FORCE_CODE7` |
| 4 | **mode-4 6-message ready checklist** `[0x112390..95]` (codes `9/a/b/c/d/1c`) | never all posted | — |
| 5+ | mode-0 interactive-init `0x270d1c`, idle repaint `[0x1116fd]`, resource-provider graph | never reached | — |

**Result of forcing gates 2 + 3:** the mode-0d advance and its init burst run, and the
sequencer reaches **mode 4** (confirmed: the VBAT gate is consulted a second time from the
mode-4 caller `0x27139a` at t=0.88). But it then parks on gate 4 (the 6-message checklist),
and — decisively — **the display never changes: the frame stays `d8a9a7` ("Insert SIM
card")**, with no interactive-init and no idle repaint.

**Take-stock conclusion.** The visibly-interactive screen is not one or two gates away — it
sits behind a **chain of subsystem-readiness gates**, each a signal our reconstructed boot
does not produce (battery-voltage confirmation; the code-7 trigger from an unrun subsystem;
six more subsystem-ready posts; then the resource-provider graph). Forcing the leading gates
does **not** cascade to pixels, because each unblocked gate merely exposes the next, and the
downstream subsystems (the ready-message posters, the resource providers) still are not
running. This is the coherent-boot wall, now *enumerated* as a concrete gate sequence rather
than a vague region: reaching the interactive screen requires coherent bring-up of that whole
graph, not a bounded set of pokes. Levers (opt-in, diagnostic): `EXPERIMENT_VBAT_GATE_PASS`,
`EXPERIMENT_FORCE_CODE7`.

## Follow-up (#29, #32): the idle flag is downstream, and the VBAT gate is a fork not a wall

**#29 — idle-repaint flag `[0x1116fd]`.** A ram-write watch shows it is written **only to 0**
(pc `0x2982e8`, the display manager's "clear idle request") and to 2 (after a paint); **never
to 1**. The display-manager loop's own idle-request accumulator (`[sp+4]`) stays 0, and no
reachable code writes `[0x1116f8+5]=1`. So the idle repaint is never *requested*. That request
is issued when the phone transitions into the idle state — which is behind the whole gate chain
above. **#29 is the display-side manifestation of the same coherent-boot wall, not an
independent seam** (confirmed: the forced #31 run, which reaches mode 4, still leaves the flag
0 and the frame unchanged).

**#32 — can the VBAT gate be unblocked faithfully?** Traced the classifier end-to-end:
sample generator `0x27cc74` → raw reader **`0x2b1bb2(7)`** (a *different* source than CCONT ADC
ch2 — which is why sweeping `NOKI3210_ADC2` did nothing; it returns a **ROM default** via
`[0x2e742d+7]` when calibration flag `[0x1124d0]==0`, scaled `×1500/313` with cal words from
`[0x11fde0]`) → accumulated into `[0x110454]`/`[0x11045c]` → ÷10 moving-average filter
(`0x27d500`) → classified (`0x27cbec`) against **ROM-constant thresholds** (memcpy'd from ROM
`0x2e1ff4`, *not* EEPROM) → **state 1**.

Two conclusions:
1. **It's not calibration/EEPROM data we can supply** — thresholds are ROM constants and the
   reading is a ROM default; the classifier structurally returns 1.
2. **More importantly, the VBAT gate is a *fork*, not the interactive blocker.** state 1/2 →
   fallback → **mode 4**; state 0/3 → advance → **mode 7**. And *both* mode 4 and mode 7 then
   wait on **message code 7** (mode-4 entry `0x27125e`, mode-7 handler `0x270f4c`). So our
   unforced boot already takes a valid branch (mode 4); making VBAT "pass" only switches the
   branch to mode 7, which hits the *same* code-7 wall. **`EXPERIMENT_VBAT_GATE_PASS` was
   therefore a detour** — the real shared blocker on both branches is the code-7 trigger.

**Verdict:** don't model VBAT (it is a fork, and mode 4 — our current unforced path — is a
faithful branch); the single remaining shared blocker to advance the sequencer is **message
code 7 arriving at task 1**, whose source is an unrun subsystem (per #27/#28). That is the
correct next target, on the *unforced* boot.

## Follow-up (#33): code 7 (and the 6-message checklist) are subsystem-ready reports — none fire

Found the emitter of code 7: **`0x2af190`** (`post code 7 to task-1 mailbox`), one of a cluster
of tiny **subsystem-ready reporter stubs at `0x2af01c…0x2af27a`**, each `report-state (0x2b5ae4)`
+ `post <code> to task 1`. Crucially this one cluster emits **every** code in the chain:
- the mode-0d startup events `0x14/15/16/17` (reporters `0x2af1da/216/03e/094`) — which **do**
  fire (that's why the limp clears),
- **code 7** (`0x2af190`),
- and the **mode-4 6-message checklist** codes `9/0xa/0xb/0xc/0xd/0x1c` (reporters
  `0x2af052/202/02a/1c6/1b2/1ee`),
- plus 3 (`0x2af252`) and 0xe (`0x2af27a`).

So the code-7 trigger and the 6-message checklist are the **same mechanism**: a subsystem calls
its reporter stub when it reaches a state. Code 7's reporter `0x2af190` has **4 callers** —
`0x21e40c` (the battery/charger state machine's "Check voltage level for shutdown" branch,
gated on `0x27cd82()` seeing VBAT sample ≤ `[0x110494]+0xe9` = 2133), `0x21f8de`, `0x255c3c`,
and `0x27b3b6` (a SIM-region code dispatcher). **Trace `code7path`: none of these is reached on
our boot — code 7 is never posted by any emitter.** Each sits behind a subsystem state machine
(battery/charger, SIM-region, …) that never reaches the posting state; the battery one is a
deep charger state machine gated on charger/voltage events, and the VBAT reading (raw `0x200` →
sample ~2453) is above its threshold anyway.

**Assessment for faithful emulation.** The mode-0d events fire because CCONT/startup complete;
code 7 and the six checklist messages do **not**, because their subsystems never reach the
reporting state. Emulating them faithfully is therefore **coherent bring-up of those subsystem
state machines** (battery/charger, display, SIM-region dispatchers) — each a deep RE + model
effort gated on hardware/charger state, not a bounded injection. This is the coherent-boot wall,
now mapped to its finest grain: the `0x2af0xx` reporter cluster and the specific subsystem state
machines that drive it. The mechanism is fully understood; the remaining work is per-subsystem
bring-up, which is open-ended.

## EMULATION (MODEL_STARTUP_REPORTS): the chain advances mode 4 → mode 0xc, faithfully

Built a faithful emulation of the subsystem-ready reports (not a gate-force): **`MODEL_STARTUP_REPORTS`**
injects code 7 + the six checklist codes `9/a/b/c/d/1c` + the post-checklist `0x74` into task-1's
mailbox via the firmware's **own** post `0x26a354(mailbox=1, code)`, chained through a sentinel
trampoline (the `MODEL_SVC_RESPONDER` / `MODEL_RES_ENABLE` pattern), triggered once at task-1's
getter `0x26ff14` after mode-0d completes.

**Result (traced):** the sequencer advances

```
mode 000d → 0004 (mode-0d limp cleared)   [pre-existing]
reports injected (t≈0.95)
mode 0004 → 000c (t≈1.06)   ← code 7 consumed, init burst ran, entered the checklist mode
6-message checklist [0x112390..95] all set   (chk_w trace)
mode-0xc completion reached, waited for + consumed 0x74   (mode0c_wait74 trace)
```

So three of the four gates are now **emulated faithfully** (driven through the firmware's own
mechanisms, not forced): the **code-7 trigger** and the **mode-4 6-message ready checklist** carry
the boot from the "Insert SIM card" park (mode 4) into mode `0xc` and through its `0x74` report.

**Where it now stops — the VBAT confirmation gate, again.** Mode `0xc`'s exit (`0x27139a`) needs
either `0x2a6942()==3` (VBAT classification **state 3**) or a keypad condition
(`0x2b2f90()==0x80 && [0x11239d]==1`); otherwise it falls to `0x2714a4` → an idle getmsg drain.
So the **VBAT voltage-confirmation** gate (goal item 1) is genuinely required *here* to reach
mode 3 → mode-0 interactive-init. The classifier `0x27cbec` writes `[0x110436]`, and it does not
reach state 3 from a simple `NOKI3210_VBAT_RAW` reading override (the classification is a
multi-comparison over the moving-average vs ROM thresholds; a faithful state-3 needs the reading
to converge into a specific band, still open). Knobs: `MODEL_STARTUP_REPORTS`,
`STARTUP_REPORTS_MS`, `VBAT_RAW`.

**Reactive feed (final form).** The one-shot mailbox injection hit an ordering problem (the
`0x74`-wait and later discard-loops eat an all-at-once injection). The model was reworked to feed
**reactively**: at task-1's getter `0x26ff14` (after mode-0d), it returns the next report code
directly (`bx lr`), one per getmsg call, in the order the successive sub-phases wait for them.
This carries the boot cleanly through the full message-gated startup handoff:

```
mode 4  (Insert-SIM park)
 feed 7          → code-7 trigger → init burst → mode 0xc
 feed 9,a,b,c,d,1c → 6-message ready checklist passes
 feed 0x74       → post-checklist report consumed
 [VBAT-confirm gate 0x27139a: model emulates 0x2a6942()==3, the confirmed value]
 → 0x2713b6 sub-phase → bl 0x2a12a0  ← HANGS
```

**The definitive terminus.** Past the VBAT-confirm gate, mode `0xc` calls the **display/UI
subsystem init `0x2a12a0`** (r0=1 → `0x2a1300`), which reports a resource (`0x2b13d4`), posts
three messages to **task 3**, and calls **`0x2355b6`** — and this long multi-step init **does not
complete** (traced: reaches `0x2713b6`/`0x2713bc`, never `0x2713c2`; inside `0x2a1300`, reaches
`0x2a1306`, never `0x2a130c`, even over a 60 s run).

**Correction (task 3 IS alive).** An earlier note here said task 3 (the display/UI server) is
"not functional". That is wrong — a liveness trace shows **task 3 runs and recv-loops throughout
the boot**, processing many codes (`0x1740, 0x2b14, 0x2c48, …`), including during the mode-`0xc`
display init. So the terminus is *not* a dead subsystem. `0x2355b6(2)` cooperatively **yields via
the RTOS scheduler `0x269d00`** (the context-switch/dispatch, not an event-wait) and task 1
progresses only slowly through the display init (a ~5 s gap between successive checkpoints),
threading many scheduler yields and subsystem calls (`0x2795e6`, `0x2b28e0`, `0x2904c0`, the
task-3 posts, resource acquisition). Over **60 s and 200 s** runs it still does not return to `0x2a130c`, mode-0 interactive-init
(`0x270d1c`) never runs, and the frame never changes. So the ceiling is the **display-subsystem
init being a deeply interconnected multi-step process that does not converge** on our reconstructed
boot — the coherent-boot wall, reached *from above*. (Skipping the yield is not an option:
`0x269d00` is core scheduler used across the whole system; forcing it past breaks the boot at
mode-0d.)

**What the init actually does (the root of the non-convergence).** `0x2355b6`, after the yield,
calls `0x2795e6`, which **resumes/starts the application-task layer** — tasks **0xa..0x11 (10–17)**
via `0x269bf4` (each `0x1093bc + id*0x10` TCB). So the mode-`0xc` display init is the "bring up the
application/UI tasks" step. Those tasks then run their own inits (needing resources, the display
render path, DSP/RF state). On our reconstructed boot that app-layer bring-up **does not converge**
— which is exactly the resource-provider / coherent-boot wall, now seen to be *the app-task layer
init*, not a single missing message. Emulating mode-0 interactive-init therefore requires driving
~8 more application tasks to convergence (each with its own report/resource dependencies, and
behind them the resource-content pipeline that blanks/crashes when forced) — an open-ended
subsystem bring-up, the documented digital ceiling.

**Status vs the goal.** VBAT confirmation — mapped + emulated at its gate (`0x2a6942()==3`);
code-7 trigger — **emulated** (fed via `0x26ff14`); mode-4 6-message checklist — **emulated**
(fed, checklist passes); mode-0 interactive-init — **not reached**: it is behind the display/UI
subsystem init `0x2a12a0` that hangs (needs the display/resource subsystem + task 3 functional,
i.e. the coherent-boot wall). Three of the four gates are driven; the fourth (mode-0) is blocked
not by another message but by a non-functional subsystem — the genuine, long-documented ceiling.
Knob: `MODEL_STARTUP_REPORTS` (`STARTUP_REPORTS_MS`).

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

## App-task forcing sweep (2026-07): the layer is ALIVE, not hung

Ran a task-liveness + message-flow sweep (`NOKI3210_TRACE_TASKS`) with
`MODEL_STARTUP_REPORTS` driving the boot. This overturns the "non-converging /
hung" framing:

- **All 22 RTOS tasks reach their recv loop.** The application tasks **10–17**
  (resumed on the mode-0xc path) come alive at t≈0.84; every task 1–22 is scheduled
  and running its message loop. Entry points (task-table `0x2d7090`, stride 0xc):
  t10 `0x21bf60`, t11 `0x2159c4`, t12 `0x273ea0`, t13 `0x23ebd0`, t14 `0x248318`,
  t15 `0x20a8a8`, t16 `0x24f5a0`, t17 `0x22391c`. Known hubs: t5 `0x2af630` (MMI VM),
  t6 `0x297fc4` (display manager), t3 `0x2b18a0` (display/resource server),
  t21 `0x27eae0` (SIM).
- **They run a busy, continuous inter-task protocol.** Communication graph (edges,
  deduped): `t5` is the central hub (receives from t0/t1/t17/t20/t21/t2/t6); `t13↔t16`
  exchange bidirectionally; `t15→t16/t17`, `t16→t10/t11/t13`, `t17→t5/t15/t20/t3`,
  `t20→t21/t5/t17`, `t21→t17/t5`, `t5→t6` (MMI→display). Message codes are per-message
  **sequence ids** (`0x17xx→0x1axx`, monotonically increasing), i.e. genuine traffic.
- **It reaches a STEADY STATE, it does not stall.** Post rate holds ~2000/s out to
  t=16s+ (still active), but **no new communication edges appear after t≈6.4** — the
  same task-pairs loop. A new phase engages at t≈6.38 (t6 display-manager → t5, t2→t5),
  then it settles.

**Reframe.** The interactive handoff is **not** blocked by a dead subsystem or a
non-converging init. The whole task graph is alive and continuously messaging; it
settles into an **intermediate steady state that still shows "Insert SIM card."**
Reaching the interactive/idle screen therefore needs a **trigger that advances the MMI
(t5) out of this steady state** — the same *kind* of missing-signal problem the reports
solved for the earlier gates, not a hardware/convergence wall.

**Next dig.** The MMI VM (t5) state machine: what state is it parked in, and what
message/event moves it from "Insert SIM card" to the idle/menu screen. Likely a
SIM-ready → MMI-idle transition (ties back to the SIM thread). Sweep knob:
`NOKI3210_TRACE_TASKS` (task liveness + `msgedge`/`msgrate`).

## MMI-VM (task 5) dig (2026-07): the idle transition IS reached — the wall is the idle render

Traced the MMI VM's own event stream with `TRACE_MMIVM` (hook `0x2af582`, the post-recv
point in the event fetch `0x2af57c`: `r0`=msg ptr, `[r0]`=raw 16-bit code → event =
`code & 0x1fff`, params = `code>>14`). This resolves the "steady-state trigger" reframe
above into a precise, and much more encouraging, statement.

- **Task 5 is a low-rate event processor, not a busy loop.** It dequeues **< 200 events
  in 25 s** (the ~2000 posts/s "steady state" is the *other* tasks; t5 sees only a
  fraction). From t≈3.5 it runs a periodic UI cycle — events `138e / 1390 / 13b6 / 13b5`
  every ~0.6 s (a refresh/poll animation while it waits).
- **The boot REACHES the idle transition.** At **t≈6.38** `display_idle 0x2a255c` fires
  **exactly once**, and task 5 dequeues render event **`0x0547`** exactly once (the
  message `display_idle` render-posts via `0x2af6ea`). Immediately after, the periodic
  `138e/1390/13b6/13b5` cycle **stops** — task 5 quiesces into "idle painted, waiting."
  So the phone is further along than "up but not alive": it actively runs its idle-screen
  *entry*. **This overturns the older claim that `display_idle` never fires / the idle flag
  is never set** — that was a less-complete-config artifact; under the full SIM stack the
  idle entry runs.
- **The render fails at one gate: the idle window can't be acquired.** Hooking the
  post-`bl` point `0x2a2566` shows **`resource-get(0x4c22) → r0 = 0`** (null handle).
  `resource-get 0x2b257e` bails at its very first instruction: `bl 0x2b12b4` (availability)
  returns 0 → `b 0x2b2690` (fail). Availability = `[0x11fee4]!=0 AND bitmap-bit(class)`;
  on our boot `[0x11fee4]` (master resource-enable) is **0** because the faithful
  contact-service cmd-`0x70` was never issued. `display_idle` **does not check the return**
  — it render-posts `0x0547` regardless — so task 5 processes the render with a null idle
  window and composes nothing → the frame stays "Insert SIM card."
- **`MODEL_RES_ENABLE` doesn't rescue the *natural* idle draw.** Re-run with the faithful
  cmd-`0x70` model: `resource-get(0x4c22)` **still returns null** because the default blob
  enables class `0x22`, not class **`0x4c`** (the idle window). Enabling the 5 ROM-backed
  classes (`0x4c/0x4f/0x50/0x52/0x56`) renders the "Insert SIM card" chrome; enabling all
  ~18 idle-content classes blanks/crashes (the unbacked-provider dead-end already proven in
  `docs/resource_providers.md`).

**Convergence.** The "interactive handoff" thread and the "resource-provider" thread are
now proven to be the **same wall**, meeting at one runtime instant:

```
t≈6.38  idle transition reached
     → display_idle 0x2a255c
       → resource-get(0x4c22)  [0x2b257e]
         → availability 0x2b12b4  →  0   (because [0x11fee4]==0, cmd 0x70 never issued)
       → null idle window
     → render-post 0x0547 → task 5 composes nothing
   ⇒ "Insert SIM card" persists
```

The last sweep's "missing trigger to advance the MMI" is resolved: the trigger
(`display_idle → 0x0547`) **is** emitted and consumed. The single remaining blocker to a
richer screen is the idle-window/content **resource acquisition** — the known
`[0x11fee4]` / cmd-`0x70` + class-backing wall — now pinned to the real idle moment rather
than a forced experiment. Diagnostic knob (curated, opt-in, log-only): `NOKI3210_TRACE_MMIVM`
(`display_idle` entry, `resource-get(0x4c22)` result, event dequeue histogram / late-event
cadence).

## Resource-enable dig (2026-07): a swap16 mask-table bug — and the natural idle draw now proven to blank on content

Followed the `resource-get(0x4c22)` failure into the resource-enable machinery. Findings,
in order:

- **The cmd-`0x70` enable is the *only* way to set the availability bitmap; there is no
  self-issue init path.** The registrar `0x2b140a` has 4 callers; the two outside the
  channel-map handler (`0x236e6c`, `0x236f10`) are both *disable* calls (all-zero args →
  `config_ptr=0`). Enable comes only from a received `0x70` message dispatched through the
  contact-service loop (`0x237bc6` recv → `0x237400` → `0x23670c`). On our boot only cmd
  `0x64` is injected (`MODEL_SVC_RESPONDER`); a real session also carries `0x70` + its
  0x40-byte blob. `MODEL_RES_ENABLE` synthesises the `0x70` the same faithful way.
- **With `MODEL_RES_ENABLE`, `[0x11fee4]=1` and the class bitmap is written — yet
  `resource-get(0x4c22)` still returned null.** Instrumenting inside `resource-get 0x2b257e`
  showed `available(0x4c22)=0` even though enable and the bitmap byte `[0x11ff11]` were both
  set. The formula is `enable!=0 AND (masktable[class&7] & bitmap[0x11ff08 + class>>3])`.
- **Root cause: the mask table `0x2e2f5c` is a swap16 trap.** The swap16-image bytes read
  `{40,80,10,20,04,08,01,02}` (recorded as "permuted" in older notes), but `ldrb` reads the
  *real* rom byte `image[addr^1]`, so the firmware sees `{0x80,0x40,0x20,0x10,0x08,0x04,
  0x02,0x01} = 0x80>>(class&7)`. Class `0x4c` (`class&7=4`) → real mask `0x08`, but the old
  `MODEL_RES_ENABLE` sparse blob (built from the permuted table) set bit `0x04` → the
  availability test `0x08 & 0x06 = 0` failed. The old blob had actually been enabling the
  *wrong* classes `{0x4d,0x4e,0x51,0x53,0x57}`, so **`0x4c22` was never available in any
  prior "sparse-5" run** — the display stayed on "Insert SIM card" precisely because the
  idle draw failed at *window acquire* and never reached content.
- **Fixed the blob to the real masks (byte9=`0x09`, byteA=`0xa2`, enabling
  `{0x4c,0x4f,0x50,0x52,0x56}`).** Now — for the first time — the *natural* t≈6.43
  `display_idle` acquires the idle window: `available(0x4c22)=1`, `resource-get(0x4c22)→5`
  (non-null). The idle draw then proceeds into content composition and the **display blanks**
  (uniform frame `94a2dc…`, all `o000`), because the ~13 idle-content classes
  (fonts/icons/layout sub-windows) have no ROM backing.

**Net.** This corrects a real bug (the swap16 mask-table trap, which had silently
invalidated both the documented mask table *and* the sparse blob) and, more importantly,
proves the content-backing wall **from the natural idle path** rather than a forced draw:
with the idle window genuinely available, the real `display_idle → 0x0547` render advances
one layer further and dies on the unbacked content classes. The two boot outcomes are now
cleanly separated: **`0x4c22` unavailable → "Insert SIM card" persists (pre-idle screen);
`0x4c22` available → idle draw proceeds → blank (content wall).** Neither reaches a real
idle screen; the terminal blocker remains the unbacked ~13 content resource classes
(`docs/resource_providers.md`), needing the display/font/window subsystems' coherent
bring-up (no bitmap shortcut). `MODEL_RES_ENABLE`'s default blob is now the corrected one.

## Content-backing wall dig (2026-07): the idle window is an empty *container*, not a failed content fetch

Went at the content wall empirically (skeptical after the mask-table swap trap that the
"~13 unbacked content classes queried by the render" framing might also be imprecise). It
is imprecise. Findings:

- **The idle render issues NO content resource-gets.** With the corrected blob so `0x4c22`
  actually acquires, `TRACE_MMIVM` shows the *only* resource-get in the whole idle sequence
  is `0x4c22` itself (the window). After task 5 dequeues the render event `0x0547`, it does
  **no** `resource-get`/availability calls for fonts/icons/layout and then goes quiet. So
  the earlier "the `0x0547` handler composes ~18 content classes via resource-get" model is
  **wrong** — content is *not* resource-acquired inside the render.
- **The idle window is a container that opens empty.** Over a 30 s run with `0x4c22`
  available, the LCD does ~9 full refreshes and **every frame is blank** (`o000`; content
  byte count 0) — the firmware composes an empty framebuffer and blits it, forever. No
  content ever appears. The window's child content (clock / operator name / signal bars /
  indicators) is drawn by **separate child render events that subsystems post** to task 5
  after the window opens; on our boot none arrive (task 5 quiesces after `0x0547`), because
  those producers are gated on live subsystem state (network/operator = RF; clock = RTC).
- **"Insert SIM card" is the no-idle-window fallback.** Baseline (no `RES_ENABLE`) is
  deterministic (45 MMI events/boot) and *does* reach the idle transition at t≈6.38, but
  `resource-get(0x4c22)` fails (`[0x11fee4]==0`) → the pre-resource "Insert SIM card" screen
  (which uses no resources) persists. Making `0x4c22` available flips the boot to draw the
  empty idle container **instead of** "Insert SIM card". So "Insert SIM card" is precisely
  what shows when the idle window can't open — the correct terminal for a no-network phone.

**Refined verdict.** The content-backing wall is *not* "~13 resource classes with no ROM
backing that the render tries to acquire" — the render acquires none of them. It is: **the
idle window opens as an empty container, and its content is populated by child render
events that live subsystems (network/operator/clock) post — which never fire without the
SIM/network/RF bring-up.** Same terminal wall, more precisely mechanized: forcing the
window open just yields an empty frame; the missing piece is the *content producers*, not a
resource bitmap.

*Harness note (important for reproducing).* `MODEL_RES_ENABLE` and `MODEL_SVC_RESPONDER`
have a deliberate ordering interlock: SVC_RESPONDER's cmd-`0x64` completion waits for
`m_resen_state==3` (so `0x70` lands before `0x64`). resen can only be injected in the early
~0.45 s contact-service window (the trigger point `0x237bc6` is not re-entered later), so
`RES_ENABLE_MS` must stay ≈440. A wrong (late) `RES_ENABLE_MS` → resen never delivers →
SVC_RESPONDER stalls → the boot never reaches the idle transition at all (≤5 MMI events),
while still showing "Insert SIM card" from the early render. The MS=440 run used for the
findings above is clean (resen delivers → 0x64 fires → boot proceeds normally).
