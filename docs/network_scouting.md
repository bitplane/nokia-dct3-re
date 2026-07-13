# Network and registration boundary

This document records the current conclusion for reaching the classic operator
idle screen. Earlier scouting based on the former `MODEL_SIM_CARD` comparison
harness and the removed `MODEL_RES_ENABLE` and `TRACE_DSPDRV` probes is
superseded by the organic SIM and lower-radio work in `sim_subsystem.md`,
`sim_registration.md`, and `dsp_interface.md`.

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

## Active bounded frontier

The current blocker is not “the GSM stack never starts” and is not a missing
display-resource registration. The organic preliminary startup reaches the
generic-service/lower-radio boundary, but never receives the owned object which
would complete this chain:

```text
service-5 object-bearing 0x05e8
  -> 0x05ea -> task-15 0x07dd
  -> successful parse -> task-14 0x09d8
  -> opcode 0x2a / lower result 0x0fbf
  -> task-10 0x1392 -> task-17 0x0434
  -> task-5 0x13e2 -> task-14 0x1776 session request
```

The static portions are normalized in `tools/profiles/noki3210_v600.json` and
checked by `message_census.py`. The missing predecessor remains generic-service
session/queue population before callback-table entry `0x28`; reviewed runtime
shows the numeric sweep callbacks but no object-bearing completion.

## Emulation direction

A message-boundary DSP/lower-radio peer remains feasible and is the appropriate
MAME architecture. It should answer organically observed requests at the shared
ring or lower transport boundary. It must not inject “registered” state, task
messages, callback selectors, or render events.

The next peer behavior is implementable only after the request/response object
ownership is pinned down. The falsified type-`0x80` primitive-`0x70` reply is not
that response and must not be restored.

## Acceptance

Network work becomes composable when one coherent run:

1. supplies the missing owned object through an evidenced peer transport;
2. reaches organic `0x0fbf -> 0x1392 -> 0x0434`;
3. publishes `0x13e2` and constructs task-14 `0x1776` without intervention;
4. continues through SIM registration and emits operator/signal child content;
5. preserves both 3210 oracles and the 3330 smoke baseline.

Until then, operator-idle is a downstream target, not an independent display
shortcut.
