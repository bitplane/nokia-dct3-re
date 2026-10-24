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

## Current native boundary

NSE-1 ROM4 acquisition, external reproduction and local native loader/menu
execution are established, not future milestones. The local backend executes
the recovered resident program/DROM and firmware uploads without synthesized
DSP completion. `make check-c54x-observed-coverage` validates the exact 626-word
cold-idle opcode set and its Menu/power controls. The separate laboratory
`nse1pin` configuration replaces the default untimed boot echo with actual
externally clocked BSP transmit/receive pins. Its native self-test, physical
key-2 DTMF output and partial-word save replay pass
`make check-c54x-rom4-clocked-boot`; the observed instruction union is 643
words, with exact-word assertion coverage checked separately. The supplied
1 MHz bit clock, 8 kHz frame cadence and sync phase are laboratory inputs,
not recovered production handset wiring. See the authoritative
[physical-clock contract](cobba_control_boundary.md#nse-1-physical-clock-contract).
The paired `nse1aclk`/`nse1pclk` laboratory configurations hold standard
SWWSR and pin clocks constant while changing atomic/staged CPU execution.
Run `tools.run_c54x_rom4_pcm_clock --staged --coverage` with its required
MAME/ROM paths: atomic pin-clocked boot and key-tone acceptance passes.
Staged `7721` (`STM` to BDXR) at `0x0e61` now retains its literal until
Execute, using the existing partial peripheral-access model rather than
DAGEN's early-write path. The backend pin test executes the same store on
both frontends: no ready/echo without clocks, then the original `0x0aaa`
word and independent RX/TX sources with accumulators/status preserved.
The boot retry's overlap is specifically older `7721` at `0x0e61` and
younger `4920` at `0x0e5c` (BDXR write/BDRR read). This independent endpoint
pair now retains one peripheral-access wait per instruction; other
overlapping peripheral accesses remain guarded. This is partial stage
placement, not a recovered silicon arbitration policy. The backend test
also executes adjacent STM/LDM, checks the retained previous receive word,
and requires external clocks before the new echo or ready sources appear.

`make check-c54x-rom4-staged-codec` validates laboratory pin traffic, DTMF
waveform, delayed boot echo, exact paired screen and active partial-word
save/load on both frontends. Demand helper entries use the CPU's execution
observer in these laboratory configurations: program fetch alone is not
proof of execution, because staged prefetch can enter the next helper and
then be discarded. Each executed entry has its matching bitmap commit.
The memory-tap PC contexts are frontend-specific:
boot BDRR consumption `0e5d` atomic / `0e60` staged; demand commits
`a4c4,a4e0` atomic / `a4c3,a4df` staged. These contexts are not instruction
ownership claims. Coverage retains actual two/three-word `6f` tuples from
staged execution, not re-reads of potentially changed program memory; the
original first extension survives DAGEN and save/load. This gate establishes
laboratory codec integration, not fitted clock wiring or native handset speech.
Neither profile is an ordinary handset configuration;
atomic instruction fingerprints are not imposed on the staged observation.
Isolated native FR and EFR
encoder/decoder gates validate algorithm results, not handset speech. The
EFR reference, entry order and replay scope are maintained in
[COBBA control boundary](cobba_control_boundary.md#native-efr-encoder-acceptance).
The separate [DTX conversation probes](cobba_control_boundary.md#native-efr-dtx-conversation)
target nonzero background/SID/unusable-frame intervals and speech resumption.
They reproduce the complete 518-frame nonzero-background conversation after
correcting generic XC Access-stage condition visibility. Active-speech error
policy and handset audio transport remain outside that acceptance.
The separate [decoder homing gate](cobba_control_boundary.md#native-efr-decoder-homing)
also reproduces a 46-frame reset/repeated-speech conversation with full RAM
replay. It models the original caller's parameter-pointer convention, not an
extra reset between frames. Error/homing combinations remain unproved.
The [encoder homing gate](cobba_control_boundary.md#native-efr-encoder-homing)
separately matches all parameters, VAD/SP and native packed words for a
46-frame EHF/repeated-speech input with full RAM replay; reset decisions stay
inside the unchanged encoder routine. Neither gate attaches handset audio.

The remaining NSE-1 boundaries are RF sample ownership/encoding and SCU
tuning, and an organic DSP-to-PCM speech lifecycle. Matching internal images
or independent traces are still required for other DSP products; the recovered
5110 mask is not product-correct evidence for a 3210. Keep generated replies,
host-assisted DSP output, opaque register labels and measured peripheral
transactions separate. The authoritative local execution contract and current
RF frontier are in [ROM4 DSP loader](rom4_dsp_loader.md); the catalogue below
retains corroboration and costly-to-repeat negative knowledge.

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

The first reproduced run was not promotable as an unassisted DSP
implementation. The `SEEDDARAM` compatibility mechanism was eliminated by
correcting MVDP program writes under `PMST.OVLY`; the COBBA model still loops
serial samples, and that revision's `SELFTEST_MEAS` overwrote the DSP's computed
challenge response with validator-compatible fields. Run
`make check-dsp-rom4-cosim LOG=<captured-log>` to reproduce this classification;
the checker reports promotion as blocked whenever an assist is observed.

### Compatibility-assist audit

Paired 30-million-MCU-instruction runs disabled each assist independently. The
results classify the missing contracts without treating the native harness as
a hardware specification:

| Disabled assist | Observed result | Classification and replacement condition |
|---|---|---|
| `SEEDDARAM` | without the assist, the old core produced 70 rather than 74 DSP acknowledgements, runaway execution and a blank framebuffer | **Resolved.** Loader1 repeatedly executes MVDP from data source `0x0d00` to program destination `0x23c4`. The core bypassed its OVLY-aware `prog_write()` helper, so it wrote an inert program backing store instead of executable DARAM. Correct MVDP routing restores 74 acknowledgements and stable UI with `SEEDDARAM=0`. |
| `SELFTEST_MEAS` | the old donor EEPROM made the DSP reach the validator with a record that triggered an organic reason-4 warm reboot | **Resolved without the assist.** The generated 24C16 profile supplies records encoded for COBBA serial `00160010`; the real C54x transform decodes them and the MCU validator continues with `r6=1`. The native harness's direct overwrite remains classified as output synthesis and is not part of the MAME path. |
| COBBA codec-serial loopback | external reproduction proceeds through upload and self-test measurement with 74 acknowledgements, but presents `CONTACT SERVICE`; this is not the current local graphical-boot result | The DSP sets COBBA register 8 bits `0x0610`, enables the C54x BSP with `0xc008 -> 0xc0c8`, sends BDXR `0x0aaa`, polls BDRR, then clears the COBBA bits. TI's BSPC map proves the transition sets RRST/XRST while DLB remains clear, so COBBA externally returns the word. `nokia_cobba_device` owns the evidenced register predicate. The default local backend retains untimed completed-word echo; separate `nse1pin` acceptance completes the exchange through actual externally clocked BSP pins with zero untimed echoes. That laboratory timing does not establish the production clock attachment. |

The earlier failed promotion runs remain useful negative controls. With donor
EEPROM contents the backend still reaches the MCU validator and requests an
organic reason-4 reboot. Serial COBBA registers 5 and 6 supply the smaller
subcommand `0x13` measurement. Subcommand
`0x16` is a different contract. MCU routine `0x25864c` allocates a 30-byte
type-2 object, identifies it as `0x70/0x16`, and asks `0x28c496` to generate 24
bytes with selector `0x20`. When marker `[0x10fdde] == 0x5a`, MCU routine
`0x2578fc` transforms those bytes before the object is sent. The observed
pre-transform bytes are
`e6 a6 02 24 ea f2 6c 9e 42 0f 03 32 c1 26 b1 d8 7c 5e 56 cf b0 e8 b3 b2`;
the transmitted challenge is
`c9 f4 cd 44 19 2c 0f fe 37 96 d0 f5 ee 74 7e b8 8f 80 35 af c5 71 60 75`.

The DSP builder at `0x4b73` receives that exact object at `AR2=0x0825`, calls
the resident transform at `0x7f2d` twice, and returns a structured response
which MCU routine `0x258b60` range- and packed-decimal-checks. Slot `0x250b` is
an organically uploaded no-op; there is no evidence that it represents a
missing ADC acquisition step. One confirmed interpreter error was the old
RPTB completion test: `RPTB` at `0x7ff5` encloses a two-word `CALL` ending at
`0x7ff8`, but comparing its branch target numerically with REA skipped five of
the six calls. Waiting for the sequential return to `REA+1` materially changes
the response. The later public backend also corrected status-opcode decoding,
repeat-mode immediate-address updates and shifted cross-accumulator logical
operations. These are C54x execution semantics, not COBBA measurements.

The local coherent boot uses a freshly generated external-EEPROM profile. Its
two transform inputs are `d6fb 4394 e437 da16 9668 964f` and `5cd4 32fe 5be2
dba6 9643 82d7`; the C54x returns the decoded wildcard record and the MCU
validator continues with `r6=1`. Persisted donor EEPROM state instead supplies
`c9f4 cd44 ... 6075`, producing `3532 0000 312b ... 88b2 0000`; the MCU rejects
that record and requests a reason-4 warm reboot. The earlier coherent gate did
not isolate NVRAM and therefore mistook DSP transport completion for semantic
acceptance. The corrected gate regenerates the profile, starts with an empty
NVRAM root, and checks the validator continuation as well as the DSP-side
completion.

### Unassisted external closure

A clean checkout of public commit `2b4c10f` was independently run with the
same recovered ROM4 program and DROM inputs and both compatibility assists
disabled:

```text
DSP54_SEEDDARAM=0
DSP54_SELFTEST_MEAS=0
```

At 5 million MCU instructions the run completed all 74 DSP acknowledgements
without the earlier reason-4 reset. At 30 million instructions it remained
stable and rendered an interactive `Menu` frame. This closes the external
ROM4 loader and challenge-response question: slot `0x250b` is an intentional
uploaded no-op, and no synthetic measurement tuple is required. The accepted
DSP response begins at word `0x1200` with:

```text
3532 0000 ffff ffff ff0f 0000 0078 54c2 5cd4 32fe 5be2 dba6 9643 82d7
```

This is an external behavioral oracle, not code available for import. The
backend is GPL-licensed, while this driver is intended for MAME under a
compatible clean-room implementation. The local MAME backend reproduces a
different response to the different MCU-provided challenge described above.
Both observations are retained without asserting that one response validates
the other's input.

The instruction-level boundary can be checked independently of the native
backend's implementation:

```sh
make check-c54x-rom4-transform \
  TRACE=/path/to/instruction.log \
  DATA_MEMORY=/path/to/data-memory.bin
```

The checker requires the `0x7ff5` RPTB setup once, six complete executions of
the enclosed two-word CALL at `0x7ff7`, the descending destination walk
`0x1208..0x1203`, and the accepted response above. This is the first executable
conformance gate for a clean-room MAME C54x core; it does not accept a final
response alone while the repeat-block control flow is wrong. The in-MAME gate
captures the response when it is published because the operational DSP later
reuses the same buffer. The native `.api`
capture begins at DSP data word `0x0800`; use `--data-base` for another capture
layout rather than treating a DSP word address as a file offset.

A second gate validates the deterministic transform-entry snapshot produced by
halting immediately before DSP PC `0x4b73`:

```sh
make check-c54x-rom4-snapshot PREFIX=/path/to/rom4-transform-entry
```

The private four-file snapshot is not distributed. Its SHA-256 digests are
recorded in the checker, which also requires PC `0x4b73`, SP `0x1ec3`, PMST
`0xffac`, AR2 `0x0825`, and the unprocessed identity object at data word
`0x1200`. Together the entry and completion gates provide deterministic input
and output contracts for the clean-room executor without importing code from
the external backend.

`make check-c54x-rom4-execute` loads that private snapshot into the local
clean-room CPU and currently executes from `0x4b73` through the transform,
memory-mapped-register save/restore, DSP control setup, and serial-port polling
to the ROM's `IDLE 3` at `0x7ec9`. The saved frontier is PC `0x7eca` with the
CPU explicitly idle, not an unsupported-opcode stop. Advancing beyond it now
requires an evidenced DSP wake source and COBBA port behavior. Unmapped I/O in
the isolated fixture happens to read as zero; that is harness behavior and is
not accepted as a recovered codec response.

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

An 80-million-instruction native-harness follow-up used physical-key replay to
enter the handset security code. It reached stable NSE-1 standby with the
operator `Radiolinja` and `Menu` visible, while retaining the same 74 DSP
acknowledgements. That is a useful independent reference, not this repository's
acceptance result.

The local MAME backend now independently eliminates both historical assists.
With a generated EEPROM profile the real C54x transform produces the accepted
COBBA security record, the operational ROM consumes the MCU ring and returns
to its ordinary idle, and a physical Menu-key transition renders the Phone
book menu. `make check-c54x-rom4-coherent` guards loader, measurement,
validator, and ring behavior; `make verify-5110-menu` guards the unassisted
keypad-to-UI path and its deterministic rendered frame, and
`make verify-5110-save-state` repeats that path after restoring the real-DSP
composition.

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

The local real-DSP composition still has no demonstrated network or call
scenario. Its earlier 30-second boundary scheduled 6,497 CTSI frame edges but
left INT0 masked and performed no RF reads. The one-bit cold-entry contract
below now activates that receiver organically; synthesizer programming and a
sample-level GSM input remain unproved.

A post-loader differential also found and removed one local CPU artifact:
`RPTB` was tracked only in private core state, leaving architectural
`ST1.BRAF` set after the block terminated. The ROM4 scheduler consequently
reached its second task scan with `ST1=0x6101` rather than the reference's
`0x2101`. The core now asserts BRAF on `RPTB`, clears it on the terminal
retirement and tests that contract directly. Menu, save-state and coherent DSP
gates remained unchanged, and the corrected run still left INT0 masked until
the independent cold-entry contract below was recovered. BRAF was therefore a
real CPU defect but not the receiver-enable boundary.

The remaining interrupt boundary is now reduced to one bit. Runtime records
only two operational writes to `IMR`: mask-ROM code ORs `0x0204` into the
incoming value, then the uploaded operational block ORs `0x015a`. Starting
from the emulator's zero-initialised member therefore produces `0x035e`; the
firmware does not overwrite a pre-existing bit 0. The recovered vector table
independently assigns INT0 to the receiver handler at `0x3204`, while INT3
branches to a one-instruction `RETF` handler at `0x241a`. Routing CTSI frames
to INT3 services and discards them without one RF read, so INT3 is disproven
as the receiver clock route.

That comparison exposed a generic CPU omission rather than a Nokia contract:
`0xf49b` is TI's fast interrupt return (`RETF`). The clean-room core now saves
the internal `RTN` latch on interrupt entry and implements `RETF` as RTN-to-PC,
stack discard and interrupt re-enable, with an isolated conformance check.
The ordinary ROM4 gates remain green after the correction.

A separate diagnostic seeded only `IMR.bit0`, leaving every other reset and
firmware value unchanged. The two organic OR operations then produced
`0x035f`; INT0 entered `0x3204` and performed 33,888 RF sample-port reads in a
five-second run, versus zero at `0x035e`. No port-`0x32` write appeared. Because
hardware reset preserves IMR, the ROM deliberately uses OR operations, the
recovered vector assigns the receiver to INT0 and the alternative INT3 route
is disproven, the backend now retains exactly this one-bit ROM4 cold-entry
contract. It does not import any unrelated bit from the external `0x52fd`
snapshot. A real reset capture can still refine where MAD2 establishes the
retained bit, but is no longer required to keep the receiver artificially
disabled.

One diagnostic-only causality run substituted `0x52fd` when ROM4 wrote its
local `0x0204` mask. With BRAF corrected, that run organically scheduled task
2, executed the `0x3000` initialization family, changed IMR to `0x53ff` and
entered the RF loop at `0x323b`. It then exposed the previously unreachable
one-word `EDxx` load-immediate-to-ASM instruction, now implemented and covered
by the core test. The substitution is not present in the driver: it proves that
the mask is sufficient to activate the receiver, but remains inadmissible as a
power-on value because its source snapshot is not ROM4/MAD2 reset evidence.

Using that same disposable reachability condition as an ISA coverage probe
exposed additional generic instruction contracts in order: delayed block
repeat (`RPTBD`), dual-memory multiply and MAC, parallel store/MAC, square
distance, delayed ANEQ branch, and accumulator negate. Each is now implemented
from the TI instruction definition with an isolated conformance fixture. After
the final correction, a four-second diagnostic run completed the normal
MCU/DSP handshake without an illegal opcode and performed 26,944 RF sample-port
reads before returning to `0x31a5`; it emitted no port-`0x32` write. The mask
substitution was removed before the normal gates. This proves the newly
reachable receive/FIR instruction surface, not RF acquisition or a power-on
mask value.

A word-aligned census of the recovered and transform-entry program images
finds no `0x52fd` or `0x53ff` literal. The reference source identifies its
`0x52fd` as an Osmocom/Calypso post-bootloader snapshot, so it cannot be
promoted to MAD2 state without an independent Nokia capture or mask-ROM
derivation.

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

### Recovered IDA annotations

The archive's `dsp_full.idb` uses legacy `IDA0` / B-tree 1.5 format.
`python-idb` 0.8.0 rejects its signature; this is a reader limitation, not
evidence of archive corruption. The independent
[pyidbutil reader](https://github.com/nlitsme/pyidbutil/tree/e77d0e79e5c11507a207d503883dcccd15f9c696)
opens it and enumerates 132 names:

```sh
python PATH_TO_PINNED_PYIDBUTIL/idbtool.py --names PATH_TO_EXTRACTED_ARCHIVE/dsp_full.idb
```

Relevant original annotations and their current validation scope:

| DSP address | Original label | Evidence use |
| --- | --- | --- |
| Data `0x29` | `BSCR` | Annotation, not proof of the custom MAD2 peripheral decode or of an HPI interrupt register |
| Data `0x32` | `SPC1` | Candidate second serial-control register; not proof of MAD2 board attachment |
| Data `0x1390` | `FR_prm` | Candidate full-rate parameter buffer |
| Program `0x80e7` / `0x80ff` | `FR_prm2bits` / `FR_bits2prm` | Native FR parameter round trip validated; see packing contract below |
| Program `0x812f` | `EFR_FR_crc3_AR4` | Candidate CRC helper used by the type-`0x50` buffer consumer |
| Program `0x8458` / `0x860c` | `FR_decoder` / `FR_coder` | Decoder and encoder validated in isolated algorithm fixtures |
| Program `0xc8dc` | `EFR_decoder` | Isolated decoder matches 163 ETSI reference frames, with full-RAM frame replay; see `cobba_control_boundary.md` |

There are no named addresses in `0x8d00..0x93ff`. Querying both ordinary
comment slots (`S,0` and `S,1`) at every address in that range and at
`0x34`, `0x35`, `0x36`, `0x3b92`, `0x7e9c` returns no comments.
Thus the annotated database does not resolve the NSE-6 port-36 completion
contract. Codec labels are useful leads, not grounds to rename unknown
peripherals or claim native speech. Extract the archive and pinned reader
under ignored, disk-backed `tmp/`, not `/tmp` tmpfs. No reader implementation
is imported into the driver.

The request/HPINT range `3598..37be` likewise has no named request-source
register and no ordinary comments in slots `S,0` or `S,1` (1,102 address/slot
queries). The adjacent `37ce` label resolves as `DSP_to_MCU_send_message`,
providing a positive reader control. This bounds only these annotation
classes, not every possible IDA metadata record. The archive's `Task info.txt`
describes MCU task structures at `100xxx`, not a DSP-port request map.

TI SPRU131G sections 8.6 and 10.3 locate generic HPIC at data `002c` and
BSCR at `0029`; BSCR bit 3 is reserved in that documented layout. NSE-1's
parameter consumer writes data `0029` with masks `0008`/`fff7`. Do not turn
that strobe or the historical BSCR label into a proven HINT mapping: the
MAD2 custom peripheral layout still needs independent evidence. Generic TI
instruction/BSP contracts do not establish every Nokia external-port map.

#### Native carry/XOR helper acceptance

`make check-c54x-rom4-crc-helper` executes acquired program ROM at
`0x8179..0x8180`, the inner loop called by `0x812f`. It checks 512 cases:
eight accumulator patterns, both initial carry values, and 1..32 iterations.
The independent reference rotates the low 32 accumulator bits right through
carry and XORs `6` when the outgoing bit is set. Native execution agrees on
the accumulator, carry, scratch accumulator, return PC and stack pointer.
Only a return-to-IDLE sentinel is fixture code; no handset state or peer
response is injected. The gate also requires the acquired ROM checksums.

The inner-loop gate alone does not validate GSM frame CRC correctness.
The outer routine has a separate gate, described below; native speech remains
unvalidated by either gate.

#### Packed CRC contract

`make check-c54x-rom4-crc-packed` executes acquired ROM at `0x812f` and
compares its result with independent polynomial division using
[GSM 05.03 v5.0.0, section 3.1.1](https://www.etsi.org/deliver/etsi_gts/05/0503/05.00.00_60/gsmts_0503v050000p.pdf).
The reference protects 50 bits with polynomial `D^3 + D + 1` and produces
the complemented three-bit parity. It does not reproduce DSP instructions.

With `AR4` pointing at twelve 16-bit words `w`, define
`c = (w[10] >> 9) | ((w[11] & 0x1ff) << 7)`.
The recovered input stream is:

- for `j = 0..15`, append `w[0].bit(j)`, then `c.bit(15-j)`;
- for `j = 0..8`, append `w[1].bit(j)`, then `w[10].bit(8-j)`.

The normal polynomial parity's bit order is reversed and placed in
accumulator A bits 13..11. All 192 input-bit basis vectors, zero/all-one
vectors and 64 deterministic mixed vectors agree, each with both initial
carry values: 516 native cases. This also checks the return PC and balanced
stack. Words 2..9, word 1 bits 9..15 and word 11 bits 9..15 do not contribute.
The isolated mask-ROM call at `0x23c6` returns directly; the fixture does not
substitute the handset's uploaded resident helper or prove that upload's ABI.

The first execution exposed missing `ROLTC A/B` support in the CPU. Its
[TI SPRU172C, page 4-144](https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/81/6136.spru172c.pdf)
contract is now implemented and checked with 256 core vectors: insert TC,
export source bit 31 to carry, clear guard bits, preserve TC and other status.

This establishes the packed routine's CRC arithmetic. The parameter packing
gate below connects its protected input to GSM importance ordering; the
speech codec and handset delivery remain separate boundaries. No native
speech or end-to-end call claim follows.

#### Native FR parameter packing

`make check-c54x-rom4-fr-packing` runs acquired routines `0x80e7` (pack),
`0x812f` (CRC) and `0x80ff` (unpack), in that order. The parameter buffer
contains the 76 standard full-rate parameters in serial order; `AR3` points
to it and `AR4` points to a 17-word packed buffer. The shared DROM table at
`0xb205` has 272 entries: its high byte selects the parameter word and its
low nibble selects source bit `15-n`. Output consumes each group of sixteen
entries MSB first. The table carries 260 speech bits, three parameter-word-0
bits 13..15 reserved for parity, and nine zero-padding references to
parameter-word-1 bit 15. This is a DSP storage format, not a libgsm 33-byte
frame or a complete over-air channel-coded block.

The gate passes all 260 serial speech-bit basis vectors, an all-zero frame,
and 32 deterministic mixed frames (293 vectors). It verifies every packed
word against the acquired table, compares the CRC's 50-bit input stream
against the independent `gsm::tch_f::importance_order` reference, checks
native CRC against polynomial division, and recovers all 76 parameters
through native unpacking. Each call must return with a balanced stack.
The fixture's 1 ms observation interval is a bounded execution allowance,
not a peripheral-latency or cycle-accuracy claim.

`make check-c54x-rom4-fr-decoder` validates acquired decoder `0x8458`,
initialized by native `0x8449`. Its 76 serial-order parameters occupy
`0x1390..0x13db`; AR2 supplies the caller's 160-word PCM destination.
Four eight-frame sequences (zero, impulse, alternating and deterministic
noise) retain native history between frames. All 5,120 output samples match
the independent GSM FR codec exactly. The 5 ms observation interval is an
execution allowance, not measured hardware timing.

The decoder requires `%` address updates to remain linear when BK is zero:
its RPE inverse quantizer uses `*AR2+0%` with AR0=3 and BK=0. Suppressing
that update overwrites pulses. The retained behavior is protected by 320
core cases covering signed strides, wraparound and single/dual-memory forms,
but its authority is the acquired codec's bit-exact execution, not a silicon
measurement. [TI SPRU131G section 5.5.3](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
instead describes suppressing modification with BK=0.
[TI SPRU429A section 3.3.6](https://www.ti.com/lit/ug/spru429a/spru429a.pdf)
recommends removing unnecessary percent modifiers during C54x-to-C55x
migration; it does not explicitly resolve the nonzero AR0-stride discrepancy.
Keep the ROM4-compatible linear behavior pending independent execution or
hardware evidence, and do not promote it to a universally validated C54x
rule. Nonzero circular buffers follow the separately tested CPU contract.

`make check-c54x-rom4-fr-encoder` (atomic) and
`make check-c54x-rom4-fr-encoder-staged` invoke native reset wrapper `0x8045`,
preprocessing `0x85e0` with AR4 pointing to 160 PCM words, then `0x860c`.
Preprocessing writes `0x127d..0x131c`; encoding writes the 76 parameters
at `0x1390`. All 2,432 parameters across the same four eight-frame sequences
match the independent codec exactly. Filter histories are also compared:
`0x1000` is z1, `0x1001/2` hold the low/high split of L_z2, and `0x1003`
is preemphasis history mp. The reset includes the wrapper's additional state
initializers, not just preprocessing reset `0x85d7`.

Both fixtures also checkpoint every prepared frame using MAME save state.
Decoder checkpoints precede decoder execution; encoder checkpoints precede
preprocessing and cover both preprocessing and encoding on replay. The
independent reference advances once only: its expected samples/parameters
and the first native result remain outside the machine snapshot. After
restoring the original CPU/RAM state, replay must reproduce all 65,440
ordinary data-RAM words (`0060..ffff`), including histories and buffers,
as well as the existing bit-exact reference comparison and balanced stack.
Before re-execution, the fixture checks the live entry PC, non-IDLE CPU,
input stack and every pre-frame RAM word; an unchanged completed result
cannot stand in for restoration.
The check excludes data MMRs from the RAM comparison; it is not a peripheral
timing or in-flight interrupt test. Each direction requires 32 replayed
frames, separately from the encoder's native mute controls. These are
isolated algorithm save/load tests, not a handset speech stream or codec
clock attachment.
All 32 replays per direction pass: 5,120 decoded samples and 2,432 encoded
parameters remain bit-exact. The 293-vector packing and eight native mute
controls also pass with the replay fixtures enabled.

`MAX` must select B and set carry on equality (SPRU172C pages 4-99/100).
The encoder's predictor and RPE grid searches depend on retaining the earlier
candidate when correlation/energy ties. Selecting A on equality changes
parameters even for silence. The strict comparison is protected by 256
signed 40-bit MIN/MAX cases, including equal operands and both destinations.

The remaining boundary is activation and delivery through the handset's
native speech wrappers and audio transports. Isolated bit-exact codec tests
do not establish native handset audio, speech during a call or end-to-end calls.
The isolated algorithm runner retains `evidence.json` before execution:
command/BIOS, SHA-256 and size of the actual MAME executable, runner,
fixture source, GSM-FR adapter/packing sources and prepared ROM4 binary inputs.
Packing, 32-frame decoding and 32-frame encoding remain bit-exact after
the SQDST/FIRS arithmetic corrections; the separate native mute cases pass.
This manifest identifies the selected files, not every build dependency or
the dynamically loaded system codec library. Logs and manifests use
disk-backed repository `tmp/`, not system tmpfs.

### Native speech activation and PCM buffers

The codec contract checker protects seven additional resident-ROM sequences:
encoder/decoder mode gates, encoder PCM input, decoder PCM destination,
mode publication, and the ingress/egress ring-copy pointers. These are bounded
byte-checked paths, not an exhaustive closure of indirect writers/callers.

- Encoder wrapper `0x9c4f` and decoder wrapper `0x9cb9` require data
  `0x1836 == 0x00c1`. The encoder also branches on `0x6bb & 3`.
- The encoder's normal path passes `0x1536` to conditional input-mute
  helper `0xa0b8`, then preprocesses 160 PCM words and invokes the validated
  encoder. After its `0x24c1` hook, the helper tests bit 0 of data `0x1f2b`:
  clear returns without changing the PCM buffer; set repeats the zero store
  160 times through AR2. The acquired resident hook is a return, but future
  uploaded replacement behavior is outside this bounded contract. Both
  sequences are byte-pinned and word-mutation tested. The isolated
  `check-c54x-rom4-fr-encoder` gate also executes the acquired helper with
  flags `0000/0001/fffe/ffff` at two buffer bases: all 1,280 sample results,
  16 adjacent guards, return-stack balance and AR2 advancement pass.
  Its runner requires this result separately from the bit-exact encoder
  comparison and rejects absent or duplicate completion markers. This is
  isolated algorithm validation, not an organic handset fixture or proof
  that speech mode activates. Boot-time flag provenance is mapped below;
  its organic call-time value remains unclassified. Silence alone must not
  be attributed to a broken encoder or used to force the flag clear.
  A raw operand scan of the checked program image finds 35 `1f2b` words:
  31 bit-test contexts and four write contexts. The whole-word copies at
  `0xa53d` and `0x27ac` load from shared `0x085e` via AR4. The paired OR
  `0x1ca0` at `0xb885` and AND `0xe35f` at `0xb891` preserve bit 0.
  All four write contexts are byte-pinned. This bounds literal
  references in this image, not computed-address writers or future uploads.
  The next mute-specific predecessor is the MCU/DSP owner of shared
  `0x085e`, not a local encoder arithmetic correction.
  The organic 11-second physical-key gate now correlates MCU write
  `PC=272034`, bus word `100bc`, upper-halfword mask `ffff0000`, value
  `df290000`, with DSP shared read `085e=df29` and flag write
  `1f2b=df29`, both at reported PC `27ae`. The byte-pinned MVDK
  `7194,1f2b` begins at `27ac`; both bus accesses follow consumption of
  its destination extension. The observed bus PC is not the instruction
  entry. Thus the duplicate copy does
  execute; native mode remains separate from static label ownership.
  The same ordered read/copy occurs after the physical keypress. Bit 0
  stays set in this boot/tone lifecycle, while speech mode is inactive;
  this is not evidence of erroneous call-time muting. The gate rejects
  wrong MCU lanes, missing copy order and absent post-key observations.
  MCU field writer `271d70` receives selector/value in R0/R1, retains
  them in R4/R6 and dispatches through the 47-entry table at `271da4`.
  Selector `28` uses table word `271e44 -> 271e9a`, loads destination
  `100bc` from pool `272214`, and forwards R6 through the common halfword
  store at `272034`. The fresh capture reports selector `0028`, input
  and output `df29`, destination `000100bc` and merge mask zero. Thus
  this publication forwards the supplied word rather than constructing
  its mute bit. Twenty-four original-MCU byte spans protect entry, dispatch,
  selected table entry, branches, stores, pools and storage addressing;
  every byte has a negative
  mutation test. Other selectors are not semantically decoded by that
  fixture. Entry-time capture identifies original caller `275445`
  (Thumb return from `275440`), parameter zero, index 9 and source
  base `10b2b8`. The caller loads `[R5+2]`, hence value `df29` from
  `10b2ba`, compares it with the retained prior record and selects field
  `28` from byte table `2a5df8`. Live LR `271d83` at the final store is
  an internal lock-call return, not the original caller. The gate requires
  the entry observation before the shared publication and rejects that
  misattribution. A passive cold-boot RAM watch identifies the preceding
  record write: PC context `274cba` accompanies `df29` in the lower bus
  halfword (`mask=0000ffff`) at `10b2b8`, i.e. field `10b2ba`, at
  0.158787846 s before shared publication. Instruction `274cb8` loads
  `[R4+16]` and `274cba` stores it into `[R5+12]`. Instruction decoding,
  rather than a tap PC alone, establishes the writer; attributing this
  publication to the preceding `LDRH` is rejected by the checker.
  The same record refresh occurs after the physical keypress. Adjacent
  upper-halfword writes and later low-byte updates are distinct fields
  or partial updates, not full-word replacements. The next predecessor
  is the upstream structure field `[R4+16]`, rather than a missing
  DSP-originated response. Pool `27500c` sets R4 to `10a6b0`, identifying
  that field as `10a6c6`. Its observed initialization is a four-byte serial
  read through `28c496`, selector `0210`, destination `10a6c4`, caller
  `2894fd`. The byte reader `28e10e` accesses GPIO `20020`; stores at
  `28c4c0`/`28c4d0` assemble bytes `00 e0 df 29` at 0.157115692 through
  0.157406923 s. Thus the low halfword reaches the record refresh before
  publication. Routine `28c308` sends the high address bits through the
  EEPROM slave address and the low byte through the word address, so
  `0210` is a physical storage offset. Both the seed EEPROM and the fresh
  isolated NVRAM contain `00 e0 df 29` there; `make_5110_eeprom_profile.py`
  preserves this region rather than deriving it. This is provenance from
  the current seed, **not independent factory-default evidence** and not
  a donor-free speech provisioning contract. The configuration field's
  semantics remain unresolved; a matching ROM literal alone is not
  provenance. This observation does not establish
  call-time speech activation or the semantics of every flag bit.
  Descriptor-reader entry `2894c8` receives destination `10a6c4`, logical
  identifier `0301`, R2/R3 zero and caller `283689`, before the physical
  read. Caller `28367c` loads destination and identifier from pools
  `28385c`/`283878`. Adjacent conditional ROM copies at
  `28369c..2836de` target other structures at `10a6f8`/`10a72c`, not this
  four-byte field. These tables do not establish a fallback for record
  `0301` or justify replacing the seed's mute bit. Lookup `289138` uses
  group 3 in catalogue `2a6438` (pool `28950c`); group entry `2a6450`
  selects table `2a6108` with count three. Its direct-index record
  `2a6110` is `00000301 0210 0004`: identifier, physical offset and byte
  length. It supplies storage metadata, not the default payload. The
  structured decoder checks this observed path with malformed pointer,
  count, identifier and truncated-image negatives; it is not a complete
  descriptor catalogue or an absence proof for every fallback writer.
  MCU lifecycle branches `27503c..275042` and `275054..27505a` respectively
  set and clear bit 3 in record byte `[R5+13]` (the low byte of field
  `[R5+12]`). Both preserve bit 0; they cannot by themselves release the
  encoder's decoded bit-0 mute. Their full upstream branch meaning remains
  unresolved. The gate requires the selector-`0210` read before record
  initialization and rejects a changed selector or inverted event order.
- The decoder supplies destination `0x1496` before postprocessing at `0x94c9`
  and `0xab7c`; further gain/filter/mute stages precede output copying.
- `0xaa39` copies from the ring cursor in `0x6de` into the selected input
  buffer. `0xaa4f` copies 160 output words into the cursor in `0x6df`.
  Both use BK=`0x278` and save their advanced cursor. These are RAM buffers,
  not direct DSP I/O `0x21` samples.
- The checked publication at `0x579e` selects handler `0xa436` and writes
  `0x1836=0xc1` when `0x1869=0xc3` on that branch. The producer of that
  operating-mode state still needs tracing through normal MCU/DSP traffic.

Passive write taps in `c54x_rom4_codec_observe.lua` cover the mode cells and
ring cursors. A fresh four-second native NSE-1 boot initializes `0x6de=0x400`,
`0x6df=0x560` and `0x6bb=0x24`, while `0x1836`, `0x1869` and selected
input pointers remain zero. This is evidence that boot alone does not activate
the speech wrappers, not proof that the runtime activation path is absent.

The four-second fresh-profile RF observation contains thirteen ordered pairs:
two descriptor bursts of `2a04/0006`, `0041/0040`, `27a2/0030`, `0041/0020`,
`0488/0010`, `0000/0050`, followed by `2813/0030`.
The queue-owned `0041/0040` pair comes from request words
copied by resident `0xa1ba` from `0x216d` into queue `0x1920`; runtime taps
observe `0x1922=0x41` and `0x1923=0x40` at reported PC `0xa1bc`.
The slot handler reads them through cursor `0x197d` at `0x370c/0x3710`, then
advances the cursor. Both producer and consumer sequences are byte-checked.

Native TX-completion FIQ1 releases this firmware request and advances the TX
pointers to `0041/0041`; omitting it leaves them at `0038/0038`. MAX equality,
BK=0 handling and passive taps do not own this request. The short-run checker
requires the complete descriptor bursts and their queue producer, rejecting
the obsolete three- and five-pair partial fingerprints. This does not
assign physical tuning semantics to `0041/0040`.

The 30-second acceptance profile captures a scan in progress: a fresh run
receives another queue request at approximately 6.43 seconds, repeats `2a04/0006` twice,
then writes `2727/0030`, `2728/0030` and subsequent values. Its terminal
summary is checked against the exact longer-run counts in
`c54x_rom4_rf_boundary_check.py` with EF_SMSS correctly placed in DF_TELECOM.
The short-run check rejects this run by design. The explicit `--scan-30s`
profile requires those counts, all thirty-two logged initial writes, the uploaded shorter-reload
write and the later search-list selector. A fresh isolated rerun reproduces
it. Both profiles retain the producer, loop-completion and
post-scan consumption checks below; neither profile claims acquisition
or whole-boot completion.

Passive timer-port taps identify the changed cadence: uploaded code writes
I/O `0x0e=0x0546` at reported PC `0x0959`, time 6.454126 seconds. The current
controller interprets this as a 1,351-quarter-symbol period instead of 5,000.
The same write occurs at 1.583580 seconds but is then restored to `0x1387`
by resident PC `0x30fc`; the later cycle retains the shorter period.
Mode-AC writes at resident `0x442a` repeatedly select `0x0c`, and `0x32b3`
clears bit 3 to produce `0x04`. This explains the observed counter increase
within the current model, not the physical validity of the shortened period.
Decode the uploaded helper around `0x0948..0x0959` and its caller before
changing timer semantics or accepting this as normal RF operation.

A passive program snapshot confirms that the shorter reload is literal
uploaded code, not a corrupted computed operand: `0x0942` selects AR2 `0x0060`,
`0x0944` stores `0x1387`, and `0x0946` outputs it to port `0x0e`; `0x0952`
again selects AR2 `0x0060`, `0x0954` stores `0x0546`, and `0x0957` outputs
it to the same port. Calls to resident `0x407c` separate the stages, with
`0xa15f` between them. The helper then changes queue flags at `0x195c`,
updates I/O ports `0x02` and `0x04`, and branches on data `0x0284` at
`0x0975..0x0978`. Its complete exit/restore paths and the meaning of that
selector were initially unclassified. The literal write alone does not validate
the controller's reload semantics or the helper's intended duration.

The passive selector trace distinguishes the invocations: the first request
writes `0x0284=0x7c` at reported PC `0x3d8f`, then overwrites it with `1` at
`0x3dae`; the later request writes `0x7c` without that overwrite. Resident
type-`0x1a` processing sets this default at `0x3d8d` and, when its TC branch
is clear, calls `0x48f7` and replaces it at `0x3dac`. Uploaded `0x0975`
compares `0x0284` with `1`: equality branches to `0x0a1a`, whereas inequality
enters the repeated measurement body at `0x098a..0x09cc`. In the later run,
`0x0934` stores `0x7b` to `0x0285` and `0x097a` initializes `0x0e13=1`.
The repeated mode `0x0c/0x04` activity therefore corresponds to a different
search-list lifecycle, not merely a duplicate of the first single-entry
invocation. This does not yet prove the loop termination or RF acquisition.

One-second passive snapshots through 30 seconds show the helper's channel
cursor AR7 advancing through the `0x0e14`-based list and restarting, while
data `0x0e13` alternates between zero and one. Samples at idle PC `0x408d`
also expose outer-counter AR6 values `7`, `4`, and `3`; AR6 samples inside
`0x09ac` are not that counter because the nested routine has repurposed it.
The trace establishes ongoing loop progress, not a frozen wait, but a
terminal return was absent from that 30-second window. A 60-second passive
boundary run resolves it: AR6 decreases from `6` to `0` at `0x0981` in
3.712548-second steps, exits to `0x09ce` at 36.154511 seconds, and reaches
`0x0a18` at 36.156061 seconds. Resident `0x30fc` restores port `0x0e=0x1387`
at 36.156065 seconds. Thus this helper completes its eight-pass list scan
and restores the original frame period without a timer or firmware shim.
The ensuing sampled execution remains around `0x479e..0x47a1` through 60 seconds,
with mode-AC `1`, 1,991 RF writes, 30,380 frame expiries and 971,232 RF reads.
This sampling is not evidence of a stalled queue; scan completion is not RF
acquisition, native speech activation or whole-boot completion.
Resident `0x479c..0x47a2` compares data `0x06b7` with the pointer at `0x06b8`
and loops while equal; after inequality it loads AR4 from `0x06b8` and
copies ring entries with BK `0x48`. This is a queue-empty consumer boundary,
not an unfinished channel-list loop.
The annotated resident image identifies two producer tails: `0x3334..0x3345`
and `0x3359..0x335e` advance `0x06b7` after storing filtered RF results.
Both sit downstream of the `0x32f4` receive/FIR branch, whose input reads
port `0x27`. The `0x3357` producer and `0x4798` empty-wait sequences are
checked against acquired ROM words. Passive post-scan execution taps show
mode-AA reaching zero, mode-AC remaining one, and the ISR taking `0x3230`
to `0x32f4` and publishing at `0x3345`. Across consecutive frames, both
queue cursors advance together from `0x1a00` through `0x1a3c` in four-word
steps at the restored 4.615-ms cadence. The consumer therefore drains
organically produced samples; the observed polling PC is normal inter-frame
waiting, not a missing producer. Zero-valued modeled RF input does not
establish acquisition or justify synthetic queue completions.

The pending speech-mode field has an indirect host-owned writer: resident
`0x3a09` decodes type `0x02` payload words, and `0x3a21..0x3a2b` advances
AR5 from `0x1868` to store the high byte of the fifth consumed word at
`0x1869`. The annotated data dispatch table maps index `0xb00f+2` to
`0x3a09`; the acquired cold-data image independently contains big-endian
word `0x3a09` at byte offset `0x16022` (data address `0xb011`).
`0x570f..0x5714` copies eight pending words `0x1863..0x186a` to active
`0x1830..0x1837`, including this field at `0x1836`. Both instruction
sequences are byte-checked against the acquired program image. This is
a legitimate configuration path, not proof that sending an isolated
type-`0x02` packet can establish an ordinary traffic-channel lifecycle.
Pending modes are not copied indiscriminately: `0x55a7..0x55c0` selects
transition tails using the high nibble and low two bits of `0x1869`, with
the active `0x1836` family selecting additional setup. Multiple tails
converge on `0x56ef`, which performs the checked pending-to-active copy.
Resolve the MCU producer, uploaded dispatch interception and copy trigger
before any handset experiment; do not set `0x1869` or `0x1836` directly.
A fresh eight-second verbose native NSE-1 run observes no type-`0x02`
TX publication or consumption. Its type-`0x1a` payloads begin `001098`
at 1.552444 seconds and `001198` at 6.425143 seconds, both committed by
MCU PC `0x0027183e`. This bounded boot trace does not close the type-`0x02`
producer census; it establishes that ordinary initial scanning does not
activate the mapped speech mode. Traffic assignment and its channel
configuration must precede a handset speech claim.

A bounded Thumb-pattern scan of NSE-1 v5.30 finds two independently decoded
type-`0x02` construction sites, at `0x20d162` and `0x20e7be`. Both store
wire length `0x14` at object byte 2, type `2` at byte 3 and initial operation
`4` at byte 4. The first also stores object halfword `2`; the second has
the same halfword store. This matches the independently recovered NSE-3
constructor shape without borrowing its addresses or payload meanings.
The scan examines 524,272 halfword offsets and returns six length/type-store
candidates; four use dynamic type values and remain unclassified. This is
not an exhaustive type-`0x02` producer census. Recover containing functions,
all payload stores and submission callers before assigning native mode
semantics or building a traffic fixture.

The first containing function is `0x20d14c..0x20d1d8`: it allocates and
zeroes a `0x18`-byte object, writes its pointer through argument R0, and
stores argument R1 at object byte `0x0b`. If R1's high nibble is `0x10`
or `0x50`, it sets byte `0x0c` to `0x60` or `0x50`, respectively; otherwise
that byte retains the initialized zero. A halfword-wide direct-BL encoding
scan finds four candidate callers (`0x20d312`, `0x20dbb0`, `0x210bba`,
`0x210e5e`), not NSE-3's five-call topology. The first two visibly supply
R1 `0x10` and `0x1a`; the third supplies a table-derived byte. Indirect
calls remain outside this closure. The final caller at `0x210e5e` supplies
R1 `0x50`, then submits to task 3 at `0x210e96`. The table-derived caller
loads from `0x2aa404`, indexed by input byte `[R4+0x15]` plus stack byte
`[SP+0x1e]`; the first nine bytes are `10..18`. The complete reachable
index range is not proved, so adjacent ROM bytes are not table entries.
The second inlined constructor submits its object to task 3 through
`0x254c84` at `0x20e858`, while its channel selector at byte `0x0c` comes
from controller offset `0xe4`. These are distinct producer paths, not
interchangeable evidence of speech activation.

The `0x20dbb0` caller submits to task 3 at `0x20dc0c`; the table-derived
`0x210bba` caller submits at `0x210c5e`. Neither inspected constructor-to-
submission interval changes object byte `0x0c`. This bounds those paths,
not all aliases or callers of the surrounding functions.

One previously dynamic candidate has an explicit type-`0x02` setup:
`0x20ed02..0x20ed04` stores type `2` in stack slot
`SP+0x4c`, which the construction sequence at `0x20ef98..0x20efa4` copies
to the object halfword and byte 3. Its operation is held in FP (initialized
to `4` at `0x20ecfe..0x20ed00`); its selector byte `0x0c` is copied from
stack byte `SP+0x3a` at `0x20efbe..0x20efc6`. Recovering that stack byte's
writers and the intervening branch paths is the next concrete MCU-side
question. A second dynamic candidate at `0x20f3aa` copies type from `[SP]`
and selector from an indexed array at `[SP+0x28]`; its type and reachable
selector values remain unresolved. Neither path yet proves that `0xc3`
reaches the native pending speech-mode word.

The stack selector has two mapped writers: `0x20ef26..0x20ef32` stores
`0x80` when incoming request byte `[R5+6]` is zero; otherwise
`0x20ef36..0x20ef44` calls `0x20e198` with request bytes `[R5+7]` and
`[R5+0x48]`, then stores its result at `SP+0x3a`. The leaf routine's
explicit `0xc3` return at `0x20e1c0` selects first argument `0x21`,
independent of the second argument. Other explicit first-argument cases
are `0/1/3/11/15/19`, returning the `b0/b1`, `c0/c1`, `d9`, `d4/d5`,
`db`, and `d2/d3` selector families. The unmatched cascade returns
`R0-0x21`, so byte input `0xe4` also produces `0xc3` by residual arithmetic;
that is not a second evidenced speech mode.

`tools/nse1_channel_selector_check.py` executes the identified unswapped
MCU image's leaf routine with Unicorn and checks all 65,536 byte-input
pairs, including a bounded clean return. This is an isolated algorithm
fixture, not a handset-state intervention. The live writers and valid
domains of request bytes `6/7/0x48` remain necessary before constructing
any native traffic assignment fixture.

The native side of that correspondence is byte-checked: `0x387b` consumes
the ring header with `*AR2+%`; `0x387f` extracts its low-byte type while
`0x3881` extracts its high-byte length. Thus the type-2 handler starts at
the first payload word, not at the header. Its reads at `0x3a0c/15/1b/1d/26`
consume payload words 1 through 5; `0x3a27` writes word 5's high byte to
`0x1869`. The existing transport's `tx_payload_byte` independently uses
header cursor + 1 and high-byte-first packing.

The MCU copy closes the field alignment: `0x2869a6..0x2869aa` loads length
from object byte 2 and calls `0x27185a` with object + 3. That wrapper forms
the length/type header and advances the source to object + 4. Its sole
direct ring-writer call at `0x271886` reaches `0x2717de`, whose loop packs
two source bytes high-byte first and publishes the producer at `0x27183e`.
The isolated checker executes this wrapper through producer commit for
all 82 start cursors, verifies all eleven words of a 20-byte payload,
and checks wrap and producer advancement. It stops before scheduler calls;
this is not a live handset transaction. Together with the native parser,
this establishes object byte `0x0c` -> payload byte 8 -> pending word
`0x1869`. Valid live controller arguments and traffic assignment remain
unproved; packet alignment is no longer the open question.

The request object is identifiable independently of selector semantics:
constructor entry `0x20ebec` saves argument R0 in R5 at `0x20ebfa`.
Its single candidate direct-BL caller, `0x20f144`, forwards the incoming
object unchanged from dispatcher `0x20f094` (R0 saved in R5 at `0x20f096`).
That dispatcher compares the signed object halfword against literal
`0x03ed` and selects the constructor-containing handler `0x20f102` on
equality. The isolated checker verifies that route and the neighboring
`0x03ef/0x03f1/0x03f2` routes before any handler side effects. A direct-BL
scan finds one candidate caller of the dispatcher, `0x211ca0`, passing
`[SP+0x18]`; indirect calls remain unclosed. The next producer boundary is
therefore the incoming `0x03ed` object's fields `6/7/0x48`, not a guessed
global controller write. No protocol name is assigned to `0x03ed` yet.

The incoming object's constructor is `0x26c394`. Its table base is
`0x03e9` (literal `0x26c6d8`); entry 4 at `0x26c3ec` selects
`0x26c5b0` for request `0x03ed`. That branch copies `0x58` bytes from
channel record `0x109650` into object + `0x48`, explaining the selector's
second argument. At `0x26c5f8..0x26c612`, channel-record byte 0 values
`0x20/0x21/0x2f` set object byte 6 to 1 and copy mode-record
`[0x109736+3]` into object byte 7; other channel values clear byte 6 and
leave byte 7 untouched. The checker executes this fragment for every
channel byte and four representative mode bytes (1,024 cases), including
the disabled field's untouched behavior. The common tail at `0x26c6ca`
posts the object to task 10 through `0x254c84` at `0x26c6ce`.

A literal-load scan of 524,287 halfword positions finds five candidates
loading exact value `0x03ed`: two consumers (`0x20e904/0x20f09c`) and
three calls into this constructor (`0x24ff44/0x24ff88/0x250600`). This is
not a closure over computed IDs or indirect calls. The live writers of
channel record `0x109650` and mode byte `0x109739`, and the lifecycle
conditions around those constructor calls, are the remaining producer
questions. No acquired native boot trace has shown this request completing
traffic assignment.

One mode-record writer is mapped: `0x24fa28..0x24fa32` copies `[SB+1]`
to `0x109739` and `[SB+2]` to `0x109740`. The entry is shared by a
direct pointer load at `0x24f81e` and a channel-base + `0xe9` calculation
at `0x24fa24`; the following call at `0x24fa38` constructs request
`0x03f0`, not `0x03ed`. Isolated execution verifies the byte-copy
fragment for all 256 mode values. This establishes an unmodified source
byte, not its valid protocol domain or runtime lifecycle. Channel-record
writers at `0x24f7f4` and `0x24f9fa` copy `[R8+0x1a]` into `0x109650`;
the staging records' upstream population remains open. A bounded literal
reference scan finds 56 candidates for exact pointers `0x109650`,
`0x109736`, or `0x109739`; aliases, computed addresses and indirect
writes remain outside that coverage.

Dispatcher entry `0x24ea70` binds R8 to fixed record `0x10961a` and SB
to fixed record `0x10963c` at `0x24ea84/0x24ea8e`. Isolated execution
checks both bindings before any initialization calls. Consequently the
mode writer reads staged byte `0x10963d`, and the channel writers read
staged byte `0x109634`; neither source is a transient incoming pointer.
The mode-copy test now uses that actual staging record. The next unresolved
ownership boundary is population of these staging records, not recovery
of the dispatcher's register aliases. The receive/parser helper
`0x26c8cc` calls `0x23d980` from `0x26c9c0` for one accepted incoming
packet path; whether that parser populates these exact fields remains
unproved.

An additional direct mode producer is now executed: element dispatcher
`0x23ca44` routes element ID `0x63` to `0x23d050`. With at least one
source byte, it searches the ROM table at `0x2aab70`, accepting its first
eight entries `00/01/21/41/0f/03/0b/13`; index 8 is rejected. When the
presence byte at `0x109738` is zero, `0x23d08c` writes the accepted value
to `0x109739` and `0x23cc1e` marks presence. The isolated checker runs
all 256 source bytes through the full dispatcher in three configurations
(768 cases): absent field with one source byte, absent field with zero
length, and already-present field with one source byte. Accepted writes
occur only in the first configuration; rejected values, short input and
an already-present field preserve the destination. This is a firmware-derived element
contract, not yet a named GSM field or a demonstrated live packet path.
It supplies an independently accepted `0x21` mode value without using
the unmatched `0xe4` arithmetic residue. Tracing the enclosing message
parser and its physical ingress is still required for native speech.

The enclosing optional-element parser `0x23d334` is independently executed
with message selector `0x2e` and the two-byte element `63 <mode>`. Its call
at `0x23d448` reaches the same element dispatcher. All 256 byte values,
with both absent and already-present mode fields (512 cases), reproduce
the acceptance and preservation contract above. This closes the optional
element-to-record link, not the complete message parser, its live receive
route, or native speech activation. No packet is injected into a handset
run by this isolated test.

Channel-description element `0x64` independently reaches `0x23cf3a`
and writes the request constructor's channel record at `0x109650`.
For a non-hopping channel number 1, all 256 first-byte values are executed:
type bits 1 map to channel `0x2f`, 2/3 to `0x20/0x21`, 4..7 to
`0x40..0x43`, and 8..15 to `0x80..0x87`; type 0 and 16..31 preserve
the record. The low three bits become the timeslot. The mapping uses the
ROM tables at `0x2aab90/0x2aab94`. Thus parsed types 1/2/3 supply exactly
the three channel values that enable request `0x03ed`.

A further 65,536 cases cover every second/third-byte combination for type
1, timeslot 0. In the non-hopping branch, only channel numbers 1..124
write the record; 0 and 125..1023 preserve it. Bit 4 of the second byte
selects hopping, which writes the six-bit sequence number from the third
byte and the six-bit index from the second byte's low nibble plus the
third byte's high two bits, while clearing the channel-number field.
Both branches retain the second byte's high three bits as the training
sequence. This is exhaustive for those two input bytes at one accepted
type, not proof of a live assignment or physical RF behavior.

The receive-loop dispatch at `0x26caec` routes message IDs `0x05ea`
and `0x05eb` to `0x26c98c`. Isolated execution checks both IDs and
every protocol-byte value (512 cases): the payload pointer is read from
message offset 8, and payload byte 4's low nibble must equal 6 to reach
the `0x23d980` parser call at `0x26c9c0`. Its arguments are the unchanged
payload pointer and message byte 3. Other protocol values take the
non-RR branch. This verifies the internal receive boundary; the producer
of these messages, complete message parsing, and native physical ingress
remain unproved. Subtract-cascade arithmetic is checked by execution,
not inferred from neighboring message numbers.

Constructor `0x237df8` allocates a 12-byte message and writes its requested
ID. Its `0x05eb` branch at `0x237e98` marks message byte 4 as 1 and
copies receive-record byte `0x157` to message byte 3. Receive-record byte
`0x164` selects payload allocation: values `40/50/60/a0` request 26
bytes; other values request receive-record byte `0x12b` plus 4 bytes.
The isolated checker executes 1,536 metadata/allocation cases (six kinds,
all 256 length bytes), stopping before allocation. With an isolated buffer
supplied as the allocator return, it then executes the constructor and
ROM copy routine through `0x237f08`: message offset 8 retains the buffer
pointer, the payload length halfword retains the original length byte,
and receive-record offset `0x114` is copied to payload offset 4. The fixed
branch copies 22 bytes even when the length field differs; the other
branch copies exactly the original length. Payload bytes 2/3 and a trailing
guard remain untouched, and source length byte `0x12b` is cleared after
copying. This tests copying, not allocator behavior or mailbox delivery;
the receive-record's lower transport producer remains open.

The constructor's publication tail reports its ID through `0x268e48`
at `0x237f6a`, then prepares task `0x10` and the unchanged message
pointer for `0x254c84` at `0x237f72`. Those argument-preparation fragments
are executed in every constructor case; neither external primitive is
substituted or counted as executed. A whole-image aligned Thumb BL scan
finds 14 direct-call candidates for `0x237df8`: `238312/23843e/238580/`
`23858c/2385a4/238f86/238fc2/23903e/239220/23934e/23962a/2396b0/`
`239a22/239c50`. This is a direct-call candidate inventory, not exhaustive
indirect-call coverage. At `0x23843e`, the literal ID is `0x05eb`, reached
after receive-record processing at `0x237fcc`. The record's transport
ownership and live publication remain the next boundary to establish.

Receive processor `0x237fcc` supplies the constructor's record in its
fixed-block branch for input kinds `40/50/60/a0`. Full isolated routine
execution covers all 256 header bytes for each kind (1,024 cases): input
byte 13 becomes record byte `0x157`, kind byte 11 becomes `0x164`, and
header byte 14 shifted right two becomes length byte `0x12b`. The routine
clears 23 bytes at record `0x114` before validation. Only header low bits
`01` with declared length 1..22 copy 22 bytes from input offset 15; rejected
headers leave the cleared region, although the length metadata is still
stored. This verifies one record-population branch without claiming the
segmented branch, incoming-kind meanings, or native RF delivery.

A distinct unsegmented branch of the same processor is executed for kind
0, address/control bytes `03/03`, and all 256 length-header bytes. Header
low bits `01` and length 1..20 clear the 23-byte record region, copy exactly
the declared length from input offset 17, and store that length at record
`0x12b`. All other length headers preserve the payload region and length
field (the earlier metadata-byte write is separate). This verifies 256
cases of the non-fixed block path, not its sequence-number/reassembly
branches or the semantics of incoming kind 0.

The sequence-aware append branch is also executed with kind 0,
address/control `03/00`, and length-header low bits `01`: 11,264 cases
cover every prior accumulated-length byte, incoming lengths 0..21, and
matching/mismatching expected sequence values 0/1. Lengths 1..20 append
from input offset 17 to record offset `0x18 + prior_length` only when
the expected sequence is 0 and the new total is strictly below 252.
Successful appends update record length byte `0x113`; all other cases
preserve that byte and the 251-byte accumulation region. This exercises
one sequence value and the append bound, not acknowledgement, completion,
all sequence numbers, or the caller-owned record lifecycle.

Receive loop `0x238280` binds its two records to `0x108094` and
`0x107f2c`; its dispatcher routes incoming message `0x040b` to block
processing at `0x238400`. Both facts are executed in isolation. Selector
`0x237f78` is checked in 4,096 cases (eight input kinds, flag 0/1, all
address bytes). Kinds `40/50/60/a0`, or kind `80` with nonzero byte 12,
select return 0 directly. Other inputs use address bits `0x1c` from input
byte 14 (byte 16 for kind `70`): zero returns 0, `0x0c` returns 3,
and other values return `0xff`. The caller uses 0 for the first record,
3 for the second, and rejects other results. Incoming `0x040b` ownership
is now the concrete lower-producer query; these checks do not establish
its native DSP origin.

The aligned Thumb literal-load scan for `0x040b` finds four candidates:
consumer `0x2382ac` and constructors `0x26b8b8/0x26bb1c/0x26bc6e`.
The first two constructor fragments are executed in 768 cases: they
rewrite the original message ID to `0x040b`, copy original byte 4 to
byte 11, and set byte 12 to zero (`26b8b8`) or original byte 6
(`26bb1c`, tested with flag 0/1). All other 40-byte message contents
are preserved. Their publication argument fragments target task `0x0d`
with the same message pointer. The intervening `0x26923c` trace call and
the post primitive are not executed by this check. The third constructor,
computed/indirect producers, and these handlers' incoming DSP dispatch
remain open; literal-scan coverage is not an absence proof.

The containing handler for constructors `26bb1c/26bc6e` starts at
`0x26ba64`; its direct BL candidate is `0x291cda`. Dispatcher
`0x291c34` reads message byte 3 and, with dispatcher-state byte 1 zero,
routes type `0x80` to that call with the unchanged message pointer.
Isolated execution checks state zero and state one: zero reaches the
handler call; one instead branches to `0x291cea`. Handler `0x26ba64`
also calls `0x26b774` at `0x26bb6e`, whose branch contains constructor
`26b8b8`. This connects the three observed `0x040b` constructors to
the type-`0x80` handler statically; it does not yet prove which handler
branch runs, ring-to-dispatch delivery, or a native DSP type-`0x80` packet.

The dispatcher's receive loop calls `0x27191e` at `0x291d36` when
draining the shared ring. Its payload-copy fragment is executed in 1,300
cases: all 100 receive-ring cursors, with 0..12 available payload words
and a requested 12 words. Unlike the 82-word transmit ring, the receive
ring spans shared bytes `0x10100..0x101c7`; consumer word cursor
`0x101ca` ranges `0x80..0xe3`. The fragment clamps to availability,
copies big-endian words to object offset 4, wraps at the 100-word boundary,
and commits the resulting consumer cursor. Header decoding/allocation,
the receive loop's scheduler path, and native DSP publication have not
been executed as one chain by this check.

Receive-header decoder `0x2718a8` is executed for all 65,536 header
words at cursor zero and header `0x0180` at all 100 cursors (65,636
cases). It requests high-byte length plus 4 from `0x255aa0`; after an
isolated buffer is supplied as that return, it writes object bytes
`18 02 <length> <type>`, preserves the following guard bytes, advances
the consumer by one ring word with wrap, and returns `(length + 1) / 2`
rounded down as the payload word count. This verifies that a ring header
with low byte `0x80` constructs the type byte consumed by `0x291c34`.
Allocator semantics and native publication of such a ring entry remain
outside this isolated execution proof.

The acquired native DSP ROM contains type-`0x80` publication sites,
independently pinned by `c54x_rom4_codec_contract.py`: `0x4f1c..0x4f23`
sets AR5 length 14, AR4 type `0x80`, and AR3 payload `0x125b` for
publisher `0x37ce`; `0x50d5..0x50de` sets length 33, the same payload
address, and type `0x80` in the delayed call's instruction slot. The
common publisher sets BK=100, combines length/type into a ring header,
writes circularly, and conditionally commits data `0x08e4` before writing
1 to port 1 (`0x37ff..0x3809`). These are native instruction contracts,
not observed successful reception: the branches' decoder prerequisites,
payload construction at `0x125b`, capacity/commit gates and physical
input still need tracing. Native type-`0x80` publication is therefore
present in the image, but not yet demonstrated in a handset run.

The publisher's containing receive-decode engine starts at `0x4ef7`:
it stores incoming A/B in data `0x121f/0x121e`, and operation selector
`0x40` enters the short-block branch. That branch calls `0x5441` at
`0x4f18` and skips publication when B is nonzero. Byte-pinned entry
chains supply A from nonzero data `0x194f` (`0x9b40`, B=0), `0x1950`
(`0x9be2`, B=1), or `0x11fd` (`0x9dfd`, B=0 with an additional
active-mode/deferred-state guard). A fourth direct call at `0x9bd4`
also consumes `0x194f`, with an operation-`0xa0` timing guard.
These cells are operation selectors at these calls, not identified GSM
channel modes. Their producers, decoder input vectors, and successful
result predicates remain open; the existence of publication code does
not supply the physical input needed to execute it successfully.

Three known operation producers are byte-pinned to common routine `0x4c6f`:
`0x9a99` stores its result directly to `0x1950`, while `0x9acc`
stores its result in `0x194f` through AR2 at `0x9ad5` after a delayed
mask operation; `0x9b20` has the direct `0x194f` store described below.
This indirect store is why a direct-address-store scan
cannot close selector ownership. Within `0x4c6f`, `0x4c8e` waits for
data `0x00b1` to reach the selected sample-buffer start plus `0x7c`,
and `0x4ca3` waits for the final threshold (`0x1b1a` for operation
`0x40`, otherwise `0x1c24`); both loops use IDLE while waiting.
The first buffer choices are `0x1a00/0x1b72`. These are recovered
sample-progress prerequisites, not yet a named DMA register contract.
The cell's producer and actual sample contents must be established
before an operation result can be treated as a native radio decode.

Data `0x00b1` has firmware-owned pointer producers: the filtered-sample
frame branch reads it into AR3 at `0x33b6`, consumes port `0x27` at
`0x33c9/33cf/33d5/33db`, filters into successive AR3 words, and stores
the advanced AR3 back at `0x33e9`. Its loop runs eight iterations with
two destination writes each. Configuration routine `0x4414` conditionally
seeds the same pointer at `0x4424` when input A bit 1 is set, and writes
AR2 to `0x00b0`. These instruction sequences are byte-pinned; this is
not evidence for an autonomous DMA register. The native receive-decode
chain therefore bottoms out at the already-open port-`0x27` sample
boundary plus its scheduling/configuration, rather than requiring an
invented message peer. Authentic or independently generated RF sample
vectors and validation of their format remain required to demonstrate
successful native type-`0x80` reception.

An eight-second NSE-1 native cold boot with a generated EEPROM and the
passive `c54x_rom4_mode_observe.lua` observer exercises pointer seeding
twice and filtered-frame pointer commits eleven times. The observed
pointer starts at `0x121e` and advances in `0x10`-word increments; its
final value is `0x125e`. The positive-control frame site `0x33f3` executes
2617 times. In the same window, operation entry `0x4c6f`, decoder entry
`0x4ef7`, and both type-`0x80` publication sites execute zero times;
`0x194f`, `0x1950` and `0x11fd` finish zero. Thus sample-pointer movement
already occurs without receive-operation activation. This bounded run
does not prove those operations absent later or with valid RF input.
COBBA's RF receive callback is currently unbound and supplies zero, so
pointer movement is not evidence of valid received samples. The next
discriminating boundary is the operation-start configuration alongside
the RF sample contract, not a replacement pointer producer.

A further byte-pinned primary-operation writer starts at `0x9b15`:
after `0x2476`, it returns when data `0x194c` is zero. Otherwise it sets
AR3 to zero and AR4 to `0xffff`, calls `0x4c6f` at `0x9b20` with
A masked by `0x195b & 0x0100` in the delay slot, clears that flag bit,
and stores the result directly into `0x194f` at `0x9b27`. This additional
caller is not an exhaustive ownership census. Its control-word
provenance and runtime scheduling remain unresolved.

A 60-second native run using the generated EEPROM profile extends this
observation through the scan window: entries `0x9a7a`, `0x9aa8` and
`0x9b15`, their three mapped `0x4c6f` callsites, and decoder entry
`0x4ef7` all have zero observed executions. Frame-loop positive control
`0x33f3` executes 30351 times; filtered pointer commits remain eleven.
Control words `0x194c`, `0x1951` and `0x195b` are only observed being
cleared and finish zero, as do the three decode selectors. Consequently
these particular local early-return tests are not the first executed
blocker in this run: their containing entrypoints are never reached.
Investigate the scheduling/configuration that selects those routines
before adjusting a local gate or supplying a synthetic completion.
This is bounded runtime coverage, not proof of exhaustive caller absence.

The scheduling census must include indirect data-memory call lists.
Byte-pinned dispatcher `0x98b7` reads index `0x07fc`, looks up a
program-space branch target in the five entries at `0x98c1`, and jumps
indirectly. Its branch wrappers select AR5 values `0xee38`, `0xee3e`,
`0xee45`, `0xee4e` and `0xee57`. Byte-pinned interpreter `0x9903`
loads a function address from data memory through `*AR5+`, preserves
AR5 around an indirect call, and repeats until the next data word is
`0x00ff`. Those list contents are not program words at the same numeric
addresses: ROM-image indexing alone cannot recover them. Capture the
DSP data-space lists or establish their initialization before claiming
that the receive-operation callers have no scheduling producer.

Passive data-space capture after an eight-second native boot recovers
all five sentinel-terminated lists (sentinel omitted below):

| Data address | Function addresses |
| --- | --- |
| `ee38` | `990d,99d5,9aa8,9bbb,9d20` |
| `ee3e` | `990d,99d5,9aa8,9bbb,9d20,9d3d` |
| `ee45` | `990d,99d5,9a56,9a71,9aa8,9bbb,9d20,9d3d` |
| `ee4e` | `9aa8,9b4a,9b59,9bbb,9be0,9d20,9d3d,9d49` |
| `ee57` | `99aa,99d5,99b3,9a56,9a71,9a7a,9aa8,9b4a,9a34,9b59,9bbb,9be0,9d20,9d3d` |

Every list includes primary-operation wrapper `0x9aa8`; the last also
includes secondary wrapper `0x9a7a`. Thus a scheduling producer exists
in data, although dispatcher `0x98b7` and interpreter `0x9903` both
execute zero times in that bounded run. The next predecessor question
is selection of this scheduler, not construction of missing call lists.
The same five lists are independently checked against the complete
acquired data image with `c54x_rom4_codec_contract.py --data-image
mame/roms/noki5110/nse1_rom4_dsp_data.bin`; mutation tests cover all
46 table words, including their sentinels, and reject incomplete images.

Byte-pinned scheduler selections write callback cell `0x07fb`:
`0x55bb` assigns `0x98b7` unless active-mode high nibble
`0x1836 & 0xf0` equals `0xc0`; `0x5783` also assigns `0x98b7`.
The latter path can subsequently override it: channel `0x1835 == 0x2f`
selects `0xa27d`, `(0x1835 & 0xfe) == 0x20` selects `0xa2cf`, and
pending mode `0x1869 == 0xc3` selects `0xa436` while setting active
mode `0x1836` to `0xc1`. These are verified local store/branch fragments,
not an exhaustive callback-writer census or proof their enclosing
configuration routine runs during the observed scan. They connect
scheduler choice to the pending/active channel configuration already
mapped on the MCU side; do not set `0x07fb` directly to activate it.

An eight-second native observation counts zero executions of stores
`0x55bb`, `0x5783`, `0x578f`, `0x579b` and `0x57a3`. Channel
`0x1835`, active mode `0x1836`, pending mode `0x1869` and callback
`0x07fb` all finish zero; the added mode-word write observations contain
only initialization clears. Therefore this boot does not demonstrate
channel configuration arriving and being rejected by the scheduler's
local mode tests. Investigate the configuration entry/request lifecycle
upstream, retaining RF acquisition as an independent prerequisite;
do not infer that a forced type-`0x02` request would establish reception.

The byte-pinned configuration entry `0x5514` calls `0x23d0` and then
tests `0x1949 & 0x0008`, branching to `0x58a7` when clear. An
eight-second passive run observes zero executions of entry `0x5514`,
the test `0x5516`, and downstream sites `0x5536/0x5554`. Cell
`0x1949` receives only initialization clears and finishes zero; the
candidate bit-set site `0x6943` is also unexecuted. The zero flag does
not explain a local rejection in this run because the test itself is
not reached. Caller/scheduling provenance of `0x5514` remains the
next configuration-side boundary, distinct from valid RF acquisition.

Byte-pinned initialization chain `0x545d` calls `0x481a`, `0x5a34`,
`0x5514`, `0x5929` and `0x5475`. Its direct caller at `0x5e6a`
lies in routine `0x5e62`, which later loads callback `0x07fb` and
calls it indirectly at `0x5eef` (also byte-pinned). A separate
eight-second passive boot observes zero executions at `0x5e62`,
`0x5e6a`, `0x545d`, `0x5463` and `0x5eed`, with frame-loop positive
control `0x33f3` executing 3724 times. The absent configuration is
therefore already upstream of this enclosing routine in that run.
These counts are from a reused generated-profile NVRAM directory,
not a deterministic fresh-storage comparison with earlier runs.

The verified direct caller is main-loop site `0x318a`, returning to
`0x316b` after `0x5e62`. Byte-pinned gate `0x313f` selects this
lifecycle when `0x1949 & 0x0080` is set. Initialization at `0x315c`
then assigns `0x1949=0x0008`, `0x17a8=4`, `0x1868=0x30`,
`0x1869=0x40` and schedule index `0x07fc=0`. The loop calls host
processing `0x390a`; `0x06e3` bit 0 returns to `0x3000`, and bit 1
selects a separate branch rather than the configuration call. This
shows that initial configuration need not wait for a type-`0x02`
channel request: the transition through bit `0x0080` has its own
default initialization. The next predecessor is that flag's producer
and its main-loop consumption, not a forced pending-channel value.

A byte-pinned direct flag producer ends the routine that calls RF
sample-queue processor `0x4701` at `0x4695`. Its result tail at
`0x46e1` returns for signed `0x1956 < 0` or signed `0x198c <= 0`,
otherwise publishes a shifted value from `0x198f` to `0x1933` and,
for channel high nibble `0x20`, copies a double word to `0x177e`.
It then returns when `0x1f24 >= 0x01f4`; only the remaining path
ORs `0x0080` into `0x1949` at `0x46fd`. These are firmware numeric
result predicates, not yet calibrated RF units or an exhaustive
flag-writer census. They tie this lifecycle entrance to sample-processing
results rather than an invented DSP completion message. The port-`0x27`
input remains zero by default, and successful acquisition remains unproven.

A fresh-storage 60-second observation counts zero executions of
routine `0x4677`, queue-processing callsite `0x4695`, and result-tail
sites `0x46e1/0x46e6/0x46f8/0x46fd`; frame-loop positive control
`0x33f3` executes 30351 times. Thus this run cannot attribute the
missing lifecycle flag to any of the three result predicates: none
is evaluated. The observer retains bounded result snapshots for a
future run that reaches them. Static annotations identify candidate
callers at `0x9975` and `0x9995`; their selection and actual instruction
contracts require verification before interpreting the flag as an
acquisition failure. Fresh storage here means a newly created private
NVRAM directory containing only the generated NSE-1 EEPROM profile.

Both candidate callers are now byte-pinned. Entry `0x996b` calls
`0x2472`, tests B bit `0x8000` using A, and returns when clear;
otherwise it passes `0x195b & 0x0800` in A to `0x4677` and clears
flag bits `0x0c00` in the delay slot. Entry `0x9988` instead masks
B with `0x4000`; its branch at `0x9991` is **BEQ**, not AEQ,
so loading and masking `0x195b` into A does not change that gate.
When B is nonzero, it passes `0x195b & 0x0800` in A and sets
bit `0x0800` in the delayed call's slot. These fragments establish
the argument/branch contract only; the provenance of incoming B
and the runtime contents of hook `0x2472` remain separate questions.

For the mapped wrappers that first call `0x990d`, its byte-pinned
return tail `0x995a` supplies B from control word `0x194a`. It tests
bit 0 through A and either returns immediately or updates `0x195b`,
clears selectors `0x194f/0x1951`, and returns without changing B.
Thus their acquisition bit tests consume control-word flags, subject
to any modification by hook `0x2472`, rather than directly consuming
a raw RF sample. This source contract does not apply automatically
to every possible indirect caller of `0x996b/0x9988`; call ordering
and hook contents must be checked at each candidate boundary.

The data-image contract also covers the six lists selected by the
annotated `0xa27d` dispatcher: `0xedf4`, `0xee00`, `0xee0c`,
`0xee18`, `0xee26` and `0xee31`. The first five contain primary
wrapper `0x9b15`; the last contains secondary wrapper `0x9a7a`.
This supplies data-driven scheduling provenance for the additional
primary caller, without claiming that the alternative callback is
selected at runtime. All eleven known lists are checked word by word;
this is not an exhaustive census of every DSP scheduling table.

Fresh-storage 60-second flag observation finds only initialization
clears of `0x1932`, `0x194a` and `0x194b`, all finishing zero.
Entries `0x990d`, `0x996b`, `0x9988`, flag-builder candidates
`0x6318/0x659b`, and alternative callback `0xa27d` execute zero
times, against 30351 positive-control frame executions. This does
not support a local acquisition-bit rejection or a failed transfer
between the neighbouring flag words: their mapped lifecycle is
unentered. The open boundary remains selection of that lifecycle
from the active scan/request path, not a fabricated control flag.

Queue processing does not require the `0x4677` wrapper. A bounded
scan of the live program image finds direct-call candidates to
`0x4701` at uploaded addresses `0x0acf` and `0x0c16`, as well as
resident `0x4695`. An eight-second reused-profile run observes one
execution of `0x4701` and zero of `0x4677`; it does not reach
`0x4798` within that window. This reconciles the earlier post-scan
queue-consumer observations with inactive acquisition wrappers.
The opcode scan reports candidates, not an exhaustive disassembly
or proof either uploaded callsite executed. The observer suppresses
fetch counters while reading the live image at shutdown so its own
census cannot inflate execution counts. Trace the uploaded callers
before treating queue processing as acquisition-lifecycle entry.

A fresh-storage 60-second run identifies the active uploaded route:
`0x0acf` and resident `0x4701` each execute 5159 times, while
queue-empty entry `0x4798` executes 5157 times. Uploaded `0x0c16`
and wrapper `0x4677` remain uncalled; frame positive control executes
30351 times. The live-code window at `0x0ac7` contains the call
followed by `B = A - data[0x219b]` and a delayed BLEQ branch to
`0x0af3`. Its next comparison is `B = A - data[0x198d]`, with
another delayed BLEQ to `0x0af3`. Its delay slot ORs bit 0
through AR5 (set to `0x1266`); only fallthrough stores A at `0x198d`.
The flag alone is therefore not proof the second comparison passed. These
instruction bytes are captured by the passive observer, not imported
from the resident ROM image. The comparisons are an executed-path
candidate for result acceptance, not yet quantified predicate outcomes
or physical signal-strength units. Follow this uploaded result path
before extending the dormant resident acquisition-wrapper chain.

Fresh-storage 60-second operand observation quantifies the first
rejection: all 5158 completed calls reach `0x0ad1` with A zero;
the four bounded operand snapshots show threshold `0x219b=0x0574`
and prior result `0x198d=0`. Neither second comparison `0x0add`
nor accepted-value store `0x0ae4` executes. There are 5159 callsite
executions, leaving one queue-processing call unfinished at shutdown.
Thus a real executed comparison rejects the current zero-input RF
result before the accepted-result update. This does not calibrate
the threshold's physical units or prove how a valid waveform should
be represented at port `0x27`; those remain the software sample-source
contract needed before an acquisition fixture can be interpreted.

Encoder execution also requires conditional accumulator store `SACCD`.
[TI SPRU172C pages 4-152/153](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)
specifies that even a false condition reads and writes back the destination,
with address modification independent of the condition. The core implements
that contract; 576 cases cover both sources, twelve conditions, three signed
test values, four address forms and two ASM settings.
Conditional delayed returns with accumulator predicates are covered by 36
additional cases: all twelve predicates against negative, zero and positive
values, preserving a two-word delay instruction and popping the stack only
when taken (SPRU172C page 4-133).

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

## MAD2 hardware-acquisition routes

### Retained MADos source reference

The local source archive is
`roms/research/nok-mados/Nok-MADos-c4b9591e243a74484e27becf343655bb459f99b7.tar.gz`,
from [Nok-MADos](https://github.com/fandigunawan/Nok-MADos) revision
`c4b9591e243a74484e27becf343655bb459f99b7`, with SHA-256
`e568f33b16afecc05a9ae3cbb94590bf4eff2db0173d81ef568ad0ea27546802`.
This LGPL source is a research reference, not code imported into the devices.

`include/dspblocks/block00.h` through `block1B.h` preserve 28 upload
descriptors, 20 with nonzero declared sizes. `hw/dspblocks.c` exposes blocks
`00`, `01` and `14` as initialization, MDI setup and secondary initialization.
These are upload payloads, not resident mask-ROM captures. Their product and
firmware provenance must be established by byte comparison before any use in
a handset configuration; archival MCU addresses alone do not establish it.

A complete-payload comparison against 58 collected `.bin`, `.fls`, `.mcu`
and `.mcusw` files of at most 16 MiB found all 20 nonempty payloads in
`roms/noki3310/3310f639e.fls`, as big-endian 16-bit words. Parsed payload
lengths equal the declared descriptor sizes for all 20 blocks. The three
exposed blocks match at these **file offsets**, not MCU program addresses:

| Archival block | Words | 3310 v6.39 file offset |
| --- | ---: | --- |
| `00` | 638 | `0x1020c0` |
| `01` | 2986 | `0x1025cc` |
| `14` | 629 | `0x108128` |

All 20 matches also have the archival five-word header immediately before
the payload, within the firmware's six-word descriptor layout (12 bytes).
The sixth word is zero in every case. For blocks `00`, `01` and `14`, the
descriptor file offsets are respectively `0x1020b4`, `0x1025c0` and
`0x10811c`. This checks both payload identity and descriptor metadata; it
does not establish which blocks a particular runtime boot requests.

The own 3310 v6.39 image also contains the complete ordered pointer catalogue
at file offset `0x0f35bc` (MCU address `0x2f35bc`, image base `0x200000`).
All 28 entries index the matching six-word descriptor and complete payload,
including the eight zero-length blocks. Verify this stronger correspondence
by supplying only that image to the matcher with
`--catalogue-offset 0xf35bc --image-base 0x200000`. This is a static catalogue
contract, not proof of its MCU reader or the runtime block-request sequence.

The v6.39 boot initializes a RAM copy at `0x1107c0`: passive catalogue
reads at PC `0x200194`, beginning at 0.007424 s, show source `0x2f35bc`
and destination `0x1107c0` advancing together. The RAM-table literal has
three statically decoded consumers:

| MCU site | Observed instruction contract |
| --- | --- |
| `0x2b93bc..0x2b93cc` | Loads the RAM-table base, selects `base + index*4`, reads descriptor destination and size, and skips the payload path for size zero. |
| `0x2bcad4..0x2bcaea` | Loads entry zero, reads size at `+4`, and forms payload address `descriptor+12` and staging field at `+6`. |
| `0x2bcfbc..0x2bcfce` | Selects an indexed descriptor and copies its header halfwords into another structure. |

The base literals are at `0x2b974c`, `0x2bcb6c` and `0x2bd1f4`.
`tools/noki3310_upload_catalogue_observe.lua` records only existing memory
reads and CPU registers, with a per-PC log cap and an uncapped total. In
the 16-second research-HLE boot, flash-table reads also occur in a strided
reader at `0x2bc904` and a bulk reader at `0x2d292c`; neither observation
alone is evidence of a DSP block request. Runtime selection at the RAM
consumers is separately observed below. No resident-ROM compatibility follows
from this MCU-side mapping.

The passive RAM-table tap records exactly three reads in a fresh 16-second
v6.39 research-HLE boot:

| Time (seconds) | MCU reader | Table entry | Descriptor |
| ---: | --- | --- | --- |
| 0.106164 | `0x2bc890` | `1b` at `0x11082c` | `0x3091d8` |
| 0.220730 | `0x2bcad6` | `00` at `0x1107c0` | `0x3020b4` |
| 2.156058 | `0x2b93be` | `15` at `0x110814` | `0x308628` |

The entry-`1b` access adds a fourth consumer to the three base-pointer sites:
`0x2bc88e..0x2bc8a4` loads the fixed entry pointer, reads size at `+4`,
and forms the payload address at `+12`. Entry `15` is selected with index
offset `0x54` and enters the previously decoded indexed path. Blocks `01`
and `14` are not selected through this RAM table in this window. This
bounded observation does not establish the complete request lifecycle,
successful native execution, or the absence of other selection mechanisms.

The own v6.39 entry-`1b` extraction is supported by
`tools/extract_nsm3_verifier.py --product 3310`: 223 words, SHA-1
`6646da3c5be9c70deda7e0b5b9f257d5d2ace815`, from descriptor file offset
`0x1091d8`, guarded by the complete own-image hash. Static initialization
at `0x2bc8ae..0x2bc8d8` writes the tuple
`0100 0300 0000 e800 0001 0001 0200` beginning at MCU HPI `0x100f6`.
The verifier's flash feeder starts at MCU `0x200040`, samples halfwords
at stride `0x20`, sends 115 full 512-word chunks followed by 510 sampled
words and two `ffff` words. Thus the own MCU establishes the 116-chunk
geometry independently; it is not inferred from the matching 8210 payload.
`verify-3310-native-verifier` executes this own-input payload in standalone
native C54x configuration `nhm5verify`, without a donor mask ROM. It requires
all 116 ordered chunks and fails closed at the first unsupported peripheral
read: port `0x2d`, PC `0x0f9f`. Explicit comparison variants supply the
existing COBBA model and PROM-version inputs; they publish at PC `0x0f64`
with own-flash calculation fingerprint `ce0f433f` and PMST `ffa8`. Changing
the supplied register-F value changes the published result, and the ROM4
variant changes the supplied version result. These are peripheral
sensitivity checks, not identification of the fitted DSP's PROM version,
COBBA register values, a native handset boot, RF acquisition or speech.

This establishes byte identity, not unique archival provenance or a compatible
resident DSP ROM. Other products share complete individual payloads: for
example, 3210 v6.00 matches blocks `05`, `06`, `0f` and `16`. Raw images were
searched in both 16-bit byte orders; transformed/container offsets require
their own loader mapping. No handset configuration changes follow from this
comparison. The useful next boundary is the matching firmware's descriptor
and upload consumer, rather than executing these blocks against an unrelated
mask ROM.

Reproduce complete-payload matches with
`tools/dsp_upload_reference_match.py tmp/nok-mados-reference/include/dspblocks roms/noki3310/3310f639e.fls roms/3210f600a.fls`.
The JSON includes image and payload hashes, all occurrences in both byte
orders, and explicit file-offset labeling; malformed or size-inconsistent
source arrays fail rather than silently contributing partial signatures.
Each occurrence includes its preceding six-word descriptor candidate and
an explicit comparison with the archival five-word header. A payload match
without a matching descriptor is retained as such, not promoted to a
matching upload contract.

Inspection of `hw/dsp.c`, `hw/m_dsp.c`, `hw/dspblocks.c` and their DSP headers
does not identify the 8810 port-`0x36` completion-bit owner or its timing.
The MCU SIMI offsets in `include/hw/ioports.h` are a separate address space
and do not supply a DSP port mapping. This bounded negative result does not
exclude other historical sources.

### Capture boundary

A September 2026 public-source census found no complete, reproducible MAD2 DSP
proxy build. The current external emulator contains a remote-DSP transport and
describes a work-in-progress bridge to a custom MADos handset image, but its
public tree contains neither that phone-side image nor DSP-proxy wiring or
firmware. Its published ESP32 design is a SIM-card reader and is unrelated to
DSP register capture.

The preserved
[`Nok-MADos`](https://github.com/fandigunawan/Nok-MADos) tree nevertheless
contains the useful historical foundation. `hw/dsp.c` includes AlexD-attributed
C54x bootstrap words, uploads code through the ordinary MAD2 HPI window, and
contains a `dsp_readmem()` path that copies DSP-visible results into phone
flash. `apps/dspread.c` repeatedly releases and resets the DSP with cycle-offset
timing to recover protected program-ROM contents into MCU-visible RAM. This is
the documented security-hole family behind the ROM4 dump, not an operational
register capture. The existing payload initializes its own architectural state
and does not report the pre-operational `IMR` value required here.

That code makes a future no-solder experiment plausible: flash a custom MADos
image through an ordinary DCT3 flash/service cable, upload a small DSP payload,
and return captured words over MBUS. It does not eliminate the need for a real
ROM4 handset. Capturing the load-bearing boundary also requires arranging the
payload before the operational ROM performs its `IMR` OR-mask sequence; dumping
registers after a separately reset diagnostic payload would answer a different
question.

Two lower-level alternatives are documented but weaker:

- A published 5110 forensic experiment identifies MAD2 `JTRST`, `JTCLK`,
  `JTDI`, `JTMS` and `JTDO` test pads and uses fine-wire soldering for ARM-side
  JTAG access. That can observe MCU memory and the HPI window, but does not by
  itself expose the embedded C54x architectural registers.
- The NSE-1 service manual identifies `DSPXF` at test point J222 and documents
  phase transitions for DSP initialization, synchronization and registration.
  A logic analyser can validate phase timing, but the one-bit flag cannot
  recover `IMR`.
  The [NHM-3 manual](6250_bringup.md#product-documentation-boundary) supplies
  an additional board mapping, `DSPXF` at J314, with initialization/download,
  synchronization and registration diagrams. Its embedded captures predate
  the manual and are not independently established 6250 timing measurements;
  retain the phase vocabulary without importing numerical delays.

The historical
[`5110 DSP ROM v4` discussion](https://nokiafree.org/forums/archive/index.php/t-39175.html)
also rules out a generic ICE or Code Composer Studio recipe: the authors state
that the on-chip protection blocks ordinary external reads and that the dump
used a ROM-code security hole. No public service-tool command, NokiX script,
JTAG recipe, ESP32 design or archived binary was found that captures MAD2 DSP
MMRs at the required boundary.

The smallest credible future hardware task is therefore not a new electrical
bridge. It is a MADos-derived ROM4 capture image plus an MBUS result protocol,
run on a real 5110 with a conventional flash/service cable. Before attempting
it, obtain or reconstruct the unpublished proxy details and verify that the
capture executes before operational receiver initialization.
