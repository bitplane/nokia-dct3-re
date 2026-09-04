# Structural boot regression

The LCD frame oracle proves the final visible state. Under `-video none`, the
Lua harness mirrors the PCD8544 command/data stream and snapshots completed and
frame-visible states; the phone driver has no parallel capture implementation.
When firmware gates video frame notifications but continues partial LCD writes,
the periodic oracle publishes the dirty terminal mirror so `make frame` cannot
fall back to an early blank boot image.
The structural oracle
guards stable mid-boot behavior that can regress while still converging on the
same frame.

`mame_nokia_dct3_input_exerciser.lua` writes a deterministic summary to
`NOKIA_DCT3_BOOT_SUMMARY`. `make run` places it at
`RUN_DIR/boot_summary.txt`; `make verify` checks the semantic predicates in
`oracles/noki3210-default.struct` without requiring an LCD frame. That target
explicitly disables the 3210 peer devices and is a negative failure baseline.
`make verify-frontier` checks the machine-default request-driven
external-service/SIM composition against `oracles/noki3210-frontier.struct`.
`make verify-radio-camp` adds the opt-in deterministic radio peer and requires
an organically usable ARFCN, accepted channel change, task-11 acquisition
action, matching SI3 identity, and complete SI1--SI4 bitmap.
`make verify-radio-registration` continues through one accepted Location
Updating exchange. It requires the exact contention-resolution UA payload, the
firmware's `UPDATE BINARY` of `EF_LOCI`, RR release, channel deconfiguration and
at least four channel-`0x50` BCCH blocks and a channel-`0x60` no-identity PCH
block after release. This distinguishes a completed registration from
unrelated type-`0x80` traffic or a retry loop. `make verify-radio-paging`
selects a named network-event fixture and requires one IMSI page in the
subscriber's calculated paging group, organic RACH/Immediate Assignment and
Paging Response, bounded release, and return to PCH fill.
`make verify-radio-incoming-call` uses a separate event fixture and continues
from that entrance through an SC=0 Cipher Mode Command, one-way DSP type
`0x14`, organic Cipher Mode Complete, acknowledgement-gated MM Information,
one incoming SETUP, organic Call Confirmed and Alerting, handset clearing,
an RR TCH/F Assignment Command, organic channel configuration and new-link
SABM/UA, Assignment Complete, network Release, organic Release Complete,
DISC/UA, physical teardown and return to PCH fill. It deliberately makes no
A5, answered-call speech or codec claim.
`make verify-radio-incoming-call-answered` retains the same page and radio
path but provisions the erased identity's real security-code verifier, enters
`12345` through physical keys, waits for the organic MAD2 PUP buzzer gate and
presses physical Answer once. It requires the ringtone enable/disable
lifecycle, CC Connect, network Connect Acknowledge and a stable answered
interval. The post-answer census admits only empty type-`0x1b` TCH polls and
the independently classified type-`0x05` external-service poll; it fails on an
unclassified DSP packet family. Its lower-boundary checker also requires the
answer-only committed shared-control command `0x08/0x060b`, the exact 900 Hz
acknowledgement-tone start/stop group, its 100--150 ms recovered duration, and
no continuing MCU shared-control traffic. This locates a codec-control
frontier. The separate speech-media gate selects a network-side voice peer,
whose independent GSM-FR encoder supplies a 1 kHz service-test signal only
through the radio downlink queue. It requires 20 ms full-duplex cadence,
continuing uplink/downlink frame counts, and non-zero COBBA receiver blocks;
the same gate passes under v6.00 and v5.01 with 150 uplink frames, 145
downlink frames and 145 non-silent receiver blocks. This is an energy-bearing
boundary test, not a fixture write into handset PCM or call-control state.
That cadence is calculated by the MAD2 PCM endpoint from the configured
160-sample, 8 kHz converter contract; the DSP HLE carries no independent
20 ms timer constant.

`make verify-radio-outgoing-call-lifecycle` physically enters `5551234` and
presses Call. It requires the firmware-owned CM Service Request and SETUP,
standards-shaped Call Proceeding/Alerting/Connect, exactly one traffic
assignment, Connect Acknowledge, sustained non-silent GSM-FR, physical End,
CC/RR teardown and a new post-release PCH fill. The 3310, preserved-PMM 3330
and 3410 variants retain their independently checked radio, PCM and release
contracts. `make verify-radio-outgoing-call-state` additionally requires exact
active-call digital replay across save/load before ordinary physical teardown.
The `verify-radio-outgoing-call-busy`,
`verify-radio-outgoing-call-no-answer` and
`verify-radio-outgoing-call-service-reject` gates require distinct negative
lifecycle invariants rather than a changed final screen. Busy must not assign
TCH; no-answer must not Connect; service rejection must not accept CM, observe
SETUP or create request state. All three return to a newly observed PCH fill.
`verify-radio-outgoing-call-no-answer-state` repeats local clearing after an
exact save/load while Alerting.
`verify-radio-outgoing-call-delayed-decision-state` saves before a delayed
post-SETUP decision, then requires the same request ID to be queued and
consumed before the busy Disconnect. It therefore detects unsaved timer,
request or decision state, duplicate consumption and transport shortcuts.
`verify-radio-outgoing-call-host-adapter` additionally starts MAME's loopback
WebSocket endpoint and requires the exact request JSON, bounded parser
failures, stale-ID rejection, one accepted busy decision, duplicate rejection
and ordinary CC/RR teardown with the deterministic fallback disabled.
`verify-radio-outgoing-call-host-termination` and
`verify-radio-outgoing-call-host-alerting-termination` submit a separately
typed, cause-16 termination correlated to that saved request. The former
crosses an exact save/load while the accepted termination is pending and
requires its deterministic replay through Connect Acknowledge before it is
consumed; the latter
holds the call at network Alerting. Both reject a wrong ID and a duplicate,
then require network Disconnect, handset Release, network Release Complete,
RR Channel Release and a new PCH fill. The WebSocket callback only bounds and
queues the event; the saved generic session owns when it becomes a GSM
downlink and all subsequent CC/RR state.
`verify-radio-outgoing-call-host-media` additionally waits for the typed
connected-state publication, returns 200 good uplink GSM-FR frames as
contiguously sequenced downlink frames, and requires every frame to be accepted
under the same request ID before remote clearing. The structural media test
locates this boundary after independent uplink TCH/F decoding and before
downlink coding/interleaving/burst transport, while forbidding adapter
dependencies on DSP or COBBA. This is encoded silence transport coverage;
non-silent host audio is deliberately not inferred from it.
`verify-radio-outgoing-call-host-physical-media` adds the isolated physical
boundary: a host 1 kHz microphone source crosses COBBA, firmware speech
control, uplink TCH/F and the WebSocket as organically encoded GSM-FR. The host
returns those exact correlated frames; they cross downlink TCH/F and the
handset PCM/playback route. The composite gate requires 200 non-silent
microphone/network frames, sustained non-silent physical playback, ordinary
remote clearing and rejection of media after release. It does not require a
lossy speech-codec round trip to preserve the pure-tone spectral oracle used
by the independent synthetic-network-source test.
`verify-radio-outgoing-call-host-reconnect` disconnects before the decision
and again during traffic, requiring request/state snapshots without changing
the transport epoch. It then crosses save/load, requires an incremented epoch
and a restored snapshot, rejects a termination from the old epoch and accepts
the current event through ordinary clearing. Adapter sockets and callback
queues are explicitly absent from save registration.
`verify-radio-outgoing-call-host-two-calls` completes two separately dialled
busy calls. Request IDs advance from one to two only after the first RR release
and PCH return; a late request-one decision is rejected while request two is
pending, and the second call then clears independently.
`verify-radio-outgoing-call-host-hostile` connects before SETUP at realtime
speed, submits malformed and unsupported messages plus more pre-SETUP
terminations than the 16-entry callback queue can hold, and requires both
state-neutral rejection and an explicit overflow count. The later organic
request and correlated busy decision must remain independently valid.
`verify-radio-outgoing-call-host-local-end` requires physical handset
Disconnect and complete RR teardown before stale host termination/media are
submitted and rejected. `verify-radio-outgoing-call-host-alerting-reconnect`
requires request and Alerting state republication without an epoch change.
`verify-radio-outgoing-call-host-media-restore` requires saved media cursors,
old-epoch rejection and 80 contiguous restored frames.
`verify-radio-outgoing-call-host-release-restore` waits on the saved,
observational `awaiting_handset_release` output and compares the reference and
restored CC/RR release records exactly.

Product promotion pairs each `verify-*-radio-outgoing-call-lifecycle` target
with its host target. The 3410 principal additionally runs
`verify-3410-radio-outgoing-call-host-media`; the 3310 and preserved-PMM 3330
run their focused `host-termination` targets. Shared host checkers contain no
product DSP, PCM or analogue assumptions.
`radio_physical_uplink_trace_check.py` covers the opposite direction in
audio-enabled runs. An external host source enters through MAME's microphone
record stream; the gate requires at least 100 non-silent, unclipped COBBA
microphone blocks and output above the GSM-FR silence floor from the network
peer's independent uplink decoder. It checks whole-call maxima so an earlier
clipped checkpoint cannot be hidden by the final sample. The public target
runs v6.00 and v5.01 at real-time speed; each passes with 250/250 non-silent
microphone blocks, its revision-specific control oracle, sustained media,
FACCH interruption/recovery and organic End-to-idle teardown.
The same gate uses a second, non-looped PulseAudio sink for handset playback.
`radio_physical_downlink_check.py` analyzes its recorded mono 8 kHz waveform
and requires a sustained 1 kHz component for longer than the bounded answer
tone. `pulse_route_mame.py` selects only MAME's live streams by application
identity; the host defaults are preserved and restored. The source generator
uses FFmpeg's explicit Pulse `-device` option, preventing accidental default-
speaker playback from masquerading as microphone input.
`make verify-radio-incoming-call-lifecycle` presses the context-sensitive Navi
key again after that stable interval. It requires organic CC
Disconnect/Release/Release Complete, the release channel change, and the
ordered command-`0x08` desired-state lifecycle
`0x0002 -> 0x060b -> 0x040a -> 0x0002`, including its firmware producer sites.
It deliberately assigns no semantic names to the control bits.
`make verify-radio-incoming-call-lifecycle-v501` repeats the physical
Answer-to-End flow under the independently relocated v5.01 firmware and checks
the invariant shared-offset-`0x0a8` wire lifecycle
`0x8002 -> 0x860b -> 0x840a -> 0x8002`.

`make verify-3330-radio-incoming-call-lifecycle` first validates organic
fresh-PMM provisioning, then cold-boots that preserved state. Its input
orchestration observes a read-only firmware-owned CC Alerting output before
pressing the physical Navi matrix key; it does not write call or firmware
state. The gate requires the NHM-6 cipher publication, TCH assignment,
Connect/acknowledgement, command-`0x08` `0x860b` speech request, physical Navi
Disconnect, Release Complete, `0x14` channel release, `0x840a` speech release
and return to PCH.

That lifecycle additionally requires at least 100 sustained frames in both
directions over the independently declared 1 MHz/8 kHz, 125-clock NHM-6 PCM
profile. `make verify-3330-radio-media-resilience` adds bidirectional burst
impairment, FACCH/BFI recovery, SACCH coexistence and exact active-call
save-state replay before the same physical teardown. The fixture can enable
only the product-owned PCM component; removing or invalidating its typed
profile leaves speech blocked.
`make verify-radio-call-state-roundtrip` additionally saves during the stable
speech interval under both v6.00 and independently relocated v5.01, records
two seconds of DSP/network media evolution, restores the save, and records the
same emulated interval again. Its verifier compares the ordered speech
records exactly rather than accepting mere post-load activity. The discarded
reference branch is removed only for the ordinary monotonic lifecycle/media
checks; each restored branch then reaches a physical End and clean teardown.
The v6.00 half uses the decoded control-lifecycle oracle while the v5.01 half
uses the shared-wire oracle, so the gate preserves the known product contract
difference rather than forcing both ROMs through one interpretation.
The same target runs `radio_facch_interruption_trace_check.py`. It requires
good uplink and downlink FACCH only after the firmware speech-route request
and before speech teardown, a subsequent concealment increment at each
independent codec boundary, and then more than 100 recovered handset/network
speech frames. Thus the organic control blocks are proven to steal traffic
and cross the BFI seams rather than merely appearing somewhere in the trace.
`make verify-radio-pcm-missing` runs the same paired-ROM lifecycle with the
MAD2/COBBA PCM component disabled. It correlates the firmware's command-`0x08`
wire value with the DSP-facing payload, requires the unsupported-link fault,
rejects every codec tick and good uplink frame, and still requires independent
network downlink timing plus organic Release Complete. This is the negative
composition proof that an unknown product profile cannot manufacture handset
speech.
`make verify-cobba-control` is a separate mapped-device diagnostic for the
opaque DSP serial-control plane. It checks the recovered latch/select/read
grammar, reset handshake and ROM4 register-8 codec-serial loopback predicate,
restores all touched register state, and assigns no mux, gain or call-state
meaning to any other register bits. The ordinary paired-ROM
call gates run without this diagnostic bit, keeping controller conformance
distinct from firmware behavior.
`make verify-radio-degraded-speech` likewise runs both ROMs through the
bidirectional burst-impairment profile. It requires protected-frame failure,
explicit handset and network BFIs, independent downlink continuation during
uplink loss, non-silent concealment, clean-frame recovery and organic
teardown. It also saves during active degradation and requires the two-second
reference/restored branches to match exactly across ordered impairment,
Layer-1, codec and peer records. Ordinary monotonic checks discard only the
speculative reference branch. The v6.00 half retains its decoded
control-lifecycle oracle and the v5.01 half its shared-wire oracle.
`make verify-radio-incoming-sms` uses another event fixture, traverses the same
SC=0 control boundary, and requires one SAPI-3 SABM/UA exchange, both exact
segments of the ordinary `hello`
SMS-DELIVER, the handset's SAPI-3 acknowledgement, an organic 176-byte
`EF_SMS` update, and the exact unread record in card NVRAM. Neither the default
v6.00 run nor a separate physical security-code-unlock control exposes the
CP/RP closing tail, so that verifier makes no RR teardown or
visible-notification claim.
`make verify-radio-incoming-smart-message` replaces the text TPDU with one
complete 251-byte Nokia RTPL ringtone queued as two concatenated parts. It
requires every exact stop-and-wait SAPI-3 segment of each part, including
TP-UDHI, DCS `f5`, the port-`1581` UDH, reference `7a`, count `2`, indices
`1`/`2` and RTPL bytes. Each transaction must receive the firmware's CP-ACK
and independently referenced RP-ACK, network CP-ACK and RR release before the
next page. Exactly two pages occur and `EF_SMS` record 1 stays free. The
separate state gate replays the first close, queued successor and second close
boundaries exactly.
`make verify-radio-smart-message-application` continues from that boundary
through physical security-code entry, firmware Options/Play, a stable named
Playing-tone frame and note-varying PUP output. Its commandless twin must reach
the untitled Play rejection frame without playback. `make
verify-radio-smart-message-envelopes` fixes the observed per-part notification
matrix for missing, mismatched-reference, incorrect-total, wrong-port,
out-of-order, duplicate and truncated-UDH compositions while requiring
`EF_SMS` to remain free. Its stale-then-valid composition requires no
completion for the stale set or fresh part 1 and exactly one completion after
fresh part 2. The duplicate case deliberately records NSE-8's evidenced
acceptance of two independently transported UDH sequence-1 parts; it must not
be confused with an idempotent LAPDm retransmission or normalized into a
rejection. `make verify-3410-radio-smart-message-application`
and the focused NHM-5 gate retain separate UI oracles. These gates prove
temporary reassembly, dispatch and playback parsing, but not save/discard or
cold-boot ringtone persistence. The application-state gate restores after
part 1 and immediately before the completion notification, proving that
firmware RAM—not a parallel host reconstruction cache—owns both boundaries.
It also restores after terminal Play acceptance and commandless rejection,
requiring the exact named/untitled UI and playback/no-playback result.
`make verify-radio-operator` adds the unobscured firmware-rendered test-PLMN
label. None alters either boot oracle.
`make verify-mmi-menu` adds provisioned identity data and one delayed physical
left-softkey press. It requires the same coherent structural predicates and an
exact hash of the stable post-input `Phone book` pixels. The animated 20x12 icon
region is excluded; the text, softkey, layout and remaining pixels are protected.
This is the interactive MMI oracle; the semantic missing-hardware profile remains
an explicit negative control.
`make verify-sim-phonebook` extends the interactive gate across a mutable card
transaction. It enters `ADA`/`123` through physical keypad input, requires the
firmware to issue an absolute 32-byte `UPDATE RECORD` for `EF_ADN`, validates
that only record 1 changed, then restarts with the same SIM NVRAM and matches
the stable pixels of the firmware-rendered `ADA` search result. It does not
inject an APDU or conflate card storage with the handset EEPROM.
The task running at the final emulation tick is deliberately excluded:
final-tick sampling is timing-sensitive, and a harmless schedule shift can
sample scheduler idle `0xff` instead of task 1 without any change in durable
state or the stable menu frame. Startup mode, service/SIM state, resets and
hardware activity remain protected.
`make verify-3210-v501` runs the same-product v5.01 control with a BIOS-specific
EEPROM profile and checks `oracles/noki3210-v501-smoke.struct`.
`make verify-mad2-interrupts` runs three non-oracle controller conformance
fixtures: overlapping physical keypad/charger sources, an IRQ held pending
behind its masks, and register-enabled extended FIQ8 routing. The separate
`make verify-mad2-sleep` gate covers Timer-1, keypad and FIQ8 wake from MAD2
clock-stop. The fixtures use
only input ports and mapped MAD2 registers; their bounded trace checker does
not treat timing-sensitive interrupt totals as structural-oracle fields.
`make verify-mad2-clocks` checks both 3210 ROMs against reset-cause reads and
the SIM peripheral-clock lifecycle, while recording that MAD2-watchdog service
is conditional rather than part of ordinary boot. Ordinary boot
does not execute the Timer-1 readers. `make verify-mad2-timer1` accelerates only
the hardware timebase and proves terminal-count/FIQ5/acknowledgement; paired static
decode proves the same Timer-1 algorithm in both ROMs. `make verify-mad2-reset`
uses mapped controller MMIO to prove the software-reset, MAD2-watchdog and
CCONT-watchdog expiry paths. All reset the shared digital-baseband domain;
software and MAD2 watchdog publish distinct MAD2 causes, while CCONT watchdog
preserves the complete pre-expiry CCONT status rather than inventing a cause.
`make verify-charger-wake` powers the running phone off through its physical
power key, connects the charger while CCONT retains power, and requires cause
bit `0x04`, a complete digital-domain restart, a post-reset VCHAR sample and
acting-dead mode `0x0005`. The checker consumes only device traces and the
structural summary; it does not write firmware state.
`make verify-mbus` checks the identical v6.00/v5.01 receive-mode
initialization, asserts that ordinary boot transmits no bytes, and uses one
external byte to verify RX-ready, firmware consumption and FIQ2 acknowledgement.
The arbitrary byte's later parser outcome is not an acceptance predicate.
`make verify-display` checks the version-specific descriptor-`0x0749` EEPROM
locations, all fields explicitly authored by the equivalent v6.00/v5.01 reset
constructors, GENSIO LCD selection, command prefix and a complete 504-byte RAM
transfer in both 3210 ROMs. Constructor-unassigned bytes remain visibly erased
rather than being promoted to inferred product data.
The final startup-event field is deliberately excluded from both subsets: the
dispatcher continues receiving events after reaching the same accepted mode,
flags, contact state, SIM state, and exact frame.

`make verify-frontier` checks the 3210 request-driven external-service/SIM composition's
stable semantic predicates. `make verify-frontier-stability` repeats that check
with freshly seeded NVRAM. It reports full-summary
hash drift without failing because LCD-command, CCONT-byte/read, and similar
raw counters vary with harmless scheduling. Set `FRONTIER_STABILITY_STRICT=1`
when investigating those counters specifically. Use the repeatability target
before banking a new frontier; ordinary RE iterations should use the faster
single-run target.

`make verify-3310-frontier` boots the local 3310 v6.39 BIOS with its product
profile and requires the deterministic idle-screen frame. The profile supplies
only device-boundary behavior: the shared DSP/external-service/SIM models, the
58-exchange DSP calibration and the standard-channel 3310 battery-pack tuple.
No firmware address or state is changed by the gate. Unless
`PRESERVE_NVRAM=1` is requested explicitly, the harness removes the correctly
suffixed `noki3310_3` mutable flash, EEPROM and SIM files before each run so
MAME reconstructs them from the declared ROM regions and card defaults. Two
independent clean directories reproduce idle hash
`5871dd93badb1fa410dd22a6b7a12cf2d3b8f938e1514e989858dd45a2b35b74`;
the earlier oracle had silently depended on an undeclared persisted user
profile.

`make verify-3310-menu` extends that run with two separated physical Menu
switch cycles. The first is consumed by the ROM's wake/debounce lifecycle; the
second is decoded through IRQ0 and the five-row matrix and must draw the
deterministic `Phone book` menu. The gate changes only MAME input fields.

`make verify-3310-navigation` continues through the same physical matrix into
the Phone book submenu, moves the highlight once, and checks that the resulting
frame is deterministic. A separate run repeats that prefix and uses two
physical C-key cycles followed by an explicit 1.8-second firmware settling
interval to return to the exact `verify-3310-frontier` idle frame.
The two endpoints prevent a broken key path from passing merely because a
later screen happens to look plausible.

`make verify-3310-radio-authentication-boundary` protects the independent
NHM-5 authenticated-registration lifecycle. The call-control gates then cover
paging, incoming SETUP, ringing, physical Navi Answer/End and clean return to
PCH. `make verify-3310-radio-media-resilience` requires the product's
1 MHz/125-clock PCM profile, exact active-call save-state replay, bidirectional
FACCH/BFI recovery and SACCH/TF coexistence. The isolated
`make verify-3310-radio-physical-duplex` gate routes host capture only through
MIC2 and playback only through EAR; UI or protocol-level audio injection is
not accepted.

`make verify-3330-frontier` starts from the canonical virgin PMM and drives its
real first-boot editors through physical five-row keypad switches: stored phone
code `12345`, time `12:00`, and date `01.01.2002`. It requires the organic
compact DSP service-control completion through FIQ0, a save-state round trip,
and the deterministic v4.50 idle screen. `make verify-3330-navigation` repeats
that provisioning prefix in isolated NVRAM, enters Phone book, moves to
Messages and returns to the exact idle oracle.

`make verify-3410-frontier` starts from the canonical virgin NHM-2 PMM, lets
firmware compact the M28W320ECT parameter blocks, and uses one physical End-key
cycle to wake the idle UI after its normal blank-display timeout. The gate
rejects all-white 96-by-65 captures and requires the exact visible idle frame,
the organic compact DSP completion through FIQ0, a save-state round trip and
zero soft resets. `make verify-3410-menu` instead presses the physical
Menu key and requires the exact `Messages` screen. `make
verify-3410-navigation` proves both endpoints in isolated runs: Menu must open
`Messages`, then End must return to its expected idle phase. Correcting MAD2
FIQ8 from a 1 kHz placeholder to the firmware's 100 Hz centisecond source
changed the animation phase captured at unattended idle and in Messages;
returning from Messages still reaches the prior idle phase. Repeated runs
reproduce all three lifecycle-specific stable-pixel hashes. The fixtures operate
only MAME matrix fields; they do not write firmware state or post messages.
These exact historical UI oracles use the named `radio_disabled` hardware
composition so registered signal/operator pixels cannot be mistaken for an
MMI regression. Default-hardware registration is proven by the separate
NHM-2 radio gates below.

`make verify-3410-radio-registration-preserved` extends that physical
fresh-PMM prefix through NHM-2's autonomous scan, ARFCN-1 SI1–SI4, SDCCH
Location Updating, EF_LOCI persistence, release and steady camp, then repeats
the lifecycle from retained PMM. `make verify-3410-radio-registration-state`
requires protocol-identical replay both before acquisition and between
assignment and LAPDm completion. `make verify-3410-radio-unsuitable-cells`
proves barred, unattainable-RXLEV and mismatched-request-reference inputs do
not promote the handset into Location Updating.

`make verify-3410-radio-paging-preserved` proves one correctly grouped page,
organic Paging Response and clean release after both fresh and retained-PMM
registration. `make verify-3410-radio-paging-state` replays before page
delivery and during the assigned exchange. The negative paging gate requires
continued idle fill without RR access for wrong-group, unmatched and malformed
pages, while the unsuitable-cell composition must never reach paging.

`make verify-model-frontier-state` performs isolated mid-frontier save/load
round trips for both products. The positive frontier summaries require FIQ0,
complete LCD transfers and zero soft resets in addition to their LCD oracles.

The default runner reseeds an isolated per-run NVRAM directory from the
generated EEPROM profile. This prevents an old shared `mame/nvram` file from
silently changing product data. The structural oracle records the
profile-backed baseline, including its four stable EEPROM transactions and
corresponding FIQ/CCONT accounting.

Direct WebSocket acceptance targets use the same `prepare-run-files` contract
as `make run`: known LCD mirrors and the append-only MAME log are cleared, and
mutable handset NVRAM is reseeded unless `PRESERVE_NVRAM=1` is explicit.
Provisioning/cold-boot and provisioning/call pairs name their shared NVRAM
directory explicitly. Reusing a `RUN_DIR` therefore reruns a fresh-state gate;
it cannot silently inherit an earlier host-call session. Relative and absolute
run roots are both supported, subject to ordinary host write permissions.

MAME names the alternate v5.01 BIOS NVRAM system `noki3210_1`; the Makefile
seeds that directory explicitly rather than the default `noki3210` directory.
This distinction is load-bearing: regenerating a v5.01 EEPROM while retaining
an older `noki3210_1/eeprom` silently runs the stale product state.

The post-outgoing-call acceptance sweep uses unique roots and four build jobs:

- `make verify-power-lifecycle verify-power-lifecycle-v501 JOBS=4
  RUN_DIR=run_cleanup_power`;
- `make verify-radio-outgoing-call-host-media JOBS=4
  RUN_DIR=run_cleanup_host_media`;
- `make verify-3410-radio-outgoing-call-host-termination
  verify-3410-radio-outgoing-call-host-media JOBS=4
  RUN_DIR=run_cleanup_3410_host`;
- `make verify-3310-radio-outgoing-call-host-termination JOBS=4
  RUN_DIR=run_cleanup_3310_host`; and
- `make verify-3330-radio-outgoing-call-host-termination JOBS=4
  RUN_DIR=run_cleanup_3330_host`.

The complete 3210 hostile/reconnect/save-state list remains catalogued in the
outgoing-call section above; these commands are the focused cleanup gates.

## Summary fields

The generated summary records:

- emulated frames and LCD command/data/full-transfer counts;
- MAD2 soft reset count, IRQ/FIQ lines observed at frame boundaries, final
  pending status and optional save-state round-trip result;
- GENSIO control values;
- CCONT byte/read counts and command-byte set;
- EEPROM START conditions and GenIO signal-write count;
- reads and writes above the 3210's physical 128 KiB SRAM boundary;
- startup modes observed; and
- final task, startup, service-session and SIM gate values.

The committed oracle intentionally selects terminal state and lifecycle
predicates from that larger summary. It also requires both upper-SRAM access
counts to remain zero while the provisional wider map exists. Other raw
counters, timestamps, scheduler order, and full traces are too sensitive to
harmless timing changes. They remain available for review but are not pass/fail
criteria.

The Lua MMIO observer decodes ARM big-endian byte lanes explicitly. Summary
files are refreshed every 30 frames and from a display-independent periodic
callback through an atomic rename. This keeps the terminal state current while
firmware gates the LCD clock and lets a partial result survive a livelock or
externally terminated research run.

## Default profile

The historical CONTACT SERVICE frame with SHA-256 prefix `d8a9a7a58e587be8`
is not an acceptance oracle; the missing-hardware profile stops before that
presentation state. Its semantic subset requires
GENSIO controls, startup modes, running task, mode/flags, service-session status, and
SIM gates. LCD, IRQ, CCONT, and EEPROM counts remain in the generated summary
for diagnosis without making timing-dependent totals part of acceptance.

## Coherent frontier profile

`make verify-frontier` enables the request-driven DSP/external-service peer and the
ordinary SIMI/FIQ6 card device. Its structural oracle records startup mode
`0x0004`, flags `0x0f`, service-session status `0x0049`, no-SIM clear, and SIM enable
set. This semantic state is the forcing-free frontier oracle.

`make verify-dsp-transport` protects the separated transport/HLE/peer
composition. It requires complete-packet TX consumption, RX publication before
FIQ0, shared-service completion through IRQ4, the established type-`0x70`
completion, and the request-correlated external session. Its v5.01 leg checks
only the common doorbell/service-completion mechanics. An active coherent run
also crosses a save/load boundary.

Fixture frames and their oracle status:

| Fixture | Frame hash (SHA-256) | Oracle status |
|---|---|---|
| Unprovisioned Security-code prompt | prefix `6471d1a5803619c2` | Research evidence only; outside `make verify-frontier` because the hardware-boundary profile does not reproduce the additional display transfer |
| Provisioned idle `Menu` | prefix `dbf2704cb945d56b` | Research evidence; structural state remains mode `0x0004` |
| Provisioned `Phone book` menu after left softkey | raw one-shot mirror `9b2ac7477b5be11aa6b4f178f781ff2799754b0b5ff6ce8f66221564d0f914d1` | Masked stable pixels (animated 20x12 icon region excluded) are protected by `make verify-mmi-menu`; the raw hash is not part of the canonical structural oracle |
| Registered test operator `01` | crop SHA-256 `59dd0d4f80f705c98be148c7f60f3171d2b66d7a434fba51feef7a0134ada9a8` | `make verify-radio-operator` couples the full registration trace to a 12x7 glyph crop, excluding unrelated animated indicators |

The provisioned EEPROM profile matches the synthetic phone identity and
suppresses the security prompt. The IRQ0 keypad source reaches the real matrix
scanner and publishes decoded keys while mode `0x0004` remains selected. The
separate unprovisioned `12345` fixture completes the security editor through
`0x0578`. The verifier returns one and the observed callback publication is
`0x05e1`; that status is callback-scoped and is not, by itself, an accept/reject
code.

## Nokia 3210 v5.01 control

`make verify-3210-v501` runs the independent NSE-8 v5.01 full flash with its
own generated EEPROM profile as a same-product structural control.
`make verify-mmi-menu-501` adds provisioned identity and physical left-softkey
input, opening the same `Phone book` menu as v6.00 with the same stable-pixel
oracle. Its input is delayed until seven seconds and the run lasts fourteen
seconds because periodic CCONT RTC work shifts the v5.01 editor-ready schedule;
the v6.00 fixture retains its five-second input.

The useful invariants are the task-1 and SIM results. The run organically observes modes
`0x0001`, `0x000d`, and `0x0004`, then remains in mode `0x0004` with readiness
flags `0x0f`. At its relocated state block it also clears
no-SIM and sets SIM ENABLE organically. This independently reproduces the v6.00
task-1 terminal mode and validates the shared SIM device contract. Both
revisions now settle at service-session status `0x0049`; the earlier v5.01
`0x00c9` result depended on the removed speculative result-5 service response.

`tools/find_literal_loads.py` scans Thumb-1 PC-relative literal loads while
normalizing the swapped image's 32-bit halfword order. It is intended for static
producer censuses; use `--raw` only when searching for the on-disk literal value.
