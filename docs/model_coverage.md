# DCT3 model coverage

This matrix records demonstrated product coverage, not family resemblance. A
cell is promoted only by a named reproducible gate or reviewed hardware or
firmware evidence. `Partial` means that the preceding acceptance level works
but a material hardware contract remains calibrated, opaque, or unverified.

| Product / tested firmware | Booting | Interactive | Registered | Call control | Internal media | Physical duplex | Hardware-faithful | Principal evidence or next boundary |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Nokia 3210 NSE-8 v6.00 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Radio lifecycle, paired GSM-FR/FACCH/degraded-media and isolated physical audio gates pass. Real COBBA DSP-controlled mux/gain semantics remain opaque. |
| Nokia 3210 NSE-8 v5.01 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Independent ROM gates cover the call lifecycle and isolated physical duplex. The same COBBA/DSP limitation applies. |
| Nokia 3310 NHM-5 v6.39 | Yes | Yes | No | No | No | No | No | `verify-dsp-bootstrap-3310`, `verify-3310-frontier`, and `verify-3310-navigation`. `verify-3310-radio-boundary` proves its startup grammar is not the NSE-8 grammar, anchors the profile-selected `0x20/0x21/0x22` configuration constructors, and identifies `0x56` as the candidate-channel list. Its inbound result contract remains the next boundary. |
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
candidate-channel-list command; it does not yet establish that the numeric
value may be substituted with the NSE-8 laboratory ARFCN or that NHM-5 accepts
the NSE-8 `0x8b` record layout.
