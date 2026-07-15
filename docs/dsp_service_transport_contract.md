# DSP and generic-service transport contract

This document defines the boundaries relevant to the current radio-registration
frontier. It separates mechanisms with distinct owners and transports. The
separation is important: identical numeric classes or
events do not imply a shared transport.

## Boundary inventory

| Boundary | MCU producer | Peer owner | Completion mechanism | Current status |
| --- | --- | --- | --- | --- |
| Shared-control service | `0x290cf4` updates DSP RAM control words and writes retained DSPIF command 4 | DSP | DSP-owned counts at shared offsets `0xda`, `0xe2`, and `0xe4`; MAD2 IRQ 4 enters `0x291068` | Consumption/interrupt partially modelled; reply state incomplete. |
| MCU-to-DSP packet ring | Task 3 calls `0x290840`; `0x2907c4` commits packets | DSP | DSP advances consumer `0x0a6` | Format and ownership mapped; current model only drains complete packets. |
| DSP-to-MCU packet ring | DSP writes packets and advances producer `0x1c8` | MCU task 4 | FIQ 0, then `0x290904`; MCU advances consumer `0x1ca` | Delivery mechanism validated with probes; active reply format unknown. |
| L1 mailbox | MCU L1 send stubs write DSPIF and ring the doorbell | DSP/task 22 | Task-22 class/primitive decoder `0x23d62c` | Decoder mapped; no normal downlink traffic in the coherent boot. |
| Generic-service framework | Firmware registrations and queued objects | Firmware service framework | Task-5 dispatcher `0x2af652 -> 0x2638e4` | Runs organically; it is downstream of hardware ingress. |
| External-service transport | Task 2 through task 7 | External service/test peer | Class-`0x40` framed responses | Separate protocol; not a DSP-radio completion path. |

## Implementation audit

`nokia_dsp_peer_device` currently aggregates four logical responsibilities:

1. the DSP shared-memory and DSPIF register windows;
2. DSP-owned ring indices, pending counters and IRQ/FIQ signaling;
3. a small request-derived DSP HLE for the boot transactions exercised here;
4. the semantically separate external service/test peer carried through that
   transport.

This co-location proves that the transactions compose through firmware-owned
rings and tasks, but it is not evidence that the class-`0x40` peer is part of
the DSP. Bootstrap-ready read overrides, a 100 us producer wakeup, 5 ms service
ticks and the 36-tick external-session delay are calibrated prototype behavior.
They must remain visible as fidelity debt rather than protocol constants.

The eventual separation should preserve one shared-memory/DSPIF transport
owner, attach a DSP HLE or core behind it, and attach the external service peer
at its recovered logical boundary. Do not split it before focused ring,
interrupt and session tests can protect the current composition.

DSPIF command 4 is the hardware doorbell for several DSP-owned activities, but
it is not the only observed work boundary. Service-transport ring delivery and
shared-control completion now have independent device timers. After that split,
a doorbell-only run still left service-session status `0x0089`, task 1 in mode `0x000d`,
and SIM disabled: not every service-transport ring producer commit is paired with command
4. The validated ring-producer and service-pending transitions remain the
behavioral scheduling edges; DSPIF is retained and observed but is not yet a
complete arbitration model.

## Packet-ring ownership

All offsets below are byte offsets in the DSP shared window at `0x10000`.

| Range/word | Writer | Reader | Meaning |
| --- | --- | --- | --- |
| `0x000..0x0a2` | MCU | DSP | Outbound packet data. |
| `0x0a4` | MCU | DSP | Outbound producer index. |
| `0x0a6` | DSP | MCU | Outbound consumer index. |
| `0x100..0x1c6` | DSP | MCU | Inbound packet data. |
| `0x1c8` | DSP | MCU | Inbound producer index. |
| `0x1ca` | MCU | DSP | Inbound consumer index. |

Packet header halfword `LLTT` contains payload length `LL` and packet type `TT`.
The occupied size is `(LL + 3) / 2` halfwords. The peer must never advance a
consumer over a partial packet or overwrite a producer owned by the other side.

## Current organic state publication

The deep profile reaches the peer without an isolated producer probe:

```text
task 17 0x09ec -> task 15 case 6 -> task 16 0x07d6
  -> task 10 0x03e9 -> 72-byte work object
  -> task 3 -> MCU-to-DSP packet ring
  -> type 0x1a, payload 68, prefix 44 1a 00 81 98 00
```

The task-15 leg is firmware-selected, not an emulation divergence: the organic
message is `09 ec 00 ...`, and task 15 copies byte `+2` into its protocol mode.
Mode zero selects `0x07d6`; the nearby fast path requires mode `0x10`.

The DSP-owned consumer must drain earlier type-`0x51` and type-`0x70` traffic for
this packet to become visible. Consumption is not completion: advancing `0x0a6`
must not synthesize a firmware result, queued generic-service object, or inbound
packet.

No response contract is established for type `0x1a`. Builder `0x219f0c` is
called only from task-10 state dispatcher `0x21ba54`; after posting the packet it
retains no transaction identifier, reply object, or task-3 completion callback.
It arms the global DSP-service timer, clears the activity counter, and returns.
Task 17 has already received the immediate `0x043c` acknowledgement. The packet
is therefore a fire-and-forget state/control publication unless later evidence
identifies a correlated inbound transition.

The type-`0x80` inner-command-`0x60` decoder does test a byte against `0x1a`,
but the byte is radio-controller state at `0x10dbd2`. It is `0x00` when the
organic type-`0x1a` packet is emitted. That dormant later-state path updates a
counter and can post `0x1395`, not the required `0x1391`; it is not a reply
correlation for the outbound packet.

Timer `0x23`, armed with delay `0x0a0a` beside this publication, belongs to the
global DSP-service cadence. It expires and rearms through task 4 at roughly
34 ms intervals without delivering a task-10 status. It is not the missing
semantic acknowledgement.

## Expected firmware-side completion

The downstream chain is mapped independently of its missing ingress:

```text
service-5 status 0x05e8 -> 0x05ea -> task-15 0x07dd
  -> parser success -> task-14 0x09d8
  -> opcode 0x36 -> lower event 0x1033 -> result 0x0fc1
  -> task-10 0x1391 -> task-17 0x0434
```

Opcode `0x2a` is an adjacent context path which produces event `0x102f`, result
`0x0fbf`, and context-handler dispatch `0x253610`; it is not the completion.
The opcode-`0x36` object is a firmware translator product, not a raw peer packet,
so this corrected chain still does not prove that type `0x1a` directly produces
`0x05e8`. The relationship
between the organic request and the firmware state which activates a recovered
argumentless publisher remains unresolved. A valid peer model must establish
that relationship from request/response evidence rather than inject any member
of the downstream chain.

## Descriptor evidence

The static census finds 149 descriptor registrations: 112 ROM-backed, 18 stack,
12 dynamic-RAM, six fixed-RAM, and one pointer unresolved. Of the 37 descriptors
not decoded from ROM, 30 have a recovered service other than service 5; seven use
a dynamic service argument. Six of those seven construct event `0x0114` with
callbacks other than `0x05e8` and are excluded by their fields. The final site,
`0x28c672`, registers an indirect resident descriptor from `[0x110f1c]`.

In the named eight-second deep-GSM run, transient-registration tracing observes
only service `0x0a`: callers `0x26341e`, `0x296ec8`, and `0x296f16`, all with
event `0x0114`. No transient service-5 registration executes, and no resident
registration through `0x263d30` executes. Therefore the unresolved descriptors
do not explain organic `0x05e8` publication in the current boot. The indirect
resident site may execute in another firmware state, so this is not a global
absence proof.

## Prohibited shortcuts

- Do not post `0x05e8`, `0x05ea`, `0x07dd`, `0x09d8`, `0x0fbf`, `0x0fc1`,
  `0x0fc2`, `0x1391`, `0x1392`, or
  `0x0434` directly.
- Do not treat an outbound packet type as an inbound type without decoder
  evidence.
- Do not use IRQ 4 for packet-ring receive delivery; the validated inbound ring
  notification is FIQ 0.
- Do not conflate task-22 class 5, generic service 5, class-`0x40` service command
  numbers, and DSP packet type 5.
- Do not make packet consumption timing responsible for semantic completion.

## Next acceptance point

There is no focused device test for ring wrap/full handling, partial packets,
interrupt acknowledgement, reset state or external-session correlation. The
coherent frontier and trace manifests are integration evidence only.

A transport investigation succeeds when a peer-owned state change or inbound
packet is correlated with a firmware consumer through the real hardware
boundary and advances the ordinary task-17 registration lifecycle. Type `0x1a`
and service-5 `0x05e8` must not be assumed to be the two ends of that contract:
type `0x1a` has no proved reply dependency, while the exhaustive non-SAT census
does not establish `0x05e8` as an ordinary-registration requirement.
