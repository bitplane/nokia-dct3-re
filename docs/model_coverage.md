# DCT3 model coverage

This matrix records demonstrated product coverage, not family resemblance. A
cell is promoted only by a named reproducible gate or reviewed hardware or
firmware evidence. `Partial` means that the preceding acceptance level works
but a material hardware contract remains calibrated, opaque, or unverified.

| Product / tested firmware | Booting | Interactive | Registered | Call control | Internal media | Physical duplex | Hardware-faithful | Principal evidence or next boundary |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Nokia 3210 NSE-8 v6.00 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Radio lifecycle, paired GSM-FR/FACCH/degraded-media and isolated physical audio gates pass. Real COBBA DSP-controlled mux/gain semantics remain opaque. |
| Nokia 3210 NSE-8 v5.01 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Independent ROM gates cover the call lifecycle and isolated physical duplex. The same COBBA/DSP limitation applies. |
| Nokia 3310 NHM-5 v6.39 | Yes | Yes | Yes | No | No | No | No | `verify-dsp-bootstrap-3310`, `verify-3310-frontier`, `verify-3310-navigation`, and `verify-3310-radio-registration`. Its typed `0x56` candidate search selects and camps on ARFCN `0x0058`; SI1--SI4, Random Access and Immediate Assignment remain firmware-owned. The NHM-5 `CHANNEL_CHANGED_CNF` success bit is correlated with the pending assigned-channel context, after which v6.39 organically emits its LAPDm SABM and Location Updating Request. The independent network returns contention UA, Location Updating Accept and RR Channel Release; firmware acknowledges N(R)=1 and N(R)=2, persists `EF_LOCI`, deconfigures SDCCH and resumes steady BCCH/PCH monitoring. The next boundary is paging and call control; no 3210 call or audio assumptions are claimed. |
| Nokia 3330 NHM-6 v4.50E | Yes | Yes | No | No | No | No | No | `verify-3330-frontier` and `verify-3330-navigation`; later product contracts are not established. |
| Nokia 3410 NHM-2 v5.46E | Yes | Yes | No | No | No | No | No | `verify-3410-frontier` and navigation/menu gates; later product contracts are not established. |
| Nokia 6110 NSE-3 family | No | No | No | No | No | No | No | Hardware documentation informs shared DCT3 boundaries, but no local declared 6110 ROM/profile or executable acceptance gate exists. Acquire and identify a lawful firmware image before implementation claims. |
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

The alternative `0x8b` measurement terminal still has its independently
recovered 40-record layout. That path organically constructs type `0x55/4` at
`0x2a7baa` with payload `03 05 00 00`; it is a terminal/control publication,
not the start of the active candidate window. Firmware-named type `0x8a`
`NO_PSW_FOUND` reads its channel from object offsets 4/5 and is a failure
result, not an acknowledgement.
