# SIM Registration State Machine — full static map

> Synthesis of a 5-pass static disassembly sweep (2026-07-10) of the Nokia 3210
> (DCT3/MAD2) firmware's SIM registration machinery. Goal: understand the machine
> well enough to model it faithfully, so the firmware drives its **own** SIM file
> reads (instead of the `MODEL_SIM_CARD` injection short-cut) and can advance past
> "Insert SIM card". All addresses are static; RAM addresses are swap16-corrected
> (see the swap16 note below). Confidence tags: **C**=confirmed by disasm,
> **L**=likely, **G**=guess, **RV**=runtime-verify.

## ⚠️ swap16 correction (supersedes older notes)

Pool literals are stored halfword-swapped: `REAL = ((w & 0xffff) << 16) | (w >> 16)`.
Empirically anchored this session: literal `dddc0010` → `0x10dddc` (a trace read
`debug_ram_byte(0x0010dddc)` and got correct SIM response lengths). By the identical
transform:

- **`dca80010` → `0x10dca8`** — the SIM manager (task 21) control struct.
  The long-standing note **`0x10a8dc` is WRONG** (a double-swap error; `0x10a8dc`
  would need literal `a8dc0010`). Cross-check: struct `[+7]` = `0x10dca8+7 = 0x10dcaf`,
  which is exactly the teardown gate an independent pass found at `[0x10dcaf]`.
- **`d21a0010` → `0x10d21a`** — task 21's scratch/phase mirror (older note `0x10a1d2`
  is the same double-swap error).
- `0x10dca8` sits in the same RAM neighborhood as its buffers `0x10dddc / 0x10deec /
  0x10dcc8`; `0x10a8dc` was an outlier — another tell it was wrong.

Everywhere below, the SIM manager struct is **`0x10dca8`**.

## The subsystem: five cooperating tasks

SIM bring-up is spread across five RTOS actors. The registration "state machine" is
not one function — it is this message choreography.

| Task | id | Body / entry | Role |
|------|----|--------------|------|
| **Task 1** (startup) | 0x01 | `0x270170` (disp `0x270c8e`, jump tbl `0x270ca8`) | Global boot state machine. Emits event/msg **`0x1587`** at `0x270faa` on its **own** schedule — no SIM gate. C |
| **Task 17** (SIM bring-up) | 0x11 | `0x22391c` | Early SIM-hardware/power module (code `0x224xxx`–`0x225xxx`). Consumes `0x1587`; fires the **premature** `0x119a`/`0x119b` static-template posts to task 20. C |
| **Task 20** (read sequencer) | 0x14 | `0x208134`, ctrl block `0x111c64` (r8=`0x111c87`) | Walks the EF read-list `0x2e0c04`, issuing SELECT/READ APDUs to task 21. Gated by the read-enable gate `0x208218`. C |
| **Task 21** (SIM driver/manager) | 0x15 | `0x27eae0`, ctrl struct `0x10dca8` | Low-level SIM: reset `0x27e024`, ATR parse `0x27e046`, APDU exchange, status reports `0x27e240`. C |
| **SIM-server** (registration) | ? (RV) | router `0x253d30` (from disp `0x245d32`, tbl `0x245cf0`), state array `0x111304` (8×0x14), session `0x110f1c` | The registration message router. Its **commit case `0x254b40`** (msg code **`0x177b`**, L) emits the **legit** read-enable messages `0x1199`/`0x1196` to task 20, then frees the session. Task identity not yet pinned (likely task 17 or an adjacent SIM task). |

## Control structures

### SIM manager struct `0x10dca8` (task 21) — offset → meaning (C unless noted)
`+0` busy/reset-in-progress · `+1` activation-pending (L) · `+2` last SW1 · `+3` last SW2 ·
`+4` APDU phase (3 idle / 1 mid-cmd) · `+5` last recv-code scratch · `+6` session active/detected ·
`+7` ATR/reset-pending (=`0x10dcaf`, teardown gate) · `+8` response-ready/activation-done ·
`+9` reset/ATR-in-progress · `+0xa` current clock/power mode (cmp vs `+0xe`) · `+0xb` per-reset retry
counter (vs 2/3) · `+0xc` protocol/voltage class (T=0/T=1) · `+0xd` ATR-received · `+0xe` target
clock/power mode · `+0x10` current APDU length · `+0x12` ATR timing word (→`0x2a0060` timer) ·
`+0x18` ptr to alloc'd response block · `+0x1c…+0x1e` APDU command-header buffer.
Buffers: `0x10dddc` raw I/O **rx** (`[+0]`=len hw, `[+2..]`=card bytes), `0x10deec` cmd/TPDU
template, `0x10dcc8` config/status block, `0x10d21a` scratch/phase mirror.

### Task-20 ctrl block `0x111c64` — key offsets (C)
`+1`=`0x111c65` task-21-wake guard · `+5`=`0x111c69` **SIM-SELECTED flag** (gate!) ·
`+0xb`=`0x111c6f` "SIM data valid" (set with enable) · `+0xf`=`0x111c73` re-init busy ·
`+0x12`=`0x111c76` **gate cond-1** (msg-pending/busy latch) · `+0x15`=`0x111c79` **gate cond-3 = ENABLE** ·
`+0x2d`=`0x111c91` re-init-in-progress. Gate conds 4/5 are `[r8+0x10]`=`0x111c97` and `[r8+0xf]`=`0x111c96`
(r8=`0x111c87`) — **not** `0x111c73` as an earlier note had it.

### Registration session `0x110f1c` (SIM-server) — partial (RV on populator)
Global holding a pointer to the SIM-session block: `word0`=`[P]` data/handle ptr (also a TLV
**parse cursor**, advanced at `0x28c758`), `word1`=`[P+4]`, `word2`=`[P+8]` (buffer ptrs, freed at
teardown); bytes `[+3]/[+7]/[+0xb]` = parsed SIM params (L). **No clean allocator was isolated** —
the only word0 stores are a parse-cursor advance (`0x28c768`) and two writes in the display/MMI
region (`0x2ac550`, `0x2aee84`) that treat it as a small int (shared handle, or overlapping
subsystem — open item). **This populator is the main gap.**

### Config/SST block `0x10d124`
`[+4]` = SST service-availability word; `0x207188` decodes it to gate optional-EF pointers
(bit17→group A, bits15:14==01→B, bit14→C, `[+3]>>5`→D). Completion cmd `0x64` sets `[+4] |= 0x08`
and `[0x10d37e]=2` (C). Minimal SST (bits 3/14/15/17 and `[+3]`bit5 = 0) demands no optional EFs.

## The message flow: legit vs premature (the crux)

The four producers all post to **task 20** (id 0x14) via `0x26a204`, but they are **genuinely
different code** (C):

| Producer | Msg | Built how | Callers | Nature |
|----------|-----|-----------|---------|--------|
| `0x28fea6` | **0x1199** | dynamic; carries session `word0/word1` | **only `0x254c1c`** (commit case) | **LEGIT** read-enable |
| `0x2902ac` | **0x1196** | dynamic; carries session data | **only `0x254c42`** (commit case) | **LEGIT** read-enable |
| `0x2904d4` | **0x119a** | static ROM template `0x2e2cd8` | `0x224e42/0x225198/0x2251aa/0x2253ec/0x225fce` (task 17) | **PREMATURE** control |
| `0x290618` | **0x119b** | static ROM template `0x2e2ce0` | `0x225368/0x225fc2` (task 17) | **PREMATURE** control |

`0x2e2cd8` also holds the sibling early-control family: `0x119a,0x120d,0x119b,0x119c,0x11bb,0x11be,0x11f9,0x11fa`.

## The core bug: one ordering inversion

**The whole deadlock is a single premature message.** (C, corroborated by two passes.)

1. Task 1 emits `0x1587` at `0x270faa` on its own startup schedule (`t≈0.854`) with **no
   SIM-readiness gate** — it just sets its local progress byte `[0x112364]=1` and fires. Premature
   by construction.
2. Task 17 consumes `0x1587` (`0x2253d0`) and posts **`0x119a`** to task 20 (`0x2253ec`→`0x2904d4`,
   `t≈0.858`) — **before** the SIM ATR (`t≈0.862`).
3. Task 20's `0x119a` handler checks **SIM-SELECTED `[0x111c69]==1`** at `0x2078f0`. The only setter
   is `0x27dfc4` (runs at read-complete, `t≈0.9`), so it is still **0**. → divert `0x207920`
   (status `0x274` "not ready") → re-init `0x207704` → issues a SELECT and **blocks for a reply from
   task 21, which is mid-reset** → **deadlock**. Task 20 never recv's again; its read-enable gate
   never opens; zero firmware-driven reads.

The reset itself is *chained* through this same path today (`0x207704`→`0x2077be`→`0x293a6a` wakes
task 21), which made it look load-bearing — but see next section.

## Can the reset be decoupled from `0x119a`? — YES, with care

- **Task 21 owns the reset machinery** (`0x27e024`, 3 callers `0x27f00e/f058/f098`, all internal). C
- **But it has no autonomous trigger.** Init (`0x2a05cc`/`0x2a0094`) arms nothing (no self-post, no
  pre-reset timer); the `0xe9` timer is armed *by* the reset. After init task 21 **blocks in recv**
  (`0x27defc`→`0x26a458`) until an inbound message returns "code 1" → `0x27f098` → reset. C
- Today that first message is task 20's `0x293a6a` wake (the `0x119a` chain). So the reset currently
  depends on the premature path — but only for the **trigger**, not the mechanism.

**Fix shape (L):** supply task 21 its **own first activation message** (a "code 1" / ATR-window
event), independent of task 20, so the reset+ATR+detect happens on its own; **then** gate task 20's
`0x119a` read-continuation (the `0x2078f0` branch, or the task-17 `0x1587→0x119a` post) until
`[0x111c69]==1`. That preserves the reset while removing the too-early SELECT.

## The read-enable gate `0x208218` and how it legitimately opens

Gate needs all five: `[0x111c76]==0`, `0x26a698()==0` (mailbox drained), `[0x111c79]==1` (ENABLE),
`[0x111c97]==0`, `[0x111c96]==0`. Pass → `0x208254` (post `0xe8`) → read phases
`0x201a3a/0x201c1c/0x201fc8` walk the EF table.

**Legit ENABLE sequence** (C chain):
1. Task 20 selects MF→DF, sending APDUs to task 21 (builders `0x293722`/`0x293f30` → poster
   `0x293522`, msg code `0x1e` to task 21).
2. Task 21 replies; task-20 dispatcher routes `0x1196`→`0x207234` and `0x1199`→`0x206dcc`.
3. In `0x207234`, when reply-parser `0x293f30` returns 2 → `0x20733c` sets **`[0x111c79]=1`**
   (+`[0x111c6f]=1`). (Alt: SIM-ready status epilogue `0x207ec0`.)
4. Gate opens → task 20 walks EF table `0x2e0c04`.

**The `0x1196`/`0x1199` that legitimately drive step 2 come only from the SIM-server commit case
`0x254b40`** (below) — which is why the gate never opens on our boot.

### EF read-list `0x2e0c04` (26 × {u16 id, u16 len}, C)
`2FE2/10(ICCID) 6FAD/03(AD) 6F07/09(IMSI) 6F74/10 6F78/02(ACC) 6F7E/0B(LOCI) 6F20/09(Kc)
6F7B/0C(FPLMN) 6FAE/01(PHASE) 6F31/01(HPPLMN) 6F37/03 6F41/05 6F43/02 6F46/11(SPN) 6F13/01
6F98/16 6F9B/25 6F91/01 6F93/01 6F95/1D 6F96/1D 6F9F/01 6F92/01 EA00/12 EA03/0B 2FE6/04`.
APDUs are CLA `0xA0`; SELECT primitives `0x200282/0x200364`, READ `0x2019cc`.

## The keystone: SIM-server commit case `0x254b40`

`0x254b40` is **not a standalone sequencer** (zero callers/ptrs). It is the fall-through tail of
router case **`0x177b`** (idx 11 of jump table `0x25495c`) inside the SIM-server router `0x253d30`.
When `0x177b` arrives **with `[0x110f1c]` populated**, the case runs a ~40-call straight-line batch
that (a) emits the legit `0x1199` (`0x254c1c`) and `0x1196` (`0x254c42`) to task 20, plus dozens of
apply-calls into display/CCONT/DSP, then (b) **frees the session** (`0x254d0c`+). It is the
**terminal "commit SIM session"** handler. C (fall-through); L (code=`0x177b`).

**Why dormant on our boot** (C mechanism):
1. **Code `0x177b` never arrives** — the SIM-server only advances through the *early* states (task
   17 fires the premature `0x119a/0x119b`); it never emits the internal "session complete / commit"
   event. Candidate self-emitter: `0x253b9d` (RV).
2. **`[0x110f1c]` is never populated** — no allocator/populator runs, so even an injected `0x177b`
   would fault on the deref/free. **This is the biggest open gap.**

## Registration completion / end-state (the target)

Read-complete decision `0x27ea88` (caller SIM loop `0x27f06e`): `[0x111c64]!=0` → status `0x1f`
"Insert SIM card" (via `0x27e240`); `==0` → SIM-present handler `0x27dfc4` (sets `[0x111c69]=1`,
gated `[0x111c9d]==0` and struct `[+7]==1`) + arms timers `0xE8`(immediate)/`0xEA`(delayed) — these
are **internal SIM-loop pacing timers, not the MMI hand-off**. No unconditional "SIM-ready→MMI"
message is emitted at the SIM layer.

**"SIM registered" acceptance checklist (C unless noted):** `[0x111c64]==0` held; `[0x111c9d]==0`;
SIM struct `0x10dca8[+7]==1` then cleared, state `[+0xa]` settles (not retry-looping); valid ATR →
status `0x15` then `0x16`; mandatory EF reads succeed (CLA `0xA0`, SW `9000`); completion cmd `0x64`
sets SST `[0x10d124+4] |= 0x08`; minimal SST → no optional-EF demand; no VERIFY-CHV issued, FCP
access class `[0x10dca8+0xc]` = CHV1-not-required (L); timers `0xE8/0xE9/0xEA` cycling normally.

## The intended flow (reconstructed)

1. Task 17 brings up SIM hardware (CCONT power, reset pulse).
2. Task 21 gets its first activation message → reset `0x27e024` → ATR → parse → SIM detected.
3. `0x27dfc4` sets `[0x111c69]=1` (SIM-selected).
4. Task 20's control messages (currently premature `0x119a`) now find `[0x111c69]==1` → proper
   handler → MF/DF select + initial reads → **populate session `[0x110f1c]`**.
5. SIM-server posts `0x177b` → commit case `0x254b40` → emits `0x1199`/`0x1196` → task 20's
   read-enable `[0x111c79]=1` → gate `0x208218` opens.
6. Task 20 walks the full EF table `0x2e0c04`; registration data complete.

The single inversion at step 3↔4 (task 20 sees `[0x111c69]==0`) is what breaks the chain.

## Model build plan (proposed)

Incremental, each step observable via `TRACE_SIMKICK`:

1. **Independent task-21 activation.** Post task 21 its first activation ("code 1") message from a
   faithful sequencer point, so reset/ATR/detect happen without task 20's `0x293a6a` wake.
   Observable: `0x27e024` fires, status `0x15`→`0x16`, `[0x111c69]=1` — with the `0x119a` withheld.
2. **Gate the premature read-continuation.** Delay task 17's `0x1587→0x119a` post (or task 20's
   `0x2078f0` branch) until `[0x111c69]==1`. Observable: task 20 no longer deadlocks in `0x207704`;
   it stays at a wakeable recv, then takes the proper `0x119a` handler `0x20797c`.
3. **Verify task-20-driven reads.** With ordering fixed, confirm task 20 issues uninjected
   SELECT/GET_RESPONSE/READ (via `0x293522` to task 21) — this is where the **faithful multi-file
   responder (Phase B)** plugs in to answer them from `0x2e0c04`-sized synthetic EFs.
4. **Session populate + commit.** Determine whether steps 1–3 naturally populate `[0x110f1c]` and
   trigger `0x177b`→`0x254b40`. If not, isolate the populator (disassemble `0x257c1c`, `0x2525ce`,
   the "SIM files read OK" branch) and model it. Observable: `0x1199`/`0x1196` emitted from
   `0x254c1c`/`0x254c42`, read-enable gate opens.
5. **Retire the injection.** Remove `SIM_CARD_EF` injection + `SIM_CARD_CLEAR_NOSIM` poke; the
   firmware now drives its own reads and clears no-SIM itself. Verify the end-state checklist.

## Open items (RUNTIME-VERIFY)

- The `[0x110f1c]` **allocator/populator** (the main gap) — watch writes to `0x110f1c..0x110f2c`
  during registration; suspects `0x257c1c`, `0x2525ce`, the display-region writers `0x2ac550`/`0x2aee84`.
- The exact emitter of commit code **`0x177b`** (`0x253b9d` is the lead).
- Which of task 21's native reset callers (`0x27f00e/f058/f098`) fires on our boot; the exact
  `[msg+4]` code `0x26a204` delivers for task 20's `0x293a6a` send.
- The SIM-server **task number** (walk up from `0x245d32` to the task table).
- Gate conds `[0x111c96]/[0x111c97]` writers (abort latches) — confirm they stay 0.

## Strategic caveat (unresolved)

One pass argues the SIM-layer end-state is *already reached under injection*, and the real wall to a
visible screen is the **display resource-content pipeline** (the ~18-class idle provider graph +
cmd-`0x70` resource-enable), which prior work deemed a hard coherent-bringup wall. If so, completing
firmware-driven registration yields "SIM registered internally" but may **not** by itself advance
past "Insert SIM card". Against this: GPT's milestone-2 argument (offline idle+menu is not
display-content-gated the same way) and the observation that the MMI VM is alive. **Whether a real
firmware-driven registration reaches an interactive menu, or stalls at the same display wall, is the
key open question** — best answered empirically once step 3 above produces real task-20 reads.

## Scheduling deep-dive (5-pass) — the drive path corrected

A second 5-pass sweep (2026-07-10) traced why the `SIM_REG_BOOTSTRAP` attempt (drive task 20
via `0x207704`) stalls, and **redirected the whole approach**:

- **The `0x293a6a`/code-2/`0x207704` path is a re-init detect-confirm, NOT the read driver.**
  `0x293a6a` is one of a 25-caller task-20→task-21 **RPC family** (`0x293946/0x293a96/0x293fa0/…`):
  post a SIM primitive to task 21, block on `0x26a458` for the paired reply. `0x207704` needs
  reply `2`/`0x67`, then does only config reads (`0x20764c`→`0x200282`) and returns — it emits **no
  `0x119x`**. So even a correct reply leaves task 20 with an empty read queue and gate `0x208218`
  shut. (Confirmed.)
- **The code-2 I saw from `0x27e394` is a red herring** — the SIM-present *status enum* (async
  detect broadcast to task 20 via `0x26a95c`), numerically colliding with the reply value 2, not
  task 21's paired RPC response. recv `0x26a458` is **unfiltered** (3-source priority scan: TCB+8
  linked-list → ring#2 → ring#1), so routing isn't the issue; the non-wake was a timing/state
  artifact — but moot, since this path doesn't drive reads.
- **RTOS mechanics mapped:** recv `0x26a458` on current task `[0x100022]`; TCB `0x101484+task*0x1c`
  (ring#1 `+0xc/+0x10/+0x11`, ring#2 `+0x14/+0x18/+0x19`, reply-list `+8`); MB `0x1093bc+task*0x10`
  (`+0xd` state: 1=run/2=ready/4=blocked/5=ready; `+0xf` recv-arm). Posters wake **inline**
  (`state←2` + `0x2699be` into ready-list `0x10004c` + reschedule `0x269acc`) iff target is state 4
  at post time — **never** via `0x269bf4` (so a `0x269bf4` probe can't observe the wake). `0x26a204`
  posts ring#1, `0x26a95c` posts ring#2.
- **The real read engine (theory b, CONFIRMED):** task-20 codes `0x1195..0x11b4` (jump table
  `0x2084dc`) write per-EF read descriptors (`[ctrl+0x34..0x3c]`) that gate `0x208218` services;
  those codes are emitted **only** by the SIM-server commit `0x254b40` (`0x1199` producer `0x28fea6`
  single-caller `0x254c1c`; siblings `0x11a7`=`0x290348`, `0x11a8`=`0x290314`).
- **The missing trigger:** SIM-ready must propagate from task 21's detect **up** to the service
  layer `0x245a84` (62 callers) → router `0x253d30` (populates session `0x110f1c` / table
  `0x110413` / mask `0x1112f4`) → SIM-server task `0x2af630`/dispatcher `0x253e20` → commit
  `0x254b40` (code `0x177b`, L). Our faked boot only did the low-level task-21↔task-20 code-2
  handshake; it never broadcast SIM-ready to the app/SIM-server layer, so `0x253d30` never populates
  the session and the commit never runs.

### Corrected build direction
Abandon driving task 20 through `0x207704`. **Keep** `SIM_REG_BOOTSTRAP` step A (independent
task-21 activation — still valid, decouples the reset). Retarget the driver at the **SIM-server
commit chain**:
1. Propagate task 21's detect (status 0x16) as a SIM-ready event into the service layer `0x245a84`
   / router `0x253d30` so it populates session `0x110f1c`.
2. Trigger the SIM-server commit `0x254b40` (dispatcher `0x253e20`, code `0x177b`) so it emits the
   `0x1199`/`0x11a7`/`0x11a8` read-enable family to task 20.
3. Task-20 `0x2084dc` handlers write read descriptors → gate `0x208218` opens → task 20 issues
   SELECT/READ → **the file-driven responder answers them.**

Cheap decisive test before modelling the full chain: post `0x1199` (+`0x11a7`/`0x11a8`) to task 20
directly and confirm the gate opens and reads flow into the responder. If yes, the task-20 read
engine + responder are proven, and only the SIM-server-commit trigger (+`0x110f1c` populate)
remains to model faithfully.

New symbols: service dispatcher `0x245a84`, SIM-server task `0x2af630`, SIM-server dispatcher
`0x253e20`, task-20 family jump table `0x2084dc`, read-descriptor gate `0x208218`, RPC family head
`0x293a6a` (+ `0x293946/0x293a96/0x293fa0`), reply status enum path `0x27e394→0x27e960→0x26a95c`.

## BREAKTHROUGH — firmware-driven APDUs, and the ring#2-delivery fix (2nd 5-pass)

A third 5-pass sweep cracked the "task 21 doesn't serve task-20 commands" wall and produced the
**first firmware-driven SIM APDUs**.

### The wall: ring#1 mask (CONFIRMED, proven at runtime)
- recv `0x26a458` (current task `[0x100022]`) scans, by priority: **reply-list `TCB[+8]` → ring#2
  `TCB[+0x14/0x18/0x19]` → ring#1 `TCB[+0xc/0x10/0x11]`** (TCB=`0x101484+task*0x1c`, also aliased
  `0x108414` in some literals). Ring#1 is **gated by `MB[+0xf]` bit0** (MB=`0x1093bc+task*0x10`;
  for task 21 the byte is **`0x10951b`**): **bit0 SET ⇒ ring#1 MASKED** (recv skips it, blocks);
  CLEAR ⇒ served. Polarity CONFIRMED (`0x26a496`: `lsrs #1; bhs block`).
- Only setter: **`0x26abba`** (mask, part of the "reply-wait" idiom `0x26abba`→recv→`0x26abf8`).
  Only clearer: the **real recv auto-clears bit0** when it delivers a reply-list/ring#2 message
  (`0x26a674`/`0x26a620`). No standalone re-arm.
- Task 21's post-detect command loop is `0x27efb0 → 0x27defc (recv) → 0x27ede0 (dispatch)` — coded
  correctly (Pass 1). But it blocks in the recv **because bit0 is left SET** after a masked
  reply-wait recv, so ring#1 (where task 20's relayed commands land) is skipped forever.
- Task-20 commands are posted to task 21's **ring#1** via `0x26a204(0x15)` (`0x293522` relay);
  they are code **3** (read requests). Code-3 only *stashes* the request into `0x10deec`; the APDU
  is emitted only when the pumped 3 is consumed at the `0x27efb0` site → `0x27ede0` → `0x27e98c`.

### Proof (SIM_REG_REARM): first firmware-driven APDUs
Clearing bit0 at each task-21 recv unblocks ring#1 → task 21 receives code-3, runs `0x27ede0`, and
emits **uninjected firmware-driven APDUs**: `a0 2c`, `a0 f2` STATUS, `a0 a4 00 00 02` SELECT.
(Band-aid on the wrong layer: it got one SELECT then desynced — the real fix is below.)

### The root cause + production fix (Pass 4, CONFIRMED)
`MODEL_SIM_CARD`'s recv-intercept is **architecturally wrong**: it hooks recv `0x26a458`
(LR=`0x27df10`) and returns a synthetic message **without running the real recv**, so the rings are
never drained and the wake/arm state (`MB[+0xd]` wait, `MB[+0xf]` arm) desyncs — task-20 ring#1
commands strand. Also **`0x2aec34` is only a trace trampoline** (to `0x2b13a2`); the real APDU-out
is `0x2a02e6`/byte-engine `0x2a0268` on SIM-UART MMIO **`0x020000`**, and genuine card responses
arrive via RX-complete handler **`0x2a0454` → `0x26aac0(0x15) → RING#2`** as code **5** (ATR) /
code **9** (data).

**THE FIX:** deliver ATR/responses by **posting the response message to task 21's ring#2 via the
real poster `0x26aac0(0x15, msg)`** (`{[+2]=len, [+4]=5|9, [+5..]=card bytes}`) and let the
firmware's own recv run **unmodified**. Then ring#2 (responses) and ring#1 (task-20 commands) drain
in priority order coherently, the arm-bit is maintained by the real recv, and no re-arm hack is
needed. The most faithful variant drives the `0x020000` SIM-UART MMIO so `0x2a0454` emits the
`0x26aac0` post itself; the minimal correct substitution is to post at `0x26aac0` directly.

### Acceptance spec (Pass 5) — task 21 serving one task-20 read
| recv code (via `0x27defc`) | task-21 action | APDU (`0x27e98c`) | response (rx `0x10dddc`) | reply to task 20 |
|---|---|---|---|---|
| **3** (payload→`0x10deec`) | `0x27edfc`: parse `[+6]`=INS `[+9]`=len | 5-byte hdr from `0x10deec+5` | ack code 7 | — |
| **0xb** | `0x27ee94`: procedure byte, remaining len | GET_RESPONSE/continuation | data+SW1SW2 at `rx[len]` | — (loops) |
| **0xa/0xf/0x11** | `0x27ef0a`: terminal report | — | final SW → `S[2]/S[3]` | `0x27e240`→`0x26a95c(0x14)` code **0x64/0x65/0x67/0x6a** (INS-keyed) + data+SW |
| **0xd** (from task 20) | `0x27ee72`: `0x27dfc4`+`0x290208` | — | — | SIM-ready (`0x2ed42c`); clears no-SIM `[0x111c64]` |

Reply target = task 20 via `0x26a95c` (ring#2); transport id `0x127`; SW at `rx[len]/rx[len+1]`.
Async statuses (0x15 ATR / 0x16 detect / 0x1e err / 0x1f no-SIM) use the same path with no payload.
