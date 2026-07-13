# Contact-service command topology

This document is the authoritative 3210 v6.00 map for contact-service class
`0x40` commands `0x64`, `0x65`, `0x70`, `0x71`, and `0x74`. It distinguishes an
incoming command consumer from an MCU constructor carrying the same command id.
That distinction matters because several constructors are acknowledgements and
therefore do not identify the initiating producer.

The machine-readable evidence is produced by `tools/message_census.py`. The ROM
scan covers all 98 recovered calls to `contact_message_alloc_234634`; all five
target constructor callsites and payload lengths are checked by `--check`.

## Frame and transport path

`contact_message_alloc_234634` builds a class-`0x40` frame:

```text
[0] destination node  [1] source node       [2] transport state
[3] 0x40              [4:5] payload length + 3
[6] sequence/state    [7] 1                 [8] command
[9...] payload
```

`contact_message_send_234684` reports the frame, offers it to service channel
`0x6400` through `0x2b203e`, and queues it through `0x2b0482`. For ordinary
frames, `0x2b0482` posts the resulting message to task 7. Task 2 receives
class-`0x40` frames in `contact_service_response_dispatch_237400` and dispatches
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
request.  It does **not** populate the contact-service header globals: task-2
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
   command `0x64` result 1, command `0x70` with the `0x5f`/`0x62` channel map,
   and command `0x64` result 5. Firmware replies through task 7; the peer returns
   compact class-`0x7f` acknowledgements derived from each outgoing transaction.
5. The final `0x622a` report is likewise acknowledged at the transport boundary.
   No semantic echo is needed and no firmware state is written by the model.

In the five-second acceptance run this leaves contact status `0x49`, advances
startup `0x000d -> 0x0004`, activates SIM control
`0x32 -> 0x33 -> 0xb3 -> 0xe3`, and begins the normal SIM SELECT/READ/STATUS
conversation. Later SIM-card corrections now raise SIM enable organically and
complete the non-CPHS SIM initialization pass before entering the timed
card-presence monitor; it is no longer a contact-service blocker.

## Command inventory

| Command | Incoming consumer | Sole MCU constructor | Constructor role | Initiating producer |
| --- | --- | --- | --- | --- |
| `0x64` | `0x236dc4` | `0x236dd8`, len 9 | status/timeout frame; the same routine consumes a healthy result | organic MCU outbound status; external peer is the counterparty |
| `0x65` | `0x236bac` | `0x29bd4a`, len 1 | status notification from `[0x11fed3]` | **organic MCU outbound**, including caller `0x292462` outside the contact dispatcher |
| `0x70` | `0x23670c` | `0x236742`, len 1 | ack after applying incoming 0x40-byte channel map | **external service/test peer request**, MCU ack |
| `0x71` | `0x23670c` | `0x236736`, len 0 | ack after disabling channel map | **external service/test peer request**, MCU ack |
| `0x74` | `0x236560` | `0x23662c`, len 0 | completion ack after incoming indexed NV operation | **external service/test peer request**, MCU ack |

The `0x65` constructor has callers at `0x236c52`, `0x2379f4`, and `0x292462`.
The latter establishes an MCU-side initiating path independently of receipt of a
same-id contact command. By contrast, the `0x70`, `0x71`, and `0x74`
constructors are dominated by their incoming handlers. Combined with their
external-service transport destination, they are the reply side of peer-initiated
contracts.

## Runtime evidence

Two coherent profiles were traced without adding an emulation shim:

- default four-second boot: one firmware `0x64` construct/send and no receive;
- deep boot: the provisional model posts one header-incomplete healthy `0x64`
  directly to task 2. The firmware consumes and frees that allocation once. Its
  resulting status report is then routed through task 7, assigned route selector
  `1`, and delivered back to task 2 as a fresh allocation. That self-delivery
  repeats result `5`; it is not a watchdog result-`2` loop or repeated delivery
  of the original allocation.

Neither profile constructs, sends, or receives `0x65`, `0x70`, `0x71`, or
command `0x74`. Absence from these two profiles proves dormancy in those boots,
not absence from the ROM. The static constructor census supplies the latter
coverage.

A later request-derived DSP-ring run established the missing incoming
class-`0x40` session and compact acknowledgement grammar. The earlier zero-header
self-route is retained only as the falsification which identified the learning
point; it is not present in the recovered session.

The formerly reported 31,068-entry `0x64` loop is therefore model-induced. It
occurs with or without `MODEL_SVC_CHANNEL_DRAIN`; the drain changes its start
time but is not its cause. The responder fires before observing a request and
writes only `{[3]=0x40,[8]=0x64,[9]=0x05}`, leaving address, route, sequence, and
length fields zero. It proves the result-5 firmware branch in isolation, but not
a coherent node-0x18 transaction or stable contact-service completion.

`TRACE_CSCMD` is observational only and is capped so later runs retain this
ownership and routing evidence without large logs.

## Separate `0x74` namespaces

Contact-service command `0x74` is not scheduler event `0x74`. The scheduler event
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
