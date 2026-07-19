# Network and registration boundary

This document defines the currently implemented lower-radio boundary.  The
checkpoint is serving-cell selection and BCCH system-information acceptance;
Location Updating, operator presentation and calls remain downstream work.

## Ownership

- The SIM device supplies identity and preferred-PLMN files through SIMI/FIQ6.
- MCU tasks 10--12 own search policy, candidate selection and SI parsing.
- The DSP/radio peer owns measurements, channel-change completion and received
  L1 blocks.  It returns them through the DSPIF MDIRCV ring and FIQ0.
- No peer path writes firmware RAM, posts an RTOS message directly, selects an
  MMI callback or renders content.

`NOKI3210_MODEL_RADIO_PEER=1` enables the smallest recovered radio peer.  It is
off by default because it describes one deterministic laboratory cell rather
than a generally configurable network.

## Verified serving-cell checkpoint

`make verify-radio-camp` proves this ordered firmware-owned progression:

```text
MCU SEARCH_LIST (0x1a)
  -> empty search completions while firmware narrows its bitmap
  -> ALL_RSSI_RESULTS (0x8b), ARFCN 1 becomes usable
  -> firmware publishes candidate object 0x0447, usable=1
  -> narrowed SEARCH_LIST
  -> ALL_RSSI_RESULTS + SCH RECEIVED_BLOCK (0x80, channel 0x40)
  -> MCU CHANNEL_CONFIGURE (0x02)
  -> NO_PSW_LEFT (0x8f), then CHANNEL_CHANGED_CNF (0x89)
  -> firmware accepts the channel change in controller state 2
  -> RA_INFO (0x84) -> MCU IDLE_RA (0x0c) -> completion 0x8c
  -> firmware constructs task-11 status 0x13a5, action=1
  -> paced BCCH SI1, SI2, SI3 and SI4 RECEIVED_BLOCK packets
  -> firmware SI bitmap 0x01 -> 0x07 -> 0x0f
```

The final action is observed at dispatcher `0x213c04` as a real RAM object:
status `0x13a5`, action byte `1`, argument byte `1`, controller state `3`.
It is not a synthesized task message.  The SI3 parser validates the cell
identity and the task-12 accumulator reaches `0x0f` after SI4.  This is the
project's stopping definition of a selected serving cell; it deliberately does
not claim Location Updating or a registered operator.

Task-11 byte `0x10c84c` remains `3` during this process; that is the reset-time
ordinary search lifecycle. Selected-cell acquisition is represented by the
firmware-owned action-1 object above, not by writing literal lifecycle state 1.

## ALL_RSSI_RESULTS contract

The type-`0x8b` body is 166 bytes:

| Offset | Size | Meaning in the recovered path |
| --- | ---: | --- |
| `+0` | 2 | list header; the active peer uses `00 10` |
| `+2 + n*4` | 2 | big-endian ARFCN for record `n` |
| `+4 + n*4` | 1 | peer/result flags, initially zero for the real candidate |
| `+5 + n*4` | 1 | signed RSSI in dBm |

Forty records are consumed.  Absent entries use ARFCN `0xffff` and RSSI
`-127`; the deterministic cell occupies record zero with ARFCN `1`.  Two
baseline reports use `-109 dBm` (`0x93`), below the decoded `-108 dBm` cutoff;
later reports use `-60 dBm` (`0xc4`).  Firmware reconciles the returned record
with its requested ARFCN table and derives flags `0x0e`.

Predicate `0x212048` tests the derived flags with mask `0x0b`.  The accepted
record therefore satisfies `(flags & 0x0b) != 0` and publishes status `0x0447`
with usable byte `1`, ARFCN `1`, RSSI identity `0`.  The peer does not supply
the final `0x0e` flags or the usable bit; both are firmware-derived.

Rejected result shapes are now bounded:

- a channel already above the cutoff on the baseline pass receives an
  exclusion classification and does not become the same usable candidate;
- a table containing only `0xffff`/`-127` records remains empty;
- one isolated improved result does not compose because the search policy
  requires its preceding baseline rounds;
- tuning a task-11 ceiling byte, lifecycle byte or candidate RAM object is not
  part of the peer contract.

## Channel and BCCH packets

The accepted DSP-to-MCU vocabulary is recovered from the ROM handlers:

| Type | Name | First MCU consumer |
| --- | --- | --- |
| `0x80` | `RECEIVED_BLOCK` | `0x283f1c` / task 12 for BCCH |
| `0x84` | `RA_INFO` | `0x284e26` -> task 10 `0x1394` |
| `0x87` | `NO_BCCH_LEFT` | `0x284ebc` -> task 10 `0x138f` |
| `0x89` | `CHANNEL_CHANGED_CNF` | `0x284f74` -> task 10 `0x1393` |
| `0x8b` | `ALL_RSSI_RESULTS` | `0x284fd8` -> task 11 `0x13b8` |
| `0x8c` | `IDLE_RA` completion | `0x284f48` |
| `0x8f` | `NO_PSW_LEFT` | `0x284e50` -> task 11 `0x13b7` |

`RECEIVED_BLOCK` contains channel, BSIC, error, 24-bit frame number, ARFCN,
shift and a 24-byte GSM L2 block.  Channel `0x40` is used for SCH and channel
`0x50` for BCCH.  Once `CHANNEL_CONFIGURE` has recorded BSIC `0x12`, each BCCH
block is accepted with that identity.  SI blocks are paced rather than queued
on one interrupt edge.

The laboratory cell is PLMN 234-15, LAC 1, cell ID 1.  Its minimum broadcast
set is SI1 (`0x19`), SI2 (`0x1a`), SI3 (`0x1b`) and SI4 (`0x1c`).  SI3 and SI4
carry the matching PLMN/LAC identity.  These are standards-shaped inputs to the
ROM parser, not operator/UI events.

## Closed alternatives

The following alternatives are disproven:

- type `0x88` is `NEIGHBOUR_TIMING_OFFSET`; a guessed PLMN-shaped body did not
  feed system information;
- guessed `CHANNEL_CHANGED_CNF` packets sent outside controller state 2 are
  discarded, so packet identity alone is insufficient;
- type `0x83` only updates scalar RSSI state through task-10 status `0x139f`;
  it is not a camp completion;
- type `0x86` is a separate controller/transfer protocol and does not start the
  type-`0x8e` framed session;
- `NO_BCCH_LEFT` and counted `NO_PSW_FOUND` exercise failure finalizers; they do
  not select a serving cell;
- unsolicited SI3 before the channel/RA lifecycle parses in isolation but does
  not compose into acquisition;
- service-5 `0x05e8`, task-15 `0x07dd`, task-14 resource-`0x35` requests and MM
  Location Updating results are downstream registration work.  Using them to
  bootstrap the first cell is circular.

The task-14 request catalog remains mechanically mapped: `0x26605c` encodes 22
ROM-described resource-`0x35` requests from 42 state-machine call sites.  It is
an outbound request producer, not the missing inbound camp completion.

## Next boundary

The next network milestone begins after this checkpoint:

1. identify the firmware-owned trigger for Location Updating;
2. decode the MM request envelope at the existing service/task-15 boundary;
3. return a standards-valid Location Updating Accept through its evidenced
   transport;
4. let firmware publish registered PLMN/operator and signal content;
5. keep the serving-cell verifier and both 3210 boot oracles green.

No part of that later work may relabel a raw `RECEIVED_BLOCK` as `0x07dd`, post
`0x05e8`, or write registration state directly.
