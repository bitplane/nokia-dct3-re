# Scheduler event-delivery contract

This document records the reusable 3210 v6.00 scheduler/event grammar recovered
while investigating startup mode `0x000d`. Historical event injection and boot
marching experiments are retained only in Git history.

## Two delivery channels

Firmware delivers work through distinct mechanisms:

- task messages are pointers posted through `sched_post_task_message_26a204` and
  received through `0x26a458`;
- numeric events use immediate or delayed event primitives and are normalized by
  the task-1 receive wrapper `0x26ff14`.

These namespaces can carry the same apparent low-byte value without denoting the
same object. Conclusions must identify the primitive, destination, and decoding
layer rather than matching a number alone.

## Delayed-event recoding

The table at `0x002d71a8` maps numeric event `k` to encoded value `0xc0 + k`.
The delayed publisher `0x2697aa` queues that encoded value. Task 1's wrapper at
`0x26ff14` recognizes `0xc0..0xdf`, decodes it, and may consume selected values
internally before returning an event to the startup dispatcher.

This matters for event `0x15`: its delayed form becomes `0xd5`, and the wrapper
loops rather than returning raw `0x15` to mode `0x000d`. A trace that sees the
publisher is therefore not proof that the startup state machine receives the
corresponding raw event.

## Mode-0x000d contract

Handler `0x270e22` accumulates four raw startup events in byte `0x112399`:

| Event | Flag bit |
| --- | --- |
| `0x14` | `0x01` |
| `0x16` | `0x02` |
| `0x15` | `0x04` |
| `0x17` | `0x08` |

The branch advances only when the low nibble is `0x0f` and the low nibble of
CCONT state `0x11ff6c` is `6`. Current CCONT cold-boot status and IRQ masking
produce this sequence organically; the earlier missing-event diagnosis was an
emulation artifact.

## Raw-0x15 producer result

Contact command dispatcher `0x237400` selects on `message[+8]`. Command `0x65`
reaches `0x236bac`, which can publish startup-status events from bits in
`message[+9]`. Static and runtime work established two important limits:

1. the contact command namespace is distinct from the delayed scheduler-event
   namespace;
2. the candidate `0x15` branch inside `0x236bac` is gated by
   `0x2a674c(2)`, whose even-argument path returns 1 and skips that emit.

Consequently this contact path is not the ordinary raw-`0x15` producer needed
by startup. It remains useful as a command-family map, but not as a reason to
inject command `0x65`.

## Message ownership

Task-message queues store pointers to pool allocations. A receiver may free the
message after dispatch; scratch RAM or a borrowed pointer is never a faithful
substitute. Any external peer model must enter through its hardware or transport
boundary and let the firmware allocate, route, and release internal objects.

## Receive-wait and suspend states

Task scheduler records are `0x10` bytes at `0x1093bc`; task mailbox descriptors
are `0x1c` bytes at `0x101484`. An empty receive through `0x26a458` leaves the
task in scheduler state `4`. Posting through `0x26a204` queues the message and
wakes a destination in that state. Scheduler state `5` is suspended and is not
woken by an ordinary message post.

The APIs at `0x269bf4` and `0x269c6e` suspend and resume a named task,
respectively. Routine `0x2795e6` is a bulk suspend of tasks 10--17, not a resume
group. This distinction explained why an otherwise valid `0x1587` object could
remain queued when the external-service prototype requested lifecycle state 5.

## Tooling caveat

Instruction-fetch hooks fire at actual fetched branch targets. A hook placed at
an address in the middle of a sequential Thumb basic block can report zero even
when execution passes through that instruction. Negative runtime conclusions
must use a real branch target, an MMIO/RAM watch, or static control-flow proof.

## Key addresses

| Address | Role |
| --- | --- |
| `0x2695f4` | immediate numeric-event publisher |
| `0x2697aa` | delayed/recode event publisher |
| `0x26a204` | task-message post |
| `0x26a458` | task-message receive |
| `0x26ff14` | task-1 receive/event normalization wrapper |
| `0x270e22` | startup mode-`0x000d` handler |
| `0x002d71a8` | event recode table |
| `0x237400` | contact command dispatcher |
| `0x236bac` | command-`0x65` startup-status handler |

Current task-1/report-7 and keypad behavior is documented in
`mmi_settlement.md`; current contact ownership is documented in
`external_service_topology.md`.
