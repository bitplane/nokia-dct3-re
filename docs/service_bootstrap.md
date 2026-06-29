# Service-startup bootstrap — why a blank 3210 shows CONTACT SERVICE

CONTACT SERVICE is the firmware's **correct** response to a blank/un-provisioned 3210: the
service layer can't bring up, so the contact-service watchdog halts. This doc is the current
map of that chain and the models that emulate a provisioned service environment.

> This is a **current-knowledge** reference, not a journal. The full investigation history
> (including conclusions later corrected — the `ack` red herring, "NOT the EEPROM", etc.) lives
> in git history; a terse summary is in the Appendix. Where older commits/wording conflict with
> this file, this file wins.

## Status & model stack

Four faithful models clear the entire **bit-6 / service-channel** gate; **one gate remains** —
the node-`0x18` service responder. All models are opt-in, preserve the regression oracle
(`make verify` → frame `d8a9a7`), and leave the default boot byte-identical.

| model (env) | what it emulates | gate it clears |
|---|---|---|
| `NOKI3210_MODEL_DSP_SERVICE` | DSP lower-service handshake: drains pending counter `[0x100e4]`, raises IRQ 4 | `service_ready` |
| `NOKI3210_MODEL_CCONT_PRESENT` | CCONT reg `0xe` bit 0 = present/status | service-channel idx6 |
| `EEPROM_PROFILE=selftest` (overlay) | EEPROM config checksum (`0x244`) + tune/security checksum (`0x11c`) | EEPROM config gate + service-channel idx18 |
| **remaining** | **node-`0x18` service responder** (registration + `0x5f00`→cmd-`0x05` reply) | channel-open + contact-service *completion* |

With all three models on, the service-channel array is clean and bit 6 survives the
contact-service init; enabling the channel additionally stops the D9 watchdog timing out. The
phone still **shows** CONTACT SERVICE until the contact-service genuinely *completes* (the async
cmd-`0x05` response), which needs the responder.

## How CONTACT SERVICE works (the bit-6 gate)

### The halt and its linchpin

CONTACT SERVICE is the **D9 watchdog timeout**: `0x237b2e` increments counter `0x11fed6` each
poll; at `0x0f` it calls `0x2b4dda` (the halt). The counter resets only while ack `0x11fedb != 0`
— but **the ack is a red herring**: it is *never written non-zero anywhere reachable* (only
zeroed at init `0x23471e`). The real control is whether the watchdog **arms** at all, gated by
**service-present bit 6** of the byte at `0x11fed0`:

```
batch-2 resume gate 0x29bafc reads bit 6 of 0x11fed0 (service-present):
  bit 6 SET   -> returns 1: task 0x14 resumes, NO event 0x19, NO watchdog, NO CONTACT SERVICE
  bit 6 CLEAR -> posts event 0x19 (0x29bb1a) -> watchdog 0x237b2e self-perpetuates -> CONTACT SERVICE
```

So **keeping bit 6 set** is the goal; then the watchdog never arms and the ack never matters.

### The four ways bit 6 gets cleared (and the model for each)

The contact-service init `0x2346b2` sets bit 6 (`|0x40` at `0x234758`), then clears it
(`&0xbf`) if **any** of these fail. Plus one later re-clear in the PM path:

**(a) `service_ready 0x110c2c != 1`** — checked via `0x2a8fec` at `0x2347a4`, clear at `0x2347b2`.
The setter `0x291068` writes `[0x110c2c]=1` iff the DSP-shared **pending counter `[0x100e4]==0`**;
it runs only from the IRQ handler `0x2af3ca` (dispatched from `0x2ac794`) on **MAD2 IRQ line 4**,
the DSP service-completion interrupt. The DSP is unemulated and `assert_irq(4)` never happens, so
the setter never runs. Nuance: the ready byte is also reset at the top of the startup function
(`0x290b54` at `0x2a90d6`) each phase, so it must be (re)set *within* the phase that reads it —
i.e. the DSP must signal continuously, not once.
→ **`MODEL_DSP_SERVICE`**: when the MCU writes a non-zero pending count to `[0x100e4]` (pc
`0x290c98` writes `0x0002`), after a short delay drain it to 0 and `assert_irq(4)`, then re-arm at
a service-tick rate (a running DSP raises a periodic per-frame interrupt).

**(b) EEPROM config checksum** — `0x234810` compares `sum16(EEPROM[0x120..0x243])` against the
stored halfword at `EEPROM[0x244..0x245]`; mismatch clears bit 6 at `0x234832`. The firmware
computes `0x1ee1` = `sum16 (0x20df)` minus a correction `EEPROM[0x154]+[0x155]` (`0x2978c0`), and
reads `0x244/0x245` **big-endian-in-word**.
→ **`EEPROM_PROFILE=selftest`** overlay returns `0x244=0x1e`, `0x245=0xe1` → `ldrh=0x1ee1`, match.

**(c) service-channel status array** — a loop `0x23487e..0x2348a2` walks 24 bytes at `0x11fc60`;
bit 6 is cleared (`0x234896`) if any entry `[0x11fc60+i]` (i≠11) is not `0x00/0xfe/0xff`. Two are
dirty on a blank phone, each gated by one availability check (clean = `0x00`, else `0xfd`):

- **idx6 `[0x11fc66]`** ← `0x295ebe: bl 0x2afb44` (`ccont_reg_read`, arg `0x9001` = index `0x10`,
  mask `0x01`). The cached shadow byte `[0x11238c]` is `0x70`, so the helper does a **live CCONT
  serial read** (cmd `0x74` → CCONT register `0xe`) and returns **bit 0**. The firmware's own CCONT
  IRQ dispatcher `0x2b08c6` masks bits 0–2 off (`and #0xf8`), so bit 0 is a **present/status bit,
  not an interrupt**. It is also a *fallback*: the primary service-6 detector `0x297540` (a config
  struct) is consulted first. A functional CCONT reports bit 0 set on any phone; the emulation
  never did.
  → **`MODEL_CCONT_PRESENT`**: report CCONT reg `0xe` bit 0 set (read-time only; the dispatcher
  ignores it, so it doesn't perturb the IRQ latch).

- **idx18 `[0x11fc72]`** ← `0x295ea4: bl 0x264c56`, which checks `sum16(EEPROM_cache[0x00..0x11b])
  == 32-bit big-endian word[0x11c]` (sum16 = `0x2a41d0`). Computed `0x1ae4` (exactly `0x11c × 0xff`
  — the erased region) vs stored `0xffff` (virgin EEPROM).
  → **`EEPROM_PROFILE=selftest`** supplies `0x11c..0x11f = 00 00 1a e4` (outside the summed range,
  so the sum is unchanged). The init re-checks at `0x234796` and stamps `0x12` on failure.

**(d) the PM-read re-clear `0x237b04`** — even with (a)–(c) satisfied (bit 6 builds to `0xcc` and
survives the init), it is cleared again at `0x237b04` (`lr=0x2b13b0`, the PM service-read validity
return) when the `0x5f00` read is **dropped** (channel disabled). Enabling the service channel
removes this clear (and the watchdog timeout) — but completion still needs the response. This is
the remaining gate, below.

## The remaining gate: the node-`0x18` service responder

The contact-service reads logical address **`0x5f00`** (count 2) from destination **node `0x18`**
(`[0x11fee5]`) every ~9 ms (the D9 watchdog tick). Two things are missing, and **both reduce to
one problem — modelling node `0x18`'s responses**:

**1. The channel is never opened.** The validity check `service_channel_lookup_2b12b4` requires the
master enable `0x11fee4 != 0` **and** the address registered (ROM bit-table `0x2e2f5c` gated by RAM
mask `0x11ff08`). `0x11fee4` is only ever *reset* (`service_channel_reset_2b13c0`), never set, so
reads are dropped (`0x2b13a2` returns without transmitting). The only writer of `0x11fee4` is the
channel-open `0x2b140a` (`strb r1,[0x11fee4]`; if `r1 != 0` it also copies the registration block).
Its four callers (`0x2366f6`, `0x23672c`, `0x236e6c`, `0x236f10`) are **all contact-service
message-command handlers** — cases in the dispatch switch at `0x2377fc` and inside the response
dispatcher `0x236dc6`. They process a *received message struct* (`0x2366c8` checksums its payload
`[r0+9..+0x40]`). So **channel-open is response-driven, not a local write**: a real phone opens the
channel by receiving a registration message from node `0x18`.

**2. The contact-service never *completes*.** The healthy completion is the async response
dispatcher **`0x236dc6`** with command **`0x05`** (`0x236efe` maps result `5` → substate 5,
HEALTHY). `0x236dc6` has **no static references** (computed dispatch only) — it runs solely when a
correctly-formatted **response message** is received and routed. The request is on the **internal
message bus** (frame starts `0x00`, not the MBUS-serial `0x1f`), so the response must be an injected
internal scheduler message. The *synchronous* `0x5f00` read (D9 watchdog dispatch `0x237994`,
`value-0xd9` jump table) only steers watchdog sub-handlers, never completion.

**Proven (with the models on):** forcing the channel enable (`EXPERIMENT_FORCE_SVC_CHANNEL`) makes
the reads transmit and **stops the D9 watchdog timing out** (`D9TIMEOUT` 1→0, the `0x237b04` clear
gone) — but the frame stays `d8a9a7`: watchdog-satisfied ≠ completed. No local provider answers
(`svc_response` count 0 in every configuration); node `0x18` is genuinely absent.

**The build:** a **node-`0x18` service-message responder** — post the response message(s) the node
would send. The RE is now complete; what remains is the injection engineering.

**Response message spec (fully traced).** The contact-service task loop `0x237bb4` calls
`recv 0x26a458` → `r4` = message, then dispatches on **`[msg+3]`**; `[msg+3]==0x40` →
`0x237c70` → the sub-dispatcher **`0x237400`**, which dispatches on **`[msg+8]`**; `[msg+8]==0x64`
→ `0x23785e` → **`0x236dc4([msg+9])`**; `[msg+9]==0x05` → result 5 → **substate 5 (HEALTHY)**. So a
message `{[3]=0x40, [8]=0x64, [9]=0x05}` delivered to the contact-service task **completes it**. (The
registration message that opens the channel is the same mechanism — a different `[msg+8]` command
routing to `0x23670c`/`0x2366c8`, which checksums its payload `[msg+9..+0x40]`.)

**Injection mechanism (mapped).** Messages are per-task: a TCB (stride `0x1c`) holds a ring of
message **pointers** at `[TCB+0xc]` indexed by tail `[TCB+0x10]`. `alloc = 0x26afe0(size)`;
`post = sched_post_task_message_26a204(task, msg)` (ring write at `0x26a2a2` + wake); `recv =
0x26a458`. **Crucially, the contact-service frees the message after dispatch** (`0x26abf8(msg)` at
`0x237c8e`), so an injected message must be a *real pool buffer*, not scratch RAM.

**Therefore the faithful injection** is to drive the firmware's own machinery: allocate a real
message (`0x26afe0`), fill `{[3]=0x40,[8]=0x64,[9]=0x05}`, and post it to the contact-service task
(`0x26a204`) — e.g. by trampolining those calls from a driver hook at a safe scheduler point. That is
the remaining build step; `EXPERIMENT_FORCE_SVC_CHANNEL` remains a diagnostic stand-in for the
*enable* only.

## Beyond the gate (the "limp" frontier — what comes after)

Running the boot *as-if-provisioned* with the diagnostic forces (`EXPERIMENT_DSP_IRQ4
EXPERIMENT_FORCE_ACK`) clears the watchdog and advances the mode `0001 → 000d`, where it **holds**:
the LCD cycles a white fill (`94a2dc`) / black fill (`4aab13`) display-init pattern, the
contact-service substate `[0x11feda]` stays `0x03` (watchdog-active, never healthy), and the
readiness loop `0x2a92fc` is never reached. PC sampler hot spots in this steady state: `memset`
`0x2b65e4` (the fills), sum16 `0x2a41d0`, a render routine `0x25exxx` (4-bit type field at
`0x25e682`), service-startup `0x290a94`. Task 0x14 (batch-2) also needs the startup **phase byte
`0x112449` ∈ {0,2}** (currently `01`). These are the threads *past* the responder gate — the forces
prove the root-cause map but only let the boot "limp"; a genuine boot needs the responder (and,
later, real calibration data / readiness predicates).

## Reference

### Key addresses

| addr | role |
|---|---|
| `0x237b2e` | D9 watchdog (counter `0x11fed6`, halt at `0x2b4dda`) |
| `0x11fed0` | service-present byte; **bit 6** is the linchpin |
| `0x11fedb` | ack (red herring — never written non-zero) |
| `0x2346b2` | contact-service init: sets bit 6, then the clear checks |
| `0x2347a4`/`0x2347b2` | service_ready check / clear-bit-6 |
| `0x234810`/`0x234832` | EEPROM config checksum check / clear-bit-6 |
| `0x23487e..0x2348a2` (`0x234896`) | service-channel array loop / clear-bit-6 |
| `0x237b04` | PM-read re-clear of bit 6 (dropped `0x5f00` read) |
| `0x110c2c` | `service_ready` byte (setter `0x291068`, needs `[0x100e4]==0`) |
| `0x2af3ca` | IRQ handler; IRQ line 4 → `0x291068` |
| `0x100e4` | DSP-shared pending counter (MCU writes `0x0002` at pc `0x290c98`) |
| `0x11fc60` | 24-byte service-channel status array (idx6=`0x11fc66`, idx18=`0x11fc72`) |
| `0x2afb44` | `ccont_reg_read` (idx6: arg `0x9001` → CCONT reg `0xe` bit 0) |
| `0x2b08c6` | CCONT IRQ dispatcher (masks bits 0–2; proves bit 0 = status) |
| `0x264c56` / `0x2a41d0` | idx18 EEPROM checksum / the `sum16` primitive |
| `0x244..0x245`, `0x11c..0x11f` | EEPROM stored checksums (config / tune+security) |
| `0x5f00` | PM logical address the contact-service reads (cmd) |
| `0x11fee4` | service-channel master enable (writer `0x2b140a`, reset `0x2b13c0`) |
| `0x2b12b4`/`0x2b13a2` | channel validity / request-if-enabled |
| `0x236dc6` | async response dispatcher (cmd `0x05` → HEALTHY; computed-dispatch only) |
| `0x237994` | D9 watchdog sync `0x5f00` dispatch (`value-0xd9` jump table) |

### Frames

**MBUS D0 startup query** (task07→task08, reg `0x1a` byte stream), `1f`-framed serial:
```
1f ff 00 d0 00 01 01 01 31
│  │  │  │  └──┴─ len=0x0001 │  │  └ XOR checksum (1f^ff^00^d0^00^01^01^01 = 0x31)
│  │  │  └ cmd 0xD0          │  └ seq
│  │  └ src 0x00 (phone)     └ payload[0]=0x01
│  └ dest 0xff (test box)
└ frame start
```
Delivery path (now superseded by `MODEL_DSP_SERVICE`, kept for reference): the RX state machine
`0x2aae76`/`0x2b052e`/`0x2aaf44` assembles the reply and posts it to the task-08 frame handler
`0x283d6e`, which advances the lower-service state. That thread fed `service_ready`; the model
now supplies `service_ready` directly, so the D0 reply itself is no longer on the critical path.

**`0x5f00` service request** (internal message bus, captured at `0x2b0482`):
```
00 [node] 00 00 00 0a 00 01 5f 00 [seq][seq] [ctr] 02 00 [d9/da] ...
   node@+1 (from 0x11fee4)         addr 0x5f00@+8/9   count@+0xd   watchdog selector
```

**DSP shared RAM** (`0x10000–0x10fff`; DSP unemulated, `dsp_ram_r` stubbed):
`0xe4`=pending counter (`0x0002`), `0xa4/a6`=status/version, `0xda/e2`=channel counts/ptrs,
`0xe0`=busy flag, `0xfe/0x100`=ready flags, DSPIF API reg at `0x30000` (stub).

### Knobs & probes

- **Models (faithful, opt-in):** `MODEL_DSP_SERVICE` (+`_DELAY_MS`/`_TICK_MS`), `MODEL_CCONT_PRESENT`,
  `EEPROM_PROFILE=selftest`.
- **Diagnostic forces (not models):** `EXPERIMENT_FORCE_SVC_CHANNEL`, `EXPERIMENT_DSP_IRQ4`,
  `EXPERIMENT_FORCE_ACK`, `EXPERIMENT_CLEAN_SVCCHAN`, `EXPERIMENT_RESUME_TASK14`.
- **Traces/probes (`TRACE_CONTACT_COMMIT`, `TRACE_PM`, `TRACE_DSP`, `TRACE_CCONT_READ`, `TRACE_BUS`):**
  `bit6_clear`, `bit6_svcready_check`, `svcchan_write`, `idx6_ccont_check`, `ccont_read_tbl`,
  `ccont_shadow_write`, `ccont_reg_read`/`ccont_path`, `idx18_cksum`, `chan_open`/`chan_open_gate`,
  `pm_valid`/`pm_request`, `cs_write`/`cs_disp`/`contact_commit`, `svc_disp`, `dspwr-pending`.

## Bus-level view (actors, timeline, dead-end)

Reconstructed by wiretapping the scheduler message/event bus (`TRACE_BUS=1`): `post_task_message`
(`0x26a204`/`0x26a354`), `event_post` (`0x2697aa`), `event2` (`0x2698e4`), `resume` (`0x269c6e`),
`recv` (`0x26a458`).

| task | role | runs? |
|---|---|---|
| `0x00` | main startup — resumes others, mode-`0x000d` readiness loop | yes |
| `0x01` | service/event loop — spins event `0x03` | yes |
| `0x02` | contact-service — D9 watchdog (event `0x19`) → CONTACT SERVICE | yes |
| `0x07` | lower-service / MBUS — sends the D0 startup frame | yes |
| `0x05` | service task — receives 3 messages, **never resumed** | no |
| `0x08` | lower-service frame processor — receives the D0 frame, **never resumed** | no |

`0x00/01/02/07` are the core batch (resumed at mode 1); `0x05/08/0x14`… are the extended batch,
deferred by the resume gate. **The dead-end:** messages to `0x05`/`0x08` pile up undrained (they're
deferred), so the service handshake never completes → service never reports ready → task 0 readiness
loop stalls → task 2's D9 watchdog free-runs to timeout → CONTACT SERVICE. Abridged timeline: mode
`0001` resumes 00/07/01, sends the D0 frame (t≈0.251); mode flips to `000d` (t≈0.332); task 02's D9
watchdog ticks every ~9 ms from t≈0.405 and fires ~t≈0.54.

## Appendix: investigation timeline (terse)

The chain was found by chasing the symptom inward, with several mid-course corrections (full detail
in git history):

1. **Symptom → watchdog.** CONTACT SERVICE = D9 watchdog timeout. Initially read as "ack `0x11fedb`
   never set" — **later corrected**: the ack is a red herring (no writer); the real gate is bit 6.
2. **bit 6 linchpin.** The watchdog only arms when service-present bit 6 is clear; the contact-service
   init clears it on three checks (service_ready, EEPROM config checksum, service-channel array).
3. **service_ready → DSP.** Traced to MAD2 IRQ line 4 / DSP-shared RAM; the unemulated DSP never
   raises it. Modelled (`MODEL_DSP_SERVICE`), replacing the `EXPERIMENT_DSP_IRQ4` force. Found it must
   *recur* (the ready byte is reset each phase).
4. **EEPROM is a real gate** (correcting an earlier "NOT the EEPROM"): the config checksum (`0x244`)
   and the tune/security checksum (`0x11c`, idx18) both clear bit 6. Modelled in the `selftest` overlay.
5. **Service-channel fan-out = 2.** The 24-entry array's two dirty entries resolved to known
   subsystems — CCONT reg `0xe` bit 0 (idx6, `MODEL_CCONT_PRESENT`) and the idx18 EEPROM checksum —
   not an open-ended explosion.
6. **Remaining gate = the responder.** With the array clean, bit 6 survives the init; enabling the
   channel removes the watchdog halt; but completion needs an async cmd-`0x05` response. Channel-open
   turned out to be response-driven too, so both collapse into one build: model node `0x18`'s service
   messages.

See also `eeprom_analysis.md` (block/checksum layout), `static_branch_map.md` (the resume-gate
branches), `driver_structure.md` (how the models/probes live in the driver).
