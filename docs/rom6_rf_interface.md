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

## Qualified RX word arithmetic

`tools/run_rom6_native.py --scenario tuning` adds bounded, read-only Execute
observations to the same UI/radio fixture. It checks the resident formatter and
key normalizer at `388c..3937`, SHA256
`37dc843e29d036630964d06e8d310074afd2734095f1036d9dc39d2e9243e22b`.
All **2,743 formatter calls and actual P31/P32 pairs** match an independent
numeric reference; the run covers all **548 scan keys** (`0..124`, `512..885`,
`975..1023`). Thirty-nine selected paths additionally check normalized input,
addend, conditional doubling, quotient, remainder, return and destination.
This is producer/word qualification, not received-sample or RF-delivery proof.
The complete timestamped342,473-write stream is byte-identical to the plain
radio observer's capture on the same binary/input composition; the added
Execute observations do not change that checked output stream.

For the observed RX branch (`3e7e & 0x6 == 0x4`, `3ec8 & 0xa2 == 0xa0`,
`051f.bit7 == 1`), define a neutral **formatter scalar** `s`:

```text
GSM/E-GSM: k' = k for 0..124, otherwise k - 1024 for 975..1023
           s = 2 * (k' + 4675)
DCS:       s = k + 8514, for 512..885
q, r = divmod(s - 38, 64)
P31/P32 combined = 0x44000000 + ((q << 7) | r) + 38
```

The live GSM addend is `1243` (4675); DCS selects `2142` (8514). The direct
GSM addend `1162` (4450) belongs to the alternate constructor and is not TX
qualification here. E-GSM normalization is signed: key975 enters the adder as
-49, not the unsigned word `ffcf`.

For inverse arithmetic, subtract 38 **before** checking the packing gap or
extracting fields. For `u = P31 - 38`, require `u >= 0`, `u & 0x40 == 0` and
exact re-packing, then `s = 64*(u >> 7) + (u & 0x3f) + 38`. Applying the ROM4
gap test directly to the biased P31 word would reject valid ROM6 words.
`tools/rom6_tuning_contract.py` admits this positive `4400` branch only;
`6000` and unrelated control formats remain unsupported, not invalid silicon.

| Pair | Scalar | Numeric RX interpretations |
| --- | ---: | --- |
| `48c8/4400` | 9352 | GSM1 at 935.2MHz **or** DCS838 at 1870.4MHz |
| `48ce/4400` | 9358 | GSM4 at 935.8MHz **or** DCS844 at 1871.6MHz |
| `47e4/4400` | 9252 | E-GSM975 at 925.2MHz **or** DCS738 at 1850.4MHz |
| `9044/4400` | 18500 | Canonical arithmetic, no matching finite RX scan key |

There are 74 GSM/DCS collisions across the observed domain. The inverse returns
all candidates and leaves a valid unmatched word unmatched; it never chooses
the laboratory carrier. The Nokia frequency plan is numerically consistent
with `f_RX = s*100kHz` in GSM, `s*200kHz` in DCS and `f_VCO = s*400kHz` in
both. Relative to the manual's 200kHz reference, that implies `M = 2*s`;
**it does not establish that the DSP word's bit fields directly encode M**.
Port-to-wire serialization, reference-divider controls and settling remain open.

The own run issues `4a80` payloads `0402` and `0208`, not the sibling's
documented whole-word families. The firmware configuration-class contract below
is qualified; physical band-field ownership is not. Do not whitelist imported
words or arbitrary payloads sharing their low bits.

## Own configuration-class contract

`tools/run_rom6_native.py --scenario config` composes the qualified tuning
fixture with passive data-store and Execute observations. It qualifies
**2,749 key classifications, RAM selections and actual issued configuration
pairs**, covering all548 scan keys. Every configuration is staged before its
P31/P32 issue, without a device reply, RAM poke or RF-source selection.

The words originate in an upload, not a live threshold calculation:

- The native prefix at data `3e7e..3f10` matches294 MCU bytes at unique file
  offset `116b08`, SHA256
  `69d876c8a91b4807e48edeb77b2effabecb01d8c6dca05c4946cc1a9b673f5c2`.
  Individual `0402/0208` literals have another flash occurrence; the complete
  prefix distinguishes them.
- The collector records the published146-word prefix plus the incoming
  `3f10` store value. That is a transaction artifact, not a claimed post-store
  RAM snapshot and not an emulated-memory write.
- Initialization clears both slots at bus-time PC `0f15`; resident
  `22ad..22ae` copies data through AR2 into AR3, publishing `0402` at `3f0f`
  and `0208` at `3f10` (bus-time context `22af`). No further slot write occurs
  in the qualified run.

The key classifier `3d81..3d90` sets `263b.bit2` for0..124 and975..1023,
clearing it for512..885 while preserving other class bits. The **active**
configuration selector is different: uploaded `1994..199d` compares the prior
payload at `1274` with data `3efc`, loads `3f0f` on equality or `3f10` otherwise,
and stages the selected word with tag `4a80` at `1274/1275`. Its60 uploaded
bytes at `1990..19ad` match unique MCU offset `1161b0`.

| Key class | Class bit2 | Prior payload | Compared data3efc | Selected tagged word |
| --- | --- | --- | --- | --- |
| GSM/E-GSM | Set | `1054` | `1054` | `4a800402` |
| DCS | Clear | `082a` | `1054` | `4a800208` |

The run has877 GSM/E-GSM and1,872 DCS selections. The classifier, old-payload
comparison, selected slots and issued pairs agree on every event. The static
`29c3..29cc` bit-based selector remains a separate route, not the active producer
in this run. Do not replace the equality test with a class-bit claim.

`rom6_config_contract.py` provides an **analysis-only** whole-word class
interpretation. Together with the RX scalar inverse it uniquely recovers the
requested key for all2,743 native tuning pairs. Unknown words, including sibling
`0c22/2018` values or arbitrary words with the same low bits, stay unsupported.
A canonical unmatched PLL word remains unmatched under either configuration.

This is not permission to retune at a port write. At11 class transitions the
last issued configuration still describes the previous class when the native
PLL pair is issued. The first subsequent configuration matches the requested
key in every case. Raw issuance order is therefore not a coherent radio-state
update. Moreover `1054/4100` and `082a/4100` co-vary with these classes; the
current evidence does not isolate which physical command owns band selection
or what other fields the `4a80` payload controls.

## Next implementation boundary

Do not add a second descriptor queue. RX arithmetic and configuration-class
selection are qualified. The next boundary is timed delivery of the co-varying
`4100`/`4a80` words and PLL programming: recover their trigger/latch edge
(P32, P04 or compare), physical roles and completion meaning before attaching
a live RF decoder. The current stable-payload compare prefix does not choose
between those models. A clocked sample source with
provenance follows the programming contract; no carrier fallback is permitted.
Native FCCH/SCH/BCCH, camp and registration come after those boundaries.
