# Class-`0x40` service-command topology

This document is the authoritative 3210 v6.00 map for service class
`0x40` commands `0x64`, `0x65`, `0x70`, `0x71`, `0x74`, and `0x8e`. It distinguishes an
incoming command consumer from an MCU constructor carrying the same command id.
That distinction matters because several constructors are acknowledgements and
therefore do not identify the initiating producer.

Task 2 is the class-`0x40` service-command dispatcher and task 7 is the
external-service transport adapter. The fault-screen caption does not name the
protocol.

The machine-readable evidence is produced by `tools/message_census.py`. The ROM
scan covers all 98 recovered calls to `service_message_alloc_234634`; all five
target constructor callsites and payload lengths are checked by `--check`.

## Frame and transport path

`service_message_alloc_234634` builds a class-`0x40` frame:

```text
[0] destination node  [1] source node       [2] transport state
[3] 0x40              [4:5] payload length + 3
[6] sequence/state    [7] 1                 [8] command
[9...] payload
```

`service_message_send_234684` reports the frame, offers it to service channel
`0x6400` through `0x2b203e`, and queues it through `0x2b0482`. For ordinary
frames, `0x2b0482` posts the resulting message to task 7. Task 2 receives
class-`0x40` frames in `external_service_response_dispatch_237400` and dispatches
on byte `[8]`.

This is the strongest evidenced boundary: **the external service/test peer behind
task 7's lower service transport**. Task 7 is the on-device transport adapter.
After service discovery, firmware-created frames carry destination `0x02` and
source `0x00` (the phone), which is phone-to-service-peer traffic. The inverse
interpretation in the first trace pass came from labelling the two address bytes
backwards.

The current DSP-ring model sharpens the ordering.  The startup D0 exchange is
serialized by task 8/task 3 as DSP packet type `0x05`; a type-`0x8e` reply reaches
task 8 through FIQ0 and task 7 accepts the state-`4` D0 data frame organically.
That exchange is finite when the model answers only the state-`1` discovery
request.  It does **not** populate the service-frame header globals: task-2
initialization clears `0x11fedd..0x11fedf` about 130 ms after the D0 exchange and
they remain zero through the first `0x64` construction.

The firmware-owned learning point is the class-`0x40` receive path at
`0x237c70`: it copies received frame byte `[2]` to `0x11fedd`, byte `[0]` to
`0x11fede`, and byte `[7]` to `0x11fedf` before dispatching the command.  Thus the
peer must initiate a correctly framed class-`0x40` session message through the
lower transport. The current model does so; delaying D0 or supplying an address
through a firmware write is neither necessary nor correct.

## Recovered coherent startup session

The forcing-free contact path now composes in one boot:

1. Task 8 emits the state-1 D0 discovery frame as DSP TX type `0x05`. The peer
   returns its compact acknowledgement and state-4 data frame as RX type `0x8e`.
2. Contact initialization sets status `0xc8 -> 0xcc` and posts static task-3
   object `0x2db250`. Task 3 serializes it as DSP TX type `0x70`, payload
   `0d 00`.
3. The DSP returns RX type `0x74`, payload `0d 00`. Decoder `0x29bc00` builds a
   class-`0x74` task-2 object; command `0x0d` reaches `0x2349c8`. Firmware clears
   busy bit 2 at `0x2349dc` and leaves service-present bit 6 set. The two payload
   bits are failure indicators, so zero is the healthy result.
4. Only after that DSP completion does the external peer send class-`0x40`
   command `0x64` result 1 and command `0x70` with the `0x5f`/`0x62` channel
   map. Firmware replies through task 7; the peer returns
   compact class-`0x7f` acknowledgements derived from each outgoing transaction.
5. The final `0x622a` report is a one-way type-`0x05` packet. Its associated
   channel-empty transaction completes through the separate DSPIF shared-control
   pending/completion path; no RX packet or semantic echo acknowledges the frame.

Peer-to-MCU frames are delivered serially at 9600 baud with ten wire bits per
byte. The DSP HLE uses a separate response timer, so a response generated while
consuming TX cannot re-enter the MCU-facing RX ring in the same callback and
only one queued frame completes per wire interval.

In the five-second acceptance run this leaves service-session status `0x49`, advances
startup `0x000d -> 0x0004`, activates SIM control
`0x32 -> 0x33 -> 0xb3 -> 0xe3`, and begins the normal SIM SELECT/READ/STATUS
conversation. Later SIM-card corrections now raise SIM enable organically and
complete the non-CPHS SIM initialization pass before entering the timed
card-presence monitor; the modeled service session completes its startup contract.

## Extended-task readiness contract

Mode `0x000d` accumulates readiness bits in `0x112399`; event `0x15` supplies
bit `0x04`. Producer `0x2af208`, called from `0x2521cc`, fires only when the
eleven-byte application-task checklist at `0x112280` is complete.

The scheduler creates every task, initially dormant. Supervisor
`startup_power_service_init_gate 0x2a8ff2` resumes them in groups. Its second
group contains the checklist writers and is gated at `0x2a9182`: firmware calls
`service_channel_request_empty 0x2b13d4`, publishes report `0x622a`, and waits
at `0x29bb06` for `0x11fed1` bit 2 to clear. Firmware seeded that bit during
service startup at `0x2347d0`. The DSP HLE completes the organic shared-control
transaction; firmware clears its own state, resumes the tasks, completes the
checklist, posts event `0x15`, and advances startup. This replaces the historical
direct-drain experiment that first isolated the chain.

A timestamped coherent run closes the ordering contract. The DSP shared-control
completion occurs at `t=1.285269`; firmware begins the second task
group immediately, and its checklist calls follow the supervisor's static
resume order: `0x0a`, `0x0b`, `0x0c`, `0x0d`, `0x10`, `0x0f`, `0x0e`, `0x15`,
`0x14`, `0x11`, then `0x12`. Task 18's straight-line initializer at `0x285c14`
posts the final code at `0x285c5e`, and task 1 evaluates `0x2a6942` at
`t=1.298045`. There is no independently late checklist owner or ADC completion:
the complete application group is held behind the service-empty transaction.

The current external-service peer deliberately waits 36 service ticks before beginning
the external session. That calibrated delay contributes to this ordering and
remains hardware-fidelity debt, but is not adjusted merely to select a different
firmware branch. The alternate mode-`0x000d` tail has the same report-code-7
listener. Both branches nevertheless enter their shared interactive
initialization before recording mode 4 or 7, so this ordering is fidelity debt
rather than an application-start blocker.

Command `0x64`, result `5`, is not an ordinary healthy-startup response. Its
firmware handler enters lifecycle state 5, delays five scheduler ticks and calls
the bulk suspend routine `0x2795e6` for tasks 10--17. The earlier peer model
sent it speculatively; at the correct Timer-0 rate that suspended task 17 before
the later `0x1587` post. The supported peer no longer invents that transition.

## Command inventory

| Command | Incoming consumer | Sole MCU constructor | Constructor role | Initiating producer |
| --- | --- | --- | --- | --- |
| `0x64` | `0x236dc4` | `0x236dd8`, len 9 | status/timeout frame; the same routine consumes a healthy result | organic MCU outbound status; external peer is the counterparty |
| `0x65` | `0x236bac` | `0x29bd4a`, len 1 | status notification from `[0x11fed3]` | **organic MCU outbound**, including caller `0x292462` outside the contact dispatcher |
| `0x70` | `0x23670c` | `0x236742`, len 1 | ack after applying incoming 0x40-byte channel map | **external service/test peer request**, MCU ack |
| `0x71` | `0x23670c` | `0x236736`, len 0 | ack after disabling channel map | **external service/test peer request**, MCU ack |
| `0x74` | `0x236560` | `0x23662c`, len 0 | completion ack after incoming indexed NV operation | **external service/test peer request**, MCU ack |
| `0x8e` | `0x235848` | `0x2358a0`, len 1 | echoes the selector after executing its service action | **external service/test peer request**, MCU ack |

The `0x65` constructor has callers at `0x236c52`, `0x2379f4`, and `0x292462`.
The latter establishes an MCU-side initiating path independently of receipt of a
same-id contact command. By contrast, the `0x70`, `0x71`, `0x74`, and `0x8e`
constructors are dominated by their incoming handlers. Command `0x8e` selector
3 posts task-19 event `0x43`, selector 4 posts event `0x44`, and the handler then
acknowledges the selector. Combined with their
external-service transport destination, they are the reply side of peer-initiated
contracts.

## Runtime evidence

The canonical contact manifest combines a missing-hardware one-second trace with a
six-second machine-default trace. Across them the census records command `0x64`
construct/send/receive counts `3/2/2` and command `0x70` counts `1/1/1`.
The frontier run observes the peer's result-1 `0x64` and 64-byte `0x70` channel
map together with the firmware acknowledgements described above. Commands
`0x65`, result-5 `0x64`, `0x71`, contact command `0x74`, and command `0x8e` remain absent from
these bounded boots; the static constructor census supplies their ROM coverage.

**Framing caution:** a class-`0x40` reply cannot be constructed from only class,
command, and result bytes. Address, route, sequence, and length are learned by
the receive path and are required to prevent the firmware response from routing
back into task 2. The supported peer therefore responds only to an observed,
fully framed request; unsolicited mailbox-level construction is invalid.

`TRACE_SERVICE_COMMAND` is observational only and is capped so later runs retain this
ownership and routing evidence without large logs.

## Separate `0x74` namespaces

Class-`0x40` service command `0x74` is not scheduler event `0x74`. The scheduler event
has direct organic MCU producers at `0x213fcc` and `0x214836`; those sites call
the event transport and never construct a class-`0x40` contact frame. A command
census must not use those sites as evidence for the command producer.

## What remains unknown

The census closes the producer class, but not the complete external protocol or
the circumstances in which each request is sent. Faithful emulation belongs at
the task-7/lower-service transport boundary and should model an attached peer only
when the phone has actually entered the corresponding service session. Injecting
commands at the task-2 mailbox bypasses address discovery, framing, and transport
completion and is not evidence.
