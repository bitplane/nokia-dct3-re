# ROM4 native boot observation controls

Profile-specific loader/bootstrap, assist-negative and unassisted native observations. [The ROM4 loader](rom4_dsp_loader.md) owns current handset acceptance; [C54x execution contracts](c54x_execution_contracts.md) owns CPU qualification. Retained earlier captures do not replace the named current gates.

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
[physical-clock contract](rom4_audio_transport.md#nse-1-physical-clock-contract).
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
[COBBA control boundary](native_efr_codec.md#native-efr-encoder-acceptance).
The separate [DTX conversation probes](native_efr_codec.md#native-efr-dtx-conversation)
target nonzero background/SID/unusable-frame intervals and speech resumption.
They reproduce the complete 518-frame nonzero-background conversation after
correcting generic XC Access-stage condition visibility. Active-speech error
policy and handset audio transport remain outside that acceptance.
The separate [decoder homing gate](native_efr_codec.md#native-efr-decoder-homing)
also reproduces a 46-frame reset/repeated-speech conversation with full RAM
replay. It models the original caller's parameter-pointer convention, not an
extra reset between frames. Error/homing combinations remain unproved.
The [encoder homing gate](native_efr_codec.md#native-efr-encoder-homing)
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

## First reproduced run

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

## Compatibility-assist audit

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

## Unassisted external closure

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
