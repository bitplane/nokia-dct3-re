# DCT3 model coverage

This matrix records demonstrated product coverage, not family resemblance. A
cell is promoted only by a named reproducible gate or reviewed hardware or
firmware evidence. `Partial` means that the preceding acceptance level works
but a material hardware contract remains calibrated, opaque, or unverified.

| Product / tested firmware | Booting | Interactive | Registered | Call control | Internal media | Physical duplex | Hardware-faithful | Principal evidence or next boundary |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Nokia 3210 NSE-8 v6.00 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Radio lifecycle, paired GSM-FR/FACCH/degraded-media and isolated physical audio gates pass. Real COBBA DSP-controlled mux/gain semantics remain opaque. |
| Nokia 3210 NSE-8 v5.01 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Independent ROM gates cover the call lifecycle and isolated physical duplex. The same COBBA/DSP limitation applies. |
| Nokia 3310 NHM-5 v6.39 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | `verify-dsp-bootstrap-3310`, `verify-3310-frontier`, `verify-3310-navigation`, `verify-3310-radio-registration`, `verify-3310-radio-paging`, `verify-3310-radio-incoming-call-boundary`, `verify-3310-radio-incoming-call-ui`, `verify-3310-radio-incoming-call-lifecycle`, `verify-3310-radio-media-resilience`, and `verify-3310-radio-physical-duplex`. Its typed `0x56` candidate search selects and camps on ARFCN `0x0058`; SI1--SI4, Random Access and Immediate Assignment remain firmware-owned. A correctly addressed page organically produces RACH and Paging Response. On the dedicated link v6.39 accepts Cipher Mode Command, publishes its distinct DSP type-`0x14` body and emits Cipher Mode Complete. Product-specific pacing after acknowledged MM Information lets its firmware-owned time-update transaction finish before SETUP. Continuing the assigned-channel receive schedule after the SETUP acknowledgement lets its queued LAPDm messages emerge organically as Call Confirmed and Alerting. Its TCH/F channel-change context `0x0402/00/01` independently requires confirmation bit zero, unlike the assigned-SDCCH context `0x0402/01/01`; satisfying that typed contract produces the new-main-link SABM and Assignment Complete. It presents the incoming caller, physical Navi emits one Connect and changes the UI from Answer to End, and the network's Connect Acknowledge reaches the handset as a good FACCH block. A queued protocol response pre-empts stale idle-poll backoff while preserving the evidenced NHM-5 MM-information settling interval. A second physical Navi organically drives Disconnect, network Release, Release Complete and traffic-link release; its independently observed release CHANNEL_CONFIGURE carries product byte `0x14`, receives the zero-body confirmation and returns to the idle PCH schedule. The same lifecycle independently publishes DSP command `0x08/0x060b` at Answer and `0x08/0x040a` at End. Nokia's NHM-5 schematic establishes built-in MIC2 and differential EAR endpoints without NSE-8 gains; system-module pages 27--28 establish the distinct 1 MHz/125-clock, 8 kHz, 13-bit-in-16 PCM profile. The organic media gate carries 150/146 encoded-uplink/decoded-downlink frames and 147 non-silent COBBA blocks before clean End. Its resilience gate resumes active speech through exact save-state replay, recovers from 56 impaired bursts per direction and resulting BFIs, proves bidirectional FACCH substitution/recovery, and observes 33 correctly phased SACCH/TF reservations while speech advances. The isolated physical gate routes a host source only through MIC2 and captures only EAR: all 250 microphone blocks are non-silent, the network decodes non-silent unclipped uplink, and playback contains a 5.36-second 1 kHz run. Product-specific analogue gain programming remains unproved. |
| Nokia 3330 NHM-6 v4.50E | Yes | Yes | No | No | No | No | No | `verify-3330-frontier` and `verify-3330-navigation`; later product contracts are not established. |
| Nokia 3410 NHM-2 v5.46E | Yes | Yes | No | No | No | No | No | `verify-3410-frontier` and navigation/menu gates; later product contracts are not established. |
| Nokia 6110 NSE-3 v4.06 PPM B (ROM3 candidate) | No | No | No | No | No | No | No | The declared `noki6110` machine expresses the manual-backed 1 MiB flash extent as the independently reported Intel 28F800B3-T (`0089:8892`), plus a 64 KiB SRAM map, serial 8 KiB 24C64, 84 x 48 display, firmware-verified UE4 five-by-five keypad, MIC2/EAR routes and 1 MHz/8 kHz 13-in-16 PCM profile. Exact-image static gates now cover reset/vector/SRAM, direct MAD2 extent, keypad, GenIO bit-2 EEPROM clock/two-byte address framing, the generic MAD2 SIMI controller surface, its higher ATR/interface-byte parser, PPS selection and T=0 status-family classifier, generic DSPIF ring geometry, the MCU's 64-block bootstrap transfer, and the radio packet envelope. NSE-3 independently constructs two type-`1a`/length-68 bitmap searches and maps ARFCN 1 to payload byte 65 bit 0, exactly matching the shared bitmap wire boundary. Its bounded inbound dispatcher routes the established `80`, `83/84`, `86..8c` and `8f` radio-report vocabulary; type `8b` preserves the 166-byte, forty-record result geometry and enters the same state-4/6/7 measurement topology. Its firmware-private candidate stride is `0x44`, not NSE-8's `0x48`, so that variation remains outside the wire peer. A separate type-`02`/length-20 constructor establishes CHANNEL_CONFIGURE operations 4/6/7; the type-`89` handler posts fixed status `1393` and advances without inspecting a confirmation body byte, while RA_INFO remains distinct status `1394`. Product configuration therefore selects the shared `bitmap_search` wire profile independently, while NSE-3 acquisition policy remains `none` and the peer stays disabled. DSP parameter selector 8 is now bounded separately: it publishes `0x8000 | value[11:0]` at shared `0x100a8`; one service-mode table supplies `0x0600`, and the normal delta updater maps state slot zero to selector 8. That updater constructs the live slot at `0x10c020` through two mask/indexed-value table families, compares it against shadow `0x10c008`, and copies it only after successful publication. Product configuration declares the proven selector decoder independently from the still-empty speech-request policy. No organic Answer/End transition or speech meaning is connected to the table selection, so observing selector 8 cannot activate media. Full report ordering and DSP startup remain unproved. The removable lab card's `3b 10 05` ATR takes the firmware's ordinary `ff 00 ff` PPS path, and its normal status families have explicit firmware paths, so it is composed without treating its synthetic subscriber/filesystem data as NSE-3 identity; data-driven APDU instructions and their first organic sequence remain unobserved. The DSP transfer stages a fingerprinted 64 KiB stream while alternating two synchronization cells. Firmware captures final shared words `0x10000/0x10002`; selector pass-through plus the adjacent system-ASIC query and the documented Nokia service protocol establish that the first supplies COBBA identification `B06`, which one later path requires as `0x0b06`. The generic HLE ready word `1` is incompatible. This still does not establish that the stream is DSP code, its DSP destination, the second result, DSP-side publication semantics or a service grammar. The v4.06 image (`5025a6ac…`) remains a labelled ROM3 candidate; matching F711604 boot/DSP ROMs and EEPROM remain `NO_DUMP`; and every firmware-derived DSP/external-service/radio peer is disabled. Direct-to-flash reset bypass remains disabled, so this is not a boot promotion. See `docs/6110_bringup.md`. |
| Other declared DCT3 products | No | No | No | No | No | No | No | Driver ROM declarations are not executable evidence: required local images and product-specific contracts are absent. |

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
