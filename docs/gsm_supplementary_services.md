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
`InterrogateSS-Res` inside RELEASE COMPLETE. An absent or registered-inactive
subscription uses its `ss-Status` choice (`0x00` or `0x04` respectively); an
active subscription uses `forwardedToNumber [1]` and
contains the destination retained by the network. Firmware renders **Service
not active** for the first form and the decoded destination for the second. The
ordinary RR release path returns to the serving BCCH. No UI event or firmware
state is synthesized.

`make verify-radio-call-divert` requires the exact handset request, decoded
operation and identifiers, response boundary, subsequent RR release and exact
firmware-rendered result frame. Codec unit tests reject malformed BER lengths.

The Nokia single-Star registration form `*21*5551234#` produces `RegisterSS`
(`0x0a`) for service `0x21` with a tagged BCD forwarded-to number. Registration
creates an active subscription. `#21#` produces `DeactivateSS` (`0x0d`), which
preserves the registered destination but makes it inactive. `ActivateSS`
(`0x0c`) reactivates a registered destination; activating an absent
subscription returns GSM error `0x11`. `EraseSS` (`0x0b`) removes both state and
destination. Unsupported SS operations receive error `0x10`, and malformed BER
is rejected without changing network state.

Register, activate, deactivate and erase results use the GSM 04.80 `SS-Info`
choice `forwardingInfo [0]`: SS-Code `0x21`, a `ForwardingFeatureList`, status
bits, and the forwarded number while it remains registered. This differs from
the `InterrogateSS-Res` choice above. The codec follows the ASN.1 in
[ETSI GSM 04.80 v5.0.0](https://www.etsi.org/deliver/etsi_gts/04/0480/05.00.00_60/gsmts_0480v050000p.pdf).

`make verify-radio-call-divert-lifecycle` performs register, active
interrogation, deactivate and inactive interrogation in one firmware session.
It saves and reloads MAME after registration, then requires correlated invoke
ids, exact response shapes, five RR releases, the stored-number frame and the
inactive frame. Network subscription state deliberately survives a digital
baseband reset and participates in MAME save state.

An active unconditional subscription is also a routing decision. The network
rejects an incoming speech page before caller state is installed in the handset.
`make verify-radio-call-divert-incoming` registers organically, originates a
call through the host adapter, requires its external state to be exactly
`queued -> forwarded`, and rejects any handset page, alerting, connection or
buzzer activation. The terminal host event identifies the routing reason as
`unconditional` and carries the decoded forwarding destination.

The ROM's dial editor converts a second Star to `+` even after a two-second
physical-key pause, and the tested double-Hash sequence produces no SS
transaction. The organic gates therefore use the Nokia single-symbol aliases.
The parser and unit tests still cover standardized `ActivateSS` and `EraseSS`
requests and their success/error encodings without claiming this firmware UI
can originate `**` or `##`.

The network subscription is indexed independently for unconditional, busy,
no-reply and not-reachable forwarding. Each condition saves registration and
activation state, the destination, optional bearer/teleservice scope and the
optional no-reply duration. The shared lifecycle implementation accepts
RegisterSS, InterrogateSS, DeactivateSS, ActivateSS and EraseSS for every one
of those service codes. NSE-8 organically exposes the single-Star registration
forms used by the routing gates; the other operation/condition combinations
remain protocol-conformance support unless an organic product route is named.

Speech routing honors an omitted BasicService, all-speech teleservice `0x10`
and telephony `0x11`. Other teleservices and all bearer-service selectors do
not divert a speech call. This applicability rule is an isolated, exhaustively
tested GSM contract; NSE-8 did not emit a supplementary transaction for the
attempted `*13` dial-editor selector, so that failed UI probe is not promoted
to product behavior.

`make verify-radio-call-divert-no-reply` is the first organic conditional
acceptance gate. Physical MMI input `*61*5551234*11*5#` makes NSE-8 emit
RegisterSS for service `0x2a`, teleservice selector `0x10`, and a five-second
timer. An external incoming call then progresses through paging and handset
alerting. The saved session timer starts at the firmware's ALERTING message;
expiry sends ordinary CC disconnect/release traffic and the host lifecycle
ends as `forwarded`, never `connected`. This follows the GSM requirement that
CFNRy is invoked only after the served subscriber fails to answer, rather than
turning it into a pre-paging decision. Its host event identifies `no-reply`
and the registered destination, so an eventual external telephony bridge can
route the forwarded leg without reconstructing state from emulator logs.

`make verify-radio-call-divert-busy` registers speech CFB with physical
`*67*5551234*11#`, connects an outgoing host-adapter call, then requires a
second incoming host call to finish as `queued -> forwarded` without paging.
The host event identifies `busy` and carries the registered destination.

`make verify-radio-call-divert-unreachable` registers speech CFNRc with
physical `*62*5551234*11#`, then uses a deterministic laboratory RF profile
which removes both configured cells at 32 seconds. The firmware detects the
loss through its normal measurements and publishes `DOWNLINK_SIGNALLING_FAIL`;
only then is the host call submitted. It must finish as `queued -> forwarded`
with reason `not-reachable` and destination `5551234`, without handset paging.
The timed topology change sequences provisioning before loss; it does not
write firmware state or declare a physical network's loss timing.

Collective
service codes (`0x20`/`0x28`), multiple basic-service records in one result,
and a backend which actually originates a new call to the stored destination
remain unsupported. The stage-1 behavior and no-reply semantics follow
[ETSI GSM 02.82](https://www.etsi.org/deliver/etsi_gts/02/0282/05.00.00_60/gsmts_0282v050000p.pdf);
the BER fields follow
[ETSI GSM 04.80](https://www.etsi.org/deliver/etsi_gts/04/0480/05.00.00_60/gsmts_0480v050000p.pdf).

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
response, RR release, and exact firmware-rendered response frame. Other data
coding schemes remain separate contracts.

The laboratory network also exposes three deterministic terminal outcomes for
the same organic request. It can return a correlated `ReturnError` with GSM
error `0x22` (system failure), return a BER `Reject` with general-problem code
`0x00`, or remain silent. Error and Reject are carried in RELEASE COMPLETE and
the handset performs the ordinary RR release. Silence deliberately sends no
Layer-3 response: the handset retains the dedicated connection and the
supplementary session remains pending for at least the 38-second acceptance
window. No emulator timeout is invented where the firmware has not yet shown
one.

The network can also issue the phase-2 continuation shape specified by
[ETSI TS 124 090](https://www.etsi.org/deliver/etsi_ts/124000_124099/124090/10.00.00_60/ts_124090v100000p.pdf):
a FACILITY message containing an `UnstructuredSS-Request` (`0x3c`) Invoke in
the still-active mobile-originated transaction. NSE-8 v6.00 does not
accept the standards-shaped request tested here. It replies with RELEASE
COMPLETE and cause `0x60` (invalid mandatory information), for both short and
segmented text and with either tested Invoke identifier. The emulator therefore
records this as a product-capability negative; it does not synthesize an
interactive continuation that the firmware has not demonstrated.

`make verify-radio-ussd-outcomes` validates the exact ReturnError and Reject
components and their releases, the exact continued-request rejection and its
release, then saves and reloads MAME in the silent active dialogue and requires
that neither a response nor a release appears after restore. These remain
terminal-response, conformance-negative, and no-response contracts. A working
continued dialogue requires an organic handset FACILITY response and remains
unclaimed.

## Network-initiated USSD

`make verify-radio-network-ussd` starts from the registered idle phone and uses
the ordinary IMSI paging, Paging Response and dedicated SAPI-0 path. It covers
the two network-originated operations separately:

- An `UnstructuredSS-Request` (`0x3c`) REGISTER with the phase-2 SS-version
  indicator is rejected by NSE-8 v6.00 with RELEASE COMPLETE cause `0x60`.
  This is retained as an exact product-capability negative; no reply editor is
  synthesized.
- An `UnstructuredSS-Notify` (`0x3d`) REGISTER without the optional version IE
  is accepted. Firmware displays `01 Message: Nokia test network`, sends the
  empty ReturnResult FACILITY `8b 7a 05 a2 03 02 01 01`, accepts the network's
  RELEASE COMPLETE and returns to the idle common-control channel.

The notification result and transaction ownership match the network-initiated
procedure in [ETSI TS 124 090](https://www.etsi.org/deliver/etsi_ts/124000_124099/124090/10.00.00_60/ts_124090v100000p.pdf).
The exact displayed frame, request/reply bytes, paging order and RR release are
all acceptance predicates. The laboratory network constructs Layer 3 messages;
it does not post firmware messages or write handset state.
