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

Every host-to-MAME message carries the current `epoch` and a positive
`request_id`. A restore increments the epoch, invalidates queued host input and
republishes emulator-owned call state. A host must discard the old identity;
it must not resend an `incoming_call` that MAME has already accepted.

| Direction | Message | Required payload |
|---|---|---|
| MAME to host | `outgoing_call` | `epoch`, `request_id`, decimal `digits`; additive `decision_pending` boolean snapshots whether the session still needs a host decision |
| Host to MAME | `outgoing_call_decision` | identity plus `decision`: `connect`, `busy`, or `no_answer` |
| Host to MAME | `incoming_call` | identity plus 1..20 decimal `caller` digits |
| MAME to host | `*_call_state` | identity and `phase`; an incoming request may terminate as `expired` before paging; connected snapshots also carry both media cursors; a `forwarded` incoming state carries `forwarding_reason` and the decoded decimal `forwarding_destination` |
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
| Host to MAME | `incoming_ussd` | identity, DCS and packed notification data |
| MAME to host | `incoming_ussd_state` | identity and `phase`: `queued`, `delivered` or `expired` |

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

The USSD host contract preserves the BER-decoded DCS and packed payload. A
`success` response carries `dcs` and hexadecimal `data`; `return_error` and
`reject` may carry `error_code`. The session builds the correlated GSM
supplementary-service component, and `verify-radio-ussd-host-adapter` requires
the handset's organic `*123#` request, host response acceptance, firmware UI
delivery and the ordinary LAPDm channel release.

Host-originated `incoming_ussd` is deliberately a notification, matching the
network-initiated form this firmware accepts. It enters through registered-idle
paging and is reported `delivered` only after the handset ReturnResult and
channel release. The standalone bridge can exercise it with
`--incoming-ussd 'Host notice' --once`; the generic wire contract accepts
already packed data rather than assuming a text alphabet.
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
release.

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
mkdir -p run_sip_build
curl -L --fail https://codeload.github.com/pjsip/pjproject/tar.gz/refs/tags/2.16 -o run_sip_build/pjproject-2.16.tar.gz
tar -xzf run_sip_build/pjproject-2.16.tar.gz -C run_sip_build
cd run_sip_build/pjproject-2.16
./configure --disable-video --disable-sound --disable-ssl CFLAGS=-fPIC CXXFLAGS=-fPIC
make dep
make
cd ../..
.venv/bin/python tools/run_sip_stack_probe.py --pjsua run_sip_build/pjproject-2.16/pjsip-apps/bin/pjsua-x86_64-pc-linux-gnu --run-dir run_sip_probe
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
cd run_sip_build/pjproject-2.16/pjsip-apps/src/swig/python
PATH="$ROOT/.venv/bin:$PATH" make PYTHON_EXE="$ROOT/.venv/bin/python"
cd "$ROOT"
```

Alternatively invoke that `make` with the absolute project `.venv/bin` paths.
`SIP_PYTHON_PATH` points at the generated `pjsua2.py` directory and its
`build/lib.*` extension directory; `SIP_PJSUA_BIN` selects the upstream executable.
Both have local-build defaults in Makefile, but remain optional dependencies.

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
```

Both gates physically dial the 3210 into actual upstream PJSIP responses and
require the correlated failure, firmware RELEASE/RELEASE COMPLETE and LAPDm
release, with no SIP/GSM connection and zero executed bridge media. The 480
gate additionally requires the GSM session to consume cause 18. The release
checker admits either CC send-sequence bit value without changing the decoded
primitive. These gates do not measure failure-screen presentation.

The implemented mapping subset also covers 403/603 -> 21, 404/604 -> 1,
408 -> 102 and 500/503 -> 41; these have pure mapping tests, not handset runtime
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
The generic SIP runner rejects answered/media fixtures for product `6210`.

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
another product. All five SIP-tested HLE profiles now have independent
incoming and outgoing restore evidence; native DSP speech remains unproven.

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
