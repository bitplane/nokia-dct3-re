# Interactive startup handoff

This document is the current contract for the Nokia 3210 v6.00 transition from
provisioned startup to decoded keypad input. Historical forcing experiments are
recorded in `evidence/falsifications.json`; they
are not alternate boot recipes.

## Current result

The coherent frontier profile completes contact service and ordinary non-CPHS
SIM initialization. Task 1 advances to startup mode `0x0004`, while a
provisioned EEPROM identity removes the phone-lock prompt and permits an
idle-like frame with the `Menu` softkey to be painted.

The frame is not yet a proved desktop, but keypad decoding is live while task 1
remains in mode `0x0004`:

```text
active-low keypad input
  -> MAD2 KBGPIO state
  -> IRQ0
  -> ISR 0x2b3084
  -> task-1 event 0x0041
  -> internal 0x41/0x42/0x43 sequence
  -> matrix scan 0x2b2f90
  -> decoded resource 0x6e02 via 0x2b4628
```

Stable multi-key delivery is now proved. After status `0x057c` presents the
security editor, a single emulation-time coroutine supplies `12345` and the left
softkey as non-overlapping physical taps. The transaction completes through
`0x0578` and callback `0x47` returns `0x05e6`. That result does not post code 7
or leave mode `0x0004`, but it is the accepted-code result: verifier `0x2ae704`
returns one only when the five-character input transforms through `0x2ae4e8`
to the four bytes stored at `0x112460`. The security editor, keypad delivery,
and generated default code are therefore complete; startup remains separate.

## Ownership

Task 1 owns the startup consumer. Its state object is rooted at `0x1123ee`:

| field | address | current value/role |
| --- | --- | --- |
| pending startup event | `0x1123ee` | current scalar input |
| startup mode | `0x1123f0` | `0x0004` at the frontier |
| readiness flags | `0x112399` | `0x0f` after mode `0x000d` |
| substate | `0x11239c` | diagnostic context |

The master dispatcher is `0x270c8e`, with a mode jump table at `0x270ca8`.
Mode zero contains the shared interactive initialization burst at `0x270d1c`.
The coherent boot does not enter that burst.

Event `0x72` is not owned by a dormant keypad task. The IRQ handler posts it to
mailbox 1, and task 1 receives it. The missing behavior is therefore a startup
lifecycle transition, not IRQ routing or scheduler delivery.

## Report code 7

For this ROM and the currently selected branch, report code `0x07` is an actual
prerequisite. Both branches leaving mode `0x000d` converge on it:

- the coherently observed route enters mode `0x0004` and waits for code 7;
- the alternate state-0 route enters mode `0x0007` and also waits
  for code 7 before the shared initialization burst.

Supplying code 7 diagnostically proves the immediate dependency: task 1 posts
`0x0732`, starts task 6, and task 6 posts display/window event `0x0547`. The
experiment does not compose. It leaves an earlier task-5 lifecycle active, and
`0x0547` remains queued while repeated `0x0d16` timer traffic wins the receive
path. No downstream result should be injected from that experiment.

The v6.00 report wrapper is `0x2af190`. It publishes resource `0x6a01` and posts
code 7 to task 1. It has exactly four callers:

| owner family | caller | classification |
| --- | --- | --- |
| power/battery | `0x21e40c` | low-voltage shutdown outcome |
| power/charger | `0x21f8de` | charging-completed outcome |
| callback/status | `0x27b3b6` | callback `0x5d` completion |
| controller | `0x255c3c` | status-`0x0795` controller completion |

This report namespace is separate from the application-readiness selectors at
`0x2520ec`. In particular, task 18's selector `0x12` marks checklist byte
`0x112288`; it is not a neighboring task-1 report and says nothing directly
about report code 7.

Within the task-1 report family, code 7 has a genuine paired status.
Wrapper `0x2af17a` publishes resource `0x6a00` and posts code 6. Callback
`0x5d` emits that pair's code 6 on input `0x0348`, starts timer class `0x52` on
`0x05e1` or `0x05e7`, and emits code 7/resource `0x6a01` only on terminal
`0x05eb` or recoded timer completion `0x06c5`. This constrains code 7 to the
terminal side of one startup resource lifecycle; it is not a generic per-task
readiness ordinal. Code 6 is not necessarily delivered first: task 1 uses code
7 to enter the following startup phase, then accepts code 6 together with codes
`0x09`-`0x0d`, `0x0b`, and `0x1b`-`0x1c` while filling the six flags at
`0x112390..0x112395`.

### Task-1 report phase

The task-1 consumer is now fully decoded across mode `0x0004`, the alternate
mode-`0x0007` entry, and the phase opened by code 7. Both entry routes perform
an explicit mailbox comparison with code 7 before shared initialization. Code
7 then clears `0x11239d`, performs the initialization burst, and enters the
following report phase.

That phase has a finite event-to-state map:

| Report | State effect |
| --- | --- |
| `0x09` | set `0x112390 = 1` |
| `0x0a` | set `0x112394 = 1` |
| `0x0b` | set `0x112393 = 1` |
| `0x0c` | set `0x112392 = 1` |
| `0x0d` | set `0x112391 = 1` |
| `0x1c` | set `0x112395 = 1` |
| `0x06` | set exit selector `0x112398 = 1` and leave the phase |
| `0x1b` | set exit selector `0x112398 = 2` |
| `0x04` | re-evaluate the six flags and exit selector |

Once all six one-byte flags are set, selector 1 exits through the common
mode-`0x000c` tail; selector 2 uses byte `0x11ff50`; selector 0 continues into
the later event-`0x74` and display/keypad gates. Thus code 6/resource `0x6a00`
is a decisive post-code-7 outcome, not a chronological prerequisite for code
7. Byte `0x112396` is a later halfword state set at `0x271426`; `0x112397` and
`0x11239b` have no recovered direct references. Byte `0x11239c` stores the
startup classification selected by `0x2af0ae` and is also read by the startup
supervisor. Byte `0x11239d` is cleared on code-7 entry and set only by
`0x2b4652` when the previous keymap-decoded value is `0x0d` while display state
is 1; the later event-`0x74` path uses it to permit the keypad scan fallback.
This store is downstream of `0x2b46da -> 0x2b2f90` matrix scanning and cannot
be the missing pre-code-7 transition.

Callback `0x5d` is organically active in state `0x0b`. Direct inputs `0x05eb`
and `0x06c5` report code 7. Inputs `0x05e1`, `0x05e7`, and `0x05dc` start a
task-local class-`0x52` timer whose recoded completion is `0x06c5`.

All four callers are mapped, but no ordinary coherent producer has yet been
proved. This is stronger than an unbounded “missing event” search: the open
question is which valid external condition or transaction makes one of these
already-known owners complete during a real 3210 boot.

The alternate mode-0d exit no longer forms a static impossibility. Full decode
of `0x2a6942` proves monitor state 0 returns 2 and is accepted; only initialized
states 1 and 2 return zero. Battery initialization clears the state to 0 before
the sole classifier writer publishes 1, 2, or 3. The coherent run reaches the
gate after state 1 is published. A real boot may therefore avoid mode 4 through
different ordering between the readiness checklist and the first monitor sample,
without requiring the unsafe state-3 ADC region. That observation motivated the
bounded timing audit below; it did not establish that the alternate branch would
remove the report-code-7 dependency.

That contract is now resolved for the emulated boot. Battery initialization
holds state 0 from `0.200664 s`; the first classifier publication changes it to
state 1 at `0.364121 s`. The eleven application-readiness writers do not begin
until the modeled contact peer acknowledges the final service-empty `0x622a`
transaction at `1.285269 s`. Firmware then resumes the complete second task
group in a fixed order; task 18's immediate code-`0x12` call at `0x285c5e` is
last at `1.297865 s`, and task 1 evaluates the gate at `1.298045 s`.

This is a contact-peer timing dependency, not a uniquely late battery, CCONT,
or task-18 response. It also is not the code-7 breakthrough: static comparison
of the two mode-`0x000d` tails shows that the accepted state-0 branch at
`0x270eee` performs its own mailbox receive and explicit comparison with code 7
before the shared interactive initialization. The observed state-1 branch at
`0x270fa4` waits for the same report in mode 4. Changing the peer's calibrated
delay solely to win the state-0 window would select a different dead wait, so
the delay is ledgered as fidelity debt and the frontier returns to the organic
code-7 owner.

## Excluded owners

The following candidates were tested or closed statically and must not be
reintroduced without new evidence:

- normal battery voltage, BSI, temperature, charger, or held-PWRONX values;
- the mode-4 battery-characterisation route: its event `0x43` is selected only
  by external contact-service command `0x8e` payload 3, not ordinary boot;
- Advice-of-Charge initialization and exhausted-account completion;
- display/service events `0x0280`-`0x0282` and the state-7 `0x06ca` route;
- callback slot `0x45` and the `0x09d0`/`0x09d1` chooser;
- callback `0x31`, status `0x00ca`, and resource result `0x0348`;
- task-13 direct-to-task-16 `0x05eb` delivery;
- the security editor and EEPROM/SIM identity comparison;
- task-17 startup object `0x1587` and its `0x0a35`, `0x0a25`, `0x09ec`,
  `0x043c` local transaction;
- DSP heartbeat traffic, a guessed type-`0x74` `0a 09` echo, and ROM-4 class
  `0x74` payload `13 00`;
- contact/DSP self-test completion, which now clears organically before the
  mode-4 wall;
- generic-service registration paths that require later framework modes.

The detailed evidence and exact fixtures live in `falsifications.json`,
`resource_providers.md`, `battery_classifier_analysis.md`, and
`sim_registration.md`.

## Cross-ROM controls

Report code 7 is not a universal DCT3 “DSP ready” report:

- Nokia 5110 v5.30 has a structurally equivalent wrapper and four callers, but
  an independent forcing-free boot reaches an interactive Security-code frame
  without executing it. Its keypad lifecycle is serial and not a replay source
  for the 3210 KBGPIO path.
- Nokia 3310 v6.39 reaches an interactive idle-like frame in an independent
  message-level emulator without executing its equivalent wrapper.
- Nokia 3330 v4.50 retains the same four-owner reporter topology statically.
- Nokia 3210 v5.01 contains the same resource-`0x6a01`/code-7 wrapper at
  `0x2ac5bc`, with exactly four callers at `0x21e22c`, `0x21f772`, `0x252a4a`,
  and `0x277d06`. Its task-1 state machine at `0x26dc20..0x26df14` is
  instruction-for-instruction equivalent to v6.00's
  `0x27120e..0x271502`: both mode entries explicitly wait for code 7, then use
  the same code-6/report-flag/event-`0x74` control flow. The state bytes are
  relocated from v6.00 `0x112390..0x11239d` to v5.01
  `0x1121bc..0x1121c9`. This proves the contract is stable across two 3210
  firmware releases; it is not a v6.00-only lifecycle artifact. The four
  caller bodies also retain their local predicates and outcomes. Callback
  `0x5d` retains the same input cascade and terminal statuses, with only its
  local timer class changing from `0x51` to `0x52`. No product-version branch
  or additional NV/hardware predicate is exposed by the comparison.

These controls reject generic peer traffic as justification for a synthetic
report. The faithful correction must be observable by the 3210 firmware at a
real hardware, transport, or nonvolatile-data boundary.

## Retired bridge

`MODEL_STARTUP_REPORTS` formerly returned code 7 and the later startup
checklist directly from the task-1 getter. It established downstream control
flow but was a firmware-result force and did not compose. It and the associated
historical oracle have been removed.

## Active diagnostics

`NOKI3210_TRACE_HANDOFF=1` retains only the current causal seams:

- task-1 mode transitions and dispatcher state;
- task-1 mailbox publications;
- report-7 wrapper/caller and callback-`0x5d` activity;
- IRQ0 entry and the scan/decode seam;
- the small set of controller, power, and identity observations still needed
  to reject accidental regressions in the classified surface.

The trace is read-only. A successful fix must work with it disabled.

The corrected release-to-key trace uses the scheduler-backed Lua input timer.
At `1.48 s` task 1 enters mode `0x0004`; a normal softkey press raises MAD2
IRQ0, enters `0x2b3084`, runs the firmware `0x41/0x42/0x43` sequence, scans at
`0x2b2f90`, and publishes decoded keycode `0x19` at `0x2b4628`. No code-7 post
or mode change occurs. The former IRQ6/event-`0x72` result was caused by
combining keypad and CCONT onto the same emulated source.

## Closed ownership and remaining lifecycle

The independent code-6 caller at `0x28c22c` is selected by status `0x0794`.
The census finds no direct producer: its sole numeric predecessor is fixed
catalogue input `0x32b4`/`0x72b4` (status index `0x12b4`), for which there is no
in-ROM initiating producer. The adjacent code-7 status `0x0795` is a real
terminal pair, but both of its effective producers are already classified as
later conditional paths. One requires display state 7; the other requires
framework mode 11 reached through external status `0x03ab`. Status `0x03ab`
itself has one consumer literal and no recovered in-ROM producer. This closes
the independent `0x0794`/`0x0795` family without turning current-state absence
into an invented transition.

A fresh coherent trace corrects the runtime interpretation: the security/text
transaction receives multi-key input and completes through `0x0578` while task
1 remains in mode 4. Code 7 is not the input owner. The observed `0x05e6` is a
successful security-code comparison, not evidence for synthesizing code 7.

No faithful correction is proved yet. All four code-7 callers and both code-6
callers are classified. Callback `0x5d` no longer exposes an object/session
question: its start statuses directly construct a local class-`0x52` timeout,
and a wrong-code `0x05e1` control proves statuses remain scoped to callback
`0x47`. Its only mapped non-initialization selector is the already-closed
slot-`0x45`/`0x09d0` context route. Callback `0x5d` is therefore a valid dormant
contract, not the current ordinary-hardware frontier.

The same-product comparison and the closed callback census leave no justified
software-side transition to synthesize. The smallest decisive next evidence is
one real-3210 trace identifying the executed report-7 caller and its preceding
hardware or transaction state. A provisioned 16 KiB EEPROM capture with
personal identity bytes redacted but checksummed block structure preserved is
a useful secondary control, but no remaining caller currently proves an NV
predicate to target.
