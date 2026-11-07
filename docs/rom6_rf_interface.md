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
| P31 / P32 writes | 30,354 each | Backend retains words; compare-edge projection reaches the partial HAGAR command-state device, without applying RF tuning. |
| P04 writes | 35,855 | Existing comparator raises transfer pending independently of the raw command-state receipt. |
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

`tools/run_rom6_native.py --scenario serial` captures all **24,914** modeled
compare callbacks, correlated with the complete write stream by
`rom6_timed_output_check.py`. Every callback uses the latest in-range target,
reads the counter at that target and projects the live P30/P31/P32/P04 tuple.
There are 10,995 preceding target reprograms. Arm/edge payloads agree throughout,
so **even this complete run cannot distinguish write-time latching from
live-register sampling**. All24,913 intervening intervals contain a firmware
`1000` acknowledgement; every callback observes pending clear beforehand.
No one-ACK-per-packet hardware FIFO follows from this; delayed service can
coalesce pending bits.

All observed compare-edge controls are `0100`; raw `0000` writes do not reach
a compare in this run. At the edge, tags `4400`, `4100`, `4a80` occur 2,750,
5,506 and2,749 times respectively. The raw callback does not interpret these
families, declare a physical serial completion or supply an RF response.
Its bounded verbose capture rejects truncation rather than weakening coverage.

`make check-c54x-serial-compare` independently exercises the backend without a
handset ROM: target replacement, live-register projection, an armed save/restore
with exact callback time and tuple, both acknowledgement ports, periodic matches,
out-of-range cancellation, pending retention, raw zero control and reset abort.
These are executable **model conformance** tests, not a silicon latch proof.

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

Port numbers here are hexadecimal. Check literal I/O operands before importing
external labels: the stable-read helper `3ee1..3ef2` reads `000d`, which may be
called decimal "PA13" elsewhere; it is not a separate P13 input.

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
selection are qualified, and the partial device below receives raw compare
projections. The next boundary is physical interpretation of the co-varying
`4100`/`4a80` controls and receive-clock/sample ownership. The complete
stable-payload capture does not uniquely choose a physical latch edge; the
device's compare-edge policy is explicit, not a measured silicon conclusion.
A clocked sample source with provenance follows; no carrier fallback is permitted.
Native FCCH/SCH/BCCH, camp and registration come after those boundaries.

## Partial HAGAR command state

Only the `nsm3r6` research composition attaches `nokia_hagar_device`. It receives
the raw P30/P31/P32/P04 tuple on actual compare callbacks, not port reads or
firmware-PC hooks. It owns the last `4100`, `4400` and `4a80` words, their
validity and receipt count. All are saved/resettable. No second descriptor queue,
completion IRQ, PLL-lock indication or received sample is introduced.

The device exposes a **conditional arithmetic candidate**, not RF readiness:
both `1054/4100 + 0402/4a80` or `082a/4100 + 0208/4a80` must agree, and the
PLL word must have a unique matching key in that class. Disagreement, unknown
control, unsupported transactions and unmatched `9044` have no fallback.
Unsupported transactions invalidate the known-family set; all three families
must be received again. Interleaved `4680` retains the conditional arithmetic
candidate without assigning gain semantics or authorizing sample reception.

Independent executable tests cover all 548 keys, the two interpretations of
`48c8`, class disagreement, unknown words/control, unmatched PLL, snapshot and
reset. The MAME compare fixture additionally saves/restores an actual configured
device and checks repeated delivery and reset. Holding the DSP in reset stops
new compare receipts but does not invent an off-chip HAGAR reset; board-level
reset-pin ownership remains unproved. The serial acceptance checks every device
receipt against independent bus/word replay. This is partial stateful hardware
modelling, not qualification of physical divider fields, RF gain, settling,
reference-divider control, serial clocks or a working receiver.

The qualified 60-second serial run checks all 24,914 device receipts: 19,205
have a conditional candidate and 5,709 remain unresolved, covering all 548 scan
keys. Its complete timestamped write stream retains the baseline SHA256 above.

## Receive interrupt and clock boundary

`tools/run_rom6_native.py --scenario receive` adds a passive 1,024-read prefix
and a first-read program view to the qualified serial/UI fixture. The complete
342,473-write stream remains byte-identical to the earlier observers (SHA256
`40f8ae09eb4e7a3cc124bf2d7f9c4e197e831fc122e3cd58830cb24f82f32b6f`).
All 1,245,280 P27 reads still consume unattached silence, not a clocked buffer.

The prefix contains 32 groups of 32 words. Group starts are 4.589913–4.615385ms
apart, compatible with the inherited frame-driven INT0 policy; this is not a
measurement of the physical COBBA sample clock. The first read occurs with
last-written P03=`0000` and P09=`3a98`, before an in-range P09 position is
programmed. Therefore these reads cannot qualify a P09-driven producer.

Own-code ownership is concrete:

- PMST=`07ac` selects INT0 vector `07c0`, which far-branches to `22da`.
  That entry calls uploaded hook `05da`, branching to the live `1845` routine.
- The uploaded routine repeats four P27 reads eight times. Observed bus-time
  contexts `18a9/18af/18b5/18bb` accompany instructions at
  `18a4/18aa/18b0/18b6`; do not identify instruction addresses from bus PCs.
  Live `1845..18d1` matches 282 own MCU bytes at `115f1a`, SHA256
  `bce951612776b11c87f4beda52b0ee527056fc0f9e8767174b626f64c6b786c3`.
- P09's position producers and the receive reload are qualified below; these
  remain separate from physical sample production and interrupt ownership.

This supports investigating P09 as a receive-position comparator, but does not
prove its interrupt route, recurring cadence, buffer ownership, sample rate or
signed-position rule. The backend currently stores P09 without scheduling it;
its frame callback independently asserts INT0 when the CCONT clock is enabled.
Do not transplant the sibling's explicitly diagnostic PA09 cadence as measured
hardware. A receive model must separate sample production, data-available
notification and frame timing before a native decoder result can qualify it.

### Qualified receive positions

The same receive fixture captures every data 1844 store and P09 write, joined to
the independent complete I/O stream. All 5,495 observed P09 writes are classified:

| Issuing instruction / bus context | Count | Value source |
| --- | ---: | --- |
| `3e31` / `3e32` | 2,749 | Current AR1, including zero |
| uploaded `198b` / `198c` | 2,743 | Latest stored data 1844 |
| `3f17` / `3f18` | 3 | Out-of-range `3a98` sentinel |

Data 1844 has two initialization clears and 2,749 writes at instruction `140c`
(bus context `140d`). Every calculated value matches this exact reference:

```text
position = AR1 - 32
if position < 0: position += data036a
store low16(position) at data1844
```

This is one conditional addition of the **raw firmware modulus**, not arbitrary
modulo reduction and not addition of reload+1. Nine calculations use `1387`
(4999); 2,740 use `0546` (1350). The first-read code view qualifies the uploaded
producer and deferred writer against own MCU ranges, and the checker joins
every deferred read to the latest observed store. No unseen writer is assumed.

The short receive-loop reload is intentional own code: uploaded `09ed..09f3`
sets data 036a=`0546` and writes it to P0e. Its 56-byte enclosing range
`09d8..09f3` uniquely matches MCU offset `116de0`, SHA256
`3bc200ec0b2f0dc4e405da8bc274db0a660220a3ef8dc11c192c3e53cdf62825`.
All 11 reload writes are independently bus-correlated. Do not replace this with
a fixed GSM-frame interval or infer CPU corruption from the shorter scan cycle.
Physical reload/compare semantics remain a distinct question.

At the first P27 read, the eight live data 3ebd coefficients are
`0387,11a3,2ad6,4000,4000,2ad6,11a3,0387` (sum 65536). The uploaded reader
consumes four scalar inputs per repeated iteration. These constrain a later
signal/filter experiment; coefficient shape alone does not establish the ADC
sample rate, I/Q wire packing, uninterrupted capture or RF interrupt cadence.

### Capture-position programming

The receive fixture independently joins all **11,001** writes to P0a, P0b and
P11 against the complete bus stream. These are register-value contracts, not
proof of the corresponding physical capture edges:

| Port / issuing instruction / bus context | Count | Value source |
| --- | ---: | --- |
| P0a / `3e23` / `3e24` | 2,749 | AR1 minus data 3ea7; add raw data 036a once if negative |
| P0a / `3edb` / `3edc` | 2,747 | `3a98` sentinel |
| P0a / `3f26` / `3f27` | 3 | `3a98` initialization |
| P0b / uploaded `15cf` / `15d0` | 2,747 | Low A after stable P0d read, addition of data 3eb6 and one conditional subtraction of data 036a |
| P0b / `3e2b` / `3e2c` | 2,749 | `3a98` sentinel |
| P0b / `3f37` / `3f38` | 3 | Zero initialization |
| P11 / `3fe4` / `3fe5` | 3 | `01fe`; field meaning unknown |

Every observed start lead is `00a3` (163), and every observed stop tail is
`0020` (32). The checker verifies each start calculation and stop accumulator,
but does not independently capture each preceding P0d read. The stop formula
is statically decoded; its counter-input chronology is not yet runtime-qualified.
The uploaded `15c4..15d1` region uniquely matches MCU offset `115a18`, SHA256
`820291aa03aec557fd522ba30fc3d54adc3689b0e4bafb6f6ff37c3b68d65453`.
Do not assign these writes to the similar resident `3e7a` instruction.

The start routine writes P0a, defuses P0b, programs P09 and then sets P03 bits
0/2. The stop routine clears P03 bit 0 before programming P0b; the common tail
then defuses P0a. This ordering is compatible with an armed, counter-timed
capture lifecycle rather than immediate P03 level gating. It does not identify
which compare controls ADC clocks, transfer clocks or interrupt generation.
In particular, P11=`01fe` is not evidence for a 512-word FIFO.

## ADC-pin transport experiment

`nsm3adc` and `nsm3ad0` are explicit **laboratory compositions**, not promoted
handsets or RF acceptance gates. They attach `nokia_mfi_rx_device` between a
clocked COBBA ADC callback and P27. The former applies an analytic periodic
67.708333kHz I/Q tone of amplitude 1024; the latter supplies matched zeros.
Neither source uses HAGAR's candidate, an ARFCN or a firmware PC. This is
direct ADC-pin stimulation, **not a tuned over-the-air signal**.

```sh
.venv/bin/python tools/run_rom6_adc_lab.py --machine nsm3adc
.venv/bin/python tools/run_rom6_adc_lab.py --machine nsm3ad0
make check-c54x-serial-compare
```

The candidate clocks scalar words at 13MHz/12, not from P27 demand. This is
the two-complex-samples/symbol composition matched to the native accumulator
below, not a measured silicon clock. The laboratory now uses a 96-word staging
limit derived from the service-gap bound below, **not a measured MFI capacity**.
The component's default 64-word limit remains a logical negative control.
Words, transfer ticks,
queue state and the next clock deadline are saved. Block notifications
exclusively own INT0 throughout these laboratory compositions, including
inactive capture periods. The non-laboratory compositions retain their existing
frame source. Experimental enable wiring requires
P03.bit0, P21.bit10 and clear P21.bit12. This is a hypothesis, **not a recovered
truth table**; P09/P0a/P0b capture boundaries are not implemented here.

The oscillator latches simultaneous I/Q at the I transfer and serializes Q twelve
reference ticks later. Logged `source_tick` is the **scalar interface-transfer
timestamp**, not necessarily analogue conversion time. The independent checker
evaluates sine/cosine from these timestamps and rejects skipped or future words.
Periodicity is intrinsic to the analytic source, not finite-capture fallback.

Exclusive ownership removes pre-capture warm-up reads caused by synthetic
frame interrupts. In the divider-6 qualification, both laboratory inputs first read P27 at
2.537874286s (bus context `2446`, in the native discard loop). All **40,320**
P27 reads reconcile with consumed clock-produced words across activation
counter resets; none is an inactive retained-register read. The checker also
qualifies 2,752 actual word values across three activation prefixes.

The original 64-word transport result is **negative**. Matched tone/silence trials reject
the sufficiency of each exact combination below, not its individual physical
rate, comparator or capacity hypothesis:

| Scoped trial, all with 64-word buffering | Scalar divider | First overrun |
| --- | ---: | --- |
| P21-only enable | 6 | 0.803263846s, before RF work |
| P03 gating, frame INT0 outside capture | 6 | 2.548358769s |
| P03-armed one-shot P09 start / P0b stop | 6 | 2.548362462s (21,600 produced / 21,536 consumed) |
| P03 gating, cadence-only comparison | 12 | 2.548639385s (11,680 / 11,616) |
| P03 gating, exclusive MFI INT0 | 6 | 2.548358769s (22,752 / 22,688) |
| P03 gating, exclusive MFI INT0 | 12 | 2.548639385s (11,680 / 11,616) |

The timed-gate implementation was removed. The slower divider was initially
restored after its transport-only test, then selected by the independent
post-filter signal test below; it still does not qualify losslessness. Exact
binary hashes, fault contexts and retained raw-capture locations for those
rejections are in [the falsification ledger](../evidence/falsifications.json).
These failures do not establish physical buffer capacity or justify a capacity
sweep. The runner rejects every fault across counter resets.
The header distinguishes oscillator phase divisor from MFI scalar divisor;
the checker rejects disagreement with the device summary. Cadence comparisons
must not retime the laboratory oscillator.
Word/block diagnostics have per-process budgets of 4,096/128, independent of
the number of capture activations. Reset and save/load do not replenish these
host-only budgets. The checker joins every P27 read to full queue accounting
while explicitly bounding value qualification to logged prefixes; it does not
equate a small trace with a complete value census.

At both overflows the passive fault observer sees IMR=`035f`, IFR=`0021` and
ST1=`c903`/`4903`: INT0 is enabled and pending, but global INTM is set. This
identifies native critical-section backpressure at those events, not a missing
IRQ request. It does not establish the physical buffer capacity or clock gate.

Logical tests cover ordering, provenance, peeks, partial blocks, overrun,
underrun and reset. The MAME component gate saves a partial buffer and checks
its exact first restored block, transfer ticks and completion time. An
independent second backend provides an interrupt-ownership control: with the
same enabled CCONT frame clock, the default backend receives frame INT0, the
external backend receives none while idle, and its explicit data-available
input does assert INT0 without a P03 write. This is composition conformance,
not a claim about the fitted MAD2 interrupt multiplexer. An
in-callback stream load does not rewind the test timer's scheduler-local
relative deadline; its later callback is not an equal-time endpoint. Neither
this component test nor the good source prefix qualifies a native ADC session.
No RSSI, FCCH, SCH, BCCH or camping result is claimed.

### Qualified native FIR prefix

`rom6_adc_filter_check.py` joins the first 1,024 clock-delivered scalar words to
all 1,024 writes into native history `0200..020f`. It then independently checks
the first 128 output words stored at `00c8..00d7`: tone produces 128 nonzero
words, silence produces 128 zero words. The 32-word discard, subsequent
four-input/two-output ordering and signed arithmetic are checked, not inferred
from a changed screenshot or merely nonzero RAM.

The reference is an eight-tap signed convolution over the observed circular
history, with Q16 coefficients and half-up high-word rounding. Native output
order is **Q,I**, with complex decimation by two. A separate constant-lane
test uses the coefficients' unity DC gain as its oracle rather than generating
expected values with the convolution under test. Actual native runs agree
word-for-word; no CPU arithmetic change was needed.

The checker pins the resident image, live `2468..24d3` filter/mixer bytes, own uploaded
entry and the complete own `0590..0735` payload. The capture is intentionally
bounded: its counter sees 496 filter stores within the first 1,024 inputs, but
only the first 128 output **values** are qualified. It does not validate RF
timing, an RSSI result, FCCH detection or later data after an overrun.

### Qualified quarter-rate accumulator prefix

The resident `249f..24d2` accumulator multiplies successive complex samples
by `1,-j,-1,+j`, then sums groups of four (observed mode 0), or eight (static
mode-1 branch). Eight live mode-0 blocks, all 32 output words, match an
independent complex rotation and the native 72-word ring-pointer updates.
Mode 1 has a logical reference test, not a native execution qualification.

For the same physical +67.708333kHz tone:

| Scalar divider | First adjacent native accumulator outputs (signed A,B) | Adjacent-group dot product |
| --- | --- | ---: |
| 6 (four complex samples/symbol) | `(444,-2232)`, `(-444,2232)` | -5,178,960 |
| 12 (two complex samples/symbol) | `(804,-1936)`, `(804,-1936)` | +4,394,512 |
| 12, zero input | `(0,0)`, `(0,0)` | 0 |

This selects divider 12 for the **laboratory baseband** on signal evidence,
not because it hides an overrun. With the proved decimation by two, a quarter-
rate FCCH tone becomes stationary at the native quarter-rate mixer only when
its output runs at the GSM symbol rate. The known FCCH relationship is also
described in the primary [FCCH detector patent US7590419B2](https://patents.google.com/patent/US7590419B2/en).
The inference assumes unshifted complex baseband at the ADC fixture; it does
not identify an actual MFI wire clock or exclude an undiscovered hardware NCO.

The original 64-word six-second divider-12 runs reconcile 975,264 tone and 1,340,800 zero
P27 reads to clocked queue consumption; each independently checks a bounded
4,096-word prefix. Both retain the initial overrun at 2.548639385s. Changed
measurement activity after that fault is not an accepted RSSI/FCCH result.
MFI bank/acknowledgement semantics
(notably P21), COBBA control gating, capture edges and P11=`01fe` remain the
next boundary. Retire the direct-ADC compositions and field probes when a
focused clocked-air gate covers the recovered contract.

The native read-side control sequence sets P21 bits 10/11 at uploaded
`1863..186a` and clears bit 11 at resident `25bc..25c3` on exit. These live
bytes are checked against the own upload and resident image. Bit 11 therefore
brackets the software read interval; its physical meaning (bank ownership,
acknowledgement, clock control or something else) is not established. Do not
treat the current opaque P21 latch as a recovered MFI handshake.

### Service-gap staging bound

A passive first-window trace has 11,648 P27 reads, 794 control transactions
and 362 read gaps above half a block period, without hitting its cap. The
fault lies in a **55.934us** read gap, from 2.548587824s (after read 11,616)
to 2.548643758s (read 11,617). It includes COBBA register-8/9 transactions
while INTM is set; P21's software read-interval bit is clear. The program-fetch
mask observations are samples, not exact instruction-ownership or idle-state
proofs.

For the declared uninterrupted producer, first source tick 32,992,152 and
scalar divider 12 imply 11,685 produced words just before that next read:

```text
backlog before next read = 11685 - 11616 = 69 words
smallest 32-word-rounded staging limit covering this bound = 96 words
```

`continuous_queue_bound` independently accounts for nanosecond log rounding;
both bounds are 69. No new 32-word notification would occur between the old
overflow and this read (the next boundary is 11,712), so this local demand
calculation does not assume an extra interrupt or a changed CPU path. A
different physical capture gate or banking policy could change that demand;
69 is not a lower bound on silicon RAM independent of this producer model. A
P21.bit11 service-hold/re-notification candidate with the original 64-word
queue fails at the identical first overflow and was removed; it establishes
no physical acknowledgement semantics.

The single pre-registered **96-word laboratory staging** trial completes
matched three- and six-second tone/zero runs with no raw transport faults,
and reports a peak of **69** in every run, matching the independent bound.
No samples are overwritten, discarded, padded or demand-generated; all P27
reads still reconcile with clock-produced queue consumption. The 96-slot
backing is saved/resettable, and focused tests exercise full wrap/provenance
and snapshot copies at that limit as well as the default 64-word overflow.

This unblocks native signal-processing experiments, **not hardware receiver
qualification**. It neither proves that silicon has a 96-word FIFO nor that
all future workloads fit it. The fixture reports provisional limit/peak and
keeps `physical_buffer_capacity_qualified=false`; a later overflow is a new
contract question, not permission to keep increasing storage. Current MFI
bank/acknowledgement and capture gating remain open. The raw first-window
trace and its read-only observer are retained at
`tmp/rom6-adc-lab/window-nuza6oxg`.

### Qualified raw logarithmic measurement

The optional read-only measurement observer joins Execute entries in resident
`4315..4347`, the return to `242f` and the subsequent raw `022b` store at
`2431` (bus context `2432`). The live code and own return hook are pinned;
the observer neither calls the routine nor changes any firmware value.

```sh
.venv/bin/python tools/run_rom6_adc_lab.py --machine nsm3adc --measure
.venv/bin/python tools/run_rom6_adc_lab.py --machine nsm3ad0 --measure
```

The 64-word data-ROM table at `b09e` independently equals
`floor(640 * log10(index))` for indices 1..63, with zero at index 0. For a
positive 40-bit input `P`, the routine uses its six leading significant bits:

```text
e = max(0, bit_length(P) - 6)
m = P >> e
log_q16 = (floor(640*log10(m)) << 16) + e*(1541 << 13)
```

Nonpositive input follows the decoded passthrough branch, not a logarithm of
zero. Calibration either subtracts a signed direct value scaled by 64, or
selects a signed Q16 correction from `0474`/`049c` plus the caller's index.
The native arithmetic is therefore in **1/64-dB units relative to firmware
calibration**, not independently established physical dBm.

In paired fault-free six-second runs, all 32 captured conversions per input
match the full 40-bit results. Each run also has **15** joined raw measurement
stores. For those calls, ADC epoch ticks determine the analytic tone phase;
independent convolution predicts exactly the native input powers
`274576`, `274657` or `275282` (zero for silence), not merely a range or a
nonzero result. The observed table-calibrated raw words are signed **-551**
for the tone and **-7020** for zero. Only the `0474` calibration branch is
native-qualified by these captures; the other branches have logical vectors.

### Qualified frequency-correction prefix

The same observer captures 16 calls to resident `71a3..71d7`, joining all
15 qualified raw RSSI stores to their subsequent correction. Each paired
tone/silence run covers channel keys 0..15: eleven stores (including the
zero-delta boundary at key 10) and five interior no-store returns.

The signed descriptor at `3ea1..3ea4` contains lower/upper channel boundaries
and left/right slopes: `(10, 90, 472, 722)` in these runs. Outside the strict
interior, the routine adds `(signed_delta * signed_slope + 128) >> 8` to the
signed raw word, wrapping the result to 16 bits. The first tone value changes
`fdd9` to `fdeb`. Flags `051f.bit7` select alternate-band handling: keys
125..974 use descriptor `3ed6..3ed9`; keys 975..1023 are interpreted as
negative extended channels relative to 1024 and use the main left slope.
Those alternate/right branches have logical vectors, not native coverage in
this bounded prefix. The checker pins the correction code, checks arithmetic
mode, and requires the predicted write or no-write behavior at each return.

This qualifies the **observed corrected RSSI-field prefix**, not its final
response. The MCU-bound measurement reply is the next unresolved join.
Response-packet, physical-dBm, FCCH and camping flags remain false. The
direct-ADC source still does not establish a tuned RF carrier or physical
MFI capacity.

### Native MCU-bound publications under ADC-pin stimulation

The measurement observer also records actual DSP writes to receive-ring
producer `08e4`, retaining the preceding cursor, consumer, and committed
words. The checker validates cursor continuity, wrapping and packet lengths;
it never drains or fills the ring. In the paired six-second trials there are
five commits, all from bus context `2d08`: four type-`74` packets with payload
prefixes `340e`, `3532`, `0d00` and `0a090006`, then type `8a` with payload
`0004`. The last packet arrives at 2.687767604 seconds for the analytic tone
and 3.143782044 seconds for silence. Their packet bodies are identical.

There is **no type `83` or `8b` RSSI-results publication in this bounded
capture**. Internal corrected RSSI fields must therefore not be presented as
a qualified MCU RSSI response. Type `8a` belongs to the `NO_PSW_FOUND` family
in the mapped MCU decoder; the observed negative result is not FCCH success.
Whether and how the corrected fields affect the native search decision is
the next join. The capture does not yet independently qualify MCU packet
consumption, nor prove that RSSI replies cannot occur in another lifecycle.

### Native search correlation products

`run_rom6_adc_lab.py --detect --machine nsm3adc` (and `nsm3ad0`) uses a
separate bounded observer for resident `4146..423a`. Its first initialization
window computes seventeen complex products via `422b..423a`. All seventeen
captured calls match independent signed dot/cross products of their four
actual input words, with the resident code, own entry hook and arithmetic
mode checked. In observed mode 0, for `(x0,y0,x1,y1)`:

```text
A = x0*x1 + y0*y1
B = x1*y0 - y1*x0
```

The tone supplies `(804,-1936,804,-1936)`, yielding `(4394512,0)` every
time; silence yields `(0,0)`. This extends the qualified FIR/mixer prefix
into the native search computation without substituting a detector verdict.
Sixteen subsequent normalized-metric returns per input also match an
independent integer ratio of the observed correlation sums and energy. For
the observed nonnegative-energy, nonsaturating mode-0 domain, normalize all
three inputs with `s = 31 - bit_length(energy)` (zero uses `s=0`), take signed
bits 32..16 after shifting by `s-9`, square them, and compute:

```text
denominator = max(1, normalized_energy_squared & 0xffff)
numerator = 1024 * (normalized_real_squared + normalized_imag_squared)
quotient, remainder = divmod(numerator, 2 * denominator)
native_accumulator = (remainder << 15) | quotient
```

The tone has correlation `(74706704,0)`, energy `19775304`, denominator
`1369`, numerator `20647936`, and native accumulator `01531d75`: metric word
**7541**, above both stored thresholds `0405` (**1029**). Silence returns
zero through the denominator-one guard. This validates ratio arithmetic for
these actual inputs, not independent generation of the entire energy window.
The search's consumption of the positive metric, finite-burst requirements,
other detector modes and FCCH success remain unqualified. In particular a
continuous tone is not a complete GSM FCCH/SCH waveform; whether its missing
burst edge prevents progression must be tested rather than assumed.

The detector observer reserves debugger
temporaries independently of the measurement observer; the base observer's
`temp9` is reserved for ARM self-test pointers and must not be reused as a
DSP counter.

### Uploaded search threshold and continuous-tone rejection

The actual consumer is the handset's uploaded `0b40..0d7f` code, not the
resident `40bc` wrapper. Its 1152 bytes match MCU offset `1170b0` and SHA256
`245f696206a75730ab96ea480d5927b1b194533947d8fb732f057d4dbb3ecfb0`.
The `0b92` call returns the metric to the `0b94` comparison against data
`3eaf`, whose observed value is **`0574` (1396)**. Read-side observations at
bus context `0b9a` join all sixteen qualified metric returns to the actual
threshold operand. Tone `7541` is above it; silence `0` is below it.

The uploaded positive path increments `3044` at `0b9d`; `0bc3..0bc7` branches
to failure `0d68` when that count reaches **500**. The continuous-tone run
has exactly **500** threshold reads, while silence has **15938** before its
later negative packet. Thus continuous tone already clears the actual
metric threshold, but triggers a duration/count guard. This supports testing
an independently timed **finite FCCH burst**, not raising signal amplitude,
changing thresholds or injecting a success result. The bounded first-32-read
capture does not claim that every later metric was individually recorded;
the guard interpretation combines the pinned uploaded code with total read
counts and the observed native negative packets.

### Finite FCCH ADC-pin fixture

`nsm3afb` is a separate **laboratory** composition, not another supported
handset or a tuned RF source. Its ideal rectangular tone follows
[GSM 05.02 v5.1.0 section 5.2.4 and table 3](https://www.etsi.org/deliver/etsi_gts/05/0502/05.01.00_60/gsmts_0502v050100p.pdf):
148 zero bits including tails, equivalent to a +1625/24-kHz offset, in TS0
of frames 0/10/20/30/40 of each 51-frame multiframe. With the declared 13-MHz
reference, a frame is 60000 ticks and the active interval is 7104 ticks.
The waveform's epoch is absolute zero, independent of firmware requests,
capture enable and FIFO reads. Q remains latched from the preceding I
conversion even across an envelope boundary.

```sh
.venv/bin/python tools/run_rom6_adc_lab.py --machine nsm3afb --seconds 6
```

Other slots contain zeros: no SCH, BCCH, BTS power envelope, RF gain/noise or
HAGAR-selected carrier is implied. An isolated C++ test checks all five
bursts, 296 complex samples per active interval, exact edges and multiframe
periodicity. The independent Python trigonometric reference checks delivered
sample provenance, including the latched-Q boundary. Existing continuous
tone and silence compositions are unchanged.

The finite waveform advances beyond the continuous-tone path. After the
independently tested CPU alias fixes, it reaches uploaded sample-processing
helper `1cc5..1cff`. Those 118 bytes match MCU offset `11681a` and SHA256
`5d7bf683dab9d7869f16ff09ae114740d11c7c694b9f5e678e11433fac6ed3ba`.
The helper deliberately uses mapped AR6/AR7 addresses as scratch extrema:
`1cc6/1cc8` initialize AR2=`0016`, AR3=`0017`; its conditional stores update
these registers before computing their sum.

The mapped AR7 store at `1ce7` now uses the independently tested generic
Xmem-register path. The current six-second trace has 702272 total native
reads, peak queued 69 and no transport fault; all 4096 retained source
words match the independent waveform.

### SCH training-correlation identification

The finite stimulus now reaches resident `76b0..76cf`. Its program words at
`7737..773a` are **`b962 040f 2d45 761b`**, exactly the 64-bit extended
training sequence in GSM 05.02 section 5.2.5, encoded MSB-first. This is an
independent protocol anchor, not a sibling-emulator label. The routine reads
each mask through READA into AL, then tests its high bit and shifts it to
choose the sign of successive complex sample components over four groups
of sixteen iterations.

The mapped AL BIT read and subsequent single-repeat paired addition now
use independently tested generic CPU paths. The unchanged six-second
waveform also passes `RPTZ A,#9` at `77b0`, inside block `77b0..77ba`,
preceding `MAC *AR3+,*AR2+,A,A` at `77b2`. Eighteen generic nested-repeat
cases check re-zeroing and independent outer/inner counts with replay;
192 existing block/single-repeat controls pass.

The mapped BRC-to-AR copy at `9007` (`7212 001a`) now follows the generic
retained Read path, before the routine replaces BRC with 6 and stacks AR2.
The current waveform remains **rejected** at `9018`, opcode `8e93`:
`CMPS A,*AR3+`. Displayed `901a` is not the rejected instruction.
The trace still has 702272 native input reads, peak queue 69 and no
transport fault. Terminal AR3 is `3218`, A is `fffffffe00`, B is zero.

[TI SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf),
CMPS description (4-35), identifies `8e/8f` as compare,
select and store maximum: compare the signed high/low sixteen-bit halves
in **Read**, retain the larger half for the memory store, and update
TRN/TC in Execute (low wins ties). This timing rule is explicit in the
manual, not inferred from this firmware. The paired `8e93`/`8f94` calls
are after dual-lane arithmetic in the routine; this alone is not proof
of a decoded SCH payload.
The corresponding short/extended-immediate and RAM-count DADD tests check
every intermediate result and paired read across 72 save/replay cases;
the 8192 ordinary paired-arithmetic controls also pass.

Reaching this standard-pattern correlator and its later processing does
not prove FCCH acquisition, SCH synchronization, decoded frame number or
camping, particularly because the stimulus contains no SCH burst. Next:
implement and independently qualify CMPS with Read-owned accumulator
comparison, signed halves/ties, memory addressing, TRN/TC publication
and save/replay, then rerun the unchanged waveform.
