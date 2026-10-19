# Native AAC validation

## Boundary

The MU4 bench executes the 5510's separate music DSP. Its captured recording
and FFmpeg comparison are diagnostic evidence, not decoder conformance or
native handset speech acceptance. All 375 observed decoder calls have consumed
input-byte coverage; detailed arithmetic and output-store binding cover eight.

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
contains ALS reference code, not an acquired AAC comparison utility.

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
Remaining ABI prerequisites are compressed-input buffer/refill ownership.

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
boundary; compressed-input and refill construction remain open.

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
Per-store ownership, raw output normalization, actual input-read binding and
broader media accuracy remain open.

```sh
python3 tools/mu4_aac_abi_check.py PRIVATE_RUN_DIRECTORY \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00 \
  --reference-pcm GENERATED_CONTROL_HOST.s16
```

The checker accepts an internally consistent observation and reports errors;
its zero exit status is not waveform conformance. Unit tests explicitly
retain nonzero waveform errors while rejecting malformed/faulted/partial
captures and raw-to-converted disagreement.

## Next Acceptance Boundary

1. Recover the native decoder initialization and input framing contract;
   distinguish raw access units, ADTS and recording-container transport.
2. Establish supported LC tools and a fixture transport preserving original
   compressed payload bytes. Do not transplant another decoder's state.
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
