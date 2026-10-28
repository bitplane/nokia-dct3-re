# `djr-747/nokia-dct3-emulator` DSP knowledge catalogue

This catalogue records architectural knowledge reviewed from
[`djr-747/nokia-dct3-emulator`](https://github.com/djr-747/nokia-dct3-emulator)
without importing its GPL implementation. Reviewed references include
`c50f7e272bf10c37a40c57174dbde84d9717b7e3`, `b93a7f7` and
`2b4c10f`; their admitted contracts and rejected interpretations are below.

The upstream native analysis backend combines a modified C54x core with a
bring-your-own Nokia 5110 DSP ROM4 dump. That dump is git-ignored and was not
present in the reviewed repository. Consequently, upstream address annotations
were initially useful leads rather than local primary evidence. The historical
archive has since been recovered, retained outside Git, hashed, and structurally
validated. No dump or third-party source is copied into this BSD-licensed
driver. The public repository still excludes the Nokia mask-ROM image; local
hashes and provenance are recorded in `roms/README.md`.

## Contract references

- [Locally reproduced native observations](rom4_native_observations.md).
- [Recovered image annotations and loader lifecycle](rom4_dsp_loader.md).
- [Locally observed native activation and sample buffers](native_speech_activation.md).
- [MADos and physical acquisition references](mad2_acquisition.md).

## Findings and admission status

| Finding | Confidence here | Architectural consequence |
|---|---|---|
| At pinned revision `2b4c10f`, `src/mad2/dsp/dsp_rom4.c` explicitly derives its ROM4 registration HLE from this project's protocol work | dependent implementation, not independent corroboration of a native DSP | Successful sibling HLE registration cannot validate missing resident code, DSP-port completion or RF sampling. |
| `src/services/identity_provision.c` describes tool-node10 MBUS status/version/MSID and record-write exchanges, citing service-tool captures | protocol lead only: the cited raw captures are not in the acquired source archive | Own NPE-3 firmware produces a D2 version reply with its MBUSTIM clock enabled; the external sequence ACK stops retransmission. See `mbus_controller.md` for the independently decoded asymmetric ACK receiver and storage control. Identity writes, minted identity and arbitrary memory access remain unqualified. |
| Upstream maps MCU shared word `0x100A8` to a host-command request and adjacent `0x100AA` to acknowledgement on its 5110 ROM4 input | **not portable as stated**: NSE-8 call-time `100AA` writes have a product-local contract; independently checked NSE-1 packed `100A8` parameters must survive acceptance and reach HPINT consumers intact | Do not alias packed payload bits onto requests. NSE-1 native ring-plus-slot/HPINT boot and level delivery pass without that alias; the in-service-mask projection and other physical request sources remain unvalidated. |
| Shared cell `0x85D` (MCU byte offset `0x0BA`) is a distinct audio/tone command | upstream ROM4 annotation only | retain as a tracing lead; do not conflate it with host request cell `0x854` or implement its command meanings yet |
| The reference labels I/O range `30..3c`, including `34`, as COBBA mode/rate output | **not admitted for the word peripheral**: at reference `2b4c10f`, writes are sinks and `35/36` have no explicit receive/status branch. Local ports `34/35/36` have buffered transfer/state grammar, and the pending queue contains two alternating-XOR-valid FBUS-shaped frames. | No independent completion/clock evidence follows from the reference label. Preserve this boundary; packet types `34/35/36` in bootstrap replies are a separate namespace from DSP I/O ports. See `5110i_bringup.md`. |
| COBBA serial control uses DSP I/O port `0x2D` as a 12-bit data latch/readback and `0x2C` as `{read,register}` select: low nibble is one of 16 registers, bit 4 distinguishes read from write, and a write-select commits the previously latched data | upstream ROM4 disassembly and port-level co-simulation; consistent with Nokia's documented serial control bus, but individual register meanings remain unverified locally | `nokia_cobba_device` now owns this opaque control transport. Nothing drives it from MCU command `0x08`, and no audio-mux meaning is assigned until paired/local evidence supplies one |
| The external ROM4 co-simulation labels I/O `0x21` as bidirectional microphone/earpiece PCM and `0x20` as a second channel; it separately labels data MMR `0x22` as parallel control | **unresolved interpretation**: the local NSE-1 resident image has nine candidate I/O `21` reads, all constant-mask/write-back sequences, and twelve writes including three initialization words. These do not establish sample traffic. See `cobba_control_boundary.md` and its checked census. | do not promote the external port label into a microphone/DAC contract. Keep data MMRs, DSP I/O, PCM and RF interfaces distinct; operational sample-source/readiness evidence is still required. |
| ROM4 serial-control routines are labelled at DSP addresses `0x4604` (write), `0x465c` (read) and `0x4610` (single/multi-register wrapper). The self-test path at `0x4ae1` selects registers 0, 5 and 6. | public annotations derived from the private 5110 image; protocol shape agrees with the locally admitted opaque device | retain as address-level reproduction guidance. Registers 5/6 are measurement inputs, not evidence for audio mux/gain fields. |
| The external reference labels data MMR `0x22` / `0x32` writes as packed parallel MFI control, including `c008` at instruction `301f` | **ownership disputed**: local `0022` capture confirms the word, not its owner. TI's section 8.2 maps `20/21/22` to BDRR/BDXR/BSPC and the local self-test matches serial reset/release. The `0032` alias claim is not locally established. | Native `0022` now owns BSP state; unsupported `0032` forwarding has been retired to an opaque saved latch. Neither address supplies independent MFI evidence. See `cobba_control_boundary.md`. A `Cxxx` upper nibble alone is not an MFI transaction. |
| The native co-sim uses serial-register defaults 5=`0x160`, 6=`0x010`, parallel AGC=`0x160`, ready status D=`0x00c`, and zero I/Q. Local instruction and register-access traces now reproduce reader `0x4610`: selectors 5/6 organically read `0x160`/`0x010`, and the caller stores `0x0016`/`0x0010` in its self-test input object. | calibrated nominal no-signal inputs, not measured reset contents | `nokia_cobba_device` owns the serial input values and ready status. They are hardware inputs to the unmodified DSP calculation, not synthesized report output; retain that distinction until physical measurements are available. |
| The boot self-test builder calls resident slot `0x250b`, which is a no-op in the recovered image. The actual analog acquisition is the later serial-register sequence at `0x4ae1..0x4afb`, through reader `0x4610`. | local program decode plus successful mapped-device trace | `0x250b` needs no replacement. COBBA supplies the register inputs and the DSP owns the response transform. |
| Exact audio interrupt vector, activation cells, and a roughly 18.6 kHz tone cadence | explicitly experimental/tuned in upstream comments and potentially product-specific | not admitted; DCT3 voice PCM documentation currently supports an 8 kHz frame boundary, so rate/vector remain configuration/evidence questions |
| Host-side PCM sinks, resampling, injected samples, and environment gates | implementation conveniences | do not adopt as hardware behavior; host audio is a consumer/producer beyond the emulated COBBA analogue pins |
| NSE-8 and NHM-5 MCU firmware independently publish tone oscillator words at shared byte offsets `0x0ae`/`0x0b0`, amplitude at `0x0b6`, with oscillator values in quarter-Hz units | local paired-ROM traces; the common `0x0e10` value produces 900 Hz | `nokia_dsp_hle_device` owns this typed mailbox contract and drives the temporary MAME tone sinks; `nokia_dspif_device` remains transport-only |
| The reference's ROM6 HLE answers type11 with a type88/error1 timing result carrying the requested carrier | **admitted only as a bounded lab negative-completion lead**: own NSM-3 decode and passive delivery prove the error/candidate fields and firmware counter0->1/selector2->1 update, followed by late-PIN DCS registration; the reference is not a raw DSP capture | A typed NSM-3 research-peer contract completes unsupported unlisted mode4 requests without fabricating measurement success. Fixed PIN schedules and independent call/SMS/storage/restoration gates are in `8210_bringup.md`; native ROM6 behavior and general mode semantics remain unproved. |

## CPU comparison leads

The reviewed `b93a7f7` interpreter's block-repeat endpoint test uses the
sequential continuation `PC == REA + 1`, rather than a numeric `PC > REA`
test. Its explanation identifies calls whose targets lie outside the loop:
the callee must execute before its return completes the endpoint. This is
a useful hypothesis for the MU4 recorder's outside-body CCD/interrupt path;
the local `repeat-isr` fixture exercises both an in-body return and REA+1.

This is not an independent timing oracle. The reference executes instructions
atomically, whereas the local staged core must retain operands and order
overlapping accesses. The reference also keeps a separate `rptb_active` flag;
its ST1 write path alone does not establish reactivation after an inner loop
finishes and firmware restores BRAF. Validate that composition against TI's
architectural contract and executable fixtures, not the existence of RPTB or
FRETE handlers in another emulator. Use small shared instruction programs to
compare architectural results; do not import its implementation or assume
that its 5510 scaffold validates the separate MU4 recorder.

One concrete incompatibility is the reviewed header's `ST1_BRAF` at bit 14.
TI SPRU131G figure 4-2/table 4-2 assigns BRAF to bit 15 and CPL to bit 14,
matching the local core. Compare architectural flag meanings before raw
status words; do not adopt the reference mask as an expected value for
saved-context tests.

For `f48d/f58d` SQUR, the reference squares a signed 16-bit high word.
SPRU172C section 4-161 specifies A(32-16), a 17-bit operand, and leaves T
unchanged for the accumulator form. The local atomic handler already uses
that 17-bit operand; its staged admission needs independent signed-width,
FRCT/OVM, untouched-T and producer-ordering tests, not a copied handler.

The reviewed Smem MAC handlers are likewise not a result oracle. Their
`FC00/2800` match includes MACR but treats opcode bit 9 as subtraction,
preceding the separate MACR handler; that handler also rounds the product
before accumulation. TI SPRU172C section 4-82 instead specifies rounding
the accumulated result, with opcode bit 9 selecting rounding for this form.
Use the shared Smem addressing grammar as a decoding lead, then validate
operation, rounding and untouched-T independently. The local `mac-bk` and
`mac-smem` fixtures exercise these contracts without importing reference code.

The reference's `f48f/f58f` NORM handler is also unsuitable as an oracle:
it conditionally shifts by one and decrements T. SPRU172C section 4-122
instead specifies `dst = src << TS`, with TS the signed six-bit T count
in the documented -16..31 range. Do not use successful execution in that
interpreter to justify accepting the MU4 recorder's out-of-domain count.
The former local failing count was loaded from memory at `03:dacf`, not
supplied by an adjacent EXP. It exposed a direct-ISR-return repeat-restoration
defect, now independently reproduced and corrected; the original recorder
continues through twenty seconds without an instruction exclusion. Its
remaining transaction boundary is in [5510 bring-up](5510_bringup.md).

The reviewed DST handler also forces an even address. SPRU172C section
4-47 example 2 explicitly starts at odd address `0101`, storing the high
word there and the low word at `0100`. Preserve the requested-address/XOR-1
pairing rather than treating the reference's alignment as a memory contract.

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

## Local backend prototype

The distributable driver now has a source-level substitution seam:
`nokia_dsp_backend_interface`. DSPIF commit, service, doorbell and shared-cell
notifications target that abstract endpoint, as do reset and tone observation.
It is a `device_interface`, so both an ordinary device and a future
`cpu_device` can implement it without duplicate `device_t` inheritance. The
existing `nokia_dsp_hle_device` implements it without behavioral changes;
only HLE-specific product contracts retain an optional concrete finder.

No GPL analysis implementation is imported. A clean-room, MAME-compatible C54x
device occupies the backend seam while using the existing DSPIF, MAD2 interrupt,
COBBA and PCM devices. Promotion still requires product-correct ROM declarations
and complete peripheral validation.

`nokia_dsp_c54x_device` contains the clean-room C54x CPU, maps the recovered
HPI DARAM alias through `nokia_dspif_device`, implements the ROM4 OVLY program
view, and terminates DSPINT at HPINT. The NSE-1 research configuration now
selects this backend and executes reset/release, staged upload, host-command
INT2 dispatch, the complete MCU transmit ring and the challenge response
without either historical compatibility assist. Other handsets retain the HLE
as the established behavioral reference.

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
