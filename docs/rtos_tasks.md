# RTOS task registry

This is the authoritative task-identity table for the Nokia 3210 v6.00 ROM.
Task numbers are firmware mailbox/scheduler identities.  The aliases below are
project terminology derived from observed behaviour; they are not recovered
Nokia names and must not be assumed to apply to another ROM.

Use `task N (alias)` in prose.  Keep `task N` in traces, machine-readable
topology, and cross-ROM comparisons.  A task alias describes the narrowest
proved responsibility of the task, not the subsystem that happened to expose
it first.

Confidence means:

- **Proved:** its receive loop and at least one substantial input/output path
  are mapped.
- **Partial:** some behaviour is mapped, but the task's overall ownership is
  not.
- **Unknown:** only the initialization entry or state storage is known.

## Nokia 3210 v6.00

The creation table is at `0x2d7090`, with 23 records of 12 bytes.  `init` is the
entry stored in that table; it is not necessarily the main receive dispatcher.

| Task | Init | Project alias | What is actually established | Confidence |
| ---: | ---: | --- | --- | --- |
| 0 | `0x2a92d2` | task activation supervisor | Participates in task creation/resume ordering. Complete receive ownership is not mapped. | Partial |
| 1 | `0x270170` | startup and power-lifecycle coordinator | Owns the startup mode word, readiness reports, power-key shutdown transition, and shared interactive-initialization tail. | Proved |
| 2 | `0x237bb4` | class-`0x40` service-command dispatcher | Receives framed class-`0x40` commands and applies channel-map, status, indexed-NV, and related service operations. | Proved |
| 3 | `0x2b18a0` | resource/work serializer | Queues and serializes mapped MCU-to-DSP work and is also reached by resource/display initialization. The complete dispatcher is not classified, so neither “DSP task” nor “display server” is an adequate whole-task name. | Partial |
| 4 | `0x2b3fb8` | DSP receive broker | Woken by FIQ0/timer activity; decodes DSP-to-MCU packet families and routes resulting messages. It is not a display helper. | Proved |
| 5 | `0x2af630` | MMI event/context engine | Consumes packed events, runs callback/descriptor state machines, and drives presentation/session actions. “VM” is a project description, not a recovered Nokia name. | Proved |
| 6 | `0x297fc4` | UI window/context manager | Consumes window setup/control traffic and owns mapped idle/editor window-selection state. | Proved |
| 7 | `0x2a5890` | external-service transport adapter | Carries framed service traffic between task 2 and the lower external transport. Its broader ownership is not closed. | Partial |
| 8 | `0x283ce8` | lower-service/DSP packet handler | Participates in the D0 discovery exchange and handles DSP RX packet families `0x85`/`0x8d`. Do not generalize this alias across products. | Partial |
| 9 | `0x28e164` | unknown task 9 | Initialization entry and state near `0x1123ac`/`0x1123b0` are known; semantic ownership is not. | Unknown |
| 10 | `0x21bf60` | controller-status coordinator | Handles mapped statuses including `0x1391`, `0x1392`, and `0x03e9`, constructs lower-layer work, and reports readiness. “Radio” alone is too broad. | Partial |
| 11 | `0x2159c4` | unknown task 11 | Posts readiness report `0x0b` and receives some DSP-broker families; that does not identify the subsystem. | Partial |
| 12 | `0x273ea0` | unknown task 12 | Posts readiness report `0x0c`; no defensible subsystem name is established. | Partial |
| 13 | `0x23ebd0` | segmented-object consumer | Handles a mapped segmented transaction and posts task-16 status `0x05eb`; subsystem ownership remains unresolved. | Partial |
| 14 | `0x248318` | object/session decoder | Decodes task-15 translated objects and session requests. It is not proved to be a raw DSP or radio task. | Partial |
| 15 | `0x20a8a8` | object parser/translator | Parses service objects and translates successful results into task-14 statuses. | Partial |
| 16 | `0x24f5a0` | completion/status relay | Relays mapped completion/status traffic between tasks 13, 10, and 17. Broader ownership is unresolved. | Partial |
| 17 | `0x22391c` | registration/control coordinator | Participates in the mapped registration transaction and task-10 completion chain; “radio init” describes only that observed path. | Partial |
| 18 | `0x285c14` | lower-service readiness task | Posts readiness report `0x12` in the resumed application-task group. Broader ownership is unresolved. | Partial |
| 19 | `0x21de4c` | battery/power state machine | Consumes battery-classification and pack-characterisation events and owns mapped shutdown-level reporting. | Proved |
| 20 | `0x208134` | SIM application/session router | Owns the mapped SIM registration, card-presence monitoring, and proactive-command routing paths above task 21. | Proved |
| 21 | `0x27eae0` | SIM transaction manager | Builds card requests, consumes SIMI/card replies, and drives SIM activation. It is not the battery task. | Proved |
| 22 | `0x2b6548` | DSP interface control task | Owns the mapped DSP-interface mailbox/control decoder. This is distinct from task 3 TX and task 4 RX packet workers. | Proved |

## Naming cautions

### Service startup

Task 2 is the class-`0x40` service-command dispatcher. Task 7 is the
external-service transport adapter. The visible startup fault screen is an
outcome of an incomplete service session, not a task or protocol name.

### Caller versus owner

A helper that posts a report to task 1 is not thereby part of task 1. The old
Ghidra prefix `task1_service_*` on the `0x21bfxx..0x2209xx` controller/battery
state machinery made this mistake and has been replaced by the neutral
`controller_service_*` prefix. Apply the same rule to future symbols: name the
owner only when the executing task context is established; otherwise name the
operation and destination separately.

### Readiness reports

A task posting startup report `0x0b..0x15` proves that it participates in the
readiness checklist. The report number does not name the task or establish its
primary subsystem. Earlier tables incorrectly promoted these reports into task
roles; the registry records them only as observed outputs.

### Cross-ROM use

Task numbers and table positions are product/ROM-specific. Cross-ROM matching
must use entry-point signatures, message contracts, and hardware interactions,
then record the other ROM's task number separately. Never assume that `task 8`
on another DCT3 phone is the same component as 3210 v6.00 task 8.
