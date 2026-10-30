# C54x MU4 execution observations

Original-application CPU composition, bus and atomic negative-control maps. [C54x execution contracts](c54x_execution_contracts.md) owns instruction qualification and the complete regression checkpoint. These application observations do not promote handset or waveform acceptance.

## Absolute ADD from mapped SP

Absolute ADD `00f8/01f8 0018` now reads mapped SP, independent of CPL.
This is not SP-relative direct addressing: the extension is the absolute
register address, and the operand is captured once in Read. SPRU172C's
ADD single-memory syntax and SPRU131G section 7.1 define the data-operand
read; section 7.5.5's compiler-base and stack-operation latency categories
must not be applied to this operand merely because the register is SP.
The existing Read ordering captures it before a younger FRAME changes SP.
No phase, forwarding, loop or status implementation is otherwise changed.

`add-sp` covers 4,096 arithmetic/save-replay cases across four source words,
four accumulator initial values, SXM/OVM, A/B, SP/RAM, two pages, CPL
clear/set and four snapshots. Both the address literal and source are
changed after capture; independent arithmetic verifies result, carry,
overflow and saturation. Another 576 cases/restores place FRAME, PSHM,
POPM or STM before ADD with zero, one or three NOPs, positive/negative
SP values, both CPL states, pages and accumulators. An adjacent younger
FRAME and later SP replacement must not change ADD's captured input.
Twenty negative cases preserve rejection of IMR/IFR, status, repeat,
mapping and peripheral sources before operand reads or arithmetic effects.
The fixture fails against the old SP guard and passes with SP admitted.
It establishes this bounded staged contract, not physical bank timing,
all SP-producer hazards, interrupt composition or new delay-slot support.

MVMD also accepts BK as its direct register source, sharing the existing
retained register-to-AR/BK move path. The `mvmd-bk` profile has 432
cases/restores: isolated transfers and STLM A,BK followed by NOP/MVMD,
all eight AR destinations plus BK self-copy, four source words, two pages
and three snapshots. It verifies one source read and destination write
(plus the producer's one write), source changes after capture, exact
publication clocks and preservation of other registers/status. The existing
432 T/TRN cases remain controls through the same fixture. Immediate
STLM/MVMD DAGEN-write conflicts and control-delay composition are not
claimed by this acceptance surface.

A two-word long-offset/absolute STL/STH may occupy the complete two-word
control delay (SPRU172C B[D], BC[D], CALL[D], RET[D]). The decoder
counts both words and retains the literal as data, including literals
that resemble RC, INTR or BACC. It does not admit a store that starts in
the final one-word slot. `store-delay` passes 2,880 cases/restores over
BD, taken/untaken BCD, CALLD and RETD; both accumulators and low/high
stores; offset/preincrement/circular/absolute forms; six literals, two
pages and three snapshots. It checks exact word fetch/write counts,
retained addresses after pointer mutation, stack ownership and the selected
continuation. The sixteen `st-delay-edge` cases/restores reject partial
instructions without a data access or atomic fallback. RETD retains its
early-prefetched words until the stack Read completes (SPRU131G example
7-8); BCD has its own extra cycle relative to BD/CALLD. These are retained
frontend projections, not physical timing measurements. Interrupt arrival
during a long delay store and far-control composition remain outside this
new fixture's acceptance scope.

MVMD non-accumulator sources and MVDK RAM sources may target T/TRN
through the retained move path. Source Access captures one word; the
move's Read publishes it without repeating the source access. Following
T consumers need no additional NOP (SPRU131G table 7-3/category 6 and
table 7-12). `move-t` passes 1,920 cases/restores over MVMD AR6 and
direct/short/offset/absolute MVDK, both destinations, four source values,
FRCT/OVM, gaps zero/one, two pages and three snapshots. It mutates the
source after capture and the destination after consumer sampling, checking
one move write and independent signed-MPY or mapped-LDM results, status
and saturation. Accumulator-sourced MVMD to T/TRN, other control-register
destinations, delayed-control and interrupt composition are not admitted
by this acceptance claim.

Signed memory MPY A/B may occupy one delay word, or both words when it
has an offset/absolute extension. The same full-instruction budget check
rejects a two-word MPY starting in the final slot. `mpy-delay` passes
18,432 cases/restores over BD, taken/untaken BCD, CALLD, cross-page FCALLD
and RETD; short, absolute RAM, absolute AR7 and offset sources; both
accumulators; sixteen operand pairs, FRCT/OVM, two caller pages and three
snapshots. The fixture changes the pointer after Access, the source/T
after Read and the literal after Fetch. The selected callee copies the
product, verifying availability at the boundary as well as independent
40-bit arithmetic, overflow/saturation, frame/page ownership and one source
read. Sixteen `mpy-delay-edge` cases reject straddling instructions without
data accesses or atomic fallback. The signed MAC/MAS extension is covered
separately in [Signed MAC/MAS in control delay words](c54x_execution_contracts.md#signed-macmas-in-control-delay-words).
Rounded/unsigned MPY and immediate-multiply delay compositions remain excluded. Exact stages remain
manual-derived frontend projections, not silicon measurements.

## Atomic Metadata Negative Control

The retained atomic `bootrecsave` diagnostic schedules a resident status poll
after the native return, then parameter `7` with filename `TEST` and empty
artist/title strings. Its checksummed candidate packets are
`1e 02 aa 01 49 04 fa 55` and
`1e 0a aa 01 36 07 04 54 45 53 54 00 00 05 99 55`.
That observation failed before the metadata packet was sent: the resident poll remained
incomplete at the 20-second endpoint. The retained replay below reaches cursor
28 with RX-ready set, three replies and two subsequent ACKs. Do not interpret
this as a failed metadata command.

Endpoint PC is `02:3d73` in the TINT handler; the live
OVLY-folded vector and handler prologue match the original AA22 upload.
PMST is `202c`, ST1 `6900`, IMR `0ac9`, IFR `0c78`. Debugger-visible
timer registers read TIM `0000`, PRD `0001`, TCR `0000`, giving the current
model a two-CPU-cycle timer period. The original AA22 idle routine at
`02:98d0`, called by the wrapper at `02:3d37`, writes PRD `1` at
`02:98d8` and TCR `0020` at `02:98dc`. It then clears IMR bit 3 with
`ANDM #fff7` at `02:98e0`, enables other wake sources with `ORM #08c0`
at `02:98e4`, and executes IDLE1 at `02:98e9`. Thus the short timer is
firmware-owned and is immediately followed by an intentional TINT mask;
it is not an unexplained final configuration to replace with a slower rate.
The same routine first reads `a531` with absolute LD `10f8 a531` and
branches directly to IDLE1 when that word is nonzero. Only the zero path
performs the setup above, then writes `a531=1` before sleeping. The
read-only context tool pins the entire routine and its caller wrapper,
including an exhaustive local zero/nonzero predicate. Other writers or
resets of that flag are not yet a closed census.

A read-only write/interrupt trace before the INTM guard correction observes
this warm setup at 14.001318 seconds, then repeated TINT entry with return
PC `02:98de`, IMR `0ac9`
and IFR `0c78`. The CPU never reaches the following mask instruction.
This establishes interrupt starvation before masking, rather than a
metadata-parser rejection. The temporary CPU probes are retired.

[TI SPRU131G](https://www.ti.com/lit/ug/spru131g/spru131g.pdf), sections
6.10.7 and 7.2, requires already-decoded instructions to drain before
interrupt-vector execution. The current atomic-instruction core instead
enters immediately at an instruction boundary. This is an evidenced model
gap, but its exact correction still requires a standalone timing fixture:
do not simply defer an arbitrary number of instructions, change the timer
divisor, or suppress interrupts at firmware PCs. Section 8.4.2 specifies
the timer rate as CPU clock divided by `(TDDR+1)*(PRD+1)`, consistent with
the short period. The staged frontend now has executable original-vector,
handler, worker, FRETE and recurring-interrupt coverage in
[the CPU contract](rom4_dsp_loader.md). The native warm profile still uses
the atomic frontend. The staged CPU fixture now composes all 44 original
idle/wrapper words with the 94-word timer vector/context route. With zero
flag and TINT enabled, decoded-instruction drain completes ANDM before
vector execution: the stacked continuation is `98e4`, not atomic `98de`.
After one ISR the original ORM/flag writes complete and the CPU sleeps with
TINT masked. Even/odd stacks and in-flight save/replay reproduce this bounded
result without a post-return delay or timer change. This bounded fixture
does not establish complete surrounding opcode coverage, native warm
execution or the atomic metadata acceptance behavior described below.
The staged execution stop is maintained in [Current boundary](5510_bringup.md#current-boundary).

`mu4nand -bios bootrecstage` is a separate fail-closed staged coverage
diagnostic using the same original upload, retained NAND and provisional
shared-RAM composition as `bootrecsave`. It enables the staged frontend
before reset-vector execution, without firmware-PC-conditioned switching or
atomic fallback. Existing profiles remain atomic. Reset's STM immediate to
PMST and the original RPT/MVPD copy now have staged conformance coverage.
The serial entry at `00:0e41` is reached. Its original STM/ADDM/ANDM prefix
selects aligned SP=`15fe`, with bounded staged RMW replay coverage.
Immediate ARP setup, READA into AR1 and BANZ also have staged consumer/
replay coverage. The original BANZ takes the loop to `00:0e5b`; its
`47f8 0011` (RPT *(0011), count from AR1) and repeated `7e92` (READA *AR2+)
now execute in the staged diagnostic, as does `00f8 0011` (ADD *(0011),A).
Startup reaches the original `00:090f` routine and executes its compiler
direct stack-frame decrement. It selects the modeled 95.9616 MHz PLL/system
clock, still with zero NAND reads. OR *AR1,A at `00:0936` now reads GPIO
address `003c` through the existing bench model with the staged peripheral
access clock. `75f8 0008 0080` at `00:093b` is PORTW *(0008),0080 (AL to
external I/O), not a data-to-data move. Compiler direct `7600 0001` at
`00:0940` now has staged ownership/replay coverage, and the original PORTW
retires: the diagnostic sink records port `0080`, value `00df`. This is an
observed I/O write, not a modeled external-device response. Direct STLM to
AR0-AR7 now has staged saturation, consumer-latency and save/replay coverage,
including `8811` at `00:3092` (STLM A,AR1). Long-offset and absolute DST now
retain their address and saturated pair, with 3072 save/replay cases covering
ordinary offset, preincrement and circular addressing. Original `4ee1` at
`00:309b` is accepted. Accumulator AND/OR/XOR now have staged coverage over
all encoded shifts and source/destination combinations; original `f180`
at `00:2fed` (AND A,0,B) is accepted. CMPL also has staged source/destination
and save/replay coverage, including original `f493` at `00:2fef` (CMPL A,A).
Compiler direct MVKD `7001` at `00:30b5` now has retained source/address
and replay coverage, including its AR1 source. Original SFTA A,-1,A
(`f47f`) at `00:323f` is accepted, with staged
SFTA/SFTL result/status and replay coverage. SFTA left/zero-shift carry
remains an explicitly unvalidated policy because the TI formula/example
disagree. Long-offset DSUB `54e6` at `00:3244` now has staged paired-read,
arithmetic and replay coverage. Long-offset immediate ST `76e6` at
`00:322e` now owns its offset/value words and cached destination.
BIO-conditioned BC `f803` at `00:2fca` now samples the bench's existing
NAND R/B attachment without overriding its level. MVMM SP,AR5 (`e785`) at
`00:2fbe` now has Decode-source/Access-publication and source/consumer
latency coverage. Short RPT #32 (`ec20`) at `00:2fc1` now executes its
retained NOP delay loop. The staged diagnostic observes the preceding NAND
strobe: byte `00`, HD0/CLE asserted, HD2/CE low, direction `7`, ready high.
That write observation alone does not prove NAND data-read or startup completion.
`9634` at `00:2fc3` now executes as `BIT *AR5,4`: memory bit 11 is
published to ST0.TC, with no pointer modification (SPRU172C 4-21).
The following two NOPs provide the tested visibility window for `XC 1,NTC`.
The original run retires five NAND strobes: command byte `00` with CLE high,
then address bytes `00,01,00,00` with ALE high; CE remains low and direction
is `7`. The write sequence alone is not evidence of a completed NAND read.
PORTR `74f8` at `00:302f` now reads port `4000` into AL. The bounded
device-boundary observer records the first 512 bytes. The prefix is
`fa33c08ed0bc007c8bf45007501ffbfc`, with CLE/ALE/CE low and ready high;
all 512 captured bytes compare with retained NAND row 1 at raw offset `528`,
with SHA-256 `7876777bc75f8e33789d0b917ac2ca71debc977170e6bc6b0f2dcf9b2e2c9310`.
Two further observed commands `00` with address
bytes `00,20,00,00` select row 32. Only the first 512 bytes are captured
and verified. The first-page comparison
verifies device-read data, not
the firmware buffer, spare-area handling or startup.
`80e6` at `00:3269` now has staged long-offset
STL conformance. Long-offset MVDK `71e6` at `00:326b` now has source,
DAGEN-publication and replay coverage, including a coherent fixture of the
preceding STL and immediate AR consumer. `BC NTC` (`f820`) at `00:3270`
now has retained-predicate and producer/replay coverage. Counter ADDM
`6be6` at `00:3277` has staged long-offset RMW conformance.
RPTB `f072` at `00:32ee` now executes. The current one-second original
bootstrap observation constructs `MCUSI16 .BIN` at descriptor `36b0`,
with length words `36c2=0001`, `36c3=e1e8`, matching the atomic reference.
NEG `f484` at `00:0e35` now executes with one-word Execute-owned
semantics. The pre-correction one-second observation enters loader `3538` with
index zero after 4,612 NAND reads, correct name/extension and length, then
reads the first file-backed block count `0010`. `SUB *(0008),B`
(`09f8 0008` at `00:2f46`) now consumes mapped AL and the run reaches
the original DMA-completion polling sequence. `BITF *(0008),0001`
(`61f8 0008 0001` at `00:2fa5`) now reads its mapped AL source after
the preceding DMPREC load. That observation reaches resident `02:6d62` and stops
on unsupported `STM #lk,IMR` (`7700` at `02:6d64`, live PC `026d66`).
That pre-correction entry was not a correct-transfer acceptance: a
side-effect-free comparison at the first resident Fetch checked all
48,261 program words from the original `aa22` section stream and found
40,657 mismatches. The first was `02:8e8b`, actual `826c`, expected
`0001`. The source has
51,921 words in total; 3,660 data words are explicitly excluded from
this program-only observation. The configured common-window mapping
is used, not asserted as physical DA150 bank identity. A separate fresh
atomic `bootrecsave` run with the same retained NAND and shared-memory
profile matches all 48,261 program words, with zero mismatches. Therefore
the discrepancy is staged execution/loading, not a mismatch shared by
the reference mapping. First divergence is word 11 of source record 434
(destination `02:8e80`, 16 words), file byte offset `4000`. The first five
bad words, byte-swapped, match the original sequence at `3fda`, exactly
one 38-byte record earlier. This is a boundary/stale-data witness, not
proof of a specific cache or byte-swap defect. Bounded write observations
show that the crossing record receives only 11 new words at `3c21..3c2b`,
then the original swap and DMA process all 16 words, including five stale
words. DMA source ownership matches the original buffer. The atomic
reader advances descriptor cluster fields `36c4/36c6` from 3 to 4 and
fills the remaining five words; staged resets cursor `36c5` but leaves
both cluster fields at 3. The initial cluster comparison reads 3 and 3
in both engines. The staged next-cluster helper `343a` subsequently
returns `0fff`, whereas atomic returns 4. Earlier fetch snapshots in
offset helper `345c`, at `3482`, already differ (atomic A=2, staged A=0).
An actual mapped-AL read tap now isolates the difference: `MPY #3,A`
(`f066 0003`, `3479..347a`) is immediately followed by `LDM AL,A`
(`4808`, `347b`) and `SFTA A,-2,A` (`f47e`, `347c`). Atomic reads AL=9;
staged reads AL=3, the pre-multiply value, giving offset 0 instead of 2.
SPRU131G table 7-28 explicitly assigns zero required gap to a two-word
direct accumulator producer followed by a mapped accumulator consumer.
The CPU now finishes a matching mapped accumulator Read after the older
two-word immediate MPY Execute, retaining exactly one operand sample.
Other reads and the one-word producer restriction are unchanged.
`tms54test -bios mpy-mapped` passes 3,072 cases and saved-state replays
over AL/AH/AG, BL/BH/BG, both producer/consumer accumulators, signed
operand patterns, FRCT/OVM, and zero/one-word gaps. It rejects the old
ordering, poisons T after its capture and the producer accumulator after
the consumer Read, and requires one mapped read and one literal fetch.
Long-offset `MVKD` (`70e1 0016 0012`, `00:33aa`) now executes through
the same retained move path. It copies the recovered next-cluster value
from mapped AR2 (`0012`) into `*AR1(0016)` (`36c6`) without injecting a
descriptor field. Its two extensions retain offset-before-source order;
Access owns the resolved destination and any pointer modification, Read
owns one source sample, and the existing write phase owns publication.
`tms54test -bios long-mvkd` passes 19,200 saved-state replays covering
25 forms, four offsets, selected-AR/AR2/RAM/alias sources, four values,
two compatibility states and two program pages. Snapshots cover partial
Decode, retained Read and completed write; later literal/source/pointer
mutations cannot retarget or duplicate the move (SPRU172C 4-113).
The full C54x conformance runner and native 5110 save-state gate pass.

The current one-second original bootstrap reaches resident `02:6d62`
with all 48,261 compared program words correct, zero mismatches. The
16-KiB crossing record now has all 16 correct words. A separate read-only
data audit checks all 3,656 ordinary data destinations, with zero
mismatches in both staged and atomic controls. Later program writes that
alias previously initialized data words retain record order under the
configured common-RAM profile; physical banking remains unvalidated.
Four low register-window writes at `007c..007f` (source values
`0000,0000,4000,017b`) are explicitly excluded: live register contents do
not prove their write effects. The inventory has 3,660 unique data
destinations (`007c..c281`).
`python3 tools/noki5510_bootstrap_check.py --resident-memory LOG` requires
one ordered program/data report, exact counts and zero mismatches, and
rejects duplicate or failed evidence. It accepts original RAM contents
only, not hardware register effects or full boot.
`STM #0,IMR` (`7700 0000`, `02:6d64`) now executes via the retained
non-DAGEN control-write path, with one literal fetch, no destination read
and one Execute publication. The isolated `imr-store` fixture passes
4,608 cases and saved-state replays over all 16 IRQ bits, four masks,
both initial masks/INTM states, no source/IRQ/NMI and two program pages.
It preserves IRQs accepted before a later clear, leaves masked IRQs
pending, and admits newly enabled IRQs only after publication. Physical
bus-phase timing remains a partial model (SPRU172C 4-176/class 12A;
SPRU131G 6.10.2).
The full C54x conformance run, native 5110 save-state gate and original
resident-memory checker pass. Fixture inputs return low before the next
test group so its IRQ/NMI edges remain independent.
The current original run retains both complete memory-audit passes and
advances through resident startup to `STM #7fff,*(0028)` (`7728 7fff`,
`02:6d05`, live PC `026d07`). Address `0028` is SWWSR, the software
wait-state register; the following `7729 8006` at `02:6d07` targets BSCR,
the bank-switching control register (SPRU131G 10.3.1/10.3.2). These
external-bus controls are the next contract to recover, not arbitrary
register storage to admit silently. Program/data RAM contents are
verified; low register effects, physical banking, full boot and recording
remain unaccepted.

### External-bus compatibility contract

`tools/mu4_external_bus_contract.py` pins the original resident prefix at
`02:6d00..6d08` against the R060 source hash. It decodes the two writes
under the existing C5416-compatible hypothesis, not exact DA150 identity.
The authority is [TI SPRS095P](https://www.ti.com/lit/ds/symlink/tms320vc5416.pdf),
sections 3.6.1-3.6.3 and tables 3-3/3-4/3-5.

| Register | Address | C5416-compatible reset | Writable fields |
| --- | --- | --- | --- |
| SWWSR | `0028` | `7fff` | XPA and five three-bit wait counts |
| BSCR | `0029` | `f000` | CONSEC, DIVFCT, IACKOFF, HBH, BH (`f006`) |
| SWCR | `002b` | `0000` | SWSM (`0001`); other bits unaffected by writes |

The CPU's opt-in `set_c5416_bus_profile` now owns these registers, their
masked/partial writes, reset values and saved state. It is enabled only
by the isolated `tms54test -bios bus-registers` configuration, not MU4 or
other handset profiles. `clkout_divisor_cb` publishes the exact divider;
the existing system-clock callback remains the separate base clock.
`iack_enable_cb` publishes output gating, not a physical IACK pulse, and
cannot suppress logical interrupt acknowledgement. Reset and post-load
re-drive these derived configuration outputs.
`run_c54x_core.py --bus-registers` requires exhaustive 65,536-encoding
native register conformance, partial writes, saved-state restoration,
reset and a logical IRQ with IACKOFF set. Its marker explicitly says
`bus_timing_acceptance=0`; it is not a bus-timing or full-core gate.
The staged SWWSR/BSCR instructions remain unsupported until wait payment
is integrated. The original resident frontier is therefore unchanged.
The isolated profile, full legacy CPU suite, native 5110 save-state gate
and original resident-memory checker pass on this register-state tree.

This BSCR dialect is **not** SPRU131G table 10-5's BNKCMP/PS-DS/IPIRQ
layout. In the C5416-compatible dialect, resident `8006` requests
consecutive external memory reads (three-cycle framing per read),
undivided CLKOUT, enabled IACK output and both bus holders. A generic
bank-compare interpretation of the same value would be the wrong model.
Resident SWWSR `7fff` selects seven wait states for every external range,
assuming SWCR remains at reset; SWSM doubles the count to fourteen.
XPA changes the program-field boundary from bit 15 to bit 22. Internal
program/data accesses do not pay these external waits. C5416 PORTR/PORTW
I/O uses the external IS bus; MU4 NAND is attached at I/O `4000..7fff`.

The tool's tests exhaust all 65,536 BSCR encodings and 655,360 SWWSR/
multiplier/range selections, plus internal/external ownership and the
original pinned source. These are field predictions, not runtime timing
acceptance. There is no complete SWCR-writer census, and neither these
values nor the test bench's flat RAM map establish physical bank extent.

Implementation requirements:

1. Select the C5416-compatible bus dialect explicitly; do not change
   other native handset profiles by importing its fields/reset values.
2. Own masked/resettable/saved register state in the CPU. Separate the
   physical IACK output from logical interrupt acknowledgement, and
   propagate CLKOUT division without changing the system clock itself.
   These are peripheral MMRs (SPRS095P table 3-16), unlike IMR: preserve
   the additional peripheral-access cycle in class-12 writes.
3. Pay external I/O waits while retaining one address/data transaction
   across stalls and replay; clocks continue and interrupt entry cannot
   split that transaction. READY is only sampled after software waits,
   with at least two software waits programmed (SPRS095P section 5.5.5).
4. Add program/data framing and bank penalties only with explicit bus
   ownership. Physical DA150 banks, READY wiring and electrical bus-holder
   behavior remain separate unknowns, not reasons to fake register reads.

The staged frontend now pays the C5416-compatible external-I/O wait
component before a PORTR read or PORTW write. It freezes the programmed
count and READY eligibility at transaction start, retains the existing
instruction address/source slots, and samples READY only after at least
two software waits. Zero/one-wait requests ignore READY. While stalled,
clock consumers continue and new interrupt entry cannot split the
transfer; its single side effect occurs only when the waits finish.
The wait counter, eligibility and per-slot payment flags are saved.
The complete-pipeline stall placement is a declared partial model, not
a recovered silicon pipeline waveform. Simultaneous read/write I/O bus
arbitration remains fail-closed rather than guessed.

`run_c54x_core.py --bus-io` requires 576 cases and saved-state replays:
eight base counts, both SWSM values, both directions, READY held/released,
no source/IRQ/NMI and three checkpoint positions. It checks single I/O
side effects, retained literal/address/data despite later changes, timer
continuity and acknowledgement after the transfer. Input pulses settle
before same-transaction snapshots; queued scheduler events are not a
pending CPU interrupt. The original source and handset profiles remain
unchanged: the profile is selected only by isolated conformance and the
fail-closed MU4 `bootrecstage` original-firmware diagnostic.

Atomic execution now retains I/O transactions and retirement context.
`run_c54x_core.py --atomic-io` passes 2,880 transfer cases and 2,880
save/restores across short, long-offset, absolute, peripheral-MMR and
AR-alias addressing, with
the same software-wait, READY, IRQ/NMI and timer matrix. Extension taps
check both three-word literal orders and reject refetch after mutation.
Base transfer clocks are retained individually rather than charged as
upfront cycle debt. Selected repeat and delayed/guarded compositions
have their separate bounded fixture below. The next software target is
accumulator-to-IFR acknowledgement at the original MU4 frontier.
Program/data bus ownership, physical timing and
the remaining pin behavior remain open; neither register nor staged-I/O
conformance is full bus or boot acceptance.
Both isolated bus gates, full legacy CPU conformance, native 5110
save-state restoration and the unchanged original memory audit pass.

The atomic integration must defer retirement as well as the I/O callback.
`execute_run` currently applies repeat redirects, delayed-word accounting,
XC protection and INTM-guard consumption immediately after `execute_one`.
A retained PORTR/PORTW must therefore own the instruction PC, original
delay/XC eligibility, repeat-armed state and consumed word count until
completion. Resume a pending transaction before `service_interrupt` or
another fetch; retire exactly once after its single bus side effect.
Preserve the existing extension order: long-offset forms fetch the port
before the address extension, while short/absolute forms prepare the
address before fetching the port. PORTW also retains its sampled source;
PORTR retains its destination and must not read the port before READY.
Pointer modification must occur once, at a documented phase, rather
than being repeated on each resumed clock. All pending transaction and
retirement fields require save-state coverage. Acceptance must include
two/three-word forms, repeated transfers and delayed/guarded followers,
in addition to the staged gate's wait/READY/IRQ/NMI cases. Subtracting
extra cycles after an immediate callback does not satisfy this contract.

The isolated `bus-io-compose` fixture additionally tests 24 saved/replayed
cases: RPT counts 0..3 and read/write transfers, delayed-branch slots and
RSBX INTM followers, all with seven software waits. It checks one pointer
update per transfer, deferred IRQ acknowledgement and the stacked
sequential/delayed continuation. This is a bounded composition matrix,
not exhaustive support for every repeat/control form or peripheral MMR
operand. Full legacy CPU conformance and native 5110 save-state restoration
pass with the integration. The original MU4 audit still matches all
48,261 program and 3,656 ordinary data words; no handset profile has changed.
Peripheral acceptance uses the ordinary mapped register-window word
`0030`: both directions retain its additional access clock rather than
charging an upfront instruction debt. READY eligibility still depends
only on programmed external software waits, not the extra MMR clock.
The mapped AR1 alias uses `*AR1+` with AR1 initially `0011`; PORTR writes
the received word to AR1 before its one postincrement. Its postincrement
mode is saved until completion, while PORTW retains the source before
its pointer modification. This validates that alias and representative
peripheral timing, not every timer/control-register side effect or
measured silicon bus phase. Production profile promotion remains separate.

`run_c54x_core.py --bus-stm` accepts 36 cases and save/replays for immediate
SWWSR, BSCR and SWCR writes: four literals, three destinations and three
checkpoint positions. The literal is retained despite a later program
mutation, no destination read occurs, and one masked write publishes at
clock 8 rather than early DAGEN publication. The extra peripheral clock
uses the existing saved per-slot wait state. Admission is conditional on
the C5416-compatible profile; other STM controls remain excluded.

With that profile enabled only on `bootrecstage`, the original MU4 loader
still matches all audited resident program/data words and executes past
`7728 7fff` / `7729 8006`. READA `7ef8 000b` at `02:6d12` transfers the
program word at `02:74b5` into BL. Its six accumulator-word destinations
are now accepted by `--mapped-read-a`: 288 cases and save/replays across
direct/absolute forms, four values and two source pages. A changes after
capture without redirecting the held program read; one write preserves
untouched accumulator words and status. Repeated mapped destinations,
other controls and peripheral destinations remain excluded.

Absolute LD Smem,DP `46f8` now shares direct LD's retained sample and
zero-gap DP forwarding, with one retained address extension and correct
two-word retirement. `--dp-absolute` tests all 512 DP values across 1,536
saved/replayed cases: source Read at clock 6, DP Execute at 9 and adjacent
consumer Execute at 10. Later source/extension mutations cannot redirect
or resample the transfer. Other indirect memory-to-DP forms remain
excluded; the absolute fixture uses ordinary RAM, not peripheral input.

The original `46f8 007f` at `02:6d2b` executes. The retained-initialization
checkpoint passes full CPU conformance, native 5110 restoration, seven
isolated gates and 101 targeted Python tests. Physical program/data bus
ownership remains unvalidated; this is not full board boot or native speech.

The next MVDM contract is pinned by SPRU172C 4-110: a two-word,
two-cycle Class 19A transfer from absolute data address DAR to an MMR,
without ordinary status-bit changes. Its repeat stream increments DAR
and has a separate single-cycle steady-state contract; non-repeated
admission must not imply repeat acceptance. SPRU131G tables 7-7, 7-9
and 7-13 group MVDM with MVKD for AR, compiler-mode SP and T latency,
not with MVDK's earlier publication. The existing retained Class 19A
MVKD path is therefore the candidate implementation boundary. Required
tests are one owned source literal/read/write, source/destination aliasing,
saved pre-read/post-read publication, AR consumer gaps, and separate
compiler-mode SP/T consumers before admitting those destination classes.

Direct MVDM destinations AR0..AR7 now share that retained Class 19A path.
`run_c54x_core.py --profile mvdm-ar` requires 1,152 cases and save/replays:
eight registers, RAM/self/other-AR sources, four values, zero/one-gap CMPR
consumers, two execution pages and three snapshot positions. One absolute
source literal is captured; one source read and one encoded MMR write
occur at the complete slot's Read clock 6. DP does not select the MMR.
Later RAM/literal changes cannot alter the owned transfer. Repeat and
non-AR destination classes remain excluded.

The original `7212 b6be` at `02:df9b` executes. MVDM
`7211 b6a8` at `02:dfa6` occupies the two delay words following BCD
`fa47 dfbe` at `02:dfa4`. The diagnostic records `decode_words=2`,
`control=02dfa4`, `control_words=4` and the branch in Access; live PC is
`02:dfa3`, not the stopped instruction address. This is a real excluded
Class 19A delay-slot composition, not a wrong-literal or delay-owner
inference. `--profile mvdm-delay` now accepts 576 cases and save/replays:
eight AR destinations, four values, both outcomes of the original ALEQ
branch family, none/IRQ/NMI and snapshots before/after publication and
during entry. Branch and MVDM literals are owned separately, the transfer
publishes once before acknowledgement, and RETE resumes the selected
continuation with SP restored. Snapshots at clocks 8/9/12 now straddle
the observed source/publication boundary, rather than assuming clock 8
was after publication. Direct-AR MVDM and basic two-word MVKD are exempted
from the long-memory delay exclusion; extended forms remain excluded,
and this is not exhaustive acceptance of other control contexts.

That original delay slot executes. `fb82 dfc2`
at `02:dfaf` is FCALLD to `02:dfc2` in extended-program
mode. Its two delay words are NOP at `02:dfb1` and LDM AR6,A at
`02:dfb2`; the return continuation is `02:dfb3`. Its accepted contract
covers caller-fetched words and stacked PC/XPC ownership across those delays.
`--profile far-call-delay` accepts 243 cases and save/replays: three
caller and target pages, three delay layouts, none/IRQ/NMI and three
snapshot positions. PC after both delay words is stacked before the
retained caller page; FRET restores both and SP. RETE removes only the
separate interrupt frame. Layouts cover two one-word operations, one
two-word STM and the original NOP/LDM pair.

Mapped XPC visibility inside a delay-slot Read is unvalidated. The
primary instruction text (SPRU172C 4-57/4-58) specifies the stored frame
and fetched delay words, not that cycle-level read. A caller-2/target-0
probe observes XPC=0 because the current frontend publishes its speculative
Fetch page into the shared XPC field. The fixture retains this probe but
does not assert its value or change global publication to enforce a guess;
its acceptance marker explicitly excludes that fidelity claim.

The original call enters `02:dfc2`. Its `29f8 0011` at `02:dfc9` is
absolute MAC Smem,B reading mapped AR1, with T initialized to 3.
MAC/MACR/MAS/MASR now share arithmetic with the atomic interpreter while
retaining staged memory and T in Read. The accumulator is consumed at
Execute, consistent with the existing accumulator-feedback path.
`--profile mac-smem` tests 73,728 cases and save/replays: four operations,
both destinations, indirect/absolute RAM and mapped AR1, 64 signed input
pairs, 16 FRCT/OVM/SMUL/SXM combinations and three checkpoint positions.
Rounding, product saturation, sticky target overflow, the untouched
accumulator and absence of T/data republishing are checked. Later source,
T and extension changes cannot alter the sampled inputs. SQUR's staged
T side effect and repeated MAC forms remain excluded. Complete control-delay
MAC composition has separate acceptance above; other accumulator-producer
compositions need separate acceptance.

The original MAC executes. MVKD `7084 0013` at
`02:dfd8` copies AR3 to the RAM word addressed by AR4 in both delay
words of BCD `fa4d dfdd` at `02:dfd6`. The diagnostic records two remaining
decode words and that control owner; live PC is `02:dfd5`. Its accepted
contract is the two-word Class 19A data-copy delay, not its three-word
extended forms. `--profile mvkd-delay` accepts 1,728 cases and save/replays:
eight pointer registers, both outcomes of the original BEQ branch family,
AR sources, RAM-source postincrement and same-address RAM copying,
none/IRQ/NMI and snapshots before Read, after Read and during entry.
Read is observed at clock 9 and RAM publication at clock 10. Mutating the
source after sampling proves retention, including the same-address case;
one pointer update and complete RETE return are checked. The reused MVDM
gate still passes its 576 cases with the corrected snapshots.

The original MVKD delay executes. MVMD `7316 b6a9`
at `02:e078` copies encoded MMR AR6 to absolute data address `b6a9`.
Direct MVMD sources AR0..AR7 now share the established Class 19A MVDK
transfer path, but bypass DP/indirect source addressing. `--profile mvmd-ar`
accepts 576 cases and save/replays: eight registers, four values, RAM/
self-AR/other-AR destinations, two execution pages and snapshots before
Read, after Read and after publication. The source is sampled at clock 5;
RAM publishes at 6, DAGEN destinations at 5. Later source/extension
changes cannot alter the retained transfer. Status and both accumulators
are preserved. Repeat, indirect and non-AR source classes remain excluded;
SP/BK destination consumers and source-producer gaps need separate coverage.

The original MVMD executes and returns to the initialization spine. The
LDM IFR,A `4801` / STLM A,IFR `8801` pair at `02:6d3e..6d3f` now executes
after the interrupt-register acceptance below. Absolute CMPM `60f8`
at `02:909c` now executes with following words `3762 000c`.
SPRU172C page 4-33 identifies `60f8` as
absolute CMPM: the address extension `3762` precedes the comparison
constant `000c`. It reads a 16-bit source and sets TC on equality, without
a destination write. The absolute form adds one word and one cycle to
the two-word/two-cycle base instruction. A retained source/address/literal
fixture and adjacent TC-consumer coverage are supplied by the isolated
`cmpm-absolute` profile: 96 cases/restores cover equal/unequal sources,
both initial TC states and TC/NTC branches. The address and immediate
fetch exactly once in order; later program/source changes cannot alter
the retained comparison. Read occurs at clock 7, TC publishes at 8, and
a taken adjacent branch fetches its target at 10. Both accumulators and
the non-TC status bits are preserved. Mapped-register sources remain
excluded. The shared mapped-BITF gate still passes 13,824 cases/restores.

The original selects the high-rate PLL clock and executes MVKD
`7082 0010` at `02:8fbd`: source `0010` (AR0), destination `0049`
through AR2. MU4 maps `0048/0049` to the existing McBSP control index/data
pair. SPRU172C Class 19A (page 3-40) requires one additional cycle for a
peripheral memory-mapped access. Peripheral destinations now publish in
Execute after that extra clock, rather than using the early Read-stage
DAGEN-register write. The retained source is not read again during the
wait. Low control destinations remain excluded.

The `mvkd-peripheral` profile passes 648 cases/restores: AR0/BK/RAM sources,
indirect/postincrement/absolute destinations `003b/0048/0049`, four
values, two execution pages and three snapshots around Read/wait/write.
The source read is at clock 6 (7 for absolute), with publication at 8
(9 for absolute). Source and extension changes after capture cannot
alter the transfer; pointers modify once, status/accumulators remain
unchanged, and the destination is never read. These are CPU bus/replay
checks, not physical McBSP timing measurements or serial output proof.
BK reads use the same retained Read-owned source path. Later changes to
BK cannot alter the transfer; adjacent BK-producer timing is not covered
by this fixture and remains a separate acceptance requirement.

The original advances beyond the BK-source boundary and executes the
T store `8c82` at `02:8fed`. Peripheral T/TRN stores use the existing
Execute-owned source/publication path and pay the extra peripheral clock;
low CPU-register destinations remain excluded. The `t-peripheral` gate
passes 864 cases/restores with instruction-driven STLM producers, T/TRN
sources, gaps 0/1/2, indirect/postincrement addresses `003b/0048/0049`,
four values, two execution pages and snapshots before the producer,
during the wait and after publication. Producer writes at clock 6;
peripheral publication is at 8 plus the gap. No destination reads or
duplicate writes occur, and status/accumulators are preserved. This
tests the admitted short store composition, not every possible producer.

Original words `61e1 0001 0002` at `02:9521` now execute and test an
AR1-plus-one source with mask `0002`. The preceding `7711 0048` and
`7681 0001` select McBSP control register 1 through the `0048/0049`
index/data pair. The existing McBSP register-1 read exposes transmit
ready in bit 1. BITF now participates in the peripheral-read wait path.
The `bitf-peripheral` gate passes 384 cases/restores with long-offset and
absolute addressing, two execution pages, both initial TC states and
TC/NTC branches. It samples the source once at clock 8, publishes TC at
9 and fetches a taken adjacent branch target at 11. Changing peripheral
status after Read cannot change the result. No destination write occurs.
These clock positions describe the executable partial pipeline model,
not measured silicon bus phases.

The resident AA22 image emits six native McBSP TX words (`0c10 0818 0a01 0e53 1023
1201`). Its FCALA B `f5e7` at `02:40ef` executes in the resident run
after the frame, interrupt and producer gates below. SPRU172C
page 4-55 identifies this as nondelayed FCALA B: push PC+1 and caller
XPC, then load PC from B[15:0] and XPC from B[22:16], with no status
change. It is a one-word/six-cycle Class 30B instruction and cannot be
repeated or placed in a block repeat. Retained accumulator target,
caller-page/return-frame ownership, producer timing, interrupts and
save/replay require executable acceptance. Serial word output
alone does not prove board boot, recorder readiness or audio.
This is not the AA99 REL image's `6f8c 0c9e` at the same logical address;
source-image identity is part of every instruction claim.

FCALA/FCALAD are implemented by the atomic extended-transfer
decoder `(op & 0xfcfe) == 0xf4e6`; the disassembler recognizes that
family too. Its acceptance is independent of the staged fixtures.
The isolated `far-atomic` gate passes all 16 existing branch/call/return
encodings, including A/B accumulator calls and delayed forms, from caller
page 1 to target page 2. It verifies return frames, caller continuation,
delay-word execution and target retention. It explicitly claims no
staged acceptance, save/replay or silicon phase validation. Its setup
initializes PMST, timer and interrupt controls rather than inheriting the
full suite's preceding fixture state.

The nondelayed staged prototype owns one instruction word, PC+1, a
saved 23-bit accumulator target and the Class 30B six-cycle cadence.
The generic operand slots are 16-bit and cannot hold that full target.
A front-end barrier prevents younger sequential instructions from
entering while target and frame ownership settle. Caller XPC comes
from the retained instruction page, not target prefetch. The two stack
writes have distinct saved ownership. The isolated `fcala` profile
passes 216 cases/restores: both accumulators, caller pages 0/2/7f, target
pages 0/3/7f, ignored high accumulator bits, post-sample source changes
and four snapshots around target/frame publication, with no interrupt,
IRQ or independent NMI. Target sampling is
at clock 5; return PC and caller XPC publish at 5 and 6; target Prefetch
is at 7, Fetch at 8 and first callee Execute at 12. FRET restores SP and
the caller continuation, without duplicate frame accesses or changes
to the unselected accumulator or non-INTM status. IRQ/NMI arrival during
frame construction is acknowledged only after both call words publish.
Its stack word is above the complete call frame, RETE removes only that
word and resumes the target, and the callee executes once before FRET.
The interrupted target has one initial Fetch and one legitimate return
refetch. Interrupted snapshots allow the queued line pulse to settle
before saving. These are declared partial pipeline placements, not
measured silicon phases. The separate `fcala-producer` gate passes 216
cases/restores with DLD building an A/B target from two RAM words, gaps
0/1/2, nine caller/target page pairs and four snapshots. DLD samples
its pair at clock 5; later RAM changes cannot change its publication,
and post-FCALA-sample accumulator changes cannot change the call target.
Zero-gap FCALA samples after older DLD Execute publication in the same
clock. This validates that producer family, not every accumulator
producer or a combined DLD-plus-interrupt cross-product. The separate
[FCALAD contract](c54x_execution_contracts.md#delayed-far-accumulator-call) covers delayed calls but
does not inherit the nondelayed DLD-producer acceptance. The atomic,
FCALLD and IFR reference gates still pass.
SPRU172C page 4-55's
detailed operation pushes return PC then XPC; SPRU131G section 6.4.3's
short prose reverses their mention order and must not override the
instruction operation or the tested FRET frame layout.

CC `f945 3ff1` at `02:3f3c` now executes. Nondelayed AEQ/BEQ calls
retain the branch predicate and target, then pay Class 31B's additional
clock on both paths. Only a taken call allocates SP, publishes PC+2
and updates RTN. The `conditional-call` gate passes 64 cases/restores:
A/B predicates, four 40-bit patterns including guard-bit-only nonzero,
two pages and snapshots around decision/held-clock/publication. Later
literal and accumulator changes cannot change the retained outcome.
The taken frame writes at clock 7 and target Fetch follows at 8; RET
restores the caller. Untaken cases check unchanged SP on every clock,
with no stack reads or writes. Both cases preserve accumulators and
status. CCD, other condition groups, adjacent producer and IRQ/NMI
composition remain unaccepted. The atomic core's broader CC/CCD family
is unchanged.

Conditional return `fc45` at `02:3f41` and signed `fc43` at `02:3e44`
now execute. The `conditional-ret` gate passes 768 cases/restores for all
twelve nondelayed signed A/B predicates (GEQ, LT, NEQ, EQ, GT, LEQ),
eight 40-bit patterns, two pages and four snapshots. Sign is determined by
bit 39, including positive values with bit 31 set and nonzero guard words.
Read samples the predicate after older Execute publications, before a
conditional SP increment. Only a taken return reads
its captured stack address at clock 5; later stack-word changes cannot
change the return target. Untaken returns preserve SP on every clock
and never read or write data memory. Accumulators, non-INTM status and
INTM are preserved. Changing the selected accumulator after Read to an
opposite-predicate value does not change the decision. RCD, other condition
groups and IRQ/NMI composition remain unaccepted. Adjacent unsigned loads
have the separate `ret-load` composition coverage described above.

The isolated runner rejects MAME driver-validity `Errors:` output even
when the conformance marker and intentional completion exit are present.
BIOS identifiers must fit MAME's 16-character limit; a passing CPU
fixture cannot substitute for a valid driver definition.

SUBS `0a39` at `02:3f5a` now executes. SPRU172C page 4-194 specifies
an unsigned 16-bit memory operand regardless of SXM, with C and sticky
destination overflow updates and OVM saturation. The staged path reuses
retained memory/address ownership and the shared arithmetic helper but
bypasses sign extension for this family. The `subs` gate passes 52,224
cases/restores: 17 read-capable encoding forms, eight address registers, A/B,
eight value patterns, SXM/OVM states, both compatibility modes and three
snapshots. Post-capture source/literal/pointer changes cannot alter the
result, and no data write occurs. Mapped-register sources, C16 arithmetic
and adjacent producer/branch composition are not accepted by this gate.

MVMD AL/BL to RAM now has a separate `mvmd-acc` gate: 288 cases/restores
with POPM and mapped STL producers, gaps 0/1/2, four values, two pages
and three snapshots. Zero-gap mapped-source reads follow the older mapped
write's Execute publication in the same clock (SPRU131G section 7.5.10,
example 7-67's no-conflict rule). Direct-ALU producers retain their
separate latency rules; this change does not forward all accumulator
updates. The source reads once at clock 6 plus the gap, and RAM publishes
at 7 plus the gap. Later low-word changes cannot alter the captured move;
other accumulator/status and stack ownership are preserved. AH/AG/BH/BG,
non-RAM destinations and direct-ALU producer composition remain unaccepted.

The verified pre-status checkpoint stopped at the destination guard for MVMD AL
`7308 0007` at `02:3f62`, live PC `02:3f65`, directly after POPM AL
`8a08` at `02:3f61`. Retained instruction and destination observations
confirm ST1 (`0007`). The staged subset admits AL/BL to ST1 and
publishes the retained word at the move's Read clock, rather than Execute.
Its `mvmd-st1` fixture passes 288 cases/restores (POPM/STL producers,
zero through two gaps, two pages, four full-word patterns), with one source
read, one status write and no destination read. The `mvmd-cpl` bank in `mode-latency` retains
16 cases/restores: both CPL directions, basic LD/ST consumers and zero
through three NOPs, checking the two-cycle consumer latency.
These addressing-mode labels refer to banks in `mode-latency`; its
[fixture-validity limits](c54x_execution_contracts.md#harness-preservation-contract)
distinguish preserved model observations from valid-program conformance.

The adjacent arithmetic fixture `mvmd-status-alu` passes 768 cases/restores:
SXM-dependent indirect LD and ASM-dependent register LD, both SXM states,
all 32 ASM fields, two pages and three snapshot positions. The old status
fields deliberately differ; each case checks the independently computed
40-bit result, source preservation and one status publication. These tested
consumer forms have zero latency. The `mvmd-block` fixture passes 24
cases/restores with immediately adjacent RPTB, counts 0/1/2/7, two pages
and three snapshots. It checks the BRAF update survives status publication,
then checks every byte-swapped body word and count-plus-one completion.

Original `3f83` at `02:3df3` is `ADD *AR3,16,B,B`. The memory shift-16 subset executes
the Class 3 ADD/SUB family using retained addresses, extensions and source
words. Its separate `memory-shift16` fixture passes 208,896 cases/restores:
17 read-capable encoding forms, eight AR selectors, all eight operation/source/destination
selectors, eight arithmetic patterns, four SXM/OVM settings, both CMPT
states and three snapshots. The reference independently derives signed
40-bit results, sticky destination overflow and OVM saturation. ADD sets
C only when a carry occurs; SUB clears it only when a borrow occurs
(SPRU172C syntax 3, pages 4-5 and 4-188). Other cases retain incoming C.
RAM is read once; there are no data writes; mapped-register sources remain
excluded.

Unsigned `LDU *AR2,A` (`1282`) at `02:3dfa` reads source `0000` (IMR),
after `MVDK *AR2,AR2` at `02:3df8`. LDU now admits IMR via the ordinary
mapped read path, not a substituted zero. The `ldu-imr` gate passes 384
cases/restores across direct, absolute, indirect and adjacent MVDK-producer
forms, four actual IMR values, both SXM states, both accumulators, two pages
and three snapshots. Each case reads IMR once and retains the unsigned
result despite later IMR changes. The 96 producer cases check one RAM
read, one AR2 write and captured address ownership despite subsequent
pointer changes. All cases preserve status and the unrelated accumulator;
other newly untested unsigned control/peripheral sources remain excluded.
These fixtures establish instruction composition, not that the original
zero-pointer lifecycle is correct; that distinction remains explicit.

Short accumulator STL/STH now admits preincrement modes `98..9f` and
increments the selected AR before capturing its Read-owned destination.
The `preinc-store` gate passes 6,144 cases/restores across four STL/STH
selectors, eight registers, four source/address patterns, four SST/SXM
settings, both CMPT states, two pages and three snapshots. It checks one
pointer update, no destination read, one write, 16-bit wrapping into actual
IMR and Execute-live source saturation despite later pointer changes.
T/TRN preincrement forms remain excluded.

`STM #k,BRC` now publishes early enough for an adjacent RPTB, as required
by SPRU131G tables 7-23/7-24. It does not inherit DAGEN-bank arbitration.
The `stm-brc` gate passes 30 cases/restores with counts 0/1/2/7/127,
two pages and three snapshots, beginning with a deliberately different BRC.
It verifies one retained literal, no counter data read, one counter write
and complete count-plus-one byte-swap loops. In-loop BRC modification stays
excluded because table 7-25 requires a separate latency contract.

Short immediate preincrement ST passes 1,536 cases/restores in `preinc-st`:
eight registers, four literals, four SST/SXM settings, both CMPT states,
two pages and three snapshots. Program-literal and pointer changes after
capture cannot affect the one write; unrelated accumulator changes cannot
replace the retained literal. Single-instruction repeat of this new mode
and its interrupt cross-products remain unaccepted.

These changes execute the original `809a` at `02:3e1c`, then `771a 007f`
at `02:98a5` and `769a 0000` at `02:98b1` in the clearing loop.

Long-offset immediate ST now admits peripheral destinations `20..5f`,
while low CPU-control destinations remain excluded from that form. The
`st-long-periph` gate passes 6,144 cases/restores across eight AR selectors,
offset/preincrement forms, four values, both CMPT states, offsets 3b/38/49,
RAM/peripheral spans, two pages and four snapshots. Identical offset words
with different pointer bases distinguish actual peripheral accesses from
RAM. The captured destination/literals survive pointer and program changes;
there is one write, no data read and one extra clock only for the resolved
peripheral address, including replay of the held write.

Long-offset ADDM/ANDM/ORM similarly admit peripheral destinations, with
low controls and untested short ADDM controls still excluded. The
`rmw-long-periph` gate passes 368,640 cases/restores: three operations,
eight AR selectors, offset/preincrement forms, offsets 3b/39/49, both spans,
eight value pairs/eight status states, both CMPT states, two pages and five
snapshots (including the paid-wait clock). One captured read and one write
share the single peripheral extra clock; RAM adds none. Source/pointer and
program changes after capture cannot alter the result. These are CPU MMIO
conformance fixtures backed by RAM, not a validation of device side effects.

The original `ST #1,*AR1(38)` (`76e1 0038 0001`) at `02:97d9` now writes
destination `0038` and the following `68e1 0039 fffe` executes. The
C5416-compatible map identifies these as McBSP0 SPSA/SPSD; the native
diagnostic routes them to the McBSP0 device, rather than a fabricated reply.
The `mcbsp-control` fixture now connects the CPU's 38/39 window to the actual
McBSP device rather than RAM and executes the six original selector/ANDM
words. Its 16 cases/restores cover two pages, both SXM states and semantic
snapshots at partial Decode, selector publication, captured read and reset.
The device initially exposes XRST/XRDY (`0003`). A DXR write after the CPU
read makes it busy (`0001`); the CPU must still write the captured masked
word (`0002`). The device then reports actual transmitter reset (`0000`).
Selector/read/reset each happen once through replay; CPU accumulators,
status and AR1 are preserved. This validates the coupled model boundary,
not the physical DA150 register map or measured silicon phase placement.

`STLM A/B,IMR` is now admitted using the existing Execute-live saturated-
store path. The `stlm-imr` fixture passes 9,216 cases/restores across both
accumulators, all 16 IRQ sources, four masks, two prior masks, both INTM
states, none/IRQ/NMI input, two pages and three snapshots. Changing the
source at clock five must affect the sole Execute-clock-six write. There
are no destination reads or extension fetches. Pending-mask enable/clear,
previously selected interrupt ownership and NMI independence are checked
alongside restoration. The ISR marker does not perform RETE; this fixture
does not establish complete ISR return/context behavior or every SST source
pattern, which require their separate existing/future contracts.

The original stream observation around `8900` at `02:8b14` contains three
zeros and `0480`; four `beef` words continue after a core halt.
Peripherals continuing after a CPU halt is distinct from validated stream
content: these words do not establish decoded media or native speech.
Current execution and resident-memory acceptance are indexed above.

SPRU131G section 7.5.9 assigns PMST OVLY/IPTR/MP/MC evaluation to
Prefetch; table 7-26 gives POPM (the "all other instructions" row)
consumer-category latencies of 0/1/2/3. DROM has a separate Access/Read
contract in table 7-27. The frontend saves the physical mapping alongside
each logical Prefetch address and carries it into Fetch. The STM-based
`pmst-map` later-branch cases do not alone establish intervening PMST
publication ownership; the distinct Prefetch/IPTR checks below delimit that
coverage. MP/MC and DROM physical decoding remain separate scope, not
implied by an OVLY result. POPM REA/RSA/BRC (`8a1c/8a1b/8a1a`) also
require their own block-repeat ownership contract, not a blanket pop-range
change; separated outside-body active restoration is covered by the
[retained context contract](c54x_execution_contracts.md#active-repeat-context-and-stack-bypass).
For POPM BRC followed by a new RPTB, SPRU131G table 7-24 and example
7-54(d) require one intervening instruction; STM BRC has zero latency.
This is distinct from the in-loop BRC-write latency in table 7-25. A
restoration fixture must separate these cases and independently count loop
iterations rather than treating a stored BRC value as execution proof.

The bounded `pmst-pop` prototype now passes 64 cases/restores: four values,
two source pages, lower/upper branch targets and four snapshots. It captures
one stack read at Read and publishes once at Execute; changing the stack word
after capture must not change the published PMST. SP advances exactly once.
The ordinary branch is separated by three NOPs, so this fixture does not
establish every adjacent-consumer latency or interrupt-vector composition.
The current original-firmware stop and resident-memory comparison are maintained
above; this PMST fixture does not establish interrupt composition.

The `pmst-map`, `pmst-pop` and `pmst-prefetch` isolated
profiles each pass 64 cases/restores. The last is explicitly a debugger-state
conformance fixture: it changes PMST after the first Prefetch, with an illegal
opcode in the newly selected bank, and saves at clocks 1/2/5/6. Fetch must use
the original selected bank, including after restoration; subsequent slots
use the new mapping. This establishes retained slot ownership, not an
organic firmware transition or every table-7-26 consumer latency.
The `pmst-iptr` fixture exposed an early vector-page selection: with a pop
publishing `PMST=0000` during the foreground drain, the first run executed
the old vector page's `0066` marker. SPRU131G 7.5.9 and example 7-18 put
IPTR evaluation at the later ISR Prefetch. The staged core now retains the
accepted interrupt number but selects its IPTR page there, after older
Execute publications. The fixture passes 1,024 cases/restores across four
PMST values, two source pages, all 16 maskable IRQ sources, early/late
pending latches and snapshots at
clocks 3/5/6/11. Its old/new vector markers are distinct; the expected
new-page marker is `0055`. It supplies a controlled CPU pending latch,
not an asynchronous pin-edge or handset fixture. Both physical banks carry
the same vector marker, so this checks IPTR, not vector OVLY bank ownership.
The ISR does not return. A further mapping change between ISR Prefetch and
Fetch, other interrupt sources and full return composition remain separate
acceptance work. Broad regression coverage is indexed in
[Core regression checkpoint](c54x_execution_contracts.md#core-regression-checkpoint); current resident
execution is described separately in [Current boundary](5510_bringup.md#current-boundary).

The `pop-repeat` prototype admits direct POPM BRC/RSA/REA only
outside an active staged repeat. Its 192 cases/restores cover the three
registers, four words (0/1/8000/ffff), two source pages, two branch target
regions and snapshots at clocks 4/5/6/11. One captured stack read advances
SP once and supplies one Execute write despite later stack-word mutation;
the other two repeat registers, PMST and status are preserved. Three negative
cases place each pop inside an active RPTB and prove rejection before any
stack read, SP change or repeat-register write. RSA/REA are now exposed as
debugger states, using the existing saved register storage. The `pop-brc`
fixture additionally passes 30 cases/restores: counts 0/1/2/7/127, two pages
and snapshots at clocks 4/5/6. It places one NOP between POPM BRC and RPTB,
mutates the captured stack source, checks one stack read/write publication,
and independently observes count-plus-one byte-swap reads/writes. The
zero-latency `stm-brc` control also passes. Active-loop writes and interrupt
composition are not accepted by these cases.

Staged interrupt Access reserves its stack slot
after older foreground Read-owned SP changes in the same clock. The atomic
entry path is unchanged. The `stack-irq` fixture passes 48 cases/restores:
PSHM AL, PSHD RAM, POPM T and POPM SP; IRQ0/NMI; two source pages; and
snapshots at clocks 4/5/6. It checks source Read at 5, interrupt SP reservation
at 5, foreground push Write at 6 and hardware return-PC Write at 7, with one
correct source acknowledgement at 6. This entry-only fixture makes no
ISR-return acceptance claim. The separate `stack-irq-return` gate passes
80 cases/restores over the same forms, sources and pages, with snapshots
at clocks 4/5/6/10/14. It adds RETE, verifies one return-frame read, resumes
the caller, pops the caller's preserved word for push forms, and checks
final SP/T, cleared INTM and ordinary IDLE. Those extra snapshots cover
the retained return operand and caller cleanup, not just entry allocation.
The earlier core reserved SP before an older PSHM Read, interleaving the
hardware return frame with the foreground push; the independent fixture
failed on that slot ownership. FRAME's older-POPM ordering was already
protected and was not changed.

The native return now confirms the correction: FRETE at `02:8b76` resumes at
the populated `03:9b08`, not `03:0000`, and the original bootstrap readiness
gate reaches 1. LDU at `03:8f13` (`1281`) is now admitted for its mapped
ST1 source at 0007 through the existing retained Read path. The `ldu-st1`
fixture passes 288 cases/restores: direct/absolute/indirect addressing,
eight full-word status patterns, both accumulators, two pages and snapshots
before/after address and source capture. Changing ST1 after Read must not
change the zero-extended result; changing pointers or the address literal
after capture must not retarget it. CMPT's documented ARP update is checked
against the captured mode, separately from the preserved arithmetic flags.
One register read and no writes are observed. This is raw mapped-register
read acceptance, not a claim about every ST1 producer latency or peripheral
source. The existing 384-case IMR unsigned-read control also passes.
Broad regression and native 5110 save-state coverage are indexed in
[Core regression checkpoint](c54x_execution_contracts.md#core-regression-checkpoint), rather than
inherited from the chronology of these individual instruction admissions.

Short direct CMPM now shares the retained BITF/absolute-CMPM path for RAM
sources. The `cmpm-direct` fixture passes 384 cases/restores over DP/SP
layouts, four source words, two comparison literals, both initial TC states,
TC/NTC branch conditions, two pages and snapshots at clocks 3/6/7. Address
capture survives later DP/SP mutation; source and literal survive later
replacement. Read is at 6, TC publication at 7 and the following branch's
taken target Fetch at 10 (one intervening NOP). One source read, one literal
fetch and zero writes are checked. The 96-case absolute-CMPM control passes.
Smem CMPM also shares BITF's retained indirect address/literal path
(SPRU172C, CMPM, page 4-33). The `cmpm-smem` fixture passes 52,224 cases
and restores over 17 read-capable encoding forms, eight auxiliary-register selections,
eight equality/inequality pairs, both initial TC states, four SXM/OVM
settings and both compatibility modes. Snapshots bracket address capture
and Read. Later pointer, source and literal mutations cannot retarget or
change the comparison; offset/address extensions precede the distinct
comparison literal, and only TC changes at Execute. Each case observes
one source read, no duplicate address modification and no data write.
The `cmpm-branch` fixture adds 192 cases/restores over two program pages,
TC/NTC conditions and both initial TC states, with the branch immediately
following `CMPM *AR1,#lk`. Source Read is at clock 6, TC publication at 7
and taken-target Fetch at 9. The post-Access pointer and post-Read source
are replaced to verify retained ownership, not just final equality.
Mapped/peripheral CMPM sources remain excluded by the Access guard;
these tests do not establish physical pipeline phases, repeated CMPM or
control-delay composition.
Focused acceptance passes ten profiles: `cmpm-smem`, `cmpm-branch`,
`cmpm-direct`, `cmpm-absolute`, `bitf-peripheral`, `memory-shift16`,
`mpy-delay`, `store-delay`, `move-t` and `stack-irq-return`. The tool suite
runs 2,801 tests with one skip, including 93 core-result verifier tests;
native 5110 save-state restoration also passes. The original firmware
passes `6081 0006` at `02:a6b1` and loads its recorder overlay, rather
than merely moving to another instruction in the resident initializer.
Failure diagnostics report the retained logical instruction separately
from the ahead live PC. For folded multiword forms, `last_fetch_physical`
identifies the final extension fetch, not the opcode's physical address.

Extended STL/STH now admits mapped AR0..AR7 destinations only. SPRU131G
table 7-3 groups these writes with STLM: publication is in Execute, and the
following two words must not use that AR. The `st-extended-ar` fixture
passes 24,576 cases/restores across absolute, long-offset, preincrement and
circular-preincrement forms, eight destinations, A/B high/low stores,
four accumulator patterns, SST/SXM combinations, self/other pointer aliases
and two program pages. Snapshots at clocks 4/6/7 bracket literal capture,
destination resolution and publication. Replacing every AR after address
capture cannot retarget the write; replacing the source accumulator after
Read verifies the retained path's Execute-live source convention. One
write at clock 7, one literal fetch, no destination read and independent
signed/unsigned store-saturation arithmetic are checked.

The `st-ar-consumer` fixture adds 6,144 cases/restores with legal gaps of
two or three NOP words before an unsigned indirect load. It checks one
AR write and one load of the selected sample, including replay after its
Read. No behavior inside the manual's forbidden two-word window is
claimed. Extended SP, BK and other register/peripheral destinations remain
excluded, as do new control-delay or interrupt-composition claims.
Focused controls cover these two profiles plus `store-delay`,
`st-delay-edge`, `stlm-bk`, `mvmd-bk`, `cmpm-smem`, `cmpm-branch` and
`stack-irq-return`. The tool suite runs 2,802 tests with one skip, including
94 core-result verifier tests; native 5110 save-state restoration also
passes. Original recorder STH A (`82f8 0011`) at `03:f7dd` now executes;
the next stop is maintained in [Current boundary](5510_bringup.md#current-boundary).

TS-controlled memory loads (`14xx`/`15xx`, SPRU172C LD syntax 2) use the
shared retained Smem address path and capture both memory and T in Read.
The current implementation projects all six-bit codes to -32..31; coverage of
raw codes 32..47 is not evidence that their behavior is architecturally defined.
NORM's separately evidenced negative aliases do not establish the behavior of
this memory-load family outside its documented range; neither fixture should
be used as the other's shift-domain oracle.
SXM selects sign extension and
arithmetic right shifting; OVM does not saturate this load form, and carry
and accumulator-overflow flags are preserved. Atomic and staged execution
share the same shift arithmetic, but only the staged frontend retains the
Read-owned count. Address/offset literals are consumed before this count
occupies the retained immediate slot; PC accounting still uses the original
opcode's instruction length. Mapped/peripheral sources below `0060` remain
excluded from the staged TS path.

The `ld-ts` fixture covers 208,896 cases/restores over 17 legal addressing forms,
all 64 shift encodings, four input words, SXM/OVM combinations, both
accumulator destinations and compatibility modes. AR selection and program
page rotate with shift encoding/destination; this is not their full Cartesian
cross-product. Three snapshots bracket address capture and Read. Pointer,
literal, memory and T replacement after their owned phases must not alter
the result. An independent bit-at-a-time 40-bit shift model checks the
result and unchanged arithmetic flags; each case has one data read and no
data write. Short Read/Execute clocks are 5/6, extended forms 6/7.

The `ld-ts-producer` fixture adds 4,608 cases/restores for immediate STM T,
memory LD T and accumulator STLM T, eight shift encodings, four input words,
SXM/OVM combinations, both destinations and two program pages. STM/LD T
have adjacent consumers; STLM T has the required intervening NOP word
(SPRU131G table 7-3). The consumer retains its count when T is replaced
after Read, without duplicating producer reads/writes or literals on replay.
No behavior inside STLM T's forbidden one-word window, new SXM-producer
latency, repeat or control-delay composition is established by these cases.
Focused acceptance passes `ld-ts`, `ld-ts-producer`, `cmpm-smem`,
`st-extended-ar`, `st-ar-consumer`, `move-t`, `mpy-t-imm`, `mpy-mapped`,
`load-offset` and `stack-irq-return`. The tool suite runs 2,803 tests with
one skip, including 95 core-result verifier tests; native 5110 save-state
restoration passes. The atomic `bootreplay` bootstrap/status control also
passes pin-level exchange, selected-state replay equality and unchanged
NAND after the shared arithmetic extraction. Original `1481` at `03:f0f6` now executes. The
current execution boundary and exact resident/overlay comparison are recorded in
[Current boundary](5510_bringup.md#current-boundary), not a completed recorder transaction.
The original CMPM at `03:91eb` executes with CPL set, SP=`1246` and
direct source `125a`. The `7725` at `02:6d47` is STM immediate to PRD,
covered by the timer-register contract below. The new Smem and adjacent-branch
profiles have focused acceptance; they do not inherit all-profile coverage
from the separately versioned shared regression checkpoint.

Immediate TIM/PRD/TCR writes (`7724..7726`) now retain their literal and
publish through the timer's existing register writer at Execute, after one
peripheral-access clock; they do not use the DAGEN early-write path. The
`timer-stm` profile passes 36 cases/restores across three registers, four
values per register and snapshots before/during the wait and after publication.
It checks one literal fetch, no destination reads, one write at clock 8,
stopped TIM persistence, PRD without implicit TIM reload, TCR writable masks,
read-only PSC and TRB reload of TIM/PSC. These are stopped-timer transaction
checks, not new running-timer or silicon-phase claims. The original firmware's
current execution boundary and resident-memory comparison are maintained
in [Current boundary](5510_bringup.md#current-boundary), not inferred from individual
instruction fixtures.
The register-save loop is `STM #000b,AR7; RPT #000b; PSHD *AR7+`:
twelve stack pushes read the mapped register window `000b..0016`, not
ordinary source RAM. The repeat contract covers fresh source sampling,
per-iteration stack allocation/write, AR7 modification and saved partial progress.
The single-push prerequisite is now covered by `pshd-mapped`: 288
cases/restores over IMR, IFR, ST0, ST1, AL/AH/AG, BL/BH/BG, T and TRN,
two pages and three snapshots (Access/Read/Execute). Accumulator/T/TRN
sources use four data patterns; control sources use fixed safe state.
The resolved source is sampled once at clock 5 and written once to the
Access-allocated stack slot at clock 6, despite later source and AR mutation.
This prerequisite alone did not advance the original repeat stop. The
repeated-push contract below adds ordinary DAGEN sources; SP and modifying
self-AR aliases remain excluded because this fixture establishes neither
ordering rule.

`pshd-repeat` passes 768 cases/restores for short and long immediate RPT,
counts 0/1/4/11, upward/downward RAM and mapped-register reads, two program
pages, no interrupt/IRQ0/NMI, and four snapshots. Each copy traverses the
ordinary Decode/Access/Read/Execute slots at a one-clock issue interval;
it is not collapsed into the atomic repeat-stream helper. A saved issue
counter owns copies still to enter Decode; RC owns retiring iterations.
Tests observe count-plus-one distinct reads and retained stack writes,
single opcode Fetch, one AR7 update per iteration, RAM mutation after Read,
deferred IRQ/NMI acknowledgement and a complete ISR far-return round trip
to the exact caller PC and program page. Two negative fixtures confirm
SP/self-AR exclusions occur before stack allocation, AR mutation or data access.
Mapped upward reads cover the original BL..AR6 save sequence. SP,
modifying self-AR aliases and peripheral sources remain fail-closed, as
do memory-sourced repeat counts. This establishes the declared stage model,
not measured silicon bus-phase timing. `4b97` increments AR7 (`MOD=2`);
the former decrement interpretation was incorrect.
The combined acceptance below also covers this repeated-push contract.
The original resident load still matches all 48,261 program and 3,656 data
words. These are execution/model checks, not a full-board boot, physical
DA150 bus-map validation or native speech claim.
EXP appears in the original sequence `LDU ...,B; EXP B; LD #1e,A; LDM T,B`.
SPRU172C 4-52 defines EXP's signed exponent in T without accumulator or
status changes, including zero's special result of zero. SPRU131G table
7-13 requires one instruction of T-consumer latency; the original has that
gap. The CPU-only `exp-t` fixture retains 3,072 distinct inputs/restores across both
accumulators, sixteen 40-bit vectors, zero/one/two-gap LDM consumers,
eight status configurations, adjacent unsigned LDU producers, two pages
with four balanced checkpoints rather than four copies of each tuple.
EXP publishes T in Execute without changing its source
or flags; the adjacent LDM retains old T while a gap exposes the new
exponent. Mutating T after the consumer Read must not alter its result.
The unsigned producer's RAM input is also changed after Read, checking
that EXP sees its retained published value rather than stale accumulator
state or a reread. NORM and interrupt composition are not admitted by this
fixture. Its focused LDU, push and conditional-return controls pass.
ADDS at `02:3fad` is a real one-word opcode, not
an unconsumed extension of the preceding SFTL.

ADDS shares the retained memory ADD/SUB pipeline and unsigned-source
handling with SUBS. SPRU172C 4-10 requires zero extension regardless of SXM,
with C and destination overflow updates and OVM saturation. The `adds`
fixture passes 52,224 cases/restores over seventeen read-capable RAM address forms,
eight address registers, both accumulators, eight arithmetic patterns,
SXM/OVM states, standard/compatibility addressing and three snapshots.
Captured addresses/literals and Read operands survive later mutation;
one source read and no data writes are observed. Mapped/peripheral sources
remain excluded. The SUBS control covers the same 52,224-case domain.
`9421` at the following load is
`LD Xmem,SHFT,A` (syntax 5), not another unsigned ADD or a stray extension word.

Syntax-5 LD now shares Xmem BIT's captured AR2..AR5 address and one-time
Access modifier, retains its RAM word in Read and shifts that word at
Execute through the existing signed/unsigned load helper. SPRU172C 4-66
defines all 512 encodings (two destinations, sixteen Xmem modes and shifts
0..15). The `xmem-load` fixture covers those encodings with eight data words,
eight flag configurations, two pages and three snapshots: 196,608 replays.
Distinct AR2..AR5 addresses detect incorrect register selection; percent
addressing covers BK=0 linear behavior and a nonzero-BK wrap. Pointer and
source mutation after capture must not retarget or change the load. The
old accumulator remains through Read at clock 5 and the shifted result
publishes at Execute at clock 6; other accumulator, T and status stay intact.
There is one opcode fetch and one data read, with no data writes. Mapped/
peripheral sources, repeats and interrupt composition remain outside this
acceptance.

Nondelayed CALA A/B now shares the Class 30B held-Read/retained-target
path with FCALA, but selects only the accumulator's low sixteen bits on
the caller's logical page and publishes one PC+1 stack word, not an XPC
word. The held clock remains even without FCALA's second bus write.
`cala` passes 216 cases/restores over both sources, three caller pages,
three conflicting upper-target fields, four snapshots and no interrupt/
IRQ/NMI, including RET and exact caller-page restoration. `cala-producer`
passes another 216 cases with DLD producers and gaps 0/1/2; RAM and
accumulator mutation after capture cannot change the target. Coverage includes
FCALA, FCALA-producer and stack-IRQ controls. No new saved CPU field was
needed. CALAD, repeated calls, other producer families and producer-plus-interrupt composition
are outside this acceptance; stage placement remains a declared model.

A read-only executed-address trace starts after CALA at `02:40e1` in
`02:3e55` and later returns to `02:40e2`; the original return at `02:3ebb`
is RC ANEQ (`fc44`). The MVDD worker is reached later by FCALL
`02:993d -> 02:a164`, so its FRET does not belong to the near-call frame.
The debugger's low-window mnemonic text in this run prints zero-word ADDs
for nonzero ROM instructions; these labels are not used as opcode evidence.
Addresses were cross-checked against the original AA22 word image.

MVDD (`e5XY`, SPRU172C Class 14 and SPRU131G section 5.5.4) captures
both addresses at Access, reads its source at Read and writes the retained
word at Execute. When both operands select the same AR, Xmod wins; Ymod
must not modify it a second time. `mvdd` passes 49,152 cases/restores over
all 256 encodings, distinct/equal/overlapping address layouts, four values,
BK=0/16, two pages and four snapshots. Post-capture pointer/source mutation
cannot retarget the transfer or change the retained word.

`mvdd-repeat` passes 221,184 cases/restores over all encodings, immediate
short/long counts 0/1/15, three layouts, BK=0/16, two pages, four snapshots
and no interrupt/IRQ/NMI. Each iteration occupies real pipeline slots:
Read of iteration n precedes the older iteration's Execute write in the
same clock, including overlapping buffers. The body is fetched once,
long count literals once, and copies issue one clock apart. IRQ/NMI wait
for repeat completion, return through RETE and restore the caller's page
and stack. Input pulses settle into CPU-owned state before snapshots.
Circular fixtures start inside a valid BK-sized queue whose base alignment
is the next power of two strictly larger than BK, not merely BK alignment.

PSHD and MVDD share the retained repeat issuer and its saved remaining-issue
count. Mapped/peripheral MVDD operands and memory-count repeats remain
fail-closed; external-bank contention and silicon phase accuracy are not
established by these DARAM fixtures. The original run passes this worker
and still compares all 48,261 program and 3,656 data words successfully.
These focused fixtures do not establish complete resident initialization,
board boot or decoded audio.

Absolute AND/OR/XOR now retain the address extension through Decode,
capture the resolved address at Access, sample one zero-extended word at
Read and update only the selected accumulator at Execute. SPRU172C
4-11/4-123/4-201 specifies one additional word/cycle for absolute Smem and
no status changes. `logic-absolute` passes 1,536 cases/restores over all
three operations, both accumulators, four RAM addresses (including the
original `b582`), four value pairs, SXM=0/1, two pages and four snapshots.
Literal mutation after Fetch and RAM mutation after Read cannot change
the result; the next instruction is reached without executing the literal.
Mapped/peripheral operands, interrupt composition and control-delay
composition are not established by this fixture. No new saved CPU field
is required. The original restoration sequence is `STM #001d,AR7;
RPT #4; POPD *AR7-`, followed by `STM #0016,AR7; RPT #000b;
POPD *AR7-`. These restore PMST/REA/RSA/BRC/BK and AR6 through BL;
the mapped/repeated contracts below cover these ranges. Resident-memory
comparison remains exact; this is not complete initialization or native audio.

The shared regression checkpoint covers these contracts without promoting
uncovered instruction combinations, physical bus ownership or silicon phases.

Mapped POPD retains its Smem destination at Access and consumes one RAM
stack word, incrementing SP once, at Read. SPRU131G table 7-4 puts AR/BK
publication in Read; accumulator/T/TRN and inactive control destinations
publish in Execute. An older store to the DAGEN bank defers the mapped
write to Execute without rereading the source. The existing register writers
own partial accumulator-word masks and PMST mapping effects. No new saved
CPU field is needed. `popd-mapped` passes 4,032 cases/restores over 21
destinations (AL through AR7, BK, BRC, RSA, REA, PMST), four values,
direct/postincrement/postdecrement forms, CMPT=0/1, two pages and four
snapshots. Indirect forms use AR7, including a modifying self-AR destination;
pointer mutation after Access cannot retarget the retained write.

`popd-repeat` passes 6,528 cases/restores with short/long immediate counts,
four patterns, CMPT=0/1, two pages, four snapshots and no interrupt/IRQ0/NMI.
RAM layouts cover incrementing/decrementing/fixed destinations and forward/
backward stack aliases with counts 0/1/15; mapped ranges use the original
counts 4/11. Each body copy owns real pipeline slots: Read n precedes the
older Execute write, while DAGEN writes retain their Read-stage ownership.
The final BK publication can therefore precede an older BRC publication in
the same clock; the checker tracks per-iteration writes rather than assuming
publication order equals instruction order. There is one body fetch and one
long-count literal fetch. IRQ/NMI waits for final retirement, returns through
RETE with one balanced frame and resumes the exact caller page, including
PMST common-window changes. Pin pulses settle before any snapshot.

`popd-edges` passes 31 cases/restores: 15 model-boundary rejections and
16 older-STL/DAGEN conflicts over same/different registers and two pages.
Conflicting source Read remains at clock 6 and deferred publication at 7.
SP, status, XPC, peripheral and active block-control destinations remain
fail-closed, as do mapped stack sources, memory-count repeats and extended
POPD addressing. Destination rejection precedes pointer modification and
stack consumption; unsupported stack sources stop before SP increments,
after any earlier legal destination Access. RPTB guard fixtures account for
its setup flush/refetch rather than treating its body as an ordinary stream.
The existing `popd-ram` 50-case control retains compiler SP latency and the
original descriptor-copy ordering.

The POPD checkpoint `862b0ce1` passed twelve isolated profiles: the four POPD
profiles, POPM BRC/repeat controls, PMST pop/prefetch controls, both stack-IRQ
controls and PSHD/MVDD repeats, plus native 5110 save-state restoration.
The shared broad acceptance is recorded in the
[core regression checkpoint](c54x_execution_contracts.md#core-regression-checkpoint).

Nondelayed BACC/FBACC A/B share the retained Class 30 transfer boundary
with CALA/FCALA, but allocate no stack words and leave RTN unchanged.
SPRU172C 4-15/4-54 and class 30A specify six cycles, no status changes and
no repeat support. BACC uses the source's low sixteen bits on the retained
caller page; FBACC selects the low 23 bits, including XPC. The target is
sampled after older Execute publications and retained through the modeled
held boundary, front flush and redirected Fetch. Only calls allocate or
publish a call frame. The existing saved pending/target fields are renamed
from `acc_call` to `acc_transfer`; no new CPU save member is added. Stage
placement remains a declared model, not measured silicon phases.

`bacc` and `fbacc` each pass 216 cases/restores over both accumulators,
three caller pages, three conflicting upper-target fields, four snapshots
and no interrupt/IRQ0/NMI. They verify target sample at clock 5, Fetch at
8 and first target Execute at 12, post-sample accumulator poisoning, no
call frame or RTN update, flushed sequential writes and balanced IRQ return
to the retained target. Interrupt entry, not the branch, owns RTN in IRQ
cases. Pin pulses precede every snapshot.

`bacc-producer` and `fbacc-producer` each pass 432 cases/restores over
DLD and absolute READA producers, both accumulators, gaps 0/1/2, three
caller pages, three upper-target fields and four snapshots. DLD's two RAM
reads and READA's program-table read are retained despite input mutation.
READA performs one AL/BL publication at clock 11 before a zero-gap branch
samples its target in the same clock. The original `READA *(0008); BACC A`
composition therefore uses the published accumulator, not a stale target.
Producer-plus-interrupt composition, delayed forms and block-loop interaction
are outside this acceptance.

The branch checkpoint passes these four profiles plus CALA/FCALA and their
producer profiles, conditional call/return, both stack-IRQ controls, PMST
prefetch and repeated POPD (14 profiles total). Evidence validation and
native 5110 save-state restoration pass. The shared broad acceptance is
recorded in the core regression checkpoint.

Long-offset AND/OR/XOR now use the same retained extended Smem address
machinery as loads. TI SPRU172C's instruction descriptions specify an extra
word and cycle for long-offset Smem, with no arithmetic-status changes.
The literal is retained at Decode, address and pointer modification at
Access (clock 5), source at Read (6), and accumulator publication at Execute
(7). Circular fixtures use BK=13 with a 16-word-aligned queue base and an
initial index inside the queue; BK=0 remains linear. CMPT's encoded-AR0
selection uses the retained ARP, while nonzero encoded ARs update ARP.

The `logic-offset` profile passes 294,912 cases/restores over six operation/
accumulator combinations, 24 addressing encodings, four signed offsets,
four value pairs, four SXM/OVM states, both CMPT settings, BK=0/13, two
pages and four snapshots. Poisoning the fetched literal, all ARs after
Access and source RAM after Read checks single modification and retained
operands. Other accumulator, T, stack and arithmetic flags are preserved.
Mapped/peripheral offset sources, repeat/control-delay composition,
interrupt composition and physical silicon phases are not qualified here.
Separate short/peripheral/absolute logic controls pass 576/864/
1,536 cases, and the offset-load control passes 12,672. Both accumulator-
branch producer controls and repeated POPD also pass.
Native 5110 save-state restoration and the shared broad checkpoint pass.

MVMD now admits T/TRN sources through its existing retained Class-19A
move path (SPRU172C, MVMD instruction). The `mvmd-t` profile passes 432
cases/restores: two sources, eight AR destinations plus BK, four word
patterns, two pages and three snapshots. Source sampling and AR/BK
publication occur once at the modeled Access clock 5; changing the source
after that clock cannot change the destination or cause another access
after replay. Unselected registers, accumulators, stack and status are
preserved. This is not a producer-latency, repeat, IRQ or physical-phase
claim, nor new acceptance of other destination classes. Six neighboring
controls (`mvmd-acc`, `mvmd-st1`, `mvmd-cmpt`, `mvmd-block`,
`mvmd-status-irq`, `popd-repeat`), evidence validation
and native 5110 save-state restoration pass. No new CPU saved fields are
needed; the shared broad checkpoint covers these profiles too.

RAM-to-peripheral MVDD now shares the saved peripheral-wait mechanism.
SPRU172C Class 14 specifies one extra peripheral-access cycle. The
`mvdd-periph` profile passes 1,536 cases/restores across three peripheral-
window destinations (`0020`, `0033`, `005f`), four values, sixteen dual
modifier combinations on AR2/AR3, two pages and four snapshots. Address
selection/modification occurs once at Access (4), RAM is sampled at Read
(5), the extra wait is paid at 6, and the retained word publishes at 7.
Pointer and source poisoning checks that the wait and replay cannot repeat
address modification, reads or writes. These destinations are test sinks
in the CPU fixture, not independent device validation. Peripheral sources,
CPU-register operands and repeated peripheral transfers remain excluded;
IRQ composition and silicon phase placement are not claimed. Existing
ordinary/repeated MVDD (49,152/221,184 replays), T/TRN moves, peripheral
logic, long peripheral stores and actual McBSP control also pass (seven
profiles total), along with evidence validation and
native 5110 save-state restoration. No new CPU saved fields are required.

At original MVDD `03:9c70`, AR2=`14b2`, AR3=`0033` and source word
`007f`: this is ordinary RAM to the bench's McBSP2 TX register, not a
CPU-register transfer. Original recorder entry is `02:9e90`. The input
cursor counts delivered words, not parsed commands; pin-decoded acknowledgement
and output traffic do not establish a completed transaction or decoded audio.
Failure diagnostics distinguish the decoded instruction from the potentially
ahead live PC. See [Current boundary](5510_bringup.md#current-boundary) for the single
authoritative staged stop; physical DA150 mapping remains unverified.
ABS uses the existing Class-1 register ALU Execute path and shared atomic
arithmetic (SPRU172C, ABS instruction). The `absolute` profile passes
13,824 cases/restores over all four accumulator selections, sixteen
40-bit patterns, 32 status configurations, two pages and three snapshots.
It covers zero carry, sticky destination overflow, OVM saturation including
the 40-bit minimum, unchanged non-destination accumulator/status and 1,536
adjacent immediate-load producer cases. The source is live at Execute,
not a newly invented Read-stage latch. There are no data-memory accesses
or new CPU saved fields. IRQ and repeat composition are not claimed.
The matching NEG control, conditional returns, T/TRN moves and peripheral
MVDD also pass (five profiles total), along with evidence validation and
native 5110 restoration. The shared broad checkpoint
also covers these profiles.

Memory/immediate multiply uses MPY syntax 3 and MAC syntax 4 (SPRU172C;
MAC page 4-82). Instruction qualification, fixture domains and admission
controls have one owner in the [execution contracts](c54x_execution_contracts.md).
The positive `mpy-mem-imm` profile is separate from `source-domain`'s
implementation-limit controls; neither promotes physical bus timing or
handset acceptance.

Original R060 AA88 contains `6582 0009` at `03:ba1a`:
`MAC *AR2,#9,A,B`. `mu4_context_contract.py` pins its eleven-word
context in the original upload, with mutation and
wrong-page negatives. This static witness establishes provenance and
syntax, independently of executable core tests or recorder acceptance.

Near B and nondelayed BC retain block-loop ownership. The two-word branch
must finish before REA and target the same block; forward and backward
targets are supported. Conditional calls are not branches for this admission.
A taken redirect reconstructs the
discarded endpoint-fetch prediction from Decode-owned BRC. An untaken
BC restores its held word's body/endpoint/final-first tags alongside its
word and address. Those three saved booleans belong to the
CPU's pending instruction state; they are reset and registered for replay.
BRC decrement and final-iteration BRAF timing remain as specified by
SPRU131G sections 6.8/7.5.8.

The `rptb-branch` fixture passes 12,288 cases/restores across a two-arm
join and a BC immediately before an endpoint NOP, counts 0/1/2/7, four
taken/untaken patterns, both accumulators, two pages, eight save points,
four IRQ/NMI arrival clocks and an uninterrupted control. Source and
selected-arm writes occur once per iteration, with exact terminal
counters, pointers, status and one balanced interrupt frame when requested.
A later ordinary branch clears the held tags before replay, requiring
restoration of the earlier loop-owned tags. Pin arrivals are allowed to
settle before a coincident save, and restored pin changes settle before
CPU resumption. These are declared model clocks, not measured silicon
phases.

The `rptb-br-edges` fixture passes 32 fail-closed replay cases for targets below
and above the block, delayed control at REA, conditional near calls, nested blocks and far
branches, with no control frame or body write. Branches occupying the
endpoint, XC spans crossing the block end, IDLE in the block and software
loop-control modification remain excluded by the RPTB guards. Wholly in-block
XC is specified in the retained contract above. Non-delayed far-call acceptance
and backward-loop acceptance are specified in the retained contracts above;
near-call, outside-block branch and other delayed compositions are not promoted.
The bounded internal BD and signed BCD contracts have their own focused
profiles; this exclusion set does not reject those admitted forms.
The shared broad baseline remains the versioned checkpoint above, independently
of focused acceptance added since that run. These are not measured silicon
phase claims.

All 48,261 program and
3,656 compared resident data words still match. This remains partial
initialization, not complete board boot, native speech or a completed
service transaction.

The former bad return produced a zero-filled walk, a bootstrap restart and
a later FRET into page 8. These were downstream symptoms, not missing ROM
evidence. Zero is a valid ADD opcode, so illegal=0 did not establish useful
execution in the RAM-backed bench. Program-read taps include speculation and
program-data accesses; debugger execution tracing was needed to identify
the earlier return. The atomic `bootrecsave` observation remained in populated
code, but its later stimuli differ and it was not an instruction-equivalence
proof. No empty region was filled and no return target or firmware state was
forced to repair the continuation.

Status acceptance
is limited to the following
tested consumer/composition forms, not every status-register interaction.
Indirect address selection and pointer modification retain CMPT
in the saved Decode slot, separately from the DP/CPL sample (CMPT's bit
position overlaps DP in that packed sample). Staged short/long address
selection and AR modifications consume the same retained mode; atomic
helpers keep their live-status defaults. The fixture passes 16 cases/restores
across both directions, LD/ST with postincrement and zero through three
NOPs, checking the old mode with zero/one and the new mode with two/three.
ARP-writer latency and other indirect consumer categories require separate
coverage; this fixture establishes the basic CMPT category only.

The `mvmd-status-irq` fixture passes 576 cases/restores across IRQ/NMI,
four requested INTM pairs, enabled/disabled IMR, arrivals at clocks 4/6/8,
two pages, three snapshots and one-/two-word followers. It checks one
status publication, accepted continuation/frame ownership, no masked frame,
one foreground marker, RETE restoration and masked-pending IFR persistence.
The shared register map prevents memory writes from setting INTM. A
write-one cannot make an unmasked source ineligible; old-mask/set-mask
selection races are therefore not established by these cases. Zero-write
clearing remains a legacy, unverified policy: see the
[ST1 access contract](rom4_dsp_execution.md#st1-memory-access-constraints).
Generic status writes do not inherit RSBX/SSBX following-instruction
protection. Negative masked cases keep a
valid bounded IDLE continuation: pending sources may wake IDLE without
being serviced. This fixture does not establish arbitrary interrupt arrival
phases or all combinations with status consumers/block repeats.

The arithmetic fixtures do not establish all possible SXM/ASM consumer forms.
SPRU131G table 7-14 lists MVMD among ST1 writers safe directly
before RPTB; a late write must not erase a younger BRAF update. Section
6.10.7's protected-following-instruction rule specifically names RSBX/SSBX
INTM and must not be assumed for every full-ST1 writer. The resident
memory audit remains exact.

The status gates distinguish the following SPRU131G contracts:

- Table 7-16: MVMD has two intervening-cycle latency for CMPT consumers;
  POPM has three or two depending on the consumer category.
- Table 7-19: MVMD has CPL latency two for ordinary direct addressing and
  one for the listed extended-shift/data-copy category. Decode-time CPL
  snapshots must not be replaced indiscriminately with the newly written bit.
- Table 7-20: MVMD is absent from the one-cycle SXM-latency list, hence
  covered consumers have zero latency; POPM and accumulator stores are listed.
- Table 7-22: MVMD is likewise absent from the one-cycle ASM-latency list;
  adjacent ASM-dependent arithmetic must use the new field, while POPM and
  accumulator stores retain their documented consumer latency.
- Table 7-14: an immediately following RPTB must retain its BRAF update.

These are consumer-visible latency requirements, not a claim that every
status field is physically written at the same pipeline stage. Publication,
consumer sampling and save/replay have separate gates. Special INTM
following-instruction protection is not assigned to arbitrary full-ST1 writes.

The peripheral/control/IMR checkpoint passes full CPU conformance (288
required markers), native 5110 save-state restoration, all 33 isolated
profiles in `tools/run_c54x_core.py`, 135 targeted Python tests and evidence
validation. The original audit still matches 48,261 program and 3,656
ordinary data words, with four register-window words excluded. Full
board boot, native speech and the explicitly excluded instruction or
producer/interrupt compositions remain unaccepted; do not
infer full board boot from this initialization frontier.

TI SPRU131G section 6.10.1 explicitly defines writing the current IFR
contents back to IFR as clearing all pending interrupts. The observed
`4801 -> 8801` pair therefore has a documented acknowledgement purpose,
not a request for a fabricated peer response. The existing staged STLM
path samples the accumulator in Execute, rather than using the older
Read-stage operand snapshot; the adjacent LDM fixture confirms that
publication order without an additional forwarding change.
SPRU172C page 4-175 identifies STLM as a one-word Class 10A store with
page-zero addressing independent of DP. Pending/accepted interrupt
ownership and NMI isolation are covered by the composition fixture below.

The isolated `ifr-pair` profile admits direct `8801/8901` stores and
executes adjacent `4801/4901` loads, covering both accumulators, four
pending-bit patterns, with and without a physical IRQ12 pulse after the
read, and three snapshot positions. The pulse is delivered strictly between the
read and store; a newly set bit survives unless the captured mask already
contained that bit. Snapshots after the pulse allow its scheduled delivery
to settle before saving. This proves read-back acknowledgement and
preservation of the unselected accumulator. Additional cases unmask IRQ12
after the read with its flag already set, or pulse independent NMI. Both
finish one vector acknowledgement after the older store and return through
RETE with SP restored. The selected IRQ survives clearance of its own IFR
bit; NMI is not cancelled by IFR clearance. All 96 cases and 96 restores
pass. IMR stays zero in the pending-only and NMI cases. This is executable
composition coverage, not a claim of measured silicon cycle timing.

Mapped BITF admits AL/AH/AG, BL/BH/BG and T/TRN; other CPU control
register sources below `0020` remain excluded. Peripheral-read acceptance
is described above. `tms54test -bios mapped-bitf` requires
13,824 replay cases: eight register sources, four masks including zero,
both initial TC states, direct/absolute forms and 1,536 AL-producer cases.
It captures one source in Read and changes only TC in Execute according
to whether any masked bit is nonzero (SPRU172C 4-28/4-29). Source changes
after Read cannot change the result, and every address/mask extension is
owned once. The producer cases use zero/one values for the status-bit
question without synthesizing a DMA status or completing a transfer.

Mapped memory ADD/SUB now admits AL/AH/AG, BL/BH/BG and T/TRN, using
one retained Read sample through their existing register map. Controls,
peripheral sources and mapped SUBC retain their exclusions. The mandatory
`tms54test -bios mapped-word` fixture has 6,912 save/replay cases covering
eight registers, four add/subtract/destination selectors, direct/absolute
forms, signed/unsigned operands, OVM, accumulator aliases, changed source
registers and two program pages. Its 768 producer cases pin SPRU131G
table 7-28: a short mapped AL consumer sees the previous value after an
adjacent one-word accumulator load; one NOP exposes the new value. The
long-offset consumer has no such gap requirement. Register sampling and
flag publication stay separate; no RAM write or frontend fallback is used.
These are bounded CPU contracts, not measured DA150 bus phases.

The isolated `tms54test -bios negate` fixture requires 13,824 cases with
save/replay, all A/B source/destination combinations, 16 patterns including
32/40-bit extrema, 32 initial status combinations, two pages and 1,536
immediate-load producer cases. It checks carry only for zero, sticky
destination overflow, OVM saturation, unrelated accumulator/status
preservation, one opcode fetch and no data-memory access. The source is
live in Execute, including after a preceding producer. Arithmetic uses
the same one-word helper as the atomic interpreter without changing
frontend mode. Repeated NEG and measured silicon pipeline phases remain
outside this acceptance scope. The instruction contract is TI SPRU172C
pages 4-119/4-120.

The recorder's original `03bdb6..03bdc2` escape prefix also requires
pending double-store forwarding: `DST A,*SP(a)` followed by `DLD *SP(a),A`
must consume the retained pair, not old RAM. A passive native trace read
old low word `0088` immediately before the actual store of `0007`, causing
the bit writer to receive `0088` at width three. Mapped AL/BL reads already
returned seven; changing accumulator latency would address the wrong cause.
The staged bypass now forwards both retained DST words while preserving
the two real EB writes and their order. The exact original prefix passes
128 cases with save/replay; an independent 4,608-case matrix covers A/B,
word/pair consumers, three store addressing forms, saturation, sign extension,
aliases, gaps, pages and two save points. Physical bank timing is unvalidated.
A fresh two-second native recorder observation contains 1,010 bit-writer
inputs with zero overwide values (previously two); the first corrupt escape
is now `0007` at width three. Changed downstream field ordering means later
ordinal numbers are not identities. Full saved-media and native audio
validation remain separate requirements.

The corrected-core full `bootrecac` recording completes successfully and
passes resident-memory comparison (48,261 program and 3,656 data words)
and converted-input checks. Its independently serialized NAND SHA256 is
`6801c3b7ca072cae90c99aa0ec131d0ab9bd6a344dd77294049cc339396e413b`;
independent volume extraction yields 143,680-byte `TEST`, SHA256
`4ba59580f8bf50b8d45d7a62d8ddf42ea94dfb7d884ff05b5092e7c13141470d`.
These identify the post-DST-fix recording, not a decoded-audio oracle.

Fresh read-only transform observations match all 96 source prefixes,
scalar outputs and independently extracted saved chunks (3,072 bytes,
unique file origin 622, one state and cursor 0..3,040 in steps of 32).
Captured plaintext SHA256 is
`5b722a6fc85fb40368b7d4099d4cc7fbaae99e520161fd3875d7952e29295938`.
FFmpeg decodes its first 2,977 bytes as eight packets/eight frames,
8,192 samples and zero decode errors, versus one frame/seven errors before
the DST correction. This is host validation of captured native encoder
output, not native decoder or analog audio acceptance. Reusing a prior
run's transform state fails and is not a valid substitute for fresh capture.
Native cold playback takes the processing body on its first invocation.
The original `0260b3..0260bc` caller loads T from `1170`, initializes A/B
from the first two coefficients at `c22a`, repeats `POLY *AR2+` three times
and stores A to `1174`. Ten source words are pinned by
`mu4_context_contract.py`; mutation tests reject every changed source word.
[TI SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf), page 4-126,
defines POLY: A receives rounded signed A(32..16) times T plus old B;
B receives the coefficient shifted 16, with SXM extension. T is unchanged.
The staged path captures memory/T in Read and shares arithmetic with the
atomic path. Short direct/indirect forms and pipelined RPT are admitted;
mapped/peripheral sources, extended forms and control-delay composition
remain excluded. SMUL-set behavior and interrupt composition are unvalidated.
The `mac-smem` profile now also requires 4,608 POLY cases/restores over
three addressing forms, eight input patterns, FRCT/SXM/OVM combinations,
no repeat/RPT #0/RPT #2, two pages and four snapshots. Tests preserve T,
unrelated status and exact coefficient-read/pointer counts. POLY must remain
excluded from generic Smem-special T publication: reading its coefficient
does not write T.

`mu4_playback_poly_observe.lua` captures actual reads of `1170=4600` and
`c22a..c22e={1282,be62,76db,3842,0000}`, then actual stores
`1174=68a9,1175=0000`. The source-contract tool independently computes
all three steps and matches `68a90000`. Live PC is context only, not
retained operand ownership or retirement proof.

SQURA/SQURS (TI SPRU172C 4-163/4-164) square the signed memory word,
add/subtract it from A or B with FRCT and final OVM saturation, and load
T from memory. The staged path retains the operand, publishes T in Read
and must not republish it in Execute. Do not substitute the MAC product
clamp: these square operations retain the full product before accumulation.
The atomic and staged paths share one arithmetic helper. The `mac-smem`
profile requires 36,864 square cases/restores over both operations and
destinations, indirect/absolute RAM and mapped AR1 sources, 64 input pairs,
16 FRCT/OVM/SMUL/SXM settings and three snapshots. Tests change RAM/T after
Read and require one captured source read, no data writes, sticky overflow,
the unaffected accumulator/status and preservation of the later T value.
This does not validate all mapped-producer latencies, other extended
address forms, RPT/interrupt composition or measured physical bus phases.

Fourteen original AA99 words at `0260cd..0260da` pin the block-repeat
setup and `0260d8: SQURA *AR2,A`; source mutation tests guard the extent.
Native playback advances past that boundary. The global playback frontier
belongs in Current boundary above; source and dependency acceptance here
must not be mistaken for complete decoder-output validation.

## T-input Product Dependencies

The register forms `MPYA A/B` and `MACA[R]/MASA[R] T,src,dst` use signed
A(32..16) and signed T, with FRCT, final OVM saturation and sticky target
overflow. MACA/MASA additionally use the source accumulator, SMUL product
saturation and optional rounding; MPYA does not apply that intermediate
product clamp. Atomic and staged execution share one arithmetic helper.
No data-memory transaction or T publication belongs to the product itself.

SPRU131G 7.5.6/table 7-13 gives these multipliers zero T-producer latency:
they are not the Category-I shift/bit-test consumers. Table 7-3's generic
recommendation must not be turned into an old-T multiplier oracle. The
partial staged model retains T after older Execute and before younger Read
publication, then samples A and the source accumulator in Execute. Reading
live T in Execute would instead risk consuming a following STM-to-T.
Smem MPY/MPYR/MPYU/MAC/MACR/MAS/MASR and POLY retain dedicated T at the
same older-write boundary. A mapped-T Smem operand defers its single
memory read to that boundary too; other sources keep their existing read
ownership. Immediate-T products retain their operand during the complete
two-word Read, whose extension clocks already separate the tested older
producers. They need no production change in this audit.

Two executable counterexamples establish why early capture is wrong.
Adjacent STLM-to-T followed by `2081` consumes zero rather than `1234`,
producing zero instead of `fff6e61234` with memory `8001`. Adjacent STLM
followed by `3681` uses stale T=1 rather than `8000`: with A=`12340000`
and B=`7fff8000`, it produces `0080000000` rather than `0076e60000`.
Both fail before their capture-order correction and pass afterwards.

The `mac-smem` profile requires these dependency banks, each with eight
numeric patterns, two pages and three save-state snapshots:

| Bank | Cases And Replays | Domain |
| --- | ---: | --- |
| Register products | 69,120 | 18 opcodes, 16 control settings, five layouts |
| Short Smem products | 107,520 | 14 opcodes, RAM/mapped T, 16 settings, five layouts |
| POLY dependencies | 2,304 | Indirect RAM, eight settings, six layouts, SMUL excluded |
| Immediate-T products | 27,648 | Six opcodes, 16 settings, six layouts |
| Absolute Smem products | 64,512 | 14 opcodes, RAM, 16 settings, six layouts |
| EXP producers | 35,328 | 18 register and 28 short RAM/mapped-T forms, 16 settings |

The first five layouts are no producer, adjacent/separated STLM-to-T,
an A producer and a following STM-to-T. The sixth is adjacent EXP A.
An independent scalar model checks signed/unsigned arithmetic, rounding,
FRCT/OVM/SMUL, fresh/sticky overflow, and POLY coefficient extension.
Adversarial T, RAM and extension replacement after Read checks retention.
Each case requires exactly one opcode fetch, the expected zero/one source
and literal reads, and only the expected producer write. The existing
MAC/MAS, square and repeated-POLY arithmetic banks also pass.
Nine AA99 words at `025b0d..025b15` pin the original T-producing caller
and `025b14: MPYA A`, with per-word mutation tests.

This is bounded family/dependency acceptance, not every producer/address
combination: long-offset Smem, absolute mapped-T and other mapped producers,
POLY with SMUL, dependency/repeat/interrupt composition and physical phases
remain unvalidated. Keep Category-I TS/NORM/bit-test restrictions separate.
`c54x_remaining_t_multiplier_category_audit` retains these residual limits.
TS load/arithmetic producer, EXP-to-NORM, EXP-to-register and invalid MOD=3
profiles pass. A fresh 20-second native playback after the POLY correction
passes DMA/source-range and capture-integrity checks; all three complete
captured streams are byte-identical to the preserved baseline. Neither
dependency correction is an observed waveform improvement in this workload.

With these register forms admitted, the preserved recording produces a
complete 20-second legal run, not an unsupported-opcode stop. Observation
checks report 1,631,772 actual channel-3 source reads (1,582,728 nonzero)
within `1000..137f`, 1,661,065 TX words and 1,691,842 DIN words, with
1,597,627 nonzero at each serial boundary. First-session method coverage
has 375 body/publication/return paths and no early returns; parent coverage
completes 11,956 wrapper/body/return paths and 5,978 dispatches. The selected
AA99 range matches original source at 1,170 visited addresses and
6,631,884 Execute entries, with no opcode changes. DMA and source-range
checkers pass; the diagnostic still deliberately exits rejecting audio
completion. Counters alone are not a waveform-quality or complete
transport-pairing oracle; analog conversion and speech remain unproved.
