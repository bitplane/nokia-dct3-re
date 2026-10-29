# Native ROM4 EFR codec contracts

Bounded independent EFR encoder/decoder, DTX, erasure, recovery and homing qualifications. These are isolated algorithm fixtures, not native handset speech or physical COBBA acceptance. [COBBA control](cobba_control_boundary.md) owns that hardware boundary.

## Bounded EFR table and CRC contracts

`tools/c54x_rom4_efr_tables.py` checks the acquired NSE-1 ROM4 data image
independently against GSM parameter widths and CRC input positions:

```
python3 tools/c54x_rom4_efr_tables.py roms/noki5110/nse1_rom4_dsp_data.bin
python3 -m unittest tools.test_c54x_rom4_efr_tables
```

The 272 descriptors at data `d8c2` cover all 244 EFR speech bits, eight CRC
bits, two extra copies each of speech bits 70/120/173/223, and twelve references
to CRC parameter bit 8. The latter are padding positions, zero only while the
CRC parameter is eight-bit. Descriptor high bytes select parameters; the low
nibble uses TI BITT numbering (actual bit `15 - selector`).

The 65 descriptors at `d9d2` match the standard's protected-input sequence,
with its first eight entries reversed. This matches the routine's eight-bit
seed followed by LSB-first processing; CRC arithmetic is not executed by this
static checker. All 272 packing descriptors also match the standard's complete
channel-importance order through the recovered native storage permutation:
182 class-1 bits occupy twelve words with ten padding positions, and 78 class-2
bits occupy five words with two padding positions. Within those regions, the
native even/odd class-1 and reversed-word class-2 layouts are checked separately
from the normative ordering. An order-only mutation fails despite unchanged
descriptor counts. These table facts do not prove native speech encode/decode,
a running handset codec, or an attached audio clock.

Normative references: [GSM 06.60, table 6](https://www.etsi.org/deliver/etsi_en/301200_301299/301245/04.01.01_60/en_301245v040101p.pdf)
for the 57 parameter widths, and [TS 45.003, section 3.1.1 and table 6](https://www.etsi.org/deliver/etsi_ts/145000_145099/145003/08.02.00_60/ts_145003v080200p.pdf)
for preliminary EFR coding and channel ordering.

`make check-c54x-rom4-efr-crc` executes the acquired `c2f2` CRC routine without
replacing its instructions or its resident `2527` return hook. All 244 speech
basis vectors, zero and 32 mixed vectors pass at two RAM locations (554 cases).
The independent oracle performs systematic polynomial division using `0x11d`
over the standard's 65 protected bits, then reverses the eight-bit remainder
to match the native output representation. Every case checks balanced return,
unchanged speech parameters and four adjacent guards. The runner records binary,
source and input hashes in its isolated evidence directory.

Acceptance also reruns FR packing, encoder and decoder with their complete
frame-history replay checks. This proves the EFR CRC component, not EFR speech
encoding/decoding, bad-frame policy, handset routing or audio.

`make check-c54x-rom4-efr-packing` checks the complete static table and executes
the unchanged `c28f` packer and `c2b7` unpacker. All 244 speech basis vectors,
zero and 32 mixed patterns pass (277 vectors, three stages each). The packer
matches seventeen independently constructed words, including the CRC and
padding. Unpacking uses separately generated normative frames, not the native
packer's output; it reconstructs all 57 speech parameters and returns zero.
Flipping the packed MSB (`d30 = s24`, protected by CRC8) changes only its decoded
parameter bit and returns one for every vector. All stages check CRC parameter
representation, balanced return and adjacent memory guards.

The packing fixture explicitly selects ordinary speech (`09ba` bit 0 set).
SID/DTX transformation, repetition-error combining policy and native EFR speech
encoding remain outside its result. Native synthesis and encoding have separate
acceptance boundaries below; none of these fixtures attaches a handset clock.

## Independent EFR speech reference

EFR encoder qualification is independent of FR. Both atomic and staged
frontends pass all 163 complete speech frames: 9,291 parameters, 326 VAD/SP
flags and 2,771 packed words, with full-RAM save/replay of every frame.
`make check-c54x-rom4-efr-encoder-staged` verifies the acquired ETSI fixture
and selects `efr-encoder-pipe`; it retains the atomic gate's strict comparison
requirements. Both BIOSes load the same hashed PCM and expected-vector files,
not merely the same DSP program/data images.

The [CPU qualifications](rom4_dsp_loader.md) cover the admitted instruction
compositions and operand-retention rules. First-frame entry snapshots match
all 65,440 ordinary RAM words after reset and PCM publication. This is not
physical-pipeline or full register-phase parity. Optional `--trace-efr`
(`--trace-efr-reset` alias) retains at most 8,192 reset and 200,000
first-frame instruction observations, excluding replay; tracing cannot weaken
vector acceptance.

This is an isolated DTX-disabled encoder algorithm gate, not handset speech.
The staged clean-speech decoder, homing and DTX suites have separate acceptance
gates below. Native RF/traffic-channel activation remains a distinct,
unaccepted boundary.

The separately acquired ETSI source archive and its provenance live in
`roms/README.md`. `tools/verify_etsi_efr_reference.py` verifies its complete
archive hash before extracting to an isolated ignored run directory. The source
is unmodified; GCC uses `-std=gnu89 -O2 -fwrapv -DWMOPS=0` to build its original
makefile. All three installation outputs match their supplied byte streams:
163 encoded frames (80,196 bytes), interface-converted frames (80,522 bytes),
and 26,080 decoded samples (52,160 bytes). The input has 96 trailing samples
which the original reference does not consume as a complete frame.

Evidence records archive identity, commands, executable hashes and output
hashes. The installation checks establish a usable external software oracle,
not full GSM 06.54 conformance, native execution or handset sound. Do not copy
the reference implementation into the MAME device/CPU sources or substitute an
AMR codec on the assumption that its 12.2-kbit/s mode is identical. The next
comparison must align native initialization, parameter order, postfilter and
DTX/homing policy before interpreting a PCM mismatch as a CPU defect.

## Native EFR decoder acceptance

`make check-c54x-rom4-efr-decoder` rebuilds and verifies the external reference,
exports only the verified decoder input/PCM into ignored fixture ROM storage,
and executes acquired reset `c941` and frame wrapper `c8dc`. All 163 frames and
26,080 final signed PCM samples match the ETSI installation output bit-for-bit.
History persists between frames. Each frame is saved at its prepared-input
boundary and re-executed after restore; every ordinary data-space word
(`0060..ffff`, 65,440 words) must reproduce, not just the 160 output samples.
The native routines are unchanged; synthetic return sentinels and CPU
microtests occupy separate fixture program addresses.

`make check-c54x-rom4-efr-decoder-staged` uses the same hashed input/PCM
and the same 163-frame, 26,080-sample and full-RAM replay requirements.
Its ten-case delayed-repeat preflight remains atomic, then an explicit
frontend marker precedes staged decoder reset and all speech frames.
The required [BD READA composition](c54x_execution_contracts.md#reada-in-unconditional-bd-delay-words)
has its own 512-case cleared-pipeline replay matrix. This is algorithm and
ordering evidence, not measured physical bus timing.

The accepted input has BFI=0 and SID=0 throughout, with six TAF=1 frames.
This is not bad-frame/comfort-noise conformance, native EFR encoding, handset
transport activation or speech delivered through COBBA. The twenty-millisecond
fixture polling allowance is an execution budget, not measured codec latency.

Three CPU contracts are required by this ordinary native path:

- **NORM negative-count alias:** raw TS codes `20..2f` select right shifts
  `-16..-1`, like codes `30..3f`. The documented domain remains -16..31.
  ROM4 Pow2 at `d54e` uses T=`ffe5` with a later conditional SFTA -16,
  yielding an overall -27 shift. Treating NORM alone as -27 collapses the
  gain to zero; the alias returns 6 for exponent 2/fraction 16683, matching
  the independent reference. A [first-hand 2001 C54x report](https://ftp.fpgarelated.com/showthread/c55x/51-1.php)
  independently describes -17 behaving as -1. This supports an inferred
  execution model outside the [TI documented range](https://www.ti.com/lit/ug/spru131g/spru131g.pdf),
  not a new datasheet guarantee or a measurement of every silicon revision.
  Atomic and staged NORM share the decode; `norm-edge` is a compatibility
  profile name for 8,192 distinct alias inputs/restores with balanced replay
  checkpoints. Other TS instruction families
  are not broadened by this result.
- **High logical immediates:** AND/OR/XOR with an unsigned literal shifted
  by sixteen support all A/B source/destination selectors. The postfilter
  uses `f363 ffff` at `c588`; the staged logical-immediate bank covers
  1,536 cases/restores, including the existing 0/1/7/15 shifts and shift 16.
- **Repeated delayed-control slot:** an atomic RPT body in a BD/RETD slot
  completes before the transfer takes effect. The LSF path at `c6ff..c702`
  uses BD, RPT #9 and MVDD to publish all ten predictor-history words;
  redirecting after the first iteration leaves nine stale words and breaks
  the next speech frame. Ten focused BD/RETD cases cover counts 0/1/2/9/39,
  pointers, guards and replay from an armed-repeat/pending-control snapshot.
  This does not establish staged RPT-in-control-delay or interrupt composition.

Temporary synthesis/gain probes are not part of acceptance. The runner requires
every exact frame result, full-RAM replay and delayed-repeat marker, validated
ROM inputs and the terminal completion. Existing FR packing, encoder and
decoder gates remain independent regressions. Broader receive/DTX policy and
the separate MCU request latch and physical audio-clock boundaries remain
unresolved; ordinary EFR encoding is covered separately below.

## Native EFR encoder acceptance

`make check-c54x-rom4-efr-encoder` verifies the same external reference and
executes the acquired encoder with DTX disabled, matching the reference coder's
default invocation. The private caller follows this byte-checked ROM order:

| Role | Entry |
| --- | --- |
| Reset | `cc0e` |
| Preprocessing | `cf9a` |
| LPC analysis and preparation | `bed6`, `befa`, `bf20`, `bf92` |
| Four subframes | `c093`, with native index `0e2c` set to 0 through 3 |
| Final packing and homing-reset policy | `c273` |

These are isolated algorithm calls, not a reproduction of the handset's
scheduler or request/interrupt lifecycle. Synthetic caller instructions occupy
private fixture program memory; no acquired routine is patched. Input samples
come from verified `spch_dos.inp`, at native buffer `0cf4`. All 163 frames
retain their histories. The 57 parameters at `0e36` match all 9,291 parameters
decoded from external `spch_dos.cod`; native `09ba` VAD/SP bits match its 326
sideband values. All 2,771 packed output words at `1897` match the independent
normative packing reference applied to those external serial bits. Every frame
is restored before preprocessing and replayed, comparing all 65,440 ordinary
data-space words, not just the output. The 96 trailing source samples are not
a complete frame and are unused, as in the reference's loop.

Atomic instruction contracts supporting this gate:

- Single-condition `BC/BCD AOV/ANOV/BOV/BNOV`: tests use the original flag,
  clear only the selected sticky flag, and retain the delayed decision across
  flag-changing slots. The focused `ov-branch` gate covers all eight forms,
  four initial flag combinations and two save points (64 cases/restores).
  Flag consumption follows [SPRU131G table 4-1](https://www.ti.com/lit/ug/spru131g/spru131g.pdf).
  SPRU172C's BOV example table retains OVB, contrary to that explicit CPU-guide
  rule; the gate is model conformance, not an independent silicon measurement.
- `SRCCD` and `STRCD`: even a false predicate performs a destination read and
  write-back, and updates Xmem. `cond-reg-store` covers all twelve predicates,
  six signed 40-bit patterns, sixteen Xmem forms, four source values and two
  save points (18,432 cases/restores), including circular wrap. Existing SACCD
  arithmetic is unchanged; the separate `saccd` profile retains its 672 cases.
  These semantics follow SPRU172C 4-165/4-186; mapped peripheral timing is not
  established by the RAM fixtures.
- All twelve signed accumulator `RC` predicates now share the already decoded
  `RCD` predicate logic. `atomic-ret` covers 144 cases/restores, including
  positive guard-width values and negative values, exact taken stack reads,
  no untaken stack access and unchanged status. The staged return gate remains
  a separate regression, not newly inferred pipeline behavior.

The runner requires every exact frame, the complete history-replay result,
validated ROM/reference identities and terminal completion. This is ordinary
installation-vector acceptance, not full GSM 06.54 conformance, DTX/SID or
bad-frame policy, a production PCM clock, native handset speech or an
end-to-end call. Those transport and physical boundaries remain separate.

## Native EFR DTX conversation

**Accepted boundary:** native encoding and decoding reproduce the complete
518-frame nonzero-background conversation, including SID updates and two
resumptions of speech. This is isolated codec acceptance, not handset audio.

Both directions pass with either atomic or staged execution. Staged encoding
matches 29,526 parameters, 1,036 flags and 8,806 packed words; staged decoding
matches 82,880 PCM samples, eight SID frames, 135 erased frames and 46 nonzero
comfort-noise frames. Both include full-RAM replay. Acceptance targets are
`check-c54x-rom4-efr-dtx-encoder-staged` and
`check-c54x-rom4-efr-dtx-decoder-staged`. The
[loader contract](c54x_execution_contracts.md#staged-lms) records instruction ownership
and remaining composition limits. Staged frontend selection does not
weaken the 518-frame comparisons or nonzero comfort-noise requirements.

`make check-c54x-rom4-efr-dtx-encoder` and
`make check-c54x-rom4-efr-dtx-decoder` use a generated reference conversation:
96 quiet-background frames, all 163 complete installation speech frames,
another 96 quiet-background frames, then the same 163 speech frames. Background
samples are deterministic values from -64 through 56 in steps of eight, not a
physical microphone calibration. The original partial trailing
frame is excluded. The unmodified reference coder runs with its `dtx` argument;
its original encoder/decoder interface and decoder produce the receive input
and PCM. Each program is also run in a second fresh process and must reproduce
its output. The verifier first requires the original installation checks,
then checks local hashes for the generated input and three generated outputs.
These hashes are reproducibility pins, not published conformance vectors.
The archive/source identities, commands and coverage remain in isolated run
metadata; no reference source or speech data enters the MAME overlay or Git.

The 518-frame conversation has 375 speech and 143 inactive encoder frames,
with two resumptions of speech. The reference interface produces eight valid
SID frames and 135 unusable frames between SID updates. The reference has
nonzero PCM in 46 inactive frames, avoiding a degenerate zero-output oracle.
The native gates retain histories and require the following complete result:

- The encoder uses the same native call order as the ordinary encoder gate,
  with `0bac` bit 2 selecting DTX. All 29,526 parameters, 1,036 VAD/SP values
  and 8,806 native packed words must match the external reference and normative
  packing model; all match.
- The unchanged `c941` reset and `c8dc` decoder must reproduce all 82,880 PCM
  samples exactly, including nonzero PCM in 46 inactive frames and return to speech;
  all match.
  Both reference generation and native acceptance require this nondegenerate
  comfort-noise coverage; matching zero PCM alone is not sufficient. Each native encoder
  and decoder frame is restored at its input boundary and re-executed,
  comparing all 65,440 ordinary data-space words.

The recovered CNI path includes atomic `XORM` at `28ac`, toggling word `0e2b`.
Its implementation
follows [SPRU172C 4-203](https://www.ti.com/lit/ug/spru172c/spru172c.pdf):
a sixteen-bit memory XOR with unchanged arithmetic status. The focused `xorm`
profile checks 1,024 cases/restores over DP, SP/CPL, indirect, increment,
decrement, circular, long preincrement and absolute RAM addresses; source and
literal boundary patterns; SXM/OVM combinations; and snapshots before/after
the single read/write transaction. Decoder text also checks literal/address
ordering. This fixture adds atomic execution, not staged timing, peripheral
ownership, repeat legality or interrupt composition. The separate staged
`xorm-absolute` qualification covers retained operands and adjacent forwarding
for absolute RAM; see [the loader contract](c54x_execution_contracts.md#absolute-xorm).

The load-bearing CPU contract is XC's Access-stage predicate visibility
([SPRU131G 7.1.4, example 7-13](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)):
neither of the preceding two one-word instructions, nor the preceding two-word
instruction, has published its Execute result when XC tests its condition.
Atomic execution retains a saved two-word condition history without delaying
ordinary register writes. Immediate transfers, interrupt entry and idle discard
older history; a non-delayed conditional return drains it even when untaken.
BIO remains an external callback, not a delayed register sample.

This matters to the unchanged `cbc5..cbe2` pseudonoise helper: sampling live TC
in its paired XC/SSBX/XC/RSBX sequence incorrectly cleared feedback, turning
seed `70816958` into `1c205a56` instead of polynomial-required `7c205a56`.
Matching initial noise bits or zero-background PCM did not expose that error.
The correction is generic CPU behavior, with no codec-PC special case.

Focused `xc-status`, `xc-latency` and `xc-barrier` profiles compare atomic and
staged execution independently. Per frontend they cover 64 status cases,
384 TC/C/A/B producer-consumer cases and 12 return-barrier cases, replaying
every case across save/restore. Coverage includes one/two-word producers,
zero through three intervening NOPs, XC controlling one/two words and a taken
return whose target equals the sequential address. These are bounded visibility
tests, not physical pipeline timing validation or exhaustive compositions with
I/O stalls, interrupts, repeats and delayed transfers.

These gates target this software conversation only. The unusable frames
occur during comfort-noise intervals; active-speech recovery equality and
homing/error combinations remain separate. Invalid-SID decoder coverage is
bounded by the additional fixture below, not inherited from this clean profile.
No RF scheduling, handset PCM clock, organic native speech transport or
end-to-end calling follows from the result.

## Native EFR invalid SID input

`make check-c54x-rom4-efr-invalid-sid` derives a fixed 518-frame conversation
from the verified DTX profile. It corrupts just LTP LAG 1 bits b0/b1 at
zero-based serial positions 45/46 in SID frames 51, 95, 259 and 311.
Those positions belong to the SID codeword in
[TS 46.062 table 1](https://www.etsi.org/deliver/etsi_ts/146000_146099/146062/19.00.00_60/ts_146062v190000p.pdf).
Two codeword errors give SID=1 under the classification in
[TS 46.081 section 6.1.1](https://www.etsi.org/deliver/etsi_ts/146000_146099/146081/19.00.00_60/ts_146081v190000p.pdf),
also independently checked by the acquired reference's original classifier.
No classifier implementation or 95-entry table is imported.

The original TAF and all other payload bits remain unchanged. BFI is additionally
set at 95 and 263, providing both a bad SID=1 codeword and an uncorrupted
SID=2 codeword with a bad-frame indication. The latter is not a valid received
SID frame. There are four SID=1 frames, one bad SID=2 frame, three good SID=2
frames and 137 BFI-marked frames overall. The reference's usual `ed_iface`
rejects SID=1 rather than modeling channel errors; the separate read-only
classifier probe therefore validates every derived decoder-entry flag directly.

The installation-verified reference reproduces the generated input/output in
fresh decoder processes. Local research pins are:

| Derived input | SHA-256 |
| --- | --- |
| `invalid_sid.dec` | `ea9ecb8fa25d734bdcd5744362264a0a51d489682ad5bd12c16f5e9bfa45cc69` |
| `invalid_sid.out` | `272757031568500197fe395934096dfd2f53a87f0af935a66de14e7ff7cc7d8d` |

Both atomic and staged execution of original native `c941`/`c8dc` match all
82,880 PCM samples and replay all
65,440 ordinary RAM words per frame. Every frame independently checks the
original RX-invalid-SID bit at `09c0`: it is set for the five invalid received
SID events and clear elsewhere. Output differs from clean DTX in 101 frames,
so the input changes are not merely accepted and ignored. The staged gate is
`check-c54x-rom4-efr-invalid-sid-staged`: its short `efr-isid-pipe` BIOS loads
the same hashed program, cold data and vectors as the atomic profile. The runner
requires exactly one invalid-SID-specific staged selection marker and rejects
driver-validity errors, incomplete comparisons and missing replay evidence.

This is bounded decoder-entry coverage with independently classified input,
not native RF SID detection, all corruption patterns, homing/error combinations,
published conformance vectors or handset speech transport. No CPU, codec state,
production clock or reference PCM was altered to obtain agreement.

## Lost-SID muting and native recovery difference

`tools/run_efr_lost_sid_reference.py` prepares the untested previous-muting
branch using channel inputs, not internal state edits. Starting from the
hash-pinned 518-frame DTX input, only frames71/95 change from received SID
to unusable frame (BFI1, SID0); payload and TAF are preserved. Both updates
are scheduled (TAF1), while the initial SID at51 remains valid.
The separately acquired reference enters muting at95/96 and resumes speech
with control0501 at97; its later clean resumption at356 remains0401.
Read-only link wrapping of `rx_dtx` observes every frame's input and control.
The runner checks installation PCM, fresh-process replay and equality with
the unobserved decoder before accepting the generated output.

| Local research fixture | SHA-256 |
| --- | --- |
| `lost_sid.dec` | `98af464e95397ad9cfd14f23905060770b170ad706261766941294204ffc57d6` |
| `lost_sid.out` | `771374b3a45dcd5286b4886b8d15ad55d3c5f6beace0b56a63ab3886fcc020c0` |

Distinct native BIOS profiles `efr-lost-sid` and `efr-lsid-pipe` load this
declared input/output pair without replacing clean-DTX data. Both strict
runs match all15520 PCM samples in frames0..96, then stop at97 with86
sample differences. Thus the complete518-frame fixture is not reference-
conformant; its full PCM characterization is described below.
The observed muting flags0290/02c0 at95/96 match the reference, but native
first-speech flags at97 are0401, not0501. The acquired recovery patch
explains this flag divergence: before its RX handler, `28ee..28f3` clears
history0ab9 and ANDs09c0 withff7f. Original/replay observations at28b0 and
28f7 show02c0/history1 becoming0240/history0 with BFI0/SID0. The subsequent
RX handler therefore no longer sees prior muting and `c79b` publishes0.
Both frontends reproduce this sequence. The patch words are also pinned
by `c54x_rom4_efr_homing_path_check.py`; no instruction is bypassed.

The prediction that this input would execute the bit0100-set writer branch
is falsified for this acquired patched decoder. It would be incorrect to
force0501, disable the patch or change reference PCM to obtain a pass.
This establishes the flag-policy difference, not a complete explanation
of all86 PCM differences or a whole-ROM proof that the set branch is dead.
The strict comparison deliberately remains failing; its completed prefix
replays each frame's full ordinary RAM, not the unexecuted remainder.
No handset speech or RF result follows.

After preparing the declared reference files in the bench ROM directory:

```sh
python3 tools/run_c54x_rom4_crc.py --mame mame/mame --rompath mame/roms \
  --efr-lost-sid --observe-dtx-history
# Add --staged for the independent staged frontend; both return failure.
python3 tools/c54x_rom4_efr_dtx_history_check.py RUN/error.log \
  --lost-sid-failure-console RUN/console.log
```

The second command checks the bounded negative result and reports
`conformance_claim=False`, not a full fixture pass. The ordinary DTX
PCM/replay gate remains a separate positive regression control.

A passive semantic comparison narrows the first PCM difference to code
gain. Through frame97, both native frontends match all980 reference R2
words,980 LSF words and392 pitch gains. All code gains before97 match;
at97 native gains are8/9/12/84 versus reference8/8/8/8 (pitch is zero in
all four subframes). Native clamp-entry observations read previous-bad
history0 and previous gains10/8/9/12. Their full B accumulator candidates
have integer parts8/9/12/84, exactly the published gains. The reference
reads previous-bad history1 and previous gains10/8/8/8. Applying that
reference cap to each observed native candidate reproduces all four
reference gains. This identifies the first differing semantic boundary
and an evidenced cap-policy explanation for its values, not a proof that
every later arithmetic/synthesis boundary is equivalent.

`run_efr_lost_sid_reference.py` also generates `state.log` with the existing
external link-time state/gain observer. All518 reference frames must have
complete ordered observations; a fresh-process replay must reproduce them,
and both observed PCM files must equal the unobserved reference output.
The native observation checker requires98 original/replay frame pairs and
rejects any earlier R2/LSF/pitch or code-gain difference. Reproduce with:

```sh
python3 tools/run_c54x_rom4_crc.py --mame mame/mame --rompath mame/roms \
  --efr-lost-sid --observe-lost-sid-state
# Strict failure is retained; add --staged for the other frontend.
python3 tools/c54x_rom4_efr_lost_sid_state_check.py RUN/error.log REFERENCE/state.log
```

Neither observer writes firmware/codec state. No raw-synthesis equality,
remaining420-frame result or handset speech claim is inferred from this
prefix alone. The surviving native/reference mismatch should not be repaired
by deleting the acquired recovery patch or importing the reference codec.

The separate `efr-lsid-observe` / `efr-lso-pipe` profiles execute all518
frames without suppressing the recorded differences. Each frame captures
all160 output samples and replays65440 ordinary RAM words; the independent
checker recomputes every mismatch count from that PCM and requires complete
ordered records and terminal replay evidence. Both frontends produce the
same native PCM SHA-256
`af8a73ffe823001cf043f947616cdb5ea71f55a4d3d8b2ea6879c19ec425ae6a`.
Exactly22 frames (97..118) differ, totaling3178 samples; maximum absolute
difference2888, whole-conversation RMS114.38768665001865 sample units.
Frames119..517 match all63840 reference samples, including the later DTX
and speech resumption. Both first-speech history publications remain0;
this fixture still does not execute the bit0100-set writer alternative.

```sh
python3 tools/run_c54x_rom4_crc.py --mame mame/mame --rompath mame/roms \
  --efr-lost-sid-observe --observe-dtx-history
# Add --staged to compare the full staged result.
```

The runner retains `native.pcm`, per-frame statistics and input/source hashes.
Its result is explicitly `status=characterized`, `reference_equal=False`,
`conformance_claim=False`. The strict `--efr-lost-sid` control still fails
at97 with86 differences; no production DSP/CPU code, patch instruction,
reference PCM or internal codec state was changed to complete observation.
The first differing gain boundary is explained above; equality of every
later internal arithmetic boundary has not been established.

The complete read-only semantic observer independently covers both frontends
through all 518 original/replay frame pairs. Its reports and semantic logs
are identical across atomic and staged execution:

| Boundary | Compared words | Differences |
| --- | --- | --- |
| Saved R2 | 5,180 | None |
| Saved LSF | 5,180 | None |
| Pitch gain | 2,072 | None |
| Code gain | 2,072 | Three, all in frame 97 |

Thus the measured later PCM differences do not coincide with further saved
R2/LSF or gain differences. This narrows the remaining explanation to the
downstream excitation/synthesis/postprocessing histories after the known
gain-policy divergence; it does not prove those histories or every arithmetic
operation equivalent. Native PCM retains the same SHA-256 and 3,178 sample
differences above. The strict prefix control still rejects frame 97 with
86 differences. No codec state, patch instruction or reference output changes.

```sh
python3 tools/run_c54x_rom4_crc.py --mame mame/mame --rompath mame/roms \
  --efr-lost-sid-observe --observe-lost-sid-state
# Repeat with --staged, retaining independent PCM/replay characterization.
python3 tools/c54x_rom4_efr_lost_sid_state_check.py RUN/error.log \
  REFERENCE/state.log --complete --output RUN/semantic-characterization.json
```

Complete mode requires all 518 frame pairs, exact semantic replay, a complete
reference and the previously explained 98-frame entrance. It reports later
differences rather than accepting them as an oracle. Input-log hashes accompany
the retained report. Default checker mode still requires the strict 98-frame
prefix; neither mode claims codec conformance or handset speech.

Add `--synthesis` to the complete checker to require the original `c907`
pre-postprocessing boundary as well. All 22,792 LPC coefficient words match
the reference; 3,335 of 82,880 raw synthesis samples differ, only in frames
97..118. Raw synthesis and final PCM are both exact again from frame 119 onward.
Atomic/staged semantic logs and native PCM remain byte-identical. This rules
out a coefficient mismatch and a postprocessing-only origin for this fixture;
it does not prove every excitation/filter history or that the known three
gain differences are the sole cause of every subsequent sample difference.
The strict control still fails at frame 97 with 86 final-PCM differences.

The observer emits long vectors in 16-word debugger-command chunks while
retaining a single complete record per boundary. MAME's 128-parameter command
limit rejects a single 160-word action; absent RAW/LPC records must fail the
checker, not imply equality. Synthesis mode rejects incomplete widths, missing
boundaries and altered replay, and is available only with complete mode.

The decoder's two delayed calls at `c88c`/`c898` enter synthesis filter
`d014` with return addresses `c890`/`c89c`. Four calls per frame select
11 coefficients at `1317 + 11*subframe`, 40 excitation words through AR3,
output at `1388 + 40*subframe`, and BRC=19 for 20 paired samples. The ten
preceding output-buffer words carry synthesis history. The enhanced branch
uses input `0af2`; the other uses its current excitation pointer. Calls from
postprocessing (`c5e0`/`c608` returns) are explicitly excluded.

The read-only reference wrapper observes only `Syn_filt` calls inside
`Decoder_12k2`. Every call requires length 40/update 1 and ordered
coefficient/excitation/history vectors. Installation PCM, two fresh observed
runs and unobserved reference PCM all remain unchanged. Both native frontends
replay all 2,072 decoder filter calls and produce identical logs/PCM:

| Filter input | Compared words | Differences | Last differing subframe |
| --- | --- | --- | --- |
| Coefficients | 22,792 | None | None |
| Excitation | 82,880 | 2,576 | Frame 116, subframe 2 |
| History | 20,720 | 828 | Frame 118, subframe 2 |

Excitation has reconverged by frame 116/subframe 3 while history still differs
through frame 118/subframe 2. This identifies a history-bearing synthesis tail rather
than a later gain/coefficient divergence or postprocessing-only cause. It does
not yet establish numerical equivalence of every filter operation or sole-cause
equivalence to the frame 97 gain-policy difference. No reference or native codec
state is replaced. The strict frame 97 failure remains unchanged.

```sh
python3 tools/c54x_rom4_efr_filter_check.py RUN/error.log REFERENCE/state.log \
  --output RUN/filter-characterization.json
```

Generate the reference with `run_efr_lost_sid_reference.py`; its evidence now
requires 6,216 ordered filter vectors in each fresh observed process. The native
observer uses digit-prefixed numeric offsets so debugger accumulator symbols
`a`/`b` cannot silently replace hexadecimal offsets. The checker validates native
call ownership, vector widths/order, reference coverage and exact native replay.

`run_efr_filter_replay.py` separately checks filter arithmetic on copies of
these captured arguments. Its small I/O harness links the acquired reference's
unmodified `Syn_filt`, not a copied implementation. All 2,072 calls reproduce
all 82,880 native raw-synthesis samples exactly in both frontends. Each
calculated history update equals that call's final ten output samples; all
2,071 observed native inter-call history handoffs independently equal the
preceding output tail, including frame boundaries.

This closes the measured filter arithmetic/history handoff for these vectors,
not every CPU flag/saturation case, whole-decoder conformance or the earlier
excitation-generation differences. Arguments and history copies are supplied
only to a standalone calculator: neither decoder nor emulator state is edited,
and the acquired recovery patch and strict PCM failure remain untouched.

```sh
python3 tools/run_efr_filter_replay.py --source ACQUIRED_REFERENCE_SOURCE \
  --native RUN/error.log --output NEW_DIRECTORY
```

The calculator compiles original reference routines into an isolated executable, verifies the
original decoder's installation PCM, compares two fresh-process calculations
and checks that the original executable, source and harness hashes do not
change. The report pins arguments, calculator output and native input log.
Incomplete binary cases, truncated vectors and an inconsistent history update
are errors; arithmetic differences are explicitly reported, never silently
accepted as native speech or full-decoder conformance.

The read-only gain-mixing boundary at `c85a` separates the sharpened fixed
codebook (`0aca`, 40 words) from adaptive excitation (`[0ac9] + [1312]`).
The latter offset is data `1312` under DP=26, not the AR2 register. Each
frame must supply four offsets 0/40/80/120 and exact replay. The reference
observes the fixed vector at original `d_gain_code` entry and adaptive output
at original `Pred_lt_6` return; installation/generated PCM remains unchanged.

| Component | Compared words | Differences | First / last differing subframe |
| --- | --- | --- | --- |
| Sharpened fixed codebook | 82,880 | None | None |
| Adaptive prediction | 60,000 | 2,648 | Frame 97/subframe 2 through frame 116/subframe 3 |

Adaptive coverage is exactly the 1,500 reference speech-admitted predictor
calls; the 572 comfort-noise subframes do not execute that predictor and their
stale adaptive buffers are not compared. The admission bit must agree in both
implementations; other control bits retain the documented policy difference.
Adaptive divergence starts after the first differing code gain. Its last
differing subframe has pitch gain zero and already-equal filter input. These
observations constrain the feedback/recovery window, not every predictor
operation or sole-cause equivalence. Atomic/staged logs and native PCM remain
identical; the strict frame 97 comparison still fails with 86 differences.

```sh
python3 tools/c54x_rom4_efr_excitation_check.py RUN/error.log REFERENCE/state.log \
  --output RUN/excitation-characterization.json
```

The reference generator now requires all 2,072 gain-boundary calls and 3,572
ordered excitation vectors in both fresh processes. Wrong pointer ownership,
missing predictor calls, altered replay and short/reordered vectors are errors.

The predictor's native call at `c7f6` returns through `c7fa` and enters `d080`
with the excitation pointer in AR2, interpolation table `eaaa` in AR3,
integer lag in AR0 and signed fractional lag in A. Data `130e`/`1301`
independently retain the lag/fraction; the observer requires agreement with
the live arguments. Its 154 preceding excitation words match the reference's
allocated prediction-history window. Only speech-admitted calls are observed.

All 1,500 native/reference lag/fraction pairs match; the fixture executes
integer lags 17..142 and all six fractions -2..3. Of 231,000 history words,
9,898 differ from frame 97/subframe 2 through frame 117/subframe 2. History
therefore outlasts the differing predictor outputs; the full buffer is not
equivalent to the taps selected for a particular lag/fraction.

`run_efr_predictor_replay.py` invokes the acquired reference's unmodified
`Pred_lt_6` on copies of these native arguments and histories. Both frontends
match all 60,000 native adaptive samples exactly. The shared argument calculator
builds its own isolated executable and checks installation PCM, immutable
sources and two fresh-process outputs. The synthesis-filter control still
matches all 82,880 native samples through the same builder. No original source
tree executable is replaced and no decoder/emulator state is edited.

```sh
python3 tools/c54x_rom4_efr_predictor_check.py RUN/error.log REFERENCE/state.log \
  --output RUN/predictor-characterization.json
python3 tools/run_efr_predictor_replay.py --source ACQUIRED_REFERENCE_SOURCE \
  --native RUN/error.log --output NEW_DIRECTORY
```

The reference generator requires 1,500 ordered predictor inputs and 231,000
history words in both fresh processes. The native checker rejects wrong call
ownership, missing/short history, inconsistent saved/live arguments, altered
replay or mismatched lag/fraction. The calculator rejects truncated cases and
out-of-contract lag/fraction before invoking the original routine. This validates
predictor arithmetic for captured inputs, not every CPU flag/saturation case,
whole-decoder reference equality, every excitation-mixing operation or native
handset speech. The acquired patch and strict frame 97/86-sample failure remain
unchanged; atomic/staged semantic logs and PCM remain identical.

The gain-mixing check captures the in-place total excitation at decoder-owned
`d014` entry, after the `c86b..c86e` mixing loop. Enhancement uses separate
scratch; the total excitation remains at `[0ac9] + [1312]`. Pre-mixing adaptive
and sharpened fixed vectors come from the earlier `c85a` boundary, alongside
the already observed pitch/code gains. Four complete vectors per frame and
exact original/replay agreement are mandatory.

`run_efr_mixing_replay.py` supplies these 82-word argument records to the shared
standalone calculator. Its scalar composition invokes original reference
`L_mult`, `L_mac`, `L_shl` and rounding primitives; it is not a copied codec or
an altered reference decoder. All 2,072 calls and 82,880 native total-excitation
samples match in both frontends. Predictor and synthesis-filter controls remain
exact through the same isolated builder. The source and calculator executable
hashes, installation PCM and two fresh-process outputs remain checked.

```sh
python3 tools/run_efr_mixing_replay.py --source ACQUIRED_REFERENCE_SOURCE \
  --native RUN/error.log --output NEW_DIRECTORY
```

Thus prediction, mixing and synthesis arithmetic all match their independent
calculations on captured native arguments. This mixing result alone does not
prove every CPU saturation/flag case, enhancement gain control, sole-cause equivalence
of all later samples or whole-decoder conformance. The known gains remain
different; neither decoder/emulator state nor reference PCM is changed to
remove that difference. Native PCM and the strict frame 97/86 failure are
preserved. Truncated argument cases, short or missing total vectors and changed
replay are errors; computed arithmetic differences are reported explicitly.

The independent enhancement check observes `c546` only for caller return
`c885`: AR2 points to the subframe's total excitation, AR3 to scratch `0af2`,
and AR6=1 selects unsmoothed normalization. Smoothed postfilter callers are
excluded. Executed calls must agree with the original pitch-gain threshold
(`pitch > 2048`), with complete before/after vectors and exact replay.

`run_efr_agc_replay.py` calls the acquired reference's original `agc2` on copies
of those two input vectors. Both frontends match all 39,080 normalized samples
from 977 calls exactly. Every AGC input equals captured total excitation and
every normalized output equals the later synthesis input. The other 1,095
subframes independently prove direct total-excitation bypass into synthesis.
These checks bind the arithmetic calculation to the observed handoffs rather
than treating an isolated matching output as whole-path evidence.

```sh
python3 tools/run_efr_agc_replay.py --source ACQUIRED_REFERENCE_SOURCE \
  --native RUN/error.log --output NEW_DIRECTORY
```

The shared isolated calculator additionally pins the original gain-control and
inverse-square-root sources/table/headers. Predictor, mixing and synthesis-filter
controls remain exact; installation PCM, immutable sources and two fresh-process
outputs are still required. No implementation is transplanted into MAME and no
decoder/emulator state is changed. Smoothed AGC, other energy/CPU states,
every enhancement-input construction step, sole-cause equivalence and whole-
decoder conformance are not promoted. Native PCM, semantic replay and the strict
frame 97/86-sample failure remain unchanged.

## Native EFR active-speech erasures

Atomic and staged execution complete all 163 frames and replay 65,440 ordinary
RAM words per frame. Both produce native PCM SHA256
`a3d282aa7387a59283a9b82cea688e43b7e17b45eba603e1fdd8d626b5025515`,
with 2,874 reference differences in 26 recovery frames (first at frame 9).
`observe-c54x-rom4-efr-loss-staged` characterizes that result; it is not a
passing recovery comparison. The strict
`check-c54x-rom4-efr-erasure-decoder-staged` remains rejecting.
The staged path has independently qualified BD-delay long accumulator
multiply, pending-PSHM SP forwarding into MVMM, and absolute XORM; see
[the loader contract](c54x_execution_contracts.md#short-mapped-ar-addm).
Staged/atomic profiles share hashed program/data and vectors; selection
markers and driver-validity checks cannot be bypassed by characterization.

**Current frontier:** `make check-c54x-rom4-efr-erasure-decoder` deliberately
fails at recovery frame 9 (zero-based), with 127 of 160 PCM samples different.
Frames 0--8 match exactly, including the first erased frame. This localizes
the first final-PCM difference to recovery after a single loss. The earlier
hidden-state difference is now identified: frame 8's LSF prediction residual
uses full-precision subtraction in the native routine, unlike the reference's
truncated intermediate. A final-PCM mismatch alone is not evidence of a CPU bug.

The external reference first passes its original installation checks. A
generated input preserves every speech payload bit, SID=0 and original TAF,
changing only BFI for four fixed bursts: frame 8 (one frame), 32--34 (three),
80--87 (eight), and 120--135 (sixteen). All 163 frames are decoded twice in
fresh processes. Local SHA-256 pins cover the generated input and output;
these are research reproducibility pins, not published conformance vectors.
Every burst produces nonzero concealment differing from the clean reference;
83 output frames differ overall, including subsequent recovery effects.
No external reference code or vectors enter Git or the MAME overlay.

The strict native gate uses unchanged `c941` reset and `c8dc` decoding, retains
histories between frames, and requires every PCM sample plus full ordinary-RAM
save/replay for every frame. It rejects the partial result rather than
rebasing expected output on native execution. A separate observation fixture
below characterizes every burst without weakening this gate. Broader invalid-SID
patterns, homing/error combinations, RF scheduling and handset speech transport remain
separate boundaries.

Passive native observations and external link-time observers separate this
result from a gain-recovery hypothesis. The externally observed decoder's
complete output remains byte-identical to the unmodified reference run.
Frame 8's raw synthesis, all 44 LPC coefficients and saved LSFs match, but
all ten saved prediction residuals differ:

| Frame-8 residuals | Values (signed words) |
| --- | --- |
| Reference | 9, 155, -284, -547, -1174, -1385, -398, 249, -177, 149 |
| Native | 8, 154, -285, -548, -1175, -1386, -399, 248, -178, 148 |

For saved LSF `v`, ROM mean `m` and preceding residual `p`, the reference
computes `v - m - floor(p * 21299 / 32768)`. The native `c69a..c69c`
MAC/ST||SUB sequence instead retains the fractional prediction through
subtraction: `floor(v - m - p * 21299 / 32768)`. These expressions independently
reproduce every observed residual. At frame 9, three saved LSFs, 34 LPC
coefficients and 149 raw synthesis samples differ before the final 127-sample
PCM mismatch. Native frame-9 pitch/code gains match the observed reference;
the uploaded `2870` recovery wrapper clamps parameter indices and clears
`0ab9` before the ROM gain decoder, so that clear is not itself a lost-state
CPU defect.

[SPRU172C 4-185](https://www.ti.com/lit/ug/spru172c/spru172c.pdf) specifies
ST||SUB against the full opposite accumulator. The isolated `parallel-sub`
profile passes 128 cases/restores covering all source/destination accumulator
pairs, eight signed fractional patterns and four memory operands. It checks
the old source store even when source equals destination. This is atomic
ordinary-RAM arithmetic coverage, not staged timing, every flag/saturation
case or proof of physical DSP output. Do not truncate the CPU's accumulator
to make the reference comparison pass. The remaining question is how the
implementation's precision and recovery-policy differences account for errors, not
whether frame 9 can be matched by changing expected output or forcing state.

### Complete loss/recovery characterization

`make observe-c54x-rom4-efr-loss` uses the same hash-pinned inputs, original
reset/decoder routines and per-frame ordinary-RAM replay, but selects the
separate `efr-loss-observe` bench. It captures all 160 native PCM words per
frame and continues after a reference difference. Its terminal record says
`COMPLETE`, not conformance `PASS`. The independent checker recomputes every
frame's mismatch count and signed PCM errors from the reference file, verifies
the fixed loss schedule and ordered complete capture, and writes
`characterization.json` plus native-only `native.pcm` under ignored scratch.
The strict decoder checker rejects an observation result.

Two fresh processes reproduce all 163 frames and identical PCM/report bytes.
All 28 erased frames match reference PCM exactly. Only recovery differs:

| Erased interval (zero-based) | Differing recovery frames | First subsequent exact frame |
| --- | --- | --- |
| 8 | 9--12 | 13 |
| 32--34 | 35--48 | 49 |
| 80--87 | 88--91 | 92 |
| 120--135 | 136--139 | 140 |

The 26 differing frames contain 2,874 differing samples out of 26,080.
Maximum absolute signed-PCM difference is 1,480; whole-sequence RMS is
98.62506307975 integer PCM units. These are observations, not an acceptance
tolerance. Native PCM SHA-256 is
`a3d282aa7387a59283a9b82cea688e43b7e17b45eba603e1fdd8d626b5025515`;
the reference remains
`1c05e9951ffbe91fb25e2ddd4b349f4e563a021ebd5ad7d1e3115881ce326fc9`.
Each frame independently restores and reproduces all 65,440 ordinary RAM
words. The strict gate still fails at frame 9 with 127 differences; clean
speech, decoder homing and DTX comparisons remain independently bit-exact.

This narrows the unresolved comparison to recovery state, not missing execution
of the longer concealment bursts. No ROM arithmetic, expected samples, codec
state or conformance criterion is altered by the observation mode.

### Recovery-state comparison

The observation bench uses the CPU's Execute-entry callback, not program-read
or live-PC coincidence, to capture saved residuals/LSFs, four pitch/code gains,
raw synthesis and LPC coefficients. Replay emits no duplicate records.
Gain results are the published RAM words `1303`/`1302`, not an assumed
accumulator-return ABI. At code-gain predicate `c4d0`, the candidate is B(31--16)
as subsequently stored by original `STH B`; the observer checks opcode and DP
before reading `ab9` and `abe` as previous-bad-frame and previous gain.

The external reference has separate read-only link-time wrappers. Its original
decoder executable is not replaced, observed PCM must equal the unwrapped
reference exactly, and state records must reproduce across fresh processes.
No reference algorithm or source is imported into the driver. Reproduce with:

```sh
python3 tools/verify_etsi_efr_reference.py \
  roms/research/etsi-efr/original/ts_146053v080000p0.zip \
  --erasure-fixture --erasure-state-observer
make observe-c54x-rom4-efr-loss
python3 tools/c54x_rom4_efr_state_check.py \
  NATIVE_RUN/console.log REFERENCE_RUN/source/erasure_state.log \
  --output tmp/efr-state-comparison.json
```

The printed run directories supply `NATIVE_RUN` and `REFERENCE_RUN`. The
comparator requires all 163 ordered frame boundaries, all eight gain results
per frame, all 540 good-frame code-gain policy records on each side and the
four original recovery clears. Missing, duplicate and malformed observations
fail closed; a complete comparison still claims no conformance.

All 652 pitch gains match. Code gains differ in seven of 652 subframes:

| Recovery frame | Native code gains | Reference code gains |
| --- | --- | --- |
| 35 | 53, 59, 88, 122 | 23, 23, 23, 23 |
| 136 | 18, 57, 108, 23 | 18, 18, 18, 18 |

The original `76f8 0ab9 0000` at `28ee` changes `ab9` from 1 to 0 on
frames 9, 35, 88 and 136, before gain decoding. This is executed recovered
code, not bench initialization or a missing device response. The reference
passes previous-bad-frame=1 for those four frames and caps each good-frame
code gain at its preceding gain. All 16 reference recovery gains equal
`min(native pre-clamp candidate, reference previous gain)`. The native
publication matches its own observed flag/candidate/previous-gain predicate
for all 540 good-frame subframes. On frames 9/88 the cap is nonbinding;
on 35/136 it changes the reference output. Restoring a flag to force agreement
would undo the original firmware's policy.

Residual precision remains a separate difference: among 280 residual words
after the 28 erased frames, 180 native words are one lower and 100 match.
Saved LSFs first differ only on the four recovery frames. This extends the
earlier first-loss localization without proving every later PCM sample's
cause. Neither the policy difference nor exact concealed PCM justifies
changing CPU arithmetic, replacing reference samples or promoting handset
speech; strict whole-sequence reference equality remains unresolved.

## Native EFR decoder homing

**Accepted boundary:** `make check-c54x-rom4-efr-homing-decoder` reproduces
all 46 frames and 7,360 PCM samples of a generated external-reference homing
conversation. Each frame replays all 65,440 ordinary RAM words after restore.
This is clean decoder-homing acceptance, not error/homing combinations,
the separate encoder-homing result below or handset speech transport.

The paired `make check-c54x-rom4-efr-homing-decoder-staged` gate passes
the same complete fixture and full-RAM replays on the staged frontend.
It uses the same hashed reference input/PCM, not a separate relaxed oracle.

After verifying the original installation vectors, the unmodified reference
encoder processes two frames of the EHF sample pattern (`0008`). Its second
encoded frame supplies the canonical DHF; no pattern table is copied into
the generator. The conversation is two DHFs, the first 20 installation speech
frames, two DHFs, the same 20 speech frames, then two DHFs. Two fresh
interface/decoder process sequences must reproduce the locally pinned input
and output. Steady homing frames 0, 1, 23 and 45 emit `0008` in every sample;
the two nontrivial 20-frame speech outputs are byte-identical. Generated
vectors and external source remain private research inputs, not Git or
MAME-overlay content or published conformance vectors.

The unchanged native `c941` reset and `c8dc` wrapper handle all resets after
the initial fixture entry. No reset is injected between frames. The private
caller must supply `AR2=1344`: the first-subframe homing check consumes the
entry parameter pointer. The real caller's `c8c7` unpack path establishes it
with `c8d7..c8d9` (`7712 1343 8092`, load then postincrementing store).
The fixture checks those original words before using the calling convention.
Leaving AR2 at the previous routine's arbitrary value can leave ordinary
speech tests green while silently disabling this first homing check. Treat
that as a fixture-ABI error, not missing hardware or a reason to synthesize
a reset flag. Clean speech and nonzero DTX results remain separately gated;
active-speech erasure precision remains the distinct unresolved comparison
above.

## Native EFR homing with bad frames

**Unresolved conformance boundary:** `check-c54x-rom4-efr-homing-errors`
and its `-staged` counterpart reject the acquired decoder's reset sequence
and PCM. The separate
`observe-c54x-rom4-efr-homing-errors` captures every output and reset entry,
independently recomputes PCM differences and requires all 49 full-RAM frame
replays. A completed observation is not a conformance pass.

`observe-c54x-rom4-efr-homing-errors-staged` selects `efr-herr-pipe` with the
same hash-pinned program, cold data and vectors as the atomic bench. Both
frontends reproduce all 49 frames, 7,840 PCM samples and 65,440 ordinary RAM
words per frame. Their comparison reports differ only in the frontend label:
native PCM SHA256 is
`6175d3a503c0cd2fe6da74b6ae2fcf7bb850cfe4a0963dd999dec84761368b3b`,
with identical observed reset entries and 512 reference differences. Staged
acceptance requires exactly one frontend marker; validity errors, missing
replay, changed recovery observations or incomplete output invalidate the
characterization. The strict staged gate still rejects the reference reset
differences at frames 1, 24 and 47; parity is not conformance.

The generated input uses three canonical DHFs, speech20, three DHFs,
the same speech20 and three final DHFs. Only the first DHF in each triplet
has BFI=1 (frames 0, 23 and 46); payload, SID=0 and TAF are retained.
The installation-verified external decoder reproduces the input/output
hashes in two fresh processes. Its reset sequence contains frames
1, 2, 24, 25, 47 and 48. The native wrapper instead resets at 2, 25 and 48:
bad-frame suppression holds, but the first clean DHF after each loss fails
the full homing comparison.

The acquired recovery code explains a concrete obstruction. The canonical
parameter 45 at `1371` matches the ROM table's `000b` at entry. Recovery
executes `28dc` (`9e0f`, `SACCD A, *AR2, BLEQ`) and changes that parameter
to zero before the full comparison at `c930`. The original `c941` reset is
not reached on that frame. These are passive instruction/data observations;
the fixture must not restore the parameter, reset the codec or alter a CPU
condition to force the reference sequence. Generic SACCD behavior has its
own independent instruction fixtures. This identifies an original-code
input mutation, not every source of PCM differences or silicon validation.

The full conversation differs in 512 samples: frames 2, 24, 25, 47 and 48
have respectively 160, 96, 80, 96 and 80 differences. All three bad DHFs
match the reference. Both post-reset speech segments (3..22 and 26..45)
match all 6,400 samples exactly. The second clean DHF in each triplet is
still state-dependent in this implementation; it must not be relabeled
as the reference EHF mask merely because reset occurs at its end.
[TS 46.060 section 8](https://www.etsi.org/deliver/etsi_ts/146000_146099/146060/14.00.00_60/ts_146060v140000p.pdf)
defines homing/reset behavior; the generated fixture is not a published
conformance vector and proves no handset audio transport.

### Acquired recovery patch ownership

Read-only static and runtime checks establish the route preceding the mutation:
`c8de CALL 24a2`, `24a2 B 2692`, then the return-address-sensitive helper
at `2692..269f`. It saves AR2/ST0, locates the return address through SP+2
and compares it with `c8e0`. For that caller it restores the registers,
discards the helper return and branches to `2870`, rather than returning to
the ordinary wrapper prologue. The acquired code owns this interception;
the fixture does not install it. Recovery eventually branches from `28f7`
to the wrapper continuation `c8ff`.

The bounded eight-fragment word check in
`tools/c54x_rom4_efr_homing_path_check.py` pins this route and the clean-frame
recovery guards: `28b0` requires BFI plus SID at `0079` to be zero, then
requires history `0ab9` nonzero before the additional recovery logic.
Across all49 reference/replay frame pairs, both atomic and staged execution
take the intercepted path. Only frames1/24/47 change parameter45 from000b
to0000; each enters with BFI0 and recovery-history1. Original SACCD execution
at28dc is observed with AR2=1371. Both captured frontends retain exactly512
PCM differences and the existing reset differences; neither becomes a
conformance pass. Debug-hook SP samples differ between the frontends, so
the checker declares its frontend explicitly rather than treating them as
a common retirement boundary or physical pipeline timing evidence.

A data-space write watch independently accounts for all111 history writes:
the decoder tail publishes92 clean and6 bad-frame values (both original and
replay executions), reset clears it7 times, and the acquired recovery patch
clears it6 times. Original `c8ad..c8b3` computes `(flags[0ac8] & 4) >> 2`
and stores it to `0ab9` in a delayed return. Thus the clean recovery entry's
history1 comes from the preceding bad frame, not an external peer or a
missing initialization write. No other writer occurs in this complete
49-frame fixture; this is not a whole-ROM static absence proof. The fixture
does not reach the separately identified conditional writer at `c79b`.
That writer is exercised by the independent 518-frame DTX decoder fixture.
At `c78f`, all1036 original/replay observations have DP=0013, so direct
operand40 addresses09c0. The original `BITF #0400` admits four writer
executions, each with flags0401; `BITF #0100` then selects A=0 and the
store at `c79b` clears0ab9 with decoder state0aba=5. These masks match
`RX_FIRST_SP_FLAG` and `RX_PREV_DTX_MUTING` in the acquired ETSI EFR
reference archive's `DTX.H`. This is a first-speech-after-DTX history
adjustment, not evidence of an absent external history producer. The
fixture does not exercise the bit0100-set alternative. All82880 PCM
samples remain reference-exact, with full frame/history replay; this
does not change the separate recovery/homing-error nonconformance.
The staged DTX frontend reproduces the same two original decisions and
their two replay executions, also with bit-exact PCM. The invalid-SID
fixture supplies the same bit0100-clear coverage, not the missing set
alternative. Synthetic checker tests cover rejection and both Boolean
store values; those tests are not acquired-code execution evidence.
Reproduce with BIOS `efr-dtx-decoder`, debugger enabled and
`tools/c54x_rom4_efr_dtx_history_observe.lua` as the zero-delay autoboot
script; use `efr-dtx-dec-pipe` for staged execution or `efr-invalid-sid`
for the invalid-SID fixture. Run
`python3 tools/c54x_rom4_efr_dtx_history_check.py PATH/error.log` and retain
the ordinary fixture PCM/replay checker as well as the trace.
The checker pins the tail/reset instructions, write values and BFI agreement.
Atomic/staged write callbacks report different advanced PCs for the same
tail store; neither sampled PC is relabeled as the opcode's owner address.

Reproduce with the isolated `tms54rom4` fixture, BIOS `efr-home-errors` or
`efr-herr-pipe`, debugger enabled and
`tools/c54x_rom4_efr_homing_observe.lua` as the zero-delay autoboot script.
Then check the acquired big-endian program and isolated `error.log`:

```sh
.venv/bin/python tools/c54x_rom4_efr_homing_path_check.py mame/roms/tms54rom4/dsp_full.bin RUN_DIR/error.log
# Add --staged for efr-herr-pipe.
```

The independent full-PCM/reset checker remains mandatory alongside this
path observation. This narrows the next question to recovery history and
parameter reconstruction in the acquired patch; it does not prove that
every contributing instruction is silicon-correct or justify bypassing
the patch, restoring its edited parameter or injecting a homing reset.

## Native EFR encoder homing

**Accepted boundary:** `make check-c54x-rom4-efr-homing-encoder` reproduces
46 frames, all 2,622 parameters, 92 VAD/SP flags and 782 packed words.
Every frame replays all 65,440 ordinary RAM words from its pre-preprocessing
checkpoint. This is isolated encoder reset/restart coverage, not handset
speech, DTX/homing combinations or a physical microphone calibration.

The paired `make check-c54x-rom4-efr-homing-encoder-staged` gate passes
the same parameters, flags, packed output and full-RAM replays. The atomic
and staged modes share the codec fixture contract; only frontend selection
differs.

The generated PCM conversation contains two EHF frames (`0008` samples),
the first 20 complete installation speech frames, two EHF frames, the same
speech segment, and two final EHF frames. The independently installation-verified
reference encoder runs twice in fresh processes without DTX, requiring exact
replay and local input/output SHA-256 pins. Steady EHF frames 0, 1, 23 and 45
encode identically; the two nontrivial 20-frame speech segments also encode
identically after homing. These are generated research vectors, not published
conformance vectors, and remain outside Git and the MAME overlay.

The native fixture keeps the existing `cf9a` preprocessing, `bed6/befa/bf20/bf92`
preparation, four `c093` subframes and `c273` packing sequence. It initializes
with `cc0e` once; subsequent reset decisions belong to the original routine.
In particular, `c287..c28b` tests `0bac` bit 1 after packing and calls `cc0e`
when the EHF path requests reset. No homing flag or inter-frame reset is
injected by the fixture. Exact parameters/flags are checked independently of
normative packing, so a matching packed frame alone cannot hide a reset or
parameter-history discrepancy. Ordinary speech, nonzero DTX and decoder
homing remain independent preservation gates.
