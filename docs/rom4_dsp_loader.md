# ROM4 DSP loader contract

## Current result

The NSE-1 v5.30 MCU drives a real staged DSP boot. A cold transfer runs through
loader1 at DSP address `0x0f00`, the MCU asserts DSP reset, and a second release
preserves DARAM. Loader1 then requests block `0x12`; the matching descriptor
installs loader2 at `0x0d80`. Loader2 enters the run-mode dispatcher, which
requests further blocks by catalogue index.

This result no longer requires the compatibility path that copied words from a
flat DSP image into `0x2000..0x27ff`. Its old explanation was wrong: loader2
does not clear that range. Loader1 itself populates the resident branch table
with a repeated MVDP data-to-program transfer.

## Extended program overlay

[TI SPRU131G section 3.2.5](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
defines a common lower 32K program window when `PMST.OVLY=1`: CPU accesses
to `xx0000..xx7fff` resolve to page zero, while the upper half remains
page-specific. The core applies this to fetches, program operands and
accumulator-addressed `READA`/`WRITA`. Logical PC/XPC and far-call stack
values retain the executing page. DMA addresses are not passed through this
CPU translation. This does not establish the physical RAM extent or the
data/program alias map of any particular C54x variant.

`check-c54x-core` exercises overlay-on fetch and accumulator transfers,
wrong-page sentinels, the distinct upper-page window and logical XPC.
Existing overlay-off far-control, repeat and save/replay fixtures remain
independent-page tests.

The MU4 loaded entry independently exposes `f4a0` (`LD #0,ARP`) and
`46f8` (`LD Smem,DP` with an absolute address extension). TI SPRU172C's
Load T/DP/ASM/ARP syntaxes 5 and 2 specify one and three cycles respectively,
plus the long-address extension cost. Eight ARP immediates and three absolute
DP values check field masking, unrelated ST0 preservation and extension
consumption in the core gate.

MU4 opcode `fa47` is delayed conditional branch ALEQ (SPRU172C condition
`47`): signed 40-bit A less than or equal to zero. Its three-cycle branch
decision precedes two delay words. Five executable fixtures test both signed
extremes, zero and positive values; changing A in the first delay word must
not change the captured decision. Both paths retain the expected delay-word
effects and measured port-to-port cycle intervals.

## Descriptor fields

The six halfwords in each recovered catalogue record are:

| Word | Current meaning | Evidence |
|---|---|---|
| 0 | DSP destination | Matches loader2 home `0x0d80` and later installed block homes. |
| 1 | transform/decoder selector | Stable within block families; values such as `0x1e80` and `0x0b39` are callable DSP addresses. Exact algorithm names remain open. |
| 2 | remaining output words | Block 2 falls `0x011c -> 0x00a4 -> 0x002c` as its destination advances by `0x78` words per chunk. |
| 3 | staging address | Stable during a block and points into the loader's low-DARAM work area. |
| 4 | input chunk length | Matches the upload-header values observed at each transfer. It need not equal output length. |
| 5 | flags | Zero in the recovered 27-record catalogue; no semantics assigned. |

The multi-chunk observation is important: catalogue records describe a
transform from staged input to an output range, not a direct flat-memory copy.
Tools must therefore not reconstruct DARAM by copying bytes following a
descriptor to word 0.

## Observed lifecycle

The normalized ordered trace is:

1. loader1 receives the cold double-buffer stream and the first DSP release;
2. reset is asserted after the cold transfer;
3. descriptor 0 (`fd00 ff80 0244 0500 0078 0000`) is installed and acknowledged;
4. the warm release resets CPU state while preserving DARAM;
5. DSP request `0x12` selects descriptor 18 and installs loader2 at `0x0d80`;
6. run mode requests block 1, then block families with input lengths `0x0078`,
   `0x0118`, and `0x04ec`.

`make check-c54x-rom4-cold-execute` starts the local clean-room core at the
mask-ROM reset vector with the recovered program image and complete
`0xb000..0xefff` DROM populated. Its expected isolated upload frontier is
PC `0x0f01`, with program word `0f00` still zero: loader1 is not resident in
the mask image and must be uploaded by the MCU. A fixture-only read observer
terminates on the fetch at `0f00`, before its word executes; the PC has already
incremented. No loader word is supplied or CPU state forced.

Zero is a valid ADD opcode, not an illegal-instruction sentinel. Waiting for
an illegal opcode let the isolated core execute unpopulated program memory
and eventually reach unrelated idle code at `7eca`; that is not an upload
frontier. The cold fixture initializes the selected DROM image before reset
execution, just as the transform-entry fixture initializes its data snapshot.
This gate establishes the reset-vector branch to the absent loader only,
not native RF, speech, or a completed MCU/DSP upload exchange.

`tools/dsp_rom4_upload_trace_check.py` verifies this ordering and rejects an
observed input length absent from the recovered catalogue.

The local MAME tree contains a selectable `nokia_dsp_c54x_device` backend. It
maps DSP data addresses `0x0800..0x0fff`
directly onto DSPIF's existing MCU-visible shared store and resolves program
accesses in `0x0080..0x27ff` through the same data store while `PMST.OVLY` is
set. The host doorbell latches C54x `HPINT` (maskable source 9, vector 25).
The `noki5110` research configuration selects it instead of the HLE. NSE-1
firmware now uploads loader1 through ordinary HPI writes, drives the recovered
hold/release sequence, and executes the uploaded code. Runtime interleave is
boosted only at DSP reset release and the two mailbox ownership writes; no DSP
reply or shared word is synthesized.

The cold exchange, block-`0x12` request, and loader2 install now complete in
MAME. Loader1 publishes matching header words `0x0802=0x0803=0x0004`, the MCU
enters its 64-block streaming loop, and the core executes the uploaded loader
and mask-ROM transform helper. After the warm release it transforms descriptor
0, writes request selector `0x0012` to DSP word `0x0871`, and raises the
DSP-to-MCU service interrupt. The MCU's ordinary IRQ4 path consumes that
selector, streams descriptor 18 (`0x01f4` input words), and installs loader2 at
`0x0d80`; no shared word or acknowledgement is synthesized.

Loader1 also fills the otherwise undeclared resident range beginning at
`0x23c4`. Its `RPT`/`MVDP *AR2+, pmad` sequence increments both the indirect
data source and the encoded program destination on each repetition. Modeling
that architectural repeat behavior removes the last flat-image/`SEEDDARAM`
dependency from the MAME backend. The installed program subsequently requests
and receives another block (`0x044c` input words) and executes into the
operational ROM. The DSP's INT2 input is the level comparison between the
port-1 command latch plus live MDISND ring state and the port-2 accept mask.
The operational ISR now consumes the complete MCU ring (`0x0033/0x0033`) and
returns to its ordinary idle at `0x31a5` without an illegal instruction.
DSP port-1 completion strobes also ring the MCU doorbell. The ordinary FIQ0
handler drains the DSP receive ring to `0x0083/0x0083`; this is transport
delivery, not a synthesized MCU message.

The coherent run also executes the challenge transform on the MCU-supplied
security records. With the generated factory profile and a fresh NVRAM root,
the input halves are `d6fb 4394 e437 da16 9668 964f` and
`5cd4 32fe 5be2 dba6 9643 82d7`. The C54x decodes them to the wildcard profile
and publishes its response through FIQ0. At the validator boundary the MCU
object contains `3532 0000 ffff ffff ff0f 0000 0078 54c2 0000 0000 0000 0000
087c 0000`, and validation continues with `r6=1`. No reason-4
warm reset occurs. The input acquisition is also organic: DSP reader `0x4610` selects
COBBA serial registers 5 and 6, reads the modeled nominal values `0x160` and
`0x010`, and the caller stores `0x0016`/`0x0010` in its input object. The
coherent gate checks these values after the transaction. The old rejection
conclusion used stale ROM addresses and mistook the
DSPIF backing array for the mapped HPI view. It is retired.

`make verify-5110-menu` extends that result through the handset UI. It creates
a fresh NSE-1 EEPROM profile, boots v5.30 with the C54x backend, and applies a
physical Menu-key transition during ordinary awake idle. The key reaches
MAD2 IRQ0 and the ROM4 five-by-five matrix scanner;
firmware renders the Phone book menu with deterministic frame SHA-256
`d82cc6891fcf4efb0bd11ded583508f40826f58aa69463708a46897b76fdffb5`.
The gate rejects baseband resets and illegal DSP instructions. It uses no HLE
DSP reply, firmware-state write, or injected firmware input event.

NSE-1 uses KBGPIO data/command ports `0x2b`/`0x2c` and a five-row matrix.
Firmware temporarily masks all columns while changing row drive and consumes
the cold-start power indication as a one-shot. MAD2 IRQ0 acknowledgement must
therefore clear that latch and adopt the released matrix as its new baseline;
retaining it as a held key makes column 4 permanently low and hides subsequent
physical input transitions.

The first NSE-1 wiring hypothesis used special column mask `0x02`. It reached
IRQ0 and scanner `0x290c2c`, but yielded raw `0x81` and no shutdown. The
translation at `0x291ba0` reads selector byte `0x10b5ba` (zero throughout the
observed boot) and indexes the five-byte special-key table at `0x2ab518`.
The active table maps `0x81` to `0x3e` (no key), but `0x84` to `0x0d` (power).
The former claim that a neighboring key-map variant was needed was wrong:
`0x0d` is in the active variant, at special column 4.

With NSE-1 power wired to column mask `0x10`, the cold-entry/menu oracle still
passes. A physical press at 8 s produces semantic `0x0d`; a 220 ms press
releases without rail-off, while a sustained hold reaches CCONT rail-off at
9.363 s. `make verify-5110-power-lifecycle` checks both paths without a
firmware-state write or injected key event. This establishes a ROM-consistent
input contract; the exact board trace still awaits physical-board evidence.

The former claim that ROM4 ordinary idle writes clock-control `0x0e` around
8.54 s was wrong. A focused register trace over 35 unattended seconds instead
observes `0x2c` at `0x27f0d6` and `0x0c` at `0x27f10a` around that interval;
both leave clock-stop bit 1 clear. The direct callsite scan for helper
`0x29284c` found a setter call at `0x23193c` in a teardown branch and a
clearer at `0x28664e`, not an ordinary idle entry. This scan covers direct
Thumb BL sites, not indirect dispatch.

`make verify-5110-late-input` presses physical Menu at 12 s and reproduces
the Phone book frame while checking every traced MAD2 clock-control write
before the press for a bit-1 request. This proves later idle responsiveness,
not wake from suspended ARM execution. The NSE-1 profile still disables the
ROM6-derived clock-stop action until a ROM4 path actually requesting it and
its wake contract are characterized.

The backend also terminates the distinct C54x memory-mapped `0x22`/`0x32`
parallel control path at COBBA. Frames retain their recovered opaque form:
bits 15--12 select one of 16 registers and bits 11--0 carry data. The coherent
ROM organically emits register-C transitions `0x008 -> 0x0c8` during codec
bring-up and writes codec serial port `0x21`. A corrected twelve-second
interface census originally recorded zero reads from DSP sample port `0x27`
and zero writes to port `0x32`. That absence is now
explained by the ROM4 cold-entry interrupt-mask contract rather than RF data.
The recovered code ORs `0x0204` and then `0x015a` into retained IMR state, and
the INT0 vector at `0x3204` owns the receiver. Supplying cold-entry bit 0 yields
terminal `IMR=0x035f` and organic sample reads; routing the same frame edge to
INT3 reaches only an empty `RETF` handler.

A 30-second run now schedules more than 6,000 CTSI frame edges and services
the receiver continuously. Ports
`0x31` and `0x32` are retained as saved, passive port-write observations and
port `0x27` remains connected to deterministic unattached input. The active
INT0 loop reads 32 words per frame after a 21-frame startup offset: the 12-second
census measured 82,432 reads over 2,597 frame expiries, and the 30-second gate
measured 207,232 reads over 6,497 expiries. Four unrolled reads at ROM word
addresses `0x3249/0x324f/0x3255/0x325b` repeat in the active loop. These are
DSP-facing sample words; their source, electrical scaling, bit packing and
I/Q ordering are not established. The repeated FIR operations establish a
sample-processing path, not that port `0x27` is the RF burst interface.
Supplying valid FCCH/SCH/BCCH input is therefore still open.
The cadence is only 32 words per 4.615 ms GSM TDMA frame, so it is not
evidence that this loop transfers an entire radio burst. The recovered ROM
branches on control words `0x00ac/0x00ad/0x00af` after the unrolled reads;
one branch calls `0x48d0` and another reaches `0x42f6` before returning to
the frame handler. A temporary write watch in a five-second coherent NSE-1
run found no post-upload changes to candidate processing words `0x2180`,
`0x21a9`, or `0x21c2`, and no receiver-driven advance of the MCU receive-ring
producer at `0x08e4`. Those words were each initialized once by the DSP
upload at PC `0x31d7`; the four ring advances observed were startup traffic
at PC `0x3805`. This is a negative result for the unattached-input run, not
proof that the processing branches cannot publish results with real samples.
The branch conditions and sample format remain the next receiver questions.
An earlier relative-compare instruction-PC census resolves that model's default
route more tightly. `0x00ac=0` fails both mode comparisons at `0x322e/0x3232`,
so INT0 runs the four-read block at `0x3249..0x325b` eight times. It then
reaches `0x3268`, tests `0x00af` at `0x326b..0x3272`, and branches directly
to `0x32c3` and the common exit at `0x33f3`. The `0x3274..0x32c2` work,
including calls to `0x48d0`, `0x42f6`, and `0x44b6`, does not execute on that
first zero-input frame. These are observed branch PCs, not decoded purposes
for the callees. ROM `0x4414..0x4460` is an initializer that stores `0x00ac`
and `0x00af`; direct call sites include `0x09a4`, `0x0ab8`, `0x4de3`,
`0x4e00`, and `0x9b62/0x9b9f/0x9bb0`. Whether ordinary acquisition reaches
this initializer was unproved under that model. A relative-compare 30-second no-cell
run recorded zero
executions at `0x4414/0x4428/0x4433/0x4460` while servicing 6,497 frame
expiries and 207,232 port-`0x27` reads. That absence is superseded by the
current absolute-compare observation below; it is not a current exclusion.
No firmware or DSP state was forced for either census.
The host-command jump table is distinct: DSP code `0x398d..0x3997` adds the
incoming type to data-ROM base `0xb00f`, reads the function pointer, and
branches through it. Type `0x1a` selects table entry `0xb029 = 0x3d5e`,
consistent with the observed `0x3d70` search-list handler; it does not select
`0x4414`. Direct calls to `0x4414` instead come from `0x09a4`, `0x0ab8`,
`0x4de3/0x4e00`, and `0x9b62/0x9b9f/0x9bb0`. The last group is reached from
the separate `0x3660` control dispatch or ROM function lists. This classifies
`0x4414` as a DSP control-mode operation, not a direct host-packet handler.
A changed-write watch over that earlier relative-compare boot found no writes to
`0x1973` or `0x00ac`. Word `0x00af` was decremented at PC `0x3273` on 6,476
INT0 frames after the 21-frame startup interval, starting at `0xffff` after
its zero-initialized first pass. The branch at `0x326f` therefore continues
around `0x3274..0x32c2` instead of entering its processing calls. Candidate
writers of `0x1973` in the ROM are
`0x4e33` and the immediate stores at `0x77c6/0x78f0/0x7c76`; which control
transition reaches them in ordinary acquisition is still unresolved. All
three immediate-store sites first OR bit `0x8000` into `0x1949`, so they are
one repeated state-change pattern, not three independent host requests.
The `0x77c2` path reaches that pattern only when the `0x1953` value is not
positive; it follows calls into the separate `0x7b0a` mode family. The
`0x78ed/0x7c73` paths likewise follow `0x7a75` and join `0x7778` after
setting the flag. In that earlier 30-second no-cell run, a changed-write watch
recorded zero writes to `0x1949`, `0x1953`, or `0x1973`. These are dormant
control-mode paths in that run, not evidence of a missing direct MCU command.
The source of the transition, and whether it belongs to ordinary search or a
later channel mode, remain open. The recovered ROM also has three `PORTR`
sites for port `0x39` (`0x40ff/0x4102/0x41a4`) beside
port-`0x38` status reads. A coherent 12-second run records zero reads on both
`0x38` and `0x39`; those sites are dormant while the `0x27` loop runs. Their
activation and sample encoding must be recovered before a controlled GSM
burst can be meaningfully attached.

The current absolute-compare fresh boot reaches `4414/4428/4460` three
times and `4433` once. Mode `00ac` is cleared at startup, then written as
`1` at 2.104547481 s, `2` at 2.109165308 s, `9` at 2.138664673 s, and `1`
at 2.161736846 s. The first three writes occur at PC `442b`; the final write
occurs at `32b6`. The active frame paths include `32f4` 6,033 times, `33b4`
seven times and sample-read instruction `3249` 3,384 times. Thus the
mode initializer is live, and the old zero-mode-only description is not the
current receiver lifecycle. Mode 1 branches through `32f4` and has its own
four-read block beginning at `330a`; its sample meaning and publication
contract remain unproved. Word `1973` still receives only zero initialization
in this run. The RF gate independently passes with 6,499 frames, 207,040
sample reads and three control writes, without acquisition. These counts
come from passive program/data taps in `c54x_rom4_mode_observe.lua`, with
31 observed fetches at `3da2` as a positive control. Observer handles are
retained for the whole machine lifetime; otherwise Lua garbage collection
can invalidate a cached-fetch tap. The next receiver analysis must start
from this active mode-1 path rather than try to activate `4414` again.

The current mode-1 processing census records 48,264 fetches of `330a`,
eight per mode-1 frame, followed by 6,033 executions of the absolute
`CMPM 06fd,#1` at `3326`. The reduction branch at `332b` executes twice;
`3347` executes 6,031 times, with `06fd=1` at its first two observed entries.
All 6,033 frames reach `3360`, `337c` and `33b0`. At the first two `337c`
entries, control word `06bc` is `0828/082a`; the corresponding exit
observations are `082a/0828`. This is live processing, not an idle sample
reader. The delayed conditional branch includes the bit-clearing instruction
at `337c` in its delay words, so that instruction's execution alone does not
prove the following processing arm was selected.

At these bounded snapshots, `2194=3` and `2196=0`. No fetch occurs at the
candidate processing call `33ac` to `42f6`; `42f6` itself executes four times
elsewhere in the boot. Its global execution count therefore must not be
used as evidence that this sample-processing call succeeds. The passive
observer records these exact instruction-start sites (not their immediate
operand words), and the fresh RF gate still passes with 207,040 sample reads.
The sample encoding, the computation feeding `2194/2196`, and whether this
arm reports RF acquisition remain unresolved. No nonzero sample or control
state has been synthesized to satisfy the comparisons.

The operands at `2194` and `2196` are double-word loads, not independent
16-bit samples. `tools/c54x_rom4_comparison_observe.lua` records their complete
pairs as `0003d9de` and `00000fa0`. At the first four observed `3382` load
entries B is zero; after the accumulator subtraction at `3384`, A is
`0000000fa0` (+4,000). The `BCD ... ALT` at `3385` is fetched 3,017 times;
its negative-result destination `33a1` and the later comparison at `33a4`
are never fetched. Thus the later `2194/5` comparison does not explain this
run's exclusion: the preceding comparison already selects another arm.
No RF units or meaning are assigned to the pair values.

The observer sees three CPU-bus writes to `06fd`: startup zero, zero at
PC `312c` (2.120203519 s), then one at `3135` (2.180201942 s), consistent
with the two early alternate reduction frames. Each pair half has only a
startup-clear write through this CPU-bus tap even though its later value is
nonzero. Backend upload/copy writes bypass that tap, so this is not an
all-writer closure or proof that the operands never change. Their original
upload ownership and the computation feeding B remain the next static/data
flow questions. The fresh RF acceptance gate passes alongside this passive
capture; neither an artificial measurement nor a threshold override is used.

The steady reduction at `3347..3356` consumes 16 words beginning at `1a48`.
Ignoring saturation/guard effects, its two shifted linear combinations are
`A=(x1+x2-x5-x6+x9+x10-x13-x14)<<16` and
`B=(x0-x3-x4+x7+x8-x11-x12+x15)<<16`; it is not a sum of squares.
The final read does not advance AR2, leaving it at `1a57`. Four passive entry
snapshots contain sixteen zero words and leave both accumulators zero.
This explains the observed zero B without assigning I/Q or RF units to the
input buffer.

Live program reads at `2400/2402` both return `fc00` (`RET`), not an uploaded
replacement, and the first four caller/return snapshots preserve zero A/B.
After the `3360` hook, the code at `3362..336b` processes the `06d2` vector;
the `2402` hook at `336c` likewise preserves B before the saved-result and
comparison tail. The hooks are each observed 6,033 times. Therefore these
hook slots do not supply an absent nonzero measurement in this boot.
Recovering the producer/encoding of the `1a48` reduction input and its
upstream port-`27` filtering is the unresolved boundary; changing the
comparison operands or installing a hook result would bypass it.

The producer census closes the unfilled-buffer explanation for this boot.
Besides 16 startup clears, the `1a48..1a57` CPU-bus write tap records 48,264
writes at PC `3322` and 48,264 at `3323`, corresponding to the parallel-store
instructions `d6e9/e4e9` at `3321/3322`. Their total is exactly 16 writes
per observed mode-1 frame. All 96,528 live writes are zero. Independently,
the port-`27` read tap records 207,040 reads with zero nonzero input words.
Thus the running pipeline fills the reduction buffer; it is not waiting for
an absent buffer-initialization event. In the demonstrated unattached-input
composition its live zero input and zero filtered output are consistent.
This is not a nonzero-signal conformance result or a specification of RF
sample packing. Further native receiver advancement requires evidence for
the actual input contract, not a fabricated internal result. The passive
comparison observer retains both counts alongside the full-width operand
and hook snapshots, and the independent RF gate remains green.

The live program snapshot further bounds the upstream filter. Setup at
`32f8..3309` selects output `1a48`, copies AR2 to AR3, sets BK to `0010`,
AR0 to `0002`, AR6 to `0080`, AR4 to `0084`, AR5 to `21a9`, and repeats
through `3325`. Four `74d6 0027` port reads occur at `330a/3310/3316/331c`.
Between those reads the dual-memory MPY/MAC instructions consume history
and coefficient words; `d6e9/e4e9` publish the two output words. The first
four passive loop-entry snapshots show ST1=`e900`, BK=`0010`, AR0=`0002`,
AR3=`1a48/1a4a/1a4c/1a4e`, AR4=`0084/0088/008c/0080`, and
AR6=`0080/0084/0088/008c`. AR5 remains `21a9`, whose first eight words are
`0387,11a3,2ad6,4000,4000,2ad6,11a3,0387` in all four snapshots.
This identifies circular history and a symmetric coefficient sequence,
not a physical I/Q mapping, scaling or sample-ready protocol. In particular,
ST1.FRCT is clear in these snapshots; interpreting the words as normalized
fractional coefficients would require additional evidence. The comparison
observer records these bounded entries without modifying input or state.

`make check-c54x-rom4-processing` regenerates the product's own EEPROM in a
private directory, runs both passive observers for 30 seconds and checks the
RF interface separately from the processing results. It requires sustained
mode-1 execution, eight filter iterations per mode-1 frame, the four ordered
filter-entry snapshots including status, history/output pointers and exact
coefficient order, matched caller/hook counts, the two early alternate
reduction frames, every-other-frame comparisons, exact full-width bounded
snapshots, live producer counts and zero unattached input/output. Missing or
duplicate observer records fail closed. These are regression expectations
for this acquired ROM and composition, not universal radio constants;
the success line explicitly reports `acquisition_claim=0`.

The physical bus is constrained independently by Nokia's NSE-1 System Module
manual: COBBA uses a twelve-data-bit/four-address-bit parallel interface for
control and radio samples, separate from serial audio PCM. The exact DSP-port
mapping and sample representation are not specified by that width alone;
see [COBBA control boundary](cobba_control_boundary.md). Do not truncate or
sign-extend port `27` by inference from the board bus without establishing
the intervening MAD2 register contract.

Port `0x27` is bidirectional in ROM4: seven static `PORTW` sites at
`0x4248/0x424d/0x4258/0x425d/0x4267/0x426c/0x4271` belong to a separate
transmit routine. The driver now forwards those writes to a replaceable COBBA
transmit callback, as it already did for receive reads; no transmit activity
has been observed in the coherent boot. The port-write sites on
`0x31/0x32` are `0x36f7/0x36fb/0x370c/0x3710/0x4020/0x4025/0xa23a/0xa23e`.
The earlier relative-timer census did not reach these writes. With the
free-running CTSI counter and absolute compare model, fresh-profile boot
emits `2a04/0006` twice through `0xa23a/0xa23e`, then `2813/0030` through
`0x4020/0x4025`. The first pair is table-driven; the latter is sourced from
accumulator MMRs 8/9. Persisted EEPROM additionally reaches `0041/0040`
through `0x370c/0x3710`. See `7110_bringup.md` for the bounded operand
observation and isolated counter/compare acceptance. Electrical meaning
and subsequent radio-mode activation remain unresolved. Calling these ports a synthesizer pair was
premature: Nokia's NSE-1 manual assigns synthesizer control to MAD2's SCU,
while COBBA produces analog TXC and AFC signals. The manual also describes
the COBBA parallel interface as carrying control and receive/transmit samples
over a 12-data-bit bus; that corroborates the boundary, not the sample encoding:
<https://www.eserviceinfo.com/preview_html.php?fileid=26879&previewid=13251>.

The fresh native control-source capture further fixes the operand provenance.
Both calls into `a22f` return to `a1d5`; setup `a228` stores pointers
`197b=1914` and `197d=1920`. At the second port write, AR5 is `1921`,
and data words `1920..1927` are
`2a04,0006,0041,0040,27a2,0030,0041,0020`. The observed first pair
comes from `1920/1921`, not an invented peer response. At `4025`, A and B
are `0000302813`; the `75f8 0008 0031` / `75f8 0009 0032` sequence
publishes A's low/high MMR halves as `2813/0030`. These observations do
not identify electrical registers, RF sample packing or acquisition.
`tools/c54x_rom4_rf_operand_observe.lua` captures the bounded sources;
`tools/c54x_rom4_control_source_check.py <error.log>` requires their exact
counts and words alongside the existing 30-second RF-boundary acceptance.
The observer retains its tap handles and suppresses callback re-entry while
reading program snapshots; otherwise those reads falsely inflate fetch
counts. Debugger data-space reads of MMR addresses are not substituted for
the architectural accumulator state.

The SIM transaction ending near 8.51 seconds is not a stalled initialization
sequence: the firmware has read all ten configured ADN records. At 31.002
seconds it organically issues `A0 F2` STATUS as its periodic card-presence
monitor while the UI also refreshes the LCD. The long quiet interval is a
healthy maintenance cadence and was unrelated to the former masked-INT0
receiver boundary.

## RF ownership and capture target

### Architectural XF diagnostic

Nokia's NSE-1 *Disassembly & Troubleshooting Instructions*, original 03/98,
pp. 11-12, identifies MAD2 D200 pin 91/test point J222 as DSPXF. Its
diagnostic diagrams distinguish initialization, network synchronization and
registration, with intermediate search/access stages. This is a potential
independent milestone signal, not a specification of RF sample packing.
Source: [Nokia manual, pp. 11-12](https://www.manualslib.com/manual/3283741/Nokia-Nse-1-Series.html?page=11).
The accessible text confirms the stage labels; exact waveform polarity and
edge-to-stage correspondence are not yet established from the figures.

TI SPRU131G table 4-2 identifies ST1 bit 13 as XF, set/cleared by SSBX/RSBX:
[TI CPU reference](https://e2echina.ti.com/cfs-file/__key/telligent-evolution-components-attachments/13-106-00-00-00-00-46-75/TMS320C54x-DSP-CPU-and-Peripherals-Reference-Set-Volume-1.pdf).
The acquired `nse1_rom4_dsp_program.bin` has seven candidate `f6bd/f7bd`
words. A fresh 30-second native run with
`tools/c54x_rom4_xf_observe.lua` observes `2603/2604` twice,
`260b/260c` once, `2613/2615` zero times and `309c` 6,461 times.
Successor fetches show XF low after `2604` near 0.134 and 2.091 seconds,
then high after `260c` near 2.114 seconds. The first `309c` changes XF from
low to high near 0.134 seconds; subsequent captured writes retain high.
The existing RF-boundary checker independently passes with 6,499 frames,
207,040 reads and the same three control pairs.

This is a bounded architectural observation. It is not an all-writer XF
census, a mapped handset-board pin, or proof of synchronization or
registration. Whole-register ST1 loads and other uploaded code may also
change XF. Matching the manual's figures to firmware-owned callers remains
necessary before assigning stage semantics.

The active caller contract is now explicit. Entry `2523` branches to
`25fe`, not to separately called helpers. `CMPM` tests the low B MMR at
`000b` for **equality** with 1, then 2: selector 1 sets and clears XF,
ending low at `2605`; selector 2 clears and sets XF, ending high at
`260d`. Other selectors reach `260e`, whose `BITF` tests ST1.XF and
selects the opposite state. That third arm is statically decoded, not
observed in this run.

Three live `2523` entries carry selectors `1,1,2`, with stack-top return
addresses `0df3,30c8,30eb`. Read-only program windows confirm actual
`CALL 2523` at `0df1,30c6,30e9` and the preceding immediate B selectors.
The first site's program words are zero in the acquired flat program image
but populated at runtime. A raw CALL/CALLD census finds ten candidate
`2523` call sites in that image and therefore does not close the uploaded
caller set. The two late callers sit in the `30xx` initialization/control
spine; their execution alone does not establish network synchronization.
`309c` independently sets XF during the periodic path, so observing XF high
cannot by itself establish registration. The native RF-boundary regression
remains unchanged under the extended observer.

The core now exposes `xf_out_cb()` as a latched architectural output, derived
from ST1 bit 13. Reset publishes high; instruction retirement and debugger
ST1 imports publish only changed levels; post-load forcibly re-drives the
restored level rather than preserving a stale external output. The output
cache is derived state, not a second saved flag. Eight executable status
variants check edge counts and rejection of unrelated-bit changes, and the
core fixture explicitly changes the debugger level then restores a saved
low output. `check-c54x-core` requires the XF-output success marker.
The 30-second native NSE-1 RF regression is unchanged. This callback does
not model OFF/HOLD high impedance, decode Nokia stage pulses or connect an
RF source; no board-level or acquisition claim follows from exporting it.

`tools/c54x_rom4_port_census.py` inventories candidate `PORTR`/`PORTW` sites
in the recovered big-endian ROM image. It accounts for the extra Smem address
word in absolute `74f8`/`75f8` instructions; treating that word as the port
would mislabel sites. The static counts are 16 reads and seven writes on
`0x27`, six reads and four writes on `0x38`, three reads and five writes on
`0x39`, and four writes each on `0x31`/`0x32`. These are opcode-pattern
candidate sites, not proof that every word is executed code.

The `0x38/0x39` cluster has a polling routine at `0x41ad`: it rereads
port `0x38` while bit 7 is set, then returns the low `0x60` status bits.
ROM functions reached from `0x5074/0x5078` and `0x7b0c/0x7b10` call nearby
parallel-interface routines; `0x7b0a` calls `0x410e` and `0x4185` in order.
The latter can read one port-`0x39` word at `0x41a4`. In the coherent
12-second run, port `0x27` is read 82,432 times, while ports `0x38` and
`0x39` are never read; both firmware mode words `0x00aa/0x00ac` remain zero.
`tools/c54x_rom4_port_census.py --call-target 0x7b0a` finds seven candidate
direct `CALLD` sites: `0x50b6`, `0x77a0`, `0x77af`, `0x7843`, `0x7ac2`,
`0x7ad9` and `0x7bc0`. The first sits behind conditional branches at
`0x50ac/0x50b0/0x50b4`; the other six are in the `0x77xx--0x7bxx` routine
cluster. These are two-word opcode matches in a mixed code/data ROM, not a
closed call graph or proof of execution.

A read-only 30-second no-signal watch of those seven sites and `0x7b0a`
recorded zero hits; a changed-write watch on DSP data words
`0x00aa/0x00ac/0x00b0/0x00b1` also recorded zero. The existing RF gate still
passed with 6,497 frame expiries and 207,232 port-`0x27` reads, but no
port-`0x38/0x39` reads or port-`0x32` writes. The temporary watch hooks were
removed. This excludes a missed live call into this cluster during that boot;
it does not identify the dormant MCU request, establish SCU register ownership,
or decode a port-`0x27` sample word. DSP I/O ports `0x38/0x39` must not be confused
with MCU MAD2 offsets `0x38/0x39`, which are SIMI registers in this driver.
The callers are guarded by a second, compact control block rather than one
isolated enable. Static decoding shows the `0x50b6` caller selects values
`0x0070`, `0x0080`, `0x00b0` or `0x00b1` from DSP data word `0x121f`; the
neighboring callers consume words in `0x18df..0x1973`. A read-only terminal
snapshot after the same 30-second run found `0x121f` and all 16 referenced
words in that range still zero. This excludes a final missed edge into an
otherwise initialized `0x7b0a` path: the operating-mode state consumed by the
whole caller family was never established. It also weakens the assumption that
this family is the ordinary cell-search entrance. Traffic-channel or dedicated-
channel activation is a plausible interpretation, not yet an established name.
`0x121f` is also the final word of the host search-list scratch copy described
below; its address alone does not establish persistent receiver-mode ownership.
A big-endian word scan of the recovered program finds 15 literal candidates:
`2733`, `3da7`, `4efa`, `5007`, `5013`, `502a`, `5040`, `504b`, `50a5`,
`50d8`, `5226`, `5299`, `52dd`, `5327`, and `53d6`. Two candidate absolute
stores are `80f8 121f` at `4ef9` (low accumulator A) and
`76f8 121f 00c0` at `5006` (immediate `00c0`). These are raw code/data
matches, not a closed writer census; indirect copies and overlays remain
outside this literal scan. The first follows a candidate direct call to
`23d0`, also used by neighboring control routines. The unresolved target is
the lifecycle entering these control routines and initializing the
`0x18xx/0x19xx` block, not another search for the already observed scratch
copy. Do not synthesize port-`0x38/0x39` readiness from these candidates.

`tools/c54x_rom4_mode_observe.lua` passively watches every write to `121f`,
`1835`, `07fb` and `1974`. Run it with `-autoboot_script` in the same isolated,
fresh-generated-EEPROM composition as `check-c54x-rom4-rf-boundary`.
Under the current absolute CTSI compare model, a 30-second run records 12
writes, all zero: startup clearing at PCs `0f10/0f15/3074`, the search-list
scratch tail at `3da3` (2.091269308 s), another write at `33e7`
(2.114326750 s), and clearing at `3074` (2.180219058 s). All four watched
words finish zero. The RF gate passes independently with 6,499 frame
expiries, 207,040 port-`27` reads, three port-`32` writes and no port-`38/39`
reads. This refreshes the earlier inactive-control observation without
assuming that the older relative-compare timing still applies. It does not
close the writer census or prove these routines are the cell-search path.
Preserved-NVRAM runs have a different RF sequence and must not be checked
against that gate's fresh-profile port-sequence expectation.

A separate fresh 30-second opcode-fetch watch records zero fetches at the
candidate stores `4ef9/5006`, their common predecessor `23d0`, and its branch
target `2626`. The positive control at `3da2` records 31 fetches of the
search-list copy instruction `e5ca`; this validates cached instruction-fetch
observation rather than treating an untested tap's silence as absence.
`23d0` contains `f073 2626` (a branch veneer); the recovered `2626` routine
contains guarded comparisons and branch tails, not a direct mode assignment.
These four candidate sites therefore do not execute in this no-cell boot.
They remain static candidates in other lifecycles, not missing peer responses
or proven ordinary cell-search initializers. The passive observer includes
the positive control and these candidate counts for reproduction.
The DROM function-list mechanism is an immediate call walker, not a persistent
frame schedule. Routine `0x9a56` calls `0x771c`; the recovered data ROM contains
`0x9a56` in 17 `0x00ff`-terminated lists between `0xedf7` and `0xeee6`.
Callers in `0xa280..0xa47f` use AR2 to select a branch, put the chosen list
address in AR5, then call `0x9903`. That routine reads a function address from
AR5, preserves AR5 on the stack, calls the function through `CALA A` at
`0x9905`, restores AR5 and repeats until the terminator. The separate
mode-dependent callback at DSP word `0x07fb` is called through another
`CALA A` at `0x5eef`. The ROM sets `0x07fb` to `0x98b7`, `0xa27d`,
`0xa2cf` or `0xa436` in the `0x55xx/0x57xx` control paths. The `0x57xx`
selection reads mode word `0x1835`.

Read-only probes in the 30-second no-cell boot saw no call to `0x9903`,
`0x9a56`, `0x771c`, `0x5eef`, `0x5514`, `0x5e62` or the list-selecting entry
points. Neither `0x07fb` nor `0x1835` changed; both ended at zero. The
type-`0x1a` handler's state was live (`0x0284=0x0001`, `0x0287=0x7fff`),
while `0x121f`, `0x1974` and the downstream mode block were zero. Thus this
control path is not active in the observed search lifecycle. Its first missing
boundary is the command or mode transition that sets `0x1835` and `0x07fb`;
the evidence does not yet establish that this path owns ordinary acquisition.
The temporary probes were removed.

Both indirect calls use `CALA A` (`f4e3`), which the C54x core previously
lacked. The core now implements `CALA[D]` for A and B, with focused return-
address and delay-slot tests. This is required for these list and callback
paths when firmware enters them, but it does not activate them by itself.
The opcode and return semantics follow the
[TI C54x CPU reference](https://www.ti.com/lit/ug/spru131g/spru131g.pdf).
Focused long-immediate load conformance also covers the dormant control
paths: `LD #lk[,SHFT],dst` now respects `SXM` rather than unconditionally
sign-extending its constant. The `LD #lk,16,B` encoding is `0xf162`, not the
previously accepted `0xf362`. Six executable cases exercise signed/unsigned
loads, the 15-bit shift, 40-bit guard extension, destination selection and
preservation of the other accumulator. The encoding and sign-extension
contract follow [TI SPRU172C, LD, pages 4-66--4-69](https://www.ti.com/lit/ug/spru172c/spru172c.pdf).
Core conformance, coherent NSE-1 execution and the 30-second RF boundary gate
pass; these arithmetic corrections do not activate acquisition or establish
the unmeasured sample interface. They are not a claim of complete instruction
timing or overflow-flag conformance.
`ABS` additionally clamps overflow to the signed 32-bit maximum under `OVM`,
sets carry for a zero result, and preserves sticky destination overflow.
Executable cases cover the TI guard-bit example `03 1234 5678` and a following
zero result; these are unary-operation checks, not general ALU conformance.
The saturation and sticky-flag rules follow SPRU131G section 4.2.2 and
SPRU172C's ABS description.
NEG uses the same signed-result overflow/saturation rules as subtraction and
retains its zero-only carry contract. Executable tests cover TI's 40-bit minimum
example with OVM off and on; the guard-bit overflow is no longer limited to the
previous special case at the signed-32-bit minimum.
Ordinary immediate, accumulator and memory ADD/SUB forms now share bit-32
carry/borrow, sticky destination overflow and signed-32-bit OVM saturation.
The shifted-16 memory ADD form preserves carry when no carry occurs. Boundary
tests cover positive/negative saturation and memory wraparound/borrow. SFTA's
right shift obeys SXM and publishes the outgoing bit as carry. This does not
yet establish multiply-accumulate, dual-16-bit mode or complete shift/load
overflow conformance; those require separate instruction-family tests.
Shifted accumulator, ASM, long-immediate and extended-memory LD forms share
SXM-aware shifting and sticky overflow/OVM handling while preserving carry.
The executable ASM test crosses the signed-32-bit limit without requiring a
40-bit wrap. Fixed-16 memory loads have separate SXM-on/off, pointer/timing,
and OVM-on fixtures: the fixed shift does not saturate or change overflow
status under OVM, unlike variable-SHIFT loads (SPRU172C, LD page 4-66).
Other multiplier and shift combinations remain separate audit surfaces.
The fractional multiply/MAC paths now double signed products with bounded
multiplication instead of left-shifting potentially negative C++ values.
Negative-product fixtures check memory MAC and dual-memory MPY results,
pointer/T effects, and one-cycle costs. A fixture-only `SQDST` case checks
negative `(Xmem - Ymem) << 16` and fractional-square accumulation; its signed
scaling is likewise defined C++ multiplication. These checks agree with TI
SPRU172C's arithmetic expressions and class-7 timing, but do not establish
all multiplier flag and rounding variants.
Fixture-only `MPYR`, `MACR`, and `MASR` Smem tests now check TI SPRU172C's
rounding rule (add `0x8000`, then clear the low word) for negative product,
accumulate-before-round, and negative subtract results. Each also checks
T/AR preservation, carry preservation, and one-cycle DARAM timing. This
does not establish every OVM, FRCT, or long-offset combination.
An `MPYU *AR2,A` fixture checks that FRCT doubles an **unsigned** 16-by-16
product without changing T or AR2, at the documented one-cycle DARAM cost.
SPRU172C explicitly lists FRCT as affecting MPYU; unsigned multiplication
must not bypass the fractional shift.
`DADD`/`DSUB` now implement double-precision and C16 dual-lane arithmetic.
Fixture-only `5193`/`518b` and `5493`/`548b` reproduce TI SPRU172C's
published add/subtract examples, including independent lane results, long
operand pointer steps, carry, and one-cycle DARAM timing. `50f8` checks
absolute-address extension and two-cycle cost with OVM saturation; `5083`
and `5483` check that low-lane carry/borrow does not enter the high lane and
that C16 mode does not saturate under OVM. None of these words appeared in the
captured ROM4 boot. Fixture-only `5283` and `5383` additionally check B as
the DADD source with A/B destinations, under both C16 modes. The same
low-word carry yields `0x50001` in double-precision mode and `0x40001` in
independent-lane mode; these are two opcode words across four state cases,
not four distinct encodings. `DRSUB` now shares the long-word decoder; fixture-only
`5893`/`588b` reproduce TI's reverse-subtract examples in both C16 modes,
including the cleared borrow flag and AR3 movement by two words. Fixture-only
`5a8b`/`5a93`, `5c8b`/`5c93`, and `5e8b`/`5e93` now check the
T-based long-word forms (`DADST`, `DSUBT`, `DSADT`) in both C16 modes against
TI's published result examples. `5bf8` checks absolute B-destination
addressing and its two-cycle cost. `5aea` checks long-offset preupdate,
extension consumption, and the same two-cycle cost. The published DSADT C16
example sets C despite an upper-lane subtraction borrow; the fixture does not
assert C for that case, so its carry behavior still needs independent evidence.
Fixture-only `5583`, `5983`, `5d83`, and `5f83` now check the B routes of
`DSUB`, `DRSUB`, `DSUBT`, and `DSADT`, each in double-precision and dual-lane
mode. The operands distinguish cross-lane carry/borrow from independent lanes;
all eight cases assert B, unchanged A/T, and one-cycle DARAM timing. Their
carry and overflow status outcomes remain outside this fixture batch. TI's
opcode table also exposed a pre-existing `SUBC Smem,B` decode error: its
encoding is `1f`, not `5e` (which belongs to DSADT). Fixture-only `1f83`
checks the corrected B destination, unchanged A, carry, and indirect timing.
The previously absent `SUBB` family now consumes the inverse of C as an
incoming borrow. Fixture-only `0e83` and `0f83` check TI's A/B examples,
including carry-out and one-cycle indirect timing.
Fixture-only `0183` checks ADD to B carrying across bit 32; `0b83` checks
SUBS to B using the unsigned memory word despite SXM. `1983` checks AND to B
zeroing the upper bits without changing carry, and `2183` checks a negative
signed MPY to B without changing T or carry. All four check one-cycle DARAM
timing and cover high-byte groups absent from the earlier fixture.
The table-driven fixture-only `2383`, `2583`, `2783`, `2983`, `2b83`,
`2c83`, `2d83`, `2e83`, and `2f83` cases check rounded/signed/unsigned
multiplication, square-to-B with T publication, and MAC/MAS accumulation or
rounding in both accumulators. Each checks its result, T, the untouched
accumulator, carry preservation, and one-cycle DARAM timing. They do not
cover FRCT/OVM combinations or all memory-addressing variants.
TI SPRU307A divides the dual-memory `0xb*` range into MAC/MACR (`b0..b7`)
and MAS/MASR (`b8..bf`). The core previously added the product for both
halves; it now subtracts for MAS. A 16-word table checks A/B source and
destination routing, rounded and unrounded results, T publication, the two
auxiliary-register paths, preserved carry, and one-cycle DARAM timing.
Only `b03a` was already in the fixture; none of the 15 added encodings was
observed in the current ROM4 idle trace. FRCT/OVM and other X/Y address
combinations remain outside this table.
TI SPRU307A likewise defines parallel `ST||MAC[R]` at `d0..d7` and
`ST||MAS[R]` at `d8..df`. The core previously decoded only the MAC half;
it now decodes the MAS half with a subtracted product. Sixteen fixture-only
words check the old source value stored to Ymem, destination A/B, rounded
and unrounded accumulation, untouched T, X/Y pointer updates, preserved
carry, and one-cycle DARAM timing. Other ASM, FRCT/OVM, and most aliased
X/Y addressing cases remain unaudited.
TI SPRU172C's published `ST A || MAS *AR5,B` and `ST A || MASR *AR5+,B`
examples now have exact-result fixtures. They check the negative 40-bit
accumulator outputs, ASM=5/1 stored words (`0222`/`0022`), rounding, both
pointer updates, and one-cycle cost. The second example adds one new fixture
word; the first reuses `d93a` from the routing table. Other ASM cases remain
separate checks. Fixture-only `d9b7` puts X and Y on the same AR5 with
opposing Xmod/Ymod. It checks the TI SPRU131G section 5.5.4 rule: X is read
before the parallel store overwrites that address, and only Xmod updates the
shared auxiliary register. This covers one alias case, not every dual-memory
form.
Additional `b83a` and `d93a` runs check FRCT product doubling and OVM
saturation after subtracting a negative product. They assert the appropriate
OVA/OVB flag, T and store side effects, and one-cycle DARAM cost. These are
status-mode variants of existing fixture words, not new opcode encodings;
other FRCT/OVM product and rounding boundaries remain unaudited.
Fixture-only `b43a` now checks the negative `MACR` half-word boundary from
TI SPRU172C: adding `0x8000` and clearing the low 16 bits rounds `-0x8000`
to zero, but `-0x8001` to `-0x10000`. Both cases assert T publication,
pointer update, preserved carry, and one-cycle DARAM timing. This does not
cover rounded FRCT overflow or other X/Y addressing combinations.
Fixture-only `3383`, `3583`, and `3783` check `MASA`, `MACA`, and `MACAR`
using A's high word as the multiplicand, including B accumulation, T
publication, rounding, and one-cycle DARAM timing. `3693` uses TI's `POLY`
example operands and results with simple `*AR3+` addressing, not the example's
circular `*AR3+%`: it checks the rounded A result, B's shifted memory word,
unchanged T, postincremented AR3, and one-cycle cost. Other `0x30..0x3f`
variants remain untested.
Fixture-only `3083` checks `LD Smem,T`; `3983` and `3a83` check the
remaining square-add/subtract accumulator destinations. `3c83` through
`3f83` check all four `ADD Smem,16,src,dst` A/B routes, signed memory
extension under SXM, untouched source accumulators, carry preservation when
no carry is generated, and one-cycle DARAM timing. TI SPRU307A specifies the
`0011 11SD` routing; these fixtures do not cover OVM/FRCT combinations,
absolute operands, or every flag outcome.
TI SPRU172C specifies that `SUB Smem,16,src,dst` clears C only when the
subtraction borrows; a no-borrow result leaves C unchanged. The existing
`0x40..0x43` handler instead overwrote C on every result. Fixture-only
`4083` through `4383` now check all four A/B source/destination routes,
borrow and no-borrow carry behavior, preserved T and source accumulator,
and one-cycle DARAM timing. Other addressing and overflow variants remain
outside this check.
External-memory wait states also remain outside this fixture audit.
`SQUR Smem` now copies its source word to T as SPRU172C specifies; the prior
handler calculated the square but left T unchanged. Fixture-only `2682`
checks FRCT doubling, OVM saturation, sticky OVA, and one-cycle indirect
timing. Fixture-only `27f8` checks signed squaring into B, T publication,
absolute-address extension consumption, and the two-cycle cost. These tests
do not establish every square/rounding mode or external-memory wait state.
The accumulator-source `SQUR A,dst` forms are now implemented. Fixture-only
`f48d`/`f58d` check signed AH as the source, unchanged T, destination choice,
FRCT/OVM saturation, and one-cycle cost. Existing `SQURA`/`SQURS` handlers now
have fixture-only `3882`/`3bf8` checks for signed square addition/subtraction,
T publication, preserved carry, and indirect/absolute timing. Their encodings
were cross-checked against TI SPRU307A's opcode table: text extraction from
SPRU172C's bit diagrams misleadingly suggested a collision with XOR.
Indirect MMR addressing was a separate core defect: `LDM`/`STLM` and the
other seven-bit MMR operand forms previously read the opcode's low bits as a
fixed address even when the indirect bit was set. They now select the AR's
low seven bits, apply pre/post modification, and clear its upper nine bits
after the access, per SPRU131G section 5.6. Fixture-only `4882`, `8892`, and
`489a` check read, postincrement write, and preincrement read at one cycle.
SFTL shifts only the low 32 bits, clears destination guard bits and sets carry
from the outgoing bit (or clears carry for shift zero), per SPRU172C page
4-158. Executable left/right/zero-shift cases distinguish this from SFTA's
40-bit arithmetic operation.
SFTA left-shift tests cover the guard-bit operand `80 AA00 1234`, shift +5,
with OVM off/on. SPRU172C page 4-156's example prints C=1, while page 4-155's
explicit rule `src(39-SHIFT)` selects bit 34, which is zero for this operand.
The implementation and fixture follow the explicit rule (C=0); this source
contradiction remains unresolved by silicon evidence and is not advertised as
hardware-validated carry behavior.
Control-flow conformance includes an IRQ raised on the first operand read of
`RPT #3; ADD`: the ISR observes all four results, and operand reads are one
cycle apart. A separate BD fixture raises IRQ inside the first delay slot;
both slots complete before service, and cycle stamps establish BD's documented
two-cycle cost (SPRU172C B, page 4-14). This corrects a previous four-cycle
charge. These fixtures do not claim full pipeline/wait-state timing coverage.
SPRU131G section 6.10.7 also protects the complete instruction following
`RSBX INTM` or `SSBX INTM`. The core retires this separate guard by
instructions, not program words, and saves/resets its pending state.
Three pending-interrupt fixtures distinguish a one-word load, a three-word
store and an interrupt-return sequence: the ISR's stacked PC proves the
following instruction completed before entry. These fixtures failed before
the guard was implemented. They do not establish general six-stage pipeline
drain timing or protect arbitrary writes to ST1.
`make check-c54x-timer-registers` asserts the stopped TRB reload contract
before running the separate timing observations. Forty-eight firmware-free
instruction cases write PRD `1234`, reload with TSS set and each TDDR value,
then read TCR through `LDM`: reload alone, `ORM #0,TCR`, and an attempted
write of all PSC bits. TIM must remain `1234`, PSC must equal TDDR,
and TRB must read zero (SPRU131G table 8-14). Section 8.4.2 explicitly
identifies PSC as readable but not directly writable. TCR writes therefore
retain the counter rather than clearing it or accepting the supplied PSC;
TRB and timer expiry reload it from TDDR. Timer rearming includes the current
PSC, and IDLE2/3 reads use their existing saved timer phase. The correctly
initialized identity-write baseline fails divider one (`11` instead of `51`).
These assertions do not establish arbitrary live TDDR changes, general
stop/restart timing, interrupt latency or native recording completion.
Fifteen additional controller-only cases load PSC `1..15` with TIM `2`,
change TDDR to zero while stopped, then restart without TRB. After one CPU
clock TIM must still be `2` and PSC must be one less than its loaded value.
Each then checks the first borrow, subsequent one-clock decrement and PRD
reload/TINT at expiry. This follows section 8.4.2: the current read-only PSC
survives a TDDR change, with the new TDDR used on the next prescaler reload.
The core saves the initial TIM for that first interval, which can be longer
than later intervals; division by the new TDDR alone cannot recover it.
Remaining clocks are rounded upward so fractional attosecond conversion
does not advance the hardware counter early. Save replay overwrites a timer
reload, restores the captured current interval and repeats the same checks.
The baseline prematurely reports TIM `1` in the first one-clock case.
Fourteen additional controller cases write TIM after one elapsed CPU clock
without an intervening counter read. TIM writes synchronize the live PSC
before replacing TIM, preserving its current interval rather than rearming
from stale state. Sixty-four TCR cases check all reserved high nibbles and
Free/Soft storage combinations: bits 15..12 read zero, stopped PSC is retained,
and Free/Soft remain writable (table 8-14). These storage checks do not
establish debugger-dependent Free/Soft clock behavior.
The full CPU suite passes, and the native NSE-1 receiver regression retains
6,499 frames, 207,040 port-27 reads, three port-32 writes and no port-38/39
reads. This preserves the existing no-cell boundary, not radio acquisition.

`make probe-c54x-timer-boundary` runs a separate, firmware-free observation
profile (`tms54test -bios timer`). It reloads PRD through indirect stores,
then sets AR1 to IMR and masks TINT. The ISR consists only of `PSHM XPC`
and `FRETE`. With PRD 1 the current core repeatedly returns to `01:0608`,
before the mask, with SP `1000` restored and IMR/IFR `0008`. With PRD 256
it reaches the marker after masking, then the branch loop at `01:060f`;
SP is again `1000`, IMR is zero and IFR retains pending TINT. No ROM,
NAND, serial peripheral or handset state participates in this reproduction.
The long-period control is checked, but the short-period result is printed
rather than asserted as correct: this probe is not a fidelity gate. It is
the small executable comparison for a future generic interrupt/write/drain
timing correction. Full pipeline behavior still requires an independently
derived timing oracle; changing PRD to make the handset proceed is not a fix.
The same profile now checks direct-address masking as well: replacing the
post-reload AR1 setup and indirect mask with `ANDM #fff7,*(0000)` leaves
the short-period marker clear, while the long-period control completes.
It reports four cases, with both long-period controls asserted and neither
short-period outcome treated as physical acceptance. This rules out the
extra pointer-setup instruction as the sole cause; it does not resolve
Decode-stage acceptance versus write visibility.
SPRU131G section 7.2, example 7-18, constrains that correction more tightly:
recognition at the end of cycle 3 inserts INTR at Decode in cycle 4,
replacing `i2`; already-decoded `i1` completes at Execute in cycle 6.
INTR then occupies cycles 7--9, and the first vector instruction executes
in cycle 10. The saved continuation is `i2`, not a fixed number of later
foreground instructions. A blanket three-instruction deferral would therefore
implement the wrong contract. The core's current `service_interrupt()` joins
recognition, IFR clearing, return-PC capture, stack update and vector transfer
at one retirement boundary and charges five cycles. That aggregate is not a
stage model; the manual's three-cycle INTR overhead alone does not justify
replacing it without accounting for recognition and drain separately.
The next executable timing fixture must distinguish request latching,
Decode-stage acceptance, memory-write visibility and vector entry. Timer
requests accepted before a later IMR write must not simply be cancelled by
that write. The existing short/long-period probe does not resolve this order.
An independent return-bus assertion covers `RETF` (`f49b`): SPRU172C
section 4-141 and SPRU131G example 7-11 specify PC from RTN and an SP
increment, without reading the stacked PC. A passive tap in the existing
hardware-interrupt fixture detects the former discarded `pop()` read.
The implementation now increments SP directly; return state and the
three-cycle instruction cost are unchanged. This does not establish
interrupt recognition/drain timing or native recorder completion.
The delayed form `RETFD` (`f69b`) uses the same RTN/SP contract and the
existing two-word delayed-transfer machinery. The fixture covers two
one-word loads and one two-word load, checks their destination accumulator,
the returned continuation, balanced SP and cleared INTM, and requires zero
stack reads in all three return cases. Disassembler fixtures cover both
forms. The specified delayed instruction cost is one cycle; these state/bus
assertions do not independently validate pipeline stage timing.
The full CPU suite and native NSE-1 receiver gate pass. The original MU4
recorder replay retains cursor `27`, pending RX and PC `02:3d76`; eliminating
the discarded stack read does not advance that independent boundary.
The existing `RPT #3`/one-cycle `ADD` interrupt fixture also reports cycle
markers without treating them as hardware acceptance. Four operand reads span
three cycles; the final body operand read to the first vector operand read
spans six cycles in the current atomic core. This separates the protected
repeat interval from its five-cycle aggregate vector-entry charge. Neither
marker is a Decode-stage observation, so this measurement cannot justify a
pipeline-drain delay or reinterpret the TI example's three-cycle overhead.
TI's [SPRU598B simulator overview](https://www.ti.com/lit/pdf/spru598),
sections 1.1 and 2.3, documents timer-capable C54x configurations and a
pipelined execution mode. This is a potential software-only differential
oracle for the synthetic probe, not a simulator run already performed.
Pipeline-flush-on-halt mode changes observation and profiling behavior;
an eventual comparison must identify the selected device and execution mode.
TI's [C54x simulator support clarification](https://e2e.ti.com/support/tools/code-composer-studio-group/ccs/f/code-composer-studio-forum/249280/how-to-use-ccs-v5-1-for-tms320c54xx)
places that simulator in CCS 3.x or earlier, not CCS 5.x. The referenced
CCS 3.3.83.20 TI archive paths returned HTTP 404, but the independent mirror
acquisition below now supplies the C54x driver and legacy automation package.
No simulator execution or timing comparison has yet been performed. The
acquired SR12 update described below is not itself that engine.

### Acquired CCS 3.3 C54x simulator package

The [FreeCalypso CCS archive](https://www.freecalypso.org/pub/GSM/TI_tools/CCS/CCS_3.3.83.20_win32.zip)
returned HTTP 200 and supplied a complete 677,575,677-byte ZIP. The ignored
collection retains `roms/research/ti-ccs/CCS_3.3.83.20_win32.zip`, SHA-256
`77e20d9daf28ffe188d4430d06667e49b79f9698dc77b088b51096f2b401715f`.
This is a third-party mirror of a vendor package, not an authenticated TI
download or a silicon timing capture.

Static cabinet inspection establishes these available components:

- `Drivers_C5400_SIM.CAB`: `TISIM54X.DVR` (528,384 bytes), C541 through
  C5416 target configurations and corresponding GEL initialization files.
  Driver strings identify simulator version 4.6.5 and pipeline information
  support; configuration descriptions include Timer0 on C549.
- `CCS_Scripting.CAB`: legacy server/COM/Perl interfaces, a C server header
  and register/memory/GEL examples. These are not modern DSS bindings.
- `Tools_C54x_sim_analysis.CAB`: the accompanying analysis component.

Only static extraction/inspection has been performed; no installer, driver
or scripting binary had been executed at acquisition. Subsequent isolated
setup attempts are described below. The PE import table includes
`CCReg.dll`, MFC42 and Windows runtime dependencies. Availability is not
proof that this package runs under the current host or licensed setup.

The next comparison must first establish a runnable C54x target and explicitly
select pipelined rather than flush-on-halt execution. Then run the existing
firmware-free short/long PRD and direct/indirect IMR-mask fixtures unchanged,
recording cycle markers, IMR/IFR, SP and return continuation. Identify the
target, package hash and execution mode in the result. Do not change Nokia
timing, cancel accepted requests or add a foreground-instruction delay from
archive availability alone. Native recorder completion remains unresolved.

### Isolated simulator setup boundary

`tools/run_ccs33_isolated.sh WORK COMMAND ...` supplies a disposable network-
and process-isolated namespace around Wine/Xvfb. Persistent writes are limited
to `WORK`; host `/data`, `/home` and `/root` are hidden. This optional vendor-
tool runner is not a MAME acceptance gate. It requires `bwrap`, `timeout`,
`xvfb-run` and Wine; prepare `WORK/home` and `WORK/prefix` first.
The default timeout is 120 seconds; `CCS33_TIMEOUT_SECONDS` accepts an
integer from 1 to 1800 for vendor installation steps. Normal command exit
also stops and waits for Wine's server so registry writes can finish before
the namespace disappears. This does not make an interrupted installation
successful.
The runner disables GLX: the host NVIDIA/EGL loader crashes Xvfb in this
namespace otherwise. Harmless `/bin/true` and `wine --version` controls both
exit zero with that setting. A child process printing output is insufficient
display evidence; the stock Xvfb wrapper can run it after exhausting failed
server starts and then report a cleanup error.

Administrative MSI unpacking populated
`run_ccs33_oracle/prefix/drive_c/CCS33Admin/CCStudio_v3.1`, including the IDE,
simulator driver and scripting DLL. The directory label is the MSI's layout,
not evidence of a different package version. Full installation stopped at
`TIBuildInstallDirRegKey` before completing installation. Inspection shows
that action constructs a registry path from `INSTALLDIR` and commits the MSI
database; it is not a target program or simulator execution. Registering
Wine's supplied `vbscript.dll` and `msi.dll`, together with the standard
`.vbs`/`VBSFile` script-engine association, lets the original installer pass
that action. A read-only query of the original MSI confirms its Component
Manager `Target` registry value comes from `[CommonFilesFolder]Texas Instruments`.
No substitute component database or modified installer is used.

The longer installation attempt reaches the bundled DSP/BIOS installer,
but its silent mode displays a hidden **Error in startup script** dialog:
`InstallProgramFolderShortcut` fails with Windows error `-2147221164`
(class not registered). Wine separately reports the missing Shell Link COM
class `{00021401-0000-0000-c000-000000000046}`. Acknowledging the dialog makes
`BIOS_Install_CA` return zero and the parent installation roll back; the
runner exits 91. This is a Windows setup dependency, not a timer result or
evidence that a longer wait would succeed. Retain
`run_ccs33_oracle/full-install-long.log`, `bios-installer-error.png` and
`run_ccs33_oracle_full_long.log` for this boundary.

Registering Wine's supplied `shell32.dll` resolves that prerequisite; a
registry query verifies the Shell Link `InprocServer32` points to Wine's
`C:\windows\system32\shell32.dll`. The unchanged installer then passes
DSP/BIOS installation. Its XDS560 physical-debugger driver reports a DevCon
installation error under Wine; acknowledging that error lets setup reach
plugin registration, not proven target initialization.

The Component Manager path and installation boundary are now resolved.
The earlier `SETUPinstallPlugins` attempt used literal `%SystemDrive%` in
its registry path and was explicitly stopped, not completed or timed out.
Expanding the isolated prefix's `ProgramFilesDir` to its existing
`C:\Program Files (x86)` value lets the unchanged MSI exit zero and write
the correct Component Manager target. This is setup success, not simulator
success: its INI remains empty until the original plugin registrar runs.

The registrar needs Wine's legacy `msxml.dll` registration for COM class
`{2933BF90-7B36-11D2-B20E-00C04F983E60}`; registering `msxml3.dll` alone
does not supply that class. With the legacy class registered, the original
registrar writes a 41,745-byte INI containing C54x analysis plugins, but
exits 6 with an XML parse error. The installed IDE then displays
**A problem has been detected in the component manager database**. Its log
reported missing Wine Mono. Installing the official Wine Mono 10.4.1 MSI
does not clear the registrar's XML failure. Its SHA-256 is
`071f4b2887e1c97a11d791ff3d65be9429eed6dec4c2708888bfd546ba358e23`,
matching the value pinned in
[Wine 11.0's addon source](https://raw.githubusercontent.com/wine-mirror/wine/wine-11.0/dlls/appwiz.cpl/addons.c).

The IDE also reports RPC service startup failure; a registry query confirms
RpcSs is absent. Wine's standard `wineboot -u` restores that service, but
reveals a registration-view mismatch: the original Component Manager target
exists in the 64-bit view and is absent with `/reg:32`. The 32-bit registrar
then reports an empty path. Running the original MSI through
`C:\windows\syswow64\msiexec.exe` completes with exit zero, including
`SETUPinstallPlugins` return value 1. The `/reg:32` query now returns
`C:\Program Files (x86)\Common Files\Texas Instruments`; the original
installer generates a 43,030-byte component INI there. The earlier database
and empty-path warnings are superseded by this corrected installation, not
evidence of a permanent simulator incompatibility.

The current boundary is target automation initialization. Registering the
original `CCS_Scripting_Com.dll` with 32-bit `regsvr32.exe` makes its public
`CCS_Scripting_Com.CCS_Scripting` object respond with **CCStudio Scripting
Version 1.52**. Its documented `CCSOpen(0x54, 0, 0, 1, 0)` call then fails
with a CreateInstance automation error `0x80040154` rather than returning
a board/CPU identity. Wine also reports an unsupported interface on
`CodeComposerSetup.SystemSetup` (`{3620D2F0-F091-11D1-8BE4-0020182ABDCA}`);
the 32-bit class registration exists and points to the installed
`cc_setup.exe`. This does not establish that this class alone causes the
failure. No target instruction, timer fixture or pipeline mode has executed.

A COM activation trace identifies the requested application class as
`CodeComposer.Application` (`{DE4FA3E7-B47F-11D1-B009-0040055C1953}`),
registered in the 32-bit view to the installed `cc_app.exe`. Thus
`0x80040154` is not proof that its registry key is absent. Observing the
application during the actual call captures **Error reading Code Composer
Setup information**, not a usable target. The documented alternative
`CCSConfigClear`/`CCSConfigImport` sequence fails already at `CCSConfigClear`
with the same unsupported SystemSetup interface; it never reaches import of
the package's `drivers/import/c549sim.ccs`. This rules out an unnoticed
drag/drop dialog as a sufficient explanation for that attempt. The remaining
dependency is application-side setup activation, not a changed DSP fixture.
Direct `CreateObject("CodeComposerSetup.SystemSetup")` succeeds and reports
`ISystemSetup`. Its documented `Clear`, `Import` of the supplied
`c549sim.ccs`, and `SaveConfig` methods each return zero. Thus setup is not
universally unavailable, and the scripting convenience wrapper's failure
does not prove a missing board configuration. Opening the application after
that saved configuration still fails. The setup server's own `/RegServer`
entry point also exits zero without resolving application initialization.

A reversible isolated-prefix trial exported the original class registration
and removed only its default `InprocHandler32` entry. The application still
reports the same unsupported interface/failure; the original registration
was restored. Do not repeat that handler-entry trial or claim it establishes
a working simulator. No vendor binary or generated board database was
patched. A different compatibility environment or a working direct
application API is required before this route can provide timing evidence.

Retain `full-install-expanded-path.log`, `installed-ide.log`,
`installed-ide-screen.png`, `run_ccs33_oracle_plugin_msxml_legacy.log`,
and the earlier `full-install-shell-registered.log`, `xds-install-error.png`
and `plugin-install-error.png`. Additional evidence is `mono-install.log`,
`run_ccs33_oracle_plugin_with_mono.log`,
`run_ccs33_oracle_wineboot_repair.log` and `plugin-prefix-repair-error.png`.
Current verification evidence is `full-install-wow32.log` and
`run_ccs33_oracle_automation_registered.log`,
`run_ccs33_oracle_automation_com_trace.log.zst`,
`automation-observed-screen.png` and `run_ccs33_oracle_automation_import.log`.
The direct-API results are in `run_ccs33_oracle_system_setup_direct.log`,
`run_ccs33_oracle_system_setup_import.log` and
`run_ccs33_oracle_automation_direct_config.log`; the rejected handler trial
is `run_ccs33_oracle_automation_without_handler.log` with registration backup
`system-setup-original.reg`.
The verbose COM trace is also retained uncompressed. Resolve the remaining COM
initialization boundary using the vendor tools; do not edit the generated
component database or interpret copied files as a working simulator.

The acquired vendor SDK's `CCDspUser.h` defines `GetSimMode()` and, under
`USE_TI_INTERNAL_APIs`, `SetSimMode(long)`: mode 0 flushes the pipeline as an
emulator, mode 1 does not. Its comments explicitly restrict support to
certain simulators and require querying the mode to verify it. These are
automation leads only; neither mode has been selected or measured here.

The package-owned MFC42/Xerces directories in `WINEPATH`, a working software
display and acknowledgement of the drag/drop registration warning allow
`cc_setup.exe` to reach its real factory-board UI. The C54xx list contains
`C549 Device Simulator`; adding it displays the Texas Instruments C54x
simulator CPU with `drivers/tisim54x.dvr`. Save-and-launch starts `cc_app.exe`
and `ccsmonitor.exe`. The missing Component Manager path was resolved by
the correct 32-bit installation above. No simulator initialization, pipeline-mode
selection or timer result is proved by these UI observations.

Retained setup evidence is `run_ccs33_oracle/admin-install.log`,
`full-install.log`, `setup-observed.log`, `setup-windows.log` and
`setup-screen.png`, plus `run_ccs33_oracle_setup_display_fixed.log` and
`run_ccs33_oracle_launch_c549.log`. The next dependency is functioning legacy
installation/component registration and automation, not a changed Nokia
fixture or an invented interrupt delay. The Wine log also reports missing
Mono for a launched runtime component; it does not prove Mono alone prevents
the C54x driver from running.
TI's [C5400 scripting support answer](https://e2e.ti.com/support/tools/code-composer-studio-group/ccs/f/code-composer-studio-forum/1026307/code-composer-c5400-debugging-with-dss-api)
also rules out a modern DSS-based shortcut: C54x simulation ends at CCS
3.3, whereas DSS begins at CCS 4. TI explicitly states that no version
supports both. An acquired legacy simulator therefore needs its original
automation interface, not a modern target-configuration transplant.
TI's [C54x design workshop](https://e2echina.ti.com/cfs-file/__key/telligent-evolution-components-attachments/00-120-00-00-00-00-18-64/TI-TMS320C54x-DSP-_BE8BA18B0763FC5B_.pdf)
documents GEL-based simulator initialization as a legacy automation lead;
it is not an acquired engine or an executed differential test. The separate
[CODECOMPOSER product](https://www.ti.com/tool/CODECOMPOSER) page concerns
older C2x/C5x/C3x/C4x products and offers only a C3x/C4x service pack,
not a replacement C54x simulator download.
Two publicly accessible alternatives do not cover this boundary:
[gDSPsim 0.30](https://sourceforge.net/projects/gdspsim/files/gdspsim/gdspsim-0.30/)
has a staged C54x pipeline and software INTR implementation, but the inspected
C54x/common source has only register-display uses of IMR/IFR and no hardware
timer or maskable-request delivery implementation. Its source RPM SHA-256 is
`a57eda4aa6402aba0439146ddcc60ff8665ed1d116b82d4c2158e1d211b1bb7b`;
the supplied GUI build requires unavailable legacy `gnome`/`gtk+`
dependencies. The repository-owned adapter in `tools/gdspsim_reference/`
instead compiles the original execution sources unchanged with modern GLib,
opaque GTK widget types and no-op display callbacks. It preserves the
original memory preference defaults and aborts on file-I/O execution.
No external execution code is imported into the MAME core.

Reproduce the bounded comparison with:

```sh
.venv/bin/python tools/gdspsim_reference/build.py \
  /path/to/gdspsim-0.30 run_gdspsim_reference
```

The NOP fixture advances PC from `0100` to `0120` in 32 pipeline steps,
leaving ST1 `2900`. The XF fixture places `f6bd` at `0100` and `f7bd` at
`0108`; ST1 becomes `0900` at step 6 and `2900` at step 14. These status
results agree with TI's bit-13 contract and the native core's XF fixtures.
The five-step publication delay describes this simulator's pipeline, not
a Nokia device latency or independently established MAME cycle correction.
The runner rejects the reference's unknown-opcode-to-NOP fallback and
retains each fixture's output in the isolated build directory. Only these
NOP and XF fixtures agree with the architectural expectations; neither
establishes general arithmetic or hardware IRQ timing. A separate `INTR 16` probe executes
`f7d0` at `0100`, with PMST `2000`, SP `1000`, IMR zero and IFR `ffff`.
The original pipeline sets INTM and reaches the `2040` vector, but stacks
`0102` instead of the next-instruction address `0101`, and leaves IFR
`ffff` instead of clearing the selected bit to `fffe`. These are measured
reference discrepancies, not desired native-core behavior. The runner
explicitly reproduces and reports them; its PASS means reproduction, not
INTR conformance. The native core's existing 64 INTR cases check next-PC
stacking and selected-IFR clearing against SPRU172C. This reference cannot
substitute for the RTN contract either: SPRU131G example 7-6 explicitly
loads RTN during software INTR. Each native case now continues through
RETF after replacing the stacked word with a sentinel, proving the saved
RTN supplies the original next PC while SP balances and INTM clears.
Ordinary near `CALL pmad` also publishes RTN, per SPRU131G section 7.1.2,
example 7-4. The executable page-2 case calls a paused callee, checks the
stacked continuation, replaces it with `dead`, and executes `RETF`; return
must use RTN while balancing SP and preserving XPC. It failed on the former
stack-only CALL implementation and passes with RTN assigned before the push
(`run_c54x_call_rtn_red`, `run_c54x_call_rtn_green`). The four-cycle CALL cost
is unchanged. This establishes the non-delayed immediate CALL contract, not
delayed/conditional-call RTN behavior or interrupt-pipeline timing.
The same executable case now saves the paused callee with a sentinel stack
word, completes its fast return, then executes another CALL on page three
with a different continuation. Restoring the snapshot must recover page two,
the sentinel and the original RTN: a second RETF reaches the same original
continuation with balanced SP. This validates restoration of the otherwise
inaccessible return register after real architectural overwrite, rather than
merely checking its save-item declaration (`run_c54x_call_rtn_restore.log`).
The full executable core suite and tool suite pass. The post-change ROM4
RF-boundary integration check also passes: 207,040 RF reads, three port-32
writes, and no port-38/39 reads. This preserves the known native no-cell
boundary, not completed radio operation. The initial invocation failed
during fixture generation under `/tmp`; rerunning with workspace `TMPDIR`
resolved that storage failure without changing the fixture or acceptance
criteria (`run_c54x_call_rtn_rom4_workspace.log`).

Adjacent call forms remain a separate conformance question. `CALA`,
`CALAD` and `CALLD` currently push their continuation without publishing
RTN; their existing executable cases test target, stack and delay slots,
not fast return through RTN. Do not extend the immediate-CALL fix by
analogy: [SPRU131G example 7-5](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
labels delayed-call RTN as `a3`, the first delay-slot address, whereas
[SPRU172C pages 4-27/4-28](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)
explicitly stack instruction-PC plus four (after both delay words).
The stack continuation is unambiguous; whether that diagram mislabels
RTN or describes a distinct fast-return value is not settled by these
sources. Resolve that distinction before asserting a CALLD-to-RETF
expectation or changing its RTN behavior. The non-delayed immediate CALL
regression does not close accumulator, conditional or far-call coverage.
An isolated 30-second native 5110 run with passive `-verbose` opcode
observation executes `f074` CALL 103,727 times (first PC `0f15`) and `f274`
CALLD 6,502 times (first PC `0db3`). No `f4e3`/`f5e3` CALA or
`f6e3`/`f7e3` CALAD is observed in that window; this is not a static absence
claim. The same run passes the RF-boundary checker with the unchanged
207,040 reads and three port-32 writes. Its raw evidence is retained in
`run_rom4_call_inventory_verbose/error.log`. CALLD therefore has a measured
native workload, but execution frequency cannot resolve the RTN ambiguity
or prove its fast-return contract.

Software INTR publishes RTN just as hardware interrupt entry already does;
the existing three-cycle cost is unchanged. These are 64 architectural
return assertions, not a pipeline-latency oracle. The external reference cannot
adjudicate the outstanding interrupt-pipeline question without separate
validation. Adding our own timer/request model would remove the
independence needed to validate that model.

A bounded arithmetic comparison executes `2883` (`MAC *AR3,A`) with
T/source `1`, A `007fffffff` and ST0.C set. With OVM clear, the reference
agrees on A `0080000000`, sticky OVA and unchanged C/T/AR3. With OVM set,
it still publishes `0080000000`, rather than the TI-defined saturation
result `007fffffff`. The native core's existing MAC overflow fixtures
independently pass both outcomes against the primary contract. The runner
reports and reproduces the reference's OVM discrepancy without patching
its execution engine. Its PASS is not an arithmetic-conformance claim;
the simulator is not a drop-in oracle for status-mode behavior.
[DSPsim](https://mpcomplete.org/proj/dspsim/) instead implements a C audio
interface, not instruction execution. Neither is a differential oracle for
the short-period timer probe. The TI legacy simulator remains an acquisition
lead, not evidence that the current outcome is correct.
TI's public [CCS 3.3 service-release index](https://software-dl.ti.com/ccs/non-esd/releases/CCSv3/CCSv3.3/CCSv3.3ServiceReleases.htm)
still serves the SR12 update archive
`SR12_CCS_v3.3_SR_3.3.82.13.exe` (135,946,254 bytes; SHA-256
`a05ce0fae2485869ac55aaeb00b3a466fbdde24fe599e9db8279cce4b036bd33`).
The acquired archive is retained locally under ignored `run_ccs33_oracle/`.
Static extraction with 7-Zip and the distribution's `unshield` reader exposes
1,677 nested payload files. `drivers/import/ccs5400sim.txt` describes C5402
and C5410 device simulators with cycle-accurate CPU and Timer0 support;
C5402 additionally lists Timer1. These are vendor capability declarations,
not measurements of the short-period probe. The manifest contains C54x GEL
files, compiler interface `bti5400.dll` and hardware-emulator drivers, but
none of the C54x `.ccs` simulator configurations named by that table and no
identifiable C54x simulator driver. Do not mistake the XDS drivers or the
compiler interface for the simulator engine. Static PE inspection of the
included `cc_app.exe` also identifies vendor runtime dependencies, including
`CWDLL32.dll`, `CWSTART.dll` and `TISCC.dll`.
The index explicitly requires a previous CCS 3.3 installation. This update
therefore supplies useful capability/configuration evidence, not a standalone
timing oracle; acquisition still targets the base installation and its C54x
simulator component. No installer has been run or license accepted.
The official base-package referral linked by TI's
[CCS download support response](https://e2e.ti.com/support/processors-group/processors/f/processors-forum/1213508/ccstudio-ccs-compile-error)
redirects an unauthenticated request to the myTI login page. Acquisition via
that route requires user account access; no credentials or export/license
forms have been submitted. This is an observed access boundary, not evidence
that no legitimate independent copy exists.
TI's surviving [evaluation-tools landing page](https://www.ti.com/sc/eur)
also links a C5000 free-tools referral at
`https://www.ti.com/sc/docs/tools/dsp/c5000freetools.html`. An unauthenticated
HTTP check follows that referral to the current DSP product overview, not an
evaluation installer. A successful final HTTP 200 therefore does not establish
package availability. The pre-CCS standalone simulator remains a distinct
acquisition lead: TI's [SPRU302B reference](https://www.ti.com/lit/ug/spru302b/spru302b.pdf)
identifies the C54x Simulator Getting Started manual (SPRU137), with DOS and
Unix installations. That reference establishes historical software availability,
not timer/pipeline accuracy or a presently obtainable binary. Neither referral
provides a runnable differential oracle for the current timer probe.
The acquired [SPRU099D debugger guide](https://www.ti.com/lit/ug/spru099d/spru099d.pdf)
narrows this lead: sections 2.5/4.11 list timer simulation for `-mv540`
through `-mv549`; section 8.8 exposes pipeline opcode/address pseudoregisters
`p_ins/p_add`, `f_ins/f_add`, `d_ins/d_add`, `a_ins/a_add`, `r_ins/r_add`
and `x_ins/x_add`. It places ARAU updates in operand-access-1. A comparison
must therefore record Decode acceptance, auxiliary-register visibility and
Execute timing separately, not infer them from the final stacked PC. This
is a documented observation interface, not an acquired executable or measured
timer result. The 419-page PDF is retained at
`roms/reference-docs/c54x/ti_tms320c54x_debugger_spru099d.pdf`, SHA-256
`20e6c1573d959f3752d9f32187bd6119dee2c4a848b020af66ddbf23f685c909`.
TI's [C54x family datasheet](https://www.ti.com/lit/gpn/tms320lc543),
development-support table 14, identifies the standalone simulator products
as `TMDS324L851-02` (PC DOS/Windows) and `TMDS324L551-09` (SPARC).
These are acquisition identifiers, not the similarly named assembler/linker
or XDS hardware-debugger packages. Searches by these identifiers and by
`sim54x` have not yielded a runnable distribution; this is a bounded search
result, not proof that the software is unavailable.
The GNU-toolchain lead also needs this distinction: the author's
[BFD page-support discussion](https://sourceware.org/pipermail/binutils/2000-January/003156.html)
describes remote-debugger address-space encoding and assembler/linker work.
It does not establish an instruction simulator, timer implementation or
pipeline oracle. A C54x target name in binutils/GDB is insufficient evidence
for any of those capabilities. No GNU simulator result is used to validate
the short-period interrupt probe.
CALLD and RETD likewise use two and three cycles respectively, rather than
four each. A cycle-stamped call/return fixture checks both delay pairs and
stack balance against SPRU172C pages 4-27 and 4-139. CALA/CALAD retain their
documented six/four-cycle costs.
`XC 2` now charges the two rejected instruction words as NOP slots. Its
interrupt guard retires by instruction words, so a single two-word guarded
instruction releases a pending IRQ before the next instruction. Exact ROM4
`ff4d` fixtures cover taken/rejected BEQ timing, and a port/ISR fixture
checks the two-word guard boundary (SPRU172C, XC pages 4-198--200).
The long-immediate ALU decoder charges two cycles, including the separately
decoded XOR form. A cycle-stamped LD fixture checks the additional cycle;
the LD contract is SPRU172C page 4-68. This does not cover all extended-address
or repeated multicycle instruction timing.
The current HINT/absolute-compare model starts fresh-profile port-27 reads
on frame 30. The 30-second observation has 6,499 frame expiries and 207,040
reads (`32 * (6499 - 29)`), terminal IMR `035f`, IFR zero and no burst-port
activity. The gate pins that first-read frame, the three ordered RF control
pairs above and the 32-reads-per-frame cadence, allowing only the bounded
in-flight frame count at the fixed-time cutoff. The older frame-29,
zero-control-write oracle described the superseded timer boundary.
The twelve-second verbose MCU trace contains a type-`0x1a` search-list
publication at 1.511395 s (`00109800...`, 68 payload bytes). The seven
type-`0x51` packets at 2.065--2.071 s are segmented command-`0x22` DSP memory
uploads: their destination words advance from `0x2286` through `0x2370`.
Calling them radio configuration was wrong. The search-list publication proves
an MCU-side search request, but neither it nor the current MAD2 register map
establishes an SCU synthesizer write or a request that enters `0x7b0a`.
A conservative NSE-1 swap16 literal-seeded MMIO census resolved 562 direct
accesses from 235 seeds; all resolved offsets are below `0x40`. This does not
exclude dynamic/table-derived accesses or identify the SCU register. The
coherent first-access ledger likewise shows no offset above `0x3f` by 12 s.
Neither `0x27` nor `0x39` is established as the complete FCCH/SCH sample
stream. The sibling emulator supplies only a constant for port `0x27`, so it
offers no independent sample-format evidence. No valid signal fixture follows yet.

A focused consumer trace under the earlier relative-compare timer model
closes the direct type-`0x1a` activation hypothesis for that measured lifecycle.
The resident host-command dispatcher advances the transmit-ring consumer at
C54x PC `0x3909`. The selected handler spans `0x3d70..0x3db6`: it derives
local value `0x0010`, copies the payload into scratch words `0x1200..0x121f`,
updates control
words `0x0284/0x0286/0x0287`, sets bit 3 at `0x06bc`, and increments `0x06e3`
from zero to one before returning. It performs no I/O-port access. Across the
same 30-second receiver gate, none of those identified control words is read
after the handler returns; the existing port-`0x27` cadence continues and the
`0x32/0x38/0x39` counts remain zero in that earlier run. The current absolute-
compare gate records three port-`0x32` writes; the old zero count is not a
current invariant. Temporary write/read taps used for this
classification were removed. Thus the observed search-list packet is accepted
and stored, but does not by itself enter the dormant parallel receive path or
establish an RF acquisition. Which lifecycle consumes the stored control state
and which DSP path owns ordinary acquisition remain unresolved. Separately,
the `0x7b0a` mode initializer remains useful for later dedicated-channel work;
injecting a reply or waveform at type-`0x1a` would skip both boundaries.

The earlier relative-compare post-handler reader census covers `0x1200..0x121f` and the complete
`0x0284..0x02b0` control/vector interval: 77 DSP data words. Temporary
read/write taps at the backend data bus observed zero accesses to those words
from 1.52 seconds through the end of a coherent 30-second no-cell run. These
taps see indirect and table-derived addresses as well as literal accesses;
they do not cover another lifecycle or unexecuted ROM paths. The RF gate
still recorded 6,497 frames and 207,232 sample reads. The taps were removed.
The original `0x1219` bound was too short: the handler stores one word and
repeats the payload copy 31 times, reaching `0x121f`.

Static word scanning finds 95 occurrences of literal `0x1200`, 28 of
`0x0284`, five of `0x0286`, and twelve of `0x0287`. These are candidate
references, not a complete decoded call graph. `0x1200` is also used by host
packet construction and memory-upload handlers, so its address alone does
not identify persistent search-list ownership. Candidate control consumers
include the `0x0926/0x0974` comparisons and the `0x6bxx/0x77xx/0x7axx`
mode routines; low program addresses additionally require overlay validation
before their raw-ROM instructions can be treated as the executing code.
No active consumer or missing emulated activation input follows from this
census. It does not distinguish an inactive command lifecycle from
signal-dependent activation. The next discriminating evidence is the
no-cell/carrier capture specified below, not a synthesized host reply or
forced dormant mode.

The next evidence should compare a no-cell boot with a real NSE-1 receiving
one known GSM-900 test carrier. Capture the ordered MCU-to-DSP request and
DSP-to-MCU response words, DSP program counter around `0x407c`, `0x410e`,
`0x4185`, `0x4414`, `0x3268..0x32c3` and `0x7b0a`, and DSP data words
`0x00ac/0x00ad/0x00af/0x1949/0x1973` before and after each INT0 frame.
Capture reads/writes
of DSP I/O ports `0x27`, `0x38`,
`0x39`, `0x31` and `0x32` with timestamps. To establish electrical sample
packing and tuning, also capture the COBBA parallel address/data, read/write
and data-available strobes and MAD2 `SynthEna/SynthClk/SynthData` pins (or
equivalent DSP/MMIO instrumentation). Record the 13 MHz reference and TDMA
frame edges so data order and latency can be aligned. An HPI RAM snapshot
alone cannot establish DSP I/O-port traffic or 12-bit bus sign extension.
The pin names and bus width come from Nokia's NSE-1 service manual cited
above; the requested trace contents are an experiment specification, not a
claim that any particular request or encoding has been recovered.

TI's C54x CPU guide places an important limit on this result: hardware reset
clears IFR and sets INTM, but does not initialise IMR or SP. The clean core
preserves IMR across MAD2 reset pulses. The recovered ROM's OR-mask sequence,
its INT0 receiver vector and the one-bit A/B result establish a product cold
entry value of `0x0001`; firmware then produces `0x0205` and `0x035f`. The available
external co-sim instead starts from `IMR=0x52fd`, an imported post-handshake
processor snapshot rather than a ROM4 power-on capture, and consequently
reaches `0x53ff`, executes `0x3065` (`IMR |= 1`), services INT0 at `0x3204`,
and reads port `0x27`. An A/B run with its optional RF model disabled still
takes that path with zero-valued samples, proving that the RF model supplies
sample contents rather than activation. It does not prove that the unrelated
bits in `0x52fd` belong to NSE-1.

An aligned-word census closes the literal variant of that question. Neither
the recovered `dsp_full.bin` nor the transform-entry program snapshot contains
an aligned big-endian word `0x52fd` or `0x53ff`. The program image contains four
aligned `0x0204` words and no aligned `0x035a`; runtime attribution identifies
the relevant OR-mask operations rather than assigning semantics to every
literal match. The transform-entry data snapshot likewise contains no aligned
`0x52fd`; its two aligned `0x53ff` words are captured data, not register
provenance. The reference implementation's own source describes `0x52fd` as a
post-bootloader Osmocom/Calypso register snapshot. It is therefore cross-silicon
differential evidence, not a Nokia MAD2 reset contract.

`make check-c54x-rom4-coherent` protects the short coherent boot and interface
initialization. `make check-c54x-rom4-rf-boundary` separately runs for 30
emulated seconds and protects the newly reached receiver boundary: it requires
at least 6,000 CTSI frame expiries, terminal `IMR=0x035f`, serviced INT0, more
than 1,000 RF reads and no port-`0x32`, `0x38` or `0x39` activity. This is a
receiver-activity gate, not a synthesizer-tuning or acquisition gate. With
the current evidence, a generated FCCH signal would require guessing both
the active receive interface and the COBBA sample representation; neither
is admitted as a hardware model.

`make verify-5110-save-state` saves the running real-DSP composition at seven
seconds, restores it, verifies MAD2 and C54x idle state, and then opens the same
Phone book menu through a physical key transition. This guards the C54x core,
uploaded program/data overlays, DSPIF, COBBA, timers, and keypad composition
against state-registration regressions.
The core fixture additionally saves during a 65,536-iteration ADD repeat with
IRQ2 pending, then compares original and restored completion (accumulator,
ISR observation and continuation PC). Its DSPIF instance contains two queued
RX packets; loading restores all 2,048 shared words, including payloads and
ring cursors, and the interface register after deliberate fixture disturbance.
The internal timer is stopped by an STM instruction in this fixture to isolate
repeat/IRQ replay from unrelated timer wakeups. This is executable state
restoration evidence, not just a source-level save-registration check.

The extended-program core fixture also saves between the two delay words of
`FCALLD` and `FRETD`. It completes each transfer, deliberately disturbs the
page, registers and stack, restores the snapshot, and compares replay with
uninterrupted execution. Distinct program-page contents check the pending XPC
transfer as well as the return PC, stack order and delay-slot effects. The
checkpoint is a scheduler boundary after the first operand instruction retires,
not a save from inside that instruction. This validates generic core state;
it does not establish the DA150 memory map or a working 5510 music subsystem.

The historical harness's headline `74 acknowledgements` counter is not a count
of DSP port-1 completion strobes. The local gate separately records shared
mailbox writes and completion strobes; their counts depend on the observation
window and repeated firmware polling. The external counter was attached to
intercepted MCU mailbox writes with different boot-phase lifetime rules. Keep
these as separately named measurements; do not tune the local transport merely
to make the integers equal.

An older persisted donor EEPROM produces the distinct `c9f4 cd44 ... 6075`
challenge and the structurally valid but rejected `3532 0000 312b ... 88b2
0000` response. That run repeatedly requests a reason-4 reset and is a negative
control, not a coherent-boot result. The coherent gate therefore regenerates
the external 24C16 profile and uses a fresh NVRAM directory before requiring
both the C54x completion and the MCU validator's `r6=1` continuation. Resident
slot `0x250b` is still an organically uploaded no-op; the recovered acquisition
path is the later COBBA serial-register read rather than a missing overlay at
that slot. No response field is synthesized.

The NSE-1 external EEPROM participates in this transaction. Firmware organically reads
its board-level 24C16 through PUP GenIO (SDA bit 0, SCL bit 2). A virgin repair
image generates a different challenge; a provisioned image generates the exact
known-good challenge. Runs must use a fresh NVRAM directory when changing the
ROM seed because MAME correctly persists the device contents.

The ROM4 data map also contains read-only dispatcher entries at offsets ending
in `0x07` across `0x9000..0xdfff`. A live run proved code at `0x37fc` otherwise
attempts to overwrite `0xb707` immediately before the challenge. The backend now treats
those mask-ROM writes as no-ops. This prevents later dispatcher corruption but
does not by itself correct the first challenge's task selection.
COBBA control ports `0x2c`/`0x2d` and codec serial port `0x21` now terminate in
the COBBA device. PCM sample timing remains open.

The core corrections required to reach this point are generic C54x semantics:
absolute `STM` extension order, `MVDM`, `DLD`/`DST`, `CMPL`, immediate `XOR`,
compound `XC`, all `IDLE` and ST0/ST1 bit-set/reset variants, `RPTZ`, and
memory-counted repeat of multiword instructions, repeated `MVDP` and `MVDM`
destination update, sign-extended shifted loads, carry rotations, delayed and
conditional control flow, interrupt return,
extended absolute arithmetic/load/store, and accumulator-indirect branch.
Operational ROM execution has additionally established signed/unsigned
accumulator arithmetic, accumulator-shift-mode loads, memory compare,
accumulator-addressed program writes, stack data pushes, conditional
branch/call/return families, signed `FRAME`, immediate cross-accumulator ALU,
and dual-memory moves. Focused core tests cover selected encodings in these
families; execution alone does not establish every variant's semantics. None
recognizes a Nokia address or loader byte pattern.

## Executed-opcode coverage

### Debugger control-flow decoding

The native core disassembler decodes the reviewed branch/call/return,
repeat, idle, interrupt and status-bit forms, including extended accumulator
branches and calls. `check-c54x-core` checks 46
exact encodings at two program addresses, including an extension word crossing
the 16-bit page boundary. Near targets retain their program page; far targets
carry their explicit extended address. The native 5110 Phone book menu gate
also passes with this decoder enabled.

This is bounded debugger coverage, not a complete ISA disassembler or new
execution/timing evidence. Other forms remain unsupported raw words.
Non-delayed calls and returns carry debugger stepping flags. Delayed calls
and returns deliberately omit those flags: C54x delay slots count words,
whereas MAME's extra-step count counts instructions, so blindly requesting
two extra instructions would be wrong for a two-word slot instruction.
Encodings and delay-slot rules follow the
[TI C54x instruction reference](https://www.ti.com/lit/ug/spru172c/spru172c.pdf).

`tools/c54x_opcode_coverage.py` compares `[opcov]` records from a verbose
30-second 5110 v5.30 run with the standalone `tms54test` fixture. A fresh native
idle run dispatches 594 distinct words in 107 high-byte groups (set SHA-256
`5ec81f25976d365d2bbfe09037d70ac44676b5803535892d8237744c6a167d4e`).
Against the executable fixture, all 594 are asserted, none execute without an
assertion, and none are absent; `--require-all-asserted` passes. The independent RF-boundary checker
passes with 6,499 CTSI frames, 207,040 port-`0x27` reads and three port-`0x32`
writes; this is not acquisition or speech evidence. Exact `5086` checks
stationary AR6 double-word ADD into A using absolute port markers independent
of AR6, distinct AR3 source data, preserved B/status/source/pointers,
continuation PC and one-cycle execution.
Exact `f3e6` checks B logical shift by six with guard clearing and outgoing
bit-26 carry; exact `f640` checks zero-shift B-to-A load with guard preservation.
Both preserve the unrelated accumulator/TC/ST1, with the zero-shift load
preserving carry, and execute in one cycle.
Exact `f78f` checks B-to-B normalization left by 19/right by seven from TI
example values, signed six-bit T interpretation, preserved A/T/carry/status
and one-cycle execution.
Exact `f677`, `f762`, `f767`, `f77c` and `f77e` check B-source shifts by
-9, +2, +7, -4 and -2, including cross-accumulator destination selection,
SXM clear/set right shifts, shifted-out carry, preserved TC/unrelated
accumulator/ST1 and one-cycle execution.
Exact `f462`, `f47c`, `f47e` and `f57a` check A-source shifts by +2, -4,
-2 and -6, including cross-accumulator destination selection, SXM clear/set
right shifts, shifted-out carry, preserved TC/unrelated accumulator/ST1 and
one-cycle execution.
Exact `808c` and `80d2` check low A stores before AR4 decrement/AR2 circular
wrap, preserved neighbors/accumulators/status, unchanged AR3, continuation PC
and one-cycle execution.
Exact `82d2` and `83d2` check A/B high-word stores at the old AR2 address
before circular wrap, preserved neighbors/accumulators/status, unchanged AR3
and one-cycle execution between absolute port markers.
Exact `7184` checks stationary AR4-to-immediate-data-address transfer,
preserved source/neighbors/A/B/status/pointer, extension-complete continuation
PC and two-cycle execution.
Exact `6b84` and `6b8c` check stationary/decrementing AR4 immediate memory
adds using the TI negative-overflow example: saturated stored result, bit-32
carry plus overflow, preserved A/B/neighbors/ST1, write before pointer update,
extension-complete continuation PC and two-cycle execution.
Exact `6885` and `6985` check AR5 immediate AND/OR memory updates against a
distinct AR3 word, preserved neighbors/A/B/status/pointers, extension-complete
continuation PC and two-cycle execution.
Exact `5685` and `56d3` check stationary AR5 and circular AR3 double-word
loads into A with SXM clear/set, preserved B/status, long-pointer wrap and
continuation PC. One-cycle DLD gives a four-cycle delta with the absolute
port-write markers used by these fixtures.
Exact `5385` checks double-word ADD into B through stationary AR5 with C16
clear/set, distinct AR3 data, preserved A/status/pointers/source memory and
one-cycle DARAM arithmetic; C16 prevents carry crossing between the halves.
Exact `5082` and `5085` check double-word ADD from stationary AR2/AR5 into A,
using distinct AR3 data to verify source selection, preserved B/status/source
memory and pointers, and one-cycle DARAM arithmetic between port markers.
Exact `4e82`, `4e95` and `4f8d` check stationary A stores through AR2,
incrementing A stores through AR5 and decrementing B stores through AR5.
The checks cover high/low placement (including odd addresses), long-pointer
updates by two, preserved neighbors/A/B/status, continuation PC and two-cycle
execution.
Exact `4488` checks shifted loads into A with SXM clear/set, decrement after
reading AR0, preserved B/status/source, unchanged AR2/AR3 and one-cycle
execution.
Exact `308a` checks loading T from the old AR2 address before decrement,
with a distinct AR3 source, preserved A/B/carry/source memory and one-cycle
execution.
Exact `2882` checks stationary AR2 MAC into A with signed factors and FRCT
clear/set; exact `2d92` checks MAS into B and increment after reading AR2.
Both preserve the unrelated accumulator, T, carry and ST1, leave AR3 unchanged,
and execute in one cycle.
Exact `2082` and `218a` check signed T-times-memory multiplication into A/B,
preserved unrelated accumulator/T/carry, stationary AR2 versus decrement
after reading its old address, unchanged AR3 and one-cycle execution.
Exact `01f8` checks absolute ADD into B with both SXM settings, preserved
A/TC/source, carry clear, continuation PC and two-cycle execution. Exact
`1080` checks stationary AR0-to-A loading against a distinct AR1 source,
both SXM settings, unchanged pointers/B/T/status and one-cycle execution.
Exact `f930` checks TC-clear/set conditional calls, unchanged stack sentinel
when rejected, extension-complete return PC pushed when taken, preserved
A/B/status, and three/five-cycle call costs. Exact `ff44` checks ANEQ for
zero and guard-bit-only nonzero A, preserved A/B/status, both guarded MAR
slots, continuation PC and equal accepted/rejected cycle cost.
Exact `f684`/`f784` check B-source negation into A/B for negative and zero
inputs plus signed 32-bit overflow with OVM clear/set: preserved unrelated
accumulator/TC, zero-only carry, destination overflow flags, continuation PC
and one-cycle execution. Exact `e5e9` checks copying old AR4 before AR0-step
circular wrap, independent AR3 increment, preserved source/accumulators/
status and one-cycle execution.
Exact `f477`/`f67d`/`f77a` check arithmetic shifts by -9/-3/-6 with both
SXM settings, signed/logical guard-bit fill, outgoing-bit carry, preserved
TC/source accumulator, and one-cycle execution. Exact `f561`/`f766` check
A-to-B left shift by one and B-to-B left shift by six, preserved unrelated
accumulator/TC, cleared outgoing-bit carry and one-cycle execution.
Exact `8180` checks B-low storage through stationary AR0, unchanged
accumulators/status/pointers/adjacent memory and one-cycle execution.
Exact `7708` checks immediate replacement of AL while retaining A's high
and guard bits, unchanged B/status, continuation PC and two-cycle execution.
Exact `8292` checks A-high storage at old AR2 before increment, preserved
accumulators/status/adjacent memory and one-cycle execution.
Exact `7483` checks stationary AR3 port-input selection, untouched adjacent
cells/pointers/accumulators/status, continuation PC and two-cycle transfers.
Exact `758a` checks old-AR2 output before post-decrement, preserved source
and unrelated pointers, and two-cycle execution. Exact `3c82` checks signed
shifted ADD from stationary AR2 against a distinct AR3 word, preserved B/T,
retained set carry and one-cycle execution.
Exact `ff4f` checks two-slot BLEQ with negative, zero and positive 40-bit B
values (including guard-bit-only sign differences), preserved A/B/status,
continuation PC and equal accepted/rejected cycle cost. Exact `4582` checks
stationary shifted AR2-to-B loads with both SXM settings, unchanged pointer,
preserved A/status/source and one-cycle execution.
Exact `7088` checks copying the source word to old AR0 before decrement,
preserved source/unrelated pointers and two-cycle extension-consuming
execution. Exact `720b` checks replacing B's low word while retaining its
high and guard bits, preserved A/status/AR3/source, continuation PC and
two-cycle memory-to-MMR transfer.
Exact `4082` checks stationary AR2 source selection against a distinct AR3
word, shifted subtraction into A, retained set carry on no borrow, preserved
B/T/source and one-cycle execution. Exact `4f90` checks B high/low word
ordering through AR0, two-word post-increment, unchanged adjacent cells,
unrelated pointers/accumulators/status, continuation PC and two-cycle store.
Exact `4911` checks AR1-to-B selection against a distinct AR2 value,
zero-extension with SXM set, preserved A/status/source registers and
one-cycle execution. Exact `f689` checks rounded MACAR with signed old-A
high word as multiplicand and old B as accumulation source; FRCT clear/set
straddle the rounding boundary, while B/T/status remain unchanged.
Exact `7690` checks immediate-word consumption, old-AR0 destination and
single post-increment, untouched neighboring cells/AR3/arithmetic state,
continuation PC and two-cycle execution. Exact `f500`/`f600` check opposite
accumulator ADD at positive signed overflow, destination-specific overflow
flags, cleared carry with TC retained, preserved source/T, OVM clear/set
results and one-cycle execution.
The short-immediate load fixture asserts all 512 `e800..e9ff` encodings
with SXM clear and set: unsigned byte values, independent A/B destination,
preserved other accumulator/status, continuation PC and one-cycle loads.
This closes the formerly executed-only `e809`; it does not establish
long-immediate or memory-load behavior, which have separate fixtures.
Exact `f5bc`/`f4bc` check setting/clearing ST0.TC while preserving carry
and ST1. Exact `f6b8`/`f7b8` check clearing/setting ST1.SXM while preserving
OVM and ST0. All four check one-cycle execution between port markers.
Exact `7595`/`7592` check independent AR5/AR2 port-write selection using
distinct source values, pre-increment output, one pointer increment,
preserved source/unrelated pointers and two-cycle execution.
Exact `fd20` checks both TC outcomes for single-slot conditional execution,
unchanged status/B, continuation PC and equal accepted/rejected cycle cost.
Exact `6e82` checks zero/nonzero AR2 without modification, both executed
delay words, distinct fallthrough/taken continuations, preserved B/AR7 and
the two-cycle delayed branch cost.
Exact `8090`/`8190`/`8191` check low-word A/B selection with distinct
accumulator values, AR0/AR1 destination selection and post-increment,
preserved accumulators and unrelated pointers, and one-cycle stores.
Exact `0881` now checks SUB through AR1 with both SXM settings, 32-bit
no-borrow carry, unchanged B/source/pointers and one-cycle arithmetic.
Exact `e4e9` checks loading T while storing old A, independent Y increment,
circular X wrap, unchanged accumulators/status and one-cycle execution.
Exact `d6e9` checks the signed rounded MAC into A, old-B store, unchanged
B/T/source and independent circular-X and incrementing-Y updates.
Exact `4392` checks shifted SUB B-to-B, preserved A/T/source, AR2
post-increment, retained cleared carry on no borrow, and one-cycle execution.
Exact `e245`/`e249` check SQDST's signed X-Y difference and old-A-high square
accumulation into B, preserved T/source cells, and independent decrementing X
with decrementing/incrementing Y. The two fixtures separately exercise FRCT
clear/set, including the doubled square in fractional mode.
Exact `6183` checks both BITF TC outcomes, preservation of other ST0 bits,
accumulators, pointer and source memory, and two-cycle execution with its
mask extension word. Exact `3c92`/`3f92` check shifted ADD routing into A/B
and AR2 post-increment; `3f82` checks the non-modifying AR2 form. All three
check T, the unaffected accumulator, retained carry and one-cycle arithmetic.
The immediate-repeat fixture asserts every `ec00..ecff` encoding: `K+1`
iterations, exact AR1 advancement, continuation PC, one setup cycle and one
cycle per repeated MAR instruction. This includes the newly observed `ec0d`
and the previously executed-only `ec02`. It does not establish all repeatable
instruction classes or interrupt behavior; those have separate fixtures.
Exact `4592` checks signed/unsigned shifted loads into B, unchanged A/status,
AR2 post-increment, preserved source and one-cycle execution. Exact
`82d3`/`83d3` check A/B high-word stores before four-word circular wrap,
unchanged accumulators/status and the established store cycle cost.
Exact `4a60`/`4a61` and `8a61`/`8a60` check seven-bit data-address ownership,
descending stack order, balanced restore, and preserved A. The fixture uses
an explicit register read/write mode at its otherwise interrupting-read
peripheral addresses `60/61`; these tests do not assign Nokia silicon meaning
to those addresses. Exact `4fd2` checks high/low word order, a two-word circular
AR2 advance, unchanged AR3/accumulators, and untouched adjacent memory cells.
Exact `a249`/`a201` check signed dual-memory SUB into A, 32-bit carry,
unchanged B/T/source and decrementing-X/incrementing-Y versus stationary
pointers, with one-cycle execution. Exact `e723` distinguishes AR2-to-AR3
from the adjacent AR5 source and checks source preservation and one-cycle
execution. Exact `7083` checks dmad-to-AR3-indirect movement, preserved source
and pointers, extension consumption and two-cycle execution.
Exact `74d4` checks one port read, extension consumption, destination-before-
circular-wrap ordering and unchanged AR6. Exact `0092`/`0192` check both SXM
settings, A/B destination routing, the `1 + ffff` carry boundary, preserved
source, single AR2 post-increment and one-cycle arithmetic. Exact `438a`
checks shifted SUB's read-before-decrement ordering and partial-carry rule.
Exact `b43e` checks signed rounded MAC into A, preserved B/source, X publication
in T, and independent stationary-X/circular-Y pointers.
Exact `e6e9` complements `e4e9` with distinct A/B contents, proving old-B
store selection while loading T, preserving both accumulators/status and
independently advancing/wrapping Y/X. Exact `7092`/`7090` extend MVKD to
post-incrementing AR2/AR0 with unchanged source and two-cycle execution.
Exact `1e84` checks both SUBC quotient/carry outcomes through AR4, preserved
B/source/pointer and one-cycle arithmetic.
Exact `7583` checks AR3-selected port output against a distinct AR2 source,
unchanged source/pointers, port-extension consumption and two-cycle execution.
Exact `8192` checks low-B store-before-increment and preserved B in one cycle.
Exact `6d95` checks one AR5 increment, unchanged AR6/memory and one cycle.
Exact `7e92` distinguishes A-addressed program memory from different data
memory at the same address, checks AR2 post-increment, unchanged A/AR3 and
five-cycle execution.
The fresh trace's assertion gaps are closed. The integrated idle/Menu/power
gate now enforces the 594-word, 107-group primary fingerprint, passes the
long power-key lifecycle and verifies explicit assertions for all 594 union
words. Menu and power add no distinct words in this run. This is not evidence
of acquisition or a reason to fabricate RF samples.

Direct comparison with the retained relative-timer trace finds no removed
opcode words and 137 added words. Its summary has zero slot expiries,
29 completion strobes, mode `AC=0` and no port-32 writes; the current run has
6,467 slot expiries, 6,497 completion strobes, mode `AC=1` and three port-32
writes. The port-write sequences agree through the first 44 records; the new
run then continues at `0x0dfe/0x0e03` around 0.134 s instead of returning to
the older codec-control sequence around 0.158 s. This matches the already
documented free-running CTSI/absolute-compare expansion introduced in
`d5c1022`, including its table-driven and accumulator-sourced port-31/32
writes. It is a comparison of emulated traces, not independent silicon
validation or a single-change causal experiment.

The retained narrower baseline dispatches 457 words in 91 high-byte groups (set SHA-256
`e5ab0413453f271100996a54cea8f712eebe6381d4f7b4f7bf959f55631efefc`).
The tap is in `execute_run` immediately before `execute_one`, not in the
extension-word fetch helper: these are instructions dispatched by the current
emulated core, not raw program-memory reads or independent silicon evidence.
`first_pc` is only the first observed site; each count aggregates all sites.
The coverage gate also runs seeded 5110 Menu and long power-key traces. It
checks the Menu press/release and the power lifecycle, then unions all three
opcode sets before checking fixture assertions. Neither additional trace
currently adds an opcode word; these are measured negative results, not
evidence that other interactive or radio paths cannot execute more DSP
instructions. The fixed idle fingerprint still applies only to the untouched
primary run.
The fixture asserts all 594 fresh-run words plus fixture-only words. This
closes exact-word assertion coverage for that captured run, not every operand,
status or hardware interaction those instructions can encounter.
The generated report gives the current fixture-only total; these are *word*
counts, not instruction-family counts. Fixture execution alone is not
proof that a particular result is asserted, and this one boot is not a census
of every possible ROM4 path.
`make check-c54x-observed-coverage` regenerates both verbose logs and fails if
the ROM4 idle word set changes or any observed word lacks an assertion. It is
also part of `make check-c54x-cross-rom`; the gate does not validate unobserved
instruction variants or silicon-level timing. The tool is given the current
core source for a separate static decoder inventory.
The consolidated 594-word checkpoint passes `make check-c54x-cross-rom`:
native core conformance, ROM4 RF-boundary and observed coverage checks,
3210 coherent frontier, native 5110 physical Phone book menu, and the
3310/3330/3410 product frontiers. The sibling frontier checks retain their
configured backend distinctions; this aggregate pass is regression evidence,
not native DSP, RF acquisition or speech validation for every handset.
That inventory matches top-level cases and opcode masks and reports the
current number of matching words that do and do not execute in the fixture.
The generated report identifies matching high-byte groups without a fixture word;
even covered groups do not establish their remaining words. These are **candidates**,
not a verified implemented-instruction count: nested validity, extension-word
grammar, and behavior are not established by a source mask. The report keeps
the ROM4-observed/fixture-asserted class separate, so a new observed gap can
be ranked by execution count before expanding tests into unused encodings.

### TI Arithmetic Family Audit

This is a syntax-level comparison against SPRU172C tables 2-1 through 2-6,
not a claim that every opcode variant or memory-bank timing is validated.
The newly added immediate-16 arithmetic, immediate MAC and DELAY forms are
fixture-only in the current 5110 idle/Menu/power union; the exact observed-word
assertion gate remains the higher-priority regression.

| TI table | Decoder and fixture state | Remaining contract |
| --- | --- | --- |
| 2-1 ADD | Smem, TS, shifted Smem/Xmem, dual X/Y, immediate SHFT/16, and accumulator SHIFT/ASM forms have selected value/status/cycle fixtures. | Other status/addressing variants remain unasserted. |
| 2-2 SUB | Smem, TS, shifted Smem/Xmem, dual X/Y, immediate SHFT/16, and accumulator SHIFT/ASM forms have selected fixtures; SUBB/SUBC are separately asserted. | Other status/addressing variants remain unasserted. |
| 2-3 multiply | MPY/MPYR, MPYU, MPYA, and SQUR syntax families have decoder paths and selected fixtures, including signed 17-bit A-high cases and an unsigned MPYU FRCT/OVM overflow boundary. | Other FRCT/OVM and long-offset variants are only partly asserted. |
| 2-4 MAC/MAS | Smem, dual X/Y, T-source A-high, immediate MAC, MACSU, MACD/P, and SQURA/SQURS families have selected fixtures. | Rounded/overflow and remaining addressing variants are incomplete. |
| 2-5 double operand | All six named Lmem families are decoded; C16=0/1 and long-offset fixtures cover selected forms. | Other addressing and overflow/carry boundaries remain unasserted. |
| 2-6 application | The named arithmetic/application families have decoder paths and selected fixtures, including DELAY, EXP/NORM, MAX/MIN, RND/SAT, FIRS/LMS and SQDST. | Full operand-mode and status coverage remains open. |

Every syntax listed in those six TI summary tables now has a decoder owner.
That is the result of a manual syntax inventory, not an exhaustive opcode or
silicon-conformance claim. The next audit layer is the unasserted status and
addressing combinations in the right-hand column.

SPRU172C's listed cycle counts assume DARAM. The focused tests assert those
baseline costs and selected absolute/long-offset surcharges; they do not
model external-memory wait states. A static decoder mask match is not counted
as a validated instruction in this matrix.

The immediate-MAC fixtures reproduce SPRU172C's `MAC #345h,A,B` fractional
example and `MAC *AR5+,#1234h,A` example. Immediate multiply word `6383`
(`MPY *AR3,#fffe,B`) additionally checks signed products -6 and -12 with
FRCT clear/set, T publication, unchanged A/source/AR3/carry and the two-cycle
DARAM cost. This destination variant is fixture-only, not newly observed
ROM4 boot execution.
The B-source immediate-MAC words `6683` and `6783` independently select A
and B destinations: with source B=10 and memory/immediate product -6, the
nonfractional cross-destination case yields A=4 while retaining B=10; the
fractional same-destination case yields B=-2 while retaining A=4. Both
assert T/source-memory/AR3/carry preservation and two-cycle DARAM cost.
These are fixture-only destination checks, not RF or speech acceptance.
Four additional B-source cases distinguish positive overflow for both
destinations with OVM clear/set: `7ffffffe + 3*2` retains `0080000004`
without OVM or saturates to `007fffffff` with it. Only the destination's
OVA/OVB bit is set, carry is retained and the other accumulator is unchanged.
T, AR3, source memory and the two-cycle DARAM cost are independently checked.
The integrated observed-coverage gate passes with these destination fixtures:
the idle/Menu/long-power union remains 594 asserted words across 107 groups,
with its accepted fingerprint unchanged. Neither physical input trace adds
an unasserted word; the extra arithmetic words remain fixture-only.
The immediate-MAC fixtures also assert absolute extension
order, preincrement timing, T publication, and DARAM cycle cost. A long-offset
fixture verifies immediate-before-offset fetch and the extra cycle; a separate
FRCT/OVM fixture verifies saturation and OVB for the immediate form. The shared
Smem/immediate operand reader preserves the corresponding MPY behavior.
The `ADD/SUB #lk,16` fixtures assert the TI table-2-1/2-2 two-word,
two-cycle forms, including SXM sign extension, a distinct destination,
carry/no-borrow, and overflow with SXM clear. These forms are fixture-only;
the observed 5110 union has not executed them.
The table-2-6 comparison exposed a missing `DELAY Smem` decoder path. It now
shares `LTD`'s source-to-successor memory copy without updating T. Fixtures
cover the manual's `*AR3` example, preincrement, and both absolute and
long-offset extension-cycle surcharges; no current 5110 trace executes this
family.
Fixture-only `2483` checks that FRCT doubles the unsigned `MPYU` product,
OVM clamps the overflowing accumulator result, and OVA is set without
modifying T or AR3. It runs in one DARAM cycle.

The coverage tool now separates fixture execution from explicit result
assertions. All 457 narrower-baseline ROM4 words have an `opassert` marker
after a passing exact-word check; none of that set are executed-only or absent.
The fresh 594-word trace also has explicit passing assertions for every word.
Neither result
establishes unexecuted ROM4 paths, unobserved encodings, or the instruction-family
audit. Existing result
checks now explicitly
assert `7214`, `f5e2`, `f520`, `3292`, `e902`, `e903`, and `f120`, removing
them from the executed-only class without changing CPU behavior. Observed
`1093`, `1084`, and `1094` now check `LD` sign/zero extension, pointer behavior,
preserved status and one-cycle cost. Observed `4912` and `730b` now assert
MMR load/move direction and their one-/two-cycle costs; `6882` and `6884`
assert indirect `ANDM` masking, distinct AR selection, unchanged status, and
two-cycle costs. `6db1` and `6dc2` now assert AR0-offset and circular `MAR`
updates in one cycle; `e900` and `e901` assert short-immediate B loads reset
the guard and leave status unchanged. A synthetic fixture combining the
observed `ec1f` and `7693` words checks that `RPT #31` executes 32
consecutive `ST #lk,*AR3+` stores at TI's two-cycle body rate; it does not
claim those words are adjacent in the ROM. TI SPRU172C's opcode diagrams
distinguish `0x76xx` ordinary-memory `ST` from `0x77xx` page-zero `STM`;
the core comment was corrected, and the earlier `768a` ordinary-memory
fixture remains valid.
Observed `f032`/`f035` assert shifted long-immediate `AND` results,
unchanged carry and two-cycle cost. `f0fb`/`f0fe` assert one-cycle logical
right shifts, outgoing-bit carry and cleared accumulator guard bits against
TI SPRU172C's `SFTL` contract.
The previously leading absent word,
`fa20`, now has taken and not-taken `BCD NTC` fixtures
that check both delay slots, branch destination, and cycle cost. The leading
observed `1183` and `0093` fixtures cover negative Smem sign extension under
SXM, a fixed versus postincrementing AR3, OVM saturation and sticky overflow,
and the one-cycle DARAM cost. Together they account for 18 ROM4 executions.
Exact `f1a0`/`f2c0` test 40-bit cross-accumulator OR/XOR without source or
status changes; `f620` tests SUB B,A with OVM saturation and sticky overflow.
All three assert one-cycle timing and cover 23 observed ROM4 executions.
Exact `818a` and `81d3` store tests distinguish ordinary postdecrement from
post-access circular increment/wrap, preserve carry, and assert one-cycle
timing. They cover 16 observed ROM4 executions (SPRU131G section 5.3).
The observed MMR-load words `4815` and `4915` assert that LDM AR5 zero-extends
under SXM, replaces the full destination accumulator without changing the
other one or status, and costs one cycle (SPRU172C, LDM page 4-73). They
remove 18 observed executions from the exact-word gap list. The leading
MMR-store words `8814`, `8813`, `8811`, and `8912` now assert accumulator
low-word transfer to their respective address registers in one cycle. The
`ec0c` fixture checks thirteen iterations of `MAR` after `RPT #12` and the
one-cycle setup; the following `e80f` checks `LD #15,A` independently. The
`13f8`, `81f8`, and `7582` fixtures separately check unsigned absolute load,
absolute B low-word store, and the value and timing of an indirect port write.
Long-offset `BITF` and `CMPM` now charge TI's extra cycle; fixture-only
`61ea`/`60e2` check TC, AR updates, and three-cycle timing. Exact ROM4 `6180`
checks a false bit-field test, while `e736` checks AR3-to-AR6 MMR transfer.
Exact `fa4d` and `fc4d` fixtures check B-equal branch/return decisions in
both directions, delayed-slot execution, stack effects, and TI's cycle counts.
Fixture-only `47e2`, `4bea`, and `8bea` now check the long-offset surcharge
for `RPT Smem`, `PSHD Smem`, and `POPD Smem`, including repeat count, stack
data, AR update, and cycle cost.
Fixture-only `4fea` and `57e2` check the Lmem long-offset decoder: `DST`
preupdates AR and writes the high/low pair in three cycles; `DLD` leaves AR
unchanged, reads both words, and sign-extends the result in two cycles.
Fixture-only `40ea`, `34e2`, and `20ea` check the arithmetic/multiply Smem
long-offset surcharge against TI's instruction tables: `SUB`, `BITT`, and
`MPY` each consume an extension word and two cycles, with the expected AR
preupdate or preservation and an asserted result. These tests correct timing
for the corresponding decoded instruction families, but do not add a new
exact-word assertion for the observed ROM4 run.
Exact ROM4 `1282` (`LDU *AR2,A`) and `1082` (`LD *AR2,A`) fixtures use the same
negative 16-bit operand to distinguish zero- from sign-extension under SXM.
Both assert unchanged AR2 and the one-cycle indirect cost; this retires the
previously most-executed absent word (`1282`, 32 observed executions).
Exact ROM4 `f84e` (`BC pmad,BGT`) now asserts both the taken and fallthrough
targets, a negative 40-bit B test, and TI's five/three-cycle costs. It was the
next most-executed absent word (28 observed executions).
Exact ROM4 `ec01` (`RPT #1`) asserts two MAR body executions, the resulting
AR0 value, and the one-cycle repeat setup. It was observed 25 times.
Exact ROM4 `fa30` (`BCD pmad,TC`) checks both condition outcomes, both delay
words, the selected destination, and delayed-branch timing. Exact `8914`
(`STLM B,AR4`) checks the low-word transfer and one-cycle cost.
Fixture-only `71ea` and `70ea` check long-offset MVDK/MVKD: the Smem
displacement precedes the direct destination/source extension, with AR2
preupdate, copied data and three-cycle cost. Original MU4 `71e7/70e7`
execution independently exercises this ordering through cache program and
read-back. Fixture-only `75ea` checks PORTW's port-before-offset ordering,
AR2 preupdate, transferred data and three-cycle timing. These fixtures do not
change the captured ROM4 opcode-set coverage.
Fixture-only `7dea` checks MVDP with long-offset source addressing: program
destination before offset, AR2 preupdate, the program-memory word written,
and TI's five-cycle cost. This also leaves observed ROM4 coverage unchanged.
Fixture-only `7fea` and `7eea` check long-offset WRITA and READA in opposite
directions: the word transferred between data and A-addressed program memory,
AR2 preupdate, and TI's six-cycle cost. They do not alter observed ROM4 word
coverage.
TI SPRU131G section 5.5.3.2 states that the long offset is the last code
word of a two- or three-word instruction. The three-word fixtures for MVDK,
PORTW, and MVDP were re-encoded accordingly; fixture-only `7cea` now checks
MVPD's long-offset destination decode and four-cycle cost. Absolute Smem
forms retain their separately tested address-before-immediate ordering.
Fixture-only `74ea` checks PORTR's port-before-offset order, port-read value,
AR2 preupdate, and TI's three-cycle long-offset cost.
`6fea` checks the shifted Smem decoder's offset-before-extension-word
order, accumulator result, AR2 preupdate, and three-cycle long-offset cost.
Original MU4 InitDisk independently establishes this encoding with
`6fe1 0010 0c48` at `3ac3`: `LD *AR1(0010),8,A`. The prior synthetic
shift-before-offset encoding was wrong and is corrected. The generic `6f`
decoder resolves long Smem before reading its opcode-extension word;
absolute and short forms retain their existing regression coverage.
Original InitDisk's arithmetic helper also uses `f808` (`BC NC`). The core
implements carry-clear/set branches, including delayed forms, against TI's
condition codes; eight fixtures cover both carry states and both delay
forms while requiring status preservation.
Original InitDisk also corroborates long `CMPM` displacement-before-immediate
with `60e1 0002 0001` at `431f`. Four fixtures cover equal/unequal memory
and preupdate/no-update addressing while requiring TC-only status changes.
The previous decoder reversed these fields and rejected a valid context.
Original MCUSI16 `61e1 0001 0002` at logical `02:9521` likewise requires
BITF displacement-before-mask, selecting McBSP1 status `0049` and mask 2.
The reversed decoder read adjacent `004a`; the corrected native run reads
`0049` only. Four executable long-BITF fixtures cover set/clear TC,
preupdate/no-update, wrong-address sentinels and preserved unrelated status.
The earlier synthetic preupdate BITF encoding is corrected without changing
its expected result or cycle cost.
Original InitDisk's file-write path uses `9488` at `3be7`, the compact
`LD *AR2+,8,A` form documented by TI SPRU172C LD syntax 5 (pages 4-66/68).
The core implements `94/95` Xmem loads through the existing shifted-load
and dual-address update helpers. Sixteen executable fixtures cover both
accumulators, signed/unsigned input, shifts 0/5/8/15, postincrement and
preservation of the other accumulator. With this instruction the original
receiver accepts all seven R060 segments; this is not yet file-backed DMA
load or complete MU4 execution.
Exact ROM4 `e58b` (`MVDD *AR2+,*AR5+`) asserts the copied word, both pointer
increments, and its one-cycle cost; it was the most-used absent word in the
captured boot (24 executions).
Exact ROM4 `13d2` (`LDU *AR2+%,B`) asserts unsigned load of `0x8001`,
post-read circular wrap of AR2 with BK=4, and one-cycle cost. Exact `1882`
(`AND *AR2,A`) checks its masked result, unchanged pointer, and one-cycle
cost. They were the next two most-used absent words (23 executions each).
Exact `1081` (`LD *AR1,A`) checks SXM sign extension, unchanged AR1, and
one-cycle cost. Exact `ee01` (`FRAME #1`) checks SP advancement across a
page boundary and its one-cycle cost. Both had 22 observed executions.
Exact `f340` (`OR #lk,B`) checks preservation of B's upper bits; exact `f230`
(`AND #lk,B,A`) checks the zero-filled result in A and unchanged B. Both assert
TI's two-cycle immediate-logical timing and had 22 and 21 observed executions,
respectively.
Exact `f517` (`ADD A >> 9,B`) now checks both SXM=0 zero-fill and SXM=1
sign-fill of a negative 40-bit source, with A unchanged and one-cycle timing.
TI SPRU172C specifies SXM-controlled fill for a right-shifted ADD operand; the
generic shifted-ADD/SUB decoder previously always sign-filled. The old
`f517` and `f508` exact cases were unreachable behind that generic decoder and
have been removed. Exact `f4e2` (`BACC A`) checks the A-selected target,
unchanged stack pointer, and six-cycle branch cost.
Exact `f474` (`SFTA A,-12`) checks zero-fill and sign-fill under SXM=0/1,
the shifted-out bit in carry, and one-cycle timing. Exact `7722`
(`STM #lk,MMR22`) checks immediate-to-register transfer and two-cycle timing.
They had 20 and 16 observed executions, respectively.
Fixture-only `f537` (`SUB A >> 9,B`) checks SXM=0 zero-fill and SXM=1
sign-fill of a negative 40-bit source, the unchanged A source, and one-cycle
timing. This guards the SUB half of the generic shifted-arithmetic correction
without claiming that ROM4 executed this exact word in the captured boot.
Exact `6184` (`BITF *AR4,#lk`) now checks both TC outcomes, preservation of
carry and AR4, and TI's two-cycle cost; it had 14 observed executions.
Exact `e908` (`LD #8,B`) checks replacement of B without changing A in one
cycle. Exact `4914` (`LDM AR4,B`) checks that the MMR value zero-fills B even
with SXM set, leaving AR4 and A unchanged in one cycle. They had 14 and 13
observed executions, respectively.
Exact `f3ff` (`SFTL B,-1`) checks the logical result, cleared guard bits,
carry from bit 0, and one-cycle timing despite SXM=1. Exact `60f8`
(`CMPM *(lk),#lk`) checks the absolute address-before-immediate order, TC and
carry, and the three-cycle absolute surcharge. Both had 13 and 12 observed
executions, respectively.
Exact `0892` (`SUB *AR2+,A`) checks SXM sign extension of a negative Smem
operand, post-read AR2 increment, and one-cycle timing. Exact `8082`
(`STL A,*AR2`) checks low-word storage with A and AR2 unchanged in one cycle.
They had 10 and 13 observed executions, respectively.
Exact `10d2` (`LD *AR2+%,A`) checks SXM sign extension and circular AR2
post-update with BK=4 in one cycle. Exact `e50b` (`MVDD *AR2,*AR5+`)
checks the copied word, unchanged X pointer, Y post-increment, and one-cycle
cost. Both had 12 observed executions.
Exact `1083` (`LD *AR3,A`) and `1284` (`LDU *AR4,A`) check signed versus
unsigned loading of the same negative-looking word with SXM set. Both
preserve their pointers and take one cycle; they had 13 and 11 observed
executions. Exact `890e` (`STLM B,T`) checks B's low-word transfer to T,
unchanged B, and one-cycle timing; it had 11 observed executions.
Exact `f1f4` (`SFTL A,-12,B`) checks a logical right shift, carry from
source bit 11, unchanged A, and one-cycle timing. Exact `f280` (`AND B,A`)
checks that B remains intact while A changes in one cycle. Exact `f350`
(`XOR #lk,B`) checks a two-word immediate logical operation, unchanged A,
and two-cycle timing. Each had 10 observed executions.
Exact `71d2` (`MVDK *AR2+%,dmad`) checks the copied word, circular AR2
post-update with BK=4, and two-cycle cost. Exact `f842` (`BC pmad,AGEQ`)
checks both the zero-A taken path and negative-A fallthrough, their PCs,
and five/three-cycle costs. Both had 10 observed executions.
Five older result checks now carry exact-word markers: `f062` and `f070`
for the long-immediate load/repeat sequence, `f944` for conditional call,
`f485` for signed absolute value, and `ec03` for the four-iteration repeat.
These are existing scenario assertions, not new isolated timing fixtures.
The last three executed-only words now have isolated checks: `0083`
(`ADD *AR3,A`) checks SXM sign extension, unchanged AR3, and one-cycle cost;
`e726` (`MVMM AR2,AR6`) checks the register transfer and one-cycle cost;
`7726` (`STM #TSS,TCR`) checks the two-cycle store by reading TCR back
through fixture-only `4826` (`LDM TCR,A`).
Exact `f945` (`CC pmad,AEQ`) now checks taken and fallthrough PCs, the
pushed return address and stack pointer, and five/three-cycle costs. Exact
`fc44` (`RC ANEQ`) checks both return outcomes, stack effects, and the same
five/three-cycle costs. Each had 10 observed executions.
The challenge-loop words `6d8c`, `8084`, `108a`, `1a8b`, `1c84`, and `e598`
now have isolated
assertions as well as the aggregate transform-result check.
The 143-word class is a priority list
for new fixtures, ordered by observed execution count, not proof that those
instructions are incorrect. A marker establishes the checked outcome only,
not complete coverage of an instruction's operand or flag variants.
`--group-report` with `--fixture-log` ranks unasserted ROM4 executions by
opcode high byte. This is a workload ranking, not an instruction-family
decoder. `--all-gaps` lists every unasserted word by observed use, with an
`absent` or `executed-only` class; the default report shows only the leaders.
In the current 30-second idle/Menu/power union, `f4` leads with 3,155,723
executions of `f495` NOP and 1,572,960 of `f491` ROL A. Both exact words
are asserted; group frequency is not an untested-opcode count. The formerly
dominant `4a`/`8a` gaps are now covered by
a twelve-register MMR save/restore fixture: it initializes each register,
checks all twelve stack values, overwrites the registers, and checks the
restored values after the reverse pops. This adds exact checks for 24 ROM4
stack words and the ROM4 `STM` initializer words. The current group report
ranks `ff` highest among unasserted groups (7 absent words, 26 executions).
The asserted MMR stack words `4a09`/`4a0a` and `8a0a`/`8a09` check
accumulator-A high and guard-word stack order, guard width, preservation of
the low accumulator word, and final SP restoration. The corresponding
`4a0b`/`4a0c`/`4a0d` and `8a0d`/`8a0c`/`8a0b` fixtures check all three B
slices, LIFO order, A preservation, and SP restoration. TI SPRU172C specifies
one word and one cycle for each PSHM/POPM form; the core has no extra cycle
charge for these opcodes. It specifies two cycles for `STM #lk, MMR`;
exact ROM4 `7712`, `4a12`, and `8a12` now check AR2 transfer, stack
movement, and those costs. Other MMR register encodings remain separate tests.
The high-use ROM4 `8807`/`4807`/`4a07`/`8a07` forms now check the ST1 MMR
write, read, and stack round trip at their one-cycle costs. TI SPRU172C
states that `LDM MMR,dst` ignores SXM; a status value with bit 15 set exposed
the core's erroneous sign extension. Both A and B LDM destinations now
zero-extend, with `4907` exercising the B form in the fixture.
TI SPRU172C gives `IDLE K` a four-cycle minimum before its unbounded idle
interval. The core now charges that minimum instead of one cycle. An exact
ROM4 `f4e1` fixture checks `IDLE 1` entry, retained continuation PC, and no
premature execution of the next word. Six additional executable cases check
SPRU131G section 6.11.1: external IRQ and timer requests remain latched but
do not wake IDLE1 while masked in IMR. Enabled requests wake independently
of INTM; INTM clear enters the ISR, while INTM set resumes the continuation
without servicing the request. The timer continues during IDLE1. These
fixtures do not independently time the minimum or establish reset wake.
Four further cases implement the timer-clock part of SPRU131G sections
6.11.2/6.11.3: `f6e1` (IDLE2, NN=10) and `f5e1` (IDLE3, NN=01) freeze TIM
and its prescaler while retaining TSS=0. Masked external INT0 leaves the
timer frozen; enabled INT0 with INTM set resumes the foreground and timer,
which can subsequently latch TINT during IDLE1. All four cases save and
restore the frozen counter/prescaler, then repeat the wake outcome. The core
saves the selected idle mode and remaining timer ticks; it does not substitute
a period reload on wake. Reserved NN=11 is not accepted as an idle mode.
This establishes timer suspension and restoration, not PLL lock time,
cycle-exact sleep-entry latency, pulse-width qualification, reset wake,
or complete clock-source behavior of attached peripherals.

External NMI is a separate architectural input, not an IFR/IMR source.
[SPRU131G sections 6.10/6.11 and table 6-20](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
specify NMI priority, vector 1 at `IPTR:0004`, global masking on ISR entry,
and wake independently of IMR/INTM. The core latches an assertion, wakes any
idle mode and preserves maskable IFR bits when servicing NMI. Twelve
executable cases cover IDLE1/2/3, both INTM values and empty/full IMR;
simultaneous pending maskable sources do not displace NMI. Acceptance checks
the vector, stacked idle continuation, unchanged IFR/IMR and ISR INTM state.
`check-c54x-core` requires this result and twelve pending-request save/replay
cases. The fixture suspends instruction execution after idle, presents NMI,
and saves after the input has latched but before the vector/stack write.
Restoration recovers the pre-entry PC, SP, original INTM and stack sentinel;
resuming reaches the same ISR result exactly once. A repeated held assertion
does not enqueue a second NMI after restoration. This fixture-only execution
suspension is not a modeled physical clock-stop or interrupt latency.
[SPRS039C's interrupt timing requirements](https://www.ti.com/lit/gpn/tms320lc543)
describe the physical synchronizer and pulse qualification. These logical
fixtures do not establish that timing, PLL restart, nested-NMI behavior or
pipeline drain; the existing aggregate entry-cycle charge remains calibrated.
No handset NMI wiring or boot-unblocking timer delay is inferred.
SPRU302B sections 2.10/3.8 explicitly qualify the generic power-down summary:
externally clocked/frame-synchronized McBSP and DMA transfers may continue and
wake IDLE2/3. DMA automatically gates its transfer clock and uses X2/CLKIN
instead of the halted PLL in IDLE3. A blanket CPU interrupt-source filter or
blanket DMA suspension would therefore be incorrect.
The CPU now publishes its peripheral-clock stop line to the MU4 McBSP devices.
Only the internally generated bit timer pauses; its pending interval and shift
state survive save/load and resume. Four coupled CPU/McBSP cases verify a
queued internal word stays pending during deep idle and completes after
enabled external wake, including save-state replay. Four additional pin-driven
cases verify externally clocked words complete while asleep and wake the CPU
only with the serial IMR bit enabled. These synthetic edges establish ordering,
not a physical baud rate. DMA input-clock switching, PLL stabilization and
unsupported serial clock/frame configurations remain outside this coverage.
Six coupled CPU/DMA cases additionally cover all three IDLE modes with masked
and enabled completion interrupts while INTM remains set. The real DMA device
copies a four-word data-memory block after the CPU has entered idle; masked
completion leaves the foreground asleep, while enabled completion resumes it.
Each case saves before the first transfer, restores the pending device timer
and sleeping CPU, and reproduces the block and wake result. The fixture selects
the documented DMAC0/BRINT2 vector slot explicitly; it does not validate a
product's interrupt mux. Its 100-us observation window establishes completion
and ordering only, not the two-clock transfer cadence or IDLE3 input-clock rate.
ROM4 `ed00` now clears a preloaded ASM=-1 before an ASM-based accumulator
load; `8083` then stores A's low word through *AR3 without changing the
pointer. The fixture checks both results and the three one-cycle operations
as an aggregate. Other ASM values and STL addressing forms remain separate
tests.
Exact ROM4 `f490` (`ROR A`) and fixture-only `f590` (`ROR B`) now check
incoming carry into bit 31, outgoing bit 0 into C, cleared accumulator guard
bits, other-accumulator preservation, and TI SPRU172C's one-cycle cost.
Exact ROM4 `f845` (`BC pmad, AEQ`) checks both branch outcomes: zero A takes
the branch in five cycles; A with only a nonzero guard byte falls through in
three cycles. The intervening port-write markers check destination and cycle
cost without assuming a cycle-exact wall-clock timer.
Exact ROM4 `f3b0` (`OR B >> 16, B`) checks zero-filled right shift of a
negative 40-bit source, unchanged A and carry, and one-cycle cost. TI SPRU172C
specifies no sign extension for this OR shift. The generic decoder already
implemented that rule; two unreachable switch cases beneath it instead used
arithmetic shift and have been removed to prevent a misleading fallback.
Exact ROM4 `f3e8` and `f3f8` are `SFTL B, 8` and `SFTL B, -8`, not 40-bit
rotates. Their fixtures check the low-32-bit shift, cleared guard byte,
outgoing carry bit, unchanged A, and TI SPRU172C's one-cycle cost. Six
unreachable switch cases labeling these shifts as rotates or arithmetic shifts
were removed; the generic SFTL decoder was already taking precedence.
Exact ROM4 `f330` (`AND #lk, B`) and `f130` (`AND #lk, A, B`) now check the
zero-filled immediate mask, source preservation in the cross-accumulator form,
unchanged carry, and TI SPRU172C's two-cycle cost. The latter was previously
absent from the fixture; neither marker claims all shift-count variants.
Exact ROM4 `f010` (`SUB #lk, A`) and `f000` (`ADD #lk, A`) now check both SXM
states with immediate `0xff80` and the two-cycle cost. The SXM-clear SUB test
failed before the correction: the shared long-immediate arithmetic path
sign-extended the operand unconditionally. It now uses the existing SXM-aware
operand helper; logical immediates remain unextended. These checks do not
cover all shift counts or saturation cases.
Exact `f310` (`SUB #lk,B`, 82 observed dispatches) now checks SXM sign
extension, B-only destination, and the two-cycle cost.
Exact `6d8a` (`MAR *AR2-`, 96 dispatches) checks one-cycle pointer modification.
Exact `f846` (`BC AGT`, 81 dispatches) and `f84d` (`BC BEQ`, 81 dispatches)
check the 40-bit accumulator conditions and taken/untaken five-/three-cycle
costs respectively; these are selected outcomes, not full condition coverage.
Exact `11f8` (`LD *(lk),B`, 80 dispatches) checks SXM sign extension and the
two-cycle absolute-address surcharge. Exact `f110` (`SUB #lk,A,B`, 68
dispatches) checks the source/destination split and two-cycle cost. Exact
`7482` (`PORTR PA,*AR2`, 79 dispatches) checks the port-to-memory value,
unchanged pointer, and two-cycle interval between consecutive reads; external
I/O wait-state timing remains outside this fixture.
Exact `56f8` (`DLD *(lk),A`, 65 dispatches) checks high/low word ordering,
SXM guard extension, and the absolute-address two-cycle cost. Exact `7213`
(`MVDM dmad,AR3`, 66 dispatches) checks the MMR destination and two-cycle
cost. Exact `708a` (`MVKD dmad,*AR2-`, 64 dispatches) checks source/destination
direction, postdecrement, and two-cycle cost.
Exact `ec0e` (`RPT #14`, 68 dispatches) checks fifteen one-cycle ADD
iterations after one-cycle setup. Exact `e753` (`MVMM AR5,AR3`, 60
dispatches) checks register direction and one-cycle cost. Exact `7194`
(`MVDK *AR4+,dmad`, 64 dispatches) checks source postincrement, destination,
and two-cycle cost.
Exact ROM4 `6c88` (`BANZ *AR0-`, 55 dispatches) now checks both outcomes in
compatibility mode with physical AR0 and ARP-selected AR4 holding opposing
values. The core had tested physical AR0 while updating AR4; both BANZ and
BANZD now select the same register for test and modification. Fixture-only
`6e88` checks the delayed form's selected-register test and two delay words.
Exact ROM4 `f063` (`AND #lk,16,A`) now checks the upper-word mask, preserved
carry and TI SPRU172C's two-cycle cost; its special-case decoder had charged
one cycle. Exact `8184` (`STL B,*AR4`) checks the low-word store and one-cycle
cost. Fixture-only `f162` (`LD #lk,16,B`) checks signed guard extension and
the same two-cycle immediate-load cost; both fixed-shift `LD` special cases
had also undercharged one cycle.
Exact ROM4 `6d96` (`MAR *AR6+`) now checks the selected register's increment
and one-cycle cost. `e712`/`e713` (`MVMM AR1,AR2/AR3`) check source preservation,
both destinations and one-cycle costs.
Exact ROM4 `6c8b` (`BANZ *AR3-`) checks the pre-decrement predicate and
taken/false cycle costs. `f847` (`BC ALEQ`) checks both signs of the 40-bit
accumulator and both branch costs. `880e` (`STLM A,T`) checks the low-word
transfer to T and one-cycle cost.
Exact ROM4 `0af8` (`SUBS *abs,A`) now checks sign-extension suppression with
SXM enabled, carry for both borrow outcomes, and its two-cycle absolute cost.
Exact ROM4 `1e82` (`SUBC *AR2,A`, 16 observed executions) now checks both
conditional-subtraction outcomes: the shifted quotient bit, carry, unchanged
address register and one-cycle indirect cost. The earlier `1ef8` fixture
covered only a successful subtraction.
Exact ROM4 `6dea` (`MAR *+AR2(-7)`, 25 observed executions) now checks signed
long-offset consumption, pointer result, and TI's extra cycle.
Exact ROM4 `f300` (`ADD #lk,B`, 29 observed executions) now checks SXM-driven
negative-immediate extension, carry at the 32-bit boundary into the guard
byte, and two-cycle cost in the shared immediate ALU path.
Exact ROM4 `f210` (`SUB #lk,B,A`) checks B-to-A source/destination selection,
source preservation, both borrow outcomes and the two-cycle cost. Exact
`f150` (`XOR #lk,A,B`) checks the unextended logical immediate, source/guard
preservation, unchanged carry and two-cycle cost.
Exact ROM4 `0282` (`ADDS *AR2,A`, 21 observed executions) now checks
sign-extension suppression despite SXM, carry at the 32-bit boundary, stable
AR2 and the one-cycle indirect cost. The decoder's older `ADD uns` comment
has been aligned with TI's instruction name.
Exact ROM4 `6d90` (`MAR *AR0+`) now checks the compatibility-mode AR0 alias:
with CMPT set, an AR0 operand modifies the register selected by ST0.ARP and
leaves physical AR0 and ARP unchanged. Fixture-only `1090` checks the same
selection for an indirect read and postincrement. The previous core always
used physical AR0. TI SPRU172C's MAR compatibility table specifies this
distinction; standard mode still names physical AR0. The existing `RPTBD`
fixture was corrected to set standard mode explicitly, matching its expected
physical-AR0 result.
Exact ROM4 `1c8b`, `1d93`, and `1c82` now check the indirect XOR sequence:
`*AR3-` reads before decrementing, `*AR3+` reads the new address before
incrementing, and `*AR2` leaves its pointer unchanged. The A/B results keep
the 16-bit memory words unextended even with SXM set; carry is preserved.
Port markers bound the three operations to three cycles in total, matching
[TI SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)'s one-cycle
indirect XOR specification for DARAM. This fixture does not claim cycle
accuracy for external memory wait states.
Exact ROM4 `768a` (`ST #lk,*AR2-`) and `8092` (`STL A,*AR2+`) now form a
pointer round trip: the immediate is stored before decrement, then A's low
word is stored at the decremented address before increment. A middle port
marker checks their separate two-cycle and one-cycle costs, matching TI
SPRU172C's DARAM instruction table. This does not establish external-memory
wait-state behavior.
Exact ROM4 `e5a8` and `e5ca` now check MVDD X/Y memory transfers. The first
copies `*AR4+` to `*AR2+`; the second reads `*AR2+0%`, writes `*AR4+`, and
wraps AR2 with BK=4. Both fetch the source and destination addresses before
modifying either pointer. Port markers check one cycle each for the fixture's
DARAM operands, consistent with [TI SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf);
[TI SPRU131G](https://www.ti.com/lit/ug/spru131g/spru131g.pdf) defines the
dual-operand address-modification rules. The fixture does not model external
memory contention or wait states.
Exact ROM4 `12f8` (`LDU *(lk),A`) and `138b` (`LDU *AR3-,B`) now check
zero-extension of high-bit-set 16-bit data even when SXM is enabled. The
absolute operand consumes its address extension and costs two cycles; the
indirect form reads before decrementing AR3 and costs one. Separate port
markers verify those costs against [TI SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)'s
DARAM LDU table. External-memory wait states are not covered.
Exact ROM4 `f3c8` (`XOR B,8,B`) now checks that the source is B's original
40-bit value, the left shift discards bits beyond bit 39, the result uses
the guard byte, A and carry are unchanged, and the operation costs one cycle.
This matches [TI SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)'s
accumulator-XOR definition; other source/destination and shift variants
remain separate checks.
Exact ROM4 `6b8a` (`ADDM #lk,*AR2-`) exposed a missing memory-arithmetic
contract. With SXM and OVM set, TI SPRU172C's `0x8007 + 0xfff8` example
saturates to `0x8000`; the core previously wrapped to `0x7fff` and left C/OVA
unchanged. It now updates those flags and applies 16-bit saturation, with
separate fixtures for positive overflow with OVM clear and unsigned carry
without signed overflow. Fixture-only `6b80` verifies compatibility-mode AR0
selection through ARP; `6880`/`6980` check the same selection for ANDM/ORM.
[TI SPRU538](https://www.ti.com/lit/ug/spru538/spru538.pdf) confirms that a
memory-plus-immediate addition affects C. [TI SPRU131G](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
defines carry at bit 32, even though ADDM stores a 16-bit result. A separate
SXM-clear fixture checks `0x8000 + 0x8000`: the stored word wraps to zero and
sets OVA, but produces no bit-32 carry. This corrected the first
implementation's 16-bit carry test.
The fixtures do not cover every SXM-clear overflow and OVM combination.
Exact ROM4 `7182` (`MVDK *AR2,dmad`) checks the extension-addressed copy and
unchanged AR2 in two cycles. Exact `8183` (`STL B,*AR3`) checks that only BL
is stored while AR3 stays fixed in one cycle. Separate port markers verify
the DARAM costs against [TI SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf);
external-memory wait states remain outside this fixture.
Exact ROM4 `f820` (`BC pmad, NTC`) and `f84c` (`BC pmad, BNEQ`) check taken
and fall-through destinations at TI SPRU172C's five- and three-cycle costs.
The BNEQ true case has a nonzero guard byte and zero low 32 bits, so a
truncated accumulator predicate would fail. Other branch conditions remain
separate cases.
The high-frequency control words `f495` (`NOP`), `f074` (`CALL pmad`), and
`fc00` (`RET`) now have separate port-marker checks for TI SPRU172C's one-,
four-, and five-cycle costs, respectively. The CALL/RET checks also verify
the pushed return address, destination, and final stack pointer. Existing
exact-word checks for `f273` (`BD pmad`) and `f4eb` (`RETE`) verify delay-slot
continuation and interrupt-return PC/SP/INTM state; they do not independently
measure those two cycle costs. This separates validated outcomes from words
that merely execute during a larger transform.
Four further high-use words already had exact-result checks and now carry
assertion markers: `f484` checks negate result, carry, and overflow;
`f0b0` checks zero-filled `OR A >> 16`; `fa44` checks delayed ANEQ branch
continuation; and `e801` checks the loaded A value in a BANZD delay slot.
These markers do not add cycle-cost claims for those words.
Exact ROM4 `f493` (`CMPL A`) now checks all 40 complemented bits, unaffected
B and carry, and the one-cycle cost. Exact `f0e8` (`SFTL A, 8`) checks a
guarded input's low-32-bit shift, cleared guard, outgoing carry, unaffected B,
and the one-cycle cost. Other source/destination variants remain separate.
Exact `f065` (`XOR #lk,16,A`) now checks the shifted result and TI SPRU172C's
two-cycle cost. The core previously charged one cycle to all four
source/destination variants of this encoding; the shared path now charges
two. The 30-second RF-boundary run retains 6,498 frame expiries and
`IMR=0x035f`, with 207,168 RF reads; at 40 seconds it has 8,664 expiries
and 276,480 reads. Both differ from the ideal completed-frame count by
two 32-read frames at the fixed-time cutoff, so the gate admits up to two
in-flight frames and still rejects a three-frame deficit.
TI SPRU172C table 2-16 specifies two cycles for both `RPTZ dst,#lk` and the
delayed `RPTBD pmad` form. Exact ROM4 words `f071` and `f272` now have
result/control-flow and cycle assertions; their shared A/B and block-repeat
paths previously charged one and four cycles respectively. The non-delayed
`RPTB` form remains at its documented four cycles, now checked directly with
exact `f072` and the same one-word-block fixture. The five-ROM cross-regression
passes after both timing corrections.
TI SPRU172C's Smem tables add one word and cycle for absolute addressing.
ROM4 `34f8` (`BITT absolute`) now checks both T=0 (bit 15) and T=15 (bit 0),
and its two-cycle cost. The decoder's broad `0x30..0x3f` Smem handler already
implemented `15 - T[3:0]` correctly; a later exact-`34f8` handler was
unreachable and used the wrong bit mapping. The unreachable case is removed,
and the absolute cycle surcharge is applied in the actual broad handler.
Exact `70f8` (`MVKD dmad, absolute Smem`) now checks its data movement and
three-cycle cost: two base cycles plus one for absolute Smem. The five-ROM
cross-regression passes with these corrections.
Exact ROM4 `61f8` (`BITF absolute`) and `6182` (`BITF *AR2`) fixtures now
check the TC result and TI SPRU172C's three- and two-cycle costs. The shared
handler previously charged one cycle in both forms. Exact `68f8`, `69f8`,
and `6bf8` fixtures check ANDM, ORM, and ADDM data movement plus their
three-cycle absolute costs; their handlers likewise charged one cycle.
The underlying two-cycle indirect forms receive the same corrected base
charge. The five-ROM cross-regression passes after these timing changes.
The most-executed non-NOP fixture-executed-only word was `ff0c` (over one
million ROM4 executions). TI SPRU172C's condition table identifies it as
`XC 2,C`: it tests carry, not TC. Exact-word fixtures now check both C=0
(skip two following words) and C=1 (execute both), the one-cycle XC cost,
and the resulting `f793` full-width B complement. No core behavior changed
for this test; both words are now explicit assertions in the coverage report.

Execution counts identified `ROL A` (`f491`) and `ROL B` (`f591`) as the largest
previously unasserted ROM4 encodings, at roughly 524,000 executions each in
this run. Both now have core assertions for carry transfer, cleared guard bits,
and preservation of the other accumulator, checked against TI SPRU172C
section 4. Immediate `XOR` (`f050`, about 262,000 executions) now has an
exact-encoding result and extension-word assertion. The next high-use group
was `74d6`/`b03a`/`b0be`/`b3be` (155,000-207,000 executions each). `74d6`
is `PORTR` into circularly post-incremented AR6; its exact-word fixture now
checks port data, pointer wrap, and TI SPRU172C's two-cycle base cost. The
paired `PORTW` path has the same cycle-cost correction and a symmetric
fixture. `b03a`, `b0be`, and `b3be` now have exact-word signed-MAC and
pointer-update assertions. The I/O wait-state component of `PORTR`/`PORTW`
timing remains unmodeled. `f844` (`BC pmad, ANEQ`) exposed a shared branch timing error:
TI specifies five cycles taken and three not taken. Both outcomes now have
executable cycle assertions, and the decoded `BC` variants share that charge.
`10f8` (`LD` with absolute Smem) likewise has an exact-word sign-extension
and two-cycle assertion; the other signed/unsigned accumulator-load variants share
the absolute-address surcharge. In the next loop at DSP PC
`0x324c..0x3268`, exact `e2e4` (`SQDST`) asserts that B receives the square
of A's *old* high word while A receives the signed vector difference. Exact
`4f81` (`DST B,Lmem`) asserts high/low store order and TI's two-cycle cost;
the absolute `DST` and `DLD` forms also receive their documented extension
cycle surcharge. TI SPRU131G table 5-4 specifies that single-operand MOD
increment/decrement is two words for a 32-bit operand. Separate `DST`/`DLD`
fixtures now cover post-increment, pre-increment-before-read, and circular
wrap with a two-word step; `4f81` itself uses a non-modifying AR mode.

The same loop exposed a decoder error at `0x3262`: `6ded fff9` is a two-word
`MAR *+AR5(-7)`, not `MAR` followed by an `XC` opcode. Consuming the signed
extension reveals `4092` (`SUB *AR2+,16,A`) and `5781` (`DLD *AR1,B`), both now
implemented or asserted in the core fixture. The corrected stream still
passes the 5110 coherent, 30-second RF-boundary, and menu gates. Exact-word
fixtures now cover signed dual-memory `a5be` (`MPY`), rounded `b736`
(`MACR`), and parallel `d6e1` (`ST`/`MACR`). The most-executed remaining
ROM4-only word was `4593` (`LD Smem,16,B`, 32,770 executions); an exact-word
fixture now checks sign extension, post-increment, and its one-cycle cost.
The same one-cycle `LD Smem,16` family now has exact-word fixtures for
`4492`/`4493` (A with AR2/AR3 post-increment) and `458a`/`458b` (B with
AR2/AR3 post-decrement). They assert signed and positive 40-bit results,
pointer direction, and cycle cost, covering four further ROM4-dispatched
words without changing the decoder.
The register-indirect `OR Smem` group now has independent result, pointer,
and one-cycle assertions for `1a82`/`1a83` (A) and `1b82`/`1b83` (B),
matching TI SPRU172C table 2-8's base cycle cost.
The challenge loop's `6d8c` (`MAR *AR4-`) and `1c84` (`XOR *AR4,A`) now have
independent one-cycle assertions. The paired test also proves that `XOR`
reads the address selected by the preceding pointer decrement.
Exact-word fixtures also separate `108a` (`LD *AR2-,A`), `8084` (`STL A,*AR4`),
`1a8b` (`OR *AR3-,A`), and `e598` (`MVDD *AR3+,*AR2+`) from the aggregate
challenge response. They check signed loads, stores, source/destination
addressing, both pointer updates, and TI's one-cycle base costs.
The high-use `e742`/`e743` words are `MVMM AR4,AR2/AR3`, not `MVDK` as the
decoder comment formerly claimed. TI SPRU172C includes SP as the ninth
allowed register. The decoder now uses the full four-bit source/destination
fields and MMR accessors; fixtures cover those two ROM4 words and the SP forms
`e748`/`e782`. The 30-second ROM4 trace also dispatches `e782` 25 times.
Exact `e5c9` (`MVDD *AR2+0%,*AR3+`) now verifies a five-word circular
source wrap, independent linear destination increment, data copy, and TI's
one-cycle base cost. It was previously only executed in a broader fixture.
Exact `f0f8` (`SFTL A,-8`) now checks the logical low-32-bit shift, cleared
guard bits, carry from source bit 7, and one-cycle cost against TI SPRU172C.
Independent exact-word tests now also cover `1293` (`LDU *AR3+,A`) with SXM
set, `f0c8` (`XOR A<<8,A`) across the 40-bit guard field, and `e800`
(`LD #0,A`). Each checks its one-cycle cost rather than relying on the
combined startup/challenge sequence.
TI SPRU172C specifies five cycles for a taken `RC` and three when false.
The core previously charged only one for false conditions. A shared return
path now applies the documented costs to the decoded `RC` variants; exact
`fc30` (`RC TC`) fixtures verify both outcomes, stack behavior, and timing.
TI SPRU131G section 5.8 specifies that the addressed word of a 32-bit
operand is the most-significant word; the second word is at the next address
when even and the previous address when odd. DST/DLD formerly rounded every
address down, swapping the halves for odd addresses. The corrected handlers
have an exact ROM4 `4ef8` absolute-DST test for word order and three-cycle
cost, plus odd-address DST/DLD fixtures for the shared memory rule.
Exact ROM4 `fe00` (`RETD`) now checks that two one-word delay-slot instructions
retire before the saved return target, the stack pops once, and the return
itself has TI's three-cycle delayed-return cost.
ROM4 `1092` (`LD *AR2+,A`) now has an isolated signed-load, postincrement,
and one-cycle assertion. The existing 26-word `e59c` (`MVDD`) receive-ring
copy checks its copied words and final X/Y pointers and is now tagged as an
exact-word assertion rather than misclassified as executed-only.
Exact ROM4 `fc4b` (`RC BLT`) fixtures check the signed 40-bit B predicate
and TI's five/three-cycle taken/false costs. `fa45` (`BCD pmad,AEQ`) fixtures
check both outcomes, both delay words, and the three-cycle cost. The latter
found a shared-core defect: the false path had cost only one cycle because
the cycle charge was inside the taken branch; both paths now pay it.
Exact ROM4 `f947` (`CC pmad,ALEQ`) now checks the signed 40-bit predicate,
stack return address, and TI's five/three-cycle taken/false costs. Its shared
handler had charged four/one cycles. Exact `6e8f` (`BANZD pmad,*AR7-`) checks
that the branch uses the pre-decrement value, both delay words retire, and
both outcomes cost TI's two cycles; the handler had charged one.
ROM4 `f020` (`LD #lk,A`) now has an isolated negative-immediate fixture for
both SXM settings and TI's two-cycle cost. Existing result/timing checks for
`e802` as a RETD delay word and `f274` (`CALLD`) are now tagged as exact-word
assertions rather than left in the executed-only class.
ROM4 `8094` (`STL A,*AR4+`) now checks the low-word store, pointer update,
and one-cycle cost. Absolute arithmetic `00f8` (`ADD Smem,A`) and `09f8`
(`SUB Smem,B`) now have SXM-on/off, result, carry, and two-cycle assertions.
Exact `f0ff` (`SFTL A,-1,A`) checks the low-32-bit logical shift, outgoing
carry, and one-cycle cost.
The previously highest-frequency absent word, `ec05` (`RPT #5`, 188 observed
dispatches), now checks six executions of its one-cycle arithmetic body and
TI's one-cycle immediate-repeat setup.
Exact ROM4 `8091` (`STL A,*AR1+`, 160 observed dispatches) checks its AL
store, postincrement, and one-cycle cost. `7593` (`PORTW *AR3+,PA`, 128
dispatches) checks the transferred source word, postincrement, and TI's
two-cycle indirect port-write cost.
Exact ROM4 `fc20` (`RC NTC`, 127 dispatches) checks the three-cycle false
and five-cycle taken paths, including SP behavior. `1086` (`LD *AR6,A`, 120
dispatches) checks SXM sign extension, an unchanged AR6, and one-cycle cost.
Exact `fc45` (`RC AEQ`, 109 dispatches) checks both paths, their three- and
five-cycle costs, and a nonzero guard byte with a zero low word.
Exact `ff20` (`XC 2,NTC`, five dispatches) checks both TC states: the next
two one-word MAR instructions execute when TC is clear, or consume two NOP
slots without changing their ARs when TC is set. Both paths take five cycles
between the fixture's port markers.
The next observed words now checked are `e741` (`MVMM AR4,AR1`, source
preservation and one-cycle cost), `1181` (`LD *AR1,B`, SXM sign extension and
one-cycle cost), and `1a8a` (`OR *AR2-,A`, old-address read, decrement, and
one-cycle cost).
Exact `12d2` (`LDU *AR2+%,A`) now asserts unsigned extension under SXM,
post-read circular wrap, and one cycle. Exact `1cf8` (`XOR *(lk),A`) asserts
the absolute-address extension, unchanged status, and the two-cycle cost
specified by TI SPRU172C.
The `ORM #lk,Smem` family now has exact `6982` and `6984` checks for distinct
AR-selected addresses, unchanged pointers and status, and TI's two-cycle
immediate form. Exact `1b8d` (`OR *AR5-,B`) checks old-address read,
post-decrement, unchanged status, and one cycle.
Exact `4595` checks shifted signed load into B and AR5 post-increment, while
`4917` checks that LDM reads AR7 without SXM extension. Both cost one cycle.
Exact `6883` and `6983` check the complementary ANDM/ORM operations through
AR3 with immediate extension, unchanged pointer/status, and two-cycle cost.
The ROM4 stream at DSP PC `0x3c5e` contains exact `6fd2 0c45` (`LD
*AR2+%,5,A`). Its fixture checks SXM sign extension before the five-bit
left shift, AR2's post-read circular wrap, and TI's two-cycle cost for the
two-word shifted-load form.
Exact `7e8b` (`READA *AR3-`) checks the accumulator-addressed program read,
the old AR3 data destination, post-decrement, and TI's five-cycle cost.
Accumulator ALU exact-word checks now cover ROM4 `f1c0` (`XOR A,B`) preserving
the 40-bit guard and status, `f508` (`ADD A<<8,B`) updating only B, and
`f400` (`ADD A,A`) setting OVA across the signed 32-bit boundary. Each takes
one cycle, as specified in TI SPRU172C.
Exact `f028 00aa` checks the ROM4 two-word, two-cycle `LD #lk,8,A` stream.
Exact `f1b8` checks a logical right shift in `OR A>>8,B`, source preservation,
and unchanged status; `f468` checks one-cycle `SFTA A,8,A` and OVA when a
positive source crosses the signed 32-bit boundary.
Exact `3182` (`MPYA *AR2`) now checks a negative product from A's signed high
word, transfer of Smem to T, unchanged AR2, and one-cycle timing. Exact `0885`
(`SUB *AR5,A`) checks SXM sign extension without pointer motion in one cycle;
`1df8` (`XOR *(lk),B`) checks absolute-address extension, guard/status
preservation, and the two-cycle surcharge.
Exact `80d3`, `818d`, `81d2`, and `8395` now check low/high accumulator stores,
old-address writes before circular or linear AR updates, preserved accumulator
and status values, and one cycle per unshifted store.
Exact `8815`, `e552`, and `e754` now check low-accumulator storage into AR5,
an AR3-to-AR4 data move with source post-decrement, and the AR5-to-AR4 MMR
transfer. Their composed marker span checks one cycle per move.
Exact `f843` now checks `BC pmad,ALT` on both signs of A's 40-bit guard with
five-/three-cycle taken/rejected costs. Exact `ff30` checks `XC 2,TC` with two
executed MAR slots and two rejected NOP-equivalent slots at the same marker
span. Both conditions and costs match TI SPRU172C's control-flow tables.
Exact `1192`, `6dd2`, `7683`, and `8085` now check a sign-extending load through
AR2, circular MAR wrap, an immediate store through AR3, and accumulator-low
storage through AR5. A composed marker span checks their 1+1+2+1 cycle costs.
Exact `771d` is a direct `STM #lk,PMST`, not indirect AR5 addressing; its fixture
checks PMST publication, unchanged ARs/status, and two cycles. Exact `ff45`
(`XC 2,AEQ`) and `ff4e` (`XC 2,BGT`) each check accepted and rejected two-slot
paths, with BGT decided by B's 40-bit guard sign.
Exact `4812`, `4813`, `4814`, `4817`, and `4910` are LDM reads of AR2/3/4/7/0,
not stack operations. Interleaved STL stores preserve each A result for
independent checks; all five loads zero-extend despite SXM and cost one cycle.
Exact `f200` checks signed `ADD #lk,B,A`, source preservation, carry, and two
cycles. Exact `f2a0` checks 40-bit `OR B,A` without status changes; exact `f540`
checks unshifted `LD A,B`, including OVB when A is outside signed 32-bit range.
Both accumulator-to-accumulator forms cost one cycle.
Exact `10da`, `6d93`, `7308`, `7313`, and `7681` now check a circular load
through AR2 using AR0's step, a linear MAR update, two MMR-to-data moves, and
an immediate store. The marker span verifies their combined 1+1+2+2+2 cycles.
Exact `7718`, `7728`, and `7758` check direct `STM` writes to SP, page-zero
data, and CLKMD at two cycles each. A fixture-only `LDM CLKMD,A` reads back
the model's status bit; this is not a silicon PLL-lock timing assertion.
Exact `8812`, `881a`, `8915`, and `891a` check direct `STLM` writes to AR2,
BRC, AR5, and BRC. An intervening `MVMD BRC,dmad` preserves A's first BRC
write before B overwrites it; the marker span checks four one-cycle stores and
one two-cycle MMR move.
Exact `e589` checks a dual-memory move through AR2+ and AR3+, with both words
addressed before the pointers advance. Exact `e734` then copies the advanced
AR3 into AR4. Both moves cost one cycle under TI SPRU172C.
Exact `ec06` checks `RPT #6` executing `MAR *AR1+` seven times after a
one-cycle setup. Exact `f84a` and `f84b` check `BC pmad,BGEQ/BLT` against
B's 40-bit guard sign, each with taken and rejected paths and TI SPRU172C's
five-/three-cycle costs.
Exact `fc47` checks `RC ALEQ` against A's 40-bit guard sign, including the
stack pop only on return and the five-/three-cycle costs. Exact `ff47` and
`ff4c` check `XC 2,ALEQ/BNEQ` with both accepted and rejected two-slot paths;
the guarded MAR effects and five-cycle marker spans agree with TI SPRU172C.
Exact `0882` checks an SXM-sign-extending indirect subtraction without AR2
motion. `1b84` and `1bf8` check indirect and absolute OR into B, preserving
its guard byte and status. `45f8` checks an absolute `LD Smem,16,B` with SXM
extension before shifting. Their marker spans cover TI SPRU172C's one-cycle
indirect forms and the extra word/cycle for absolute Smem.
Exact `4816`/`4913` check direct AR6/AR3 MMR loads into A/B, including zero
extension despite SXM and one-cycle costs. The AR6 case uses a non-mutating
port marker so the timing probe cannot advance the register before the load.
Exact `7215`/`7314`/`7315` check a data-to-AR5 move followed by AR4/AR5-to-data
moves; their preserved source values and eight-cycle marker span establish
three two-cycle transfers under TI SPRU172C. The ROM4 `4920` MMR-0x20 load
is tested separately below because its peripheral contents are product-owned.
Exact `8816`/`8817`/`8910`/`8911` check accumulator-low stores to AR6/AR7
and AR0/AR1. Exact `8821`/`4920` check the CPU-side transfer to MMR `0x21`
and from MMR `0x20` through test backing memory, including LDM zero extension.
The six one-cycle transfers have an eight-cycle marker span. TI's C54x MMR
map identifies `0x20/0x21` as peripheral space on related parts; this fixture
does not validate the Nokia product's peripheral semantics or external timing.
Exact `71da`, `7692`, `7c8b`, `7d82`, and `8182` form a composed transfer
fixture: MVDK reads the old AR2 word before an AR0-step circular wrap; ST
writes at the wrapped address before incrementing; MVPD copies program to
data at the old AR3 address; MVDP copies data back to a separate program
address; STL overwrites only the data word. The marker span checks TI
SPRU172C's 2+2+3+4+1 cycle costs and the direction and order of each move.
Exact `e762` checks a one-cycle MVMM AR6-to-AR2 transfer without changing
the source. A seven-word table checks `e80d`, `e820`, `e905`, `e906`, `e90c`,
`e90f`, and `e97c`: each one-cycle `LD #K` changes only its destination
accumulator and preserves status. Seven `RPT #K` words (`ec09`, `ec0a`,
`ec0f`, `ec11`, `ec13`, `ec1e`, `ec9f`) repeat MAR exactly K+1 times after
one setup cycle; the longest fixture executes 160 MAR iterations.
Exact `f6bd`, `f6bf`, `f7bd`, and `f7be` check one-cycle ST1 bit clear/set
operations while preserving ST0. Exact `fd30`, `fd4b`, and `fd4d` check both
accepted and rejected `XC 1` guarded slots, including 40-bit B sign tests and
the one-cycle conditional-execute cost. Exact `f1fc`, `f463`, `f470`, `f578`,
and `f763` check logical and arithmetic shifts, carry, sign/guard behavior,
and one-cycle costs. These checks follow TI SPRU172C; they do not establish
unobserved status-bit selectors or shift variants.
Exact-word fixtures now also cover `f830` (`BC pmad,TC`) taken/not-taken timing,
`f030` (`AND #lk,A`) zero-extended immediate and two-cycle cost, and `f073`
(`B pmad`) four-cycle cost. Absolute port transfers `75f8` and `74f8`
(about 13,000 executions each) now have exact-word assertions for their
three-cycle cost, extension-word order, and data movement. The highest-use
ROM4-only word was `f040` (`OR #lk,A`); it now has a zero-extension and
two-cycle fixture. `6082` (`CMPM *AR2,#lk`) exposed a missing cycle charge:
TI specifies two cycles, or three with absolute/long-offset addressing.
The exact-word fixture checks both TC outcomes, pointer preservation, and the
two-cycle form. `8093` (`STL A,*AR3+`) now checks the stored low word and
post-increment. Exact `f7bb`/`f6bb` fixtures cover setting and clearing
ST1.INTM; `4a08`/`8a08` cover an AL push/pop with stack-pointer restoration.
`7212` (`MVDM`) and `7312` (`MVMD`) now have exact-word data-direction and
two-cycle assertions. Their core handlers previously charged one cycle.
Repeated-move fixtures cover three successive data addresses in each direction
and TI's one-cycle pipeline rate after the first two-cycle move.
The formerly uncovered `4a`/`8a` MMR save/restore burst is now asserted
by the twelve-register fixture described above.
`6c8a` (`BANZ`) has taken/not-taken timing and pointer-update assertions; TI SPRU172C
specifies four and two cycles, respectively. Long-offset Smem access outside
`MAR` remains a separate
decoder audit; this correction does not establish those addressing forms.
The multiplier paths now set sticky `OVdst` on 32-bit overflow and clamp the
destination when `OVM` is set. A dedicated `MAC *AR3,A` fixture checks both
OVM states, preservation of carry, and TI SPRU131G's `FRCT`/`SMUL` example
(`0x7fffffff` versus `0x7ffffffe`). Exact `b03a` and `d6e1` fixtures also
assert saturated overflow in dual-memory and parallel-store MAC forms.
Other rounded boundaries and encoded variants remain unaudited; fixture
overlap does not claim those cases are complete.
The executable rounded-multiplier boundary matrix covers twenty-two explicit
MPYR/MACR/MASR vectors across A/B destinations, `*AR2`/absolute operands,
and clear/pre-existing destination overflow: 176 cases. It checks fractional
minimum-times-minimum multiplication, positive rounding across the 32-bit
boundary, negative subtraction overflow, OVM saturation, sticky destination
and opposite overflow, preserved carry/TC, T, address and opposite accumulator.
The arithmetic follows TI [SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)
MPY/MAC/MAS definitions. Six additional vectors apply TI
[SPRU131G](https://www.ti.com/lit/ug/spru131g/spru131g.pdf), section 4.5.3 and
PMST table 4-3: rounded MAC/MAS distinguish product saturation before
accumulation with SMUL clear/set, and SMUL has no effect when either OVM or
FRCT is clear. Eight MPYR vectors additionally check that SMUL does not alter
its result under the four OVM/FRCT combinations, negative exact-half rounding,
negative fractional rounding, positive below-half and positive exact-half
rounding. Each case initializes and preserves PMST explicitly. This
does not cover every addressing form or SMUL combination, 40-bit wrap boundary,
cycle cost or Nokia RF sample format.
PMST.SST now saturates the shifted 40-bit accumulator value to 32 bits before
ordinary `STH`, `STL`, `STLM`, and `DST` stores, without modifying the
accumulator. Focused fixtures reproduce TI SPRU131G's signed `STH` and
unsigned `DST` examples and preserve their one- and two-cycle costs. The
implemented parallel `ST||MAC[R]` form also saturates the pre-MAC store
source after its ASM shift. Exact `d6e1` fixtures check both zero and nonzero
ASM shifts, the old B value, unchanged B, A's MACR result, and the one-cycle
cost. The additional `ST||ADD/SUB/LD/MPY` tests below cover selected encodings,
not the entire parallel-store family. The opcode-coverage gate also ranks
untested static decoder matches
within high-byte groups observed on the 5110. The ranking discounts each
group's most-executed word so the `f495` NOP does not make every untested `f4`
variant look urgent. It is a prioritization list, not an implementation count:
the static mask scan does not evaluate nested validity checks or prove the
instruction's arithmetic, flags, addressing, or cycle cost. Exact-word
fixtures remain the evidence for those claims.
On the current 30-second ROM4 idle/Menu/power union, `f4` still leads after
discounting `f495`: 1,626,477 executions of its other observed words, versus
1,029,136 for `f0`. The result prompted an exact-word `f4` audit; it does not
make its 137 untested static matches valid instruction encodings.
The exact-word report resolves the apparent `f4` lead: `f491` ROL A accounts
for 1,572,960 of those remaining executions and already has carry/guard
assertions. In `f0`, `f050` and `f065` XOR-immediate forms account for
786,564 and 786,540 executions and are likewise asserted. Fixture-only
`f165` now checks the two-cycle, shifted-immediate XOR across accumulators:
it updates B from A while preserving A's guard and carry. The report does
not establish the other `f0` encodings or all XOR status variants.
Fixture-only `f43f` (`SUB A>>1,A`) now checks the accumulator arithmetic
family's right-shift fill under both SXM states: a negative 40-bit source
sign-fills when SXM is set and zero-fills when clear. Both cases assert TI
SPRU172C's one-cycle cost. This does not establish the remaining shift
amounts, source/destination combinations, or status variants.
Exact `47f8` (`RPT *(absolute)`) now checks the three executions of its
repeated instruction and the four-cycle setup cost specified by TI SPRU172C.
The `4782` indirect form checks the three-cycle base cost and unchanged AR2.
The core previously charged only its default single cycle to both forms.
Observed `7d92`/`7c92` program-memory moves now have exact-word direction,
pointer, and cycle assertions. TI SPRU172C gives `MVDP` four cycles and
`MVPD` three, with one additional cycle for absolute Smem; the `7df8`/`7cf8`
fixtures check that surcharge and extension-word order. A repeated `7c92`
fixture checks three distinct program source words, consecutive data addresses,
and one cycle per move after the first. Previously `MVPD` reread its initial
program word on every repeat iteration. These checks do not establish bus
wait-state timing or untested addressing variants.
Exact `7f92` (`WRITA *AR2+`) now checks the A-addressed program write, pointer
update, unchanged accumulator, and TI SPRU172C's five-cycle base cost. Exact
`7ef8` (`READA *(absolute)`) checks the reciprocal read and six-cycle
absolute cost. A repeated `7f92` fixture checks three consecutive program
writes and the documented one-cycle rate after the repeat pipeline starts.
The shared handlers previously charged only one cycle per transfer. Other
Smem addressing variants and external-memory wait states remain unaudited.
Immediate stores have exact `771a` (`STM #lk,BRC`), `76f8`
(`ST #lk,*(absolute)`), and `7682` (`ST #lk,*AR2`) result and cycle assertions.
TI SPRU172C specifies two cycles for the MMR and indirect Smem forms, and
three for absolute Smem. The core previously charged one cycle to all three.
The absolute fixture also checks address-before-immediate extension order.
Exact `4bf8`/`8bf8` stack fixtures now check an absolute-source push and
absolute-destination pop, SP round trip, and TI SPRU172C's two-cycle cost.
The core previously charged one cycle to both. Non-absolute forms retain the
documented one-cycle charge; long-offset addressing is still unaudited.
Exact `4a06`/`8a06` now check a one-cycle ST0 push/pop through the MMR
stack path. The saved word and SP round trip match TI SPRU172C; no core
change was needed. The ROM4 trace exercises long-offset `MAR`, covered by
exact `6dea`. The shared Smem read/write path now consumes MOD12-14 extension
words and applies no-update, signed preupdate, or circular preupdate as
specified in TI SPRU131G table 5-4. Fixture-only `06ea`, `02e2`, `02f2`,
and `80ea` check the three read modes and a long-offset store, including
the ALU/STL extra cycle. Handlers that compute their own memory addresses
still need separate long-offset review. Fixture-only `6bea` checks ADDM:
the address displacement precedes the immediate, preupdate reaches the
destination and the instruction costs three cycles. Original MU4 `6be6`
context-counter updates exercise the same ordering during erased-media mount.
Fixture-only `76ea` checks long-offset `ST #lk,Smem` with displacement before
immediate, preupdate and three-cycle cost; original `76e1` context setup
independently uses that encoding. The existing `68e2/69f2` ANDM/ORM cases
check addressing and timing, but their extension order still needs independent
machine-code corroboration rather than reliance on synthetic fixtures.
Other manual-address handlers remain open.
Fixture-only `82ea` and `83e2` assert the two-cycle long-offset surcharge
for `STH A/B,Smem` and the distinct preupdate/no-update addressing behavior.
Fixture-only `8cea` checks the same surcharge for `ST T,Smem`.
The direct `LD`/`LDU` accumulator handlers now charge that surcharge too.
An arithmetic decoder audit removed shadowed exact-case switch arms for
`f000`/`f300` ADD, `f010`/`f310`/`f210` SUB, several immediate AND/OR forms,
and `f0c8`/`f3c8` shifted XOR. The earlier generic ALU and shifted-XOR paths
already own these encodings; the dead arms omitted some flag or cycle handling
and could be mistaken for live behavior. Existing exact-word fixtures and the
cross-ROM gate establish that deleting them does not change execution.
Fixture-only `10ea`, `12e2`, and `45ea` distinguish sign extension, unsigned
loading, 16-bit shift, address update, and two-cycle long-offset timing.
The one-word Smem arithmetic/logical handlers (`ADD`, `ADDC`, `SUB`, `SUBS`,
`AND`, `OR`, `XOR`, and `SUBC`) now charge the extra cycle TI SPRU172C assigns
to absolute addressing. Exact `07f8`, `1af8`, and `08f8` fixtures check an
arithmetic carry input, a logical result, a signed subtraction result, and
their two-cycle costs. The core previously charged one cycle to these
absolute forms. The shared ALU operand path now handles long-offset forms,
but this does not establish every flag and accumulator variant in the family.
An indirect ROM4 sequence now asserts `0883` signed SUB under SXM,
`1d83`/`1c83` unextended XOR source words, and `1c93` postincrement against
two distinct memory operands. The four arithmetic operations take four
cycles in aggregate as specified by TI SPRU172C; `7713` changes the source
address between XORs and adds its separate two-cycle cost. Individual flag
variants and unexecuted addressing forms remain unaudited.
The Smem multiply/MAC handler now charges TI SPRU172C's extra cycle for an
absolute operand. Exact ROM4 `2494` checks an unsigned `MPYU` product,
postincrement, and one-cycle indirect cost. Fixture-only `24f8` checks the
same product and the two-cycle absolute form. ROM4 did not execute `24f8`
in this capture; this fixture tests a family rule, not observed boot traffic.
The `0x7100` handler is `MVDK Smem,dmad`, not `MVDM`; exact ROM4 `7192`
now checks source postincrement, destination data, and the two-cycle base
cost. `MVKD` now advances its immediate source address on repeated transfers;
exact `7093` checks three distinct words, consecutive data destinations, and
the one-cycle repeat rate after the first move. The existing exact `70f8`
handler and fixture continue to establish that absolute variant's extension
order and three-cycle cost; this pass did not change that special encoding.
Absolute one-word load/store forms now charge TI SPRU172C's extension cycle.
Exact ROM4 `44f8`, `80f8`, `82f8`, and `8cf8` fixtures check shifted load,
low/high accumulator stores, T-register store, and their two-cycle costs.
The correction covers the corresponding A/B shifted-load and STL/STH paths;
it does not claim generic long-offset Smem support.
Exact `71f8` now checks absolute-source `MVDK` address-before-destination
extension order, data movement, and TI's three-cycle cost. A repeated
`7192` fixture checks three distinct source and destination addresses and
the one-cycle rate after its first two-cycle move. Both passed on the
existing core; this was a coverage and contract check, not a behavior fix.
The live extended `0x6f` Smem decoder now charges TI's two-cycle indirect
and three-cycle absolute costs. Exact ROM4 `6f8a`/`6ff8` fixtures check
shifted-load data, pointer/extension consumption, and both costs. The later
`case 0x6f00` arm was unreachable behind the earlier unconditional decoder
and has been removed. Exact `6f82`/`6f83` fixtures check shifted ADD/SUB;
`6f92`/`6f93` check shifted low/high stores, indirect pointer updates, and
two-cycle indirect costs. Exact ROM4 `6f8b` additionally checks SXM extension,
postdecrement, extension consumption, and the two-cycle cost. Other shift, flag, and long-offset variants remain
unasserted.
TI SPRU307A's opcode table assigns `LMS Xmem,Ymem` to `E1xx`, one word and
one cycle. The detailed opcode diagram in SPRU172C prints an `F0`-shaped
pattern, which conflicts with that table and live `F0xx` control/ALU
encodings; the decoder uses `E1xx`. Fixture-only `e13a` checks TI's LMS
example values for A and B, unchanged T, dual-address update, and one-cycle
cost. No observed 5110 trace executes this family, so this is instruction
coverage, not evidence about the ROM4 boot path.
TI SPRU307A assigns `MACSU` to `A6xx/A7xx`, one word and one cycle.
Fixture-only `a6ab` reproduces TI SPRU172C's unsigned-X positive-product
example; `a7ab` additionally checks signed-negative Y, FRCT doubling, the B
destination, unchanged A, T update, and both pointer increments. Neither
encoding appeared in the observed 5110 trace union.
The `78xx/79xx` MACP and `7axx/7bxx` MACD program-memory multiply families
now have TI SPRU172C example fixtures (`788b` and `7a8b`). Both check the
three-cycle base cost; MACD additionally copies the source word to its
successor, while MACP leaves that word alone. A repeated `7893` fixture
checks consecutive program coefficients, T and AR3 updates, and the
one-cycle rate after the first multiply. These are fixture-only encodings;
other addressing and status variants remain unasserted.
Fixture-only `798b` and `7b8b` extend those assertions to destination B:
they check A preservation, B's signed product, T publication, AR3 decrement,
the MACP/MACD successor distinction, and the same three-cycle base cost.
The `79` and `7b` decoder groups therefore no longer have zero fixture words;
neither group is observed in the current ROM4 trace union.
Fixture-only `79f8`/`7bf8` check absolute Smem address before program
coefficient, while `79ea`/`7bea` check the opposite long-offset extension
order, AR2 preupdate, and the successor write only for MACD. All four check
TI SPRU172C's four-cycle cost for a three-word form. These tests do not
establish external-memory wait states or every repeat/address combination.
TI SPRU307A assigns `FIRS` to `E0xx`, with a three-cycle first pass and
one-cycle repeat rate. As with LMS, the detailed SPRU172C opcode diagram
conflicts with the opcode table and live `B0xx` MAC encodings; the decoder
uses `E0xx`. Fixture-only `e03a` checks TI's X/Y sum into A, previous-A
coefficient multiply into B, unchanged T, pointer update, and three-cycle
cost. A repeated case checks coefficient progression, use of the previous
A value, and one-cycle steady rate. The observed 5110 trace union does not
execute FIRS; other status and addressing variants remain unasserted.
The dual-memory `A0xx/A1xx` ADD and `A2xx/A3xx` SUB families now have
fixtures for both accumulator destinations, shifted X/Y results, pointer
updates, and TI's one-cycle cost. `E3xx` ABDST reproduces TI SPRU172C's
absolute-distance example and a separate SXM-negative/FRCT case. These are
fixture-only families in the current 5110 trace union; overflow and shared-AR
variants still need separate evidence.
TI SPRU307A assigns `LTD Smem` to `4Cxx`; `4c83` now checks the SPRU172C
source/T/successor example and one-cycle cost. `4c93` checks postincrement
after the successor write. These fixture-only encodings guard the standalone
delay primitive used by program-memory MACD. Fixture-only `4cf8` and `4cea`
also check absolute and long-offset addressing: T receives the selected
word, its successor receives the same word, AR2 preupdates only for the
long-offset form, and both cost two cycles. Other addressing and bus
wait-state combinations remain unasserted.
TI SPRU131G defines `Smem` bit 7 clear as direct addressing: with
`ST1.CPL=0`, `ST0.DP` supplies the page and the operand supplies a seven-bit
offset; with `CPL=1`, the address is `SP+offset`. The core formerly treated
those operands as AR-indirect. Fixtures now check DP-relative load/store and
`ADDM`, SP-relative load/store and `LTD`, their cycle costs, and unchanged ARs.
The hand-written delay, long-word arithmetic, read-modify-write, MACP/MACD,
and program/data move paths share that direct-address calculation. Existing
AR-increment fixtures were corrected to use bit-7-set encodings. DP/CPL
write-pipeline timing and external-memory wait states remain unmodeled.
Fixture-only `2045` and `2945` check the arithmetic side of the same boundary:
DP-relative `MPY` produces a signed product, SP-relative `MAC` accumulates a
negative product into B, both preserve AR5, and both cost one cycle. They do
not cover every multiplier status or overflow variant.
Fixture-only `26ea` checks TI SPRU172C's two-cycle long-offset `SQUR` form:
the signed source squares into A, T receives that source, and AR2 preupdates
by the extension offset. Earlier `2682` and `f58d` fixtures already assert
FRCT/OVM saturation for short Smem and accumulator-source SQUR respectively;
`3882` asserts SQURA's carry preservation. Those assertions do not establish
every status and source/destination combination.
TI SPRU172C's direct-address examples for `SQUR 30,B`, `SQURA 30,B`, and
`SQURS 9,A` now have exact fixtures (`271e`, `391e`, `3a09`). They check DP
selection, the documented accumulator and T results, and the one-cycle
short-operand cost. FRCT, OVM, and other source/destination variants are not
inferred from these examples; the separate status fixtures above supply the
specific FRCT/OVM evidence.
Fixture-only `0605` and `0e05` check direct-page carry arithmetic under SXM:
`ADDC` zero-extends `ffffh` before adding incoming C, while TI SPRU172C's
`SUBB 5,A` example subtracts the inverse of C and leaves the 40-bit result
at `ff ffffffff`. Both assert outgoing C and the one-cycle short-Smem cost.
Fixture-only `6b94` reproduces TI SPRU172C's saturated `ADDM` example:
`8007h + fff8h` stores `8000h` under SXM/OVM, sets OVA, advances AR4, and
charges two cycles. The fixture does not establish every RMW flag combination.
The TI SPRU172C opcode diagrams were checked from the PDF page image, not its
scrambled extracted text: `e5xx` is `MVDD`, while `ccxx..cfxx` is the distinct
`ST src,Ymem || MPY Xmem,dst` family. The core now decodes that parallel
multiply. Fixture-only `cdb9` reproduces TI's FRCT example (old A stored,
`4000h * 4000h` into B, both ARs advanced, one cycle); `cfb9` checks that
a same-register store uses old B before the multiply replaces B. These tests
do not establish the other parallel store arithmetic families or all ASM/SST
and overflow variants.
TI's adjacent opcode diagrams also assign `c0xx..c3xx` to `ST||ADD`,
`c4xx..c7xx` to `ST||SUB`, `c8xx..cbxx` to `ST||LD` into an accumulator, and
`e4xx/e6xx` to `ST||LD` into T. The core now implements those forms through a
shared pre-operation store path. Fixture-only `c131` checks the opposite
accumulator as the ADD source; `c5f5` reproduces TI's signed `ST||SUB` result,
ASM-shifted store, and circular pointer update; `ca31` checks signed load into
A; `e411` checks that Xmem is read before the same cell is overwritten as
Ymem. Each asserts the one-cycle DARAM cost. Store-side carry and hardware
memory-bank wait states remain outside these fixtures. A further `c131` OVM
fixture checks saturated B, sticky OVB, cleared carry, and the unaffected
pre-arithmetic store. A `c531` OVM fixture checks negative saturation, sticky
OVB, set carry, and the opposite subtraction order (`Xmem<<16 - A`).
The TI SPRU172C `ADD`, `SUB`, and `LD` diagrams also assign `04xx/05xx`,
`0cxx/0dxx`, and `14xx/15xx` to the single-memory forms shifted by TS, the
signed shift quantity in T's low six bits. These groups were absent from the
decoder. The core now applies SXM to the memory source, shifts by TS, and
charges one DARAM cycle (plus the existing long-offset/absolute surcharge).
Fixture-only `0483` and `0d83` check positive TS with ADD/SUB carry behavior;
`1583` reproduces TI's signed `LD *AR1,TS,B` example using `fedc` shifted by
eight; `1483` checks arithmetic right shift for TS=-1. Exact `0583` additionally
checks B-destination ADD with SXM, TS=-1 encoded in `T=abff`, unchanged A,
T and AR3, and one-cycle arithmetic. This guards sign fill and ignoring T's
upper ten bits independently of the load variant. Exact `0c83` checks the
complementary A-destination SUB with SXM clear and the same negative TS:
zero-filled memory, no-borrow carry, unchanged B/T/AR3 and one-cycle arithmetic.
These fixtures do not establish out-of-range TS values or memory wait states.
SPRU172C's adjacent `ADD/SUB Xmem,SHFT` diagrams assign `90xx/91xx` and
`92xx/93xx` to the single-X dual-address forms. The core now reads Xmem,
applies SXM and the four-bit left shift, updates the selected accumulator and
its X pointer, and charges one DARAM cycle. Fixture-only `9083` checks an
X-pointer postincrement and positive ADD; `9304` checks signed SUB, borrow,
unchanged other accumulator, and the unmodified-pointer form. Neither opcode
appears in the observed 5110 trace union; other X modes and overflow cases
remain unasserted.
TI SPRU172C also assigns `f486/f586` to `MAX A/B` and `f487/f587` to
`MIN A/B`, each one word and one cycle. The core now compares signed 40-bit
accumulators, copies the chosen value to the destination, and clears C when
A wins or sets C when B wins. In a MIN tie, TI's example chooses B. Fixtures
assert both destinations, negative values, tie selection, guard-byte
comparison, C, and one-cycle timing. These four words are not in the current
5110 trace union.
TI SPRU172C's `EXP` (`f48e/f58e`) counts redundant sign bits in the full
40-bit accumulator, subtracts eight, and writes the signed exponent to T;
zero writes T=0. `NORM` (`f48f/f58f/f68f/f78f`) shifts the selected source
by T's low-six-bit signed TS and writes the selected destination, applying
SXM and OVM. Both are one-cycle instructions. Fixtures reproduce TI's EXP
examples for -53 (T=25) and a positive guard-byte value (T=-4), and its
NORM examples for A shifted left by 19 and B shifted right by seven into A.
Additional cases assert EXP zero, NORM saturation/overflow, carry preservation,
and timing. Neither family occurs in the observed 5110 trace union; TS values
outside TI's -16..31 range remain unasserted.
TI SPRU172C assigns `RND` to `f49f/f59f/f69f/f79f`: it adds `8000h`
to the selected source, clamps on OVM, writes the destination, and does not
change status flags. `SAT A/B` (`f483/f583`) instead clamps the signed
40-bit source to 32 bits regardless of OVM and sets or clears the destination
overflow bit according to whether clamping occurred. Both cost one cycle.
Fixtures reproduce TI's negative-source and OVM RND examples and all three
SAT examples (positive overflow, negative overflow, and in-range); they also
assert carry preservation and the unaffected other accumulator. These are
fixture-only words in the current 5110 trace union.
TI SPRU172C's multiply table assigns `62xx/63xx` to `MPY Smem,#lk,A/B`
and `f066/f166` to `MPY #lk,A/B`. Both use signed 16-bit operands and FRCT,
and the Smem form publishes its memory operand to T. They cost two DARAM
cycles, plus one for long-offset or absolute Smem. Fixture-only `6283`
checks FRCT, T, and two-cycle timing; `f166` reproduces TI's signed
`MPY #fffe,B` example and preserves T; `62f8` checks address-before-immediate
extension order and the absolute three-cycle cost. `629b` checks that the
preincrement updates AR3 before reading Smem. Long-offset word order, other
addressing modes, and multiplier saturation variants remain unasserted.
SPRU172C specifies A(32–16) as a signed **17-bit** multiplicand. The existing
`MPYA/MACA/MASA` Smem path had narrowed it to `s16`, reversing the product's
sign when bit 31 was set but bit 32 was clear. A shared 17-bit extraction now
serves those forms, FIRS, POLY, ABDST, SQDST, and accumulator-source SQUR.
The T-source `MPYA` (`f48c/f58c`) and `MACA[R]/MASA[R]`
(`f488..f78b`) forms are now decoded at TI's one-cycle cost. Fixtures
`f58c`, `3183`, and `f488` distinguish the 17-bit result from the old 16-bit
result; `f48b` checks rounded subtraction, while `e211` and `f48d` check
the shared high-word rule for SQDST and SQUR. Other FRCT/OVM boundaries and
the remaining source/destination combinations are not established by these
fixtures.
TI SPRU172C's `ADD/SUB src,ASM,dst` diagrams assign `f480..f780` and
`f481..f781` to the one-word, one-cycle accumulator forms. They were absent
from the decoder despite the corresponding immediate-SHIFT forms already
working. The core now shifts the selected source by ST1.ASM with SXM-controlled
right fill, then performs the signed addition/subtraction into the selected
destination. Fixture-only `f480` checks negative ASM, sign fill and carry;
`f681` checks B-to-A subtraction, unchanged B and borrow. Additional selected
overflow boundaries are covered below; other ASM values remain unasserted.
An additional exact `f480` vector checks ASM=3: A=1 becomes 9, B's full
40-bit sentinel remains unchanged, carry stays clear, and the instruction
retains its one-cycle cost. The native `tms54test` suite passes after this
extension (`run_c54x_asm_core_retry_20261009.log`). This asserts one positive
left-shift outcome, not all ASM values, saturation, Nokia RF behavior or
native speech. The suite deliberately exits via its completion exception
after printing `TMS320C54x core conformance: PASS`.
Four additional ASM=0 `f480` vectors cover positive and negative 32-bit
overflow with OVM clear/set: guard-preserving 40-bit results versus signed
32-bit saturation, A-owned sticky overflow, low-word carry, unchanged B and
one-cycle cost. `check-c54x-core` requires their dedicated pass marker.
The complete native suite reproduces in
`run_c54x_asm_overflow_core_20261009.log`; these selected outcomes do not
establish other accumulator selections, ASM values or speech behavior.
Four exact ASM=0 `f581` (`SUB A,ASM,B`) vectors independently cover positive
and negative overflow with OVM clear/set, B-owned sticky overflow,
borrow/carry, unchanged source A and one-cycle cost. The dedicated subtraction
marker and whole-core conformance pass in
`run_c54x_asm_sub_core_20261009.log`. ADD's A-owned result is not substituted
for this cross-accumulator test. Nonzero subtraction shifts, all other
operand/status combinations and native RF/speech remain outside this claim.
Re-run
`make check-c54x-opcode-coverage LOG=<rom4-log> ROM4_IDLE=1` to check the
opcode-set fingerprint; add `FIXTURE_LOG=<core-log> GROUPS=1` to rank
unasserted words and high-byte groups by observed execution count.
`make check-c54x-cross-rom`
runs the 3210 frontier, 5110 menu, and 3310/3330/3410 idle gates in order.

The retained NSE-1 trace fixes reset polarity and edge behavior without an
inference: MCU writes to MAD2 byte `0x20002` are `1` (cold release), `0`
(hold), `0`, then `1` (warm release). A later value `3` leaves the DSP running.
MAD2 now exports this product-mask-derived level to the backend; the C54x
implementation drives the CPU reset input so CPU state restarts while the
external DSPIF/on-chip DARAM stores survive the warm transition.

## Resolved flat-image dependency

Before the decoder correction, sampled words at
`0x2000`, `0x216a`, `0x2286`, `0x23c3`, `0x2470`, `0x25b4`, and `0x27ff` are all
zero. Subsequent demand loads populate catalogue-owned ranges, but the gap
`0x23c3..0x25b3` is outside every declared destination. Without the assist the
co-sim completes 70 rather than 74 DSP acknowledgements and does not reach a
stable UI.

The gap's bytes occur in MCU flash, but location alone is not a destination
proof. In particular, the bytes matching `0x23c3..0x25b3` immediately follow
descriptor 0, whose declared output destination is `0xfd00`. They may be
encoded input, shared source material, or an artefact of how the recovered flat
image combined address spaces.

The producer is loader1 at `0x0f1f..0x0f28`. It computes a count from the
`0x23c4..0x252a` bounds, selects source `0x0d00`, and executes repeated MVDP to
program destination `0x23c4`. With `PMST.OVLY=1`, those program stores must
resolve to the same DARAM cells used by program fetches.

The external experimental core originally wrote MVDP directly to an immutable
program store, bypassing its overlay helper. MAME's backend already routes
program writes through the DSPIF overlay, but initially repeated the encoded
program destination without the C54x's repeat-mode address update. Advancing
that destination per repetition populates the resident range organically. The
external unassisted run's 74 acknowledgements and stable UI remain the complete
ROM4 reference; MAME has now independently crossed the same loader boundary.
This is an instruction-semantics correction, not a reconstructed image or
timing adjustment.

## Delayed conditional return

The core implements `0xfe44` as `RCD ANEQ`: test the full accumulator A
before the two following delay words, pop the return address only when A
is nonzero, and charge three cycles on either path. This follows
[TI SPRU172C, RC[D], pages 4-133/4-134](https://www.ti.com/lit/ug/spru172c/spru172c.pdf).
The executable core fixture tests both paths, including a delay-word write
that changes A after the decision, stack preservation on a false condition,
and port-to-port cycle counts. Separately tested `0xfe4c` (`RCD BNEQ`) uses
the same documented three-cycle contract for accumulator B. Both paths test B
before delay words overwrite it and preserve or pop the stack as appropriate.
This is manual-derived core conformance, not an observed handset requirement
or interrupt-pipeline timing evidence. Other conditional delayed-return
encodings are not implied by these two words.

ROM4 uses this instruction at `0x90eb` when additional demand-load
continuations are delivered through BSCR.HINT. The bounded NSE-5 investigation
documents that signal, accepted counter/compare model and research-only overlay
in [7110 bring-up](7110_bringup.md#diagnostic-publication-and-partial-code-block-transfer).
