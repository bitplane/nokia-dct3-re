# ROM4 DSP loader contract

The native boundary profiles and the staged-frontend prototype have different
acceptance scopes. The staged frontend reaches the first organic
host-packet decoder, records twenty-two consumer advances across observed
initialization/service passes. The frame-buffer polling loop, delayed TC
branch inside block `473c..4777`, conditional delayed return and uploaded
`ADDM #1,BRC` now execute. The clean cold-boot and lifecycle gates pass;
a physical Menu press opens the existing byte-exact Phone book oracle.
The passive ten-second checkpoint does not by itself prove boot. Physical
menu access has its own gate; RF acquisition, native calls and speech remain
unaccepted. The separate [processor qualifications](c54x_execution_contracts.md)
do not promote those claims. See
[Current result](#current-result) for the established loader lifecycle and
[Observed lifecycle](#observed-lifecycle) for the bounded live checkpoint.

The staged profile remains opt-in. Next work is broader native lifecycle and
frontend/bus comparison, rather than inventing a message to escape a CPU guard.
Absolute data address `0012` is AR2, not external RAM.
Parallel-store carry and physical bus phases remain unvalidated.

Processor execution and regression contracts are maintained in
[C54x execution contracts](c54x_execution_contracts.md),
separately from this handset/loader boundary.

## Current result

The shared physical-input harness selects NSE-1 keypad/LCD handling from the
declared `noki5110` parent, including research aliases. The atomic standard-bus
profile (`nse1astd`) passes the unassisted Phone book menu gate. This does not
extend to staged/legacy `nse1plg`, whose existing loader guard remains open.
On `nse1astd`, physical `1,1,2,Navi` reaches the firmware's "Attempting emergency
call" screen. An isolated 18-second run and an empty-input control both record
zero entries to codec wrappers `9c4f/9cb9`, active mode `1836=0`, and no DSP
illegal-instruction flag. This is call-attempt presentation, not registration,
speech activation or a connected call; no external network backend is enabled.
Read-only MCU write taps and DSP data-read taps narrow this boundary further:
after Navi, MCU shared word `100a8` publishes `8626`, `9204`, then `9ec4`
at approximately 9.755, 9.755 and 9.773 seconds; the DSP reads those same
values at data word `0854` (read-time PC context `27f2`). The preceding digit
keys publish the distinct `920c/920f` tone-control dialogue. An empty-input
control instead publishes `8406/8002/900f` at approximately 11.73 seconds.
DSP I/O write taps then observe `2d=0626,2c=0008` for `8626`,
`2d=0204,2c=0009` for `9204`, and `2d=0ec4,2c=0009` for `9ec4`.
The first two also bracket a register-8 read (`2c=0018`) and rewrite of
`0626`. These are the recovered COBBA control-data/select transactions,
not a speech-channel assignment. Thus the call-attempt controls cross the
mailbox and reach the codec-control device; zero speech-wrapper entries
cannot be attributed simply to their loss in transport. Analog meanings of
the register bits and the prerequisites for speech-channel activation remain
unresolved. Read-time PC context is not an instruction-retirement or
physical-timing claim.

Reproduce this passive instrument with
`.venv/bin/python tools/run_c54x_rom4_call_observe.py --machine nse1astd`;
use `nse1pipe` for staged/standard execution and `--keys ''` for an empty-input
control. Each run has isolated generated storage, input/binary/observer hashes,
its launch command, console, bus log and snapshots under `tmp/native-call-*`.
Inherited DCT3 fixture/backend settings are removed. This is a manual research
instrument, not a connected-call gate: a zero process exit is not acceptance.
The runner writes `summary.json` and rejects observer errors, missing/duplicate
summaries, wrong-machine/faulted completion, invalid masks/times and truncated
or count-inconsistent bus captures. Each kind reports its total and 512 cap.
Completeness is not semantic acceptance: independently inspect the physical
input/screen and transaction evidence before drawing a behavioral conclusion.
Both standard-bus frontends reproduce 68 post-boot host writes, 18 payload
reads and 75 COBBA writes for this dial fixture, with zero codec entries and
active mode zero. Equal counts do not establish physical timing parity.

The current native boundary profiles are distinct: the four-second coherent
boot observes thirteen ordered RF control writes, including the firmware-owned
`0041/0040` queue item released by TX-completion FIQ1. The 30-second RF
gate observes the later multi-entry scan in progress; a
60-second passive run proves its return at 36.156 seconds, frame-period
restoration and ordinary receive/FIR queue consumption. Neither establishes
RF acquisition or handset speech. See
[the current native boundary contract](djr_dsp_integration.md#native-speech-activation-and-pcm-buffers).
Three-write and 6,499-frame measurements retained below describe the older
pre-TX-FIQ1 baseline, not the current acceptance profile.

The NSE-1 v5.30 MCU drives a real staged DSP boot. A cold transfer runs through
loader1 at DSP address `0x0f00`, the MCU asserts DSP reset, and a second release
preserves DARAM. Loader1 then requests block `0x12`; the matching descriptor
installs loader2 at `0x0d80`. Loader2 enters the run-mode dispatcher, which
requests further blocks by catalogue index.

This result no longer requires the compatibility path that copied words from a
flat DSP image into `0x2000..0x27ff`. Its old explanation was wrong: loader2
does not clear that range. Loader1 itself populates the resident branch table
with a repeated MVDP data-to-program transfer.

## ST1 memory-access constraints

[SPRU131G table 4-2](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
requires bit 10 to read zero and prohibits memory writes from setting
INTM. The shared CPU register map enforces both: full/partial writes cannot
cause an INTM 0-to-1 transition, and reserved-bit writes/imports cannot
produce bit-10 readback. Data-bus taps still see the requested write word;
register readback applies the hardware constraints. Debugger setup and
save-state restoration can seed architectural INTM independently of the
firmware memory-write path. Dedicated SSBX/RSBX and interrupt-entry/return
operations retain their explicit INTM behavior.

The passage does not explicitly resolve whether a memory write of zero
may clear INTM. That behavior is retained from the legacy model and is
**unverified**, not promoted to a silicon contract. No C55x compatibility
rule or another emulator's implementation settles this C54x question.
Staged STLM A/B to ST1 uses this same constrained register map.

`st1-rules` exhausts 65,536 input words, both old INTM states and four
write masks (524,288 cases), checking readback, other fields, XF and one
save-state restoration. Its marker labels the retained clear policy
`legacy_unverified`. `st1-bits` separately checks 128 executable status-bit
cases plus replay: SSBX still sets INTM, and bit 10 remains zero. The
576-case MVMD IRQ/NMI fixture covers four **requested** INTM pairs, not
four effective transitions; write-one cannot mask an already unmasked CPU.
These gates establish the declared register model, not independent silicon
measurement of the unresolved clearing behavior.

The DST regression uses PMST.SST and ST1.SXM for signed saturation; ST1
bit 10 is not a saturation control. Its overflow inputs have low words
different from the clamped result, so a disabled clamp cannot satisfy the
data oracle accidentally.

### Staged immediate status publication

`STM #literal,ST1` retains its second word in Decode and publishes once at
Read, without reading the destination or applying SST saturation. The
`stm-st1` fixture passes 18,432 cases and cleared-pipeline replays over raw
status, SXM loads and ASM load/add/subtract consumers, four gaps, two pages
and three checkpoints. Poisoning the consumed literal cannot change the
publication. Reserved-bit, XF and INTM handling uses the existing register
contract; zero-write INTM clearing remains explicitly unverified.

`stm-cpl` and `stm-cmpt` each pass sixteen cases plus replay for basic LD/ST
followers: two directions and gaps 0/1/2/3. They select the new addressing
mode after two separating instructions, matching SPRU131G tables 7-16/7-19.
The tested SXM/ASM followers need no separating instruction. Sixteen negative
cases plus replay reject delay, short-repeat, block-body and XC compositions
before data effects. Other status-producer hazards, interrupt composition
and physical bus-stage timing are not qualified by these fixtures.

### Staged accumulator-to-status publication

STLM A/B to ST1 publishes the Execute-live, SST-qualified low word once
through the data bus, independent of DP/CPL, without reading the destination.
The `stlm-st1` fixture checks 18,432 cases with replay: both sources, raw
status words and negative-source saturation, SXM loads, ASM register
loads/add/subtract, long-offset SXM loads, four gaps, two program pages and
three save points.
It mutates the source before Execute to distinguish live publication from
an incorrectly retained Decode operand.

SPRU131G tables 7-20/7-22 require one separating instruction for short
SXM/ASM consumers. Their Read-owned mode sample is retained across Execute
and save/load; the long-offset load supplies its own separation. Dedicated
ASM loads still forward with zero latency. This qualification covers the
tested load/register-ASM families, not every arithmetic/multiply consumer.
The `stlm-cpl` and `stlm-cmpt` profiles each check 16 cases with replay:
basic load/store consumers retain the old addressing mode with zero to two
NOPs and use the new mode with three (tables 7-16/7-19).

`stlm-st1-irq` checks 576 cases with replay, using B, a separating NOP,
IRQ/NMI, all requested INTM pairs, IMR enabled/disabled, two follower forms,
three arrival clocks, two pages and three save points. It requires one
status publication, no source-memory read, at most one interrupt frame
and exactly one foreground marker. It does not inherit the special
SSBX/RSBX INTM guard or settle the unverified clearing policy.

The `stlm-st1-edge` fixture rejects 64 cases with replay before data-side
effects: delay slots, single/block repeat, XC, and adjacent RPTB/RPTBD,
including adjacency across the 16-bit PC wrap on both tested pages.
The adjacent-repeat case is a documented BRAF overwrite hazard (7.5.7.1),
not a safe composition or an emulated hardware trap. Other excluded cases
remain implementation boundaries, not claims that silicon forbids them.

## Shifted peripheral operand reads

The staged `LD Smem, SHIFT, A/B` path admits peripheral sources
`0020..005f` through the ordinary data bus. It retains the computed address,
address/operation words and source operand, paying the existing one-clock
peripheral hold before Read and publishing in the following Execute clock.
This follows the peripheral-access cost in classes 4A/4B, pages 3-7/3-8
of [SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf), not an
independently measured placement of the peripheral bus cycle.

`shift-periph` checks 245,760 cases with replay: all 64 peripheral addresses,
shifts -16/-1/4/15, DP/SP/absolute/postincrement/long-offset addressing,
SXM/OVM, A/B, four values, two pages and three snapshots. These are volatile
synthetic endpoints, not assertions about the identity of every mapped
register. Literals are poisoned after Decode and postincrement's pointer
is changed after Access; the original address and word must still be used.

`shift-bsp` checks 40,960 cases with replay against the existing completed-word
BSP state, using all 32 shifts and an additional post-Execute snapshot.
It starts with receive-ready and overrun, requires exactly one read to clear
both, then supplies a second frame which must remain pending after Execute
and replay. Debugger peeks cannot acknowledge either frame. This validates
CPU/register composition, not a physical serial clock or handset speech.

Mapped shifted ADD/SUB remain outside this admission, as do ST1/BRC sources
and the tested repeat/block/XC/delay compositions. The 64-case
`shift-per-edge` fixture requires rejection before data-side effects and
reproduces it across save/load. These are implementation boundaries, not
claims that real silicon prohibits those instructions.

## Repeat endpoint and page hazard

The C54x repeat-end comparator is 16-bit, not XPC-qualified. An active
loop can be consumed by code on another program page if its low PC reaches
REA; an RSA reload retains that code's current XPC. TI documents this
extended-addressing hazard in [SPRA599A, section 2, page 4](https://www.ti.com/lit/an/spra599a/spra599a.pdf).
Do not make a repeat loop page-private to protect a caller or ISR. The
instruction-reference restriction on far calls inside repeat blocks does
not specify a page-qualified comparator or a hardware trap, and does not
justify importing the C55x compatibility mode's automatic BRAF clearing.

The `rptb-page` fixture checks 384 cases with replay: both atomic and staged
frontends, BRC 0/1/2/7, caller pages 0/2/127, foreign-page REA collisions
and safe controls, FCALL/FCALLD, and four save points. In the colliding case, a foreign
callee consumes the remaining loop count and repeats at its own page's
RSA. Caller iteration counts, foreign prefix/endpoint counts, BRC/BRAF,
SP, return PC/page, ordered two-word frames and once-only delay effects
are checked independently.
The former staged page qualifier suppresses this result; the atomic
frontend already follows the 16-bit comparator. This is a manual-derived
regression, not a silicon timing measurement or permission to admit every
far-call/repeat composition.

```sh
python3 tools/run_c54x_core.py --profile rptb-page \
  --mame mame/mame --rompath roms --output-root tmp
```

The focused repeat/control profiles, native 5110 save-state gate and
MU4 cold saved-track catalogue gate pass. Delayed far-call/loop composition
has separate `rptb-fcalld` acceptance; none of these is full
[REL playback acceptance](5510_bringup.md#saved-track-playback-boundary).
The reference PDF is retained locally at
`roms/reference-docs/c54x/ti_tms320c54x_extended_addressing_spra599a.pdf`,
SHA-256 `fbbe43aa06609af2491e71213b19145f11db3d05169939daae5358efcea52432`.

## Extended program overlay

The staged frontend retains the physical program address selected by each
Prefetch slot in save state, separately from its logical PC. The isolated
`pmst-map`, `pmst-pop` and `pmst-prefetch` fixtures each pass 64 cases with
replay. The last deliberately changes debugger PMST state between Prefetch
and Fetch; it is CPU conformance, not a firmware-ordering fixture. The full
288-marker core run and native 5110 save-state gate pass. These results do
not certify all PMST fields or full interrupt composition. The `pmst-iptr`
profile separately passes 1,024 cases/restores across all 16 maskable IRQ
sources, early/late pending CPU latches, four PMST values and two source
pages. IPTR is selected at ISR Prefetch after decoded foreground writes,
while the accepted interrupt number is retained. NMI, ISR return and a
second OVLY change before vector Fetch are outside this fixture. Inactive
repeat-control restoration now passes 192 replay cases plus three active-loop
exclusions. POPM BRC followed by one NOP and RPTB passes 30 replay cases,
with independently checked count-plus-one output; active-loop writes remain
excluded. The staged SP-ordering correction lets FRETE at
`02:8b76` resume at the populated `03:9b08`; interrupt Access reserves
its slot after older foreground Read-owned stack updates. The focused
`stack-irq` gate passes 48 cases/restores, including IRQ0/NMI and three
snapshots; `stack-irq-return` passes 80 replays, adding RETE, caller cleanup
and snapshots of the retained return operand and returned caller. The
original readiness gate reaches 1. The mapped-ST1 LDU at `03:8f13` now
passes through the existing retained Read path, with 288 focused replay
cases and the existing 384-case IMR control. Short CMPM at `03:91eb` is
now admitted for direct RAM through the existing retained
operand path. Its 384-case DP/SP branch/replay fixture and 96-case absolute
control pass; mapped/peripheral and indirect comparisons remain excluded.
Immediate TIM/PRD/TCR writes now use retained literals and one peripheral
wait before Execute publication. The `timer-stm` profile passes 36 replay
cases checking stopped-timer register effects, writable masks and TRB reload;
it does not claim new running-timer timing validation. Original R060 now
executes conditional return `fc43` at `02:3e44` after the full signed
accumulator predicate family gained 768-case replay coverage. Repeated PSHD
at `02:3f7e`, EXP B (`f58e`) at `02:3fa8` and ADDS at `02:3fad` also
execute. Syntax-5 LD Xmem at `02:3fbc` and CALA B at `02:40e1` also
execute. Repeated MVDD (`e598`) at `02:a169` and absolute OR
(`1af8 b582`) at `03:abca` now execute too, as do both repeated POPD
restoration loops at `02:4062`/`02:4066`. Mapped AR/BK writes retain their
Read-stage ownership, including older-store conflicts. BACC A (`f4e2`) at
`03:9de4` now retains the accumulator target after READA's mapped write,
without allocating a call frame. Long-offset AND (`18e2 1462`) at
`03:9dff` now executes through retained address and source stages. Its
294,912-case replay fixture covers AND/OR/XOR, both accumulators, all
24 offset/preincrement/circular encodings, CMPT selection and BK=0/13.
Mapped/peripheral sources, repeats and control-delay composition remain
fail-closed, with 24 exclusion cases. Mapped MVMD (`730e 0011`, T to AR1)
at `03:9ba2` now executes too. T/TRN sources use the existing retained
move path; 432 replay cases cover all eight AR destinations and BK.
Producer/interrupt composition is outside that fixture. MVDD (`e501`) at
`03:9c70` now sends RAM `14b2=007f` to the bench's McBSP2 TX register
`0033`. RAM-to-peripheral transfers retain their operands and pay the
Class-14 extra cycle; 1,536 replay cases and ordinary/repeated RAM controls
pass. The original firmware emits the pin-decoded acknowledgement
`007f,0001,0055`. ABS A,A (`f485`) at `02:e5e5` and ABS B,B (`f785`)
at `02:e5e7` now execute, with 13,824 arithmetic/status/producer replay
cases and an unchanged NEG control. MPY (`62f8 0008 012c`, mapped AL
times immediate 300) at `03:95b5` now executes through retained
memory/immediate operands, with Read-owned T publication and 98,304 legal
replay cases; 256 additional replay cases reject write-only short MOD=3
in both execution frontends. The original recorder routine at `02:9e90` is entered and
output words `0580,0a09` are observed. Forward near B/BC inside
`02:a0ea..02:a0fd` now executes, with 12,288 IRQ/NMI/replay cases and
32 exclusion cases. Software INTR 25 (`f7d9`) at `02:9c74` now
executes, as does unsigned load `1281` at `02:9edc`. Its source `002a`
is backed by bench RAM, not an identified DA150 register. Same-location
STL/STH/T/TRN-to-load bypass is now covered by 288 replay cases, avoiding
stale stack locals and the resulting IMR corruption/default-vector loop.
Absolute ADD from BK (`01f8 0019`) at `02:e1e5` and STLM B,BK
and MVMD BK,AR0 are admitted. Long-offset STL `80e2 0001` at
`02:e206` fits the complete two-word delay of `f273 e209` at `02:e204`
and now executes, as does MVMD AR6,T (`7316 000e`) at `02:e109`.
Absolute MPY (`20f8 0017`, AR7 source) at `02:e10f` in FCALLD's
delay now executes. Smem CMPM now passes its retained addressing and
adjacent TC/NTC branch fixtures, and the original recorder overlay loads
and enters startup. The single authoritative staged execution stop is in
[5510 Current boundary](5510_bringup.md#current-boundary).
This is not a complete command transaction or native
audio, and the physical DA150 register map remains unverified.
The exact resident program/data comparison still passes; this is not full boot.
Single PSHD now reads CPU-register sources `0000..000f` through the
existing mapped Read path. The `pshd-mapped` fixture covers the twelve
owned registers with 288 Access/Read/Execute replays, retaining one source
word despite later source/pointer mutation. The `pshd-repeat` fixture adds
768 replay cases for short/long immediate repeats, four counts, upward and
downward RAM/MMR reads, two pages, and IRQ/NMI deferral with ISR return.
Each copy owns real Decode/Access/Read/Execute slots rather than using the
atomic stream shortcut. Ordinary DAGEN reads are admitted, but SP,
modifying self-AR aliases, peripheral sources and memory-count RPT remain
excluded. The original `4b97` increments AR7 (`MOD=2`); its first window is
BL..AR6. The decrementing form is `4b8f` (`MOD=1`).
`exp-t` adds 12,288 replay cases for both EXP sources, sixteen 40-bit vectors,
eight status configurations, optional adjacent LDU producers, two pages
and zero/one/two-gap T consumers. T publishes at Execute: an adjacent LDM
samples old T and a gap samples the new exponent. Source/flag preservation,
mutation after the consumer Read and four snapshots are checked. NORM and
interrupt composition remain outside this acceptance.
ADDS adds 52,224 read-capable RAM-source replay cases through the shared memory-arithmetic
fixture. The unsigned operand ignores SXM; carry, sticky destination overflow,
OVM saturation, retained address/literal/read operands and single-access
counts are checked over all eighteen covered forms. Its SUBS control also
passes 52,224 replays. Mapped/peripheral sources remain excluded.
Syntax-5 Xmem LD covers all 512 encodings in 196,608 replay cases, retaining
the selected AR2..AR5 RAM address and Read word before Execute's shift.
Distinct source pointers, source mutation, BK=0/nonzero wrap, both
destinations, all sixteen shifts and three snapshots are checked. T,
unselected accumulator and status are preserved. Mapped/peripheral sources,
repeats and interrupt composition are not covered.
Nondelayed CALA now retains a low-16-bit target on the caller's logical
page and one PC+1 frame word through Class 30B's held Read clock. Its
216-case IRQ/NMI/return/replay gate and separate 216-case DLD-producer
gate pass; the existing far-call controls remain green. CALAD, repeat and
other producer/composition cases remain unaccepted. An executed-address
trace confirms CALA's `02:3e55 -> 02:40e2` return path; the later MVDD
worker is entered by FCALL at `02:993d`, not directly by this near call.
Resident loading remains exact;
see [5510 bring-up](5510_bringup.md) for the bounded scope and frame-provenance
question. Suite version, profile counts, runner scopes and native 5110
restoration coverage have one authoritative home in the
[core regression checkpoint](c54x_execution_contracts.md#core-regression-checkpoint).
These checks establish the declared execution models, not full MU4 boot,
the physical DA150 map or native speech.

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

`tools/dsp_rom4_upload_trace_check.py` verifies upload ordering and rejects an
observed input length absent from the recovered catalogue.

### Staged frontend integration boundary

The separate `tms54rom4 -bios cold-pipeline` fixture uses the same acquired
mask ROM and cold data, without register seeding. It reaches the empty
`000f00` loader fetch with PC `0f01`, SP `0000` and PMST `ffa8` after 12
staged clocks. `tools/run_c54x_core.py --mame mame/mame --rompath mame/roms
--profile cold-pipeline` requires that exact boundary; this does not supply
or execute a loader.

`nse1pipe` is a separate research machine using the ordinary NSE-1 MCU,
DSPIF, native backend and product inputs, with the staged frontend and
explicit standard SWWSR option. The MCU organically uploads loader1;
staged execution executes its mapped-AR ADDM and the initialization routine's
ROL B, then reaches a replacement upload at `0f00`. Its RPTZ/STL loops
execute, and the backend observes completion strobes. Execution
reaches resident code, executes the absolute BITT in RETD's delay span
and RC TC, and passes short circular MAR, the delayed SUBS under BCD
BEQ at `008e4f`, CC ALEQ and repeated `WRITA *AR2+` at `0031d6`.
The uploaded immediate STM writes at `000e20` and `000e22` publish `c008`
then `c0c8` to the existing BSP-control endpoint `0022`. A passive data-write
tap observes both full-word writes in order, at 0.133888750 and 0.133888808 s.
Absolute BITF `61f8 0007 0800` at `004615` reads ST1's INTM bit and
execution passes the following `f7bb` (set INTM). Execution returns from
this resident routine and executes uploaded `000e30`, `8821`
(STLM A to BSP transmit register `0021`), publishing `0aaa` once at
0.133893750 s. It passes the conditional exit `f845 0e66` from block
`0e5c..0e62` and the explicit outside BRAF clear. It passes `000e14`,
`7ef8 001a` (absolute READA to BRC) and the following table-initialization
loop and executes resident `003427`, `8807` (STLM A to ST1). The native
status is subsequently `4900`. It executes resident `003461`,
`6ff8 0020 0c44`, a shifted load from BSP receive register `0020`.
A passive read tap observes two firmware reads of BSP's retained test
word `0aaa`, at 0.133909558 and 0.133937654 s. The second is the newly
admitted shifted read, not a new serial frame or an injected reply.
The staged frontend executes SAT A/B at `3473/3475` and the circular
post-increment store `80d3` at `34d3`, then the TC conditional call `f930`
at `354c`, followed by accumulator-lane CMPM at `0025fe`, `60f8`, whose
source `000b` is BL. Repeated MVKD `7090` at `3053` copies three words
from `b706` through post-incrementing AR0, and STM AL `7708` at `45b0`
executes, as does absolute ST T `8cf8` at `a548` (destination `1f28`).
The shifted load `6f83 0c5f` at `a5ef` now reads through the existing
board map at `0003`, as described above. Circular-address PORTR `74d6`
at `3249` reads port `0027`, the existing RF sample input. The block
established by RPTBD at `3245` (end `3268`), including MACR `b736`, parallel
store/MAC `d6e1` and SQDST `e210`, no longer encounters an unsupported form
in the observed prefix. The first incoming host packet is then read twice
from `0800` as `0a05` at 0.175897519/0.175897808 s. Circular-source MVDK
`71d2 0015` at `3eee` executes; the ring consumer advances through 6, 11
and 15. Repeated MVDK `7192` at `7f8e` executes, as does CALL `f074 7ffa`
at `7ff7` inside block `7ff7..7ff8`. MVDD `e59c` after the memory-count
repeat also executes. The live 0.5-second checkpoint has PC `408d`, BK
`00fa`, AR0=1, AR2=`06e4`, ST0=`081f`, ST1=`6103`, A=B=0, T=`000b`,
AR3=`06e3`, AR4=0 and PMST `ffac`, with ILLEGAL clear. These are checkpoint
registers, not the later stopped instruction's operands.
The final observation at 10.016666667 s has PC `09ac`, ILLEGAL clear,
nine reads through the ring-base tap, twenty-two consumer advances,
1,642 completion strobes, 74 mailbox writes and final device counters of
134,560 RF reads, 1,591 slot expiries and 4,234 frame expiries. There are
262 RF-control writes and 2,878 transfer expiries; only bounded prefixes
are logged as described above. `mode_aa` is one and `mode_ac` is twelve.
The terminal architectural snapshot is A=B=0, T=`5555`, ST0=`081f`,
ST1=`6103`, BK=`00fa`, AR0=1, AR1=`0332`, AR2=`0060`, AR3=`00ab`.
This passive snapshot is not, on its own, proof of a healthy interactive UI.
The ring-base tap's legacy `host_headers` name counts memory reads, not packet
headers: reads include `7fff/0000/7fb2` at PC context `3f4a` and zero at
`3da3`. The structured checkpoint fixture and checker reproduce this
observation from fresh NVRAM; the rejected reset-lifecycle fingerprint is no
longer accepted. The reusable staged menu and lifecycle gates pass without
firmware writes or baseband resets.
Older pending
slots need not retire after Decode rejection. SWWSR has actually retired
to `2000`; C5416-only BSCR/SWCR
remain zero. The live `0f00..0f27` words bind
uniquely to the 5110 v5.30 MCU image at file offset `0x94cf8` (the initial
loader1 image is at `0x99704`). The 24 uploaded words at `0e10..0e27`
also bind uniquely to offset `0x98a60`, including the READA-to-BRC
instruction and successive BSP control
writes `c008` and `c0c8`; these are uploaded instructions, not mask-ROM
contents. The 24 observed words at `0e58..0e6f` bind uniquely to MCU file
offset `0x98af0`: the loop reads BSP receive `0020`, compares it with AR6,
branches to `0e66` on equality, and otherwise writes another `0aaa` using
STM `0021`; `0e66` explicitly clears BRAF. This run uses the existing
default untimed COBBA test echo, not the separately clocked serial-pin
profile; passing the loop is not new evidence of physical serial timing.
An additional strobe alone does not prove upload completion. The observed BSP
test sequence publishes TX words `0aaa/ffd5`, reads two retained `0aaa`
words, and writes `c008/c0c8` followed by four `c008` values. It no longer
repeats after a reason-4 baseband reset. The snapshot counters at 0.5 s are
reads=4/writes=4/gap=6; the endpoint counters are reads=14/writes=12/gap=9.
These are firmware writes, not supplied receive frames. Menu/lifecycle
acceptance is independent of this passive observation and does not establish
complete RF acquisition or native speech.

Immediate STM `0022` retains its extension and publishes through the normal
data bus in Execute, paying the modeled peripheral access clock; it does
not use DAGEN's early-write path. `serial-stm` checks 12 cases plus replay
with `c008`, `c0c8`, zero and all-one literals, mutated after fetch, and
snapshots before/during the wait and after publication. It proves one
literal fetch and one destination write without a destination read. The
isolated endpoint is external RAM, not a second BSP implementation; the
native uploaded-code observation supplies the backend-composition evidence.
Neither fixture establishes silicon phase timing, serial traffic or speech.

`status-bitf` covers ST0/ST1 sampling in 768 cases plus save-state replay:
direct and absolute forms, four masks, both initial TC values, two program
pages and three snapshots around source Read/TC publication. Each source
is mutated after its single Read, including ST0's own TC bit, to verify
that publication uses the retained sample while preserving other current
status bits. No data writes occur. Admission adds only status registers
`0006`/`0007` to BITF's existing mapped-source set. CMPM separately admits
`0008..0017` as qualified above, not these status registers. Low controls
and other unvalidated sources remain guarded.
This is functional pipeline coverage, not a silicon phase measurement.

`reada-brc` and `reada-brc16` each execute 18,432 cases plus replay, with
fixed 23-/16-bit program addressing respectively. DP, SP, absolute, plain,
postincrement and postdecrement destinations cover eight indirect AR
encodings, two source pages, OVLY clear/set and four snapshots. Standalone
controls use `0/1/8000/ffff`; following RPTB cases use counts `0/1/2/7`
and one/two/three intervening NOPs. The gap follows SPRU131G table 7-24:
READA belongs to the one-instruction-latency writer class, not STM/MVDK's
zero-latency class. Uncomposed controls are repeated alongside each gap.
The tests change A after PAR capture, poison program data after its one
Read, and mutate the absolute destination extension after fetch. They
check one retained program read and BRC publication, no data reads, exact
subsequent loop counts and pointer updates, preserved status/accumulators,
and no replay duplicates. Source aliases under OVLY and the standard
address mask are checked separately; changed A has a different low-word
decoy even when overlay masks its page.
`reada-brc-edge` checks 16 repeated/active-block exclusions plus replay,
with no program/data reads, data writes or BRC changes. New BRC admission
requires an inactive staged block controller; it does not model in-loop
count-write latency. Zero-gap RPTB and interrupt/conditional composition
are not established by this fixture. Program-bus phase placement remains
the declared partial READA model, not measured silicon timing.

`stlm-bsp` checks 3,072 cases plus replay: A/B, eight accumulator patterns,
16 SST/SXM/OVM/FRCT combinations, CPL clear/set, two program pages and three
snapshots around the peripheral hold/publication. The source is changed
before Execute to establish live accumulator sampling; DP is nonzero and
CPL varies to distinguish encoded MMR addressing from ordinary Smem. Each
case writes once to `0021`, performs no data reads, preserves accumulators
and status, and applies the existing saturated-store semantics. The model
adds one peripheral access clock (publication at core clock 7), not a
measured silicon phase. `stlm-bsp-edge` checks 32 cases plus replay with no
bus/accumulator effects for excluded delay, single-repeat, block-repeat
and XC compositions. Only standalone STLM `0021` is newly admitted.
The native passive transmit tap proves the backend received the one `0aaa`
write. This is register-level publication, not proof of PCM transport,
speech or handset boot.

Repeated short-source WRITA uses a retained logical PAR stream, with each
program destination translated independently for overlay and address-width
wrapping. The first source and PAR are captured before retirement; later
iterations read fresh source data and apply one addressing update each.
The `rpt-writa` and `rpt-writa16` fixtures cover respectively 38,400 and
19,200 cases plus save-state replay: five source forms, five count sources,
eight AR encodings, four counts, overlay crossings and program-address wrap.
They mutate the captured source, count and accumulator to distinguish
retention from re-reading. Repeated extended-source, delay/XC and block
compositions remain excluded. WRITA-specific interrupt-arrival composition
is not independently established by these fixtures; stream timing is a
declared core model, not a measured silicon phase claim.

In the standard C54x map, `0028` is SWWSR, not TCR (`0026`). Staged bus
register admission can use `set_standard_swwsr(true)` independently of the
C5416-compatible bus profile. SPRU131G section 10.3.1/table 10-2 defines the
standard option: reset `7fff`, reserved bit 15 reads zero, and five 3-bit
wait fields without a multiplier. I/O uses waits 0..7 and samples READY
only with at least two software waits. BSCR/SWCR ownership, the C5416
multiplier and its output controls are not imported. Program/data fields
are retained but their external-memory waits remain unimplemented; physical
Lead bus mapping is not established by this option. Ordinary `noki5110`
retains its previous configuration. The distinct 5510/MU4 request
`7728 7fff` must not replace this handset's observed `7728 2000` operand.

The `std-swwsr` fixture exhausts all 65,536 encodings, partial writes,
save/restore and reset, including non-ownership of BSCR/SWCR. `std-stm`
checks 12 retained STM writes/restores. `std-io` and `std-io-atomic` check
288 and 1,440 cases/restores respectively, including READY, IRQ/NMI,
ongoing timer clocks and exactly-once transfers. Multiplier-enabled raw
cases are omitted rather than relabeled as duplicate original-family
cases. These tests do not assert a fitted bus or external RAM-bank map.

`clock-stm` separately checks 12 retained CLKMD writes/restores through its
existing register implementation, including the loader's `3146` request.
No PLL reference is configured in that fixture. Its success validates
instruction publication, not the handset's physical clock source, lock
latency or frequency. Unsupported younger Decode can halt before older
slots retire: accepting an opcode alone is not evidence that its write
occurred; the integration observer checks the actual SWWSR state.

`clock-mvdk` covers 72 retained RAM-to-CLKMD transfers: direct DP, indirect
postincrement and absolute sources, four control words, program pages 0/2
and three save/load boundaries. Source/address literals are overwritten after
capture to reject rereads. Each leg samples once, pays one peripheral-access
clock and publishes the retained word once, then reaches the same continuation
with unchanged status. The existing CLKMD register owns writable/status-bit
behavior; no PLL reference is configured in this isolated fixture.
TI SPRU172C sections 3-40/3-42 and 4-108 identify the class-19 memory transfer
and extra peripheral cycle. Placement of that extra clock in the staged
prototype is not measured silicon phase timing.

`clock-mvdk-edge` checks 24 repeated/delayed compositions across three source
forms, two pages and two save/load points. They fail before data reads/writes,
pointer changes or CLKMD changes. Other control destinations, mapped peripheral
sources, repeat/control composition and simultaneous peripheral arbitration
remain excluded. The original AA22 INT2 handler's `0288cb:71f8 1420 0058`
is a concrete native consumer: it restores the model's PLL selection after
deep-idle wake. This does not establish physical lock time or completed native
wake continuation by itself. The following interrupt-register stack operations
are covered separately below.

`pop-irq` covers 192 staged `POPM IMR` / `POPM IFR` cases and restores:
four saved words, four pending-IFR patterns, two program pages and three
snapshot boundaries. The stack is read/incremented once in Read; Execute
publishes the retained word through the existing register implementation.
IMR is replaced, whereas IFR is **write-one-to-clear**, not restored as a
pending-bit snapshot. Changing the stack word after Read does not change the
publication, and neither destination is read as an instruction operand.
Interrupt delivery is masked in this isolated fixture: it does not qualify
general unmasked POPM/IRQ arbitration. TI SPRU172C section 4-128 identifies
POPM as a one-word class-17A stack-to-MMR operation with no status-bit effects.

`pop-irq-edge` checks 24 fail-closed cases/restores: both destinations,
single repeat and both branch-delay positions, two pages and two snapshot
boundaries. These unsupported compositions stop before stack reads, SP
changes, mask writes or pending-bit acknowledgements. The decoder captures
delay ownership before consuming the final delay word; testing only the
remaining count afterward would incorrectly admit the last delay slot.
This prototype exclusion is not a claim that physical silicon traps there.

Standalone extended MVPD now retains its absolute or long-offset data
destination separately from its program source. Absolute form consumes
destination then `pmad`; long-offset forms consume `pmad` then offset.
Read resolves the destination once, the held program-bus clock captures
one source word, and Execute publishes that retained word. The three-word
instruction has the documented four-clock total; exact silicon bus phase
remains partial. This admits the loader's `7cf8 0802 ff87` transfer without
an atomic fallback or any synthesized payload.

`mvpd-extended` checks 6,144 cases/restores: absolute, offset,
preincrement-offset and circular destinations, eight AR initializations,
four values, two program pages/regions, both OVLY/CMPT states and three
snapshots. Post-read source/mapping changes and late AR mutation must not
retarget or repeat the transfer. `mvpd-ext-edge` adds 512 cases/restores
requiring zero source reads, destination writes and AR/status changes for
unsupported CPU/peripheral destinations, including rollback of a long
address preview. These are emulator exclusions, not hardware traps.
`mvpd-controls` reruns the existing short-transfer, repeat and deferred
interrupt banks. Absolute/long-offset single-instruction repeats remain
forbidden by the independent TI addressing guard; block/XC/control-delay
composition and unenumerated destination forms remain outside acceptance.

`literal-guard` independently checks 54 cases/restores of legal absolute
ST inside RPTB, with destination literals `7cf8`, `7df8` and `7ff8`, three
block counts, two pages and three snapshots. MVPD/MVDP/WRITA admission
guards apply only to a completed valid opcode slot, not a pending
instruction's first extension. The adversarial fixture rejects a core
that classifies that extension as a transfer opcode. This does not admit
the transfers themselves inside an otherwise unsupported composition.

For a bounded reproduction, seed `nvram/nse1pipe/eeprom` with
`tools/make_5110_eeprom_profile.py` using the NSE-1's own `nse-1.bin` and
`5110f530.fls`. Run `nse1pipe` for three emulated seconds in an isolated
directory, with `-noreadconfig -video none -sound none -log -nothrottle`
and `-autoboot_delay 0 -autoboot_script` pointing to the absolute path of
`tools/c54x_rom4_staged_observe.lua`. The observer reads ordinary uploaded
program RAM, never status or I/O. Then run
`python3 tools/c54x_rom4_staged_check.py CASE/error.log roms/noki5110/5110f530.fls`.
Its result classifies an unsupported boundary, binds all 40 observed words
to the source image, and explicitly declines boot, complete-upload and
fitted-bus-dialect claims. The normal atomic and clocked native fixtures
retain their existing acceptance contracts.

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
An MDISND producer commit updates this INT2 comparator only; it must not
also assert HPINT. HPINT belongs to the explicit DSPIF parameter doorbell.
The NSB-1 cross-ROM control exposed the duplicate pulse: the unsolicited
vector-25 parameter handler leaked stack into uploaded code and eventually
stopped the core. Removing only that pulse preserves the NSE-1 coherent,
menu and RF gates and permits 90-second NSB-1 physical menu operation with
nonzero receive-activity checks. See `5190_bringup.md` for the paired evidence
and remaining fitted-mask/provisioning limits.
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

The backend maps C54x data `0x20/0x21/0x22` to BSP receive, transmit and
control state. The observed `c008 -> c0c8` words reset/release the serial
engines; their upper nibble is not evidence of a COBBA register-C write.
Data `0x32` is an opaque saved latch with no COBBA forwarding. These data
addresses are distinct from DSP I/O ports `0x21/0x27/0x31/0x32` and control
ports `0x2c/0x2d`. The authoritative ownership and pin-level acceptance are
in [COBBA serial-control ownership](cobba_control_boundary.md#serial-control-ownership-correction).

Cold-entry interrupt masking explains the earlier absence of RF sample
reads and SCU-port writes, not a missing codec completion.
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

The earlier isolated single-entry profile (not current cold-scan acceptance)
reaches `4414/4428/4460` three
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

That isolated mode-1 processing census records 48,264 fetches of `330a`,
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
unchanged T, postincremented AR3, and one-cycle cost. Sixty-four additional
`3682` cases check old A-high=1 and T=1 with four old-B rounding boundaries,
FRCT/OVM/SXM clear/set and pre-existing OVA clear/set. They assert positive
rounding overflow and saturation, sticky OVA, preserved C/OVB/T, and the
independent signed/unsigned memory load into B. All pass without changing
the implementation, following TI SPRU172C POLY (4-126). These cases fix
SMUL=0 and C16=0; other addressing modes, negative saturation and physical
pipeline timing remain unestablished. Native FR encoding and decoding retain
their exact independent comparisons on this build.
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
after the access, per SPRU131G section 5.6. Fixture-only `4882` and `8892`
check read and postincrement write at one cycle. `489a` instead rejects:
short MOD=3 is write-only, including in the MMR grammar. LDM, PSHM and
PSHD reject this source encoding before data, AR masking/modification or
stack effects; `stack-mod3-edge` checks 512 cases/restores and the legacy
atomic prefix. The ordinary Smem long-offset preincrement form remains
read-capable, but long forms are separately excluded from MMR addressing
by section 5.6. Implicit-copy and read/modify/write forms are not decided
by these tests.
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
Single-repeat endpoints retain every 16-bit PC value: `ffff` is not an
inactive-state sentinel. Twelve firmware-free `MVMD` cases cover ordinary,
`ffff` and wrapped `0000` endpoints, compact/long repeat counts, and IRQ/NMI
requested after the penultimate transfer. All three transfers and their
destination progression must finish before entry, with the post-body PC
stacked in the unchanged executing page (SPRU131G sections 6.7 and 6.10.7).
The previous 16-bit sentinel fails at body `fffd`, leaving the final word
unwritten and stacking `fffd` in `tmp/c54x-repeat-endpoint-red.log`.
The internal endpoint uses an out-of-range sentinel and remains save-state
registered; conformance requires `TMS320C54x repeat endpoints: PASS`.
All twelve cases pass in `tmp/c54x-repeat-endpoint-green.log`. Native RF
counts and save-state restoration retain their acceptance contracts in
`tmp/c54x-repeat-endpoint-regression.log`; the timer observer still shows
short-period starvation. This is repeat-state/protection evidence, not
physical pipeline latency or recorder completion.
Block-repeat activation follows architectural ST1.BRAF, not a separate
private latch (SPRU131G section 6.8). Clearing BRAF through `RSBX`, `STM`
or `POPM` cancels looping without consuming another BRC iteration; restoring
RSA/REA/BRC and setting BRAF resumes looping without a new `RPTB`.
Four executable cases require `TMS320C54x BRAF control: PASS`, checking body
counts, remaining BRC and stack balance. The old duplicated latch fails the
cancellation assertion in `tmp/c54x-braf-control-red.log`. BRAF and its loop
registers are already saved architectural state, so no parallel active flag
is retained. These are execution-state checks, not Decode-stage BRC decrement
timing or general interrupt-pipeline evidence.
Single-instruction repeat completion must also honor an active block boundary.
In particular, `RPT #0` executes its body once but does not cancel or bypass
the enclosing BRAF loop. Four executable fixtures place that body at REA,
with short/long counts zero and two and BRC three; they require respectively
four/twelve body address updates, exhausted BRC and cleared BRAF. The old
early return leaves AR1 one, BRC three and BRAF set in
`tmp/c54x-nested-repeat-red.log`; the corrected core passes all four cases in
`tmp/c54x-nested-repeat-green.log`. The runner requires
`TMS320C54x nested repeats: PASS`. This checks architectural execution counts,
not Decode-stage loop-counter timing or a physical pipeline implementation.
`tmp/c54x-nested-repeat-regression.log` preserves the exact native RF snapshot,
5110 save-state restoration and original MU4 storage/native-entry acceptance.
The timer observer still exposes short-period starvation; recorder finalization
and native speech are not established by these gates.
XC's two-word interrupt protection (SPRU172C page 4-199) must count the
retired body's words before repeat handling redirects PC. Otherwise an
ordinary one-word body ending a block appears to consume a large PC delta,
admitting IRQ/NMI before the second protected word. Four executable cases
cover one/two-word bodies and both sources: the short body requires ISR entry
after the next loop's first word; the long body exhausts protection immediately.
The old ordering stacks `0802` with observed A one for the short body in
`tmp/c54x-loop-xc-red.log`; corrected short cases stack `0803` with A nine in
`tmp/c54x-loop-xc-green.log`. The runner requires
`TMS320C54x loop XC protection: PASS`. This is word-guard accounting across
repeat redirection, not a general decoded-instruction frontend or the six-stage
interrupt pipeline.
`tmp/c54x-loop-xc-regression.log` preserves the exact native RF counts,
5110 save-state round trip and original MU4 storage/native entry. The separate
timer observation still exposes short-period starvation.
Operand-fetch conformance separates Smem address preparation from its read/write
so MVDK, MVDP, PORTW, PORTR, READA, MVKD, CMPM, BITF, POPD, WRITA and MVPD fetch their instruction extensions
before reading data, I/O or program operands. FIRS likewise fetches its coefficient
address before reading X/Y. Fifty-three passive bus-tap cases cover direct,
postincrement, long-offset and absolute forms plus FIRS, MVPD preincrement
and all eight MVPD absolute-address encodings; the runner requires
`TMS320C54x operand fetch order: PASS`. The original MVDK direct case reads
data before its destination extension and fails this check. The original CMPM
direct case independently fails before its immediate fetch. Comparison/test
cases also check TC and retain displacement-before-immediate encoding.
POPD's long-offset negative control observes the stack read before its address
extension; resolving the destination before `pop()` closes that ordering defect.
Its four forms also check a distinct destination sentinel, unchanged stack word,
single SP increment and selected pointer updates.
WRITA's four forms additionally check a distinct program destination and
unchanged accumulator A; they verify existing execution, without a timing change.
MVPD's preincrement negative control writes the old destination, failing the
distinct destination sentinel in `tmp/c54x-mvpd-preincrement-red.log`.
Shared address preparation now increments before the write and completion
does not increment a second time (SPRU131G table 5-4, MOD 3). The preceding
destination and program source remain unchanged. MOD 15's unused AR bits
use the same absolute address-before-pmad order for all eight encodings.
Instruction-family extension layouts and aggregate cycle charges remain unchanged;
MVPD's preincrement ordering is the separately evidenced correction above.
This enforces Fetch-before-Read ordering (SPRU131G section 7.1), not physical
Access-stage address-update timing, overlapping execution or interrupt drain.

The accepted isolated 30-second RF snapshot is 20,280 frame expiries,
648,032 RF reads and 1,537 port-32 writes. Independent queued-transfer
completion accounts for twenty native `3712` writes; four immediate
`a240` writes and 1,513 scan `4028` writes complete the inventory.
The multi-entry processing profile requires 1,024 mode-1 frames, twenty
early reductions, 1,004 steady reductions and two control continuations,
with zero RF/reduction inputs. See
[the boundary contract](cobba_control_boundary.md) for descriptor, selector,
operand and ordering evidence. Neither continuation establishes acquisition.
The checker also requires the
ordered programming sequence, frame-30 activation, 32-word read cadence,
firmware queue producers, IMR `035f` and zero port-38/39 reads. Two isolated
long transfer runs reproduce its complete inventory; independent fresh RF
and processing acceptance gates pass. Each generates this handset's own EEPROM
and fresh flash state. This is an emulator regression snapshot, not measured
RF timing or acquisition.

The earlier 20,285/648,192/1,520 snapshot depended on the card contract:
EF_SMSS (`6f43`) belongs to
DF_TELECOM (`7f10`), not DF_GSM (`7f20`). A reversible source control changing
only that parent to the incorrect `7f20` reproduces the previous exact
20,282/648,096/1,519 totals. Restoring the correct parent reproduces
+3 frames, +96 reads and +1 write against the old snapshot. Evidence:
`tmp/rf-smss-placement-control.log` and
`tmp/rf-smss-correct-placement-restored.log`. The correct placement is retained;
the checker is deliberately re-banked, not made advisory or widened to accept
both regimes. Its tests reject the old counts and changed programming sequences.

The first observed card divergence is SELECT `6f43` at `4.41506615` seconds:
the incorrect placement returns not-found, while the correct placement selects
the file and permits a two-byte READ BINARY before selecting `6f42`.
The first 23 MCU TX commits still match within 2 microseconds. Commit 24 carries
the same type-`1a`, 68-byte body (`001198` followed by zeroes), but moves from
`6.425337` to `6.418223` seconds. DSP work-flag publication follows by
2 microseconds in both traces; RF sequence 6 moves one 4.615-millisecond frame
earlier. This localizes the schedule change upstream of DSP dispatch.
No CPU timing, peer latency or firmware state is changed to match the oracle.
Separate RPT-zero and XC-word-accounting controls did not remove the drift;
their corrected instruction contracts retain their executable regressions.
Retained post-run NVRAM does not reconstruct the cold fixture.

The late MCU publication chain is observed through passive taps:

| Boundary | Observed caller / publication |
| --- | --- |
| Task-15 `09ec` | Constructor `21fd22`, caller `223010`; post `21fe24` |
| Task-16 `07d5` | Branch `208d40` receives `09ec`; caller `208d44`, post `206d3e` |
| Task-13 `05df` | Callers `24f388/24f392`, selectors 0/3; post `26c896` |
| Task-16 `05e8` | Constructor `237df8`, caller `238f86`; post `237f56` |
| Task-10 `03e9` | Receive returns `05e8` at `24f47c`; caller `24f49e`, post `26c6ce` |
| MCU search packet | Caller `211548` enters `20fc94`; branch `20fe38`, TX commit `27183e` |

`tools/c54x_rom4_scan_schedule.lua` is a capped, observation-only NSE-1 v5.30
instrument, not another handset acceptance gate. It reads CPU registers and
bounded ordinary-RAM task headers; it writes no firmware state. A shared
publication helper or register snapshot alone does not establish timer
ownership. These checks establish native ROM execution and the recorded
RF-port lifecycle, not physical RF acquisition, handset speech or end-to-end calls.

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
frame-driven port-27 reads with no port-38/39 reads. The exact current scan
snapshot is specified in the BRAF contract above. This preserves the existing
no-cell boundary, not radio acquisition.

`make probe-c54x-timer-boundary` runs a separate, firmware-free observation
profile (`tms54test -bios timer`). It reloads PRD through indirect stores,
then sets AR1 to IMR and masks TINT. The ISR consists only of `PSHM XPC`
and `FRETE`. With PRD 1 the atomic core repeatedly returns to `01:0608`,
before the mask, with IMR/IFR `0008`; the observation deadline may fall
inside the ISR with SP `0ffe` or `0fff`. With PRD 256
it reaches the marker after masking, then the branch loop at `01:060f`;
SP is again `1000`, IMR is zero and IFR retains pending TINT. No ROM,
NAND, serial peripheral or handset state participates in this reproduction.
The long-period control is checked, but the short-period result is printed
rather than asserted as correct: this probe is not a fidelity gate. It is
the small executable comparison for generic interrupt/write/drain modeling,
not proof that a timer period shorter than ISR service time must permit
foreground progress. Full pipeline behavior still requires an independently
derived timing oracle; changing PRD to make the handset proceed is not a fix.
The same profile now checks direct-address masking as well: replacing the
post-reload AR1 setup and indirect mask with `ANDM #fff7,*(0000)` leaves
the short-period marker clear, while the long-period control completes.
It reports four cases, with both long-period controls asserted and neither
short-period outcome treated as physical acceptance. This rules out the
extra pointer-setup instruction as the sole cause; it does not resolve
Decode-stage acceptance versus write visibility.
The passive probe distinguishes reload-opcode fetch, TCR bus write, IMR
mask bus write, first vector fetch and the first stacked return PC. CPU-owned
MMRs now take precedence over broad board RAM windows through a sparse
MAME data-space device map. CPU instructions, passive taps and debugger data
reads therefore use the same register state. Reserved gaps remain board-owned;
partial writes preserve unselected bits, with IFR's write-one-clear rule
handled separately. Snapshot fixture loading initializes writable RAM only,
not live CPU or peripheral registers. The core gate requires an executable
`TMS320C54x MMR bus: PASS` test of this boundary. No per-instruction diagnostic
callback is added to the core.

The timer observer requires one actual TCR reload write per case and one
IMR mask write in each long-period control. It reports short-period outcomes
without asserting them as physical acceptance. These are emulator-accounting
timestamps at observable bus accesses, not Fetch/Decode/Execute pipeline
phases or silicon latency. Retained vector prefetch precedes the stack write;
the continuation is captured from the latter's passive tap. Do not choose a
foreground delay from this trace or interpret bus observability as a pipeline
correction.
In `tmp/c54x-decode-timer-boundary.log`, opcode fetch and TCR bus write have
the same accounting timestamp; the first short-period vector fetch follows
five accounting cycles later with explicit INTR Decode before stack Access. Both
short cases still enter 108 times and stack `0608`,
with no mask write. Long-period indirect/direct cases mask four/two cycles
after the reload respectively and never enter the vector. This makes the
collapsed fetch/write boundary directly observable without changing PRD,
interrupt admission or firmware state; it does not settle physical acceptance.
The changed vector-fetch timestamp does not resolve the short-period stall.
`tmp/c54x-mmr-map-rf.log` independently retains the ROM4 no-cell boundary:
648,288 RF reads, 1,580 port-32 writes and no port-38/39 reads. The tool suite
passes 2,452 tests with one skip. None of these gates proves recorder save
completion, RF acquisition or handset speech.
SPRU131G section 7.2, example 7-18, constrains that correction more tightly:
recognition at the end of cycle 3 inserts INTR at Decode in cycle 4,
replacing `i2`; already-decoded `i1` completes at Execute in cycle 6.
Interrupt overhead covers cycles 7--9 before the first vector instruction
executes in cycle 10; the inserted INTR's actual stage placement is detailed
below. The saved continuation is `i2`, not a fixed number of later
foreground instructions. A blanket three-instruction deferral would therefore
implement the wrong contract. The former atomic `service_interrupt()` joined
recognition, IFR clearing, return-PC capture, stack update and vector transfer
at one retirement boundary and charged five cycles. The current entry splits
the bus operations and defers IFR clearance to vector fetch; foreground
recognition still occurs at an instruction boundary. The five-cycle aggregate
is not a full pipeline model; the manual's three-cycle INTR overhead alone does not justify
replacing it without accounting for recognition and drain separately.
The next executable timing fixture must distinguish request latching,
Decode-stage acceptance, memory-write visibility and vector entry. Timer
requests accepted before a later IMR write must not simply be cancelled by
that write. The existing short/long-period probe does not resolve this order.
SPRU131G section 6.10.4 ties IACK and IFR clearance to the first vector-word
fetch. The rendered example 7-18 must be read alongside that prose: its
vector fetch occurs in cycle 6, overlapping `i1` Execute, before the inserted
INTR's stack write in cycle 7. Therefore moving IFR clearance to the core's
former post-stack `fetch()` would implement the wrong bus order. The core
now prefetches and retains the first hardware-vector opcode before the
stack write, calling MAME's IRQ acknowledgement callback once for the accepted
IRQ/NMI source. INTR Access decrements SP before that fetch; only the return
word's memory write follows it. A combined post-fetch `push()` would preserve
memory-write order but incorrectly leave the old SP visible at Fetch/IACK.
The pointer update and memory write are therefore separate operations.
Extension words are fetched normally. This is a bus-order
correction, not a Decode-stage recognition/drain model. The figure places
INTR Access/Read/Execute in cycles 5/6/7; the prose's cycles 7--9 describe the
interrupt overhead before first ISR execution in cycle 10, and must not be
treated as three later foreground executions. A stage-aware fixture must
observe acceptance, foreground mask write, vector prefetch, acknowledgement,
stack write and ISR execution separately. Preserve the accepted source and
continuation across later IMR changes. Neither the atomic core's fetch tap
nor its unchanged aggregate five-cycle charge measures these physical
boundaries.
An executable IRQ/NMI fixture observes decremented SP with unchanged stack
memory at vector fetch and acknowledgement, then one continuation write
without a second pointer decrement. It checks selective IFR clearance and
one source acknowledgement. It changes the
vector opcode after that read and checks execution of the retained word,
normal extension fetch, two save/replays, survival of later IMR masking,
reset discard and explicit debugger-PC redirect discard. The combined-push
core fails the Access-before-Fetch assertion in `tmp/c54x-intr-access-red.log`.
The core gate also replays five modeled entry boundaries for both IRQ and NMI
(ten independent save/load cases): acceptance, Decode, stack Access, vector
Fetch/IACK and stack write. Each continuation requires exactly one vector
read, acknowledgement and original-continuation write, including when IMR
changes after acceptance. Post-Fetch cases replace vector memory and still
execute the retained opcode with its normally fetched extension. These are
entry-state ownership tests, not foreground-pipeline drain or silicon-cycle
validation; the instruction-atomic timing frontier remains unchanged.
The complete core gate requires `TMS320C54x vector prefetch: PASS` with
`sp_access=1`. Core, timer observation, original MU4 storage/loader/serial and
native NSE-1 RF gates pass in `tmp/c54x-intr-access-final.log`; the RF boundary
retains 648,288 reads and 1,580 port-32 writes, with no port-38/39 reads.
The tool suite passes 2,452 tests with one skip. These gates do not include
recorder save completion or handset speech. The separate recorder-control
and undelivered-metadata observations remain documented in `5510_bringup.md`.
Vector bus order is corrected; the recognition/drain timing question is open.
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

### Standalone vendor assembler operand control

The acquired `CodeGeneration_C5400.CAB` supplies an independently runnable
C54x assembler/compiler driver, version 4.2.0. This does not resolve the CCS
simulator automation boundary. Extract `ASM500.EXE*` and `CL500.EXE*` from
that cabinet and retain copies named `bin/asm500.exe` and `bin/cl500.exe` in
a disposable work directory. `c54x_vendor_operand_check.py` pins both exact
executable hashes and the separately authored assembly source.

Copy `fixtures/c54x_vendor_operands/operand_probe.asm` and `probe.cmd` into
the work directory. Using the isolated runner below, initialize a fresh
WoW64 prefix with `wineboot -i`, setting `WINEPREFIX=/mnt/work/wow64` and
`WINEDLLOVERRIDES=mscoree,mshtml,winemenubuilder.exe=` in the command's
environment. Then invoke `wine cmd /c probe.cmd` with those same settings.
The command file sets Windows TMP/TEMP explicitly to writable scratch;
on the current Wine host, Unix TMP/TEMP appear only as `WINE_HOST_*` and
are not usable vendor scratch settings. All persistent files remain inside
the disk-backed work directory; no vendor installer is needed.

The reviewed listing emits 13 words for eight instructions. It gives
write-only warnings for `LD *+AR2,A` and all four short-preincrement
ANDM/ORM/XORM/ADDM controls. Ordinary postincrement LD, long-offset
preincrement LD and short-preincrement STL are unflagged. The assembler
still emits the warned encodings and exits zero; warnings must therefore
be checked, not mistaken for successful admission. This agrees with
SPRU131G table 5-4's restriction and disambiguates the RMW source role for
our fail-closed core. It is not a measured silicon trap or timing result.

Check `operand_probe.lst` using `tools/c54x_vendor_operand_check.py`, supplying
`--source`, `--assembler` and `--driver` paths. It verifies version, exact
word/address sequence, warning identities/count and assembly completion,
and explicitly leaves DSP execution and silicon-trap validation false.
Accepted artifacts are under `tmp/c54x-vendor-operand-probe/`. The separate
[execution rejection matrix](c54x_execution_contracts.md#operand-domain-acceptance)
now verifies the corresponding early core guard and both frontend/save-state
paths. The assembler probe itself remains a toolchain observation, not
execution, timing or silicon-trap evidence.

The post-operand-guard default regression is independently complete in
`tmp/run_c54x_core_hwymx7ti`: all 293 required markers pass, the fixture
returns its intentional completion status 3, and the runner reports native
conformance success. This covers the default instruction-model regression
after the RMW guard, in addition to the focused rejection matrices above.
It does not replace the separate optional profiles, provide an exact-opcode
execution inventory, or establish fitted silicon, RF acquisition or speech.
Large replay matrices make this a long-running gate:

```sh
.venv/bin/python tools/run_c54x_core.py --mame mame/mame \
  --rompath mame/roms --output-root tmp
```

### Isolated simulator setup boundary

`tools/run_ccs33_isolated.sh WORK COMMAND ...` supplies a disposable network-
and process-isolated namespace around Wine/Xvfb. Persistent writes are limited
to `WORK`; host `/data`, `/home` and `/root` are hidden. This optional vendor-
tool runner is not a MAME acceptance gate. It requires `bwrap`, `timeout`,
`xvfb-run` and Wine; prepare `WORK/home` and `WORK/prefix` first.
The runner creates `WORK/tmp` and binds it as the isolated `/tmp`; choose
a disk-backed `WORK` so vendor unpacking and Xvfb scratch do not consume tmpfs.
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
`CodeComposerSetup.SystemSetup` (class `{3620D2F0-F091-11D1-8BE4-0020182ABDCA}`);
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

The failed interface request is `IOleObject`, IID
`{00000112-0000-0000-c000-000000000046}`, not the SystemSetup class ID or
its custom setup interface (see the
[Microsoft SDK definition](https://raw.githubusercontent.com/microsoft/win32metadata/main/generation/WinSDK/RecompiledIdlHeaders/um/oleidl.h)).
The COM trace records `0x80004002` at that request before the scripting
wrapper reports `0x80040154`.

Reversible controls in a disposable copy of the retained prefix explicitly
removed `InprocHandler32` first in `/reg:32` alone, then in both registry
views. Queries verified each removal; both controls reproduced the baseline
failure and restored the registrations. Installed and administrative copies
of `cc_setup.exe` and `ccreg.dll` are byte-identical. A fresh direct setup
control still returns zero for `Clear`, `Import` and `SaveConfig` after
restoration. The handler-view hypothesis is exhausted; do not repeat it.
No vendor binary or generated board database was patched. A different
compatibility environment or a working direct application API is required
before this route can provide timing evidence.

#### Direct driver loading boundary

`tools/ccs33_driver_load_probe.c` loads `tisim54x.dvr` without COM and checks
eight GTI exports, including initialization, simulator mode, pipeline
inspection and stepping. The installed driver loads successfully under the
isolated Wine runner; a nonexistent-driver control exits 1. Evidence is in
`tmp/ccs33-driver-load.log` and `tmp/ccs33-driver-load-negative.log`.
This establishes DLL loading only: no GTI function is called, no target is
initialized and no instruction executes. The SDK header search did not find
the direct GTI initialization ABI. Recover a documented initialization and
configuration contract before attempting execution; exported names alone
do not establish function signatures or pipeline behavior.

Static export-body audit further limits this route. For `tisim54x.dvr`
SHA-256 `2d98a6d9729c4ebd5534d8e1d46ec92444102ce7067a03da530470d476e9c9f0`,
`GTI_GET_PIPELINE_INFO` (RVA `0x3dc0`) selects a context, clears 30 DWORDs
at output offsets `0x100..0x177`, and returns zero; it does not obtain stage
state. `GTI_GET_PHASE_NAME` (`0x3d90`) bounds its index against a helper
that unconditionally returns zero, so it always returns null. Neither
export supplies pipeline inspection in this binary. The mode getter/setter
(`0x3e40`/`0x3df0`) instead dispatch simulator operations `0x44`/`0x45`;
their existence keeps execution-mode testing a candidate, not a verified
timing oracle. Initialization uses context/configuration data and pointer
arguments whose complete contract is still unestablished. Retain the
disassemblies in `tmp/ccs33-pipeline-export-audit.log` and
`tmp/ccs33-init-export-audit.log`; do not infer callable prototypes from
export names or replace missing configuration with fabricated structures.

Stage the source in `WORK` and compile with the installed Win32 compiler:

```sh
sh tools/run_ccs33_isolated.sh WORK wine \
  'C:\CCS33\bin\utilities\sim\gcc\bin\gcc.exe' \
  -Wall -Wextra -o driver_load_probe.exe driver_load_probe.c
sh tools/run_ccs33_isolated.sh WORK wine driver_load_probe.exe \
  'C:\CCS33\drivers\tisim54x.dvr'
```

`tools/ccs33_activation_probe.vbs` is the fail-closed activation control:
scripting version alone is insufficient; successful opening and nonempty
board/CPU identities are required. Even its success would establish target
identity only, not instruction execution or pipeline timing. Stage it in a
disposable work directory and invoke it through
`sh tools/run_ccs33_isolated.sh`, never directly against the host prefix.
The explicit-view controls are retained in
`tmp/ccs33-interface-{baseline,without32,withoutboth}.log`; the fresh setup
control is `tmp/ccs33-interface-setup-control.log`.

Retain `full-install-expanded-path.log`, `installed-ide.log`,
`installed-ide-screen.png`, `run_ccs33_oracle_plugin_msxml_legacy.log`,
and the earlier `full-install-shell-registered.log`, `xds-install-error.png`
and `plugin-install-error.png`. Additional evidence is `mono-install.log`,
`run_ccs33_oracle_plugin_with_mono.log`,
`run_ccs33_oracle_wineboot_repair.log` and `plugin-prefix-repair-error.png`.
Current verification evidence is `full-install-wow32.log` and
`run_ccs33_oracle_automation_registered.log`,
`run_archive/cleanup-20261009/root/run_ccs33_oracle_automation_com_trace.log.zst`,
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
Nondelayed `CALA A/B` also publishes the next-instruction address to RTN.
Two executable cases pause in the callee, replace its stacked continuation
with `dead`, then require RETF to return correctly and balance SP without
reading that sentinel. Both accumulator forms retain their six-cycle charge;
the core runner requires `TMS320C54x accumulator call fast return: PASS`.
The former stack-only implementation fails this test. Delayed and far-call
RTN publication remain separate, unvalidated contracts.
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
SPRU172C page 4-199 separately specifies a one-word conditional-execution
span for `XC 1` and states that XC plus the two following instruction words
are uninterruptible. Protection therefore remains two words for accepted
`XC 1`; rejecting its first slot leaves one protected word. The core keeps
this word-counted protection distinct from the complete-instruction INTM
guard above. Four firmware-free fixtures assert IRQ during XC's opcode fetch
and check accepted/rejected `XC 1` and `XC 2`: the ISR samples A only after
the two following words, with stacked continuation `0803`. Conditional
execution remains unchanged. The new assertion fails on the previous core
(`tmp/c54x-xc-irq-red.log`); the corrected core passes in
`tmp/c54x-xc-irq-green.log`, and conformance requires the
`TMS320C54x XC interrupt protection: PASS` marker. This is an architectural
interrupt boundary, not physical pipeline latency or evidence of the
recorder's failure cause.
The timer-boundary observer and original ROM4 RF gate still pass
(`tmp/c54x-xc-irq-regression.log`), preserving short-period mask starvation
and the no-acquisition boundary. Original MU4 storage/native-entry regressions
also pass (`tmp/mu4-xc-irq-regression.log`).
The long-immediate ALU decoder charges two cycles, including the separately
decoded XOR form. A cycle-stamped LD fixture checks the additional cycle;
the LD contract is SPRU172C page 4-68. This does not cover all extended-address
or repeated multicycle instruction timing.
The current HINT/absolute-compare model starts fresh-profile port-27 reads
on frame 30. The 30-second observation has 6,499 frame expiries and 207,040
reads (`32 * (6499 - 29)`), terminal IMR `035f` and no burst-port
activity. Fresh execution on the built `552db94` checkpoint reproduces
these counts (`run_5110_native_rf_current_20261009`), with terminal IFR
`0020`: INT0 is drained, but other pending sources are not required to be
zero by the gate. The gate pins that first-read frame, the three ordered RF control
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
The opt-in `mad2_static_census.py --constant-indexes` analysis tracks immediate
MOV values after each literal seed and admits register-indexed accesses with
known constants. On the same NSE-1 image it produces 691 candidates from
the same 235 seeds, versus the original 562; all still lie at offsets
`00..3f` (`run_archive/cleanup-20261009/root/run_5110_constant_index_census_20261009.json`). This removes one
specific census blind spot without identifying SCU. The scanner remains
linear rather than control-flow complete; pre-seed constants, computed
indexes, table pointers, ARM-mode code and other address windows remain
outside this result. Do not promote a candidate to silicon ownership
without checking its branch path and observed transaction.
A wider literal-seeded Thumb scan over `000000..0fffff` yields 942
linear candidates from 6,029 seeds
(`run_archive/cleanup-20261009/root/run_5110_internal_window_census_20261009.json`). Candidates above
`040000` originate at seeds `2a8026` and `2ab3b2`, in data-like instruction
streams; they are not established executable MMIO paths. Passive taps on
`040000..07ffff` during an isolated 30-second native boot observe just one
32-bit MCUIF write at `040000`, data `6a0f6120`, reported PC `20005c`.
The separate DSPIF `030000..030003` positive control observes 20 writes.
No wider candidate transaction is observed; the RF gate still reports
6,499 frames and 207,040 reads
(`run_5110_internal_window_runtime_20261009/error.log`). This bounds that
boot lifecycle only, not all internal windows or dormant SCU behavior.
The boot write itself demonstrates why the Thumb census cannot close
ARM-mode initialization ownership. DSP-local programming and computed
addresses remain independent open routes.
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
emulated seconds and protects the current receiver boundary: 20,280 CTSI frame
expiries, 648,032 RF reads, 1,537 port-`0x32` writes with the reviewed ordered
prefix and queue producers, terminal `IMR=0x035f`, serviced INT0 and no
port-`0x38/0x39` reads. The earlier zero-port-32 observation above is not a
current gate invariant. This is a receiver/programming-activity gate, not an
acquisition gate. With
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
DSP I/O control ports `0x2c/0x2d` terminate in the opaque COBBA transport.
Data-space BDXR/BDRR use the separately owned BSP state and evidenced untimed
boot echo; I/O port `0x21` remains an opaque mask/write-back word, not a proven
microphone/DAC port. Operational PCM attachment remains open; see
[the address-space contract](cobba_control_boundary.md#native-nse-1-address-space-audit).

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

Operand admission is separate from observed instruction coverage. MMR
addressing excludes the long-offset/absolute Smem forms even where the same
modifier is legal for an ordinary memory instruction. The nine byte-encoded
MMR classes have a 4,608-case rejection/replay fixture, including reserved
bytes and both execution frontends. See the current
[operand-domain contract](c54x_execution_contracts.md#mmr-long-form-boundary) for scope;
this is not a hardware-trap claim or complete closure of RMW/copy operands.

## Executed-opcode coverage

### Current audit boundary

The current native cold-idle inventory is 626 words in 110 high-byte groups,
SHA-256 `05356c59d7b0843b8023b7a68822b1b258a2a18cd161db6dd33955580bf33a59`.
Two isolated controls (`tmp/c54x-frontend-idle-a` and `-b`) reproduce the full
ranked report exactly. All 626 forms now have executable fixture assertions,
with no execution-only or absent words. The reviewed fingerprint deliberately
replaces the former 594-word expectation after those gaps were closed; a unit
negative control rejects the previous fingerprint and a wrong current hash.

The original `nse1pcm`/`nse1bit` native DTMF tests have a separate 642-form inventory, jointly
covered by executable assertions in completed default, `mac-smem` and
`xc-signed` CPU fixtures. Its fingerprint, strict comparison and scope limits
are maintained in `cobba_control_boundary.md`; it does not change the cold-idle
fingerprint or establish native call speech.
The clock-owned boot-loopback configuration `nse1pin` additionally executes
retry-store `7721`; its 643-form inventory is covered when the native backend
controller fixture is included. The ordinary handset still has no evidenced
production audio-clock attachment.

With verbose logging, the core emits `[opdecode]` alongside `[opcov]`, using
the actual debugger decoder and synthetic zero extensions without target
memory accesses. `tools/c54x_opcode_coverage.py --frontend-report` requires a
consistent decoder record for every observed word, rejects invalid lengths
and ranks unsupported forms as `length=unknown`; raw-word fallback is never
promoted to a boundary. The synthetic-extension census reports 617 decoded
and nine unsupported first words. Those nine are extension-controlled `6f`
forms, resolved separately from actual fetched tuples below. Conditional
BC/CC/XC and unshifted Smem loads have reviewed boundaries. These reports
measure observed frontend coverage, not ISA completeness or pipeline timing.
`check-c54x-observed-coverage` passes the reviewed cold-idle fingerprint,
physical Menu and long-power runs, and all 626 union assertions. Menu adds no
new words; long power executes 625, also adding none. Execution assertions
and debugger boundaries remain independent evidence; neither establishes
physical operand visibility or interrupt drain.
The synthetic-extension report cannot characterize `6Fxx` forms whose
operation is selected by a second opcode word. Verbose `[opwords]` records now
capture these actual execution fetches without extra target-memory reads.
The isolated coverage gate requires `--extension-report`: every observed `6f`
word must have complete two/three-word tuples whose counts sum to its execution
count. Idle/Menu/power currently capture 18 distinct tuples across nine first
words. `6f83` selects both `0c1e` and `0c5f`; `6ff8` has nine distinct
address/operation pairs. `[optupledecode]` applies the real decoder to those
captured words using a bounded buffer, never rereading target memory. The gate
also requires `--require-extension-decoded`: every variant must decode at its
actual fetched length. All 18 tuples pass; together with first-word-independent
decoding this covers all 626 observed words. Missing variants, invalid lengths,
absent opcodes, malformed records and count drift fail validation. A zero
extension still cannot stand in for the executed operation.

Shifted memory ADD/SUB/LD/STL/STH boundaries follow SPRU172C syntax 4.
The required marker is `TMS320C54x shifted memory disassembler: PASS`:
114,688 legal address/shift/accumulator combinations on two program pages,
130,176 reserved-extension rejection cases, all 18 captured tuples and two
additional STH syntax cases. Signed shifts are -16 through 15; LD/STL/STH
reserve the source-selection bit that ADD/SUB use independently of destination.
Long/absolute Smem's address word precedes the opcode-extension word.
This decoder closure changes no execution semantics. A physical frontend still
needs operand effects, hazards, overlapping stages and interrupt-drain timing;
the observed boundaries alone do not provide those contracts.

ADDM immediate-memory decoding covers all 256 addressing forms on ordinary
and wrapping program pages, with bounded two/three-word extension reads and
four exact syntax checks. Long-offset and absolute forms retain address-before-
literal layout; this is debugger coverage, not a change to arithmetic execution
or physical pipeline timing. The required gate marker is
`TMS320C54x ADDM disassembler: PASS` (SPRU172C, ADDM and classes 18A/18B).

NEG, ABS and NORM decode all twelve source/destination combinations as
one-word instructions, checked on ordinary and wrapping program pages without
extension reads. Source is bit 9 and destination bit 8 (SPRU172C instruction
definitions); the required marker is `TMS320C54x unary disassembler: PASS`.
This adds debugger boundaries only, not arithmetic or pipeline validation.

RND adds four source/destination forms; SAT, MAX, MIN and EXP each have only
the bit-8 accumulator selector. Twelve exact syntax cases and four rejected
extra-selector cases are checked on two program pages without extension reads.
The required marker is `TMS320C54x normalization disassembler: PASS`, based on
the corresponding SPRU172C instruction definitions; arithmetic and physical
timing remain separate contracts.

Accumulator ADD/SUB signed shifts decode all 256 combinations of shift and
source/destination; ADD/SUB/LD ASM forms add twelve combinations. Each is
one word and checked on ordinary and wrapping program pages without extension
reads. The required marker is `TMS320C54x accumulator ALU disassembler: PASS`
(SPRU172C, ADD/SUB syntaxes 9/10 and LD syntax 9). This establishes debugger
boundaries only, not arithmetic validation or physical pipeline timing.
The executable `6f82/0c01` ADD and `6f83/0c21` SUB vectors separately assert
that their A results (14 and 6) leave B's full 40-bit `8176543210` sentinel
unchanged. `make check-c54x-core` verifies these assertions; they do not
establish other source/destination selections or physical pipeline timing.
A 104-case execution matrix verifies ADD and SUB with all four A/B
source-destination selections. It includes memory 8 at shifts -1, 0 and +1,
and memory `fff8` with SXM clear/set at shifts -16, -1, 0, +1 and +15.
Initial A/B are 10/30; expected shifted operands are explicit constants,
independent of the core's shift helper. Assertions also require unchanged
memory and AR2, preservation of the unselected accumulator, legal execution
and the two-word continuation. The gate requires the 104-case completion
marker. This does not cover every operand, intermediate shift magnitude,
saturation or physical pipeline timing.

Immediate LD decodes shifts 0–16 and both destinations (34 forms), checked
on ordinary and wrapping program pages with one literal extension. Unlike
immediate arithmetic, its documented opcode fixes bit 9 to zero; 34 extra-
source-bit aliases remain unsupported despite the execution handler's broader
mask. The required marker is `TMS320C54x immediate load disassembler: PASS`
(SPRU172C, LD syntaxes 7/8). This is decoder evidence, not alias-legality or
physical timing evidence.

LDU has reviewed one/two-word boundaries for all 512 addressing/destination
forms, with bounded extension reads on ordinary and wrapping program pages
and six exact syntax cases. The required marker is
`TMS320C54x unsigned load disassembler: PASS` (SPRU172C, LDU). Its unsigned
execution behavior is unchanged; decoder coverage is not a timing claim.

WRITA decodes all 256 data-source addressing forms with bounded one/two-word
reads on two program pages and four exact syntax cases. Its required marker is
`TMS320C54x WRITA disassembler: PASS` (SPRU172C, WRITA). Separate executable
fetch-order cases validate its existing data-to-program transfer, not physical
pipeline or repeat timing.

MVPD decodes all 256 destination-address forms on two program pages with
five exact syntax cases. Long offsets follow pmad; absolute addresses precede
it, matching the independently tested execution layouts. The required marker
is `TMS320C54x MVPD disassembler: PASS` (SPRU172C, MVPD; SPRU131G
section 5.5.3.2). Separate passive bus-tap fixtures validate Fetch-before-Read
and destination effects; neither proves physical pipeline stages or wait states.

Parallel ST/LD decodes 1,536 one-word forms: four accumulator source/destination
combinations and two accumulator-to-T variants across all dual-memory modifiers.
The gate checks two program pages, bounded reads and four exact syntax examples.
Its required marker is `TMS320C54x parallel load disassembler: PASS`
(SPRU172C, ST||LD). Parallel syntax does not establish physical overlap timing.

FRAME decodes all 256 signed eight-bit immediates. MVMM admits only AR0–AR7
and SP: 81 register pairs, with 175 rejected pairs. Both are one word, checked
on two program pages; the required marker is
`TMS320C54x frame/MMR move disassembler: PASS` (SPRU172C, FRAME/MVMM).
FRAME latency has conflicting primary wording: SPRU172C says no next-instruction
address latency, while SPRU131G table 7-9 assigns category-I latency 1 and
category-II latency 0 in compiler mode. Existing execution and its 24-case
stack-address gate follow the latter detailed table; decoding does not resolve
that physical-timing discrepancy or justify changing the execution model.

MVDD decodes all 256 packed dual-memory operands independently of ST/LD;
immediate DP loads decode all 512 nine-bit values. Both are one word and checked
on two program pages without extension reads, including four exact MVDD source/
destination examples. The required marker is
`TMS320C54x dual move/DP disassembler: PASS` (SPRU172C, MVDD and LD syntax 3).
Execution, address visibility and physical timing are unchanged.

Special-register loads cover 768 Smem forms targeting T, DP or ASM and all
eight immediate ARP selectors. Tests bound one/two-word reads on two program
pages, check destination syntax and retain an undocumented ARP-alias negative
case. The required marker is `TMS320C54x special load disassembler: PASS`
(SPRU172C, LD T/DP/ASM/ARP). Decoder lengths do not imply equal cycle costs:
the memory-to-DP execution remains its separately modeled three-cycle form.

Accumulator AND/OR signed-shift forms cover 256 combinations of shift −16–15
and independent source/destination selectors. Tests check exact syntax and
one-word bounded reads on two program pages; the required marker is
`TMS320C54x accumulator logical disassembler: PASS` (SPRU172C, AND/OR syntax 4).
This complements the reviewed XOR form without changing logical execution or
claiming physical pipeline timing.

Smem MPY, MPYR and MPYU retain separate signed/rounded/unsigned mnemonics
and both destinations; BITT names the T-selected bit test without an accumulator
operand. All 1,792 addressing forms have bounded one/two-word reads on two
program pages, with seven exact syntax cases. The required marker is
`TMS320C54x memory multiply/bit disassembler: PASS` (SPRU172C, MPY syntax 1,
MPYU and BITT). Arithmetic and timing execution are unchanged.

SFTL decodes all 128 signed-shift/source/destination combinations as one-word
logical shifts. Exact syntax and bounded reads are checked on ordinary and
wrapping program pages; the required marker is
`TMS320C54x logical shift disassembler: PASS` (SPRU172C, SFTL). Its existing
low-32-bit execution and carry behavior remain separate from this decoder test;
no physical pipeline timing is established.

Dual-memory ADD/SUB decodes 1,024 packed operand/destination forms; accumulator
MACA/MACAR/MASA/MASAR adds sixteen source/destination/rounding forms. Tests
bound reads to one word on two program pages and check exact operand ordering.
The required marker is `TMS320C54x dual/accumulator arithmetic disassembler: PASS`
(SPRU172C, ADD/SUB syntax 6 and MACA/MASA syntax 2). This does not change
arithmetic execution or establish parallel physical timing.

Memory-sourced RPT and ST T/TRN decode 768 addressing forms with bounded
one/two-word reads on two program pages and six exact syntax cases. The required
marker is `TMS320C54x memory repeat/store disassembler: PASS` (SPRU172C, RPT
syntax 1 and ST syntaxes 1/2). Repeat scheduling and register-store execution
remain unchanged; these are debugger boundaries, not physical timing evidence.

DADD, DSUB and DRSUB decode 2,048 long-memory addressing/accumulator forms
with bounded one/two-word reads on two program pages and seven exact syntax
cases. DADD retains independent source/destination selectors; the subtract
forms retain their direction and single accumulator selector. The required marker
is `TMS320C54x double arithmetic disassembler: PASS` (SPRU172C instruction
definitions). Paired-word execution and physical timing are unchanged.

SUBC decodes all 512 Smem/accumulator forms with bounded one/two-word reads
on two program pages and five exact syntax cases. The required marker is
`TMS320C54x conditional subtract disassembler: PASS` (SPRU172C, SUBC).
Its divide-step arithmetic, carry/overflow behavior and timing remain separately
tested execution contracts, not conclusions of debugger decoding.

Accumulator-A MPYA/MACA/MACAR/MASA memory forms cover 1,024 addressing
encodings; short MPYA/SQUR adds four forms with selected destinations. Tests
check two program pages and exact syntax separately from arithmetic execution.
The required marker is `TMS320C54x accumulator-A multiply disassembler: PASS`
(SPRU172C, MPYA, MACA/MASA syntax 1 and SQUR syntax 2). Fixed accumulator-A
inputs are not interchangeable with the T-based ordinary multiply family.

T-based memory MAC/MACR/MAS/MASR covers 2,048 addressing, rounding and
destination encodings on two program pages, with eight exact syntax cases.
The required marker is `TMS320C54x memory MAC/MAS disassembler: PASS`
(SPRU172C, MAC/MAS syntax 1). Extension reads are bounded; arithmetic,
accumulator visibility and physical timing are unchanged by this decoder work.

Short shifted STL/STH syntax 3 covers 1,024 one-word forms on two program
pages, with four exact syntax cases including acquired `9a5f`. The shift is
unsigned 0-15; the address uses the dual-address modifier, not Smem encoding.
The required marker is `TMS320C54x short shifted store disassembler: PASS`
(SPRU172C, STL/STH syntax 3). Execution semantics and timing are unchanged.

Conditional RC/RCD use the same 22 reviewed individual predicates as BC/CC/XC,
giving 44 one-word forms checked on two program pages. Non-delayed RC has
conditional step-out flags; delayed RCD deliberately omits immediate step-out
because two delay words still execute. Compound predicates remain unreviewed.
The required marker is `TMS320C54x conditional return disassembler: PASS`
(SPRU172C, RC[D]); no execution or physical delay timing is inferred from it.

Exact acquired `6f95/0e20` (`SUB *AR5+,0,B,A`) and indirect loads `1087`
(`LD *AR7,A`) and `1182` (`LD *AR2,B`) have six independent execution cases,
each with SXM clear/set and memory `8001`. They assert signed/unsigned results,
preserved source accumulator and memory, and the single AR5 postincrement
versus unchanged load pointers. The required marker is
`TMS320C54x native gap arithmetic/load: PASS`; this is selected operand/status
coverage, not all inputs or physical pipeline timing.

Twelve additional cases assert `BANZ` forms `6c8d/6c8e` with zero and nonzero
AR5/AR6, including comparison before postdecrement; `MAR 6db7` with positive
and negative AR0 increments; and stores `80b4/838a/8383` with independent low/
high-word patterns, addressed writes and post-updates. Unselected ARs and
accumulators are also checked. The core runner requires
`TMS320C54x native gap branch/address: PASS`. Its simulation ceiling is two
seconds because the expanded fixture chain exceeds the former one-second
limit; completion still requires every marker and the intentional terminal
exit, never a timeout. No instruction timing is changed by this deadline.

Eight cases additionally assert `f67b` (`SFTA B,-5,A`) under both SXM
settings, `3185` accumulator-high/memory multiply under both FRCT settings,
`f301/4000` immediate addition with clear/set carry outcomes, and `f648`
(`LD B,8,A`) with positive/negative source values. They check full 40-bit
results, carry, T publication, unchanged source/memory and AR5. The required
marker is `TMS320C54x native gap math: PASS`. Overflow/saturation boundaries
and physical stage timing are not established by these selected operands.

Twelve memory cases assert `6b82` ADDM carry/overflow and 16-bit writeback,
`4583` signed/unsigned high-halfword loads, `21f8` absolute MPY under both
FRCT settings, `3ed3` partial-carry add with circular BK zero/eight, `5793`
double load with guard extension and two-word postincrement, and `818b`
low-word store with postdecrement. They check selected result/status values,
unchanged T/source/memory where applicable and exact AR2/AR3 changes. The
required marker is `TMS320C54x native gap memory: PASS`; other operand/flag
combinations and physical timing remain outside this coverage.

The final twelve cases assert `9a5f` shifted high-word store/postdecrement,
`f20f` immediate ADD, `f47d` signed/logical right shift, `f50f` shifted
accumulator ADD, `f736` shifted accumulator SUB and `f761` overflow/saturation.
They check full results, exact carry/overflow flags, source preservation and
the compact store's address update. The runner requires
`TMS320C54x native final gaps: PASS`. This closes assertion coverage for the
captured 626-word trace, not complete ISA, stage timing or native speech.

### Debugger instruction boundaries

MVDM/MVMD decode their two-word MMR transfers; PSHD/POPD decode ordinary
and extended Smem addresses. The gate checks 960 forms, 64 rejected MMR
encodings, bounded extension reads and ten exact examples on two pages.
Its marker is `TMS320C54x move/stack disassembler: PASS`. POPD separately
has four executable fetch-order cases: destination extensions precede the
stack read, with preserved value, SP and pointer results. Those assertions
are part of `TMS320C54x operand fetch order: PASS`, not inferred from
debugger coverage or promoted to physical pipeline timing.

Ordinary and ASM-shifted STL/STH decode their accumulator and Smem address;
short accumulator and signed ASM loads decode without extensions. The gate
checks 2,048 store encodings and 544 short-immediate encodings on two pages,
fourteen exact examples and one invalid ASM encoding. Its marker is
`TMS320C54x store/short disassembler: PASS`. Address-update and store-stage
semantics remain those separately covered by execution fixtures.

Parallel ST||MAC/MACR/MAS/MASR and SQDST decode both dual-memory operands
without extension reads. The gate checks 4,352 forms on two pages and ten
exact examples, including all four CMPL accumulator pairings. Its marker is
`TMS320C54x parallel disassembler: PASS`. Parallel syntax does not establish
physical concurrency: the core's existing effects remain instruction-atomic.

LDM, STLM, PSHM and POPM decode their single-word direct/short-indirect
MMR forms. The gate checks 1,344 forms and 192 rejected extended encodings
on two pages, no extension reads and ten exact syntax/rejection examples.
Its marker is `TMS320C54x MMR disassembler: PASS`. Indirect POPM separately
has four executable cases for pre/postincrement, compatibility-mode ARP
selection and an SP destination. It uses the shared page-zero MMR helper;
forbidden extended encodings remain unimplemented. The fixtures failed on
the former direct-only handler and pass with the corrected handler, requiring
`TMS320C54x indirect POPM: PASS`. This is selected stack/addressing coverage,
not physical stack-write or subsequent compiler-mode DAGEN timing evidence.

BANZ/BANZD decode short-indirect and long-offset Sind operands. The gate
checks 240 supported forms and 272 unclaimed direct/absolute forms on two
pages, bounded target/displacement reads and seven exact examples. Long
forms consume the displacement before the target, consistent with the
reviewed execution fixture; TI's BANZ example 3 confirms a three-word
fallthrough. Its marker is `TMS320C54x BANZ disassembler: PASS`. This is not
a new branch timing or delay-slot execution claim.

DLD/DST decode the accumulator and Lmem operand in load/store order,
including one-word ordinary and two-word extended-address layouts. The gate
checks 1,024 mode encodings on two pages, bounded address reads and eight
exact examples. Its marker is `TMS320C54x double memory disassembler: PASS`.
These frontend checks do not establish memory alignment, address-update or
write-stage timing beyond the existing execution fixtures.

Shift-16 Smem ADD/SUB decode independent accumulator selectors and the
one-word direct/short-indirect or two-word extended-address layout. The gate
checks 2,048 mode encodings on two pages and eight exact operand examples.
Its marker is `TMS320C54x high memory ALU disassembler: PASS`. Execution
assertions remain separate from these debugger length/syntax checks.

MAR decodes the 128 explicit indirect forms, including long-offset and
absolute extensions; 128 direct forms stay unsupported. The gate checks both
sets on two pages and seven exact examples. Its marker is
`TMS320C54x MAR disassembler: PASS`. The older RPTBD fixture's mislabeled
`6d10` was corrected to `6d90` (`MAR *AR0+`, I=1). This retires an accidental
fixture assumption, not a runtime direct-MAR legality claim: the execution
handler's treatment of direct encodings remains unaudited.

Dual-memory MPY and MAC/MACR/MAS/MASR decode their X/Y pointer modifiers
and accumulator fields as single-word forms. The gate checks 4,608 encodings
on two pages with no extension reads and seven exact operand/rounding examples.
Its marker is `TMS320C54x dual multiply disassembler: PASS`. This describes
the TI operand layout; it does not expand arithmetic or memory-hazard
execution coverage.

Immediate ADD, SUB, AND, OR and XOR decode the literal, independent accumulator
selectors and shifts 0 through 16, including the separate shift-16 opcode
forms. The gate checks 340 forms on two pages with bounded literal reads,
thirteen exact syntax examples and a negative unsupported-family example.
Its marker is `TMS320C54x immediate ALU disassembler: PASS`. Other immediate
families remain raw until reviewed; execution and timing are unchanged.

Accumulator SFTA, shifted LD and shifted XOR decode the signed five-bit
shift and independent source/destination bits. ROL, ROR and ROLTC decode
their accumulator source without reading an extension. The gate checks
384 shift forms on two pages and exact examples including all six rotates
and the -16/+15 shift endpoints. Its marker is
`TMS320C54x accumulator disassembler: PASS`. These are debugger boundaries,
not new arithmetic execution assertions or pipeline timing evidence.

The ordinary Smem ADD/SUB, ADDS/SUBS, TS-shifted ADD/SUB and ADDC/SUBB
families expose one-word direct/short-indirect and two-word extended address
layouts. The gate checks 4,096 mode encodings on two pages, bounded reads
and eight exact syntax examples. Its marker is
`TMS320C54x memory ALU disassembler: PASS`. These frontend assertions do
not establish execution for every operand or alter arithmetic/timing.

Unshifted, TS-shifted and shift-16 `LD Smem,A/B` expose their one-word
direct/short-indirect and two-word long-offset/absolute layouts. The gate
checks 1,536 mode encodings on two pages, bounded extension reads and ten exact address/destination
examples. Its required marker is `TMS320C54x load disassembler: PASS`.
This is a frontend contract; it neither changes execution nor supplies
pipeline-stage timing.

Memory AND/OR/XOR decode their Smem address and accumulator. The gate checks
1,536 encodings on two pages with bounded extension reads and six exact
examples; its marker is `TMS320C54x memory logical disassembler: PASS`.
This does not establish additional arithmetic/status semantics.

BC/BCD, CC/CCD and XC decode 22 explicit named predicates. The core gate
checks 132 forms on two pages, bounded extension reads, stepping flags and
exact asymmetric examples; unreviewed predicate combinations remain raw.
This validates debugger syntax and lengths, not execution of every condition
or physical pipeline timing. The required marker is
`TMS320C54x conditional disassembler: PASS`. A fresh native coverage run
retains all 626 asserted words and the protected opcode-set fingerprint.

The native core disassembler decodes the reviewed branch/call/return,
repeat, idle, interrupt and status-bit forms, including extended accumulator
branches and calls. `check-c54x-core` checks 48
exact encodings at two program addresses, including an extension word crossing
the 16-bit page boundary. Near targets retain their program page; far targets
carry their explicit extended address. The native 5110 Phone book menu gate
also passes with this decoder enabled.

The timer reload/mask families `STM #lk,MMR`, `ST #lk,Smem`,
`ANDM #lk,Smem` and `ORM #lk,Smem` are decoded as complete two- or
three-word instructions, with direct, short-indirect, long-offset and absolute
operands. STM excludes the long-offset/absolute modes forbidden for MMR
addressing by SPRU131G section 5.6. The gate checks 992 decoded mode encodings
at ordinary and page-end addresses, all 32 invalid STM/MMR encodings, exact
extension-read bounds and twelve asymmetric operand/modifier examples.
Operand ordering matches the reviewed executable decoder fixtures:
long-offset logical immediates precede the offset; immediate stores and
absolute logical forms put the address extension first. This agreement is not
an independent silicon oracle for extension ordering. Evidence is
`tmp/c54x-literal-boundary-final.log`, requiring
`TMS320C54x literal disassembler: PASS`. This checks debugger decoding, not
the runtime legality of every addressing mode or the physical pipeline.

MVDK, MVKD, MVDP, PORTR, PORTW, READA, CMPM and BITF now expose complete
instruction lengths and operand layouts. The gate checks 2,048 mode encodings
on two pages, exact extension-read bounds and nine asymmetric operand examples.
READA has no extra immediate; long-offset PORTR/PORTW/MVDP put their port or
destination before the displacement, unlike the displacement-first move/test
forms. These are debugger/frontend boundary checks against the reviewed
execution layouts, not proof of every mode's silicon legality or stage timing.
The required marker is `TMS320C54x move/test disassembler: PASS`.

Unknown forms still return a raw one-word display entry without `SUPPORTED`.
That fallback is not a verified instruction boundary and must not be used to
populate a future decoded-instruction pipeline. The current observed 626-word
set, including its 18 captured extension-controlled tuples, now has reviewed
boundaries. This satisfies the observed-set frontend prerequisite, not full
ISA coverage or a physical pipeline implementation.

### Native timing continuation

Native configurations still recognize interrupts before an instruction-atomic
`execute_one()`. Hardware interrupt entry retains the accepted vector,
source, then captures the continuation at INTR Decode before stepping SP
decrement, vector fetch/IACK and stack write on separate CPU cycles. All
entry continuation state is saved.
The explicit IRQ/NMI fixture requires the stack write one cycle after vector
fetch; retained-vector save/replay snapshots now interrupt entry before that
write and verify its completion without a second fetch or acknowledgement.
Reset and debugger redirection cancel the pending entry. The aggregate entry
cost remains the existing calibrated five cycles. Its former residual cycle
is now assigned to Decode before Access; this does not establish foreground
drain or the complete physical pipeline.
IFR remains pending after source acceptance and is selectively cleared at
the first vector fetch/IACK, as specified by SPRU131G 6.10.4. A one-clock
checkpoint after acceptance asserts unchanged SP/stack, both original IFR bits
and no acknowledgement. A second one-clock checkpoint requires those same
properties after Decode and saves the entry before Access. Changing IMR to
enable a different pending source cannot reselect the accepted vector; normal
and restored continuations both clear only the originally accepted bit and
stack the original PC once. The early-clear implementation fails this
checkpoint (`tmp/c54x-ifr-fetch-red.log`); the required marker is
`TMS320C54x accepted interrupt checkpoint: PASS`. This checks entry ordering
and replay, not Decode-stage foreground drain or full pipeline latency.
`check-c54x-core` requires the IRQ/NMI entry-spacing marker.
Maskable INTM setting belongs to that same vector Fetch/IACK stage, following
SPRU131G 6.10.8, rather than the earlier source selection. The accepted-entry
checkpoint requires INTM clear before IACK and set inside the acknowledgement
callback, both before and after save/load; its marker includes `intm_at_iack=1`.
The prior early-setting implementation fails this checkpoint. NMI's existing
acceptance behavior and the aggregate entry cycle count are unchanged. This
is an entry-side ordering correction, not foreground pipeline retirement or
proof of the full instruction pipeline.
The `decode_before_access=1` checkpoint fails the former Access-first entry
(`tmp/c54x-decode-before-red.log`). Return-PC capture now belongs to Decode,
not source acceptance; vector Fetch/IACK follows Access and stack write follows
Fetch by one cycle for both IRQ and NMI. The first ISR execution retains its
previous aggregate placement. The short-period timer still enters 108 times
without reaching the foreground mask write, so this ordering correction does
not resolve that pipeline-drain boundary or promote recorder completion.
The previously banked `check-c54x-rom4-rf-boundary` snapshot is 20,282 frame
expiries, 648,096 RF reads and 1,519 port-`32` writes. Current fresh-storage
runs at that milestone instead reproduced 20,285 / 648,192 / 1,520, with no port-`38/39` reads.
A generated-overlay control using the original instruction-boundary interrupt
routine reproduces the same changed totals: the staged-frontend refactor is
not their cause. A second generated-overlay control restores the CPU's
`device_start`, `device_reset` and timer-clock reader from `146b9ff8` (the last
documented passing interrupt-entry milestone); it reproduces those same totals
too. These clock/reset additions therefore do not explain the mismatch either.
Both controls use newly generated own EEPROM and no retained flash/SIM state;
reusing the first run's NVRAM is not a valid cold control. The exact snapshot
gate was deliberately left failing pending the provenance audit. The
EF_SMSS parent correction above settled that mismatch; the current
independent-transfer scan snapshot supersedes both historical counts.
The earlier save-state pass protects its tested restoration boundary, not
RF acquisition or handset speech.

#### Staged frontend prototype

The optional `set_pipeline_frontend` path owns Prefetch, Fetch, Decode,
Access, Read and Execute slots inside the existing CPU, reusing its semantic
backend and interrupt-source/entry operations. The separate `nse1pipe`
research machine enables it; ordinary handset and MU4 configurations do not.
The basic memory/ALU subset covers NOP, one-word immediate loads, two-word
shifted immediate loads, direct DP-relative loads and STL/STH stores, and
short indirect loads using plain, post-decrement, post-increment or
pre-increment addressing, and ANDM/ORM with DP-relative, plain/post-decrement/
post-increment short indirect or three-word absolute (`f8`) operands, and
RETF/RETFD with one-word load/NOP or two-word immediate-load delay slots.
Two-word unconditional B/BD use the same retained-literal and redirect path.
ADDM now shares the retained RAM read/modify/write path with ANDM/ORM.
SPRU172C page 4-9 specifies SXM/OVM-dependent arithmetic and C/OVA updates,
not unconditional 16-bit wrapping. The atomic and staged frontends use one
unchanged arithmetic helper: staged Read retains the source word, address
and immediate; Execute publishes the result and flags once. The original
worker's `6b82 0002` form is included. There are 768 replay cases across
DP-relative, stationary/postincrement AR2 and absolute RAM addresses, eight
value pairs and eight SXM/OVM/sticky-OVA combinations. They include TI's
`8007 + fff8 -> 8000` saturation example, signed/unsigned boundaries and
carry clearing. Source RAM changes after Read cannot alter the retained
result. Accumulators remain unchanged. Absolute ADDM also admits AR0..AR7
and SP destinations, but not other mapped registers. The `addm-ar` profile
checks all eight ARs, four initial values, four immediates, all sixteen
SXM/OVM/C/OVA combinations, both CMPT settings and two instruction pages,
with save/load before Read, before Execute and after publication (24,576
cases plus replay). Operand mutation after Decode/Read verifies retention;
the result and C/OVA oracle is independent of the CPU helper. The
`addm-ar-load` profile checks 768 cases plus replay with short and long-offset
unshifted LD consumers, two or three separating NOPs, and one AR publication
before the consumer's data read. Both LD forms are Category I under SPRU131G
table 7-7; the address extension alone does not make the long-offset form a
Category II consumer. Forbidden-gap behavior is not claimed. Other mapped
destinations, long-offset ADDM addressing, repeats, compiler-mode direct
sources and adjacent producer/consumer hazards remain excluded or unvalidated.
The phase placement inherits the bounded RMW prototype, not measured silicon.

Register-only ROR, ROL and ROLTC on A/B are admitted through the existing
semantic backend. SPRU172C pages 4-143..4-145 specify rotation of the low
32 bits and clearing of bits 39..32: ROR/ROL use C as input; ROLTC uses TC;
all publish the shifted-out bit to C and leave other status bits unchanged.
The `rotate` profile independently checks 4,608 cases plus save/load replay:
six forms, eight guard/sign/endpoint patterns, all C/TC combinations, four
SXM/OVM combinations, two pages and three snapshots. Each case executes two
adjacent rotates and checks state every clock, including carry forwarding
and preservation of the other accumulator. Repeat-body admission remains
excluded; these fixtures make no silicon bus-phase claim.

RPTZ A/B,#lk shares immediate RPT's retained-count, body-latching and
interrupt-protection path, additionally clearing the selected 40-bit
accumulator once at Execute (SPRU172C 4-150). The `rptz` profile checks
768 cases plus replay across both accumulators, counts 0/1/3/17, C/TC and
SXM/OVM settings, two pages and three save points. An existing immediate
store body proves exactly count-plus-one ordered writes and AR updates;
mutating the setup literal after Decode cannot change the repeat. Other
accumulator and status bits remain unchanged. Existing nested, block and
control-delay repeat exclusions also apply to RPTZ. This does not admit
new body instructions by itself.

Short indirect STL/STH A/B bodies now use the pipelined repeat issuer:
each iteration owns a Read-stage address update and Execute-stage store,
not a host-side memory fill. The `rpt-store` profile checks 49,152 cases plus
replay across all ARs, plain/postdecrement/postincrement/preincrement modes,
counts 0/1/7/255, RPT/RPTZ setup, SST/SXM combinations, two source patterns,
two instruction pages and three save points. A test-only source mutation
after each write verifies Execute-live sampling and independent saturation
results. There are no data reads, exactly count-plus-one writes, and no
changes to other ARs, the other accumulator or status. `rpt-store-edge`
checks 4,096 cases plus replay for mapped first destinations 0000/0010/
0020/005f: rejection precedes both DAGEN updates and bus effects, including
preincrement wrap. These are implementation exclusions, not claimed silicon
traps. Direct, indexed, extended, ASM-shift and mapped repeated stores remain
outside this admitted subset; physical bus phase is not claimed.

BITT consumes retained Smem and T(3..0), setting only TC from bit
`15-(T&15)` (SPRU172C 4-23). It shares Smem address resolution and the
single Read-owned bus access, but does not publish T as LD/MPYA do.
The selector is retained in Read, not re-read at Execute; SPRU131G 7.5.6
defines T-consumer latency and protected T writers. The `bitt` profile
checks 6,144 cases plus replay: DP/SP direct, short plain/postdecrement/
postincrement, absolute and two long-offset forms, all selectors, two
T-high-bit patterns, complementary data and both initial TC values, two
pages and three save points. Mutating both operands after Read cannot
change the result; T, accumulators and other status bits are preserved.
`bitt-retd` adds 1,536 replay cases with absolute or long-offset BITT
occupying RETD's two delay words: exactly one stack read, one operand read,
one extension fetch and target-only continuation. This two-word delay
admission is limited to RETD; other control owners retain their exclusions.

Short BITT also admits either delay word of BD, BCD, CALLD, FCALLD and
RETD. `bitt-ctrl` checks 49,152 cases plus cleared-pipeline replays: DP,
plain/postincrement AR and SP sources, all sixteen selectors, two T-high-bit
patterns, complementary words, both initial TC values, two pages and three
checkpoints. The second-slot cases use an organic first-slot LD to T.
Separate post-Read memory and T poison cohorts check retention without
allowing the two mutations to cancel. BITT publishes TC at Execute, while
TC/NTC BCD retains its earlier branch decision. The fixture checks owner-only
stack effects, poisoned opcode/target/return data and exact continuation.
These are prototype ordering checks, not measured physical pipeline phases;
mapped operands, extended BITT under other controls and arbitrary interrupt
composition remain outside this qualification.

Nondelayed RC TC/NTC shares the existing conditional-return stack and
redirect path. SPRU172C 4-133 specifies no status changes, one stack pop
only on a taken return, and the Class 32 taken/untaken distinction.
The decision samples TC after the older Execute publication, then retains
both that decision and the popped target. `tc-return` checks 384 cases plus
replay: both predicates/outcomes, initial status or short/absolute BITT
producers, zero/one separating NOP, two bit selectors, two pages and four
save points. Mutating TC and the stack after the decision cannot change
the path or target; untaken paths perform no stack access. The fixtures
retain the existing three/five-cycle untaken/taken cadence. Delayed and
compound status predicates remain outside the admitted subset.

Short circular MAR admits the four modifiers -1/-AR0/+1/+AR0 through the
existing Access-owned DAGEN update. `mar-circular` checks 49,152 cases plus
replay across all AR encodings, CMPT states, BK=1/2/3/13/16/31/32/250,
four legal positions/stride classes, two pages and three save points.
Explicit alignments and a signed-residue oracle are independent of the
CPU's mask/adjust implementation; inputs obey SPRU131G 5.5.3.4's buffer
and stride constraints, including the AR0 pointer/index alias. The opcode
performs no data access, updates only the selected AR/ARP, and does not
repeat its update after restore. The native 6dc2 entry used BK=00fa and
AR2=0701, a nonzero-buffer case. BK=0 remains the qualified ROM4 behavior
described in `djr_dsp_integration.md`, not a newly validated silicon rule.
Long-offset circular MAR and close BK/AR producer compositions remain
unvalidated; no physical pipeline-phase claim is made.

BCD AEQ/BEQ admits a complete two-word Smem ADD/SUB/ADDS/SUBS instruction
in its delay span, using the existing retained operand and ALU semantics.
`bcd-arithmetic` checks 36,864 cases plus replay across both accumulators,
both branch predicates/outcomes, absolute and two long-offset forms, four
operand/source patterns, SXM/OVM combinations, two pages and three save
points. Arithmetic may change the branch-tested accumulator without
changing the already sampled decision. An independent signed-40-bit oracle
checks the result, saturation and C/OV flags; memory mutation after Read
cannot change the operand. There is one data read and no data write or call
frame. `bcd-arith-edge` checks 288 replay cases preserving the new subset's
ANEQ/BNEQ exclusions and rejecting a two-word operand starting in the final
delay word before DAGEN or operand effects. The pre-existing absolute ADD
admission for other owners is unchanged. Other new owner compositions and
physical pipeline timing are not claimed.

Nondelayed CC now admits all twelve signed A/B predicates, sharing CCD's
existing signed-40-bit predicate decoder without changing frame ownership.
The `signed-call` profile extends the existing CC/RC fixture to 768 replay
cases: eight zero/sign/guard patterns, two pages and four save points.
Changing the predicate after Read and the target literal after Decode
cannot change the selected path. Taken calls publish one frame, fetch the
target once and return with balanced SP; untaken calls never access stack
memory. The original 64-case AEQ/BEQ and 768-case signed-return profiles
remain unchanged. Compound/status-call predicates are not newly admitted.

Short RAM PSHD now retains its source address/value and stack destination.
SPRU172C page 4-131 places its SP read in Decode and update in Access;
the prototype preserves that allocation separately from the existing PSHM
Read-owned allocation. Thirty-two cases cover DP/compiler-relative and
stationary/postincrement AR2 sources with four save/replay boundaries.
Six compiler-mode consumers verify table 7-9's Category-I one-clock SP
latency: the adjacent load uses pre-push SP, while one NOP exposes updated SP.
Six cases compose the original `6b82 0002 4b92` ADDM/PSHD sequence.
SPRU131G table 7-2 assigns DB reads to the first DARAM half-cycle and EB
writes to the second, but this does not determine the consumed value:
section 7.3.2 specifies an internal-bus bypass for a same-location on-chip
read with a pending write. PSHD retains the forwarded ADDM result without
a second physical RAM read; the ADDM Write clock and flag publication
remain unchanged.
All 44 cases require one stack allocation/publication, retained samples and
no duplicate replay accesses. PSHD preserves status and accumulator state.
Peripheral sources, extended addressing, general stack/DAGEN conflicts and
physical bank arbitration remain excluded or unvalidated; these fixtures do
not execute the complete zero-control-word worker body or native recorder.

Short RAM POPD retains a captured destination and one stack sample before
publication (SPRU172C page 4-127, class 17A). Its Read-owned SP increment
shares the partial POPM placement model, rather than asserting a measured
silicon update phase. Thirty-two cases cover DP/compiler-relative and
stationary/postincrement AR3 destinations with four replay boundaries;
six compiler-mode consumers verify one-clock Category-I SP latency.
Twelve cases execute the original six words at `02:3dc5`--`02:3dca`:
ADDM #2,*AR2; PSHD *AR2+; MVDK *AR2,AR3; POPD *AR3+.
Both SXM states retain the forwarded result for the stack copy, while ADDM
updates source RAM and its arithmetic flags. MVDK loads the second source
word as the destination pointer; POPD writes to that captured address despite
AR3 postincrement. All 50 cases require one stack consumption/publication,
no duplicate replay access and preserved non-arithmetic context. The static
context tool pins the six original words, but does not assign meanings to
the operands. Peripheral/extended destinations, general stack/DAGEN/bank
conflicts and native recorder behavior remain unvalidated. Complete worker
composition is specified below.

Short accumulator stores now include stationary/postdecrement/postincrement
indirect addressing, and short ST T/TRN use the same retained destination
path (SPRU172C page 4-167, class 10A). Read captures the address and applies
its AR update once; Execute publishes the selected register value to that
captured address. Seventy-two cases cover A/B high/low words, T/TRN,
DP/compiler-relative and stationary/postincrement AR3 destinations, and
three save/replay boundaries. Changing AR3 after address capture must not
retarget the write. Three sequence cases verify consecutive high/low B and T
publications with a neutral gap; that gap does not claim to execute the
original intervening CMPR. All 75 cases preserve register/status values and
produce no duplicate writes on replay. Source-register producer latency,
extended modes and physical bus placement remain unvalidated; T/TRN stores
are limited to RAM destinations. Complete worker composition is specified
below.

Staged CMPR samples unsigned ARx/AR0 in Access and retains its predicate
until Execute changes TC, without re-reading live address registers
(SPRU172C page 4-34; SPRU131G table 7-7 Category I). The 435 replay cases
include all four comparisons, all eight auxiliary registers and four
unsigned boundary pairs. Mutating the registers after Access must not
change the comparison result. Forty-eight producer controls require zero
intervening instructions after STM/MVDK, one after POPM and two after STL
before the new AR value becomes visible. Those controls use DP zero for
the direct memory/register operands, rather than relying on an inherited
data page.

Three cases execute the original `8393 8193 f4ab 8c93` at
`02:3dcb`--`02:3dce`: high/low B stores, CMPR EQ,AR3, then ST T.
With initial AR3=`0470` and AR0=`0471`, CMPR samples `0471` in Access
before the low-word store's Read-stage increment; its Execute result must
stay true after the pointer advances to `0473`. Replay around Access,
Read and Execute must preserve all three writes exactly once. This is
bounded composition of original instructions, not completion of the
whole worker, physical bank arbitration or native recording.

Staged MAR supports stationary, single-word postdecrement/postincrement
and signed linear long-offset modification (`6d80..6d97`, `6de8..6def`).
SPRU172C pages 4-92--4-93 define compatibility-mode AR0 redirection through
ARP and nonzero ARF publication of ARP; the standard mode leaves ARP alone.
The complete slot owns the retained long-offset word and applies its AR
modification once in Access. The following short LD must see the new
pointer with no intervening NOP, consistent with SPRU131G table 7-7's
unlisted-instruction zero-latency rule. This placement is a bounded model,
not measured silicon phase timing or a general DAGEN arbitration claim.

The 294 replay cases cover all eight AR fields, both compatibility states,
three short forms and long offsets `0003`, `ffff`, `8000`. A pointer
mutation after the following LD captures its address must not retarget
that load or cause MAR to modify the pointer again at Execute. Program-bus
taps require exactly one offset fetch per leg, including restoration of
the retained word before Access. Six cases
execute the contiguous original `108a 6d8a fd30 8082 6dea 0003` at
`02:3dcf`--`02:3dd4`, covering both XC outcomes and three checkpoints.
The initial AR2=`0470` decrements twice, then advances by three to `0471`;
an accepted store publishes the captured source at `046e`, despite the
same-clock long MAR update, while a rejected store leaves that location
untouched. Each replay performs one data read and zero or one store.
Circular/bit-reversed/AR0-indexed MAR forms, absolute MAR and general
producer conflicts remain excluded. Complete worker composition is
specified below; live native recording remains a separate gap.

The original worker's `1892` and `1a8a` are **AND** `*AR2+,A` and
**OR** `*AR2-,A`, not memory ADD/SUB. SPRU172C syntax-1 logical opcode
diagrams and the existing atomic decoder agree. The six words at
`02:3dd5`--`02:3dda` are now pinned by the read-only context tool:
`f000 0004 1892 1a8a 6d8a 8082`. The immediate operation is ADD #4;
the following memory operations perform mask/merge, not arithmetic.
Operand meanings remain unclassified.

Staged short AND/OR/XOR A/B now retain their Access-stage address and
Read-stage zero-extended word, then operate on the selected accumulator
in Execute without changing status flags. Basic DP/CPL sampling and
short AR updates use the existing LD address path. The 576 replay cases
cover three operations, both accumulators, four addressing forms, four
value pairs and both SXM states. Changing source RAM after Read must not
alter the result; the unselected accumulator and status remain unchanged.
Register/peripheral operands, extended forms and general source-producer
latencies remain excluded or unvalidated.

Five cases execute the contiguous original six-word logical tail on
page 2. With A=`1234`, AR2=`0470`, mask=`ff0f` and merge word=`00f0`,
the result is `12f8`, written at `046f`. Both memory operands are changed
after Read and AR2 changes after the final destination capture; neither
mutation may affect the retained result or store. Replay around both
logical reads and the final store requires exactly two reads and one
write. This closes the selected tail composition, not the complete
zero-control worker, IRQ entry or a native recorder transaction.

The complete original worker is now separately pinned at
`02:3dbc`--`02:3de1`: 38 words, including its 22-word zero-control body.
The composed fixture also executes the four original CALLD/STM-parameter
words at `02:3d92`--`02:3d95`. Forty-eight cases cover both CMPR/XC
outcomes, both saved INTM states, both SXM states and six replay boundaries.
Each leg requires ten ordered data reads and nine or ten writes,
including the forwarded RMW source through PSHD/POPD, consecutive B high/low
and T publications, the conditional control-word store and final mask/merge
store. The stack balances, the worker restores the saved INTM predicate,
and RETD reaches its page-2 continuation without fetching a third delay
word. The caller continuation is an explicit test marker, not a claim to
execute the rest of the original outer handler.

The non-taken BC retains the already-fetched first body word; its body
timeline is one clock earlier than a redirect-style expectation. With
the selected fixture, ADDM Read is clock 20, PSHD retains the pending result
through the internal bypass before ADDM Write in clock 21, and the final
result is written in clock 41.
The selected CMPR match controls a store in clock 33. The exact values
and clocks are bounded CPU-fixture observations, not measured silicon or
recovered recorder operand provenance.

The logical and worker fixtures use ST1 bit 8 for SXM; bit 9 is OVM.
The corrected 576-case matrix checks zero-extension against actual SXM.
The original outer-context handler is also composed with the active body:
51 handler words plus all 38 worker words, rather than the former
skip-body subset. The 128 cases cover pages 0/2, both saved INTM and SXM
states, both CMPR/XC outcomes and eight replay checkpoints. Every leg
requires sixteen ordered stack writes, seven ordered worker-source reads,
six or seven worker RAM writes, one reserved-return read and one caller
continuation fetch. The worker's B publications contain the handler's
incremented `13572469` counter, while all original caller registers and
ST0/ST1 are restored on return with SP=`1000`. The original compiler-mode
reserved-return store and real caller restoration instructions execute;
there is no synthetic continuation inside this composition.

This closes the original outer-handler/body composition for the supplied
CPU fixtures. Real worker operand provenance, application foreground
progress under short recurring interrupts and coherent native
recording remain unvalidated; handsets and MU4 still use the atomic frontend.

Non-delayed staged FB retains both the seven-bit target page and its
16-bit target word (SPRU172C page 4-53, class 29A). It uses the existing
unconditional-branch redirect/flush path without call stack effects.
Forty replay cases cover source pages 0/2 and target pages 0/1/2/3/7f,
requiring one literal fetch, one target fetch, no sequential instruction
effects and the committed target page. The frontend exposes Fetch-time
XPC before retirement; that intermediate register value is not a claimed
silicon commit phase. General XPC-consumer hazards, FBD/FCALLD and nested
far control/IRQ interactions remain unvalidated or excluded.

The original MU4 timer wrapper is pinned separately by the read-only
context tool. Uploaded `02:204c`--`02:204f` contains
`4a1e f495 f882 3d73`: PSHM XPC (MMR `001e`), NOP and FB `02:3d73`.
The original reserved return at `02:4044` is `f4e5` (FRETE), which restores
the saved page/PC and clears INTM. Thus a complete timer fixture must
include the page save and far return, not merely jump to the handler or
stop at its reserved-return address. Static upload words do not by
themselves validate their low-vector runtime mapping or hardware IRQ
entry; the executable composition below covers a bounded fixture.

The original timer vector, all 89 handler/worker words and original FRETE
now execute together through a CPU-timer interrupt. The 48 replay cases
vary foreground pages 0/3, SXM and CMPR/XC outcome across six checkpoints.
The fixture uses the observed warm lower-32K OVLY mapping, PMST=`2020`,
IMR bit 3, initial TIM=`001f` and PRD=`ffff`: one timer expiry followed by
a deliberately long next period. Those are explicit CPU-conformance
inputs, not a recovered application timer configuration.

Every replay leg requires one timer IACK, one original vector/handler
entry and one FRETE. The eighteen stack writes include the hardware PC,
vector XPC and context saves, with FRAME's reserved return slot skipped
until its final `4044` publication. FRETE consumes the saved XPC then
hardware PC exactly once, returns to the original foreground page and
clears INTM. The unmodified context restoration balances SP and preserves
the caller's saved status/registers. A subsequent foreground marker is
both fetched and retired (B=`66`), so the check does not equate a return
address or speculative fetch with actual foreground execution.

This proves the bounded original ISR composition with supplied CPU
inputs; it does not prove real recorder operand provenance, recurring
interrupt fairness, the warm application's short timer lifecycle or
recording completion. No handset or MU4 profile is switched to the
experimental frontend by this gate.

Recurring original-ISR coverage uses 96 cases, with eight complete timer
returns per replay leg. It varies PRD=`00ff`/`0001`, foreground pages 0/3,
SXM and the initial CMPR outcome, with six checkpoint classes. The
foreground executes an absolute ADDM counter loop; a counter write is a
retirement observation, not a fetched marker. Original worker state is
never reseeded between entries. A successful conditional store changes
the control word from zero to `0470`, so later entries naturally skip
the body; the other fixture continues its original copy/mask progression.
Every case restores caller context, increments the original tick counter
eight times, and reproduces the exact eighth-return clock and foreground
counter after save/restore.

The period comparison is intentionally discriminating. PRD=`00ff`
continues foreground work after the first return: representative cases
finish with 146 or 164 writes, versus five at the first return. With
PRD=`0001`, the original ISR still returns eight times, but the counter
stays at five and timer IFR bit 3 remains pending. Thus completing the
original staged ISR does **not** alone remove the short-period starvation.
These are supplied CPU-timer workloads, not validated DA150 timer-clock
units or proof that the real application should use either fixture rate.
The next boundary is the original warm application's timer configuration,
masking and scheduling lifecycle, not a guessed ISR completion message.

The timed replay harness resumes through the restored checker deadline.
Restoring a stream inside a callback and then scheduling from the cached
pre-load callback time shifts emulated timer phase; the exact-cycle gate
guards against that harness error. Neither device timer semantics nor
the default atomic handset/MU4 path are changed by this fixture correction.

Staged `10f8`/`11f8` absolute LD now retains its 16-bit address word and
Read-stage data, then loads A/B with the selected SXM behavior (SPRU172C
page 4-66, syntax 1/class 3B). The 64 replay cases vary accumulator,
actual SXM bit and four data values across four boundaries. DP and CPL
are deliberately unrelated to the absolute source. Mutating the program
literal after Fetch and RAM after Read must not retarget or change the
load; each leg performs one literal fetch and one operand read. The gate
includes the original idle flag address `a531`, but does not establish
absolute peripheral timing or a complete idle/wake lifecycle.

The read-only context tool pins all 29 original words at
`02:98d0`--`02:98ec` and the 15-word wrapper at `02:3d2f`--`02:3d3d`.
The routine's absolute load of `a531` is followed by BC ANEQ to IDLE1:
zero executes the ordered PRD=`1`, TCR=`20`, TINT-mask, wake-mask and
flag=`1` writes; all 65,535 other input words skip that setup. This is an
exhaustive **local** predicate, not a global flag-writer or reset census.
After wake the routine removes its local frame, restores AR1 and uses
FRET. Its wrapper aligns SP, performs FCALL `02:98d0`, restores CPL/SP
and returns. The staged IDLE1 primitive has the bounded coverage below;
the complete call and bounded timer-ISR composition have the coverage below.

Staged IDLE1 (`f4e1`) is a Decode barrier: older instructions drain,
younger fetched words are discarded, and the continuation is fetched anew
after wake. CPU pipeline clocks stop during the idle interval while the
peripheral timer continues. IMR-enabled requests wake independently of
INTM; INTM=0 accepts the ISR before continuation, whereas INTM=1 resumes
without servicing it. Masked pending requests do not wake the CPU, and NMI
wakes and vectors regardless of IMR/INTM (SPRU131G section 6.11.1).

`check-c54x-core` covers 48 staged IDLE1 cases: external IRQ, masked IRQ
followed by enabled IRQ, timer IRQ, INTM-blocked external/timer wake and
NMI; source pages 0/2; and four save/restore boundaries before entry,
during sleep and after return. The fixture changes its own continuation
word during sleep to distinguish a fresh fetch from stale speculative
execution. ISR cases verify continuation PC and page stack traffic; blocked
cases retain IFR without an IACK. The saved barrier/resume state reproduces
each leg. This verifies the declared restart/refetch model, not a measured
silicon restart bus phase or execution of the original 44-word call.
Staged IDLE2/3 use the same drain/refetch barrier and the shared mode-specific
clock-stop/wake implementation as the atomic frontend. The focused
`idle-modes` profile covers 96 cases: all three modes, enabled external IRQ,
INTM-blocked IRQ, a masked request followed by enabled IRQ, NMI, pages 0/2
and four save/load boundaries. Its timer continues in IDLE1 and freezes in
IDLE2/3; continuation replacement, real ISR stack/return traffic and replay
are checked. The unchanged core fails at the first IDLE2 case with `illegal=1`;
the corrected staged decoder passes all 96. IDLE in a control-delay window
and reserved NN=11 remain excluded. Physical clock/restart latency is not
established by this fixture.

```sh
.venv/bin/python tools/run_c54x_core.py --mame mame/mame \
  --rompath mame/roms --profile idle-modes --output-root tmp/c54x-idle-modes
```

The original AA22 instruction `028922: f5e1` is a concrete native consumer
after a serial playback-control transition; AA99 has different contents at
that address. Instruction admission does not itself qualify that complete
control transaction, EOF draining or handset boot.

The complete original 44-word idle routine/wrapper executes in eight staged
cases with INTM set: zero/nonzero local flag, even/odd caller SP and two
save/restore checkpoints during sleep. The zero path owns exactly the ordered
PRD=`1`, TCR=`20`, IMR TINT-clear, IMR wake-mask and local flag writes.
Its short-period timer remains pending but masked; the nonzero path leaves
timer/masks untouched. A real external IRQ6 wakes both paths without an ISR
under INTM, then original FRAME/POPM/FRET and the wrapper restore AR1,
caller SP and CPL and reach the same-page caller. Both legs retain the
unserviced wake request. These are typed CPU inputs, not a claim about the
real flag's initialization or a promoted MU4 execution profile.

Shared SP updates must respect instruction age: a younger FRAME must not
retarget an older PSHM/POPM's Read-owned stack slot. FRAME's Access placement
remains a partial model, but its update follows those older stack operations
within the clock. The original adjacent PSHM AR1/FRAME pair and final AR1
restoration regression-test this constraint.

Eight additional cases compose the nonzero-flag idle path with INTM clear,
the original 94-word timer vector/context handler/worker/FRETE route and
the observed lower-32K shared program mapping. A real timer expires during
IDLE1, acknowledges TINT with continuation `98ea`, visits each original
entry once and returns before the wrapper reaches its caller. Even/odd SP
and four replay checkpoints cover sleep, active ISR and completed return.
The final caller SP, AR1, B, CPL/page and cleared IFR reproduce without
re-seeding worker state. The initial timer interval and worker inputs are
typed CPU-fixture inputs, not recovered native startup provisioning.
Four zero-flag cases additionally begin with TINT enabled and INTM clear.
The original PRD/TCR writes start the short timer, but the staged frontend
drains the already-decoded ANDM mask before vector execution. The ISR stacks
continuation `98e4`, executes the original context route once and returns to
the ORM wake-mask write. All five setup writes complete, `a531` becomes `1`
and the CPU sleeps with IMR=`08c0`, masked IFR=`0008`, PC=`98ea` and SP=`0ffa`.
Even/odd caller stacks and two in-flight replay checkpoints reproduce this
result without a post-return guard, timer divisor change or forced
continuation. This is a bounded 138-word staged composition, not acceptance
of native warm startup or measured silicon interrupt timing. Global flag
writers, complete surrounding opcode coverage and native warm scheduling
remain open; production MU4 profiles still use the atomic frontend.

Non-delayed staged FCALL now retains a distinct caller-page and page-stack
destination alongside the PC return word (SPRU172C page 4-57, class 29B).
The original opcode word owns the first SP allocation; the complete
two-word slot owns the second. It publishes PC then XPC in separate
clocks, while target Prefetch uses the explicit target page. It must not
save the live Fetch-stage XPC, which can already belong to the callee.
These retained fields are registered for save states. The placement is
a bounded class-29B model, not measured silicon bank arbitration.

The 80 FCALL/FRET replay cases cover source pages 0/2, target pages
0/1/2/3/7f and eight boundaries across literal assembly, both stack writes
and both return reads. FRET consumes saved page then PC; changing their
RAM locations after the respective reads must not alter the return.
The speculative caller continuation is flushed, then fetched again and
retired only after the callee returns. Each leg has exactly two pushes,
two stack reads, balanced SP, unchanged status/RTN and the correct source
page. FCALLD, nested control/IRQ interactions and general stack-bank
conflicts remain excluded or unvalidated. This covers the idle wrapper's
far-call primitive, not execution of the complete original idle call.

Direct STM immediate initialization of AR0--AR7, SP, BK and T is supported;
PMST is the separately covered non-DAGEN exception below. Qualified low-RAM
direct and plain-indirect forms are described above; other MMR destinations
and extended indirect STM forms require their own contracts.

STM immediate to PMST (`771d`, class 12A in SPRU172C page 4-176) retains
its second word and publishes once in Execute rather than inheriting DAGEN
early writes. This publication phase is a declared partial model, not a
measured PMST latency. Sixty-four replay cases cover four full register
values, source pages 2/3, lower/upper program targets and four boundaries
around literal assembly, publication and redirection. Changing the fixture
literal after Fetch must not alter the write. Subsequent lower-bank fetches
use the new OVLY bit; upper targets stay in their original logical page.
The full PMST value (including IPTR), XPC, SP and ST1 reproduce without
duplicate literal fetches or writes. This STM fixture alone does not
establish IRQ/IPTR selection or prefetch ownership; the isolated PMST
profiles in the current overlay contract cover their separately stated
subsets. No native production profile is promoted by these cases.

Short-form MVPD (`7cxx`, destination mode below `98`) retains its absolute
program-source literal separately from the data destination. Its Read stage
owns destination addressing/postincrement; an extra held program-bus clock
samples program space through the source instruction's logical page and
current OVLY mapping. Execute writes that retained word once. SPRU172C
pages 4-117/118 distinguish this program-to-data transfer from a data move;
the held Read placement is a declared partial model of its extra cycle,
not measured bus arbitration.

The 128 replay cases cover DP/stack direct, indirect and postincrement
destinations, source pages 2/3, lower/upper source regions, both OVLY states
and four checkpoints around literal assembly, held program read and data
write. Mutating the literal after Fetch and source RAM after its read cannot
retarget or change the stored word; each leg has one literal fetch, one
program read and one data write. Long-offset forms, register/peripheral
destinations and control-delay placement fail closed. Interrupt/mapping
changes during an ordinary held read remain unvalidated.

Long-immediate RPT (`f070`) now owns a retained short MVPD body. Repeat
Decode disables acceptance, including NMI, before its literal finishes;
complete body Decode discards younger speculative words. The first MVPD
uses its held program read, then the stream performs one program read and
data write per clock, with implicit source increment and encoded destination
modification. Exactly count+1 transfers retire before a fresh continuation.
First/steady-state bus placement is partial, not measured silicon timing.
MVPD and the separately covered READA stream below are the only admitted
repeat bodies. Nested repeats and control-delay RPT fail closed.

The 128 transfer replay cases cover counts 0/1/3/15, pages 2/3, both OVLY
states and indirect/postincrement destinations, with four checkpoints across
capture, initial read, streaming and completion. Sixteen additional cases
present IRQ or NMI at RPT Decode and replay with the request pending. IACK
must follow the final write, stack continuation `9004` and restore a balanced
stack before the changed continuation retires. Count-zero pending-interrupt
cases drain an additional already-prefetched continuation before discarding
it; it must not retire. No timer/return guard or atomic fallback is used.

The original `bootrecstage` reset sequence also executes its full 65,313-word
copy: observation-only taps verify every program source address, every data
destination and value equality, then the diagnostic reaches `00:0e41`.
This closes the reset copy, not native startup or recorder completion; the
current unsupported startup boundary is in
[the MU4 contract](5510_bringup.md).

Absolute ADDM to SP (`6bf8 0018 lk`) is the covered exception to ADDM's
register/peripheral exclusion. It retains the SP Read operand and immediate,
then publishes the wrapped result in Execute using the existing ADDM
arithmetic. The original serial-start prefix `STM #1200,SP; ADDM #03ff,*(SP);
ANDM #fffe,*(SP)` therefore selects SP=`15fe`, without a firmware-state poke.
The 256 replay cases cover four initial values, four deltas (including wrap),
both SXM states, two pages and four boundaries around retained Read/write.
Bus taps verify exactly three ordered SP reads and writes, including a later
LDM read of the aligned result. Other ADDM register destinations stay
fail-closed. General compiler-SP latency, concurrent stack operations and
register-bank conflicts for this RMW form remain unvalidated; these cases
prove the bounded original prefix, not every SP consumer hazard.

Immediate LD #k,ARP (`f4a0`--`f4a7`) has no latency to its following
indirect consumer (SPRU131G tables 7-3/7-15 and example 7-41). The staged
model publishes ARP once in Access, after older stack reads; Execute must
not overwrite a younger indirect instruction's ARP update. This publication
phase remains partial. In CMPT mode encoded ARF=0 selects loaded ARP;
nonzero ARF selects its encoded register and updates ARP. Standard mode uses
the encoded register without that update; TI requires software to keep ARP
zero in standard mode.

The 512 replay cases cover all eight immediate values, encoded ARF=0/2,
both CMPT states, with/without postincrement, two pages and four checkpoints
around publication, destination selection and retained operand Read.
Nonzero ARP in standard mode is an adversarial CPU fixture, not handset
provisioning. Each leg verifies one source read, only the selected auxiliary
register's update and preservation of unrelated ST0/ST1 bits. Mutation after
Read cannot alter the loaded accumulator. General ST0-load/ARP conflicts,
CMPT-change hazards and interrupt acceptance during publication remain open.

READA (`7exx`) captures accumulator A as its program address; extended
devices use A(22:0), independent of the instruction's XPC (SPRU172C page
4-136). The short destination forms and absolute `7ef8 dmad` retain their
data destination separately. Four extra held Read clocks model the
instruction's documented non-repeat cost; A is sampled after older Execute
publications, then the selected program word is retained for one Execute
write. Sampling and bus placement remain partial, not measured silicon
phases. RAM and AR0--AR7 destinations are admitted for nonrepeated READA;
the repeated RAM stream is covered separately below. Other register/
peripheral destinations, long-offset forms and control-delay placement fail
closed.

The 192 absolute-to-AR1 replay cases compose a preceding ADD #1,A with
READA, three source pages (0/2/7f), lower/upper program regions, both OVLY
states, zero through three consumer gaps and four checkpoints. Mutation of
A after source capture and program RAM after Read cannot retarget the
transfer. Younger indirect consumers with zero/one gap retain their old
AR1 address; two/three gaps select the published pointer under the declared
stage model. Forty-eight short RAM/postincrement cases independently check
the one-word form's program read, data write and single auxiliary update.
Each replay leg has one program read and one destination write. General
DAGEN conflict chains, conditional-execute placement and interrupt acceptance
across an ordinary held bus remain unvalidated; repeated RAM READA has the
separate coverage below.

Short non-delayed BANZ (`6c80`--`6c97`) retains its target literal, tests
the selected auxiliary register before modification and owns its encoded
update in Access. A taken branch discards the two sequential Decode slots;
a zero predicate leaves the fall-through intact. SPRU172C page 4-16 assigns
class 29A four taken/two not-taken clocks; the Access redirect models that
gap without BC's additional conditional-evaluation bubble. Exact publication
and bank arbitration remain partial.

The 288 replay cases cover values 0/1/ffff, no modification/decrement/
increment, encoded ARF=0/1, both CMPT states, pages 0/2 and four checkpoints
around predicate/update/redirect. Tests distinguish zero-before-decrement
from nonzero-after-decrement and one-before-decrement from zero-afterward.
Only the selected AR changes, once, on either outcome; BANZ does not read or
write data memory. Mutation of its target literal after Fetch cannot
retarget it, and taken branches must not retire fall-through markers.
Delayed/long-offset forms, repeat and control-delay placement fail closed.
Preceding DAGEN-writer conflicts, conditional-execute placement and interrupt
acceptance through the redirect remain unvalidated.

Absolute memory-sourced RPT (`47f8 Smem`) samples its AR0--AR7 or RAM
count in Read and retains it through two extra clocks (SPRU172C page 4-146,
class 5B). Repeat protection begins in Decode, not after that Read. The
common repeat stream now also admits RAM-destination READA: it retains A's
initial program pointer separately, increments that internal pointer per
iteration and does not update architectural A. A one-word READA body resumes
at body+1; the two-word MVPD body still resumes at body+2. Bus placement is
partial. Other count addressing forms and control/peripheral count sources
remain excluded; repeated READA register destinations fail closed.

The 384 replay cases cover counts 0/1/3/15, program pages 0/2/7f, both OVLY
states, ordinary and 7fff-to-8000 crossing sources, AR1/RAM count sources and
four checkpoints around count assembly/Read, captured program pointer and
streaming. Mutating the count after Read, A after pointer capture and the
first source word after its program Read cannot change the copy. Each leg
has one count read and exactly count+1 ordered transfers and postincrements.
Sixteen AR1-count cases additionally present IRQ/NMI at repeat Decode and
replay with the request pending: IACK follows the last write and stacks the
one-word body's actual continuation. The ISR's discarded prefetched words
are distinguished from copy-source reads. No source re-seeding, return
guard or atomic fallback is used. General preceding-writer conflicts,
conditional-execute placement and repeated register/peripheral publication
are not established by these fixtures.

Absolute accumulator ADD (`00f8`/`01f8`) retains its source-address literal,
selects the AR0--AR7 or RAM operand in Access and samples it once in Read.
Execute uses that word with the existing 40-bit ADD arithmetic; source
mutation after Read cannot change the result. Other register/peripheral
sources, shifted/short forms, repeat and control-delay placement remain
excluded. This does not infer a physical MMR arbitration delay.

The 2,048 replay cases cover four operands, four accumulator values (including
32-bit overflow and 40-bit negative values), both SXM/OVM states, A/B,
AR1/RAM sources, two pages and four checkpoints around operand ownership
and retirement. Independent integer expectations verify the result, low-32
carry, destination overflow flag and OVM saturation; the other accumulator,
SP and unrelated status bits are preserved. Each leg has one literal fetch
and one operand read. Conditional-execute placement, preceding accumulator/
status-writer hazards and peripheral source timing remain unvalidated.

Short direct ANDM/ORM/ADDM now admit compiler addressing as well as DP
addressing. CPL/DP belong to complete Decode; SP is sampled in Access using
the existing basic-compiler visibility path. The effective address and Read
operand are retained through Execute, rather than consulting live CPL/SP/DP
again. The same retained context also applies to noncompiler direct RMW;
absolute and indirect forms retain their existing ownership.

The 768 replay cases cover three operations, two offsets, four data values,
two immediates, both initial CPL states, two program pages and four checkpoints.
They deliberately change CPL after Decode, SP/DP after Access, the immediate
after Fetch and RAM after Read. Each leg still has one literal fetch, one
operand read and one write to the original frame/page address, while the
later context changes survive. This is a CPU ownership fixture, not a
firmware-state intervention. General SP-producer/stack interleavings,
memory-sourced DP forwarding, conditional-execute placement and RMW
peripheral arbitration remain unvalidated.

Short accumulator AND/OR/XOR now admit peripheral-window sources
`0020`--`005f` through the same paid extra-access-clock path as peripheral
loads. Lower register sources remain excluded. Access owns the effective
address and auxiliary modification; the wait holds the stages without
issuing a read, then Read samples the peripheral once and Execute consumes
the retained zero-extended word. This models CPU access cost, not a specific
peripheral's electrical response latency or bus-priority arbitration.

The 864 replay cases cover three operations, DP direct/indirect/postincrement
addressing, three PRD values, A/B, both SXM states, two pages and four
checkpoints before/during/after the wait and publication. Each instruction
has one peripheral read, no peripheral write and at most one auxiliary
update. A later PRD mutation cannot change the retained logical result.
Fixture mutations and debugger peeks are excluded from instruction bus
counters: timer state import/export traverses MMIO. The tests do not establish
GPIO `003c`'s electrical contract; the native bench maps it through the
existing GPIO model. IRQ overlap and simultaneous peripheral read/write
arbitration remain separate work.

PORTW uses opcode family `75xx`, not MVKD (`70xx`). The short and absolute
forms retain data-source and I/O-destination operands separately (SPRU172C
page 4-130, class 28A/28B). Absolute `75f8` consumes source address, then
port address; Access owns source addressing/modification, Read captures its
word and Execute writes through AS_IO. Peripheral data sources use the
existing extra-access-clock path. External READY/IOSTROBE timing is not
modeled by these minimum-cycle fixtures. Repeat, long-offset and control-delay
forms remain excluded.

The 320 replay cases cover absolute low-accumulator/RAM/peripheral sources,
short postincrement and compiler direct sources, four values, both SXM
states, two pages and four ownership checkpoints. Each leg has the original
literal order, one source read, one real I/O-handler write and at most one
auxiliary update. Mutating the literals after assembly and source after Read
cannot retarget/change the write. This is CPU transfer conformance, not
validation of a connected peripheral's behavior.

The MU4 diagnostic's `75f8 0008 0080` reads AL at data MMR `0008` and writes
external port `0080`. That port has no modeled device; `bootrecstage` exposes
an observation-only diagnostic sink with no invented state or response.
A following unsupported Decode can stop an older accepted instruction before
retirement, so advancing the reported opcode boundary alone does not prove
the external write occurred. With compiler immediate ST covered below, the
sink now observes value `00df` at port `0080`; its external meaning remains
unknown. Existing atomic profiles are unchanged.

Short immediate ST now retains complete-Decode CPL/DP and Access SP for its
direct destination, admitting compiler stack-frame stores. Read presents the
cached address and Execute writes its original immediate without reading the
destination. This reuses the basic compiler-addressing path rather than
consulting live context during the store.

The 128 replay cases cover four values (including opcode-shaped `f495`), two
offsets, both initial CPL states, two pages and four ownership checkpoints.
They mutate CPL/literal after Decode and SP/DP after Access. Each leg still
has one literal fetch, no destination read and one write to the original
address, preserving the later context changes. General SP-producer/stack
interleavings, memory-sourced DP forwarding, conditional-execute placement
and peripheral-write arbitration remain unvalidated.
Supported compiler-mode, stack, branch/call/return and conditional-execute
contracts are specified below. Remaining forms fail closed without atomic fallback.

Direct STLM A/B to AR0-AR7 publishes the saturated low accumulator word in
Execute, with no destination read and no DP/CPL-derived address. SPRU172C
4-175 establishes page-zero addressing; SPRU131G 4.3.3 establishes SST/SXM
saturation, table 7-5 places store-type DAGEN writes in Execute, and table
7-7 requires two intervening instructions for a normal indirect AR consumer.
The source/saturation phase remains this partial frontend's Execute model,
not an independently measured silicon phase. Indirect STLM and other MMR
destinations remain unsupported. STLM also participates in the existing
DAGEN-bank conflict checks for younger STM, MVDK and POPM early writes.

The 768 replay cases cover all eight AR destinations, both source
accumulators, four input patterns, both SST/SXM states, zero through three
intervening NOPs and saves after Decode, Read or Execute. Each leg verifies
one register write, one consumer read, an unchanged source accumulator and
the old/new pointer boundary at two intervening instructions. Original MU4
startup advances through `8811` at `00:3092`. This remains coverage evidence,
not native startup or recorder acceptance; existing atomic profiles are unchanged.

Extended DST A/B now retains its second-word offset/absolute address in
Decode. Access applies table 5-4's ordinary offset (AR unchanged), linear
preincrement or circular preincrement once; absolute addressing uses the
retained literal without consulting AR. Read captures the saturated pair
after older Execute writes. Execute writes the high word, then the held
clock writes the low word at `address ^ 1`, preserving odd-address ordering
(SPRU172C 4-47). The extra extension word adds one clock to the existing
two-write path. Stage placement remains partial, not measured silicon.

Short-indirect DST A/B uses the same retained pair and two-write path.
Access applies MOD 0--11 once: linear increment/decrement and circular
unit steps are two words, while AR0-indexed and reverse-carry steps retain
their index semantics. The first write is clock 6 and the paired write is
clock 7 in this partial frontend model. Another 9216 replay cases cover
all twelve modes, eight encoded registers, both accumulators, even/odd
addresses, CMPT and SST, with signed-store source patterns and BK=16.
They poison AR/BK after Access and the accumulator after Read, and restore
after Decode, source capture or the first write. No destination reads,
duplicate pointer updates or duplicate writes are permitted. Unsigned
saturation, arbitrary BK regions and control-delay compositions for these
new short forms remain outside this fixture matrix.

The 3072 replay cases cover four address forms, eight encoded registers,
both accumulators, four offsets/patterns, both compatibility and SST states,
and checkpoints after Decode, Read or the first write. They change the
literal after Decode, AR/BK after Access and source accumulator after Read;
the original address/pair still publishes exactly once with no destination
read. Opcode-shaped offset `f495`, negative offsets and both address parities
are included. Register/peripheral destinations, control-delay placement,
general bank arbitration and circular-region provenance remain unvalidated;
the first two fail closed. The original upload passes `4ee1` at `00:309b`,
still without native startup/recorder acceptance or atomic fallback.

Zero-shift long-offset LD A/B (`10xx/11xx`, modes `e0..f7`) retains
the signed offset in Decode. Access uses the shared ordinary-offset,
linear-premodify or circular-premodify helper once; Read samples RAM or
AR0--AR7 once; Execute sign-extends according to SXM and publishes only
the selected accumulator. The existing absolute `f8` path is unchanged.
The offset literal adds one clock: Read is clock 6 and Execute clock 7
in this partial frontend model. These are declared stage placements, not
measured silicon timings. New offset forms reject other control/peripheral
sources and control-delay placement before any source read.

The 12288 RAM replay cases cover the three offset forms plus absolute,
eight encoded registers, both accumulators, eight offsets/patterns,
SXM/OVM and compatibility mode. Another 384 cases resolve an ordinary
offset into each mapped AR source under both SXM states and both destinations,
including a source/address-register alias. Fixtures change the literal after
Decode, the selected pointer/BK after Access and the source word after Read;
replays after Decode, Access or Read require one source read, no data writes,
unchanged non-ARP status and an unchanged unselected accumulator. Arbitrary
circular-region provenance, operand-bank conflicts, XC/interrupt composition
and silicon phase fidelity remain unvalidated.

Unsigned memory load LDU (`12xx/13xx`) shares the retained LD address/
Read path, but Execute always publishes the raw 16-bit sample with all
upper accumulator bits clear, independently of SXM and OVM. Like signed LD,
it excludes write-only short MOD=3. LDU admits direct DP/CPL,
the eleven read-capable short-indirect modifiers and the offset/absolute forms; other
control/peripheral sources fail closed before Read, while mapped AR0--AR7
are explicitly covered. Extended control-delay placement and repeats
remain unsupported.

The 52,224 RAM replay cases cover seventeen address forms, eight encoded
registers, both destinations, eight values including `8000,ffff,f495`,
SXM/OVM, compatibility mode and Decode/Access/Read saves. Another 2304
cases cover each mapped AR source through direct, absolute and ordinary
offset addressing, including an address/source-register alias. The shared
signed-load matrix remains a separate acceptance marker. Fixtures change
the literal after Decode, AR/SP/BK after Access and the source after Read;
each replay requires one source read, no data writes and no literal refetch.
Every fixture tick checks that only Access-owned ARP changes before Execute,
and that the accumulator publishes at clock 6 for short or 7 for extended
forms in this partial frontend model. These checks are not silicon timing
evidence. Physical memory-bank arbitration, mixed status/producer hazards
and wider control/interrupt composition remain unvalidated.

Indirect DLD A/B (`56xx/57xx`) admits short MOD 0--11 except write-only MOD=3, and long
offset/absolute MOD 12--15. Decode retains an extended operand literal;
Access applies the shared long-word address helper or short pointer step
once. Linear and circular unit steps are two words; AR0-indexed and
reverse-carry steps retain their index semantics. Read retains the high
word at `address`, then the low word at `address ^ 1`. Execute combines
the 32-bit pair and extends its high sign bit into the guard byte when
SXM is set, independently of C16. It changes only the chosen accumulator
and any Access-owned compatibility ARP update.

The 23,040 replay cases cover fifteen address forms, eight encoded
registers, both accumulators, four payload patterns, both address parities,
SXM/C16 and compatibility mode. They change extended literals after
Decode, AR/BK after Access and both source words after Read. Saves in
Decode, Access or Read require exactly two ordered source reads and no
data writes on each replay. Short paired reads occur at clock 5, extended
paired reads at clock 6, with publication one clock later in this partial
frontend model. This does not establish physical bus arbitration or
silicon phase timing. Register/peripheral pairs and extended control-delay
placement fail closed; arbitrary BK regions and short control-delay,
XC/interrupt and source-bank conflict compositions remain unvalidated.

Long-offset BANZ (`6cxx`, modes `e0..f7`) is a three-word branch:
opcode, signed offset, then same-page target. It is not a memory RMW
instruction. Decode retains both literals separately. Access tests the
resolved offset address for zero, with ordinary offsets leaving AR
unchanged and linear/circular premodification updating AR once. Thus an
ordinary `AR=1, offset=-1` falls through while retaining AR=1. No data
memory is read or written. The retained decision redirects taken paths
to the original target even if AR/BK or the program literals subsequently
change. Instruction extent is three words for control and retirement
bookkeeping; existing two-word short BANZ remains covered independently.

The 2304 replay cases cover all three offset forms, eight registers, eight
initial-AR/offset pairs, compatibility mode, two program pages and saves
after the first literal, complete Decode or the Access decision. They
cover zero/nonzero resolved addresses, 16-bit wrap, circular wrap and
opcode-shaped offset `f495`. Taken paths fetch the retained target once
at clock 7 in this partial frontend model; not-taken paths execute the
fallthrough markers without fetching the target. Predicate/redirect phase
timing is not silicon-validated. BANZD, broader short-modifier coverage,
arbitrary BK regions and XC/interrupt or preceding DAGEN-writer composition
remain outside this fixture matrix.

Short-indirect BANZD (`6e80..6e97`, SPRU172C BANZ[D] 4-16) shares
BANZ's Access-owned pre-modification count, selected AR update and retained
same-page target. A taken delayed branch redirects Prefetch without flushing
the two delay words; a not-taken branch retains its sequential continuation.
The existing control-delay fence covers the branch and both delay words.
The 3,072 replay cases cover all eight AR selectors, values `0/1/ffff/8000`,
stationary/postdecrement/postincrement modes, CMPT, two program pages and
both two one-word instructions and one two-word instruction in the delay.
Target poisoning after Decode and count changes after Access must not alter
the outcome. Delay effects publish before the chosen continuation; snapshots
cover retained target/count and both delay-publication clocks. Tests require
one target fetch when taken, no data accesses and no repeated AR update.
These are partial frontend timings, not measured class-29 silicon timing.
Long-offset/preincrement/circular BANZD and new pending-IRQ cases are not
covered by this matrix; unsupported delayed addressing forms stay rejected.

Short-memory immediate ST repeat bodies (`76xx`, modes below `98`) retain
one opcode and literal. ST is not in SPRU131G table 6-17's accelerated-repeat
list; its declared stream therefore retains separate address and write clocks
for each subsequent iteration. The address clock captures EAB and updates the
selected pointer once, and the write clock publishes the original literal and
consumes one repeat count. Pending address/phase are saved explicitly. The
38,400 replay cases cover DP, CPL, stationary AR1, postincrement and
postdecrement; short/long RPT; counts `0,1,15,32,255`; literals
`0000/0001/8000/ffff`; SST/SXM/OVM; two program pages and none/IRQ/NMI.
They require count+1 writes, one opcode/literal fetch after poisoning, no
destination reads, unchanged accumulators/ST0, correct half-iteration
pointer/counter ownership, deferred interrupt return and restoration before,
between or after writes. Long-offset/absolute, preincrement and
circular/bit-reversed repeated ST forms remain rejected, as do mapped/
peripheral destinations. Clock placement is partial, not measured silicon.

SUBC (`1exx/1fxx`, [SPRU172C 4-192](https://www.ti.com/lit/ug/spru172c/spru172c.pdf))
has single-step staged coverage. Access retains the source address, Read
samples one memory word and Execute performs the conditional division step
through the helper shared with atomic execution. SXM extends the divisor;
the ALU difference is `src - divisor*32768`. A nonnegative difference yields
`difference*2+1` and C=1; a negative difference yields `src*2` and C=0.
The result wraps to 40 bits without OVM saturation. The helper now projects
sticky OVsrc from the ALU difference being outside signed 32-bit range;
this follows the core's common ALU-overflow model but the exact SUBC
overflow-edge mechanism remains unvalidated against silicon. This replaces
the atomic path's complete omission of OVsrc, not a claim of final fidelity.
The 30,720 replay cases cover six RAM forms with AR1/AR6, all eight mapped
AR sources, both accumulators, 64 dividend/divisor pairs and SXM/OVM.
TI's intended division domain uses positive dividends and divisors; negative
or guarded fixture values test the declared model, not validated division use.
Changed RAM/pointers and Execute-live accumulator inputs distinguish operand
ownership; Decode/Access/Read snapshots check one read and no data writes.
Other addressing forms and mapped accumulator/T/TRN inputs remain outside
this matrix. Control-delay SUBC remains rejected.

Short-memory SUBC repeat bodies now use the retained repeat stream after
the first staged iteration. Each subsequent clock reads the current divisor,
updates the selected pointer once, applies the shared conditional-subtract
helper and consumes one repeat count. The opcode is fetched once; DP/CPL
context is retained, and interrupts remain pending until count+1 executions
complete. The 38,400 legal replay cases cover DP, CPL, stationary AR1,
postincrement and postdecrement; short/long RPT; counts
`0,1,15,32,255`; both accumulators; four dividend/divisor pairs; SXM/OVM;
two program pages and none/IRQ/NMI inputs. Decode/first/middle/final
snapshots check arithmetic at every iteration, one divisor read per iteration,
no opcode refetch after poisoning, correct pointer footprint, deferred IRQ
return address and only interrupt-owned stack writes. TI's independent
65/7 example must produce `00020009` after sixteen iterations. Long-offset,
circular/bit-reversed and mapped/peripheral repeated sources remain rejected.
These checks establish the declared repeat model, not silicon timing or the
unresolved SUBC overflow-edge behavior.
Short MOD=3 preincrement is write-only, not a valid SUBC source. A separate
512-case replay bank covers its rejection in both frontends, standalone and
under short RPT, without data, pointer or arithmetic-result effects. The
adjacent repeated immediate-store bank retains its 38,400 legal cases.

Variable-shift memory ADD/SUB (`6fxx` with operation-extension masks
`0c00/0c20`, SPRU172C extended Smem arithmetic) retains ordered address
and operation extensions. Access owns DAGEN; Read samples memory once.
Execute extends according to SXM, applies the signed five-bit shift without
independent shift flags, and adds/subtracts into the selected destination
using the live source accumulator and the shared carry/OVM/overflow helper.
The 245,760 replay cases cover ADD/SUB with all four source/destination
pairs, all 32 shifts, SXM/OVM and four memory/guarded-accumulator patterns.
RAM cases cover DP, CPL, postincrement, long offset/no-update,
long preincrement and absolute forms with AR1/AR6; mapped cases cover all
eight AR registers via absolute addresses. Changed pointers and poisoned
extensions test address retention; changed memory after Read tests sampling;
changed accumulators between Read and Execute test live-source ownership.
Decode/Access/Read snapshots check no duplicate reads or data writes.
Other allowed addressing modes and mapped accumulator/T/TRN sources are
not covered by this matrix; producer interlocks, physical bank timing,
repeat bodies and control-delay slots remain unvalidated/restricted.

Variable-shift STL/STH (`6fxx` with `0c80/0d80` or `0c60/0d60`
operation extension, SPRU172C syntax 4 classes 11A/11B) retain address
then operation extensions. Read presents the destination and updates its
pointer once; Execute uses the live source accumulator, applies the signed
five-bit shift, applies SST with SXM, and writes the selected low/high word.
There is no destination read or arithmetic-status update. This follows the
existing store frontend's partial Read-EAB/Execute-EB placement, not measured
silicon operand timing. The 172,032 replay cases cover seven long-offset/
absolute forms (AR1/AR6, offset/no-update, preincrement, circular and absolute),
four offsets, both sources, both word halves, all 32 shifts, SST/SXM and
four guarded/sign-boundary accumulator patterns. Extension poisoning,
changed pointers after Read and changed source before Execute distinguish
address retention from live-source sampling; Decode/Read/Execute snapshots
check one write and no repeated pointer update. Short forms are admitted
but not covered by this matrix; mapped/peripheral destinations, repeat
bodies and control-delay slots remain rejected.

Variable-shift memory LD (`6fxx` plus `0c40/0d40` operation extension,
SPRU172C LD syntax 4) retains the address-extension word, when present,
before the operation extension. Access owns DAGEN and Read samples memory
once; Execute applies SXM and the signed five-bit shift through the shared
shifted-load helper. Its 104,448 replay cases cover 17 read-capable RAM address forms,
AR1/AR6, both accumulators, four values, all 32 shifts and SXM/OVM states.
Separate Decode/Access/Read checkpoints, poisoned extension words and
changed sources/pointers check retention and single publication. The 16-bit
source and shift range `-16..15` cannot exceed signed 32-bit range on a
left shift, so these fixtures check preservation of existing overflow flags
rather than claiming an unreachable new overflow. Mapped `0008..0017`
sources are permitted but are not covered by this matrix. The shifted
store and arithmetic extensions have separate coverage; other `6fxx` operation extensions,
repeat bodies and control-delay slots remain rejected.
The stage assignment remains a partial frontend model, not measured timing.

Memory LD shifted by 16 (`44xx/45xx`, SPRU172C LD syntax 3) shares
the retained memory-load address and Read path. Execute sign-extends
according to SXM, shifts by 16 and masks to 40 bits, without changing
OVdst, carry or other status flags. Its 54,528 replay cases cover 17 RAM
address forms, all AR selectors, both accumulators, eight memory values,
SXM/OVM combinations, compatibility addressing and mapped AR sources.
Snapshots at Decode, Access and Read check one sample, one address update
and no data writes; source/literal/pointer changes after their sampling
boundaries must not alter the result. Mapped accumulator/T/TRN sources
are permitted but are not covered by that matrix. Control/peripheral
sources below `0060` outside `0008..0017` remain rejected. The stage
assignment is a partial frontend model, not measured silicon timing;
repeat and delayed-control bodies remain restricted separately.

Immediate MPY (`f066/f166`, SPRU172C MPY syntax 4, class 2) retains one
literal in Decode and signed T in Read before older Execute. Execute forms
the signed 16x16 product, applies FRCT and publishes through the shared
destination overflow/OVM helper; it does not rewrite T or access data memory.
The 86,016 replay cases include 12,288 isolated operations with 64 value
pairs, both destinations, FRCT/OVM/SXM/SMUL and two program pages.
The 73,728 producer cases add both STLM sources/destinations, SST and
`0..2` intervening NOPs. Gap-zero STLM consumers remain an out-of-contract
stage projection; the defined spacing cases are distinguished from them.
Opcode-shaped literals are poisoned after Decode and T changes after Read;
complete-Decode, Access and Read snapshots check one literal fetch and no
data accesses except the optional producer's single register write. Source
and destination differ independently in the producer matrix. SXM does not
reinterpret the multiply's signed operands; SMUL's pre-MAC/MAS saturation
does not apply to this standalone multiply. Physical bank/interlock timing,
repeat bodies and delayed-control slots remain unvalidated/restricted.

Direct STLM now admits T/TRN (`880e/890e/880f/890f`) in addition to
the covered AR destinations. It ignores DP/CPL, publishes the saturated low
source word in Execute and does not read its destination. SST handling is
supported by SPRU131G's saturation-on-store list, which explicitly includes
STLM; the instruction's unchanged status flags do not imply SST is ignored.
T latency depends on the consumer: SPRU131G 7.5.6/table 7-13 distinguishes
multiply from Category-I TS consumers. The multiply path retains T in Read
after older Execute but before younger Read publications. A zero-gap
STLM-to-multiply therefore sees the published value without a separate
store-forwarding mechanism; this does not relax spacing for TS consumers.
The 12,288 replay cases cover A/B sources, eight guarded/boundary values,
SST/SXM/OVM/FRCT, gaps `0..3`, MPY/MPYR/MPYU T consumers and LDM TRN
consumers. Source changes after Read test Execute-live STLM sampling;
changed T/TRN and RAM after consumer Read test retained operands.
Decode/Read/consumer snapshots require one register write, one consumer
read, unchanged source and no replay duplication. Gap-zero TRN behavior
remains a stage projection, not a measured silicon guarantee. Spaced consumers
have distinct acceptance coverage; other non-DAGEN MMR destinations and
indirect STLM forms remain outside this extension.
The focused `stlm-t` runner uses the separate `tms54xtra` fixture namespace
because `tms54test` has exhausted its eight-bit BIOS index. It also checks
3,072 STLM BRC/LDM cases and restores; neither bank substitutes for an
interrupt or block-repeat composition test.

Memory LD T (`30xx`, SPRU172C LD syntax 1 at 4-70) publishes one
retained Smem sample in Read, after older Execute. Retirement advances the
instruction but never rewrites T. Signed MPY/MPYR (`20xx..23xx`) retain
Smem in Read and T after older Execute; Execute multiplies signed 16-bit inputs, applies FRCT,
rounds MPYR by adding `8000` and clearing the low word, then applies the
existing destination OVM/overflow handling. SXM does not reinterpret their
signed operands. Each family has 52,224 RAM/mapped-source replay cases;
LD T preserves both accumulators and all arithmetic flags. Changed T after
Read distinguishes retained multiply operands from live Execute inputs.
The additional 768 adjacent `30e1 0004; 20e1/21e1 0003` fixtures cover
eight T values, four memory values, both destinations and FRCT/OVM across
three restoration points. They require loader Read at clock 6, consumer
Read at 8 and result publication at 9, with no resampling or data writes.
These are explicit partial frontend clocks, not measured silicon interlocks;
repeat and delayed-control bodies remain restricted separately.

Unsigned memory MPYU (`24xx/25xx`, SPRU172C 4-106) retains one
Smem sample and its unsigned T input at Read, before older Execute work.
Execute multiplies the retained 16-bit unsigned operands, applies FRCT,
then the existing OVM/result-overflow behavior; it does not publish T.
The partial Read ownership is not measured silicon interlock timing.
Its 52,224 replay cases cover both accumulators, T=`8000/ffff`, eight
memory/source patterns, 16 flag combinations, nine RAM address forms and
all sixteen mapped register sources `0008..0017`. Decode/Access/Read
snapshots, changed T after Read, poisoned literals and changed RAM/pointers
check no resampling or duplicate pointer update. PMST.SMUL does not
saturate this unsigned product before destination publication; repeat and
control-delay forms remain separately restricted.

Memory MPYA/MASA/MACA/MACAR (`31xx/33xx/35xx/37xx`) retain one Smem
sample through Read and use a shared operand-only arithmetic helper at
Execute. The multiplier uses signed A bits 32--16 as a 17-bit value and
a signed 16-bit memory operand, independently of SXM. A's lower word and
bits above bit 32 do not participate. FRCT doubles the product. MPYA
overwrites B; MASA subtracts from B; MACA adds to B; MACAR adds and rounds
by adding `8000` then clearing the low word. The existing product/result
SMUL/OVM policies are shared with the atomic decoder, not newly inferred.
A and carry remain unchanged; result overflow affects sticky OVB.

TI SPRU172C specifies memory-to-T publication in Read for these forms.
The staged path publishes retained memory to T after older Execute in
that clock and does not write T again at retirement. B and its flags
publish in Execute. The 52,224 replay cases cover 27,648 RAM cases over
nine read-capable address forms and 24,576 mapped-register cases over AL/AH/AG, BL/BH/BG,
T/TRN and AR0--AR7. They cover all four operations, eight independent
input patterns and FRCT/OVM/SMUL/SXM combinations. Saves in Decode,
Access or Read require one source sample, no data writes and no literal
refetch. RAM/literals/pointers are changed after their owning phases;
T is deliberately changed after Read to prove Execute cannot republish it.

Short Read/T publication is clock 5 and B publication clock 6; extended
forms add one clock in this partial model. A/B/flag sampling, operand-bank
arbitration and older/younger producer interactions are not independently
silicon-validated. The RAM matrix uses AR1/AR6 with BK=16 and standard
addressing; broader modifiers/compatibility and status/producer conflicts
remain outside it. Other control/peripheral sources, repeat bodies and
control-delay composition fail closed. Opcode coverage is not native
startup or speech acceptance.

Register ADD/SUB admits the one-word encoded-shift forms (`f400/f420`
families) and ASM-selected forms (`f480/f481` with source/destination
selectors). These known register-only operations use the existing atomic
arithmetic semantics at staged Execute; they fetch no operand and perform
no control redirect. Unknown families still fail closed in Decode.
Register SUB computes destination minus shifted source, unlike immediate
SUB's source-minus-literal form. Aliasing uses the original source and
destination values. Right shifts fill from bit 39 under SXM and otherwise
zero-fill; left shifts discard bits above the 40-bit accumulator. Carry
is computed before OVM saturation and destination overflow is sticky.
The unselected accumulator and other status fields are preserved.

The 196608 standalone replay cases cover ADD/SUB, all four A/B selections,
every signed shift -16--15, both shift sources, eight independent A/B
patterns, eight SXM/OVM/sticky-overflow states and two pages. A bit-level
shift oracle is independent of the CPU helper. Encoded-shift fixtures set
ASM to an unrelated -13, while ASM fixtures use its signed five-bit value.
Saves during Decode, Access or Read require one opcode fetch, no data
access and publication only at Execute, clock 6 in this partial model.
Another 288 producer/replay cases compose a signed immediate load and
register ADD/SUB through zero, one or two NOPs, checking aliasing, retained
load literals and drainage to IDLE. Load publishes at clock 7 and register
arithmetic at `8 + gap`. This is not silicon phase evidence; mixed status
producers, wider operand conflicts, repeat and delayed-control/interrupt
composition remain unvalidated.

Shifted register LD (`f440` family and ASM-selected `f482`, with source/
destination selectors) uses the known one-word register-only Execute path.
It overwrites the destination with the shifted original source rather than
combining the two accumulators. Right shifts sign-fill from bit 39 under
SXM and otherwise zero-fill, without adding overflow or applying OVM.
Nonnegative shifts, including zero, check the full signed source/shift
against the signed-32 range before 40-bit truncation, set sticky destination
overflow and saturate under OVM. Thus `LD A,0,B` may saturate an out-of-range
guard value; it is not an unconditional raw copy. Carry is preserved.

The 196608 standalone replay cases cover all signed shifts -16--15, four
A/B selections, both shift sources, eight source/destination patterns,
sixteen SXM/OVM/carry/sticky-overflow states, two pages and Decode/Access/Read
saves. A bit-level shift oracle and signed full-width overflow calculation
are independent of the CPU helper. Another 144 signed-load producer/replay
cases cover zero/one/two NOP gaps and drainage to IDLE. The shared register-ALU
fixture still runs the separate ADD/SUB matrix with its different flag
contract. LD publishes at clock 6 standalone and `8 + gap` after the load
producer in this partial model. No data-memory access, extension fetch or
duplicate opcode is permitted. Mixed status producers, silicon source-read
phases, wider operand conflicts and repeat/control/interrupt composition
remain unvalidated.

The isolated core runner permits five emulated seconds as a finite stop
bound for the full matrix. Reaching that bound is not acceptance: all
required markers and the intentional test-completion exit are mandatory,
and a normal exit is rejected even if it contains PASS markers.

Accumulator AND/OR/XOR register forms now retire in Execute using the
existing one-word arithmetic semantics. They read no memory, consume no
extension and change no flags. SPRU172C 4-11/4-123/4-201 specifies a logical
zero-filled shift over the 40-bit accumulator, independent of SXM, before
combining with the selected destination. Source/destination aliasing uses
the original value of that instruction's source.

The 6144 save/replay cases cover all three operations, four A/B selections,
all 32 encoded shifts (-16 through 15), four independently chosen operand
pairs, both SXM states and Decode/Read checkpoints. A bit-by-bit oracle
checks the result and unchanged status each clock, with one opcode fetch
and no data access. Original AND A,0,B (`f180`) at `00:2fed` is accepted;
CMPL A,A (`f493`) at `00:2fef` is also accepted. This is opcode
coverage, not native startup or speech acceptance. Control-delay and
interrupt composition for these logical forms are not separately validated.

CMPL uses the same one-word Execute path with no memory access or flags.
SPRU172C 4-32 specifies the full 40-bit ones complement of the selected
source into the selected destination. Its 64 replay cases cover four A/B
selections, four operand pairs, both SXM states and Decode/Read checkpoints;
the bitwise oracle checks the result and preserved status every clock.
The original upload passes CMPL without establishing native startup completion.

MVKD now retains its immediate data-source address and separately owned
Smem destination. Short DP/CPL, plain/postincrement/postdecrement/
preincrement indirect and absolute destinations are supported. Absolute
Decode consumes destination then source, not PORTW's source then port.
Source and destination are RAM or AR0-AR7; other control/peripheral operands,
long offsets, repeated MVKD and control-delay placement remain unsupported.

Read samples the source once, after Access's encoded pointer modification.
Thus a literal AR source which also addresses the destination must expose
the modified AR, not a stale sample. RAM publishes in the complete slot's
Execute. AR publication belongs to the opcode's Execute (the complete
two-word slot's Read), satisfying SPRU131G table 7-7's one-gap Category-I
latency. A first implementation using the RAM publication clock failed the
CMPR one-gap consumer test and was corrected; this is a CPU conformance
constraint, not a firmware-timing workaround. Precise bank/bus placement
remains partial, with no claim of physical silicon measurement.

The 672 replay cases cover seven destination forms, AR1/RAM/same-address/
AR2 sources, four values, two pages and three checkpoints. They poison
literal/CPL after Decode, SP/DP after Access and source/pointer after Read,
while retaining one read and one write. AR consumers with zero and one
intervening instruction verify the old/new value boundary. Sources which
alias their pointer and destinations which alias their source are explicit
fixtures. Original compiler MVKD `7001` at `00:30b5` now runs; the upload
passes SFTA A,-1,A (`f47f`) at `00:323f`. Native startup, recorder acceptance
and speech remain unproven; no atomic profile is promoted.

SFTA/SFTL now use the admitted one-word Execute path without data access.
The staged fixture has 32768 save/replay cases: both operations, all four
A/B selections, all encoded shifts (-16 through 15), eight source patterns,
both SXM/OVM and initial carry states, and Decode/Read checkpoints. A
bit-by-bit shift oracle plus signed-range checks validates result, carry,
sticky overflow and saturation only at Execute, with one opcode fetch.
SFTA right shifts sign-fill from bit 39 when SXM is set; SFTL operates on
bits 31-0, clears guard bits and explicitly clears carry for shift zero.

SFTA left carry is not a closed hardware contract: SPRU172C 4-155 specifies
source bit `39-SHIFT`, but 4-156 example 2 (`80aa001234 << 5`) prints C=1
where that formula gives zero. Its zero-shift carry is also not explicit in
the descriptive cases. The existing formula/preserve-zero policy is kept
and named in the fixture marker; passing it is conformance to that policy,
not proof against silicon. A bounded TI-document search found the same
manuals, not a resolving correction. Independent simulator/hardware
evidence is still required before changing this shared arithmetic helper.
This ambiguity does not affect the upload's negative-shift instruction.

The original upload passes long-offset DSUB `54e6` at `00:3244`.
General shift interrupt/control-delay composition remains unvalidated;
opcode coverage does not establish native startup or recorder acceptance.

DSUB now retains its long-word data operand before Execute. Addressing
reuses the extended DST path for ordinary offset, linear/circular
preincrement and absolute forms; short indirect modifications use a
two-word step. Complete-Decode CPL/DP and Access SP own direct operands.
Read samples MSW at the retained address and LSW at `address ^ 1` in the
same clock, including odd-address ordering (SPRU131G 5.6). Execute calls a
shared, operand-only long-arithmetic helper; atomic DADD/DSUB/DRSUB and
T variants use that same calculation without changing their bus paths.

The 6,144 replay cases cover eight read-capable address forms, A/B destinations, eight
operand pairs (including TI's C16 example), both C16/SXM/OVM states,
both parities and Decode/Read/Execute checkpoints. They verify one paired
read, no memory write, retained values after RAM/literal/AR/SP changes,
32-bit borrow/overflow/saturation and separate C16 lanes. The existing
dual-mode sign-extension and overflow-retention policies remain modeled,
not independently silicon-validated. Exact bank arbitration, peripheral
pairs, repeats and control-delay composition are not claimed; the last
three remain unsupported. Original `54e6` now executes and the upload
passes long-offset immediate ST `76e6` at `00:322e`. No native startup,
recorder or speech acceptance follows from this opcode boundary.

DADD (`5000`--`5300` families) shares that retained-pair/address path,
but bit 9 independently selects its source accumulator and bit 8 its
destination. Execute reads the selected original source and adds the
captured memory pair; it does not use an unrelated destination value.
The 12,288 replay cases cover all four A/B selections, eight read-capable address
forms, eight source/memory pairs, C16/SXM/OVM and both parities. Direct
offsets 4/5 include exact `5304` under compiler-relative addressing.
Fixtures change AR/SP after Access, literals after Decode and RAM after
Read, then restore in Decode, Read or Execute; each leg requires one
ordered pair, no memory writes and unchanged unselected accumulator.

Short MOD=3 is omitted from these read fixtures with raw IDs preserved;
SPRU131G table 5-4 restricts it to writes. Atomic paired arithmetic and the
staged DADD/DSUB/DRSUB classifier reject it before operand effects. The
`long-pair` and `drsub` gates additionally require 3,584 rejection/replay
cases across all 14 paired-read opcode classes, all ARs, CMPT/C16 settings,
pages, frontends and two snapshots, with no data or pointer effects.
This adds no positive staged T-variant support; paired DST writes and long
MOD=13 reads retain their separate domains.

Ordinary mode extends the memory high word under SXM, computes full
40-bit addition, low-32 carry, sticky destination overflow and OVM
saturation. The modeled C16 path adds independent low/high 16-bit lanes,
does not propagate low carry into high, extends the high result into
guard bits and derives carry from the high lane. It retains overflow
and does not clamp the independent lanes under OVM, as in the existing
long-arithmetic model. Those dual-mode flag policies and paired Read
phase remain not independently silicon-validated. The matrix uses encoded
AR1/AR6 and BK=16; other modifiers/compatibility ARP, producer/status
conflicts and physical bank arbitration remain outside it. Register/
peripheral pairs and control-delay placement fail closed.

Long-offset immediate ST now consumes offset then value as a retained
three-word operation. Its Read-owned destination calculation shares the
same offset/preincrement/circular helper as atomic addressing and staged
DST/DSUB, with no operand fetch at address generation or Execute. Execute
writes the cached value once. Register/peripheral destinations for these
new forms, repeats and control-delay placement remain unsupported; existing
short and absolute immediate-ST paths retain their behavior.

The 6144 replay cases cover three offset modes, eight encoded registers,
four offsets, four values, both compatibility states, two pages and saves
after partial Decode, complete Decode, address generation or publication.
They poison each extension after capture and change AR/BK/DP after address
generation, verifying no destination read, no refetch and no repeated write.
Opcode-shaped offsets/values are data, not independently decoded opcodes.

Control-delay rejection is likewise restricted to unowned instruction words,
not extensions of an already decoded instruction. The call matrix now has
132 replay cases with nine STM-T parameters, including `76e6`, `4ee6`,
`7001` and `54e6`, inside valid CALLD delay slots. The unchanged call/stack
and retained-parameter results protect that ownership rule while actual
unsupported instructions still fail closed. Original startup passes
BIO-conditioned BC `f803` at `00:2fca`; a new opcode boundary alone proves
neither native startup nor physical timing.

Staged BC/BCD admit BIO and NBIO via the CPU's existing board-supplied
line callback. BIO is low-active and NBIO high-active (SPRU172C condition
codes); SPRU131G 8.3.1 places branch sampling in Read. Both forms sample
once in the retained operation's Read, before Execute, and store the outcome
across branch/delay retirement. Delayed accumulator predicates retain their
separate existing Evaluate path; BIO is not sampled through that path again.
The retained-slot clock alignment is still a partial model, not measured
silicon phase placement.

The 256 replay cases cover plain/delayed forms, both conditions, initial
low/high levels, static/change-after-Decode/change-before-Read/change-after-
Read inputs, two pages and four checkpoints. They verify one line callback,
one literal fetch, the selected target and real execution/suppression of
delay markers, with unchanged status. Fixtures explicitly wake between
IDLE endpoints and restore their external input observation state; no
handset firmware state is changed.

MU4 already connects NAND R/B to the bench's `m_ready` and `bio_in_cb`,
backed by the recovered schematic. The original staged upload now consumes
that live model output without forcing it and passes `f803` and
MVMM SP,AR5 (`e785`) at `00:2fbe`. Interrupt/held-peripheral composition of
these branches remains unvalidated. This is bounded startup coverage, not
native recorder acceptance or a physical-board proof.

MVMM admits exactly AR0-AR7/SP source/destination encodings. SPRU172C
4-116 explicitly places the source read in Decode and destination write
in Access. The staged implementation retains the source once and orders
Access publication before a younger Decode sample, after older publications.
This preserves immediate destination visibility and the older producer's
required source latency. Execute does not reread or rewrite the registers.
SP writes use the existing compiler-addressing visibility window rather
than delaying the architectural SP update.

The 1944 all-pairs replay cases cover all 81 register pairs, four values,
two pages and Decode/Access/consumer checkpoints. They change the source
after Decode and verify one read/write plus zero-gap AR consumption.
Another 24 cases verify normal DP and compiler SP consumers with zero/one
intervening NOP. The 48 producer-chain cases verify STM's zero latency and
STLM's two-gap Category-I source latency, including simultaneous older
Execute publication and MVMM Decode sampling. All preserve the retained
sample through replay. Wider stack/interrupt and register-bank contention
compositions remain unvalidated; repeat MVMM remains unsupported.

The original upload passes short RPT #32 (`ec20`) at `00:2fc1`. An opcode
boundary alone remains insufficient to prove older I/O retirement, native
startup acceptance or speech validation.

Short immediate RPT now loads its encoded eight-bit count at Execute and
activates interrupt protection in Decode. NOP is a supported retained body
for short/long RPT: the first instruction takes its normal frontend path,
then each stream clock retires one further NOP and decrements RC without
program/data refetch or pointer modification. Count zero still executes
the body once. The existing saved counter is exposed as debugger state
`RC`; no new repeat-runtime state or firmware intervention is introduced.

The 240 replay cases cover both immediate forms, counts 0/1/3/32/255,
no interrupt/IRQ/NMI, two program pages and four checkpoints. They check
RC every clock, poison the already fetched body and speculative
continuation, and verify one body fetch, fresh continuation, deferred
acknowledgement and balanced interrupt return. With OVLY clear, even the
NMI vector's low slot inherits XPC, so each fixture's ISR is placed in its
active page. Input edges are queued before the Decode checkpoint; the
maskable fixture asserts that IFR already owns the event before saving.
Saving an undelivered MAME input synchronization request is not a test of
saved CPU interrupt state. Short-repeat transfer-body composition and wider
peripheral/HOLD interactions remain unvalidated; other bodies fail closed.

Staged BIT admits the `96xx` Xmem/BITC family (SPRU172C 4-21).
Access captures the selected AR2-AR5 address and applies its packed pointer
modifier once; Read retains one RAM word; Execute copies bit `15-BITC`
into ST0.TC without changing other status or either accumulator. Sources
below `0060` fail closed before pointer modification; peripheral wait and
control-register composition are not enabled by the RAM tests.

The 9,216 replay cases cover all 16 packed Xmem fields, all 16 bit codes,
both selected-bit values and initial TC states, zero/one/two-gap XC consumers,
and Decode/Access/Read checkpoints. Poisoning RAM after Read must not
change the result. XC samples before Execute, so zero/one gaps observe old
TC while two gaps observe the BIT result. The original `9634` at `00:2fc3`
uses unmodified AR5 and tests bit 11; its two NOPs before `fd20` match the
tested window. Percent addressing is tested with BK=0 and AR0=3; nonzero
BK, page variation, repeat bodies and wider producer conflicts remain
unvalidated. These are staged-model contracts, not measured silicon phases.

The staged-only, first-16-strobes observer records five original-upload
NAND writes: command `00` with HD012=`1`, then address bytes `00,01,00,00`
with HD012=`2`, direction=`7`, ready=`1`. CLE/ALE distinguish the cycles;
CE is low. No device state or response is synthesized by that observer.
Staged PORTR retains its port literal and, for `74f8`, the preceding
absolute destination literal. Basic direct destinations retain Decode
CPL/DP and Access SP through the existing consumer-latency machinery;
short indirect destinations retain the address and postmodify in Access.
Read samples the external I/O space once; Execute writes that retained
word to data space. Only RAM and AL/AH/AG/BL/BH/BG/T/TRN destinations
are accepted. Control, DAGEN and peripheral destinations fail closed before
the I/O read; long-offset forms, control-delay composition, repeat bodies
and external READY/HOLD wait behavior remain unvalidated. This is a partial
Class-27 stage model, not measured silicon bus timing.

The 416 replay cases cover those eight register destinations and absolute
RAM, DP, CPL, AR postincrement and postdecrement forms; four values, two
program pages and four checkpoints cover partial Decode through publication.
Literal poisoning after complete Decode and port changes after Read must
not retarget or resample the operation. Tests assert no data-destination
read, one I/O read, one data write, unchanged status/unselected accumulator
lanes and no duplicate pointer update after restoration.

The original upload now retires PORTR `74f8` at `00:302f`. The bounded
NAND read observer captures the first 512 bytes with HD012=`0`, direction=`7`
and ready=`1`. They compare byte-for-byte with retained NAND row 1
at raw offset `528`; the prefix is `fa33c08ed0bc007c8bf45007501ffbfc` and
the full captured page SHA-256 is
`7876777bc75f8e33789d0b917ac2ca71debc977170e6bc6b0f2dcf9b2e2c9310`.
The bounded observer also sees two further commands `00` with address
bytes `00,20,00,00`, selecting row 32.
Only the first 512 bytes are captured and verified. The first-page
comparison validates device-read data,
not the resulting firmware buffer, spare-area handling or native startup.
The observer returns the single existing device read, never a second
read or a synthesized reply. STL A `80e6` at `00:3269` and long-offset
MVDK `71e6` at `00:326b` are now accepted by the staged decoder.
RPTB `f072` at `00:32ee` now executes.
Same-address RMW forwarding now lets the unchanged directory builder
construct `MCUSI16 .BIN` with descriptor length `0001:e1e8`, matching the
atomic reference. NEG `f484` at `00:0e35` now has 13,824 staged replay
cases, covering all source/destination selectors, carry, sticky overflow,
OVM saturation, 32/40-bit boundary patterns and immediate-load producers
without data-memory accesses or frontend fallback (SPRU172C 4-119/4-120).
The pre-correction one-second observation enters loader `3538` with descriptor
zero after 4,612 NAND reads and reads block count `0010`. Its mapped AL
subtraction (`09f8 0008` at `00:2f46`) and mapped AL BITF
(`61f8 0008 0001` at `00:2fa5`) now execute. The preceding load reads
DMPREC; BITF itself reads AL, not a DMA peripheral. Mapped BITF has
13,824 replay cases for the eight accumulator/T/TRN words, four masks,
two initial TC states, direct/absolute forms and AL producers, with one
retained source read and no data writes. Only TC changes in Execute;
zero mask clears it (SPRU172C 4-28/4-29).
That pre-correction observation reached resident `02:6d62`, stopping at
`STM #lk,IMR` (`7700` at `02:6d64`). Its read-only program comparison
found 40,657 mismatches among 48,261 words: first `02:8e8b`,
actual `826c`, expected `0001`. It excludes 3,660 data words from the
51,921-word source and uses the configured common-window mapping without
claiming physical bank identity. Correct resident transfer is unproven;
a fresh atomic `bootrecsave` control with the same NAND/shared-memory
profile matches every compared program word. The first staged divergence
is record 434, word 11, file offset `4000` (16 KiB): its first five wrong
words match byte-swapped source bytes at `3fda`, one 38-byte record earlier.
The crossing record gets only 11 fresh buffer words before the original
16-word swap/DMA, leaving five stale words; DMA still reads the original
buffer. Atomic advances both descriptor cluster fields from 3 to 4;
staged resets the cursor without advancing either field. Their initial
cluster comparison reads equal values, but next-cluster helper `343a`
returns `0fff` staged versus 4 atomic. Offset helper `345c` has an earlier
fetch-snapshot discrepancy at `3482` (A=0 staged versus 2 atomic).
Snapshots do not establish retired operand values. A subsequent actual
mapped read observation establishes AL=3 staged versus AL=9 atomic at
`LDM AL,A` (`347b`), immediately following two-word `MPY #3,A`
(`3479..347a`). `SFTA A,-2,A` then gives offset 0 instead of 2.
SPRU131G table 7-28 requires no gap for this two-word producer/consumer
composition, unlike a one-word producer with a short mapped consumer.
Matching mapped accumulator reads now finish after the older two-word
immediate MPY Execute without changing other reads or one-word latency.
The isolated `mpy-mapped` fixture passes 3,072 saved-state replays over
six accumulator words, both source/destination accumulators, signed
patterns, FRCT/OVM and zero/one-word gaps, including mutations after T
and mapped-word capture. The unmodified CPU fails the same fixture.
Long-offset `MVKD` (`70e1 0016 0012`, `00:33aa`) now retains offset,
source, destination and pointer modification through the existing move
path, publishing the recovered cluster value into `36c6` organically.
Its 19,200-case isolated replay fixture covers 25 destination forms,
four offsets, selected-AR/AR2/RAM/alias sources, four values, both
compatibility states and two program pages, with one read, one write and
two ordered extension fetches. Snapshots cover partial Decode, retained
Read and completed write (SPRU172C 4-113).
The full C54x conformance runner and native 5110 save-state gate pass.
The current original bootstrap reaches resident `02:6d62`; all 48,261
compared program words match, including the former 16-KiB crossing.
A separate side-effect-free data audit now checks all
3,656 ordinary data words with zero mismatches in both staged and atomic
controls. It preserves later common-RAM program/data alias writes in
record order for the configured profile. The four low register-window
writes at `007c..007f` remain excluded; their live values cannot establish
write effects. `noki5510_bootstrap_check.py --resident-memory LOG` requires
both complete audit reports with exact coverage and no mismatches or
duplicate evidence. Original program/data RAM contents are verified;
physical bank identity, register effects and full boot are not.
The immediate IMR clear (`7700 0000`, `02:6d64`) now publishes through
the retained non-DAGEN Execute path. Its 4,608-case replay fixture covers
16 IRQ bits, four masks, both initial masks/INTM states, no source/IRQ/NMI
and two pages, with one literal fetch/write and no destination read.
Acceptance before a clear is preserved; a newly enabled pending source
is accepted only after publication. Bus-phase timing is a partial model
(SPRU172C 4-176/class 12A; SPRU131G 6.10.2).
The full C54x conformance run, native 5110 save-state gate and original
resident-memory checker pass; fixture IRQ/NMI inputs return low on exit.
The MU4 sequence includes `STM #7fff,*(0028)` (`7728 7fff`, `02:6d05`).
SWWSR at `0028` and the following BSCR write (`7729 8006`, `02:6d07`)
configure external wait states/bank switching (SPRU131G 10.3.1/10.3.2).
Storage alone must not be described as faithful wait-state modeling. No
filesystem or cluster state is synthesized; full boot remains unaccepted.
The [MU4 external-bus contract](5510_bringup.md#external-bus-compatibility-contract)
pins these original requests using SPRS095P's C5416-compatible dialect:
BSCR's upper fields differ from the older generic family table. Its field
decoder and retained I/O paths are tested as described below, but physical
DA150 identity, program/data bank ownership and physical bus phases remain
unvalidated. This compatibility profile is distinct from the optional
standard SWWSR model used by the NSE-1 staged execution probe.
The isolated C5416-compatible register profile now validates reset,
masking, partial writes and saved-state/output restoration across all
65,536 encodings. Its CLKOUT divider and IACK-enable callbacks are
configuration outputs, not pin waveforms; logical IRQ acknowledgement
remains independent. No handset selects this compatibility profile.
The isolated staged I/O profile now validates software waits `0..14`,
READY eligibility after at least two software waits, single read/write
side effects and saved transaction ownership across 576 replays. Timers
continue and IRQ/NMI acknowledgement follows the transfer. Whole-pipeline
stall placement is partial; simultaneous I/O arbitration is fail-closed.
The atomic frontend now retains waited I/O and retirement context.
Its separate gate passes 2,880 saved/replayed cases across short,
long-offset, absolute, peripheral-MMR and AR-alias addressing. A 24-case composition gate checks
RPT counts 0..3, delayed branch slots and INTM-protected followers,
including pointer updates and stacked interrupt continuations.
The MMR fixture checks the extra access clock at `0030`; the AR1 alias
checks write-before-postincrement ordering. Other control-register side
effects and physical bus phases remain outside this bounded acceptance.
Bus-control STM admission has a separate 36-case saved/replayed gate:
one retained literal and one masked Execute write at clock 8, including
the peripheral clock. The profile is also selected by the isolated
`bootrecstage` diagnostic, not a handset composition. Original resident
memory still matches all checked words. Mapped-accumulator READA now has
288 saved/replayed cases for AL/AH/AG and BL/BH/BG, direct/absolute forms
and two source pages, preserving status and untouched accumulator words.
The original `7ef8 000b` transfer at `02:6d12` executes. Absolute LD
Smem,DP has a 1,536-case saved/replayed gate for all 512 DP values,
retained extension/source and zero-gap consumer forwarding. The original
`46f8 007f` at `02:6d2b` also executes. That checkpoint passes full CPU
conformance, native 5110 restoration and the isolated profile gates.
Direct MVDM-to-AR now has 1,152 saved/replayed cases for all eight
registers, RAM/self/other-AR sources and table-7-7 consumer gaps. The
original `7212 b6be` at `02:df9b` executes. MVDM `7211 b6a8` at
`02:dfa6` also executes after a separate 576-case branch-delay gate,
including both outcomes, pending IRQ/NMI and RETE/save/replay. Repeat
remains unqualified in this AR gate; accumulator destinations have the
separate [slice qualification](c54x_execution_contracts.md#mvdm-accumulator-slices). Other non-AR
destinations remain excluded. FCALLD `fb82 dfc2` at
`02:dfaf` has a 243-case saved/replayed gate for caller-fetched delay
words, stacked PC/XPC and IRQ/NMI return. Delay-slot mapped-XPC read
visibility is explicitly unvalidated, not inferred from the frame contract.
The original enters `02:dfc2` and executes absolute MAC Smem,B
`29f8 0011` at `02:dfc9`. The MAC/MACR/MAS/MASR family has a 73,728-case
saved/replayed gate for retained memory/T, both accumulators, rounding,
FRCT/OVM/SMUL/SXM and sticky overflow. MVKD
`7084 0013` at `02:dfd8` now executes as both delay words of BCD at
`02:dfd6`, after a 1,728-case saved/replayed MVKD delay gate. It includes
AR sources, postincrement and same-address RAM read-before-write. Three-
word extended delay forms remain excluded. MVMD `7316 b6a9` at
`02:e078` now executes after a 576-case saved/replayed gate for direct
AR sources, RAM/register destination ownership and unchanged status.
The original returns to the init spine and executes STLM A,IFR `8801`
at `02:6d3f`, immediately following LDM IFR,A. The isolated pair gate
passes 96 cases/restores: sampled flags, late physical IRQ, selected IRQ
whose flag is cleared, independent NMI, and RETE stack restoration. The
absolute CMPM `60f8 3762 000c` at `02:909c` now executes after a 96-case
retained-extension/source and adjacent TC/NTC branch replay gate.
Mapped-register CMPM is separately qualified for `0008..0017` above;
the shared mapped-BITF gate also passes.
MVKD `7082 0010` at `02:8fbd` now copies AR0 through AR2 to McBSP's
mapped control-data register `0049`, after a 648-case peripheral
wait/publication/replay gate. The extra peripheral clock precedes a
single Execute-owned write; DAGEN destinations retain their earlier
publication path. BK sources now share retained Read ownership and
post-capture-change replay; adjacent BK-producer latency remains
unvalidated. T/TRN peripheral stores now pass an 864-case STLM-producer
and peripheral-publication replay gate. Peripheral BITF now passes
384 status-read/TC-branch replays with its extra access clock. Original
`61e1 0001 0002` at `02:9521` polls the McBSP register-1 transmit-ready
bit through `0049`, following index selection through `0048`. The
original emits six native serial words and executes FCALA B `f5e7`
at `02:40ef`. The nondelayed staged prototype
now passes 216 saved/replayed page/frame round trips with a full-width
retained accumulator target and two separately owned stack writes.
IRQ/NMI cannot split the call frame; RETE resumes one callee execution
and FRET restores the caller. A separate 216-case DLD-producer gate
validates target publication with gaps 0/1/2 and retained RAM inputs.
Other producer families and the producer-plus-interrupt cross-product
remain unvalidated. Nondelayed CC AEQ/BEQ now passes 64 replay cases
with retained predicates/literals, conditional PC+2 frame publication
and zero untaken stack accesses. Original `f945 3ff1` executes; the
conditional returns now cover all twelve signed A/B predicates in 768
predicate/pop/target replay cases (eight 40-bit patterns, two pages and four
snapshots). Untaken returns have zero stack
accesses. SUBS passes 52,224 unsigned-source/address/arithmetic
replays across 18 forms, A/B and SXM/OVM states; mapped sources and C16
remain excluded. The pre-status checkpoint stopped at MVMD AL `7308 0007` at
`02:3f62`, live PC `02:3f65`, immediately after POPM AL. AL/BL-to-RAM
now passes 288 POPM/mapped-STL producer replays, with mapped-write
no-conflict ordering and retained source publication. The staged ST1 subset
passes 288 transfer/replay cases and 16 basic CPL consumer/replay cases;
the original passed `3f83` at `02:3df3` after a separate 208,896-case
memory ADD/SUB shift-16 gate. That new RAM-only subset covers all eight
operation/source/destination selectors and the syntax-3 partial carry rule;
its full checkpoint regression passes. The original's LDU
`1282` at `02:3dfa` reads actual IMR at `0000` after a separate 384-case
unsigned mapped-load gate, including the adjacent pointer-producing MVDK.
The preincrement STL/STH subset passes 6,144 save/replays; immediate
preincrement ST passes 1,536; STM BRC followed immediately by RPTB passes
30, including count 127. These additions pass the original clearing loop.
Long-offset peripheral ST passes 6,144 save/replays; ADDM/ANDM/ORM passes
368,640, including RAM controls with identical offsets, one peripheral
extra clock total and captured source/destination ownership. These are RAM-
backed CPU MMIO conformance tests, not device-side-effect acceptance.
The original McBSP0 SPSA/SPSD store/mask sequence at `02:97d9..02:97de`
executes through the real device model. A separate 16-case coupled McBSP
gate validates selector/read/reset order, volatile XRDY capture and joint
CPU/device restoration. STLM A/B to IMR passes 9,216 mask/IRQ/NMI replays.
The current original stop is `8a1d` (POPM PMST) at `02:8b3b`, live `02:8b3d`.
McBSP0 stream words are now observed, but their source/audio meaning is not
validated. Physical DA150 mapping and PMST restoration/fetch ownership
remain open; checkpoint regression for the admitted peripheral/control
subset passes.
The earlier zero address is observed, not proof that the pointer lifecycle is correct.
Adjacent SXM/ASM LD consumers pass 768 save/replay cases; immediately
following RPTB passes 24, including preservation and final clearing of BRAF.
Basic CMPT indirect LD/ST passes 16 cases/restores after replacing live-mode
address selection with saved Decode sampling shared by pointer updates.
The status IRQ/NMI fixture passes 576 save/replay cases with requested mask
pairs, three arrivals and one-/two-word followers. Memory writes cannot set
INTM; zero-write clearing retains the explicitly unverified policy in the
[ST1 access contract](#st1-memory-access-constraints). Generic word writes
do not inherit special INTM-bit-instruction protection. Other CMPT categories, ARP-writer latency and
arbitrary cross-products of consumers/block repeats with IRQ remain unvalidated;
the accepted subset does not establish every status-register interaction.
Delayed FCALAD has separate staged frame, delay and IRQ/NMI coverage;
see the [delayed accumulator-call contract](c54x_execution_contracts.md#delayed-far-accumulator-call).
Serial output does not prove audio or full board boot.
Resident program/data memory still matches exactly. Shared regression and
native 5110 restoration coverage are indexed in the
[core regression checkpoint](c54x_execution_contracts.md#core-regression-checkpoint).
Excluded instruction and producer/interrupt compositions remain unaccepted.
Mapped ADD/SUB has 6,912 replay cases for AL/AH/AG,
BL/BH/BG and T/TRN, direct/absolute forms, destination aliases and retained
Read values. Producer cases distinguish the short-consumer one-word
latency from the long-offset zero-gap case (SPRU131G table 7-28).
Controls, peripherals and mapped SUBC remain excluded. Physical bus
phases and bank contention are not established by these fixtures.
Repeated NEG and measured silicon phases remain unvalidated. The former malformed
names/zero-length loader retry tail is superseded. Board boot, resident
transfer, music decoding and recording remain unaccepted.
The scoped bypass uses retained RMW operands, preserves EB/flag timing and
excludes register/peripheral reads; physical memory-bank contention and
other pending-write producer families remain unvalidated. The mandatory
768-case `rmw-forward` fixture covers adjacent/spaced loads, same/different
addresses, boundary arithmetic, SXM/OVM, two pages, DP/compiler addressing
and save/replay. It does not claim measured DA150 bus phases. See
[the original-bootstrap contract](5510_bringup.md) for the bounded
descriptor and stale-offset data-bus witnesses. No firmware state is forced.
STLM B,BRC at `00:32ec` now has 3,072 staged replay cases covering
Execute publication, saturation, both accumulator sources and retained
LDM reads. The context tool pins the original nine-word sequence:
`891a f495 f072 32f4 1386 f678 f3e8 f1a0 8196`.
Its intervening NOP satisfies SPRU131G table 7-3 category 9's BRC-store
spacing rule; the requested block spans `00:32f0`--`00:32f4`.
Straight-line block execution and the forward near-branch subset have
bounded conformance below; delayed/nested and other control-changing
blocks remain outside it.
Zero-gap LDM cases test the declared pipeline
projection, not silicon forwarding or RPTB latency.

The static setup contract follows SPRU172C's RPTB[D] entry: two instruction
words, RSA=`PC+2` (RPTB) or `PC+4` (RPTBD), REA=the literal last word,
nominal four/two cycles respectively, BRC+1 executions, and interruptible
blocks. RPTBD executes two delay words and is not single-repeatable.
SPRU131G sections 6.8/7.5.8 place BRC decrement at the last block
instruction's Decode stage. The atomic frontend's post-instruction
`REA+1` check is therefore not sufficient evidence for staged timing.
The staged implementation retains endpoint predictions before younger
fall-through work executes, independently of Decode-owned BRC updates.
Interrupt acceptance discards undecoded fetch predictions and reconstitutes
the remaining count from the architectural state at its saved return PC.
Software BRC/BRAF modifications have separate five/six-word latencies
(tables 7-3/7-25), not ordinary Execute forwarding. The context tool's
setup arithmetic tests do not prove these runtime requirements.

The `tms54test -bios rptb` runtime fixture covers 6,144 cases with counts
0/1/2/7, sixteen source patterns, sixteen flag configurations, two program
pages and three save points. The original five-word body has uninterrupted
reads at clocks 9+5i and writes at 14+5i, with no loop padding. Twenty-four
IRQ/NMI cases save before/through interrupt acceptance, use the existing
same-page vector contract and return through RETE. Sixteen further replay
cases cover one-word MAR and two-word LD endpoints and twelve fail-closed
cases: nested RPTBD, a backward branch occupying the endpoint, STLM BRC, endpoints inside opcode
or extension words, and an aliased STL to BRC. All block-prefetch tags,
retained extension tags, active context and speculative count are saved.
Terminal BRC remains zero in this declared model, matching the atomic
frontend; exact silicon terminal-counter and phase behavior are not claimed.
This fixture is not the frontend's complete composition surface. Separate
RPTBD, branch/call, XC and context-restoration scopes are maintained in
[the retained execution contracts](c54x_execution_contracts.md#retained-recorder-execution-contracts).
Nested block repeats, IDLE inside a block, general in-loop register-modification
latencies, wrapped endpoints and measured silicon timing remain outside those
bounded contracts. The forward near B/nondelayed BC cases below establish
their own subset, not an exclusion of separately validated compositions.
These tests do not establish complete DSP startup or speech.

Forward near B/BC may target the same block at or after the sequential
continuation, but neither word of the branch may occupy REA. Taken
redirects rebuild the discarded endpoint prediction from architectural
BRC. Untaken BC restores held block tags, including a held endpoint;
three pending-tag booleans are explicitly saved. The `rptb-branch` profile
passes 12,288 replays over two layouts, counts 0/1/2/7, four data patterns,
both accumulators, two pages, eight snapshots and IRQ/NMI arrivals at
model clocks 9/10/12/14, plus uninterrupted controls. Each iteration has
one source read and selected write, with Decode-owned BRC/BRAF and
balanced ISR return. A later outside branch changes the held tags before
restoration. Thirty-two `rptb-br-edges` cases preserve rejection of
below-block targets, endpoint-overlapping branch words, delayed endpoint
control, call, nested and far-branch compositions without a frame or body
write. Forward internal-branch interrupt composition is covered by these
fixtures; arbitrary control flow, software BRC/BRAF changes and measured
silicon timing are not.

The separate `rptb-exit` profile admits forward nondelayed near B/BC targets
beyond REA, with both branch words before the endpoint. Its 1,152 cases
plus replay cover B/A-equal/B-equal, counts 0/1/2/7, first/middle/last/never
exit conditions, two endpoint layouts (including a store's literal at REA),
two pages and six snapshots. Taken exits annul the remaining body while
preserving BRC and BRAF; final-iteration BRAF clearing still belongs to
Decode, not the branch. Untaken paths retain their endpoint tags and
complete all iterations. An outside RSBX BRAF then retires the private
fetch predictor only when no younger body words are pending. A deliberate
revisit to the old endpoint after six NOPs and a branch checks that the
cleared loop cannot restart or consume BRC. RSA/REA remain unchanged.
This follows SPRU131G 7.5.8.2's software-clear spacing and does not import
C55x automatic clearing on exit. In-loop software-clear latency, pending-body
clear arbitration and interrupt composition of these exits remain outside
this bounded gate; silicon phase timing is not claimed.

The original block has a concrete data oracle: `1386` is
`LDU *AR6,B`; `f678` is `SFTA B,-8,A`; `f3e8` is
`SFTL B,8,B`; `f1a0` is `OR A,0,B`; `8196` is `STL B,*AR6+`.
The unsigned load bounds B to 16 bits, so the stored low word is
`((source << 8) | (source >> 8)) & ffff`, an in-place byte swap.
AR6 advances once per executed block. The context-tool desk check covers
all 65,536 source words independently using byte serialization; it does
not execute this loop or prove its source range, BRC value, final buffer,
flags, pipeline timing or native startup. Those remain runtime obligations.

The isolated staged CPU body fixture (`tms54test -bios swap-body`) now
executes those five words without RPTB in 1,536 cases: sixteen input
patterns, sixteen SXM/OVM/FRCT/CPL combinations, two program pages and
three save points. It poisons the source after the single Read sample,
checks dependent A/B results and carry, and restores before/after Read
and before the store. One read, one write and one AR6 increment survive
each replay. Store address/AR6 publication is in the current frontend's
Read phase, and the write is in Execute; this remains a model phase,
not recovered silicon timing. The full core runner requires this marker.

The zero-gap join fixture (`tms54test -bios swap-join`) executes two
consecutive copies in another 1,536 replay cases. It reads successive
words at clocks 5/10, writes their byte-swapped results at clocks 10/15,
and advances AR6 twice. Both sources are poisoned after their respective
Read samples. Save points cover pre-Read, the first postincrement, and
the overlapping first write/second read. The older STL's implicit DAGEN
update must precede the younger LDU's same-clock Access: resolving them
in the opposite order rereads the first address. Store data publication
still belongs to Execute; explicit MMR-store forwarding is unchanged.
These checks prove the straight-line iteration join without inserting a
gap, not RPTB count, endpoint, interrupt or control-flow behavior.

Reproduce the isolated fixture in a disk-backed working directory:

```sh
/absolute/path/to/mame tms54test -bios swap-body -video none -sound none \
  -nothrottle -noreadconfig -log -seconds_to_run 5
```

Require the single `staged original swap body: PASS cases=1536` marker and
terminal `Fatal error: TMS320C54x isolated swap body complete` (exit 3).
Another exit-3 exception is failure, not completion.

For the join, use `-bios swap-join` and require its single
`staged original swap join: PASS cases=1536` marker and terminal
`Fatal error: TMS320C54x isolated swap join complete` (exit 3).
`MPY #lk,A`, `f066` at `00:2eda`, now has retained literal/T,
arithmetic and producer/replay coverage. STLM A,T,
`880e` at `00:2ed8`, now has publication and consumer/replay coverage.
Unsupported Decode can still stop older Execute; the final PC is not
retirement proof for instructions still in the pipeline. Repeated
`ST #lk,*AR1+`, `7691` at `00:08ea`, now has retained-literal,
two-clock stream and pending-IRQ/restoration coverage. BANZD,
`6e87` at `00:0a50`, now has short-indirect delayed-branch coverage.
The repeated `SUBC Smem,A`, `1e02`
behind `RPT #15` (`ec0f` at `00:352e`), now has staged repeat coverage.
The observed runtime reaches a different program range, but this does not
identify or validate the complete loaded firmware buffer or startup.
`ADD *(0016),0,A,B` at `00:319c`, encoded
`6ff8 0016 0d00`, now has staged coverage. Address `0016` is
mapped AR6, not RAM. The preceding `LD *AR1(0004),T` at `00:3194`, encoded
`30e1 0004`, and dependent MPY at `00:3196` now have staged coverage.
The preceding
`STL A,-1,*AR1(0003)` at `00:313e`, encoded `6fe1 0003 0c9f`, now has
staged store coverage. Its `0c9f` operation extension selects a low-word
store with shift `-1`, not a load.
`LD *AR6(0010),8,B` at `00:3291`, encoded `6fe6 0010 0d48`, now has
staged coverage. The checked original AA55 contract in
`mu4_context_contract.py` pins all three words and decodes the address
extension before the operation extension (SPRU172C LD syntax 4).
The signed five-bit shift is `+8`; destination is B. Unlike fixed
shift-by-16 syntax 3, syntax 4 affects OVdst and uses OVM saturation.
The static decoder tests all 32 shifts, both destinations and short/extended
forms; the separate executable matrix covers retained RAM/DAGEN behavior.
Short-indirect DST `4e81` at `00:30c6`, long-offset
LD `10e1` at `00:30c7`, short-indirect DLD `5681` at `00:30d3`
long-offset BANZ `6ce2` at `00:30df`, immediate SUB `f010` at `00:3216`
register SUB `f620` at `00:321c`, register LD `f540` at `00:30f7`
long-word DADD `5304` at `00:30fa`, LDU `12e1` at `00:3114`
MPYA `310b` at `00:3520`, LD Smem,16,A `440a` at `00:3521`
and MPYU `240b` at `00:3523`
now have staged coverage, but
unsupported Decode can stop older Execute work; later PC
observations alone are not a substitute for retirement evidence.

Long-offset/absolute STL/STH admit `80xx..83xx` with modes `e0..f8`.
Decode retains one offset/absolute literal; Read presents the destination
and applies the shared offset/preincrement/circular helper once; Execute
stores the selected low/high word using existing live-source SST/SXM
saturation. This preserves the short-store source policy rather than
adding an early accumulator sample. Sources/status are not changed by
saturation. RAM destinations only are accepted; peripheral/MMR, control-delay,
repeat-body and broader producer-conflict compositions remain unvalidated.
Read address/Execute source placement is a declared partial stage model,
not independently measured silicon timing (SPRU131G 7.1, SPRU172C STL/STH).

The 38,400 replay cases cover 24 long-offset forms plus absolute addressing,
four offsets including an opcode-shaped literal, four accumulator/lane
selectors, four SST/SXM combinations, four guard/sign patterns, both
compatibility modes and three checkpoints. Register 6's fixtures use the
high program page. BK=16 circular wrap and compatibility ARP selection
are checked. Poisoning the retained literal, changing the AR after Read,
and changing the source before Execute test the address/source boundaries.
Every leg must write once without reading the destination, preserve
unselected registers/status, and restore without duplicate pointer updates.
Extended MVDK admits `71xx` source modes `e0..f8` as three-word operations:
source offset/absolute address precedes the destination literal. Its complete
slot's Access computes and modifies the source address once, retains one RAM
sample, and follows the existing early DAGEN publication/defer machinery.
RAM destinations publish the retained word in Read after older Execute;
Execute advances past both extension words without another write. Only RAM
sources and RAM/AR0-AR7/SP/BK destinations are accepted. Peripheral/MMR
sources, control destinations, repeat bodies, control-delay slots, extended
bank-conflict cases and compiler-SP consumer composition remain unvalidated.
Class-19 bus placement is partial, not independently measured silicon timing.

The 38,400 replay cases use the independently calculated 25 source forms
and four offsets from the accumulator-store fixture, twelve destination
classes, four unsigned values, both compatibility states and four checkpoints.
They cover BK=16 circular wrap, high-page register-6 cases, source-pointer
and RAM aliases, and zero-gap CMPR consumption of the early AR publication.
Literal/source poisoning must not change the retained operands; every leg
must read and write once, preserve accumulators and avoid duplicate pointer
updates. Eight paired fixtures use the original `80e6,0010,71e6,0011,0012`
words: STL writes at clock 7, MVDK reads and publishes AR2 at 8, and an
immediate CMPR sees the new AR2. Four checkpoints replay both writes once.

TC/NTC BC and BCD (`f820/f830/fa20/fa30`) retain their predicate in the
complete slot's Read, corresponding to the opcode's Evaluate clock in
SPRU131G examples 7-16/17. Plain forms use the existing held-word/redirect
path; delayed forms use the existing late Evaluate path after older Execute.
Later delay-slot TC changes cannot reselect the destination. No new CPU
state or atomic fallback is introduced. Phase placement remains the partial
frontend model; overflow/carry and other conditional-branch families remain
outside this acceptance scope.

The 1,536 replay cases cover both forms/conditions/outcomes, BIT, CMPR-EQ,
status-bit and CMPR-LT producers (including the original `f5aa` family),
zero/one/two gaps, two pages, four checkpoints and a post-Evaluate TC-change
control. The latter changes debug CPU state in a firmware-free conformance
fixture, never the original upload. Tests assert one target-literal fetch,
one taken-target fetch, real delay-slot effects, the selected continuation,
unchanged unrelated flags and one BIT memory read where applicable.

Long-offset ADDM/ANDM/ORM now retain the offset and immediate as separate
Decode operands. Access uses the shared long-offset helper and records one
RAM address; Read retains the source word; Execute performs one write at
that same address. Ordinary/preincrement/circular modes `e0..f7` use the
existing three-word absolute grammar without treating the offset as a second
instruction. ADDM reuses the established SXM/OVM, bit-32 carry and sticky
OVA behavior; ANDM/ORM preserve arithmetic flags. No atomic arithmetic or
short/absolute publication rule is changed. New long-offset operands below
`0060` fail closed before the memory read. Repeat bodies, control-delay
slots and broader producer/peripheral arbitration remain unvalidated.
Class-18 Access/Read/Execute placement remains a partial stage model.

The 153,600 replay cases cover the three operations, 24 long-offset forms
plus absolute addressing, four offsets, eight value pairs and eight
SXM/OVM/initial-OVA combinations, both compatibility states and four
checkpoints. They reuse independent address and arithmetic fixture calculations;
register-6 cases use the high program page. Opcode-shaped offset retention,
BK=16 circular wrap, compatibility ARP, changed pointers after Access and
changed RAM after Read are checked. Each leg must read and write once with
the expected operation-specific flags and unchanged accumulators. The original
upload now consumes counter ADDM `6be6` at `00:3277`, long-offset SUB
`08e6` at `00:327c` and `BC AGT` at `00:327e` through repeated NAND reads.
The following BITF, short-indirect DST, long-offset LD and short-indirect
DLD, long-offset BANZ, immediate SUB, register SUB, register LD, DADD and
LDU, memory MPYA and memory LD shifted by 16 now have staged coverage;
unsigned memory multiply now has staged coverage. The subsequent extended
single-step and short-memory repeated SUBC now have coverage;
short-indirect BANZD and short-memory repeated immediate stores now have
coverage; the subsequent accumulator-to-BRC MMR store remains incomplete.
This is bounded transfer
coverage, not startup acceptance.

Zero-shift memory ADD/SUB A/B (`00xx/01xx/08xx/09xx`) now retain their
memory operand through Read and use the established 40-bit arithmetic
helper at Execute. Direct forms retain Decode CPL/DP and Access SP;
short indirect modes apply one pointer modification in Access, including
postincrement/postdecrement, AR0 indexing, reverse carry and BK circular addressing.
Short MOD=3 preincrement is write-only and is rejected for these sources.
Long-offset forms retain one Decode literal and use the shared address helper;
absolute forms retain the address directly. The existing absolute-ADD path
is preserved. Sources are bounded to RAM and mapped AR0-AR7 words; control
and peripheral sources fail closed. Carry is taken at bit 32, destination
overflow is sticky, and OVM clamps to signed 32 bits. Shifted/TS, unsigned,
carry-consuming, repeat and wider producer/control-delay compositions remain
outside this acceptance. Source/mode placement is a partial stage model,
not measured silicon timing.

The 104,448 RAM replay cases cover DP/CPL, the eleven read-capable short indirect
modifiers, three long-offset forms and absolute addressing; eight encoded
registers, four operation/destination selectors, eight guard/sign patterns,
four SXM/OVM states, both compatibility states and three checkpoints.
The direct-field matrix spans all 128 encoded offsets. Register-6 cases use
the high program page. Independent arithmetic expectations assert bit-32
carry, overflow and saturation without invoking the production helper.
Changed literals, pointers after Access and RAM after Read must not retarget
or resample the operand. An additional 1,024 cases read mapped AR0-AR7 via
direct/absolute addressing, change the source register after Read, and restore
before/after that sample. Each leg reads once, performs no data write, and
preserves the other accumulator and unrelated status.

Protected absolute ADD is explicitly classified as two words at retirement,
despite retaining its separate legacy decode/arithmetic path. Omitting it
missed XC's protected-window endpoint: the bounded pre-fix fixture read the
operand but observed zero vector fetches, acknowledgements or stack writes
by clock 36. The correction affects word accounting, not arithmetic or memory
sampling. The mirrored control-retirement classifier uses the same correction.

The 32 IRQ/replay cases cover XC1/XC2, absolute ADD and long-offset SUB,
both accumulators, two program pages and checkpoints before/after retirement.
An input-line edge is delivered before saving, with IFR owning the request.
After the full arithmetic instruction retires, each leg must acknowledge and
fetch one IRQ vector, save/restore XPC and the return address, execute the
continuation and reach IDLE with balanced SP. The data source is read once;
only the interrupt frame writes RAM. Other multiword protected-instruction
families still require their own retirement/composition coverage.

Signed accumulator BC/BCD now admit all six zero comparisons for A and B:
GE, LT, NE, EQ, GT and LE (SPRU172C condition table, page 4-18).
The predicate uses the full 40-bit accumulator, including guard sign and
nonzero guard bits, independently of SXM/OVM. Plain and delayed forms retain
the same existing Read/Evaluate and redirect paths; this extends the former
zero/nonzero predicate without changing TC/BIO behavior or atomic execution.
Phase placement remains a partial stage model, not silicon timing proof.

The 12,288 replay cases cover both accumulators/forms, six conditions, eight
signed/guard patterns including 40-bit limits, four SXM/OVM combinations, two
pages, four checkpoints and a post-Evaluate source-change control. The latter
is a firmware-free CPU conformance mutation, never an original-upload hook.
Target-literal poisoning, one taken-target fetch, real delay-slot source
changes, unchanged status and no data accesses are asserted. Twelve additional
LDM/long-offset-SUB/AGT fixtures use counts `0,1,2,65535`: the source reads
occur at clocks 5/7, SUB retires before evaluation, and taken targets fetch
at 10. Three checkpoints replay negative/zero/positive outcomes without
duplicate reads or writes.

BITF `61xx` retains its immediate mask separately from its source address.
Direct/short-indirect forms are two words; long-offset/absolute forms retain
the address extension before the mask and are three words (SPRU172C 4-22).
They share the existing captured-address machinery with memory arithmetic;
Read samples one RAM/mapped-AR word, Execute changes only TC according to
`(source & mask) != 0`. RAM and AR0-AR7 sources are accepted; control/peripheral,
repeat, protected/control-delay and broader producer compositions remain
unvalidated. Stage placement remains partial, not measured silicon timing.

The 52,224 RAM replay cases cover 17 read-capable encoding forms, eight encoded registers,
eight word/mask pairs (zero, bit 15 and opcode-shaped masks included), both
initial TC states, four SXM/OVM combinations, both compatibility states and
three checkpoints. Literal, pointer and post-Read RAM poisoning must not
change the result. Tests assert one read, no data writes, TC-only status
publication and unchanged accumulators. Ninety-six mapped-AR7 cases cover
immediate TC/NTC consumers, both initial TC values and three checkpoints;
source reads occur at 7, TC publishes at 8 and taken targets fetch at 10.
They include the exact upload sequence `61f8,0017,0001`; `007f` is an additional
conformance mask, not the original literal. The register changes after Read
must not be resampled, and neither leg may duplicate the source/target read.
Short MOD=3 is write-only and is rejected by CMPM/BITF in both frontends.
A separate 512-case replay bank checks rejection across both initial TC
states without data, pointer, accumulator/T or status effects. Long MOD=13
remains read-capable. These encoding checks do not settle the older circular
fixtures' input-domain constraints; see the [addressing audit](c54x_execution_contracts.md#operand-domain-acceptance).

This establishes bounded CPU and real I/O-retirement evidence, not complete
NAND-page, recorder, startup or speech acceptance. Existing atomic profiles
remain unchanged.

The frontend's firmware-free fixture reproduces the entry-side ordering of SPRU131G
example 7-18: acceptance after Decode at cycle 3, INTR Decode at 4, SP
decrement at 5, vector Fetch/IACK and the older decoded foreground instruction's
Execute at 6, continuation stack write at 7, and first ISR Execute at 10.
The already-prefetched flush word reaches Fetch at cycle 4 but never Decode
or Execute. The continuation is the first not-decoded instruction, not the
prefetch cursor. The test uses a one-word immediate load as the first ISR
instruction. Separate fast-return fixtures cover the example's return side.

RETF/RETFD increment SP and read RTN in Access, initiating target Prefetch
without a stack-memory read (SPRU131G examples 7-11/12). Execute clears INTM.
RETF discards the two sequential delay words; RETFD preserves them and
defers interrupt acceptance until they retire. Eight fixtures cover both
forms on base and extended program pages, restoring either pre-Access or
post-Access checkpoints. With interrupt acceptance at 3, return Access is
at 8, target Fetch at 9, return Execute at 10, delayed loads Execute at
11/12 and the resumed target Execute at 13 (example 7-18). No replay
increments SP twice or reads the stacked continuation. Four additional
RETFD cases fill both delay words with one two-word immediate load, on
base/extended pages. They restore partial Decode at cycle 8 or completed
Decode at 9, require one literal Fetch at 8, retire the load at 12 and
resume the target at 13 without treating the literal as an opcode.
Four pending-IRQ cases pulse INT0 after cycle 9 on base/extended pages and
checkpoint cycle 11. RETF accepts after Execute at 10, fetches the second
vector at 13 and writes continuation `0602` at 14. RETFD retains the pending
flag through both delay words, accepts at 12, fetches at 15 and stacks
`0604` at 16. Already-decoded target instructions drain while this entry
progresses. Both restored forms acknowledge exactly once per entry and
perform no return-side stack read. The input pulse is scheduler-synchronized
after the fixture callback, so IFR is checked from cycle 10, not inside the
callback issuing the pulse. These are executable conformance checks of the
staged acceptance/drain contract, not independent silicon latency measurements
or general control-flow/hazard completeness.

B/BD consume the target literal in Decode and initiate target Prefetch in
Access, not at atomic retirement. Eight base/extended-page fixtures match
SPRU131G examples 7-2/7-3: opcode/literal Fetch at 2/3, target Fetch at 6
and target Execute at 10. B flushes the two sequential words; BD executes
them at 8/9. The opcode and literal together retire as one retained operation
at 7; this frontend does not expose the manual's separate first-word Execute
occupancy at 6. Restoring partial Decode at 3 or redirected Prefetch at 5
fetches neither literal nor target twice. SP and INTM remain unchanged.
Eight further B/BD fixtures pulse INT0 after cycle 5, then restore at 6
or 8. The implemented acceptance contract keeps BD protected through
delay-word retirement at 9; its vector Fetch is at 12, continuation `0703`
is stacked at 13 and first ISR Execute is at 16. B accepts after its
retained two-word operation retires at 7: target Execute at 10 drains
alongside vector Fetch, `0701` is stacked at 11 and ISR Execute is at 14.
Every leg performs one vector acknowledgment and one literal/target Fetch.
This tests the model's interrupt acceptance/drain contract and restoration,
not independently measured silicon timing during a nondelayed branch's
flush occupancy. Conditional/far branches and calls remain outside coverage.

Separate empty-frontend IRQ and NMI fixtures accept at cycle 1, fetch the
vector at 4, write the continuation at 5 and retire the first ISR load at 8.
They stack the first unexecuted instruction at `0600`, not the advanced
prefetch cursor, and NMI preserves an unrelated latched IRQ flag.

DP-relative loads capture their operand address in Access, read the data bus
once in Read at cycle 5 and retire the captured value at Execute at cycle 6.
The fixtures change backing memory after Read and checkpoint that boundary:
both normal and restored retirement use the captured value, not a second bus
read. This covers both accumulators, not address/data hazards or multiple
data accesses.

Direct DP-relative STL/STH stores capture EAB in Read and commit EB in
Execute, as described in SPRU131G 7.1. Twelve fixtures cover A/B low/high
halves and existing unsigned/signed saturation semantics. Changing DP after
Read cannot redirect the pending write. All twelve restore a post-Read
checkpoint and require exactly one Execute-stage write per leg while the
older store drains alongside interrupt entry. These fixtures use ordinary
RAM; they do not establish MMR-write hazards or memory arbitration.

Short indirect loads perform DAGEN and the AR modification in Access, read
the retained address in Read and retire the captured value in Execute.
Eight AR1 fixtures cover both accumulators and plain/post-decrement/
post-increment/pre-increment addressing. They change AR1 after Access,
checkpoint there and require one read of the original address per leg,
without repeating the AR modification on restoration. Extended, indexed,
circular and compatibility-mode hazard contracts remain outside this scope.

Direct STM to DAGEN registers follows SPRU131G 7.5.2/Table 7-4 rather than
ordinary store retirement: the opcode word writes in Read, coincident with
Access of the retained two-word instruction. With STM first in the stream,
this is cycle 5. If an older STL/STH or RMW store writes ARx/SP/BK that cycle,
the register bank conflict defers STM by one clock; the deferred flag travels
with the instruction and is saved. Twelve executable fixtures cover AR2,
SP and BK, with and without a preceding STL to AR1. The older store writes
at 6 and the conflicting STM at 7, matching example 7-22's cross-register
bank arbitration. Checkpoints before/after the normal write and during/after
the deferred write restore without duplicate bus writes. These tests cover
that store/STM conflict, not general DAGEN hazard resolution or peripheral
register initialization timing. The negative Decode fixture now uses an
unsupported extended indirect STM `77e1`, not the qualified plain-indirect
RAM forms, rather than the supported direct AR1 form.

#### Peripheral MMR access cost

[TI SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf), March 2001,
classes 12A and 18A (pages 3-26/3-39), specify a two-cycle base cost for
STM and ANDM respectively, plus one cycle for a peripheral MMR access.
The downloaded reference is
`roms/reference-docs/c54x/ti_tms320c54x_mnemonic_spru172c.pdf`, SHA-256
`c4f89ce4a774a76de1d93a2413e882a71b82d236fc2c52af63f3159368a37e75`.

The native semantic backend records peripheral data accesses during each
instruction and charges one extra clock at retirement. RMW read and write
share that charge. SPRU131G's memory maps distinguish peripheral registers
`0020..005f` from CPU registers `0000..001f` and scratch DARAM `0060..007f`;
scratch accesses must not inherit the penalty. Debugger/DMA bus accesses
do not run the semantic accounting path. Its transient flag is reset before
every synchronous instruction, not saved as an in-flight wait.

Twenty executable cases measure back-to-back operand bus intervals with
the timer stopped and INTM set. STM, ANDM, ORM, ST and LD each test AR2,
PRD, the last peripheral address `005f` and first scratch address `0060`.
Read-only accesses receive the same one-clock increment; RMW receives it
once rather than twice. Representative checked intervals are:

| Operation | Destination | Observed Write Interval | TI Class Cost |
| --- | --- | --- | --- |
| STM | AR2 | 2 clocks | 2 clocks |
| STM | PRD | 3 clocks | 3 clocks |
| ANDM | AR2 | 2 clocks | 2 clocks |
| ANDM | PRD | 3 clocks | 3 clocks |
| LD | PRD | 2 clocks | 2 clocks |
| LD | Scratch `0060` | 1 clock | 1 clock |

All twenty costs are asserted. Existing instruction fixtures for MMR22,
TCR, CLKMD, STLM/LDM20 and direct operand `0045` now include the documented
peripheral cycles; their former blanket CPU-register costs were wrong.
The shared `0060` bench callback serves stored scratch data for this gate,
not its separate IRQ-raising fixture behavior.

Native 5110 save/load passes. The four timer-boundary observations still
stall with short PRD and complete with long PRD: this cost correction does
not replace Decode/Execute ownership. A new fresh 30-second 5110 run retains
20,285 frame expiries, 648,192 RF reads and 1,520 port-32 writes, exactly the
previously reproduced mismatching snapshot. The strict RF gate was not
re-banked as part of that correction; the current independent-transfer
snapshot is documented separately above.
The opt-in staged frontend holds pipeline advancement for one clock before
a peripheral LD read, STL/STH write or ANDM/ORM read. A saved per-operation
flag owns the paid wait and travels with the retained operand; RMW does not
pay again at its write. Sixteen executable cases cover LD, STL, DP-relative
ANDM and absolute ANDM in both program pages, restoring either the held
stages or the resumed access. They assert the access/retirement clocks and
that save/load never repeats a bus side effect.

This stall placement is a declared prototype latency model, not recovered
silicon bus-phase timing. These class tables establish instruction costs,
not every multiword operand's exact bus phase; do not extrapolate first-word
DAGEN Read timing to all peripheral accesses. Peripheral STM remains outside
the staged instruction subset. Simultaneous peripheral read/write stage
occupancy stops as unsupported rather than inventing arbitration; that
exclusion is checked by adjacent peripheral STL/LD in the extended page.
The fixture stops at cycle 6 with zero CPU bus accesses and unchanged PRD
and accumulator contents. Debugger PRD export itself reads the bus, so its
observation occurs only after the CPU-access taps have been removed.
This proves the exclusion, not a silicon arbitration rule. No handset enables
the staged frontend, and these fixtures do not establish native RF or speech
completion.

ANDM/ORM retain the immediate separately from the sampled data operand.
The three-word absolute form consumes address then literal; its two pending
extensions are saved independently from the completed pipeline slots.
Access owns the target address, Read samples the value and Execute commits
the logical result at that retained address. Twelve fixtures cover both
operations with DP-relative, post-increment and absolute operands, restoring
either incomplete extension assembly or a post-Read/pre-write checkpoint.
Changing DP/AR after Access or backing RAM after Read does not alter the
pending operation. Restoring after Read performs no second operand read.

A separate absolute ANDM fixture targets the real IMR register through the
ordinary CPU data map: Read at 7 samples mask 1, Execute at 8 writes mask 0,
the already-accepted interrupt finishes, and a subsequent IRQ remains pending
with INTM clear. Its restored post-Read leg writes once without rereading IMR.
The complete synthetic timer program now executes in the staged subset,
as described below. General control flow, idle rules and native enablement
remain incomplete; this IMR fixture alone does not establish timer or recorder
completion.

Two-word immediate loads retain the opcode while the following fetched word
completes Decode as its literal. They consume distinct program fetches
(SPRU131G 7.1), never interpret opcode-shaped literals as instructions and
retire one clock later than one-word loads. Four IRQ fixtures cover A/B,
unsigned/signed extension and shifts 0/15/16: literal Fetch is at cycle 3,
complete Decode at 4, load Execute/vector Fetch at 7, and stacked continuation
`0602` at 8. All four replay a saved incomplete-Decode checkpoint without
rereading the literal. A separate uninterrupted stream retires consecutive
long loads at 7/9 and the following short load at 10, fetching each literal
once. The prototype defers interrupt acceptance until its two-word Decode
completes; these are executable implementation checks, not independent
silicon validation of multiword interrupt latency or pipeline hazards.

Slot addresses, fetched words, operand addresses/values, incomplete Decode,
validity, pending frontend redirection,
continuation and active-clock counter are saved. A checkpoint after cycle-4
Decode reproduces every subsequent observed stage and architectural result,
with one vector fetch/acknowledgement per leg. The bench suspends execution
for scheduler settlement after loading inside a live callback; that pause is
not emulated interrupt latency or a physical wall-time replay claim.
`check-c54x-core` requires staged-frontend, direct-load, store, indirect-load,
RAM/MMR RMW, multiword/replay/stream, empty IRQ/NMI and unsupported-form markers.
Existing native execution remains protected by its full conformance suite.
`verify-5110-save-state` passes with the added partial-Decode fields; this
revalidates native CPU/register/I/O restoration independently of the exact
RF scan snapshot described above.

The short-period timer probe has the following remaining staged dependencies.
These are exact encodings in `start_timer_boundary_probe`, not a general ISA
completion list:

| Probe encoding | Meaning | Staged ownership still missing |
| --- | --- | --- |
| `7681 literal` | Immediate ST through AR1 to PRD/TCR | Implemented bounded short-indirect ST with retained literal/address and peripheral write wait; this is not STM |
| `76f8 0501 0001` | Absolute immediate ST of the completion marker | Implemented three-word Decode and retained memory write |
| `4a1e` | PSHM XPC at the interrupt vector | Implemented retained source sample, stack decrement/address and stack write |
| `f4e5` | FRETE | Implemented plain retained far return; pending IRQ/adjacent push timing not independently validated |

The probe's `7711` DAGEN initialization, `6881`/`68f8` mask ANDM and `f073`
branch are already in the bounded subset. Peripheral STM coverage alone
cannot make this probe executable. SPRU172C identifies immediate ST as
classes 12A/12B (two/three words and base clocks), PSHM as class 16A
(single source read and stack write), and FRETE as class 34 (six clocks,
four when delayed). FRETE restores XPC from TOS, increments SP, restores PC
from the next TOS, increments SP again and clears INTM. Those architectural
operations and costs do not independently establish each prototype bus phase.
SPRU131G Table 7-4's early Read write rule for immediate ST applies to DAGEN
destinations, not automatically to PRD/TCR.

Immediate ST now retains its two/three-word literal through Decode, owns its
short destination in Read and writes the retained value in Execute. Absolute
destinations come from the saved extension word. The existing peripheral
wait applies once before a PRD/TCR-class write. Eight executable cases cover
DP, post-increment, absolute and peripheral destinations, restoring before
or after the write. An opcode-shaped literal remains data, and post-increment
and bus writes occur once across replay. This is the same declared partial
bus-placement model as other staged stores, not independently measured
silicon timing. DAGEN destinations stop before their write because their
early-write contract is not implemented; indirect address modification may
already have occurred. Long-offset, compiler-mode direct and other indirect
encodings remain outside this subset.

Direct PSHM forms `4a00..4a1f` sample their MMR once in Read, retain the
decremented stack destination, then commit the sampled value in Execute
(SPRU172C page 4-132, class 16A). The 198 cases retain the six explicit XPC
controls and cover all 32 direct sources in both program pages, restoring
before Read, after Read or after Write. Each requires one source access,
one SP decrement and one stack write per leg. PSHM SP samples the original
SP before decrement; the original MU4 prologue's `4a06`/`4a07` ST0/ST1
pushes are now accepted by this staged subset.
Read-owned stack allocation is a declared partial stage model, not a silicon
phase claim. Other PSHM sources, indirect forms, SP/DAGEN conflicts and
preceding accumulator-producer latency are not covered by this implementation
or gate.

Staged direct POPM supports `8a06..8a19`: ST0/ST1, accumulator
parts, T/TRN, AR0--AR7, SP and BK. It retains the stack word and increments SP
in Read. AR0--AR7, SP and BK also publish in Read (SPRU131G table 7-4);
other supported destinations publish in Execute. An older Execute-stage
store into the DAGEN bank defers the early publication one clock, as
specified by table 7-5. The previous all-destinations Execute placement
was a prototype assumption contradicted by that table, not silicon evidence.
The 19 non-SP destinations have both program pages and three replay
boundaries, giving 114 cases. Each requires one stack read/increment and
one register write per leg, correct guard-byte masking and ST1/XF output.
XPC, other control/peripheral destinations and indirect forms are
rejected. Twelve additional replay cases compare an older STL to AR1
with an older STL to RAM: POPM AR2 keeps its stack read/increment in
clock 6, but only the DAGEN-bank case defers its destination write to
clock 7. Changing the stack source after Read must not change the
retained result. General mixed DAGEN/status-producer conflicts remain
partial, not measured silicon behavior.

Basic compiler-mode LD/ST retain their Access-stage SP sample separately
from Decode-time CPL/DP. This gives the one-instruction category-I consumer
latency after ordinary PSHM/POPM MMR in SPRU131G table 7-9: an immediate consumer uses the old
SP, while one intervening NOP allows the updated SP. Sixteen cases cover
PSHM AR1/SP and POPM AR1/BL, LD/ST and both spacings, restoring after the SP
sample and before consumer Read. Each verifies the operand or destination, untouched
alternate address, retained push value and final SP. Remaining SP producer classes,
extended-shift category-II consumers, indirect forms and physical stage
placement remain unvalidated. The atomic frontend is unchanged.

POPM SP is distinct: it publishes the popped value instead of leaving
the postincremented stack address, while a basic compiler-mode consumer
requires two cycles of separation (table 7-9 Category I). STM/MVDK SP
and FRAME require one; a DAGEN-bank conflict adds one to POPM SP.
The staged frontend retains the prior compiler-address SP separately
from architectural SP, including save-state registration. Stack operations
use architectural SP directly; the compiler-address window must not
delay them or generate another register publication.

The 320 replay cases cover those four producer kinds plus conflicted
POPM SP, basic LD/ST consumers, zero through three NOPs and two program
pages. A popped/moved source changed after Read must not alter SP.
They distinguish the intermediate postincrement during a deferred pop
from both the old compiler-address base and the final popped SP. No
extra source reads or register/store writes are permitted on replay.
This closes the tested basic Category-I cases, not general multiword
consumers, other SP producers or all bank-conflict chains.

Four cases execute the original `8a18 f495 f495 fc00` at
`02:3d3a`--`02:3d3d`. POPM SP restores the caller frame, then the two
NOPs and RET consume its captured return word at that restored address.
Changing the popped SP word and return word after their respective reads
cannot retarget the return. The same-page caller marker retires with one
SP publication and two data reads per leg. This is the original wrapper
suffix, not the complete idle/wake call.

Plain RET/RETE retain the stack address and increment SP in Access, sample
the return word in Read and redirect at Execute (SPRU131G example 7-7;
SPRU172C pages 4-139--140, class 32). Allocation accounts for older
PSHM/POPM Read updates. Table 7-2's first-half DB read precedes a same-clock
second-half EB write: an adjacent push exposes the old DARAM word, while
one intervening NOP exposes its completed write. No stack-return bypass is
inferred from the old prototype fixture. RET preserves INTM; RETE
clears it at retirement. Target Prefetch/Fetch/Execute is clock 6/7/11 for
a return starting at Prefetch 1. Thirty-six cases cover both forms/pages,
isolated, adjacent-push and one-NOP returns, with restoration around Access/Read.
They require one return read/pop/fetch and no sequential execution. General
DAGEN conflicts and pending IRQ timing for these plain forms remain unvalidated.

RETD follows example 7-8: its two prefetched delay words are saved rather
than decoded early or fetched again. After return Read they enter Decode at
clocks 6/7, with target Prefetch/Fetch/Execute at 6/7/11. Twenty-four cases
cover two pages, two one-word delays, one two-word STM-to-T delay, and the
original `fd45 f6bb` (XC 1,AEQ / RSBX INTM) pair. Each replays queued words
and the retained stack sample without duplicate reads or a third delay Fetch.
Twelve DARAM ordering controls require the old stack word for adjacent
PSHM/RETD and its new word after one NOP, without altering delay execution.
XC protection inside the return delay is mapped onto the resumed word stream,
not the abandoned sequential address. Twelve cases make an IRQ pending
during return: A=0 allows the INTM clear, but the first resumed instruction
must retire before IRQ acknowledgement; A=0800 rejects the clear and leaves
the source pending. This validates that bounded delay/IRQ composition, not
RETED, nested controls, NMI, other XC placements or physical silicon timing.
Ten further cases execute all seven original `02:3ddb`--`02:3de1` return-tail
words with saved ST1 values `0034`/`0834`. POPM samples the saved status,
AND reduces A to its INTM bit, and the original XC/RSBX pair either restores
unmasked state or preserves the mask. Both stack words are changed after
sampling; five replay boundaries must still reproduce the retained values,
one target Fetch and the correct delayed IRQ outcome. The first continuation
instruction retires before IRQ acknowledgement. The continuation/vector are
neutral test instructions, not the native caller/ISR. The read-only contract
tool pins all seven words and classifies every saved ST1 value: 32,768 masked
and 32,768 unmasked. It describes restoration of INTM only, not full ST1.
Complete worker-body execution and the native recorder remain unvalidated.

The original nonzero-control-word route has a coherent call/return fixture:
four CALLD/STM parameter words at `02:3d92`, nine worker-entry words and
seven return-tail words, all unchanged from R060. Sixteen cases cover saved
masked/unmasked ST1 and eight replay boundaries. The call stacks `3d96`,
the worker stacks that ST1, and its skip-body branch consumes two source
samples before the tail pops both owned words and balances the caller SP.
T receives parameter 6. Source and stack RAM changes after sampling must
not affect retained values; CPU bus counters exclude those debugger-side
mutations. Pending IRQ delivery resumes only after the caller continuation's
first instruction. There are no duplicate stack/source reads or route Fetches
on replay. The source words, caller continuation and vector remain fixture
inputs; this does not execute the zero-control-word body, outer interrupt
handler or live recorder and does not establish native speech.

The complete original outer routine now has a bounded call-route fixture:
all 51 main words plus the nine-word worker entry and seven-word return tail.
Its input status `c004` selects the call and a nonzero second worker word
selects the mapped skip-body route. Forty cases cover pages 0/2, masked and
unmasked entry ST1, and ten replay boundaries across context save, DP delay,
nested call, branch, return and restoration. The fifteen ordered stack writes
include all outer/extra saves, the nested return, worker ST1 and reserved
return slot. Terminal checks require original A/B, ST0/ST1, AR0/AR2/AR3/T,
balanced SP, one counter increment and one reserved-return Fetch. Checkpoint
bus counts are retained separately from emulated device state so replay
cannot hide duplicate operations. The routine is entered directly with
fixture RAM and a neutral final continuation; timer-driven interrupt entry,
the zero-control-word worker body and live recorder remain unvalidated.

Staged CALL/CALLD retain a stack destination at the opcode's Access clock
(complete target Decode), load RTN in the next clock and write the retained
return word at the opcode's Execute clock (SPRU131G examples 7-4/5).
For a call starting at Prefetch 1 these are clocks 4/5/6, with target Fetch 6.
CALL stacks the PC after its two instruction words; CALLD includes both delay words.
RTN is also exposed as a debugger state register without changing the MMR map.
Eighty-four round trips cover both forms/pages, with or without an older
PSHM, restoring after allocation, RTN setup or stack commitment. Each requires
one call write, target fetch and callee RET read, correct delay execution and
balanced call SP. The older push retains its own distinct stack word. General
DAGEN conflicts, pending IRQ timing and nested in-flight controls remain
unvalidated by these cases.

The exact `770e literal` STM-to-T form now uses the retained two-word register
write path. In this prototype it publishes at the opcode's Read clock, like
the existing early DAGEN STM path, but is not deferred by DAGEN-bank conflicts.
This is a partial placement model of SPRU131G example 7-39's zero T latency,
not a measured physical write phase. Sixty of the call cases use STM-to-T as
the complete CALLD delay, then LDM T in the callee. They cover parameters
`0006`, `0000`, `8000`, `f495` and `ffff`, both pages, adjacent pushes and
checkpoints after call allocation/RTN/stack write. Each requires one T write
before callee Read, correct unsigned parameter delivery, balanced call SP
and no duplicate publication on replay. The original `770e 0006` is therefore
executable at the call boundary; general STM destinations and concurrent
MMR-producer conflicts remain unvalidated.

The original worker begins at `02:3dbc` with `fd00 4a07 fd00 f7bb`:
unconditional `XC 1` around PSHM ST1 and SSBX INTM. All four words are
present in the original R060 upload. This is not a conditional worker-entry
predicate. SPRU131G section 7.1.4/example
7-13 places XC condition sampling in Access, before the following instruction
is decoded; the two preceding one-word instructions have not retired yet.
SPRU172C pages 4-198--200 permit XC 1 over one one-word instruction and XC 2
over two words, and protect XC plus two following words from interrupts even
for XC 1. Rejected slots must not decode or perform memory accesses.
The atomic implementation's execution-time predicate evaluation therefore
cannot be reused as a physical staged sampling contract. Staged acceptance
now covers the original unconditional prefix in four checkpoint/replay
cases: PSHM publishes the old ST1 once before SSBX sets INTM, with the
stack allocation preserved on restoration. Four further cases execute the
original seven words through `7192 0010 1092`, with neutral NOPs afterward.
MVDK samples the old AR2 address once, increments AR2 and publishes AR0;
the adjacent LD samples the next word and increments AR2 again. Source RAM
changes after the move sample cannot change the retained AR0 value. Saved
context at four boundaries reproduces one stack push and two ordered RAM
reads without duplicate pointer updates. This is bounded prefix composition,
not a complete worker, interrupt entry or native recorder run.
A saved word-address guard
implements the two-word protection window. Four additional XC 1/XC 2 cases
make a maskable IRQ pending after Decode: two following stores must retire
before the vector is fetched once. Restoration during protection or at its
completion preserves both stores, IFR clearance and one interrupt stack
allocation. Multiword protected slots, NMI and protected control transfers
remain unvalidated. The four TC/NTC conditional forms now sample TC in
Access before older Execute and the next word's Decode. A saved skip-word
counter replaces rejected words with NOPs before operand decoding.
Thirty-two cases cover both conditions and word counts, changing TC between
Decode and Access and again after Access. Only the Access value controls the
two following stores; rejected stores never publish, including after four
checkpoint/restoration boundaries. Another 32 cases use one two-word
immediate store with the opcode-like literal `fd30`: accepted execution
stores that literal as data, rejected execution suppresses both words, and
the following independent store still retires. Sixty-four instruction-driven
cases place SSBX/RSBX TC before XC with zero through three NOPs. XC observes
the old TC with zero/one NOP and the new TC with two/three, matching the
Access-time latency in example 7-13. All 128 cases replay saved Decode,
sampling or retirement state without duplicate writes. General multiword
operands and conditional IRQ integration remain unvalidated.
The four accumulator zero/nonzero predicates are also supported in Access.
Forty-eight cases cover AEQ/ANEQ/BEQ/BNEQ, both word counts and three replay
boundaries. Accumulators switch between Decode and Access and switch back
after sampling; a value nonzero only in the high accumulator bits must still
select the nonzero condition. Accepted stores retain their publication
clocks, rejected stores perform no writes, and ST0/ST1 are unchanged.
This makes the return tail's exact `fd45` executable; its placement within
RETD is covered by the delayed-return fixtures. Accumulator producer latency,
compound/overflow predicates and other delayed-control/XC combinations
remain unvalidated or unsupported.
The complete worker remains unsupported.

Direct `MVDK Smem,dmad`, including the original worker setup `7131 0012`,
now retains its source sample and destination literal in the staged frontend.
The bounded subset accepts RAM sources and RAM or DAGEN destinations
(`10`--`19`); peripheral/control transfers and extended/repeated forms remain
excluded. Following SPRU131G section 7.5.2 and SPRU172C class 19A, the
prototype writes DAGEN at the opcode's Read clock and RAM one clock later.
Sixty cases cover DP/CPL source addressing, AR2/SP/BK/RAM destinations,
unsigned values and restoration before/after source sampling or destination
publication. Changing source RAM after sampling must not change the copied
value; replay requires exactly one source read and destination write.
Nine cases additionally require one-clock DAGEN deferral behind an older
direct store to AR1; three RAM-destination controls retain ordinary write
timing. Checkpoints straddle source sampling and deferred publication.
General producer hazards and precise physical phase placement therefore
remain unvalidated; this does not enable the staged frontend for native firmware.

Short indirect MVDK now uses the same retained transfer. Forty-eight additional
cases exercise `7182`/`7192` (stationary/postincrement AR2), copying to AR0,
AR2, SP or RAM. The source is sampled at its old address once, then the
source AR is updated before the destination is published. Twelve overlapping
AR2-destination cases require the copied value to replace the incremented
pointer, rather than a later increment corrupting the result. Source RAM is
changed after sampling, and all three replay boundaries must retain the old
word with no duplicate read/write. This makes the original `7192 0010` worker
setup executable in isolation. Short modes below `98` use the existing
address generator; other AR/mode combinations, producer hazards and physical
phase placement are not established by these fixtures. Complete worker and
native recorder execution remain separate, incomplete requirements.

The read-only context tool also pins all nine original worker-entry words
`02:3dbc`--`02:3dc4`. Its non-delayed `f844 3ddb` branch skips the body at
`02:3dc5` when the second source word loaded through AR2 is nonzero; zero
falls through into that body. Exhaustive unsigned-word classification gives
65,535 taken inputs and one fallthrough input, without claiming which value
native recorder execution supplies. The first source word goes to AR0, the
second to A, and AR2 advances twice; neither source's producer is identified
by this bounded census. SPRU131G section 7.1.5/example 7-16 requires BC to
take five cycles when taken and three when not taken, discarding sequential
work only for the taken outcome. BCD's unconditional delay-slot execution
cannot be reused for this branch. The staged BC subset now supports
AEQ/ANEQ/BEQ/BNEQ with a saved prefetched sequential word. At the prototype's
Evaluate clock it either releases that word to Decode or discards it and
the second prefetched word before either can access data RAM. Forty-eight
cases cover both pages and outcomes, with restoration before/after word
retention and predicate evaluation. Each sequential word is fetched once;
taken branches fetch the target at clock 7 (Execute 11) and perform zero
sequential data reads, while rejected branches retain the two reads at
clocks 8/9. Changing the tested accumulator after evaluation cannot change
the route. Existing BCD delay-slot gates remain unchanged. This is bounded
comparison against example 7-16, not independent silicon timing evidence;
multiword sequential slots and IRQ integration remain unvalidated.
Six further fixtures compose the original nine words at their original
page-2 addresses, with neutral NOPs after the prefix and at the target.
The adjacent LD's sampled second word is zero or one, causing BC to
fall through or take `02:3ddb` respectively. Three checkpoints surround
the load sample/retirement and branch decision. Source RAM changes after
sampling and A changes after evaluation cannot alter the retained route.
The selected target or fallthrough Fetch occurs once at clock 14; both
outcomes retain one old-ST1 push, two source reads and two AR2 updates
without duplicates on replay. This validates that specific adjacent producer
composition, not the unknown worker body, return tail or native recorder.

Plain FRET and FRETE retain two ordered stack samples, mask the restored XPC
to seven bits, pop SP twice and redirect. FRET preserves INTM; FRETE clears it
after both samples, following SPRU172C pages 4-61--62.
The prototype holds Read for the second sample, flushes sequential words
and restarts target Prefetch at clock 7 (Fetch 8, Execute 12 for a return
starting at Prefetch 1). Twelve cases cover both instructions, cross-page
directions and restore before the first read, between reads or after both reads. Older
Execute writes precede the first stack sample, so an adjacent PSHM's word
can be observed. This is a declared bus-placement model of the documented
six-clock return, not independent silicon phase evidence. The FRET cases
start with INTM set and require it to remain set across return and replay.
Delayed FRETD/FRETED retain XPC in Access and PC in Read, then start target
Prefetch at clock 5 (Fetch 6, Execute 10). This is a declared placement model
of the documented four-cycle transfer, not independent silicon phase evidence.
Two one-word delay instructions retire at clocks 7/8, or one two-word delay
instruction retires at clock 8. The next sequential word is discarded;
control-word ownership prevents maskable entry before the delay completes.
Twenty-four delayed cases cover both return forms, both cross-page directions,
both delay-slot layouts and restore before, between or after stack samples.
Together with the twelve plain cases, all 36 require one target fetch and two
ordered stack reads across replay, correct SP/INTM and no extra sequential
instruction execution. Nested control transfers in delay slots are rejected.
Eight pending-IRQ cases cover plain/delayed returns in both cross-page
directions, with a request after clock 4 and replay from clock 5. FRETE/FRETED
fetch the vector in the restored page and stack the first undecoded target
continuation (`0700` plain, `0702` delayed); decoded target work drains before
entry. FRET/FRETD controls preserve INTM and the pending request, with no
vector fetch or entry stack write. Every enabled case requires one vector
fetch and one continuation write per replay leg. These are staged-model
observations, not independent evidence of silicon re-entry phases. General
stack conflicts and physical pending-interrupt timing remain unvalidated.

The exact `f6bb`/`f7bb` RSBX/SSBX INTM forms establish protection in Decode
and retain it through retirement of the next complete instruction, including
its extension words (SPRU131G section 6.10.7). This is instruction-owned
state, not a fixed clock delay; both the following-instruction latch and
per-stage protection survive save/restore. Twelve cases cover both forms with
one-, two- and three-word followers, injecting a pending maskable interrupt
after INTM Decode and replaying a checkpoint during follower Decode. Six
include adjacent opposite INTM operations: protection extends through the
second operation's complete follower without an intervening vector fetch.
RSBX
permits one vector fetch only after the follower retires; SSBX leaves the
request pending without a vector fetch. The three-word store commits once
per replay leg. NMI interaction and silicon phase
placement are not validated by these cases. Handset configurations still
use the atomic frontend; this does not resolve the MU4 recorder boundary.

Both RSBX and SSBX support all sixteen bits in ST0 and ST1, matching the
one-word class-1 forms in SPRU172C pages 4-151 and 4-166. The staged
implementation changes only the selected bit at Execute, without invoking
the atomic backend's interrupt guard. Only the ST1 INTM forms establish
Decode-owned follower protection. All 128 combinations of register, bit,
set/clear and initially all-zero/all-one status are executable fixtures;
each restores immediately before Execute and verifies both status registers
on every observed cycle. The device's XF output must match ST1 bit 13 on
every cycle and after checkpoint restoration; staged retirement publishes
the output through the same memoized callback as the atomic frontend.
This includes the original MU4 handler's `f6be`
RSBX CPL. Repeat legality and full handler execution through the staged
subset remain unvalidated.

Basic direct LD/ST retain Decode-time CPL and DP independently of the later
address-generation stage. SPRU131G tables 7-18 and 7-19 require three
intervening instructions after RSBX/SSBX DP or CPL for these category-I
consumers. Thirty-two cases cover CPL and DP bit zero, set/clear, LD/ST and
zero through three NOPs, with distinct data-page and stack-relative
sentinels (or two data pages for DP). Zero through two NOPs use the old
field; three use the new field. Every case restores after consumer Decode,
before its address is generated, and reproduces the selected address and
untouched alternate location. Other DP producers, extended-shift category-II
consumers, SP conflicts and immediate/RMW direct forms are not established
by this gate. The atomic frontend and handset configurations are unchanged.

The staged `LD #k9,DP` form forwards the nearest in-flight immediate DP to a
basic direct consumer's retained Decode state, implementing the zero-latency
contract in SPRU131G table 7-17 and example 7-44. Architectural ST0 changes
at Execute, not early Decode. All 512 page values are tested with an adjacent
LD, and again behind two consecutive immediate-DP producers (1,024 cases).
Each case restores after consumer Decode but before producer retirement;
the new page supplies the operand, while upper ST0 bits and ST1 remain
unchanged and the old architectural DP persists until Execute. This is a
bounded forwarding implementation, not measured silicon bus placement;
mixed status-register producers and other DP consumer families remain open.

Direct `LD Smem,DP` retains one memory sample, holds pipeline advancement
for two additional clocks and commits the low nine bits into DP at Execute
(SPRU172C page 4-71, class 5A). Forwarding updates younger basic direct
operands, giving the zero-DP-latency contract in SPRU131G table 7-17 without
early architectural ST0 commitment. All 512 page values have restore before
Read, after Read or during the held interval (1,536 cases). Each requires
one source read, one adjacent-consumer read and preserved upper ST0/ST1/B;
changing source memory after Read must not affect either DP or the consumer.
The declared prototype samples at clock 5, retires the producer at 8 and
the consumer at 9; physical stage placement remains unvalidated. The gate
covers a RAM source and adjacent basic LD only. Indirect/extended forms,
peripheral-source timing, mixed producers, store consumers, interrupt overlap
and delayed-control composition remain outside this acceptance scope.

Staged FRAME accepts all signed eight-bit offsets. The prototype updates SP
in Access and does not apply the offset again at Execute; the immediately
following PSHM therefore allocates from the adjusted stack, as required by
SPRU172C page 4-60. All 256 offsets have three checkpoint/replay boundaries
(before/after FRAME's update and after the push's source sample), giving 768
cases. Each verifies unchanged status, one FRAME update, one push decrement
and one retained write per leg. Access placement is a declared partial model,
not measured silicon timing. Basic compiler-mode consumers, earlier SP
writers, interrupt overlap and wrapping SP values remain unvalidated. The
original MU4 prologue's `eeff` is accepted, not a claim that its full handler
or the recorder completes.

Direct DLD retains the high word at its generated address and the low word
at address XOR 1 in Read, then assembles the accumulator at Execute
(SPRU172C page 4-42, class 9A). Both destinations, SXM/C16 settings,
high-word signs, odd/even addresses and DP/CPL addressing have 128 cases
with restore before/after Read. Changing both source words after Read must
not affect retirement. The other accumulator, SP and both status registers
remain unchanged; negative SXM loads extend the guard byte. Register/peripheral
pairs, indirect and extended-address forms are rejected. Preceding status-
producer latency remains unvalidated. Paired Read placement is a declared
prototype model, not independent physical-bus timing evidence.

Staged two-word immediate ADD retains its literal through complete Decode
and uses the shared arithmetic helper at Execute without another program
read (SPRU172C page 4-4, syntax 7). All source/destination combinations,
shifts 0/1/7/15, SXM/OVM settings and positive/negative boundary operands
have 256 cases with restore during partial or complete Decode. They require
correct carry, destination overflow and saturation; changing the literal
after Fetch must not affect retirement. The additional SUB/shift-16 matrix
below covers the other arithmetic forms; mixed status producers and
delayed-control overlap remain unvalidated.

Immediate SUB (`f010` family, shifts 0--15) and the separate shift-16
ADD/SUB encodings (`f060/f061` with source/destination selectors) use
the same retained-literal path. Execute extends the 16-bit literal under
SXM, shifts it, then uses the shared 40-bit arithmetic helper without
fetching program or data memory. SUB computes selected source minus
operand, not selected destination minus operand; source/destination
aliasing reads the original source. Carry reflects the low-32-bit
operation before OVM saturation, and destination overflow is sticky.
The unselected accumulator, other overflow bit and remaining status
fields are preserved.

The new matrix has 26112 SUB cases across shifts 0--16, all four A/B
selections, eight source/literal patterns, eight SXM/OVM/sticky-overflow
states, two pages and saves during partial Decode, complete Decode or
Read. Another 1536 cases cover shift-16 ADD. Opcode-shaped literal `f495`,
40-bit guard/sign boundaries, borrow/carry and saturation are included.
Fixtures change the program literal after Decode and assert publication
only at Execute, clock 7 in this partial frontend model, with one literal
fetch and no data-memory access on each replay.

Another 144 firmware-free producer/replay cases compose a two-word
signed immediate load with SUB through zero, one or two intervening NOPs,
all four A/B selections and source values `0,1,8000,ffff`. They verify
that SUB sees the load's Execute publication and that the sequence drains
to IDLE without literal refetch. Load publishes at clock 7 and SUB at
clock `9 + gap` in this model. These fixtures do not validate silicon
source-read phases, mixed status producers, wider producer conflicts or
delayed-control overlap.

Immediate AND/OR/XOR retain unsigned literals through the same two-word
Decode path and leave both status registers unchanged, independently of
SXM/OVM (SPRU172C instruction contracts). Each operation has all four
source/destination combinations, shifts 0/1/7/15, positive/guard-negative
sources and both partial/complete-Decode replay points: 768 cases total.
Changing the literal after Fetch must not alter the result or cause another
program read. The other accumulator is preserved and guard bits are not
arithmetically saturated. Special shift-16 forms, mixed status producers
and delayed-control overlap remain unvalidated.

Direct DST retains the saturated 32-bit source after older Execute writes,
then writes the high word and XOR-1 low word in two clocks (SPRU172C page
4-47, class 13A). The pending second write holds pipeline advancement and
is saved explicitly; interrupt acceptance is reconsidered only after the
pair completes. All 192 cases cover A/B, SST on/off, in-range/guard-overflow
signs, odd/even addresses, DP/CPL and restore before/after source capture
or between writes. Changing the source after capture must not alter either
write. The gate verifies partial memory state, one write per word and
unchanged status/SP. Exact write-phase placement, interrupt overlap and
preceding SST-producer latency remain unvalidated; indirect, extended and
register/peripheral pairs are rejected.

Direct LDM retains a page-zero register/RAM operand in Read and zero-extends
it into A/B at Execute, independently of DP, CPL and SXM (SPRU172C page
4-73, class 3A). The existing peripheral-read wait applies at addresses
`20..5f`. Nine representative sources cover status, accumulator parts, AR1,
SP, XPC, negative-valued RAM and PRD. Both destinations, program pages,
SXM/CPL and pre/post-Read replay give 288 cases, requiring one read and no
status/SP or other-accumulator change. PRD has the modeled extra clock;
post-Read RAM changes must not affect the result. Indirect forms, preceding
MMR producer conflicts and physical wait placement remain unvalidated.

Staged BCD supports AEQ/ANEQ/BEQ/BNEQ. The complete two-word slot retains
its 40-bit zero predicate in Read (the opcode's Evaluate clock), after older
Execute writes, and uses that predicate at redirect. One empty prefetch slot
models SPRU131G example 7-17; both delay words execute on either outcome.
Taken branches discard the next sequential word. Ninety-six cases cover all
four conditions, both program pages, both outcomes and three checkpoints;
changing the selected accumulator after sampling must not change the route.
Forty-eight cases compose a memory-to-DP delay slot and NOP, verifying its
single source sample, held clocks and the target load through the new DP.
Target Fetch/Execute is clock 8/12 with immediate-load delays, or 10/14 with
the memory-DP delay. These are prototype stage observations, not silicon
measurements. Nondelayed BC, other condition families, interrupt overlap and
general multiword delay instructions remain unvalidated.

The exact original R060 AA22 skip-worker path composes 37 words:
`02:3d73..3d8b` and `02:3d9a..3da5`. It saves status/accumulator context,
increments a long-memory value, computes the status mask, executes BCD with
the real memory-DP/NOP delays, then restores context and executes RET through
the common tail.
Six executable cases cover pages 0 and 2 with checkpoints after the first
retained source, a partial accumulator save or the POPM BL/store overlap.
Each requires eight ordered context saves and one reserved-slot write,
restored A/B/ST0/ST1, final SP `1000`, the incremented long-memory value and
return slot `0fff=4044`, followed by one fetch at `4044` in the same program
page. The store uses the pre-POPM SP sample, not the live
incremented SP. Replay must not duplicate writes. The bytes were checked
through the original upload decoder against `InitData_R060.a00`; this
CPU-conformance fixture runs the copied path through the original `fc00`,
then executes NOPs at the reserved continuation, leaving the original image
and firmware runs unchanged. This establishes the skip-worker routine path,
not its real interrupt-entry integration or live recorder completion. The
worker-call path is not executed by this fixture.
Native handset/recorder execution still uses the atomic core.

The read-only `mu4_context_contract.py` pins 51 original AA22 words through
`02:3da5` against the R060 source identity. `487e; f030 c004; f050 c004`
leaves A equal to `(status[007e] & c004) XOR c004`. Therefore BCD ANEQ
skips the worker exactly when one of bits 15, 14 or 2 is absent. Exhaustive
predicate coverage gives 57,344 skip values and 8,192 worker-call values;
this is not an observation of the live recorder's status or branch choice.

The branch always has delay words `467f` and `f495`: LD from page-zero
`007f` into DP (three documented base cycles) and NOP. BCD itself has three
base cycles; SPRU131G example 7-17 explicitly shows conditional evaluation
and a prefetch bubble. Reusing unconditional BD's Access-stage redirect
without that distinction is not justified. The untaken branch additionally
saves AR0/AR2/AR3/T, reaches CALLD `02:3dbc`, executes `770e 0006` (STM #6,T)
as its two-word call delay, with return continuation `02:3d96`. The skip
path and returning-worker continuation reach `02:3d9a`, where a current-DP
load precedes CPL setup and context pops.
The skip-worker path's DP comes from the branch delay; the worker's effects
on DP remain unclassified. The pinned `POPM BL -> STL A,6` pair remains
the known compiler-mode SP-latency boundary. The final `fc00` is pinned,
not a claim of correct interrupt-return semantics or full handler execution.

Reproduce the bounded original-code audit with:
`python3 tools/mu4_context_contract.py roms/5510-mu4-reference/InitData_R060.a00`.
Its unit tests reject missing or changed witness words and enumerate the
complete status predicate. CALL/CALLD round trips and the original worker's
setup MVDK are supported in isolation; the callee instruction stream remains
incomplete. Establish that and interrupt integration before a full
original-handler fixture; neither skipping
delay effects nor a fixed interrupt delay is a substitute.

The original four short/long-period, direct/indirect-mask streams run through
this staged subset without an unsupported-opcode stop. Each records one
foreground fetch, one mask write and marker 1, with SP back at `1000`, but
zero vector entries. The timer is not silent: TCR reload occurs at clock 15,
and short-period IFR is `0008` by the mask at clock 19 (indirect) or 18
(direct). In this dense stream, incomplete younger multiword Decode defers
acceptance until the older mask can retire. That interpretation still needs
silicon validation; these cases alone do not test ISR execution.

Four controls insert four explicit NOPs after the reload, leaving timer
periods and hardware state unchanged. The short-period direct control enters
the adjacent PSHM/FRETE ISR once, completes the mask/marker and restores SP
to `1000`. The short-period indirect control instead records 143 vector
entries, one foreground fetch and no mask/marker, with a live ISR at the
observation deadline. Both long-period controls complete without an entry.
The gate requires actual ISR entry for short gap controls and successful
direct-mask stack restoration; the indirect result remains an observation,
not a passing completion claim. It demonstrates that the staged frontend
does not generally solve short-period starvation.

Staged taps observe program bus reads without the atomic backend's
PC-at-fetch guard. Reload/mask/final observations include PRD, TIM, TCR,
IMR, IFR and ST1; they separate a live timer from deferred acceptance.
Evidence: `tmp/c54x-staged-timer-audit.log`. The atomic probe still stalls
for short PRD and completes for long PRD; its behavior is unchanged.

The capped continuation trace distinguishes loss from repeated entry. In the
short-period indirect gap control, the first entry stacks PC `060e` at clock
22; FRETE samples XPC `0001` and PC `060e` at clocks 25/26. The mask opcode
at `01:060e` is fetched at 28, then again at 37/46/55 as successive entries
replace Decode before the literal arrives. The continuation is preserved,
but immediate re-entry prevents its two-word Decode completing. In the
direct control, the already-decoded absolute mask drains during entry at
clock 22; PC `060f` is stacked at 23, returned through reads 26/27, then
fetched at 29 before the marker completes. This is implementation evidence
for retained drain versus pre-Decode replacement, not a silicon proof or
justification for an added interrupt guard. Evidence:
`tmp/c54x-staged-continuation.log`.

Next validate far-return re-entry timing against independent pipeline evidence;
repeat/delayed-control and idle rules remain separate completeness work.
Those contracts must precede enabling
the frontend for native firmware or claiming the timer/recorder stall fixed.
A constant interrupt delay or adjusted timer period cannot replace in-flight
instruction ownership. The legacy vendor activation failure limits the
optional differential oracle; it does not block this software implementation.
Native acquisition, recorder finalization and handset speech remain unpromoted
until their own end-to-end gates pass.

The passive foreground-fetch tap in `probe-c54x-timer-boundary` narrows that
implementation boundary: both period-one cases accept 108 entries with
return PC `0608`, but never fetch the opcode at `010608` and never write the
IMR mask. Long-period controls fetch that opcode exactly once. Indirect
addressing fetches it two scheduler cycles after the reload and writes IMR
four cycles after reload; direct addressing fetches and writes IMR two cycles
after reload. Evidence: `tmp/c54x-foreground-fetch.log`. Thus the starvation
is upstream of operand access or instruction retirement, not a fetched mask
instruction whose store was lost. The staged controls above retain in-flight
instruction ownership and expose a different result according to whether
the mask was decoded before entry. A late write queue alone cannot model
that distinction. Neither observation proves that overloaded short-period
foreground starvation is itself a silicon defect. These are scheduler-accounting
observations, not measured silicon latencies or permission to insert a fixed
two-cycle interrupt delay.

This is bounded debugger coverage, not a complete ISA disassembler or new
execution/timing evidence. Other forms remain unsupported raw words.
Non-delayed calls and returns carry debugger stepping flags. Delayed calls
and returns deliberately omit those flags: C54x delay slots count words,
whereas MAME's extra-step count counts instructions, so blindly requesting
two extra instructions would be wrong for a two-word slot instruction.
Encodings and delay-slot rules follow the
[TI C54x instruction reference](https://www.ti.com/lit/ug/spru172c/spru172c.pdf).

`tools/c54x_opcode_coverage.py` compares `[opcov]` records from a verbose
30-second 5110 v5.30 run with the standalone `tms54test` fixture. The current
fingerprint and assertion totals are defined only in **Current audit boundary**
above; `--require-all-asserted` rejects executed-only and absent words.
The instruction contracts below retain their value/status/addressing evidence,
not a separate coverage baseline. Exact `5086` checks
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
The fixture asserts the current observed set plus fixture-only words. This
closes exact-word assertion coverage for that captured run, not every operand,
status or hardware interaction those instructions can encounter.
The generated report gives the current fixture-only total; these are *word*
counts, not instruction-family counts. Fixture execution alone is not
proof that a particular result is asserted, and this one boot is not a census
of every possible ROM4 path.
The coverage reader rejects malformed execution, assertion, decoder and
extension records rather than silently dropping them from the measured set;
unrelated emulator output remains permitted.
`make check-c54x-observed-coverage` regenerates the fixture and handset logs and fails if
the ROM4 idle word set changes or any observed word lacks an assertion. It is
also part of `make check-c54x-cross-rom`; the gate does not validate unobserved
instruction variants or silicon-level timing. The tool is given the current
core source for a separate static decoder inventory.
All four MAME invocations disable host joystick and MIDI providers: these
headless scenarios use scripted emulated keypad inputs, not physical host
devices. This avoids irrelevant host-device initialization without changing
handset inputs or CPU/peripheral semantics.
Its core-fixture leg uses the standalone runner's five-emulated-second stop
bound and `check_result` contract: every required conformance marker, the
intentional completion exit, and the terminal completion message are mandatory.
A coverage capture keeps verbose mode to emit opcode records; only its exact
two-line SDL teardown suffix is permitted after completion.
A timed normal exit is not acceptance. Failed coverage runs retain their
isolated directory and print its location; successful runs remove scratch
artifacts. The stop bound is a harness budget, not a DSP timing claim.
`make check-c54x-cross-rom` combines
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

The eight memory/T-source families MPY, MPYR, MPYU, SQUR, MAC, MACR, MAS and
MASR now share the staged frontend's fail-closed admission for unvalidated
absolute aliases `f9..ff`. Before the correction, the first atomic exclusion
case consumed an operand and changed A instead of stopping. The atomic
guard now precedes the data read and SQUR's T publication; admitted `f8`
absolute addressing is unchanged. This is an emulator-supported-domain
boundary, not a claim that physical silicon traps on those aliases, and it
does not close operand admission for every other Smem family.

`run_c54x_core.py --profile mem-t-abs-edge` executes 1792 cases and 1792
save/load replays: seven aliases, eight families, two accumulators, CMPT
off/on, two program pages, atomic/staged frontends and snapshots before/after
rejection. It asserts no operand data reads/writes, unchanged A/B/T/status
and every AR, and rejected execution in both modes. The original failing
case 56 and fixed acceptance are retained separately under
`tmp/c54x-memory-t-alias-before` and `tmp/c54x-memory-t-alias-after`.
The corrected core also passes the full default fixture (all 293 required
markers exactly once, unchanged 5690 asserted opcode forms) and the native
`nse1pipe` physical Phone book menu gate. This is regression evidence, not
completion of the separate MAD2 serial/RF contracts.

The six scalar load families LD, LDU, LD with TS, LDR, LD shifted by 16 and
DLD have the same bounded absolute-alias admission. The negative native
fixture failed at the first atomic case (56): an operand transaction and an
A change occurred without rejection. The atomic guard now rejects `f9..ff`
before those effects, matching staged admission without changing `f8`,
long-offset reads or store instructions. `--profile load-abs-edge` passes
1344 cases and 1344 save/load replays across six families, seven aliases,
both destinations, CMPT settings, program pages and frontends, with two
checkpoint positions. It checks zero operand transactions and preservation
of A/B/T, status and all ARs. Before/after evidence lives separately in
`tmp/c54x-load-alias-before` and `tmp/c54x-load-alias-after`. This is an
emulator-domain consistency correction, not a newly observed handset opcode
gap or proof that silicon traps on these aliases. Other read-source families
still require independent admission audits.

The corrected load core passes the complete default native conformance run
(`tmp/c54x-load-alias-default`) and the `nse1pipe` unassisted physical Phone
book menu gate. The final rebuilt fixture also repeats the 1344-case/replay
acceptance (`tmp/c54x-load-alias-final`). All 5196 tool tests pass with one
skip; the socket-dependent tests require the authorized localhost test
environment. These regressions do not close the separate serial/RF boundary.

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
These destination fixtures are additional arithmetic evidence, not a new
observed-word inventory. Their fixture-only classification and the current
idle/Menu/power inventory remain separate.
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

The coverage tool separates fixture execution from explicit result
assertions. An `opassert` marker follows a passing exact-word check;
execution alone is not assertion coverage. Neither establishes unexecuted
ROM4 paths, unobserved encodings, or the instruction-family audit. Existing result
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
not a physical baud rate. PLL stabilization and unsupported serial clock/frame
configurations remain outside this coverage.
Ten coupled CPU/DMA cases additionally cover all three IDLE modes with masked
and enabled completion interrupts while INTM remains set. The real DMA device
copies a four-word data-memory block after the CPU has entered idle; masked
completion leaves the foreground asleep, while enabled completion resumes it.
Each case saves before the first transfer, restores the pending device timer
and sleeping CPU, and reproduces the block and wake result. The fixture selects
the documented DMAC0/BRINT2 vector slot explicitly; it does not validate a
product's interrupt mux. Its 100-us observation window establishes completion
and ordering. An additional completion-time assertion checks sixteen DMA
device clocks for each four-word block, including every save-state replay,
against SPRU302B section 3.5's four-clock internal-word transfer. This does not
establish DA150 arbitration, external wait states, separate read/write bus
phases or a product's physical IDLE3 input-clock rate. The generic DMA now
receives a separate CPU PLL-stop output: IDLE3 selects configured, undivided
X2/CLKIN while IDLE1/2 retain the system clock, as specified in SPRU302B 3.8.
Two cases and their replays assert sixteen clocks at a deliberately different
3.25 MHz test input. Two additional controller cases leave X2/CLKIN absent,
check that an enabled transfer remains pending without modifying its destination,
and verify completion after restoring the system clock, including replay.
Two further cases interrupt the first DMA word after exactly two clocks,
retain its pending state through a 100-us clockless interval, and assert the
remaining two clocks rather than a restarted four-clock word, including replay.
Pending clock switches round up to the next whole transfer clock; sub-cycle
switching and PLL startup latency remain unvalidated. MU4 now supplies the
11.9952 MHz Y101 rate evidenced by its own schematic sheet 4 to this connection.
The original MU4 system-clock requests now drive delayed PLLSTATUS, CPU
execution and system-clocked peripheral rates. The MU4 schematic's `111`
straps select an explicitly C5416-compatible DIV/2 reset, and IDLE3
invalidates retained lock for software-managed PLL re-entry. Exact DA150
reset-table identity, automatic PLL-selected wake and sub-cycle switching
remain unvalidated; see the
[MU4 CLKMD contract](5510_bringup.md#original-clkmd-contract).
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
The separate 16-case CLKMD fixture exercises writes through original
`STM`, reads through `LDM` and captures through `STL`: once in PLL mode,
attempted changes to PLLMUL, PLLDIV, PLLCOUNT and PLLON/OFF preserve the
active configuration. PLLNDIV can still select DIV, after which new control
fields are accepted; software cannot assign PLLSTATUS. This follows
[SPRU131G table 8-18 and the mode-switch procedure](https://www.ti.com/lit/ug/spru131g/spru131g.pdf).
The old core fails the protected-field assertion (`tmp/c54x-clkmd-red.log`);
the corrected core passes all 16 settings and the full native suite
(`tmp/c54x-clkmd-green.log`). The native NSE-1 RF-boundary regression also
passes (`tmp/c54x-clkmd-rom4-rf.log`: 648,288 reads, 1,580 port-32 writes,
no port-38/39 reads). These tests establish register write eligibility,
not PLL lock latency, input-clock geometry or clock-rate switching. The
unconfigured core retains its declared immediate-status limitation. The
separate input-clock-configured fixtures now cover delayed status and selected
execution/peripheral rates, without asserting measured DA150 timing; see the
[MU4 clock contract](5510_bringup.md#original-clkmd-contract).
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
The executable rounded-multiplier boundary matrix covers twenty-six explicit
MPYR/MACR/MASR vectors across A/B destinations, `*AR2`/absolute operands,
and clear/pre-existing destination overflow: 208 cases. It checks fractional
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
also covers positive MACR and negative MASR crossing the signed 40-bit guard
boundary, with OVM clear/set: unsaturated results wrap to the accumulator
width, while OVM saturates the mathematical result before truncation. These
four additional vectors preserve opposite accumulator/status, carry, TC, T,
PMST and addressing state across both destinations and addressing modes.
It does not cover every addressing form or SMUL combination, cycle cost or
Nokia RF sample format.
An additional 192-case literal-result control matrix compares Smem,
dual-memory and parallel-store MAC/MAS, rounded and unrounded, across all
eight FRCT/OVM/SMUL combinations and both destinations. It uses
`8000h * 8000h` with -1/+1 accumulation to distinguish pre-product
saturation, checks rounding-owned overflow separately, and preserves the
opposite accumulator/overflow, carry/TC, control registers and operands.
Pointer updates and the parallel store are checked too. This extends the
existing rounded Smem boundary tests to the other two families; it is not
exhaustive operand or pipeline coverage and requires no arithmetic change.
Removing the OVM guard from product saturation makes this matrix fail;
the restored core passes. TI limits SMUL to particular C54x revisions, so
these generic-core fixtures do not prove its presence on every MAD2 DSP.
PMST.SST now saturates the shifted 40-bit accumulator value to 32 bits before
ordinary `STH`, `STL`, `STLM`, and `DST` stores, without modifying the
accumulator. Focused fixtures reproduce TI SPRU131G's signed `STH` and
unsigned `DST` examples and preserve their one- and two-cycle costs. The
conditional `SACCD` store is different: TI [SPRU172C, page 4-152](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)
specifies ASM/SXM, and [SPRU131G, table 4-3](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
excludes it from the SST instruction list. It therefore stores the shifted
accumulator lane without SST saturation; negative ASM shifts obey SXM.
The core previously saturated this lane and always sign-extended right
shifts. In addition to the 576 existing cases, 96 literal-result boundary
fixtures cover guard-bit overflow, ASM -16/-1/0/15, both SXM/SST settings,
both sources, false-condition write-back, preserved accumulators/status and
unconditional pointer update. They failed before the correction and pass
after it; the full core suite and isolated ROM4 FR encoder/decoder comparisons
also pass. These are instruction results, not physical pipeline timing,
native RF acquisition or handset speech evidence.
ASM `STL/STH`, long-immediate `STL/STH`, and parallel stores also obey SXM
when shifting right, before applying SST. Their previous handlers always
sign-extended. A separate 120-case literal-result fixture covers shifts
-16/-1, negative guard-bit inputs, both SXM/SST settings and both sources
across these five forms (parallel `ST||LD` is the fifth). It checks stored
lanes, preserved accumulators/status and address updates. The fixtures fail
against the old handlers and pass with SXM-aware right shifts. This is not
exhaustive parallel-store or pipeline coverage. The
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
TI SPRU307A's opcode table and the locally retained SPRU172C page 4-80
assign `LMS Xmem,Ymem` to `E1xx`, one word and one cycle.
Atomic fixture `e13a` checks TI's LMS
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
extension order and the absolute three-cycle cost. `629b` is a write-only
short-preincrement source and is now a rejection control, not a valid MPY
timing fixture. Long-offset MOD=13 remains read-capable; its retained
memory/immediate coverage is described above. Other addressing and multiplier
saturation variants require their own acceptance scope.
SPRU172C specifies A(32–16) as a signed **17-bit** multiplicand. The existing
`MPYA/MACA/MASA` Smem path had narrowed it to `s16`, reversing the product's
sign when bit 31 was set but bit 32 was clear. A shared 17-bit extraction now
serves those forms, FIRS, POLY, ABDST, SQDST, and accumulator-source SQUR.
The T-source `MPYA` (`f48c/f58c`) and `MACA[R]/MASA[R]`
(`f488..f78b`) forms are now decoded at TI's one-cycle cost. Fixtures
`f58c`, `3183`, and `f488` distinguish the 17-bit result from the old 16-bit
result; `f48b` checks rounded subtraction, while `e211` and `f48d` check
the shared high-word rule for SQDST and SQUR.
The SQDST B accumulation now uses the shared multiplier/accumulator overflow
path instead of an unchecked 40-bit assignment. Eight executable `e201`
fixtures pin old A-high=32768, FRCT=1, B=0/-1, OVM clear/set and pre-existing
OVB clear/set: B=0 produces `0x80000000` with sticky OVB, or `0x7fffffff`
under OVM; adding B=-1 produces the in-range `0x7fffffff` without clearing
an existing OVB. They preserve OVA, T and both operand addresses. The new
fixtures fail before this correction and pass afterward. The rule follows
[TI SPRU172C, SQDST, 4-160](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)
and the existing accumulator saturation contract.
The parallel difference now uses SXM-controlled memory extension and the
shared ALU subtraction path, rather than unconditional signed-16 narrowing
and a raw accumulator assignment. Thirty-two `e201` fixtures use four
high-bit X/Y pairs, SXM clear/set, OVM clear/set and pre-existing OVA
clear/set. Explicit signed/unsigned differences assert carry/borrow, sticky
OVA and both signs of saturation; B still receives the square of old A,
and its OVB remains unchanged. This follows SQDST's status list and
[TI SPRU131G, ST0/ST1, 4-3/4-5](https://www.ti.com/lit/ug/spru131g/spru131g.pdf).
The earlier signed-input fixture now explicitly sets SXM instead of inheriting
SXM=0 from its preceding multiply test; its expected signed value is unchanged.
These cases fix SMUL=0 and C16=0; SMUL saturation variants, C16 behavior,
physical pipeline timing and native speech remain unestablished.
FIRS now passes its two SXM-extended, shifted memory operands separately to
the ALU addition helper. Adding them first and passing `0 + sum` preserved
the result but lost carry from the original operands. Thirty-two executable
`e001` fixtures cover four high-bit X/Y pairs, SXM and OVM clear/set, and
pre-existing OVA clear/set. They assert carry, sticky OVA, signed saturation,
unchanged T and operand addresses, and B accumulation from old A. These
fixtures fail on the previous implementation and pass on the corrected core,
following [TI SPRU172C, FIRS, 4-59](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)
and the ST0/ST1 rules above. This selected coverage fixes FRCT=0, SMUL=0
and C16=0; it establishes neither physical pipeline timing nor RF acquisition.
ABDST has sixteen additional executable `e301` cases for old A-high=-65536,
B=`0x7fffffff`/-1, FRCT and OVM clear/set, and pre-existing OVB clear/set.
They verify the absolute signed-17 distance, its documented FRCT doubling,
B overflow/saturation and sticky OVB, while the parallel zero difference
preserves OVA, T and operand addresses and sets subtraction carry. All pass
without changing the implementation. The reference is
[TI SPRU172C, ABDST, 4-2](https://www.ti.com/lit/ug/spru172c/spru172c.pdf);
these cases fix SXM=1 and C16=0 and do not establish other addressing modes
or physical pipeline timing.
The core, bounded mixed RF filter, 5110
Menu/save-state and short/long physical power gates pass on the corrected
build. Remaining FRCT/OVM and source/destination combinations are not
established by these selected fixtures.
TI SPRU172C's `ADD/SUB src,ASM,dst` diagrams assign `f480..f780` and
`f481..f781` to the one-word, one-cycle accumulator forms. They were absent
from the decoder despite the corresponding immediate-SHIFT forms already
working. The core now shifts the selected source by ST1.ASM with SXM-controlled
right fill, then performs the signed addition/subtraction into the selected
destination. Fixture-only `f480` checks negative ASM, sign fill and carry;
`f681` checks B-to-A subtraction, unchanged B and borrow. Additional selected
overflow and shift/fill combinations are covered below.
An additional exact `f480` vector checks ASM=3: A=1 becomes 9, B's full
40-bit sentinel remains unchanged, carry stays clear, and the instruction
retains its one-cycle cost. The native `tms54test` suite passes after this
extension (`run_c54x_asm_core_retry_20261009.log`). This asserts one positive
left-shift outcome, not all ASM values, saturation, Nokia RF behavior or
native speech. The suite deliberately exits via its completion exception
after printing `TMS320C54x core conformance: PASS`.
`check-c54x-core` uses `tools/run_c54x_core.py` to retain independent
`run_c54x_core_*` directories containing `console.log` and the opcode
assertions in `error.log`. Acceptance requires every current marker in
`run_c54x_core.REQUIRED`, exit status3 and the terminal test-completion
exception; a marker followed by an unrelated crash or interruption is not
success. Earlier smaller marker sets are not current full-core acceptance.
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
Systematic `f580/f581` (`ADD/SUB A,ASM,B`) fixtures cover all 32 signed
ASM values with SXM and OVM clear/set and initial OVA/OVB clear/set,
for 512 selected vectors. Source A is fixed
at -65537 and destination B at 17, so negative right shifts distinguish
sign fill from zero fill and positive shifts include 32-bit overflow.
Expected signed right-shift values use explicit division/rounding rather
than the host's negative shift behavior. Each vector checks unchanged A
and ST1, the guard-preserving or saturated B result, sticky overflow,
low-word carry/borrow and one-cycle cost. The reference is SPRU172C's
`ADD/SUB src,ASM[,dst]` contract and SPRU131G's ST0/ST1 arithmetic rules.
The dedicated family marker and whole-core conformance pass in
`tmp/c54x-asm-saturation-matrix.log`; no core implementation change is needed.
This closes selected shift/fill/saturation coverage for these operands and
encodings, not other accumulator selections, all operands, RF acquisition
or speech. Initial OVA and OVB are varied together, not independently.
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
