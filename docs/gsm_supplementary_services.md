# GSM Supplementary Services

## Validated call-divert boundary

NSE-8 v6.00 organically interprets the physical MMI sequence `*#21#` as an
unconditional-call-forwarding status request. It establishes a dedicated RR
connection with CM service type `0x08`, then sends this call-independent GSM
04.80 REGISTER message:

```
1b 7b 1c 0d a1 0b 02 01 01 02 01 0e 30 03 04 01 21 7f 01 00
```

The Facility IE contains Invoke id 1, operation `InterrogateSS` (`0x0e`) and
SS-Code `0x21`. `gsm_supplementary` owns the bounded BER-TLV decode and result
encoding. The laboratory network returns a transaction-correlated
`InterrogateSS-Res` whose SS-Status is not active, inside RELEASE COMPLETE.
Firmware renders **Service not active**, and the ordinary RR release path
returns to the serving BCCH. No UI event or firmware state is synthesized.

`make verify-radio-call-divert` requires the exact handset request, decoded
operation and identifiers, response boundary, subsequent RR release and exact
firmware-rendered result frame. Codec unit tests reject malformed BER lengths.

The single-Star registration form `*21*5551234#` produces `RegisterSS` (`0x0a`)
for service `0x21` with a tagged BCD forwarded-to number. The network retains
that subscription across baseband resets, the firmware renders its decoded
destination, and later interrogation reports the active/registered status.
`#21#` produces `DeactivateSS` (`0x0d`) and clears the subscription. Both use
transaction-correlated operation results and the normal RR release path.

`make verify-radio-call-divert-controls` runs registration and deactivation as
separate organic handset fixtures and requires their exact requests, decoded
operations, responses, releases and result frames. The two-Star registration
syntax cannot yet be entered by the key harness because the ROM's Star-key
multi-tap turns the second Star into `+`; this is a harness limitation, not a
network shortcut. Erasure, failure outcomes, basic-service groups and forwarded
call delivery remain separate contracts.

## Validated USSD boundary

The physical MMI sequence `*123#` opens the same call-independent connection
and sends `processUnstructuredSS-Request` (`0x3b`):

```
1b 7b 1c 14 a1 12 02 01 01 02 01 3b 30 0a 04 01 0f
04 05 aa 98 6c 36 02 7f 01 00
```

The parameter sequence contains DCS `0x0f` and the GSM-7 packed `*123#` text.
The laboratory network returns a correlated `ReturnResult` containing DCS
`0x0f` and GSM-7 packed `Nokia test network`. Firmware renders that text with a
Back softkey, acknowledges the release and returns to the serving BCCH. The
codec bounds both the Layer-3 and USSD payloads and rejects malformed or trailing
BER parameter data.

`make verify-radio-ussd` requires the exact organic request, semantic decode,
response, RR release, and exact firmware-rendered response frame. Continued
USSD sessions, network-initiated USSD, reject/error outcomes and other data
coding schemes remain separate contracts.
