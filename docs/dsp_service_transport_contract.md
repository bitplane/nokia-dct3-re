# DSP and generic-service transport contract

This document defines the mapped DSP, lower-service, and generic-service
boundaries. It separates mechanisms with distinct owners and transports. The
separation is important: identical numeric classes or
events do not imply a shared transport.

## Boundary inventory

| Boundary | MCU producer | Peer owner | Completion mechanism | Current status |
| --- | --- | --- | --- | --- |
| Shared-control service | `0x290cf4` updates DSP RAM control words and writes retained DSPIF command 4 | DSP | DSP-owned counts at shared offsets `0xda`, `0xe2`, and `0xe4`; MAD2 IRQ 4 enters `0x291068` | Consumption/interrupt partially modelled; reply state incomplete. |
| MCU-to-DSP packet ring | Task 3 calls `0x290840`; `0x2907c4` commits packets | DSP | DSP advances consumer `0x0a6` | Format and ownership mapped; current model only drains complete packets. |
| DSP-to-MCU packet ring | DSP writes packets and advances producer `0x1c8` | MCU task 4 | FIQ 0, then `0x290904`; MCU advances consumer `0x1ca` | Boot-subset replies are focused-tested; wider reply vocabulary remains unknown. |
| L1 mailbox | MCU L1 send stubs write DSPIF and ring the doorbell | DSP/task 22 | Task-22 class/primitive decoder `0x23d62c` | Decoder mapped; no normal downlink traffic in the coherent boot. |
| Generic-service framework | Firmware registrations and queued objects | Firmware service framework | Task-5 dispatcher `0x2af652 -> 0x2638e4` | Runs organically; it is downstream of hardware ingress. |
| External-service transport | Task 2 through task 7 | External service/test peer | Class-`0x40` framed responses | Separate protocol; not a DSP-radio completion path. |

## Implementation audit

The implementation has three explicit owners:

1. `nokia_dspif_device` owns shared RAM, DSPIF, ring indices, packet framing and
   FIQ0/IRQ4-facing completion;
2. `nokia_dsp_hle_device` owns request-triggered shared-service completion, stateful bootstrap
   publications and the established type-`0x70` completion;
3. `nokia_external_service_peer_device` owns discovery, class-`0x40` session
   correlation, channel map and healthy-state sequencing.

The transport contains no service commands or session state. The external peer
contains no shared-RAM offsets, ring arithmetic or interrupt ownership. The
100 us producer wakeup, 5 ms peer packet poll, synchronous bootstrap replies and 36-tick
external-session delay remain calibrated prototype behavior rather than
protocol constants.

All MCU reads are answered from DSPIF-owned backing RAM. The MCU first verifies ordinary shared
RAM, then performs 64 alternating zero-write/peer-acknowledgement exchanges at
`0x0fe` and `0x100`. The peer publishes ready words `0x000..0x004 = 1` when the
exchange completes. Both 3210 ROMs reproduce this state transition. Command 4
similarly causes the peer to clear busy word `0x0e0` in backing RAM. Publication
latency remains HLE policy because no DSP timing oracle is available.

DSPIF command 4 is the hardware doorbell for several DSP-owned activities, but
it is not the sole scheduling edge: not every service-transport ring producer
commit is paired with command 4, so the ring-producer and service-pending
transitions are distinct, required triggers (ledger
`single_timer_dspif_doorbell_replaces_shared_triggers`). Service-transport ring
delivery and shared-control completion use independent device timers. DSPIF is
retained and observed but is not yet a complete arbitration model.

## Packet-ring ownership

All offsets below are byte offsets in the DSP shared window at `0x10000`.

| Range/word | Writer | Reader | Meaning |
| --- | --- | --- | --- |
| `0x000..0x0a2` | MCU | DSP | Outbound packet data. |
| `0x0a4` | MCU | DSP | Outbound producer index. |
| `0x0a6` | DSP | MCU | Outbound consumer index. |
| `0x0dc` / `0x0e0` | MCU request, DSP completion | MCU | Shared-control request/busy words selected by the command-4 helper. |
| `0x0da` / `0x0e2` / `0x0e4` | DSP | MCU | Shared-service counters consumed by IRQ4 handling. |
| `0x100..0x1c6` | DSP | MCU | Inbound packet data. |
| `0x1c8` | DSP | MCU | Inbound producer index. |
| `0x1ca` | MCU | DSP | Inbound consumer index. |

Packet header halfword `LLTT` contains payload length `LL` and packet type `TT`.
The occupied size is `(LL + 3) / 2` halfwords. The peer must never advance a
consumer over a partial packet or overwrite a producer owned by the other side.

## Current organic channel-set publication

The deep profile reaches the peer without an isolated producer probe:

```text
task 17 0x09ec -> task 15 case 6 -> task 16 0x07d6
  -> task 10 0x03e9 -> 72-byte work object
  -> task 3 -> MCU-to-DSP packet ring
  -> task-3 object length 0x44/type 0x1a
  -> wire payload 68 bytes, prefix 00 81 98 00
```

The task-15 leg is firmware-selected, not an emulation divergence: the organic
message is `09 ec 00 ...`, and task 15 copies byte `+2` into its protocol mode.
Mode zero selects `0x07d6`; the nearby fast path requires mode `0x10`.

The DSP-owned consumer must drain preceding type-`0x51` and type-`0x70` traffic for
this packet to become visible. Consumption is not completion: advancing `0x0a6`
must not synthesize a firmware result, queued generic-service object, or inbound
packet.

Type `0x1a` publishes a GSM ARFCN set. Builder `0x219f0c` accepts GSM 900
channel numbers `0..124` and DCS 1800 numbers `512..885`, remaps the latter by
subtracting 383, and encodes the resulting 503-position domain in a reversed
63-byte bitmap with control bytes. These ranges match 3GPP TS 45.005. The
coherent boot's mode-2 source is rejected by the density check, producing flag
`0x81` and a zero bitmap: an organic no-usable-channel-set state.

No response contract exists for this publication. Builder `0x219f0c` is
called only from task-10 state dispatcher `0x21ba54`; after posting the packet it
retains no transaction identifier, reply object, or task-3 completion callback.
It arms the global DSP-service timer, clears the activity counter, and returns.
Task 17 has already received the immediate `0x043c` acknowledgement. The packet
is therefore a fire-and-forget channel-set publication.

The type-`0x80` inner-command-`0x60` decoder does test a byte against `0x1a`,
but the byte is radio-controller state at `0x10dbd2`. It is `0x00` when the
organic type-`0x1a` packet is emitted. That dormant later-state path updates a
counter and can post `0x1395`, not the required `0x1391`; it is not a reply
correlation for the outbound packet.

Timer `0x23`, armed with delay `0x0a0a` beside this publication, belongs to the
global DSP-service cadence. It expires and rearms through task 4 at roughly
34 ms intervals without delivering a task-10 status. It is not the missing
semantic acknowledgement.

## Unreached lower-radio lifecycle

The downstream chain is mapped independently of its ingress:

```text
service-5 status 0x05e8 -> 0x05ea -> task-15 0x07dd
  -> parser success -> task-14 0x09d8
  -> opcode 0x36 -> lower event 0x1033 -> result 0x0fc1
  -> task-10 0x1391 -> task-17 0x0434
```

Opcode `0x2a` is an adjacent context path which produces event `0x102f`, result
`0x0fbf`, and context-handler dispatch `0x253610`; it is not the completion.
The opcode-`0x36` object is a firmware translator product, not a raw peer packet,
so this chain does not prove that type `0x1a` produces `0x05e8`. No evidence
links the ARFCN publication to this lifecycle. If an organic application path
later requires it, a peer model must establish the ingress from an observed
request/response contract rather than inject a downstream event.

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
registration through `0x263d30` executes. Therefore those descriptors do not
produce organic `0x05e8` in the measured boot. The indirect
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

`make verify-dsp-transport` covers wraparound, full-ring rejection and
partial-packet retention as transport-only conformance fixtures, plus the organic complete-packet lifecycle,
consumer advancement, RX publication followed by FIQ0, shared-service
completion through IRQ4, the established type-`0x70` reply, external-session
correlation, v5.01 doorbell/service mechanics and active-profile save/load.
Layering tests prevent protocol vocabulary from leaking back into the transport.
Malformed-packet fault reporting and physical timing remain unexercised gaps.

The paired packet census observes the same semantic sequence in both ROMs;
the complete vocabulary, per-type counts, and per-packet dispositions are
single-homed in the generated `dsp_packet_semantics.md`. Request-derived
behavior is limited to discovery echo/completion, external transport
acknowledgements and the type-`0x70/0x74` `0d00` completion. External
registration and the channel map remain peer-initiated canned frames. All
observed MCU-to-peer packets have classified MCU-side semantics; most are
one-way publications or transport acknowledgements and receive no peer packet
response, including the segmented type-`0x51` DSP memory image and the
non-`0d00` type-`0x70` publications. Do not infer request/reply behavior from
packet type alone.

The shared-service timer is one-shot: a nonzero MCU publication at `0x0e4`
schedules one counter clear and one IRQ4 completion. Periodic zero-to-zero
"completions" are idle IRQs whose absence preserves both ROM transport gates
and the interactive menu oracle. The retained `MODEL_DSP_SERVICE_TICK_MS` name
is compatibility residue: it controls peer packet polling only and does not
create periodic service completions.

A future lower-radio investigation succeeds only when a peer-owned state change
or inbound packet correlates with a firmware consumer through the real hardware
boundary. Type `0x1a` and service-5 `0x05e8` must not be assumed to be the two
ends of such a contract: type `0x1a` has no reply dependency, while the
exhaustive non-SAT census does not establish `0x05e8` as an ordinary-registration
requirement.
