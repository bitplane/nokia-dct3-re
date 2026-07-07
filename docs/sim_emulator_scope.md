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
   non-reject status → the idle flag `[0x11f81b]` can set → `display_idle`
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
5. **[LOW] Idle-flag gating.** `[0x11f81b]` (idle) may have gates beyond the SIM
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
| Idle flag | `0x11f81b` | gates `display_idle 0x2a255c` |

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

- Reach the read: 4 milestone models + `MODEL_SIM_ATR_MSG` + `SIM_ATR_HEX=3b1005` (TA1 non-special → `[+0xc]=1`, which enters the file-read phase directly). (The old `EXPERIMENT_SIM_CODE5` forcing probe and the refuted `EXPERIMENT_SIM_ACCEPT` were pruned as dead ends.)
- Conversation driver: `MODEL_SIM_RESPONDER`(`SIM_RESP_CODE=7`) + `MODEL_SIM_LOOP`(`SIM_LOOP_CODE=3`, `SIM_LOOP_SCRIPT=<cmds>`, `SIM_LOOP_DONE=<n>`). Caller-gate override lives at `0x27ee56`.
- Traces: `TRACE_SIMBUF` (`0x10deec`+`0x10dddc` reads), `TRACE_SIMSTATUS` (`0x27e240` posts), `TRACE_SIMPATH` (state-machine branch targets, cxx markers), `TRACE_SIM`/`TRACE_SIMTX` (UART/APDU).

### The open problem for the pass

The conversation driver completes the read but lands on no-SIM because `0x10dddc` never gets coherent per-command file responses. A faithful emulator must, driven by the phone's own commands, emit **consistent** GSM 11.11 responses into `0x10dddc` (info blocks + data + SWs) so the read state, `[0x111c64]`, and the struct fields all settle to "valid SIM" together — not by overriding dispatch codes. Start by tracing, on a *real*-style boot path, what a valid response into `0x10dddc` must contain for the parser `0x27e046` to advance the state without the no-SIM flag being set.
