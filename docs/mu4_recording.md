# MU4 recording and metadata contracts

Original recorder, storage, metadata and cold-catalogue observations. Metadata/cold-open acceptance is not complete file or playback acceptance. [AAC validation](aac_validation.md) owns current quantities, source identities and waveform/conformance limits; [5510 bring-up](5510_bringup.md) owns whole-handset scope.

## Original recorder candidate

Nokia's [5510 User's Guide, issue 2](https://www.instructionsmanuals.com/sites/default/files/2019-05/Nokia-5510-en.pdf)
documents recording from radio or external audio equipment, saving a named
track, and subsequent playback (printed pages 58-59). Recorded radio files
can be copied to a PC but are described as playable only on the handset
(printed page 82). This establishes an original-firmware media producer,
not the MU4 selector, codec, container format or a verified recorder bench.

Original `aa22` dispatcher `03:acf2` consumes the selector from `b9d4`
and parameter from `b9d5`. Its `03:ad3d..ad47` subtract/table branch
covers selectors `29..3c`: twenty words at extended program `02:40f5`,
with branch destinations on page 3. Independent GNU C54x disassembly and
overlay-local final-write extraction give these relevant entries:

| Selector | Handler | Verified static contract |
| --- | --- | --- |
| `30` | `03:b349` | Parameter 2 requires nonzero track count `3768` before playback selection; not an empty-media recorder entrance. |
| `31` | `03:b332` | Calls `02:9e1a`; this requires lifecycle `373e == 4` before changing state and reloading index zero. |
| `32` | `03:b30a` | Parameters 2/3 call `02:9e47/9e51`, setting `3742` to 1/2. |
| `36` | `03:b1a9` | Parameter 5 checks `374e != 0`, sets `3750 = 3`, selects `3762 = 7/8` from `ba24`, then calls `02:9e90`. |

Entry `02:9e90` itself requires `373e == 2`. It sets that lifecycle
to 8, copies original uploaded data `c0a9 = "RERSI16 "` and
`c0b2 = "BIN"` to the named-loader buffers, builds a file context
at `9d0e`, and reaches the original `02:9186` named loader at
`02:9fa8`. A successful downstream return clears `3750` and restores
`373e = 2`. This is a recorder candidate supported by file-producing
context setup and the original overlay selection, not proof that the
overlay encodes audio or emits an accepted playback file.

Complete uploaded startup with the read-only status command has
`373e = 2`, `374e = 0f36`, `3750 = 0`, `3762 = 0`, and `3768 = 0`.
The original main-loop state-1 branch at `03:aac9` advances `373e` to 2.
The older retained routine-startup snapshot has state zero; it must not be
used to infer a missing prerequisite in complete startup.

The separate `bootrecord` probe sends pin-level packet
`1e 03 aa 01 36 05 01 84 55`: payload class 1, selector `36`, parameter
5, then distinct sequence token 1. Native acknowledgement is `7f 01 55`.
It enters `02:9e90` with state 2/capacity `0f36`/mode 3, reaches
`02:9fa8` in state 8, and invokes the original loader with descriptor
index one naming `RERSI16 .BIN`, length `0002:aca2` (175,266 bytes).
The resident runtime entry is observed twice. The second entry is stale code,
not the selected overlay's startup: `aa88` writes its vector table through
data space at `2000`, branching to `0073f8`, while the separate program view
still branches to resident `02:6d62`. No firmware state is written by the
command peer.

The bounded 20-second endpoint remains state 8/mode 3/continuation 8,
with zero tracks, nine received bytes and only the three-byte request
acknowledgement transmitted. Ten data writes of byte `01` reach the NAND
port after command byte `01`; the controller reports them as unexpected.
These writes follow stale-vector execution and are not evidence for a new
NAND opcode or a recorded file. The negative-control dispatch gate proves
command/loader reachability only; it explicitly reports `music_recording=0`.
All 73,762 non-register overlay destination words match the original image
before execution (4,622 records, 73,766 total words; four MMR writes excluded).

`bootrecordram` tests static physical program/data sharing at `2000..7fff`.
The DA150's exact shared-RAM extent is unvalidated: this is a separate
diagnostic profile, not a promoted board configuration. PMST.OVLY's CPU
extended-page folding is distinct from physical RAM sharing; making this
alias conditional on OVLY breaks the original AA55 copy sequence when it
temporarily clears OVLY.

With the provisional map, the original recorder startup at `0073f8` executes.
It reads DRR without RRDY at `03:8edb`; the bench now forwards this to the
existing McBSP latched-register behavior and counts empty reads separately,
rather than asserting that every DRR read consumes a fresh character.
Native replies have class/selector pairs `1/5e` (11 words, token `80`) and
`0/02` (16 words, token `01`), with independently checked framing and XOR
checksums. The external peer acknowledges both through serial pins.
At about 6.596 seconds, original code `02:891d` writes `REL_001 ` directory
bytes after NAND program command `80`, replacing the negative control's
invalid command sequence. This is initial storage activity, not a complete
recorded file or accepted media.

The provisional shared-RAM probe completes its 20-second execution window
without an unsupported instruction. It observes 160 NAND program setups and
160 confirms, with 84,480 data bytes (160 pages of 528 bytes). This does not
verify a finalized recording. The native lifecycle remains state 9, mode 3,
continuation 8, with zero accepted tracks.

The CPU conformance suite covers 512 compact-store variants, all twelve
signed/zero A/B delayed-branch predicates over five accumulator values,
nine delayed-call cases and 128 ASM-controlled store cases. Combined
accumulator/overflow call predicates use conjunction. ASM-store coverage
uses SXM set and saturation disabled; it is not exhaustive saturation
coverage. The store encoding and
shift contract are documented in [TI SPRU172C](https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/81/6136.spru172c.pdf),
pages 4-172--174; GNU tic54x disassembly independently identifies the original
instruction stream. Next derive the original recorder stop/save command
from its receive parser, then verify storage completion and the resulting
media before claiming playback.
Do not inject file contexts, lifecycle state or a guessed media header.

The AA88 receive path is separately mapped: McBSP2 RX vector `2058`
targets `03:8543`, which queues bytes at `f7e9` using head `f9de`.
Parser `02:82c0` is pumped by `02:8418`; dispatcher `02:84cd`
obtains its request through `02:8457` into buffer `045f`.
Its selector `36` branch at `02:85a8` tests parameter `0462`:
`3` clears `045e` and calls `02:8805`, `6` sets it and calls the
same routine, `5` rejects through the default response, and `4`
clears `3750`, selects continuation `9` or `10` according to the
old continuation, and calls `03:84fc` and `03:86ec`.
This is an original control-command route, not yet proof that parameter
`4` completes and saves a recording. Reconstruct the dispatcher range
`284cd..28560` and control range `285a8..2877a` with the inventory tool
and `--segment aa88 --disassembler-little-endian`; their output SHA-256
values are `0f11d770bd6456ddcb613a9bf0823d73388bbb75ced9095d865d26cac5a5ec96`
and `95437b37da97d1f0b85161d13b4af80f0a1fac7cb4d2af279bc0ed21698cd452`.
These addresses are overlay-specific: the resident image has different
instructions at the same addresses.

`bootreccontrol` sends the evidenced parameter-4 control packet
`1e 03 aa 01 36 04 02 86 55` (sequence token 2) at emulated time
10 seconds, after both startup replies and their acknowledgements.
All nine bytes reach the native receive path and receive `7f 02 55`.
At the 20-second endpoint RX is drained/not busy, ring head and tail
`f9de/f9df` are both 15, and parser state `f9e8` is zero. Total receive
events are 24, versus the baseline's 15; native output is 33 words.
The application dispatcher receives one request at 10.667 seconds: class
`1`, selector `36`, parameter `4`. The previous-command fields `3766/3767`
are also `1/36`. Its duplicate filter at `02:84db..84ec` rejects a matching
class/selector when the selector is greater than `28` hex (GNU's immediate
`#40` is decimal), before parameter dispatch.
The application control handler at `02:85dc` therefore has zero entries.
Transport acknowledgement is not application completion or saved media.

The separate `bootrecpoll` fixture retains that direct-repeat negative
control and adds a read-only status query before parameter 4. AA88 selector
`49` at `02:856b` packages mode/state into a class-1 selector-`71` reply;
it does not write recorder lifecycle state. The query packet is
`1e 02 aa 01 49 02 fc 55`, followed, after its native reply and pin-level
acknowledgement, by control packet `1e 03 aa 01 36 04 03 87 55`.
Both packet checksums are compile-time checked. The dispatcher receives
the query at 11.154 seconds and the control at 11.201 seconds, with previous
selector `49` rather than `36`. Handler `02:85dc` executes once; the native
processing completion reaches `8009`, and the original named loader
returns to `MCUSI16 .BIN`. At 20 seconds lifecycle is 2, mode 0,
continuation `0a`, with zero tracks. Native NAND activity is 224 program
setups/confirms and 118,272 bytes (224 pages of 528 bytes), versus the
no-control baseline's 160 pages. RX/TX totals are 35/47 words, three
checksum-valid replies and two subsequent request acknowledgements.
This proves the control transition and native return, not a finalized
recording or that the handset's ordinary sender uses this exact poll sequence.
The raw media inventory and save/name grammar are recovered below; native
warm-return receipt remains unresolved before that transaction can execute.
Disk NAND stays unchanged because NVRAM saving is disabled.
Reproduce with `MU4_BOOTSTRAP_BIOS=bootrecpoll` on the same isolated gate.
The profile exports `mu4_recorder_endpoint.nand` using the NAND device's
storage serialization without issuing commands or changing CPU state.
`tools/mu4_recorder_media_inventory.py` compares it with the source and
the gate records `media_inventory.json`. The 69,206,016-byte endpoint has
SHA-256 `1e5be7246427efedfa856b63b87bb860d9cf0b34326895e1d153fbe10a960cfd`.
Only pages 34, 161, 288 and 352--372 differ; spare bytes are unchanged.
This observation includes the generic INTM-following-instruction guard and
IMR-qualified IDLE1 wake corrections. Masked pending requests no longer
resume idle code. The changed-page extent remains unchanged; the older
pre-INTM scheduling profile changed only through page 369.
The upload/control counts remain unchanged. The endpoint hash is an
observation, not an acceptance constant or codec-validity oracle.
Page 288 begins with literal `REL_001 `; page 352 begins with `ID3` and
contains literal `POCP` at data offset 110. These are raw signature
observations, not proof of a valid filesystem entry, decoded codec,
protection mechanism or playable track. No payload transformation or
protection bypass is performed. The inventory's five synthetic tests
cover page boundaries, separate spare changes, disjoint runs and size errors.

## Recorder metadata request contract

The resident AA22 selector-`36` parameter-`7` branch at `03:b1f8`
consumes three length-prefixed byte strings following the parameter:
new base filename, TPE1 artist and TIT2 title. The order is verified from
the resulting NAND header, not inferred from argument-register names. It
copies each to a local zero-terminated buffer and calls `03:8dc3` with
the firmware-owned context at data `bf1c`, operation argument zero and
the three pointer/length pairs. Return zero selects the class-1
parameter-7 success reply via `03:b6ce`, then refresh routines
`03:941d/03:bd34`; nonzero selects its failure reply. Parameter `8`
calls the same routine with operation argument one and zeroed string
arguments. These are static call contracts, not verified save/delete
semantics. The extracted dispatcher range `03:acf2..b4b0` has SHA-256
`70cc83f46258fe7b29ddb51188b62383c4055c16284f39c4643a36c0f5d71144`;
routine `03:8dc3..8f0d` has
`61c127d1a53db86b5d2a84fbc9c2407d89aa16a1ea5c96515fbadeba2ae6aa37`.

The observation-only endpoint context capture reads `bf1c..bf2b`.
Its name is `REL_    `, terminated at `bf24`, with extension `REL`
at `bf25..bf27`. This is a prefix selector, not an incomplete selected
filename: `03:8e01..8e18` compares only four name bytes using `02:e3a7`
(a bounded character comparison), then compares the extension using
`02:e36f`. The entry scan independently tests suffix bytes 4--6 for
ASCII digits and ranks their numeric value. The NAND name `REL_001 `
therefore is not excluded merely because the context contains padding.
The comparison extraction `02:e300..e420` has SHA-256
`0c867e27045d0684472c883a12bab4dbf10ffac27caab0244d3c0fbeeca2f7c3`.
No filename digits need to be injected. The `bootrecmeta` fixture sends the
mapped length-prefixed metadata through serial pins; the native reply and
endpoint NAND delta are separate observations. A scan of 48,261 AA22 extended words finds only
the two direct `bf1c` operands in these callers; indirect writes and
initialization/data references remain outside that literal census.
An exhaustive runtime write watch shows that the recorder overlay reuses
this data range for codec work and resident initialization restores the
prefix at 13.908 seconds. The broad watch is retired after harvesting that
ownership observation; the small endpoint capture remains. The endpoint
media/context export now precedes acceptance assertions, so failed runs
retain postmortem storage without issuing NAND commands or saving the source
NVRAM. An exported image is an observation, not a passing test.

The staged empty-field control reaches both file scans in `03:8dc3` and
selects `REL_001 .REL`. Its call at `03:8ec7` to updater `02:8385`
returns `3`; the outer routine takes `03:8eef` and returns `1`. This is
not the outer routine's no-match return `2`. The updater recognizes the
existing ID3 header and calls builder `02:8519` at `02:844c`. The builder
requires both metadata pointers to be nonzero and both lengths positive
(`02:8535..8543`); it returns `1` for the fixture's zero lengths. Updater
`02:8452` translates that into `3`, then the protocol reply reports failure.
Accepted title/artist lengths are capped to twelve bytes. The filename is
handled separately by the later directory-name copy at `03:8ed8`.

`bootrecstage` retains the empty-field input. `bootrecmeta` uses the same
original staged execution, input media and pin protocol, but supplies
filename `TEST`, artist `TEST`, title `MAME`. Its request has 18 payload
bytes, 24 total bytes and XOR checksum `93`; its positive acceptance requires
the native success response, not merely any parameter-7 reply. It introduces
no firmware-state writes. Both profiles require exactly one instruction-owned
startup entry; no duplicate suppression or relaxed expected count is used.

The protected earlier 69,206,016-byte NAND retains the independently created input,
whose unchanged SHA-256 is
`75a523a9c1b6a0f6d98d47812b3082717e0764a79e0956b47b2de4fa7160d6c3`.
Changed data pages are 34, 161, 288, 352--631 and 127008; no spare bytes change.
Page 288 begins `TEST` followed by a zero terminator and retains extension
`REL`; the following padding is not normalized. Page 352 begins an ID3v2.3
header with size `00000064`, then `TIT2` containing encoding byte zero plus
`MAME`, and `TPE1` containing encoding byte zero plus `TEST` (five-byte
payloads each). This corrects the static title/artist order inference and
proves these particular stored fields, not filesystem consistency, audio
completeness or playback. Resident-memory comparison still passes exactly.

In that protected artifact, post-save refresh reaches idle without an instruction exclusion and reports
one track. The endpoint hash
is `93548b594a91df0ba87acc316435b12fb6fa35da97b066f0c7141748c5d94bb8`;
directory and ID3 bytes match the fields above. Page 127008 changes after
refresh but is not yet decoded as a filesystem commit. The internal native
metadata check passes; this is not a recorded-audio or playback acceptance.

Reproduce from a completed retained run:

```sh
make check-mu4-bootstrap-original \
  MU4_BOOTSTRAP_BIOS=bootrecmeta \
  MU4_BOOTSTRAP_SOURCE_RUN=/path/to/completed-retained-run
```

Use `bootrecstage` for the empty-field rejection control. Each run copies
that bench's own NAND and checks the copy remains unchanged. The staged
runner requires exact resident/overlay comparison, one Execute-owned startup
entry, original request acknowledgement, native dispatch and the expected
success/failure reply. It does not use the atomic profile's live-PC-qualified
loader-name read tap as a staged loader identity oracle.

The fresh `bootrecmeta` gate also passes with the corrected DMA frame-count
shadow and staged memory/immediate MAC support. It reaches the native
parameter-7 success reply and the 20-second endpoint at `02:98ea`, with
one Execute-owned recorder startup and exact resident/overlay comparisons.
Its NAND SHA-256 is
`523e9417a37fd2353edbcf3ad4d3f8bdc5231c184e8ac88ec8ae9639da8fc2f8`;
it is separate from the protected earlier playback input, not a silent
replacement. Independent 7-Zip 26.00 extraction of the fixture volume reads
the 143,168-byte `TEST` file, SHA-256
`b986b3fa41950b04711981460901c21f09b8f62778616f703a7ac2868509adc8`.
The first difference from the earlier extracted file is byte offset `0564`;
the stored metadata prefix remains identical. Forced AAC probing still
reports sample rate zero, no decoded-frame count and decoder errors
(672 diagnostic lines). As with the earlier file, that is neither decoded
audio acceptance nor proof that this native container is invalid.
The independently verified result is protocol/storage reproduction; native
codec output must be checked separately against this fresh input.

## Metadata boundary comparison

The recorder bench serializes `mu4_recorder_before_metadata.nand` immediately
before transmitting the first parameter-7 request byte. This calls only the
NAND's buffer-to-stream serializer, without NAND commands, device-state
changes or emulated-time advancement. The 20-second endpoint remains
byte-identical to the fresh recorder hash above, including with this observer.
The pre-request NAND hash is
`86bef66c843aff4933045bc0a4799c90e85b139c1153b460ceb108ce95cd6a0a`.
Only physical pages 288, 352 and 127008 differ afterward; no spare bytes
change. Page 127008's role is not established by this comparison.

Independent 7-Zip 26.00 FAT extraction reads `REL_001.REL` before the
request (142,958 bytes, SHA-256
`520a27329f3c058658aa905668b9e019561c9a1a4b521cfd80f40998593da6f0`)
and `TEST` afterward (143,168 bytes, hash above). In their common extent,
exactly 33 bytes differ, all within the first 56 bytes of ID3 metadata;
all bytes from offset 56 through the earlier EOF are identical. The added
210 bytes are all `ff`, not verified audio or benign padding. `POCP` is
absent before the request too. Thus this rewrite does not remove that
marker or alter the common payload, but its EOF extension remains a
separate correctness question. Forced AAC probing of the pre-request file
also reports sample rate zero, no decoded-frame count and decoder errors;
neither readback independently validates the native container or codec.

The existing NAND inventory reports physical-page changes. Its separate
`--extracted-files` mode compares independent file readbacks, quantifying
common changed ranges and added/removed byte counts without interpreting
FAT or codec syntax. Synthetic tests cover changed ranges, appended bytes,
shortening, empty inputs, read-only CLI behavior and incompatible export
options. All 2,950 tool tests pass (one skipped).

The untouched pre-request NAND also runs the original player for 20 seconds:
`illegal=0`, PC `02:b5e1`, 87,390 completed arithmetic-loop calls, one
bootstrap/twelve resident/24 loader-API Execute entries and fourteen replies.
Its 1,524,209 TX words, 1,691,842 DIN words and 1,363,518 actual DMA source
reads are all zero, with the same hashes and `1000..137f` source range as
post-update playback. This falsifies the metadata step, including its EOF
extension, as the sole cause of the observed silence; it does not explain
the extension or establish valid recorded audio. This is a negative coverage
experiment, not a passing playback gate. Its title publication begins
packed `3030,3039,3038` (`000908`), not `MAME`; `title_verified=0` is retained,
and the existing strict playback-observation checker does not accept it.
The native audio diagnostic still intentionally fails. No verifier was
relaxed to bank this comparison.

The baseline converted-ADC fixture supplies constant `1234`/`5678` channel
values. Native filtering behavior, encoder consumption and decoded-sample
provenance must be established rather than assuming that this input should
survive unchanged. The separate varying-input fixture below exercises a
different external source without changing the baseline.

## Varying converted-input fixture

`bootrecac` uses the same original staged recorder, pin-level requests,
nonempty metadata and independent seed NAND as `bootrecmeta`. Only its
converted-ADC source changes: the left channel alternates +8192/-8192
over 64 requested frames, the right +4096/-4096 over 32. These are sample
periods, not measured physical frequencies. Generation counters determine
phase and are reset/saved with the bench; phase replay is not newly accepted
by this experiment. No firmware state or waveform is written into DSP RAM.

The complete recorder gate passes with 871,742 generated stereo frames.
Actual channel-2 source reads at data `0021` observe counts
`201120,201155,201141,201134` for `2000,e000,1000,f000`, respectively
(804,550 reads, no other values). Attribution uses the active-source-read
API, not the enabled channel's address. Another 902,505 reads are outside
that ownership selection remain unclassified for that protected artifact;
the finalized recording's separately checked all-owner census is documented
in [Recorder DRR read ownership](aac_validation.md#recorder-drr-read-ownership).
Complete delivery/drop accounting and encoder consumption are not inferred. The strict
`--recorder-input` check independently verifies square-source statistics
and both received polarities for both amplitudes. This proves varying
peripheral input, not encoder consumption or decoded audio.

The captured endpoint NAND SHA-256 is
`5b857e584ac4ebb1806cc54cc5bb6d6bd4825c6605c490f3645cf94a649014d9`.
Independent FAT extraction reads `TEST`, 143,680 bytes, SHA-256
`5b9eeb7a4d194d202a332bd4d17bddf8cd072fd7f8658a05e57b793db6b7aee5`.
Against the constant-input file, 43,567 common bytes differ, starting at
offset 1005 (decimal), and length increases by 512 bytes. Forced AAC
probing still produces errors, sample rate zero and no decoded-frame
count; format/codec validity is not established. Re-running the unchanged
constant profile on the same build reproduces NAND hash `523e9417...` above
exactly. All 2,952 tool tests pass (one skipped).

```sh
make check-mu4-bootstrap-original \
  MU4_BOOTSTRAP_BIOS=bootrecac \
  MU4_BOOTSTRAP_SOURCE_RUN=/path/to/completed-retained-run
```

Playback of this separate AC-input NAND retains the ordinary constant
playback source and the existing audio-unvalidated diagnostic. At 20 seconds
it has `illegal=0`, PC `00:3276`, a verified `MAME` title/overlay and 22
replies. Execute observations record one bootstrap, twenty resident and
forty loader-API entries, but no calls/iterations/returns in the tracked
`02:a2c6` arithmetic loop. This differs from the constant-input file's
87,390 completed calls and twelve resident entries. It is an input-dependent
earlier/different route, not evidence of a stuck filter or missing opcode.
### Playback reactivation contract

**Open question:** can an independently validated, generated AAC media
fixture separate native playback behavior from recorder-output/framing
defects? Retain the recorded files as controls. Do not force a bit, wakeup,
node, callback, saved context, method result or input value.

The first verified AA99 sessions start at 0.818115786 seconds and end at
2.368206564 (constant) / 1.726312648 (varying). Source-checked observation
covers all 3,122 words at `0200a0..020cd1`, with zero opcode mismatches:

| Execute-entry observation | Constant | Varying input |
| --- | ---: | ---: |
| Distinct visited addresses | 1,162 | 954 |
| Total entries in range | 957,582 | 336,192 |
| Parent `02a10c` | 1,478 | 0 |
| Service `020495` | 791 | 1 |
| Processing entry `0204d6` | 743 | 0 |

These are entry observations, not retirement or complete branch outcomes.
The method-9 dispatch at `0205d0` and method-15 selector at `0205ac` are
absent in varying playback; the getter's word 13 is not an evidenced cause.
If latch `aa56` remains zero, `0204d4` bypasses processing to `0205ae`.
Constant playback first writes one at 1.043100872 seconds. Fixed-time
snapshots miss transient operands and cannot establish writer ownership.
`7211 aa59` is `MVDM aa59,AR1`, a memory-to-register load
(SPRU172C 4-110), not an AR1 store. The paired `7217 aa60` loads AR7.
The referenced area contains return-like words and must not be named a
buffer descriptor from its shape.

**Original initialization and source coverage.** The AA99 literal census
covers 49,843 mapped words. Its sole `0495` at `02716a` belongs to a
count/destination/data record, not a standalone invocation packet:

| Source record | RAM destination | Initialized words |
| --- | --- | --- |
| `027167` | `804a..804f` | `0002,0495,d7b8,d796,0001,054f` |
| `027079` | `d7b0..d7b5` | `0002,0419,0000,8044,0001,054f` |

The preceding structure is
`8044..8049=ffff,80a4,0010,0000,0000,0000`.
The C-initialization parser walks 228 records from `027000` to exclusive
end `027692`; this alone does not establish the table's true beginning
or runtime initialization ownership. The only `aa56` literals are at
`0204b4,0204d0,0204d3,0205af,027a05`; the last is zero initialization.
Initialization also sets `aa59=d796` at `027a0d`. Computed aliases remain
outside this literal census. All six `804a` descriptor words match their
initialization at 0.9, 1.116666667 and 1.4 seconds in both runs, not proof
against transient corruption. After varying playback's first session,
reads at 1.741077 seconds instead yield `0002,8f56,12a2,1280` at the same
RAM addresses: identities must stay overlay/session-qualified.

**Observed activation chain.** Actual descriptor reads and source-checked
executed histories establish:
`d7b0` descriptor -> trampoline `020478..020494` / `02048f FCALA B` ->
wrapper `020419` -> `02041f CALL 0202d7` -> node `8044` insertion ->
scheduler `020341` / `020375 CALL 020478` -> service `020495`.

Both runs initially select the service descriptor at 0.834083498 seconds.
Constant playback's later callback reaches the trampoline at 1.022780485,
wrapper at 1.022780685 and enqueue helper at 1.022780785. The node is
written zero at 1.022780877, then ffff at 1.022781944 before the next
service entry at 1.022782161. Varying playback has only the initial three
node writes (zero, zero, ffff), ending at 0.834083281, throughout the
common 0.8182--1.7-second observation interval; this is below the cap of
32 per address. Source `1282,8292,fc45` near `0202d7` loads the node's
first word, writes zero and returns if the loaded word was zero;
the nonzero path updates links. The scheduler writes ffff near
`02035c..020363`. Live PC is context, not bus-owning instruction identity.

The first `d7b0` transfer comes through `0201de`; later samples also use
`02018b`. Constant playback reads `d7b4=0001`, then
`d7b0..d7b3=0002,0419,0000,8044`; all eight sampled reads of each field
match original initialization. Varying playback has no selected `d7b0`
entries in its first session and no reads in the common interval.
Helper `0201b3` entry history begins at call `02045d` in `020454`:
initial raw context is AR1=`d7b8`, AR2=`d7cb` at 0.820211608.
Constant playback's later entry at 1.022780152 has AR1=`0000`,
AR2=`d7a9`, A=`0008000000`, B=`0000023e64`. These are raw contexts,
not retained operands or proof of the referenced structure's identity.

A 128-entry bounded Execute history before the first `d7b0` callback
reaches beyond these helpers: it traverses `029c6f..029c7e`, returns
through `029bfc` to `023eea`, returns at `023ef7` to `0293da`, and reaches
`0293ed -> 020454 -> 02045d -> 0201b3 -> 0201de`. Every retained word
matches original AA99 source; its final four entries also match the
independently retained immediate callback history. The enclosing source
calls method-9 wrapper `029873` through receiver reference `15eb`, then
accesses result word `15f0` before the publication branch. Initialization
sets `15eb=aaac`; the C-initialization table sets `aaac=c686`, whose
method-9 pointer at `c698` is `0002,3e64`. This names a source-supported
method candidate consistent with the executed return, not a claim of
codec/decoder ownership or complete indirect-call closure. Varying playback
has no selected callback and therefore emits no trigger-qualified path.
The path checker rejects missing/duplicate terminals, wrong counts/indices,
source mismatches, reordered entries and disagreement with the immediate
history; the observer ring is bounded, reset/saved and never changes CPU
execution. Fresh paired runs reproduce the established counts.

Separate first-session Execute counters now establish the method split:

| Selected method entry | Constant | Varying input |
| --- | ---: | ---: |
| Entry `023e64` | 331 | 332 |
| Early-result entry `023e84` | 284 | 332 |
| Longer-body entry `023e99` | 47 | 0 |
| Epilogue return `023ef7` | 331 | 332 |
| Caller result access `0293da` | 331 | 332 |
| Publication call `0293ed` | 47 | 0 |

The varying file therefore reaches and returns from the method; it is
not missing the virtual dispatch. Both files initially take the early
path. Its observed writes include `15f0=0485`, followed by the caller's
`15f0=090a` transformation. These are numeric result words, not a decoded
error-name or codec-validity claim. The source initializes AR2 to zero,
tests `ba4f`/`ba51`, sets AR2 to one only on the nonzero/zero combination,
copies AR2 into result offset 3, then branches to `023e99` when nonzero.
The other branch sets result offset 2 to `0485` and exits. The globals'
literal references are enumerable: `ba4f` at
`023d7f,023d93,023dbf,023e74,025811,0273cd`; `ba51` at
`023da1,023db0,023dbb,023e78,0273d3`. These are references, not a closed
writer census. Their nearby setup includes comparisons with literals
`4144` and `4946`; neither that signature nor an early result alone proves
the saved file's format, validity or the missing behavior. The method
checker verifies original words, bounded per-site samples, totals and
first-session ordering without requiring a guessed successful outcome.
Both fresh runs retain prior range counts, and the full tool suite passes
(2,963 tests, one skipped).

Read-only gate taps narrow this to `ba51`, not an observed `ba4f` change.
In the common 0.8182--1.7-second interval, varying playback has 996 reads
and 332 writes of `ba4f`, all sampled values one with no value-change cap
exhaustion. Constant playback has 576/192 reads/writes with the same value.
For `ba51`, varying playback has 996 reads / 664 writes, constant 549 / 357;
both clear it to zero and later write one. Its transition samples reach
the cap of 32, so the log explicitly does not close all intermediate
transitions. Terminal totals and last-access values remain available.
These are scoped bus observations, not a new physical clock/protocol claim.

The setup at `023da0..023dbb` clears `ba51`, calls `02a81a` with `a8ab`,
and sets `ba51=1` on its nonzero-return path before calling it again.
`02a81a..02a82a` stores SP and register context; the neighbouring
`02a82b..02a83f` restores saved context. The sole immediate-call encoding
candidate for the latter is `02453d` in wrapper `024538`, which supplies
`a8ab` and stack argument one. Its ten immediate caller candidates are
`023dd1,0242ba,0242c1,02438c,02533b,02535c,02587e,025e14,025e28,025e35`.
This is not yet a runtime caller proof or a named format-error contract.
Gate verification rejects missing identities, bad ordinals/counts, wrong
time scopes and inconsistent terminal values; exhausted sample caps are
reported rather than interpreted as stability. Both fresh runs retain the
same method and range counts.

Execute history at wrapper `024538/eefe` identifies actual callers, not
just references. Varying playback's first eight selected callers are
`02535c,02587e,02587e,02587e,02587e,025e35,02587e,02587e`; constant
playback's first eight are all `02587e`. The first varying entry is
1.008694684 seconds, following return from `02510c` at `025142` and
the sequence `025356/f7b8,025357/f468,025358/f478,025359/f845,
02535b/e800,02535c/f982`. Constant playback first restores at
1.008497130 through `02587e`, following the `0256df` call and comparison
at `02587b`. These are bounded samples, not a complete caller-frequency
census or evidence that the shared caller is always an error.
Both 128-entry histories match original AA99 words and their immediate
caller tails. The same bounded-path reporter serves callback and restore
observations without changing CPU behavior. Fresh runs preserve the
method counts and full selected-range counts. Input bits, saved-stream
positions and the return-value predicates now need independent checking;
neither a numeric restore result nor a generic file probe establishes
invalid media or a missing peripheral.

**Bit-reader comparison.** The scalar routine `02510c..025142` uses
remaining count `a854`, a 32-bit high/low reservoir at `a858/a859`, word
index `b331` and base pointer `b332`. An independent host model appends a
16-bit word when the remaining count is **at most** 16 (the source branch
is BGT, not BGEQ), then extracts the requested MSB-side window and reduces
the count. A zero-width request still permits refill; the bounded scalar
domain is widths 0--16 and remaining counts 0--32. Model tests independently
compare binary-string slicing across all widths/counts and four words;
these are model tests, not executable CPU coverage of every case. The
contract pins all 55 original routine words and tests every word mutation
and a wrong-page alias.

Fresh runs retain eight initial calls and up to eight calls separately
from each of `025354` and `025e2d`, preventing one caller from starving
the other. An additional one-attempt optional-field window has its own
cap of 128 calls. Constant playback records 16 pairs; varying records 19.
All 35 raw entry/return contexts agree with the model's returned value,
remaining count, reservoir, index and unchanged base. The observations
cover widths 0, 1 and 15, not every width or all calls. Source-adjacent
literal candidates are reported separately from raw requests; neither is
promoted to a retained-operand API. Counters, selection and pairing state
are reset/saved. Malformed pairs, source/caller mismatches and out-of-session
timestamps fail the observation checker, while an actual model disagreement
is reported as a research result rather than hidden or accepted as audio.

The varying file's `025354` call at 1.008694075 seconds has raw width one,
remaining `0010`, reservoir `bc2cce39`, index `01d7`, base `162e` and next
word `3785`. It returns one, remaining `001f`, reservoir `ce393785`,
index `01d8` and next word `99c7` at 1.008694551. The local byte sequence
`bc2cce39378599c7` occurs exactly once in independently extracted `TEST`,
at byte offset 1048; the selected MSB of `ce39` is at byte 1050 and is one.
This ties the particular input window to the saved data, not a global
file-offset mapping, valid container/codec interpretation or decoder
correctness. Constant playback has no captured `025354` call in this
session, so comparing the same file offset is not a matched parsing proof.

The pulse/TNS/gain-control tail in the upstream
[FAAD2 AAC syntax parser](https://raw.githubusercontent.com/knik0/faad2/master/libfaad/syntax.c)
provides a candidate interpretation for the consecutive checks around
`025354`. The ROM's `025232` reads 2/6-bit fields followed by a 5/4-bit loop,
matching the pulse-data shape. Its parent reads a flag, optionally calls
that routine, reads another flag, optionally calls `025190`, then checks
the final bit. The uncapped first varying-input window at
1.008693067--1.008694626 contains exactly three width-one calls:
`02532f=0,025344=0,025354=1`, all agreeing with the scalar model.
The conditional data parsers are skipped in this attempt; the final one
causes the observed restore. Constant playback selects no such window
in its first session. Boundary/ordinal/source checks preserve that absence
without synthesizing a matching attempt. This strengthens the local
optional-field interpretation, but earlier section/scalefactor parsing and
the global stream position are not independently validated. No external
implementation is imported, and this observation does not prove invalid
media or justify clearing a bit.

**Do not infer ownership from aggregate counts.** Immediate-call encoding
candidates for the trampoline are `02018b,0201de,0201ec,02023d,020375`,
with counts `7,1571,2,1131,4271` / `2,1,2,601,1669`.
Helpers `020454` / `02046b` call `0201b3` at `02045d` / `020472`;
their counts 787/784 versus 1/0 sum to the respective `0201de` counts.
This neither proves nor excludes a particular callback's causal role.
Both runs skip `0201d6` and `0202a4`; that status branch is not the
distinction. Direct far-call candidates for wrapper `020419` at
`020cba,0295c8,038c40` carry preceding literals `8052,8060,807c`, not
`8044`; indirect/cross-overlay caller closure remains unproved.

**Observation safeguards.** `mu4_context_contract.py` pins source
relationships and rejects changed witnesses/extra census references.
`--playback-range --source-a00` checks the complete source range, bounded
histories, register-context pairing, callback field values and time/ordinal
scope. Histories select live AR4/AR2 only as context, not an operand-ownership
API. A live-A wrapper selector missed observed calls and is not an absence
proof; wrapper history is retained unconditionally and paired at enqueue.
Observation state is reset/saved. Lua bus taps exclude snapshots and
side-effects-disabled C++ reads. No observer writes firmware state and no
observation check accepts decoded audio.

### Saved-file ADTS candidates

Read-only `mu4_recorder_media_inventory.py --extracted-files --adts-headers`
decodes candidate fixed headers and follows declared lengths from the first
candidate. It does not invent a 372-byte grid, discard inconvenient headers,
validate CRC/payload or assume that the first candidate is the correct start.
Both independently extracted files yield the same initial chain:

| Byte Offset | Raw Fixed Header | Declared Bytes | Channel Config | Blocks Minus One |
| --- | --- | ---: | ---: | ---: |
| 995 | `fff950802e8600` | 372 | 2 | 0 |
| 1367 | `fff950802e8600` | 372 | 2 | 0 |
| 1739 | `fff950442518c7` | 296 | 1 | 3 |

The next declared boundary is 2035, with no candidate header; another
candidate exists at 2111. These headers encode profile bits 1 and sampling
index 4, but that does not establish valid AAC payload. The constant file
has 335 candidate headers, varying 336. Payload false positives, leading
data, legitimate configuration changes, storage layout and recorder defects
remain distinct possibilities. In particular, this shared chain break is
not proof that changing the converted input alone broke the encoder.
The ID3 header declares a 100-byte tag body (audio region begins at 110);
this alone does not identify subsequent framing or explain its leading data.

### Generated AAC external-media control

`mu4_generated_aac_fixture.py` authors a separate copy of this bench's own
NAND with four seconds of independently generated AAC: MPEG-2 AAC-LC,
44.1 kHz stereo, left 440 Hz and right 660 Hz. FFmpeg's
`mpeg2_aac_low` profile excludes MPEG-4 PNS; setting the ADTS MPEG-2 header
bit alone is not an equivalent encoder constraint. FFprobe checks the
configuration, host decoding must report no errors, and independent tone
measurements must retain both requested channels. This establishes input
media validity, not native DSP output or speech.

The fixture uses mtools to replace the existing short name `TEST.REL`
(long name `TEST`), then independently extracts `TEST*` through 7z and
requires byte equality before publishing NAND. The checked legacy FAT12
volume has BPB media `f8` but reserved FAT bytes `ff ff ff`; only this
specific geometry/mismatch permits mtools' compatibility override. Spare
bytes, the boot sector and everything outside the mapped partition remain
unchanged. The original NAND is never modified. Unknown layouts and
readback mismatches are errors, not reasons to weaken verification.

With FFmpeg 8.0.1 and mtools 4.0.49, repeated independent generation yields:

| Artifact | SHA-256 |
| --- | --- |
| Authored NAND | `b344ba14f993299bea895910357261ebd0da0c082f101e9f137b0ac423d4a432` |
| Extracted 65,449-byte track | `f24f23ba9eaa19ba9187a04fb2376597a04e3e75e7b05e5b38e14ba6b8109f32` |
| Host-decoded stereo s16 PCM | `fec0191741b174a8fe7fc032bf3449ed4fe06d05edd5fafc9257cd87808e7bdf` |

The encoded stream contains 174 complete declared-length ADTS frames;
host decoding yields 178,176 stereo frames. The fixture changes 131 NAND
data pages. Reproduce with the generator's source-NAND and new-output-directory
arguments, adding `--mcopy PATH` when mtools is not installed globally.

Native `bootplaystage` on this input still stops at instruction `025137`
(`f48f`, NORM), reported at PC `02513b`, with raw T=`0029` at cycle
111,871,465 after the shifted-store bypass correction below. This lies
inside the pinned bit reader. T's low six bits encode
-23, outside TI's documented -16..31 range. Plain MAME exit status zero
does not accept this run: the strict playback checker still rejects its
illegal instruction and incomplete session. No native decoded PCM is
established. Do not extend the CPU's shift range to accommodate this state.

The operand-localization capture before that correction observes raw A=`0032`,
remaining=`0009`, reservoir=`28254497`, index=`0017`, base=`162e`, next
word=`9271`, SP=`d70c`, ST1=`610f`, with call-site history `024343` at
0.993192041 seconds. Interpreting raw A as width 50 would give
`9 + 16 - 50 = -25` remaining after refill, hence shift `16 - (-25) = 41`,
matching T=`0029`. This is outside the reference model's width domain;
it explains the arithmetic but does not establish retained argument ownership
or a CPU defect. Source immediately before that call is
`024340: 62e6 0003 0005; 024343: f982 510c`.

The multiply's retained Execute-entry operands establish the immediate
predecessor: `024340` reads source `a88b`, value `000a`, literal `0005`,
with ST1=`610f` (FRCT clear), at 0.993192000 seconds. Its signed product
is 50, matching the next bit-reader entry's A. The preceding multiply at
`02433a` reads `a88a=0003` with the same literal. These are captured
pipeline operands, not late memory snapshots; the diagnostic accessor
rejects mismatched instructions and performs no bus access. Existing
MPY/MAC conformance verifies the accessor alongside operand mutation,
exact access counts and save-state replay (98,304 MPY, 262,144 MAC and
1,664 exclusion cases). This does not make the complete parser valid.

The bounded parser-record write trace observes `a888..a88c` becoming
`0001,000f,0003,000a,000e`, with `a88b=000a` written at
0.993186456 seconds (bus context PC `0242eb`). The source sequence is
`0242e6: e804; 0242e7: f982 510c; 0242e9: f495;
0242ea: 80e6 0003`: a four-bit reader call followed by a store.
The larger field-width/order sequence resembles an AAC program-configuration
element, including five-bit channel entries. This is a syntax hypothesis,
cross-checked against [FFmpeg's PCE decoder](https://ffmpeg.org/doxygen/trunk/libavcodec_2aac_2aacdec_8c_source.html),
not proof of correct stream alignment or an imported codec implementation.

More importantly, the native buffer does not retain the host-valid header.
Independent write taps on `162e..1631` observe:

| Time (seconds, first word) | Words | Bus Context PC |
| --- | --- | --- |
| 0.983259227 | `f9ff 8050 9f26 defc` | `003356` |
| 0.988100171 | `fff9 5080 269f fcde` | `0032f5` |
| 0.988777742 | `ca8b 4136 269f fcde` | `029e7a` |

The middle row matches the generated ADTS header exactly; the last row
matches the bit reader's observed initial words. Thus a later native
buffer transformation, not authoring/readback alone, lies between valid
stored AAC and parser input. Nearby source is
`029e74: e8ff 9508 18e2 0001 f2a0 8093 6dea 0002`.
Bus PCs are context, not a retained writer-identity API. Neither the
transformation's purpose nor the native external-media packaging contract
is established; do not call this proof of corruption, decryption, or a CPU
arithmetic defect.

The loop near `029e74` is a byte-packing stage, not the first observed
change: its data reads from `aa24..aa2b` are already byte-expanded output
from the processing helper. The surrounding load/AND/OR/store
sequence is consistent with packing adjacent low bytes into output words.
Earlier write taps on `aa24..aa27` distinguish unpacked input
`00ff,00f9,0050,0080` near `029e5c/029e5f` from replacement bytes near
`02b5ac/02b5b8/02b5c0/02b5cc`.
Source `02b5a1: fb82 c9ac` calls a processing helper before those stores;
`02c9ac` accesses state near `b9ef` and calls `02c62b`. This identifies
the upstream processing boundary, not its format/key initialization semantics.
Do not repair the later packer or pre-adjust external media to cancel bytes.
Host-valid AAC alone is not proof of valid native REL packaging.

### Shifted-store dependency and prefix block model

The complete AA99 helper `02c62b..02c641` selects one nibble from a 16-bit
word: `(value >> (4 * (3 - selector))) & 15`, for selectors 0..3.
It uses `6f04 0c82` (shifted store) immediately followed by `1004` (load
of the same stack location). The core omitted that store form from its
existing same-address pending-write bypass. For input `c410`, observed
indices were `2,4,8,0` instead of `c,4,1,0`: the load saw the unshifted
count rather than the pending shifted value.

[TI SPRU131G section 7.3.2](https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/81/6114.spru131g.pdf)
documents bypassing same-location on-chip reads through an internal bus
before the pending real write. The correction extends that existing path
to explicit-shift stores and shares its value calculation with the real
Execute write. No firmware address, key, file byte or shift-range guard
is special-cased. The original complete helper fails the access-count
check before the correction and passes after it: 128 cases/replays,
four selectors, eight inputs, two SXM settings and two pages, one count
read, one input read and two real count writes. Original store, Xmem,
ASM, shifted-register bank, delayed and repeated-block regressions pass.

`mu4_context_contract.py` pins all 23 selector words and all 872 words
of processing routine `02c9ac..02cd13`. Its independent scalar block model
uses 18 state words plus four 16-entry lookup tables, 16 rounds and two
16-bit halves. A read-only native snapshot captures those 82 words and
the helper's stack inputs `f9ff,8050`. After the bypass correction, the
model yields `8bca,3641`, matching native replacement bytes
`ca,8b,41,36` and the packed words above. Tests retain that captured vector,
an independent nibble/digit oracle, domain rejection and mathematical
forward/reverse controls. This establishes one block under captured state,
not initialization, all blocks, a named cipher or valid native packaging.
The earlier `ef76,bdc5` prefix belongs to the pre-correction core, not the
current fixture oracle.

The generated control still fails native playback. Keep the processing
initializer and format selection explicitly unresolved; host AAC validity
does not identify the native file contract.

### Fresh Native Recording/Playback Control

A new `bootrecac` run on the shifted-store-corrected core starts from the
untouched firmware-created NAND seed, uses ordinary serial recording/stop/
metadata requests and closes with native metadata success. Exact resident
comparison passes (48,261 program plus 3,656 data words). The converted-input
checker observes 871,742 generated stereo frames and actual DMA symbol-read
counts `201120,201155,201141,201134`; these are digital bench observations,
not analog input or valid encoding. The saved endpoint changes 285 data
pages and no spare pages. Independent 7z extraction yields:

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| Native saved NAND | 69,206,016 | `e845f9aebdf6ef0a5877fe953dd49c2a989d4c77fca794ab20c1d654492413e7` |
| Extracted `TEST` | 143,680 | `c9da011f76ca98b91d3680d032439efea68454ee87c9ad7918bb63a264a7f231` |

The new file differs from the pre-bypass varying capture in 40,478 bytes,
starting at offset 622. Its declared ID3 body remains 100 bytes, followed
by 512 zero bytes at offsets `[110,622)`. It still has 336 candidate ADTS
headers. The initial declared-length chain is `995 -> 1367 -> 1739 -> 9529`;
the third candidate declares 7,790 bytes/channel 1/two raw blocks. That is
not validated framing or a reason to trim the file to the first sync.

Independent cold `bootplaystage` reaches its complete 20-second endpoint
with `illegal=0`, verified title/overlay and a completed first AA99 session.
It still produces no nonzero samples: 1,125,070 actual channel-3 reads,
1,402,561 TX words and 1,691,842 DIN words are all zero. The strict DMA and
source-range observation checks pass; the deliberate audio-completion
diagnostic still rejects acceptance. All 17 sampled bit-reader pairs match
the bounded scalar model; method `023e64` takes its early result on all
332 calls and never publishes its longer-body result.

The first captured processing input is 32 zero bytes under the same
82-word state as the generated-media control. The independent block model
maps its first zero pair to `c641,474f`, exactly matching the bit reader's
packed words `41c6,4f47`. This supports local processing arithmetic, not
the association with a unique file offset: the file has multiple zero
regions and no retained file-cursor ownership was captured here.
The file-start/header contract remains unresolved; the more discriminating
initializer-input boundary is mapped below.
Do not remove the zero region, change a seek or synthesize header bytes to
claim playback. The recorder contains an exact copy of the 23-word nibble
helper at AA88 `03f0e6`, a software-accessible anchor for that comparison.

The Lua observer detaches all bus taps after the last 2.4-second
observation window instead of retaining hot-stack callbacks for the entire
run. A separate three-second repeat reproduces all 708 selected first-window
lines exactly and closes the taps at 2.416666667 seconds. That is observer
equivalence over the stated window, not a shorter substitute for the complete
20-second playback observation or decoded-audio acceptance.

### Recording Header and Selected Windows

**Open question:** why do seven of the first eight source-framed packets
remain rejected directly from the recorder's captured pre-transform arrays?
The selected-block phase/state and saved-byte correspondence are accounted
for across this region. Compare bit-writer inputs and their output arrays
before attributing the problem to encoder field values or CPU packing.
Partial tails, global phase ownership and complete REL framing remain open.

AA88 `03a930..03a9c5` contains fifteen ten-word field blocks. Each loads
AR6-relative source offsets `0227..0235`, places a 32-bit value and bit width in its
caller frame, then calls `03acc5`. The widths are
`12,1,2,1,2,4,1,3,1,1,1,1,13,11,2`, totaling 56 bits. The source tool
checks all 150 words through their field grammar. The passive observer
selects each frame on an actual high-word argument read; low word, width,
receiver and return address are caller-frame snapshots, not retained CPU
operand ownership. Its first eight headers supply 120 ordered records;
missing/duplicate fields, wrong calls/widths, order, scope and closure fail.

Independent MSB field concatenation produces `fff950802ea600` for the
first header (373 bytes), and `fff950802e8600` for each of the next seven
(372 bytes). The corresponding projected file positions are
`622,995,1367,1739,2111,2483,2855,3227`. Positions 995 and 1367 match
the saved seven-byte headers directly; the first header begins at the
previously observed transformed block. Do not confuse native header 2 at
995 with header 1 merely because it is the first clear sync candidate.

The apparent header-length anomaly at 1739 is explained by the transform:
source fields predict suffix `802e8600`, while saved bytes at 1742 are
`4fcdd201`. The independently captured recording state maps little-endian
words `2e80,0086` to `cd4f,01d2`, and the inverse recovers them exactly.
It is not evidence of a malformed header field or bad bit packing.

96 consecutive native transform calls at `03ee95` match the scalar block
model. The first input is `f9ff,8050`; the next seven are zero pairs,
producing `fe90,b156`. The observer captures the full 32-word byte array
before each transform as well as actual output writes. In all 96 samples,
the array's first four bytes agree with the inner caller-frame inputs.
These remain data-read-selected context snapshots, not retained CPU operands.

The inner frame places its far return at SP+8; candidate wrapper SP is
SP+10. Wrapper snapshots show source `fd4e`, local position zero and
words `16..1b` equal to `0000,0020,ffa0,0000,001c,0004`. The cursor
snapshot advances from 0 to 3,040 in exact 32-byte steps, and all 96
captured 82-word states are identical. The pinned wrapper prefix sums
arguments `1a/1b` and calls `03e69b` with the retained span and cursor.

The actual first output block plus unchanged source-array tail uniquely
matches saved `TEST` at offset 622; the checker does not assume this origin.
All 96 native output blocks plus their 28-byte source tails then match the
next 3,072 saved bytes exactly. This establishes the selected first-four/
preserve-28 correspondence across the first eight framed packets, not just
a recurring byte pattern. The concatenated captured plaintext SHA-256 is
`58b898113ee90aa6aeaf2d5e75ea3a75650d271137692e9367765c4a3ee94da8`.
It does not close the complete modulo helper, arbitrary starting phases,
later state changes or partial-tail rules.

`mu4_recorder_transform_check.py` verifies upload provenance, sample order,
time and frame domains, scalar/native outputs, input-array correspondence,
unique file origin, cursor continuity and saved chunks. Malformed evidence
fails; genuine model/file disagreements remain explicit research results
with exit 2, never coerced to match. Tests cover changed states per call,
ambiguous origins, corruption, malformed records and read-only CLI behavior.

In a host-only experiment, this inverse model processes bytes
`[622,3630)` in memory and supplies only the first 2,977 bytes to FFmpeg
8.0.1-3ubuntu2. All eight resulting headers match the source-derived values
at their declared boundaries. FFmpeg reads eight packets and decodes one
frame/1,024 samples at 44,100 Hz stereo, but rejects seven packets with
band-count errors. Exit zero is therefore not whole-stream acceptance.
No derived bytes are fed to MAME. This is partial host validation of
native-produced media, not native decoded PCM, analog audio or speech.
The same host test using the first 2,977 captured pre-transform bytes directly
gives the same one-frame/seven-error result. Transform state/phase or saved
file changes therefore do not explain those errors within the observed region;
encoder fields, CPU packing and their syntax interpretation remain open.
The earlier host test on raw bytes omitted this transform layer and cannot
by itself establish encoder failure.

### Post-Publication Input Branch

**Open question:** how does the original recorder produce the fields of the
sampled saved-data region, and is that region being interpreted under the
correct framing contract? Do not clear the rejected bit, force a successful
result or synthesize another activation to bypass the consumer gate.

After the first `0293ed` publication, a new transaction sets `15d9=1`
and reaches the restore wrapper through `02533b` at 1.017096345 seconds.
The complete 21-word local branch `02532e..025342` requests one bit,
stores it at `ba66`, skips the conditional branch if zero, and otherwise
loads `*AR1`. A zero indirect word calls `024538`; a nonzero word calls
`025232`. Read-only taps observe `ba66=1`, candidate AR1=`ac36`, and an
actual read of `ac36=0` in the matching branch context. This establishes
the local predicate, not a retained-register API or global stream validity.

The 45-word helper `025f94..025fc0` sets the receiver's first word to
zero when its selector is 2 and to one otherwise. The preceding parser
requests two bits at `025dee` and stores them at receiver+1. Actual writes
observe `ac37=2` at 1.016848687, followed by `ac36=0` near `025fa1` at
1.016849329. Earlier and later attempts instead show selectors 1/3/0
with first word one. The helper and branch are source-pinned with every-word
mutation tests; local route tests cover all four selectors and both flag
values across every 16-bit indirect word. These are scalar/source contracts,
not complete executable CPU or codec acceptance.

At the rejected flag write, remaining bits are 31, reservoir is `ce393785`,
word index is `001c`, and base is `162e`. The nearby captured words are
`ce39,3785,99c7,26f0`. Their eight bytes occur exactly once in independently
extracted canonical `TEST` (`c9da011f...231`), at offset 1050. The selected
leading bit of `ce39` is one, consistent with the captured one-bit result
and post-read reservoir. This ties the particular input window to saved
bytes, not all cursor ownership or preceding parsing alignment.

A host-only diagnostic sends unchanged bytes `[995,1739)` to FFmpeg
8.0.1-3ubuntu2's AAC
demuxer: these are the two 372-byte candidate frames at 995 and 1367,
not an authored replacement or a MAME input. It reads 2 packets/744 bytes,
decodes 0 frames/0 samples and reports band counts 19>14 and 53>49.
The native branch and independent decoder therefore both reject aspects of
this interpretation of the saved bytes. This does not prove valid frame
boundaries, an invalid complete REL container or an encoder defect: an
unrecovered transformation/framing layer remains possible. No trimmed media
is fed back to the guest and no native-audio claim follows.

### Processing-Latch Consumer

**Boundary:** the receiver word-22 and `15d9` interaction explains why the
first publication does not immediately enable the processing latch. Its
full producer/lifetime contract is not closed; the subsequent transaction
does execute, and its input branch above is the current discriminating lead.
Do not name either field as a missing readiness report from its numeric
shape, or force the latch/counter to enter the processing body.

The newly observed publication activates service `020495` at
1.0167937 seconds. `7211 aa59` / `7217 aa60` load AR1/AR7 from memory;
SPRU172C 4-110 specifies this MVDM direction. Read taps observe
`aa59=d796`, `aa60=d7b8`, `aa54=1`, `aa55=0`, `aa56=0`,
receiver word 6 (`d79c`)=1 and receiver word 22 (`d7ac`)=1.
The service reads `15d9=0` and later reads `aa56=0`, then writes the
zero latch again near the `0205ae` tail. The observations are actual data
bus events; logged PC values remain context rather than operand ownership.

The pinned 21-word gate `0204c1..0204d5` first skips its update if
receiver word 6 is zero. Otherwise it loads `15d9` shifted by 16, adds
receiver word 22 shifted by 16, stores the high half in AR2, and uses
BANZ to skip `aa56=1` when AR2 is nonzero. The observed 0+1 therefore
skips the update. `LD aa56,A; BC AEQ,05ae` then bypasses processing.
This stops before the result-flag and output-configuration calls at `0204f2`;
one result publication does not imply those consumers ran.

Receiver word 22 is initialized to 2 near `02771b` at 0.818124231 seconds
and changes to 1 near `020184` at 1.007644005. The adjacent source
`020182: ADDM #-1,*AR2` provides a decrement candidate, not a closed
writer/receiver-ownership census. Receiver word 6 changes from 0 to 1 near
`0201cc` at publication. Importantly, word 22 then reaches zero near
`020184` at 1.016818475, after the service check; `15d9` changes back to
one near `0293c6` at 1.016818725. It was one at 1.007644906, then zero
near `0293e4` immediately before publication. This is not proof of a
permanently stuck counter or missing hardware event. Follow the subsequent
transaction and why it supplies no further activation, alongside its input
region, before assigning meaning or inventing a peer.
The bounded observer removes all taps after 2.4 seconds. Its three-second
probe establishes this local path only, not a replacement for full playback.
With the added taps, all 729 existing bounded Lua observation lines reproduce
the complete corrected-core playback log exactly (excluding new service
records and the changed tap-count line). The 25 pinned source words include
the two receiver loads and the complete gate; every word mutation is tested.

### Recording Transform and Initializer Inputs

The read-only `mu4_recorder_transform_observe.lua` samples the first
observed data-path forward-transform call, excluding reviewed key-expansion
return addresses. AA88 `03f0fd..03f466` is pinned as 874 words; its
82-word state is at `fc6c..fcbd`. The observed caller return is `03ee95`;
`03edae` was not observed in its bounded window, not proved absent.
At 1.125195309 seconds the sampled input is `f9ff,8050`, representing
ADTS bytes `fff95080` in little-endian halfwords. The independent forward
model yields `2ee7,dcc0`, matching actual writes to `6e9c/6e9d` and the
independently extracted file's bytes `e72ec0dc` at offset 622. Applying
the inverse model under that recording state recovers `f9ff,8050`.
This validates the sampled block boundary, not complete AAC encoding or
a unique file-offset association. The observed full recording repeats the
unobserved NAND byte-for-byte, with the same converted-input counts.

The recording state differs from the initial playback state. Different
header/content stages could legitimately use different state; the difference
alone is not a defect. `mu4_transform_key_observe.lua` narrows it to these
initializer-frame and actual bus-read observations:

| Overlay | Buffer | Length In Caller Frame | Actual Seven-Word Read Sequence |
| --- | --- | ---: | --- |
| AA88 recording | `ff84` | 7 | `0000,0000,0000,0000,0000,0000,0000` |
| AA99 playback | `e3d0` | 7 | `0000,0000,0000,0000,0000,0000,ffff` |

The initializers are AA88 `03f7cf` and AA99 `02cd14`; their source uses
the buffer/length in its key-mixing loop. The first seven data-bus reads
cycle through all seven addresses; this is not an inference solely from
late RAM contents. Snapshots remain frame context rather than a retained
CPU argument API. Before initialization, only the first six buffer words
receive zero writes near `03e446` (recording) or `02d028` (playback).
The preceding helper is a copier, not a clear routine: its `e589` body is
`MVDD Xmem,Ymem`, with source through AR2 and destination through AR3.
Both 17-word copies (AA88 `03e438`, AA99 `02d01a`) are pinned by the
source tool. Under CPL=1/far-call layout, it reads source pointer from
`*SP(2)`, count from `*SP(3)`, and transfers incoming A to AR3. Playback's
actual argument bus reads include `d75f=0006` and `d75e=e3e2`; the latter
is the source, not the destination receiving the zero writes. The paired
snapshot records AR3=`e3d0` and return context `02ab45`, following call
`02ab43`. Its loop
writes BRC=`0005`, consistent with six copies, not evidence of a lost
seventh iteration. Zero-valued writes alone did not establish clearing.
The last destination word `e3d6` still holds
`ffff` from a prior observed write near `03c862` at 0.309186156 seconds;
no write to recording's seventh word was seen in the installed-tap epoch.
Neither fact licenses a claim about physical power-on RAM contents.

The table and six-copy observations above are pre-immediate-ALU correction.
The original AA99 prefix `02ab09..02ab15` computes
`((bit_length - 1) >> 3) + 1`, then stores the byte count at object offset 7.
For the observed 56-bit source it must produce 7. Its two-word `ADD #1,A`
is immediately followed by `LDM AL,A`. SPRU131G table 7-28 specifies zero
mapped-accumulator latency for this producer; the previous core sampled AL
before the older ADD executed, returning 6. The exact 13-word prefix
reproduces that wrong read in an isolated CPU test. Deferring that retained
mapped read until after the two-word immediate producer corrects it without
changing guest inputs or the one-word producer restriction.

The prefix passes 16 cases/restores over eight positive lengths and two pages;
960 immediate ADD/SUB/AND/OR/XOR mapped-consumer cases/restores and the
existing 3072 immediate-multiply cases also pass. This is bounded instruction
dependency acceptance, not complete Table 7-28 or silicon-phase validation.
An unchanged native NAND in a two-second playback probe now publishes
`e405=7`, passes copy count 7 with BRC=6, and initializes from pointer `e3ce`
with a seven-zero-word snapshot. Allocation moved from `e3d0`; a repeated
probe with bounded heap-neighborhood taps observes the first seven actual
initializer reads at `e3ce..e3d4`, all zero. Historical fixed-address taps
could not establish this after relocation. The older silent playback capture
alone is not evidence of corrected output; the complete repeat below is.

A complete `bootrecac` repeat on the immediate-ALU-corrected core closes
through native metadata success and reproduces the canonical NAND above
byte-for-byte (`e845f9ae...413e7`). The seed remains unchanged. Resident
comparison again checks 48,261 program and 3,656 data words; the converted
input again has 871,742 generated frames and DMA symbol counts
`201120,201155,201141,201134`. The correction therefore changes the observed
playback initialization without changing this recorded media. This is a
controlled media-generation result, not codec validity or audio acceptance.

Cold playback of that exact media completes the 20-second observation with
`illegal=0`, verified title/overlay and 22 replies. Attributed channel-3 reads
total 1,125,032, TX words 1,402,542 and DIN words 1,691,842; all are zero.
The strict DMA and source-range observation checks pass, while deliberate
audio-completion rejection remains. The first captured zero processing block
under the corrected 82-word state gives scalar `1bce,8003`, matching native
packed output `ce1b,0380`. This is captured-state arithmetic, not file-offset
ownership or valid stream alignment.

The first-session method `023e64` now has 378 calls: 377 early returns and
one longer-body/result publication through `0293ed`. Previously the varying
control had no such publication. The observed longer body writes `ac44`
at `15f5`, `0002` at `15f6` and `0ba0` at `15f7`; entering and publishing
this result does not validate AAC syntax, sample meaning or decoded output.
The downstream consumer and input branch above map that publication.
Do not reopen the resolved
six-word-copy premise or compensate with file trimming, key edits or seeks.

The reviewed initializer wrapper at `02b25c` has an immediate call at
`02b268` to `02cd14`, after copying its caller's length slot to the outgoing
frame. This explains the seven-word argument; it is not a terminator convention.
Do not compensate by changing reset RAM policy or replacing either word. No key, pointer,
length, seek or file byte is forced. The key observer detaches after
1.8 seconds; its two-second probes are local initialization evidence, not
complete recording/playback runs. Retain these bounded diagnostics until
the producer/initializer contract is normalized, then remove redundant taps.

The metadata rewrite is not a new primary suspect. Fresh pre-request
`REL_001.REL` is 143,470 bytes, SHA-256
`f2d1e98be033aaf66d473dff58e2e4df0b3828bfe4fd42753265b98028965add`.
It already contains the same zero region and no `POCP`. Compared with
post-request `TEST`, only 33 common bytes change (all before offset 56),
and 210 `ff` bytes are appended. This reproduces the earlier narrow
metadata-boundary result; it does not resolve the EOF extension or certify
record completion.
The observer retains at most eight unusual entry contexts, is reset/saved,
and never changes firmware state. Preserve original recorded media and
acceptance constraints. A separate broader AAC-LC control (MPEG-2
header flag without the MPEG-2 encoder profile) stops at `02568a`, T=`ffee`;
it is not interchangeable with the constrained control above.

The inventory handles MPEG-2/4 header IDs, CRC-dependent header size,
declared length, raw block count and file truncation. Tests include embedded
sync candidates that must not replace the declared-length chain, divergent
but parseable headers, missing boundaries and read-only CLI operation.
The complete tool suite passes (2,986 tests, one skipped), including eight
generated-media tests covering authoring boundaries and failure paths.

In the pre-bypass constant-file control, all 1,125,032 actual channel-3
source reads in `1000..137f` remain zero,
as do 1,402,542 TX and 1,691,842 DIN words. Their hashes are
`fc93daf5`/`1e4f2315`; clocks/frames remain 230,090,537/845,921. The strict
`--playback-dma` observation check passes without any changes, and the
coverage diagnostic still intentionally fails audio acceptance. Nonzero
output, if later observed, requires waveform/media provenance rather
than a weakened predicate. The varying source demonstrably reaches the
peripheral and changes the saved file, but valid encoding and decoding
remain unproved. Both sample input and file-format interpretation remain
separate from analog audio or native speech acceptance.

Public format clues must preserve the recording/import distinction.
[Nokryptia's author-written manual](https://manpage.me/index.cgi?apropos=0&manpath=FreeBSD+11.1-RELEASE+and+Ports&q=nokryptia&sektion=1)
describes MP3 packaging without encrypting or decrypting the payload; it
does not provide a REL recording decoder. It is not a plain-AAC validity
oracle for these captures. No header workaround or firmware patch from
external tools is used by the native recorder/playback acceptance path.

## Cold Saved-Track Catalogue

`bootopenstage` starts a fresh process at the original serial bootstrap,
using the staged CPU and the same explicitly provisional bus/memory model.
Its input is the untouched `bootrecmeta` endpoint above, not a RAM snapshot
or donor medium. It sends only the ordinary pin-level status request;
firmware mounts the NAND and runs scanner `03:941d` and ID3 reader
`03:9fe9` once each. Exact resident comparison passes (48,261 program and
3,656 data words). The final lifecycle is 2, mode zero and track count one.

The native reader reconstructs title `MAME` in `b953`, and the scanner
copies it to `b6c9` and packs it at `c682`. The Execute-owned witness at
`03:95af`, immediately after the packing loop, checks both the unpacked
`004d,0041,004d,0045,0000` and packed `4d41,4d45,0000` forms. `c682`
is later overwritten by storage work (observed around 9 seconds); it is
scratch storage, not a persistent endpoint invariant. Neither a late RAM
comparison nor merely entering the reader establishes this title result.

The resident also sends 16-byte class-0/selector-`02` notifications after
the initial status reply. The external peer validates length, framing,
checksum and next modulo-eight sequence, then returns the ordinary
`7f <token> 55` acknowledgement through receive pins. Four notifications
with tokens 1--4 produce 23 RX and 78 TX bytes in the verified twenty-second
window; both rings and the typed queue drain, with no response pending or
retransmission. Counts are checked against the accepted frames, not assumed
to remain the status-only profile's 11/14 bytes. Payload semantics and
physical cadence are not established by acknowledging these frames.

```sh
make check-mu4-bootstrap-original \
  MU4_BOOTSTRAP_BIOS=bootopenstage \
  MU4_BOOTSTRAP_SOURCE_RUN=/path/to/completed-retained-run \
  MU4_BOOTSTRAP_INPUT_NAND=/path/to/bootrecmeta-run/mu4_recorder_endpoint.nand
```

The source run supplies the same original ROM inputs; the explicit NAND
input is copied into an isolated cold profile and checked unchanged after
execution. Its hash remains
`93548b594a91df0ba87acc316435b12fb6fa35da97b066f0c7141748c5d94bb8`.
The gate proves cold catalogue/title recovery and settled pin transport,
not the recorded audio body, decoder selection, playback, physical MU4 reset
or a handset boot. Cold-open save/replay is not established: its 78-word TX
history exceeds the older status peer's 64-word snapshot bound.

The same cold profile with the original no-track seed is a negative control:
it completes the same 23/78-byte transport but fails the catalogue predicate
with one scanner entry, zero ID3-reader entries and zero tracks. The fixture
does not manufacture a title or accept an ordinary status reply as file proof.

## Independent Recorded-File Readback

`mu4_recorder_media_inventory.py --export-fixture-volume <new-image>` exports
the data area of the single partition created by original InitDisk. It is
specific to this fixture: physical page 1 holds the MBR; its partition LBA
31 becomes physical page 32 through original lower reader `3035`'s row+1
mapping. The BPB's hidden-sector field is 32, not 31, and is not used as a
replacement partition address. Extents, signatures and 512-byte sectors are
validated; 16 spare bytes per NAND page are omitted. Existing outputs are
refused. No filesystem is synthesized or modified by the exporter.
Thirteen inventory/export unit tests cover extents, spare omission, invalid
formats, unchanged inputs and refusal to overwrite an existing output.

```sh
python3 tools/mu4_recorder_media_inventory.py source.nand endpoint.nand \
  --export-fixture-volume recorded-volume.img
7z l -slt recorded-volume.img
7z x -orecorded-file-readback recorded-volume.img TEST
```

On the saved endpoint, 7-Zip 26.00 independently identifies FAT12 with
16,384-byte clusters, despite the BPB's `FAT16` text. It extracts one file
named `TEST`, size 143,168 bytes, occupying 147,456 allocated bytes; the
native directory separately retains extension `REL` and non-normalized
short-name padding. Exported volume size is 63,946,752 bytes (124,896
sectors), SHA-256
`c89c8e4695fa7939f9b5789a98079024a09166250e5e98be66beeaaf5242eebb`.
The extracted file's SHA-256 is
`832058a7d5042cfa1c559a42a062d7dfb38bbaeabe9e60436fb6f46b8af87021`.
This verifies independent filesystem/extent readback of these bytes, not
their audio correctness. `ffprobe` rejects the file as generic audio;
that is not proof that the original Nokia consumer rejects its REL format.
Forcing `ffprobe -f aac -count_frames` recognizes AAC-LC-like headers and
two channels but reports sample rate zero, no decoded-frame count and
repeated decode errors. Its zero process exit status is not a successful
decode. This result does not distinguish a malformed recording from REL
packaging/transforms; the extracted file hash remains unchanged.

A separate [R060/R061 patch author's report](https://retrohack.eu/gsm/nokia-5510-dsp-repair-tool/)
identifies an existing MP3 decoder and an LSE payload offset of 896 bytes.
This is a lead for checking the original LSE consumer and constructing an
independent external media fixture, not a REL specification or a reason to
patch firmware to manufacture playback acceptance.
