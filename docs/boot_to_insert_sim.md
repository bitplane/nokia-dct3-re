# Boot to "Insert SIM card"

> **Current status (2026-07-11): event `0x15` path restored.** The cleaned
> driver initially remained in startup mode `0x000d` because its service peer
> completed the channel-empty request too late for the firmware's bounded poll.
> A one-microsecond asynchronous response restores the organic extended-task
> resume and checklist path. See `structural_regression.md` for acceptance
> status.

The headline result beyond CONTACT SERVICE: a blank, un-provisioned Nokia 3210
**boots all the way to the "Insert SIM card" screen** — the correct home state
for a DCT3 phone with no SIM inserted. This doc traces the whole chain from the
CONTACT SERVICE clear to that screen, the walls hit along the way, the single
root cause that gated everything, and the one opt-in model that unblocks it.

All addresses are for the pinned `3210f600a` image. The system is **big-endian
ARM7** (`fw_byte`: even address = high byte, odd = low byte) — relevant when
reading byte offsets out of 16-bit RAM words. Disassembly is via
`tools/disrom.py` on the swap16 image; instruction-fetch hooks fire only at
branch/call targets.

## Boot config

Reaching "Insert SIM card" needs the three CONTACT-SERVICE models plus the
service-channel drain:

```
NOKI3210_MODEL_DSP_SERVICE=1
NOKI3210_MODEL_CCONT_PRESENT=1
NOKI3210_MODEL_SVC_RESPONDER=1
NOKI3210_MODEL_SVC_CHANNEL_DRAIN=1     # the one new model documented here
```

The default (no models) still reproduces the CONTACT SERVICE oracle frame
`d8a9a7a58e587be8` byte-for-byte; every model is opt-in.

## The chain, end to end

```
CONTACT SERVICE cleared (DSP + CCONT + responder models)
        │
        ▼
startup mode 000d  ── waits for event 0x15 (bit 0x04 of flag [0x112399])
        │             0x15 = "all 11 subsystem tasks initialised" barrier
        │
        ▼
the 11 reporter tasks never run their init
        │             they are created dormant; the startup supervisor
        │             (startup_power_service_init_gate 0x2a8ff2) never
        │             resumes them (block 2 of its resume list)
        ▼
block-2 resume is gated on service readiness 0x29bafc
        │
        ▼
0x29bafc busy-waits on service-channel-busy bit [0x11fed1] bit2   ◄── ROOT CAUSE
        │             the bit is a real contact-service startup seed bit,
        │             set once and never cleared on our faked boot
        ▼
MODEL_SVC_CHANNEL_DRAIN clears the bit  ── models the service completion
        │
        ▼
supervisor block 2 resumes the 13 app tasks (10–18, 20, 21)
        │
        ▼
their init fills the 0x112280 checklist  11110000000 → 11111111111
        │
        ▼
event 0x15 posts  →  000d advances  →  boot proceeds
        │
        ▼
MMI renders  →  SIM reset fails (no card)  →  "Insert SIM card"
```

## Detail

### 1. The `000d` wall and the readiness barrier

Past CONTACT SERVICE the boot enters startup mode `000d` (handler `0x270e22`),
which accumulates a flag `[0x112399]` and only advances when all four sub-event
bits are set. Bit `0x04` comes from receiving event `0x15`. The genuine
producer of `0x15` is the CCONT thunk `0x2af208`, whose sole caller `0x2521cc`
fires only when an **11-byte checklist** at `0x112280[0..0xa]` is fully set.
Each byte is set by the init function of one of 13 tasks in the task
registration table at `0x2d7090`. So `0x15` is really *"every registered
subsystem task has run its init"* — a boot-wide readiness barrier.

In our boot **none of the 11 reporter tasks run**; the checklist stays
`00000000000` and `0x15` never posts.

### 2. The tasks are created but never resumed

Both scheduler-init walkers (`0x26a74c` builds TCBs, `0x26a7b0` registers
mailboxes) iterate all 23 table entries, so **all tasks are created**, dormant
(TCB state `[+0xd]=5`). A startup-supervisor activation function
`startup_power_service_init_gate` (`0x2a8ff2`) makes them ready in three
`resume` (`0x269c6e`) blocks. Our boot runs only block 1 (core tasks 1–9, 19,
22). Block 2 — which resumes exactly the missing reporter tasks
(`0xa`–`0x12`, `0x14`, `0x15`) — is gated at `0x2a9182`.

### 3. The gate: a service-readiness busy-wait

The block-2 gate calls `0x29bafc` (inside `init_29b700`). Every *other* gate
condition is already satisfied in our boot (phase `[0x110c2c]==1`,
`[0x11239c]==5≠3`, `[r7]==2`). But `0x29bafc` first calls
`service_channel_request_empty 0x2b13d4` (posts msg `0x2a62`), then
**busy-waits** at `0x29bb06`:

```
lsrs r0, #3 ; bhs 0x29bb06     ; loop while [0x11fed1] bit2 (0x04) is SET
```

i.e. it spins until the **service-channel-busy** bit clears. On a real phone
the service peer drains the channel and clears the bit. On our faked boot the
bit stays set, so the supervisor spins forever (~t=0.84) and never reaches the
block-2 resumes.

### 4. The bit is a real seed bit (faithfulness)

`TRACE_SVCBIT2` confirmed `[0x11fed1]` bit2 is set `0→1` exactly once, by real
firmware at `0x2347d0` inside `contact_startup_fault_seed_bits_234750` — it OR's
in `0x04` and posts a service message. It is **never cleared** in our boot
because `MODEL_SVC_RESPONDER` injects the node-`0x18` reply instead of running
the full service startup that would complete the operation and clear the seed
bit. So clearing the bit is a faithful model of that completion, consistent
with the responder approach — not an override of real hardware behaviour.

### 5. The fix cascades to a rendered screen

`MODEL_SVC_CHANNEL_DRAIN` schedules an asynchronous peer completion when
`0x29bafc` begins. Firmware then calls `0x2b13d4`, sets the busy bit and polls at
`0x29bb06`; the peer timer completes after one microsecond. Clearing it
synchronously at `0x29bafc` entry is too early because the request sets it
again. The whole chain
then fires end to end (each link verified in traces): block 2 resumes the app
tasks → their init fills the checklist → event `0x15` posts (first time ever) →
`000d` advances → the MMI comes alive and renders glyph content. The LCD shows
**"Insert SIM card"** in a bordered layout with the scrollbar and status icons.

## Why it stops at "Insert SIM card"

This is the *correct* end state for a SIM-less phone, not a stall. The operator-
idle home screen (clock / signal / operator name) requires a SIM. The idle flag
`[0x11f81b]` stays 0 by design without one, so `display_idle 0x2a255c` never
fires. Reaching operator-idle is a separate **SIM-emulation** project — see
`docs/sim_subsystem.md`.

## Diagnostics (opt-in knobs)

| Knob | What it shows |
|------|---------------|
| `MODEL_SVC_CHANNEL_DRAIN` | the fix: clear `[0x11fed1]` bit2 at the readiness gate |
| `TRACE_SVCREADY` | the block-2 gate conditions at `0x2a9182` |
| `TRACE_RESUME` | which of the 23 tasks get resumed (`0x269c6e`) |
| `TRACE_SWEEP15` | the `0x112280` checklist filling + `0x15` post |
| `TRACE_SVCBIT2` | writes to the service-busy bit (faithfulness check) |
| `TRACE_MMI` | the MMI loop / `display_idle` |
