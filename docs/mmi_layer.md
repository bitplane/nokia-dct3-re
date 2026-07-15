# MMI and keypad lifecycle

This document summarizes the firmware-owned MMI input path at the current
coherent Nokia 3210 v6.00 frontier. `interactive_handoff.md` owns the startup
report investigation; this file owns the keypad hardware-to-firmware contract.

## Current state

The provisioned profile paints an idle-like frame with a `Menu` softkey while
task 1 remains in startup mode `0x0004`. The screen is presentation state, not
proof that the application desktop is interactive.

A scripted logical press changes the active-low MAD2 keypad state and raises
IRQ6. The
firmware acknowledges the interrupt and posts event `0x72` to task 1. Task 1
receives the event, but the mode-4 handler takes unhandled-event return `0x2701b0` instead of
entering the matrix scanner. No decoded key event reaches the MMI.

The corrected coherent trace drives Enter 250 ms after readiness through
MAME's ordinary input field. It observes one IRQ6/ISR delivery and one
event-`0x72` return at `0x2701b0`, with zero post-press calls to matrix scanner
`0x2b2f90`.
The only scan in the run is the firmware's early mode-1 initialization scan.

## Hardware path

```text
MAME input ports COL.0..COL.4 (five columns, four row bits each)
  -> MAD2 row signal 0x28 / direction 0xa8
  -> active-low column input 0x2a / interrupt mask 0x6b
  -> masked falling-edge latch
  -> IRQ6 aggregation
  -> firmware ISR 0x2b5da0
  -> event 0x72
  -> task-1 mailbox
```

The IRQ source, polarity, acknowledgement, mailbox destination, and scheduler
delivery are therefore proved. MAD2 line 6 is shared with CCONT, so the model
keeps separate keypad-edge and CCONT-level sources and ORs them at the
aggregator. The driver must not invent a separate keypad task or post a decoded
MMI key directly.

## Register contract

The complete v6.00 matrix scanner `0x2b2f90` aligns uniquely with v5.01
`0x2b0208`; the downstream decoder block `0x2b4628` aligns with `0x2b18b0`, and
the IRQ ISR `0x2b5da0` aligns with `0x2b3200`. Both firmware versions implement
the same sequence:

1. Save column-interrupt mask `0x6b` and set its low five bits to mask all.
2. Set row signal `0x28 = 0x0f` and row direction `0xa8 = 0` for the direct
   power-column check.
3. If no direct column is low, configure one of four row outputs in `0xa8`,
   drive that row low through `0x28`, and read five active-low columns at
   `0x2a`.
4. Restore idle row direction `0x0f`, row signal `0xf0`, and the saved column
   mask.

The five MAME `COL.n` ports are therefore columns, not rows. Their bits 1..4
are the four row contacts; power is the direct bit-0 input. The former driver
reversed these axes, ignored direction and interrupt mask, and asserted IRQ6
both from an input callback and a 200 Hz poller. The corrected model derives
columns from driven-low output rows and latches one IRQ only for an unmasked
high-to-low column transition.

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

Once the interactive lifecycle is active, event `0x72` enters the master input
handler at `0x270a8c`, which publishes resource `0x7317` value 1 and starts the
internal `0x41`/`0x42`/`0x43` key-state sequence. Those internal events call
`0x2b46da`; it scans the matrix and translates the result through the selected
keymap. Function `0x2b4628` then publishes decoded resource `0x6e02`. Its store
at `0x2b4652` arms `0x11239d` when the previous decoded key is `0x0d` and the
display state is 1. It is downstream of scanning, not a pre-input hardware
event or a possible source of report code 7.

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
