# GSM network and registration contract

This document defines the retained lower-radio model and its verified boundary.
The current checkpoint includes organic cell selection, Location Updating,
acceptance, SIM persistence, dedicated-channel release, return to serving-cell
monitoring, PCH fill, one bounded paging/Paging Response lifecycle, one
bounded mobile-terminated call-control attempt and one persistently delivered
ordinary mobile-terminated SMS, plus one bounded port-addressed Nokia ringtone
delivery. Packet data and speech traffic channels are outside this checkpoint.

## Ownership

- The SIM card supplies subscriber identity, location cache, preferred PLMN and
  service-provider files through SIMI/FIQ6.
- MCU tasks 10--17 own search policy, candidate selection, system-information
  parsing, RR/LAPDm, Mobility Management and operator-resource publication.
- `nokia_gsm_network_device` owns immutable standards-shaped GSM cell and MM
  data: ARFCN, signal level, BSIC, PLMN/LAC/cell identity, SI1--SI4, Immediate
  Assignment, Location Updating Accept and RR Channel Release.
- `nokia_gsm_session_device` recognizes the complete Layer-3 Location Updating
  Request and owns the per-handset MM progression from contention-resolution
  delivery through acknowledgement of Location Updating Accept and Channel
  Release. It queues typed downlink actions without owning LAPDm sequence
  numbers or Nokia scheduling.
- After registration it retains the organically supplied IMSI mobile identity.
  Named network-event fixtures may move the idle session through Paging
  Response into either bounded RR release or an unciphered MM connection,
  deterministic MM Information, mobile-terminated call control or SAPI-3 SMS.
- `nokia_lapdm_link_device` owns the decoded dedicated-link boundary: SABM
  validation, contention-resolution identity, UA construction and downlink
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

## Verified sequence

`make verify-radio-camp` proves serving-cell acquisition. The stronger
`make verify-radio-registration` proves the following ordered firmware-owned
sequence:

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
time-only MM Information and waits for its LAPDm acknowledgement before SETUP.
The v6.00 firmware then organically sends MM STATUS, Call Confirmed, Alerting
and Disconnect. The network acknowledges each uplink I-frame, answers
Disconnect with Release, then closes RR after the handset's organic Release
Complete and returns to PCH fill. This is a bounded signalling result: the
default erased-identity profile presents its security-code editor, and no
traffic channel, speech codec, ringing UI oracle or answered-call state is
claimed.

`make verify-radio-incoming-sms` selects the ordinary-text fixture. It reuses
the same page and unciphered MM entrance, establishes LAPDm SAPI 3 exactly
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
| RX `0x89` | CHANNEL_CHANGED_CNF | response to an organic channel-configure request |
| RX `0x8c` | IDLE_RA completion | completes receiver/random-access configuration |
| RX `0x8f` | NO_PSW_LEFT | closes the initial power-scan work list |
| MCU `0x0c` | IDLE_RA/random access | form 0 carries the CHANNEL REQUEST octet |
| MCU `0x1b` | SEND_BLOCK | two-byte DSP channel header followed by LAPDm |
| MCU `0x02` | CHANNEL_CONFIGURE | establishes or releases the recovered logical channel |

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
acknowledging N(R)=2, before physical teardown. A focused extra-uplink probe
after that acknowledgement returned only the firmware's ordinary `01 03 01`
UI/fill frame; the firmware independently issued channel deconfiguration and
did not expose DISC at this boundary. The peer therefore does not invent a
DISC/UA exchange. RR release causes firmware to issue its own deconfiguration
request before the peer confirms it.

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

The checkpoint is registered and camped, not a complete cellular network.
Known extensions are authentication/ciphering, periodic and mobility-driven
Location Updating, answered-call/TCH control, MO SMS and MT SMS CP/RP closure,
multipart Smart Messaging and ringtone UI/persistence, handover, measurement reporting,
loss/reselection, rejected registration and configurable multi-cell topology.
Each extension must begin with an organic MCU request or a standards-defined
network event and retain the same request-correlation rule.

`grey_salamander_integration.md` catalogues the external compatibility
knowledge available for those extensions, the local ownership boundary, and
the order in which each item can enter the validated model.

The checkpoint is accepted when:

1. `make verify-radio-camp` passes;
2. `make verify-radio-registration` proves one accepted update and steady camp;
3. `make verify-radio-paging` proves the one-page bounded entrance;
4. `make verify-radio-incoming-call` proves the bounded MT call attempt;
5. `make verify-radio-incoming-sms` proves segmented MT text delivery and the
   exact persistent unread SIM record;
6. `make verify-radio-incoming-smart-message` proves the first part of a
   two-part port-addressed Nokia ringtone across nine stop-and-wait LAPDm
   segments, distinct from ordinary `EF_SMS` filing, while retaining the
   second part until the handset organically closes CP/RP;
7. `make verify-radio-operator` proves the unobscured operator frame;
8. the default coherent frontier and tool/evidence suites remain green; and
9. no diagnostic packet scenario or firmware-state force is retained.
