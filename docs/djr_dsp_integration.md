# `djr-747/nokia-dct3-emulator` DSP knowledge catalogue

This catalogue records architectural knowledge reviewed from
[`djr-747/nokia-dct3-emulator`](https://github.com/djr-747/nokia-dct3-emulator)
without importing its GPL implementation. The reviewed checkout was commit
`c50f7e272bf10c37a40c57174dbde84d9717b7e3` dated 25 July 2026.

The upstream native analysis backend combines a modified C54x core with a
bring-your-own Nokia 5110 DSP ROM4 dump. That dump is git-ignored and was not
present in the reviewed repository. Consequently, upstream address annotations
are useful leads and reproducible claims about its private input, but are not
local primary evidence until checked against a legally obtained, hashed dump or
physical traces. No source is copied into this BSD-licensed driver.
A second fetch on 25 July 2026 confirmed that `origin/main` still ends at
`c50f7e2`; there is no newer public register trace to catalogue.

## Findings and admission status

| Finding | Confidence here | Architectural consequence |
|---|---|---|
| Upstream maps MCU shared word `0x100A8` to a host-command request and adjacent `0x100AA` to acknowledgement on its 5110 ROM4 input | **not portable as stated**: both 3210 v5.01 and v6.00 organically write `0x100AA=ffff` through shared-control command `0x1c` during the answered interval, then restore zero during clearing | retain the 5110 mapping only as a product-specific research lead; do not implement request/ack pending arithmetic in the generic DCT3 HLE |
| Shared cell `0x85D` (MCU byte offset `0x0BA`) is a distinct audio/tone command | upstream ROM4 annotation only | retain as a tracing lead; do not conflate it with host request cell `0x854` or implement its command meanings yet |
| COBBA serial control uses DSP I/O port `0x2D` as a 12-bit data latch/readback and `0x2C` as `{read,register}` select: low nibble is one of 16 registers, bit 4 distinguishes read from write, and a write-select commits the previously latched data | upstream ROM4 disassembly and port-level co-simulation; consistent with Nokia's documented serial control bus, but individual register meanings remain unverified locally | `nokia_cobba_device` now owns this opaque control transport. Nothing drives it from MCU command `0x08`, and no audio-mux meaning is assigned until paired/local evidence supplies one |
| The reviewed ROM4 co-simulation later corrected its codec-sample finding: DSP port `0x21` is bidirectional (read microphone ADC, write earpiece DAC) and is serviced per codec-frame interrupt; port `0x20` is a second channel. MMR `0x22` carries `0xCxxx` parallel COBBA control frames rather than PCM | upstream private-ROM disassembly/trace, not yet reproducible with the local 3210 DSP image | retain as a strong backend design lead: a future C54x backend should terminate per-sample I/O at `nokia_mad2_pcm_device`; do not collapse control, PCM and RF/MFI surfaces or claim local port numbers yet |
| Exact audio interrupt vector, activation cells, and a roughly 18.6 kHz tone cadence | explicitly experimental/tuned in upstream comments and potentially product-specific | not admitted; DCT3 voice PCM documentation currently supports an 8 kHz frame boundary, so rate/vector remain configuration/evidence questions |
| Host-side PCM sinks, resampling, injected samples, and environment gates | implementation conveniences | do not adopt as hardware behavior; host audio is a consumer/producer beyond the emulated COBBA analogue pins |

## Resulting local boundary

The faithful direction is:

```text
MCU firmware
  -> DSPIF command doorbell + request/ack mailbox
  -> DSP command dispatcher / Layer 1 and speech processing
  <-> DSP serial PCM endpoint
  <-> COBBA codec and its sample clock
  <-> microphone / earpiece analogue endpoints
```

The observed MCU-shared words carry control, not speech samples. Their exact DSP-side
mailbox/register semantics may vary with the product DSP firmware. GSM traffic-channel
bursts and speech frames belong to Layer 1/speech processing, while COBBA owns
PCM conversion and physical sample timing. The current driver preserves the
raw `0x100A8` value without interpreting command bits and passively traces the
independent `0x100AA` writes. The next implementation is admitted only when evidence identifies
the DSP-side speech-frame or serial-sample contract; command value `0x860b`
alone is not such evidence.

## NSE-8 DSP-image recovery limit

The paired MCU firmware publishes one contiguous DSP data-memory image: v6.00
uploads 247 words at `0x2206`, while v5.01 uploads 241 words at `0x2286`.
Address-independent sequence comparison gives an 85.2459% match, including a
final 174-word common block at offsets `0x049`/`0x043`. The contents form
coefficient/configuration runs, not an observed sequence of COBBA
`{register,value}` transactions. Separate type-`0x0d` and type-`0x3c`
bootstrap tables are byte-identical between the firmware versions and have no
Answer lifecycle correlation.

The local NSE-8 DSP inputs are placeholders:

| Input | Size | Classification | SHA-256 |
|---|---:|---|---|
| `dsp_prom` | 49,152 | uniform `ff` fill | `abd8e2a70d51a53287941838446e6ed141005d401faf597fd7c6de0bbc8c329d` |
| `dsp_drom` | 16,384 | uniform `ff` fill | `0fbba07a833d4dcfc7024eaf313661a0ba8f80a05c6d29b8801c612e10e60dee` |
| `dsp_pdrom` | 4,096 | uniform `ff` fill | `f47a8ec3e9aff2318d896942282ad4fe37d6391c82914f54a5da8a37de1300c6` |

MCU execution can therefore prove when speech is requested, and the uploads
prove DSP-owned data-memory contents, but neither exposes the DSP-local port
writes that program COBBA. Recovering the analogue-mux encoding requires a
non-placeholder, legally obtained NSE-8 DSP image executed through a compatible
backend, or a physical COBBA-control-bus trace. Until then the product's
MIC2/EAR internal path is explicitly contained in an `hle_voice_profile` and
must not be presented as decoded register behavior.

## Reusable research workflow

Upstream demonstrates a useful licensing and architecture pattern: keep a real
C54x core plus private ROM dump in native analysis tooling and expose a narrow
backend seam to the distributable emulator. If adopted locally, the core must
come from a license-compatible source or remain an optional analysis tool; its
traces should be converted into independent protocol tests rather than copied
implementation.
