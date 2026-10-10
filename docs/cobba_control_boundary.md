# COBBA control boundary

## Current result

The current COBBA device models a 16-address, 12-bit control-register file and
the analogue conversion endpoints. This is not a claim that every COBBA
revision uses the same physical bus. MAD2 owns the typed PCM wire. The DSP backend
owns the control policy. The MCU ROM does not directly address COBBA,
and the current DSP HLE does not fabricate COBBA control writes.

Consequently, microphone/output selection and gain remain explicit HLE profile
data. They are not decoded COBBA register semantics. The production driver
must not translate MCU call state or Nokia mailbox values directly into COBBA
register writes.

Nokia's NSE-1 System Module manual (03/98, pp. 3-11, 3-32 and 3-41)
distinguishes the physical interfaces: the parallel connection has twelve
data lines, four address lines, read/write strobes and a data-available signal;
audio PCM has separate serial data, clock and frame-sync wires. The parallel
connection carries control and transmit/receive samples. See the
[Nokia manual text](https://www.eserviceinfo.com/preview_html.php?fileid=26879&previewid=13251).
The recovered DSP control-word packing is not evidence that this physical
control bus is serial. Nor does its twelve-bit width establish how MAD2
presents a sample at DSP I/O port `0x27`: address selection, sign extension,
word ordering and readiness still require a mapping or timing capture.

### NSE-1 synthesizer boundary

Nokia's NSE-1 System Module manual (03/98, pp. 3-50--51 and 3-61--63),
available as a [complete manual reproduction](https://www.scribd.com/document/74806191/41-Mobile100-Technical),
separates PLUSSA tuning from COBBA sample/control traffic. The synthesizer
bus uses SDATA, SCLK and SENA1; SCLK is 3.25 MHz and SENA1 latches the
transferred word. The startup diagram lists five 23-bit transfers: mode,
VHF reference divider, VHF N/A, UHF reference divider and UHF N/A.
The UHF PLL uses a 64/65 prescaler and 200 kHz reference/channel spacing;
the VHF PLL uses a 16/17 prescaler. The divider relation is `M = N*P + A`.

These are physical constraints, not decoded MAD2 SCU registers. In particular,
the observed DSP port-31/32 pairs are not established as these 23-bit words.
The native MCU upload and resident accumulator packing are mapped below;
physical bit order, address mapping and timed latch mechanism remain unknown.
A future tuning model must connect
an evidenced software write path to this bus rather than infer an ARFCN
from arbitrary COBBA control values. RF sample encoding remains a separate
unresolved MFI contract.

The resident destination branch `0x401a..0x402d` is byte-pinned:
`*AR2 == 0x31` sends AL to port 31 and AH to port 32, then advances
AR2 by two; the other branch stores accumulator halves in memory.
The packing helper `0x403a..0x4048` is separately byte-pinned, but its
input's physical units are not established. Fresh native zero-input
and rotating-input controls capture the selected `006d` entry alongside
AR2=`0031`, flags `0x216a=0004`, field `0x0060=0050` and
accumulator `00302813`. Port 31 receives `2813` (advanced PC `4023`),
then port 32 receives `0030` (advanced PC `4028`). Thus these two writes
are one accumulator's halves, not two independent register addresses.
Other writers use different PCs (`a23c/a240`, `370e/3712`); the port
pair is not exclusively owned by this resident helper. The bounded
observer records the first 32 writes without changing their values.
`check_tuning` validates the selected-entry pair and accumulator halves;
it does not establish PLUSSA's 23-bit layout, clock/latch timing or RF
frequency. Those require tracing the helper's inputs and programming owner.

For the captured selected entry, the byte-pinned scan caller at `0x0a84`
loads `*AR4` into A and delayed-calls `0x3ffb`, setting AR2=`0031`.
The bounded native stage capture observes entry A=`006d`,
`0x06be=0010` (bit 7 clear), `0x216a=0004` and both addend fields
`0x216b/0x216c=13a6`. The `0x3ffb` path selects `0x216b`; at
`0x400f` its B operand is `13a6`, and at `0x402e/0x403a` A is
`1413`. Flag bit 1 is clear, so the `0x4049` transform is skipped;
bit 2 selects `0x402e`, whose bit-7-clear path does not double A.
For this nonnegative branch, `0x403a..0x4048` takes quotient and
remainder at 64, packs `(quotient << 7) | remainder | (0x30 << 16)`
and leaves the quotient in `0x0060`. Here quotient `0050`, remainder
`0013` produce `00302813`, matching the returned A/B and port pair
in both fresh controls. `check_tuning_stages` checks the ordered
operands, branch flags, quotient and result; its arithmetic reference
is not a native basis test or proof of alternate signed/doubled paths.
The resident hook defaults `0x244a/0x244c` are return/NOP sequences
in the checked image. Candidate direct-call scanning finds callers
`0x099d` and `0x0a85` for `0x3ffb`, none for `0x4006`; this is
raw direct-call coverage, not closure over computed calls or patches.
The checked initial DSP data image holds zero in `0x216a..0x216c`.
Fresh zero/rotating controls observe data-space clearing writes at advanced
PC `0x0f15`, then program-overlay writes at advanced PC `0x31d7` around
0.134044 seconds: `0x216a=0004`, `0x216b=13a6`, `0x216c=13a6`.
The byte-pinned loop at `0x31d4..0x31d9` copies data through AR2 into
program space; the backend's active overlay maps those destinations to
the same data storage. Observed AR2 advances `0931,0932,0933`. A
data-space-only watch therefore misses the load-bearing upload writes.
`check_tuning_upload` requires the native copy-loop tuple before the
already-validated programming stages. The original NSE-1 v5.30 MCU image
contains the tuple at file offsets `0x095450`, `0x098694`, `0x0998a0`;
each has nearby destination word `216a`. Native MCU reads identify the
selected descriptor as CPU address `0x299894` (raw offset `0x099894`,
catalogue record 26). At `0x272122`, firmware reads its five header
halfwords `216a,1e80,011c,0130,0078`; at `0x2936f0`, it reads payload
words `000413a6` and `13a62a04` from `0x2998a0/a4` before the DSP
overlay writes. Big-endian ARM decoding identifies `0x2936f0` as the
source LDM in the copy loop, not a Thumb instruction. Other blob copies
are read later at PC `0x276e88`; those reads do not establish upload
selection. `check_tuning_blob` checks the selected header, payload,
read masks and ordering before the native upload in both fresh controls.

Static Thumb decoding at `0x2720f8..0x272142` explains the selector:
request index 2 can be remapped by `0x28dd00` to index 11 (return 2)
or 26 (return 3). It then indexes the descriptor-pointer table and copies
five halfwords to the shared header, derives the staging pointer from
descriptor offset 6, and advances the source by 12 bytes. The live
descriptor address establishes the selected record. Static decoding of
`0x28dd00..0x28dd20`, with pools `0x28ddec=0002000e` and
`0x28ddd8=00020000`, identifies its inputs as MAD2 external-status
byte `0x2000e` bit 3 and PUP GenIO byte `0x20020` bit 1. Both clear
select return 3; otherwise the status bit chooses return 1 or 2. This
names the register predicates, not their external pin functions. The values are
firmware-uploaded configuration, not a backend-invented addend. Physical
the selector's input semantics remain unresolved. The numeric physical
consistency test below does not replace the missing port-to-wire contract.

#### Frequency-plan consistency

The [Nokia NSE-1 manual](https://www.scribd.com/document/74806191/41-Mobile100-Technical)
(03/98, pp. 3-48, 3-51--53) specifies a 64/65 UHF prescaler with
200 kHz reference, upper-side receive injection at 71 MHz IF and
116 MHz transmit IF. [ETSI TS 45.005, section 2, table 2-2](https://www.etsi.org/deliver/etsi_ts/145000_145099/145005/11.03.00_60/ts_145005v110300p.pdf)
gives P-GSM 900 carriers `935 + 0.2*n` MHz receive and
`890 + 0.2*n` MHz transmit for channels 1..124.

**Inference:** the recovered addend `13a6` is 5030; at 200 kHz that
is 1006 MHz. Adding selected entry 109 gives divider 5139, whose
64-based quotient/remainder are 80/19, exactly the captured packing
`00302813`. Its inferred LO is 1027.8 MHz, simultaneously equal to
channel-109 RX 956.8 MHz plus 71 MHz and TX 911.8 MHz plus 116 MHz.
Thus two independent IF relationships support a channel/divider
interpretation, rather than an arbitrary numeric coincidence.
`nse1_gsm900_tuning_consistency` checks this relation in integer Hz;
unit coverage of all 124 channels is reference arithmetic, not native
execution coverage. Cold-run summaries label it
`numeric-consistency-inference` with `wire_validated=false`.

This does not prove that the DSP pair is serialized unchanged onto
PLUSSA, decode control bits `0x30`, establish settling/readiness, or
validate sample cadence/encoding. Keep it out of the operational hardware
decoder until the port-to-wire path is evidenced. No DCS/PCS/extended-band
mapping or native acquisition follows from this bounded P-GSM result.

#### Transfer boundary audit

The NSE-1 manual (pp. 3-42 and 3-51) identifies MAD2's SynthEna,
SynthClk and SynthData outputs connected to PLUSSA. It does not give
the DSP I/O register mapping or the serializer's latch/queue semantics.
The checked program-image candidate census is now regression-protected:

| DSP I/O port | Candidate reads | Candidate writes |
| --- | --- | --- |
| `30` | 0 | `0a94,30b5,4565,6976,a17c` |
| `31` | 0 | `36f7,370c,4020,a23a` |
| `32` | 0 | `36fb,3710,4025,a23e` |

All thirteen candidate writer instructions are covered by byte-pinned
sequences, including the existing `rf_slot_queue_consumption` contract.
This is coverage of the checked image, not every future upload, other
port status register or computed entry. There is no busy poll at these
three ports in that image. Do not infer that hardware has zero latency,
no queue, or no readiness signal elsewhere.

The fresh controls capture initialization write `port30=1` at advanced
PC `4567`, followed by repeated `port30=1` at `30b8`. Static sequences
also write zero at `a17c` and use an indirect data operand at `0a94`.
Port 30's meaning is still opaque: it is not established as a transfer
trigger, reset or enable. Trace caps are per port because these repeated
early writes otherwise hide the later selected-entry pair. The native
backend currently retains all three as saved latches; no physical serial
transfer or settling timer exists. The next evidence needed is the SCU
register/serializer contract or an independently captured pin transfer,
not a guessed busy bit or a replayed firmware result.

#### Sibling RF documentation

The [Nokia RAE-2 service manual](https://www.scribd.com/document/969092511/nokia-9110-rae-2-1),
BS8_RF pp. 3-35..3-36, explicitly specifies the GSM channel-1..124
UHF counters as `N=INT((channel+5030)/64)` and
`A=MOD((channel+5030)/64)`, with reference divider 65. This independently
corroborates the recovered NSE-1 arithmetic; it does not validate its
physical transfer.

That board uses SUMMA, not NSE-1's PLUSSA. Its external serial protocol
places a three-bit address before register data; address 3 selects the
18-bit UHF divider. The adjacent timing diagrams describe 23-bit
transfers. These are sibling-device facts, not a license to interpret
the captured `00302813` as an unchanged NSE-1 wire frame. Neither the
external register table nor the timing diagrams map MAD2 DSP ports
`30..32` to serial launch, length, latch or completion behavior.

The bounded public-document search therefore supplies a stronger
divider interpretation, but leaves the SCU serializer contract open.
Do not invent a transfer timer or control-bit decoder from the sibling
format. Native acquisition still needs that contract and independently
supported RF sample timing/encoding.

### Software RF reference boundary

An independent baseband waveform source is available in
[OsmoTRX's signal-processing implementation](https://github.com/osmocom/osmo-trx/blob/master/Transceiver52M/sigProcLib.cpp).
Its `modulateBurst` accepts burst bits and produces a shaped complex
GMSK signal; `generateDummyBurst` supplies a standard dummy-burst
input. The [OsmoTRX manual](https://downloads.osmocom.org/docs/osmo-trx/master/osmotrx-usermanual.pdf)
describes device-specific samples-per-symbol settings. These are
waveform references, not Nokia register or sample-packing specifications.

The software-only experiment must keep two contracts separate: generate
known baseband I/Q with an independently validated implementation, then
adapt it to the recovered Nokia sample lane/order, numeric scaling,
clock cadence and tuning selection. None of those adapter properties is
established by selecting OsmoTRX's four-sample setting. Do not transplant
reference code into the BSD driver; use independently generated fixture
data with recorded provenance and licensing. First validate the adapter
against the resident filter's reads and arithmetic, then test acquisition
with physical-interface input only. A passed energy threshold alone is
not decoded FCCH/SCH, native registration or native speech.

The NSE-1 resident scan filter is now byte-pinned in the ROM4 contract
checker. Setup `0x32f4` selects a 16-word circular history, input cursor
`0x0080`, history cursor `0x0084`, coefficient pointer `0x21a9`,
AR0 stride 2, ASM zero, and eight loop iterations. Loop
`0x330a..0x3325` performs four port-`0x27` reads per iteration and
stores two high-accumulator output words through AR3 into the
`0x1a48` intermediate area. Thus this consumer reads 32 input words
and emits 16 intermediate words per invocation. This is a software
word-consumption contract, not proof of four samples per GSM symbol,
which lane is I/Q, ADC sign extension, or continuous physical cadence.
A fresh provisioned eight-second native run captures the first two loop
entries at about 1.6402 seconds with ST0=`0x181f`, ST1=`0xe900`, and
coefficient words at `0x21a9..0x21b0`:
`0387 11a3 2ad6 4000 4000 2ad6 11a3 0387`.
In the current C54x core, ST1 bit 8 (SXM) is set, bit 6 (FRCT) is clear,
and bit 9 (OVM) is clear. These are bounded live observations, not a
claim that every filter invocation uses the same arithmetic state.
The passive `c54x_rom4_mode_observe.lua` observer reproduces this capture.
Its bounded operand census observes history reads in order
`84,85,87,86,88,89,8b,8a,8c,8d,8f,8e,80,81,83,82`
at DSP PCs `0x330d..0x3322`, followed by a read of `0x0084` at
`0x3323`. The coefficient reads advance through `0x21a9..0x21b0`;
these data-tap PCs are the core's advanced execution PC, not a claim
that the operand's instruction starts at that address. An expanded
coefficient probe covering `0x21a0..0x21b8` observes the same prefix,
with no ninth coefficient read. A live 32-word program capture at
`0x330a` matches the byte-pinned resident sequence; the program probe at
`0x3322` reads opcode `0xe4e9`, AR4=`0x0084` and AR5=`0x21b0` on its
first invocation (AR4=`0x0088` on the second). Do not interpret the
operand trace PC as the start of a supposed extra MAC instruction.
the census excludes the observer's own memory reads. This demonstrates
interleaved circular-history consumption in the executed core, but does
not assign physical I/Q names to either lane. The capture uses zero RF
input and is not an impulse-response or nonzero arithmetic test.
Validate a captured log with
`tools/c54x_rom4_scan_filter_check.py /path/to/error.log`. The checker
requires both bounded state captures and the exact 32-read prefix,
including coefficient values; it deliberately allows nonzero history
values so it does not encode silence as an RF-format requirement.
The diagnostic `c54x_rom4_rf_impulse.lua` supplies a single configured
word through the I/O read boundary at port `0x27`, followed by zeros.
It never writes firmware RAM, registers or messages. Its external event
starts at emulated time 1.6402 seconds (a fixture alignment, not physical
timing); the first supplied word is consumed at PC `0x330c`. Output
observation excludes earlier initialization and filter writes.
With `ROM4_RF_IMPULSE_INDEX=0`, the first 16 output words have nonzero
values only at indices 1,3,5,7: `0469 1000 0ab6 00e2`.
With index 1, the same values appear at indices 0,2,4,6 instead.
The isolated runner `run_c54x_rom4_rf_impulse.py` verifies all 32 input
positions with both `0x4000` and `0xc000` (-16384), using a fresh
generated EEPROM and independent run directory per case. All 64
eight-second cases pass against 48 captured output words, including
tails crossing an invocation boundary. For input position n, the
output starts at `2*floor(n/4) + 1 - (n mod 2)`, with stride 2.
Its four coefficients are entries `phase, phase+2, phase+4, phase+6`
of the captured table, where `phase = 1-floor((n mod 4)/2)`.
Each expected word is `((signed_input*coefficient + 0x8000) >> 16)`
modulo 65536. Thus adjacent words feed separate, oppositely ordered
output lanes, and the two tested signed amplitudes agree with rounded
fixed-point multiplication in the executed core.

Reproduce with `.venv/bin/python tools/run_c54x_rom4_rf_impulse.py
--mame mame/mame --rompath mame/roms`. This validates the numerical
firmware/core contract for impulses, not arbitrary-input linearity,
saturation, continuous sample cadence, physical ADC scaling or which
lane is I versus Q. The Lua override is a diagnostic port-input
fixture, not a shipped RF model. Those remaining physical mappings
must be explicit assumptions in any independent waveform experiment.

The downstream queue helper is not adequately described as an energy
detector. Byte-pinned `0x47e6..0x47f5` reads previous pair `(x1,y1)`
from `0x1252/1253` and current pair `(x2,y2)` from `0x1254/1255`.
With TC clear, it forms A=`x2*x1 + y2*y1` and
B=`x2*y1 - y2*x1`, subject to the core's multiply modes.
These are adjacent-pair dot/cross products, consistent with a complex
lag correlation without identifying which physical lane is I.
TC set selects a different, reduced path and copies the current pair
back into the previous-pair storage; do not apply the TC-clear formula
to that path. The byte-pinned result tail `0x4778..0x4797` normalizes,
squares and combines accumulated terms, then performs 15 SUBC steps
against a nonzero denominator. Accordingly the uploaded caller's
threshold comparison is a processed correlation-score boundary, not
an established raw ADC-energy or dBm threshold. Exact score units and
the active TC-mode interpretation remain to be validated numerically.

`c54x_rom4_rf_score.lua` supplies three explicit mathematical controls
at port `0x27`: zero; DC word pairs `(2048,0)`; and rotating word pairs
of amplitude 2048 with one turn per sixteen pairs. Pair order and
normalized frequency are diagnostic assumptions, not GSM sample-rate
or physical ADC claims. It observes the uploaded score at `0x0ad1`,
the second comparison at `0x0add`, and accepted-value store `0x0ae4`.
Fresh generated-profile 60-second runs produce:

| Input | Completed scores | Nonzero | Maximum | Second comparison | Accepted store |
| --- | ---: | ---: | ---: | ---: | ---: |
| Zero | 5158 | 0 | 0 | 0 | 0 |
| Rotating pairs | 5146 | 5146 | `0x7fff` | 500 | 9 |

A separate 60-second DC run using storage from its preceding eight-second
run has 5568 completed scores, all zero, with no second comparison or
accepted store. Its counts are not a fresh-boot oracle. Validate captures
with `tools/c54x_rom4_rf_score_check.py /path/to/error.log`; the checker
requires sustained observation, not the single completion in eight seconds.
The rotating input therefore advances the actual native score/selection
path without firmware-state or message injection. Saturation at `0x7fff`
is an observed score ceiling, not proof of correct physical RF modeling.
No control contains a GSM burst, FCCH/SCH timing or broadcast information;
these results establish neither acquisition nor registration. Next recover
what the accepted selection publishes and attach an independently generated
waveform only with explicit lane, scaling and cadence assumptions.

The fresh rotating-input selection trace reproduces 5146 scores and
nine stores at `0x0ae4`. Those stores increase `0x198d` through
`060c,0d62,17e5,250e,3589,4885,5efa,77c9,7fff`; AR4=`0x1257`
and AR5=`0x1266` throughout. Live uploaded code after that store
maintains candidate-associated records, counters and flags; it does
not immediately call the MDI publisher. The traced exit `0x0ca3`
executes twice: once during setup and once at 38.52345 seconds with
score=`0x7fff`, flags at `0x1266`=`1`, and count at `0x1344`=`500`.
Its live instructions are `CALL 0x44b6; RET`. Follow-on routine
`0x0cef` executes zero times. The captured decision stream tests
`0x1266` bit 1 before branching toward the follow-on path; accepting
ever-better scores does not establish that second flag. Bit 0 is set
in the delayed-branch slot of the second comparison, including cases
that do not reach the accepted-value store; it is not an acceptance
predicate by itself.
Consequently these inputs prove candidate selection, not publication
of acquired radio data. The flag's decoded software producer is below;
its presence does not establish acquisition in this fixture.

The expanded live-code capture identifies an explicit bit-1 setter:
`0x0b78` performs a delayed branch to `0x0b42` with
`OR #2,*AR5` in its delay slot (`0x0b7a`, AR5 selected as `0x1266`).
The preceding continuation at `0x0b6a` clears A, pops AL, subtracts 8,
and branches to that setter if A is nonzero. A popped value of 8
takes the other path. This is a software continuation-value gate,
not evidence that a hardware interrupt must directly set the flag.
Its caller/return contract remains unresolved. The bounded write watch
captures setup clears and repeated bit-0 writes at advanced PC `0x0ae4`;
the first 24 writes do not contain bit 1. This bounded prefix is not
an exhaustive absence proof for all later flag writes. Trace the
continuation entry and its return-value source before attributing it
to RF acquisition, an external peer or a missing DSP instruction.
The fresh 60-second continuation census observes zero entries at
`0x0b6a`, the post-pop `0x0b6c`, and the setter branch `0x0b78`.
The unrelated/shared helper entry `0x2523` executes five times and
branches to `0x25fe`, also observed five times. Its first four captured
(A,B) pairs are `(13,1),(11,1),(12,2),(11,1)`; these are not recovered
arguments of the unexecuted continuation. The live helper bytes match
the raw ROM and are byte-pinned in the contract checker. Therefore the
current missing link is the continuation's entry/selection contract,
not an observed pop returning the wrong value. Search its code/data
references and active dispatch source before modeling an external event.
The live exact-word reference census checks 65536 words in each of the
program and data spaces. Each contains one `0x0b6a` word, at `0x0b59`
in the same uploaded sequence: `CMP #1,*(0x06fd); BC 0x0b6a,TC`.
The duplicate code/data occurrence is not two independent producers.
This recovered branch requires `0x06fd == 1`; the rotating-input run
has `0x06fd == 0`. It also follows the unexecuted `0x0cef` routine,
so mode selection is not proven to be the only missing prerequisite.
No other exact pointer word is present in the final live images, but
computed targets, earlier overwritten tables and other entry addresses
are outside this census. The next backward boundary is the upstream
candidate-validation branch reaching `0x0cef`, not an invented event
to activate the bit-1 setter directly.

The upstream rotating-input path reaches `0x0b00` 500 times and
`0x0b0b` 499 times, but `0x0b0d` and the entire captured history path
(`0x0b1e/24/2e/3a/42/4a`) execute zero times. At `0x0b0b` the live
code tests A after incrementing `0x1256` and subtracting `0x05f3`
(1523), branching back to the scan loop when A is negative. The
uncapped counter observation over all 499 checks ranges from 1 to
499; the separate `0x1344` counter exits at 500 before another check.
Thus the history-comparison and continuation gates are downstream of
an unsatisfied counter predicate in this mathematical-input run.
Do not infer a firmware bug or remove either guard: initialization,
other writers and product/request context of `0x1256` remain open.
Its initialization/write contract is recorded below; this guard does
not justify supplying an MDI reply or continuation return value.
The complete 60-second write census observes five writer PCs for
`0x1256`: loader `0x0f15` (1), reset `0x3074` (2), filtered-buffer
store at advanced PC `0x33e5` (2), uploaded reset `0x0ac7` (3), and
candidate-loop store at advanced PC `0x0b09` (499). Uploaded code
explicitly clears B and stores it to `0x198d`, `0x1256` and `0x1344`
before calling the queue helper. The scan-entry reset supersedes the
earlier filtered-buffer value `0xfc38`; it is not an input offset that
survives into this scan. The captured loop writes begin `1,2,3,4`,
and the uncapped comparison range remains 1..499. No other writer
executes during that candidate interval. This establishes the observed
lifecycle's counter constraint, not an exhaustive static writer census
or proof that all valid request/product contexts share it. Independently
audit the guard's decoding and uploaded request context before treating
the apparent 1523-versus-500 incompatibility as a firmware defect.
The independent encoding audit uses TI
[SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf),
SUB syntax 7 (pp. 4-187/188) and BC conditions (p. 4-18): `0xf010`
selects immediate subtraction from A with zero shift, `0xf843` tests
A less than zero, and `0xf842` tests A greater than or equal to zero.
Original NSE-1 v5.30 MCU flash contains the reset block at byte
`0x0959ec`, the 500-result guard at `0x095a68`, and the 1523 counter
guard at `0x095a74`. Their relative positions agree with the captured
DSP program layout. `c54x_rom4_codec_contract.py --mcu-image
roms/noki5110/5110f530.fls` verifies unique aligned source occurrences
and that common layout; mutation tests cover every checked word.
This rules out these constants being introduced by the live-code
observer or a swapped upload capture. It does not validate all core
pipeline effects or establish the request's intended physical context.
The scan-entry census observes `0x0a5c` and the reset at `0x0ac2`
three times each. At all entries active mode `0x1836`, pending mode
`0x1869`, and `0x06fd` are zero; A at `0x0a5c` is 12. The final
live program and data images each contain two exact `0x0a5c` words:
the restart branch at `0x0b69` and gateway target at `0x2421`.
The gateway is `GOTO 0x0a5c`, also present in the acquired image.
Resident sequence `0x3129..0x3133` clears `0x06fd`, calls `0x3198`,
then calls gateway `0x2420`, and sets `0x06fd=1` only after return.
Both resident sequences are byte-pinned. The runtime call/return census
confirms three entries at `0x3129`, `0x3130` and gateway `0x2420`,
with two returns to `0x3132`. Firmware writes `0x06fd=0` three times
at advanced PC `0x312c`, and `0x06fd=1` twice at advanced PC `0x3135`.
The returns occur at 1.644830615 and 38.523453018 seconds; the third
scan remains in flight at the 60-second stop. Thus the gateway-stall
hypothesis is falsified: the bounded scan can return without entering
the dormant follow-on route. Zero mode inside the scan is the selected
pre-return lifecycle, not an absent mode-setting hardware event.
Exact-word scans do not cover computed entries or overwritten tables.

The fresh 60-second rotating-input post-return capture observes both
returns through `0x3138/3a/3d/3f/42`, with `0x06e3=0` and
`0x1949=0`. Neither the host restart selection nor the operational-ready
bit is set. Both execute the fallback call at `0x314d` and continue at
`0x3151`; `0x3153/5c/6b` remain unexecuted. The fallback prepares a
two-byte type-`0x8a` body from data `0x1867` at buffer `0x1200`.
The observer at native publisher entry `0x37ce` captures type `008a`,
length 2, and bodies `006d` at 1.644835731 seconds and `0001` at
38.523456633 seconds. These are not the contemporaneous `0x1983`
cursor values (`03fe` and `047a`). Do not conflate those fields.
The byte-pinned `0x311a..0x311f` sequence loads AR4 from data
`0x1983`, then copies `*AR4` to `0x1867`. The fallback at
`0x3144..0x3152` copies `0x1867` into its two-byte publisher buffer.
A retained-storage eight-second capture confirms the selected-list
relationship: at scan entry `0x3129`, cursor `03ff` points to `006d`
and `0x1867=006d`. At fallback call `0x314d`, cursor `03fe` points
to sentinel `7fff`, while the selected word remains `006d` and is
published. The cursor changed position; this does not show the selected
entry being overwritten. These words are software search-list entries;
physical ARFCN interpretation and RF tuning must be tied to the request
producer rather than inferred from their plausible numeric values.

The native type-`0x1a` request captured at `0x3d5e` contains 33 words:
control `0010`, then `9800`, with `1000` at zero-based word index 26
and all remaining words zero. The byte-pinned receiver at `0x3d97`
copies the low byte of word 1 and words 2..32 into scratch
`0x1200..0x121f`. With control bit 0 clear, helper `0x48f7/0x4909`
walks those scratch words backwards, testing bits low-to-high. Its first
eight words select entries 1..128; the remaining 24 select 512..895.
The captured bit therefore selects entry 109 (`006d`), matching both
the live selected-list word and the native response body. This is a
static general mapping with one live single-bit corroboration, not an
executed native basis test. The later control `0011` request bypasses
bitmap expansion and must not be decoded by this rule.
`c54x_rom4_rf_delivery_check.py --search-list` checks this request-to-list
correlation; the isolated cold-control runner also requires it. Physical
band/channel interpretation still requires the MCU request producer and
RF tuning contract, independently of these numeric list entries.

Publisher entry alone proves call arguments, not delivery. A separate
fresh eight-second capture reaches the publisher's port-1 doorbell
instruction at `0x3807` for type `008a`, length 2, at 1.644837058
seconds, with producer `00ab`, consumer `00a9` and control `0x06e1=0`.
This establishes ring publication and notification for the first response;
the device's port-1 handler invokes the transport RX notification. A fresh
eight-second MCU-side write capture then observes the consumer advance
from `00a9` to `00aa` at 1.645136923 seconds (PC `00271908`), and to
`00ab` at 1.645150077 seconds (PC `00271998`). These are low-halfword
writes at aligned MCU address `000101c8`, mask `0000ffff`, selecting
the consumer halfword at `000101ca`. Thus firmware drains both header
and body of this native response. This proves ring consumption, not
application-level interpretation or successful RF acquisition.
DSP-local data-space write taps cannot
establish absence of MCU-side shared-memory consumer updates.
For NSE-1 v5.30, the first-level switch at `0x291c34` selects table
entry `0x291c70` for type `0x8a`; its corrected pool word targets
`0x291ca0`, which calls handler `0x26be28`. This is not the adjacent
`0x26bf18` handler. The native eight-second trace reaches `0x26be28`
at 1.645216538 seconds and its non-discard branch `0x26be3c` at
1.645217923 seconds, with controller state byte `0x108427=2`.
Static decoding shows state 1 frees the message; other states clear
bit 1 at controller offset `0x7c`, rewrite its status to `0x1390`
(pool `0x26c034`), and post the same object to task 10 via `0x254c84`.
The narrow post-call tap was silent; the wider capture below establishes
the actual post. At function entry R4 still belongs to the
caller; R0 is the message pointer, and the branch observation confirms
R4 becomes that pointer (`0x101438`).
An additional repeated run with retained storage observes the actual
header store at 1.599568385 seconds: trace PC `0x26be48`, aligned
object address `0x101438`, value `13900000`, mask `ffff0000`.
This independently confirms the status rewrite, not task-10 receipt;
its timing is not a fresh-storage baseline. A narrow read tap on the post
primitive did not fire even with its argument filter removed. A wider
`0x254c80..0x254cff` fetch window captures its actual entry: PC
`0x254c84`, R0=`10`, R1=`0x101438`, R14=`0x26be5d`, at
1.599574385 seconds in the retained-storage repeat. Thus the rewritten
native response is submitted to task 10's RTOS post primitive. The entry
generates fetches at `0x254c84/88/8c` with the same PC and arguments;
the observer retains only the first address to avoid triple counting.
This resolves the silent probe as an observation-boundary issue, not a
missing firmware publication. It does not establish receiver processing.

Static consumer candidate: the subtract cascade at `0x211cf2` uses
base `0x1389`; cumulative decrements 1, 2, 2, 1 lead to a range test
covering `0x138f/0x1390`, both targeting `0x211eb0`. That case calls
`0x269266` with the incoming object and length 8, tests the shifted
controller word at offset `0x7c`, and conditionally increments halfword
`0x10fee0` (pool `0x211f58`). The carry-producing shifts must be
decoded before assigning flag semantics. Task ownership and runtime
receipt at this candidate are not yet established: a 60-second retained-
storage repeat captures a second post from `0x26be5d` at 36.631877231
seconds, but no candidate-case observation. That silent window is not
an absence proof; confirm the actual task entry/receive loop first.
The contiguous 12-byte creation records begin at `0x2a4fa0`; record
10 at `0x2a5018` contains Thumb entry `0x211a55`. Entry `0x211a54`
calls initializer `0x20cb80` before reaching receive call `0x254ed8`
at `0x211a96`. The received object is stored at `[SP+0x18]` and its
signed status halfword is read at `0x211aa0`. Initializer `0x20cb80`
independently passes task number 10 to `0x23f130` and calls `0x211f6c`
with `(2,0,0)` before initializing controller state. This strengthens
task ownership beyond address similarity. A bounded receive-window
probe was uninformative and removed; first establish initializer
completion and the scheduler's actual receive return before diagnosing
an absent task consumer or introducing any readiness response.
The wider `0x211a00..0x211bff` window resolves that observation gap:
fresh storage executes task entry at 1.082075385 seconds, initializer
entry at 1.082077462 seconds and its return to `0x211a66` at
1.082957154 seconds. A retained-storage repeat then observes receive
return PC `0x211a9a` at 1.599812000 seconds, R0=`0x101438`,
status `0x1390`: the same object submitted by `0x26be28`.
`tools/c54x_rom4_rf_delivery_check.py` validates ordered construction,
two-word publication, both consumer advances, the header rewrite,
task-10 post and receipt with object identity and monotonic timestamps.
Its negative tests reject every omitted link, mismatched objects/status,
wrong masks/cursors and reversed time; ring wrap is covered. The checker
passes this capture. It does not prove payload preservation into the task,
candidate acquisition, or the downstream case's flag/counter semantics.

The isolated cold-control runner is:

```sh
.venv/bin/python tools/run_c54x_rom4_rf_delivery.py --mame mame/mame --rompath mame/roms
```

It creates a unique evidence directory under `/tmp`, generates matching
NSE-1 EEPROM for each case, and keeps MAME's working directory, logs and
NVRAM separate. Default coverage is two fresh 60-second runs per mode.
Both score and delivery checkers must pass; repeated score totals,
accepted-store counts, first response body and received object address
must agree within each mode. It is a standalone research gate, not a
physical-RF or registered-network acceptance gate.

| Port control | Fresh runs | Scores per run | Accepted stores | First native body | Task receipt |
| --- | ---: | ---: | ---: | --- | --- |
| Zero | 2 | 5158, all zero | 0 | `006d` | status `1390`, object `101438` |
| Rotating pairs | 2 | 5146, all nonzero | 9 | `006d` | status `1390`, object `101438` |

Both controls deliver the same first response despite different score
behavior. Candidate scoring therefore does not by itself establish a
successful acquisition result. The repeat checker has negative coverage
for changes to every compared observable and treats the two modes
independently. This is within-build repeatability, not a cross-host
determinism claim.

The consumer's `LSR #26; BCS` tests original controller bit 25: set
skips the counter increment. A later `LSR #27; BCS` tests bit 26.
One fresh capture with a case-window tap and narrow counter watch records
two halfword writes at trace PC `0x211ec4`, values `162f0000` and
`16300000`, mask `ffff0000`, at 1.645476077 and 1.645605077 seconds.
The counter had been initialized bytewise to `162e036b`; do not assume
a zero initial count or assign physical meaning to that value. These
writes were not reproduced by later retained-storage or a second fresh
capture with revised probes, although the delivery checker continued
to pass. The first capture did not correlate the write with the task's
stack object, so attribution to `0x1390` rather than its shared `0x138f`
case remains unclosed. Counter behavior is an observation discrepancy,
not an established emulation defect; the additional probes were removed.

The MCU bitmap-to-entry path and selected-entry tuning arithmetic are now
correlated with the native capture; see the synthesizer sections above.
The remaining operational boundaries are SCU serialization/settling and
RF sample lane, scaling and cadence. Task 10's downstream handling remains
independently open. The
score checker still passes (5146 nonzero results, 500 second comparisons,
nine accepted stores); it does not validate packet delivery or acquisition.
The 1523-versus-500 constraint remains a property of the pre-return scan,
not proof that the resident lifecycle is blocked there.

The present port-`27` receive implementation calls
`nokia_cobba_device::rf_receive_sample()` synchronously for every DSP
read. COBBA increments a saved observation counter and invokes its
external callback; there is no independently clocked RF receive FIFO,
sample-ready transition or overrun behavior. Consequently, read counts
per second measure this execution's consumption, not a recovered ADC
sample rate. A waveform fixture indexed by reads alone cannot establish
physical cadence, even when the filter arithmetic and score checks pass.
The next timing audit must distinguish the firmware's frame/slot pacing
from a peripheral sampling clock before adding either buffering or a
GSM waveform adapter. Codec serial PCM timing is a separate bus contract
and must not be reused for RF samples.

The cold zero and tone controls independently capture the first 64
port-`27` reads at advanced PCs `324b,3251,3257,325d`. Each initial
32-word group spans approximately 4.788 microseconds, with group starts
4.615385 milliseconds apart. Adjacent reads within the groups are as
close as 115.384 ns; counter snapshots advance from 1 through 6 and
restart for the next group. This establishes early burst consumption
under the current frame-counter model, not uniformly clocked ADC
sampling. It does not characterize the later `330a` scan-filter loop.
The two stimuli produce identical timing for this bounded initial window.
`c54x_rom4_rf_consumption_check.py` requires exactly 64 consecutive
observations and non-regressing timestamps; the isolated cold runner
records their measured span, gaps, PCs and counter values separately
from the score and delivery results. Its summary explicitly denies a
physical sample-rate claim.

The separately bounded scan window starts at 1.640203134550 seconds in
both fresh controls and captures advanced PCs `330c,3312,3318,331e`.
Its first two 32-word groups are again frame-spaced bursts, with minimum
adjacent gap about 115.384 ns. Across all 64 reads the zero stimulus spans
4.619480769 ms and the tone spans 4.619557692 ms; their largest gaps are
4.611250000 ms and 4.611326923 ms respectively. The roughly 77 ns
difference is an observed execution-timing difference, not a changed
physical ADC clock. Counter snapshots again range from 1 through 6.
The checker requires the exact repeating advanced-PC sequence
`330c,3312,3318,331e`, separately from startup. Each 32-word scan group
spans about 4.115385 microseconds; the start intervals are
4.615365384 ms (zero) and 4.615442307 ms (tone). The cold-run summary
retains group spans and start intervals rather than inferring a sampling
rate from aggregate reads. These
captures constrain consumption scheduling but do not select a receive
buffer depth, latch policy, lane order or sample-production clock.
Do not connect read-indexed tone/GMSK data as an operational RF source
merely because both windows are frame-paced.

### Product-specific physical buses

Do not generalize the NSE-1 parallel connection across DCT3. Nokia's
[NSB-5/7190 System Module manual](https://manualmachine.com/nokia/7190/5055290-service-manual/)
(03/01, pp. 19, 37-38 and 42-43) describes serial connections between
COBBA_GJP and MAD2WD1. Its pin tables distinguish control data/select
(`COBBASDa/COBBACSX`), separate bidirectional I/Q sample lines
(`COBBAIDa/COBBAQDa`), and the audio PCM wires. COBBA also has mode-dependent
pin assignments; the listed I/Q pins are not proof of the active mode in
another handset. This corroborates a revision/product distinction, not a
replacement register map for NSE-1 or a serial bit format for all products.

The attachment methods below are logical transaction boundaries, not an
implemented serializer for either RF bus. Promoting native RF support on
another product requires its own bus/mode evidence plus the MAD2-to-DSP
mapping. Neither service manual establishes DSP port `0x27` sample packing,
sign extension, interleaving or ready/clock behavior. Keep these contracts
separate from the already typed audio PCM wire.

## Implemented capture seam

`nokia_cobba_device::control_data_w`, `control_select_w` and
`control_data_r` are the attachment contract for a future real DSP backend or
physical trace replay. With verbose logging, every select transaction records:

```text
cobba: control sequence=N direction=read|write address=A data=DDD t=T
```

The sequence and read/write counters are saved device state. The conformance
gate checks ordered writes, non-destructive reads, 12-bit data masking and
four-bit address selection without assigning meanings to any register.

## Evidence required for promotion

A trace intended to replace an HLE voice profile must include ordered COBBA
transactions across these boundaries:

- DSP reset and audio initialization;
- traffic-channel setup before speech starts;
- physical Answer and the first accepted PCM block;
- volume and hands-free route changes;
- physical End, channel release and return to idle; and
- save/load or reset while a route is active.

The same transaction pattern must be corroborated by a second ROM using the
same COBBA revision before it becomes a shared register decode. Differences
belong in a typed product/DSP contract. Silence, board schematics, or the
current HLE gain values establish pin connectivity at most; they do not define
register fields.

## Remaining boundary

MCU images do not directly expose DSP-local COBBA register traffic. The native
NSE-1 C54x backend now exposes boot/self-test transactions, but this does not
close the mux/gain contract. Operational DSP execution or physical bus capture
is still needed for those fields. The opaque register file and declared HLE
voice profile remain the current boundary.

Nokia's [NSB-6 technical documentation, page 37](https://www.manualslib.com/manual/1616779/Nokia-Nsb-6-Series.html?page=37)
describes a 24-bit hardware-random serial read from COBBA and used with
software/identity data to establish stored flash authority. This is evidence
that hardware/provisioning pairing exists in the family, not a register map
for NSE-5. In particular, neither that description nor control-bus
traffic establishes registers 5/6 as analog measurements or unique identity
fields. Their current values are calibrated inputs, not measured defaults.

The public 5110 ROM4 analysis provides a software-only route when its
bring-your-own mask image is available. It identifies serial I/O ports
`0x2c/0x2d`, a distinct parallel MFI `0xCxxx` control frame, and bidirectional
PCM ports. These are separate buses and must remain separate local device
interfaces. See `djr_dsp_integration.md` for provenance, addresses and the
missing-overlay limitation.

The ROM4 boot self-test separates the two owners. It first reads COBBA control
register 8, sets `0x0610`, and writes it back. It then writes C54x BSPC
`0xc008` followed by `0xc0c8`, transmits `0x0aaa` through BDXR and polls BDRR
for the same word. Finally it clears the `0x0610` bits in COBBA register 8.
Texas Instruments' BSPC definition shows that the changed `0x00c0` bits are
RRST and XRST, enabling the DSP receiver and transmitter; DLB bit 1 remains
clear. The echo is therefore externally returned by COBBA under its register-8
test mode, not the C54x's internal digital loopback. See the
[TMS320C54x DSP CPU Reference Guide](https://www.ti.com/lit/ug/spru131g/spru131g.pdf),
section 9, for the DSP-side register contract.

`codec_serial_transmit`, `codec_serial_receive` and
`codec_serial_receive_ack` now model COBBA's side of that word boundary. The
only decoded register-8 behavior is the complete evidenced predicate
`(value & 0x0610) == 0x0610`; individual bit names remain unknown. The methods
operate on completed serial words, so a future C54x backend must still own
BSPC, BDXR/BDRR ready flags, interrupts and bit timing. The production HLE does
not call this test path.

## Native NSE-1 address-space audit

`tools/c54x_rom4_codec_observe.lua` passively taps DSP data and I/O spaces
separately, recording the first 16 accesses per direction/address without
additional device reads. A fresh four-second `noki5110` run with its own
generated EEPROM observed the following sequence:

| Space/address | Observation | DSP PC reported by tap |
| --- | --- | --- |
| Data `0022` | `c008`, then `c0c8` | `0e22`, `0e24` |
| Data `0021` | Self-test transmit `0aaa` | `0e31` |
| Data `0020` | Self-test receive `0aaa` | `0e5d` |
| I/O `0021` | Initialization writes `1482`, `1482`, `0482` | `4555`, `4559`, `455d` |
| I/O `0021` | Repeated reads `0482` / `0c82`, writes `0c82` / `0482` | `3221` / `3228`, `33f6` / `33fd` |

The PC is sampled during the memory callback, after operand fetch; it is not
automatically the instruction's start address. Reproduce with `-log`, an
isolated NVRAM directory and `-autoboot_script tools/c54x_rom4_codec_observe.lua`.
The audit's output cap limits observations, not execution.

The backend keeps I/O `0021` in its opaque saved I/O bank, independently of
data `0020/0021` and COBBA's boot echo. Firmware initializes the I/O word to
`0482`, sets bits `0c00` on frame entry and clears `0800` on exit; the observer
therefore records `0482 -> 0c82 -> 0482`. I/O writes do not transmit codec
samples. The removed mapping returned the retained `0aaa` self-test echo at
every I/O read and discarded firmware bitfield preservation. That alias was
not an evidenced microphone input. This partial register-storage model still
does not decode hardware status transitions or establish silicon ownership.
Establish operational sample source, frame cadence and readiness separately
from the boot echo before promoting native microphone/earpiece behavior. The
four-second audit does not prove absence of other register paths or validate
physical serial timing.

The local ROM also makes the bit operations explicit. At `321e`, `PORTR 21`
loads accumulator-low data cell `0008`, `OR #0c00` modifies it, and `PORTW`
writes it back. At `33f3`, the same port is read, masked with `f7ff` (clearing
bit `0800`), and written back. Initialization at `454e` builds two words in
adjacent memory and emits `1482`, `1482`, `0482` using post-decrement
addressing. `tools/c54x_rom4_codec_contract.py` checks all words in these three
bounded sequences against the acquired big-endian program image; its tests
reject a mutation at every checked word. This is not a whole-ROM ownership
census. These control-shaped sequences challenge the imported simple PCM
label but do not by themselves identify a MAD2 register or prove that no
operational sample path uses the port. Preserve the independent claim as a
hypothesis, not an implemented microphone contract.

The complete raw-word immediate-port census finds nine candidate reads and
twelve candidate writes to I/O `21`. All nine readers are covered by the
checker and perform constant mask/set operations before writing back to the
same port; the three remaining writers are the initialization sequence. The
seven additional readers are `3c6f`, `3c84`, `4231`, `4275`, `43c2`, `43ef`
and `4473`. They modify masks spanning low bits `0001/0002/0004` and higher
bits through `0800`. Thus the reviewed resident access surface is entirely
control-shaped; no sample-processing interpretation is established by these
sites. The checker rejects additional candidate sites rather than silently
claiming complete coverage after the image changes. This quantified scope
excludes dynamic port addressing, flash uploads and other product masks;
raw-word census matches alone are not proof of instruction reachability.

Passing `--trace LOG` to the codec contract checker additionally verifies the
data-space `0aaa` self-test and three ordered operational I/O readback cycles.
The correction was checked with a fresh four-second native RF-boundary run
(865 frames), a fresh thirty-second no-cell processing run (6033 mode-1
frames), and physical Menu input against the existing exact Phone book frame
oracle. These establish regression preservation and register separation, not
native speech or physical audio clocks. The I/O bank already participates in
native backend save states; no new unsaved latch is introduced.

`verify-5110-save-state` now uses `c54x_rom4_codec_restore.lua` to observe
pre-save/post-load boundaries, comparing emulated time, DSP PC, ST0, ST1, SP
and the non-destructive I/O `21` and serial-control data `22` words exactly. `--restore-log LOG` rejects a
missing, duplicated or changed snapshot pair and a failed round-trip harness.
This check supplements, rather than replaces, physical Menu input and the
exact Phone book frame. The fixture does not write DSP registers or assert
that an idle snapshot covers an active codec transaction.

### Operational data-space sample path

The acquired resident ROM supplies a distinct serial ISR entry, independent
of I/O `21`. At `ff80`, `ST #ffa8,PMST` selects vector page `ff80`; entry
`ffd0` (vector index 20) delayed-branches to `3416`, with context-save slots.
Within that routine, the bit test on data `06be` mask `2000` selects a direct
copy branch: `LD data[0020],A; STL A,data[0021]` at `342d..3430`, then a branch
to the common epilogue. The other path ultimately executes `STLM A,0021` at
`358a`. These are actual data-space output instructions, not DSP I/O `21`
sample writes. The meaning of the `06be` bit and all intervening processing
branches remains unclassified.

The resident tone initializer at `a598` separately reads shared data `0856`,
masks bit 0 and conditionally selects its disabled tail at `a5e2`. This
identifies an organic command input to trace, not permission to set the cell.
The codec contract checker covers the vector-page setup, vector entry, direct
copy, accumulator publication and tone-gate words. Its mutation tests apply
to these signatures too. This static evidence does not prove that vector 20
is exercised continuously in ordinary idle, define its external pin/cadence, or promote
native microphone/earpiece PCM. The next attachment must establish the
firmware's serial readiness/enable contract before introducing a codec clock;
neither a fixed interrupt cadence nor shared tone-cell writes may be added
merely to make this routine run.

TI's CPU Reference Guide tables 6-24/6-25 identify vector 20 as BRINT0
and vector 21 as BXINT0 on the buffered-port variants. This corroborates the
local vector-20 receive handler; it is not a measured MAD2 pin assignment.
The coherent native idle mask `035f` enables source bit 4 (vector 20) but
not source bit 5 (vector 21). Therefore transmit-ready changes alone cannot
be assumed to drive the observed sample ISR. The next attachment audit must
establish the receive frame source and its ready edge, with the BSP receive
enable and firmware interrupt mask observed independently. IMR/IFR are
CPU-internal MMRs in this implementation and bypass data-space taps; an
empty tap at data `0000/0001` would not prove absent mask activity.

The native endpoint routes each accepted receive-ready edge to source bit 4.
The existing external boot echo consequently exercises vector 20 after the
firmware's own polling read: BDRR retains `0aaa` for the ISR read at `3464`,
and the real processing tail writes `ffd5` at `358b`. No second receive edge
or periodic clock is synthesized. This exposed the genuinely executed
`9a5f` at `346f` (`STH A,15,*AR3-`), now implemented from TI SPRU172C
section 4-169 with 128 executable shift/source/address/SST cases. The CPU
suite, coherent boot, physical tone delivery, exact save/load and unchanged
Phone book hash validate the route. This is a boot-ISR execution result,
not continuous codec playback or speech.

An accepted boot transfer following a DXR write also raises BXINT0 (source
bit 5, vector 21) on the XRDY rising edge. The physical-key observation
retains `IFR=0020` with `IMR=035f` through the 11-second run: this request
is pending but masked, not a second sample ISR. Retained-word underrun
frames without a preceding DXR write must not generate another XRDY edge.
The tone gate checks this pending/masked observation independently of its
unchanged two TX/two RX boot accesses; continuous frame attachment remains
unimplemented.

The native backend exposes completed-word `codec_transmit_frame` and
`codec_receive_frame` attachment methods. Both the existing boot echo and
a future physical frame source use the same ready-edge interrupt routing.
Transmit underrun repeats the retained word without raising BXINT0 again;
disabled/unloaded TX and RX overrun do not manufacture ready edges. The
executable BSP helper tests distinguish these cases. These methods provide
no timing source and do not enable the currently unattached NSE-1 PCM clock.

The physical `1` fixture now retains separate boot/interactive serial trace
caps and observes IMR/IFR plus non-destructive BSPC readback. At idle,
`IMR=035f`, `IFR=0020`, `BSPC=c8c8`: RRST/XRST are released, XRDY is set,
and RRDY is clear. At 8.076920923 s the organic tone initializer writes
`c008` at `a4a7`, then `c0c8` at `a4a9`; its COBBA control traffic writes
register 0 with `0000` and reads/re-writes register 8 with `0626`. Later
release handling repeats the BSP reset/release sequence. Counts still show
the boot echo plus its one receive-ready ISR and no post-key samples. Thus this
fixture does not lack BSP release or the receive interrupt mask; it lacks
observed receive-frame delivery. The COBBA register-0/8 field meanings and
clock/mux gating remain unresolved, so these numeric transactions are not
permission to fabricate microphone samples or assert a periodic interrupt.

The resident register-8 pair is independently checked from ROM words:
`a4a5` resets/releases BSPC; `a4b4` reads selector 8 through `4610`, ORs
`0600`, and writes through `45c2`. Conversely, `a502` resets BSPC, reads
selector 8, ANDs `09ff` (clearing those two bits), and writes it back. Thus
`0600` is a paired firmware activation mask, distinct from the self-test's
additional `0010` echo bit. These instructions do not identify the two
bits as oscillator enables, converter enables or routing controls. The
checker protects all three sequences against word mutations.

The own-product manual's pin table (pp. 3-40/3-41) identifies MAD2 pin 135
as PCMTxData/DX, pin 137 as PCMRxData/RX, pin 138 as PCMDClk/CLKX and pin
139 as PCMSClk/FSX. This strengthens the external transmit-clock/frame
attachment, but does not separately identify a CLKR/FSR wiring or internal
MAD2 clock fan-out. Do not import another COBBA revision's clock rate or
assign receive timing from a pin name alone. The public manual documents
rates and connectivity, not the register-8 decode or frame edge/pulse width.

The extension-register observation retains data `0023` taps alongside BSPC.
In the coherent physical-tone fixture no write to `0023` is observed and
its saved backend latch remains `0000`. Under the generic BSPCE definition
in TI section 9.3, zero selects no autobuffering, default clock polarity
(receive on falling edges, transmit on rising edges), and active-high frame
sync. BSPC `c0c8` selects external clocks/frame sync, burst transfers and
16-bit words. These are compatible with the own-product PCM interface;
they do not establish the undocumented MAD2 receive-clock fan-out or prove
that its register reset matches every generic C54x variant. Also, FSM=1
requires a frame pulse per word, not specifically a one-clock-wide pulse:
TI section 9.2.4 explicitly supports longer pulses with delayed shifting.
The PCM wire's current one-clock profile constraint must not be mistaken
for an independently measured NSE-1 sync width.

### Organic tone request boundary

`tools/c54x_rom4_tone_observe.lua` combines the physical-input harness with
passive MCU/DSP tone-cell taps and the serial-word observer. ARM taps span the
complete 32-bit bus word `100ac..100af` and retain the raw byte mask; the upper
halfword corresponds to DSP shared cell `0856`, not its neighboring parameter.
Boot and interactive records have separate caps, and final serial counters
are uncapped.

In a fresh eleven-second NSE-1 run, physical key `1` at 8 s produces an MCU
upper-halfword `00e1` write at 8.076714462 s, followed by native reads of `0856`
and a copy to data `00fe` at `a5de` by 8.076919730 s. Release at 8.22 s is
followed by control `00e0`, then `0001` and its native initializer copy. Total
tone-cell reads/copies are 29/7, while total data-space serial writes/reads are
2/2: the boot `0aaa` exchange and its receive-ready ISR's retained-word read
and computed `ffd5` output, no post-key operational samples. `--tone-log LOG`
checks the physical key, ordered post-key command/initializer, intact boot
echo, control readback and uncapped final counts; it refuses to use an earlier
boot initializer as evidence of a key response. These observations rule out
missing MCU tone delivery for this fixture, not missing serial clocks on real
hardware or tone support in other images.

The next native implementation boundary is therefore serial enable/readiness
and externally clocked word delivery, not a synthesized tone command. TI's
[C54x Applications Guide, BSPC configuration example](https://www.ti.com/lit/ug/spru173/spru173.pdf)
(section 3, p. 3-38) defines receiver/transmitter reset bits and receive/transmit
ready flags separately. That primary reference supplies generic peripheral
semantics; it does not establish the MAD2/COBBA frame-clock rate, pin mapping,
or whether every native data `0022` write is the generic BSPC interface.
Keep those product attachments explicit before promoting operational audio.

### NSE-1 physical clock contract

The control-write helper at `45c2` has 27 raw direct CALL/CALLD candidates
in the resident image. Reproduce their operand windows with
`c54x_rom4_port_census.py IMAGE --call-target 0x45c2 --context 10`.
This is full raw-word candidate coverage, not proven executable-call or
data-flow coverage; indirect calls and uploaded code are excluded. The
reviewed `b89c` sequence calls `a484` with register index 8, then supplies
`0602` to `45c2` with index 8, followed by `0944` with index 9. Its exact
words are checked by `codec_register8_constant_setup`, including mutation
tests. This is another local control-use context, not evidence that `0600`
means clock enable or that this path executes during the tone fixture.

The passive helper-entry capture in the physical `1` fixture observes 43
calls over 11 seconds, including register 8/value `0626` returning to
`a4bb` during the keypress (8.077 s). The tone checker requires that
post-key caller independently of the shared tone-cell writes. The constant
setup at `b8a2` is not observed in this fixture. This confirms execution of
the known activation path, not its clock-field meanings. Program-tap PC
must be `45c3` because fetch increments PC; the captured return address is
read from ordinary stack RAM. Output is capped at 64 entries with an
uncapped total, so a longer capture must check its truncation explicitly.

Run `make check-c54x-rom4-codec-tone RUN_DIR=run_native_codec` for the
isolated 11-second acceptance capture. Each invocation creates fresh own
EEPROM/NVRAM and a separate working directory under that root, retaining
stdout, error log and frames even on failure. It checks the local ROM
signatures, reset/echo/receive ISR, masked transmit request, physical key
delivery and register-8 activation caller, and rejects new data-`0032`
traffic or reset/illegal failures. This is a native serial/control gate,
not a waveform or speech gate; save/load remains independently tested by
`verify-5110-save-state`.

The [NSE-1 System Module manual](https://www.eserviceinfo.com/preview_html.php?fileid=26879&previewid=13251)
(03/98, pp. 3-32--3-33) supplies product-specific clock evidence: COBBA
divides the 13 MHz reference by 13 to generate a 1 MHz PCM data clock, then
by 125 for an 8 kHz sample/frame clock. Its diagram shows a 16-bit word with
13 converter bits and sign extension. This is not the 520 kHz profile used
by other product-family attachments. The same manual specifies a 13 MHz DSP
reference multiplied internally to 52 MHz, corroborating the native backend
clock rather than leaving it solely as a co-simulation pacing calibration.

`PRODUCT_5110` now records those four PCM properties. Sync width and transfer
edge are not promoted from another handset; the unsupported shape remains
inert and the native backend is not attached to the HLE block-transfer path.
Thus these constants supply the physical clock boundary for future native
serial work, not a periodic interrupt source or proof of audio output. The
native tone fixture still requires serial readiness/frame delivery to be
modeled separately.

### Serial-control ownership correction

TI's [C54x CPU Reference Guide](https://www.ti.com/lit/ug/spru131g/spru131g.pdf),
section 8.2 tables 8-2 through 8-7, maps data `20/21/22` to BDRR0/BDXR0/BSPC0
on the relevant buffered-serial variants. Section 9.2 distinguishes writable
configuration from read-only readiness/clock-pin fields and defines reset
and frame-sync behavior. Combined with the local ROM's `c008 -> c0c8`,
BDXR write and BDRR poll, this corroborates the serial-port interpretation;
it does not corroborate a COBBA parallel register-C transaction.

The native backend owns data `20/21/22` through a saved standard-mode
completed-word state model. Writable configuration is masked with `c0ff`;
RRDY, XRDY and receive-overrun status derive from receive, transmit and reset
transitions rather than firmware writes. An unread receive word is preserved
on overrun, reception resumes after its acknowledgement, and transmit
readiness returns at an explicit frame transfer. Executable C++ tests cover
transmit overwrite before a delayed frame and retained-word retransmission
on externally clocked underrun (TI section 9.2.4): repeated frames do not
invent a new XRDY transition. Reset cancels the valid loaded-word state;
reset-time preloading remains outside the established subset. FO=0 16-bit
words and FO=1 8-bit words are tested over all 65,536 input values: the
transmit latch retains its upper byte while the wire ignores it in 8-bit
mode, and BSP reception sign-extends the low byte (TI section 9.2.2).
Extension bit 7 (FE) now combines with BSPC bit 2 (FO) to select 16/10/8/12
bits as specified by TI table 9-10. Each format transmits the right-justified
payload and sign-extends reception; all 65,536 input words are tested in
each of the four formats, including retransmit/ready-edge behavior. Native
frames consult the saved data-`0023` latch, without changing its reset value
or claiming that firmware uses FE in the observed boot. The generic TI
BSPCE reset value is `0003`, whereas the current MAD2 latch is `0000`;
the product-specific reset/decode remains unresolved. DLB also remains
unimplemented: TI connects DX/FSX internally to DR/FSR but still requires
external CLKR when MCM is zero, so DLB cannot justify an untimed receive
completion or bypass the missing receive-clock attachment (section 9.2.3).
The suite also checks all 65,536 control-write values and the state transitions. Native coherent
boot, 30-second processing (6,033 mode-1 frames), organic physical-key tone
command delivery and exact idle save/load pass with this state model. The
tone run observes only the boot echo and its one receive ISR, not continuous
or post-key operational samples.

This remains partial hardware: clock-pin status, transmit-shifter status,
autobuffering, reset-time transmit preloading and physical frame/bit timing
are not established. Only the separately evidenced external boot echo
provides an untimed transfer; ordinary pending samples do not acquire a
fabricated frame or interrupt. No operational PCM is claimed.

The coherent gate now checks real serial reset/release and echo observations
instead of expecting synthetic COBBA parallel register-C logs. Data `0032`
now retains an opaque saved latch without forwarding writes into COBBA.
The local resident candidates at `3b48`, `3b5c` and `3b5e` store `c00c`,
`c008` and `c0c8`; exact sequence fixtures protect that serial-control-shaped
observation. No data-`0032` access occurs in the 11-second physical-key
capture. TI assigns `0032` to different peripherals across variants
(including TSPC and Timer1 TCR); neither identifies MAD2's implementation.
Thus this retires an unsupported MFI side effect, not completes a second
serial port or establishes hardware readback semantics. The actual MFI
attachment remains to be recovered independently. Before attaching the 8 kHz frame
clock, establish the remaining enable, frame and interrupt-routing contract
and validate word timing. MFI port mapping remains an independent boundary; do not
double-drive both owners to preserve an old log predicate.

## Physical capture option

The NSM-3 v5.31 flash-staged verifier independently uses the serial port pair
`2c/2d` to read register F around a register-D status handshake. The isolated
core fixture corroborates the existing register-selection grammar and checks
input sensitivity, not the real 8210 register-F value or its meaning. See
`8210_bringup.md` for the unresolved DSP memory contract. No production
COBBA/DSP response is promoted from those fixture inputs.

The Nokia 5110 NSE-1 service material names factory test points for `DSPXF`,
`VCOBBA`, `COBBARSTX`, `COBBAWRX`, `COBBARDX` and `COBBACLK`. This makes a
logic-analyser capture plausible using test pads rather than soldering onto
the COBBA BGA. A fixture still needs a stable battery/bench supply, common
ground, voltage-compatible high-impedance probes, and a working phone placed
through the same lifecycle cases listed above.

The 3210 repair guide identifies COBBA clock and reset measurement points and
diagnoses parallel/serial-bus failure, but the reviewed public pages do not
yet establish equivalent exposed data/strobe pads. Do not transfer NSE-1 test
point numbers to NSE-8. Schematics or board continuity work are still needed
before proposing a 3210 probe layout.
