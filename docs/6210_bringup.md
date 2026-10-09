# Nokia 6210 bring-up

## Current boundary

NPE-3 v5.56 PPM C reaches the final DSP verification wait at
`0x426cc2..0x426cc8` after 232 alternating buffer handoffs. The product-local
profile models the independently decoded MAD2 DSP release/ready and GENSIO
contracts. Final DSP completion is not published. Boot/UI, SIM, network,
calls/audio and handset save/load are not yet validated on the normal machine.

Separate research compositions establish the own-upload/runtime boundary:
`npe3stage` executes the acquired verifier/loaders and retains native ownership
at absent mask routine `2c75`; `npe3hle` explicitly hands transport to HLE there.
The latter uses the unchanged acquired PMM, request-correlated D0 discovery,
the own compact self-test consumer and a declared nominal battery sample.
Acceptance gates prove graphical Menu/SIM PIN interaction, Calculator,
persistent SIM contacts with cold readback, laboratory registration/operator
presentation, physical incoming/outgoing call signaling and incoming/outgoing
SMS. Physical Menu decodes as `19` on the 96x60 display. Native resident DSP
execution, measured final silicon verdict, speech and full hardware fidelity
are not claimed. The research phone-service milestone is complete; the normal
machine remains fail-closed pending authentic native DSP completion.

Physical `*123#` followed by Send also completes a laboratory USSD transaction
on `npe3hle`: the exact processUnstructuredSS request receives a correlated
successful response, the firmware renders `Service reply / Nokia test network`,
and physical Back returns to the registered idle frame. The isolated runner's
`--scenario ussd` checks ordered handset inputs, protocol release, both reviewed
96x60 frames and persisted EF_LOCI. This is HLE network service acceptance,
not native DSP or speech evidence.

`verify-6210-ussd` repeats that acceptance. `verify-6210-call-divert` separately
drives physical `*#21#` and Send: an exact InterrogateSS request for service
`0x21` receives an inactive result, renders `Service not active`, releases RR
and returns to registered idle after Back. These checks do not establish
divert registration, activation, deactivation or persistence.

The separate runner scenario `divert-lifecycle` extends coverage within one
coherent session: physical `*21*5551234#`, `*#21#`, `#21#`, `*#21#` produce
register/active-query/deactivate/inactive-query responses in order. Reviewed
frames show `Divert activated`, `Done`, the inactive service summary and
`Service not active`, followed by registered idle. The active query's frame
does not establish displayed forwarding-number content. Save/load and
cold-process forwarding persistence are not covered by this scenario.

`state-divert` saves after physical forwarding activation, restores exact
CPU/RAM/time and compares the one-second observed protocol replay. Only after
load does a fresh physical query establish retained active forwarding; physical
deactivation and a final inactive query complete normally with the same four
reviewed result frames. This scenario establishes save/load, independently of
cold-process persistence.

`verify-6210-call-divert-cold` runs three independent processes with unchanged
own provisioning: physical registration, retained-storage active interrogation
and external incoming-call forwarding without handset paging, then a separate
fresh-storage inactive interrogation. The network-owned subscription file stays
byte-identical during the retained query. Result and idle frames are reviewed
6210 oracles; the fixture does not import another product's provisioning or
claim native speech.
The gate also copies only its own retained handset state into an isolated
fourth process and damages the network subscription checksum. MAME must report
the failed NVRAM read; a physical query must then return inactive with the
reviewed result and idle frames. The original retained evidence is unchanged.

## Inputs

`roms/noki6210/6210_556c.fls` is 0x3a0000 bytes with SHA-1
`3d9ea319503e78ec69b60d72cda23e461e118ea9`. The product-local PMM tail is
0x6000 bytes with SHA-1 `b3a527ede1be87bd715fb3741a81eef5bd422efa`, mapped
at region offset `0x3fa000`. Sources and archive hashes are in
`roms/README.md`. Uniform-fill DSP audit files are not executable ROM6 evidence.

## Hardware contracts

- At `0x4dc0e4`, firmware writes CTSI offset 2 with `0x40`, then sets bit 2
  and polls bit 4. `LSRS #5` tests original bit 4 through carry, not bit 5.
  The profile exposes that ready bit only when the release line is asserted.
- Verifier start `426c36..426c3e` separately sets CTSI+2 bit 0 after writing
  the uploaded descriptor. Execution therefore uses bit 0, not the bit-2
  early readiness handshake; the original frontier gate still reproduces.
- At `0x4ec7b0`, GENSIO control `0x22` selects CCONT; a write to offset
  `0x2c` precedes polling status offset `0x6d` bit 2 at `0x4ec7bc` and
  reading offset `0x6c`. Control bit 2 stays clear, so receive-ready follows
  the byte write rather than a control-bit trigger. The LCD uses `0x2e/0x6e`.
- The 6250 has a separate decoded NHM-3 configuration and initial-record
  research-PMM comparison; its phone-service gates are documented in
  [6250 bring-up](6250_bringup.md). Neither its native boundary nor its
  provisioning is inherited from the 6210 configuration.

## Keypad

The NPE-3 v5.56 scanner `4f84c8` masks `6b`, drives `a8/28` and samples
`2a`. Its ordinary scan forms `drive * 5 + sense` at `4f8550..4f8556`.
Decoder `4facfa` uses the normal table at `2869b8` and the special table
at `2869d4`. The normal table is:

```text
5a 5a 5a 5a 5a
11 19 01 02 03
0e 17 04 05 06
0f 18 07 08 09
10 1a 0c 0a 0b
```

Special input bit 4 maps to Power (`0d`); the other four special entries
are `5a`. These tables independently select the same logical input layout
as the 5210, so the 6210 reuses its port declarations with a product-local
five-line controller contract and Power mask `10`, not its hardware profile.
The previous inherited 3310 input declarations and four-line default were
not NPE-3 evidence.

`make verify-6210-keypad-controller` checks both pinned ROM tables, all 20
matrix keys across all five driven lines (100 reads), and held-Power
press/release through the actual MAME controller. It is an MMIO conformance
fixture: Lua drives controller registers and host inputs, never firmware RAM
or messages. It restores the row/direction/mask registers. This proves the
configured matrix, not firmware decoding, debounce or usable menus. The
separate research-HLE menu gate below proves physical firmware decoding and
menu interaction; the normal machine still retains the native DSP boundary.
The ordinary input exerciser selects this product's logical key layout too.

Nokia's [NPE-3 board schematics](https://altehandys.de/downloads/ser-no-6210-schematics.pdf)
(Version 1.0, 09.02.2001) show the fitted matrix switches on page 5 and
identify the MAD2WD1 ROM6 V16 part on page 4. The physical sheet's ROW/COL
names are not the input fixture's abstract drive/sense names. The archived
PDF is `roms/research/npe3/npe3-schematics-v1.pdf`, 2,656,135 bytes,
SHA-256 `559e9718a9dad703694f17d349383f75af4237b1c26cf94ecdb4aafe641f1e43`.
It does not specify the missing DSP PROM word or final HPI publication.

## Display

The same Nokia sheet identifies H400, LPH7690-1, as a GD45 **96x60** LCD
(page 5). This is primary evidence; the 96x65 claim in the reference
emulator and 95x60 secondary descriptions are not the selected geometry.
Firmware `4e656c..4e659c` clears banks 0..7, writing 96 zero bytes per
bank: command `24`, then `40|bank`, `80`, 96 data bytes, and finally `20`.
The profile therefore uses 96x64 controller storage with a 96x60 visible
area. The capture harness uses the same storage/visible dimensions.

The bootstrap gate requires all eight ordered bank clears (768 bytes),
and a 96x60 blank capture, not merely a blank final picture of any size.
This validates the boot transfer extent,
not ordinary rendering or the complete controller command vocabulary.
The PCD8544-compatible backend remains provisional, as do orientation and
the isolated unused commands observed outside this clear routine.

## DSP upload

The pointer at RAM `0x170070`, read without modifying firmware state, selects
flash descriptor `0x225c2c`: `0f00 0000 00df 0f00 00dc 0000`.
The 223-word program following it has SHA-1
`6646da3c5be9c70deda7e0b5b9f257d5d2ace815`, identical to the stock NSM-3
staged verifier. Identical code does not establish identical final results.

The MCU initializes DSP buffer descriptors with remaining count `0001:d000`
and block size `0200`. The loop at `0x426c58` sends 231 full blocks, followed
by 510 samples and two `ffff` terminators. Flash source advances by `0x20`
per halfword. Shared offsets `0fe/100` alternate ownership, with 116 writes
of zero to each. The HLE acknowledges transport ownership but supplies no
final verdict. At `0x426cc2`, firmware waits for shared offset 2 to leave
`ffff`, then stores the result pair in its own bootstrap state.

The next question is the staged program's final publication under NPE-3's
ROM6 memory/peripheral contract and larger input stream. The NSM-3 fixture's
explicit ROM-version and COBBA assumptions must not be promoted as measured
6210 values. The collaborator's self-test responder is a later request/reply
lead, not evidence for this earlier bootstrap completion.

### Executable calculation and remaining inputs

`make verify-6210-verifier` runs the actual 223-word program with NPE-3's
own flash samples, remaining count `0001:d000` and 232 handoffs. It first
fails closed at the unsupported peripheral read (`port 002d`, PC `0f9f`).
Three explicit sensitivity configurations then attach the existing COBBA
register model and supply PROM word `ff87` as either 6 or 4. All calculate
fingerprint `f65a:0d46` at DSP data `04f7:04f8`, with PMST `ffa8`.
The companion 8210 fixture still calculates `c2e0:6006` over 116 blocks.

These are core-fixture calculations, not measured silicon results. At the
final publication, shared word 0 follows the supplied COBBA register-F value
(0 or `0016`), while words 1/2 follow the supplied immutable PROM value
(6 or 4). Neither is selected by the calculated fingerprint. Thus the
larger NPE-3 stream does not itself resolve the missing publication input;
ROM6 PROM mapping/content and the peripheral contract remain unvalidated.
The handset profile does not receive any of these sensitivity values.

At MCU `426cca..426cd2`, the loader stores shared word 1 at `16fff0`
and word 0 at `16ffee`, relative to bootstrap structure `16ffe4`.
The base pointer comes from literal `426ddc`. This identifies the result's
firmware-owned destination without claiming the later self-test responder
or its IRQ4 upload-completion flag supplies the earlier verifier result.

## Acceptance

`make verify-6210-bootstrap` performs a fresh isolated run and checks CCONT
receive-ready, all 232 ordered handoffs, no reset and the uncompleted final
wait. It is a frontier gate, not usable-phone acceptance. Generic harness
task/mode RAM addresses still describe the 3210 and are not NPE-3 semantics.

## Research-HLE acceptance

Own descriptor `224a44` uploads 104 program words from file `24a50`, SHA-1
`440bf49f1eba4cadb12f7f7581c992b0025807d6`. Descriptor `224b70` uploads
**629** second-loader words from `24b7c`, SHA-1
`734491708f00ca396b422ec4d489d0d4c491d0c9`. Native execution publishes
`0000/0006` under the explicitly fragment-derived PROM/COBBA inputs, requests
selector `14` once and selector `01` 156 times, verifies the whole second
loader and installs 422 words at `0590..0735`. The next call is missing
resident `2c75`. This executes own acquired uploads, not a ROM6 mask dump.

The runtime comparison's type-05 discovery request is
`1eff00d000030101e000`; response `1e0002d000030401c100` is acknowledged by
the MCU with `1e0200d0000305014100`. Own class-74 dispatcher `305a0c..305a5e`
calls `3029d4`; command `0d` selects `302a52`, checks flag `17fd99` bit 2,
cancels timer `1b` and consumes fault bits 0/1 from byte 9. The declared
compact peer clears fields `17fbef/17fbf0/17fbf1`; identity/record replies
are not enabled.

Independent PMM replay finds 295 records, a 2500-byte initial cache record,
and journal end `1bb0`. Both base and replayed application checksums compute
and store `6d85`. No journal deletion, donor storage or checksum edit is used.
This static grammar assessment is not a full runtime NV-reader census.

Analogue routine `3d71f8` reads source 7 through `4f3962`; table `2869dc`
maps it to selector 2. Calibration gain/offset are at `17fce0/17fce4`, followed
by integer scale 1500/232. Both samples must be in `0708..157c`. Conservative
full scale `3ff` produces rejected `19f0`; the declared nominal input `230`
produces accepted `0e31`, with acquired gain `3f809bca` and zero offset.
This is a nominal battery fixture, not a measured electrical transfer curve.

`make verify-6210-stage`, `verify-6210-runtime` and `verify-6210-menu` invoke
`run_noki6210_acceptance.py` with a new isolated `RUN_DIR`. The runner checks
pinned own inputs, refuses an existing directory and writes `acceptance.json`.
Menu acceptance requires all 50 ADN reads, physical Menu followed by own
decoder `4fad30` reporting `19`, and the reviewed Messages frame SHA-256
`8c7650fdb0514ec34c85b89795e529de062e6f141268a507bafc7eb77370df65`.
Neither a host press alone nor a blank framebuffer establishes interactivity.

### Applications and persistent phonebook

`verify-6210-calculator` navigates the product's physical menu keys and pins
the reviewed `12 - 3 = 9` Calculator frame (SHA-256
`2c5e99fd98ab56d41574c613021a7ed5270fe7d39e94ec57a1f52b9f732199fc`).
Menu position is harness policy, not a hardware contract.

`verify-6210-phonebook` enters `A` / `123` through physical keys, requires
the EF_ADN record-1 UPDATE RECORD body and `9000` response, then starts a
second isolated process with the first process's persisted NVRAM. The cold
process must read record 1, select the contact physically, issue no update,
and display the reviewed contact frame (SHA-256
`39ca7b13f4afdc8c6e3ca553d7fd0bafcdd7dd3de42c054edf0f445713dd09bc`).
The storage validator independently checks the saved name and number.
Neither process patches phone memory or seeds a contact directly.

### Laboratory registration

The runtime emits an organic type-`0x56` acquisition request with a 160-byte
body, starting `0023` followed by erased candidate entries. Own RX dispatcher
`4f604a` indexes the thirteen-entry table at `4f6078` for types `83..8f`.
Type `8b` calls `45835c`, which posts to **task 14**, not the NSM-3 task 12.
Type `89` calls `4580e8`; `458106..458110` compares body bit 0 against pending
context byte 2. `noki6210_radio_contract.py` pins these product-local facts.

The NPE-3 candidate-window peer completes organic acquisition, Location
Updating and acknowledged release. `verify-6210-registration` requires the
ordered exchange, persisted laboratory LAI `00f1100001` and updated EF_LOCI
status, plus the reviewed `DCT3 LAB` idle frame (SHA-256
`1138954cc94944c83019823ea500fa9ea9f8857c76e3d8ba929cdc40db4c0b74`).
The unattached-input accessory contract is described below. Neighbour/handover
and speech are not accepted; unrecovered handover fields remain unset.

### Phone-service acceptance

`verify-6210-outgoing-call` physically dials `1234567` and presses Send/End.
`verify-6210-incoming-call` queues one laboratory network call, captures
ringing and connected screens and physically answers/ends it. Both require
ordered CC/RR establishment, traffic assignment, Connect acknowledgement,
Disconnect/release and return to paging. Own traffic/release configurations
are 24 bytes, not the NSM-3 20-byte forms. Physical End emits release
parameter `14`, now selected by the NPE-3 peer. Incoming Call Confirmed is
`8308040460020081150101`. These prove signaling, **not speech**.

The physical NPE-3 board now attaches COBBA EAR to the speaker and the
internal microphone to MIC2, as specified by Nokia System Module issue 1
(09/00), table 8. Unity MAME routes declare connectivity, not analog gain or
the runtime codec mux. The PCM profile remains unspecified and the HLE
voice selector remains disconnected; these routes do not enable fabricated
call audio. The independently mapped command-8 speech-request field and
exact missing clock/framing evidence are owned by
[`external_call_bridge.md`](external_call_bridge.md).

`verify-6210-incoming-sms` requires segmented GSM delivery, CP/RP acknowledgments,
EF_SMS delivery/read-status writes and persistent `hello`, plus its graphical
read frame. `verify-6210-outgoing-sms` physically composes `A` for `5551234`,
requires the exact own SMS-SUBMIT and network acceptance/CP/RP/RR closure,
then pins `Message sent`. NPE-3 selects relative TP-VP `ff` (63 weeks), not
the sibling fixture's `a7`; its message reference remains handset-managed.
The composer is the first Messages submenu; menu positions are harness policy.

`verify-6210-security` changes only the external laboratory SIM to a
PIN-enabled profile. Physical `1234` and OK must produce VERIFY CHV1 and
`9000`, then open the reviewed Messages menu. The acquired phone PMM is still
unchanged and does not request a phone-lock code on this boot. This gate
proves SIM PIN interaction, not a recovered phone-lock EEPROM contract.

Delayed-PIN registration requires coherent laboratory topology: acquisition
uses ARFCN 35, whereas the default network configures ARFCNs 1/2. With physical
PIN entry starting at eight seconds, the `57/03050000` background measurement
request receives an HLE `8b` response through the firmware-owned task-14 route
and completion parser. An isolated ARFCN 35/36 configuration reports the
receivable serving cell at -60 dBm and permits Location Updating after CHV1
acceptance, including persisted EF_LOCI LAI and status. The exact observed
candidate/release channel header is `12 02`, rather than the no-PIN fixture's
`00 00`. `verify-6210-slow-pin-registration` requires the ordered background
request, serving-cell measurement, enabled task-14 route/completion, physical
PIN acceptance, full registration sequence and persistent EF_LOCI. The ordinary
security gate remains PIN/menu-only; no-PIN registration is checked separately.

`verify-6210-pin-host-incoming-call` composes the PIN-enabled card and coherent
carrier topology with host-originated paging, physical Answer/End, the own
NPE-3 CC/RR grammar, reviewed caller pixels and exact registered-idle recovery.
It requires PIN acceptance and location persistence before validating the call.
This proves host call signaling, not speech or native DSP execution.

`verify-6210-pin-host-incoming-sms` independently composes the same physical
authentication and registration with host SMS delivery, CP/RP acknowledgments,
physical Read, the existing exact message-body image and persistent read-status
SIM record. The host checker is explicitly bound to ARFCN 35; the original
no-PIN host SMS fixture remains on its default topology.

`verify-6210-pin-host-outgoing-call` verifies physical `1234567`/Send/End,
the host connect decision and complete own CC/RR lifecycle after PIN-enabled
registration. `verify-6210-pin-host-outgoing-sms` verifies physical `A` to
`5551234`, the host submit decision, CP/RP completion and reviewed Message sent
text. Both retain the unchanged acquired PMM and explicit HLE boundary.

`verify-6210-pin-host-rejected-sms` and `verify-6210-pin-host-silent-sms`
independently verify authenticated host RP error and full firmware-timeout
recovery on the coherent ARFCN35/36 network. They require physical submission,
one successful PIN VERIFY, own registration/EF_LOCI, correlated host outcome,
the respective protocol closure, reviewed failure-screen pixels and physical
End/Menu recovery. Fresh evidence is `run_6210_pin_host_rejected_sms_01`
and `run_6210_pin_host_silent_sms_01`; neither requires timing overrides or
firmware-state writes.

`verify-6210-pin-phonebook` saves `A / 123` through physical keys, starts a
new MAME process with the saved card and identical laboratory topology, enters
PIN again, and verifies exact contact readback. It checks retained-location
registration, one successful VERIFY per process, enabled CHV1/PIN/retry state
and byte-identical card storage across readback. No session authorization is
transferred between processes.

The `verify-6210-pin-state-idle`, `-call` and `-sms` gates save the
authenticated runtime, then require exact CPU/RAM/emulated-time restoration
and a matched protocol replay window. Physical PIN confirmation, successful
VERIFY and completed registration are required in order before saving, not
merely somewhere in the whole run. The call fixture physically releases
the restored call; the SMS fixture physically reads the restored delivery and
checks the exact message image and persistent read-status record. Each run
retains enabled CHV1, PIN `1234` and a single successful startup VERIFY.

All service runners start with new working directories; incoming network
events are selected through MAME configuration, not firmware injection.
No donor PMM, forced phone state or borrowed DSP verdict is used. The staged
verifier's declared PROM/COBBA inputs and runtime HLE substitution at missing
mask routine `2c75` remain research assumptions. Native mask execution,
measured DSP self-test values, speech/audio parity and electrical ADC units
remain separate work, not implied by the phone-service gates.

## SIM Toolkit

`make verify-6210-sim-toolkit RUN_DIR=run_6210_toolkit` uses unchanged
acquired phone PMM and a Phase-2+ laboratory SIM selected through SATCFG.
The own firmware reads EF_PHASE `03`, sends a nine-byte TERMINAL PROFILE,
receives normal completion, then discovers a card-owned proactive command
through STATUS `91 16`. It performs FETCH `A0 12 ... 16`, renders `DCT3 SAT`,
and after physical OK sends the exact successful DISPLAY TEXT TERMINAL
RESPONSE. The gate requires ordered APDUs, both reviewed 96x60 frames,
registered-idle recovery and persisted EF_LOCI. It does not inject any latch,
message, firmware state or proactive object into the handset.

`verify-6210-sim-toolkit-interactive` independently extends the own profile
to GET INKEY and GET INPUT. Physical `5` and `42` return exact successful
terminal-response text TLVs `0d020435` and `0d03043432`. Ordered FETCH,
TERMINAL RESPONSE and pending-status lengths are shared protocol checks;
five reviewed NPE-3 frames and its own registration/EF_LOCI remain separate.
The final idle hash is product-local, not inherited from 6250.

`verify-6210-sim-toolkit-menu` additionally accepts card-owned SET UP MENU:
the preceding GET INPUT response returns `9128`, FETCH length `28`, and the
handset returns successful command-4 response `810304250002028281030100`.
Physical Menu/Scroll Up opens `DCT3 menu`; selecting Continue sends menu-item
1 ENVELOPE `d30702020181100101` through `A0 C2` and receives `9000`.
Physical End returns to the reviewed registered idle. Four extra own frames
cover the menu entry, items, selection result and idle, independently of the
existing five interactive frames. No callbacks or menu objects are injected.
Menu-triggered SMS/calls and the second item's behavior remain unvalidated.

This establishes these command lifecycles, not all Toolkit commands or native
DSP execution. The completion trace records the status bytes actually appended
to data-bearing card replies; ordinary `9000` and proactive `9116` responses
use the same observation-only path.

## Host incoming call

`tools/run_noki6210_acceptance.py RUN --scenario host-incoming-call` enables
only `CALLHOST` in private configuration, leaving acquired PMM unchanged.
An idle frame at 32 seconds releases a host request for caller `5551234`;
the runner requires correlated queued/paging/alerting/connected/ended phases.
Physical Send and End must satisfy NPE-3's own Call Confirmed and 24-byte
traffic/release contracts. Reviewed caller text and exact registered idle
before/after the call are required. `--port` selects the unused HTTP port
(default 16210). This is HLE signaling acceptance, not speech or native DSP.

## SIP caller cancellation

Pending outgoing restoration is also covered by
`verify-6210-sip-pending-outgoing-restore`: physical `1234567`/Send, SIP 180,
exact architectural save/load, external CANCEL/487, a single cause-41
termination, complete CC/RR release and exact registered idle. The host
dialog is cleared rather than saved. No CONNECT, media or redial is allowed;
this does not establish answered calls or native speech.

`verify-6210-sip-alerting-incoming-restore` saves during an organically
ringing incoming call and restores exact PC, SP, RAM checksum and time.
The outstanding SIP INVITE is rejected rather than replayed; one cause-41
clear completes CC/RR release under the new epoch. Physical Exit dismisses
the single missed-call notification and returns to exact registered idle.
Answer, CONNECT and accepted media are forbidden; connected-call SIP
restoration and speech remain unproved.

`make verify-6210-sip-cancel RUN_DIR=run_6210_sip_cancel` runs the unchanged
acquired PMM in `npe3hle`, with fresh private NVRAM/configuration and the
optional PJSIP 2.16 stack. A reviewed registered-idle snapshot at 32 seconds
releases the real SIP INVITE. The endpoint cancels only after the handset
reaches alerting; the checker requires CANCEL/487, exactly one accepted
termination, CC release, LAPDm channel release and the ended phase. Answer,
CONNECT and all bridge media counters are rejected.

The firmware renders `1 missed call`. A physical right-softkey Exit then
returns to the exact registered-idle frame. Product-owned bootstrap,
registration and persisted EF_LOCI are checked as well as SIP signaling.
The runner refuses an existing run directory or stale readiness artifact.
The generic SIP runner permits this unanswered scenario and the outgoing
busy/unavailable fixtures below; answered SIP, speech and native DSP are not promoted.

`make verify-6210-sip-idle-restore RUN_DIR=run_6210_sip_idle_restore` first
performs the existing exact idle architecture/time and protocol-replay check.
The fresh incoming call must use host epoch 2 and start after restoration;
the same missed-call frame, physical Exit and registered-idle recovery are
required. The save contains no SIP dialog and no media is admitted.

## Host outgoing call

`host-outgoing-call` runs the ordinary physical `1234567` dial/Send/End
fixture with only `CALLHOST` enabled. The external runner verifies those exact
digits and submits a correlated connect decision; wrong-ID and duplicate
decisions must be rejected. The own product checker requires traffic assignment,
physical clearing and registered paging. An independent host checker requires
one queued/accepted connect and connected/ended phases. The shared queue log
is not itself evidence of fallback: host decisions enter that same saved queue.
This verifies outgoing host signaling, not audio or a SIP call.

`make verify-6210-sip-outgoing-busy RUN_DIR=run_6210_sip_outgoing_busy`
independently verifies physical `1234567`/Send against a real local PJSIP
486 response. The checker decodes the handset's SETUP digits, requires the
correlated busy decision and complete CC/RR release, and rejects CONNECT
and bridge media. The final frame must match registered idle exactly.
The runner retains unchanged acquired PMM, private fresh storage, own upload
and self-test checks, registration and persisted EF_LOCI checks. This is
research-HLE failure signaling and recovery, not answered-call or speech
acceptance.

`make verify-6210-sip-outgoing-unavailable RUN_DIR=run_6210_sip_outgoing_unavailable`
uses the same physical dial and isolated own-PMM preparation against real
PJSIP 480. The firmware consumes the no-answer termination with cause 18,
completes CC/RR release and returns to the exact registered-idle frame.
CONNECT and bridge media are rejected, as in the busy fixture.

## Host SMS acceptance

Named gates use the same isolated own-PMM runners and require new run
directories. Execute them sequentially:

```sh
make verify-6210-host-incoming-sms RUN_DIR=run_6210_host_sms_incoming
make verify-6210-host-outgoing-sms RUN_DIR=run_6210_host_sms_outgoing
make verify-6210-host-rejected-sms RUN_DIR=run_6210_host_sms_rejected
make verify-6210-host-silent-sms RUN_DIR=run_6210_host_sms_silent
make verify-6210-host-silent-sms-realtime RUN_DIR=run_6210_host_sms_silent_realtime
```

The real-time silent-SMS gate uses `--throttle`, retaining the same own-PMM,
protocol, failure-pixel and physical End/Menu recovery checks as the fast
fixture. Its accepted host decision at 34.09 seconds completes at 101.11
seconds, within the host runner's explicit 180-second total wall-clock
completion budget. This budget is not an emulated firmware timer; unrelated
notifications do not extend it. The generic runner accepts a finite positive
`--completion-timeout` override for slower host environments.

The isolated runner's `host-incoming-sms` scenario enables only `CALLHOST`,
waits for current-epoch adapter readiness and laboratory registration, and
delivers host `hello` from `5551234`. Independent host correlation and NPE-3
handset checks require CP/RP/RR closure, exactly one page, delivery/read-status
writes, persistent read content and the reviewed full message screen.

`host-outgoing-sms` uses the ordinary physical composer to submit `A` to
`5551234`. The host runner requires exactly GSM7 `41`/one septet, rejects an
unrelated request ID and rejects a duplicate success decision. The handset
must independently satisfy its own relative-validity `ff` SMS-SUBMIT grammar,
channel release, paging recovery and reviewed Message sent text (excluding
the animated envelope, unlike the unchanged ordinary full-frame gate). No automatic
SMS network fixture is enabled in either host scenario. The unchanged acquired
PMM and research HLE/native distinction apply; these do not prove native DSP.

`host-rejected-sms` uses the same physical `A` submission but requests an
explicit RP error. The own NPE-3 checker requires CP acknowledgment, RP error,
handset CP acknowledgment, release and resumed paging, forbidding a success
RP-ACK. Correlated error acceptance, wrong-ID/duplicate rejection, physical
End/End/Menu decoding and reviewed `Message not sent this time`/Messages
frames are also required. The independently reviewed 96x60 failure/menu text
regions share the 6250 frame checker; packet and input contracts remain NPE-3.
`host-silent-sms` separately supplies CP-ACK without an RP result. Firmware
paints `Message sending failed`, issues main-link DISC, receives UA and
deconfigures traffic before closing the host request (observed near 101.1
seconds). The checker requires no RP error/success, resumed paging and
physical End/End/Menu recovery. Its shared release grammar receives the
independently decoded NPE-3 `ff` validity value, not the sibling `a7` value.
The 135-second fixture does not shorten firmware timers.

## Save-state acceptance

`verify-6210-state-idle`, `verify-6210-state-call` and
`verify-6210-state-sms` save and restore emulated time, all 37 exported ARM
registers (including CPSR and banked state), and a digest of the complete
handset RAM. The shared read-only `arm_architecture_snapshot.lua` supplies
the same register order and RAM interval as the 6210/8890 alarm fixtures.
The checker rejects any register mismatch or incomplete snapshot.
Each requires a nonempty, identical
ordered protocol replay interval after restoration. Location Updating Accept
and its subsequent Channel Release must occur before the saved snapshot;
post-load registration cannot satisfy this prerequisite. The call fixture then
presses physical End and verifies release and resumed paging; the SMS fixture
opens the delivered message and verifies its persistent read status and frame.
These remain signaling/storage checks, not speech validation.

All three scenarios reproduce on the built `0e4649a` source checkpoint,
including the C54x XF-output changes. Independently fresh accessory boots
also pass the cold-repeat comparison on that build. The acceptance covers
exact saved architectural state/time, selected protocol replay and the
scenario-specific physical-input continuation; it does not establish
cross-host determinism, connected SIP restoration or native speech.
Use the project's Capstone-enabled Python environment. For Make invocations,
pass an absolute `PYTHON` path: the nested MAME build changes working
directory. These gates select the research-HLE composition, not normal
NPE-3 native verification.

MAME cancels pending Lua waits when loading a state. The host fixture therefore
explicitly resumes its physical input schedule after the replay interval;
it does not restore or inject handset state. ARM7's named `PC` is a debugger
cache: the snapshot reads the saved architectural `R15` instead.

## Physical power lifecycle

`verify-6210-power-cycle` starts a fresh private `npe3hle` process with its
unchanged acquired PMM. Registered idle at 20 s is followed by physical
Power at 25 s, held for four seconds. Firmware commands rail-off at about
31.68 s; the display is blank. At least fifty seconds of continuous CCONT
RTC ticks are required with no DSP RX publication, peer shared-RAM writes,
FIQ0 notifications, native port activity or radio LAPDm output. This exceeds
the last 49-second watchdog reload and prevents an old counter from silently
rebooting the supposedly off domain. RTC progression is anchored to the last
pre-shutdown tick, including minute rollover, not an assumed clock origin.

A separate physical Power press at 90 s restores the digital domain through
PWRONX cause `02`, not a charger event. Firmware consumes ready/PWRONX
status low bits `03` (observed full status `33` with retained RTC sources),
executes its own native verifier/loaders before the explicit
missing-mask HLE handoff, and completes its own compact self-test again.
Both boot stages are checked independently; passive debugger log caps are
re-armed without changing firmware registers or RAM.

The warm Location Updating request carries retained laboratory LAI:
`0080013f4905087200f110000133080910101032547698`. Organic EF_LOCI reading
must precede it. Unlike the NSB-6 warm lifecycle, NPE-3 rewrites the LAI
after accept but does not redundantly write the already-valid location
status byte. The cold registration grammar remains unchanged. Exact
registered-idle frames before and after restart and physical Menu decoding
as `19` into the reviewed Messages frame prove UI recovery without security
code injection or manual phone-lock entry.

NPE-3 firmware explicitly writes RTC seconds `07=00` after PWRONX wake.
Retained device counters while off do not imply untouched software time,
cold-process calendar persistence, measured wake latency, complete
peripheral power gating or native speech.

## Physical Calendar entry

`tools/noki6210_calendar_input.lua` navigates the own research-HLE handset
through physical Menu, seven Down presses and Select. Fresh storage produces
the Time prompt rather than Calendar immediately. Physical `13:47` and OK
write CCONT's shared latch registers `0b=2f`, `0c=8d`, then expose the Date
prompt. Physical `07-10-2026` and OK render `7 October 2026`, `Wednesday`
and the ordinary Options/Back Calendar window. No calendar RAM, internal UI
events, clock registers or foreign provisioning are written by the fixture.

`make verify-6210-calendar-cold RUN_DIR=run_6210_calendar JOBS=4` runs the
entry fixture, copies only that process's own NVRAM into a separate cold
process, and executes `noki6210_calendar_cold_input.lua`. The second fixture
only navigates to Calendar; it never enters replacement time/date. Both
processes must pass the own upload/self-test and registration checks; the
cold process must consume retained EF_LOCI. Both retain CCONT hour/minute
`13:47` and render the reviewed Calendar hash
`fcf0327cc0cc4edb952d0dc37621e467c0810b867738de6c91f39354a9c3a3b3`.
Firmware resets seconds during cold initialization, but does not replace
hour/minute. This proves deterministic retained time/date, not elapsed
offline time or boundary-date arithmetic. Eight checker tests cover
missing/reordered inputs, missing time-register programming, cold replacement,
bad RTC data and midnight/day-consumption ordering.

`make verify-6210-calendar-midnight RUN_DIR=run_6210_midnight JOBS=4` enters
`23:59` and the same date physically, then waits for ordinary emulated time.
CCONT reaches `00:00:00`, day counter 1. Physical Back triggers firmware
reads of day register `0a=01` followed by its rebase to zero; Select then
reopens Calendar with `8 October 2026`, `Thursday`, hash
`a04551d523cb8e0adf4efe4da7e6bdcc6ec683dc264a8e791aed57ae798d4d01`.
A separate retained cold process reproduces the next-day pixels and
registered SIM location without new date entry. The already-open Calendar
remains on its selected date until navigation; this gate proves the
leave/reopen path, not automatic redraw of that window. No host-clock
advance, RTC register fixture or Gregorian date state is injected.

Three date-only variants reuse that same physical clock/day-consumption/cold
runner and the product's own flash journal:

| Gate | Physical date at 23:59 | Reopened and cold-retained Calendar |
| --- | --- | --- |
| `verify-6210-calendar-leap-day` | 28 February 2024, Wednesday | 29 February 2024, Thursday |
| `verify-6210-calendar-nonleap` | 28 February 2023, Tuesday | 1 March 2023, Wednesday |
| `verify-6210-calendar-year-end` | 31 December 2026, Thursday | 1 January 2027, Friday |

Each checks independently reviewed pixels both before and after midnight,
hardware day increment/firmware rebase, retained RTC hour/minute, own staged
verification and fresh/retained-location registration. The original and
next-day hashes are profile data in `run_noki6210_calendar.py`, not borrowed
from another handset. Non-leap February additionally passes in
`run_6210_calendar_nonleap_acceptance` after independent review of both date
frames. These cases do not establish century rules, every month boundary,
offline elapsed time, alarms or native speech.

## Physical alarm acceptance

`noki6210_alarm_input.lua` physically follows Settings -> Alarm clock
(Menu 4-1 in the [Nokia-authored user guide](https://manuals.plus/m/6bb77733be85dbf473e9cdb264eefd193dbc61e6ef4bd6382a5fef5d30ca7144.pdf)).
On retained storage created by this product's physical Calendar entry at
13:47, keypad `13:48` and OK program CCONT registers `0b/0c` to `30/0d`.
Natural RTC advance to 13:48 produces the firmware's `Alarm! 13:48` screen
with Stop/Snooze softkeys and PUP buzzer programming. Physical Stop returns
to `DCT3 LAB` idle and disables the buzzer. Own staged/self-test and
retained-location registration checks pass independently.

`verify-6210-alarm` creates that clock through a fresh independently checked
Calendar fixture, then runs alarm entry/expiry/Stop in a separate process
retaining only its own generated NVRAM. It requires ordered physical input,
CCONT programming, natural 13:48:00, alarm-cause read/acknowledgement, buzzer
enable/disable, all three reviewed 96x60 frames and retained-location
registration. Negative tests reject missing/reordered evidence, wrong time
or alarm cause, clock replacement and a buzzer left enabled after Stop.
`verify-6210-alarm-cold` adds an independent armed cold boot. Physical input
sets 13:48, then exits before expiry; a new process retains only that run's
NVRAM and supplies no clock/alarm entry. Startup reads CCONT `0b/0c=30/0d`,
the first natural tick is 13:47:01, and expiry at 13:48 produces the same
reviewed alarm frame. Physical Stop restores the reviewed operator frame
and disables the buzzer. The alarm capture is at 77 seconds in both gates;
an earlier 70-second capture differs and is not used to re-bank the oracle.
Cold restoration retains hour/minute with seconds starting at zero; no
offline elapsed time is claimed. Negative tests also reject missing retained
register reads, cold observation, clock restoration, or replacement alarm entry.
Neither gate establishes audible output, Snooze, powered-off wake or native
DSP behavior.

### Snooze delivery contract

`verify-6210-alarm-snooze` uses the same fresh physical Calendar seed and
alarm entry. Right Softkey / C at 77 seconds makes firmware program CCONT
`35/0d` (13:53): this ROM selects a five-minute interval. The gate requires
natural 13:53:00, status `b1`, acknowledgement `a1` (minute plus alarm),
renewed PUP buzzer programming and physical Stop. Exact frames cover the
original alarm, `Snooze active`, repeated `Alarm! 13:53` and final idle.
The repeated capture is at 361 seconds, in its independently observed visible
phase. Audible output and powered-off wake remain outside this acceptance.

Alarm title/time pixels are phase-dependent. The passive matched-phase run
`run_6210_snooze_render` observes a blank text region at 61..73 seconds and
visible `Alarm! 13:48` at 74..76; the repeated alarm shows `Alarm! 13:53`
at 361..373 and blanks at 374..376. Therefore a blank 377-second repeated
capture is not evidence of a missing repaint. No display state or rendering
behavior is changed to obtain the visible capture.

CCONT's held source must survive a MAD2 acknowledgement until firmware
clears it over GENSIO. NPE-3's mask write can expose the minute IRQ before
its register helper updates the RAM mask shadow. Passive reads at
`0x4f2766/0x4f2774` in `run_6210_snooze_reader_valid` establish mask/status
returns `30/31`; at `0x4f277a` the shadow at `0x1742fd` has become `10`, but
the already-sampled mask yields active `01`. With held-input retention,
firmware receives the source again, selects `21`, clears it and lowers the
line. Minute events then drain normally and the second alarm is delivered
(`run_6210_snooze_level`, `run_6210_snooze_acceptance`). The old rising-edge
bridge instead lost the second alarm behind the uncleared minute source
(`run_6210_snooze_probe`). This establishes a source-level integration
defect, not a CPU sleep or serial-latency defect; physical serial timing
remains unmeasured.

### Powered-off alarm boundary

The Nokia-authored user guide specifies alarm operation while switched off
and a Stop/activation choice. `verify-6210-alarm-off-no` and
`verify-6210-alarm-off-yes` physically set 13:48, shut down before expiry,
and verify autonomous RTC wake at the natural deadline. CCONT retains alarm
cause `80` and presents the held source after digital-domain reset. Firmware
acknowledges it, programs the buzzer, paints the reviewed alarm frame and
responds to physical Stop with "Switch the phone on?". No returns to rail-off
with silent powered endpoints and a blank frame. Yes requests an MCU reset
and returns to the exact registered DCT3 LAB idle frame. The gates verify
each own-upload phase independently, including normal self-test/analogue
acceptance after activation. This is research-HLE control/presentation
acceptance, not audible-output or native-DSP acceptance.

`verify-6210-alarm-off-restore` additionally saves the powered-off countdown
at emulated second 45 and compares all 37 exported ARM registers (including
CPSR and banked state), full SRAM checksum and emulated time after restoration.
Reference and restored windows span
45..46.25 seconds, deliberately avoiding an endpoint at the RTC callback's
timestamp. Both contain the same single RTC tick, blank reviewed frames,
no premature wake and no powered DSP/radio activity. Only the restored
timeline continues to the natural 13:48 deadline; held alarm delivery,
buzzer programming and physical Stop/No must still pass the ordinary
powered-off acceptance. The fixture changes emulator save-state only, not
firmware, MMIO, clock or alarm registers. Fresh physical clock-entry and
own provisioning seed the run (`run_6210_alarm_off_restore_quarter`).
`verify-6210-alarm-off-restore-yes` uses the same architecture/RTC replay
contract but physically chooses Yes. It independently verifies the subsequent
software-reset request, restored-countdown alarm boot and normal post-restart
upload/self-test phases, forbids a new rail drop after Yes, and requires the
unchanged exact registered idle frame (`run_6210_alarm_restore_yes`). This
closes both activation choices after a loaded countdown; neither substitutes
for native DSP or audible-output evidence.

Warm MCU reset preserves SRAM. NPE-3 startup reads the reason at `0x17fe48`
before clearing ordinary workspace `0x100020..0x175668`. Physical Yes stores
reason `0c`; it survives reset and is consumed by startup. Clearing all SRAM
in the board reset helper previously destroyed that firmware-owned reason.
Software reset publishes MAD2 status `04`, distinct from cold rail-on `01`:
readers `0x2b7a1a/0x4b0272` match the 3210 readers `0x230608/0x2a922e`.
Bit 1 set selects cold reason `6b`; with bit 1 clear, bit 2 selects the saved
software reason. The second reader separately tests cold-power bit 0 before
software bit 2. Consequently `05` selects conflicting startup lifecycles;
`07` is also wrong, not an internal-reset fix (`BHS` takes carry set).

Acceptance evidence: `run_6210_alarm_yes_rearmed` and
`run_6210_alarm_off_no_warm_regression`. Debugger-only log caps are re-armed
before Yes so the second boot is observed, without changing firmware/MMIO.
Do not substitute a power key, watchdog reset or injected event for alarm
wake, or infer missing firmware work from exhausted trace caps. Exact silicon
reset timing/encoding and off-process elapsed time remain unmeasured.

## Unattached accessory input

The NPE-3 schematic sheets 2/3 connect `HEADDET` to CCONT's EAD pin A2
and the microphone bias network. With no external microphone fitted, that
network provides a pull-up path. The sheet names pins, not ADC selector
numbers; the numbered contract below comes from the product's firmware.

Reader `46b3d6` selects ADC input 0 via `504100`. Consumer
`39d678/39d686/39d692` stores the result and evaluates accessory state byte
`173a97`. Initialization `46b982` writes `0f`. At a constant zero input,
`39ccc8` advances that state to `10`; `39d784` calls UI selector `41a944`
with argument 1, and the idle frame displays `Headset`. A nominal high input
`03ff` leaves the state at `0f` and the reviewed idle frame has no accessory
label. This controlled input/state/frame correspondence supports the
product-local unattached default; it is not measured voltage calibration.

`verify-6210-accessory` requires a real decision at `39d6b2`, high sample
and state `0f`, the reviewed registered unattached idle frame, and physical
Menu decoding with the reviewed Messages frame. It does not inject firmware
state, change the PMM, or synthesize an accessory event. The idle oracle was
deliberately re-banked to remove the false attachment indication; menu and
application oracles remain unchanged. All 15 product scenarios, including
call/SMS save-load continuation, pass with this profile.

Two independently fresh accessory scenarios also reproduce identical selected
protocol traces, CCONT/flash/SIM persisted bytes and registered-idle/Menu pixels.
`make verify-6210-cold-repeat RUN_DIR=RUN` creates both fresh scenarios
sequentially under `RUN/first` and `RUN/second` and requires their comparison.
Compare accepted runs with
`tools/noki6210_cold_repeat_check.py FIRST_RUN SECOND_RUN` using the project
Python environment. The checker rejects a shared directory, unaccepted
scenario, missing/empty artifacts and differing fingerprints; PNG metadata
is excluded by hashing grayscale pixels. This establishes repeatability of
the measured composition on this build, not cross-host determinism or native
DSP behavior. Generate each input with `run_noki6210_acceptance.py --scenario
accessory` in a distinct directory; copying a completed run is not a cold-boot
experiment.

Do not import the selector-7/EAD interpretation from the NSE-8 documentation:
raising NPE-3 selector 7 does not remove `Headset`. The ADC audit observed
179 paired reads through 19 caller addresses over a 24-second old-profile
boot; selectors 1/6 were not observed, not proved unused. Static direct-BL
scanning found 39 ADC-reader candidates and excludes indirect calls. Full
accessory identification, button/hook behavior, electrical thresholds and
cross-product mux identity remain outside this unattached-input contract.

Registration verification permits SIM EF_LOCI writes after Location Updating
Accept either before or after radio release. The network and SIM consumers
are independently scheduled; each ordered chain and final persistent state
is required, not the accidental interleaving of a prior trace.

## Missing native bootstrap observation

No matching raw NPE-3 final publication or ROM6 mask image was identified in
the acquired collection or checked-out reference implementation. The latter
answers the parked shared-offset-2 read with zero; its later self-test
responder does not establish the earlier verifier's result. The separate
bridge default `1eff` is described as an 8210 value without a matching raw
NPE-3 capture, and must not be transplanted.

The smallest input that can justify an HLE continuation is an unmodified
NPE-3 v5.56 boot capture identifying the board/DSP revision and firmware
hash, and recording shared halfwords at MCU `10000` and `10002` around
the final (232nd) buffer acknowledgement. Include `10004/10006` and
the ownership words `100fe/10100` to distinguish phase/order, with raw
capture bytes and a hash. Read substitutions, boot patches and bridge-HLE
values must be disabled and declared. A standby snapshot alone can contain
later reused mailbox data, so it is not equivalent to this boundary capture.

Alternatively, core execution needs ROM6 PROM content/mapping at program
`ff87` under PMST `ffa8`, plus the COBBA register-F/status-D contract.
The stock MCU flash and PMM do not provide validated copies of those hardware inputs.
The same staged code on another product is a comparison, not a substitute.
