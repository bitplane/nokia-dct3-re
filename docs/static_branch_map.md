# Static Branch Map: Boot Service Gates

Generated from Thumb disassembly of the swap16 firmware image. The purpose is to keep the next two boot branches explicit so runtime experiments can target known pass/fail conditions instead of rediscovering the same path.

> **⚠️ Read `docs/service_bootstrap.md` first (its Executive summary).** This file frames the D9
> watchdog around the **ack `0x11fedb`**. The ack is real *mechanism* (the watchdog counts up while
> `ack==0`) but a **red herring as a fix target** — the firmware never writes the ack, and the
> watchdog is gated **earlier by service-present bit 6** of `0x11fed0`. Keeping bit 6 set (via
> `MODEL_DSP_SERVICE` + `MODEL_CCONT_PRESENT` + the EEPROM `selftest` checksums) stops the watchdog
> arming at all, so the ack never matters. Trust the bit-6 chain over any "ack is the gate" wording here.

---

## 2026-06-26: Runtime-confirmed CONTACT SERVICE commit chain (forcing-free boot)

This supersedes the MBUS-centric reading further down. With all firmware forcing
removed, the boot reaches CONTACT SERVICE and **the commit was traced empirically**
(probe knob `NOKI3210_TRACE_CONTACT_COMMIT=1`, which logs the terminal handler,
the D9 timeout builder, the contact-service state block, and MBUS register access).

**Where it dies — a 140 ms internal watchdog, not a self-test failure:**

```
t=0.339s  contact-service block freshly initialized (memset 0x2b65e4, init 0x2346b2);
          ack byte 0x11fedb cleared to 0.
t=0.407s  watchdog counter 0x11fed6 starts ticking 1,2,3… one step every ~9.4 ms,
          stored at 0x237b50 (D9 handler), dispatched by the scheduler timer.
t=0.534s  counter reaches 0x0f (15) → timeout_signal(reason 2) @0x236dc4
          → fatal halt @0x2b4dda (infinite spin at 0x2b4e14).
```

The counter increments because the poll at `0x237b42` reads `ack` (`0x11fedb`) == 0
(`if ack != 0 → counter = 0, else counter++`). **`0x11fedb` is never written in the
whole 8 s run** — the completion path is never entered, not merely incomplete.

**The causal chain (each step runtime- or disasm-confirmed):**

```
no 0x70 channel-map frame ever delivered to the contact-service task
  └─ 0x5f00 service channel never enabled   (0x11fee4 only ever written =0 @0x2b13c4;
     │                                        0x11ff13 never written)            [traced]
  └─ D9 watchdog's 0x5f00 request is dropped, not sent
     │   (dispatch 0x237994 calls service_channel_request_if_enabled(2,0x5f00,sp)
     │    @0x2379a4 → lookup 0x2b12b4 fails on disabled channel → silent drop)    [disasm]
  └─ no response → ack 0x11fedb never written  (it is the D9 queue record's target
     │   pointer; the scheduler only writes it when the 0x5f00 request completes)
  └─ watchdog free-runs to 15 → CONTACT SERVICE                                   [traced]
```

**Two findings that redirect the strategy:**

1. **It is entirely internal — the MBUS serial bus is not involved.** Zero MBUS
   register activity (MAD2 0x18/0x19/0x1a) during the watchdog window 0.34–0.54 s;
   all 20 MBUS writes happen earlier (0.238–0.271 s, the D0 startup frame). So
   injecting `MBUS_RX_BYTES` cannot ack this watchdog — the handshake never reads
   that bus. The MBUS-centric analysis below is therefore **not** the path to ack D9.

2. **The contact-service task is a normal boot task, not fault-spawned.** Its entry
   `0x237bb5` is a standard 12-byte descriptor in the boot task table at `0x2d70a8`
   (siblings: tasks `0x270171`, `0x2b18a1`, `0x2b3fb9`). The phone is running its
   normal startup; we are simply never completing the handshake that dismisses the
   watchdog. The CONTACT SERVICE text is drawn by this task's own poll (the
   `0x25edf2` text calls gated on flags bit 7), not by the terminal halt.

**0x70 producer hunt — negative result.** The only path that enables the channel is
`channel_map_apply_2366c8` (requires an inbound `0x70` frame: `frame[5] > 0x42`, a
checksum-validated 0x40-byte map at `frame+9`) → `service_channel_config_2b140a`. The
other three `service_channel_config` callers (`0x23672c`, `0x236e6c`, `0x236f10`) pass
all-zero args = teardown only. **No code in the contact-service module `0x234000–
0x23c000` ever constructs a `0x70` frame** (no `movs #0x70` into a frame command
byte). So the firmware only *consumes* an inbound `0x70`; it never originates one.
On a standalone phone (no service box, no MBUS), the `0x5f00` channel staying disabled
is most likely normal — so the ack/disarm must come from a different path.

**Event `0x19` traced — it is the watchdog's own self-rearm, not a server signal.**
`0x2697aa(id=0x19, arg=0x202)` writes a delayed entry into the RAM event table at
`0x100140` (slot `0x10026c`); when it fires it re-posts the `0xd9` command. So tracing
it loops back to the watchdog. Dead end as a lead.

**Complete input set of the contact-service task over 8 s (traced, `0x237994` /
`0x23670c`):** `0xd9` ×15 (the self-reposted watchdog) + `0xda` ×1 (at t=0.412) +
**zero frame dispatches** (no `0x70`, ever). The supervisor runs in near-isolation: it
issues a `0x5f00` request that is dropped (channel disabled) and waits for a response
that never comes. The single `0xda` (handler `0x237ade`) is a one-time flag-state
command (clears flags `0x40`/`0x04`, posts a follow-up); it does not ack the watchdog.
The command jump table (`0x2379c0`): `d9→0x237b28 da→0x237ade db→0x237a7a dc→0x237a70
dd→0x237a18 de→0x2379fa e1→0x2379f2 e2→0x2379e8`. The richer handlers (`0xdd` re-inits
and posts event `0x1e`; `0xdb`…) are never driven because the task never receives them.

### ROOT CAUSE — the watchdog is a symptom; the real gate is the lower-service idle check

Tracing the arming function `0x29bc70` showed it is called from the **mode-0x000d
startup readiness loop** at `0x2a92fc`, and its first action is to **reset the watchdog
counter** (`0x11fed6 = 0` @0x29bc78). So while that loop iterates, the watchdog stays
reset. The loop runs `0x29bc70`, then checks readiness predicates; on any failure it
sets a wait flag and **busy-waits at `0x2a9344` until the flag clears**, only then
re-running `0x29bc70`. The counter only runs away because the loop **stalls** and stops
resetting it. Confirmed: the loop runs **once** (t=0.405), fails, and never re-iterates.

The failing predicate is the **first** one (traced, `0x2a930e`):
`service_context_ready_2b03d8 == 0`, and its failing sub-check is
`service_lower_idle_check_2ad1c8 == 0` (short-circuits before `service_event_queue_empty`).

Exact stuck bytes at the failing idle check (traced @0x2b03e0, t=0.405):

```
service_lower_queue_block   0x110d30 = 01   (nonzero → not idle)
service_lower_tx_busy_d     0x10f4ac = 01   (nonzero → not idle)
service_lower_tx_flags_a    0x10f4a8 = 08   (nonzero → not idle)
service_lower_queue_block_4 0x110d34 = 00
service_channel_ready_flags 0x111794 = 00   (ready bit 0x08 never set)
```

So the real boot blocker is **Branch 1 (Lower-Service Idle Gate), below**: the
lower-service **transmit of the D0 startup frame is stuck "busy"** (queue_block/tx_busy_d
set, ready bit clear) and `service_transport_complete_2b052e` never runs to clear it. On
real hardware the MBUS/CBUS TX-done event drives that completion; in emulation it does
not fire, so the queue never goes idle → `service_context_ready` fails forever → the
startup loop stalls → the contact-service watchdog (a normal parallel task) free-runs to
15 → CONTACT SERVICE. The D9/`0x5f00`/`0x70`-channel machinery is entirely downstream
and is NOT the thing to fix.

**This is the D0 startup-frame transport completion** (MBUS activity at t≈0.238–0.271s;
see `complete_mbus_d0_startup_frame` / `m_mbus_flow_d0_*` in the driver).

### Traced to the exact abort: MBUS RX state machine `0x2aae76`

The completion (`service_transport_complete_2b052e`) is called only from the MBUS RX
state machine `0x2aae76` (reads MAD2 reg `0x1a`, runs a state machine on the state byte
`0x10f4a8`, RX countdown `0x10f4ae`, RX byte buffer `0x10f4ad`, write ptr `0x10f4b0`).
Traced state progression: **0 → 1 → 8, then frozen at state 8** (terminal) for the rest
of the run; `service_transport_complete_2b052e` is **never called (0 times)**.

- State 0 (`0x2aaf86`): accepts RX byte `0x1f` (frame-start) → state 1, stores `0x1f`
  to `0x10f4b4`.
- State 1 (`0x2aaf44`): requires `[0x10f4b4]==0x1f` (ok) **and** the next RX byte
  `[0x10f4ad]` to equal the expected address byte `[0x111794]`. On mismatch it falls to
  `0x2aaf56`, clears MBUS ctrl bit `0x40`, and **sets state = 8 (abort)**.

Confirmed at runtime (t=0.2714):

```
state-1 cmp:  rx_byte[10f4ad] = 0x08   vs   expected [0x111794] = 0x00   → MISMATCH → abort(state 8)
canned MBUS_RX_BYTES = 0x1f,0x08,0xff,…   ← byte[1]=0x08 is what the firmware rejects
```

So the **canned MBUS RX response is malformed**: the firmware's lower-MBUS protocol
expects the second response byte to equal `[0x111794]` (the frame address byte, `0x00`
here), but the injected feed supplies `0x08`. The RX machine aborts into terminal state
8, which leaves the lower-service queue permanently "busy" → idle check fails → startup
loop stalls → contact-service watchdog fires → CONTACT SERVICE.

### RESOLVED (option A): the D0 MBUS response is now modelled in the driver

The bus response is generated from the transmitted frame instead of canned bytes
(`mbus_generated_response_byte` in the driver): the device answers with the same frame
addressed back to the phone — `dest := original src` (so byte[1] matches `[0x111794]`,
the phone's own address) and `src := original dest`, padded with two trailer bytes so
the state-6 countdown (`len+2`) reaches 0 and completion fires. The malformed canned
`MBUS_RX_BYTES` default has been removed from the recipe; the model is self-sufficient
(verified with the env emptied) and is **frame-oracle-preserving** (`d8a9a7`).

Result — the boot now advances **two further readiness gates** before stalling:

```
service_lower_idle_check_2ad1c8  r0=1  ✓ (was 0)
service_context_ready_2b03d8     r0=1  ✓ (was THE blocker)
display_init_ready_2a2680        r0=1  ✓ (the old "0x0199" display gate)
task14_ready_28ff14              r0=0  ✗  ← NEW blocker
```

The oracle frame is still `d8a9a7` because the contact-service watchdog still fires on
the new blocker (same CONTACT SERVICE screen) — progress is internal, measured by the
`cs_pred:` trace, not yet by the frame hash. **Next blocker: `task14_ready_28ff14`.**

Caveat: the response model is a faithful *mirror* of the request (good enough to
satisfy the RX state machine and unblock two gates), not yet a byte-accurate model of
the real bus device. Revisit if a later gate needs specific response content.

### Mode-0x000d startup readiness gate chain (mapped)

The loop at `0x2a92fc` must pass all of these (in order) before it advances out of
mode 0x000d (`0x2a934c` path); any failure stalls it and the watchdog fires:

| # | predicate | needs | status |
|---|---|---|---|
| 1 | `service_context_ready_2b03d8` | lower-service idle + event queue empty | ✓ fixed (MBUS model) |
| 2 | `display_init_ready_2a2680` | (the old `0x0199` display gate) | ✓ passing |
| 3 | `task14_ready_28ff14` | task-14 ready flags set | ✗ **current blocker** |
| 4 | `pred_2a6566` | `func_2a64fa() == 0` | not yet reached |
| 5 | `pred_2a0ec4` | a byte `== 0` | not yet reached |
| 6 | `pred_279282` | always returns 1 | free pass |

**Blocker 3 — `task14_ready_28ff14`** requires three conditions, all currently false:
- `[0x111c93]` (`FW_TASK14_READY_FLAG`) ≠ 0
- `func_26ec10()` ≠ 0 — true when `([0x10d1c0 HELPER_MODE]==0 && [0x10dcae HELPER_READY]==1)`
  or `([0x10d1c0]!=0 && [0x10dcae]!=0)`
- `[0x10dcb0]` (`FW_TASK14_FINAL_READY_FLAG`) ≠ 0

Traced lifecycle: **none of these flags is ever written to non-zero** — they are only
zeroed at boot init (`pc=0x200168`). So task 14 never completes its startup. The flags
are written by task 14 itself via a computed struct pointer (no direct-literal writer in
ROM). Predecessor knobs that used to *force* these flags (now removed) were
`FORCE_TASK14_READY` / `FORCE_TASK14_READY_FLAGS`, confirming task-14 readiness was
always the real, unmodeled dependency here.

Dig-in findings (probes `t14_drive:` / `t14_tcb:`, `TRACE_CONTACT_COMMIT=1`):
- `0x26a204(0x14, msg)` is `sched_post_task_message` → it indexes a 28-byte-per-task
  TCB array at base `0x101484`; task 0x14's TCB is `0x1016b4`.
- The **task-14 notification chain is entirely dormant**: the message sender
  `0x28ff38` is never called, and its event-dispatcher caller `0x275ffc` (handles event
  codes `0x64`/`0x65`, called from `0x27609c`/`0x2768d8`) is never reached in 8 s.
- Task 0x14's TCB reads idle (`ffff ffff ffff …` — empty message queue) at the failing
  readiness check (t=0.405).
- Candidate flag-setter: the READY-flag base `0x111c90` is referenced by code near
  `0x26fb14` (`0x26fxxx` scheduler/task region) — the exact setter and its gate are the
  next thread to pull.

Open: whether the ready flags are set by task 14's own (unscheduled/stalled) startup
body, or only via the dormant notification chain. The chain being dormant suggests an
upstream event branch (possibly fed by the service layer we just started modelling) is
not yet exercised — i.e. task 14 may unblock as more of the boot comes alive. Next:
find task 14's startup body / what its readiness waits on.

Resolved (probes `task_recv_seen:` / `task_dispatched:`): **task 14's body never runs —
the task is never dispatched.** The scheduler current-task id is at `0x100022`; over the
whole 8 s boot only **four tasks ever run**: `0x00` (t=0.0015, init/idle), `0x07`
(t=0.2508), `0x01` (t=0.2512, the startup/MMI task that runs the readiness loop), and
`0x02` (t=0.3385, the contact-service task, body `0x237bb4`). Task `0x14` is never among
them. So task 14 is not "stuck" — it is never started/created by the time the boot
stalls at ~0.4 s. The gate is therefore the **task-start/creation mechanism**: what
creates or resumes task 14, and why it has not happened. Candidate angle: task 0x02
(contact-service) is first dispatched at t=0.3385 — finding what started it reveals the
start mechanism, then check whether task 14 is started by the same (gated) path or a
later boot phase we have not reached. The READY-flag base `0x111c90` (setter near
`0x26fb14`) only matters once task 14 actually runs.

### RESOLVED: task 14 is deferred by the `FW_STARTUP_SERVICE_BUFFER` gate

Full trace of the task-start mechanism (probes `task_dispatched:` / `ready_insert:` /
`resume_gate:`):
- Scheduler dispatch (`0x269bae`) pops the **ready list** head (`[0x10004c]`); a task is
  made runnable by the resume function `0x269c6e` (state 5 → 2, then insert via
  `0x2699be`).
- `0x269c6e` is called only from one **startup task-resume sequence** at `0x2a9120`,
  which resumes ~25 tasks in two parts: an always-on **core set** (7, 1, 2) and an
  **extended batch** (`0xa,0xb,0xc,0xd,0x10,0xf,0xe,0x15,`**`0x14`**`,0x11,0x12`).
- The sequence runs once at **t=0.2508, mode `0x0001`**, and **diverts to the minimal
  (core-only) path** at the early gate `0x2a9132`: it skips the extended batch when
  **`[0x110c2c] == 0`** (the other trigger, `[r7+1]==5`, was false: `[r7+1]=01`).
  Measured: `[0x110c2c]=00` → extended batch (incl. task 14) **deferred**.

`0x110c2c` is **`FW_STARTUP_SERVICE_BUFFER`** — the startup service-ready byte. The
removed `FORCE_STARTUP_SERVICE_READY` knob forced exactly this byte at exactly the gate
PC (`0x2a9132`, mask `0xff00`), confirming this is the genuine gate. So:

```
CONTACT SERVICE ← readiness loop stalls on task14_ready ← task 14 never runs
  ← never resumed ← extended resume batch deferred ← FW_STARTUP_SERVICE_BUFFER(0x110c2c)==0
```

Note: forcing that byte was audited **frame-inert** (oracle stayed `d8a9a7`), i.e. resuming
task 14 alone does not flip the visible screen — there are further gates downstream
(same as the MBUS fix: internal progress, not yet a frame change). Next threads:
(1) what naturally sets `FW_STARTUP_SERVICE_BUFFER` (the service-startup completion —
likely tied to the service layer we are now modelling), and whether the resume sequence
re-runs after it is set; (2) an MBUS-style experiment — set the gate at `0x2a9132`,
confirm task 14 resumes and runs, and trace how far `task14_ready` and the rest of the
readiness chain then get.

### Update (investigations on threads 1 and 2)

**Thread 1 (what sets the gate byte) — DEAD END, corrected understanding.** Modelling
the service-startup function `0x2909e4` revealed the gate byte `0x110c2c` is **never set
non-zero by design**: it is written once at `0x290c9c` as `strb r4`, and `r4` is set to
`0` by a *straight-line* `movs r4,#0` at `0x290b9a` (a clear-loop constant; confirmed
at runtime `r4=0`). There is no branch that makes `r4` non-zero before the write. So the
mode-1 resume sequence (`0x2a9120`) **always diverts to the core-only path** and defers
the extended task batch — task 14 *and* ~16 other tasks (8,3,4,5,6,9, and batch 2) — on
**every** boot, not just ours. This is normal phased bootstrap: mode 1 starts only the
core tasks (7,1,2); the extended tasks are resumed in a **later phase**.

Therefore the gate byte is NOT the natural task-14 resume mechanism, and "set the gate
byte" is a hack (the `EXPERIMENT_RESUME_TASK14` knob only resumes batch 1 anyway — task
14 is in batch 2 behind a second gate). The real question is what advances the boot from
the core-task phase to the phase that resumes the extended tasks. The only callers of the
make-ready function `0x269c6e` are this one sequence, so the extended tasks come up only
when this sequence is re-run under conditions that pass its gates — which happens once
the **core tasks (esp. the service task 0x02) complete their phase-1 work**. That loops
back to the service/MBUS layer: the honest fix is still "bring the service layer up", but
via the core **service task completing its startup**, not via this gate byte.

### Deeper dig (Option A, "model it for real"): the gate byte is a service-completion flag

Corrected the earlier conflation: `0x2909e4` (service-startup) ends at `0x290b26`; the
gate-byte writer is a **separate function `0x290b54`** that is a service-buffer *reset*
(clears the buffer, writes the gate byte `[0x110c2c+0]` = 0). So the `0` is just the
reset value, not "never ready". The service buffer is a small struct at `0x110c2c`:
- offset 0 (`0x110c2c`, the byte the resume gate reads): **service-ready flag**, reset
  to 0 by `0x290b54`, and **never written non-zero anywhere found**.
- offset 0 low (`0x110c2d`): hardware-variant byte, set to 1 by `0x290a0c`.
- offset 2 (`0x110c2e`): a **status word**, actively managed (set to `0x8002`) by the
  service function `0x290cf8` (bit-field handler at `0x290ef0`+) at mode `0x000d`.

So the service buffer's *status* (offset 2) does get populated, but the *ready flag*
(offset 0) that the resume gate reads is only ever reset, never set. Searching all four
code references to the buffer base (`0x290c0c`, `0x291004`, `0x291280`, `0x2a93a0`): two
are the reset function (`0x290b54`), one is the resume-gate reader (`0x2a93a0`), and the
status handler writes only offsets +2/+4/+0xe — none sets offset 0 non-zero.

**Honest conclusion:** the ready flag (`0x110c2c+0`) is set by a service-*completion*
event deeper than the status updates — i.e. it requires the service-startup to fully
finish, which bottoms out (again) at the service-transport/MBUS layer completing. There
is no single discrete function to "model" here; "model it for real" for task 14 means
incrementally completing the service-startup state machine (the same layer the MBUS D0
response began), not a one-shot fix. The `EXPERIMENT_*` knobs remain valid scaffolding to
map the boot past this point while that layer is built up.

**Thread 2 (experiment — diagnostic forces, opt-in env knobs, NOT real models):**
- `NOKI3210_EXPERIMENT_RESUME_TASK14=1` (force gate `0x2a9132` non-zero): batch 1 now
  runs → **12 tasks dispatched** (was 4: `7,8,3,4,1,0x13,0x16,9,2,6,5`). But task 14 is in
  **batch 2**, behind a *second* gate at `0x2a9186` (needs `[r7+1]==0`; it is `01`), so
  task 14 still isn't resumed. Task 14 has two gates.
- `NOKI3210_EXPERIMENT_FORCE_TASK14_READY=1` (force `task14_ready` to return 1 at
  `0x2a931e`): **all six readiness predicates pass, the loop advances, and the boot
  leaves CONTACT SERVICE for the first time** — frame changes `d8a9a7 → 94a2dc…` (a
  the run is much slower = lots of new code executing = real progress). This confirms
  `task14_ready` is the genuine gate and that the whole readiness chain
  (`service_context_ready`, `display_init_ready`, `task14_ready`, `2a6566`, `2a0ec4`,
  `279282`) is otherwise satisfiable. The `94a2dc` state is a **stable blank LCD**
  (identical at 8 s and 16 s emulated), so the boot clears the fault screen and advances
  to a new startup phase that has not yet drawn a UI — the next blocker lives at
  `94a2dc`. (Same further-along state the old fully-forced profiles reached.)

**Conclusion:** the path past CONTACT SERVICE runs through task-14 readiness, which runs
through `FW_STARTUP_SERVICE_BUFFER`, which runs through the **service-startup state
machine reaching ready** — i.e. the service layer we began modelling with the MBUS D0
response. The honest fix is to keep bringing that layer up so the gate byte is set
naturally by mode 1; the experiment proves the payoff (boot advances past the fault
screen) and that there are further boot phases beyond it (the 94a2dc blank screen is the
next state to investigate).

Diagnostic: `make run-boot-progress … TRACE_CONTACT_COMMIT=1` logs the predicate chain
(`cs_pred:`), the idle bytes (`cs_idle:`), the RX state machine (`rx_sm:`), the task
input set (`cs_disp:`), and the commit (`contact_commit:`).

**Key fixed addresses:** terminal halt `0x2b4dda`; D9 timeout builder `0x236dc4`;
D9 dispatch `0x237994`; D9 handler/poll `0x237b2e`; watchdog counter `0x11fed6`;
ack `0x11fedb`; channel enable `0x11fee4`; channel map `0x11ff13`; contact-service
task `0x237bb5` (table `0x2d70a8`); init `0x2346b2`.

---

## Branch 1: Lower-Service Idle Gate

`service_context_ready_2b03d8` passes only when both subordinate checks return nonzero:

```text
service_context_ready_2b03d8
  -> service_lower_idle_check_2ad1c8 must return 1
  -> service_event_queue_empty_283dce must return 1
```

Static pass conditions for `service_lower_idle_check_2ad1c8`:

- `0x110d30` `service_lower_queue_block` must be zero.
- `0x10f4ac` `service_lower_tx_busy_d` must be zero.
- `0x10f4a8` `service_lower_tx_flags_a` must be zero.
- `0x110d34` `service_lower_queue_block_4` must be zero.
- `0x111794` `service_channel_ready_flags` must have ready bit `0x08` set.
- scheduler/event id `0x64` must be absent (`sched_post_event2_2698e4(0x64)` returns zero).

Observed current failing shape: lower frame `1f/ff/08/d0/00/01/01/00` is queued, `service_lower_queue_block` and `service_lower_tx_busy_d` become nonzero, and `service_transport_complete_2b052e` does not run before the predicate loop.

Expected completion path:

```text
MAD2 MBUS interrupt/RX state machine
  -> service_lower_mbus_rx_state_machine_2aae76
  -> service_transport_complete_2b052e
  -> service_lower_queue_event_2ad2c2 / drain
  -> busy bytes clear
```

## Branch 2: Contact-Service Watchdog Gate

`contact_service_case_d9_watchdog_poll_237b28` uses `0x11fe68` as a base and polls the contact-service state bytes:

- `0x11fedb` `contact_service_ack`
- `0x11fed6` `contact_service_watchdog_counter`
- `0x11fed1` `contact_service_flags`
- `0x11feda` `contact_service_substate`

Static pass/fail logic:

```text
if contact_service_ack != 0:
    contact_service_watchdog_counter = 0
else:
    contact_service_watchdog_counter += 1

if contact_service_watchdog_counter >= 0x0f:
    contact_service_timeout_signal_236dc4(2)
```

The timeout builder creates response `0x64` with detail byte `0x45`, sets bit 0 in `contact_service_flags`, then queues the fatal contact-service frame. The current best-progress profile keeps the loop stable at `flags=0x08`, `counter=0x00`, `substate=0x03`, `ack=0x01`, and startup mode `0x000b`. Clearing ACK or forcing flag bit 0 as a blanket D9 fix are negative probes. Bit 0 is still a real dispatch gate for some contact responses, including `0x70`; it must be set in the right response-delivery context, not as a standing watchdog state. A real pass likely requires the service layer to deliver the coherent response/event that stops D9 being reposted, not more direct byte forcing.

Candidate static response path:

```text
contact_service_response_dispatch_237400
  -> response 0x70/0x71 dispatches to contact_response_code_70_71_handler_23670c
  -> handler builds a follow-up 0x70 or 0x71 frame with contact_service_state_alloc_234634
  -> contact_service_state_release_234684 queues it via service_transport_queue_2b0482
```

Refined 0x70/0x71 semantics:

- The main dispatch table is gated at `contact_response_dispatch_gate_check_23742a`
  by bit 0 of contact status byte `0x11fed1`. If the bit is clear, a `0x70`
  frame drops to `contact_response_gated_or_unknown_drop_237896`.
- `0x70` is the channel-map enable response. It calls
  `contact_response_70_channel_map_apply_2366c8`, which requires length byte
  `frame[5] > 0x42`, sums the 0x40-byte payload at `frame + 9`, and then calls
  `service_channel_config_2b140a`.
- `0x71` disables the channel map and sends the sibling follow-up frame.
- Diagnostic receive-side injection of a gated `0x70` frame is accepted and posts
  follow-up task-7 frame `02/00/f0/40/00/04/01/01/70/01/ff/ff...`, but it still
  ends in contact-service reset reason `0x06` at `0x2b4e08`. The next missing
  piece is the lower service/MBUS completion for that follow-up frame.
- Current natural runs do not deliver a task-7 response frame to this dispatcher,
  so upstream response delivery remains unsolved.

Open static question: which response path consumes or completes D9 so `sched_recv_current_task_message_26a458` stops returning the watchdog command. The queue record written near `0x237994` contains command `0xd9`, target pointer `0x11fedb`, and callback/link `0x237bc7`.

Additional D9 finding: `contact_service_d9_e2_command_dispatch_237994` calls
`service_channel_request_if_enabled_2b13a2(2, 0x5f00, sp)` before its local
watchdog branch. The channel lookup at `service_channel_lookup_2b12b4` requires
global enable byte `0x11fee4` and channel-map byte `0x11ff13 & 0x01`. The active
profile does not naturally enable this channel. Testing should use a scoped RAM
read override for those lookup bytes; instruction-fetch side effects are too
late to prove the lookup result.

If the lookup passes, `service_channel_build_request_2b12dc` immediately rereads
the global enable byte at `0x2b12f0` and branches to its return path at
`0x2b1398` when it is zero. The 0x5f00 diagnostic must therefore cover both the
lookup gate and this builder/send gate before it can prove whether queueing and
service transport completion are the next blockers.

Runtime result with both gates forced: the builder queues frames shaped
`00/01/08/00/00/0a/00/01/5f/00/ff/ff/seq/02/00/d9` and `.../00/da` through
`service_transport_queue_2b0482`. The loop still returns to D9 because the lower
MBUS/service response side is not completing these 0x5f00 requests yet.

Recent negative probes:

- Injecting a raw `0x65` frame through the MBUS RX byte stream does not redirect
  the built service-channel response; task 7 still sees `resp=0x5f`.
- If D9 seeds `contact_service_flags` with `0x44`, DA normally clears `0x40`
  and `0x04` at `0x237b04/0x237b0c`, returning to the stable `0x08` loop.
  Preserving `0x40` diagnostically leaves `flags=0x48` and skips the draw path
  at `0x237b58`, but it still loops. The branch that must be understood next is
  the 0x5f service/scheduler completion path, not the flag clear by itself.
- Rewriting the task-7 `resp=0x5f` D9 envelope into a 0x65 startup-status frame
  with status `0x00` or `0x3e` is also negative. The frame is delivered through
  `service_context_event_bit_set_2b0444` /
  `service_transport_d5_notify_wrapper_2b0474`, but state remains
  `flags=0x08 counter=0x00 substate=0x03 ack=0x01` and D9 is reposted.
  `service_transport_build_d5_notify_2a594c` builds the observed
  `1f/00/08/d5...` frame as a lower-transport notification from the original
  frame. With `BOOT_PROGRESS_FORCE_SERVICE_READY_BIT=0`, this path disappears
  entirely and the harness falls back to the direct D9 watchdog shim, so the
  next real target is natural 0x5f channel enablement/completion.

## Disassembly Windows

### service_context_ready_2b03d8

```asm
002b03d8: push     {r4, lr}
002b03da: movs     r4, #0
002b03dc: bl       #0x2ad1c8
002b03e0: cmp      r0, #0
002b03e2: beq      #0x2b03ee
002b03e4: bl       #0x283dce
002b03e8: cmp      r0, #0
002b03ea: beq      #0x2b03ee
002b03ec: movs     r4, #1
002b03ee: adds     r0, r4, #0
002b03f0: pop      {r4, pc}
```

### service_lower_idle_check_2ad1c8

```asm
002ad1c8: push     {r4, lr}
002ad1ca: ldr      r0, [pc, #0x340]
002ad1cc: movs     r4, #0
002ad1ce: ldr      r1, [pc, #0x340]
002ad1d0: ldrb     r2, [r1]
002ad1d2: ldr      r1, [pc, #0x340]
002ad1d4: ldrb     r1, [r1]
002ad1d6: orrs     r2, r1
002ad1d8: ldrb     r1, [r0]
002ad1da: orrs     r1, r2
002ad1dc: ldrb     r0, [r0, #4]
002ad1de: orrs     r0, r1
002ad1e0: cmp      r0, #0
002ad1e2: bne      #0x2ad1f8
002ad1e4: ldr      r0, [pc, #0x330]
002ad1e6: ldrb     r0, [r0]
002ad1e8: lsrs     r0, r0, #4
002ad1ea: blo      #0x2ad1f8
002ad1ec: movs     r0, #0x64
002ad1ee: bl       #0x2698e4
002ad1f2: cmp      r0, #0
002ad1f4: bne      #0x2ad1f8
002ad1f6: movs     r4, #1
002ad1f8: adds     r0, r4, #0
002ad1fa: pop      {r4, pc}
```

### service_lower_enqueue_external_frame_2ad3e4

```asm
002ad3e4: push     {r4, r5, r6, lr}
002ad3e6: adds     r6, r0, #0
002ad3e8: ldr      r0, [pc, #0x120]
002ad3ea: ldrb     r0, [r0]
002ad3ec: cmp      r0, #0x14
002ad3ee: bge      #0x2ad41e
002ad3f0: ldrh     r0, [r6, #4]
002ad3f2: adds     r1, r0, #0
002ad3f4: adds     r1, #6
002ad3f6: lsls     r1, r1, #0x10
002ad3f8: lsrs     r5, r1, #0x10
002ad3fa: adds     r0, #8
002ad3fc: lsls     r0, r0, #0x10
002ad3fe: lsrs     r0, r0, #0x10
002ad400: bl       #0x26ae4c
002ad404: adds     r4, r0, #0
002ad406: cmp      r4, #0
002ad408: beq      #0x2ad41e
002ad40a: adds     r1, r6, #0
002ad40c: adds     r2, r5, #0
002ad40e: bl       #0x2b6334
002ad412: movs     r0, #1
002ad414: adds     r1, r4, #0
002ad416: bl       #0x2ad2c2
002ad41a: movs     r0, #1
002ad41c: pop      {r4, r5, r6, pc}
002ad41e: movs     r0, #0
002ad420: pop      {r4, r5, r6, pc}
```

### service_lower_tx_busy_set_2aaca8

```asm
002aaca8: push     {r4, r5, r6, r7, lr}
002aacaa: adds     r7, r0, #0
002aacac: bl       #0x2aa8fa
002aacb0: ldr      r5, [pc, #0x3b8]
002aacb2: ldrb     r0, [r5, #4]
002aacb4: cmp      r0, #0
002aacb6: bne      #0x2aacda
002aacb8: ldr      r4, [pc, #0x3b4]
002aacba: ldrb     r0, [r4, #0xa]
002aacbc: lsrs     r0, r0, #4
002aacbe: blo      #0x2aacd6
002aacc0: movs     r6, #8
002aacc2: strb     r6, [r4, #8]
002aacc4: bl       #0x2aa8fa
002aacc8: movs     r0, #0xf7
002aacca: ldrb     r1, [r4, #0xa]
002aaccc: ands     r0, r1
002aacce: strb     r0, [r4, #0xa]
002aacd0: bl       #0x2aa914
002aacd4: strb     r6, [r4, #8]
002aacd6: strb     r7, [r5, #4]
002aacd8: b        #0x2aacdc
002aacda: strb     r7, [r5, #2]
002aacdc: bl       #0x2aa914
002aace0: pop      {r4, r5, r6, r7, pc}
```

### service_lower_mbus_rx_state_machine_2aae76

```asm
002aae76: push     {r4, r5, r6, lr}
002aae78: ldr      r5, [pc, #0x1f4]
002aae7a: ldr      r4, [pc, #0x1f0]
002aae7c: ldrb     r2, [r5, #0x1a]
002aae7e: strb     r2, [r4, #5]
002aae80: movs     r0, #4
002aae82: strb     r0, [r5, #8]
002aae84: ldrb     r1, [r4]
002aae86: cmp      r1, #0
002aae88: beq      #0x2aaf86
002aae8a: subs     r1, #1
002aae8c: cmp      r1, #0
002aae8e: beq      #0x2aaf44
002aae90: subs     r1, #1
002aae92: cmp      r1, #0
002aae94: beq      #0x2aaf3a
002aae96: subs     r1, #1
002aae98: cmp      r1, #0
002aae9a: beq      #0x2aaf32
002aae9c: subs     r1, #1
002aae9e: cmp      r1, #0
002aaea0: beq      #0x2aaf00
002aaea2: subs     r1, #1
002aaea4: cmp      r1, #0
002aaea6: beq      #0x2aaee4
002aaea8: subs     r1, #1
002aaeaa: cmp      r1, #0
002aaeac: beq      #0x2aaeb0
002aaeae: b        #0x2aafb2
002aaeb0: ldr      r0, [r4, #8]
002aaeb2: strb     r2, [r0]
002aaeb4: ldr      r0, [r4, #8]
002aaeb6: adds     r0, #1
002aaeb8: str      r0, [r4, #8]
002aaeba: ldrh     r0, [r4, #6]
002aaebc: subs     r0, #1
002aaebe: lsls     r0, r0, #0x10
002aaec0: lsrs     r0, r0, #0x10
002aaec2: strh     r0, [r4, #6]
002aaec4: cmp      r0, #0
002aaec6: bne      #0x2aafb2
002aaec8: bl       #0x2aa8fa
002aaecc: movs     r0, #0xbf
002aaece: ldrb     r1, [r5, #0x18]
002aaed0: ands     r0, r1
002aaed2: strb     r0, [r5, #0x18]
002aaed4: bl       #0x2aa914
002aaed8: movs     r0, #8
002aaeda: strb     r0, [r4]
002aaedc: ldr      r0, [pc, #0x19c]
002aaede: bl       #0x2b052e
002aaee2: pop      {r4, r5, r6, pc}
002aaee4: ldr      r0, [pc, #0x194]
```

### service_transport_complete_2b052e

```asm
002b052e: push     {r4, r5, r6, lr}
002b0530: adds     r5, r0, #0
002b0532: ldrb     r1, [r5, #2]
002b0534: ldr      r0, [pc, #0xe0]
002b0536: ldrb     r0, [r0]
002b0538: cmp      r1, r0
002b053a: beq      #0x2b0598
002b053c: ldr      r0, [pc, #0xd0]
002b053e: ldr      r0, [r0]
002b0540: cmp      r5, r0
002b0542: blo      #0x2b055a
002b0544: ldr      r0, [pc, #0xcc]
002b0546: ldr      r0, [r0]
002b0548: cmp      r5, r0
002b054a: bhi      #0x2b055a
002b054c: movs     r0, #7
002b054e: adds     r1, r5, #0
002b0550: bl       #0x26aac0
002b0554: cmp      r0, #1
002b0556: bne      #0x2b0598
002b0558: pop      {r4, r5, r6, pc}
002b055a: ldrh     r4, [r5, #4]
002b055c: ldrb     r0, [r5]
002b055e: cmp      r0, #0x1f
002b0560: beq      #0x2b0566
002b0562: adds     r4, #6
002b0564: b        #0x2b0570
002b0566: lsls     r0, r4, #0x11
002b0568: lsrs     r0, r0, #0x11
002b056a: lsls     r0, r0, #0x10
002b056c: lsrs     r4, r0, #0x10
002b056e: adds     r4, #8
002b0570: lsls     r0, r4, #0x10
002b0572: lsrs     r4, r0, #0x10
002b0574: adds     r0, r4, #0
002b0576: bl       #0x26ae4c
002b057a: adds     r6, r0, #0
002b057c: cmp      r6, #0
002b057e: beq      #0x2b059e
002b0580: adds     r1, r5, #0
002b0582: adds     r2, r4, #0
002b0584: bl       #0x2b6334
002b0588: movs     r0, #7
002b058a: adds     r1, r6, #0
002b058c: bl       #0x26aac0
002b0590: cmp      r0, #1
002b0592: beq      #0x2b059e
002b0594: adds     r0, r6, #0
002b0596: b        #0x2b059a
002b0598: adds     r0, r5, #0
002b059a: bl       #0x26abf8
002b059e: pop      {r4, r5, r6, pc}
```

### contact_service_d9_watchdog_poll_237b28

```asm
00237b28: movs     r0, #3
00237b2a: bl       #0x2b4dc0
00237b2e: movs     r0, #0x19
00237b30: ldr      r1, [pc, #0x1c8]
00237b32: bl       #0x2697aa
00237b36: movs     r1, #0x69
00237b38: movs     r4, #0x6e
00237b3a: movs     r6, #0x64
00237b3c: movs     r7, #0x6c
00237b3e: ldr      r5, [pc, #0x1cc]
00237b40: movs     r0, #0x73
00237b42: ldrb     r0, [r0, r5]
00237b44: cmp      r0, #0
00237b46: bne      #0x237b4e
00237b48: ldrb     r0, [r4, r5]
00237b4a: adds     r0, #1
00237b4c: b        #0x237b50
00237b4e: movs     r0, #0
00237b50: strb     r0, [r4, r5]
00237b52: ldrb     r0, [r1, r5]
00237b54: lsrs     r0, r0, #7
00237b56: bhs      #0x237b7a
00237b58: movs     r0, #0
00237b5a: bl       #0x2b4696
00237b5e: bl       #0x260144
00237b62: adr      r0, #0x160
00237b64: movs     r1, #0xd
00237b66: movs     r2, #0xf
00237b68: bl       #0x25edf2
00237b6c: adr      r0, #0x15c
00237b6e: movs     r1, #0xd
00237b70: movs     r2, #0x20
00237b72: bl       #0x25edf2
00237b76: bl       #0x2abd7c
00237b7a: ldrb     r0, [r4, r5]
00237b7c: cmp      r0, #0xf
00237b7e: blt      #0x237ba8
00237b80: movs     r0, #2
00237b82: bl       #0x236dc4
00237b86: movs     r0, #0x1c
00237b88: bl       #0x2698e4
00237b8c: strh     r0, [r6, r5]
00237b8e: adds     r0, r5, #0
00237b90: movs     r1, #0x6c
00237b92: bl       #0x2a41d0
00237b96: mvns     r0, r0
00237b98: strh     r0, [r7, r5]
00237b9a: movs     r0, #0x6c
00237b9c: bl       #0x2a936e
00237ba0: ldr      r0, [pc, #0x14c]
00237ba2: ldrb     r0, [r0]
00237ba4: bl       #0x2b4dda
00237ba8: add      sp, #0x10
```

### contact_service_da_status_update_237ade

```asm
00237ade: ldr      r2, [pc, #0x234]
00237ae0: ldrb     r0, [r2]
00237ae2: lsrs     r0, r0, #3
00237ae4: bhs      #0x237af8
00237ae6: ldrb     r0, [r2, #9]
00237ae8: cmp      r0, #2
00237aea: beq      #0x237af0
00237aec: movs     r0, #1
00237aee: b        #0x237af2
00237af0: movs     r0, #2
00237af2: bl       #0x2a2550
00237af6: b        #0x237ba8
00237af8: ldr      r1, [pc, #0x1fc]
00237afa: movs     r0, #0xf
00237afc: strb     r0, [r1]
00237afe: movs     r0, #0xbf
00237b00: ldrb     r1, [r2]
00237b02: ands     r0, r1
00237b04: strb     r0, [r2]
00237b06: movs     r0, #0xfb
00237b08: ldrb     r1, [r2]
00237b0a: ands     r0, r1
00237b0c: strb     r0, [r2]
00237b0e: ldrb     r0, [r2]
00237b10: lsrs     r0, r0, #5
```

### contact_service_e2_event_source_update_2379e8

```asm
002379e8: ldr      r0, [pc, #0x314]
002379ea: ldrb     r0, [r0]
002379ec: bl       #0x2a27de
002379f0: b        #0x237ba8
002379f2: movs     r0, #0
002379f4: bl       #0x29bd14
002379f8: b        #0x237ba8
002379fa: ldr      r0, [pc, #0x1b0]
002379fc: ldrb     r0, [r0]
002379fe: cmp      r0, #0
00237a00: bne      #0x237a10
00237a02: ldr      r0, [pc, #0x2dc]
00237a04: movs     r1, #0
00237a06: bl       #0x2afa74
00237a0a: ldr      r0, [pc, #0x2d8]
00237a0c: bl       #0x2b5ba8
00237a10: movs     r0, #0x66
00237a12: bl       #0x2a936e
00237a16: b        #0x237a7a
00237a18: movs     r0, #3
00237a1a: ldr      r1, [pc, #0x2cc]
```

### service_event_8e_handler_2837bc

```asm
002837bc: push     {r4, r5, r6, r7, lr}
002837be: mov      r0, r8
002837c0: push     {r0}
002837c2: ldr      r5, [pc, #0x2c0]
002837c4: ldr      r4, [r5, #0x14]
002837c6: ldrb     r0, [r5, #6]
002837c8: cmp      r0, #1
002837ca: bne      #0x283826
002837cc: ldr      r0, [pc, #0xe8]
002837ce: ldrb     r1, [r0]
002837d0: cmp      r1, #0x7f
002837d2: bne      #0x2837e0
002837d4: ldrb     r0, [r4, #5]
002837d6: cmp      r0, #0
002837d8: bne      #0x2837e0
002837da: bl       #0x2b05b2
002837de: b        #0x283826
002837e0: movs     r2, #1
002837e2: cmp      r1, #0
002837e4: bne      #0x2837f8
002837e6: ldrb     r0, [r4, #6]
002837e8: cmp      r0, #0
002837ea: bne      #0x2837f8
002837ec: ldrb     r0, [r4, #5]
002837ee: cmp      r0, #0x7f
002837f0: beq      #0x2837f6
002837f2: cmp      r0, #0xff
002837f4: bne      #0x2837f8
002837f6: movs     r2, #0
002837f8: cmp      r2, #0
002837fa: beq      #0x283806
002837fc: cmp      r1, #0x7f
002837fe: bne      #0x283826
00283800: ldrb     r0, [r4, #5]
00283802: cmp      r0, #0xff
00283804: bne      #0x283826
00283806: ldrb     r0, [r4, #7]
00283808: cmp      r0, #0xd7
0028380a: beq      #0x28381e
0028380c: cmp      r0, #0xd0
0028380e: bne      #0x283826
00283810: ldrb     r0, [r4, #0xa]
00283812: cmp      r0, #1
00283814: bne      #0x283826
00283816: movs     r0, #0x1c
00283818: bl       #0x2834ea
0028381c: b        #0x283a32
0028381e: ldrb     r0, [r4, #0xa]
00283820: cmp      r0, #2
00283822: bne      #0x283826
00283824: b        #0x283a2c
00283826: ldrb     r1, [r4, #5]
00283828: ldr      r0, [pc, #0x8c]
0028382a: ldrb     r0, [r0]
0028382c: cmp      r1, r0
0028382e: beq      #0x283832
00283830: b        #0x283a32
00283832: ldrb     r0, [r5, #0xe]
00283834: cmp      r0, #4
00283836: bne      #0x28383a
00283838: b        #0x283a32
0028383a: ldrb     r0, [r4, #4]
0028383c: movs     r1, #6
0028383e: adds     r1, r1, r4
00283840: bl       #0x283530
00283844: ldrb     r0, [r4, #6]
00283846: strb     r0, [r5]
00283848: ldrb     r0, [r4, #4]
0028384a: strb     r0, [r5, #0xd]
0028384c: ldrb     r0, [r4, #7]
0028384e: cmp      r0, #0x7f
00283850: bne      #0x283854
00283852: b        #0x283a24
00283854: ldrh     r0, [r4, #8]
00283856: adds     r2, r4, r0
00283858: ldrb     r1, [r2, #9]
0028385a: lsrs     r0, r1, #6
0028385c: blo      #0x283876
0028385e: ldr      r1, [pc, #0x11c]
00283860: movs     r0, #0
00283862: strb     r0, [r1]
00283864: movs     r0, #0x24
00283866: adds     r0, r0, r5
00283868: bl       #0x2aa052
0028386c: movs     r0, #1
0028386e: strb     r0, [r5, #7]
00283870: ldrh     r0, [r4, #8]
00283872: adds     r2, r4, r0
00283874: ldrb     r1, [r2, #9]
00283876: lsrs     r0, r1, #7
00283878: cmp      r0, #0
0028387a: bne      #0x283888
0028387c: adds     r0, r4, #0
0028387e: bl       #0x2835cc
00283882: ldrh     r0, [r4, #8]
00283884: adds     r2, r4, r0
00283886: ldrb     r1, [r2, #9]
00283888: lsrs     r0, r1, #7
0028388a: blo      #0x2838a0
0028388c: movs     r0, #0x24
0028388e: adds     r0, r0, r5
00283890: bl       #0x2aa052
00283894: movs     r0, #1
00283896: strb     r0, [r5, #7]
00283898: ldrh     r0, [r4, #8]
0028389a: adds     r2, r0, r4
0028389c: adds     r0, r0, r4
0028389e: ldrb     r1, [r0, #9]
002838a0: ldr      r6, [pc, #0xd8]
002838a2: lsrs     r0, r1, #6
002838a4: bhs      #0x2838c8
002838a6: ldrb     r0, [r6]
002838a8: cmp      r0, #0
002838aa: bne      #0x2838bc
002838ac: lsls     r0, r1, #0x1d
002838ae: lsrs     r0, r0, #0x1d
002838b0: cmp      r0, #7
002838b2: bne      #0x2838c8
002838b4: b        #0x28390e
002838b6: mov      r8, r8
002838b8: movs     r1, r2
002838ba: asrs     r4, r2, #0x1e
002838bc: lsls     r3, r1, #0x1d
002838be: lsrs     r3, r3, #0x1d
002838c0: subs     r0, r3, r0
002838c2: adds     r0, #1
002838c4: cmp      r0, #0
002838c6: beq      #0x28390e
002838c8: movs     r3, #0
002838ca: ldrb     r0, [r5, #7]
002838cc: cmp      r0, #1
002838ce: bne      #0x2838d2
002838d0: movs     r3, #1
002838d2: lsrs     r0, r1, #6
002838d4: tst      r3, r0
002838d6: beq      #0x28390e
002838d8: ldrb     r0, [r2, #8]
002838da: cmp      r0, #1
002838dc: bne      #0x28390e
002838de: ldrh     r0, [r4, #8]
002838e0: subs     r0, #2
002838e2: lsls     r0, r0, #0x10
002838e4: lsrs     r0, r0, #0x10
002838e6: strh     r0, [r4, #8]
002838e8: adds     r0, #6
002838ea: lsls     r0, r0, #0x10
002838ec: lsrs     r0, r0, #0x10
002838ee: bl       #0x26afe0
002838f2: str      r0, [r5, #0x18]
002838f4: movs     r1, #4
002838f6: adds     r1, r1, r4
002838f8: ldrh     r2, [r4, #8]
002838fa: adds     r2, #6
002838fc: bl       #0x2b5c7c
00283900: ldrh     r0, [r4, #8]
00283902: adds     r0, #2
00283904: strh     r0, [r4, #8]
00283906: ldr      r0, [r5, #0x18]
00283908: bl       #0x2b052e
0028390c: b        #0x283a08
0028390e: lsrs     r0, r1, #6
00283910: bhs      #0x28392a
00283912: lsls     r0, r1, #0x1d
00283914: lsrs     r0, r0, #0x1d
00283916: ldrb     r3, [r6]
00283918: cmp      r3, #0
0028391a: bne      #0x283922
0028391c: cmp      r0, #7
0028391e: bne      #0x28392a
00283920: b        #0x283966
00283922: subs     r0, r0, r3
00283924: adds     r0, #1
00283926: cmp      r0, #0
00283928: beq      #0x283966
0028392a: movs     r3, #0
0028392c: ldrb     r0, [r5, #7]
0028392e: cmp      r0, #1
00283930: bne      #0x28393a
00283932: ldrb     r0, [r2, #8]
00283934: cmp      r0, #1
00283936: beq      #0x28393a
00283938: movs     r3, #1
0028393a: lsrs     r0, r1, #6
0028393c: tst      r3, r0
0028393e: beq      #0x283966
00283940: ldr      r7, [pc, #0x3a0]
00283942: ldrb     r0, [r4, #2]
00283944: adds     r0, #4
00283946: lsls     r0, r0, #0x10
00283948: lsrs     r2, r0, #0x10
0028394a: adds     r0, r7, #0
0028394c: adds     r1, r4, #0
0028394e: bl       #0x2a9ea6
00283952: cmp      r0, #0
00283954: bne      #0x28395e
00283956: ldrh     r0, [r4, #8]
00283958: adds     r0, r4, r0
0028395a: ldrb     r0, [r0, #8]
0028395c: b        #0x283a06
0028395e: adds     r0, r7, #0
00283960: bl       #0x2aa052
00283964: b        #0x283a08
00283966: lsrs     r0, r1, #6
00283968: bhs      #0x283988
0028396a: lsls     r0, r1, #0x1d
0028396c: lsrs     r0, r0, #0x1d
0028396e: ldrb     r3, [r6]
00283970: cmp      r3, #0
00283972: bne      #0x283980
00283974: cmp      r0, #7
00283976: bne      #0x283988
00283978: b        #0x283a08
0028397a: mov      r8, r8
0028397c: movs     r1, r2
```

### service_event_source_8d_handler_283db6

```asm
00283db6: str      r0, [r5, #0x14]
00283db8: cmp      r0, #0xc0
00283dba: blo      #0x283dc4
00283dbc: movs     r1, #0xff
00283dbe: adds     r1, #0xc0
00283dc0: cmp      r0, r1
00283dc2: bls      #0x283d6e
00283dc4: cmp      r0, #0xb1
00283dc6: bne      #0x283cf2
00283dc8: bl       #0x283b4e
00283dcc: b        #0x283db2
00283dce: movs     r1, #0
00283dd0: ldr      r0, [pc, #0xd0]
00283dd2: ldr      r2, [r0, #0x1c]
00283dd4: cmp      r2, #0
00283dd6: bne      #0x283de0
00283dd8: ldr      r0, [r0, #0x24]
00283dda: cmp      r0, #0
00283ddc: bne      #0x283de0
00283dde: movs     r1, #1
00283de0: lsls     r0, r1, #0x18
00283de2: lsrs     r0, r0, #0x18
00283de4: mov      pc, lr
00283de6: mov      r8, r8
00283de8: movs     r1, r2
00283dea: asrs     r4, r2, #0x1e
00283dec: movs     r1, r2
00283dee: movs     r1, #0x50
00283df0: push     {r4, lr}
00283df2: movs     r2, #1
00283df4: ldrb     r4, [r1, #0xa]
00283df6: ldrb     r3, [r0, #7]
00283df8: cmp      r4, r3
00283dfa: bne      #0x283e18
00283dfc: ldrh     r3, [r0, #8]
00283dfe: adds     r3, r0, r3
00283e00: ldrb     r3, [r3, #9]
00283e02: lsls     r3, r3, #0x1d
00283e04: lsrs     r3, r3, #0x1d
00283e06: ldrb     r1, [r1, #0xb]
00283e08: lsls     r1, r1, #0x1d
00283e0a: lsrs     r1, r1, #0x1d
00283e0c: cmp      r1, r3
00283e0e: bne      #0x283e18
00283e10: ldrb     r0, [r0]
00283e12: cmp      r0, #1
00283e14: beq      #0x283e18
00283e16: movs     r2, #0
00283e18: adds     r0, r2, #0
00283e1a: pop      {r4, pc}
00283e1c: push     {r4, r5, lr}
00283e1e: adds     r4, r0, #0
00283e20: ldrh     r0, [r4, #4]
00283e22: adds     r0, #6
00283e24: lsls     r0, r0, #0x10
00283e26: lsrs     r0, r0, #0x10
00283e28: bl       #0x26afe0
00283e2c: adds     r5, r0, #0
00283e2e: cmp      r5, #0
00283e30: bne      #0x283e36
00283e32: movs     r4, #0
00283e34: b        #0x283e52
00283e36: ldrh     r2, [r4, #4]
00283e38: adds     r2, #6
00283e3a: adds     r0, r5, #0
00283e3c: adds     r1, r4, #0
00283e3e: bl       #0x2b5c7c
00283e42: movs     r4, #0
00283e44: movs     r0, #8
00283e46: adds     r1, r5, #0
00283e48: bl       #0x26a354
00283e4c: cmp      r0, #1
00283e4e: bne      #0x283e52
00283e50: movs     r4, #1
00283e52: adds     r0, r4, #0
00283e54: pop      {r4, r5, pc}
00283e56: mov      r8, r8
00283e58: ands     r1, r5
00283e5a: cbz      r3, #0x283ed8
00283e5c: strb     r0, [r2]
00283e5e: strb     r6, [r4, #0xc]
00283e60: ands     r7, r3
00283e62: movs     r0, #0
00283e64: movs     r0, r0
00283e66: movs     r0, r0
00283e68: movs     r1, r2
00283e6a: movs     r1, #0x3c
00283e6c: push     {r4, lr}
00283e6e: ldr      r0, [pc, #0x358]
00283e70: ldrb     r0, [r0]
00283e72: cmp      r0, #0
00283e74: bne      #0x283e7c
00283e76: bl       #0x2b05b2
00283e7a: b        #0x283e80
```

### contact_service_timeout_signal_236dc4

```asm
00236dc4: push     {r4, r5, r6, r7, lr}
00236dc6: sub      sp, #8
00236dc8: adds     r6, r0, #0
00236dca: ldr      r0, [pc, #0x31c]
00236dcc: adds     r1, r6, #0
00236dce: bl       #0x2b216e
00236dd2: mov      r0, sp
00236dd4: movs     r1, #0x64
00236dd6: movs     r2, #9
00236dd8: bl       #0x234634
00236ddc: adds     r4, r0, #0
00236dde: ldr      r5, [pc, #0x30c]
00236de0: ldrb     r0, [r5]
00236de2: lsrs     r0, r0, #7
00236de4: bhs      #0x236dee
00236de6: movs     r0, #0
00236de8: strb     r0, [r4, #0xa]
00236dea: movs     r7, #1
00236dec: b        #0x236df2
00236dee: movs     r7, #1
00236df0: strb     r7, [r4, #0xa]
00236df2: movs     r0, #0x45
00236df4: strb     r0, [r4, #0xb]
00236df6: movs     r0, #0xd
00236df8: strb     r0, [r4, #0xc]
00236dfa: strb     r7, [r4, #0xd]
00236dfc: strb     r7, [r4, #0xe]
00236dfe: strb     r7, [r4, #0xf]
00236e00: movs     r0, #0x1b
00236e02: strb     r0, [r4, #0x10]
00236e04: movs     r0, #0x58
00236e06: strb     r0, [r4, #0x11]
00236e08: cmp      r6, #1
00236e0a: bne      #0x236e1e
00236e0c: movs     r0, #0
00236e0e: bl       #0x2b12b4
00236e12: cmp      r0, #0
00236e14: bne      #0x236e1e
00236e16: movs     r0, #0x1c
00236e18: ldr      r1, [pc, #0x70]
00236e1a: bl       #0x2697aa
00236e1e: movs     r0, #1
00236e20: ldrb     r1, [r5]
00236e22: orrs     r0, r1
00236e24: strb     r0, [r5]
```

### contact_response_code_70_71_handler_23670c

```asm
0023670c: push     {r4, r5, lr}
0023670e: sub      sp, #4
00236710: adds     r5, r0, #0
00236712: ldrb     r1, [r5, #8]
00236714: ldr      r0, [pc, #0x24c]
00236716: bl       #0x2b216e
0023671a: ldrb     r0, [r5, #8]
0023671c: cmp      r0, #0x70
0023671e: beq      #0x23673c
00236720: ldr      r1, [pc, #0x23c]
00236722: movs     r0, #0
00236724: strb     r0, [r1]
00236726: movs     r1, #0
00236728: movs     r2, #0
0023672a: movs     r3, #0
0023672c: bl       #0x2b140a
00236730: mov      r0, sp
00236732: movs     r1, #0x71
00236734: movs     r2, #0
00236736: bl       #0x234634
0023673a: b        #0x236750
0023673c: mov      r0, sp
0023673e: movs     r1, #0x70
00236740: movs     r2, #1
00236742: bl       #0x234634
00236746: adds     r4, r0, #0
00236748: adds     r0, r5, #0
0023674a: bl       #0x2366c8
0023674e: strb     r0, [r4, #9]
00236750: ldr      r0, [sp]
00236752: bl       #0x234684
00236756: add      sp, #4
```

### contact_service_response_dispatch_237400

```asm
00237400: push     {r4, r5, r6, r7, lr}
00237402: mov      r1, r8
00237404: push     {r1}
00237406: sub      sp, #0x14
00237408: adds     r5, r0, #0
0023740a: ldrb     r0, [r5, #5]
0023740c: adds     r0, #3
0023740e: lsls     r0, r0, #0x10
00237410: lsrs     r0, r0, #0x10
00237412: adds     r2, r5, #2
00237414: ldr      r1, [pc, #0x394]
00237416: bl       #0x2b13a2
0023741a: ldrb     r4, [r5, #8]
0023741c: cmp      r4, #0x64
0023741e: bne      #0x237422
00237420: b        #0x23785e
00237422: movs     r0, #0x6c
00237424: mov      r8, r0
00237426: movs     r0, #0x69
00237428: ldr      r1, [pc, #0x394]
0023742a: ldrb     r0, [r1, r0]
0023742c: lsrs     r0, r0, #1
0023742e: bhs      #0x237432
00237430: b        #0x237894
00237432: movs     r6, #1
00237434: adds     r2, r4, #0
00237436: cmp      r2, #0x89
00237438: bgt      #0x2374d8
0023743a: bne      #0x23743e
0023743c: b        #0x2377a2
0023743e: cmp      r2, #0x66
00237440: bgt      #0x237496
00237442: bne      #0x237446
00237444: b        #0x23783a
00237446: cmp      r2, #0xb
00237448: bgt      #0x237472
0023744a: bne      #0x23744e
0023744c: b        #0x237850
0023744e: movs     r0, #4
00237450: subs     r0, r2, r0
00237452: cmp      r0, #0
00237454: bne      #0x237458
00237456: b        #0x23784a
00237458: subs     r0, #1
0023745a: cmp      r0, #1
0023745c: bhi      #0x237460
0023745e: b        #0x23785a
00237460: subs     r0, #2
00237462: cmp      r0, #1
00237464: bhi      #0x237468
00237466: b        #0x237850
00237468: subs     r0, #2
0023746a: cmp      r0, #0
0023746c: bne      #0x237470
0023746e: b        #0x23785a
00237470: b        #0x237894
00237472: movs     r0, #0xc
00237474: subs     r0, r2, r0
00237476: cmp      r0, #1
00237478: bhi      #0x23747c
0023747a: b        #0x23785a
0023747c: subs     r0, #2
0023747e: cmp      r0, #0
00237480: bne      #0x237484
00237482: b        #0x237850
00237484: subs     r0, #1
00237486: cmp      r0, #1
00237488: bhi      #0x23748c
0023748a: b        #0x23785a
0023748c: subs     r0, #0x56
0023748e: cmp      r0, #0
00237490: bne      #0x237494
00237492: b        #0x237840
00237494: b        #0x237894
00237496: movs     r0, #0x68
00237498: subs     r0, r2, r0
0023749a: cmp      r0, #0
0023749c: bne      #0x2374a0
0023749e: b        #0x237834
002374a0: subs     r0, #2
002374a2: cmp      r0, #1
002374a4: bhi      #0x2374a8
```
