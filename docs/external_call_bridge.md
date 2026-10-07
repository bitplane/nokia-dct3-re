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
{"type":"call_adapter_ready","protocol_version":1,"epoch":1,"capabilities":["network_state","calls","gsm_fr_media","sms","ussd"]}
```

The additive `capabilities` array lets hosts discover the services implemented
by this adapter without inferring them from the phone profile. Hosts written
against the earlier version-1 ready message may ignore it.

Every host-to-MAME message carries the current `epoch` and a positive
`request_id`. A restore increments the epoch, invalidates queued host input and
republishes emulator-owned call state. A host must discard the old identity;
it must not resend an `incoming_call` that MAME has already accepted.

| Direction | Message | Required payload |
|---|---|---|
| MAME to host | `outgoing_call` | `epoch`, `request_id`, decimal `digits` |
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
provides application-owned PCM callbacks. The standalone outgoing backend now
maps SIP decisions to existing call identities; GSM-FR decode/encode connects
bounded 8 kHz PCM queues to that port.
Neither SIP nor host wall-clock scheduling belongs in the emulated GSM/device
state. Restoration must invalidate the old SIP call identity instead of
replaying an already accepted incoming call. Python SWIG bindings require a
separate local build; no installed PJSUA2 binding is assumed by the stack probe.

## Outgoing SIP bridge

`tools/dct3_sip_bridge.py` supports a single explicit SIP destination. It waits
for SIP confirmation before accepting the handset's outgoing request, maps
486/600 to busy and other pre-confirmation failures to no-answer, and maps
remote release to GSM normal clearing. Handset completion hangs up the SIP leg.
The bridge does not auto-accept SMS/USSD or implement incoming SIP; unsupported
incoming SIP receives busy. No registrar, credentials or destination-number
routing policy is claimed. The explicit destination is independent of the
physically dialed number, which is logged for acceptance evidence.

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
```

The gate builds the erased-identity security-code fixture before preparing a
fresh isolated run, physically unlocks and dials `5551234`, then requires SIP
confirmation, at least 100 executed PCM/media frames in each direction, the
firmware's exact SETUP/CONNECT ACK/RELEASE and final LAPDm release. The remote
endpoint supplies a 660 Hz WAV source and clears the call after eight seconds.
The own 3210 HLE gate proves signaling and media transport, not native speech,
sound perception or a public-network end-to-end call.

`tools/run_dct3_sip_backend_probe.py` independently tests the same bridge with
a synthetic version-1 host endpoint: uplink 440 Hz and remote 660 Hz must survive
GSM-FR/PCM conversion and actual SIP/RTP in both directions. This fixture does
not inject anything into a phone or establish handset coverage:

```sh
PYTHONPATH="$SIP_PYTHON_PATH" .venv/bin/python tools/run_dct3_sip_backend_probe.py --pjsua "$SIP_PJSUA_BIN" --run-dir run_sip_backend_probe
```

Next boundaries are incoming SIP through ordinary paging/physical Answer,
executable restoration/failure coverage, and microphone/earpiece waveform
acceptance. Native DSP speech remains its independent hardware/backend milestone.
