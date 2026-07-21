# Network and registration boundary

This document defines the retained lower-radio model and its verified stopping
point. The current checkpoint is home-PLMN discovery and finite selected-PLMN
search. It does not claim Location Updating, registered
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

`RA_INFO` is structured timing input, but it is not a serving-identity
transport. The `0x1394` arm of decoder `0x217cac` reads object byte `+4` as a
flag/mask and bytes `+5..+7` as one big-endian 24-bit timing value. It derives
an eight-byte `0x0400` object for task 16. No path in this arm writes the
task-10 candidate tuple or the task-17 desired tuple. The peer's current zero
body is therefore incomplete timing fidelity, but filling it with PLMN or SI
bytes would not repair selection and is forbidden without an exact contract.

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

Action 1 first reclassifies the measured record. A subsequent mode-`0x40`
SEARCH_LIST exposes the same synchronized serving cell. The ROM then emits
MSI (`0x46`), configures logical channel `0x12` as DSP channel `0x60`, and
accepts the request-driven CHANNEL_CHANGED_CNF. Task 10 organically publishes
task-17 status `0x0433` with PLMN 001-01 and LAC 1. Task 17 stores the resulting
home/source tuple as `00 f1 10`; no peer writes either tuple.

The ensuing mode-`0x50` selected-PLMN search is also finite: RSSI, SCH,
CHANNEL_CONFIGURE, CHANNEL_CHANGED_CNF, RA_INFO, one SI1--SI4 batch, then
NO_BCCH_LEFT. The registration manager reaches its terminal predicate with
selection `00 f1 10`. RR then closes its temporary LAPDm contexts: task 15
receives `0x09ef`, task 16 opens and immediately acknowledges SAPI 0 and SAPI 3
with `0x05df -> 0x05e8`, and publishes `0x03ea` to task 10. The task-15
transaction converges internally through `0x09ff`; its generic result adapter
emits MM result `0x09f0` with operation `0x16`. The following byte is not a
reason or GSM cause: constructor `0x208ee0` allocates a `0x3c`-byte object,
writes only the status and operation fields, and leaves the tail untouched.
Successive coherent results retain different allocator residue there. The
result is firmware-created search metadata, not a malformed peer payload.

The desired tuple at `0x10fd00..02` remains zero, but this terminal operation
does not own that commit. Helper `0x2229e8` is the complete direct commit
surface: eleven BL callers invoke it, and every recovered argument is either
2 or 3. Argument 3 copies the home/source tuple at `0x10fd14..16`; other
arguments copy the selected candidate at `0x10fcf8..fa`; both set the commit
flag at task-17 context offset `0x1c`. No caller passes operation `0x16`, and
the `0x09f0` handler at `0x2226e8` does not call the helper. Firmware then
emits DEACTIVATE (`0x03`). This request is fire-and-forget at the recovered
boundary: the earlier HLE response using type `0x83` was unrelated
scalar/controller traffic and was discarded in controller state 2. Removing
that response leaves the coherent radio checkpoint unchanged. Making the
receiver quiet after DEACTIVATE instead stalls without an organic activation
request, while resuming BCCH returns to the selection loop. The missing
contract is therefore above the DSP acknowledgement layer.

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
  the retained additional confirmations answer distinct mode-`0x40`,
  mode-`0x50`, and channel-`0x60` requests;
- complete SI produces task-11 `0x13b2`, not `0x13b3`, and its conditional
  task-10 post is `0x1396`, not `0x1397`;
- task-10 `0x1397`, task-11 `0x13b3`, and controller fan-out state 11 have no
  organic entrance in the coherent camp and must not be synthesized;
- RR/LAPDm `0x05e9`, task-10 `0x03f1`, and DSP TX type `0x07` belong to later
  measurement or established-channel lifecycles;
- task-17 `0x09d6` is a post-registration current-identity-change notification,
  not a network primitive.
- the mode-`0x50` `0x09f0 / 16 0f` result does not commit the desired PLMN;
  operation `0x16` closes the selected-search transaction through a separate
  adapter, while the observed `0x0f` tail byte has no recovered semantics;
- `RA_INFO`/`0x1394` carries radio-access timing and cannot be repurposed to
  populate the candidate or desired PLMN tuple.

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

The current predecessor boundary is the firmware-owned source-identity policy.
Task 10's sole `0x0433` constructor at `0x21adac` publishes the home PLMN from
controller state 3, and task 17 stores `00 f1 10` at `0x222770`. At that instant
task 17's source-update window is clear. The subsequent `0x09f0`
operation-`0x28` lifecycle arms the window at `0x226c50`, so a later `0x0433`
would enter gate `0x22236c`; the retained selected-cell sequence does not
produce that second identity publication. Holding BCCH open after SI1--SI4
merely leaves the search transaction pending, and delaying the channel-change
confirmation shifts the identity and window together without changing their
order.

The apparent second task-10 entrance is now bounded. ROM table
`0x2e2e1c..0x2e2e33` contains status-`0x1392` SI3 change descriptors for
operations `0x19..0x1e`; task 12 passes descriptor `0x2e2e20` (operation
`0x1a`) through the data-driven dispatcher. Helper `0x21a220` consumes scalar
`0x10dc99`. With the organic zero it returns through controller fan-out and
does not publish `0x0433`. A quarantined type-`0x83` diagnostic supplied the
scalar through its sole recovered writer while controller state 3 was active;
the first descriptor pass then initialized four controller slots and cleared
an initialization bit, but still emitted no `0x0433`, desired-tuple commit, or
Location Updating request through 60 seconds. The diagnostic was removed.
Type `0x83` is therefore not a missing registration completion, and unchanged
SI3 does not provide a second descriptor pass.

The argument-2 commit gate is not the ordinary predecessor. Task-17 status
`0x158b` copies its payload byte into policy byte `0x10fcb9`; its sole
constructor at `0x27951c` is called by phone-lock initialization `0x28a338`.
That path derives bit 4 from encrypted EEPROM identity/lock records
`0x0704..0x0707` through `0x27c634`. The unlocked profile therefore publishes
zero intentionally. Both argument-2 callers at `0x225026` and `0x22613a`
require policy one and belong to network-lock enforcement, not ordinary
automatic registration.

The argument-3 commit surface is also closed. Of its eight callers,
`0x223988` zero-initializes the desired tuple during task-17 startup. Every
remaining caller is reached only after an object-bearing internal status
`0x09ee`: four inline task-17 receive branches at `0x2243f4`, `0x22451a`,
`0x2245c6`, and `0x2246c8`; two later transaction continuations at `0x225980`
and `0x225d98`; and helper `0x225e6a`, called from the explicit `0x09ee`
comparison at `0x2246f2`. These paths copy the response object's identity into
task-17 transaction scratch before committing the source tuple. They do not
consume the observed automatic-selection result `0x09f0` operation `0x16`,
and no such `0x09ee` transaction executes in the coherent camp. The zero
desired tuple is therefore not evidence of a missing peer response or a
registration prerequisite; it is state owned by separate manual/policy
transactions.

The subsequent task-17 policy cadence is also observed rather than inferred.
The first usable `0x0447` reaches admissibility while both desired and source
PLMN are zero, so it is rejected; firmware then copies candidate `00 f1 10`
into the source tuple. Later mode-`0x00` searches publish an empty preliminary
`0x0447`, but this is not a missing measurement response. Reconciler
`0x211f60` sees ARFCN 1 at -60 dBm and retained sample history during the
successful pre-camp cycle. SI parsing at `0x210c02` then sets mode byte
`0x10c851` from received-SI bit 1; while that byte is one, the reconciler
intentionally returns early. Post-camp measurement therefore cannot be used
to replay the initial admissibility path. Removing the search terminal is not
a remedy either: a bounded peer experiment returned one RSSI result and
resumed BCCH without `NO_BCCH_LEFT`, and the task-17 search transaction
remained pending. The original finite response was restored.

Current-identity helper `0x209380` is also downstream. Seven MM result
lifecycles call it after filling `0x10ffc8`; a changed identity becomes task-17
`0x09d6`. The observed pre-registration `0x09d6` carries the visible 001-01
identity.

The Location Updating entrance is now mapped rather than absent. Task 16
organically constructs internal status `0x07e0` at `0x250b5a`; task 15 consumes
it at `0x20e858`, prepares the current identity, and would build a GSM MM
Location Updating Request through selector `0x4a` at `0x20ead0`. The exact
encoder is `0x208b98`; its selector-`0x4a` arm at `0x208dae` writes MM message
type `0x08` and appends update type, key sequence, LAI, and mobile identity.

That first entrance is intentionally suppressed because task 17 still owns an
active selection transaction. Status `0x09ec` payload byte `+2` is stored at
`0x10fe4a`; task 15 mirrors the predicate `(byte == 1)` to `0x10fe7b`.
Gate `0x20e9b4` exits while that byte is nonzero. In the coherent run all other
conditions are satisfied: helper `0x208a3c` returns zero, the location object is
present, and state byte `+0x13` is zero. Task 15 returns `0x09f0` operation
`0x28`; task 17 then commits source PLMN `00 f1 10`, posts `0x09ec` mode zero,
and clears the suppression latch. This is a completed selection lifecycle, not
a missing `0x07e0` producer or a reason to force the latch.

Task 17 subsequently reaches its equal-PLMN terminal with source and candidate
both `00 f1 10` and firmware sends DSP DEACTIVATE. DEACTIVATE is one-way, but
it cancels queued receiver work: the peer must not carry an overlapping
SEARCH_LIST across it. Cancelling that stale search removes an HLE lifecycle
error but does not itself create a second Location Updating entrance.

The unresolved question is now the later RR entrance:

> Which evidenced post-selection RR/controller completion reaches the later
> `0x07e0` producer surface after task 17 has cleared the selection latch?

The producer surface is finite. `0x250b5a` is the observed initial-camp path;
`0x251552` belongs to later RR results in the `0x03fc`/`0x0404` family; and
`0x2515f0` is downstream of an already-started task-15 transaction in the
`0x07d1` family, so it cannot initiate registration. The next pass starts from
the predecessor state of `0x251552`, not from another guessed DSP packet.

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

The next investigation starts from the later `0x07e0` producer at `0x251552`
and works backward to an observed RR/controller contract. It must not force the
selection latch, replay the initial camp indication, restore the desired-tuple
hypothesis, or guess another DSP packet.
