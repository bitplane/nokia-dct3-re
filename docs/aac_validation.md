# Native AAC validation

## Boundary

The MU4 bench executes the 5510's separate music DSP. Its captured recording
and FFmpeg comparison are diagnostic evidence, not decoder conformance or
native handset speech acceptance. The coherent fresh record/save/cold-reopen
control reaches 376 successful decoder calls with consumed input-byte coverage;
detailed arithmetic and actual decoded-bank copy correspondence cover eight
calls. A bounded actual DMA capture identifies intervening processing writes at
context `02a2ea`, rather than direct consumption of copy-helper output. Its
five-pass kernel is source-pinned and bounded nonzero output epochs match DMA;
the upstream signed scaling stage matches an independent Q12 model for 3456
actual operand/output pairs. Independent fixed-point equations also match one
64-word block through each of the ten configured filter sections. Arbitrary
coefficient settings, complete playback and codec analogue routing remain open.
A bounded 64-stereo-frame block has decoded-sample attribution through actual
shift loads/completions to codec DIN. Full-session serial and ideal control-gated
DAC/headphone accounting additionally cover EOF cleanup and restart; detailed
decoded-sample arithmetic remains bounded. This is not a physical analog/filter
model. The protected
recording and fresh before-metadata captures remain separate 375-call controls.
The fresh stored-file projection has 385 complete host-decodable packets, an
incomplete packet 386 and no packet 387 in the projection. The native stop path
acknowledges its software request, resets buffer counters, then completes;
missing external completion is not the current boundary.

The metadata size increase is explained by the original writer's unconditional
byte accounting, including in-place metadata writes. The open lifecycle
questions are the processing worker's partial-buffer policy and playback
draining. Formal codec conformance, full-session decoded-sample attribution
and physical analog output remain separate obligations.

Recording dataflow is independently bound from peripheral receive words
through DMA, workspace copies and a stack-local repack ABI to actual reads
within the next processing call. A same-endpoint host projection also carries
the captured input's signal content; see [consumed PCM and recorded content](#consumed-pcm-and-recorded-content).
Full-lifecycle transform observation now validates all 4539 calls in the
recording fixture, including initialization and 4498 data windows. Native
plaintext contains 386 complete host-decodable packets; stored bytes match
only the first 143360 native bytes (385 packets plus 140 bytes). The remaining
stored suffix diverges. A stop-time RAM capture accounts for the remaining
576 transformed bytes and 28 pending plaintext bytes; together the captured
data decodes as 387 packets / 396288 stereo frames. This is not a flushed
recording: codec conformance, persisted-tail policy and handset speech remain
separate obligations. See [full recording transform](#full-recording-transform)
and [pending stop buffers](#pending-stop-buffers). The original buffer-delivery
protocol independently acknowledges exactly the persisted prefix; at stop,
delivery is disabled with unread data and no outstanding transfer. See
[buffer delivery](#buffer-delivery).

The separate `mu4aac` ABI controls execute the original decoder with its
original initialization and explicit caller-owned input:

| Control | Verified coverage | Remaining boundary |
| --- | --- | --- |
| Generated original stream, strict extent | 172 successful packets, then native `ffff` short-input return | Original end-buffer/drain policy |
| Same original prefix plus declared packet lookahead | All 174 original packets, 178176 stereo frames, exact post-return sample conversion | Caller stops before suffix; not EOF completion |
| One/four/eight/32-call controls | Initialization, reservoir continuity and repeated caller refills | Bounded regressions, not handset playback |

The independent generated-stream comparison has maximum four s16 units of
host-reference difference over the complete original prefix. Neither these
diagnostics nor the captured recording establish formal codec conformance,
per-store ownership for every frame, or analog output.

## Independent procedure

The [MPEG audio-conformance page](https://www.mpeg.org/standards/MPEG-4/26/)
provides a [January 2023 committee draft](https://www.mpeg.org/wp-content/uploads/mpeg_meetings/141_OnLine/w22220.zip).
This is a draft, not a claim about the final published standard or its
applicability to this decoder. Its RMS/LSB procedure compares each channel
against supplied 24-bit reference waveforms, normalized to [-1, 1], before
additional output operations such as 16-bit quantization. For accuracy K:

- RMS error must be less than `2^(-(K-1)) / sqrt(12)`.
- Maximum absolute error must not exceed `2^(-(K-2))`.
- K defaults to 16 unless the applicable test specifies otherwise.

At K=16 these correspond to RMS below `1/sqrt(12)` and maximum at most 2
in signed 16-bit sample units. They are **not** acceptance limits to apply
retroactively to the current quantized FFmpeg comparison. Do not select a
lower K to accommodate observed errors. Test applicability, decoder tools,
reference precision, measurement point and alignment must first be established.
Some suite sequences have no mandatory waveform comparison procedure.

The captured media declares MPEG-2 AAC-LC. MPEG-4 LC adds tools including
PNS; obtaining a MPEG-4 vector does not establish that the native library
supports those tools. The small ADTS example package includes PNS/LTP cases
and is not a substitute for a baseline LC test. The annex's `confTools.zip`
contains ALS reference code, not an AAC comparison utility. The original
SSNRCD tool is acquired separately below.

### Original comparison-tool controls

The [ISO MPEG-2 reference-software archive](https://standards.iso.org/ittf/PubliclyAvailableStandards/c039486_ISO_IEC_13818-5_2005_Reference_Software.zip)
contains `mpeg2aac.zip`, including the original SSNRCD1.8 conformance tool.
Its README identifies the RMS/maximum-error criterion, caller-supplied signed
sample delay (default zero), and truncation to the shorter input. None of
these selects the correct AL09 alignment or authorizes a fitted offset.

Private archives live in `roms/research/mpeg2-aac-reference/`:

| Original archive | SHA-256 |
| --- | --- |
| `c039486_ISO_IEC_13818-5_2005_Reference_Software.zip` | `ba372c4069e9c96d0675e1442dbcb5a9c7f46d8ca760866bd51f185641ba759e` |
| Inner `mpeg2aac.zip` | `c7037595f0152a0f8b3847a862789dcce1f8ac2c0b2b1d84f89e916434243f01` |
| McGill `AFsp-v6r8.tar.gz` | `acf50ece855875be0233fc0848df02b106f93c7199d2410106ba27a47c2855c6` |

`aac_iso_reference_control.py` checks those identities, builds the original
SSNRCD source and [McGill's contemporaneous audio-I/O library](https://www.mmsp.ece.mcgill.ca/Documents/Downloads/AFsp/index.html)
without source edits, and records build commands, compiler version, source
and binary hashes. Six exactly aligned synthetic24-bit controls exercise
identity, small error, isolated RMS/peak failures and independent stereo
channels. Python and original-tool verdicts agree except for one deliberately
retained boundary: at exactly the permitted maximum error, the published
at-most rule and our helper pass, but SSNRCD1.8's strict`<` test fails.
The original tool is not substituted for that published boundary condition.
No delay/gain search, native sample modification or tolerance fitting occurs.

```sh
.venv/bin/python tools/aac_iso_reference_control.py \
  roms/research/mpeg2-aac-reference/c039486_ISO_IEC_13818-5_2005_Reference_Software.zip \
  roms/research/mpeg2-aac-reference/AFsp-v6r8.tar.gz \
  tmp/iso-aac-metric-controls-final
```

Use a fresh output. `tmp/iso-aac-metric-controls-final` retains the qualified build,
all generated controls, raw reports and provenance. These controls validate
selected metric outcomes, not native decoding, AL09 alignment, complete-stream
handling or codec conformance. ISO notices and AFsp's commercial-incorporation
restriction remain with the original archives: neither source, library nor
binary is included in MAME or redistributed by this project.

## Acquired Original Fixtures

Original files are held outside SCM in
`roms/research/mpeg-audio-conformance/ed2-20230125/`, from the
[official ISO electronic annex](https://standards.iso.org/iso-iec/14496/-26/ed-2/en/).
Preserve their original format and applicable ISO terms; these assets are not
project-licensed material for redistribution or inclusion in a MAME patch.

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `al00_44.mp4` | 63029 | `27017a8c971a09502587b5bd1157277fcab20433b1f2fcd31fb35f21d533f372` |
| `al00_44.wav` | 986324 | `fa1eb1a8ac484d06797f1e30a18f9ff942efa117a6311e54f664aeaeb90e5cb0` |

Both MD5 values match the original annex manifest. Local provenance sidecars
record archive member, URL, CRC and retrieval metadata. Selected-member HTTP
range retrieval avoided downloading the complete 1.2 GB compressed archive
and 9.8 GB reference archive. The original manifest, `README_audio.txt` and
MPEG draft archive are retained alongside the fixtures.

AL00 is a baseline sine-sweep candidate: the acquired MP4 is AAC-LC, mono,
44.1 kHz, with 323 access units (330752 decoded samples). Its reference is
24-bit mono at 44.1 kHz with 328704 samples. The 2048-sample difference is an
unresolved delay/trimming contract, not permission to search for a best fit.
Neither file has yet been executed through the native decoder.

### Original MPEG-2 AL09 input

The separate original ISO/IEC 13818-4:2004 suite remains available through
its [AAC directory](https://standards.iso.org/ittf/PubliclyAvailableStandards/ISO_IEC_13818-4_2004_Conformance_Testing/AAC/).
The local ignored collection `roms/research/mpeg2-aac-conformance/` contains
unchanged `compressedAdts/al09_44.adts`, `referencesWav/al09_44.wav`, the
publisher's individual MD5 files, README and retrieval provenance. Both MD5s
match the original files. These are ISO research assets, not project-licensed
MAME ROMs or generated replacements.

| Original file | Bytes | SHA-256 |
| --- | ---: | --- |
| `al09_44.adts` | 64048 | `3117cfade03058ffceaf8953727bf84e03ba9c98652cb14bd7523bda4dcb72a6` |
| `al09_44.wav` | 1047764 | `a30567dea5f0869a8a1fa9ac04c3f97876d39671ae61acf11bf7ba41b69f048a` |

The source has 343 complete CRC-bearing MPEG-2 LC mono packets at 44.1 kHz.
The reference has 349184 mono 24-bit samples (341 blocks); this difference
does not authorize fitting or silently dropping samples. The separately named
`mu4aac -bios isoal09` performs one native call; `-bios isoal09stream`
attempts all packets without appended lookahead or reservoir modifications.
The strict stream completes 342 calls / 63901 consumed bytes, with all 35011
observed input reads bound to the unchanged source, then returns `ffff`
before decoding the last 147-byte packet. The qualifier accepts this known
short-input boundary, **not** full-stream success, CRC checking or conformance.

The final rejection is now bound to the original skip helper's eight-word
reserve, not inferred from the return code. At Execute entry`02442f`,
the final call first compares candidate0 with limit`0040` and extent`0048`
while eight reservoir bits remain. Its second comparison proposes
candidate`0048` against the same limit`0040`, with no remaining bits.
The pinned unsigned predicate rejects that candidate and rolls back its
word-index increment. The early return retains cursor/base`17ee`,
end`1836`, index0 and result`ffff`. The final1040-byte window starts at
source byte63008 and ends exactly at64048: no odd source byte was discarded
from this even-sized original input. Thus odd-byte packing alone cannot
explain this rejection. The caller's required EOF/drain or lookahead policy
remains open; do not bypass the reserve or append unlabelled bytes.

`tmp/mu4-al09-tail-reserve` retains the read-only capture. The qualifier
requires both reserve observations in order between successful call342
and the early return, plus the source-pinned helper. Its350208-sample raw
and converted outputs are byte-identical to the prior strict stream.
This preserves the prefix while explaining the final-packet admission
failure; it does not establish waveform alignment or complete decoding.

At return, the observer sees source pointers `e800,f000`. Original AA99
`023df6..023dfc` compares channel count `a89c` with one and aliases stack
word 5 to word 6 for mono before conversion. Consequently only the first raw
pointer is a decoded-channel source; the other raw capture is not a second
channel. All 350208 first-source words independently reproduce both native
converted copies using the established argument-14 conversion. This is a
conversion/dataflow result, not comparison against the ISO reference waveform.
Alignment, applicable accuracy criteria, unquantized normalization and the
original caller's final-packet handling remain open.

The optional`--ffmpeg ffmpeg` diagnostic decodes the same pinned ADTS input
to mono float PCM and compares the complete native prefix at **zero offset**,
without fitted gain, lag search, silence removal or source changes. Original
argument-14 conversion fixes raw-to-s16 scaling at1/16384; this comparison
uses the first raw source before additional s16 quantization. With FFmpeg
8.0.1-3ubuntu2, both independent native captures compare350208 samples with
RMS0.8229531220323921 and maximum3.81201171875 in s16 units. FFmpeg emits
351232 samples; its final1024 samples remain explicitly uncompared. The host
PCM SHA256 is`6897eab6a00afead65511d8fa496e6006618420a5d016d7721256b1ee5f619e6`.
Both outputs start with a full1024-sample zero block; their first nonzero
indices are native1025/host1024. Thus the native startup silence is not unique
to this caller, but its cause and alignment with the shorter ISO reference
remain unproved. A zero-offset ISO-reference comparison is not aligned and
must not be promoted to a conformance result.

The MPEG-2-specific [2003 AAC conformance corrigendum,2.6.6](https://cdn.standards.iteh.ai/samples/37966/c71be35413b7498fa6124a389eee4a8c/ISO-IEC-13818-4-1998-Amd-1-1999-Cor-1-2003.pdf)
also specifies the K-bit RMS/maximum procedure with default K16. This does not
authorize applying those limits to a host-decoder comparison or selecting a
lower K to fit the measured errors. Exact AL09 applicability, the normative
reference alignment and complete-stream handling remain required. Host
metrics are reported, not thresholded into a PASS.

Place the unchanged container and ADTS file in a private `mu4aac/` ROM
directory. From an isolated working directory, run the MAME binary with
`mu4aac -bios isoal09stream -rompath PRIVATE_ROMS -video none -sound none
-nothrottle -noreadconfig -log -nonvram_save -seconds_to_run 46`, recording
console output as `console.log`. Exit 3 must report exactly
`MU4 AAC wrapper returned without a new decoded block`; then qualify with:

```sh
.venv/bin/python tools/mu4_iso_aac_check.py RUN \
  --input roms/research/mpeg2-aac-conformance/al09_44.adts \
  --reference roms/research/mpeg2-aac-conformance/al09_44.wav \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00
```

Append`--ffmpeg ffmpeg` for the optional diagnostic; its JSON records the
executable version, exact decode command, output identity and compared/tail
extents. This does not change packet-acceptance or conformance status.

### MPEG-4 AL00 ABI prerequisites

The MP4 AudioSpecificConfig bytes are `12 00 05 04 00 00 00 00`:
audio object type 2, sampling index 4, channel configuration zero. The
decoded mono layout therefore must not be substituted for a fixed channel
configuration of one when preparing input. FFprobe's container trace reports
323 samples of duration 1024, no edit-list atom, and initial padding zero;
these container observations do not explain the reference's shorter length.

The existing native bench selects original `RELSI16 .BIN`, not `AACSI16`.
Its earlier externally authored ADTS experiment reached the bit reader only
after native processing had transformed the input bytes, then failed with an
out-of-domain width in the program-configuration-like parser. Consequently
replacing `TEST.REL` with this MP4 or a remuxed ADTS stream would not be an
independent decoder test. Recover the original REL preprocessing and library
configuration, or construct a separately identified decoder-ABI bench; do not
cancel the transformation by pre-adjusting the payload. See the
[native media boundary](5510_bringup.md#generated-aac-external-media-control).

The AA99 wrapper at `023d73..023d9f` has a source-pinned 45-word framing
branch. It initializes `b332` from the input receiver and clears cursor
`b331`. With `ba50` zero it defaults `ba4f=1, ba4e=0`, then recognizes words
`4144 4946` (`ADIF`) and reverses those flags. For ADIF with `ba44` zero it
calls `0241f2`, then `02542b`, and sets `ba44=1`. A nonzero `ba50` bypasses
this detection. This proves a local ADIF-recognition/initialization path,
not complete format support or permission to construct its private state.
`mu4_context_contract.py` verifies every word against the original AA99 upload;
mutation tests reject changes at all 45 positions. The next source boundary
is the original header parser and its input requirements.

The two calls have different roles, not two codec constructors. `0241f2`
reads four eight-bit characters through `02510c`, compares the resulting
string via `029a43`, consumes the optional copyright string and fixed ADIF
fields, then invokes `0242cc` for program configuration. Its return has not
been runtime-validated on the official vector. `02542b..025440` is a
22-word source-pinned byte-alignment helper: in the nonnegative remaining-bit
domain 0..31 it consumes `a854 & 7` bits through `02510c`. It does not
initialize a codec. The subsequent `02a81a` call saves execution context
(SP, return words and registers), not an AAC header dispatcher. Preserve
those distinctions when naming the future isolated ABI fixture.

The original local reset entry is `023d41..023d59` (25 pinned words).
It receives its object pointer in A, preserves it in AR1, calls `0245d7`
and `0256cf`, clears `ba50` and `ba44`, clears object words +4 and +5,
and clears the doubleword at +6. Its caller at `023d27` first copies
three configuration words into object offsets +1..+3. These are original
state-construction paths to investigate, not authority to transplant a
captured object. The reset callees' effects, incoming configuration provenance
and full object extent must be recovered before an isolated ABI run.

`0256cf..0256da` is now independently pinned: it clears reservoir doubleword
`a858`, remaining-bit count `a854` and word `a85a`, then returns. It neither
reads media nor supplies external configuration. Disassembly of `0245d7`
shows scalar defaults (`a8c7=0`, `a85e=1`, `a85f=ffff`, `a89d=1`,
`a89e=3`, `b333=aaec`), two per-channel resets through pointers at `b335`,
and a 1024-word zeroing loop covering `af30..b32f`. Those pointers must
already be valid: calling this reset on an arbitrarily cleared object is
not yet an established ABI without the original initialization records.

Those records are now pinned as 16 words at `0273db..0273ea`:

| Original record | Data destination | Values |
| --- | --- | --- |
| `0273db` | `b333..b334` | `aaec,aaec` |
| `0273df` | `b335..b336` | `ac36,acc1` |
| `0273e3` | `b337..b338` | `ad4c,ae3e` |
| `0273e7` | `b339..b33a` | `ab64,abcd` |

Each is a two-word count/destination/data record in the original AA99
C-initialization region, not instructions constructing pointers dynamically.
A read-only `bootplaystage` snapshot at 1.116666667 seconds reproduces all
eight values. This is initialization-plus-runtime agreement, not proof that
no indirect writer can change them later. The original loader can supply
these records for a future decoder bench; copied live RAM is unnecessary.
The caller-owned input descriptor and bounded refill contract are described
below; original storage integration and EOF remain separate requirements.

The complete `0245d7..02460f` reset helper is pinned as 57 original words.
It makes 1034 data writes: six scalar defaults, four channel-state clears,
and zeros at every word of `af30..b32f` (1024 words). With original pointer
records, the four channel destinations are `a89f,a8a5,ac39,acc4`.
A passive data-write tap during original `bootplaystage` observes two complete
invocations before 1.116666667 seconds: 2068 full-mask writes, each invocation
matching all 1034 expected address/value pairs and both having identical
write order. The tap uses bus-context PC to select the routine; this is
write-footprint evidence, not an instruction-retirement or timing claim.
No reset memory outside the stated footprint was inferred from a snapshot.

Constructor `023d27..023d40` is now pinned with its table pointers (30
words total). Table `c686` method 6 selects this constructor; method 11
selects reset `023d41`. In the constructor's local stack frame, SP+4
supplies AR2 and SP+6 supplies AR3. It copies `AR3[1]` to object[1],
`AR2[9]` to object[2] and `AR2[14]` to object[3], then calls the original
reset with the object pointer in A.

A narrowed passive read tap observes configuration pointers `d43d,d43b`,
then source values `d43c=0,d446=e800,d44b=f000`. The latter two match the
subsequent raw sample buffers, not compressed-media parameters. No semantic
name is yet assigned to object[1]. Raw PC-only captures from before the
AA99 overlay contain different instructions at these same addresses and
must not be interpreted as constructor execution.

That call setup and configuration are now pinned together (30 words).
`02786b` places object `aaac` in A, stack arguments `d43d,0000,d43b`,
and calls wrapper `029870`, which selects virtual method 6. Words
`d43b..d44b` are direct original AA99 upload data, not C-initialization
records or host-generated settings. Their source values reproduce the
constructor's copied fields `aaad=0, aaae=e800, aaaf=f000`.
A completed passive write capture sees those configuration words populated
by the original loader near 0.81497 seconds, before constructor use.
The earlier configuration-provenance question is therefore closed at this
boundary; the isolated input/refill controls below do not establish the
original storage integration.

## Candidate Transport Adapter

FFmpeg 8.0.1 stream-copy from the official MP4 to ADTS produces 323 frames,
61912 bytes, SHA-256
`3ba27972ae6c81b501983ead0f50c00c87d8a580999f410f3e01a6a1691a98d9`.
This is a derived diagnostic input, not the original
official fixture or a conformance result. Comparing each original MP4 packet
by its container offset/size with each ADTS payload finds:

- Packets 1..322 are byte-identical after removal of the ADTS headers.
- Packet 0 preserves the original six bytes `00c81483ea1c` as its suffix,
  preceded by seven added bytes `a0a08000000000`.
- All ADTS declared extents account for the complete derived file.

The added prefix is the remuxer's program-configuration insertion for the
zero-channel-configuration source. `aac_transport_contract.py` independently
parses the original AAC-LC AudioSpecificConfig/PCE fields and relocates them
behind an in-band ID_PCE, recomputing alignment before the comment field.
It derives exactly `a0a08000000000` from `1200050400000000`; the first
remuxed packet is exactly that prefix plus the original six-byte payload.
The syntax boundary is cross-checked against the primary
[FFmpeg PCE parser documentation](https://www.ffmpeg.org/doxygen/trunk/libavcodec_2aac_2aacdec_8c.html).
Tests cover alignment with preserved comments, truncation, unexpected tails
and unsupported/conflicting configurations. This checks the configuration
relocation, not decoded audio. Do not describe the remux as preserving every
compressed byte unchanged or silently discard the extra prefix.

The pinned bit reader `02510c` refills directly from `b332+b331` whenever
`a854 <= 16`; it has no storage callback or input-length check. Thus an
isolated caller must provide a bounded buffer with evidenced lookahead,
observe actual reads and consumed extents, and replenish only at the
original wrapper boundary. A successful file remux does not settle this
caller-owned memory contract.

### External Filename Control

A cloned original storage fixture with the derived stream plus the existing
ID3 title/artist envelope under `TEST.AAC` does not select a decoder overlay
in the bounded two-second `bootplaystage` run. The response path completes
without illegal instructions, but both verified-AA99-session entries and
decoder calls remain zero. This is a media-selection boundary, not evidence
of unsupported AAC syntax or a failed decoder. Do not force the loader's
AAC subtype or overlay selector.

The original AA22 scanner at `039494..0394a8` compares the directory
descriptor's extension against `REL` (`bff3`) and `LSE` (`bff7`) through
`02e36f`; failure advances directory enumeration at `039570`. These 29
instruction/literal words are source-pinned with mutation coverage. A passive
snapshot confirms the `.AAC` fixture has track count `3768=0`, settled
mode `373e=2`, and no playback flags at 1.116666667 seconds. Thus this
catalogue does not admit a plain `.AAC` file even though an AAC decoder
module exists. A `.LSE` extension alone is not evidence of a valid LSE
container. Keep the original decoder ABI bench as the direct route for the
official vector rather than attempting filename variants or internal flags.

The fixture was independently extracted through 7z with byte-identical
readback. NAND SHA-256 is
`a94241b0dffcb6f6ed18bee451f3dce05af4f32fff5663709f5e59e25dab9d5a`;
track SHA-256 is
`a1542e26e9e97ec7de0a011d4dae84f096e57464362afb398daf4aefb49a815b`.
Only 125 mapped data pages differ; spare bytes and bytes outside the music
partition are preserved. The source NAND remains unchanged. Local artifacts
are in `tmp/aac-conformance/filename-fixture/` and
`tmp/native-gates/mu4-aac-filename/`; no acceptance claim depends on retaining
these scratch paths.

## Isolated Native ABI Probe

Development target `mu4aac` is separate from `mu4nand` and handset profiles.
It loads the original AA99 records (3127 records, 49843 words), applies the
228 original C-initialization records, calls constructor `023d27` with the
original configuration, then calls processing wrapper `023d5d` once. Only
its synthetic ABI stack/input descriptor and the first 2048 derived input
bytes are supplied by the bench. It does not restore a live snapshot,
patch original instructions, or modify a booting phone's state.

The present probe deliberately fails closed: it returns `ffff`, index
`03f9`, remaining bits `0011`, with no illegal instruction and no ADTS
header return. Original sync search `024457..024465` explains the result:
it requests 15 bits and compares against `7ffc`. This accepts first words
`fff8/fff9` (MPEG-2 ID, layer zero) and rejects the official remux's `fff1`
(MPEG-4 ID). All 15 words are pinned and the first-word predicate is tested
over all 65536 possibilities. Do not relax that native predicate to make
the official vector run.

A diagnostic MPEG-2 transport adaptation would be a separately identified
derived control, requiring an explicit payload-tool compatibility assessment;
it is not an untouched official MPEG-2 conformance vector. No native block,
sample accuracy, full input-read bound or complete fixture ABI is accepted
from this failed probe. Preserve this distinction when adding further tests.

BIOS `mpeg2` uses the independently generated MPEG-2 AAC-LC stereo tone
control from `mu4_generated_aac_fixture.py`, not a relabelled official
vector. The 65339-byte input has SHA-256
`6b55159252fa5bc00dadfc76bab243ee1d3993314d7c2c704ed98d73469d001c`.
It reaches the original ADTS parser with sync `fff`, ID 1, layer 0,
protection absent, LC profile, 44.1 kHz, two channels, frame length `0134`
and one raw block. Processing then stops at original `025593`:
`6ff8 0019 0d00`, `ADD *($0019),0,A,B`. This reads mapped BK, not a
missing peripheral. The staged CPU's generic diagnostic calls this family
"shifted load", but the operation here is ADD. The neighboring source
`02558f..025598` (10 words) pins AR6/BK additions and AR2/AR1 publication.
The absolute-address ADD/SUB family now has a bounded CPU admission.
`shifted-bk` checks 24576 cases and replays: eight source/destination/operation
selectors, all 32 shifts, four values, SXM/OVM combinations, two program
pages and three save checkpoints. It changes BK after Read, poisons fetched
extensions, supplies live Execute accumulator operands, and requires one
source read with no data writes. The unchanged CPU guard fails this fixture;
the admitted implementation passes. `shift-bk-edge` checks 64 exclusions
and replays covering repeat, block, XC, delay, ST1, BRC and neighboring
BK load/store forms, with zero data reads/writes. Indirect addressing,
pending BK-writer composition and physical silicon phase timing are not
qualified by these tests.
The existing 245760-case shifted RAM/mapped-AR arithmetic profile and
1024-case ordinary BK ADD/SUB profile also pass with their save-state
replays. These guard neighboring supported behavior; they do not widen
the new absolute BK admission.

The generated MPEG-2 ABI control now returns zero with `illegal=0`, index
`009b` and 16 reservoir bits remaining: cursor accounting corresponds to
308 bytes, exactly the first declared ADTS extent. This is one successful
native processing call, not yet independently bound pre-quantization PCM,
all actual input reads or full-stream playback. The MPEG-4 negative control
remains rejected by the unchanged sync predicate.

Reproduce after `make build`, placing the original normalized
`mu4_initdata_container.bin` and the identified `al00_44-remux.adts` in an
isolated ROM search directory's `mu4aac/` subdirectory:

```sh
PATH_TO_MAME mu4aac -bios mpeg4 -rompath PRIVATE_ROM_DIRECTORY \
  -video none -sound none -nothrottle -debugger none -log \
  -nonvram_save -seconds_to_run 1
```

Use a private working directory for `error.log`. The present negative probe
must exit 3 with the explicit unsuccessful-block message, not pass merely
because its wrapper returned. Original module instructions and the normal
`mu4nand` fixture remain unchanged.

Select `-bios mpeg2` with `generated-mpeg2.aac` in the same private ROM
directory to reproduce the one-call positive control. Its successful exit
does not imply waveform conformance; capture and independent comparison of
raw decoder output remain required.

### One-Frame Output Capture

The bench observes the original decoder-return boundary `023de7` and uses
its returned source pointers, not presumed physical bank/channel labels.
For the generated control these are left `f000`, right `e800`, in frame
SP `8fe8`. The pointers differ from the constructor's initial bank ordering.
After the wrapper returns, the bench exports interleaved raw signed-32 and
converted signed-16 memory (`8192` and `4096` bytes respectively).
`mu4_aac_abi_check.py` checks source/header/result identity and event order,
capture extents and all 2048 rounded-shift/clamp conversions. Every native
converted word matches the independent source-derived conversion.

The output allocation is part of the ABI: use the original playback's
`7000..77ff` workspace. The decoder borrows this as raw working memory
before its final copy/conversion. An allocation at `8000` overlaps the
coefficient-recurrence working state at `80de`; passive writes prove that
state is copied into a sample. That invalid fixture is rejected by the
checker, not treated as a decoder accuracy failure. The writer's SP is
`8f88`, so it is not evidence of a stack collision.

Against the unfitted first 1024 frames of the generated control's host s16
reference, the left channel has five one-unit differences, RMS
0.0698771243; the right has four one-unit differences, RMS 0.0625. These
remain waveform diagnostics, not a chosen codec tolerance. Raw capture
SHA-256 is `13d2355308b88874511f5100f01a6dbf84bb11199af28748b383d5c791e2bf58`;
converted capture SHA-256 is
`d82f9703e1bffa46aa5d330c7935f9fc7e0720f7ab0b7eee19bc6d87bd2063a6`.
Per-store ownership, raw output normalization, out-of-window read exclusion
and broader media accuracy remain open.

The one-frame input proof records 163 actual input-window reads covering
156 distinct contiguous words (312 bytes). Every observed word equals the
independently generated control, and all 308 consumed frame bytes are
covered. Four bytes are lookahead; returned index `009b` with 16 reservoir
bits retains two bytes beyond the frame, while other lookahead comes from
header inspection. Context PCs are checked, but are not presented as
retained instruction identity. The input observer does not prove absence
of reads outside its window. Consumed-prefix SHA-256 is
`021cfce1f69f3848c960a8e6e37ce393b9231573e355d704a8253b8d350b8de3`.
For subsequent calls, distinguish fetched-word advancement from semantic
packet length; do not discard or synthesize the retained reservoir.

```sh
python3 tools/mu4_aac_abi_check.py PRIVATE_RUN_DIRECTORY \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00 \
  --reference-pcm GENERATED_CONTROL_HOST.s16 \
  --input-aac GENERATED_CONTROL.aac
```

The checker accepts an internally consistent observation and reports errors;
its zero exit status is not waveform conformance. Unit tests explicitly
retain nonzero waveform errors while rejecting malformed/faulted/partial
captures and raw-to-converted disagreement.

## Next Acceptance Boundary

1. Preserve the recovered original initialization and MPEG-2 ADTS ABI controls;
   complete the native recording finalization/drain contract before claiming
   whole-track playback. The generated controls are not recording containers.
2. Inventory supported LC tools and add independent conformance fixtures using
   the established caller-owned transport, preserving original compressed
   payload bytes. Do not transplant another decoder's state.
3. Derive delay/trimming and pre-quantization output scaling from independent
   procedure and native code, without fitted offset, gain or tolerance.
4. Run the official baseline and retain per-channel errors and complete input,
   output and configuration identities. One passing vector is not full codec
   conformance; broaden to all applicable sequences before making that claim.

Recording persistence, playback-tail completion, DMA/codec epoch pairing and
handset audio remain separately gated requirements.

`tools/aac_reference_metrics.py` supplies independently tested per-channel
metrics, original 24-bit WAV loading and explicitly scaled signed-32 loading.
It refuses unequal intervals, incomplete channel frames and non-normalized
samples; it performs no delay search, gain fitting or automatic trimming.
Its criterion result is not a conformance verdict until the obligations above
are met. `test_aac_reference_metrics.py` is included by tool-test discovery.
## Four-call isolated continuity control

The `mu4aac -bios mpeg2four` ABI fixture calls the original constructor once
and decoder four times, preserving all decoder state. The original wrapper,
not the synthetic caller, advances descriptor `1400`. Packet lengths are
308, 378, 362 and 286 bytes (1334 total). Returned descriptor/base/index
triples are `16c9/162e/009b`, `1786/16c9/00bd`, `183b/1786/00b5`, and
`18ca/183b/008f`; each return retains 16 reservoir bits and reports success.

The bounded run captures 4096 stereo frames through the returned pointers,
with all 8192 raw-to-s16 conversions exact. The generated control's host
reference differs at 3139 samples, maximum four s16 units; these are
diagnostic differences, not an acceptance tolerance or conformance claim.
There are 700 observed input reads covering 669 contiguous words, including
lookahead. Run `tools/mu4_aac_abi_check.py` with `--four-calls` and the same
source/input/reference arguments as the single-call control. The two-second
deadline is a bench bound, not a recovered decoder timing requirement.

This proves four calls without decoder reset or input-pointer intervention;
it does not cover input-window refill, EOF, full-stream playback or handset
boot. Those remain separate requirements.

## Caller-owned input-window refill

Thirty original words pin the descriptor contract: `02973c..029746`
computes `end-cursor`, the wrapper stores that extent at `b330`, snapshots
fetched-word index `b331` at `023def`, and adds it to the public cursor at
`023e59..023e63`. The caller owns the window; the decoder owns its reservoir.
These local spans do not establish the original file-reader's refill policy.

`mu4aac -bios mpeg2refill` exercises eight calls with one constructor. After
call four, the explicit ABI caller reloads the 2048-byte input window from
source byte 1336 and resets only the public cursor/end pointers. The prior
1334 bytes have been consumed; the next two bytes (`fff9`) remain in the
decoder reservoir. No decoder fields are written by the refill fixture.

All eight calls return success without illegal instructions. The 8192 stereo
frames consume 2720 source bytes. For every return the checker verifies
`window_source_offset + 2*(cursor-window_base) - remaining_bits/8` equals
the cumulative ADTS packet lengths, including the odd 349/365-byte frames.
Actual window reads are independently bound to the source bytes before and
after refill. Every one of 16384 raw-to-s16 sample conversions matches;
the host reference differs at 7109 samples, maximum four units. This is
diagnostic evidence, not formal codec conformance.

Use the usual ABI checker arguments with `--refill`. The fixture needs
`-seconds_to_run 3` to include its two-second completion bound. Single-call
and four-call controls remain available. Input exhaustion, short-window
recovery, original storage integration and EOF remain open;
this bench is not a handset playback or analog-output claim.

The `mpeg2stream` BIOS extends this contract to 32 decoder calls and seven
refills. A fixed-size synthetic DSP caller loops using ordinary instructions
and a caller-owned counter at `1403`; no observer redirects execution or
changes decoder state. This avoids an unrolled caller overlapping the input
window. Refill source-byte offsets are 1336, 2722, 4156, 5610, 7152, 8624
and 10132. The retained-bit counts at those boundaries are 16, 16, 16, 16,
24, 24 and 32, respectively; each reservoir matches the last four fetched
source bytes.

The run consumes 11658 bytes and captures 32768 stereo frames. All 65536
sample conversions match; 31116 samples differ from the host reference,
maximum four s16 units. There are 6110 observed input reads. The checker
requires all 32 call/header/capture/return groups, seven correctly ordered
refills and byte-bound contiguous source coverage. Invoke it with `--stream`;
run MAME with `-seconds_to_run 9` for the eight-second bench bound. The
single-, four- and eight-call controls retain their original call layouts.

This is repeated-refill continuity, not EOF acceptance. The generated file
has 174 packets and an odd 65339-byte extent; its final packet is 14 bytes.
The public descriptor expresses word pointers, so final partial-word and
lookahead handling must be established rather than silently zero-padding
the input or dropping its last byte.

### Strict Tail Boundary

`mpeg2eof` attempts all 174 generated packets, loading only complete words
inside the source. The last refill, after call 172, starts at byte 64882
and supplies 456 bytes with descriptor `162e..1712`; the final unmatched
source byte is not fabricated into a complete word. Any read outside the
declared window fails immediately, before comparing or recording its value.

The original wrapper returns `ffff` before decoding packet 173. It has
completed 172 calls, 176128 stereo frames and 64879 encoded bytes, with no
illegal instruction or out-of-window read. Every one of 352256 sample
conversions matches; the host reference differs at 170790 samples, maximum
four s16 units. This is successful-prefix evidence, not full-file playback.

The source-pinned skip helper `024425..024437` computes a candidate word
index and compares it unsigned against `b330-8`. When the candidate exceeds
that limit it rolls back the word-index increment and returns zero instead
of consuming residual bits. This reserves eight words when the input extent
is at least eight; smaller extents have unsigned-wrap behavior and are not
covered by this contract. On the rejected candidate the Execute-entry
observation is `AR2=00dd` (221), `AR0=00dc` (220), `b330=00e4` (228).
An earlier check in the same invocation has candidate zero and passes.
The wrapper subsequently returns with cursor/base `162e`, index zero and
remaining count zero. Do not retry it as though it preserved all entry state.

Reproduce with `-bios mpeg2eof -seconds_to_run 46` in an isolated directory;
MAME exits **3**, reporting `MU4 AAC wrapper returned without a new decoded
block`. The ABI checker option `--eof-boundary` verifies all 172 successful
groups, their source-byte/reservoir accounting and the specific reserve/
early-return observations. Its successful check means the negative boundary
reproduced, not that EOF completed. The eight-second `mpeg2stream` control
remains the clean successful repeated-refill gate.

The immediate unknown is the original caller's end-buffer and drain policy:
whether it supplies storage-block padding, an explicit final-input operation,
or a different admission path. The observed rejection precedes a read of
the final partial word, so fixing odd-byte packing alone is not established
as sufficient. Do not patch the reserve predicate, modify framing flags or
append unlabelled bytes to turn this into an EOF claim.

The original caller's `1310`-dependent input threshold is not an established
EOF signal. Query `020973..020980` clears `130f` and passes destination
`1307` to virtual method 11 through object reference `15ea`. Setup `027823`
selects decoder `aaac` and transform `9cf8`; original C-init record `02744b`
sets the latter's table to `c738`. Its method-11 far pointer at `c74e` is
zero, so the generic dispatcher returns `ffff` without calling a metadata
writer. Forty-five source words pin this chain. This establishes the selected
initial table, not its lifetime immutability or absence of indirect writers.
The five mapped `1310` literals are retained in the contract report; none
justifies treating a write to that flag as a legitimate end-of-input action.

The complete input-buffer refresh `0296ac..02973b` is 144 words, with two
conditional calls to the 50-word overlap-safe copy helper `0299cc`. The
copy ABI is destination in A, source in caller frame word 0 and count in
word 1. For receiver R, downstream D=`R[9]` and upstream U=`R[8]`:

- If D exists and unsigned `R[5] > D[1]`, copy `R[6]-R[5]` words from
  `R[5]` to `D[1]` and adjust the receiver's pointers.
- If U exists, synchronize matching receiver/upstream extents; conditionally
  preserve the upstream's two-word metadata when the receiver is empty.
- If the signed compaction predicate passes, move the unread span starting
  at `R[0]` to `R[6]+R[7]` and adjust cursor/end fields. The contract report
  retains the precise sign-extension expression rather than assuming all
  16-bit differences are interchangeable.

Neither payload path synthesizes zeros or adds extra lookahead. These 194
pinned source words establish local movement and metadata updates, not every
upstream producer, file-byte padding or live descriptor ownership. The
input-16 reply path likewise copies fixed record `3708..3710`, not a region
indexed by its argument. Neither operation supplies a missing EOF mechanism.

The selected AAC table prefix`c686..c6a1` and three method spans now have a
57-word source contract. Method12, called after each frame through`029f2b`,
points to`023d5a`: two NOPs and FRET. It consumes the return stack but performs
no payload/object access, sample production or input advancement; it cannot
be a deferred-output drain. Method10's complete13-word body at`023ef8` only
copies caller record word10 to object word1. Method9's first13 words at
`023e64` publish object word1 back to record word10 and set record word1 to
`0400`. Its complete local body is separately pinned below.
Neither a no-op post-frame call nor that one-field restoration is evidence
for an EOF operation. The table prefix is not a proven total table bound,
and other controls/upstream end-buffer ownership remain open; this result
does not authorize changing object word1 or invoking reset to discard a tail.

`decoder_info_body` extends method9 through its FRET at`023ef7`:148 local
words,32 numeric-table words and22 AA55 helper-identity words,202 total.
The local body reads object words1/4/8 and writes caller record words1..10.
When BA4F is zero or BA51 is nonzero, it publishes fixed record fields:
word1=`0400`, word2=`0485`, word3=0, double words4..5=128000 and6..7=44100,
word8=2, word9=0 and word10=object[1]. The other path multiplies A89E by6
into AR2 and reads a double word at`AR2+C152`; the incoming multiplier
status, including FRCT, remains an unclosed ABI input to that offset.
It conditionally copies A874 and walks adjacent numeric
levels at`c6a2` using AR3 initialized14. The16 pinned levels run from8000
through384000 followed by`7fffffff`; these are source quantities, not a
new physical-rate or format-conformance claim. The final path updates
record word2 through helper`029bdd`, copies A89C to word8 and object[8] to9.

The local body has no sample loop or input-buffer advance. It does call
`002096` and`029bdd`, so this is not a whole-call write/EOF absence proof.
The first target is available in the original AA55 bootstrap:
`2096: FB 003518`, followed by a complete20-word stack/register routine at
3518. That resolves its source identity without inventing resident ROM.
Arithmetic qualification and native invocation of method9 remain separate
acceptance targets. Every pinned source/table/helper
word has a mutation test, including removal of the terminating FRET.

`decoder_info_helpers` closes the local call tree with74 further AA99 words,
276 total across method, table, bootstrap and helpers. The signed wrapper
`029bdd..029c04` has one call, to`029c62`; its equality and larger-divisor
branches include their delayed instruction slots. The worker
`029c62..029c83` shifts/subtracts/adds with a counter initialized31 and returns
without another call. The AA55 helper has only multiply/accumulate/shift/add
operations after its stack saves.

With CPL=1 and a valid, disjoint RAM caller stack/metadata record, all helper
data accesses are stack-relative or MMR accesses. No helper follows a payload
pointer or accesses a device, input descriptor or sample-output buffer.
Consequently this specific method/call tree cannot perform deferred sample
production or advance the encoded input; it reports metadata. This is static
control/access closure, not exact arithmetic acceptance over every operand or
status setting. In particular, incoming multiplier flags remain relevant.
It neither proves native invocation of method9 nor closes other EOF controls,
the original controller's stop/drain policy or complete playback.

The original worker's return routing is separately pinned over
`0293cf..029403` and three virtual wrappers (62 words). It retains the
decoder result in AR1 across metadata method9, whose preservation follows
from the closed helper ABI above. It then distinguishes three local routes:

- Result0 clears`15d9`, writes`15f1=1`, stores twice record`15ef` into
  context word19 and calls`020454` before the common refresh.
- Result2 selects method12 on object`15ea`, method11 on decoder`15eb`
  and helper`02061e`, then reaches the common refresh.
- Every other unsigned-word result, including`ffff`, bypasses both groups
  and reaches the common refresh directly. The second BANZ tests
  `low16(AR1-2)`, not a second memory-loaded result.

All three routes refresh input object`15e9` through`0296ac` and branch
to`02940b`. Thus the isolated ABI bench's deliberate stop on`ffff` is not
itself the original caller's EOF transition or a sample-drain operation.
This is a static local routing contract, with exhaustive host-model
classification of the65536 result words and mutation coverage of every
pinned word. Native invocation/branch selection, retry scheduling, helper
`02061e`'s effects and the upstream final-input policy remain unqualified;
do not synthesize result2 or infer that retrying unchanged short input is safe.

The fresh finalized-media cold run`tmp/mu4-native-return` extends the
read-only worker observation with the two return branches and their actual
successors. All267 events in9.64..9.86s are uncapped, instruction-matched
and bounded inside the first verified AA99 session. Four complete observed
invocations advance processing counters372->373 through375->376. Each
returns0000, retains AR1=0000 across metadata method9, takes`0293e1`,
reaches common refresh`0293fe` and then`02940b`. No result2/ffff route is
observed in this window; that is not a whole-session absence claim.

All27 availability branches pass. Activity passes four times and skips23,
including two skips after processing count376. Thus the late window is not
blocked by the0400-word input threshold or a failed decoder return. The
remaining native cleanup question is upstream activity/refill scheduling
and the already-mapped one-pass short-input policy. This does not authorize
setting15d9, changingaa1a or borrowing the isolated ABI's reserve rejection
as the native session's cause.

```sh
.venv/bin/python tools/mu4_decoder_gate_check.py \
  tmp/mu4-native-return/error.log --returns
```

The qualifier follows complete call/return/retained-result/successor groups;
processing counters remain context, not invocation IDs. A window beginning
mid-call reports its excluded prefix explicitly; censored later calls fail.
Synthetic checker controls cover zero, two and other return classes, but
only zero is runtime-qualified here. Tested executable SHA256:
`41800b61c13a18396ffa20437c43b033c04e5581b08b96cb9ce3007f58060e0b`.

`worker_reservation` identifies the local meaning of the15d9 activity term.
The93-word contract includes the worker's acquisition prefix, the delayed
wrapper`02044a` and count-consumer prefix`020182`. A complete literal
scan of49843 mapped AA99 words finds seven15d9 references: four direct
literal writes (two to1, two to0) and three reads. This is not closure over
computed/indirect writers. The worker permits work when15d9 or context[22]
is nonzero. If15d9 is zero, it calls02044a with the context; that wrapper
places AR2 at context+22 before020182 decrements the count. The worker then
sets15d9=1. An already nonzero flag bypasses a fresh acquisition; the
decoded zero-result path clears it. Model tests exhaust the unsigned count
domain for both literal flag states, without assigning physical units or
queue capacity. Helper callbacks beyond the pinned prefix remain open.

The increment's pointer comes from `0201c7 MVDK *AR2+,AR3`, followed by
`MAR *AR2+` before `0201ca ADDM #1,*AR3`. The same complete mapped-word
scan finds exactly one literal `d7ac`, at initialization-data address
`027061` (followed by `054f`). Both sequences and the literal census are
tested. The surrounding words have the initialization-record shape
`count=9,destination=d797`, with payload
`0000,0000,0800,aa85,d7b0,0000,0000,d7ac,054f`; its `d7ac` entry
would map to `d79e`. That mapping is an initialization candidate, not
observed initialization ownership or a presumed hardware-ready notification.
MVDK retains this operand in Address stage, not the ordinary Read-stage
`pipeline_read_data` path. The CPU's `observe_staged_data_read` interface
now also surrounds that actual MVDK bus access with its Address-slot
instruction identity, without adding a read or changing its timing. It
continues to exclude forwarded values. A zero count from an observer that
covers only Read stage cannot exclude this producer; the live Execute PC
is not its owner.

`context_lanes` pins the four wrappers, publication helper and paired caller
with121 word checks. Their local address contract is:

| Acquire wrapper | Count offset | Publish wrapper | Link-field offset |
| --- | --- | --- | --- |
| `020461` | 6 | `020454` | 24 |
| `02044a` | 22 | `02046b` | 8 |

At base `d796`, these produce count/link-field pairs `d79c/d7ae` and
`d7ac/d79e`, matching the reciprocal links observed below. The helper's
AR2 walk is checked for every unsigned16-bit base, including wrapping;
optional `020198` preserves AR2 on the stack if it returns normally.
This is address algebra, not a simulation of that callback's effects.
The full mapped-word census finds15 syntactic immediate-call candidates
for the four wrappers; computed calls are not excluded.
Source branch `028fe6` conditionally calls `02046b(AR1)` when `aaa2`
is nonzero, then calls `020461(AR1)` and writes
`aaa2=1,aaa4=0,aaa3=0`. The observed calls below use context `d7b8`;
the full flag lifecycle remains unqualified. None of this establishes a
physical ring size, sample unit, scheduling clock or permission to drain
by manufacturing a ready token.

The combined native capture below closes252 wrapper intervals with no
leading partial returns. Nested-interval checker controls prevent assigning
a pointer read to an unrelated most-recent call. Its caller inventory is:

| Context | Calls to `02046b` | Calls to `02044a` | Calls to `020454` | Calls to `020461` | Count per column |
| --- | --- | --- | --- | --- | --- |
| `d796` | `020581` | `0293c1` | `0293ed` | `020508` | 4 |
| `d7b8` | `028feb` | `0204a6` | `0205f3` | `028fee` | 59 |

All four target-counter pointer reads belong to `020581 -> 02046b`
on context `d796`, not the initial-publication candidate `0204bc`.
The decoder's zero-result path publishes toward the reciprocal count
through `0293ed -> 020454`. These are local pipeline exchanges, not a
missing hardware-ready notification. Calling `d796` an encoded-input
buffer would overstate the established payload ownership. The next open
boundary is one-pass cleanup versus the remaining buffered AAC tail.
Callback cadence alone does not establish drain policy.

`exchange_publication` pins100 original words around `020545..020585`,
the method wrappers and conditional adapter getter. Method11 runs on both
`aa5a` and `aa5b`; method14 then runs on `aa5a`. Its zero low-word result skips
publication. A nonzero result reaches `020581 -> 02046b(AR1)` and clears
`aa55`. Under table `c6f0`, method14's far pointer at `c70c` selects
`02a30b`, which reads object field9: the source-exhausted field in the
independently mapped copy adapter. The bounded native capture verifies
that binding for the selected late queries, not its lifetime immutability.

`1310=0` bypasses only the intervening refresh/reset subsection, **not**
publication. With a nonzero flag, the source refreshes `15e9`, obtains
end-minus-cursor through `02973c`, and tests that result and context[22].
If either is zero, it calls `02969e(15e7)`, explicitly clears `1310`
and sets `12fa=1`; publication follows either way. Tests exhaust the
zero/nonzero boundary of each unsigned scalar input and reject every
changed or missing source word. This models explicit branch/store selection
assuming normal helper returns, not their memory effects or actual late
branch operands. It does not make `1310` an EOF flag or establish whole-track
draining. The actual adapter result is qualified below; cleanup ordering
remains separate from any dependency on a nonzero `1310` to return the token.

Cold capture `tmp/mu4-exchange-status-qualified` completes under the managed
runner with exit0/interrupted=false at10.016666667s. Its unchanged
9.64..9.86s first-session window contains118 owned `02a310/10e1` getter
reads and185 branch entries. All59 selected tests agree with the actual
getter value at object `13e0`/header `c6f0`:55 zero-result skips and four
nonzero publications. Each publication encloses the matched wrapper's
entry, target-pointer read and observed return before the common
continuation. All four eligible-branch snapshots have `1310=0`.
There are59 other common entries without a selected status test; they are
reported separately, not interpreted as extra branch outcomes. Existing
counter, call-interval, acquisition/release and decoder-return checks pass.
The756 call/entry/return events and126 owned pointer reads include
`d79e -> d7ac` and `d7ae -> d79c` four times each, and
`d7c0 -> d7ce` and `d7d0 -> d7be`59 times each. The35 owned counter
reads are four increment operands0 at `0201ca/6b83`, four decrement
operands1 at `020182/6b82`, and27 gate reads at `0293b3/1ae6`.
Four entry-state cycles agree with acquisition and release:
`(flag,count)=(0,1) -> (1,0) -> (0,0)` at gate, decoder call and refresh.
These are actual read ownership and entry-state agreement, not observed
write retirement, physical queue capacity or complete callback semantics.
The observer has separate executable acceptance across22,536 MVDK
cases/restores, including scope exclusion and a negative control; see
[5510 bring-up](5510_bringup.md). This is not a renewed full-CPU checkpoint.

The final owned status read is0 at9.725261143s; the selected branch at
9.725261252s correctly skips publication. Its context-only snapshots show
destination-full field13=1, context[22]=0 and `1310=0`.
The original loader entry at9.725659144s follows before another observed
adapter publication. This local gate is behaving coherently; no missing
hardware token is needed to explain its observed decisions. The remaining
question is why the original session chooses teardown before draining its
buffered material, not how to force this adapter to report exhaustion.
This does not qualify whole-track playback or reference-waveform alignment.
Tested binary SHA256:
`4c59785f76edf1d6aea36b94e3a6d75c8ca78fb495e6990d4197a9cff6fa9b9e`.

```sh
.venv/bin/python tools/mu4_decoder_gate_check.py \
  tmp/mu4-exchange-status-qualified/error.log --exchange-status
```

### Declared Packet-Lookahead Control

`tools/mu4_aac_lookahead_fixture.py` preserves all original 65339 bytes and
appends a copy of existing packet 173 (source offset 64879, 446 bytes). This
is a declared longer encoded stream, not recovered storage padding or an EOF
fix. It contains 175 packets; the native caller deliberately stops after the
original 174. FFmpeg decodes the longer stream without errors, and its first
712704 PCM bytes are identical to the unextended stream's complete reference.

The 65785-byte fixture has SHA256
`f49ce61bc5d665bffac8c5fc238d93a3ee2c89bf7af306cb07d5f1a30d5e3fa2`.
`mu4aac -bios mpeg2lookahead -seconds_to_run 46` completes all 174 original
calls with no illegal instruction, consuming exactly 65339 bytes. All 356352
raw-to-s16 conversions match. Against the original host reference, 172752
samples differ, maximum four s16 units. The checker uses the original
reference, not a fitted or extended comparison interval:

```sh
.venv/bin/python tools/mu4_aac_abi_check.py RUN --lookahead \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00 \
  --reference-pcm tmp/mu4-generated-aac-control-final/host-reference.s16 \
  --input-aac tmp/aac-conformance/roms/mu4aac/generated-mpeg2-lookahead.aac
```

The final refill now supplies 902 bytes, and the reserve limit becomes 443
words instead of 220. Original candidate 221 therefore passes unchanged.
The final successful return retains 24 bits and stops with public cursor
`1714`, base `170d` and index seven. Input reads include the declared suffix;
there is no claim that the decoder works without lookahead. Source extents
are tracked separately from the larger MAME ROM allocation so the strict
unextended `mpeg2eof` probe cannot consume its allocation's zero-filled tail.

This closes arithmetic/conversion coverage of the complete generated packet
prefix, not the original caller's EOF/drain policy, formal conformance,
original REL playback completion, or handset/analog audio.

## Recording Extent Metadata

The independently extracted `tmp/mu4-dst-bypass-recorder/TEST` is 143680
bytes, SHA256 `4ba59580f8bf50b8d45d7a62d8ddf42ea94dfb7d884ff05b5092e7c13141470d`.
Its flag-free ID3v2.3 tag has a 100-byte payload, ending at file byte
110: `TIT2`/`MAME`, `TPE1`/`TEST`, then 70 zero-padding bytes. It contains
no `TLEN` frame. The [ID3v2.3 specification](https://id3.org/id3v2.3.0)
defines that frame as a millisecond length declaration; its absence gives
no duration or sample count, not a zero-length recording.

The strict inventory rejects unsupported flags, bad frame extents and
nonzero padding instead of treating a failed parse as missing metadata:

```sh
.venv/bin/python tools/mu4_recorder_media_inventory.py TEST TEST \
  --extracted-files --id3-tags
```

The next 16 file bytes are `504f4350030000010002000010000000`.
The `POCP` writer's count field at header offsets 8..11 is serialized from
a caller-supplied 16-bit count, with a required minimum of `0200`; the
stored value here is 512. The field at offsets 12..15 is 16: helper
`02adb2` returns constant 16 after its normal callee return and the writer
serializes that result. The matching AA22 writer/helper spans agree after
branch/call relocation; 228 original words pin both overlays.

Neither visible count is an established total sample count. This does not
exclude useful information elsewhere in the custom body or encoder control
records. The intended recording extent therefore remains unproven; neither
375 native blocks nor the longer host-decoded prefix should be promoted to
the expected answer from these tag/header fields alone.

### Encoder Publication Control

`mu4_recorder_session_observe.lua` reuses the header observer with a separately
labelled 0.8--15-second window and 512-publication cap. The default observer
retains its original two-second/eight-header contract. Session mode checks
all 15 source-pinned argument fields for each publication, including declared
packet length. Its wrapper exits at 15.1 seconds; this is a header-observation
run, not the original twenty-second recorder acceptance gate.

A fresh `bootrecac` run from the unchanged formatted NAND observes 387
complete header-argument groups (5805 arguments), uncapped, with no illegal
instruction. Their declared lengths total 143964 bytes, and the concatenated
seven-byte headers have SHA256
`922add2bf7bccad36400d0767c25cb7da14e58e81a9fdbd5499ae423f26f9adf`.
This is not a count of committed, valid encoded packets or intended samples.

The before-metadata NAND observation is
`9832d2309e352dc3afb4f8f6ddcfbf7744fe191ee8633027f19dba46bcdea5e0`.
Independent fixture-volume export and 7-Zip extraction yield `REL_001.REL`,
143982 bytes, SHA256
`59a1f4b67f2ca07b5f4387fbf0fda459908b0be79bd751f1fdd336ad2103887b`.
The source NAND file remains byte-identical. This is a different recording
from protected `TEST`; comparing its observed headers with the protected
projection fails at header 2, byte 373. Do not fit an offset or transfer the
387 count to explain the older 375-block playback stop.

Use `mu4_context_contract.py SOURCE --recording-session-log RUN/error.log`
to check this observation. Optional `--recording-session-reference AAC`
requires exact header equality at successive declared offsets and reports
payload validity as unestablished.

### Same-Recording Native Playback

Playing that before-metadata NAND through the original first-track command
selects its generated title, not protected `TEST`. A read-only 0.8--12-second
observer captures 88443 bit-reader bus reads, with no illegal instruction;
the original twenty-second diagnostic still terminates with audio completion
explicitly unvalidated. There are 375 successful decoder calls. Their consumed
139500 bytes are completely covered by entry reservoirs and 69750 in-call
reads, without searching for an alignment or supplying missing reference data.
The extracted prefix SHA256 is
`986f2373a3a6143fef3682a29fd5037750bf473f7a21c96aaee5c0bdd34e4591`.
All 375 headers match the same recording's encoder publications at their
declared offsets, including the early 614/384/328-byte packets. Twelve later
header publications remain outside this consumed prefix; their payload and
intended output extent are not established by this comparison.

`mu4_packet_session_check.py RUN --window-last 12 --export-observed-aac PATH`
creates an exclusive new **host diagnostic**, never an emulation input or an
independent reference. It rejects missing/conflicting observations and validates
framing and net consumption through the existing packet checker. FFmpeg decodes
this prefix without errors into 375 stereo blocks (1536000 bytes). Comparison
with native returned s16 output gives maximum difference 11, RMS 1.220189855
over 768000 channel samples. This is a cross-decoder diagnostic, not bit-exact
codec conformance, physical audio, whole-file completion or speech acceptance.

The same capture also passes the source-pinned coefficient/transform-tail,
history-continuity and converter-input checks across its first eight calls.
Those include 30 observed transform windows with 128- and 1024-component
spans, including the early short-window transition. The complete returned PCM
file has SHA256
`60515c08ae555eff67d689cafe07394d36654e54f62626242a3dceca1b87549e`;
the host-decoded observed prefix has SHA256
`641f0c2ac42727cd6b75f72ace229095ba80e889517559eb247c21fb4b890fc8`.

Compose these checks without the protected fixture's `MAME` title requirement:

```sh
.venv/bin/python tools/mu4_packet_session_check.py PLAYBACK \
  --window-last 12 --source-a00 roms/5510-mu4-reference/InitData_R060.a00 \
  --encoder-log ENCODER/error.log --reference-pcm PLAYBACK/host.s16le
```

This mode requires exact agreement of packet/call/block counts and full ordinal
host-PCM comparison coverage, while retaining the host comparison as diagnostic.
It does not bypass or claim the existing protected-title transport acceptance
gate. Nor does a host decode of observed bytes provide an independently
generated encoded input; the separately generated MPEG-2 ABI controls above
remain the independent-input evidence.

For a reproducible comparison, use `mu4_context_contract.py SOURCE
--recording-session-log ENCODER/error.log --recording-session-reference
PLAYBACK/observed.aac --recording-session-reference-prefix`. The explicit
prefix mode requires a nonempty sequence ending at a declared packet boundary,
compares from byte zero and reports all unmatched publications. Default mode
still requires every published header and is not relaxed by this option.

The fresh recording's cleanup observation independently reproduces 376 input-11
requests and the length successors `0294ca` (375) then `0294fd` (one), followed
by `029500 -> 02956f` with control context one. The extracted file's 143982-byte
extent gives a 71991-word limit: 71834 words requested before the last 186-word
request, leaving 157 words. This matches the observed boundary; the live final
AR1 snapshot is `009d` (157). Unlike the older recording's six-word remainder,
this establishes that the larger fresh extent participates in the same branch,
not that a stale old-file size caused this stop. No decoder call fails.

The twenty-second debugger capture also contains later resident activity.
`mu4_cleanup_ingress_check.py LOG --first-resident-only --input-file FILE`
explicitly selects the first session between its two resident-entry markers:
1881 events, with 2041 later events excluded from 3922 captured, uncapped.
The option refuses a missing closing boundary or a selected session that reaches
the global capture cap; default checking still rejects mixed sessions. This
proves the selected source policy against a second file extent, not that its
buffered tail is intentionally disposable. The unresolved contract is whether
the native caller must drain already-buffered data before this cleanup or the
recording format declares that data outside its intended output extent.

### Fresh Stored-Tail Inventory

A short `bootrecac` replay from the same unchanged formatted NAND, using
`mu4_recorder_transform_observe.lua`, provides 96 complete 32-byte transform
observations before two seconds. All match the independently extracted fresh
file at its unique origin 622, with one captured state. This short replay is
not the recorder acceptance gate. Constant-state host projection of the file's
remaining 143360 bytes (280 times 512) produces:

| Boundary | Result |
| --- | --- |
| Native consumed prefix | 375 packets, 139500 bytes; every byte equals the projection from byte zero |
| Complete projected packet chain | 385 packets, 143220 bytes; SHA256 `b7e7b5d30eba38099e26d701b3db49e0d2f586c0cb16be834b1adae6f2b4257f` |
| Remaining stored projection | 140 bytes of declared 372-byte packet 386 |
| Encoder-only tail | Remaining 232 bytes of packet 386 and all 372 bytes of packet 387 are outside the stored projection |
| Host decoding | FFmpeg reports no errors for the 385-packet prefix and produces 1576960 PCM bytes; its first 1536000 bytes equal the host decode of the observed native prefix |

All 385 complete headers also match the fresh encoder publication trace at
successive declared offsets. The 512-byte granularity is a file-size
observation, not yet an established recorder-flush contract. In particular,
387 header publications must not be described as 387 committed complete
packets, and the ten complete projected packets beyond native playback must
not be confused with the incomplete recording tail.

Use the transform checker with `--output-projected-aac NEW_PATH
--comparison-prefix PLAYBACK/observed.aac` to enforce fixed-origin byte equality.
The supplied prefix's provenance is checked separately by the packet-session
checker; this comparison alone does not establish it. The projection remains
host-only and must never become emulation input. This bounded-prefix control
does not establish later state; the full recording observation below does.
Intended sample extent, recorder flushing and playback draining remain
distinct unvalidated contracts.

### Recorder Stop Frontend

`recorder_stop_frontend` pins 155 original AA88 words across the selected
control/completion blocks and four complete helper stubs:

| Boundary | Recovered contract |
| --- | --- |
| `0285dc..0285ef` | Clear mode `3750`; continuation `0008` becomes `000a`, otherwise `0009`; call `0384fc` and `0386ec` |
| `0384fc..038509`, `0392ae..0392c6` | Clear `edc3`; build `[3,0,0,1]` at `fd85..fd88`; submit four words through `0395e6` |
| `0386ec..0386fb`, `0388ad..0388c9` | Transmit encoded codec words `0580`, `0a09` via serial control/status `0048/0049` and transmit register `0043` |
| `039443..039475` | Selected completion `fd85==8009 && fd86==1` clears mode, updates continuation, sets lifecycle `373e=2`, mutes codec and calls API `2082` then loader `2080` |

The related B-revision [TI SLWS106H](https://www.ti.com/lit/ds/slws106h/slws106h.pdf)
register definitions identify `0580` as a simultaneous headphone mute/zero-cross
update, and `0a09` as DAC soft mute plus ADC high-pass disable. These are codec
controls, not file flushes; their analog effects are not validated here. The
local reference and revision qualification are recorded in the 5510 bring-up
document. The source helper ORs register/data arguments into the transmitted
word and polls status bit 1 on each side of the write.

The processing path downstream of `0395e6` is software-owned. Its selected
stop buffer and empty finish-method slot are characterized below; neither the
codec-control helper nor the completion consumer constitutes a file flush.
File granularity alone is not grounds to add padding, modify the file extent
or invent a flush reply.

`recorder_stop_mailbox` adds 233 pinned AA88 words for publication, callback
initialization, software interrupt dispatch and selected deferred/reset paths.
The sender waits for bit 15 at mailbox `0080`, zero-extends its record to 16
words using descending stores, then invokes the callback at `efe6`. The
original initializer selects `038c13`, which sets `05cd` and executes `INTR 25`;
the uploaded vector at `2064` branches to `038ad2`. This is a software path,
not a request awaiting an unmodeled external processor. The initialization
selection alone does not prove pointer immutability over the whole lifecycle.

Selector 3 stores `(argument==0)` at `05eb` and, when that flag is set,
sets `05f3`. The common reply marks selector 3 as `8003`, not `8009`.
The selected `05eb` processing branch invokes `0389cf(0)` and reset/cleanup
helpers. Inside the pinned counter-update helper, `05f0==0 && 05eb!=0`
clears `05ed`, `05ee` and `05eb`. The stop-time byte comparison below qualifies
their selected pending-buffer extent; the original source still does not
establish a general file-flush promise.

`mu4_recorder_stop_observe.lua` records changed RAM writes in an explicitly
bounded 0.8--15-second window. The fresh control observes 4159 changes out of
25299 writes, uncapped, with no illegal instruction:

| Observation | Emulated time |
| --- | ---: |
| Zero-extended `[3,0,0,1]` published | 10.100594552 |
| `05eb=1`, then `05f3=1` | 10.100595994 / 10.100596061 |
| Immediate `8003` acknowledgement | 10.100598703 |
| `05ed` and `05ee` each change `0120 -> 0000` | 10.101587232 / 10.101587257 |
| `05eb` cleared | 10.101587282 |
| Later `8009` copied to completion record, status 1 | 10.838815382 |
| Lifecycle `373e=2` | 10.838815965 |

Check with `mu4_recorder_stop_check.py LOG --source-a00 SOURCE`. Context PCs
are not retained writer identities. The before-metadata NAND remains exactly
`9832d2309e352dc3afb4f8f6ddcfbf7744fe191ee8633027f19dba46bcdea5e0`,
matching the encoder/publication capture. Thus the selected buffer reset
precedes completion; neither a missing stop acknowledgement nor a decoder
failure explains this recording boundary. This short observation still is
not the twenty-second finalized-recording acceptance gate. The finalized
extent and size-update accounting are measured separately below; the deeper
intended persisted-tail policy remains unresolved, not an established hardware
defect. Pending bytes are now accounted for separately below.

### Finalized Recording Extent

The complete unmodified `bootrecac` twenty-second run reaches native metadata
success, lifecycle 2/mode 0/continuation 0 and one accepted track. Resident
comparison checks 48261 program and 3656 data words; the converted-input checker
observes 871742 generated stereo frames and DMA symbol counts
`200486,200509,200499,200496`. Encoder consumption is not inferred from these
source statistics. The before-metadata NAND reproduces the preceding captures
exactly. The final NAND SHA256 is
`f09e3e8c1049a563a073fa40040c9d400e2d6212ba1a223ce44045303104277d`.

Independent fixture-volume export and 7-Zip extraction yield named file `TEST`,
144192 bytes, SHA256
`515e2c2c6f7c759d11870ae06879ae3f26036efd4b9cfca89ec4464cb7e59f48`.
Relative to the same run's 143982-byte `REL_001.REL` snapshot:

- 33 common bytes change, all in metadata ranges `[17,18)`, `[21,41)`, `[44,56)`;
- title becomes `MAME`, artist `TEST`, with the same 100-byte ID3 payload and no `TLEN`;
- all existing bytes after those ranges, including the recorded body, are unchanged;
- 210 bytes are appended, every one `ff`.

The raw NAND delta affects pages 288, 352 and 127008, with no spare-page change.
These facts do not establish that erased bytes are intended format padding or
that the new file size is correct. In particular, finalization does not recover
a further complete encoded packet. Constant-state host projection still yields
the same 385 complete packets and the same 143220-byte AAC prefix hash. It then
has 332 projected bytes of the next 372-byte packet, with 18 final bytes outside
the complete-window model. No additional complete AAC packet is recovered.

Reproduce the byte/tag comparison with
`mu4_recorder_media_inventory.py BEFORE_FILE FINAL_FILE --extracted-files
--id3-tags`; use the existing transform checker with the fresh native observed
prefix for fixed-origin correspondence. The original metadata/file-size
accounting that exposes the erased extension is established below; this is not
an unobserved later save operation flushing the encoded tail.

### Recorder DRR Read Ownership

The finalized varying-input fixture has a complete all-channel census at
McBSP0 DRR address `0021`. Ownership comes from the DMA device's active
source-read API, not enabled-channel register addresses or the CPU's current
PC. Unowned reads additionally record a bounded PC-context histogram; those
contexts are not instruction-owner claims. Side-effect-disabled observations
are excluded, and the diagnostic histogram is not a save-state output.

`mu4_recorder_read_census.py RUN/error.log` reconciles owner masks, all
unowned contexts and the existing independent square-source counter check.
It rejects missing, duplicate, inconsistent or capped evidence. The fresh
20-second `bootrecac` run at `tmp/native-gates/mu4-recorder-read-census`
has 1,707,055 reads, no dropped contexts and no multiple-owner masks:

| Active DMA mask | Zero-based channel | DRR reads |
| --- | --- | ---: |
| `04` | 2 | 801990 |
| `08` | 3 | 905064 |
| `00` | no active DMA source owner | 1 |

The sole unowned read has observed CPU context `038edb`; its instruction
and purpose remain unclassified. Thus the current fixture's 905065
`other_owner_reads` are almost entirely channel-3 traffic, not unexplained
CPU reads. This does not retrospectively assign the different protected
recording's counts, or prove that channel 3 has one fixed peripheral role:
its configuration differs from the playback TX fixture.

All four selected channel-2 symbol counts and 871742 generated stereo frames
match the banked finalized recording. Native metadata success reproduces,
and the endpoint NAND remains exactly
`f09e3e8c1049a563a073fa40040c9d400e2d6212ba1a223ce44045303104277d`.
The copied formatted seed remains
`75a523a9c1b6a0f6d98d47812b3082717e0764a79e0956b47b2de4fa7160d6c3`.
No read histogram writes guest state.

`mu4_recorder_read_census.py RUN/error.log --fresh` additionally reconciles
side-effecting DRR reads with actual receiver publications and reset boundaries.
The 20-second `tmp/native-gates/mu4-recorder-fresh` capture has 1,707,056
publications: 1,707,054 fresh reads, two invalidated by receiver reset, and none
pending at the endpoint. All 801990 channel-2 and 905064 channel-3 reads are
fresh; the sole unowned read is stale and matches the known retained DRR word.
There are no readiness/value inconsistencies or unknown stale-prefix reads.
The finalized NAND and formatted seed retain the hashes above.

This validates delivery of published DRR words, not ADC conversion, losses
before publication, destination-buffer ownership or encoder consumption.
The optional observer does not alter receive timing, FIFO or interrupt behavior;
its diagnostic accounting is fresh-run only, without save/restore support.

Adding `--destinations` requires active-channel read/write pairing across
program, data and I/O spaces. In the 20-second
`tmp/native-gates/mu4-recorder-destination` run, all 801990 channel-2 reads
pair with full-word, equal-value writes in data `1900..19ff`; all 905064
channel-3 reads pair with writes in data `1800..18ff`. No pair is overwritten,
mismatched or pending at the endpoint. The freshness counts and both NAND
hashes above reproduce unchanged. Observation belongs to the research bench,
not product-specific logic in the generic DMA device.

These are observed minimum/maximum destination addresses, not a proof of
contiguous coverage or buffer lifetime. The next unvalidated boundary is
which subsequent reads consume these writes and how they feed the encoder;
the two DMA channels must not be assigned microphone/channel semantics from
address ranges alone.

`--buffers` adds latest-write lifetimes in these two data ranges. Each
full-word DRR-sourced DMA destination write starts an epoch; any subsequent
write retires it, and side-effect-disabled reads are excluded. The
20-second `tmp/native-gates/mu4-recorder-buffers` capture reconciles:

| Data range | DMA writes | Epochs read at least once | Retired unread | Unread at endpoint |
| --- | ---: | ---: | ---: | ---: |
| `1800..18ff` | 905064 | 904744 | 320 | 0 |
| `1900..19ff` | 801990 | 0 | 801734 | 256 |

The first range has 1699328 reads of known DMA-written values, all without
an active DMA source owner, plus 24 reads without a retained DMA-write
assignment. Its bounded CPU-context histogram contains `038eb4..038eb6`,
`038ed9..038edb` and `03faa6..03faa9`; those remain contexts, not proven
instruction ownership or overlay identity. The second range has no observed
reads. No value mismatch or dropped context occurs, and both NAND hashes
above remain unchanged with native metadata success.

In particular, the square-symbol counts selected on channel 2 do **not**
establish encoder input: that channel writes the unread `1900` range in
this fixture. The live downstream investigation must follow the channel-3
`1800` range. These bus lifetimes alone prove neither conversion/FIFO loss
nor copy/filter semantics, encoder consumption or decoded audio.

`--instructions --source-a00 roms/5510-mu4-reference/InitData_R060.a00`
resolves the known non-DMA buffer reads using scoped staged operand bus-read
identity, not the current PC or most recent Execute callback. Read precedes
Execute in the staged core, so those alternatives can identify another
instruction. The query excludes forwarded values (no bus read), other
addresses and side-effect-disabled peeks; its transient context is not
hardware/save-state state and does not imply retirement.

Execute-entry observation also does not provide a historical register snapshot.
Stack allocation by PSHM/FRAME is published in earlier pipeline stages; a
younger FRAME can therefore have changed the live SP when the older PSHM's
Execute callback runs. Caller/argument attribution must use retained stack
accesses or independently qualified call-boundary observations, not interpret
the live SP as the pre-prologue value. A matching arithmetic result alone does
not validate that ABI attribution.

The 20-second `tmp/native-gates/mu4-recorder-instructions` capture identifies
every one of the 1699328 known-value reads, with no missing or capped identity:

| Actual read instruction | Captured opcode | Reads |
| --- | --- | ---: |
| `038eb5` | `e598` (`MVDD Xmem,Ymem`) | 452352 |
| `038eda` | `e598` (`MVDD Xmem,Ymem`) | 452352 |
| `03faa5` | `e598` (`MVDD Xmem,Ymem`) | 794624 |

All three opcodes match the source-hash-pinned original AA88 upload. This
establishes copy-read sites, not their destination writes, full routine
identity or encoder consumption. The existing 1536-case peripheral-MVDD
fixture and its 1536 save/restore replays additionally verify the scoped
read identity, logical page, captured opcode, wrong-address/peek rejection
and expiry before the destination write. The native buffer lifetimes and
both NAND hashes above remain unchanged. Follow these three copy sites to
their actual writes before assigning encoder-buffer semantics.

`--copies` adds instruction/source/value pairing with actual staged MVDD
destination bus writes. A bounded queue preserves read order for each
instruction/source identity; values validate the pair rather than selecting
it. The write query exposes the retained source address only during the
real MVDD write and excludes other destinations and peeks. The peripheral
MVDD fixture validates this query across 1536 cases and their save/restore
replays, including source pointers changed before the write.

The 20-second `tmp/native-gates/mu4-recorder-copy` capture pairs all 1699328
known-value reads with writes, with no value mismatch, missing pair, capped
site or pending read. There are no additional unknown-source copy pairs.

| Instruction | Paired words | Observed source range | Observed destination range |
| --- | ---: | --- | --- |
| `038eb5` | 452352 | `1800..187f` | `0db5..0e34` |
| `038eda` | 452352 | `1880..18ff` | `0e35..0eb4` |
| `03faa5` | 794624 | `1800..18ff` | `7000..7fff` |

These are actual paired-write counts and minimum/maximum addresses, not
proof of contiguous coverage or a fixed source-to-destination offset.
Native metadata success, buffer lifetime counts and both NAND hashes above
reproduce unchanged. The encoder-consumption boundary now lies downstream
of these copies; in particular, follow the `7000..7fff` writes with proper
shared program/data DARAM alias accounting before assigning encoder input
semantics. This still does not establish decoded audio or complete ADC/FIFO
delivery.

`--workspace` tracks latest-copy lifetimes in data `7000..7fff`. Every write
invalidates the previous origin, including program-space writes forwarded
through the shared DARAM handler. The nested data-space access is counted
once and tagged with its program origin; known program reads are not
silently classified as data operands. Only equal-value, full-word paired
copies from known receive-buffer words establish a new origin.

The 20-second `tmp/native-gates/mu4-recorder-workspace` run observes 802277
workspace writes, including 2430 through the program alias. Its 794624
sample-origin writes reconcile as 792576 epochs read exactly once, zero
retired unread, and 2048 still unread at the endpoint. All 792576 known-word
reads are data-space, non-DMA operand reads; there are no known-word program
alias reads. Another 62650 reads have no current sample-origin assignment
and must not inherit an overwritten origin. There are no value mismatches,
missing data-read identities or dropped sites.

| Actual workspace read instruction | Captured opcode | Reads |
| --- | --- | ---: |
| `039823` | `10eb` (`LD Smem,A`) | 396288 |
| `03982b` | `10e3` (`LD Smem,A`) | 396288 |

Both opcodes match the original AA88 source under `--source-a00`. Each count
equals 387 times 1024. The sample-repack contract below binds these reads
to actual stores and the following call; numerical agreement alone would
not establish that contract. Internally forwarded operands and values derived
after an overwrite are outside this bus-lifetime observation; an unread
epoch is not by itself evidence of lost conversion data. Native metadata
success and both NAND hashes above reproduce unchanged.

### Sample Repack Ingress

`mu4_recorder_read_census.py RUN/error.log --ingress --source-a00 SOURCE`
checks the AA88 wrapper at `0397f9` through its call at `03983b`. The
source contract pins 69 original words, including the stack-frame prologue,
selected stereo loop and return site. The wrapper saves three registers, reserves
`0803` stack words, and uses `SP+2` for a 2048-word local sample buffer.
It performs 1024 iterations: load the next word through AR3, store at AR2,
load AR3+1024 for the second lane, store at AR2+1, then advance AR2 by two.
It passes `SP+2` in A to `039852`.

The destination is **stack-local**, not a fixed hardware bank or an
independently inferred heap allocation. Stack position varies with caller
context; observing `66d8` repeatedly does not justify hardcoding it.
The two lanes describe the selected memory layout, not an independently
validated electrical ADC-channel mapping.

Read/store pairing uses actual staged operand identity and retained store
address/value inspection, followed by the real data-bus write. A word match
validates an already selected pair rather than selecting a source by sample
value. The isolated `store-observe` CPU profile checks all twelve direct
STL/STH variants and their pending-write save/restore replays, including
saturation, retained destination after DP changes, and rejection of other
instruction identities. Observation is read-only and the native recording
diagnostics remain fresh-run only.

The 20-second `tmp/native-gates/mu4-recorder-ingress-abi` capture validates
387 complete invocations. Each has 1024 known-source loads and equal-value
stores per lane, with no pending read, store, call, partial loop, mismatch or
capped observation. Both actual writes preserve the decoded stride, and
all 387 calls reach `039852` with A's low word equal to the written buffer
base. The `7000/7400` source pair occurs 194 times and `7800/7c00` 193 times.
The 792576 paired stores use these observed lane bases:

| Destination bases | Invocations |
| --- | ---: |
| `651a/651b` | 6 |
| `651c/651d` | 1 |
| `651e/651f` | 11 |
| `6520/6521` | 4 |
| `65fa/65fb` | 2 |
| `66d8/66d9` | 363 |

Native metadata success and both NAND hashes in the DRR section reproduce
unchanged. This binds actual peripheral-origin words through the workspace
and repack stores to a real pointer argument, rather than inferring sample
consumption from frame counts.

The call's stack lifetime bounds downstream provenance: follow the dynamic
pointer, not one frequent address. Repack/call acceptance is separate from
callee arithmetic, complete encoder-output, physical ADC, GSM speech and
end-to-end call acceptance.

### Stack Sample Consumers

`--consumer` follows the repacked words only from actual entry at `039852`
until return to `03983d`, using each invocation's measured buffer pointer.
Origins are seeded by the validated repack writes, not a later memory
snapshot or waveform match. Writes through either shared-memory address
space invalidate the overwritten origin. At return, all remaining tags
expire even if the stack bytes physically persist; a later reuse of that
address is not part of the same sample-processing call.

The checker binds every consumer lifetime to its corresponding repack call,
reconciles read/replaced-unread/unread-at-return epochs, and separately
counts program aliases, DMA ownership and missing staged read identities.
Forwarded operands and values derived after a write remain outside the
original-word bus-read claim. Source-word comparisons are candidates only;
an opcode match alone does not establish an entire routine's semantics.

The 20-second `tmp/native-gates/mu4-recorder-consumer` capture closes all
387 call lifetimes. All 792576 original words are read exactly once during
their corresponding call: no replaced-unread words, unread words at return,
unknown reads, in-buffer writes or missing data-read identities occur.
All reads are data-space and have no active DMA source owner.

| Actual original-word read site | Captured opcode | Reads |
| --- | --- | ---: |
| `03d9c8` | `e5b8` | 396288 |
| `03d9c9` | `e5b9` | 396288 |

Both words match AA88. Their MVDD forms read successive words through AR5
and select separate AR2/AR3 destinations, a source-level deinterleave pair.
Actual destination writes and subsequent arithmetic are not inferred from
these read counts or opcode matches. The native metadata result and both
NAND hashes in the DRR section remain unchanged. Signal-content correspondence
is qualified separately below; these reads alone do not establish encoder
output or sample timing.

### Consumed PCM And Recorded Content

The research bench writes `mu4_recorder_input.pcm` from actual known-word
reads at the native deinterleave pair. Every call must read offsets
`0..2047` in order with alternating captured instruction identities. The
output is interleaved stereo signed 16-bit little-endian data, not an
emulation input. Its terminal records count, FNV-1a, ordering failures and
cap; the checker binds the entire file to the preceding consumer/repack
proof and requires original metadata completion. It does not assign a
measured ADC sample rate to the capture.

The 20-second `tmp/native-gates/mu4-recorder-pcm` run produces 792576 words,
396288 stereo frames and 1585152 bytes, SHA256
`ea777e1f8fb86622776a5960ae1c0015074a3b6e0953c0b8973ef9e75fa0872a`.
Ordering/drop counts are zero and both NAND hashes above are unchanged.
Its independently extracted `TEST` has SHA256
`515e2c2c6f7c759d11870ae06879ae3f26036efd4b9cfca89ec4464cb7e59f48`.

The optional comparison uses the complete-packet host inverse projection,
not modified media fed back into MAME. Qualification re-exports the exact
fixture volume from this endpoint, checks its hash, independently extracts
`TEST` through 7-Zip, and recomputes the inverse projection with the existing
3072-byte native-prefix proof. The resulting AAC has SHA256
`b7e7b5d30eba38099e26d701b3db49e0d2f586c0cb16be834b1adae6f2b4257f`:
385 complete AAC-LC stereo packets declaring 44100 Hz. FFmpeg 8.0.1 decodes
all 394240 stereo frames without errors. Later native transform-state
evolution is **not** established by recomputing the host projection.

The fixed comparison excludes 4096 frames at each end of the common extent,
uses 386048 frames per channel pair, and reports all four input/output
combinations. Its integer-lag search is bounded to +/-4096; affine gain and
alignment fits are diagnostic, not model parameters or acceptance thresholds.

| Logical lane | Input RMS | Decoded RMS | Unfitted zero-lag correlation |
| --- | ---: | ---: | ---: |
| 0 | 8192 | 8104.46 | 0.994630 |
| 1 | 4096 | 4031.84 | 0.991619 |

Zero-lag cross-channel correlation magnitudes are below `0.000001`.
The periodic signals have hundreds of near-equivalent fitted offsets;
neither best-fit delay nor polarity establishes physical latency/routing.
This is content correspondence for the qualified host-projected recording,
not formal encoder conformance, measured analog behavior, native speech,
or proof that recording/playback tail policies are complete.

```sh
.venv/bin/python tools/mu4_recorder_pcm_check.py RUN --source-a00 SOURCE \
  --projected-aac PROJECTED --fixture-volume VOLUME --projection-trace TRACE
```

Omitting the projection arguments checks only capture integrity. Omitting
the volume/trace qualification explicitly leaves host projection derivation
unvalidated. NumPy supplies FFT correlation; FFmpeg/FFprobe and 7-Zip remain
external independent readers. No signal comparison changes the native media
or weakens the existing rejecting completion checks.

### Full Recording Transform

`mu4_recorder_full_transform_check.py` checks every entry to original AA88
routine `03f0fd`, its actual initial operand reads (`03f102/03f106`), final
stores (`03f45e/03f462`) and caller return. Entry SP is recovered from the
PSHM's retained stack destination, not the live pipeline register. Each call
has its own captured 82-word state; nonfinal state writes, lost identities,
unreturned calls and capped records are rejected. Source pinning covers the
original transform routine, and the independent forward model must match
both actual output words. The output-only bench is not save-state qualified.

The 20-second `tmp/native-gates/mu4-full-transform-retained` run has:

| Scope | Result |
| --- | --- |
| Entry/return/model coverage | 4539/4539; zero bad, unadmitted or dropped calls |
| Initialization callers | `03f820`: 9 calls/states; `03f847`: 32 calls/states |
| Data caller `03ee95` | 4498 contiguous 32-byte windows from cursor zero; one state |
| Total state variants | 42, including the final recording state |
| Captured native plaintext | 143936 bytes, SHA256 `bf03b9dfccf4ac54d3105c33f024c1d51aaca5a8d0abc9d29e61fed850a2526f` |
| Complete native ADTS prefix | 386 packets / 143592 bytes; SHA256 `56185de0c0b4cf42578a8c5653382b311e31b95b2c5816d61832144f3ee4e7bb` |
| Native partial packet 387 | 344 of its declared 372 bytes captured in complete transform windows |

FFmpeg with `-xerror` decodes the complete native prefix without diagnostics
to 395264 stereo frames / 1581056 s16le bytes, SHA256
`c29bdde1b26a8af68ca60082a2afe810464242295899acd018fe7a6bfbd3f9bf`.
This is a host decode of observed native plaintext, not inverse extrapolation.
It is not formal codec conformance or completed native playback.

The immutable seed, endpoint NAND, extracted `TEST` and captured input PCM
retain the hashes documented above. Independent 7-Zip extraction from this
endpoint anchors the first actual encrypted data window uniquely at byte 622.
All 143360 subsequent bytes equal actual native output. The first difference
is at data offset 143360 / file offset 143982. The extracted data region is
143570 bytes long: its final 210 bytes therefore cannot be accepted as the
continuation of the observed native stream. Native windows contain another
576 bytes beyond the equal prefix, including a complete packet 386; this
does not establish where or whether they were committed to NAND.

Default file checking rejects this divergence. `--inventory-divergence`
explicitly reports it while keeping `stored_file_correspondence_validated`,
`whole_file_validated` and `tail_completion_validated` false. Exported
`--output-native-plain` bytes are actual captured pre-transform windows,
never replacement media or a firmware input. The next persistence boundary
is the final writer/buffer lifecycle, not an assumed later transform-state
change. The final 28 bytes are accounted for by the pending adapter capture
below; intended persisted packet/sample extent remains unresolved.

```sh
.venv/bin/python tools/mu4_recorder_full_transform_check.py RUN/error.log \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00 \
  --stored-file READBACK/TEST --inventory-divergence \
  --output-native-plain NEW_NATIVE_PLAIN
```

### Pending Stop Buffers

`mu4_recorder_tail_check.py` pins 361 original AA88 words across compaction
helper `0389cf..038a30`, selected stop tail `038ff7..039001`, virtual dispatcher
`03e30c..03e332`, processing adapter `03e76f..03e7f9` and output forwarder
`03e918..03e92b`, the original method table and initial object selection.
The adapter unpacks groups of 16 source words into 32
byte-valued words for the transform/output path, then retains a shorter group
at `fd4e` with count `fd70`. Compaction uses buffer base `05e6+36=061c` and
fields `05ed/05ee`; the selected stop branch calls this helper with zero,
resets the processing descriptor and invokes method `0c` of object `05f4`.
That is a virtual call, not an unconditional file-close/flush operation.

`mu4_recorder_tail_observe.lua` takes a passive RAM snapshot at the actual
`05eb=1` stop-request write. Its 20-second `tmp/native-gates/mu4-stop-tail`
run reproduces metadata success, all 4539 transform calls, the original input
PCM and the endpoint NAND hash. The selected object is `eed6`, table `567e`:
method `0e` selects `03e76f`, but finish method `0c` is null. This is observed
at the stop boundary, not inferred to be immutable throughout the recording.

The original table at `567e..569b` independently agrees: among slots `01..0e`,
only `06 -> 03e6f5` and `0e -> 03e76f` are nonzero. Slot zero is not classified
as a callable method. Initialization records `02dcee..02dcf9` select
`eed6 -> 567e` and initialize `fd70=0`; startup `0070d6..0070e1` selects this
object through `05f4` and calls its initializer. The selected stop dispatch
therefore has no default finalizer to invoke. This is a bounded source/table
result, not an exhaustive proof over mutable/alternative objects.

A literal-word scan finds eight `fd70` references in the uploaded AA88 image:
one initializer destination and seven operands in `03e76f..03e7f9`. No extra
carry consumer appears in that census. Indirect accesses, bulk copies and
other images are outside this absence claim; do not promote it to "no flush
exists anywhere" or invent a new command from it.

At 10.100595994 seconds, `05ed=05ee=0120` and `fd70=000e`:

- The 288 words at `061c` serialize big-endian to exactly the 576 transformed
  bytes beyond the independently verified persisted prefix. Thus these
  selected pending counts have a measured two-byte-per-word correspondence.
- The 14 words at `fd4e` supply the remaining 28 plaintext bytes of packet 387.
- The terminal has both main-buffer counts zero. The previously qualified
  stop-write trace locates their clearing before completion; no missing
  external acknowledgement is needed to explain this reset.

Concatenating observed plaintext windows with that pending RAM carry yields
143964 bytes / 387 complete AAC-LC stereo packets, SHA256
`62f98e58fd41d8c12cb9481de3e48474cf76824d1c8ba9ce49ec182fc1e9a3ab`.
FFmpeg `-xerror` decodes without diagnostics to 396288 stereo frames, exactly
the captured input-frame count; s16le SHA256 is
`f2cf637767c7a9f821a36d09a8b3d939beb4f6b17f93af2d4aa0816f7588a616`.
This is an output-only host artifact from native observations, not a file the
firmware saved. It must never be substituted into NAND or playback input.

The checker rejects a buffer/suffix mismatch, changed dispatch, incomplete
capture or invalid packet chain. It keeps `stored_tail_complete` and
`native_flush_validated` false. The remaining policy question is whether a
different evidenced caller lifecycle commits these buffers or whether this
stop operation deliberately discards the partial extent. Do not manufacture
a flush, repair the file or label the observed truncation a hardware bug.

```sh
.venv/bin/python tools/mu4_recorder_tail_check.py RUN/error.log READBACK/TEST \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00 \
  --output-captured-aac NEW_CAPTURED_PENDING.aac
```

### Buffer Delivery

The nested callback is not a file writer. The original slot at `55b4..55b7`
selects `03eb4d`; its observed full-window path calls `03ee2b` at `03eb82`,
performing the in-place prefix transform. `03e76f` repacks those byte-valued
words into the main word buffer. Data reaching this callback therefore does
not establish that it reached NAND. The separate buffer-delivery/consumption
path owns that next boundary.

`mu4_recorder_delivery_check.py` adds 290 pinned AA88 words to the 361-word
tail contract. The native publisher at `038a31..038ad1` constructs an internal
five-word `0021` record, not a GSM or external mailbox opcode. After control
priorities, ordinary delivery requires `05ea!=0`, signed `05ee>0100` and
`05f0==0`. It advertises `0200` words only when `05ee>0200`; otherwise it
advertises `0100`. Exact equality at either threshold matters. The source is
`061c+05ed-05ee`; publication marks `05f0=1`. Consumers `038b43..038b55` and
`038b74..038b8b` subtract the acknowledged word count from `05ee` and clear
that transfer flag. The selector-3 stop path clears `05ea`, sets the selected
stop flags, and gives control publication priority over further data offers.

The twenty-second `tmp/native-gates/mu4-buffer-delivery-checked` run observes:

| Boundary | Result |
| --- | --- |
| Ordinary data offers | 272: 264 of 256 words and eight of 512 words |
| Matching consumer acknowledgements | 272; exact subtraction, chronological pairing, no outstanding offer |
| Acknowledged data extent | 71680 words / 143360 bytes, exactly the independently verified stored native prefix |
| Other publications | Three two-word `0016` control records |
| Final control | Argument zero; `05ea=0`, `05f0=0`, `05f1=1`, `05ed=05ee=0120` |

Thus 288 unread words remain with no transfer awaiting completion. The count
alone meets the threshold for a 256-word offer, but delivery is disabled;
even if that offer occurred, another 32 words would remain below the ordinary
offer threshold. This does not assume other control priorities are clear.
Together with the independently captured 14-word transform carry, this
accounts for the selected stop boundary without inventing a missing device
reply. The native transform calls, endpoint NAND and input PCM retain their
established hashes.

The observer uses Execute-boundary construction/consumer sites and the
already-computed record pointer, not a live-SP entry-frame assumption.
The checker rejects missing/capped events, overlapping offers, mismatched
acknowledgements, altered threshold selection and inconsistent final flags.
Its byte extent is count correlation with the independently verified file:
per-offer payload-to-NAND ownership is not established. No final buffer is
replayed into firmware. This explains the exercised immediate stop; it does
not establish which stop/drain lifecycle a normal product caller intends or
justify changing device timing to retain the suffix.

The acquired MA4 ARM image does not independently close that caller policy.
Its recovered EXT_UI constructor has only a self-test builder and an
existing-message forwarder in the bounded aligned-Thumb census, not a
recovered normal recording-finalization builder. The immediate peer of the
music DSP is the separate MSP430, whose program and normal-operation
transcript are absent. The MA4 service envelope is not the standalone DSP
command envelope. See the source-pinned sender census and explicit scan
exclusions in [5510 firmware consumers](5510_bringup.md#mu4-specific-receive-contract).
Resume the caller-policy branch on that missing evidence; keep independent
codec/CPU validation separate instead of inventing a drain command.

```sh
.venv/bin/python tools/mu4_recorder_delivery_check.py RUN/error.log READBACK/TEST \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00
```

### Metadata Overwrite Accounting

`metadata_overwrite_extent` pins 168 original AA22/AA55 words. In the selected
existing-ID3 path, updater `028420` seeks to zero, then clears the old tag using
one-word writes through `03c2c2`. For the 100-byte payload that is fifty calls.
It seeks zero again at `028455`, writes five header words and fifty payload
words. These are three phases, not three helper invocations.

The seek prefix `0033ea` resets directory-record cluster/offset fields
`+14/+15/+16`; it does not change the size at `+12/+13`. The writer tail
`03c3f6..03c3fe` doubles its processed-word count and unconditionally adds it
to that size, without taking a maximum against the seek position. Directory
serialization `03c421` writes the size's two halves into the on-disk buffer.
Caller `038eab` selects record `1444` for this update. Thus the source predicts
growth of 100 + 10 + 100 = 210 bytes despite in-place writes.

The complete twenty-second native run with `mu4_metadata_extent_observe.lua`
captures 352 writes in its 14.9--20-second record window, uncapped and without
an illegal instruction. All 52 observed size updates agree with the unfitted
source progression:

| Phase | Size (bytes) |
| --- | ---: |
| Initial independent readback | 143982 |
| Fifty two-byte increments | 144082 |
| Five-word header | 144092 |
| Fifty-word payload | 144192 |

The first/last size writes occur at 15.175756900/17.414961348 seconds. Both
NAND snapshots reproduce the before/final hashes above. Check this against
independent file readbacks with `mu4_metadata_extent_check.py LOG BEFORE_FILE
FINAL_FILE --source-a00 SOURCE`. The checker validates actual size-write pairs,
native metadata success and unchanged common payload, and reports exposed-byte
contents without assuming they are meaningful padding. Its bounded arithmetic
model excludes odd/too-small tags and overflowing extents.

This accounts for the extension through original firmware behavior; no seek,
file-size or media-byte workaround should be added to emulation. It does not
establish intended recording duration, justify decoding erased bytes, or close
the separate partial-buffer and playback-drain contracts.

### Finalized Cold-Reopen Control

The finalized NAND above cold-boots through original `bootplaystage`, using its
ordinary first-track serial request. Native title publication is `MAME`
(`title_verified=1`), so the existing protected-title transport/capture checks
apply unchanged; no title gate is bypassed. The copied NAND remains byte-identical
to its source after the run.

There are 376 successful calls and 376 converted stereo blocks. All 139872
consumed encoded bytes are covered by entry reservoirs and 69936 actual in-call
bus reads, SHA256
`8cec1952c11baf6d9410fed7bf4758c125a1e3c8640ac57036b844996aeab411`.
The captured returned PCM is 1540096 bytes, SHA256
`14ae1beaaa5fe4b4a8ba040c4b3b0929e607fe1c3314386e0fccf544c8c9e13a`.
Its first 1536000 bytes exactly equal the before-metadata capture; the larger
file extent permits one additional native call without changing those earlier
decoded samples. Against the host decode of the same complete-packet projection,
the 770048 compared channel samples have maximum error 11 and RMS 1.220195074
s16 units. Nine host-decoded blocks/3348 encoded bytes remain uncompared.

Use the original packet-session verification mode with the finalized capture,
the 385-packet projection and its host PCM:

```sh
.venv/bin/python tools/mu4_packet_session_check.py PLAYBACK \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00 \
  --reference-aac FINAL_PROJECTION.aac --reference-pcm HOST_385_BLOCKS.s16le
```

The supplied projection is a diagnostic reference, not independently generated
encoded input or formal conformance material. The first-eight-call arithmetic,
complete capture integrity, slot/timestamp checks all pass. Chronological serial
pairing matches all 1661061 completed TX words to DIN with no missing/mismatched
TX word; 30781 additional DIN words remain outside completed-TX pairing and only
have held-level value consistency, not pin/reset ownership proof. DMA-source
epoch pairing and decoder-to-DMA provenance are not established by that result.

The native twenty-second run still exits through the deliberate negative audio
completion diagnostic (exit 3), with zero illegal instructions. This is a
record/save/reopen and consumed-prefix validation milestone, not whole-track
draining, handset boot or GSM speech acceptance. Subsequent sections establish
bounded DMA/codec provenance and ideal headphone output separately; neither makes
post-cleanup serial activity proof that the first track drained.

### Finalized EOF and Lifecycle Restart

A twelve-second cold run of the same finalized NAND reaches first-session
cleanup without a decoder failure. The extent model agrees with all 377
input-11 requests: 376 take successor `0294ca`; the last takes `0294fd` and
`029500 -> 02956f`. At that last request, 72020 of 72096 file words have
been requested, leaving 76 words against a 186-word request. Finalization
changes the boundary numerically but does not bypass short-request cleanup.

Actual lifecycle writes and resident entry establish:

| Event | Emulated seconds |
| --- | --- |
| Lifecycle 4, first playback | 0.837819684 |
| Lifecycle 5 after cleanup ingress | 9.724472811 |
| Resident entry `026d62` | 9.892753474 |
| Lifecycle 7 | 9.913133926 |
| Lifecycle 3 | 9.927637877 |
| Lifecycle 4, restarted playback | 10.115552110 |

This rules out the twenty-second bench cutoff as the reason the first session
ends. Continuing output after this boundary must be attributed to its session,
not counted as draining the first session. Whether restart is the intended
repeat/next-track policy, and whether buffered encoded data should be decoded
before cleanup, remain unresolved. No firmware or media change is justified.
The copied NAND remains byte-identical to the finalized source.

Use `mu4_finalized_cleanup_observe.lua` with the cold `bootplaystage` bench,
`-debug -debugger none -seconds_to_run 12`, then run
`mu4_cleanup_ingress_check.py RUN/error.log --first-resident-only
--lifecycle-restart --input-file EXTRACTED_TEST`.

The first session contains 1886 ingress events; 405 later events are explicitly
excluded from the uncapped 2291-event capture. The lifecycle option checks all
captured lifecycle-write ordinals, ordered `4 -> 5 -> 7 -> 3 -> 4` values and
intervening resident entry, not buffered-tail contents or audio completion.
Run the RAM snapshot observer separately: it and the ingress observer install
breakpoint actions at the same address `02956f`, and composing them does not
produce the snapshot. Refill and ingress use independent debugger counters.

The original AA22 resident explains a deliberate restart route. The 70-word
source-pinned `resident_cleanup_restart` contract in `mu4_context_contract.py`
covers the state dispatcher and `03a987..03a9b6`: lifecycle 5 selects this
branch, preserves continuation 4 (otherwise writes 6), then tests mode word
`3740` against 3. In other modes it notifies with 9, increments track index
`3765`, and wraps it to 1 when it exceeds track count `3768` by one. For a
valid one-track catalogue this selects track 1 again. Mode 3 instead calls
`03999f` and notifies with `0d`, without advancing the index. Both paths clear
`3732` bit 4 and write lifecycle 7. These are decoded operations, not semantic
names for user repeat settings or evidence that mode 3 means stop.

The runtime continuation write at `03a98c` and lifecycle write at `03a9b2`
agree with that resident branch. A separate cleanup-buffer/restart capture
establishes mode 0, track 1, count 1 and continuation 2 in the cleanup snapshot.
Ordered contexts at `03a98f`, `03a9a2`, `03a9a5`, `03a9af` then show
continuation 6 and track `1 -> 2 -> 2 -> 1`, with wrap-test AR2 equal to 1.
Thus this run takes the source-pinned index-advance/wrap path, not mode 3's
nonadvance path. This identifies the selected mechanism, not the meaning or
correct default of a user repeat setting.
Tests pin every recovered word, check index advancement for all valid indices
in catalogues of sizes 1 through 128, and cover the 16-bit maximum wrap. The
cleanup snapshot also identifies object `160a`, buffer `162e..1d4c` (1822 words),
and a 32-bit reservoir `fff95080` with reader index 186. At captured decoder
ordinal 376, the reservoir plus remaining buffer exactly matches the final
projection's remaining 3348 bytes (nine complete 372-byte packets); 300 later
buffer bytes remain outside that comparison. No byte-offset fitting is used.
Those packets are present at cleanup but are not decoded in the first session.
This rules out unavailable buffered bytes as their explanation, not that they
must be decoded: intended priming/padding, refill lookahead and drain policy
remain distinct questions. Do not feed these bytes artificially or replace
the original caller's EOF result to make a completion gate pass.

Reproduce with `mu4_restart_tail_observe.lua` on the same cold twelve-second
bench, then `mu4_restart_tail_check.py RUN --source-a00 SOURCE
--reference-aac FINAL_PROJECTION.aac`. It validates the original source hash
and 70-word contract, cleanup RAM and projection correspondence, and all four
restart contexts against the snapshot-derived source model. The NAND copy
remains unchanged. The observer owns only the cleanup-buffer breakpoint plus
distinct resident sites, avoiding the ingress/snapshot breakpoint collision.

The same snapshot contains descriptor byte extent 144192, counter 144192,
policy `aa1a=1`, short-input flag `aa1c=1`, and a pending 186-word refill.
The counter equals the **full byte extent** because the decoded short-input
branch replaces its ordinary word-count value with that full extent; do not
reinterpret the terminal sentinel as 144192 consumed file words. This matches
the original branch's state transition, not a missing completion IRQ.

Reconstructed codec controls establish the output lifecycle independently of
continuing serial traffic. After lifecycle 5 at 9.724472811 seconds, firmware
writes paired headphone mute at 9.724548625 and digital DAC mute at 9.724619770.
It restores paired volume at 10.115636626 and clears digital mute at
10.115707772, after restarted lifecycle 4 at 10.115552110. The restart/tail
checker now requires both the short-input snapshot state and the
enabled/muted/re-enabled control sequence. These are digital control facts,
not measured analog silence or proof of the intended track length.

Thus the observed first session takes a software-selected EOF cleanup, mutes
output, and advances/wraps the catalogue while nine reference packets remain
buffered. The remaining question is the product caller's intended input/drain
contract, not whether DMA needs more time or a synthesized peripheral reply.
This evidence does not establish that discarding those packets is either a
firmware bug or an emulation bug. Preserve the rejecting completion gate until
an independent intended extent or legitimate end-buffer control resolves that
distinction.

The bounded original R060/R061 comparison does not expose a revised drain
contract in the surveyed AA99 paths. Across49,843 and49,863 mapped words,
respectively, both images satisfy the same cleanup-ingress, reservation,
context-lane, publication and decoder-return contracts, including their
literal/immediate-call censuses. Of235 pinned final-input words, only
`02958b` differs: the far-call operand changes from `9005` to `9019`
in the separate control-2 cleanup branch. The default `aa1a=1` store and
one-pass gate are unchanged. This is not optional-callee equivalence, R061
execution, an all-image writer closure or proof that other revision changes
cannot affect playback. Do not substitute R061 as an evidenced tail fix.

The comparison requires the hash-pinned original R061 container and records
both input identities in its JSON report:

```sh
.venv/bin/python tools/mu4_context_contract.py \
  roms/5510-mu4-reference/InitData_R060.a00 \
  --compare-playback-revision roms/5510-mu4-reference/InitData_R061.a00
```

External API material needs an implementation match before it can supply
missing semantics. TI's [1Q2002 Audio Solutions Guide, pages18/24](https://www.ti.com/download/vf/audio/audiosolutionsguide1.pdf)
describes the DA150 audio-library platform, but its cellphone example offers
UART/HPI/SPI alternatives rather than Nokia-specific wiring or a drain ABI.
The available [C55x codec package](https://www.ti.com/tool/C55XCODECS) instead
targets C55x devices; its
[AAC datasheet](https://dr-download.ti.com/software-development/software-codec/MD-SFKgRd85AD/1.00.002/MPEG4AAC_LC_Decoder_C55x_DataSheet_1_00_00_03.pdf)
specifies a C5505 implementation. Those buffer sizes are not a recovered
DA150/MU4 input contract, and TI's C5510 device name does not identify the
Nokia5510 handset. No matching end-of-input contract was established from
these materials. They do not justify extra encoded bytes, a guessed flush
command or changed `aa1a` policy.

### Decoded-Bank Copy Correspondence

`mu4_pcm_copy_observe.lua` captures actual copy-helper source reads from
`7000..7fff` and destination writes in a 1.0--1.25-second window, selecting the
mapped `13e0/1460` object contexts. It also tracks source-bank **data-space** writes
in that window, retaining the last observed write per address, and aggregates all
writes in `1000..137f` by context PC. This range includes processing/control
state as well as sample banks; a DMA minimum/maximum address is not evidence
that every intervening address is an audio buffer.

The uncapped capture has 19712 reads and 19712 selected copy writes, with
446846 source writes tracked and 449214 destination-range writes across 55
context PCs counted. An in-order read/write comparison leaves no pending read
and has no word mismatch. For the first eight complete converted blocks:

- all 16384 channel samples match their actual last observed conversion write,
  subsequent source read and destination write;
- every sample is copied exactly once, across 256 fixed 64-word chunks;
- actual destination addresses preserve channel/chunk geometry in
  `1000/1040/1080/10c0`, with no bank change within a chunk;
- producer/copy chronology, word values and addresses determine correspondence;
  no waveform delay, gain or sample-value alignment is fitted.

The 3328 later copied words are outside the detailed eight-block conversion
proof and are reported separately. The existing first-eight-call arithmetic
checks also pass. Source reads at the end of each chunk have GENPC contexts
`02a368..02a36a`, although the MVDD word is `02a366`; narrowing the read tap to
that instruction's PC would miss real bus reads. The source-pinned helper
interval, rather than a guessed single-PC owner, is deliberate.

Run the observer on the finalized NAND with original `bootplaystage`; it exits
after two seconds once its bounded window is closed. Check with
`mu4_pcm_copy_check.py RUN --source-a00 SOURCE`. This is a short memory-copy
observation, not the twenty-second playback gate. Register/PC snapshots remain
context, not retained instruction ownership. The profile aliases program-space
`2000..7fff` onto data RAM through `shared_daram_w`, which forwards writes through
`AS_DATA.write_word`. A repeat capture also taps program-space `7000..7fff`:
it observes zero alias writes in the bounded window, with all counts and sample
correspondence above unchanged. Older captures without that counter report its
coverage as unknown, not zero. This closes the mapped program-alias caveat for
this window, not the unvalidated physical DA150 RAM extent. It does not establish
downstream DMA read epochs, codec pin/reset ownership, whole-track playback,
analog sound or GSM speech.

### Actual DMA Buffer Epochs

The `bootplaystage` diagnostic records channel-3 reads only while
`source_read_matches(3, AS_DATA, address)` owns the actual bus transaction. In
the same 1.0--1.25-second window it retains the latest data-space write to
`1000..137f`, including value, mask, time and context PC. No register or sample
is changed by either observer. This is diagnostic provenance, not hardware state
and not a save/restore acceptance run.

A repeat finalized-NAND capture preserves the eight-block copy proof above and
contains 22050 consecutive actual DMA reads, from 1.000004308 to 1.249982966
seconds. Of these, 19458 match full-word writes at context `02a2ea`; 2592 have
no writer observed after the window opens and remain unclassified. **Zero** DMA
reads have the copy helper (`02a367`) as their latest writer. Thus the copied
decoder samples are not directly consumed in this window: a later processing
write intervenes. This does not prove that processing preserves sample values,
nor that every copied word eventually reaches DMA. The processing contract below
narrows that boundary without fitting a copy-to-DMA alignment.

Run `mu4_pcm_dma_epoch_check.py RUN/error.log` alongside the copy checker. It
checks actual read ownership, consecutive ordinals, last-write chronology and
word equality; any direct copy-origin claim must also match an actual logged
copy-write epoch. Its output deliberately keeps native audio validation false.

### In-Place Output Processing

`decode_output_processing` pins the original AA99 kernel `02a2c6..02a2ea`
and its five parent calls (55 source words including call/rewind witnesses).
`BRC=63` and `RPTB 02a2e9` apply a fixed-point multiply/accumulate loop to 64
sample words. The sample enters through `LD *AR7,T`; the final instruction is
`STH A,*AR7+`. State writes use AR3/AR4/AR5. Between the five calls, the parent
rewinds AR7 by 64, processing the same block again. This establishes source
geometry, not coefficient meaning or an independently evaluated filter equation.

`mu4_output_processing_observe.lua` composes the copy observer with a bounded
1.06--1.10-second sample-bus observation. It selects reads by the helper interval
and actual address matching AR7, not by a single presumed instruction PC. Actual
read contexts are `02a2d2/02a2ea/02a2cd`, including pipeline/block-repeat effects;
PC snapshots remain context, not retained instruction ownership.

The uncapped, zero-illegal capture has 52068 sample reads and 17448 processing
writes, of which 17265 are nonzero. One initial write is left-censored; no final
sample read is unmatched. All fully observed writes equal their input words in
this configuration. This is a measured no-change result, not proof that every
coefficient setting is an identity or that CPU arithmetic is independently valid.

41754 reads follow prior processing writes; 10314 follow another writer at
context `029fee`. None directly follows the copy helper. Within the same bounded
window, 3348 actual channel-3 DMA reads match observed processing-write epochs
exactly, with no unmatched DMA read whose latest writer is inside that window.
The earlier copy proof also reproduces unchanged. Check the chain with
`mu4_output_processing_check.py RUN/error.log`, alongside the copy and DMA
epoch checkers. Do not extend the result to serial/codec delivery or whole-track
audio. The scalar contract below closes the `029fee` input/write link in the
bounded window. The configured filter's bounded arithmetic proof follows below;
direct copy-to-DMA provenance remains false.

### Signed Output Scaling

The original AA99 helper `029fa2` selects signed extension, clears fractional
multiply and accumulator saturation, and enables store saturation. The pinned
kernel (`029fea..029fee`) repeats 64 signed `MPY *AR5+,*AR2,A` operations and
stores with `STH A,4,*AR1+`. Therefore the independently evaluated bus-word
result is `clamp_s16((signed16(sample) * signed16(coefficient)) >> 12)`, with
arithmetic-floor truncation. `decode_output_scale` pins the 18 setup/kernel words;
the coefficient-update policy and all helper callers are not thereby decoded.

`mu4_output_scale_observe.lua` adds actual operands/stores to the processing and
copy observers over 1.06--1.10 seconds. A global data read can belong to DMA even
when GENPC is inside this helper: 38 such reads occur. The checker excludes them
using the device's explicit active channel-3 source-read records, matched by
timestamp/address/word, and rejects ambiguous CPU/DMA collisions. PC alone is
not an operand-ownership test. The full-space read tap is installed shortly
before the window to avoid unrelated bootstrap overhead.

The uncapped, zero-illegal capture verifies all 3456 outputs against actual
sample/coefficient reads, with zero pending or left-censored scalar operands.
All coefficients are `050f`; all 3456 results differ from their input words,
and 3453 are nonzero. Every input matches an actual `02a367` copy-write epoch.
10314 subsequent processing reads match these scalar-output epochs exactly.
The eight-block decoded-bank copy proof and bounded processing/DMA checks also
pass on this same run. This explains the amplitude change through original
instructions and observed coefficient data, not fitted gain or waveform delay.

Check with `mu4_output_scale_check.py RUN/error.log --source-a00 SOURCE`, alongside
the copy, processing and DMA checkers. The scalar checker alone establishes
bounded decoded-copy/scalar/processing dataflow, not filter arithmetic,
complete-track consumption, serial/codec delivery or audible output.

### Bounded Filter State Equations

`mu4_filter_operands_observe.lua` captures actual helper reads/writes over
1.061--1.062 seconds, including the configured section-state objects beyond
`137f`. `mu4_filter_arithmetic_check.py` evaluates the pinned kernel independently
using signed 40-bit accumulator arithmetic, unsigned-by-signed MACSU products,
arithmetic right shifts and signed store saturation. Its supported mode is
SXM=1/OVM=0/SST=1, with explicit FRCT handling. The instruction semantics are in
TI SPRU172C; the same-location pending-write bypass is described by SPRU131G
section 7.3.2. This is a bounded application-kernel check, not general CPU or
codec conformance.

One iteration has 25 actual memory reads and four checked writes: current-input
history, low feedback state, high feedback state and output sample. The adjacent
MPY high-state operand is forwarded from the pending store and produces no
separate RAM read. The reference derives that value and checks the actual
published state; it does **not** claim to have bus-observed the forwarded operand.
Operand addresses must match the source-derived circular-state/coefficient
geometry, and dependent memory reads must follow their state publications.

The uncapped, zero-illegal capture validates 640 iterations/2560 writes against
16000 actual operand reads. All iterations have nonzero feedback state. Five
section-state objects (`1318/1328/1338/1348/1358`) each cover consecutive samples
`1000..103f`; five (`1370/1380/1390/13a0/13b0`) each cover `1040..107f`.
These are 64 stereo sample frames through five sections, **not** 640 audio frames.
All direct coefficients are `1000`, and the computed outputs match the neutral
sample values already observed. Eleven explicit DMA reads are excluded from CPU
operands; ten separately counted boundary-context reads occur before section
iterations. Ambiguous ownership or unmatched traffic fails the checker.

Check with `mu4_filter_arithmetic_check.py RUN/error.log --source-a00 SOURCE`.
The copy, scalar and processing checks also pass on this same run. This closes
the configured kernel's bounded state/sample arithmetic, not arbitrary equalizer
settings, every block's arithmetic, complete-track draining or analog sound.
The bounded serial-epoch contract below follows these samples through actual
buffer loads and transmission. Full-session and analogue acceptance remain separate.

### Actual Shift-Buffer and DIN Epochs

The McBSP has an optional diagnostic callback for reset, ignored write, buffer
accept/overwrite, fresh/repeated shift load, frame restart and completed word.
These are observations of internal device transitions, not external pins, saved
hardware state or inferred PC ownership. The MU4 bench records them only over
1.0--1.25 seconds. Buffer acceptance carries the active DMA destination owner
and source-read ordinal; load and completion identities are propagated from
those actual transitions, never selected by matching waveform values.

Early observer exits now finalize binary capture counts/hashes exactly once.
The original twenty-second playback verifier still rejects short execution;
`mu4_serial_epoch_check.py RUN` separately validates bounded capture integrity.
The native run's TX, DIN, DMA, slot, timestamp, DXR and decoded-return files are
byte-identical before/after this observation change. The `setup` device suite
also passes McBSP underrun, packing, synchronization and save-state checks.

The bounded capture has 66150 lifecycle events: 22050 each of fresh load,
accepted write and completed word. It attributes 22049 completed source epochs,
leaves one initial completion unattributed and one accepted word pending at the
window endpoint. No overwrite, reset, ignored write or repeat occurs in this
window; synthetic checker tests cover their distinct accounting.

All 128 final filter words (`1000..107f`, 64 stereo frames) have exactly one
actual DMA read, ordinals 21121--21248, one owned DXR write, one observed shift
load and one completed TX word. Every completion pairs chronologically with
codec DIN within the existing 84-ns clock bound, with exact values. Bank
`1000..103f` reaches callback slot 1; `1040..107f` reaches slot 0. These are
observed slot labels, not yet a physical left/right polarity verdict. The startup
reset accounting below explains the one-word phase shift in this bench.

Across the complete short capture, 105345 TX completions pair with DIN without
mismatch or missing TX. The 367 additional DIN callbacks cannot be assigned
from word completions alone; bounded transition accounting below identifies
their held-level intervals in the emulated model. The copy, scalar and
filter arithmetic checks pass on this same run. This proves a bounded native
decoded-sample path to digital codec ingress, not DAC activation, analogue sound,
whole-track draining, full-session epoch accounting, handset boot or GSM speech.

The McBSP diagnostic `wait_frame` transition now exposes the existing
external-clock underrun branch: after the active word completes with no fresh
DXR, transmission waits for the next frame rather than publishing another
word completion. The event carries retained DXR and does not change serial
timing or output. The lifecycle checker rejects it with an active shift or
full buffer, and checks retained-word consistency where known.

A fresh finalized-input capture at `tmp/native-gates/mu4-wait-frame` records
142 closed wait-to-next-load/reset intervals in the startup window. Ninety-three
codec DIN callbacks in those intervals equal the held final-bit level of the
observed retained word. This qualifies those bounded intervals; it does not
close all 367 additional callbacks or establish measured pin timing. The seven
TX/DIN/DMA/slot/time/DXR/decoded-return binary streams are byte-identical to
`mu4-dac-native`. The setup fixture separately retains its underrun recovery,
packing and save/restore checks.

The additional `nonshifting_din` check starts a held-level interval only after
a known TX completion has established its final output bit and no younger
shift has begun. `wait_frame` and reset-with-no-active-shift delimit that
interval; loading/restarting another shift ends it. A reset during an active
word deliberately invalidates pin knowledge rather than inferring it from
the word value or the reset event's zero payload. Chronological TX/DIN
pairing is retained separately, and a claimed nonshifting interval cannot
contain a TX completion. This is consistent with the current McBSP reset
implementation, which does not emit a new output bit.

On the same fresh capture, all 367 unpaired callbacks are now accounted for:
93 in frame waits and 274 in reset-held intervals, with zero uncovered words.
The validator also rejects incorrect held levels; its negative fixture keeps
an interrupted zero-valued shift unknown even when DIN is zero. This closes
the short capture's emulated nonshifting-word accounting, not measured reset
electrical behavior, physical pin timing, full-session decoded ownership or
whole-track audio acceptance.

### Full Short-Capture Buffer Accounting

The research bench also writes a compact `mu4_playback_tx.events` stream
over the same interval as its TX/DIN captures, rather than only the two text
observation windows. Each 16-byte little-endian record contains nanoseconds,
word, transition ID, active DMA owner and source-read ordinal. The terminal
pins its record/byte count and FNV-1a hash; a capped or truncated stream is
rejected. This diagnostic output is explicitly not a save-state artifact.
Use `mu4_serial_epoch_check.py RUN --full` to require it; older captures
without it retain bounded-window coverage only.

The fresh finalized-input run `tmp/native-gates/mu4-full-serial`, ending at
2.016666667 seconds, has 316,043 transitions. Every one of its 105,254 DXR
writes and 105,345 TX completions has exactly one corresponding event.
Observed loads/completions account for 105,252 completed DMA-owned source
epochs, source ordinal 1 discarded by reset, and ordinal 105254 pending at
the endpoint. The 93 initial repeated-word completions remain explicitly
unattributed to a captured DMA source; their word values are not used to
invent provenance. All 367 nonshifting DIN callbacks retain the separate
reset/wait accounting above, with zero uncovered words.

All seven pre-existing TX/DIN/DMA/slot/time/DXR/decoded-return files are
byte-identical to `mu4-wait-frame`. The validator requires coverage of every
captured TX/DXR word and rejects duplicate completions, not just malformed
files. A negative test removes a completion and recomputes the file checksum
to prove that protocol coverage still fails. This extends actual serial
buffer ownership across the full short capture; it does not extend the
independent decoder/filter arithmetic window, prove whole-track draining,
assign physical timing or establish handset speech.

### Transport Across EOF Cleanup and Restart

The separate `--transport-only` mode requires the full binary transition
stream and exact chronological/value pairing for every captured TX completion,
but does not require or promote the selected decoder/filter arithmetic window.
The ordinary checker mode retains that independent provenance requirement.
Neither mode promotes whole-track audio acceptance. Duplicate-write detection
uses direct set membership, not a growing per-write set union, so that check
remains linear in the number of buffer events.

The finalized-input run `tmp/native-gates/mu4-session-serial` observes the
original `mu4_restart_tail_observe.lua` lifecycle through 11.016666667 seconds.
Both `mu4_serial_epoch_check.py RUN --transport-only` and the source-/AAC-pinned
`mu4_restart_tail_check.py` pass. The first-session cleanup and restart timing
are unchanged, including return to lifecycle 4 at 10.115552110 seconds.
All seven digital files retain the exact prefix of `mu4-full-serial`.

The 2,651,838 transition records account for 883,938 completed TX words and
869,246 captured DXR writes: 869,242 DMA-owned source epochs complete, ordinals
1/785215/788489 are discarded by reset, and ordinal 869246 remains pending
at the endpoint. The 93 initial repeated completions still have no captured
DMA provenance. Every TX completion pairs exactly with codec DIN; all 15,574
additional DIN callbacks are explained by observed frame waits (14,698) or
reset-held intervals (876), with zero uncovered callbacks. Retained repeats
remain associated with their actual source epoch, not a fitted sample match.

This closes captured serial-buffer accounting across this EOF/restart
lifecycle. It does not explain why the original software enters cleanup with
complete encoded packets still buffered, validate those packets' native
decoding, prove intended tail/drain policy, or establish physical timing.
The strict whole-track audio gate remains separate and rejecting.

Full-capture validation also checks the recording horizon, not merely file
counts/hashes. For the declared active MU4 master profile, the final DIN
callback must be no more than one USB/272 frame before closure, using the
research machine's fixed 11,995,200-Hz MCLK and the programmed CLKIN divider.
The 11-second capture ends 15,818 ns after its last callback, below the
22,676-ns bound. A self-consistent early prefix, future callback, missing
metadata or unqualified inactive clock profile is rejected. This is an
observer-coverage check against the configured bench clock, not a physical
frequency measurement or a relaxation of the independent audio gate.

### Codec Routing and Startup Phase

The early transition capture brackets 0.817--0.824 seconds and includes actual
codec frame edges and raw programmed McBSP controls. PCR `000e`, XCR1 `0140`
and XCR2 `0044` select external clocks/frame, two 16-bit words and frame-ignore
behavior. Raw SPCR2 inspection does not synthesize its dynamic XRDY/XEMPTY bits;
buffer ownership comes from the transition observer instead.

DMA source 1 is accepted at 823308662 ns. An observed XRST reset at 823309163 ns
discards it before any shift load/completion. Source 2 therefore completes in
codec slot 0 first, and subsequent odd/even source words occupy the shifted
slots seen above. This is an actual reset/discard identity chain, not a
zero-valued waveform alignment. It explains the current bench's phase but does
not establish whether its initialization timing matches fitted silicon. Do not
"correct" it by relabeling slots or overriding the codec's LRSWAP bit.

The native control stream also requests and later restores headphone volume:

| Control | Register effect |
| --- | --- |
| `0580` | Left headphone volume code 0, simultaneous right volume/mute update |
| `05f0` at 837904201 ns | Both volume codes `70`; independent zero-cross/control fields retained |
| `0a01` at 837975346 ns | Digital DAC mute clear |

At the validated sample block the other controls are power `007`, analog path
`010`, format `053`, rate `023`, active `001`: DAC selected, DAC/output amplifier
not powered down, digital mute clear and no headphone-mute request. These are
programmed-control facts, not proof of applied zero-cross gain or analog sound.
SLWS106H sections 3.1.3/3.2.4 specify the headphone volume/mute coupling; the
device now updates the partner's seven-bit volume field while preserving its
independent controls. Executable tests cover independent writes, simultaneous
mute and right-to-left updates. Clock/framing and all seven binary captures
remain unchanged.

The ideal DAC/headphone contribution below implements the recovered control
path. Measured analog filtering/zero-cross thresholds, physical channel routing
and the cause of the initialization reset timing remain distinct from the
already-proved digital sample delivery. Whole-track draining remains open.

### Ideal DAC-to-Headphone Output

`tlv320aic23_device` now exposes two MAME sound outputs for the **DAC contribution**
to the headphones. It latches both 16-bit DSP slots together after the second
word, applies DAC LRSWAP, active/DAC-select/power/digital-mute gates, output power
and nominal one-dB headphone gain steps. Programmed volume and effective volume
are separate state: an enabled zero-cross update waits for an ideal digital
zero/sign change, with no timeout. This is explicitly not the measured analog
detector threshold. The sample pair, effective codes and pending updates are
saved. The audio stream is an oversampled zero-order hold, not a simulated
delta-sigma clock or interpolation filter.

Executable pin/DMA fixtures check complete stereo latching, selected route,
power and digital mute, nominal -6-dB gain, deferred volume across a real sample
sign change, pending-update/sample save/restore, and DAC LRSWAP. Existing digital
clock, receive, transmit and packing checks remain green. Analog ADC conversion,
bypass/sidetone/FM mixing, line-output pins, converter filters/group delay and
electrical rails are not modeled by these outputs. Do not add a fitted filter
delay to the current renderer or call its waveform physically accurate.

A finalized-NAND native run renders `headphone.wav` with `-samplerate 48000
-wavwrite headphone.wav`, using the same two-second operand observer. All seven
digital capture files remain byte-identical. `mu4_dac_output_check.py RUN --wav
RUN/headphone.wav` independently reconstructs the control and input-frame
history without assuming an uncaptured early zero crossing. All 1764 DAC frames
over 1.06--1.10 seconds match their actual DIN codes, effective volume and ideal
normalized levels (maximum decimal-rounding error `4.995e-13`). There are 3280
nonzero channel samples. The same run retains the 128-word digital source-epoch
proof and the decoder/copy/scalar/filter checks.

The host render contains 96001 stereo frames at 48 kHz, with nonzero counts
44957/44971 and peaks 642/1147. These are integration diagnostics, not fitted
waveform tolerances or a full playback oracle. The isolated MU4 bench is now
marked imperfect sound rather than no sound hardware. Whole-track completion,
physical analog acceptance, main-handset audio and GSM speech remain unproved.

### Full Lifecycle DAC Controls

The native research bench additionally records every post-overlay stereo
DAC latch in `mu4_headphone.frames`: little-endian timestamp, two signed
16-bit codes, two effective volume bytes, zero reserved word and two IEEE-754
binary64 normalized levels (32 bytes total). Count, FNV-1a, cap and observation
scope are explicit in its terminal. This is diagnostic output, not saved
hardware state. `mu4_dac_output_check.py RUN --full --wav RUN/headphone.wav`
requires the full serial capture, independently replays DIN/control epochs,
and checks every stereo latch rather than selecting only nonzero samples or
a favorable window. Missing frames, altered levels, reserved fields and
non-finite outputs are rejected. The default bounded checker remains separate.

The fresh finalized-input run `tmp/native-gates/mu4-session-dac` reaches the
same 11.016666667-second cleanup/restart endpoint. All 449,756 stereo frames
match their independently reconstructed codes, effective gains and ideal
levels, with zero normalized-level error. The source-/AAC-pinned restart-tail
check passes on the same run, and all seven pre-existing digital captures are
byte-identical to `mu4-session-serial`.

| Observed digital-mute epoch | Stereo frames | Nonzero channel levels |
| --- | ---: | ---: |
| Initial mute | 876 | 0 |
| First unmuted session | 391901 | 763969 |
| EOF/restart mute | 17247 | 0 |
| Restarted unmuted output | 39732 | 69874 |

The EOF mute's observed frames span 9.724632709--10.115698468 seconds;
unmuted restarted output begins at 10.115721144 seconds. These are actual
DAC-latch observations correlated with recorded controls, not a waveform lag
fit or a claim that encoded buffered tails were drained. The 48-kHz host WAV
contains 528001 stereo frames, nonzero counts 452865/452946 and peaks 642/1149.
Those host-render quantities establish integration/non-silence, not analog
waveform conformance or exact host-sample timing. The converter-filter,
electrical, whole-track and handset-speech exclusions remain unchanged.
