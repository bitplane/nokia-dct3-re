# GSM network and registration contract

This document defines the retained lower-radio model and its verified boundary.
The current checkpoint includes organic cell selection, Location Updating,
acceptance, SIM persistence, dedicated-channel release, return to serving-cell
monitoring, and firmware-owned operator presentation. Calls and packet data are
outside this checkpoint.

## Ownership

- The SIM card supplies subscriber identity, location cache, preferred PLMN and
  service-provider files through SIMI/FIQ6.
- MCU tasks 10--17 own search policy, candidate selection, system-information
  parsing, RR/LAPDm, Mobility Management and operator-resource publication.
- `nokia_gsm_network_device` owns standards-shaped GSM cell and MM data: ARFCN,
  signal level, BSIC, PLMN/LAC/cell identity, SI1--SI4, Immediate Assignment,
  Location Updating Accept and RR Channel Release.
- `nokia_radio_peer_device` translates between that network and the recovered
  Nokia DSP/L1 packet protocol. Packets cross the DSPIF shared rings and FIQ0.
- `nokia_dsp_hle_device` owns only DSP bootstrap and service-transport timing;
  it forwards radio packets without owning radio phases or GSM payloads.
- No peer writes firmware RAM, posts an RTOS message, selects an MMI callback or
  renders content.

`NOKI3210_MODEL_RADIO_PEER=1` enables one deterministic laboratory cell. It is
opt-in because it is a test network, not a complete configurable GSM system.

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
  -> EF_LOCI update through the SIM T=0 path
  -> RR Channel Release and channel-0x60 deconfiguration
  -> return to paced serving-cell BCCH
```

The verifier requires exactly one Location Updating Request and at least four
serving BCCH blocks after release. This excludes a registration retry loop.
Parser result `0x48`, the card-side `UPDATE BINARY`, and the channel-release
transaction prove acceptance at three independent subsystem boundaries.

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
information field receives a UA echo of that exact field. The peer parses the
resulting Location Updating Request before returning a standards-shaped Accept;
it does not post the firmware's internal MM result. RR release similarly causes
firmware to issue its own deconfiguration request before the peer confirms it.

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

These rules are modeled as a transaction state machine in `nokia_radio_peer`.
The state numbers are implementation details; packet type, request correlation
and observable firmware transitions are the contract.

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
Location Updating, paging, call control, SMS, handover, measurement reporting,
loss/reselection, rejected registration and configurable multi-cell topology.
Each extension must begin with an organic MCU request or a standards-defined
network event and retain the same request-correlation rule.

The checkpoint is accepted when:

1. `make verify-radio-camp` passes;
2. `make verify-radio-registration` proves one accepted update and steady camp;
3. `make verify-radio-operator` proves the unobscured operator frame;
4. the default coherent frontier and tool/evidence suites remain green; and
5. no diagnostic packet scenario or firmware-state force is retained.
