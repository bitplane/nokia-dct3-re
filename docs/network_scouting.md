# GSM network and registration contract

This document defines the retained lower-radio model and its verified boundary.
The current checkpoint includes organic cell selection, Location Updating,
acceptance, SIM persistence, dedicated-channel release, return to serving-cell
monitoring, PCH fill, one bounded paging/Paging Response lifecycle, one
bounded mobile-terminated call lifecycle with physical Answer/End and
bidirectional GSM-FR, one persistently delivered ordinary mobile-terminated
SMS, plus one bounded port-addressed Nokia ringtone delivery. Media coverage
includes FACCH substitution, degraded-frame handling, SACCH coexistence,
save-state replay and isolated physical microphone/playback paths.

## Ownership

- The SIM card supplies subscriber identity, location cache, preferred PLMN and
  service-provider files through SIMI/FIQ6.
- MCU tasks 10--17 own search policy, candidate selection, system-information
  parsing, RR/LAPDm, Mobility Management and operator-resource publication.
- `nokia_gsm_network_device` owns immutable standards-shaped GSM cell and MM
  data: ARFCN, signal level, BSIC, PLMN/LAC/cell identity, SI1--SI4, Immediate
  Assignment, Location Updating Accept, TCH/F Assignment Command and RR
  Channel Release.
- `nokia_gsm_session_device` recognizes the complete Layer-3 Location Updating
  Request and owns the per-handset MM progression from contention-resolution
  delivery through acknowledgement of Location Updating Accept and Channel
  Release. It queues typed downlink actions without owning LAPDm sequence
  numbers or Nokia scheduling.
- After registration it retains the organically supplied IMSI mobile identity.
  Named network-event fixtures may move the idle session through Paging
  Response into either bounded RR release or an SC=0 cipher-control exchange
  followed by an unciphered MM connection,
  deterministic MM Information, mobile-terminated call control or SAPI-3 SMS.
- `nokia_lapdm_link_device` owns the decoded dedicated-link boundary: SABM
  validation, contention-resolution identity, UA/DISC release and downlink
  I-frame sequence state. Its current organic coverage is the registration
  exchange on SAPI 0 plus independent SAPI-3 establishment and two-frame
  downlink segmentation for the SMS fixture. Wider supervisory/reassembly
  behavior remains a deliberate extension.
- `nokia_radio_peer_device` translates between that network and the recovered
  Nokia DSP/L1 packet protocol. Packets cross the DSPIF shared rings and FIQ0.
- `nokia_dsp_hle_device` owns only DSP bootstrap and service-transport timing;
  it forwards radio packets without owning radio phases or GSM payloads.
- No peer writes firmware RAM, posts an RTOS message, selects an MMI callback or
  renders content.

The validated 3210 product configuration enables one deterministic laboratory
cell. It remains explicitly an HLE test network, not a complete configurable
GSM system; a named negative fixture can disable it.

NHM-6 v4.50E independently exercises the candidate-window acquisition grammar:
its firmware publishes an eighty-entry type-`0x56` list whose first real
candidate is `0x0337`, accepts the correlated SCH/channel-change sequence and
parses the paced SI1--SI4 set. Physical completion of the fresh-PMM security,
time and date editors then leads to automatic CHANNEL REQUEST, Immediate
Assignment and Location Updating on the assigned SDCCH. The peer retains the
generic repeated assigned-uplink schedule used by this grammar.

That DCS candidate also exposed a generic network-fixture defect. SI1 formerly
used only the GSM-900 bitmap-0 Cell Channel Description, leaving an empty
allocation for ARFCN 823. The network now encodes a single non-GSM-900 carrier
with the TS 44.018 variable-bitmap format and includes the mandatory SI1 Rest
Octets byte whose band indicator identifies DCS 1800. NHM-6 receives the
resulting origin `0x0337` description organically.

`make verify-3330-radio-boundary` now proves the corrected positive boundary.
The peer preserves successive 51-frame BCCH multiframe numbers and completes
the in-progress eight-multiframe acquisition schedule before confirming the
first idle common-control activation. This lets the firmware's already queued
SI4 publication precede `CHANNEL_CHANGED_CNF`; NHM-6 then selects its `0x03ec`
node and organically emits type `0x46`. The former type-`0x57` path remains a
negative regression signature and still receives no fabricated reply.

The assigned-channel confirmation is independently product-evidenced.
NHM-6 handler `0x320be0` is instruction-identical to NHM-5's correlation
check: it masks body byte `+4` bit zero and compares it with byte `+2` of the
pending channel-change context. The organic NHM-6 SDCCH transition requires
one; zero reproducibly leaves the ROM transmitting only UI/fill frames. With
the correlated value, `make verify-3330-radio-registration` proves automatic
CHANNEL REQUEST, Immediate Assignment, LAPDm Location Updating Request and
Accept, both receive-sequence acknowledgements, EF_LOCI writes, release
deconfiguration and steady camp on ARFCN 823. No NHM-5 payload or firmware
state is injected.

`make verify-3330-radio-registration-preserved` reuses the first run's NVRAM
and proves that a cold boot issues the persisted LAI/TMSI form of Location
Updating, accepts it, preserves the already-current EF_LOCI status and returns
to camp. `make verify-3330-radio-registration-state` restores during the same
organic lifecycle and requires an identical replay after the save-state
boundary.

Cell suitability is a separate, typed laboratory-network policy rather than a
product branch. `make verify-3330-radio-unsuitable-cells` broadcasts two
standards-shaped negative cells: one sets CELL_BAR_ACCESS consistently in SI3
and SI4, and one advertises an RXLEV_ACCESS_MIN above the measured serving
level. NHM-6 parses both sets but remains on its own incomplete-cell path,
without automatic-access publication, CHANNEL REQUEST, Location Updating or
EF_LOCI writes.

NHM-6 paging reuses the same network/session/LAPDm implementation proven by
NSE-8 and NHM-5. `make verify-3330-radio-paging` starts after organic
registration release, requires a no-identity fill in the IMSI-derived TS 45.002
paging group, transmits exactly one Paging Request Type 1 for the registered
identity, and observes the firmware's CHANNEL REQUEST, Immediate Assignment,
Paging Response, release acknowledgement and return to PCH fill. The preserved
cold-boot gate repeats the transaction after the LAI/TMSI-form Location
Updating request. Two save-state gates replay identically immediately before
page delivery and during the assigned-SDCCH exchange.

The paging pass added no NHM-6 behavior branch. The post-registration PCH-fill
latch, paging-group calculation, on-air/monitored DRX distinction, page
construction and negative profiles are generic network or radio contracts.
NHM-5 and NHM-6 continue to differ only through their existing typed acquisition
and channel-confirmation contracts. `make verify-3330-radio-paging-negatives`
proves that a page sent in another DRX group is not exposed at the handset's
decoded-PCH boundary, and that a valid page for another IMSI or an invalid
Mobile Identity LV cannot trigger RR access. A barred cell cannot reach paging
at all.

The subsequent cleanup made that distinction structural: the radio peer asks
for explicitly named monitored or transmitted schedules, and derives page
visibility by comparing the two network-computed groups. Acceptance tooling
extracts the subscriber identity from the organic Location Updating Request and
derives its group rather than embedding the laboratory IMSI or frame offset.
The historical `nhm5_terminal_control` phase name is now the neutral
`candidate_terminal_control`; it belongs to the candidate-window acquisition
strategy used by both NHM-5 and NHM-6.

## Verified sequence

`make verify-radio-camp` proves serving-cell acquisition. The stronger
`make verify-radio-registration` for NSE-8 and
`make verify-3310-radio-registration` for NHM-5 prove the following ordered
firmware-owned sequence:

```text
SEARCH_LIST (0x1a)
  -> ALL_RSSI_RESULTS (0x8b)
  -> firmware publishes usable ARFCN-1 candidate 0x0447
  -> SCH RECEIVED_BLOCK (0x80, channel 0x40)
  -> CHANNEL_CONFIGURE (0x02)
  -> NO_PSW_LEFT (0x8f) -> CHANNEL_CHANGED_CNF (0x89)
  -> IDLE_RA (0x0c) -> IDLE_RA completion (0x8c)
  -> task-11 acquisition action 1
  -> paced BCCH SI1, SI2, SI3 and SI4
  -> random access and Immediate Assignment to SDCCH
  -> assigned-channel CHANNEL_CHANGED_CNF
  -> LAPDm SABM carrying GSM MM Location Updating Request
  -> contention-resolution UA echo
  -> GSM MM Location Updating Accept
  -> dedicated-channel BLOCK_REQUEST
  -> handset LAPDm SAPI-0 RR acknowledging N(R)=1
  -> RR Channel Release
  -> dedicated-channel BLOCK_REQUEST
  -> handset LAPDm SAPI-0 RR acknowledging N(R)=2
  -> EF_LOCI update through the SIM T=0 path
  -> channel-0x60 deconfiguration
  -> return to paced serving-cell BCCH
  -> no-identity PCH fill on common channel 0x60
```

The verifier requires exactly one Location Updating Request and at least four
serving BCCH blocks after release. This excludes a registration retry loop.
Parser result `0x48`, the card-side `UPDATE BINARY`, and the channel-release
transaction prove acceptance at three independent subsystem boundaries.

`make verify-radio-paging` selects a named external network-event fixture and
continues:

```text
no-identity PCH fill
  -> one IMSI Paging Request Type 1 requesting SDCCH
  -> organic CHANNEL REQUEST and correlated Immediate Assignment
  -> assigned-channel configuration
  -> LAPDm SABM carrying RR Paging Response
  -> contention-resolution UA echo
  -> bounded RR Channel Release and handset N(R)=1 acknowledgement
  -> physical deconfiguration
  -> return to no-identity PCH fill
```

The page is emitted exactly once. No call or SMS result is synthesized.

`make verify-radio-incoming-call` selects the call fixture instead. After the
same organic Paging Response and contention-resolution UA, the network sends
an acknowledgement-gated SC=0 Cipher Mode Command, then time-only MM
Information, and waits for each LAPDm acknowledgement before SETUP.
The v6.00 firmware then organically sends MM STATUS, Call Confirmed and
Alerting. The network assigns TCH/F timeslot 1 on ARFCN 1 with GSM full-rate
speech mode. The ROM publishes Nokia `CHANNEL_CONFIGURE` for channel `0xc1`,
accepts its correlated `CHANNEL_CHANGED_CNF`, establishes the new FACCH link
with an empty SABM/UA exchange and sends RR Assignment Complete. The default
erased-identity UI then disconnects; Release Complete, LAPDm DISC/UA, physical
deconfiguration and return to PCH fill are all organic. This proves TCH/F
control, not answered speech or codec data.

`make verify-radio-incoming-call-answered` uses a separate named fixture with
the generated erased-identity security verifier. Physical `12345` input lets
the ROM leave its security editor; the already queued page is then processed
organically. The input harness observes only the mapped PUP buzzer gate before
pressing physical Answer. The ROM stops the ringtone, emits CC Connect, accepts
Connect Acknowledge and remains on TCH/F. The independently recovered
command-`0x08` lifecycle and product PCM profile then gate the generic speech
pipeline; neither the call-control state nor empty packet-ring polls start
media by themselves.

`make verify-radio-incoming-call-lifecycle` presses the context-sensitive Navi
key again after the stable answered interval. The resulting Disconnect,
network Release, Release Complete and release channel change are organic.
Across that sequence the firmware-owned DSP desired word moves from idle
`0x0002` to answered `0x060b`, post-release `0x040a`, and back to idle
`0x0002`. The media gates carry GSM-FR through channel coding, interleaving,
burst transport, FACCH substitution, degraded-frame handling and SACCH
reservation before this organic teardown. The physical gate admits host audio
only through the configured MIC2/EAR board endpoints. Exact COBBA mux/gain
semantics and the individual command-bit meanings remain unassigned.

`make verify-radio-incoming-sms` selects the ordinary-text fixture. It reuses
the same page, SC=0 cipher-control and unciphered MM entrance, establishes
LAPDm SAPI 3 exactly
once, and delivers a standards-shaped SMS-DELIVER for `hello` in two I frames.
The firmware acknowledges SAPI 3, selects `EF_SMS`, writes a complete 176-byte
record through SIMI/FIQ6, and card NVRAM contains the exact unread record.
That independently proves transport acceptance, firmware SMS parsing and
persistent delivery. Neither the default run nor a separate physical
security-code-unlock run emits the expected CP-ACK/RP-ACK tail after storage,
so the fixture does not claim CP/RP closure, RR teardown or a user-visible
message notification. The unlock control rules out the security editor as the
cause of the missing tail.

`make verify-radio-incoming-smart-message` substitutes a Nokia ringtone TPDU
above that same transport. The codec builds a complete 251-byte RTPL tone as
two 128-byte-capacity parts with shared concatenation reference `7a`. The
first part carries TP-UDHI, 8-bit DCS `f5`, the 16-bit application-port UDH for
destination `1581`, and concatenation IE `00 03 7a 02 01`. It crosses SAPI 3
as eight 20-byte I frames plus an 11-byte tail; every frame waits for its
matching handset N(R), including sequence wrap. Firmware does not issue an
`EF_SMS` update and record 1 remains free. The second part is held behind the
still-unobserved CP/RP close, so no reassembly or ringtone-save/play UI is
claimed.

The laboratory cell is GSM 900 ARFCN 1, BSIC `0x12`, reserved test PLMN
001-01, LAC 1 and cell ID 1. The SIM IMSI and preferred-PLMN file use the same
home PLMN. A reserved PLMN avoids coupling the network model to handset-specific
EEPROM network-lock provisioning.

## Operator presentation

Task 11 passes live record `00 f1 10 01` to resource `0x5031` and posts event
`0x1399`; the peer never supplies a display string. The SIM advertises GSM
11.11 service 17, and firmware selects and reads its standards-shaped `EF_SPN`,
but v6.00's idle resource retains its PLMN lookup result. Reserved 001-01 has no
ROM operator-name entry, so firmware renders numeric operator `01`.

`make verify-radio-operator` runs the unobscured provisioned lifecycle, applies
the full registration trace checker, and verifies the stable 12x7 operator
glyph crop. A trial with ROM-known PLMN 234-15 reached Location Updating but
activated the synthetic EEPROM's unmodeled network-lock policy and displayed
`SIM card not accepted`; that identity was not retained.

## Nokia DSP/L1 packet contracts

| Direction/type | Recovered role | Contract used here |
| --- | --- | --- |
| MCU `0x1a` | SEARCH_LIST | four-byte control plus 512-bit ARFCN set |
| RX `0x8b` | ALL_RSSI_RESULTS | header plus forty `(ARFCN, flags, signed RSSI)` records |
| RX `0x80` | RECEIVED_BLOCK | channel, BSIC/error, frame number, ARFCN and 24-byte L2 block |
| RX `0x83` | serving-channel level | correlated RSSI between paced BCCH blocks |
| RX `0x84` | RA_INFO | timing input used by the access controller |
| RX `0x87` | NO_BCCH_LEFT | finite search terminal only when no usable cell remains |
| RX `0x89` | CHANNEL_CHANGED_CNF | response to an organic channel-configure request; NHM-5 correlates body bit 0 with its pending context: assigned SDCCH requires one and release requires zero |
| RX `0x8c` | IDLE_RA completion | completes receiver/random-access configuration |
| RX `0x8f` | NO_PSW_LEFT | closes the initial power-scan work list |
| MCU `0x0c` | IDLE_RA/random access | form 0 carries the CHANNEL REQUEST octet |
| MCU `0x1b` | SEND_BLOCK | two-byte DSP channel header followed by LAPDm; TCH/F FACCH uses active selector `0xb0` and empty polling reports `0xf0` |
| MCU `0x02` | CHANNEL_CONFIGURE | establishes or releases the recovered logical channel; accepted TCH/F configuration exposes channel code `0xc1` |

RSSI results report two -109 dBm baselines followed by ARFCN 1 at -60 dBm.
Firmware reconciles the request table, applies its -108 dBm threshold and
constructs the usable candidate itself. SI blocks are paced at a 51-frame
multiframe cadence rather than queued behind one interrupt edge.

The channel-access model preserves request correlation. Random access produces
an Immediate Assignment only after the firmware requests it. A SABM with an
information field receives a UA echo of that exact field. The session parses
the resulting Location Updating Request before selecting a standards-shaped Accept;
it does not post the firmware's internal MM result. The link records the
Accept's next receive sequence, requests another dedicated uplink block and
accepts only the handset's matching SAPI-0 RR acknowledgement before the peer
queues Channel Release. The link then requires the handset's second SAPI-0 RR,
acknowledging N(R)=2, before physical teardown. A focused registration
extra-uplink probe after that acknowledgement returned only the firmware's
ordinary `01 03 01` UI/fill frame, so the SDCCH registration path still does
not invent DISC. Empty assigned-SDCCH uplinks retain the real multiframe
cadence: a decoded downlink fill opportunity alternates with the next uplink
`BLOCK_REQUEST`. This lets asynchronous upper-layer work, including SIM
authentication, publish an organic response without stopping the DSP schedule
or fabricating a Layer-3 message. In contrast, the assigned
TCH/F call organically exposes an
empty new-link SABM, Assignment Complete and a final DISC. The peer returns
standards-shaped UA frames only for those observed transactions, then confirms
the ROM's own deconfiguration request.

NHM-5 v6.39 independently checks the `CHANNEL_CHANGED_CNF` body before arming
its assigned-channel link. Handler `0x2c4c28` compares payload bit 0 with byte 2
of the pending channel-change context, which is one for the successful SDCCH
transition. An NSE-8-compatible all-zero body is therefore not a generic
success response: NHM-5 discards it and continues returning UI/fill frames.
The product-typed peer supplies value one, after which the ROM itself exposes
the same standards-shaped Location Updating lifecycle and updates `EF_LOCI`.
The later release context is independently `0x0409/01/00`, and the reproduced
confirmation carries zero. The ROM recognizes `0x0409` before comparing the
body bit and takes a dedicated release-completion branch for either value;
zero is therefore observed rather than proved mandatory. A fixed
“success=one” interpretation is still invalid for the assigned-channel
transaction, whose non-`0x0409` context does perform the correlation.

After registration, channel `0x50` continues to carry BCCH while channel
`0x60` carries the decoded PCH/AGCH blocks already associated with the
firmware's common-control configuration. The session derives the two-multiframe
paging phase and one of the nine CCCH block offsets from the registered IMSI
under the cell's `BS_PA_MFRMS=2` configuration. Ordinary idle blocks contain a
no-identity Paging Request Type 1. The named fixture substitutes one
standards-shaped IMSI request, after which the existing RACH/assignment path is
reused unchanged.

## Lifecycle rules

- A valid selected-cell SI sequence ends after its final correlated SI/RSSI
  pair. It must not also emit `NO_BCCH_LEFT`; that contradictory terminal can be
  consumed by the next firmware transaction.
- An explicit SEARCH_LIST preempts the periodic serving-cell stream. Delaying
  it behind a complete BCCH batch leaves a stale terminal for a later search.
- DEACTIVATE is one-way and cancels queued work for the retired receiver.
- Channel confirmations answer specific requests; unsolicited or replayed
  confirmations are forbidden.
- Serving BCCH resumes only after RR release and deconfiguration complete.

These rules are modeled as a Nokia L1 transaction state machine in
`nokia_radio_peer`, with decoded link state delegated to
`nokia_lapdm_link_device` and Layer-3 transaction state delegated to
`nokia_gsm_session_device`. The state numbers are implementation details;
packet type, request correlation and observable firmware transitions are the
contract.

## Closed alternatives

The evidence ledger retains the detailed falsifications. The following must not
be reintroduced as peer behavior:

- repeated SI, invented neighbors or raw RSSI records as direct PLMN commits;
- cached EF_LOCI or EF_PLMNsel as commands to select a visible cell;
- type `0x88` timing-offset or types `0x83`/`0x86`/`0x99` as undocumented
  identity transports;
- a second unsolicited SCH or CHANNEL_CHANGED_CNF;
- task-internal statuses `0x1397`, `0x13b3`, `0x09d6` or `0x07dd` synthesized at
  the DSP boundary;
- guessed SI3, timing, CHANNEL_CHANGED_CNF or MM payload scenarios;
- direct writes to selection, registration, SIM or display state.

## Remaining fidelity

The checkpoint includes authenticated registration, bounded MT call control
and bidirectional full-rate speech, but it is not a complete cellular network.
Known extensions are A5 ciphering, periodic and mobility-driven Location
Updating, MO SMS, MT SMS CP/RP closure, multipart Smart Messaging and ringtone
UI/persistence, handover, measurement reporting, loss/reselection, rejected
registration and configurable multi-cell topology.
Each extension must begin with an organic MCU request or a standards-defined
network event and retain the same request-correlation rule.

`grey_salamander_integration.md` catalogues the external compatibility
knowledge available for those extensions, the local ownership boundary, and
the order in which each item can enter the validated model.

The checkpoint is accepted when:

1. `make verify-radio-camp` passes;
2. `make verify-radio-registration` proves one accepted update and steady camp;
3. `make verify-radio-authentication-boundary` and its 3310 counterpart prove
   organic handset-to-SIM authentication and authenticated registration;
4. `make verify-radio-paging` proves the one-page bounded entrance;
5. the 3210 and 3310 lifecycle/media gates prove physical Answer/End,
   bidirectional speech, FACCH/BFI/SACCH coexistence and clean teardown;
6. `make verify-radio-incoming-sms` proves segmented MT text delivery and the
   exact persistent unread SIM record;
7. `make verify-radio-incoming-smart-message` proves the first part of a
   two-part port-addressed Nokia ringtone across nine stop-and-wait LAPDm
   segments, distinct from ordinary `EF_SMS` filing, while retaining the
   second part until the handset organically closes CP/RP;
8. `make verify-radio-operator` proves the unobscured operator frame;
9. the default coherent frontier and tool/evidence suites remain green; and
10. no diagnostic packet scenario or firmware-state force is retained.
