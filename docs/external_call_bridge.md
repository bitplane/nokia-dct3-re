# External call bridge

The optional MAME host-call adapter exposes firmware-owned mobile-originated
calls at `ws://127.0.0.1:18080/nokia/dct3/calls`.  It carries call decisions and
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
