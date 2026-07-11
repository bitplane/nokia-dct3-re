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

### Commit dispatcher key map (runtime-confirmed)

The commit is a family of separate `0x253e20` cases, not one straight-line handler. Each case
returns through the shared epilogue `0x255fc2`. The ordered keys are `0x1770` (setup), then
`0x1584..0x157d` descending. The latter map to producers `0x290314`, `0x290348`, `0x2902e4`,
`0x2900a0`, `0x28fea6` (**0x1199**), `0x290250`, `0x290280`, and `0x2902ac`
(**0x1196**). An opt-in dispatcher probe ran all nine cases while yielding to task 20 between
them; both genuine producer entries and both task-20 receives were observed.

### Task-5 routed commit and EF-list rerun (forced probes only)

`MODEL_SIM_REG_ROUTE` replaces one redundant in-flight task-5 `0x05e2` generated status with the
registration start, then keeps execution in task 5. The normal rewrite changes it to `0x1774`, so
the route shim allocates the 0x14-byte session through firmware allocator `0x26afe0`, populates the
mapped session globals `0x110f1c/0x110f20`, and substitutes the confirmed commit keys at successive
real `0x253e20` invocations. This force-calls the real `0x28fea6`/`0x2902ac` producer functions,
but it does not demonstrate a natural registration session reaching them. Both this route shim and
`MODEL_SIM_REG_COMMIT` replay the hard-coded key family. They are isolation probes, not registration
models, and neither settles the boot loop or composes with the EF-read run below.

The downstream EF profile was rerun separately with the normal SIM bootstrap and ring-2 responder.
Task 20 issued firmware SELECT and READ_BINARY commands; the multi-file responder tracked selected
file `EA00` and returned its configured 18-byte body plus `9000`. The two runs are separate because
executing the commit family before SIM reset changes task scheduling; integrating their ordering is
the next cleanup target, not part of the responder/EF-list proof.

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
arrive via RX-complete handler **`0x2a0454` → `0x26aac0(0x15) → RING#2`**. The handler classifies
ATR as code **5**, an ordinary buffered receive as **9**, a T=0 procedure byte as **0x0b**, and a
terminal status beginning `6x`/`9x` as **0x0a**.

**THE FIX:** deliver ATR/responses by **posting the response message to task 21's ring#2 via the
real poster `0x26aac0(0x15, msg)`** (`{[+2]=len, [+4]=rx classification, [+5..]=card bytes}`) and let the
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

## Step 7 complete — coherent 0x1196 / CHANGE CHV handshake

The isolated `NOKI3210_MODEL_SIM_1196_HANDSHAKE` probe starts only after task 20 has consumed task
21's asynchronous SIM-present code `2`. This ordering matters because `0x26a458` is unfiltered:
starting at detect-handler entry allowed that unrelated status to steal `0x293522`'s nested RPC wait
and produced a false-positive ENABLE write.

With the stale status removed first, the responder exposed the real failure: it returned procedure
byte and status together (`24 90 00`) as code 9. `A0 24` is an outgoing-data T=0 command and requires
three phases:

1. Terminal sends `A0 24 00 01 10`.
2. Card returns procedure byte `24`, classified by the real RX handler as mailbox code `0x0b`.
3. Terminal sends the 16-byte body; card returns `90 00`, classified as code `0x0a`.

The ring-2 responder now models those phases. Two repeat runs produced the same causal trace:

```text
task20 recv 1196 -> 0x207234 -> 0x293f30
sim_apdu: [ a0 24 00 01 10 ]
task21 recv code=0b
sim_apdu: T0_WRITE_DATA len=16
task21 recv code=0a; SW1=90
0x27e96a posts code=2 -> 0x2935c8 reposts the paired reply
0x293f30 returns 2
pc=0x20733c writes [0x111c79] = 1
```

This completes the step-7 transport/handler contract. It does **not** prove the upstream SIM-server
registration route: the probe still invokes the genuine `0x2902ac` producer with isolated scratch
inputs. Organic session population and producer scheduling remain the next layer.

## Organic task-5 route audit (2026-07-10)

The forced commit-key probes are not evidence of an organic registration session. A correlated
`NOKI3210_TRACE_TASK5_REG` run now records task-5 mailbox input, descriptor expansion, argument
copies, dispatcher cases, and producer milestones in one timeline.

With native task-21 activation (`NOKI3210_SIM_CARD_AUTOSTART=1`) and ring-2 T=0 delivery, firmware
naturally reaches repeated dispatcher case `0x1581`. It does not reach `0x1580..0x157d` or
`0x2902ac`. The route below the missing transition is now exact:

```text
task-5 state handler 0x289e7c receives event 0x0fa4
  -> 0x28a03a copies three bytes from the current argument pointer
  -> sole call at 0x28a050 to 0x27953e
  -> allocate/post message 0x157d to task 17
  -> task-5 commit case 0x157d
  -> 0x254c42 -> 0x2902ac -> task-20 message 0x1196
```

`0x0fa4` is a registered service callback, not a SIM-UART status. Initialization code at
`0x289db4` constructs callback descriptors containing `0x0fa4` and registers them through service
`0x0b` (`0x2632fc`). In the current run this handler receives only timeout `0x05e2`; the service
callback is never delivered. A trial interpretation of ROM-table pair `0x1395/0x1f05` was
falsified: raw `0x1f05` reached the dispatcher unchanged and did not initialize this state. The
speculative responder was removed.

Two corrections follow from this audit:

- `0x110f1c` is the transient task-5 argument vector populated by descriptor expansion at
  `0x2aee84`; treating it unconditionally as a persistent allocated SIM session was incorrect.
- `0x119a` always runs reinitialization call `0x207704`, even when SIM-selected is set. Deferring
  only the premature pre-detect instance avoids the initial inversion, but later `0x119a` messages
  remain part of the retry/polling behavior and reset the selected flag.

Next implementation boundary: trace the real service-`0x0b` request/response transport around
`0x2632fc` and reproduce that peer response, including its three-byte callback payload. Do not
post `0x157d`, invoke `0x27953e`, or replay commit keys; those bypass the state being tested.

### Service-0x0b prerequisite correction

A follow-up allocator-backed diagnostic reached handler event `0x1395` with a valid linked-list
argument. The handler still correctly declined service-`0x0b` initialization: helper `0x26f278`
first tests the SIM configuration word at `0x10d126` (bit selected by `lsrs #10`). Only when that
service bit is present does it derive the three-byte value from `0x10d149` and allow `0x289c5c ->
0x289db4` to register callback `0x0fa4`.

Therefore `0x0fa4` is not the immediate missing peer response after natural `0x1581`. It is
downstream of earlier SIM EF/config population. The next organic transition to solve is
`0x1581 -> 0x1580`; then continue descending through the commit cases until the SIM configuration
needed by `0x26f278` exists. The direct-handler/list diagnostic was removed after establishing
this dependency.

### First missing transition before 0x1580

Natural dispatcher case `0x1581` enters `0x2900a0`, but its gate `[0x111c86]` is zero, so the
conditional call to `0x2793b6` is skipped. A RAM-boundary write watch found no setter during the
run. Instead, task-21 completion `0x27dfc4` writes zero to the gate and calls `0x2793b6` itself.
That completion repeats approximately every 75 ms in the current boot, matching the outstanding
SIM reset/reinitialization cycle. At the first natural `0x1581` (about 6.76 s), `[0x111c86]=0`
while SIM-manager flags `[0x10dcaf]` and `[0x10dca9]` are both 1.

This moves the immediate implementation target upstream again: make the post-detect SIM lifecycle
settle so `0x27dfc4` is not repeatedly re-entered, then observe the legitimate indirect setter of
`0x111c86`. Do not force this byte; doing so would bypass the task-17/task-21 completion contract
that is supposed to schedule `0x1580`.

## Phase 4 scaffold: GSM service peer boundary

The generic service framework around `0x262xxx` is now sufficiently mapped to attach a peer
without invoking an application callback directly:

- `0x10b2fc`: six-entry service registry (8-byte entries).
- `0x10b32c`: callback-record pool (up to 80 records, stride `0x1c`).
- `0x2632fc(service, ordinal, descriptor)`: installs a callback record.
- `0x263154(service, enabled)`: enables or disables the service.
- `0x2629d0(token, mode)`: triggers/rescans framework-owned resident data; its second argument is
  not a frame pointer.
- `0x2624b8(service)`: resolves resident data against callback records and dispatches it through
  the normal task/MMI path.

`NOKI3210_MODEL_GSM_SERVICE` now owns explicit dormant/registered/enabled peer state and arms only
from firmware-originated service-`0x0b` registration. `NOKI3210_TRACE_GSM_SERVICE` reports service
declaration, callback descriptors, enablement, trigger calls, and callback dispatch. In the current
boot the model remains dormant, as required: the unsettled SIM lifecycle never reaches service
`0x0b` registration.

### Phase 4 dispatch-mode correction

An opt-in registration diagnostic established the complete callback record created by
`0x289c5c -> 0x289db4`: parsed display data at `+0x00`, the originating PLMN/list node at `+0x0c`,
event `0x0114` at `+0x10`, callback `0x0fa4` at `+0x12`, flags `0x00000049` at `+0x14`, and byte
`0x10` at `+0x18`. The installed pool record was index 15 with framework ordinal zero.

The ROM object at `0x2cfae0` is not the GSM response for this record. It is a built-in resident
descriptor containing callbacks `0x00dc`, `0x0fa3`, and `0x0fa2`. Passing it to `0x263d30`
correctly matches resident ordinal one, but cannot select `0x0fa4`.

More importantly, `0x263d30` explicitly marks the active framework slot as resident-table mode
(`slot + 9 = 0`). `0x26309c` then extracts callback metadata from the ROM resident table; trace at
`0x262dfe` showed callback/resource value `0x001a`, not `0x0fa4`. Direct-ordinal RAM objects and
standalone calls to `0x263154` or `0x2629d0` do not change that ownership contract. Registered
callback extraction is the separate branch at `0x262bc6`, taken only while a lower service-channel
transaction owns a slot with `slot + 9 = 1`.

`0x2638e4` is the task-5 control/status handler for those transactions, not a raw inbound frame
boundary. The next peer implementation step is therefore to map the lower receive path that
creates the registered-mode transaction and feed it a service-`0x0b` response. The peer must still
not call `0x289e7c`, inject `0x0fa4`, call `0x27953e`, or force the framework mode byte.

### Phase 4 registered transaction implemented

The registered-mode completion helper is `0x2635ac(result)`. It reads the selected callback-pool
record while the service transaction still owns the active slot, then asks the normal task-5
sequencer to post `0x4000 | callback` with the record's `+0x0c` source pointer. The peer now:

1. Arms only after firmware registers callback `0x0fa4` for service `0x0b` and enables that service.
2. Re-enters the firmware service transaction with `0x263154(0x0b, 1)`.
3. Before restoring the interrupted task context, completes that same registered transaction with
   `0x2635ac(0)`.
4. Lets firmware derive both callback `0x0fa4` and source pointer `0x101c28` from callback-pool
   record 15 and enqueue the resulting task-5 message.

The validation run produced `status=0x0fa4` at the natural task-5 dispatcher with argument zero
equal to `0x101c28`. No callback value, task status, application handler, or framework mode byte is
written by the model.

That run also corrected the claimed downstream chain. Task 5's static 233-entry descriptor lookup
at `0x2aed5c` returns `0xffff` for `0x0fa4`; the status does not enter `0x289e7c` in this diagnostic
boot. The registration diagnostic had created the service callback without the earlier organic
task/application state needed to consume it. Therefore callback emission is the Phase 4 peer
milestone, while reaching `0x289e7c -> 0x27953e` remains downstream of organic SIM/session
initialization and must not be forced by the peer.

### Organic service-0x0a and post-class89 correction

Firmware naturally declares service `0x0a` and installs callback record `0x00dc` at boot. A
registered-mode peer transaction completed through `0x263154 -> 0x2635ac` and produced task-5
status `0x00dc`; it did **not** produce `0x1580` or advance the SIM commit family. The temporary
service-`0x0a` responder was removed after this falsification. Service `0x0a` is therefore not the
missing `0x1581 -> 0x1580` scheduler.

The display peer's class-`0x89` path is also a separate boundary. With the capability isolation
seed, firmware accepts the class, constructs class `0x1a`, and consumes twelve successful
class-`0x80`/subtype-`0x60` indications while its progress counter descends `0x2d` to terminal.
Firmware then enters its normal class-`0x11` polling state. Supplying an additional class-`0x11`
notification was accepted but inert and was removed. Completing class `0x89` does not itself
schedule `0x1580`.

The SIM responder also corrected two concrete GSM filesystem contradictions found during this
audit: `EF_ECC (6FB7)` now has the advertised 15-byte body size, and DF STATUS/GET RESPONSE data
advertises child-file and CHV counts rather than an impossible empty `DF_GSM`. Neither correction
changes the missing task-5 transition, but both are required for a coherent integrated peer.

### Task-5 rule-table correction

The task-5 rewrite VM does not contain a `0x1581 -> 0x1580` rule. Decoding the
eight-byte action records at `0x2cc7f0` found exactly three records in this part
of the registration family:

```text
action 0x0594 @ 0x2cf490: predicate 61/06/82 -> packed 0x5581
action 0x059b @ 0x2cf4c8: predicate 61/3f/42 -> packed 0x5581
action 0x05a9 @ 0x2cf538: predicate 61/3f/43 -> packed 0x5581
```

No action record emits packed status `0x1580` or `0x5580`. The sorted status
table entry at `0x2cb908` only describes how an already-produced `0x1580`
message expands (descriptor offset `0x0398`, five arguments); it is not a
producer. The next stage is therefore asynchronous firmware state ownership,
not another task-5 rule predicate.

The relevant ownership contract is narrower as a result. `0x2900a0`, the
natural `0x1581` handler, calls `0x2793b6` only when `[0x111c86] == 1`. Reset
completion `0x27dfc4` also calls `0x2793b6`, but first clears `[0x10dcaf]` and
`[0x111c86]`. Static references to `0x111c86` are limited to the reset/lifecycle
cluster (`0x27e224`, `0x27e6fc`) and task-5/SIM code (`0x290484`, `0x2939c0`).
The immediate target is the successful lifecycle path that re-arms this byte.

A current-tree replay exposed a baseline regression that must be resolved
before interpreting that later predicate. After organic reads through
`EF_PHASE (6FAE)`, task 20 receives a non-RAM/invalid message while the
display-peer and SIM paths overlap, and execution stops advancing at about
1.24 seconds instead of reproducing the previously observed natural `0x1581`
at about 6.76 seconds. Suppressing receive-side T=0 procedure indications was
tested and falsified: it caused immediate STATUS retries and repeated reset
completion, so the experiment was reverted. Restore the natural `0x1581`
baseline before changing the `0x111c86` lifecycle contract.

### Organic `0x1580` predecessor decoded

The absence of a literal `0x1580` action output does not mean that the task-5
VM cannot produce it. Descriptor expansion is recursive. Incoming event
`0x157d` selects dispatch-table entry `0x2cb8f0`, whose six-action range is
`0x0390..0x0395`. Action `0x0394` is:

```text
action 0x0394 @ 0x2ce490: state slot 0x61 == 5 -> packed descriptor 0x21f8
```

Descriptor `0x01f8` starts at `0x2cc148` and expands the following sequence:

```text
0x157d, 0x057a, 0x00dc, 0x1580, 0x057a, 0x00dc, ...
```

This supplies the missing causal link. The organic incoming `0x157d` is the
message allocated by `0x27953e`, whose sole relevant caller is
`0x289e7c -> 0x28a03a -> 0x28a050` after service callback `0x0fa4`. Thus the
actual predecessor graph is:

```text
registered service-0x0b callback 0x0fa4
  -> task-5 handler 0x289e7c
  -> 0x27953e posts incoming 0x157d
  -> dispatch entry 0x2cb8f0
  -> action 0x0394, provided state[0x61] == 5
  -> descriptor 0x01f8
  -> queued 0x1580
  -> 0x28fea6 produces task-20 message 0x1199
```

Consequently `[0x111c86]` is not the `0x1580` producer. It gates an optional
side effect in the preceding `0x1581` handler and may still matter to coherent
SIM lifecycle state, but it is not the next transition to implement. The
implementation target returns to organic service-`0x0b` registration and
callback consumption, plus proving the final VM predicate `state[0x61] == 5`.

### Organic service-0x0b configuration source

The bit required by `0x26f278` has a genuine SIM-filesystem writer. The generic
EF parser `0x200364(fid)` dispatches `EF_IMSI (6F07)` to `0x20194c`. That parser
copies the IMSI body to the configuration block beginning at `0x10d148 + 0x24`
and, at `0x201962`, executes the equivalent of:

```text
byte[0x10d126] |= 0x02
```

This is exactly the service-availability bit tested by `0x26f278`. The same
parser supplies the bytes at `0x10d149..0x10d14b` from which that consumer
derives its three-byte callback payload. Therefore the capability and payload
isolation seed stands in for one missing organic `EF_IMSI` parse, not for a
display-peer property.

`EF 6F98` is a separate capability source. Its decoder at `0x200a10` ends at
`0x200d08` by ORing `0x10` into `[0x10d126]`; that is not the bit tested by
service `0x0b`.

The natural preliminary read pass currently selects ICCID, ECC, and PHASE and
then stops after `EF_PHASE (6FAE)`. It never requests `EF_IMSI`; the extended
firmware EF parser is already present and does not require a synthetic config
write. The next implementation target is the organic lifecycle transition that
starts the extended EF-read pass at `0x2038ec`. Its acceptance chain is:

```text
firmware requests and reads EF_IMSI 6F07
  -> 0x200364 / 0x20194c parse the returned file
  -> 0x201956 copies the IMSI body to 0x10d148 + 0x24
  -> 0x201962 sets 0x10d126 bit 0x02
  -> 0x26f278 succeeds
  -> firmware registers/enables service 0x0b callback 0x0fa4
  -> registered peer completion delivers 0x0fa4
  -> incoming 0x157d, action 0x0394, descriptor 0x01f8
  -> organic 0x1580 and task-20 0x1199
```

The natural-`0x1581` baseline itself is still intact with the SIM peer alone
(observed at 1.3096 s and 1.6687 s). The earlier apparent baseline regression
is specifically a non-composition failure when the display peer runs
concurrently, and should be handled during integrated-peer validation rather
than used to reinterpret the SIM-only predecessor graph.

### Pre-IMSI registration owner

Runtime ordering shows that the IMSI/service-`0x0b` path is downstream of the
initial SIM-server commit. Deferring the first pre-detect `0x119a` prevents read
state `6`, but the later `0x119a` still finishes the preliminary ICCID/ECC/PHASE
pass with result zero and moves read state to `7`. The full `EF_IMSI` pass still
requires the earlier `0x1196` commit result. Class `0x89` cannot bootstrap it:
task 15 calls `0x26f278` at `0x20e942` while building that transaction, so its
session is intentionally empty until IMSI configuration already exists.

The pre-IMSI registration state machine is owned by task 14. Its receive loop
at `0x247c0c` passes the `0x09df..0x09eb` and `0x1771..0x177e` families to
`0x2a2e44`. The state machine owns its request objects at `0x11233c`, builds
service `0x2a` / resource `0x08` / subtype `0x02` / operation `0xe0` requests,
and organically emits the descending `0x177x` sequence. In coherent SIM-only,
class-`0x87`, and class-`0x89` runs task 14 is scheduled and blocked in recv,
but receives none of these messages. Multiple natural `0x1581` events likewise
do not wake it.

This locates the remaining external model boundary below task 14: the lower GSM
service peer must cause task 15 to deliver the initial `0x09e5`-family result.
Once that first result is delivered through the normal task-15 parser/poster,
task 14, task 5, and the SIM-server dispatcher must own session allocation and
the `0x1770..0x177b` progression. Posting `0x1770`, replaying commit keys, or
calling `0x2902ac` remains an invalid shortcut.

An early DSP transmit packet is visible at about 0.253 s as
`05 1e ff 00 d0 00 03 01 01 e0`. DSP framing includes its first logical byte
in the length/class word; it is not a class-`0x70` packet. Delayed test replies
do traverse DSP RX and the generic router, but the apparent `0x0a35`
correlation was falsified: initialization routine `0x223418` unconditionally
calls cleanup `0x221d52` at `0x223462`, which clears the pending list through
`0x221d3c` and posts `0x0a35`. The same event exists independently of the
injected packet. Therefore the 0.253 s transmit packet is not established as
the missing registration request, and no responder should key from it. The
remaining search starts after channel-manager initialization and must locate
the real producer of task-15 `0x07dd`/parser `0x209978`.

That producer chain is now mapped one layer further. Handler `0x28d29c`
receives generic event `0x05ea`; at `0x28d364`, a non-class-6 attached object
is forwarded by `0x28d194` as task-15 event `0x07dd`. No `0x05ea` post or
consumer entry occurs in an eight-second organic run. Service-5 callback
`0x2618e8` is responsible for returning `0x05ea` for ordinary inbound objects
(`0x261b9e`) after its `0x05dc` branch declares, registers, and enables generic
service `0x05` (`0x261ba8` through `0x261bc0`). In the current boot the callback
is invoked from framework `0x2ac65e` with `0x05f3` and then `0x05e2` at about
0.7995 s while its state byte `[0x11fcc3]` is still zero. It never receives the
required cold-start `0x05dc`, and no service-5 registration reaches `0x2632fc`.
Thus the remaining predecessor is the framework callback sweep/order that
skips `0x05dc`, not a guessed DSP response packet.

### Task-15 completion correction

The service-5 cold-start observation above is real, but it is not yet evidence
that service 5 is the immediate predecessor of task 14. A complete task-5
engine trace shows that callback index `0x2f` is the active application
callback: it receives `0x05dc` and remains selected for the subsequent status
stream. Index `0x28` (`0x2618e8`) is visited transiently for `0x05f3` and
`0x05e2`; there is no global constructor sweep advancing through it.

More importantly, task 15 already invokes its ordinary result formatter with
`0x09fe` organically at about 0.866 and 0.902 seconds. In `0x208ee0`, the
`0x09fe` case reads the lower-result discriminator at `[0x10fe73]`:

```text
0x1f -> 0x09d8
0x20 -> 0x09e5
0x21 -> 0x09c5
```

The current run leaves that byte zero, so the organic completion produces no
task-14 result. `0x209978`, entered from task-15 event `0x07dd`, is the parser
that consumes the missing lower object. Its minimum object contract is a
signed length of at least two, class nibble 5 at `object+4`, and a primitive at
`object+5`; primitive `0x20` is the required registration result family.

An isolation frame `{DSP class 5, primitive 0x20}` was tested through the real
DSP RX ring both before and after the display/resource handshake. Firmware
accepted the physical ring frame but task 4 discarded it as the wrong lower
protocol; it never reached `0x05ea`, `0x07dd`, or `0x209978`. The probe was
removed. This confirms that the display DSP ring is not a substitute for the
generic service-owned lower object. The next boundary remains the organic
service transaction that creates that object; direct writes to `0x10fe73` or
posting `0x09e5` would bypass the parser contract.

### Service-30 readiness predecessor

The common handler `0x28d29c` is a mailbox drain, not a provider-result
callback. It calls receive primitive `0x26a458` at `0x28d4d2` and dispatches
the returned message by its status word. In the canonical boot its live stream
contains `0x083b`, `0x07d6`, `0x07d5`, `0x0835`, and repeated `0x05e8`
messages. None owns an object, and no `0x05ea` is received.

The object path belongs to two static service-30 callbacks. Client handler
`0x26e4d4` and completion handler `0x26e764` both receive framework startup
`0x05e2` organically at about 0.833 s. Their descriptor and session globals
remain zero. Both return `0x05e2`; neither becomes the selected callback and
the client does not register the five service-30 records at startup. That
registration occurs only when the client later receives readiness status
`0x0578`: branch
`0x26e65c` calls `0x26e466`, which installs the five records through
`0x2632fc`. A related status `0x057c` is generated by `0x29ea68` earlier in
boot, but `0x0578` is never generated or delivered.

Once active, the client handles an object-bearing request at its `0x05dc`
branch (`0x26e620`): it copies the request descriptor from the framework
argument slot, calls allocator `0x26e400`, and populates the session under
`0x1113a4`. Completion handler `0x26e764` converts lower completions `0x0578`
or `0x0580` to `0x05ea` and releases the outstanding object at `0x1113b0`.
This is the organic predecessor chain to investigate. The immediate missing
event is service-30 readiness `0x0578`, before session population or an
object-bearing `0x05ea` can occur.

An earlier interpretation of `0x26b58c` as a subscription call was rejected
after full disassembly. For startup input `0x05e2`, it falls through its
generic status mapper, returns `3`, and does not initiate a lower transaction.

Resource decoding sharpens this further: `0x0578` is resource event
`0xaf << 3`, `0x0580` is `0xb0 << 3`, and observed `0x057c` is resource-AF
subtype `+4`. The only live `0x057c` producer is the normal cold-start reset
at `0x29ec60`, reached when network callback `0x29ea80` receives `0x05e2`.
It closes resources `0x5e/0x5d`, posts AF-unavailable, clears state, and does
not by itself prove a failed hardware condition.

The resource-AF handlers are callback indices `0x36` (`0x2689d8`) and `0x3a`
(`0x2680f8`). The service-30 client/completion pair are indices `0x7a` and
`0x7b`. All four receive only `0x05e2` in canonical and 20-second runs and
return it unchanged. The task-5 engine routes the later event batch to index
`0x2f` (`0x27cb28`); this is an RF/measurement application callback, not a
phone-profile selector. Repeated framework rescans select the same index, and
no AF base event appears.

Four firmware sites can organically post AF base: `0x268240`, `0x268284`,
`0x268a58`, and `0x268aec`. Each is inside the two AF handlers and is gated by
an already-populated descriptor/state transition. None executes in the current
boot. Consequently, directly posting `0x0578` would still bypass the unknown
resource-provider contract and is not an acceptable implementation. The next
static/runtime boundary is the input event or provider operation that populates
those AF handler descriptors.

A removed isolation probe additionally proved that an unscoped `0x0578` post
after bootstrap does not select service 30: it was delivered to the currently
selected callback, index `0x2f`, while index `0x7a` saw nothing. A callback-tail
trace over indices `0x70..0x7b` shows that the entire group receives and returns
`0x05e2` during the startup scan with an empty task-5 queue. Status `0x012f`
then selects index `0x2f`; the ROM contains a structured `0x017a` entry in the
same selector family, but no live producer has yet been observed. This narrows
the missing contract to the provider-owned selection/readiness sequence that
makes index `0x7a` current before delivering `0x0578`, rather than the AF base
event alone.

Further callback-boundary isolation separated readiness from requests. Giving
`0x0578` to index `0x7a` during its startup visit executes `0x26e466`, installs
the five service-30 records, and returns `0x00dc`; it does not create an object
request or a class-5 completion. The request input is instead `0x05dc` at
`0x26e620`, where the callback consumes the framework descriptor and calls
`0x26e400`. Thus service-30 needs two distinct organic inputs: readiness first,
then a descriptor-bearing request.

The immediately preceding callback, index `0x79` (`0x26dc1c`), is an NV-backed
request owner. Its own `0x0578` branch calls `0x26dbd6`, which allocates a
0x190-byte record buffer and reads one of four variants through `0x2abbae`.
That helper resolves the NV descriptor and calls the already-modelled serial
EEPROM path `0x2af8e2`; the latter bit-bangs address and payload bytes through
MAD2 PUP McuGenIO register `0x020020`. Delivering
readiness only to index `0x79` submits this request and returns `0x05e6`, but
does not deliver readiness or `0x05dc` to index `0x7a`; the framework revisits
`0x79`. Both isolation probes were removed.

Runtime resolution pins NV descriptor `0x0757`, variant 0 to EEPROM offset
`0x0db0`, length `0x190`. The erased EEPROM returned all `0xff`; firmware's own
fallback constructor `0x26db44` defines a canonical variant-0 record as the
9-byte header at `0x2d7fdc` plus the 0x187-byte body at `0x2d7c08`. The
`selftest` EEPROM profile now exposes that ROM-version-specific default through
the existing I2C model. A clean boot still does not request the record or emit
`0x0578`, proving that record is downstream data rather than the readiness
trigger. The remaining predecessor is the framework request that selects and
constructs provider callback `0x36` or `0x3a`; adding another EEPROM peer or
injecting a dispatcher status would target the wrong layer.

### Research checkpoint before cleanup (2026-07-11)

The final selector hypothesis above needs one correction. The records around
`0x2db660` containing `0x0136`, `0x0139`, and related values are not generic
service-registration descriptors. They are part of a display/MMI descriptor
chain rooted at `0x2db6f0`; `0x28bd4c` passes that root to `0x24b174`. The
nearby callback-function table begins separately at `0x2db720`. Although task
5 does use the low byte of a queued status to select a callback, the static
MMI records are not evidence that the service framework will organically
select provider callback `0x36` or `0x3a`.

A canonical four-second trace confirms the distinction. The startup scan
visits callbacks `0x36`, `0x3a`, `0x7a`, and `0x7b` with input `0x05e2`; each
returns without constructing a provider transaction. After the scan,
`0x012f` selects callback `0x2f`, which receives the later `0x05dc` and status
stream. No organic `0x0136`, `0x013a`, provider descriptor, service-30
readiness `0x0578`, or lower object `0x05ea` appears. The missing predecessor
therefore remains an external resource/service transaction, but its exact
transport boundary is not yet identified.

The two commit-forcing experiments remain negative controls, not candidate
models:

- `MODEL_SIM_REG_ROUTE` force-selects/replays the commit family, emits both
  producers, then creates a scheduler resume storm. It never reaches the
  ENABLE setter `0x20733c`.
- `MODEL_SIM_REG_COMMIT` also force-calls the producer family. It emits
  `0x1196` and `0x1199` but leaves the registration loop cycling and likewise
  never reaches `0x20733c`.

These results prove that delivery of `0x1196` is necessary but insufficient.
Handler `0x207234` sets `[0x111c79]` only on its reply-code-2 branch, which
requires the real card exchange and parser state. Replaying the producer,
posting a commit key, or setting the enable byte cannot substitute for the
coherent request/reply transaction.

#### State of the four organic-peer milestones

At this checkpoint none of the following should be reported as complete:

1. A lower service transaction has not organically created the required
   class-5, primitive-`0x20` object.
2. The full `0x05ea -> 0x07dd -> 0x209978 -> 0x09e5` chain is statically
   mapped, but has not executed end to end in one organic run.
3. Task 14 has not reached `0x2902ac` from that result, and task 20 has not
   established IMSI/ENABLE state through the real `0x1196` reply path.
4. The SIM, GSM/resource, and display peers have not composed into one stable
   boot satisfying all preceding milestones.

What is worth banking is the boundary map: ring-2 SIM delivery, the multi-file
APDU responder, corrected GSM FCP/STATUS data, the service-30 callback and
session contract, the task-15 parser contract, the commit producer family,
the reply-code-2 requirement, corrected commit tap `0x254bb4`, and the
ROM-derived EEPROM default for NV record `0x0757`.

#### Cleanup classification

The driver at this checkpoint is a research harness. The following families
are isolation/forcing probes and should be removed in the cleanup pass rather
than promoted to hardware models:

```text
MODEL_SIM_INIT_KICK
SIM_REG_BOOTSTRAP
SIM_REG_DEFER119A
SIM_REG_REARM
MODEL_SIM_1196_HANDSHAKE
MODEL_SIM_REG_COMMIT
MODEL_SIM_REG_ROUTE
SIM_CARD_CLEAR_NOSIM
MODEL_DISPLAY_PEER_PROBE / PROBE_SIM_CONFIG_READY
```

In particular, direct writes to the registration/configuration state at
`0x111c64`, `0x111c69`, `0x111c6f`, `0x111c76`, `0x111c79`,
`0x111c96`, `0x111c97`, or `0x10d126` are assertions about firmware state,
not emulated hardware behavior.

The SIM UART/ATR path, ring-2 message delivery, APDU filesystem responder,
EEPROM I2C behavior, DSP service, and CCONT behavior are the useful model
substrate. Service, startup, GSM, and display peer responders may encode valid
external protocol behavior, but their current PC-hook/message-mutation
mechanisms still need replacement by explicit device or transport boundaries
before an upstream-oriented driver is cut.
