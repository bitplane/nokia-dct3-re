# DCT3 model coverage

This matrix records demonstrated product coverage, not family resemblance. A
cell is promoted only by a named reproducible gate or reviewed hardware or
firmware evidence. `Partial` means that the preceding acceptance level works
but a material hardware contract remains calibrated, opaque, or unverified.

| Product / tested firmware | Booting | Interactive | Registered | Call control | Internal media | Physical duplex | Hardware-faithful | Principal evidence or next boundary |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Nokia 3210 NSE-8 v6.00 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Radio lifecycle, paired GSM-FR/FACCH/degraded-media and isolated physical audio gates pass. The cross-model type-`1f` gate independently finds four NSE-8 constructors for the shared status-2, four-byte task-3 envelope; its flag/value policy remains product-owned. Real COBBA DSP-controlled mux/gain semantics remain opaque. |
| Nokia 3210 NSE-8 v5.01 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Independent ROM gates cover the call lifecycle and isolated physical duplex. The same COBBA/DSP limitation applies. |
| Nokia 3310 NHM-5 v6.39 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | `verify-dsp-bootstrap-3310`, `verify-3310-frontier`, `verify-3310-navigation`, `verify-3310-radio-registration`, `verify-3310-radio-paging`, `verify-3310-radio-incoming-call-boundary`, `verify-3310-radio-incoming-call-ui`, `verify-3310-radio-incoming-call-lifecycle`, `verify-3310-radio-media-resilience`, `verify-3310-radio-physical-duplex`, and `verify-dct3-type-1f-static`. Its sole type-`1f` constructor is a parameterized four-bit builder for the shared status-2, four-byte task-3 envelope, rather than inherited NSE-8 policy. The frontier/navigation fixtures now resolve BIOS `639` to MAME namespace `noki3310_3`, reconstruct mutable flash/EEPROM/SIM state for every non-persistent run, reproduce clean idle hash `5871dd93…` in independent directories and wait for the firmware redraw before asserting the exact return-to-idle frame. Its typed `0x56` candidate search selects and camps on ARFCN `0x0058`; SI1--SI4, Random Access and Immediate Assignment remain firmware-owned. A correctly addressed page organically produces RACH and Paging Response. On the dedicated link v6.39 accepts Cipher Mode Command, publishes its distinct DSP type-`0x14` body and emits Cipher Mode Complete. Product-specific pacing after acknowledged MM Information lets its firmware-owned time-update transaction finish before SETUP. Continuing the assigned-channel receive schedule after the SETUP acknowledgement lets its queued LAPDm messages emerge organically as Call Confirmed and Alerting. Its TCH/F channel-change context `0x0402/00/01` independently requires confirmation bit zero, unlike the assigned-SDCCH context `0x0402/01/01`; satisfying that typed contract produces the new-main-link SABM and Assignment Complete. It presents the incoming caller, physical Navi emits one Connect and changes the UI from Answer to End, and the network's Connect Acknowledge reaches the handset as a good FACCH block. A queued protocol response pre-empts stale idle-poll backoff while preserving the evidenced NHM-5 MM-information settling interval. A second physical Navi organically drives Disconnect, network Release, Release Complete and traffic-link release; its independently observed release CHANNEL_CONFIGURE carries product byte `0x14`, receives the zero-body confirmation and returns to the idle PCH schedule. The same lifecycle independently publishes DSP command `0x08/0x060b` at Answer and `0x08/0x040a` at End. Nokia's NHM-5 schematic establishes built-in MIC2 and differential EAR endpoints without NSE-8 gains; system-module pages 27--28 establish the distinct 1 MHz/125-clock, 8 kHz, 13-bit-in-16 PCM profile. The organic media gate carries 150/146 encoded-uplink/decoded-downlink frames and 147 non-silent COBBA blocks before clean End. Its resilience gate resumes active speech through exact save-state replay, recovers from 56 impaired bursts per direction and resulting BFIs, proves bidirectional FACCH substitution/recovery, and observes 33 correctly phased SACCH/TF reservations while speech advances. The isolated physical gate routes a host source only through MIC2 and captures only EAR: all 250 microphone blocks are non-silent, the network decodes non-silent unclipped uplink, and playback contains a 5.36-second 1 kHz run. Product-specific analogue gain programming remains unproved. |
| Nokia 3330 NHM-6 v4.50E | Yes | Yes | No | No | No | No | No | `verify-3330-frontier` and `verify-3330-navigation`; later product contracts are not established. Its lower external-service transport remains available, but the typed application profile is `none`; the frontier gate still passes without inheriting NHM-5 registration delay, status body or channel bitmap. |
| Nokia 3410 NHM-2 v5.46E | Yes | Yes | No | No | No | No | No | `verify-3410-frontier` and navigation/menu gates; later product contracts are not established. It inherits the same conservative `none` external-service application profile, and its frontier gate still passes without the 3210/3310 script. |
| Nokia 6110 NSE-3 v4.06 PPM B (ROM3 candidate) | No | No | No | No | No | No | No | `verify-6110-static` proves the 1 MiB flash/64 KiB SRAM reset boundary, UE4 keypad, 24C64 wiring, SIMI/T=0 surface, DSPIF rings, 64-block sparse flash-verification transport and product-local radio/service envelopes. The shared lab card now implements fourteen of the firmware's eighteen unique SIM instructions, including profiled RUN GSM ALGORITHM; A3/A8 defaults to absent and the synthetic subscriber explicitly selects the independently vector-tested TS 55.205 section-5 AES example. Matching F711604 internal ROMs, EEPROM and the DSP-owned non-zero verification verdict remain absent, so direct-to-flash reset and every firmware-derived peer remain disabled. Detailed evidence and unresolved semantics are maintained in `docs/6110_bringup.md`. |
| Nokia 6110 NSE-3 v5.48 PPM B (ROM3) | No | No | No | No | No | No | No | The original service package, manifest ROM selection, normalized 1 MiB image and independent real-ROM3 handset record agree on `V  5.48`, `08-09-99`, DSP software `40.3.617`, DSP internal software ROM3 and physical COBBA B07. `verify-6110-v548-static` proves a four-cell bootstrap with a bounded pre-upload `0x10004/0x10006` equality gate, `0xffff` sentinels, 64 transfer blocks and final `0x10000/0x10002` capture. The captured pre-upload value has exactly two direct consumers: selector `0x09` encodes it as one ASCII digit and the mismatch UI labels it `DSP ROM`. Together with the matching running-handset record this constrains ROM3's pair to `3/3`; the typed HLE publication is separate from final completion and is emitted only after firmware initializes both cells. The gate also proves the first final result is rendered as `B06` and later required to equal `0x0b06`; the conflict with physical B07 prevents assigning that result a fitted-silicon meaning. A fourteen-root census of the complete bootstrap state finds the second final result is capture-only: the MCU requires its transition away from `0xffff` but never consumes its numeric value or passes the state pointer elsewhere. Its exact EEPROM directory independently places records `0701/0702/070a/070b` at `0x0358/0x0360/0x035c/0x035e`, not ROM4's offsets; its validator proves `(identity sum + security level) & 0xffff == stored checksum`. The record-`0702` load proves the security-level setting is byte `+0x1f`, hence EEPROM `0x037f`. The exact DSP-owned completion verdict, F711604 internal ROMs and matching EEPROM remain absent, so the profile leaves the sentinel parked, remains fail-closed and is not promoted to booting. |
| Nokia 6110 NSE-3 v05.48 PPM B (ROM4) | No | No | No | No | No | No | No | The package manifest explicitly names this separate `Rom4ImageFile`/`Rom4PpmFile` family; its normalized image embeds `V 05.48`, `03-09-99`. The static gate proves the homologous loader at a `0x1c1c` relocation and a different staged-stream fingerprint, plus the same final `0x0b06` constraint at separately relocated formatter and comparison sites. A current third-party NSE-3 HLE advances both exact ROM3 and ROM4 images for 100,000,000 instructions while supplying the same hard-coded `4/4` pair; that comparison proves transport viability but also proves boot progress cannot identify the physical DSP ROM because the firmware checks only equality/non-sentinel state. Its homologous fourteen-root state census likewise proves `0x10002` is capture-only after its required sentinel transition, without revealing the DSP verdict. Its exact EEPROM directory places records `0701/0702/070a/070b` at `0x0380/0x0388/0x0384/0x0386`, corroborating the collaborator's ROM4 record offsets while proving they cannot be inherited by ROM3; its homologous validator proves the same checksum relationship. The record-`0702` loader statically proves the power-up security setting is byte `+0x1f`, EEPROM `0x03a7`, replacing the prior sweep-only classification. Its pre-upload value, exact completion event and matching EEPROM remain unresolved. A regression gate keeps BIOS 3 fail-closed and rejects an invented ROM4 publication profile. ROM4 internal MAD2/DSP images are declared separately `NO_DUMP`; no ROM3 internal image or pre-upload publication is inherited. |
| Other declared DCT3 products | No | No | No | No | No | No | No | Driver ROM declarations are not executable evidence: required local images and product-specific contracts are absent. |

The Nokia 6110 remains unpromoted after an independent ROM4-HLE comparison:
the exact v4.06 candidate receives all 64 alternating transfer
acknowledgements but still waits at `0x2859e0` for the missing non-zero
`0x10002` publication. Its product profile therefore marks bootstrap
completion `nse3_flash_verification_b06_verdict_unknown` independently from the evidenced
transfer count. This publishes the proven first result only and remains
fail-closed at the unknown second publication. See
the reproducible comparison and provenance in `docs/6110_bringup.md`.

The same comparison suggested an outbound type `0x03` control publication.
Exact v4.06 analysis independently proves its fixed, zero-body task-3 object
and its task-11 status-`0x03f1` route. A controller-context/helper guard
selects it as the alternative to a context-`0x1a`
`CHANNEL_CONFIGURE`; both branches publish task-12 status `0x13a3`.
Because six task-17 producer sites feed `0x03f1`, neither the external
`DEACTIVATE` label nor a single acquisition ordering is accepted. This adds
a checked protocol boundary without enabling the NSE-3 radio peer or
promoting coverage.

The comparison also identifies SRAM `0x10fde1` as a possible v5.48 DSP
verdict location. Exact v4.06 analysis rejects treating it as a portable
Boolean hook: direct and derived references use bits 2, 4, 5 and 6 in
different external-service controller paths, and command `0x64` exposes only
bit 6 as its result. The bit meanings and initial state remain unassigned.
No firmware state is forced, no DSP peer is enabled and coverage remains
unpromoted.

The exact image now also proves one organic transition through that bitfield:
with bit 2 set, incoming command `0x64`/body byte 0 equal to 2 sets bit 4 and
arms shared timer `0x13` for raw duration `0x19`. Its task-2 expiry event
`0xd3` clears bit 4 and publishes the typed status with control argument 2.
The timer has three other arms and the initiating DSP condition remains
unknown, so this bounds a response contract without enabling or scripting the
peer.

The bit-2 prerequisite is now bounded too. DSPIF initialization writes mode
cell `0x10b970` to zero; a uniquely dispatched handler can promote it to one
only while shared halfword `0x100e4` is zero. Task-2 initialization maps mode
one to controller bit 2 and uniquely submits task-3 type `0x70` with
one-byte body `0x0d`; the checker independently verifies normalized storage
and lane-correct MCU byte fields. Task 3 queues it without type-specific
inspection and passes stream `70 0d` to the generic DSPIF TX writer. No
equivalence is claimed between that raw DSPIF publication and class-`0x40`
command `0x70`, and the missing DSP response condition still prevents peer
enablement or coverage promotion.

An independent HLE labels `70 0d` as a self-test and replies with type
`0x74` payload `0d 00`. Exact v4.06 proves only the bounded MCU side:
task-2 family `0x74` selectors `0x0d` and `0xd0` share a bit-2-gated status
consumer, which reads status bits from queue-object byte 9 and organically
emits lane-correct DSPIF follow-up `70 0a`. The semantic label, compact ring
layout and direct request/reply correlation remain unaccepted. Exact DSPIF RX
places raw payload at object byte 4, proving compact payload `0d 00` cannot
reach the v4.06 selector/status at bytes 8--9. The exact NSE-8 v6.00 decoder
independently identifies the missing transformation: it inserts
`00 (compact_length + 2) 01 00`, making compact `0d 00` into the
layout-compatible NSE-3 candidate `00 04 01 00 0d 00`. This candidate is
statically checked across both images. NSE-3's initializer independently sets
controller bit 2, sends `70 0d` and arms timer `14/c8`; the completion requires
that bit, cancels timer 14, clears it and emits `70 0a`, while timer event
`d4` is the alternative cleanup. Request/completion correlation is therefore
proven without accepting the external “self-test” name. Compact and framed
completion layouts are now separate typed HLE profiles; NSE-3 selects the
framed profile, but it remains dormant because its DSP bootstrap completion
and later DSP-owned service triggers remain unresolved.

## Promotion rules

The levels are cumulative:

- **Booting**: the handset completes its evidenced flash, MAD2, CCONT, SIM and
  DSP bootstrap path without firmware hooks.
- **Interactive**: physical matrix inputs reproducibly drive normal firmware UI.
- **Registered**: the product's own radio packet grammar camps and completes
  Location Updating against the independent network peer.
- **Call control**: paging, ringing, physical Answer/End and clean teardown pass.
- **Internal media**: bidirectional GSM-FR crosses the firmware/DSP boundary,
  including FACCH substitution, degraded frames, SACCH coexistence and
  save-state replay.
- **Physical duplex**: isolated host microphone and playback paths pass without
  loopback, feedback, or UI-level injection.
- **Hardware-faithful**: all material product flash, DSP, PCM, COBBA and analogue
  topology claims are backed by hardware documentation, firmware analysis or
  reproducible traces. A working calibrated compatibility response is
  insufficient.

For NHM-5, packet length similarity is not semantic evidence. In particular its
68-byte type `0x20` must not be reinterpreted as NSE-8 type `0x1a` merely because
both are 68 bytes. The current radio peer therefore remains disabled until the
NHM-5 command/report lifecycle is recovered.

## NHM-5 v6.39 radio configuration boundary

The first three previously unclassified startup families are firmware-built
configuration publications:

| TX type | Payload | Firmware constructor | Proven source |
| ---: | ---: | --- | --- |
| `0x22` | 32 bytes | `0x2c29f8` | The selected payload is an exact copy of the ROM table at `0x325e28` in the observed product/band branch. |
| `0x20` | 68 bytes | `0x2c2a54` | The selected payload is an exact copy of the ROM table at `0x325ef0` in the observed product/band branch. |
| `0x21` | 32 bytes | `0x2c2aac` | Firmware composes it from two 16-byte tables; a firmware/NV branch selects each source. |

The common initializer at `0x2c2c26` invokes the `0x3c`, `0x21`, `0x22`, and
`0x20` constructors twice. Each object carries its actual DSP type and length
in the ordinary queue object, and the task-side transport reaches the shared
ring through `0x2bc6e0`/commit store `0x2bc6c4`. This proves configuration-table
ownership and packet identity, but not individual RF field meanings or a reply
contract.

The later type `0x56` is separately constructed at `0x2a7dd8`. Firmware
allocates and clears a 164-byte queue object, declares a 160-byte payload and
fills that payload with `0xff`. The producer at `0x28a0d0` walks 16-byte
firmware-owned channel records, copies the two channel bytes from offsets 6/7
into consecutive payload entries, and stops after 80 entries. Both observed
call sites pass their selected record through this producer before publication.
The runtime packet therefore represents one big-endian candidate `0x0058`
followed by 79 erased `0xffff` entries. This establishes a bounded
candidate-channel-list command. The peer places its laboratory cell on the
requested channel rather than substituting NSE-8's ARFCN 1.

The inbound result layout is independently established in NHM-5, not borrowed
from NSE-8. Its consumer at `0x28aa0e` skips a two-byte header, reads the
big-endian channel from object offsets 6/7 and signed RSSI from offset 9,
advances four bytes and bounds the loop at 40 records. Thus the shared
`0x8b/166` encoder is a genuine protocol-layer component used behind two typed
command profiles: NSE-8 bitmap search and NHM-5 candidate-list search.

Type `0x56` publication opens the firmware's candidate-synchronisation window:
producer `0x28a158` selects the candidate record, writes the channel into the
command and sets acquisition-active byte `0x00110e40`. A channel-`0x40`
`RECEIVED_BLOCK` for that ARFCN while this byte is active reaches `0x232258`;
zero error changes the selected record to synchronised state 4. The ROM then
organically constructs type `0x02/20` through
`0x28a842 -> 0x2d5ea6 -> 0x2a874c`, preserving SCH, BSIC and ARFCN fields.
The peer completes the ordinary `NO_PSW_LEFT`, `CHANNEL_CHANGED_CNF` and
`RA_INFO` transaction, after which v6.39 consumes sustained serving BCCH and
RSSI reports and issues its own later serving-channel configuration. The
generic laboratory network encodes SI1 bitmap-0 from the selected serving
ARFCN, so NHM-5 receives a cell allocation containing `0x0058`; it does not
inherit NSE-8's ARFCN-1 cell allocation. This removes a real cross-product
contradiction, but does not promote registration.

An opt-in v6.39 firmware-boundary analysis closes the internal selection
frontier without assigning new protocol meanings. The SI parser accepts SI1
through SI4, reaches its complete `0x0f` set, and drives the selected-cell
machine from state 8 through state 4 to state 6 with a successful return.
Independently, the SIM task reads `EF_LOCI`, `EF_IMSI`, `EF_ACC`, the PLMN
selector and the remaining enabled files through SIMI/T=0. The default card
still contains `EF_LOCI` status “location not updated”; a retained cached-LAI
explanation for the missing update is therefore false. Primitive `0x09c8` is
emitted even when radio acquisition is deliberately held before SCH or SI
delivery, so it is a SIM-side publication rather than evidence that SIM and
selected-cell state have already been combined.

NHM-5 acquisition is no longer collapsed into adjacent TDMA ticks: its active
candidate waits through the same complete eight-multiframe validation interval
used by the selected-cell path. Once the firmware has accepted SI1 through
SI4, it organically configures idle common control, issues Random Access and
accepts Immediate Assignment. The network now encodes the live serving ARFCN
in that assignment instead of inheriting NSE-8's ARFCN 1; v6.39 consequently
publishes an assigned-SDCCH configuration on `0x0058`.

The previously empty assigned-channel uplink was traced to a rejected
confirmation, not a missing MM payload. NHM-5 handler `0x2c4c28` correlates bit
0 of the `CHANNEL_CHANGED_CNF` body with byte 2 of its pending channel-change
context. That context requests success value one for the assigned SDCCH; the
all-zero body accepted by NSE-8 is discarded before NHM-5 can arm LAPDm. The
typed NHM-5 radio profile now returns that recovered success value. Firmware
then organically constructs SABM with its Location Updating Request, consumes
contention UA and Location Updating Accept, acknowledges N(R)=1, consumes RR
Channel Release, acknowledges N(R)=2, updates `EF_LOCI`, deconfigures channel
`0x60` and resumes serving BCCH/PCH. `verify-3310-radio-registration` checks
that complete ordered lifecycle, exactly one request, SIM persistence and
steady post-release camp. No firmware state, internal message or synthetic
uplink payload is injected.

The release transaction has recovered context `0x0409/01/00` and the modeled
DSP returns the observed zero body. Static decode and runtime branch tracing
correct an earlier stronger claim: handler `0x2c4c28` recognizes context
`0x0409` before its bit comparison and takes the release-completion path for
either body value. Only the assigned-channel context proves a correlated
value-one requirement.

## NHM-5 v6.39 paging boundary

The first post-registration page is no longer an unexplained transport loss.
The peer emits channel-`0x60` `RECEIVED_BLOCK` with a 22-octet Paging Request
Type 1 carrying the registered IMSI. Firmware routine `0x260994` accepts the
complete block as result `0x061a`, copies the L3 body into its channel state,
and `0x2607b2` constructs received-block event `0x05eb`. The event is posted to
NHM-5 task 13 and received by the loop at `0x2a75a0`; its payload retains
protocol discriminator `0x06`, message type `0x21`, channel-needed SDCCH and
the exact mobile identity.

The task passes that object to parser `0x280664`, which classifies the page as
result `0x08a2`. Ordinary no-identity PCH fill blocks on the same transport
produce zero, so the page is distinguished and parsed rather than discarded
as fill. The active result branch is `0x2573c0`: it sets parser-completion byte
`0x0010dc5f`, releases the received object and resumes the parser wait loop.
This result is stronger than a message-type decode. The Paging Request path
calls `0x28056a` for mobile-identity IE `0x17`; that helper recovers the SIM
identity through `0x290462`, compares its length and bytes with `0x2f0dd0`,
and returns one only on an exact match. The addressed request reaches
`0x08a2`; a request carrying another valid IMSI does not. Thus the unresolved
boundary is after subscriber identity matching, not in Paging Request
construction or SIM identity availability.

The frame number in the received-block report is also intentionally a
scheduled future paging frame. A quarantined experiment delayed each PCH
report until that numbered TDMA frame. It made the NHM-5 report timestamp and
frame number coincide, but the proved NSE-8 reference then accepted no page
and emitted no RACH in a 40-second run. Restoring advance delivery restores
the established NSE-8 page-to-RACH lifecycle. The frame number is therefore
not a stale label to be corrected at delivery time: it gives Layer 1 advance
notice of the subscriber's CCCH paging opportunity. Both product profiles
already receive that generic scheduling contract.

The separate RR admission path at `0x255ff8` checks that generic completion
byte before selecting a decoder. Under the former aliased BCCH cadence it was
not reached after the page. Delaying the page until well after SIM file
activity had settled did not change that outcome. Repeating the correctly
addressed request at every subsequent 102-frame paging opportunity made the
parser repeat the same successful completion without waking the idle-RR
admission path or producing RACH, which excluded a missing network retry
before the scheduling cause was found.

The short nine-second trace was not by itself a sufficient latency bound:
the proven NSE-8 path does not emit its paged RACH until 13.64 seconds after
the page, with serving-cell revalidation in between. A 32-second NHM-5 run
covered that complete comparison window. NHM-5 received its page at
7.731663 seconds, performs two correlated channel changes after outbound DSP
type `0x46`, emits outbound type `0x57/03050000` at 10.120796 seconds, and
continued receiving BCCH/PCH reports through the end of the run without
another random-access request. The corresponding NSE-8 trace receives its
page at 16.980246 seconds and emits the paged RACH at 30.620707 seconds.
This proved that the then-missing NHM-5 transition was not an artefact of
stopping before the established NSE-8 admission latency.

The reconfiguration following the page is temporal, not causal. A matched
15-second idle control with no network event emits the same
`0x46 -> 0x02 -> 0x89 -> 0x02 -> 0x89 -> 0x57` sequence at
10.079243..10.096980 seconds, versus 10.102477..10.120796 seconds in the
paging fixture. It is periodic serving-cell maintenance. In the aliased
cadence the addressed page had no downstream effect beyond its
identity-matched `0x08a2` parser completion, and the maintenance terminal
could not stand in for page admission.

Complete receive-event traces also prevent transferring NSE-8 event numbers
by resemblance. NSE-8 receives page event `0x0811`, then the later SI3-driven
serving reconfiguration produces `0x03fc`, `0x07f9` and `0x0835`; `0x0835`
immediately publishes internal event `0x0440` and RACH follows. NHM-5 receives
its `0x08a2` page while its outer RR dispatcher is in idle state 7. Periodic
serving reconfiguration instead produces `0x0801`, `0x07fb`, `0x07d1`,
`0x07dd`, `0x05e8`, `0x08d1` and `0x07d4`, all while that dispatcher remains
in state 7. The product event grammars are structurally different; injecting
NSE-8's `0x0835` into NHM-5 would not model a shared hardware response.

The two post-page `CHANNEL_CHANGED_CNF` packets also do not fail NHM-5's
product-specific body-bit correlation. Runtime at handler `0x2c4c28` shows
that the pending-context pointer is null for both transactions, so the handler
takes its context-null completion path before the comparison at
`0x2c4c48..0x2c4c52`. The value-one body remains required only for the proved
assigned-SDCCH context `0x0402/01/01`; changing the generic confirmation body
could not supply the then-missing paging handoff.

Address-scoped writes to the RR global refine the post-release control flow.
The `0x256f1a` release branch arms timer `0x9f`, waits for the real release
events, cancels `0x9f`, sets RR state byte `r4+7` to one at `0x2570b8`, clears
`r4+0x0d` and `r4+0x0f`, and branches directly to the parser loop at
`0x257376`. The addressed page then sets exactly `r4+0x0b`
(`0x0010dc5f`) at `0x2573c8`. It does not write the A2-active byte
`r4+0x14` (`0x0010dc68`); the nearby generic-queue write at `0x2a6e72`
targets the distinct absolute byte `0x0010dc69`. This excludes both an
incomplete release-state change and accidental aliasing of the queue flag
with A2 state.

The foreground RR loop at `0x255fbe` does not enter admission merely because
the parser completion byte is set. It first waits for internal event
`0x08cb`; only that event calls `0x255ff8`, which consumes the saved parser
result and can enter the organic random-access setup at `0x256014`. Static
decode corrects the earlier task-12 attribution: ROM objects
`0x3303a0..0x3303b0` carry `0x08ca`, `0x08cc`, `0x08cd`, `0x08cb` and
`0x08c9`. The task-13 dispatcher maps `0x08ca..0x08cd` to RR timer IDs
`0x9f`, `0xa2`, `0xa0` and `0xa1`; therefore `0x08cb` is specifically timer
A2's expiry event.

RR initialization cancels timers `0x9f..0xa2`. During registration release it
later arms only `0x9f`, for 80 ticks at `0x256f50`; its `0x08ca` expiry is
received before the final channel-change request. The sole A2 arm site is
`0x255ec0`, for 501 ticks, and the aliased-cadence paging trace never reached
it.
Task 13 still sends task 12 request `0x0a05` with selector one, which task 12
stores at `0x0010e4bd`, but no evidence makes that request the producer of
`0x08cb`.

A standards-shaped, nonmatching IMSI page in place of the no-identity filler
also fails to arm A2 or admit the addressed page. Repeated nonmatching pages
eventually make firmware revalidate its serving-channel configuration and
publish its existing type-`0x57` terminal, so treating unrelated subscriber
traffic as the missing wake would be both ineffective and architecturally
wrong. Reversing the release ordering so the real `0x89` confirmation is
accepted before result `0x05e8` likewise does not change paging.

The missing admission event was caused by report scheduling rather than an
unimplemented DSP packet. While registered, the old serving cycle delayed a
whole multiframe after both decoded BCCH and advance-delivered PCH reports.
That made successive BCCH reports advance two 51-frame multiframes at a time,
aliasing the eight-multiframe schedule to SI2 and SI4 after channel release.
NHM-5 therefore never rebuilt a complete SI1--SI4 serving set around the
addressed page. PCH is now kept in its own advance-notification slot without
consuming another BCCH interval. The firmware consequently sees SI2, SI3,
SI4 and SI1 in its own schedule, enters the existing `0x0802/0x0804` handoff,
arms A2, emits paged RACH and completes Paging Response and release. No timer,
event, parser result or firmware state is synthesized.

The intervening type `0x57` is an outbound MCU-to-DSP publication, not an
inbound `NO_PSW_FOUND` report, and its four-byte body is retained verbatim in
the trace. Its firmware lifecycle is now bounded independently of the DSP.
RR result `0x07d4` selects internal event `0x03ec` at `0x2557d6..0x2557ea`;
the generic event builder `0x2a6d68` posts that event to the radio task's
third event slot. The active radio-task branch receives an object of type
`0xc8`, calls the type-`0x57` constructor at `0x2a7fa0` from `0x232144` with
arguments `(3, 0)`, then branches directly to ordinary object release and the
next event receive at `0x2324f0`. The constructor forms the exact body
`03 05 00 00`: `0x2a7b4a` encodes selector three and table index zero into
the first two bytes.

There is no type-`0x57`-specific acknowledgement state after publication.
Moreover RR produces the same internal `0x03ec` event at 5.632798 seconds,
before the addressed page, and again at 10.119838 seconds; only the later
radio-task state selects the observed terminal publication. Thus `0x57` is
not a page-specific request/reply boundary and must not gain a synthetic DSP
response. The paging failure remains the earlier missing firmware event that
would transfer the saved parser result through `0x255ff8`.

The alternative `0x8b` measurement terminal still has its independently
recovered 40-record layout. That path organically constructs type `0x55/4` at
`0x2a7baa` with payload `03 05 00 00`; it is a terminal/control publication,
not the start of the active candidate window. Firmware-named type `0x8a`
`NO_PSW_FOUND` reads its channel from object offsets 4/5 and is a failure
result, not an acknowledgement.
