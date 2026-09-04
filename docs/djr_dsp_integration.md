# `djr-747/nokia-dct3-emulator` DSP knowledge catalogue

This catalogue records architectural knowledge reviewed from
[`djr-747/nokia-dct3-emulator`](https://github.com/djr-747/nokia-dct3-emulator)
without importing its GPL implementation. The initial reviewed checkout was
commit `c50f7e272bf10c37a40c57174dbde84d9717b7e3` dated 25 July 2026. A second
review on 4 September 2026 covered public commit `b93a7f7` and its native ROM4
C54x/COBBA analysis path.

The upstream native analysis backend combines a modified C54x core with a
bring-your-own Nokia 5110 DSP ROM4 dump. That dump is git-ignored and was not
present in the reviewed repository. Consequently, upstream address annotations
were initially useful leads rather than local primary evidence. The historical
archive has since been recovered, retained outside Git, hashed, and structurally
validated. No dump or third-party source is copied into this BSD-licensed
driver. The public repository still excludes the Nokia mask-ROM image; local
hashes and provenance are recorded in `roms/README.md`.

## Findings and admission status

| Finding | Confidence here | Architectural consequence |
|---|---|---|
| Upstream maps MCU shared word `0x100A8` to a host-command request and adjacent `0x100AA` to acknowledgement on its 5110 ROM4 input | **not portable as stated**: both 3210 v5.01 and v6.00 organically write `0x100AA=ffff` through shared-control command `0x1c` during the answered interval, then restore zero during clearing | retain the 5110 mapping only as a product-specific research lead; do not implement request/ack pending arithmetic in the generic DCT3 HLE |
| Shared cell `0x85D` (MCU byte offset `0x0BA`) is a distinct audio/tone command | upstream ROM4 annotation only | retain as a tracing lead; do not conflate it with host request cell `0x854` or implement its command meanings yet |
| COBBA serial control uses DSP I/O port `0x2D` as a 12-bit data latch/readback and `0x2C` as `{read,register}` select: low nibble is one of 16 registers, bit 4 distinguishes read from write, and a write-select commits the previously latched data | upstream ROM4 disassembly and port-level co-simulation; consistent with Nokia's documented serial control bus, but individual register meanings remain unverified locally | `nokia_cobba_device` now owns this opaque control transport. Nothing drives it from MCU command `0x08`, and no audio-mux meaning is assigned until paired/local evidence supplies one |
| The reviewed ROM4 co-simulation later corrected its codec-sample finding: DSP port `0x21` is bidirectional (read microphone ADC, write earpiece DAC) and is serviced per codec-frame interrupt; port `0x20` is a second channel. MMR `0x22` carries `0xCxxx` parallel COBBA control frames rather than PCM | upstream private-ROM disassembly/trace, not yet reproducible with the local 3210 DSP image | retain as a strong backend design lead: a future C54x backend should terminate per-sample I/O at `nokia_mad2_pcm_device`; do not collapse control, PCM and RF/MFI surfaces or claim local port numbers yet |
| ROM4 serial-control routines are labelled at DSP addresses `0x4604` (write), `0x465c` (read) and `0x4610` (single/multi-register wrapper). The self-test path at `0x4ae1` selects registers 0, 5 and 6. | public annotations derived from the private 5110 image; protocol shape agrees with the locally admitted opaque device | retain as address-level reproduction guidance. Registers 5/6 are measurement inputs, not evidence for audio mux/gain fields. |
| The parallel MFI control path writes `0xCxxx` frames through DSP MMR `0x22` or an interrupt-masked `0x32` variant: bits 15..12 select one of 16 registers and bits 11..0 carry data. A reset/run-mode path reportedly emits `0xc008` at DSP PC `0x301f`. | public C54x trace annotation; locally unexecuted | a real backend needs a separate callback into an opaque parallel-control bank. Do not feed these frames into the serial register file or PCM endpoint. |
| The native co-sim uses serial-register defaults 5=`0x160`, 6=`0x010`, parallel AGC=`0x160`, ready status D=`0x00c`, and zero I/Q. | explicitly synthetic no-signal/self-test inputs, not physical measurements | never promote these values as COBBA reset constants. They are only labelled compatibility-fixture candidates. |
| The available image lacks an acquisition overlay expected behind DSP routine `0x250b`; one self-test report is therefore still staged by the analysis harness. | explicit upstream limitation | a ROM4 run is evidence for executed resident/loaded code and observed port traffic, not automatically a complete DSP or RF implementation. |
| Exact audio interrupt vector, activation cells, and a roughly 18.6 kHz tone cadence | explicitly experimental/tuned in upstream comments and potentially product-specific | not admitted; DCT3 voice PCM documentation currently supports an 8 kHz frame boundary, so rate/vector remain configuration/evidence questions |
| Host-side PCM sinks, resampling, injected samples, and environment gates | implementation conveniences | do not adopt as hardware behavior; host audio is a consumer/producer beyond the emulated COBBA analogue pins |
| NSE-8 and NHM-5 MCU firmware independently publish tone oscillator words at shared byte offsets `0x0ae`/`0x0b0`, amplitude at `0x0b6`, with oscillator values in quarter-Hz units | local paired-ROM traces; the common `0x0e10` value produces 900 Hz | `nokia_dsp_hle_device` owns this typed mailbox contract and drives the temporary MAME tone sinks; `nokia_dspif_device` remains transport-only |

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

A renewed evidence audit on 26 July 2026 found no admissible register
semantics. Public Nokia COBBA-GJP service material, including the
[NSB-5 system-module manual](https://manualmachine.com/nokia/7190/5055290-service-manual/),
independently confirms the three microphone inputs, separate EAR/HF outputs,
internal source selection, gain control, and distinct control and PCM serial
buses. It does not publish the 16-register `{address,value}` encoding. The
local NSE-8 DSP regions remain
uniform `0xff`, the paired MCU uploads remain coefficient/configuration data,
and no organic MCU-visible transaction reaches `control_select_w`. Therefore
the opaque register transport remains deliberately disconnected from the HLE
MIC2/EAR profile. `test_speech_media_boundaries.py` rejects any attempt to
make an opaque control write alter that route or its gains. The independent
`make verify-cobba-control` mapped-device gate now proves the admitted
latch/select/read grammar at runtime and restores its complete control state;
it does not promote any register to a mux or gain control.

## Reusable research workflow

Upstream demonstrates a useful licensing and architecture pattern: keep a real
C54x core plus private ROM dump in native analysis tooling and expose a narrow
backend seam to the distributable emulator. If adopted locally, the core must
come from a license-compatible source or remain an optional analysis tool; its
traces should be converted into independent protocol tests rather than copied
implementation.

## Current execution target

ROM acquisition is closed for the 5110 experiment. The next bounded milestone
is to reproduce the independent native backend with the untouched 65,535-word
program export and complete DROM map, recording the exact backend commit and
runtime options. Success requires more than reaching a screen:

1. record resident-program, DROM and uploaded-overlay execution coverage;
2. distinguish genuine DSP output from any host-staged self-test or bootstrap
   response;
3. capture ordered DSPIF, serial COBBA, parallel MFI and PCM activity through
   boot, idle and one call lifecycle; and
4. convert corroborated bus transactions into independent local fixtures before
   assigning semantics to COBBA registers or DSP mailbox words.

The recovered archive does not close the product-correct DSP-ROM problem. It is
an NSE-1/5110 ROM4 input. The Nokia 3210 still needs its own internal DSP images
or physical traces before the HLE backend can be replaced faithfully for NSE-8.

### First reproduced run

The native backend at upstream commit
`b93a7f7143ea8d6d636ae920ac29258d3f6666f6` accepts the untouched historical
131,070-byte program export and complete DROM through `DSP54_IMAGE` and
`DSP54_DROM`. A 30-million-MCU-instruction NSE-1 v5.30 run reached the Security
code framebuffer with 74 MCU-observed DSP acknowledgements. The DSP executed
the firmware-driven staged upload, host-command ISR, timer dispatcher, MDIRCV
enqueue path and COBBA serial routines.

The backend's per-instruction histogram classified 30,176,666 credited DSP
instructions at 25 million DSP steps:

| backend region | instructions | share |
|---|---:|---:|
| PROM `0x0900..0x0cff` superloop label | 0 | 0.0000% |
| PROM idle `0x4070..0x4097` | 345,578 | 1.1452% |
| BIST `0xd800..0xf200` | 0 | 0.0000% |
| overlaid DARAM `0x0080..0x27ff` | 627,055 | 2.0779% |
| low return pad | 0 | 0.0000% |
| other program regions | 29,204,033 | 96.7769% |

`other` is the backend's residual address bucket, not proof that all those
instructions came from immutable mask ROM. More precise provenance requires a
map-aware coverage export rather than inference from this coarse partition.

This run is not promotable as an unassisted DSP implementation. Its log proves
three active compatibility mechanisms: `SEEDDARAM` replays captured DARAM
content after the boot clear, the COBBA model loops serial samples, and
`SELFTEST_MEAS` writes validator-compatible report fields after the DSP's own
transform. Run
`make check-dsp-rom4-cosim LOG=<captured-log>` to reproduce this classification;
the checker deliberately reports promotion as blocked while any such assist is
observed.

### Compatibility-assist audit

Paired 30-million-MCU-instruction runs disabled each assist independently. The
results classify the missing contracts without treating the native harness as
a hardware specification:

| Disabled assist | Observed result | Classification and replacement condition |
|---|---|---|
| `SEEDDARAM` | warm boot reaches run mode with `0x0d80=0xfc00`, `0x0d00=0xfc00`, `0x0926=0`, then produces 70 rather than 74 DSP acknowledgements, runaway execution and a blank framebuffer | non-hardware snapshot replay. The recovered block catalogue targets 1,189 of the 2,048 replayed words in `0x2000..0x27ff`, so this range cannot be described as immutable ROM automatically reloading DARAM. Replace it only after the firmware-driven upload, DSP demand-load and warm-reset preservation/clear sequence accounts for the required contents. |
| `SELFTEST_MEAS` | the DSP reaches the validator without the patch, then the MCU requests an organic reason-4 warm reboot at caller `0x258d35`/resume `0x258d3d` | direct output synthesis. The native harness writes `0x1202..0x1206 = {0010,0000,0000,0000,0160}` after the DSP transform. These are accepted fixture values, not recovered COBBA readings. Replace them with the DSP routine's real acquisition inputs and COBBA response; do not promote the accepted output tuple as a reset value. |
| COBBA codec-serial loopback | upload and self-test measurement proceed with 74 acknowledgements, but the phone presents `CONTACT SERVICE` | faithful-in-intent peripheral behavior at an exploratory host boundary. The DSP sets COBBA register 8 bits `0x0610`, enables the C54x BSP with `0xc008 -> 0xc0c8`, sends BDXR `0x0aaa`, polls BDRR, then clears the COBBA bits. TI's BSPC map proves the transition sets RRST/XRST while DLB remains clear, so COBBA externally returns the word. `nokia_cobba_device` now owns the evidenced register predicate and completed-word echo; a future C54x backend must drive it through real BSP timing and ready state instead of host polling. |

The recovered catalogue is independently auditable with:

```sh
python3 tools/dsp_rom_audit.py \
  --block-map roms/research/nse1-rom4/working/dsp_blocks.txt \
  --flash roms/noki5110/5110f530.fls
```

The parser intentionally calls the four undecoded descriptor fields
`auxiliary`, `staging`, `chunk_length` and `flags`. Destination and word count
are supported by the catalogue's own block extents and the independently
decoded v5.01 upload at `0x2286..0x2376`; stronger field names require decoding
the ROM4 loader. Duplicate and overlapping catalogue entries are unioned when
coverage is reported. The flat `dsp_full.bin` is consequently treated as a
mapped execution snapshot containing ROM and overlaid runtime memory, not as a
literal chip-ROM declaration.

The untouched v5.30 flash contains all 27 six-word descriptor signatures at a
consistent raw-file displacement of `+0x840` from the recovered logical
`DAT_*` labels. The audit requires that canonical location for every record;
additional matches from duplicate descriptors are reported but do not weaken
the invariant. This establishes the catalogue's ROM provenance without using
the IDA database as executable input.

An 80-million-instruction follow-up used only the native harness's physical-key
replay to enter the handset security code. It reached stable NSE-1 standby with
the operator `Radiolinja` and `Menu` visible, while retaining the same 74 DSP
acknowledgements. This extends the execution result from boot presentation to
interactive idle; it does not remove any of the assists above.

The captured interface evidence is bounded as follows:

- DSP release, warm reset, staged MCU upload and DSP acknowledgement traffic
  are present;
- the host-command ISR, timer ISR, DSP receive enqueue/dequeue paths and
  overlaid DARAM execute;
- host-assisted COBBA codec-serial loopback and codec-frame interrupts are
  present; and
- no complete parallel-MFI transaction, bidirectional speech PCM call, or
  call setup/release lifecycle was produced by this run.

The unlocked run records 74 MCU-observed DSP acknowledgements, 19 ring-cursor
transitions, 16 DSP-to-MCU host doorbells and eight explicitly logged codec
frame interrupts. Its raw DSP-port trace contains 235 writes to serial-select
port `0x2c`, plus 147 writes and 322 reads on serial-data port `0x2d`. Port
`0x21` has five writes and two reads and port `0x20` one write; these counts
establish activity only and do not assign product-port semantics beyond the
qualified findings above. `dsp_rom4_cosim_check.py` extracts these counters so
later runs can be compared without preserving an interpretation in code.

The last item is an acquisition-harness boundary, not negative evidence about
the ROM: the native 5110 composition has no GSM network/call peer that can drive
a complete call. A call-lifecycle capture therefore requires the real backend
to attach to this repository's existing call-capable GSM composition, or a new
equivalent native peer. Replaying guessed call traffic would not close the
gate.

## Local backend prototype

The distributable driver now has a source-level substitution seam:
`nokia_dsp_backend_device`. DSPIF commit, service, doorbell and shared-cell
notifications target that abstract endpoint, as do reset and tone observation.
The existing `nokia_dsp_hle_device` implements it without behavioral changes;
only HLE-specific product contracts retain an optional concrete finder.

This is deliberately not a C54x core or an importer for the GPL analysis
backend. A future clean-room, MAME-compatible C54x device can occupy the same
machine-config tag and implement the endpoint while using the existing DSPIF,
MAD2 interrupt, COBBA and PCM devices. Promotion still requires product-correct
ROM declarations and removal of every active compatibility assist.

## Acquisition and reproduction status

The historical
[`5110 DSP ROM v4`](https://nokiafree.org/forums/archive/index.php/t-39175.html)
thread records that AlexD published a complete dump in August 2003 using a
protection weakness. The attachment later disappeared and was privately
re-shared in 2006. The surviving discussion says ordinary ICE/Code Composer
access could not read the protected ROM: external instructions receive
`0xffff`, while code executing from on-chip ROM can read it. It does not
publish a reproducible extraction program.

The later
[`GSMHosting mirror`](https://forum.gsmhosting.com/vbb/f83/alexd-made-nokia-5110-rom4-dump-288554/)
describes its archive as AlexD's Nokia 5110 DSP-ROM memory dump with an annotated
IDA project. The recovered four-volume `sss.part*.rar` package matches that
description exactly and also contains the program and DROM inputs expected by
the native backend. The untouched program export contains 65,535 words rather
than a fabricated final word at DSP address `0xffff`; see `roms/README.md` for
the volume and extracted-input hashes. No author-published canonical digest is
known, so this establishes a strong provenance chain rather than cryptographic
identity with AlexD's original upload.

The practical software-only route is therefore:

1. preserve the recovered historical volumes and extracted inputs outside Git;
2. reproduce the native 5110 co-sim before treating DSP addresses as local
   evidence;
3. capture serial `0x2c/0x2d`, parallel MFI and PCM-port activity across boot,
   call setup, volume changes and release; and
4. translate only corroborated transactions into independent BSD-licensed
   tests against the local COBBA and MAD2 PCM devices.

The official TI
[`TMS320C54x CPU Reference Guide`](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
documents the processor and on-chip ROM protection model. It is a backend
implementation reference, not a Nokia ROM source.

If the private ROM cannot be obtained, the
[NSE-1 service manual](https://www.eserviceinfo.com/downloadsm/26879/Nokia_5110.html)
lists factory `COBBAWRX`, `COBBARDX`, `COBBACLK`, `COBBARSTX`, `VCOBBA` and
`DSPXF` test points. That is the lowest-risk known physical capture target;
the corresponding NSE-8 data/strobe pad identities have not been established.
