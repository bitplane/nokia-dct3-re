# External telephony bridge

The optional MAME host adapter exposes firmware-owned calls, bidirectional SMS
and bidirectional USSD at `ws://127.0.0.1:18080/nokia/dct3/calls`. It carries call decisions and
the conventional 33-octet GSM 06.10 full-rate frame; it does not bypass CC/RR,
DSP speech control, MAD2 PCM or COBBA audio routing.

Run the provisioned 3210 with the host adapter and MAME's ordinary audio
endpoints:

```sh
make run-interactive \
  INTERACTIVE_EXTRA_ARGS='-cfg_directory ../fixtures/radio_outgoing_host_adapter -http -http_port 18080'
```

In another terminal, attach the standalone echo endpoint:

```sh
.venv/bin/python tools/dct3_call_bridge.py
```

Dial a number on the emulated handset and press Send.  The endpoint accepts the
organic SETUP, returns each good uplink GSM-FR frame as the next downlink frame,
and leaves the independent 20 ms air clock inside emulation.  MAME's configured
microphone therefore crosses COBBA, MAD2 PCM, the DSP HLE and radio uplink
before returning through the complete downlink path to the earpiece.

Press End on the handset for local release.  For a bounded remote release, run
the endpoint with `--hangup-after SECONDS`; it submits GSM normal clearing by
default.  `--once` exits after the firmware completes CC/RR release and MAME
publishes the resulting `ended` state.

The transport epoch changes after save-state restoration.  The endpoint
accepts the republished request and connected media cursors and rejects media
from older identities.  Sequence numbers and timestamps are correlation data;
they never schedule firmware or radio work.

Signaling-only acceptance can use `run_host_call_adapter_gate.py --decision
connect --observe-end`. This keeps the host attached until a correlated
connected-to-ended lifecycle completes within one90s budget, without sending
media or a remote termination. It is not an audio gate; product-specific
physical-input, protocol, storage and UI checks remain required. The
[8290 host contract](8290_bringup.md#host-decided-outgoing-call) is one
independently qualified composition.

## Protocol contract

The WebSocket endpoint speaks protocol version 1. On connection and after a
save-state restore, MAME publishes:

```json
{"type":"call_adapter_ready","protocol_version":1,"epoch":1,"calls_idle":true,"capabilities":["network_state","calls","gsm_fr_media","sms","ussd"]}
```

The additive `capabilities` array lets hosts discover the services implemented
by this adapter without inferring them from the phone profile. Hosts written
against the earlier version-1 ready message may ignore it.
The additive `calls_idle` boolean snapshots adapter/session call ownership
before individual transactions are republished. `true` means no incoming
request, pending outgoing request, connected outgoing call or alerting outgoing
call. It does not assert network registration or availability of other services.
Hosts lacking this field must not infer idle from a temporary absence of call
events.
The SIP bridge treats an explicit non-idle snapshot on its first connection
the same as an active save-state restore: it does not redial the republished
request or resume external media, and clears the handset transaction through
the correlated non-connecting decision/termination path. A pending request
needs both messages; an already accepted request needs only termination.
An explicit idle initial snapshot still permits a fresh outgoing call.
Restoration release is correlated by direction as well as epoch and request
ID: an opposite-direction `ended` event cannot clear the guarded transaction.
Executable loopback tests cover both incoming and outgoing connected initial
snapshots, repeated state publication, and a fresh call after matched release.

Every host-to-MAME message carries the current `epoch` and a positive
`request_id`. A restore increments the epoch, invalidates queued host input and
republishes emulator-owned call state. A host must discard the old identity;
it must not resend an `incoming_call` that MAME has already accepted.

| Direction | Message | Required payload |
|---|---|---|
| MAME to host | `outgoing_call` | `epoch`, `request_id`, decimal `digits`; additive `decision_pending` boolean snapshots whether the session still needs a host decision |
| Host to MAME | `outgoing_call_decision` | identity plus `decision`: `connect`, `busy`, or `no_answer` |
| Host to MAME | `incoming_call` | identity plus 1..20 decimal `caller` digits |
| MAME to host | `*_call_state` | identity and `phase`; `media_closed` stops media admission before final `ended` completes CC/RR teardown; an incoming request may terminate as `expired` before paging; connected snapshots also carry both media cursors; a `forwarded` incoming state carries `forwarding_reason` and the decoded decimal `forwarding_destination` |
| MAME to host | `*_call_media_uplink` | identity, sequence, emulation timestamp, good/BFI flag, 33-octet GSM-FR frame as 66 lowercase hex characters |
| Host to MAME | `*_call_media_downlink` | identity, host sequence, source timestamp, and one encoded GSM-FR frame |
| Host to MAME | `*_call_terminate` | identity and GSM cause in `1..127` |
| MAME to host | `outgoing_sms` | identity, decimal `recipient`, `alphabet`, TP user-data length and packed user data as lowercase hex |
| Host to MAME | `outgoing_sms_decision` | identity plus `decision`: `accept`, `rp_error`, or `rp_silence` |
| MAME to host | `outgoing_sms_state` | identity and `phase`: `accepted`, `rejected`, or `ended` |
| Host to MAME | `incoming_sms` | identity, decimal `sender`, `alphabet`, user-data length and packed user data |
| MAME to host | `incoming_sms_state` | identity and `phase`: `queued`, `delivered` or `expired` |
| MAME to host | `network_state` | `epoch`, registration status and, while registered, serving-cell identity and signal level |
| MAME to host | `outgoing_ussd` | identity, DCS and packed USSD request data |
| Host to MAME | `outgoing_ussd_response` | identity, outcome and either DCS plus packed response data or an error/problem code |
| MAME to host | `outgoing_ussd_state` | identity and `phase`: `accepted` or `ended` |
| Host to MAME | `incoming_ussd` | identity, DCS, packed data and optional `operation`: `notify` (default) or `request` |
| Host to MAME | `incoming_ussd_terminate` | epoch and request identity of an active unanswered request; network RELEASE COMPLETE, not a handset reply |
| MAME to host | `incoming_ussd_response` | identity, DCS, packed handset reply and `empty` parameter flag, after ordinary RR release |
| MAME to host | `incoming_ussd_state` | identity and `phase`: `queued`, `delivered`, `answered`, `cancelled` or `expired` |

The `*` is direction-specific (`incoming` or `outgoing`) and must match the
call. Frames are conventional GSM 06.10 full-rate payloads, not PCM. The host
does not own paging, CC/RR state, radio timing, keypad decisions, codec routing
or release completion. Queue overflow, stale epochs, duplicate decisions and
wrong-direction media are rejected without changing emulated call state. The
sixteen-event ingress limit is aggregate across all message classes, so mixed
media and control traffic cannot evade it. An admitted incoming service which
cannot obtain its paging entrance within sixty seconds of emulation time is
cancelled and reports `expired`; active firmware-owned transactions are not
timed out by the host adapter.

Media closure is not final call closure: stop audio on the correlated
`media_closed` event and retain the dialog until `ended`. This distinction
is runtime-checked on both NSM-3 and the existing 3210 composition. Fresh
`run_3210_sip_media_closed_regression` passes the complete outgoing SIP gate,
with media closure at 32.380 s and final completion at 32.410 s. These are
fixture observations, not fixed hardware delays or a portable timing policy.
Shared bridge/epoch/restoration tests also preserve the fresh-epoch/no-redial
behavior. Do not restore an external SIP dialog from a MAME save state.

The outgoing SMS acceptance runner uses a single 180-second wall-clock
budget after submitting its host decision (`--completion-timeout` is a finite
positive override). It waits for the current epoch/request's `ended` state;
unrelated events cannot restart the budget. This accommodates the independently
verified throttled NPE-3 RP-silence lifecycle, whose firmware completion arrives
about 67 seconds after the host decision, without changing emulated timers.

`network_state` is published on connection, save-state restoration,
registration changes and serving-cell changes. A host should wait for
`registered: true` before submitting incoming calls or SMS. Its registered
snapshot includes MCC, MNC, ARFCN, BSIC, LAC, cell ID and RX level. These are
the configured network facts, not values inferred from the LCD.

The SMS payload stays in its GSM representation. `gsm7` reports septet count
in `user_data_length` and carries the packed octets in `user_data`; `8bit` and
`ucs2` report octet count. The adapter does not reinterpret binary UDH or lose
alphabet information. `verify-radio-outgoing-sms-host-adapter` proves an
organic composer-to-host request and the correlated RP result through the
ordinary SAPI-3 transaction.
`verify-radio-outgoing-sms-host-restore` saves while that host decision is
pending, requires republication under a new epoch, and rejects the stale
pre-restore decision before completing normally.
NSB-7 independently covers the same pending-decision boundary with its own
authenticated physical Reply, PCS600 registration and complete card/flash/RTC
checks. `nsb7_host_sms_submit_state_check.py` requires exact37-register/RAM/time
restoration, unchanged request1 under epoch2, explicit stale-epoch rejection,
one accepted decision, one submission and one EF_SMSS update. Physical recovery
and reviewed individual frames are required; neither a host callback nor an
old-epoch generic rejection alone establishes this contract. See
[NSB-7 pending outgoing host SMS restoration](8290_bringup.md#pending-outgoing-host-sms-restoration).
The independent [incoming ingress-restoration gate](8290_bringup.md#incoming-host-sms-ingress-restoration)
admits an external SMS once, saves at observed CP-DATA ingress and requires
queued/delivered in both reference/restored epochs without resending. Exact
architecture/protocol/card/clock replay, whole final-card content and physical
hello reading are required. The host runner waits beyond reference delivery
when`--require-restore` is selected and rejects unhealthy handset console output.

For a text-only command-line ingress test, the standalone endpoint packs GSM
7-bit text and submits it after the handset has registered:

```sh
.venv/bin/python tools/dct3_call_bridge.py --incoming-sms hello --once
```

The generic wire message also admits `8bit` and `ucs2`; callers must supply the
already encoded TP user data and its alphabet-specific logical length. The
network constructs SMS-DELIVER and owns only the external network side.

CLI text uses the GSM default and extension alphabets, not ASCII byte values.
`@` and `_` have their GSM code points; braces, brackets, backslash, caret,
tilde, pipe, form feed and the euro sign use ESC-prefixed extension codes.
Each extension character counts as two septets in SMS `user_data_length`.
Single-message CLI ingress is limited to 160 septets and rejects unmappable
text rather than silently substituting or changing the alphabet. The shared
`tools/gsm7_text.py` implements the tables and packing rules from
[TS 23.038 v3.3.0, sections 6.1.2.3/6.2.1/6.2.1.1](https://www.etsi.org/deliver/etsi_ts/123000_123099/123038/03.03.00_60/ts_123038v030300p.pdf).

The 6250 `host-incoming-sms-text` acceptance scenario sends `@_{}` from
the external host. It requires CP/RP/RR closure, exact six-septet SMS-DELIVER
storage, physical Read and the reviewed four-character handset frame:

```sh
.venv/bin/python tools/run_noki6250_acceptance.py run_host_text --scenario host-incoming-sms-text
```

This proves that specific handset text path, not glyph coverage for every
alphabet character or national-language tables. `run_host_incoming_sms_gate.py`
also accepts `--text` for independently reviewed fixtures; its default remains
`hello` for the existing acceptance gates.
`verify-radio-incoming-sms-host-adapter` requires paging, firmware CP/RP
acknowledgement and SIM-backed storage before reporting `delivered`.
`verify-radio-incoming-sms-host-restore` saves during that admitted delivery;
the adapter republishes `queued` under the new epoch and reports exactly one
post-restore completion.
The runner validates each epoch's SMS events as an ordered prefix of
`queued`, `delivered`, with the admitted request identity. A reference branch
may end before delivery, but duplicates, reversed phases, unrelated requests
and regressing or repeated restore epochs fail. Restoration does not resend
the host request; the new epoch must independently reach delivery. Handset
console health and product-specific protocol/storage checks remain separate
requirements.

`run_host_incoming_ussd_gate.py` applies the same console-health and
per-epoch notification-order checks. Its optional `--ready-marker` waits for
a flushed fixture console marker before host admission; it neither writes
handset state nor changes device timing. NSB-7 uses this to send `Host notice`
after physical PIN/phone unlock, then qualifies exact text, idle/menu
continuation and unchanged card with `nsb7_host_ussd_check.py`. This is
notification acceptance, not an interactive USSD reply session. The existing
NSE-8 `verify-radio-incoming-ussd-host-restore` also passes with the shared
runner's restored-epoch handling.

NPE-3 independently uses that ready-marker contract in
`verify-6210-host-ussd-notify`: physical PIN1 authentication and coherent
registration precede admission, original firmware acknowledges/releases the
notification, and physical Back/Menu restore reviewed idle/menu pixels.
Its complete card matches normal registration, not another handset's profile.
Its separate `verify-6210-host-ussd-request` admits a network request after
the same ready boundary, physically replies2 and requires the original
ReturnResult, RR release, exact correlated JSON received by the host and six
own-product UI frames. `verify-6210-host-ussd-restore` additionally saves the
unanswered editor, checks exact ARM/RAM restoration and requires one physical
reply under the new host epoch, without another request admission or REGISTER.
This restores the handset/session state, not an external network process.

The USSD host contract preserves the BER-decoded DCS and packed payload. A
`success` response carries `dcs` and hexadecimal `data`; `return_error` and
`reject` may carry `error_code`. The session builds the correlated GSM
supplementary-service component, and `verify-radio-ussd-host-adapter` requires
the handset's organic `*123#` request, host response acceptance, firmware UI
delivery and the ordinary LAPDm channel release.
NSB-7 independently qualifies physical `*123#`, rejection of a wrong request
ID, the matching `Host network` response, exact response/idle/menu frames and
unchanged complete SIM contents through `nsb7_host_ussd_check.py --request`.
The shared outgoing runner checks stable request identity, per-epoch
uniqueness, accepted-before-ended ordering and complete-console health.
NSE-8's existing `verify-radio-ussd-host-restore` remains green. NSB-7 has a
separate `--request --restore` qualification: exact CPU/RAM/time at its pending
request checkpoint, same-request republication under epoch2, stale-epoch and
wrong-ID rejection, response/idle/menu frames, unchanged card and retained RTC.
Only the restored branch receives the host result, so equal protocol replay
is not claimed. Neither gate proves multi-turn dialogue continuity.

Host-originated `incoming_ussd` defaults to a notification. It enters through registered-idle
paging and is reported `delivered` only after the handset ReturnResult and
channel release. The standalone bridge can exercise it with
`--incoming-ussd 'Host notice' --once`; the generic wire contract accepts
already packed data rather than assuming a text alphabet.
An explicit `"operation":"request"` selects network `UnstructuredSS-Request`.
The firmware owns the editor and physical input. A correlated typed ReturnResult
is retained until RR release, then published once as `incoming_ussd_response`
before state `answered`. A handset RELEASE COMPLETE without a result ends as
`cancelled`; no reply is fabricated. Unknown operation strings are rejected.

For an active unanswered host request, `incoming_ussd_terminate` with the current
`epoch` and `request_id` asks the network application to end the dialogue.
TS 24.090 section5.1.1 assigns this termination to network RELEASE COMPLETE.
The command queues that Layer3 message; it does not clear radio/host state.
Only the ordinary acknowledged SS/RR release publishes `cancelled`. A stale
epoch, different request, duplicate, notification, queued-but-not-active request
or already completed response is rejected without replacing pending downlink
traffic. There is no automatic timeout or synthesized handset ReturnResult.
The 6210 physical-dismissal/host-termination fixture is reproducible with
`.venv/bin/python tools/run_noki6210_ussd_termination.py tmp/ussd-termination`.
Adding `--restore` checkpoints the dismissed unanswered transaction and requires
new-epoch cancellation while rejecting the pre-load epoch. It does not qualify
an in-flight release checkpoint or restore the external host process.
Both variants require physical PIN authentication before admission, complete
expected SIM contents, released input contacts and the existing exact Messages
recovery image. Restoration additionally checks raw all37 ARM/banked registers,
PC/SP, backed-RAM digest and time using `arm_architecture_snapshot_check.py`;
the Lua pass marker alone is insufficient architectural evidence.
The backward-compatible standalone bridge CLI still sends notifications;
the request acceptance runner uses `--request --expected-reply 2`.
Authenticated NSB-7 acceptance is documented in `8290_bringup.md`. Arbitrary
host multi-turn continuation is not qualified. A separate NSB-7 pending-editor
save/load gate replays the identical physical reply under epochs 1 and 2;
each host response follows its own branch's RR release, and the saved request
is republished as queued under the new epoch without a second admission.
NSB-7 End at the open editor instead sends an empty ReturnResult. Its host
response preserves `empty:true`, DCS0 and empty data, followed by `answered`;
it must not be mistaken for a typed response or the distinct no-result
RELEASE COMPLETE/`cancelled` path. This is separately checked with runner
`--request --empty-reply` and the physical dismissal fixture in `8290_bringup.md`.
CLI USSD text uses the same GSM tables but its own padding: a spare septet is
CR rather than zero (`@`), and an intended final CR on an octet boundary is
protected by another CR. USSD does not carry an SMS TP-UDL. These boundary
cases have executable packing tests; ordinary host USSD lifecycle acceptance
remains separate from handset acceptance of every such string.
The 3210 seven-septet boundary is independently verified: `1234567` displays
without a spurious trailing `@` and completes the notification/RR lifecycle.
Reproduce this text-specific check sequentially:

```sh
make verify-radio-incoming-ussd-host-adapter RUN_DIR=run_host_ussd_seven HOST_INCOMING_USSD_TEXT=1234567
.venv/bin/python tools/radio_host_incoming_ussd_trace_check.py run_host_ussd_seven/error.log --seven-text-frame run_host_ussd_seven/snap/noki3210/0000.png
```

This exact frame covers that one text vector, not all alphabet glyphs or
trailing-CR presentation. The incoming runner's `--text` option rejects empty,
unmappable and over-160-octet text before launching MAME.
`verify-radio-incoming-ussd-host-restore` saves after admission and requires
the queued state to be republished under a new epoch before the one organic
firmware completion is reported.

The forwarding reason is one of `unconditional`, `busy`, `no-reply` or
`not-reachable`. It records the network subscription which made the routing
decision; it is not inferred from the last UI frame. The destination is the
number registered organically by the handset's supplementary-service request.

`verify-radio-incoming-call-host-adapter` is the complete external-origin
contract gate. `verify-radio-incoming-call-host-restore` repeats the connected
call across save/load and proves epoch/cursor republication before remote
release. Its 80-frame quota counts only valid frames echoed under the restored
epoch; pre-save media and stale-epoch uplink cannot satisfy restored transport
coverage. The non-restoration gate retains its ordinary total-frame quota.

To originate from the external endpoint instead, run MAME with
`fixtures/radio_incoming_host_adapter` and attach:

```sh
.venv/bin/python tools/dct3_call_bridge.py --incoming-caller 447700900123
```

The adapter accepts one bounded request, waits until the phone is registered
and idle, and asks the radio peer to page the registered identity. The caller
is encoded as the GSM Calling Party BCD IE in the ordinary network SETUP.
Ringing, physical Answer, assignment, GSM-FR media and local End remain
firmware/radio-owned. `--hangup-after` sends a network DISCONNECT; this ROM may
return CC Release Complete before the LAPDm acknowledgement, so both orderings
are correlated before `incoming_call_state` reaches `ended`.

## SIP backend prerequisite

An independent loopback probe verifies upstream PJSIP 2.16, rather than a
home-written SIP parser. Two sequential calls reverse endpoint roles, negotiate
GSM/8000 RTP, reach CONFIRMED and release with 200 OK. Null audio supplies no
microphone signal: each receiving endpoint records the other endpoint's WAV
source, and the checker requires the expected tone energy, rejecting silence
and a wrong frequency. This is stack-only evidence: no MAME, handset, native
DSP, concurrent full-duplex bridge, registrar or public-network call is involved.

The reviewed release archive is
`https://codeload.github.com/pjsip/pjproject/tar.gz/refs/tags/2.16`, SHA-256
`3af2e481d51aaa095897820fa2ee26c30e530590c6ca56d23e4133bbdad369eb`.
Build outside tracked source; the upstream build was run serially because its
parallel object-directory creation raced in this environment:

```sh
ROOT="$PWD"
mkdir -p tmp/sip-build
curl -L --fail https://codeload.github.com/pjsip/pjproject/tar.gz/refs/tags/2.16 -o tmp/sip-build/pjproject-2.16.tar.gz
tar -xzf tmp/sip-build/pjproject-2.16.tar.gz -C tmp/sip-build
cd tmp/sip-build/pjproject-2.16
./configure --disable-video --disable-sound --disable-ssl CFLAGS=-fPIC CXXFLAGS=-fPIC
make dep
make
cd "$ROOT"
.venv/bin/python tools/run_sip_stack_probe.py --pjsua tmp/sip-build/pjproject-2.16/pjsip-apps/bin/pjsua-x86_64-pc-linux-gnu --run-dir tmp/sip-probe
```

Use a new run directory for repetition. Bindings and destinations are loopback;
this TLS-disabled probe build is not a deployment configuration. Runtime
evidence includes both endpoint logs, original/received WAV files and a result
manifest. The reviewed 440/660 Hz recordings exceed 0.999 tone-energy fraction.

[PJSUA2 AudioMediaPort](https://docs.pjsip.org/en/latest/specific-guides/audio/audio_frame_manipulation.html)
provides application-owned PCM callbacks. The standalone call backend now
maps SIP decisions to existing call identities; GSM-FR decode/encode connects
bounded 8 kHz PCM queues to that port.
Neither SIP nor host wall-clock scheduling belongs in the emulated GSM/device
state. Restoration must invalidate the old SIP call identity instead of
replaying an already accepted incoming call. Python SWIG bindings require a
separate local build; no installed PJSUA2 binding is assumed by the stack probe.

## SIP call bridge

The handset runner builds and prepares the selected machine and BIOS. Only
3210 gates generate the erased-identity security fixture and install its EEPROM
restore guard; sibling gates neither provision nor restore that fixture. All
products retain isolated run-local NVRAM and logs.

`verify-3330-radio-outgoing-call-sip` first completes the NHM-6 physical
security/time setup with its own PMM, then boots a separate call process using
that product-local NVRAM. It verifies the captured 18-byte SETUP, real SIP
confirmation, sustained ordered GSM-FR downlink and bidirectional host media,
then ordinary CC/RR release. Queue-drop counts remain visible in the result;
this is HLE media transport, not native DSP speech or an analogue-waveform gate.
`verify-3330-radio-incoming-call-sip` independently repeats that physical
provisioning, receives a real SIP INVITE and presses Navi only after observing
firmware CC Alerting. Its own CONNECT (`8307`) and remote-clearing RELEASE
COMPLETE body are checked; the CC sequence bit is not a fixed product property.
Physical Answer, bidirectional HLE transport and normal clearing are required.
PUP buzzer readiness is not used as its Answer trigger: the measured incoming
call reaches Alerting without asserting that bit in this composition.

`verify-3410-radio-outgoing-call-sip` cold-boots NHM-2 v5.46E with its own
product inputs, dismisses the startup UI with End, physically dials `5551234`
and presses Send. Its captured 15-byte SETUP matches the NSE-8 fixture bytes,
but its keypad, storage and DSP/PCM profile remain product-local. The gate
requires actual SIP confirmation, sustained ordered accepted downlink frames,
bidirectional HLE host media and firmware CC/RR release. Queue-drop counts are
retained; neither lossless media nor native DSP speech is claimed.
`verify-3410-radio-incoming-call-sip` independently cold-boots the same product,
dismisses startup with End, observes firmware CC Alerting and physically presses
Send. Its CONNECT (`8307`), remote-clearing RELEASE COMPLETE body, handset-owned
answer decision, sustained accepted downlink and bidirectional HLE media must
precede normal clearing. Navi/Enter is not accepted as its physical Send marker.

`verify-5210-radio-outgoing-call-sip` independently cold-boots NSM-5 v5.40E,
physically dials `5551234` and presses Send using its established startup
cadence. Its captured SETUP is 15 bytes; the shared wire encoding does not
substitute another product's DSP, PCM, storage or keypad profile. Its own
`noki5210.cfg` enables the laboratory `CALLHOST` configuration. Actual SIP
confirmation, sustained ordered accepted downlink, bidirectional HLE host media
and firmware CC/RR release are required. Queue drops remain visible, and no
native DSP speech or new physical-waveform claim is made.
`verify-5210-radio-incoming-call-sip` independently cold-boots NSM-5, physically
presses Send at its evidenced 18-second startup point, and requires the
handset-owned SIP Answer decision, CONNECT `8307`, sustained accepted downlink
and bidirectional HLE media before complete remote CC/RR clearing. The fixture
uses its own enabled `CALLHOST` configuration; Navi is not a substitute for Send.

`tools/dct3_sip_bridge.py` supports a single explicit SIP destination. It waits
for SIP confirmation before accepting the handset's outgoing request, maps
486/600 to busy and other pre-confirmation failures to no-answer plus explicit
clearing, and maps remote release to GSM normal clearing. Handset completion
hangs up the SIP leg.
The bridge does not auto-accept SMS/USSD. No registrar, credentials or destination-number
routing policy is claimed. The explicit destination is independent of the
physically dialed number, which is logged for acceptance evidence.

Incoming SIP requires a registered idle host connection and a numeric SIP user
of 1..20 digits; unavailable/busy ingress receives 486 and an unsupported caller
identity receives 484. A retained PJSUA2 Call owns the dialog throughout:
destroying a temporary Call would hang up that dialog. The bridge sends 180
while ordinary MAME paging/ringing proceeds, and sends SIP 200 only after the
handset publishes connected following physical Answer. SIP release requests
GSM clearing; handset end hangs up the SIP dialog. There is no host-generated
key press, paging shortcut or firmware-state change.

Each call owns independent libgsm encoder/decoder state. Media callbacks only
exchange 160-sample, native-endian signed PCM blocks through bounded eight-frame
queues; they never call WebSocket or emulate hardware. Queue overflow is counted.
Media activation is owned by the exact current PJSUA2 Call object. Late media
callbacks from released or replaced dialogs cannot create or attach a port for
the new dialog, even when a request identity is reused. Each port retains only
its own call's queues.
Signalling callbacks enforce the same object ownership: late confirmation or
disconnect from a released/replaced Call cannot enter the event queue or clear
a replacement dialog, including one with a reused epoch/request identity.
Executable callback tests cover both stale states and the current-dialog
positive control; this is host lifecycle coverage, not native DSP speech.
Null-device PJSIP timing drives the host media port. Run MAME throttled for this
real-time backend. Its media sequence remains host correlation, not an
emulation clock. A changed MAME epoch hangs up the old SIP leg and clears codec
state; replayed call state is cleared with temporary-failure cause 41 rather
than silently redialed. That restoration policy is implemented but does not yet
have a handset save/load acceptance gate.

Build the optional upstream binding in the local source tree (no system install):

```sh
.venv/bin/python -m pip install swig setuptools
ROOT="$PWD"
cd tmp/sip-build/pjproject-2.16/pjsip-apps/src/swig/python
PATH="$ROOT/.venv/bin:$PATH" make PYTHON_EXE="$ROOT/.venv/bin/python"
cd "$ROOT"
```

Alternatively invoke that `make` with the absolute project `.venv/bin` paths.
`SIP_PYTHON_PATH` points at the generated `pjsua2.py` directory and its
`build/lib.*` extension directory; `SIP_PJSUA_BIN` selects the upstream executable.
Both derive their local-build defaults from `SIP_PROJECT_DIR`, which defaults
to disk-backed `tmp/sip-build/pjproject-2.16`. Set that variable to reuse a
retained build elsewhere; explicit binary/binding overrides remain supported.
Make exports these paths to delegated physical-audio scripts. They remain
optional dependencies; no build is downloaded automatically.

```sh
make verify-radio-outgoing-call-sip RUN_DIR=run_3210_sip_gate
make verify-radio-incoming-call-sip RUN_DIR=run_3210_sip_incoming_gate
```

The gate builds the erased-identity security-code fixture before preparing a
fresh isolated run, physically unlocks and dials `5551234`, then requires SIP
confirmation, at least 100 executed PCM/media frames in each direction, the
firmware's exact SETUP/CONNECT ACK/RELEASE and final LAPDm release. The remote
endpoint supplies a 660 Hz WAV source and clears the call after eight seconds.
The own 3210 HLE gate proves signaling and media transport, not native speech,
sound perception or a public-network end-to-end call.
The incoming gate waits for registration before originating a numeric SIP
caller, physically unlocks and answers after the firmware's ringing output,
then requires the ordered paging/SETUP/physical key/CONNECT/media/remote
termination/RELEASE COMPLETE/host-ended chain. Its remote caller uses null
audio: this gate proves incoming signaling and media transport, not an incoming
non-silent waveform. The independent waveform probe remains separate evidence.

### Final SIP failures

A `no_answer` host decision is an alerting policy, not a final failure: the
GSM session deliberately waits for another clearing input. A final non-busy SIP
failure therefore submits that decision followed by a correlated termination;
the session queues clearing through its own assignment/alerting lifecycle and
never sends CONNECT. SIP 480 uses cause 18, following the SIP-to-ISDN table in
[RFC 3398 section 8.2.6.1](https://www.rfc-editor.org/rfc/rfc3398.html#section-8.2.6.1),
not the inverse table. Busy uses the existing GSM cause-17 decision without
an extra clearing message.

```sh
make verify-radio-outgoing-call-sip-busy RUN_DIR=run_3210_sip_busy
make verify-radio-outgoing-call-sip-unavailable RUN_DIR=run_3210_sip_unavailable
make verify-radio-outgoing-call-sip-forbidden RUN_DIR=run_3210_sip_forbidden
make verify-radio-outgoing-call-sip-not-found RUN_DIR=run_3210_sip_not_found
make verify-radio-outgoing-call-sip-service-unavailable RUN_DIR=run_3210_sip_service_unavailable
make verify-radio-outgoing-call-sip-global-busy RUN_DIR=run_3210_sip_global_busy
make verify-radio-outgoing-call-sip-timeout-response RUN_DIR=run_3210_sip_timeout_response
```

These gates physically dial the 3210 into actual upstream PJSIP responses and
require each request's busy decision or no-answer decision/termination cause
after SETUP and the host request, before CC/RR release; an earlier or later
matching event cannot satisfy the transaction. They also require firmware
RELEASE/RELEASE COMPLETE and LAPDm
release, with no SIP/GSM connection and zero executed bridge media. The 480
gate additionally requires the GSM session to consume cause 18.
The 403 gate additionally requires ordered consumption of the correlated
no-answer decision and cause-21 termination before firmware release. It passes
against the local real PJSIP endpoint, without connection or media; this does
not establish authentication retries or a public-network call.
The 404 and 503 gates independently pass against actual local PJSIP responses
and require ordered no-answer decisions plus exact cause-1 and cause-41
terminations, respectively, before clean firmware release. Numeric prefixes
cannot satisfy cause or outcome checks. These remain HLE signaling gates,
not public-network or native speech validation.
The real local 600 response independently passes the cause-17 busy-decision
route, complete firmware release and zero connection/media. An explicit
local SIP 408 response independently passes ordered no-answer/cause-102
termination and the same clean release. The latter does not measure SIP
transaction timer expiry, dropped packets or an unreachable endpoint.
The release checker admits either CC send-sequence bit value without changing
the decoded
primitive. These gates do not measure failure-screen presentation.

The implemented mapping subset also covers 603 -> 21, 604 -> 1,
and 500 -> 41; these have pure mapping tests, not handset runtime
gates. Unhandled statuses use an explicit cause-41 fallback policy. Warning and
Reason headers, authentication retries and full RFC 3398 interoperability are
not implemented.

### Incoming cancellation

```sh
make verify-radio-incoming-call-sip-cancel RUN_DIR=run_3210_sip_cancel
make verify-3310-radio-incoming-call-sip-cancel RUN_DIR=run_3310_sip_cancel
make verify-3330-radio-incoming-call-sip-cancel RUN_DIR=run_3330_sip_cancel
make verify-3410-radio-incoming-call-sip-cancel RUN_DIR=run_3410_sip_cancel
make verify-5210-radio-incoming-call-sip-cancel RUN_DIR=run_5210_sip_cancel
```

The real SIP caller cancels only after the handset reports alerting. The gate
requires CANCEL/487, exactly one correlated GSM termination, network DISCONNECT,
firmware RELEASE COMPLETE, acknowledged LAPDm Channel Release and host `ended`,
with no physical Answer, connection or bridge media on each product. The checker
also rejects connected handset states and accepted handset-media events even
when the bridge's final counters are zero. CC completion
must survive the subsequent RR release whether it arrives before or after the
DISCONNECT acknowledgement; both orderings represent an already completed CC
transaction. Stopping the traffic channel alone is not the cancellation gate's
completion criterion. This covers cancellation while alerting, not every SIP
transaction race or cancellation before paging.

`verify-6210-sip-cancel` independently covers the research-HLE NPE-3 v5.56
profile with unchanged acquired PMM. Its private runner waits for a fresh
registered-idle artifact before INVITE, verifies the same unanswered
CANCEL/487 and clean CC/RR release, pins `1 missed call`, and physically
dismisses it with the right softkey before checking registered idle again.
It does not inherit the five media-capable profiles' answered-call coverage.
The generic SIP runner rejects media-acceptance fixtures for signaling-only
products `6210`, `6250` and `8890`. The 6210 additionally admits only the
single incoming/outgoing SIP-200 `--signaling-only` fixtures described below.
The 6250 admits its separately checked incoming/outgoing SIP-200 signaling fixtures;
the 8890 admits its separately checked incoming/outgoing SIP-200 signaling fixtures.
recording and multiple calls remain excluded. Connected restoration is
limited to the independently verified signaling-only fixtures below.
The 8210 and 8850 have narrowly scoped
single-call outgoing and incoming SIP-200 HLE media-transport fixtures and
a separate connected-outgoing restore fixture. Explicit recording is
available for media fixtures without asserting waveform success.

`verify-6210-sip-outgoing-busy` additionally proves physical `1234567`/Send
against actual local PJSIP 486. It decodes the SETUP number, requires the
correlated busy decision and complete CC/RR release, rejects CONNECT and
media, and checks exact registered-idle pixels afterward. The isolated
research-HLE runner retains unchanged acquired PMM and own bootstrap,
self-test, registration and persisted SIM-location checks. The failure and
connected-signaling fixtures, pending-outgoing restoration and
alerting-incoming restoration are enabled for the 6210;
speech remains unproved.

`verify-6210-sip-connected-signaling` physically dials `1234567`, confirms a
real loopback PJSIP-200 dialog and ends it with the handset End key. The own
product checker requires exact SETUP digits, CC connection/acknowledgement,
traffic assignment and full CC/RR release; the product runner additionally
checks own uploads/self-test, registration/EF_LOCI and registered-idle pixels.
The bridge reports zero uplink and the trace must contain no HLE speech tick.
Remote RTP reception and downlink queue activity are recorded as observations,
not handset playback. `sip-result.json` explicitly records
`speech_tested=false` and `native_dsp_complete=false`. This gate establishes
an answered signaling dialog, not an audible or end-to-end voice call, and
does not authorize a guessed NPE-3 PCM clock.

`verify-6210-sip-incoming-connected-signaling` waits for a fresh registered-idle
artifact before admitting caller `5551234`. Physical Send/Answer is required
before the correlated SIP confirmation; physical End then closes CC/RR and
returns to exact registered-idle pixels. The own incoming-call checker
requires paging, assignment, one CONNECT and one DISCONNECT. Both the bridge's
handset-owned Answer marker and the decoded key must be present. The same
zero-uplink/no-codec guard and explicit non-speech result scope apply.

`verify-6210-sip-pending-outgoing-restore` physically dials `1234567`, saves
during SIP 180, and restores handset PC, SP, RAM checksum and emulated time
exactly. The external dialog is cleared, not restored: CANCEL/487, one
accepted cause-41 termination, complete CC/RR release and exact registered
idle are required. CONNECT, accepted media and redial are rejected. This is
research-HLE signaling coverage, not native DSP or speech evidence.

`verify-6210-sip-alerting-incoming-restore` admits a fresh INVITE only after
the registered-idle artifact, saves while organically alerting, and restores
PC, SP, RAM checksum and time exactly. The host rejects the outstanding
INVITE and clears the handset once under the new epoch. Complete CC/RR
release, one missed-call notification, physical Exit and exact registered
idle are required; Answer, CONNECT, accepted media and dialog replay are
rejected.

`verify-6210-sip-connected-outgoing-restore` physically dials `1234567` and
saves after a real SIP-200 connection. Restoring the handset clears, rather
than resurrects, the external dialog: one BYE, one cause-41 termination,
complete CC/RR release and exact registered-idle recovery are required.
The architectural round trip checks PC, SP, RAM and emulated time. The bridge
reports its uplink count before discarding old-epoch counters; zero uplink
and absence of HLE codec ticks are mandatory. No redial, native speech or
audible playback is established by this signaling-only gate.

`verify-6210-sip-connected-incoming-restore` admits one fresh caller
`5551234` after registered idle, physically answers it, then saves and
restores exact handset CPU/RAM/time after connection. The bridge requires
the ordered original call identity through Answer and SIP confirmation,
clears that dialog with BYE on epoch change, and terminates the restored
handset call exactly once with cause 41. Full CC/RR release and exact idle
pixels are checked. Old-epoch uplink is zero and no HLE codec tick may run;
external dialog resurrection, speech and native DSP completion are not
claimed.

`verify-6250-sip-connected-signaling` uses physical `123`/Send/End on the
declared NHM-3 initial-record comparison. Its own checker requires the
carrier-19 traffic and release contract, complete CC/RR clearing and resumed
paging, with reviewed idle recovery and persistent SIM location. The SIP
dialog confirms and releases normally, but uplink remains zero and no HLE
codec tick is permitted. `sip-result.json` retains explicit non-speech and
non-native scope. No 6210 PCM assumptions are enabled for the 6250;
connected restoration is limited to the separately checked fixtures below.

`verify-6250-sip-incoming-connected-signaling` waits for a fresh registered
idle frame before admitting caller `5551234`. Ordered unique Answer and SIP
confirmation must belong to the original call identity. Physical Answer/End
are checked through the own carrier-19 incoming CC/RR lifecycle, release
confirmation and resumed paging. Reviewed pre/post idle pixels and persistent
SIM location must hold. Bridge uplink remains zero and no HLE codec tick may
execute; remote PCM reception does not establish handset playback or speech.

`verify-6250-sip-connected-outgoing-restore` saves a physically dialed `123`
call only after real SIP-200 and handset connection. Exact R15/R13/RAM/time
restoration triggers BYE and one cause-41 clear under the new epoch, with
complete CC/RR release, own carrier-19 confirmation, resumed paging and
reviewed idle recovery. The bridge publishes its old-epoch uplink count
before resetting counters; zero uplink and no HLE codec ticks are required.
The external dialog is cleared, not restored or redialed; speech and native
DSP remain unproved.

`verify-6250-sip-connected-incoming-restore` admits a fresh caller after
idle, requires physical Answer decoded as own matrix value `0a`, and saves
after handset/SIP connection. Exact R15/R13/RAM/time restoration clears the
original dialog with BYE and the restored handset once with cause 41. The
own carrier-19 release confirmation, resumed paging and reviewed idle
recovery must pass. The original Answer/confirmation identity is ordered
before epoch change. Zero pre-reset uplink and no HLE codec tick exclude
speech claims; external dialogs are cleared, never resurrected.

`verify-8890-sip-outgoing-media` physically dials `1234567` and ends a real
SIP-200 dialog on unchanged own PMM. `verify-8890-sip-incoming-media` admits
caller `5551234` after fresh clock-settled idle, then uses physical Answer/End.
Both require own GSM900 carrier-60 signaling, decoded keys, complete CC/RR
release, reviewed idle recovery, persistent SIM location and at least100
uplink/downlink and host PCM frames. The independently acquired NSB-6 manual
supplies512kHz/8kHz,64-clock framing; the own control writer and exact PCM
clock/idle counters are checked separately. `--record-media` retains bridge
PCM, but does not by itself qualify nonsilent physical microphone/earpiece
waveforms. The separate `verify-8890-sip-outgoing-waveform` and
`verify-8890-sip-incoming-waveform` gates do qualify sustained440Hz
microphone-to-remote and660Hz remote-to-physical-speaker recordings through
isolated host audio, retaining the same own PCM/call checks and neutral gains.
This is GSM900 research-HLE audio, not native DSP speech or measured analogue
gains.

The connected outgoing/incoming restoration gates save at32/52 seconds,
respectively, after physical input and real SIP connection. Exact
R15/R13/RAM/time restoration invalidates the old dialog with BYE and clears
the restored handset once with cause41. Each gate requires positive old-epoch
uplink/downlink/PCM counters and a pre-save executed codec tick, then full own
carrier-60 release, paging, reviewed idle and persistent SIM location. The
original incoming Answer and confirmation precede epoch change. Pending
outgoing and ringing incoming gates separately prohibit connection and
external-dialog redelivery. Dialogs and media are discarded, not serialized
or replayed. These checks do not execute the missing native DSP mask.

A bounded outgoing SIP-200 research probe on unchanged own-PMM `npe3hle`
confirmed and released the real dialog, but failed full media acceptance:
the bridge counted uplink 0, downlink 354, transmitted PCM 0 and received
PCM 400 (38 dropped). The radio trace emitted bad uplink speech blocks,
not valid handset frames. DSP-HLE doorbells retained `speech_control=0000`;
the product composition selects no speech-control command/predicate, and
`speech_requested()` deliberately rejects an absent contract. Host
downlink queue acceptance therefore does not establish handset decoding
or bidirectional speech. The temporary answered-probe switches were
removed; normal answered/media restrictions remain. Recover the own
MCU command compiler and call/release speech field before configuring
this contract; do not import the 6250 predicate or synthesize uplink.

Own-ROM static recovery (`noki6210_dsp_control_contract.recover`) locates
the 55-selector compiler at `426eb4`, table `426f08`. Selector `11`
(`4271d4`) sets/removes bit `0200` using keep-mask `fdff` in parameter
shadow `16ffe6`; selector `08` (`42722e`) encodes the low 12 bits under
wire prefix `8000`. The checker pins instructions, pointers and the own
image hash. This is an encoding contract, not proof that selector 11 is
the call speech-enable operation. Next trace its callers and shadow writes
across physical call setup/release, including why the observed call only
publishes command-9 words while HLE speech control remains zero.

The full halfword-aligned direct Thumb-BL scan yields 11 candidate callers:
`303278`, `303c7c`, `303c86`, `41ab0e`, `41b07a`, `420c0a`, `420c3e`,
`420c66`, `4272ae`, `45dbfc`, `46b9ee`. This excludes indirect calls and
does not declare every encoded candidate executable. The paired publisher
at `303c76` loads adjacent parameter shadows and calls selectors 8/9.
Fresh physical outgoing calls still pass. Debugger probes armed at the
compiler and mailbox produced no hits, but the existing transport ledger
records mailbox writes at PC `42727c`; the negative debugger result is
therefore invalid as absence evidence. Those ineffective probes were
removed. Establish a working observation hook before using further null
results to classify this active compiler or its field setter.

The mailbox-boundary CPU snapshot resolves that observation problem:
fresh accepted physical outgoing calls publish `870b` at Send and `850a`
after End, both at writer PC `42727c` with compiler selector 8. Bit `0200`
is selected during the call and removed at teardown, independently matching
the own static field encoding. `verify_call_field()` checks the ordered
physical-input/control-write correspondence. This supplies the NPE-3
runtime-HLE speech-request contract (command 8, mask/value `0200`), now
selected only in `npe3hle`, not PCM or native speech acceptance. The full
media retry confirms control `070b` during the call and `050a` on release,
but still fails with uplink/transmitted PCM zero (downlink 353, received
PCM 400, dropped 39). The product's PCM bus profile is unspecified: frame
clock defaults to zero, `block_period()` returns `never`, and the HLE speech
timer cannot run. Normal answered SIP restrictions remain; temporary probe
switches were removed. Recover NPE-3's own PCM clock/framing and COBBA route
before configuring that link; do not fabricate uplink or inherit a sibling
bus profile without evidence. The own control checker now pins nine paired
command8/9 parameter records and their command80/message-byte9 consumer;
[the NPE-3 mode boundary](6210_bringup.md#paired-dsp-control-mode-boundary)
owns that map and the independently observed shared COBBA_GJP_V4.1-part
hypothesis. A positive-controlled physical call uses normal parameter-block
publish call420c0a, not that class40/command80 paired-mode route. The latter's
command80 constructor echoes a reply; it is not an initiating producer.
The normal parameter builder's states0/1 select the mutually exclusive
bits0020/0100 from context byte16ed5c; a coherent physical call observes
context0,0102/050a/070b and stable source/cache blocks. This intermediate
selection does not decode either bit as a clock or microphone route.
The setter's8 direct callers link contexts0..4 to accessory lifecycle
classifications; the unattached-call write watch observes only startup/
initializer zero writes, not a missing context-setter event. Product-owned
caller coverage and unknown accessory identities remain in the NPE-3 doc.
The same COBBA part is documented with differing NSM-3/NSB-7 clocks, so
neither table nor part identity establishes the selected PCM operating mode
or COBBA register semantics. Investigate context selection and DSP-side interpretation rather
than treating service-mode selection as an ordinary-call prerequisite.

Primary board evidence is retained in the ignored ROM research collection:
`roms/research/npe3/npe3-system-module.pdf`, Nokia NPE-3 System Module,
PAMS Technical Documentation, issue 1 (09/00), acquired from
[the service-manual mirror](https://www.eserviceinfo.com/downloadsm/231281/NOKIA_sysmod.html).
SHA256: `acaf85590cee30a1c8640f7411adf41f14fadf3de58cadb74e92be4d8d506d2e`.
Printed page 24, table 8 identifies COBBA-GJP EARP/EARN outputs and
MIC2P/MIC2N connections at system-connector pins 6/7. This establishes
physical audio connections, not the selected internal microphone route,
PCM word format or frame/data clocks. The document's 13 MHz RFI clock and
3.25 MHz RF serial rates describe other interfaces and must not be used
as PCM configuration. Those bus details remain unresolved.

The acquired NPE-3 SD4 V19 schematic (version 1.0, 09.02.2001), sheet
3/11, identifies N240 as `COBBA_GJP_V4.1`. Its explicit PCM net mapping is
`PCM(0)` -> PCMTx/B5, `PCM(1)` -> PCMDCLK/C6,
`PCM(2)` -> PCMSCLK/C5 and `PCM(3)` -> PCMRx/D5.
This corroborates separate data/sync clocks and bidirectional serial data,
but supplies neither clock rates nor edge/word format. The waveform on that
sheet is attached to J200/COBBACLK (RFICLK), not PCMDCLK: it is not a PCM
timing capture. Source: `roms/research/npe3/npe3-schematics-v1.pdf`, SHA256
`559e9718a9dad703694f17d349383f75af4237b1c26cf94ecdb4aafe641f1e43`.

An independently acquired Nokia NPE-3 Troubleshooting, issue 1 (09/00),
[archive](https://www.eserviceinfo.com/downloadsm/25790/Nokia_6210.html)
adds the audio fault chart on printed page 15. It checks the internal
microphone bias at C233 (2.1 V), its path through L200 and associated
components, and the earpiece path through R247/C248/C249 before replacing
N240 COBBA. The accompanying SD4 v19 schematic's audio sheet A-3 was
visually reviewed: it identifies COBBA_GJP_V4.1, fitted microphone/earpiece
paths and the separate PCM nets. This supports board topology, not a live
COBBA input-selection setting or neutral-gain accuracy. Neither document
provides the required PCM clock/framing/edge definition; the NPE-3 media
boundary remains unchanged.

The ignored research collection retains the archive as
`roms/research/npe3/npe3-troubleshooting.rar` (SHA256
`8251aacaec517fd667e341cbaedea2378e837220bc58d4536a8655f3db129100`),
`npe3-troubleshooting.pdf` (SHA256
`f2371b4067111a0e9311e68fc5dead843e415290f87c0afd562e505c9b42a4cb`),
and `npe3-sd4-v19-schematics.pdf` (SHA256
`1e0b057dd6a8ac5d4ef7b030c0f7af8711df044270a18c97fdbed3c31f87b6e6`).

`verify-6210-sip-outgoing-unavailable` independently passes against actual
PJSIP 480, requiring cause 18, complete CC/RR release, zero media and exact
registered-idle recovery with the same physical number and own-PMM checks.

`verify-6250-sip-cancel` independently covers NHM-3 v5.03 research HLE with
the declared initial-record PMM comparison, not factory provisioning.
Own uploads and compact self-test precede coherent carrier-19 registration,
then a fresh idle artifact admits the real INVITE/CANCEL/487 exchange.
Exact own RR deconfiguration/confirmation, resumed paging, zero media,
reviewed missed-call/idle pixels and physical Names/C Exit decoded as `15`
are mandatory. Both the shared preparation and the manifest retain explicit
non-native labels for audit-only shared ROM members. Answered SIP and speech
have not inherited the other products' promotion.

`verify-6250-sip-outgoing-busy` separately verifies physical `123`/Send,
decoded own SETUP, real PJSIP 486 and the correlated busy decision. The
pre-assignment signaling-channel release and own firmware confirmation must
complete before resumed paging and exact registered-idle pixels. It retains
the declared initial-record comparison, zero-media requirement and research-HLE
scope; it does not promote answered calls or speech.

`verify-6250-sip-outgoing-unavailable` additionally passes physical `123`/Send
against real PJSIP 480. Its no-answer decision enters carrier-19 traffic
assignment before consuming cause 18, unlike the pre-assignment busy path.
Own traffic release/confirmation, complete CC/RR clearing, zero media and
reviewed registered-idle recovery are required; CONNECT is forbidden.
`verify-6250-sip-idle-restore` separately requires exact idle architecture,
ordered replay and matching pixels before a fresh INVITE. Its CANCEL/487
result must use epoch 2 with zero media. The post-load observer is started
only after replay completes; no existing SIP dialog is restored.

`verify-6250-sip-alerting-incoming-restore` saves an organically ringing
NHM-3 handset at 40 seconds after the INVITE admitted at 32 seconds. Exact
R15/R13, complete-RAM digest and emulated time must restore. The external
INVITE is rejected, not replayed, followed by one cause-41 clear under
epoch 2. Own carrier-19 deconfiguration/confirmation, resumed paging,
reviewed missed-call pixels, physical Names/C Exit decoded as `15` and
reviewed idle recovery remain required. Answer, CONNECT and accepted media
are forbidden. The shared read-only snapshot helper carries no product
radio, input or provisioning assumptions. This is unanswered restoration,
not native speech or connected SIP coverage.

`verify-6250-sip-pending-outgoing-restore` physically dials `123`/Send and
requires a real SIP 180 before cancelling the request through save/load.
The handset snapshot at 24 seconds restores exact R15/R13, complete-RAM
digest and emulated time. One cause-41 clear completes CC/RR release with
the own carrier-19 deconfiguration body and `expected=00` confirmation;
resumed paging and reviewed registered-idle pixels are required. CANCEL/487
must close the external request without redial, SIP confirmation, CONNECT
or accepted media. This shares the NHM-3 architectural observer with the
ringing fixture, not its physical-input or cleanup policy. External dialogs
are discarded rather than saved; native speech remains unproved.

`verify-8210-sip-cancel` covers NSM-3 v5.31 research HLE with the explicitly
labelled acquired base-record comparison (invalid later journal omitted),
not factory provisioning. Physical startup precedes a real INVITE/CANCEL/487
exchange, complete CC/RR release, zero media, missed-call presentation and
decoded physical Names/C dismissal. Its separate external cell configuration
requires coherent ARFCN4 SCH, MCU tuning/release and host registration. The
post-dismissal numeric `001 01` idle frame is reviewed separately from the
initial `DCT3 LAB`; no name repaint is injected. Native DSP speech remains
unproved.

`verify-8210-sip-idle-restore` adds exact settled-idle architecture, time,
pixels and nonempty one-second protocol replay before admitting a new
epoch-2 SIP dialog. It retains the coherent ARFCN4 cell, declared base-record
comparison, real CANCEL/487, zero-media and physical notification-dismissal
requirements. No external call exists in the saved state and this gate does
not claim restoration of a SIP dialog or native DSP speech.

The separate 8210 host incoming/outgoing call gates verify physical
Answer/End and dial/Send/End, exact configured-cell traffic/release,
coherent ARFCN4 registration, request/decision correlation and reviewed UI.
They use the same labelled base-record comparison and require own native
uploads before explicit HLE handoff. Answered host signaling is not an
answered SIP/media acceptance. The 8210 SIP runner separately offers
`--outgoing-media` (`make verify-8210-sip-outgoing-media`) for one physical
outgoing call against SIP 200, in addition
to unanswered CANCEL and the outgoing failure fixtures below. It reuses the
full own-product physical-End lifecycle: handset DISCONNECT, network RELEASE,
handset RELEASE COMPLETE, RR release and return to paging. At least 100
ordered handset-accepted frames and bidirectional bridge PCM counters are
required. A terminal packet explicitly rejected as `session_closed` is safe
only after correlated `media_closed`, with later correlated `ended` and radio release; wrong requests,
active-media failures, sequence gaps and post-closure acceptance still fail.
This proves HLE media transport, not native or non-silent microphone speech.
`verify-8210-sip-outgoing-waveform` separately validates sustained 440 Hz
microphone and 660 Hz earpiece recordings across local SIP using isolated
virtual PulseAudio routes. It proves non-silent synthetic audio through the
NSM-3 HLE composition, not native DSP speech or calibrated analog gain.
`verify-8210-sip-incoming-media` waits for fresh registered idle before INVITE
and reuses the own-product physical Answer/End, full CC/RR and presentation
checks. `verify-8210-sip-incoming-waveform` independently checks sustained
microphone/earpiece tones on that incoming lifecycle; its passing recording
is not inferred from the outgoing test. Neither direction proves native DSP speech.
`verify-8210-sip-outgoing-restore` verifies exact connected handset architecture
restoration, real SIP BYE, fresh-epoch clearing and no redial. The external
dialog is discarded rather than restored; audio continuity across load is
not inferred from this gate.
`verify-8210-sip-pending-outgoing-restore` independently covers SIP 180
save/load followed by CANCEL/487, exactly one fresh-epoch clear, no redial,
no CONNECT and no accepted media. Both restoration phases pin the own dialed
number and exact handset architecture; neither restores an external dialog.
`verify-8210-sip-incoming-restore` independently covers physical Answer,
exact connected incoming architecture restoration, SIP BYE, one fresh-epoch
clear and complete firmware release without dialog replay.
`verify-8210-sip-alerting-incoming-restore` independently covers ringing
save/load, real INVITE rejection and one fresh-epoch clear without Answer,
CONNECT, accepted media or dialog replay. Waveform continuity across load
is not inferred from any restoration gate.

`verify-8210-sip-outgoing-busy` physically dials `1234567` on configured
carrier 4 against actual SIP 486. It requires the decoded SETUP number,
correlated busy decision, full CC/RR clearing, no CONNECT/media, own
loader/HLE/self-test checks and persisted SIM registration. Reviewed crops
check dialed digits and the post-release operator label, not whole-screen
idle identity. Provisioning remains the declared acquired base-record
comparison with later low-journal records omitted; factory provisioning
and the ordinary product profile are not promoted.

`verify-8210-sip-outgoing-unavailable` retains that own-carrier/base-record
scope and instead requires actual SIP 480, correlated no-answer and
consumed cause 18 before complete firmware release. Fresh 486, 480 and
incoming-CANCEL runs pass independently, including their own presentation
checks. These gates do not establish answered media, native speech or
external-dialog save-state restoration.

`verify-8850-sip-cancel` independently covers NSM-2 v5.31 research HLE
with unchanged acquired MCU/PMM. Own physical security input settles the
registered handset before INVITE at 32 seconds. Acceptance requires the
native upload-to-missing-mask handoff and compact self-test, own `nsm2`
registration, host ARFCN1, exactly one SETUP for caller `5551234`, actual
CANCEL/487 after alerting, full CC/RR release and zero accepted media.
Own 84x48 captures pin `1 missed call` with List/Exit and decoded physical
Exit. The final idle shows numeric PLMN `00101`, whereas the initial idle
shows `DCT3 LAB`; the two images deliberately have different oracles.
Registration and persisted EF_LOCI remain valid. This is an observed
presentation difference, not proof of network-name retention; the current
MM Information provides time only and its causal role is unproved.
No firmware/name override is used. This cancellation gate does not test
answered media; the separate gates below do. External-dialog restoration
is not established by cancellation or media gates.

`verify-8850-sip-outgoing-media` and `verify-8850-sip-incoming-media` require
physical Send or Answer/End, full own-product CC/RR lifecycle, actual local
SIP confirmation and at least 100 executed frames in each direction.
The own NSM-2 manual establishes the PCM bus and MIC2/EAR endpoints; the
ROM-pinned control checker verifies field `0200` and publisher `2cb3ca`.
`verify-8850-sip-outgoing-waveform` and `verify-8850-sip-incoming-waveform`
additionally require sustained microphone
440 Hz and earpiece 660 Hz recordings through MAME audio streams. These are
HLE audio gates, not native DSP or physical analogue-gain validation; see
`8xxx_bringup.md` for the product evidence and release-ordering constraints.

`verify-8850-sip-outgoing-busy` and
`verify-8850-sip-outgoing-unavailable` additionally exercise physical
`5551234`/Send against actual SIP 486 and 480. The latter requires consumed
cause 18; both require correlated firmware clearing with no CONNECT/media.
Own loader/HLE, carrier-1 and decoded-Send checks remain enforced. The
existing operator-text crops must reproduce after release; this is not a
whole-screen idle-pixel claim. Unchanged own PMM is retained.

`verify-8850-sip-idle-restore` adds exact idle PC/SP/RAM/time restoration,
byte-identical reference/replayed LCD frames and ordered one-second
transport/SIM replay. It saves at 32 seconds and admits a new INVITE at
35 seconds only after replay completes. The call must use host epoch 2;
the runner rejects pre-load paging or reuse of epoch 1. The same caller,
CANCEL/487, zero-media, CC/RR, SIM-location and physical Exit predicates
then apply. No external dialog exists at the save point. This is a fresh
call after handset restoration, not restoration of a SIP dialog. The
ordinary idle gate continues to require Menu; this variant independently
requires notification dismissal after the fresh call.

`verify-8890-sip-cancel` independently covers NSB-6 v12.20 research HLE
with unchanged acquired MCU/PMM and the configured ARFCN60 laboratory cell.
Its physical security and clock/date sequence settles before the fresh
idle artifact admits INVITE. The real caller `5551234` must appear in the
own SETUP grammar; CANCEL follows handset alerting and must complete
487, exactly one GSM clear and full CC/RR release without Answer or media.
Own native-loader/HLE ownership and compact self-test checks remain required.
The gate pins the localized `1 Anruf in Abwesenheit` notification,
decoded physical Exit, registered-idle content before/after and persisted
SIM location. Frame comparison excludes the advancing top-row clock,
not notification text or softkeys. This is real SIP signaling only;
answered SIP, PCM and native speech are not promoted.

`verify-8890-sip-outgoing-busy` independently uses physical `1234567`/Send
against real SIP 486 on the configured carrier-60 composition. It checks
the firmware SETUP number, correlated busy, complete CC/RR release and
restored reviewed idle content excluding only the top-row clock. Native
upload/HLE and self-test ownership remain required; no CONNECT/media or
answered-call capability is inferred.

`verify-8890-sip-outgoing-unavailable` retains those product checks and
instead requires actual SIP 480, correlated no-answer and consumed cause
18 before release. Fresh busy, unavailable and incoming-CANCEL runs each
pass independently. This is signaling/idle restoration, not speech or
restoration of an external outgoing dialog.

`verify-8890-sip-idle-restore` uses the same own physical setup and idle
fixture, saves at 42 seconds, then requires exact PC/SP/RAM/time restoration
and matching one-second transport/SIM replay before admitting INVITE at
45 seconds. The fresh call must use host epoch 2 and satisfy the same real
CANCEL/487, caller, release, localized notification, decoded Exit and
registered-idle predicates. The saved handset has no external SIP dialog;
this verifies epoch invalidation and a new call, not restoration of a dialog.
The ordinary idle-state gate still requires physical Menu; this variant
instead requires post-replay physical Exit after the new call.

`verify-8890-sip-alerting-incoming-restore` saves the organically ringing
NSB-6 handset at 52 seconds, after the fresh INVITE admitted at 45 seconds.
It restores all37 exposed ARM/banked registers, modeled-RAM checksum and
emulated time. The bridge rejects the outstanding external INVITE and publishes one
cause-41 clear under epoch 2; it does not replay or serialize the SIP dialog.
Full CC/RR release, the existing missed-call pixels, decoded physical Exit
and reviewed registered-idle recovery remain required. The shared checker
forbids Answer, CONNECT, SIP confirmation and accepted media. This is
unanswered call-restoration coverage, not connected SIP, speech or native
DSP completion. No passive replay interval is asserted during the required
external-dialog invalidation.

`verify-8890-sip-pending-outgoing-restore` independently dials `1234567`
through the product's physical keypad, receives a real SIP 180 and saves
the unanswered handset at 32 seconds. All37 exposed ARM/banked registers, RAM checksum and time
must restore. The external request is cancelled with CANCEL/487, followed
by one cause-41 clear, complete CC/RR release and reviewed idle recovery.
The bridge must not redial, confirm SIP, deliver CONNECT or accept media.
These NSB-6 unanswered fixtures share architectural observation code;
their own input schedules and cleanup frames remain distinct. External
dialogs are discarded, not serialized or restored. Connected SIP and
native speech remain outside these gates.

`verify-6210-sip-idle-restore` saves and restores the idle NPE-3 handset before
the fresh INVITE. It checks exact CPU/RAM/time restoration and ordered protocol
replay, requires the new host epoch 2, then independently checks the same
CANCEL/487, missed-call and physical Exit lifecycle. No external SIP dialog
exists at the save point; this does not claim dialog restoration or speech.

### Connected-call restoration

```sh
make verify-radio-incoming-call-sip-restore RUN_DIR=run_3210_sip_restore
make verify-3310-radio-incoming-call-sip-connected-restore RUN_DIR=run_3310_sip_restore
make verify-3330-radio-incoming-call-sip-connected-restore RUN_DIR=run_3330_sip_restore
make verify-3410-radio-incoming-call-sip-connected-restore RUN_DIR=run_3410_sip_restore
make verify-5210-radio-incoming-call-sip-connected-restore RUN_DIR=run_5210_sip_restore
```

The 3210/3310/3410/5210 fixtures physically answer an incoming SIP call, save at 19 seconds
and load one second later. The 3330 fixture repeats its own physical PMM setup
and saves at 17 seconds because its earlier Answer would let the eight-second
remote call expire during the later save/load window. The checker requires that connection precede the save,
the real external SIP dialog close with BYE, and the restored GSM transaction
clear exactly once with cause 41 under epoch 2, ordered CC clearing and LAPDm
release. An `ended` flag without that protocol closure is insufficient.
External dialogs are not part of MAME save
states and are never replayed. Codec state and uplink sequence tracking reset
at the epoch boundary; media delivery remains disabled during restored-call
clearing. Only the matching restored transaction's `ended` event releases that
guard, not an unrelated completion message.

This proves the connected incoming-call restoration boundary on the 3210 and
3310 HLE profiles, not native DSP speech, exact media continuity or all
outgoing/alerting save-load races. The fixture's fixed save time is validated
against the observed connected event rather than assumed to be connected.

```sh
make verify-radio-incoming-call-sip-alerting-restore RUN_DIR=run_3210_sip_alerting_restore
make verify-3310-radio-incoming-call-sip-alerting-restore RUN_DIR=run_3310_sip_alerting_restore
make verify-3330-radio-incoming-call-sip-alerting-restore RUN_DIR=run_3330_sip_alerting_restore
make verify-3410-radio-incoming-call-sip-alerting-restore RUN_DIR=run_3410_sip_alerting_restore
make verify-5210-radio-incoming-call-sip-alerting-restore RUN_DIR=run_5210_sip_alerting_restore
```

The alerting variant omits physical Answer. Loading the ringing snapshot closes
the outstanding real SIP INVITE (observed PJSIP response: 603 Decline) and clears
the restored GSM transaction with cause 41 under epoch 2. Its checker requires
alerting before save and rejects any SIP confirmation, physical Answer,
firmware CC CONNECT or accepted host media. This is distinct from the
caller-driven CANCEL gate. The 3330 variants share the same incoming gate's
physical PMM preparation, with explicit no-Answer keys and a restore script
only for the save/load scenario.
Run all handset gates sequentially. Each product's fixture must independently
reach the requested call phase before its fixed save time.

```sh
make verify-radio-outgoing-call-sip-pending-restore RUN_DIR=run_3210_sip_pending_restore
make verify-3310-radio-outgoing-call-sip-pending-restore RUN_DIR=run_3310_sip_pending_restore
```

Both products' outgoing variants save at 25 seconds while the real remote SIP
endpoint has returned 180 Ringing but no final answer. On load the external transaction
closes with CANCEL/487. The restored `outgoing_call` request itself starts
clearing: it need not have a connected/alerting state event yet. The bridge
supplies `no_answer` followed by cause-41 termination through the existing
session interface. The checker requires exactly one SIP dial, no CONNECT,
ordered SIP 180/CANCEL/487 and firmware DISCONNECT/RELEASE/RELEASE COMPLETE
plus LAPDm release before epoch-2 `ended`. An unrelated request cannot replace the transaction being
cleared. This does not claim coverage of every outgoing connected-call or
pre-SETUP restoration race.

```sh
make verify-radio-outgoing-call-sip-connected-restore RUN_DIR=run_3210_sip_outgoing_connected_restore
make verify-3310-radio-outgoing-call-sip-connected-restore RUN_DIR=run_3310_sip_outgoing_connected_restore
```

Both products' connected variants save at 27 seconds after SIP/GSM connection. The real
SIP dialog closes with BYE on load, and the restored GSM call clears with cause
41 through the complete firmware CC/RR release sequence. A false
`decision_pending` suppresses a redundant `no_answer` decision; restoration clearing is
one-shot even when the adapter republishes both a request and its connected
state. The checker rejects redial and rejected/duplicate termination. Connected
termination is applied synchronously inside submission, so its consumed trace
precedes adapter acceptance; pending termination consumes later. Neither
ordering implies that an external dialog was restored.

### Idle restoration

```sh
make verify-radio-incoming-call-sip-idle-restore RUN_DIR=run_3210_sip_idle_restore
```

The external caller waits for registered epoch 2, after an idle save/load.
An explicit `calls_idle: true` snapshot releases the bridge's epoch guard;
missing or non-idle snapshots retain conservative call clearing. A fresh SIP
INVITE is then paged to the handset, physically answered, connected with at
least 100 frames in each bridge direction, and released through CC/RR. The
checker requires paging after restoration and exactly one incoming SIP dialog.
This tests usable call service after idle restoration, not continuity of an
external SIP call through save/load.

### Waveform probe

`tools/run_dct3_sip_backend_probe.py` independently tests the same bridge with
a synthetic version-1 host endpoint: uplink 440 Hz and remote 660 Hz must survive
GSM-FR/PCM conversion and actual SIP/RTP in both directions. This fixture does
not inject anything into a phone or establish handset coverage:

```sh
PYTHONPATH="$SIP_PYTHON_PATH" .venv/bin/python tools/run_dct3_sip_backend_probe.py --pjsua "$SIP_PJSUA_BIN" --run-dir run_sip_backend_probe
```

### Handset waveform gate

```sh
make verify-radio-outgoing-call-sip-waveform RUN_DIR=run_3210_sip_waveform
```

This optional gate needs `pactl`, `ffmpeg`, a live PulseAudio-compatible server
and the PJSIP build above. It feeds a 440 Hz external analog stimulus into
MAME's microphone and plays 660 Hz from the real remote SIP endpoint. It
requires the ordinary physical outgoing-call lifecycle, both MAME host-audio
streams, and at least two consecutive one-second windows of each received
tone (RMS at least 500, expected-frequency energy fraction at least 0.5).
Louder unrelated call/key tones are not mistaken for missing speech or counted
as the expected tone.

`sip-microphone.wav` is the remote SIP recording; `sip-earpiece.wav` captures
MAME's output sink. Optional `--record-pcm` records the bridge's decoded GSM
uplink and received SIP downlink as `sip-host-microphone.wav` and
`sip-host-remote.wav`, respectively. These boundary recordings distinguish
emulator PCM/radio faults from SIP media faults; they are not native DSP dumps.
The fixture temporarily routes MAME onto two null sinks and restores the
host's original default sink/source on exit. Run it sequentially with all
other audio and handset gates.
Defaults are captured before either temporary module is loaded. Cleanup stops
streams, unloads both modules, then restores the original defaults, including
PulseAudio's automatic fallback sink. An executable failure-path regression
checks this ordering; it is host isolation, not handset calibration.

The patch `mame-sound-input-stride.patch` corrects the generic unresampled
microphone path to advance by the interleaved **source** channel count, not the
destination microphone's count. Stereo host capture feeding a mono device
otherwise corrupts sample timing despite successful calls and nonzero PCM.
The waveform gate protects this cross-channel case through the complete HLE
audio path. It does not prove native DSP speech or real RF operation.

The five-profile acceptance below covers basic SIP failures, active-call
clearing and fresh service after idle restoration. It does not establish native
DSP speech, arbitrary SIP endpoint interoperability, or additional products.

## Cross-product SIP acceptance

The 3410 v5.46E independently admits a fresh incoming SIP call after an idle
snapshot is restored. Its physical Send field at `:COL.4` answers the call;
the shared fixture defaults remain the 3210's Navi field. The gate proves
the idle snapshot's acceptance under epoch 2, exactly one incoming identity
`(2, 1)`, matching physical Answer and confirmation, ordered accepted media,
and normal CC release. The original 3210 idle-restore gate is separately
re-run under the same checker.

```sh
make verify-3410-radio-incoming-call-sip-idle-restore RUN_DIR=run_3410_sip_idle_restore
```

The fixture changes only physical input selection and emulator save/load;
it neither injects firmware messages nor restores an external SIP dialog.

The 3310 v6.39 and 5210 v5.40E independently pass the same fresh-call-after-idle
restore contract. Their fixtures press the declared product fields: 3310
`:COL.3` / `Menu`, and 5210 `:COL.0` / `Send`. Both use fresh storage and no
boot-time keypress; the shared fixture performs save/load and physical Answer.
Neither profile inherits the 3210's Navi matrix location.

```sh
make verify-3310-radio-incoming-call-sip-idle-restore RUN_DIR=run_3310_sip_idle_restore_final
make verify-5210-radio-incoming-call-sip-idle-restore RUN_DIR=run_5210_sip_idle_restore
```

The 3330 v4.50E independently passes fresh incoming service after idle restore
following its own physical PMM setup and unlock. Its driver explicitly selects
the NHM-5 keypad matrix, so the gate reuses the column-three `Menu` fixture,
not another product's provisioning or radio encoding. The call is created only
after the bridge accepts the restored idle epoch; physical Answer, sustained
ordered media and normal release all remain required:

```sh
make verify-3330-radio-incoming-call-sip-idle-restore RUN_DIR=run_3330_sip_idle_restore
```

The preceding setup evidence is in `_provision`, and the restored fresh-call
evidence is in `_call`. All five SIP-tested HLE profiles now independently
cover this idle-service boundary as well as active-call clearing.

The 3410 v5.46E independently passes physical outgoing busy (SIP 486 → GSM
busy decision) and unavailable (SIP 480 → cause 18) fixtures. Each produces one
attempt, complete CC/RR release, no CONNECT or accepted media, and no further
host request during the 45-second run:

```sh
make verify-3410-radio-outgoing-call-sip-busy RUN_DIR=run_3410_sip_busy_gate
make verify-3410-radio-outgoing-call-sip-unavailable RUN_DIR=run_3410_sip_unavailable_gate
```

The shared failure checker requires the observed handset request IDs to match
the expected attempt count and each attempt to reach `ended` after release.
This differs from the 3310's independently observed two-attempt retry fixtures;
it is not a general assertion about all retry settings or longer runs.

The 5210 v5.40E independently passes the same busy and unavailable contracts
with its own physical Send sequence and fresh product-local storage. Each
45-second fixture observes one fully ended attempt, no redial, and no accepted
media:

```sh
make verify-5210-radio-outgoing-call-sip-busy RUN_DIR=run_5210_sip_busy_gate
make verify-5210-radio-outgoing-call-sip-unavailable RUN_DIR=run_5210_sip_unavailable_gate
```

The 3330 v4.50E independently passes both failure contracts after fresh
physical PMM provisioning for each gate. Its 40-second call run observes one
ended attempt with no redial or accepted media; the normal product SETUP,
busy decision or cause 18, and full CC/RR release remain required:

```sh
make verify-3330-radio-outgoing-call-sip-busy RUN_DIR=run_3330_sip_busy_gate
make verify-3330-radio-outgoing-call-sip-unavailable RUN_DIR=run_3330_sip_unavailable_gate
```

As with its other SIP gates, `_provision` owns first-boot PMM evidence and
`_call` owns the call logs/results. These checks close failure parity for the
five SIP-tested HLE profiles, not native DSP speech or other firmware versions.

The 3410 v5.46E also independently passes outgoing connected and pending
save/load gates with fresh product-local storage and physical Send. Connected
restoration observes SIP BYE; pending restoration observes CANCEL/487 without
CONNECT. Both change host epoch, clear once with cause 41, complete CC/RR
release and reject a second SIP dial. The checker requires the tested product's
exact original SETUP frame as well as ordered restoration/release evidence.

```sh
make verify-3410-radio-outgoing-call-sip-connected-restore RUN_DIR=run_3410_sip_outgoing_connected_restore
make verify-3410-radio-outgoing-call-sip-pending-restore RUN_DIR=run_3410_sip_outgoing_pending_restore
```

The connected fixture saves at 27 seconds, after this product's physical dial
and before the remote endpoint's normal hangup; pending saves at 25 seconds.
These are harness observation windows, not device timing changes.

The 5210 v5.40E independently passes the same connected and pending outgoing
restore contracts using fresh storage and its own physical Send sequence:

```sh
make verify-5210-radio-outgoing-call-sip-connected-restore RUN_DIR=run_5210_sip_outgoing_connected_restore
make verify-5210-radio-outgoing-call-sip-pending-restore RUN_DIR=run_5210_sip_outgoing_pending_restore
```

The common pending checker also rejects accepted host media, not only CONNECT;
an unanswered restored call must clear without starting an audio session.

The 3330 v4.50E independently passes both outgoing restore contracts after
fresh physical first-boot PMM setup in each gate. Its shared fixture saves at
17 seconds and reloads one second later, inside the observed connected dialog
window and after provisional progress in the pending case:

```sh
make verify-3330-radio-outgoing-call-sip-connected-restore RUN_DIR=run_3330_sip_outgoing_connected_restore
make verify-3330-radio-outgoing-call-sip-pending-restore RUN_DIR=run_3330_sip_outgoing_pending_restore
```

Evidence is in each gate's `_call` directory; `_provision` contains only its
own preceding PMM setup. Neither dialog nor provisioning is inherited from
another product. The original3210/3310/3330/3410/5210 HLE profiles have
independent incoming and outgoing restore evidence; native DSP speech
remains unproven.

The 3310 NHM-5 v6.39 also completes a physical outgoing call through the local
PJSIP bridge: handset SETUP, SIP confirmation, bidirectional host media, and
normal CC/LAPDm release. Its SETUP bearer capability differs from the 3210;
the checker retains separate exact product expectations. The outgoing waveform
gate additionally proves sustained microphone-to-SIP and SIP-to-earpiece tones
through its own 1 MHz/125-clock PCM profile. This is research-HLE audio, not
native DSP speech or evidence for its unresolved analogue gain programming.

```sh
make verify-radio-outgoing-call-sip RUN_DIR=run_3310_sip_outgoing \
  SIP_HANDSET_MACHINE=noki3310 SIP_HANDSET_BIOS=639 \
  SIP_HANDSET_KEYS=5,5,5,1,2,3,4,enter \
  SIP_HANDSET_KEY_DELAY_MS=18000 SIP_HANDSET_KEY_DURATION_MS=70 \
  SIP_HANDSET_KEY_GAP_MS=200 SIP_HANDSET_RUNNER_ARGS='--product 3310'
```

```sh
make verify-3310-radio-outgoing-call-sip-waveform RUN_DIR=run_3310_sip_waveform
```

The actual incoming SIP fixture independently proves paging, a physical Navi
Answer, firmware CONNECT, at least 100 accepted downlink frames, and ordinary
release on NHM-5 v6.39:

```sh
make verify-3310-radio-incoming-call-sip RUN_DIR=run_3310_sip_incoming
```

Its exact CONNECT/RELEASE COMPLETE expectations include the observed CC
sequence bit for this fixture. Those bits are not fixed hardware properties.
This signaling gate alone proves media transport, not waveform fidelity.
The result retains bounded host-queue drop counts; the gate does not assert
lossless audio or zero startup drops.

Incoming waveform gates add the same sustained endpoint-tone acceptance
through each product's physical Answer path:

```sh
make verify-radio-incoming-call-sip-waveform RUN_DIR=run_3210_sip_incoming_waveform
make verify-3310-radio-incoming-call-sip-waveform RUN_DIR=run_3310_sip_incoming_waveform
make verify-3330-radio-incoming-call-sip-waveform RUN_DIR=run_3330_sip_incoming_waveform
make verify-3330-radio-outgoing-call-sip-waveform RUN_DIR=run_3330_sip_outgoing_waveform
make verify-3410-radio-incoming-call-sip-waveform RUN_DIR=run_3410_sip_incoming_waveform
make verify-3410-radio-outgoing-call-sip-waveform RUN_DIR=run_3410_sip_outgoing_waveform
make verify-5210-radio-incoming-call-sip-waveform RUN_DIR=run_5210_sip_incoming_waveform
make verify-5210-radio-outgoing-call-sip-waveform RUN_DIR=run_5210_sip_outgoing_waveform
```

Run these sequentially. `sip-waveform-result.json` identifies both the product
and call direction. Incoming and outgoing gates preserve the same frequency,
amplitude and duration thresholds; neither establishes native DSP speech or
real-radio operation.

These five products use the same 440 Hz source at 0.025 full-scale, avoiding clipping
in the 3210's +18 dB path while providing measurable input to the other products' neutral
HLE gain. This is an external test level, not a calibration change. The common
acceptance threshold remains two consecutive one-second windows with RMS at
least 500 and at least half their energy at the expected frequency. Results
identify the tested product; another product's SETUP encoding is rejected.

The 3410 v5.46E gates use fresh product-local storage and its physical Send/End
keys. Both directions independently pass sustained microphone and earpiece
tone checks without gain changes. This extends HLE endpoint waveform evidence,
not native DSP speech evidence. The 5210 v5.40E independently passes the same
incoming and outgoing waveform checks with its own physical Send key, fresh
storage and host configuration.

The 3330 v4.50E waveform gates first perform the product's physical PMM setup
in a fresh, separate run and check its boot summary. The call then preserves
only that run's own `noki3330_1/flash` storage, with physical unlock and Navi
Answer/dial sequences. Both directions pass the unchanged endpoint-tone checks.
Direct use of the audio wrapper for this profile requires
`SIP_WAVEFORM_NVRAM_DIR` naming that provisioned storage root; the named gates
create it themselves. No donor PMM or firmware-state injection is used.

The external bridge sends at most one queued downlink block per 20 ms, without
catch-up bursts after a host stall. Its eight-block queue and the emulator's
independent radio clock remain unchanged. Normal-call acceptance also requires
at least 100 consecutive, handset-accepted downlink sequence numbers and no
rejected frames: sent-frame counts alone cannot establish downlink delivery.

### NSB-7 HLE media and restoration boundary

The explicit8290 `nsb7hle` profile qualifies physical incoming/outgoing
local SIP200, independently documented PCM and sustained microphone/earpiece
tones through actual MAME host audio streams. Own PIN/registration, physical
Answer/Send/End, CC/RR release, exact UI/card/journal and retained-clock checks
remain product-owned by `nsb7_sip_check.py`; see
[the authoritative NSB-7 contract](8290_bringup.md#sip-calls-and-restoration).

`run_noki8290_sip_media.py` requires fresh isolated storage and an explicit
own-source NVRAM predecessor, validates it, records its hashes and preserves
it. Ordinary media and four restoration phases have separate Make gates.
Connected phases require positive own PCM before save and a positive
discarded-epoch media inventory, then exact37-register/RAM/time restore,
old-dialog BYE and one new-epoch cause41 clear without redial/readmission.
Unanswered outgoing CANCEL/487 and incoming rejection require zero external
media inventory, no CONNECT/Answer and clearing before the physical End.

The PCM-enabled unanswered outgoing run can execute local HLE codec ticks
only during restored radio teardown. They have zero downlink and exact
clock accounting and are not admitted external speech. The checker bounds
this behavior explicitly rather than treating codec activity as CONNECT.
Only reviewed Calling/ringing alternatives qualify. Seamless external-call
resumption, arbitrary multi-call combinations, native speech and measured
analog gains remain unqualified. `--signaling-only` retains the pre-PCM
no-codec contract and is not a substitute for current-build phase acceptance.

The shared Disconnect scheduling correction also passes fresh NPE-3 incoming
alerting restoration and NHM-5 v6.39 ordinary SIP CANCEL gates. NPE-3 requires
ended before its physical Exit (not just eventual release after a key); its
missed-call/registered-idle frames remain checked. These are cross-product
HLE signaling regressions, not evidence of native DSP or physical RF behavior.

NSB-6 additionally has independent PCS1900 ordinary media gates:
`verify-8890-pcs-sip-outgoing-media` and
`verify-8890-pcs-sip-incoming-media`. Both require own ARFCN600 acquisition,
physical call controls, exact documented PCM accounting, bidirectional local
SIP media and product-owned UI/storage recovery. Separate
`verify-8890-pcs-sip-{outgoing,incoming}-waveform` gates prove sustained
physical microphone/earpiece tones and require matching PCS media acceptance.
Connected PCS restoration is independently qualified by
`verify-8890-pcs-sip-{outgoing,incoming}-connected-restore`: own registration
and bidirectional media precede saving, and restoration clears the external
dialog rather than replaying it. Independent
`verify-8890-pcs-sip-outgoing-pending-restore` and
`verify-8890-pcs-sip-incoming-alerting-restore` require zero external media,
no CONNECT/Answer, actual SIP cancellation/rejection and own-band release.
Any local unanswered codec ticks are restricted to restored radio teardown
with zero downlink and exact clock accounting, not counted as speech.
See the NSB-6 contract in `8xxx_bringup.md`.

### Failed calls and automatic redial

The tested NHM-5 v6.39 profile automatically retries a rejected outgoing call
without another physical keypress. The 3310 failure gates keep the SIP endpoint
attached through two complete attempts rather than leaving that retry pending:

```sh
make verify-3310-radio-outgoing-call-sip-busy-redial RUN_DIR=run_3310_sip_busy
make verify-3310-radio-outgoing-call-sip-unavailable-redial RUN_DIR=run_3310_sip_unavailable
```

Each requires two distinct actual SIP INVITE dialogs and correlated handset
request IDs, zero CONNECT/media, and ordered CC/LAPDm release for both attempts.
SIP 486 consumes the busy decision; SIP 480 consumes `no_answer` followed by
cause 18 for each attempt. This proves the tested profile's retry lifecycle,
not a universal factory redial setting or native DSP operation.

The optional bridge `--calls N` stops after N normally completed handset calls;
zero retains continuous operation. It is mutually exclusive with `--once`.
`--require-frames` applies to every normally completed call, not just the last.
