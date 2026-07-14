# MMI and keypad lifecycle

This document summarizes the firmware-owned MMI input path at the current
coherent Nokia 3210 v6.00 frontier. `interactive_handoff.md` owns the startup
report investigation; this file owns the keypad hardware-to-firmware contract.

## Current state

The provisioned profile paints an idle-like frame with a `Menu` softkey while
task 1 remains in startup mode `0x0004`. The screen is presentation state, not
proof that the application desktop is interactive.

A scripted active-low press changes the MAD2 keypad state and raises IRQ6. The
firmware acknowledges the interrupt and posts event `0x72` to task 1. Task 1
receives the event, but the mode-4 handler takes fallback `0x2701b0` instead of
entering the matrix scanner. No decoded key event reaches the MMI.

## Hardware path

```text
MAME input port COL.0..COL.4
  -> noki3310_state::key_irq
  -> MAD2 keypad row/column state
  -> IRQ6 aggregation
  -> firmware ISR 0x2b5da0
  -> event 0x72
  -> task-1 mailbox
```

The IRQ source, polarity, acknowledgement, mailbox destination, and scheduler
delivery are therefore proved. The driver must not invent a separate keypad
task or post a decoded MMI key directly.

## Firmware consumer

Task 1 owns the raw event. Static and runtime evidence agree:

- the ISR selects mailbox/task 1;
- the universal receive path delivers `0x72` while task id 1 is running;
- the task-1 startup dispatcher branches according to mode `0x1123f0`;
- mode `0x0004` consumes the event without scanning;
- the matrix scan/decode path is reachable only after the shared interactive
  initialization lifecycle.

The earlier claim that the consumer was dormant was wrong. The consumer is
active but deliberately in a startup state that does not decode keys.

## Startup prerequisite

For 3210 v6.00's current branch, report code `0x07` is a real prerequisite for
leaving the mode-4 wait and running the shared initialization burst at
`0x270d1c`. A diagnostic report proved that dependency, but the resulting boot
was incoherent and is not a supported model.

The faithful target is therefore not “make IRQ6 work”; it already works. It is:

1. produce report code 7 through its organic firmware owner;
2. observe task 1 enter the shared interactive initialization path;
3. press a key through the ordinary MAME input port;
4. observe matrix scanning and a decoded MMI key;
5. observe a corresponding UI transition without firmware-state forcing.

## Acceptance evidence

A complete input milestone requires all of the following in one coherent run:

- contact status remains healthy;
- SIM initialization remains organic;
- task 1 leaves mode `0x0004` without an injected report;
- IRQ6 posts event `0x72` to task 1;
- the firmware matrix scan executes after that event;
- a decoded key reaches the MMI event layer;
- the displayed frame changes consistently with the selected key.

An idle-looking PNG alone does not satisfy this contract.

## Diagnostics

`NOKI3210_TRACE_HANDOFF=1` records the task-1 mode, event-`0x72` branch, report
surface, and scan/decode seam. `NOKI3210_TRACE_TASKS=1` provides generic
mailbox-edge context. Both are read-only and must be disabled successfully in
the final acceptance run.
