# Interactive startup handoff

This document is the current contract for the Nokia 3210 v6.00 transition from
provisioned startup to decoded keypad input. Historical forcing experiments are
recorded in `evidence/falsifications.json` and `removed_forcing_knobs.md`; they
are not alternate boot recipes.

## Current result

The coherent frontier profile completes contact service and ordinary non-CPHS
SIM initialization. Task 1 advances to startup mode `0x0004`, while a
provisioned EEPROM identity removes the phone-lock prompt and permits an
idle-like frame with the `Menu` softkey to be painted.

That frame is not an interactive desktop. A delayed keypad press proves the
electrical and RTOS delivery path is alive, but task 1 consumes the raw event in
mode `0x0004` before the matrix scanner is called:

```text
active-low keypad input
  -> MAD2 KBGPIO state
  -> IRQ6
  -> ISR 0x2b5da0
  -> raw event 0x0072
  -> task-1 mailbox
  -> task-1 mode-4 dispatcher
  -> fallback 0x2701b0
  -> no matrix scan, no decoded MMI key
```

The remaining acceptance condition is an organic run in which the same input
continues from event `0x72` into matrix scanning and produces a decoded MMI key.

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

- the ordinary fallback enters mode `0x0004` and waits for code 7;
- the alternate SIM-ready/no-charger route enters mode `0x0007` and also waits
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
| callback/status | `0x255c2e` | callback `0x5d` completion |
| controller | `0x27f14e` | conditional/later controller outcome |

Callback `0x5d` is organically active in state `0x0b`. Direct inputs `0x05eb`
and `0x06c5` report code 7. Inputs `0x05e1`, `0x05e7`, and `0x05dc` start a
task-local class-`0x52` timer whose recoded completion is `0x06c5`.

All four callers are mapped, but no ordinary coherent producer has yet been
proved. This is stronger than an unbounded “missing event” search: the open
question is which valid external condition or transaction makes one of these
already-known owners complete during a real 3210 boot.

## Excluded owners

The following candidates were tested or closed statically and must not be
reintroduced without new evidence:

- normal battery voltage, BSI, temperature, charger, or held-PWRONX values;
- the pack-characterisation recovery route;
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
  and `0x277d06`. Its task-1 branch still needs to be aligned before claiming
  that it waits on code 7 identically.

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
- event-`0x72` consumption versus scan/decode entry;
- the small set of controller, power, and identity observations still needed
  to reject accidental regressions in the classified surface.

The trace is read-only. A successful fix must work with it disabled.

## Next bounded comparison

Align the v5.01 task-1 mode-4/mode-7 handlers and the four caller families with
v6.00. If the older ROM waits on the same report, use the stable cross-version
caller structure to identify the missing external condition. If it bypasses
the wait, recover the branch predicate and its hardware/configuration inputs.
Only behavior supported by that static comparison and an organic runtime
request should be implemented.
