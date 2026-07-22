# MMI settlement contract

This document records the validated Nokia 3210 v6.00 startup/UI contract.
Detailed rejected hypotheses live in `evidence/falsifications.json`; they are
not alternate boot recipes or active research directions.

## Accepted result

The coherent profile completes the modeled external-service session and
ordinary non-CPHS SIM initialization. Provisioned EEPROM identity data removes
the phone-lock prompt and firmware paints an idle frame with a `Menu` softkey.
Task 1 remains in mode `0x0004`, but the UI is fully interactive in that mode.

A delayed physical left-softkey press follows the ordinary firmware path:

```text
active-low keypad input
  -> MAD2 KBGPIO state and IRQ0
  -> ISR 0x2b3084
  -> task-1 event 0x0041
  -> internal 0x41/0x42/0x43 sequence
  -> matrix scan 0x2b2f90
  -> decoded key resource 0x19 via 0x2b4628
  -> packed press/release 0x40c8/0x40c9
  -> statuses 0x0598, 0x0578
  -> callback 0x02
  -> pipeline terminator 0x00dc
  -> Phone book menu
```

`make verify-mmi-menu` protects the coherent structural predicates and an exact
hash of all stable `Phone book`/`Select` frame pixels. Only the animated 20x12
menu-icon rectangle is excluded. The fixture injects no firmware state, task
message, callback result, or rendered pixels.

## Security editor

The unprovisioned profile legitimately presents the phone security editor.
Physical `12345` plus left softkey completes through status `0x0578`; verifier
`0x2ae704` transforms the five characters through `0x2ae4e8`, compares four
bytes with RAM `0x112460`, and returns one. Callback `0x47` consequently
publishes `0x05e1` in the observed accepted transaction. Incorrect input can
also publish `0x05e1` while re-presenting the editor, so the scalar status is
callback-local and acceptance is proved by the verifier result and subsequent
context state, not by assigning a global meaning to that number.

Later class-2/class-3 window notifications are
ordinary editor lifecycle. They are not display acknowledgements and do not
select the provisioned idle screen. Security editing is therefore an optional
phone-lock transaction, not a startup prerequisite.

## Task-1 mode semantics

Task 1 owns the startup state rooted at `0x1123ee`:

| field | address | accepted value/role |
| --- | --- | --- |
| pending event | `0x1123ee` | current scalar mailbox input |
| mode | `0x1123f0` | `0x0004` with interactive UI |
| readiness flags | `0x112399` | `0x0f` after mode `0x000d` |
| substate | `0x11239c` | diagnostic context |

The master dispatcher is `0x270c8e`. Both exits from mode `0x000d` compare the
current report with code `0x07`; a different report records mode `0x0004` or
`0x0007`, then both paths execute equivalent interactive-initialization tails.
Mode `0x0004` records a later power/shutdown continuation. It is not a blocked
pre-desktop state.

## Conditional UI paths

Several mapped paths were initially plausible startup entrances. Runtime and
complete table/caller censuses classify them instead as conditional firmware
lifecycles:

- Callback `0x01` can map global status `0x0367` to controller status `0x03e9`.
  A physical navigation key produces `0x0367` indirectly from the active UI
  context, but callback `0x01` is no longer selected at that point.
- Controller `0x03e9` reaches `0x256f68`, which publishes global
  `0x05e7(argument 1)`; callback `0x10` uses it for application/UI
  reinitialization, not ordinary cold boot.
- Task-6 constructor `0x2b1e44` sends class 1 with selector byte one. Task 6
  then arms `0x1116fd`, requests resource `0x4c22`, and posts `0x0547`.
  Exhaustive execution of task-5 dispatcher `0x28bddc` finds only status
  `0x0732` selecting that constructor; `0x0732` belongs to power/shutdown
  ordering.
- Periodic `0x00c8` traffic is task-1 context maintenance. Repeating `0x05a7`
  is the independent three-slot timer manager at `0x2b3222`. Neither is a
  retried idle-screen transaction.

No class-1 message, `0x0547`, callback selection, context state, timer result,
hardware event, or peer response should be synthesized from these paths.

## Report code 7

Report `0x07` is a conditional power/shutdown continuation, not an ordinary
boot-completion report. Wrapper `0x2af190` optionally mirrors resource `0x6a01`
and posts the report to task 1. Its v6.00 caller surface is closed:

| owner | caller | condition |
| --- | --- | --- |
| power/battery | `0x21e40c` | low-voltage shutdown |
| power/charger | `0x21f8de` | charging completed |
| callback/status | `0x27b3b6` | callback `0x5d` terminal completion |
| controller | `0x255c3c` | controller status `0x0795` |

A physical power-key action exercises the observed route: decoded key `0x0d`
reaches `0x0795`, posts report 7, changes task 1 to mode `0x000c`, and begins
service/display teardown. `make verify-power-lifecycle` additionally proves
that a short press stays in interactive mode `0x0004`, while a two-second hold
reaches mode `0x000c`, terminal event `0x0074`, clears SIM enable and blanks the
LCD through firmware-owned teardown. Healthy sibling boots reach standby without posting
their equivalent report. Code 7 must not be injected or treated as cold-boot
readiness.

Callback `0x5d` is the paired report-6/7 status dispatcher. Input `0x0348`
posts report 6; inputs `0x05e1`, `0x05e7`, and `0x05dc` start timer class
`0x52`; terminal `0x05eb` or recoded completion `0x06c5` posts report 7. The
complete transition table has 950 records, 30 of which select callback `0x5d`.
Older claims of a unique slot, chooser, or descriptor owner are invalid.

## Closed boundary

Provisioned cold boot reaches the interactive idle UI and opens a real firmware
menu. There is no unresolved MMI-settlement event or hardware acknowledgement.
The next application work is ordinary keypad/menu traversal, persistence,
audio, and built-in application coverage. Detailed negative findings remain in
the evidence ledger to prevent disproven startup theories from being repeated.
