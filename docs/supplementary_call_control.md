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

## Validated two-call boundary

NSE-8 also accepts a second network-originated `SETUP` on the existing TCH as
transaction 1. The firmware presents **End this call / Answer / Reject**, and
answering organically holds transaction 0 before connecting transaction 1.
Its two-call menu presents **End this call / Swap / End all calls**. Swap emits
HOLD for the active transaction followed by RETRIEVE for the held transaction.
Ending one transaction retains RR and retrieves the survivor; only ending the
last transaction releases the traffic channel. The session therefore owns two
saved CC legs while LAPDm, RR and the physical TCH remain shared.

`make verify-radio-two-call` checks both transaction identities, exact answer
and swap menu frames, save/load after Swap, transaction-local release, speech
traffic, and the absence of premature RR teardown.

NSE-8 can also originate the second leg. Selecting **New call** while the first
leg is active makes the firmware HOLD transaction 0, issue a new CM SERVICE
REQUEST on the existing dedicated connection, and create transaction 1 with a
normal SETUP / CALL PROCEEDING / ALERTING / CONNECT exchange. There is no
second RR assignment: both CC legs share the original TCH. The session model
accepts that nested service transaction at the radio boundary and retains the
same saved two-leg representation used by call waiting.

`make verify-radio-second-outgoing-call` checks that ordered exchange and all
six positions of the resulting firmware menu: **End this call**, **Swap**,
**End all calls**, **Send DTMF**, **Send**, and **Phone book**. The v6.00 NSE-8
manual does not document conference calling, the exhaustive two-call menu does
not offer it, and the handset emits no call-related FACILITY request. Although
the English PPM contains the strings `Conference` and `Private`, that alone is
not evidence that this product can initiate GSM BuildMPTY. Multiparty control
therefore remains an unimplemented cross-product feature, not a missing reply
invented for the 3210.

The negative gate records consumer behavior rather than imposing a stricter
network policy. Repeating the transaction-1 SETUP produces no second CALL
CONFIRMED or third UI leg. A SETUP whose Bearer Capability length exceeds the
message is tolerated: NSE-8 responds once with its bounded capability set and
continues presenting the waiting call. Neither case releases the original call
or the shared RR channel.

Multiparty calls, explicit call transfer, call forwarding control and USSD are
not modeled. They require separate firmware and network evidence rather than
being inferred from resource strings or the validated two-leg lifecycle.
