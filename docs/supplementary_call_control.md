# Supplementary Call Control

## Validated NSE-8 boundary

The laboratory network supports one standards-shaped supplementary lifecycle
on an organically answered call. Physical keypad and menu input makes the
firmware send these GSM 04.08 call-control messages on the existing CC
transaction:

| Firmware action | Mobile message | Network response |
| --- | --- | --- |
| Press `5` | START DTMF `83 35 2c 35` | START DTMF ACKNOWLEDGE `03 36 2c 35` |
| Release `5` | STOP DTMF `83 71` | STOP DTMF ACKNOWLEDGE `03 32` |
| Select **Hold** | HOLD `83 18` | HOLD ACKNOWLEDGE `03 19` |
| Select **Unhold** | RETRIEVE `83 1c` | RETRIEVE ACKNOWLEDGE `03 1d` |

`Unhold` is the NSE-8 menu label; `RETRIEVE` is the protocol operation. The
network response remains on the handset-created transaction and does not alter
firmware RAM or synthesize UI events. Session hold state and counters are saved
for deterministic state restoration.

`make verify-radio-supplementary-call` checks the ordered Layer-3 exchange, a
save/load boundary while the call is held, and the exact firmware-rendered
active-call menu containing **Unhold**. Its checker rejects malformed DTMF IE
and reordered hold/retrieve evidence.

## Unsupported boundary

Call waiting, a second simultaneous call, swap, multiparty calls, explicit call
transfer, call forwarding control and USSD are not modeled. Their firmware
consumers and network-side transactions require separate evidence; this
milestone does not infer them from the single-call hold lifecycle.
