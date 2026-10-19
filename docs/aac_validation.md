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
