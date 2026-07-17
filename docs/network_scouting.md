# Network and registration boundary

This document records the later lower-radio contract needed for network
registration, operator identity, and signal content. Offline MMI settlement is
validated; this map applies when extending the interactive phone into networked
features.

## Current division of responsibility

- **SIM identity and files:** the stateful SIM device supplies the card through
  MAD2 SIMI registers and FIQ 6. The preliminary ATR/PPS/mandatory-EF lifecycle
  is functional.
- **RSSI input:** CCONT ADC channel 1 carries a raw RF-strength scenario value.
  It does not make the phone camp or cause signal bars to render by itself.
- **Camping, registration and operator identity:** these are owned by the
  MCU/DSP lower-radio protocol. MCU task 3 serializes work into the DSP shared
  ring; DSP-to-MCU delivery uses FIQ 0 and the lower task dispatchers.
- **Idle presentation:** task-5/MMI child events render operator, signal and
  status content after the corresponding live subsystems publish it.

## Mapped later radio lifecycle

Ordinary SIM acceptance already completes. The current firmware does not yet
receive the owned object which would complete this downstream chain:

```text
service-5 status 0x05e8
  -> 0x05ea -> task-15 0x07dd
  -> successful parse -> task-14 0x09d8
  -> opcode 0x2a / lower result 0x0fbf
  -> lower context handler 0x253610

separate unresolved completion:
  lower result 0x0fc1 -> task-10 0x1391 -> task-17 0x0434
  -> task-5 0x13e2 -> task-14 0x1776 session request
```

The static portions are normalized in `tools/profiles/noki3210_v600.json` and
checked by `message_census.py`. The unobserved predecessor is the organic path
to one of the recovered `0x05e8` publishers plus entry `0x28`'s downstream
readiness state; reviewed runtime does not execute that transition.

## Emulation direction

A message-boundary DSP/lower-radio peer remains feasible and is the appropriate
MAME architecture. It should answer organically observed requests at the shared
ring or lower transport boundary. It must not inject “registered” state, task
messages, callback selectors, or render events.

The next peer behavior is implementable only after the request/response object
ownership is pinned down. The falsified type-`0x80` primitive-`0x70` reply is not
that response and must not be restored.

### Current transport boundary

The coherent boot organically transmits the type-`0x1a` ARFCN bitmap at about
4.8 seconds, but emits no subsequent search/camp request on the recovered
MCU-to-DSP packet stream. Task 14 receives no message. Cell search is therefore
currently modeled as autonomous DSP/L1 work after configuration; a peer must
not wait for an MCU request that the firmware does not send.

The first fake-cell behavior must be an unsolicited DSP-to-MCU report delivered
through MDIRCV/FIQ0. Static classification narrows the receive surface:

- type `0x99` (`0x28464c`) is a five-sample measurement accumulator which
  publishes class `0x04`, command `0x4a` to task 8, not a camp transition;
- type `0x86` (`0x284316`) is a controller/transfer state machine for subtypes
  `0x70`, `0x80`, `0xb0`, and `0xb1`, and remains a possible prerequisite for
  accepting later framed traffic;
- type `0x8e` reaches task 7 and the task-22 class dispatcher, but the observed
  zero session phase rejects a standalone class-`0x47` candidate.

The next bounded question is which valid type-`0x86` exchange, if any, advances
the firmware-owned session phase so that type-`0x8e` traffic is accepted. If no
such path exists, record the quantified absence and begin at the type-`0x8e`
session constructor rather than guessing a registration-success payload.

## Acceptance

Network work becomes composable when one coherent run:

1. supplies the missing owned object through an evidenced peer transport;
2. reaches organic lower result `0x0fc1 -> 0x1391 -> 0x0434` without conflating
   the separate `0x0fbf` context and `0x0fc2 -> 0x1392` radio-state paths;
3. publishes `0x13e2` and constructs task-14 `0x1776` without intervention;
4. continues through SIM registration and emits operator/signal child content;
5. preserves both 3210 oracles and the 3330 smoke baseline.

Until then, operator-idle is a downstream target, not an independent display
shortcut.
