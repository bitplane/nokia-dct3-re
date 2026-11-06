# Native ROM6 radio interface

## Current boundary

The acquired-ROM6 **8210 NSM-3 v5.31** composition `nsm3r6` already issues
native radio-facing work. Its next missing external contract is **timed serial
output delivery and an attached RF/sample path**, not another MCU startup
report. No receiver, tuning decision, carrier or successful decode is supplied
by this census. [Native boot acceptance](rom6_dsp_bringup.md) remains separate.

## Reproduction and coverage

```sh
.venv/bin/python tools/run_rom6_native.py --scenario radio
```

This repeats the qualified 60-second physical unlock/menu scenario with passive
I/O taps. The runner checks native composition, own uploads, verifier/self-test,
program coverage and reviewed early/late menu frames before accepting capture.
`radio-census.json` summarizes `primary/rom6_radio_writes.tsv`; the latter
contains every write, timestamp, mask and bus-time CPU PC. Per-port read
summaries retain counts and first/last times, with explicit caps on distinct
values (16) and CPU contexts (32). These contexts are not issuing pipeline-slot
identities. Missing/overflowed streams and mismatched per-port counts fail.
This is a manual research instrument, not an RF acceptance gate. Retire its
broad overlay/port capture when focused serial-device and receiver tests cover
the recovered boundary; keep only the normalized contract and useful controls.

The qualified current run has a complete **342,473-write** stream. There
are 13 observed read addresses and 38 write addresses, not an exhaustive map
of all reachable firmware paths or physical MAD2 registers.

| Observed operation | Count | Boundary |
| --- | ---: | --- |
| P27 sample reads | 1,245,280 | Every value is zero; first 0.535343582s, last 60.014753494s. COBBA's unattached callback supplies deterministic silence, not measured receiver noise. |
| P31 / P32 writes | 30,354 each | Backend retains words but does not decode an RF chip or apply tuning. |
| P04 writes | 35,855 | Existing comparator raises transfer pending; no serial payload is delivered to a modeled RF device. |
| P30 writes | 30,789 | Raw values 0000/00bb/0100/0103; their physical meanings are not established here. |
| P0e frame-length writes | 11 | Values 0546/1387; inherited quarter-symbol/frame notification policy still needs ROM6-specific validation. |

The report has 545 distinct *issued* P30/P31/P32/P04 tuples. Only 30,348 P04
writes follow fresh writes to all three payload registers; the others lack a
complete fresh payload and their meanings are not established by this capture.
Do not treat every P04 write as a new RF
command. Delivery, bit order, latch timing, IRQ ownership and samples remain
separate obligations.

## Own-code attribution

The first-P31 snapshot preserves the live low program overlay before later
reuse. Its words `0b20..0b6f` exactly match 160 bytes at MCU offset `117070`,
SHA256 `206cc4a2a0352fea4ff5dcfdf037bea65c7eedc15faacdc243accde5035ebb46`.
The uploaded routine contains `PORTW *AR2+,P31` at `0b54` and
`PORTW *AR2,P32` at `0b58`, selecting data words `3f04` or `3f06` upstream.
Its observed bus-time PCs are `0b55/0b59`. This is one writer, not the sole
source: eight P32 CPU contexts occur in this run. A terminal overlay dump is
not evidence of bytes executed earlier at the same low address.

| P32 bus-time CPU PC | Writes |
| --- | ---: |
| `00ae3b` | 9 |
| `001a95` | 19,357 |
| `0048ac` | 3 |
| `003d2b` | 2,749 |
| `001614` | 2,747 |
| `0038c1` | 2,743 |
| `000b59` | 3 |
| `0023e0` | 2,743 |

The acquired resident routine `388c..38c8` has a separate AR2==31 destination
branch: `38bb` sends mapped AL to P31 and `38c0` sends AH to P32. Other branches
build words in RAM for later serializers, including the observed `1a8c..1a98`
P30/P31/P32/P04 path. A pair can therefore be halves of one computed word,
not two independently addressed RF registers.

## Scheduling ownership

The descriptor list belongs to firmware RAM, not a recovered hardware FIFO:

- `296a..2973` acknowledges request `1000` through P01 before testing firmware
  state. `2993..2995` reloads descriptor/payload cursors from `265e/2660`.
- `2a8d..2a99` publishes one P30/P31/P32/P04 group. `2a9d` advances the
  descriptor cursor; `2ac6..2ac8` retain both cursors for the next interrupt.
- `2a63..2a6f` cancels the modeled compare using target `3a98` and updates mirror
  `058e`. The target exceeds both observed counter periods (5000/1351).
- The alternate branch `2ae7..2af4` writes P30, then P04, then P31/P32. It
  is static evidence against assuming one universal write-P04 snapshot rule;
  this branch is not among the run's eight observed P32 contexts.

The current backend has one replaceable compare target and a pending bit,
not a packet queue. Reprogramming replaces its deadline; writing an out-of-range
target stops future matches without acknowledging an already-pending request.
P01/P02 bit `1000` acknowledges that independent pending bit; reset cancels it.
These are the existing modeled contract, not a complete SCU specification.

`tools/rom6_timed_output_check.py` correlates every write with the first 256
logged compare callbacks (24,914 total model expiries). All 256 use the latest
in-range target and read the counter at exactly that target. There are 75 target
reprograms before observed callbacks. Payload values at arm and edge agree in
all 256 cases, so **these observations cannot distinguish write-time latching
from live-register sampling**. All 255 consecutive observed-edge intervals have
a firmware `1000` acknowledgment. No one-ACK-per-packet hardware FIFO follows
from this; pending bits can coalesce when service is delayed.

## Counter-phase contract

The backend's declared 13 MHz/12 counter retains one reference epoch through
wraps. Frame, slot and transfer compares are scheduled at absolute reference
ticks, rounded upward to the first representable attosecond. They are not
relative delays from a fractional-phase register write. A compare cannot fire
while its own counter still reads one tick short. The older relative/floor
calculation violated this in 20 of 256 captured callbacks; the strict checker
rejects that evidence rather than allowing a one-tick tolerance.

Independent literal C++ tests cover fractional writes, equality/next-wrap
policy, nonzero base, short reload, long elapsed time and rational rounding.
Existing registers, pending bits and reference epoch remain saved/reset by
the backend; no new firmware state or forced counter publication is added.
The 13 MHz/12 physical rate, equality-at-write policy and RF serial latch phase
remain separately unmeasured for ROM6.

## Cross-reference limits

Dan's snapshot `dd356ff7c5fc62529ab5843ce737593ebff51f73`, especially
`rom6-rf-acquisition-current.md`, `rom6-pll-unmatched-o3x.md` and
`rom6-frame-int0-ownership.md`, supplies hypotheses, not an 8210 specification.
Our own run reproduces setup pair `81c0/4000` 17 times, `9044/4400` 7 times and
`48ce/4400` 11 times. His 3310 trace's `48c8/4400` channel interpretation must
not be transplanted onto our different word. Zero P30, unmatched tuning and
frame-versus-block notification remain distinct questions; none justifies
forcing the laboratory carrier or synthesizing a received report.

The retained Nokia NSM-3 System Module manual (issue 1, 12/1999, printed 41)
identifies HAGAR, a serially programmed PLL, 64/65 prescaling, 200 kHz reference
and `M=N*64+A`. Printed 25 names MAD2's SCU and MFI separately. These primary
constraints do not decode the DSP port packet or prove which block owns each
edge. Source/hash: [hardware atlas](hardware_atlas.md#cobba-part-identity-does-not-select-pcm-timing).

## Next implementation boundary

Do not add a second descriptor queue. The next useful software target is
decoding the observed HAGAR serial-word producers against the documented
frequency plan, with unknown modes left explicit. Before attaching that decoder
to a timed output, independently resolve its trigger/latch edge (P32, P04 or
compare), replacement and serial-completion meaning. The present stable-payload
prefix does not choose between those models. A clocked sample source with
provenance follows the programming contract; no carrier fallback is permitted.
Native FCCH/SCH/BCCH, camp and registration come after those boundaries.
