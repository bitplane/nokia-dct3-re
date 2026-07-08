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
