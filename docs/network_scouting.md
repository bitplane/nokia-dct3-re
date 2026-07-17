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
  -> task-17 phase-dependent acknowledgement/restart work

separate task-17 registration request:
  task-17 0x09d6 -> task-5 0x13e2 -> task-14 0x1776 session request

direct DSP completion family:
  RX 0x87 -> task-10 0x138f -> empty-work gate -> common finalizer
  RX 0x8a -> task-10 0x1390 -> count/bit gates -> common finalizer
  common finalizer 0x219e30 -> task-17 0x0434 when controller flag permits
```

The static portions are normalized in `tools/profiles/noki3210_v600.json` and
checked by `message_census.py`. The unobserved predecessor is the organic path
to one of the recovered `0x05e8` publishers plus entry `0x28`'s downstream
readiness state; reviewed runtime does not execute that transition.

An opt-in boundary diagnostic now proves the direct completion route
end-to-end without touching MCU state. Two type-`0x87` reports, delivered after
the organic type-`0x1a` publication through MDIRCV/FIQ0, make task 10 enter
`0x219e30` and post `0x0434` to task 17. A single report is queued but does not
wake the task-10 consumer in this scheduler state; the reason a real peer
supplies the additional wake remains unresolved. Repeating type `0x8a` instead
increments its counter toward limit `0x01f2` while the controller gate remains
clear. These runs identify `0x87` as the short terminal completion family and
`0x8a` as a counted report family, but do not establish their RF names or make
either suitable for default emulation.

The same run corrects the former downstream assumption. Task 17 consumes
`0x0434` at the dispatcher comparison `0x225240`, enters handler `0x225b6c`,
performs local acknowledgement/restart work, and continues at `0x226348`.
It does not publish `0x13e2` or construct `0x1776` in the active startup phase.
The `0x225b8c -> 0x2b3f60` publisher is selected by adjacent dispatcher input
`0x09d6`, not by `0x0434` or `0x0a22`. The former static chain joined separate
dispatcher arms.

The retained instrument is disabled by default. Setting
`NOKI3210_DIAG_RADIO_SCENARIO=1` schedules the two type-`0x87` reports after
each organic type-`0x1a` packet; value `2` schedules 64 type-`0x8a` reports.
It is a transport/consumer isolation scenario, not a fake-cell model or a
supported machine profile, and may be removed once the real sequence is known.

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
- type `0x83` (`0x284734`) is a controller-gated scalar report. In controller
  state 3 it publishes `0x139f`; task 10 only copies signed payload byte `+6`
  to `0x10dc99`, with no registration or lower-result output;
- type `0x86` (`0x284316`) is a separate controller/transfer state machine for
  subtypes `0x70`, `0x80`, `0xb0`, and `0xb1`. Its controller state is
  established by type `0x89`; exhaustive decode found no access to the
  type-`0x8e` framed-session phase, so it is not that session's bootstrap;
- type `0x8e` reaches task 7 and the task-22 class dispatcher, but the observed
  zero session phase rejects a standalone class-`0x47` candidate.
- type `0x87` (`0x284ebc`) is payload-independent at task 10 and can enter
  finalizer `0x219e30` when two outstanding-work pointers are empty;
- type `0x8a` (`0x284e88`) is the counted variant and can enter the same
  finalizer after exceeding a firmware-owned limit. The RF conditions that
  produce either report remain unresolved, so they define the next peer
  contract to identify rather than packets to inject;
- type `0x84` feeds a structured eight-byte controller event to `0x217cac`,
  while type `0x89` advances controller state through `0x1393`/`0x21bb5c` and
  does not directly call the finalizer.

The type-`0x86` bootstrap hypothesis is closed by quantified absence. The
type-`0x8e` session is instead started by firmware callback `0x0a`: event
`0x06a9` reaches `0x2831a8`, whose successful validation path calls
`0x28316c`. That routine initializes the session context, posts task-`0x1a`
event `0x0202`, sets phase `0x11fedb` to 1, and queues the first outbound
descriptor. There is no direct caller from the DSP receive machinery.

The first phase transition is also bounded. Inbound class `0x42`, command
`0x64`, payload byte `+0x0b == 0x45` is accepted only while phase is 1; it
advances phase to 2 and invokes outbound constructor `0x2824e4`. Other payload
values terminate or reset the session. This is a call/session protocol
contract, not evidence that ordinary offline boot should synthesize it.

The callback-`0x0a` contract is not the ordinary dial entrance. An organic
`123` + Navi dial attempt follows this path instead:

```text
task 5 constructs 0x0fa3 -> task 14
  -> task-14 call controller accepts the session and emits 0x09d2
  -> task 15 stores the dial object and returns internal result 0x0a02
  -> task-15 state 6 registers an asynchronous activity and waits
```

The `0x0fa3 -> 0x09d2` conversion is firmware-local and completes in about
0.3 ms. It does not contact a peer. Roughly 0.1 seconds later the firmware
organically updates the DSP shared-control/L1 configuration using commands
`0x08:0x060b`, `0x09:0x08af`, `0x09:0x09a0`, `0x25:0x0041`, followed by commit
`0x2f`. The current peer completes those writes, but no new MCU-to-DSP
packet-ring request or callback-`0x0a` framed session follows and the call
remains pending. Dialing is therefore a downstream acceptance probe, not the
missing registration producer.

The next useful experiment remains a peer-owned search/camp report delivered
through an evidenced DSP-to-MCU decoder. It must establish registration before
the parked call can be expected to progress.

## Acceptance

Network work becomes composable when one coherent run:

1. supplies the missing owned object through an evidenced peer transport;
2. reaches `0x0434` through an evidenced peer condition: either organic lower
   result `0x0fc1 -> 0x1391` or the now-mapped direct `0x87`/`0x8a` completion
   family, without conflating the separate `0x0fbf` context and
   `0x0fc2 -> 0x1392` radio-state paths;
3. supplies task-17 input `0x09d6` through its organic owner, publishes
   `0x13e2`, then constructs task-14 `0x1776` without intervention;
4. continues through SIM registration and emits operator/signal child content;
5. preserves both 3210 oracles and the 3330 smoke baseline.

Until then, operator-idle is a downstream target, not an independent display
shortcut.
