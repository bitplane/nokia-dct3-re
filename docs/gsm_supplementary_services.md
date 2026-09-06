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

Activation, registration of a forwarding number, erasure and failure outcomes
are not yet modeled. They must be driven by their own organic handset requests.
USSD shares the GSM 04.80 component layer but has a distinct operation payload
and remains the next contract.
