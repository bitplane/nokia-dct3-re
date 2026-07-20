# Network and registration boundary

This document defines the retained lower-radio model and its verified stopping
point. The current checkpoint is serving-cell selection and BCCH system-
information acceptance. It does not claim Location Updating, registered
operator presentation, calls, or packet data.

## Ownership

- The SIM card supplies subscriber identity and preferred-PLMN files through
  SIMI/FIQ6.
- MCU tasks 10--12 own search policy, candidate selection, channel lifecycle,
  and system-information parsing.
- `nokia_gsm_network_device` owns standards-shaped cell data: ARFCN, signal
  level, BSIC, PLMN/LAC/cell identity, and SI1--SI4.
- `nokia_dsp_hle_device` translates between that cell and the recovered Nokia
  DSP/L1 packet protocol. Packets cross DSPIF shared rings and FIQ0.
- No peer writes firmware RAM, posts an RTOS message, selects an MMI callback,
  or renders content.

`NOKI3210_MODEL_RADIO_PEER=1` enables one deterministic laboratory cell. It is
opt-in because it is a test network, not a complete configurable GSM system.

## Verified checkpoint

`make verify-radio-camp` proves this ordered, firmware-owned sequence:

```text
MCU SEARCH_LIST (0x1a)
  -> baseline ALL_RSSI_RESULTS (0x8b)
  -> ARFCN 1 rises above the recovered suitability threshold
  -> firmware publishes candidate 0x0447 (usable=1)
  -> narrowed SEARCH_LIST and SCH RECEIVED_BLOCK (0x80, channel 0x40)
  -> MCU CHANNEL_CONFIGURE (0x02)
  -> NO_PSW_LEFT (0x8f), then CHANNEL_CHANGED_CNF (0x89)
  -> firmware accepts the channel in controller state 2
  -> RA_INFO (0x84) -> MCU IDLE_RA (0x0c) -> completion 0x8c
  -> firmware constructs task-11 0x13a5, action=1
  -> paced BCCH SI1, SI2, SI3, SI4 RECEIVED_BLOCK packets
  -> firmware SI bitmap 0x01 -> 0x07 -> 0x0f
```

The final action is observed at dispatcher `0x213c04` as a firmware RAM object,
not as a peer-created task message. SI3 validates cell identity and task 12
reaches the complete bitmap after SI4. These are the acceptance criteria for a
selected serving cell.

The laboratory cell is GSM 900 ARFCN 1, BSIC `0x12`, reserved test PLMN
001-01, LAC 1, and cell ID 1. It matches the synthetic subscriber's home PLMN.

## Packet contracts

### ALL_RSSI_RESULTS

Type `0x8b` carries a 166-byte body: a two-byte list header followed by forty
four-byte records. Each record is big-endian ARFCN, flags, and signed RSSI.
Absent records use ARFCN `0xffff` and RSSI -127 dBm. The retained sequence
reports ARFCN 1 first at -109 dBm and later at -60 dBm. Firmware reconciles the
record with its request table, derives flags `0x0e`, applies the -108 dBm
threshold, and constructs the usable `0x0447` candidate itself.

### RECEIVED_BLOCK

Type `0x80` carries channel, BSIC, error state, 24-bit frame number, ARFCN,
shift, and a 24-byte GSM L2 block. Channel `0x40` carries SCH and `0x50` carries
BCCH. The ARFCN field is populated as 1, matching the measured and configured
carrier. SI blocks are paced at a 51-frame multiframe cadence rather than
queued behind one interrupt edge.

The retained broadcast set is SI1 (`0x19`), SI2 (`0x1a`), SI3 (`0x1b`), and
SI4 (`0x1c`). SI3/SI4 contain the matching PLMN and location identity. Content
belongs to the GSM network device; Nokia transport headers remain in DSP HLE.

### Accepted DSP receive vocabulary

| Type | Recovered role | First MCU consumer |
| --- | --- | --- |
| `0x80` | RECEIVED_BLOCK | `0x283f1c` / task 12 for BCCH |
| `0x84` | RA_INFO | `0x284e26` -> task 10 `0x1394` |
| `0x87` | NO_BCCH_LEFT | `0x284ebc` -> task 10 `0x138f` |
| `0x89` | CHANNEL_CHANGED_CNF | `0x284f74` -> task 10 `0x1393` |
| `0x8b` | ALL_RSSI_RESULTS | `0x284fd8` -> task 11 `0x13b8` |
| `0x8c` | IDLE_RA completion | `0x284f48` |
| `0x8f` | NO_PSW_LEFT | `0x284e50` -> task 11 `0x13b7` |

The first SCH is an acknowledge-and-configure step. In controller state 2 it
becomes status `0x138e` and causes the outbound CHANNEL_CONFIGURE. A second SCH
belongs to another synchronization lifecycle and is not retained.

## Post-camp state

RR-to-MM activation is organic. RR maps usable candidate `0x0447` to MM input
`0x0839`; MM consumes it in state 6, publishes `0x0a32`, receives RM `0x0a34`,
and RR follows with `0x0838`.

The serving-cell completion then follows this path:

```text
task 10 -> task 11 0x13a5/action 1
task 11 -> task 10 0x1391/result 0
task 10 -> task 15 0x042f
task 15 -> task 17 0x0a22
task 17 -> PLMN admissibility check
```

At the task-11 completion, both recovered measurement lists are empty and the
completion mode is zero. The modes-2/4 fallback copy is therefore deliberately
skipped. Task 17 then compares selected PLMN `0x10fd00..02` with current PLMN
`0x10fcf8..fa`; selected PLMN is zero while current PLMN is 001-01. It starts
the normal `0x09ec` selection transaction.

That transaction is finite. Task 17 publishes the `0x09ef / 14 ff` phase
handoff, task 15 runs the search, and returns `0x09f0 / 16 0f`. Context
continuation 2 is terminal. Later background SEARCH_LIST modes `0x40` and
`0x50` are closed by the applicable empty-search terminal.

The home PLMN publisher `0x21adac` builds task-17 status `0x0433` from the SIM
IMSI. Its two callers are later task-10 acceptance lifecycles and neither runs
during initial camp. Selected-PLMN helper `0x2229e8` cannot copy that home tuple
at reset because the tuple has not yet been published.

## Closed contract classes

The following conclusions are retained in the falsification ledger and must
not be reintroduced as peer behavior:

- raw RSSI records, a distinct neighbor in SI2, or repeated SI blocks do not
  populate the firmware-owned PLMN result list without its measurement mode;
- cached EF_LOCI and a complete EF_PLMNsel are persistence/preference inputs,
  not commands to select the currently visible cell;
- type `0x88` is NEIGHBOUR_TIMING_OFFSET and does not carry serving identity or
  activate the initial PLMN lifecycle;
- types `0x83`, `0x86`, and `0x99` have separately decoded scalar/controller
  roles and do not provide an undocumented PLMN result;
- a second SCH or an unsolicited CHANNEL_CHANGED_CNF is lifecycle-invalid;
- complete SI produces task-11 `0x13b2`, not `0x13b3`, and its conditional
  task-10 post is `0x1396`, not `0x1397`;
- task-10 `0x1397`, task-11 `0x13b3`, and controller fan-out state 11 have no
  organic entrance in the coherent camp and must not be synthesized;
- RR/LAPDm `0x05e9`, task-10 `0x03f1`, and DSP TX type `0x07` belong to later
  measurement or established-channel lifecycles;
- task-17 `0x09d6` is a post-registration current-identity-change notification,
  not a network primitive.

The direct task-10 post surface is exhaustively classified: twenty call sites
account for the observed search, result, DSP receive, SI accumulator, and
bridge families. None constructs a statically identifiable `0x1397`. This is a
quantified absence over direct posts, not proof that no data-driven or external
predecessor exists.

## Location Updating boundary

The next standards-known receive parser is `0x209978`. It accepts GSM protocol
discriminator 5 and classifies Mobility Management messages:

- `0x02`: Location Updating Accept
- `0x04`: Location Updating Reject
- `0x21`: CM Service Accept
- `0x22`: CM Service Reject
- `0x29`: CM Service Abort

This parser is downstream of signalling-channel establishment. The coherent
run does not construct its `0x07dd` input, so an MM response would currently be
unsolicited.

Current-identity helper `0x209380` is also downstream. Seven MM result
lifecycles call it after filling `0x10ffc8`; a changed identity becomes task-17
`0x09d6`. No current identity is committed in the serving-cell checkpoint.

The unresolved question is therefore singular:

> Which organic RM/MM transition selects the visible home PLMN, establishes
> the signalling channel, and emits the Location Updating Request envelope?

Only after that request is observed should `nokia_gsm_network_device` answer
with a standards-valid Location Updating Accept. The peer must not post
`0x07dd`, relabel a raw RECEIVED_BLOCK as an MM object, or write registration
state directly.

## Acceptance and next work

The radio checkpoint remains stable when:

1. `make verify-radio-camp` passes through the six ordered semantic predicates;
2. the default and coherent 3210 boot oracles remain unchanged;
3. `make evidence-check` and `make test-tools` pass;
4. no diagnostic packet scenario or direct firmware-state force is retained.

The next investigation starts backward from the Location Updating Request
consumer/transport and the selected-PLMN commit, not from guessed DSP packets.
