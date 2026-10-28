# ROM4 RF control and receive maps

NSE-1 synthesizer, tuning and receive-window observations. [COBBA control](cobba_control_boundary.md) owns admitted device semantics. Calibrated samples, score-only negatives and opaque serial writes do not establish RF acquisition.

## NSE-1 synthesizer boundary

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

### Frequency-plan consistency

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

### Transfer boundary audit

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

### Sibling RF documentation

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

## Software RF reference boundary

Nokia's [NSE/K-1/3 Repair Hints v2.0, 2001-06-13](https://files.elektroda.pl/44719,nokia%2B5110_5130_6110_6130_v20.html),
printed pp. 29..31, identifies the receive second IF at COBBA pins 22/23
as 13 MHz and separately checks the COBBA reference clock. These are
analogue/clock measurements, not a digital word-rate or port-`27` packing
specification. Do not derive the sample cadence from that frequency.

The [RAE-3 system manual, issue 06/01, pp. 3-37..3-38](https://www.eserviceinfo.com/preview_html.php?fileid=26922&previewid=13302)
describes COBBA_GJP receiving IF, separating/converting I/Q, and exchanging
digital RF data/control through SerialMFI, separately from audio PCM.
This later MADLinda board corroborates distinct interfaces only; it does
not establish NSE-1 COBBA variant compatibility, sample width, sign,
lane order, scaling or timing. No machine configuration follows from it.

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
It forms A=`x2*x1 + y2*y1` and B=`x2*y1 - y2*x1`, subject to the
core's multiply modes, on both TC paths.
These are adjacent-pair dot/cross products, consistent with a complex
lag correlation without identifying which physical lane is I.
TC selects history publication, not reduced arithmetic. The conditional
branch at `47ee` is delayed: both the MAS at `47f0` and MAC at `47f1`
execute before either return path. TC-set reaches the delayed return at
`47f3`, whose two MVDD delay words copy the current pair back into
`1252/1253`; TC-clear returns at `47f2` without those writes.

`tools/run_c54x_rom4_crc.py --mame mame/mame --rompath mame/roms
--rf-pairs` executes the unchanged 16-word helper in an isolated ROM
algorithm fixture. Eight bounded signed vectors, both TC states and FRCT
off/on give 32 cases and full 65440-word ordinary-RAM replays. Each case
checks the independently computed dot/cross result, T, history/current-pair
words, guards, final AR2/AR3, TC and balanced caller stack. `--staged`
independently repeats the same matrix on the staged frontend. No input or
result is injected into a handset. The vectors avoid overflow with OVM and
SMUL clear; saturation, other mode combinations, physical timing, I/Q
identity and sample cadence are not established by this fixture.

The byte-pinned result tail `0x4778..0x4797` normalizes,
squares and combines accumulated terms, then performs 15 SUBC steps
against a nonzero denominator. Accordingly the uploaded caller's
threshold comparison is a processed correlation-score boundary, not
an established raw ADC-energy or dBm threshold. Exact score units and
the caller's physical interpretation of TC remain unresolved.

## Current RF-control reproducibility boundary

A fresh `run_c54x_rom4_rf_delivery.py --repeats 1 --execution-probes` check
against binary SHA-256
`343ce4adb8736b95c587a8b1d639be520b032c77e90ed735723e04abc183de86`
does not reproduce the accepted-store result below. The execution-owned
legacy-bus zero control produces 4,888 zero scores; rotating input produces
4,888 nonzero scores, maximum
`0x0538`, and zero second-comparison/accepted-store entries. Threshold remains
`0x0574`. The strict rotating-input gate fails and is not rebanked.
The retained comparison data below therefore describes earlier captures,
not verified acceptance on this executable. Further comparison needs matched
capture provenance or independently supported RF input; do not increase input
amplitude or lower the acceptance threshold to make this diagnostic pass.
The RF delivery runner writes `inputs.json` before launching any case and
per-case `invocation.json` before emulation: binary/observer/generator/loose-ROM
hashes, explicit missing inputs, command and generated EEPROM hash survive a
failed gate. Source revision alone is not treated as executable provenance.
Both controls currently consume 991,360 input words and independently pass
startup/scan consumption, first response delivery (`1390`, object `101438`,
body `006d`), request correlation and channel-109 numerical tuning consistency.
Neither has an accepted score. The zero case passes its control conditions;
the rotating case fails only score acceptance. This is not physical RF input,
successful acquisition or registered-network evidence.

Passing isolated CPU and codec fixtures neither proves every RF
operand/pipeline context nor establishes a physical RF adapter.
The retained MADos reference provides matching NHM-5 upload payloads and MCU
interface definitions, not a measured NSE-1 receive-word stream. The parallel
RF contract in the NSE-1 manual and the separate 1 MHz/8 kHz PCM contract
must not be conflated: neither the PCM rate nor the twelve-bit parallel bus
width fixes port-`0x27` sample packing or timing.

An evidence-backed receiver fixture still needs these independently justified
properties before its success or failure can diagnose acquisition:

| Boundary | Required evidence |
| --- | --- |
| Word representation | Signed/offset encoding, significant bits, alignment and lane order at DSP port `27`. |
| Production | Sample cadence and retention/backpressure across reads and receive-window transitions. |
| Tuning | Selected channel/frequency and the relationship between sampled waveform and SCU programming. |
| Waveform | Independent FCCH/SCH or other known GSM input with generator and adapter provenance. |

The existing impulse and mathematical rotating-input diagnostics qualify
software consumption and arithmetic only. They do not fill these physical
contracts. Neither a continuously supplied tone nor an accepted score alone
is a native GSM decode. A new waveform experiment may declare explicit
adapter hypotheses, but must not promote them to product configuration merely
because a threshold is crossed.

The MCU response header belongs to `STRH r0,[r4]` (`8020`) at `26be4a`,
not the preceding literal load at `26be48`. The observer pins that opcode
before installing its write tap, and the checker requires the store-owned PC,
upper-halfword `1390` write, correlated post and subsequent task-10 receipt
in order. Both fresh controls reproduce this chain; a missing observation at
the adjacent load is not evidence that native response delivery stopped.

The runner writes schema-2 `result.json` per case and `summary.json` even
when a checker fails. Observed values and independently evaluated boundaries
remain visible alongside named failures; a decoded summary is not score
acceptance. Failed cases cannot make the overall exit status green. Binary,
observer, generator and actual ROM identities remain in `inputs.json`, with
the per-case command and generated storage identity in `invocation.json`.

### Frontend observation boundary

The RF runner accepts four explicit research compositions: `noki5110`
(atomic/legacy SWWSR), `nse1astd` (atomic/standard), `nse1plg`
(staged/legacy) and `nse1pipe` (staged/standard). Each has isolated own
NVRAM, recorded frontend/bus metadata and a unique observed machine header.
The final DSP illegal flag is independently checked: a normal MAME exit does
not prove that the CPU continued executing. These choices do not identify the
fitted Lead bus dialect.

`nse1pipe` zero and rotating-input controls each consume 991,360 RF words
over 60 emulated seconds and finish with the illegal flag clear after the
independently qualified MVDM-to-T/TRN extension. The original staged stop at
`47f6` was `720e 06be`, a transfer from data `06be` to T; it was a CPU
coverage boundary, not an RF-input verdict.

The Lua program-read probes are not instruction-entry probes on staged
execution: prefetch can sample a different live accumulator and can count
unretired instructions. Their staged score values are explicitly rejected.
`--execution-probes` instead selects `c54x_rom4_rf_execute.lua`; this is
automatic for staged machines. Read-only debugger breakpoints at `0ad1`,
`0add` and `0ae4` capture the score input, second-comparison entry and
accepted-store entry. The DSP is explicitly the debugger's visible CPU:
otherwise `a` can parse as hexadecimal 10 rather than the DSP accumulator.
The observer never writes firmware registers or RAM.

The execution parser requires a unique probe marker, complete source summary
and ordered result/second/store records. It derives score counts and maxima
from those entries, not fetch snapshots; the shared summary contributes only
the mathematical input mode and actual port-read count. It preserves the same
zero/DC negative control and strict rotating-input criterion. Instruction-entry
observation is not proof that a store subsequently completed or that a physical
signal was acquired.

A fresh matched comparison uses `--machine noki5110 --execution-probes`,
`--machine nse1astd --execution-probes` and `--machine nse1pipe`, each with
`--repeats 1`.
Recorded binary, observer, generator and loose-ROM hashes are identical; only
the machine/frontend/bus profile differs. Each case consumes 991,360 input words
over 60 emulated seconds and ends with the illegal flag clear.

| Input | Legacy atomic entries | Standard atomic entries | Standard staged entries | Every observed score | Second/store entries |
| --- | ---: | ---: | ---: | --- | --- |
| Zero | 4,888 | 4,887 | 4,885 | `0000` | 0 / 0 |
| Rotating | 4,888 | 4,887 | 4,885 | `0538` | 0 / 0 |

Thus the staged zero-score anomaly from fetch snapshots is excluded, and the
rotating stimulus remains below `0574` across all three compositions. The entry-count
difference is not hidden or treated as timing parity. The rotating score gate
still fails. This establishes neither a valid physical stimulus nor acquisition.

The same execution observer protects three additional boundaries:

| Probe | Checked contract |
| --- | --- |
| `37ce` / `3807` | Capture native type `008a`, its two-byte body and the producer/consumer cursors at execution entry. Require a coherent two-word ring advance, ordered MCU consumption, store-owned `1390` header, correlated task-10 post and receipt. |
| `3ffb` / `400f` / `402e` / `403a` / `4048` | Check selected-entry addition and packing against the same independent numeric model as the atomic fixture. Require the actual program-space upload before these entries, then the corresponding port-31/32 values after the result. I/O callback PCs are context, not opcode owners. |
| `330a` / `3310` / `3316` / `331c` | Require the first 64 execution entries in the exact four-instruction order, with live program words `74d6 0027` at each site. A debugger-only counter bounds observation; no firmware state is changed. |

Execution records have no synthetic timestamps. Their log order establishes
causal sequence; actual MCU/upload/I/O timestamps retain their own monotonicity
checks. The filter result is explicitly an executed-input-instruction count,
not the earlier bus-read timing measurement or proof of ADC cadence. Legacy
atomic fetch-based checks remain separate and unchanged in scope. Missing
markers, incomplete groups, wrong payload/cursor identity, changed arithmetic
or port order invalidate the corresponding execution check.

All three boundaries pass in the full matched atomic/staged controls, for both
zero and rotating input. Native `008a/006d` reaches MCU object `101438` as
status `1390`; selected-entry packing yields `00302813` and the channel-109
numeric consistency check passes. The staged delivery/tuning failures from
fetch-based snapshots therefore do not demonstrate missing firmware activity.
This is a correlated scan-response transport contract, not accepted RF data or
laboratory registration through native DSP execution.

Other broad caller/request snapshots remain diagnostic. Neither these transport
observations, score agreement nor matched SWWSR selections establishes physical
sample packing, synthesizer-wire serialization or the fitted bus dialect.
`--seconds` permits short development probes, but does not relax the score or
completeness checks; the matched comparison above uses the default 60 seconds.

The retained arithmetic audit used binary SHA-256
`67575b98a9ba96d7aa61ef3875fe5e677b9ab455ef990d6cf2bf3595896d8765`.
A minimal-observer control from `ee56297f`, without delivery/tuning taps,
also produced 991360 reads, 4889 nonzero scores, maximum `0x0538` and zero
accepted stores. That rules out the added observation layer on that binary,
not all executable/configuration differences. The operand-level proofs below
belong to that retained capture; they do not assert that every present
instruction operand is unchanged.

That arithmetic-audit executable passes all 64 signed RF-filter impulse controls after
correcting fixture alignment. The previous fixed `1.6402`-second onset selected
port reads at `0x33cb/0x33d1/0x33d7/0x33dd`, not the filter under test, and
failed its unchanged input-PC check. The fixture now waits for the first filter
read at `0x330c` after that minimum time. All 32 positions at both `+16384`
and `-16384` match the existing coefficient, lane and rounding expectations.
This validates the isolated filter contract on that executable, not the
continuous correlation score, physical RF timing or acquisition.

Retained passive taps localize the rotating-input arithmetic.
The first eight entries at `0x4778` have `A=0x0d047a80`, `B=0`,
`ST1=0x6103`; at `0x478a` the scratch denominator at data `0x0060`
is `0x0a90` and both accumulators are zero. At `0x4791`, `A=0x6e40`
and `B=0`; at `0x4796`, `A=0x800538`, before the returned low-word
score becomes `0x0538`. Independent integer checks below reproduce that
captured path through normalization; it is not an outstanding instruction
defect or grounds for changing divisor, scaling or threshold.

The captured division tail is independently consistent: at `0x4791`,
`ADD B,A` leaves `A=0x6e40`, and `SFTA A,8` supplies numerator `0x6e4000`.
For the following 15 `SUBC` iterations, unsigned integer division gives
`divmod(0x6e4000, 2*0x0a90) = (0x0538, 0x0100)`; remainder shifted by 15
plus quotient is exactly the observed accumulator `0x800538`.
`test_c54x_rom4_rf_division.py` pins this calculation without reproducing the
core's iterative implementation. This checks the captured unsigned division
domain only, not all SUBC signed/status semantics or the upstream
correlation/normalization calculation. The current discrepancy is therefore
not evidence of a wrong quotient for these captured operands.

Instruction-boundary captures likewise reproduce the normalization arithmetic
independently for these operands. `EXP` selects exponent 3 for incoming
`A=0x0d047a80`; normalization followed by right shift 9 yields high word 52,
whose square is `0x0a90`. The signed double-word loaded at `0x478a` is
`-0x14fc0440`; the same normalization and right shift yield high word -84,
whose square is `0x1b90`. The following shifts by 2 and 8 supply numerator
`0x6e4000`. The independent signed-integer test pins this complete captured
arithmetic chain. No mismatch is established within normalization for these
inputs; the incoming correlation accumulators and their lifecycle are the
next boundary to audit. This does not validate all status/saturation cases.

The first correlation input is accounted for by 17 observed returns at
`0x47f2` before the first `0x4778` entry. Each returns `A=-0x13c0040`,
`B=0`; their sum is exactly `-0x14fc0440`. Across the first 32 captured
calls, data `0x1254/0x1255` remains `0x09e0/0x0ec8`, and
`0x09e0^2 + 0x0ec8^2 = 0x13c0040`. This numerical identity does not establish
the helper's general dot/cross semantics. A subsequent data-bus capture
does establish its operands for this case: reads are `1254=09e0`,
`1253=f138`, `1252=f620`, `1255=0ec8`, `1252=f620`, `1253=f138`.
The signed stored pairs are therefore `(-2528,-3784)` and `(2528,3784)`;
their dot product is exactly `-0x13c0040`, and cross product zero.
The first 17 calls use the same six reads, with no writes by this helper
in the observed TC-clear branch. These products are independently pinned in
the arithmetic test. The producers are active: initialization writes the four
words at observed PCs `0x4725..0x4728` at 1.737134 seconds; repeated writes
at `0x4752/0x4753/0x4756/0x4757` begin at 1.741749 seconds and republish
the same four values approximately every 4.616 ms (first eight updates
captured). Earlier filter-path writes at `0x33e5/0x33e7` populate these words,
and reset clears occur at `0x0f15/0x3074`. Thus this case is not a stalled
history update. Source-buffer selection and its relationship to the synthetic
input phase remain to be audited; the advancing outer cursor alone is not
proof that each call consumes a different pair. No multiply/addressing defect
is demonstrated by this captured case. These PCs are values observed during
bus writes, not independently recovered instruction-start labels.

Source-bus tracing confirms distinct queue addresses: initialization reads
`0x1a00..0x1a03`; the next 17 four-word groups advance from
`0x1a04..0x1a07` through `0x1a44..0x1a47`. Each group contains
`f620 f138 09e0 0ec8`. The consumer waits on producer/consumer pointers
`0x06b7/0x06b8` between groups. A stuck source cursor is therefore excluded
for this first window. The repeated values originate in queue contents;
the next audit is the producers of those contents and their input phase,
not changing the helper's indirect-address arithmetic.

The queue producer publishes the four-word groups at observed PCs
`0x3338/0x3339/0x3342/0x3343`. In the first complete correlation window,
input-read counts advance from 10976 through 11520 in steps of 32, one group
per approximately 4.615385 ms. The diagnostic tone has period 32 words, so
each block sees the same stimulus phase. Repeated queue values are consistent
with that periodic input, not evidence of a stuck producer. After this window,
the distinct `0x335d/0x335e` producer publishes zero pairs. Keep these
lifecycles separate when tracing the historical score discrepancy; this does
not establish the port's physical sample cadence or the old capture's validity.

The first window's two totals have different, observed accumulation contracts.
At `0x4734/0x4735`, the firmware seeds data `0x124e/0x124f` from the
double-word energy queue at `0x1a80/0x1a81`, value `0x00b92340`.
The block with `BRC=0x0010` then makes 17 adjacent-pair comparisons. Each
adds `-0x13c0040` to the correlation at `0x124a/0x124b` and reads one
further energy double word, advancing `0x1a82..0x1aa3`. Thus the entry to
`0x4778` is exactly `18*0x00b92340 = 0x0d047a80`, while the correlation
is `17*(-0x13c0040) = -0x14fc0440`. The native energy producer publishes
`0x00b9/0x2340` at observed bus-write PC `0x3374`. Its value is not the
squared magnitude of the stored correlation pair. The independent integer
test pins both totals; unequal counts alone are not a repeat defect.
These are bounded read-only observations on the same synthetic rotating
input, not an RF-format claim. The next numerical boundary is the energy
producer around `0x3374` and how it derives its term from filter outputs;
do not replace the separate energy stream with pair magnitude by assumption.

The captured energy producer is also numerically accounted for. In the
TC-clear filter path, `0x3369` primes A with the difference at `0x1a57`
against reference `0x06d2`; the `SQDST` block at `0x336a/0x336b`, with
`BRC=3`, executes eight squares. Its sampled differences in square order
are `-340,-1708,-1448,-968,-1708,340,-968,1448`, from filtered words
`0x1a57..0x1a50`, with both reference words `0x06d2/0x06d3` zero.
Their squared sum is exactly `0x00b92340`. A ninth read at `0x1a4f`
supplies difference 340, leaving A=`0x01540000`; it is not included in
that B sum. This follows the previous-A operation specified by TI
SPRU172C, SQDST (4-160), not a missing final square inferred from read
count. `test_c54x_rom4_rf_division.py` independently pins all eight
partial sums and the remaining A value. FRCT and OVM are clear in this
capture; this does not validate their general status/overflow behavior.
No mismatch has been established from these filter outputs through energy,
correlation accumulation and normalization for the captured first window.
The historical accepted-tone result remains unreproduced; it is not an
oracle for modifying this numerically consistent path or for declaring
physical RF acquisition. Further comparison needs matched executable/input
provenance or independently supported RF sample-format evidence.

`make check-c54x-rom4-fr-decoder` and
`make check-c54x-rom4-fr-decoder-staged` independently pass 32 acquired-ROM
decoder frames: 5,120 PCM samples match the reference exactly on atomic and
staged CPUs. Inputs cover silence, impulses, alternating signed samples and
deterministic pseudorandom PCM encoded by the independent codec. Each native
frame is replayed from its prepared input/history snapshot, comparing all
65,440 ordinary RAM words without advancing the reference twice. The staged
`fr-decoder-pipe` BIOS loads the same program/DROM and checks the decoder
entry words. This is isolated decoder acceptance, not handset RF acquisition,
a fitted codec clock or end-to-end speech.

The original native clear sequence is `RPTZ A,#0027; MVMD AL,122e` at
`84d5..84d8`. RPTZ must clear A before the first AL sample, not merely before
the move retires (SPRU172C 4-150). Matched frame-8 traces had identical entry
registers, all ordinary RAM and 9,884 executed instructions; their first
different write was `122e`, atomic `0` versus staged `5`. Sampling after the
older RPTZ clear fixes the PCM differences without changing arithmetic or
clamping outputs. The `mvmd-rptz` matrix checks matching/opposite clears;
ordinary ALU producer latency is unchanged. MMRs `08/0b` are AL/BL.

The decoder also covers the qualified RPTBD delay T load at `8527` and
delayed forward exit at `84c2`. Their independent instruction matrices are
documented in [ROM4 DSP loader](rom4_dsp_loader.md). Other MMR sources,
mapped destinations and general interrupt compositions are not inferred
from successful decoder execution.

`tools/run_c54x_rom4_crc.py --fr-decoder --trace-fr` (optionally `--staged`)
retains frame-8 entry RAM/registers and execution-owned instruction/data traces.
Only CPU-executing data accesses are logged; comparison/replay reads and
debugger peeks are excluded. Trace PC contexts are not operand-owner labels.
Tracing preserves the same strict PCM/replay criteria and does not turn a
diagnostic capture into acceptance.

`check-c54x-rom4-fr-encoder-staged` validates the transmit direction on the
staged frontend; `check-c54x-rom4-fr-encoder` remains its atomic control.
`fr-encoder-pipe` loads the acquired program/DROM and verifies its entry words
before running reset, preprocessing and encoding. Both frontends match all
2,432 parameters over 32 frames: eight each of silence, impulse, alternating
and deterministic random PCM. Preprocessing histories match independently.
Every frame replays from its pre-preprocessing checkpoint and reproduces all
65,440 ordinary RAM words, with the independent reference advancing once.
The eight native PCM-mute cases also pass, including buffer guards.
Adding `--trace-fr` captures frame-0 encoding entry RAM/registers and the
execution-owned instruction/data stream without weakening parameter or replay
checks; it does not yet capture the frame-8 instruction stream.
Instruction, repeat and delay-load qualifications live in the
[ROM4 DSP loader contract](rom4_dsp_loader.md). This is bounded isolated FR
algorithm acceptance, not a handset PCM stream, fitted codec clock, native
RF acquisition or end-to-end call. Broader frontend/bus parity remains open.

## RF receive observation contract

The present port-`27` receive implementation calls
`nokia_cobba_device::rf_receive_sample()` synchronously for every DSP
read with side effects enabled. COBBA increments a saved observation counter
and invokes its external callback; there is no independently clocked RF receive FIFO,
sample-ready transition or overrun behavior. Consequently, read counts
per second measure this execution's consumption, not a recovered ADC
sample rate. A waveform fixture indexed by reads alone cannot establish
physical cadence, even when the filter arithmetic and score checks pass.
Side-effect-disabled debugger reads instead return the saved last-consumed
word (zero before the first sample or after reset), without calling the
source, advancing its count or spending the backend's RF trace budget.
This observation value is not a hardware sample-ready latch or FIFO.
The actual-backend fixture checks eight peeks around three ordinary reads,
save/load and reset against a counted source. The fixture source cursor is
itself saved; this does not automatically serialize arbitrary external
callback providers. Its separate mandatory result is
`C54x native backend RF observation: PASS` with `physical_rf_claim=0`.
The backend gate, native mixed/full-scale filter controls, native tone and
save-state gates, and default 3210 structural gate pass with this behavior.
The next timing audit must distinguish the firmware's frame/slot pacing
from a peripheral sampling clock before adding either buffering or a
GSM waveform adapter. Codec serial PCM timing is a separate bus contract
and must not be reused for RF samples.

The current 30-second zero-input gate attributes all 648,032 reads to
sixteen instruction sites in four resident families. The static census
requires the complete port-27 reader set and each `PORTR` destination;
runtime attribution additionally requires XPC zero and the corresponding
advanced PCs (instruction address plus two). This covers the acquired
resident image and this execution, not unseen uploaded/computed readers.

The byte-pinned INT0 entry first loads `00aa` into AR2 and tests it with
`BANZ` at `322a`, before consulting mode `00ac`. A nonzero value selects
the circular-history fill at `32c5`; the delayed return commits the updated
counter through `32db`. With zero holdoff, mode 1 selects the scan filter,
mode 2 selects the capture filter, and the other observed modes select
the initial filter. Every read in the full census must match this priority
order using passively observed `00aa != 0` and `00ac`; these words never
drive a model clock or interrupt. All 242,144 unfiltered-history reads have
nonzero holdoff and all other reads have zero holdoff in this execution.

The entry's I/O operation is the existing port-`21` read/mask/write-back,
not a newly decoded receive-ready discriminator. The external reference's
decimal `pa33` denotes this same hexadecimal `0x21`; the resident immediate
port census has no `0x33` accesses. Do not create a second status register
from that notation. Nokia's
[NSE-3 manual, p. 3-32](https://www.manualshelf.com/manual/nokia/6110/5-user-guide-english/page-32.html)
independently documents a data-available strobe on the parallel interface
and a separate PCM clock/frame interface. It does not map that strobe to
a DSP port or interrupt vector. The consumer dispatch therefore does not
identify a physical receive-ready producer or justify a 32-quarter-symbol
interrupt period by itself.

| Family | Instruction starts | Reads | Reads with port-3 bit 0 clear |
| --- | --- | ---: | ---: |
| Initial filter | `3249,324f,3255,325b` | 372,160 | 81,920 |
| Unfiltered circular history | `32cd,32d0,32d3,32d6` | 242,144 | 0 |
| Scan filter | `330a,3310,3316,331c` | 32,768 | 32,064 |
| Capture filter | `33c9,33cf,33d5,33db` | 960 | 160 |

The passive observer latches port-3 writes, observes ordinary mode RAM
and the holdoff predicate, and counts every input read. It does not read I/O to obtain a control
value or feed mode RAM back into the emulator. Its 64 buckets preserve
raw mode/control values and first/last timestamps. All four instructions
within each family must have identical mode/control/count partitions;
histogram totals must equal the independent backend read count. The
checker rejects unclassified pages/sites, missing or duplicated buckets,
partial runs and changed family/control-clear totals. Its JSON report
retains the mode/control partitions without assigning application names.

In particular, 114,144 reads occur with bit 0 clear. Capture-filter reads
under control `0104` continue in the observed 1.682--1.700 s interval;
scan-filter reads under control zero continue from about 1.820 to 6.436 s.
Suppressing all INT0 delivery or sample reads with that bit alone would
remove these observed paths under the current frame-clock model. This is
not independent proof that physical hardware produces fresh post-clear
samples: the frame model, FIFO draining and stale/empty reads remain open.
Do not promote a universal gate from this emulator-only observation.
The external emulator's faster receive-block schedule remains a hypothesis
for a distinct hardware source, not a license to read firmware mode RAM
to choose an interrupt period. A FIFO/ADC-ready producer and its clock
must be established separately from the already modeled frame-wrap source.

`make check-c54x-rom4-processing KEEP_NATIVE_EVIDENCE=1` retains the
fresh own-profile run and `rf_consumers.json`; omit the retention option
for normal disposable acceptance runs. The consumer checker also accepts
`--program`, `--log` and `--output` for independently retained captures.
No physical sample rate, I/Q lane, scaling or acquisition follows from
this census. The bounded cadence observations below are earlier controls,
not replacements for this full-run ownership accounting.

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
