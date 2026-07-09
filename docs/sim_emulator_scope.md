# SIM emulator — build scope

Scope for a faithful SIM emulator that completes the phone's SIM conversation
and reaches the **operator-idle** home screen. Everything below is grounded in
the reverse-engineering recorded in `docs/sim_subsystem.md` and the c0–c2
investigation. All addresses are for the pinned `3210f600a` image; the system is
**big-endian ARM7**.

Status going in: the phone **boots to "Insert SIM card"** and, with an injected
ATR, **accepts the SIM** and enters the read flow. The read flow is fully mapped
but **dormant** — reaching idle needs the SIM *conversation* to complete, not a
poke. This doc scopes that build.

## Why an emulator, not injection

The c0–c2 experiments ruled out every shortcut, and that negative result is the
justification for the emulator:

- **The reject is "command response ≠ 7"** (`0x27ee40`/`0x27ee4c` → post `0x15`),
  not a single flag. Answering 7 clears the reject but only parks on the next recv.
- **The read is multiple nested phases**, gated by read-state `[+0xc]`
  (`0x27ebde`): `3/2` = SELECT phase (`0x27ebfe`), `1` = file-read (`0x27ed44`→
  `0x27ee40`), `0` = other. The phases run **in order** and feed each other.
- **Forcing a phase doesn't work.** A crafted ATR (`SIM_ATR_HEX=3b1005`) sets
  `[+0xc]=1` directly and reaches the file-read loop — but it sends a **garbage
  command (`ff 00 ff`)** because the file-read builds its APDU from `[sp+0x34]`,
  a file descriptor **produced by the SELECT phase** that the shortcut skipped.
- So idle requires the SELECT phase to genuinely **complete** (valid file info +
  status words) so it hands the file-read its inputs — i.e. a real conversation.

Conclusion: the emulator must answer the actual T=0 / GSM 11.11 command sequence
with phase-correct content. It stays **opt-in** (like every model), so the
default boot still reproduces the oracle (`d8a9a7a58e587be8`).

## Architecture: a message-layer virtual SIM

The phone does **not** talk to the SIM via the raw UART for data (the MCU never
drains `SIM_RxD`; reception is DSP/message-assisted). It sends APDUs and receives
responses over the **service-lower message transport**. So the emulator is a
virtual SIM card at the *message* layer, not a UART device:

```
phone  --APDU (msg 0x2701)-->  [emulator]  --response (SIM-task msg)-->  phone
        0x2aec34/0x2b13a2                    0x27df0c dispatch → buffers
```

### Integration points (all mapped)

| Hook | Address | Role |
|------|---------|------|
| APDU out | `0x2aec34` (r1=`0x2701`, r2=APDU ptr, r0=len) | observe the command the phone sends |
| Command response | `0x27e98c` recv `0x26a458` @`0x27e9ca` (ret `0x27e9ce`) | must return **7** to continue |
| SIM-task message in | `0x27defc` recv `0x26a458` @`0x27df0c` (ret `0x27df10`) | inject responses here |
| Msg dispatch | `0x27defc`: `[msg+4]` → `3`→`0x27df9e`(→buf `0x10deec`), `5/8-b`→`0x27df64`(→buf `0x10dddc`), `0xc`→present, `0x11`→.. ; returns `[msg+4]` | |
| Response buffer | `0x10dddc` (`[+0]`=len, `[+2..]`=data), parsed by `0x27e046` | where file/ATR bytes land |
| Read-state | `[+0xc]` = `0x10dcb4`; accept-state `[+0xa]` = `0x10dcb2` | phase / accept tracking |
| Status → MMI | `0x27e240` (r1=status: `0x15`=reject, `0x16`=detected) | outcome |

The injector plumbing already exists as opt-in knobs: `MODEL_SIM_ATR_MSG` (ATR
into `0x10dddc`), `MODEL_SIM_RESPONDER` (respond-7 recv-trampoline at `0x27e9ce`),
`MODEL_SIM_LOOP` (code-`0xb` message feed at `0x27df10`, armed in the file-read
phase). These are the emulator's I/O layer; what's missing is the **card logic**.

## The conversation to implement

Standard GSM 11.11 / TS 51.011 SIM init (public spec; synthetic IMSI keeps it
IP-clean). The phone drives it; the emulator answers:

1. **ATR** — already injected (`3b …`). TA1 non-special (e.g. `3b1005`) so the
   parser routes into the read flow.
2. **SELECT MF `3F00`** (`a0 a4 00 00 02`, file id in data phase) → respond with
   the MF file-info block + `SW=9F XX`; phone issues **GET RESPONSE** (`a0 c0 00
   00 XX`) → respond with the info block + `SW=9000`. This is the SELECT phase
   (`[+0xc]=2/3`) that must complete to populate `[sp+0x34]`.
3. **SELECT DF_GSM `7F20`** → same SELECT/GET-RESPONSE dance.
4. **Read EFs** (file-read phase, `[+0xc]=1`): at minimum
   - `EF_ICCID 2FE2`, `EF_IMSI 6F07` (synthetic), `EF_AD 6FAD`,
   - `EF_SST 6F38` (service table — mark the minimum services),
   - `EF_LOCI 6F7E`, `EF_Kc 6F20`, `EF_PHASE 6FAE`,
   - `EF_SPN 6F46` (operator name), `EF_LP 6F05`.
   each via SELECT → GET RESPONSE (READ RECORD/BINARY) → data + `SW=9000`.
5. **PIN state** — present the SIM as CHV1-disabled (no PIN prompt).
6. **Completion** — when the phone's read state finishes, `[+0xc]→0`, the read
   completes, `[+0xa]` advances past the reject value, and `0x27e240` posts a
   non-reject status → the idle flag `[0x1116fd]` can set → `display_idle`
   (`0x2a255c`) fires → **operator-idle** (empty operator / no service, since RF
   registration is out of scope).

## Components to build

1. **APDU decoder** — parse CLA/INS/P1/P2/P3(+data) from the `0x2701` message.
   (Skeleton exists: `MODEL_SIM_RESPONDER` logs `SELECT`, etc.)
2. **GSM file system** — a small static table: file id → type (MF/DF/EF) →
   file-info block (GSM 11.11 §9.2.1 layout) → contents. Synthetic IMSI/ICCID.
3. **T=0 responder** — per command, emit procedure byte / status word / GET-
   RESPONSE payload. SELECT→`9F XX`, GET RESPONSE→info+`9000`, READ→data+`9000`,
   errors→`6A82`/`6982` as needed.
4. **Message injector** — deliver each response as the correct SIM-task message
   (code `5`/`8-b` → `0x27df64` → `0x10dddc`) with the right length/layout, and
   keep the command recv returning `7`.
5. **Conversation state machine** — track selected MF/DF/EF and the SELECT→READ
   ordering so the emulator answers phase-correctly and `[sp+0x34]` gets a valid
   descriptor.

## Risks & unknowns (ranked)

1. **[HIGH] SELECT-phase completion.** The critical unknown: can the SELECT phase
   be made to complete (populate `[sp+0x34]`, advance `[+0xc] 2/3→1`) purely via
   message injection? Every c2 attempt stalled or rejected here. **Spike this
   first** — it gates the whole build.
2. **[MED] Exact per-phase response layout.** Which message code + buffer +
   byte-layout the state machine expects at each step is only partially mapped;
   needs empirical iteration (the `SIM_LOOP_CODE`/`SIM_LOOP_HEX` knobs make this
   testable).
3. **[MED] Stack-variable dependence.** `[sp+0x34]`/`[sp+0x30]` are locals of the
   SIM manager; the emulator can't set them directly — they must be produced by
   the firmware's own SELECT handling, which is exactly why (1) matters.
4. **[LOW] Faithfulness.** The emulator bypasses the DSP service layer that would
   really carry SIM traffic. It's a behavioural model (like `MODEL_SVC_RESPONDER`),
   opt-in, oracle-preserving — acceptable, but document it as such.
5. **[LOW] Idle-flag gating.** `[0x1116fd]` (idle) may have gates beyond the SIM
   (network/RF); confirm `display_idle` fires on SIM-complete alone (the
   Phase-2/`000d` work suggests the idle screen appears pre-registration).

## Phasing & effort

- **Spike (S–M): SELECT-phase completion.** Trace the firmware's real SELECT
  handling (`0x27ebfe` command + its response processing), determine what a valid
  SELECT/GET-RESPONSE answer must contain to advance `[+0xc] 2/3→1` and set
  `[sp+0x34]`. **Go/no-go for the rest.**
- **Phase A (M): file system + APDU/T=0 responder** — the card logic, tested
  against the SELECT spike.
- **Phase B (M–L): walk the EF reads** to completion (`[+0xc]→0`).
- **Phase C (S): completion → accept → idle** — confirm the status post flips to
  non-reject and `display_idle` fires; capture the idle frame.

Overall: **L–XL**, dominated by the spike's outcome. The I/O plumbing, ATR
trigger, and full protocol map are done; the remaining work is the card logic
plus resolving risk (1).

## Spike result (done): conditional GO

The SELECT-phase spike ran. Key correction from it: the file-read block
`0x27ee40` is reached when a recv **returns code 3** (`0x27ede0` dispatch,
`r0==3`) — code 3 is the code-3 handler `0x27df9e`, which copies the message to
buffer `0x10deec`. So the read is driven by **code-3 messages** (not code-`0xb`),
and the file data lives in `0x10deec` (matching the c1 finding). Also `[sp+0x34]`
is a *fixed* init pointer, not a SELECT-produced descriptor.

Feeding code-3 messages (`SIM_LOOP_CODE=3`) **drives the read loop**: the phone
goes from **1 APDU to 13** — it actively sends command after command. It still
rejects (`0x15`) because the command it builds is garbage: the file-read APDU is
assembled from the code-3 data at `0x10deec+5` (`[sp+0x10]`, length 5), so my
placeholder bytes (`3b00`/zeros) produce `3b 00 00 00 00` and get rejected.

**Verdict: the read phase is NOT an unbreakable wall — it drives via code-3
injection.** So the emulator is viable. The remaining unknown drops from "can the
phase run?" (answered: yes) to "what exact byte layout must each code-3 message
carry so the built command is valid and the read advances to completion (code 1)
→ accept → idle?" That is the file-descriptor/response format at `0x10deec+5`,
resolvable by iterating `SIM_LOOP_HEX` against the command the phone emits.

### Format pinned (mechanism): a command-script model

Iterating the payload pinned the mechanism: the file-read block sends a **5-byte
APDU copied verbatim from the code-3 payload at `0x10deec+5`** (msg+5). Verified —
`SIM_LOOP_HEX=a0c0000016` → the phone emits `a0 c0 00 00 16` (GET RESPONSE),
`a0a4000002` → SELECT, `a0b0000009` → READ BINARY. So the phone's SIM command
*content* is supplied by the code-3 messages: **the emulator drives the command
sequence, not merely the responses** (an unusual inversion — most likely a
DSP-layer abstraction or T=0 continuation, worth noting in the design).

Two blockers remain for *completion* (valid commands alone still reject):

1. **Recv-site gating.** The file-read *loop* recv `0x27ee52` receives my code 3,
   which it doesn't handle (`0x27ef02`→`0x27ef0a`→post `0x15`). It needs code
   **`0xb`** (→`0x27ee94` process → `0x27eef6` send next). The *trigger* recvs
   (`0x27ec10`/`0x27efb0`) want code **3**. The `0x27defc` caller is on the stack
   at `[sp+0x18]` @`0x27df0c` — the feeder must choose the code by caller.
2. **Advancing sequence.** The payload is fixed, so the phone re-sends the same
   command 12× and gives up. Completion needs the payload to change per step
   (SELECT → GET RESPONSE → READ → next file).

## Build attempt result: driver works, injection has a ceiling

The conversation driver was built (`MODEL_SIM_LOOP`: caller-gated code override at
`0x27ee56`, scripted commands into `0x10deec`, code-1 completion). It **drives the
full SIM read to completion** — reject is gone (`0x15`→`0x0b`), the phone walks a
varying command sequence and finishes. But it lands on "Insert SIM card" (`0x1f`),
and building past that hit a wall:

- The read-complete handler `0x27ea88` posts the no-SIM status `0x1f` iff flag
  `[0x111c64]` != 0, else takes the success path (events `0xe8`/`0xea`).
- `[0x111c64]` is a widely-referenced global; **forcing it to 0 crashes the boot**
  (the success path needs coherent state the injection never built).
- The accept-bit check `0x27e556` (bit 4 of `0x10dddc+0x10`) is **not on the
  executed path** even with the read completing.

So the "valid SIM" outcome depends on a *web* of interdependent conditions — file
content in `0x10dddc`, global flags, struct fields — that must all be consistent.
Piecemeal injection satisfies one and breaks another. **The conversation driver is
the ceiling of the injection approach.**

## Recommendation

**Injection reaches "read driven to completion"; operator-idle needs coherent SIM
state that only a faithful emulator produces.** The realistic path is the fallback
below, OR a from-scratch faithful build: model the SIM at the message layer as a
real card that emits *consistent* file responses into `0x10dddc` (info blocks +
data + SWs) driven by the phone's own commands, so the global flags/struct fields
settle naturally — not by overriding codes. That is a substantial project; the
mapping here (buffers, dispatch, decision points) is the foundation for it.

Superseded plan (kept for reference): the emulator
must, per SIM-message recv, choose the code by caller (`3` at trigger recvs,
`0xb` in the file-read loop) and advance a code-3 command/response script through
the GSM read to the code-`1` completion → accept → idle. The format mechanism is
pinned; what remains is the state machine (the file system + sequencer) plus the
caller-gated feeder. Next concrete step: implement the caller-gated feed
(`[sp+0x18]`) with a short scripted sequence and watch for the reject clearing /
frame change. Fallback unchanged: if the conversation can't be completed by
injection (needs real DSP-path state), ship **SIM-present** as the milestone.

## Quick reference (for the emulation research pass)

Everything the emulator build needs, in one place. All addresses `3210f600a`, big-endian ARM7.

### Buffers & struct

| What | Address | Notes |
|------|---------|-------|
| SIM manager struct | `0x10dca8` | read-state `[+0xc]` (`3/2`=SELECT, `1`=file-read, `0`=other); accept-state `[+0xa]`; accept-flag `[+0xd]`; target `[+0xe]`; counter `[+0xb]` |
| **Command buffer** | `0x10deec` | = the file descriptor (`[sp+0x34]`/`[sp+0x30]`). Phone reads `+2` (halfword len, cmp 5), `+6`/`+8` (cmd INS/param). 5-byte APDU at `+5`. Written by code-3 msgs (`0x27df9e`). |
| **Response / file-content buffer** | `0x10dddc` | Parsed by `0x27e046` at `+06..+16`. Written by code-5/`8-b` msgs (`0x27df64` copies msg data → `+2`, len → `[+0]`). ATR injected here. |
| No-SIM flag | `0x111c64` | `!=0` → status `0x1f` (Insert SIM card); `==0` → success path. Load-bearing global (~50 refs) — do NOT force. |
| Idle flag | `0x1116fd` | gates `display_idle 0x2a255c` |

### Message path (message-layer SIM)

| Step | Address | Notes |
|------|---------|-------|
| APDU out | `0x2aec34` (r1=`0x2701`, r2=ptr, r0=len) → `0x2b13a2` | observe the command |
| Command send+recv | `0x27e98c` | recv at `0x26a458` @`0x27e9ca` (ret `0x27e9ce`), must return **7** |
| SIM-task recv | `0x27defc`: recv `0x26a458` @`0x27df0c` (ret `0x27df10`); `[msg+4]` → `3`→`0x27df9e`(→`0x10deec`), `5/8-b`→`0x27df64`(→`0x10dddc`), `0xc`→present, `0x11`→..; returns `[msg+4]` |
| Read dispatch (post-response) | `0x27ede0`: `r0==3`→file-read `0x27ee40`, `1`→completion `0x27ef34`, `0x37`→`0x27ef40` | `r0` = returned msg code |
| File-read loop | `0x27ee40` send (5-byte, `0x10deec+5`) → 7 → recv `0x27ee52` (ret `0x27ee56`) → dispatch `0xa/0xb/0xf/1` |
| Status → MMI | `0x27e240` (r1=status: `0x15`=reject, `0x16`=detected, `0x1f`=no-SIM) |
| No-SIM decision | `0x27ea88` (checks `[0x111c64]`) |

### Driver knobs (research infrastructure, all opt-in)

- Reach the read (empirically confirmed): 4 milestone models + `MODEL_SIM_ATR_MSG` + `SIM_ATR_HEX=3b1005` + **`EXPERIMENT_SIM_CODE5` + `SIM_CODE5_AFTER=2` + `SIM_CODE5_CONT=1`** + `MODEL_SIM_RESPONDER`(`SIM_RESP_CODE=7`) + `MODEL_SIM_LOOP`. `EXPERIMENT_SIM_CODE5` is the **ATR-parse trigger** — the SIM-mgr recv only ever returns code 6 (timeout) on a peerless boot, so forcing code 5 (data-received) at `0x27f0a6` is what routes to the code-5 handler `0x27ebbc` → parser `0x27e046`, which injects the ATR (via `MODEL_SIM_ATR_MSG`) and sets read-state `[+0xc]=1`. With this the phone emits a real GSM `SELECT` (`a0 a4 00 00 02`). (`EXPERIMENT_SIM_ACCEPT`/`TRACE_SIMACCEPT` were correctly pruned; `EXPERIMENT_SIM_CODE5` was mistakenly pruned then restored — it's research scaffolding, not a dead end.)
- Conversation driver: `MODEL_SIM_RESPONDER`(`SIM_RESP_CODE=7`) + `MODEL_SIM_LOOP`(`SIM_LOOP_CODE=3`, `SIM_LOOP_SCRIPT=<cmds>`, `SIM_LOOP_DONE=<n>`). Caller-gate override lives at `0x27ee56`.
- Traces: `TRACE_SIMBUF` (`0x10deec`+`0x10dddc` reads), `TRACE_SIMSTATUS` (`0x27e240` posts), `TRACE_SIMPATH` (state-machine branch targets, cxx markers), `TRACE_SIM`/`TRACE_SIMTX` (UART/APDU).

### The open problem for the pass

The conversation driver completes the read but lands on no-SIM because `0x10dddc` never gets coherent per-command file responses. A faithful emulator must, driven by the phone's own commands, emit **consistent** GSM 11.11 responses into `0x10dddc` (info blocks + data + SWs) so the read state, `[0x111c64]`, and the struct fields all settle to "valid SIM" together — not by overriding dispatch codes. Start by tracing, on a *real*-style boot path, what a valid response into `0x10dddc` must contain for the parser `0x27e046` to advance the state without the no-SIM flag being set.

## Build log — the T=0 responder (passes, 2026-07-07)

Grinding the response-driven responder that walks the real GSM 11.11 T=0 conversation.
Reference config (reaches the read): the 4 milestone models + `MODEL_SIM_ATR_MSG` +
`SIM_ATR_HEX=3b1005` + `MODEL_SIM_RESPONDER`(`SIM_RESP_CODE=7`) + `MODEL_SIM_LOOP` +
`EXPERIMENT_SIM_CODE5` (the ATR-parse trigger).

- **Increment 1 (a877924).** Mapped the full T=0 file-read machine: send `0x27e98c`
  → APDU out `0x2aec34` (msg `0x2701`); recv `0x27defc` (single recv site, ret `0x27df10`);
  data path `0x27ee94`; **completion = recv code `1` → `0x27ef34`**. Confirmed a real
  GSM `SELECT` (`a0 a4 00 00 02`). Forcing code-5 repeatedly is the wrong architecture —
  the code-5 handler `0x27ebbc` re-enters the ATR parser every cycle, and `0x27defc`
  clears `0x10dddc[+0]`/`sb` on every recv.
- **Pass 1 (e5132cb).** One recv site in the SIM region — the phone SPINS, not blocks.
  Decoded the `[ff 00 ff]` command: a valid ISO-7816 **PPS request** the firmware issues
  after the ATR; the SIM must echo it.
- **Pass 2 (22bcd14).** Response data path corrected: the code-5/8-b handler `0x27df64`
  copies the injected data (msg+5) to `0x10dcce` **and to `0x10dddc+2`**, then re-forwards
  via `0x2aec34` r1=`0x2700`.
- **Pass 3 (eec2481).** Runtime trace (`TRACE_SIMPPS`) of the PPS memcmp `0x2b58e8`:
  the check is `memcmp(0x10dddc+2, sent_PPS, 3)` — the PPS response must be the verbatim
  echo `[ff 00 ff]` at `0x10dddc+2`. Firing code-5 **once** (so the ATR isn't re-injected
  over those bytes) + `SIM_LOOP_ECHO` makes the PPS compare **pass** (result 0).
- **Pass 4.** The PPS handler wants match **AND recv code `9`** (`0x27defc` returns
  `[msg+4]`, so `SIM_LOOP_CODE=9`) → posts status `0x16` (SIM detected). Result: the
  genuine ATR+PPS handshake **clears the "Insert SIM card" screen** (frame `o074`→`o016`,
  blank text area = SIM present, awaiting data). Same end-state as the old forced
  `EXPERIMENT_SIM_PRESENT`, but reached faithfully via a real ATR parse + PPS echo.

**Where it stands:** the faithful ATR+PPS handshake is satisfied and "Insert SIM card"
clears. Next: sustain the manager past PPS→detected so it issues the EF SELECT/READ
sequence, and answer those with TS 51.011 file contents to reach operator-idle.

- **Pass 5.** Traced what happens after the faithful PPS→detected: the SIM manager
  **freezes** — struct `@0x10dca8[+9,+7,+a]` stays `00 01 01`, it keeps receiving the
  fed code-9 messages but issues **no further APDUs** (no EF SELECT/READ), status `0x16`
  posted exactly once. So the manager does **not** proactively read EFs after "detected";
  the EF-read phase is driven by a **higher-level SIM data service** (`0x29ff2c`, entered
  from the contact-service dispatcher `0x2378e0`) that is dormant on our stubbed boot —
  the same subsystem the earlier forced-`EXPERIMENT_SIM_PRESENT` probe found never fires.

**Net of the 5-pass grind:** the T=0 ATR+PPS handshake is now **faithfully satisfied**
(real ATR parse → real PPS echo at `0x10dddc+2` → status `0x16`), clearing "Insert SIM
card" the genuine way. The next wall is **not** more of the T=0 responder — it's the
higher-level SIM-read requester (`0x29ff2c` / the contact-service SIM path) that issues
the EF SELECT/READ sequence. Reaching operator-idle now hinges on triggering *that*
layer (analogous to how `MODEL_SVC_RESPONDER`/`MODEL_SVC_CHANNEL_DRAIN` drove the
service session), then answering the EF reads with TS 51.011 content.

## Step 1 — the trigger gap closed: `MODEL_SIM_CARD` (2026-07-07)

The 5-pass handshake was faithful in *content* but forced in *kickoff* (`EXPERIMENT_SIM_CODE5`
faked `r0=5`, `MODEL_SIM_ATR_MSG` poked the ATR into `0x10dddc` separately). `MODEL_SIM_CARD`
replaces both with a genuine message flow:

- **ATR** (phase 0): on the `SIM_CARD_ATR_AFTER`'th SIM-task recv, deliver a real **code-5**
  message carrying the ATR bytes. The firmware's own dispatch `0x27df64` copies them into
  `0x10dddc` (`[+0]`=len, `[+2..]`=bytes) and returns 5 → manager `0x27eb7c` → code-5 handler
  `0x27ebbc` → parser `0x27e046`. One real message, no forcing; delivered once (phase machine).
- **Command ACK**: return a `[msg+4]=7` message at `0x27e9ce`.
- **Per-command response** (phase 1, command-driven via a pending flag set at the APDU point
  `0x2aec34`): PPS requests (`FF …`) echoed verbatim; else `SW=9000`. One response per command.

**Result:** `MODEL_SIM_CARD + SIM_ATR_HEX=3b1005` alone (+ the 4 milestone models) drives the
faithful ATR+PPS handshake to "SIM detected" (frame `o016`), retiring `EXPERIMENT_SIM_CODE5` /
`MODEL_SIM_ATR_MSG` / `MODEL_SIM_RESPONDER` / `MODEL_SIM_LOOP` for the reach-detected path.
Minor remaining nit: the ATR timing is heuristic (Nth recv) rather than tied to the reset-start
`0x27e024`; good enough as a model. **Next (step 2):** trigger the dormant EF-read requester
(`0x29ff2c` / contact-service SIM path) and extend phase 1 to serve EF content (TS 51.011).

## Step 2 — driving the EF reads (2026-07-07)

Confirmed the EF-read requester is dormant: on the `MODEL_SIM_CARD` boot the code-3
file-read-request handler `0x27df9e` never fires — nobody asks the manager to read EFs
(`TRACE_SIMDATA`). Approach (b): **model the requester.** `MODEL_SIM_CARD` phase 2 posts a
code-3 request (the message is copied verbatim into the descriptor `0x10deec`: `[+2]`=len 5,
`[+4]`=code 3, `[+5..9]`=`a0 a4 00 00 02` SELECT, `[+0xa..b]`=file id; `SIM_CARD_EF`=file).
**Result:** `0x27df9e` fires for the first time and the manager issues a real `SELECT` for
the requested file — so the EF-read sequence is drivable by modelling the requester.

**Remaining (Phase A/B):** answer the SELECT with the T=0 dance (file-id data phase → `9F XX`
→ GET RESPONSE `a0 c0 …` → FCP block → READ BINARY/RECORD → content + `9000`), sequence the
mandatory EFs (`2FE2 ICCID`, `6F07 IMSI`, `6F38 SST`, `6F7E LOCI`, `6FAD AD`, `6F46 SPN`, …)
with synthetic TS 51.011 content, present CHV1-disabled — then see whether the completed read
flips the status past reject and lets `display_idle` fire.

## Step 2c — SELECT completion: the read is request-driven (2026-07-07)

The EF SELECT header is accepted once its response leads with the T=0 procedure byte
(INS echo) at `0x10dddc+2` (data path `0x27ee94`); a len-5 header then routes to `0x27ef0a`
→ status dispatcher `0x27e240` (reports the SIM status upward), then **`0x27efb0` recv's
again and re-dispatches at `0x27ede0`** (`3`→file-read, `1`→completion, `0xd`→keepalive).

So the file read is **request-driven**: the manager issues exactly one command per code-3
request, reports its status, and waits for the next request. Walking a full EF read means
the requester model posts a **code-3 per T=0 step** — `SELECT 6F07` → `GET RESPONSE a0 c0
00 00 XX` (→ FCP block) → `READ BINARY a0 b0 …` (→ content) — each answered with the INS-echo
procedure byte + payload + SW. That is the next increment (plus the GSM 11.11 FCP/EF layouts).

## Step 2d/2e — the full EF-read conversation completes (2026-07-07)

`MODEL_SIM_CARD` phase 2 scripts the EF-read T=0 sequence as one code-3 request per step
(`m_sim_card_step`): SELECT → GET_RESPONSE → READ → code-1 completion. The pending-driven
heuristic sequences them (post request when idle → manager issues APDU → phase-1 answers →
next request). Responses carry real payloads: GET_RESPONSE → a GSM 11.11 EF **FCP block**
(RFU/size/id/type/AC/status/structure), READ → synthetic content, both + `SW=9000`, led by the
INS-echo procedure byte at `0x10dddc+2`. Verified: all three APDUs issue in order and the
code-1 step drives the read-dispatch `0x27ede0` to the **completion handler `0x27ef34`**.

So the entire T=0 EF-read mechanism works end-to-end. **But** completing after one incoherent
EF regresses the screen `o016` (detected/blank) → `o074` ("Insert SIM card") — the phone judges
the SIM invalid. The mechanism is done; the open layer is **data coherence**: read the right EFs
(IMSI `6F07`, SST `6F38`, LOCI `6F7E`, AD `6FAD`, PHASE `6FAE`, …) with correct FCP/content in the
right order, CHV1-disabled, so the completed read is *accepted* as a valid SIM instead of rejected.
That likely also means finding where the phone decides valid-vs-invalid after the read completes.

## Step 2f — the coherence layer: reject cracked, MMI idle transition is the next wall (2026-07-07)

The completed read was being *rejected*. Found the read-complete valid/invalid decision
`0x27ea88`: it reads the no-SIM flag `[0x111c64]` -- `==0` posts the SIM-OK events `0xe8`/`0xea`,
`!=0` posts no-SIM `0x1f` ("Insert SIM card"). `TRACE_NOSIM`: the flag is set once at early init
(t=0.0055, no card then) and never cleared, so every completed read is judged invalid.

`MODEL_SIM_CARD` now clears `[0x111c64]` at read completion (`SIM_CARD_CLEAR_NOSIM`) -- the
faithful analogue of "valid SIM detected". Result: the `0x1f` post is **gone** (status posts are
`0x16` detected + read statuses only) and the boot survives (MMI alive at 20s) -- clearing it late
(post-detected) is safe, unlike the early global force that crashed. **So the SIM is no longer
rejected.**

But not idle yet. `TRACE_IDLEFLAG`: the MMI idle-draw flag `[0x1116fd]` (gate for `display_idle`
`0x2a255c` at `0x298000`) is **never written** across the whole boot. So the phone's top-level
state machine never advances from "SIM detected/read" to "idle". That is the next coherence layer
-- a new subsystem (MMI / phone-state, likely SIM-ready -> PIN-off -> idle), above the SIM read.
Knobs: `SIM_CARD_CLEAR_NOSIM`, `TRACE_NOSIM`, `TRACE_IDLEFLAG`.

## MMI idle-transition dig (passes 1-5, 2026-07-08)

With the SIM accepted (reject cleared) the phone still doesn't show idle. Dug the MMI/render path:

- **Idle flag corrected:** it is `[0x1116fd]` (MMI struct base `0x1116f8` +5, from literal `16f80011`),
  not `0x11f81b` (an old swap error). Never written on our boot -> MMI never triggers idle.
- **display_idle is reachable + executes.** `EXPERIMENT_MMI_IDLE` forces the flag at the gate `0x297ffa`;
  `display_idle 0x2a255c` then runs. But no idle pixels appear.
- **display_idle is tiny:** `push{lr}; r0=0x224c; bl 0x2b257e; r0=0x547; bl 0x2af6ea; pop{pc}` --
  "acquire display resource `0x224c`, post render `0x547`". Its render post `0x2af6ea` never fires after
  the forced draw, so it never gets past the resource-get.
- **The resource-get 0x2b257e** calls the availability check `0x2b12b4`, which returns 0 (unavailable)
  whenever the **display-ready flag `[0x11fee4]` is 0** (else it bit-tests resource bitmaps `[0x2e5c2f]`/
  `[0x1108ff]` for the id).
- **`[0x11fee4]` is NEVER written** the whole boot (`TRACE_DREADY`) -> stays 0 -> the idle draw's resource
  `0x224c` can never be acquired. The text screens render via a different path that doesn't need it.

**Root of the render wall:** the display subsystem never sets its ready flag `[0x11fee4]` (a display-init
path our boot doesn't reach). Reaching idle PIXELS needs that subsystem brought up -- a distinct wall
above the (now solved) SIM read + accept. Knobs: `EXPERIMENT_MMI_IDLE[_MS]`, `TRACE_DREADY`,
`TRACE_IDLEFLAG`, `TRACE_RENDER`(`_MAX`).

## The go-idle MMI trigger (2026-07-08)

Chased what sets the idle flag `[0x1116fd]` (=MMI base `0x1116f8` +5). Whole-image literal search
+ full MMI disassembly:
- In the MMI, `[+5]` is only READ (gate `0x297ffa`) and SET to 2 (post-draw `0x298006`). Nothing
  sets it to 1.
- The adjacent byte `0x1116fc` (`+4`) has a get/set dispatch at `0x297bc0` -- not the idle flag.
- `TRACE_IDLEFLAG` confirms `0x1116fd` is NEVER written at runtime by any path.
- The MMI is a **window/screen state machine**: `0x297ed8` indexes an array at `0x111724` by the
  active window `[0x1116f8+8]`, per-entry state `[+0x19] in {1,2,3}`. Screens are window states.

So the go-idle "trigger" is not a single pokeable flag: the idle flag is set when the MMI enters
the **idle window state**, which our boot never does. Same coherent-state wall -- the phone's UI
state machine never transitions to idle (upstream of both the idle flag and the display bring-up).
Reaching idle pixels needs that transition, not a poke.

## Boot-to-idle: the complete dependency map (2026-07-08)

After the SIM is faithfully accepted, reaching operator-idle needs TWO chains, both now mapped
end to end. Neither is a single poke; both are message/state flows a fully-provisioned boot
produces and a faked/unprovisioned boot does not.

**Chain A — the startup state machine (now traversable):**
`SIM accepted` → mode `000d` (service-ready, cleared by `MODEL_SVC_CHANNEL_DRAIN`) → **mode `0004`**:
needs raw handshake `3→0xe→7` → **ev7-init** `0x271266` (runs app-subsystem init) → readiness
accumulator `0x2712c0`: the self-tick `0xc3` hits the else branch → terminal `000c` UNLESS all
flags `0x112390-95` are set and event `0xd` triggers the completion check `0x271326` → advance
`0x271364` (charger absent) → **mode-7 event-`0x74` wait** → outcome 3. `EXPERIMENT_MODE4_EVENTS`
drives all of this (inject `3/0xe/7`, pre-set flags, tick→`0xd`, inject `0x74`). Lands on outcome 3.

**Chain B — the interactive MMI app layer (dormant):**
MMI task `0x297fc4` is alive but STARVED — it receives 2 init messages (t<0.85) then nothing.
The window manager `0x297ed8` (called from `0x2981be`, gated on MMI message subcodes `0x22/0x24/0xf0`)
never runs → no windows created → idle flag `0x1116fd` never set → `display_idle 0x2a255c` never
requested. Separately the display-resource layer is down: display-ready `0x11fee4` never set, so
`0x2b257e(0x224c)` (the idle draw's resource-get) always fails. "Insert SIM card" on screen is a
LEFTOVER from the status subsystem's early render (via the render task), NOT the MMI window manager.

**The gap:** Chain A reaching outcome 3 does NOT emit the messages Chain B needs (window creation /
"SIM ready → go idle"). On a real boot the completed startup drives the app controller to create the
idle window and bring up the display; our faked boot produces neither. So idle PIXELS require the
coherent app-layer message flow — the fundamental provisioned-boot wall, upstream of everything and
not reachable by injection. The SIM-conversation problem (the emulator's purpose) is solved; idle is
a coherent-boot problem on the MMI/display application layer.

## The MMI-wake producer chain (2026-07-08)

Traced the exact producer chain behind the starved MMI (Chain B), by matching MMI messages to
their posters by message POINTER (`TRACE_MMIPROD`: logs every send via `sched_post_task_message_26a204`
and `sched_context_post_message_26a354` plus every MMI recv at `0x29800c`).

**The MMI is task 6.** Its message loop `0x298008` dispatches on primary code `[msg+5]`:
- code 1 (`0x2982b4`) = app register/init  -- ARRIVES (t=0.57)
- code 2 (`0x29817e`) = **window management** -> falls into the window-SM path `0x298190 -> 0x2981be ->
  0x297ed8`. **NEVER ARRIVES.**
- code 3 (`0x2980c4`) = a window/key op -- ARRIVES (t=0.84)

So the single missing "wake" is a **code-2 message to task 6**. The posters are a sibling family:
- code-1 poster `~0x2b1ee0` (called), code-3 poster `0x2b1f64` (called),
- **code-2 poster `0x2b1f24`**: allocates a msg, sets `[msg+5]=2`, posts to task 6 via `0x26a354`.
  **Never called** on our boot.

`0x2b1f24` has MANY callers, ALL in the **unexplored `0x240xxx-0x242xxx` subsystem** -- a display/screen
layer that compares bitmap regions (`ldrsb`/`muls` over pixel arrays) and, on a difference, posts code-2
to the MMI (e.g. `0x24092e` with window params `r1=0xf1,r2=0x10`) and writes state `[0x11f802]`. That
whole subsystem is DORMANT on our boot -- none of its code-2 posts fire.

**So Chain B's wall is now precise and one level higher than "MMI dormant": the `0x240xxx` display/window
subsystem never runs, so it never posts the code-2 message that would drive the MMI window-SM to create
the idle window.** Next step: characterize `0x240xxx` -- is it a task (is it scheduled/resumed at all?),
and what message/state drives its screen-diff loop. That producer is the next node up the same chain.

## Into the display/window subsystem (2026-07-08, 5-pass dig)

Climbed one node further up the MMI wake-chain: characterized the 0x240xxx display subsystem that
should post the code-2 (window-mgmt) message to MMI task 6.

- **P1 (TRACE_DISPSUB):** the subsystem is ALIVE, not dark -- it enters at 0x24047e (t=0.84) and spins
  a periodic idle-redraw loop 0x2404d8-0x240552 (~0.72s), but never reaches the code-2 post sites 0x2409xx.
- **P2:** 0x24047e is a state dispatcher on a control block r4; every meaningful path gates on the type
  byte [r4+1] == 0x80 (active window). With type != 0x80 it routes to the idle redraw.
- **P3:** the control block is 0x0010e2ec, with type[+1]=0 and state[+2] cycling 0<->3.
- **P4 (TRACE_DISPCB):** the type byte is ALWAYS written 0x00 (from 0x23e694) and never 0x80 -- the loop
  keeps resetting it; the window-activate action that would write 0x80 never runs.
- **P5:** the writer/owner is a live display/window TASK at 0x23e62c that recv's at 0x23e646 (message
  codes based at 0x0b04) and, in its default handler 0x23e67e, sets manager state=[msg+2] and
  type=[0x10e461] (the window entry's type). At runtime this task receives ONLY code 0x05df (a periodic
  refresh tick, [+2]=00,[+3]=ff) -- NEVER the window-create codes 0x0b04+ that would populate an active
  window. So it is message-starved exactly like the MMI above it.

**Full chain (6 levels), all mapped:** top-level app state -> (missing) window-create message 0x0b04+ ->
display/window task 0x23e62c -> manager 0x10e2ec type stays 0 (never 0x80) -> 0x240xxx dispatch idles ->
code-2 poster display_type2_post_2b1f24 never called -> MMI task 6 never gets code 2 -> window-SM
0x297ed8 never creates the idle window. Every layer is a live-but-starved task waiting on a message the
layer above never sends, rooted in the top-level app state never issuing "show idle" -- the same
coherent-boot wall, now traced end to end with precision. Next node up: who sends code 0x0b04+ (window
create) to the display task, and what gates it.

## Climbing to the window-create sender (2026-07-08, 5-pass dig)

Pushed up from the display task toward whoever sends it window-create commands.

- **P1 (TRACE_DISPPROD, pointer-match):** the display/window task is **task 13**. Its messages are matched
  to posters by message pointer; the 0x05df refresh comes from lr=0x28d182 (movs r0,#0xd; bl 0x26a204).
- **P2:** across the boot, task 13 receives ONLY code 0x05df (42x) -- a periodic refresh tick that is BELOW
  the task's dispatch base 0x0b04, so it always hits the default handler (manager update). The real window
  commands 0x0b04+ NEVER arrive.
- **P3:** static scan for post-to-13 sites finds only 5 immediate `movs r0,#0xd` posts; most task-13 posts
  load the target from a register, so an immediate-scan can't find the dormant window-create sender.
- **P4 (EXPERIMENT_FORCE_WINTYPE, clean negative):** poking the window entry type [0x10e461]=0x80 does NOT
  make the 0x240xxx dispatch reach the code-2 post sites (0x2409xx), and no code-2 reaches the MMI. So the
  code-2 post is NOT gated on manager state -- it is driven by the window-command messages themselves.
- **P5:** the display task's 0x0b04+ handlers are real window logic: 0x0b06 (0x23e702) links a window entry
  into the manager and gates on window type [r4+1]==0x80; 0x0b04 (0x23e7ac) sets manager state via
  0x23e324/0x23e378 against window-id constants 0x061a/0x061b. So the code-2-to-MMI post is genuinely
  downstream of a real 0x0b04+ window command carrying window data.

**Net:** the wall is now precise at this level -- task 13 (display) is a live task starved of the window-create
commands (0x0b04+) that the app issues on "show a screen"; it only gets the 0x05df refresh tick. Forcing
manager state does not substitute for the command. Same coherent-boot root (app-state never issues show-idle),
one node higher. Next: find the register-target sender of a 0x0b04+ command to task 13 (needs register-aware
static analysis, not an immediate-scan), or settle the provisioning question via 3310 cross-firmware.

## Settling the provisioning question (2026-07-08, 5-pass dig)

Question: is blank-boot-to-idle blocked by missing provisioning (NVRAM/network), or by the coherent-boot
state handoff? (The 3310 cross-firmware image was BYO/local and is not present this session, so this is a
3210-only structural analysis.)

Findings:
- The app layer is ALIVE, not hung: the UI controller / phone-state machine at 0x2a0xxx, MMI task 6,
  display task 13, and the render task all run and loop. The phone is in a multi-task steady state.
- The UI controller dispatches on a phone-state byte [0x11fcdc], using states 1/3/4/6 (posts MMI window
  ops + calls the 0x240xxx display window fns 0x248ac2/0x24b6cc in the state-1 path).
- TRACE_UISTATE: [0x11fcdc] progresses 0x00 -> 0x0b (t=0.53) and then STOPS at 0x0b for the whole boot.
- 0x0b is written by a ROM-TABLE state initializer (0x2aef7e reads table 0x2cc7f0), NOT from EEPROM/NVRAM
  and NOT from a network event -- it is the initialized default. The UI controller's own states are 1/3/4/6,
  so it never drives [0x11fcdc] out of the initialized 0x0b toward the window-creation states.

**Answer.** The current wall is NOT a provisioning-DATA problem: the stall state is a ROM-initialized
default, gated by no NVRAM/EEPROM read, so modelling more provisioning data would not unblock it. It is NOT
(yet) a network-registration wall either: the app layer is alive but stuck UPSTREAM of any network gate,
before it ever attempts registration. The wall is the SAME coherent-boot state handoff as every prior one
(CONTACT SERVICE, 000d, mode-4, MMI): the faked/incoherent reconstruction never generates the internal
state-transition events that a real completed boot uses to drive the UI state machine from its initialized
state through window creation to idle. Separately, true OPERATOR-idle (operator name) would additionally
require live network registration -- a base station, i.e. hardware -- but that is a further wall we never
reach. So: provisioning data is ruled OUT as the blocker; the blocker is coherent-boot state progression
(digital but hard), with network=hardware as an additional requirement only for operator-idle proper.

## Decoding the UI state machine (2026-07-08, 5-pass dig)

Goal: decode "UI state 0x0b's handler." Result: the MMI UI is a table-driven, EVENT-DRIVEN state machine,
and the stall is event-starvation, not a single waiting handler.

- **P1 (TRACE_UICTL):** the UI controller region 0x2a0a00-0x2a0d00 executes EXACTLY ONCE (t=0.76), then
  never again -- it is an event handler invoked once and then starved, not a spinning dispatcher.
- **P2:** its caller LR is 0x2ac65e. Also corrected an earlier byte-offset slip: the dispatch state byte
  [0x11fcdc] is 0x00 (the 0x0b my halfword trace showed sits in the adjacent byte 0x11fcdd).
- **P3:** 0x2ac6xx is the MMI UI state-machine ENGINE: it indexes a per-state handler function-pointer
  table by the current state [r5+3], loads the handler ([entry+4]), and `mov lr,pc; bx r1` calls it with an
  event code in r0 (0x05ed at t=0.76). The handler returns the next state/event (r0 -> r6).
- **P4:** the engine loop (0x2ac540+) walks the state table, and on transitions calls display fns
  (0x24aedc/0x24af00/...) and posts event 0x30 (0x2695f4). State vars: [0x11cefc], [0x11c930].
- **P5:** the engine has no recv of its own in-range; it is driven per-event (MMI signal codes in the 0x05xx
  range: 0x05dd/0x05de/0x05ed). It processed the initial boot event (0x05ed) once and then stopped.

**Decoded answer.** "State 0x0b" is not a lone handler blocked on one flag; the MMI UI is an event-driven
state machine whose per-state handler (the 0x2a0aec UI controller) ran once on the initial boot event
(0x05ed, t=0.76) and then received no further driving events, so the state never advances to the
window-creation states. This is the top-level form of the same coherent-boot wall: the faked boot emits the
initial MMI event(s) but not the ongoing 0x05xx event stream a real completed boot generates to walk the UI
state machine through to idle. Open thread: identify the specific next 0x05xx event the engine expects after
0x05ed, and its (dormant) producer.

## Hunting the next UI event + producer (2026-07-08, 5-pass dig)

Goal: find the specific event that would advance the MMI UI toward window creation, and its producer.

- **P1-2:** the MMI UI engine (0x2ac6xx) is a large function whose entry/recv disrom can't cleanly locate
  (it misdecodes parts of 0x2ac6xx). Pivoted to the state handler 0x2a0aec: a big event dispatcher that
  branches on the event code (0x05de/0x05e1/0x05e8/0x09c6/0xca/...).
- **P3:** decoded the handler's subtract-cascade: the window-create path (0x2a0c3a/0x2a0c40, which posts MMI
  code-3 + calls display fns) is reached by internal event **0x05e8** (0x05e1 +2 +5 -> beq 0x2a0c38).
- **P4 (TRACE_UICTL @0x2a0aec):** the handler runs EXACTLY ONCE, with internal event **0x05e2** (t=0.76),
  never again. The window-create event 0x05e8 never reaches it. Separately (TRACE_DISPPROD) an external
  message code 0x05e8 is posted 41x -- but to TASK 16, from the display task (0x23e306).
- **P5:** the MMI UI engine runs in **TASK 5** (read via the current-task id at [0x00100022] -- note the
  swap16 trap struck again: the recv's pointer literal 0x00200010 is really 0x00100020, so the task-id byte
  is 0x00100022, not 0x00200012). Task 5 IS fed messages (0x13xx codes from 0x2af732) repeatedly, but the
  window-create handler 0x2a0aec fires only once -- the state machine transitions to a state that does not
  progress to idle.

**Net.** Concrete gains: the MMI UI state machine lives in task 5; its window-create handler advances on
internal event 0x05e8; on our boot that handler runs once (internal event 0x05e2) and the machine then sits
in a non-progressing state. The external 0x05e8 messages go to task 16, decoupled from task 5's engine.
Pinning the exact external message -> internal 0x05e8 mapping requires decoding task 5's large engine
(message->event->state), which disrom partially misdecodes -- the honest limit this round. Next: decode task
5's engine event mapping (what external message the engine turns into internal 0x05e8), or trace task 5's
recv to see which 0x13xx messages it consumes and where the state machine parks.

## Decoding task 5's message->event mapping (2026-07-08, 5-pass dig)

- **P1 (TRACE_T5RECV):** task 5's main loop recv is at 0x2af57e (ret 0x2af582); it recv's rarely (3x in 10s,
  mostly blocked doing render work between messages).
- **P2:** the loop's 0x26a698 turned out to be a scheduler queue-count helper (walks the TCB ring at
  0x00101484 stride 0x1c), NOT the message->event mapper. The mapping is done inside the engine.
- **P3:** the window-create event 0x05e8 is never a pool literal -- it is a computed value in a contiguous
  MMI event enum (~0x05dd-0x05ef), reached by the handler's subtract-cascade. So the message->event mapping
  is computed in the engine (0x2ac6xx), which disrom partially misdecodes.
- **P4 (empirical):** one concrete mapping captured -- task 5 recv'd message code 0x012e (t=0.46) and the UI
  handler then ran with internal event 0x05e2 (t=0.76). So 0x012e -> 0x05e2 in the current state.
- **P5 (EXPERIMENT_FORCE_UIEVENT, VALIDATION):** forcing the handler's one invocation event to 0x05e8 caused
  a NEW code-3 (window op) post to MMI task 6 (t=0.777, poster 0x2b1fa0) that otherwise happens differently
  -- confirming 0x05e8 IS the window-create advance event. But it did NOT complete to a code-2 (window-mgmt)
  post nor change the frame: forcing one event advances one step; full window creation needs the coherent
  follow-through (state + subsequent events), the incoherent-forcing wall.

**Net.** Confirmed (by forcing) that internal event 0x05e8 is the MMI UI window-create advance; captured one
real message->event mapping (0x012e -> 0x05e2); localized the full mapping to the engine 0x2ac6xx that disrom
misdecodes (a tooling limit). The exact external message that maps to 0x05e8 remains open -- it needs either
a disrom fix for 0x2ac6xx or a Ghidra pass on that function. Forcing 0x05e8 gets one step (code-3) but not
visible idle. Next: fix disrom's Thumb resync on 0x2ac6xx (or Ghidra-decompile it) to read the message->event
table, OR follow the forced code-3 downstream to see what the next missing step is.

## Decoded: task 5's message->event mapping (2026-07-08, 5-pass, with fixed disrom)

With disrom's Thumb-1 resync fixed, the MMI UI engine (0x2ac6xx) is readable and the mapping is decoded.

**Pipeline (task 5 top loop 0x2af630):** recv+decode 0x2af57c (msg -> 13-bit code, e.g. 0x012e) ->
rewrite mapper 0x2aefba -> engine 0x2ac3f2 -> ... The engine passes its input event straight through to the
state handler (0x2ac400 -> 0x2ac422 -> dispatch 0x2ac652, r6 unchanged), so handler input == engine input ==
0x2aefba output.

**The mapper 0x2aefba is an ITERATIVE REWRITE, not a flat map:** it stores the event at [0x112086], then
binary-searches a sorted table via 0x2aed5c and applies an action (0x2aef44/0x2aef7e), looping until the
event stabilizes.
- Binary-search table at **0x002cb218** (swap16 of literal b218002c; the trap struck again -- NOT 0x2c18b2),
  8-byte entries sorted by key: [+0]=event key, [+2]=action index, [+4]=count. Decoded entries:
  key 0x012e -> index 0x80, count 6;  key 0x05e3 -> index 0x17d;  keys run 0x00c8..0x05e3+.
- Action table at **0x002cc7f0** (= the state-init table): each action writes state bytes into the
  0x11fc80+ array (this is what set UI state byte 0x0b earlier), i.e. it drives state transitions, not a
  simple event substitution.

**Why 0x05e8 has no producer:** the window-create event 0x05e8 is NOT a table key -- it is computed in the
engine as 0xbd<<3 (0x2ac660) and used as a RETURN threshold. The return-chain never feeds 0x05e8 back as a
handler INPUT, and no rewrite rule outputs it. So window creation is not reachable by any single message that
"maps to 0x05e8"; it requires the state machine to arrive at the window-create state through the coherent
transition sequence the rewrite table encodes -- consistent with every prior finding (forcing 0x05e8 got one
step but not a coherent window). Net: the mapping is fully characterized as a table-driven state-transition
rewrite (0x2cb218 keys + 0x2cc7f0 actions); the exact transition path to the window-create state is the
remaining (large) decode. Next: walk the 0x2cc7f0 action entries for index 0x80 (the 0x012e transition) to
see the state sequence it sets, and find which state's handler emits the window-create.

## The MMI VM is ALIVE, not dormant — the wall is display repaint (2026-07-08)

Walked the `0x2cc7f0` action table and instrumented the task-5 MMI engine live
(`TRACE_MMIVM`: hooks the rewrite mapper entry `0x2aefba` for the incoming event
key, and every write into the state vector `0x11fc80..0x11fcff`). Two results,
one of them a correction to the standing framing.

**Engine structure (static).** The task-5 MMI state machine is a table-driven
bytecode VM:
- **Dispatch table `0x2cb218`** — 8-byte entries `{[+0]=key(hw), [+2]=action
  base(hw), [+4]=count(b)}`, sorted by 13-bit event key, binary-searched by
  `0x2aed5c`. `0x012e → base 0x80`; next key `0x012f → base 0x86`, so the run
  length is the base-delta (6 entries).
- **Action table `0x2cc7f0`** — 8-byte entries interpreted two ways: `0x2aeda0`
  as a **predicate** (reads `state[0x11fc80 + op]`, compares to `[+1]`, bit7 =
  negate; ops `0xb7..0xb9 → fn-ptr table 0x2cb160`, `≥0xba → table 0x111f0f`,
  `<0xb7 → direct state slot`), and `0x2aef7e` as **apply** (writes
  `state[0x11fc80 + slot] = [+2] & 0x3f`).
- **State vector `0x11fc80`.** UI-state `0x11fcdc` is slot `0x5c` of it, so the
  VM's writes and the UI-state dispatch are the same structure.

**Live behaviour (the correction).** Under the full SIM config
(`MODEL_DSP_SERVICE + MODEL_CCONT_PRESENT + MODEL_SVC_RESPONDER +
MODEL_SVC_CHANNEL_DRAIN + MODEL_SIM_CARD + SIM_ATR_HEX=3b1005 +
SIM_CARD_EF=0x6f07 + SIM_CARD_CLEAR_NOSIM=1`), the VM **runs continuously for the
whole boot** — 2639 state ops, a rich event stream (`0x0aa4 0x0517 0x06ce 0x1b59
0x09d1 0x138e/0x1390 0x13b5/6 0x1581 0x0596 0x05de/0x05e0/0x05f3 …`) through
t=29.7. The earlier "MMI dormant / task starved (2 msgs then nothing)" was an
artifact of a **less-complete boot config**; with the full SIM stack the MMI app
layer is alive and ticking.
- Dominant event **`0x05e2`** (1000× in 30s) is a **periodic timeout tick**:
  handler `0x28c464` increments the slot-`0x4e` counter (`0x11fcce`) each tick
  and, at count `0x7d` (125), fires event `0x12f` via `0x2ac3e0`. It is not even
  a rewrite key (passes through the mapper unchanged).
- **`0x05e8` (window-create) never fires** across the entire run — confirming the
  static result that it is a computed `0xbd<<3` return threshold, not an emitted
  event. Window creation is unreachable by any single message.

**So the real wall is display REPAINT, not MMI logic.** The screen still OCRs
"Insert SIM card" even though the SIM is accepted internally (`[0x111c64]`
cleared) and the MMI VM is churning post-SIM events. The blocker: the
display-ready flag **`[0x11fee4]` gets zero writes the entire boot** — the
firmware only *reads* it (an externally-set "ready" flag; `TRACE_DREADY`), so
resource acquisition `0x2b12b4` returns "unavailable" and the screen can't repaint
to the post-SIM/idle window.

**Next.** RE what sets `[0x11fee4]` on a real boot (fw never writes it — set by
service registration / external state; note `0x11fee4` is also named
`FW_SERVICE_CHANNEL_ENABLE_FLAGS` at driver line 228, a scope overlap to resolve),
and whether it is faithfully modellable (analogous to `MODEL_SVC_CHANNEL_DRAIN`),
or whether the resource-acquisition gate `0x2b12b4` is the cleaner lever. Knob:
`TRACE_MMIVM` (opt-in, cap 400).

## The display-ready wall, mechanized: resource registration never runs (2026-07-08)

Five passes on what sets display-ready `[0x11fee4]` and whether it is faithfully
modellable. Result: the wall is the **resource-provider registration** layer, and
both gates it depends on are downstream of a display-subsystem bringup our faked
boot never performs.

**`[0x11fee4]` = master resource-enable (one reader in the image).** A literal scan
finds exactly one pool reference to `0x0011fee4`, in the resource-function cluster
`0x2b12b4` (availability predicate) / `0x2b12dc` (acquire). `0x2b12b4` returns
"unavailable" the instant `[0x11fee4]==0`. Firmware only ever *writes* it to 0
(reset `0x2b13c0` clears `[11fee4]/[11ff28]/[11fee5]/[11f824]`); nothing on our boot
sets it nonzero. (This reconciles `TRACE_DREADY`'s "0 writes" — a 0→0 store is
filtered.) It is also `FW_SERVICE_CHANNEL_ENABLE_FLAGS` (driver line 228): the same
flag gates the contact-service remote read, so it is a system-wide "resource/service
channel enabled" bit, not display-specific.

**`0x2b12b4` decoded.** `available = ([0x11fee4] != 0) AND bit(class&7) of
[0x11ff08 + class>>3]`, where `class = id>>8`. The idle draw acquires resource
`0x224c` → class `0x22` → bit 2 of `[0x11ff0c]`. So availability needs *both* the
master enable and a per-class registration bit in the bitmap `[0x11ff08]`
(`FW_SERVICE_CHANNEL_MASK_BASE`).

**The bitmap is never registered.** `TRACE_RESREG` (new; writes to
`0x11ff08..0x11ff10`) logs **zero writes the entire boot** — no resource class is
ever registered. The "Insert SIM card" screen renders via a pre-resource path; the
idle repaint is what needs a registered resource. The bitmap has a single literal
reference in the image (the same `0x2b12b4` cluster), i.e. the registrar that should
set it never executes on our boot.

**Decisive test — grant the resource, does idle draw?** `EXPERIMENT_MMI_IDLE_DREADY`
was extended to force all three gates at the idle gate `0x297ffa`: idle flag
`[0x1116fd]=1`, enable `[0x11fee4]=1`, and the whole bitmap `[0x11ff08..0f]=0xff`.
Result: `display_idle` runs and gets **past** the availability check, but still emits
**no idle render post** (`0x2af6ea` — the render-task hand-off — stops after t≈0.88;
only housekeeping recvs `0x2af638` fire after the t=14.9 force) and the frame never
becomes an idle screen. So the flags+bitmap are **necessary but not sufficient**: the
resource *content/memory* the idle layout needs is built by the display-subsystem
bringup, which our boot never performs.

**Conclusion.** The display-ready wall is fully mechanized. `[0x11fee4]` and the
`[0x11ff08]` bitmap are products of a resource-provider registration that never runs;
forcing them moves the wall one step (past availability) but the idle draw still
cannot compose content — identical in kind to every prior incoherent-force → black
result, now localized to the resource-registration layer. Idle pixels remain a
faithful display/resource-subsystem bringup task, not an injectable flag. Knobs:
`TRACE_RESREG`, `EXPERIMENT_MMI_IDLE_DREADY` (now also grants the bitmap).

## The resource-provider registrar: a contact-service command (2026-07-08)

Five passes to find what should set the resource bitmap `[0x11ff08]` + enable
`[0x11fee4]`. Found it, and it closes the causal chain back to the known boundary.

**Registrar = `resource_system_init 0x2b140a(r0, enable=r1, r2, config_ptr=r3)`.**
Writers of the enable and bitmap live in the same resource cluster as the reader:
- `0x2b140a`: if `r3==0` → disable (`[0x11fee4]=0`); else sets `[0x11fee4]=r1`,
  `[0x11ff28]=r2`, `[0x11fee5]=r0`, and if `r1!=0` memcpy's a **0x40-byte config
  blob** from `r3` into the two bitmaps `[0x11ff08]` (0x20 bytes) and `[0x11fee8]`
  (0x20 bytes) via `0x2b5c7c`, then OR's bit `0x80` into `[0x11ff08]`. So *which*
  resource classes are available is decided by a **caller-supplied config table**,
  not hardcoded.

**4 callers, all in the contact-service command layer `0x236xxx`:**
- **ENABLE** — `0x2366d4 → 0x2366f6`: validates a 0x40-byte blob (`0x2a41d0`), then
  calls `0x2b140a` with `enable=[0x11fedd]` and **`config_ptr = message_base+9`** —
  the resource bitmap arrives as a **contact-service command payload**.
- **DISABLE** — `0x23672c` inside the channel-map handler `0x23670c` (when command
  byte `[+8] != 0x70`): `0x2b140a(0,0,0,0)` clears `[0x11fee4]`.
- `0x236e6c` / `0x236f10` — two more command handlers.
These are reached via the contact-service command cascade (`b`-branches on
`message[+8]`), not a jump table, so they are invisible to pointer/`bl`-caller scans
— consistent with the service dispatch.

**Never runs on our boot.** `TRACE_RESINIT` (new hook at `0x2b140a`) logs **zero
calls** the entire boot — the resource-enable command is never dispatched. So the
resource registration is gated behind the full contact-service session, the **same
boundary** as CONTACT SERVICE (cmd `0x64`/`0x65`), service-ready, and `000d`.

**Verdict.** The display-ready wall is not a standalone hardware/injectable gate —
it is another facet of the contact-service session. It is modellable in principle
(deliver the enable command with a valid config blob, `MODEL_SVC_RESPONDER`-style),
but two caveats stand: (a) the 0x40-byte config is provisioned/firmware data to
source faithfully, and (b) the prior pass already proved the bitmap alone is
necessary-but-insufficient (with resources granted, `display_idle` still cannot
compose content). So idle pixels remain the full coherent display-subsystem bringup,
now traced end-to-end: contact-service enable command → `0x2b140a` → bitmap +
`[0x11fee4]` → resource availability `0x2b12b4` → idle draw's resource content.
Knob: `TRACE_RESINIT`.

## The resource-enable command byte, pinned: contact-service 0x70 (2026-07-08)

Traced the contact-service command dispatcher `0x237400` to the exact command that
drives the resource registrar.

**Dispatcher.** `0x237400` reads the command byte `[msg+8]` into r4 (`0x23741a`) and
runs a binary-search cascade to a per-command slot in the handler table at
`0x2377f4+` (each slot: `adds r0,r5; bl handler; b`). The channel-map handler
`0x23670c` — which holds the `0x2b140a` enable/disable calls — sits at slot
`0x237814`, reached by `b 0x237814` at `0x2374be` for **command bytes 0x70/0x71**
(arithmetic: `r0 = (cmd-0x6f)-1 = cmd-0x70; cmp #1; bhi` ⇒ `cmd ∈ {0x70,0x71}`).

**Discrimination inside `0x23670c`** (same `[msg+8]` byte):
- **`0x70` → ENABLE** — sends response subcmd 0x70, then `0x2366c8` (if `[msg+5]>0x42`)
  → `0x2366d4` → `0x2b140a(enable=[msg+..dd], config_ptr = msg+9)`: registers the
  bitmap `[0x11ff08]` and sets `[0x11fee4]`.
- **`0x71` → DISABLE** — `0x2b140a(0,0,0,0)` clears `[0x11fee4]`.

**Empirical confirmation (`TRACE_CSCMD` at `0x23741a`).** On our boot the
contact-service processes **exactly one** command — `0x64`, the completion
`MODEL_SVC_RESPONDER` injects — and **cmd `0x70` never arrives** (0 occurrences).
So the resource-enable command is simply never sent on our minimal faked session:
`MODEL_SVC_RESPONDER` supplies only the `0x64` completion, whereas a real service
session would also carry the `0x70` channel-map / resource-enable command (with its
0x40-byte config blob). 

**This is the precise, final localization of the display-ready wall: contact-service
command `0x70`.** Full traced chain: service session sends cmd `0x70` → dispatcher
`0x237400` → channel-map `0x23670c` → `0x2366d4` → `0x2b140a` → bitmap `[0x11ff08]` +
enable `[0x11fee4]` → availability `0x2b12b4` → idle-draw resource content. To reach
idle pixels faithfully one would model delivery of cmd `0x70` with a valid config
blob (the necessary step), though the resource *content* pipeline is a further
coherent-bringup requirement (shown earlier: bitmap alone is insufficient). Knob:
`TRACE_CSCMD`.

## MODEL_RES_ENABLE: delivering contact-service cmd 0x70 (2026-07-08)

Built a model that delivers the resource-enable command `0x70` the real service
session would send (but our minimal faked session omits), driving the firmware's
own registrar `0x2b140a` via the real command path — no forcing of the flag/bitmap.

**Mechanism** (an `SVC_RESPONDER`-style trampoline at the contact-service loop top
`0x237bc6`): alloc `0x26afe0`(0x50) → fill a cmd-`0x70` message (`[3]=0x40` dispatch
route, `[5]=0x50 > 0x42` enable gate, `[8]=0x70`, config blob `[9..0x49]`) → post
`0x26a204` to the contact-service task. The firmware then runs `0x237400` →
channel-map `0x23670c` → `0x2366d4` → `0x2b140a`, which sets enable `[0x11fee4]=1`
(verified: `dready [11fee4] 00->01 @0x2b141c`) and memcpy's the blob into the
availability bitmap `[0x11ff08]` (verified: `resreg` writes @`0x2b5cd8`).

**Three prerequisites** the faked session doesn't satisfy, each modelled:
1. **Order** — deliver `0x70` *before* the `0x64` completion (which ends the command
   loop). `SVC_RESPONDER` now waits for `RES_ENABLE` (`m_resen_state==3`) when both on.
2. **Gate** — the dispatcher gates all non-`0x64` commands on service-ready
   `[0x11fed1]` bit0 (`0x237426`, skips to `0x237894` if clear; `0xcc` on our boot).
   Seed bit0 (analogous to `MODEL_SVC_CHANNEL_DRAIN` seeding bit2). `0x64` bypasses
   this gate (checked first), which is why it worked while `0x70` was dropped.
3. **Enable value** — the handler reads the enable arg from `[0x11fedd]` (only-read on
   our boot; set by a prior channel-setup command on a real phone). Seed it at the
   enable-handler entry `0x2366d4`.

**Config blob.** Default is **sparse** — register only the idle-draw resource class
`0x22` (bit2 of `[0x11ff0c]`). `RES_ENABLE_FILL=0xff` enables all 256 classes but
**blanks the display** (the firmware acquires classes with no real provider); the
faithful blob is the real 0x40 provisioned bytes, not obtainable here. The sparse
default keeps the display alive ("Insert SIM card" still renders) while genuinely
registering the resource.

**Result.** With resources genuinely registered via the real path, the idle-draw
availability wall is **removed** — `0x2b12b4` no longer spins (contrast the earlier
forced-bitmap probe). But `display_idle` still composes **blank** content, confirming
*through the faithful path* that resource registration is necessary-but-insufficient:
the idle **content pipeline** (populating the layout's actual pixels — clock, operator,
etc.) is the next layer, and it needs the coherent SIM/network bringup a no-SIM boot
doesn't do. The boot correctly stays on "Insert SIM card". Knobs: `MODEL_RES_ENABLE`,
`RES_ENABLE_MS`/`MSGSZ`/`FILL`/`VAL`.

## The idle content pipeline: a ~18-class resource composition (2026-07-09)

Dug into what actually fills the operator-idle pixels. `display_idle 0x2a255c` is
minimal — `resource-get(0x4c22)` (acquire the idle window) + `render-post(0x547)`
to task 5. The content is composed downstream from a large set of drawable
resources.

**Two swap16 corrections** (the recurring trap) surfaced here:
- The idle window id is **`0x4c22` (class `0x4c`)**, not `0x224c`/class `0x22`
  (literal `4c220000` unswaps to `0x4c22`; confirmed by `0x2b257e` doing
  `(id>>8)-0x4c = 0`). Earlier notes had this backwards.
- The availability bit-mask table `0x2e2f5c` is **permuted**
  `{0x40,0x80,0x10,0x20,0x04,0x08,0x01,0x02}`, not `1<<i`; class `0x4c` needs bit
  `0x04` of `[0x11ff11]` (not `0x10`).

Fixed `MODEL_RES_ENABLE`'s sparse config to register the real display resource
classes (`0x4c/0x4f/0x50/0x52/0x56` — the ROM-def-table `0x2e0a50` keys) with the
correct permuted masks. `TRACE_RESAVAIL` (new, hooks `0x2b12b4`) then confirms the
idle window `0x4c22` is **AVAILABLE** (`byte[11ff11]=06 & mask 04`).

**But the idle draw still can't compose.** With the window acquired, the
composition queries **~18 more resource classes** — `0x22 0x25 0x26 0x27 0x2a 0x2b
0x30 0x31 0x3a 0x3c 0x3d 0x44 0x4a 0x5c 0x5d 0x5e 0x78` (fonts, icons, layout
elements, sub-windows) — and **every one is unavailable** (bitmap byte 0).
`display_idle` acquires the top-level window, but every drawable inside it is
unregistered, so nothing composes → blank.

**Conclusion.** The idle content pipeline needs:
(a) the **full faithful resource registration** — the real cmd-`0x70` config blob a
real phone sends, enabling exactly this ~18-class set *with their ROM/RAM backing
providers*. This is provisioned data not obtainable here; all-`0xff` enables the
classes but also unbacked ones, destabilizing/blanking the display.
(b) the **runtime content values** — clock (RTC, available), operator/signal (SIM +
network registration = RF hardware, out of scope).

So operator-idle is fundamentally a SIM+network state; without them "Insert SIM
card" is the correct and complete terminal screen. The whole boot→pixel path is now
mapped end-to-end, down to the per-class resource composition. Boot-to-"Insert SIM
card" stands as the clean milestone. Knob: `TRACE_RESAVAIL`.

## Digging the resource-content pipeline — verdict: unbacked, not bit-flippable (2026-07-09)

Followed the last digital frontier: can the idle screen's *content* be rendered by
registering the resource classes it needs?

**Resource model.** `display_idle 0x2a255c` = `resource-get(0x4c22)` + `render-post
(0x547 → task 5)`. Resource-get `0x2b257e` for the idle window (class `0x4c`, no
caller data) routes through the acquire `0x2b12dc`, which builds a **management
descriptor (a handle)** — it allocates + fills metadata, it does **not** hold pixels.
The actual content (fonts / icons / layout) is ROM data fetched *by id at draw time*.
So in principle the data is present (it must be — "Insert SIM card" renders text),
and the question is whether registering availability lets the draw path reach it.

**Decisive experiment.** Registered *exactly* the ~18 resource classes the idle draw
queries (`TRACE_RESAVAIL`: `0x22 0x25 0x26 0x27 0x2a 0x2b 0x30 0x31 0x3a 0x3c 0x3d
0x44 0x4a 0x4c 0x5c 0x5d 0x5e 0x78`) via the `MODEL_RES_ENABLE` config blob (not
all-`0xff`), and forced `display_idle`. **Result: the display goes blank — even
"Insert SIM card" stops rendering** (`run_content`: all `o000`; `resavail` confirms
`0x5c/0x5d/0x5e` became AVAIL; the early "Soft reset" is the normal boot reset, not
a crash from this).

**Why: the content classes are unbacked.** The split is clean:
- The **ROM-def-table (`0x2e0a50`) classes** — `0x4c/0x4f/0x50/0x52/0x56` — are
  ROM-backed; registering just these (the sparse-5 default) keeps the display alive
  (renders "Insert SIM card").
- The **other ~13 idle-content classes** have **no ROM resource definition and no
  runtime provider** on our boot. Marking them *available* makes the render path try
  to acquire their (nonexistent) provider objects, and the render can't complete →
  blank. (Same failure mode as all-`0xff`, now localised to the non-def classes.)

**Verdict.** The idle content pipeline is **not digitally fakeable by flipping
availability bits.** Availability is a *promise* that a provider object exists; the
provider objects (the fonts/icons/window widgets the idle layout draws) are created
by the display / font / window subsystems during a full coherent bring-up — the same
coherent-boot state wall as everything above the SIM. Providing them would mean
faithfully running those subsystems (or hand-constructing every resource object),
not a bitmap poke. So the resource-content pipeline joins the network as a wall with
no cheap digital shortcut — for a different reason (needs real subsystem-created
providers, vs the network's RF/DSP hardware). **Boot-to-"Insert SIM card" stands as
the honest, complete digital milestone.** (Probe harness `EXPERIMENT_IDLE_DRAW` +
the 18-class blob were used to establish this and then retired; recoverable from git.)

## SIM-present transition: recognized present, but the SIM state machine never settles (2026-07)

Goal: get past "Insert SIM card" into the interactive MMI state. `MODEL_SIM_CARD`
clears the no-SIM reject, so the question was whether the SIM is *accepted into the
interactive state* or just has the reject flag cleared. Traced the decision chain
(`TRACE_SIMDEC`, reverted/git-recoverable).

**The SIM IS recognized present.** The read-complete decision `0x27ea88` reads the
no-SIM flag `[0x111c64]`: `!=0` → posts status `0x1f` ("Insert SIM card") via
`0x27e240`; `==0` → calls the **SIM-present handler `0x27dfc4`** and posts events
`0xe8` (immediate) + `0xea` (delayed). On our boot `[0x111c64]==0` **every time**, so
`0x27ea88` always takes the **SIM-present** path — the no-SIM branch never fires. And
`0x27dfc4` proceeds (its gates `[0x111c9d]==0` and `[0x10a8e3]==1` are met after
t≈0.9), setting the SIM-present flags. So the firmware does recognise the SIM.

**But the SIM state machine never settles.** `0x27ea88` is called only from
`0x27f06e` (the SIM main loop), and the surrounding logic (`0x27ea20`) reads a state
byte `[0x10a8e2]`; while it is `1` it **re-arms a retry timer (event `0xe9`, ~34 ms)
and blocks on recv**. On our boot `[0x10a8e2]` stays `1` — the SIM subsystem cycles
this "establishing/retry" poll **continuously** (0x27ea88 fires ~every 34 ms, 300+×
over the run) instead of reaching a stable "ready/done" state. So the SIM is *present*
but never *finished*: the state machine loops in ATR/reset-retry rather than settling,
so the clean "SIM ready → MMI interactive" handoff that would clear the "Insert SIM
card" screen never completes.

**The gap, precisely:** `MODEL_SIM_CARD` drives the read conversation (ATR→PPS→EF) and
gets the SIM recognised as present, but it does not drive the SIM control state
`[0x10a8e2]` (SIM struct `0x10a8dc+6`) out of the retry/establishing state (`1`) into
its settled/ready value. Until that settles, the SIM subsystem keeps polling and the
MMI never transitions. Next: find what advances `[0x10a8e2]` past `1` (which SIM-task
code/message marks the SIM fully ready) and whether `MODEL_SIM_CARD` can deliver it —
the analogue, one layer up, of the ATR/PPS/EF steps it already models. Knob (reverted):
`TRACE_SIMDEC`.

## Correction + refinement: the SIM is present AND stable; the wall is the post-SIM render (2026-07)

The prior section guessed the SIM state machine "loops in retry" — that was wrong.
Tracing the SIM control struct directly (`TRACE_SIMST` on `0x10a8dc`): the state byte
`[0x10a8e2]` (+6) is **0** and `[+9]` is **0** the whole run (RESET-START `0x27e024`
runs once early, ~t=0.86). So the SIM is **recognised present and stable** — the
`0x27ea88` poll (~every 34 ms) simply re-confirms SIM-present against an unchanging
state; it is not thrashing.

And the SIM **does report its progress upward**. The status reporter `0x27e240` is
called with `0x1f` (no SIM), `0x15` (ATR), `0x16` (detected/PPS). It dispatches per
status to handlers (`0x16 → 0x27e394`) that build and **post messages onward** — e.g.
the `0x16` handler allocates a `0x120c` message and posts it to task 20 via `0x26a354`
(gated on the SIM response buffer `[0x10dddc]` leading byte `== 0x91`). So "SIM
detected" genuinely propagates into the message system.

**So the SIM fake is, at the message layer, essentially complete:** the firmware
recognises the card as present, holds a stable SIM state, and reports/propagates the
detection status. The reason the screen doesn't visibly advance past "Insert SIM card"
is **not** the SIM — it is **downstream, in the post-SIM UI render**: composing the
next screen (idle / PIN / menu chrome) needs the display **resource-content pipeline**
(the ~18 resource-provider classes), which we proved earlier requires the full coherent
display-subsystem bring-up and cannot be faked by flipping availability bits (that
blanks/crashes the display).

**Net:** every thread — SIM acceptance, the MMI state, the input path — now terminates
at the same wall: the **display resource-provider graph** that a real boot constructs
and our reconstructed boot does not. That is the one genuine remaining blocker to a
visibly-interactive screen, and it is a large, coherent-bring-up problem (or a source of
the real provisioned resource config), not a message injection. Possible actionable
thread if pursued: the `0x16` handler's gate on `[0x10dddc]==0x91` — check whether
`MODEL_SIM_CARD`'s response words match what the status-propagation path expects (it may
return `SW=9000` where the path wants a `0x91xx` proactive/status byte). Knob (reverted):
`TRACE_SIMST`/`TRACE_SIMDEC`.
