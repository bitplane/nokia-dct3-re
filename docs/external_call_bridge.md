# External telephony bridge

The optional MAME host adapter exposes firmware-owned calls and mobile-originated
SMS at `ws://127.0.0.1:18080/nokia/dct3/calls`. It carries call decisions and
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
{"type":"call_adapter_ready","protocol_version":1,"epoch":1}
```

Every host-to-MAME message carries the current `epoch` and a positive
`request_id`. A restore increments the epoch, invalidates queued host input and
republishes emulator-owned call state. A host must discard the old identity;
it must not resend an `incoming_call` that MAME has already accepted.

| Direction | Message | Required payload |
|---|---|---|
| MAME to host | `outgoing_call` | `epoch`, `request_id`, decimal `digits` |
| Host to MAME | `outgoing_call_decision` | identity plus `decision`: `connect`, `busy`, or `no_answer` |
| Host to MAME | `incoming_call` | identity plus 1..20 decimal `caller` digits |
| MAME to host | `*_call_state` | identity and `phase`; connected snapshots also carry both media cursors; a `forwarded` incoming state carries `forwarding_reason` and the decoded decimal `forwarding_destination` |
| MAME to host | `*_call_media_uplink` | identity, sequence, emulation timestamp, good/BFI flag, 33-octet GSM-FR frame as 66 lowercase hex characters |
| Host to MAME | `*_call_media_downlink` | identity, host sequence, source timestamp, and one encoded GSM-FR frame |
| Host to MAME | `*_call_terminate` | identity and GSM cause in `1..127` |
| MAME to host | `outgoing_sms` | identity, decimal `recipient`, `alphabet`, TP user-data length and packed user data as lowercase hex |
| Host to MAME | `outgoing_sms_decision` | identity plus `decision`: `accept`, `rp_error`, or `rp_silence` |
| MAME to host | `outgoing_sms_state` | identity and `phase`: `accepted`, `rejected`, or `ended` |

The `*` is direction-specific (`incoming` or `outgoing`) and must match the
call. Frames are conventional GSM 06.10 full-rate payloads, not PCM. The host
does not own paging, CC/RR state, radio timing, keypad decisions, codec routing
or release completion. Queue overflow, stale epochs, duplicate decisions and
wrong-direction media are rejected without changing emulated call state.

The SMS payload stays in its GSM representation. `gsm7` reports septet count
in `user_data_length` and carries the packed octets in `user_data`; `8bit` and
`ucs2` report octet count. The adapter does not reinterpret binary UDH or lose
alphabet information. `verify-radio-outgoing-sms-host-adapter` proves an
organic composer-to-host request and the correlated RP result through the
ordinary SAPI-3 transaction.

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
