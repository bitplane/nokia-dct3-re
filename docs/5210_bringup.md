# Nokia 5210 NSM-5 profile

## Validated result

The local v5.40 PPM E Wintesla set reaches a stable, registered and interactive
standby screen through the normal firmware lifecycle. `make
verify-5210-frontier` checks the unattended firmware-rendered operator frame
against an exact stable-pixel oracle. The separate menu, Messages and
navigation gates use only physical matrix keys to open the main menu, enter the
Messages application, and return through both Back levels to registered
standby. No firmware RAM, message, rendered pixel, or completion flag is
injected.

The archive members, normalization command, and image hashes are recorded in
`roms/README.md`. The shared MAD2 inputs remain documented placeholders; this
profile does not establish NSM-5-specific internal-ROM identity.

## Product contracts

NSM-5 differs from the other validated products at explicit device boundaries:

- GENSIO control `0x22` selects CCONT without the NSE-8 control-bit
  convention. Receive-ready follows a completed command byte.
- Firmware organically issues DSP bootstrap command 4 with pending value 2
  and later type `0x70` payload `0d00`. The product enables the existing
  request-derived service completion and compact type-`0x74` completion only.
  A read-only run observed upload latch `0x13b528 == 1`, expected and actual
  self-test values both `0x9c49`, and the verdict retaining healthy bit 6.
- The external-service transport answers organic D0/01 discovery. NSM-5
  accepts class-`0x40` registration sequence `0x42` and channel-map sequence
  `0x43`, then organically exercises channel `0x5f`. Channel `0x62` remains in
  the accepted map but has not been exercised independently by this gate.
- The SIMI block and removable synthetic GSM card use the standard controller
  boundary. Card contents remain external input.
- The CCONT inputs represent a BLB-2 pack. Nominal BSI `0x150` and BTEMP
  `0x140` lie inside recovered windows `0x13e..0x172` and `0x133..0x157`;
  VBATT is `0x2c0`. The generic tuple was outside those windows and selected
  the fault screen even after the DSP self-test had passed.
- The 84-by-48 display uses reversed segment order. This is a product display
  property, not a capture-only mirror.
- The five-row NSM Family-A keypad uses operational power-column mask `0x10`
  and a dedicated MAME
  matrix containing softkeys, scroll, call, end, volume, digits, star and hash.
- NSM-5 emits the ROM6 type-`0x56`, 160-byte candidate-window request. Its
  virgin PMM supplies ARFCN 86 (`0x56`), which the product-owned direct-octet
  radio contract carries through candidate acquisition, random access and the
  assigned-channel confirmation. Firmware then emits its own Location Updating
  Request, accepts the laboratory network, updates EF_LOCI, releases SDCCH and
  returns to the serving paging channel. The idle UI renders `DCT3 LAB`.

## Acceptance gates

- `make verify-5210-radio-registration` proves candidate selection, ordinary
  Location Updating, both EF_LOCI writes, channel release, steady camp and the
  exact registered operator frame.
- `make verify-5210-radio-authentication` repeats registration with network
  authentication enabled. The phone issues RUN GSM ALGORITHM to the removable
  SIM, fetches `SRES || Kc`, emits MM Authentication Response, then completes
  the same registration lifecycle. Internal PC-keyed consumer taps remain
  product-local; NSM-5 is checked at the SIM and GSM protocol boundaries.
- `make verify-5210-radio-a5-1-incoming-call` extends authenticated service
  through a network Cipher Mode Command, redacted product-local DSP key
  publication, organic Cipher Mode Complete, bidirectional ciphered SDCCH and
  traffic bursts, and the complete physical Answer/End lifecycle. The default
  regression composition remains A5/0.
- `make verify-5210-save-state` saves registered standby, restores it, and uses
  physical Menu/Enter keys to reach the exact Messages frame. This protects the
  radio, SIM, UI and product-device state across serialization.
- `make verify-5210-radio-call-state` saves and restores while an answered call
  is active, resumes the restored branch, then uses physical End to complete
  CC/RR and speech-control teardown and return to exact registered standby.
- `make verify-5210-power-lifecycle` proves the corrected physical power-column
  contract: a short press opens the firmware-rendered profile/power menu, while
  a sustained press is repeatedly scanned by firmware and ends at CCONT digital-baseband
  rail-off. The earlier `0x02` assignment produced IRQ edges but no lifecycle
  transition and is retired.
- `make verify-5210-charger-wake` continues from organic rail-off: a physical
  charger edge restores the CCONT-powered baseband with cause `0x04`, firmware
  reads that cause and samples charger-present VCHAR. Product-local task RAM is
  deliberately not interpreted through the 3210 summary addresses.
- `make verify-5210-sim-phonebook` opens the product's `Names` menu with its
  right softkey, saves `Ada`/`123` through the firmware's editor and absolute
  `EF_ADN` update, then cold-boots with the same card image and renders the
  stored contact through Search. This proves a non-SMS application write and
  removable-card persistence without importing the 3210 menu route.
- `make verify-5210-pin-phone-memory` separately selects Phone through
  Names/Settings/Memory in use, checks the reviewed empty-memory result,
  then physically saves `Ada`/`123`. An independent cold process inherits
  the own flash/storage files, authenticates and registers, and renders
  both the name list and number Details without reselecting Phone.
  Exact selector/editor/Saved/contact/idle pixels and the stable Messages
  title are required. Both complete SIMs remain the registered PIN-enabled
  laboratory profile, with no UPDATE RECORD or `EF_ADN` write. Every source
  NVRAM file's hash must survive the cold child unchanged; the acceptance
  inventory is `phone_memory_storage.json`. This exercises phone-local
  Add/Save/Search/Details, not edit/erase/capacity or arbitrary flash records.
  The route agrees with [Nokia's 5210 user guide](https://www.manualslib.com/manual/505197/Nokia-5210.html?page=22#manual);
  capacities stated by that guide are not inferred as tested capacity here.
- `make verify-5210-pin-phone-memory-edit` retains that fresh empty/save/read
  chain, then cold-opens `Ada/123`, selects Options/Edit, confirms the unchanged
  name and physically replaces the last number digit with4. Edit starts with
  the name even when selected from number Details. Reviewed editor/Saved
  pixels precede another cold process rendering `Ada/124`. All four processes
  authenticate/register and preserve complete expected SIM contents; source
  NVRAM hashes are guarded for every preserved-storage run. The original
  save/read gate still expects123. This proves the selected number edit,
  not name editing, erase/cancel, capacity or power-failure atomicity.
- `make verify-5210-pin-phone-memory-erase` uses six independent processes:
  fresh Save/Read, Back from `Erase? Ada` followed by retained `Ada/123`,
  then OK and a cold empty search. Both confirmation choices have reviewed
  result frames. The empty reader first inspects the retained Phone selector
  without changing it: the generic empty notice alone cannot distinguish an
  empty SIM from empty Phone storage. All stages retain exact full SIM
  contents and reject UPDATE RECORD/`EF_ADN` writes; predecessor storage is
  hash-guarded. Cold follow-ups are explicit scenario metadata with terminal,
  storage-dependent readers. This is a single-contact seed, not proof of
  selective multi-contact preservation, capacity or failure atomicity.
- `tools/run_noki5210_acceptance.py DIRECTORY --scenario pin-phone-memory-rename
  --preserved-nvram SAVED_RUN/nvram` extends a firmware-created `Ada/123`
  Phone-bank fixture with physical name replacement and automatic cold read.
  The editor clears Ada, enters A, confirms the retained number123 and explicitly
  chooses **Replace**, not Add. Reviewed replacement/Saved frames precede a
  distinct cold **A** list frame and unchanged123 Details. Details alone cannot
  prove the rename because that screen contains no name. Both processes retain
  the complete expected SIM, reject SIM updates and hash-guard their predecessor
  NVRAM. Capture: `tmp/nsm5-phone-rename-qualified`. This qualifies a single-letter
  replacement, not arbitrary encoding, multiple-contact selection or capacity.
- `make verify-5210-pin-phone-memory-multiple` starts with a fresh physical
  empty/Save/cold-read chain, then edits Ada to A but explicitly selects
  **Add**, not Replace. An independent cold process separately selects A and
  Ada and reads123 from each Details screen. Both distinct name-list frames
  are required because number Details alone cannot distinguish equal numbers.
  It then physically confirms `Erase? A`; a sixth independent process
  cold-retrieves the original Ada/123. The selective result has its own
  reviewed `Erased` frame rather than borrowing the last-contact erase oracle.
  All six processes authenticate/register, preserve the complete expected
  SIM and reject SIM record writes. Every predecessor NVRAM inventory remains
  unchanged. This proves two-contact coexistence, cold retrieval and
  non-selected-contact preservation after confirmed erasure, not capacity,
  arbitrary record enumeration or interrupted-write atomicity.
  Accepted capture: `tmp/nsm5-phone-selective-qualified`; the gate also preserves
  the original single-contact Save/Read oracles without rebanking them.
- `make verify-5210-radio-supplementary` enters `*123#` and `*#21#` through
  the physical keypad and submits them with the product's dedicated Send key.
  The resulting processUnstructuredSS-Request and InterrogateSS transactions
  complete through GSM 04.80, render the network response, and release RR.
- `make verify-sim-toolkit-5210` supplies a Phase-2+ card, observes the
  product's nine-byte TERMINAL PROFILE, then requires `91xx`, FETCH, rendered
  `DCT3 SAT`, physical OK and a successful TERMINAL RESPONSE. The profile
  length is product evidence rather than a borrowed 3210 constant.
- `make verify-5210-sim-pin-registration` uses a PIN-enabled laboratory card
  in BIOS-local `noki5210_3` storage, physical `1234` and OK, one successful
  VERIFY before own laboratory registration, reviewed masked-code/idle/menu
  frames and physical End recovery. Entry starts at 7 s after SIM startup;
  this does not establish arbitrary user-delay recovery. A separately observed
  12-s start with slower keys accepts PIN at 20.018 s and remains interactive,
  but never requests Location Updating during the 45-s window. Late SIM
  readiness fails with the default ARFCN 1/2 topology; the coherent 86/87
  topology independently completes delayed-PIN registration as described below.
- `verify-5210-radio-measurement-static` pins the own MCU hash and RX table:
  `3de7a0[8] -> 3de7f4 -> 37019c`; the selected forwarding branch posts to
  task 12. Completion `2e336c -> 2bd782` consumes forty four-byte records,
  with big-endian carrier at message offsets 6/7 and signed RSSI at 9.
  These are MCU receive semantics, not measured DSP or RF behavior.
- `verify-5210-late-pin-boundary` repeats delayed physical PIN entry for
  45 s and passively proves `57:03050000 -> 8b/166 -> 37019c -> 2e336c ->
  2bd782` before successful VERIFY. The peer now answers that organic
  request without interrupting its serving receiver. The parser runs at
  10.209 s; the later accepted PIN still produces no random access during
  the bounded run. This negative gate must fail if radio progress changes,
  and does not promote late-PIN registration. Recovery is therefore beyond
  delivery of this one measurement completion; do not repeat it or delay
  channel acknowledgements solely to make registration pass.

## Delayed-PIN request ownership

### Validated coherent-network result

`make verify-5210-late-pin-registration` starts physical PIN entry at 12 s
with 1.85-s inter-key spacing, using the acquired PMM's ARFCN 86 and a matching
86/87 laboratory topology. The background response reports serving carrier
`0056` at -60 dBm. Firmware accepts PIN, completes Location Updating, persists
EF_LOCI, releases the dedicated channel and returns to steady camp. Both
request roots clear. Reviewed entered-code, registered idle, Messages menu and
physical End recovery frames are checked independently. The PIN-enabled
coherent run emits release header `041202`; the original no-PIN/early-PIN gate
retains its separate `040000` expectation. Neither checker accepts both.

`make verify-5210-pin-incoming-call` composes this delayed physical PIN and
coherent topology with laboratory paging, caller `5551234`, physical Send/End,
Connect/Disconnect, CC/RR closure, speech-control start/stop and exact registered
idle recovery. The ringing frame and masked PIN frame are independently pinned.
The PIN-enabled traffic request is
`041202000271012fc10000560000000400000000`; release is
`041202001117001a600000560000001400000001`. The original call profile retains
its different headers. This is research-HLE signaling and speech-control
coverage, not native DSP execution, intelligible speech or a host-originated call.

`make verify-5210-pin-incoming-sms` independently composes the same delayed
physical PIN and coherent topology with laboratory paging, segmented SAPI-3
SMS-DELIVER, handset CP-ACK/RP-ACK, network CP-ACK and service RR release.
Physical Read opens `hello`; the exact message-body frame matches the existing
no-PIN 5210 oracle. The saved card must contain the same SMS-DELIVER with read
status, and the trace must contain exactly two EF_SMS writes (delivery and
read-status update). This is durable card storage and firmware UI interaction,
not a network-side delivery assertion. It does not establish PIN-enabled
outgoing messaging, save-state replay or host SMS composition.

`make verify-5210-pin-idle-state` separately saves coherent, PIN-authenticated
registered idle and restores exact emulated time, PC/SP and the checksum over
the modeled MCU RAM window. Reference and restored idle frames must be
byte-identical and match the registered-idle oracle. Physical post-load Menu
opens the reviewed Messages frame; End returns to the same idle oracle. This
does not by itself establish active-call or delivered-SMS restoration.

`make verify-5210-pin-call-state` separately answers the laboratory incoming
call after delayed physical PIN, saves while connected and restores exact
time/PC/SP/RAM and reference pixels. The call checker requires the round trip
after Connect Acknowledge and before physical End/Disconnect; the restored
branch completes CC/RR release and returns to the exact registered-idle frame.
Idle and call use the same retained-notifier fixture core. This is HLE
signaling/speech-control state coverage, not proof of native DSP audio or host
media restoration.

`make verify-5210-pin-sms-state` separately saves after incoming SMS delivery
and CP/RP/RR closure, before physical Read. It restores exact time/PC/SP/RAM
and identical notification pixels, then physically opens the stored message.
The body matches the existing `hello` frame and the final card retains read
status; exactly two EF_SMS writes remain required. The checker places save/load
between transport closure and the Read inputs, so saving an already-read
message cannot substitute for this test. PIN-enabled idle, active-call and
delivered-SMS restoration are independent gates on the same fixture core.

`make verify-5210-pin-outgoing-call` physically dials `5551234` after delayed
PIN and coherent registration, then verifies exact called-party SETUP digits,
CM Service, Call Proceeding, traffic assignment, remote Alerting/Connect,
handset Connect Acknowledge, physical End, CC/RR release and exact idle
recovery. Its traffic/release headers are explicitly the coherent PIN variant;
the original outgoing gate continues to require its own different headers.
The default laboratory session supplies the remote party. This does not prove
PIN-enabled host/SIP composition or native speech.

`make verify-5210-pin-host-outgoing-call` independently connects the physical
`5551234` dialling route to the loopback host adapter. The host requires the
correct digits and transport epoch; wrong-id and duplicate decisions are
rejected, exactly one connect decision is queued/accepted, and connected/ended
states surround handset-owned physical End and full CC/RR release. Registered
idle/recovery pixels remain pinned. This proves outgoing host call signaling,
not SIP, external call audio, native speech or external-dialog restoration.

`make verify-5210-pin-host-incoming-call` waits for the physical PIN fixture's
registered-idle snapshot before queuing a host call from `5551234`. Automatic
laboratory incoming calls are disabled. Host queued/paging/alerting/connected/
ended phases and registered ARFCN 86 are correlated to request 1, epoch 1;
physical Answer/End separately completes own CC/RR and speech-control release.
Pre-call idle, the reviewed caller screen and recovered idle are exact pixel
oracles. The host caller frame has its own capture-phase oracle rather than
relaxing the laboratory-call hash. This proves incoming host signaling and
physical UI interaction, not speech, SIP or external-dialog restoration.

`make verify-5210-pin-outgoing-call-state` separately restores the physically
originated call after Connect Acknowledge and before physical End. Time,
PC, SP, full modeled RAM and connected reference/restored pixels must match;
the restored call then completes CC/RR release and returns to the exact idle
oracle. The gate also requires the physical dialled digits and coherent PIN
channel headers. This covers handset-originated HLE call restoration, not
an external SIP dialog or native speech.

`make verify-5210-pin-outgoing-sms` physically enters Messages after delayed
PIN, composes the predictive-text character `a`, selects Send and enters
`5551234`. It requires exactly one complete SMS-SUBMIT
`390118000100069121436587090d11010781551532f40000a70161`, decoded GSM-7 length
one, network CP-ACK/RP-ACK, handset final CP-ACK and service RR release.
Reviewed composer and `Message sent` frames are exact pixel oracles. The
transaction uses the coherent laboratory network; no editor contents or
message bytes are injected. This does not establish PIN-enabled host SMS
submission or outgoing-SMS transaction save-state replay by itself.

`make verify-5210-pin-host-outgoing-sms` independently repeats that physical
composer through the loopback host adapter. The host requires the submitted
GSM-7 `61` character, sends a correlated accept decision and observes the
completed transaction; the firmware checker separately requires exact
SMS-SUBMIT, CP/RP/RR closure and the same reviewed `Message sent` pixels.
The trace checker requires host request/decision/result ownership. This proves
software-host SMS submission, not external SMS delivery, rejection/timeout
parity, SIP composition or native DSP execution.

`make verify-5210-pin-host-incoming-sms` separately enables only the host
adapter (no automatic laboratory SMS), waits for PIN-authenticated network
registration and sends `hello` through the correlated host request. Paging,
SAPI-3 CP/RP closure and host queued/delivered states must precede physical
Read. The final card retains the message with read status, exactly two EF_SMS
writes occur, and the body frame matches the existing oracle. The host trace
requires registered ARFCN 86; another product's default carrier cannot satisfy
this check. This proves incoming host SMS/UI/storage, not external-network
delivery or native DSP execution.

`make verify-5210-pin-host-rejected-sms` separately sends the same physically
composed SMS and supplies a correlated host RP-error decision. It requires
RP-ERROR, final handset CP-ACK, RR release and absence of success RP-ACK.
The reviewed `Message not sent, try again later` frame is pinned separately
from successful submission. Physical End returns to the exact idle oracle;
Menu opens the reviewed Messages frame. No retry or timeout coverage is
inferred from this explicit rejection.

`make verify-5210-pin-host-silent-sms` supplies one network CP-ACK but no
RP-ACK/RP-ERROR. It requires the firmware-owned main-link DISC in the observed
60..80-second interval after submission (about 67 seconds in the verified
run), peer UA, channel release and host transaction termination. The reviewed
`Message sending failed` pixels are distinct from explicit RP rejection;
physical End/Menu must still recover the exact idle and Messages frames.
No timeout, release or application state is injected. Retry coverage is not
inferred; local RP-wait restoration has its own gate below.

`make verify-5210-pin-sms-wait-state` independently uses the local laboratory
RP-silence setting, saves after network CP-ACK and restores before firmware
DISC. Exact time/PC/SP/full modeled RAM and reference/restored pixels must
match. The restored handset then times out, receives peer UA, releases the
channel, resumes paging and physically recovers the exact idle/menu frames;
the reviewed timeout pixels are also required. Local submission outcome 3
is checked separately from host-deferred outcome 0. This validates one
restored RP-wait continuation, not a reference/replay comparison of the
whole timeout interval or restoration of an external host/SIP transaction.

The former delayed-PIN failure is a mismatched external network fixture, not
proof of an unavailable firmware continuation or missing DSP acknowledgement.
The retained `verify-5210-late-pin-boundary` deliberately keeps ARFCN 1/2 as a
negative control; all observations below describe that mismatched topology.
No firmware state, message or callback is forced, and no DSP reply is replayed.

### Mismatched-topology control

Paired passive RAM observations distinguish successful PIN verification from
radio-request promotion. The own v5.40E root is `0x13e78c`: selector state is
the halfword at `+2`, current request is `+8`, queued request is `+12`, previous
request is `+16`, and context is `+20`. The request's flag is at object offset
12; another product's offset must not be imported.

With early physical PIN entry, firmware queues a request at 9.398 s and
promotes it at 10.211 s through `0x2e24de -> 0x2e1df4`; registration follows
and the request roots clear. With delayed entry, measurement completion puts
the selector in state 6 at 10.210 s, before PIN verification. Firmware queues
the post-PIN request at 21.025 s, but it remains queued alongside the original
current request through the 45-s observation. Both captures stay below the
80-write observation cap (44 early, 28 late).

The state-6 selector case requires argument zero to reach the common promotion
path, additionally gated by byte `0x13c52e`. The observed post-PIN caller
`0x232064` supplies argument 3 and preserves state 6 instead. A full-image,
aligned old-Thumb direct-BL census finds eight calls to selector `0x2e23ac`,
with all eight adjacent literal arguments recovered. Its three argument-zero
sites are `0x234f3e`, `0x2355d6`, and `0x2358a0`. This does not close indirect
or tail entries and is not a proof that an ordinary producer is absent.

Selector-entry observations establish that the eligibility byte is nonzero
(`5`) in both runs. The early control has seven entries: its post-PIN
argument-3 call is followed by argument zero from `0x234f3e` at 10.211 s,
with the new request already queued. The delayed control has six entries:
argument-zero calls from that same site occur at 10.208 and 10.209 s, before
PIN acceptance, and its only post-PIN entry is argument 3. Thus neither a
missing eligibility flag nor absence of the argument-zero caller explains
this negative control. The coherent-network run supplies the missing physical
carrier consistency without changing selector behavior. The negative gate checks
retained distinct request pointers, the eligible state-6 post-PIN entry, and
complete selector coverage below the observation cap.

Passive caller-context captures identify the message-side difference. At
`0x234f3e`, the successful early run receives internal message `0x1802` with
type byte `0x89` while the post-PIN request is queued. The delayed run reaches
the same caller with `0x1802`/`0x89`, then `0x1802`/`0x8b`, before PIN
acceptance. Its post-PIN argument-3 call receives `0x03ec`/`0x83` instead.
These headers are read from the live caller context, not inferred from nearby
transport timestamps. The ROM-pinned contract also checks the explicit
`0x1802`/`0x89` branch to the common argument-zero call; runtime proves the
`0x8b` case reaches that call too, without claiming it enters that same branch.
The captured object headers differ (`0x03eb` for the initial current request,
`0x03ec` for the queued request). ROM function `0x2e2000` consumes the current
request's signed halfword and subtracts literal base `0x03ea`: the four exact
branches are `03ea -> 2e20e2`, `03eb -> 2e20dc`, `03ec -> 2e202e`, and
`03ed -> 2e20e6`. The initial variant calls `0x2e1f5e`; the post-PIN variant
instead enters the previous-request comparison and configuration path. The
static gate checks this entire discriminator cascade, not an inferred name
for these requests. Their complete lifecycle semantics remain open.

The retained request in this control does not establish a missing DSP reply.
A second channel-change or measurement reply is not justified by its ordering.
The initial continuation `0x2e1f5e` is observed once in the delayed run at
10.209223 s, after background-result parsing and before PIN acceptance; the
successful early control never enters it. It passes value 5 to `0x2bd3d8`,
whose recovered operation replaces record byte 13's upper three bits while
preserving its lower five bits, then calls `0x2bd424` and marks context byte 4.
This is a configuration/record update, not proof of request retirement or a
new completion message. The late boundary gate requires those ordered entry
observations; the static contract checks the setter and continuation tail.
The post-measurement type-`0x11` mode-4 instruction in the negative control is
currently logged as unaccepted by the neighbour decoder; this is a separate
candidate contract mismatch, not evidence that fabricating its response would
promote the queued request.

Mode-4 DSP semantics remain independently unresolved, but are not required to
claim the coherent-topology registration result. Do not force selector
arguments, promote pointers externally, or replay measurement completions.
`run_noki5210_acceptance.py --scenario pin-registration --observe-readiness`
provides the successful physical-input control; `--scenario late-pin-boundary`
provides the delayed negative control. Both use passive root observations.

## Additional acceptance gates

- `make verify-sim-toolkit-interactive-5210` independently accepts GET INKEY
  `5` and GET INPUT `42`, with exact successful terminal responses and five
  reviewed prompt/entered/registered-idle frames. NSM-5 requires physical OK
  after the INKEY digit; submitting the digit alone leaves the reply pending.
  The gate uses fresh private storage and own v5.40E MCU/PMM, not firmware
  state injection. Card-menu and network-triggered Toolkit services are not
  established by this text-reply gate.
- `make verify-5210-radio-paging` proves one IMSI-addressed page, random access,
  Paging Response, contention resolution, clean release and return to the
  subscriber's paging group.
- `make verify-5210-radio-paging-negatives` proves the registered handset
  ignores its own IMSI on the wrong DRX frame, another subscriber's valid
  identity, and a malformed identity LV. Each composition continues PCH fill
  without RR access, Paging Response, or EF_LOCI mutation.
- `make verify-5210-radio-incoming-sms` carries an ordinary `hello` SMS through
  paging, SAPI 3, segmented CP-DATA and SIM EF_SMS storage, then pins the
  firmware's `1 message received` frame. The NSM-5 DSP cipher-control
  publication begins `00eb`, independently of the NSE-8 `00f4` encoding.
- `make verify-5210-radio-incoming-sms-read` uses physical softkey presses to
  open sender `5551234`, display `hello`, and make firmware change the EF_SMS
  record status from unread `0x03` to read `0x01`.
- `make verify-5210-radio-incoming-call-ringing` pins the firmware-rendered
  caller `5551234` and ringing UI after paging, CC SETUP and traffic assignment.
- `make verify-5210-radio-incoming-call-answered` proves physical Send emits
  CC Connect, accepts Connect Acknowledge, displays `Call 1`, and publishes the
  independently observed NSM-5 DSP wire value `0x860b` as speech control
  `0x060b`.
- `make verify-5210-radio-incoming-call-lifecycle` adds physical End, CC/RR
  teardown, ARFCN-86 channel release, DSP wire `0x840a`/control `0x040a`, and
  exact return to registered standby.
- `make verify-5210-radio-media-resilience` applies the independent NSM-5
  System Module PCM contract: COBBA-GJP derives 1.000 MHz `PCMDClk` from
  13 MHz / 13 and 8.0 kHz `PCMSClk` by /125, carrying a sign-extended 13-bit
  sample in a 16-bit word. The coherent gate proves bidirectional GSM-FR,
  non-silent downlink, FACCH stealing, degraded-frame concealment, SACCH
  coexistence, exact active-call save/load replay and physical End teardown.
  `make verify-5210-radio-a5-1-degraded-state` separately combines organic
  authentication/ciphering with bidirectional burst impairment and a save at
  23 seconds. Its two-second restored interval reproduces 41 digital speech
  records exactly; physical End returns to PCH after 500 microphone and 470
  earpiece frames. The own NSM-5 lifecycle, concealment/recovery, FACCH and
  SACCH checks remain mandatory. This is HLE codec/protocol restoration,
  not native DSP execution or restoration of an external SIP dialog.
  Nokia's matching troubleshooting guide traces the internal microphone
  components `R268/C274/C263/C278/C262` to COBBA MIC2P/MIC2N and the receiver
  components `C292/C291/L272/L271` to EARP/EARN. Consequently,
  `make verify-5210-radio-physical-duplex` can require isolated host capture
  and playback through those exact routes: all 250 microphone blocks reach the
  network non-silently and the captured receiver output contains a sustained
  1 kHz downlink. `make verify-5210-radio-outgoing-physical-duplex` repeats
  that proof after physical number entry and Send, so neither call direction
  is inferred from the other. Analogue gains remain neutral rather than
  guessed.

The troubleshooting test-point table labels `PCMDCLK` as 512 kHz and reverses
its documented direction, conflicting with both the system-module divider
description and circuit diagram. The profile follows the explicit COBBA-GJP
13 MHz / 13 and /125 architecture; the conflicting test-point label is not
silently combined with it.
- `make verify-5210-radio-outgoing-call-lifecycle` physically enters
  `5551234`, starts CM Service and emits the firmware's called-party SETUP. It
  then proves Call Proceeding, one traffic assignment, Alerting, Connect,
  physical End, CC/RR teardown, the NSM-5 `0x860b`/`0x840a` speech-control
  pair, and exact return to registered standby. The media-resilience gate owns
  the separate digital PCM claim.
- `make verify-5210-radio-outgoing-sms` uses only physical keys to enter
  Messages, compose the predictive-text character `a`, select Send, and enter
  destination `5551234`. The gate requires the firmware's CM Service Request,
  SAPI-3 establishment and SMS-SUBMIT, network CP/RP acknowledgements, handset
  final CP-ACK, clean channel release, and the firmware-rendered `Message sent`
  acknowledgement.
- `make verify-5210-radio-reselection-same-lac` and
  `verify-5210-radio-reselection-different-lac` exercise the NSM-5 direct-octet
  neighbour contract on an evidenced GSM-900 pair (ARFCN 86/87, BSIC 0).
  Firmware either resumes PCH without subscriber mutation or performs a fresh
  Location Update and EF_LOCI write when the LAC changes.
- `make verify-5210-radio-loss-recovery` proves standards-counter serving-cell
  loss, the product's padded `03 05 00 00` SCH request, synchronization after
  RF recovery, and resumed PCH without a spurious Location Update.
- `make verify-5210-radio-all-cell-loss` proves the finite NSM-5 synchronization
  search reports no usable cell and stops PCH without falsely selecting a cell
  or mutating EF_LOCI.
- `make verify-5210-radio-reselection-paging` proves an IMSI page can still be
  answered and released after same-LAC reselection.

## Remaining scope

`make verify-5210-pin-phonebook` physically saves `Ada/123` after delayed PIN
and coherent registration, verifies the exact record-1 UPDATE and untouched
remaining 49 ADN records, then starts a separate cold process with a private
copy of only that run's NVRAM. The cold process re-enters PIN, uses physical
Names/Search/Detail and renders the reviewed `Phone number: 123` frame.
Stored `Ada/123`, enabled CHV state and registered idle pixels remain required;
the cold read must not issue an ADN update. Registration uses the explicit
preserved-location contract rather than requiring a redundant fresh status
write. No card record, editor content or handset state is injected.

The gates validate v5.40 PPM E, board bring-up, authenticated and
unauthenticated registration, operator presentation, save/load, the main menu,
application entry, clean return navigation, idle mobility, RF loss/recovery,
and paging after reselection. They do not yet validate call media/audio,
handset-local application writes, or another 5210 firmware revision. The unexercised external-service
channel and placeholder MAD2 inputs must not be promoted to cross-product
facts.
