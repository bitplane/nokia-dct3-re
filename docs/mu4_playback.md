# MU4 playback and serial provenance

Original file selection, decoding, serial-word ownership, refill and pending-tail maps. [AAC validation](aac_validation.md) owns the accepted bounded waveform/conformance claims. Reference-valid pending RAM is not committed media or a fully drained native track.

## Saved-Track Playback Boundary

`bootplaystage` starts from the same saved endpoint with the recorder's
provisional shared-DARAM mapping, but it does not request recording.
The original dispatcher receives this twelve-byte external request:

```text
1e 06 aa 01 30 02 00 01 00 01 81 55
```

Class 1, selector `30`, parameter 2 are followed by no-shuffle value zero,
track ordinal one and reply-cookie zero, then sequence token one and XOR
`81`. At `03:b3b0` the firmware checks the nonempty catalogue; `02:9d8c`
handles the shuffle field, and `02:9d1d` selects track one and advances
its own lifecycle. No mode, selection index, decoder pointer or internal
message is supplied by the bench. The shared media peer validates and
acknowledges the native frames; its `mu4_native_recorder_response` prefix
is retained for compatibility and does not classify playback as recording.

The verified run enters the original request and inspection paths once,
reaches the named-loader call at `03:a9c3`, and materializes original
`RELSI16 .BIN` (`aa99`), not `AACSI16`. At the common vector entry the
independent final-write comparison accounts for all 3,127 source records,
49,843 written words and 49,839 compared RAM words, with four low-window
words excluded. Incoming replies have class/selector/size
`1/58/12`, `0/02/16`, `0/03/20`, and each receives the normal pin ACK.
Those replies do not alone establish decoder completion or sample output.

Mapped-SP MVKD is supported through the existing retained data-copy path.
SP (`0018`) is a Read-owned data operand, separate from the earlier
compiler-mode destination address. `mvkd-sp` independently verifies 384
copy cases and their replays: direct DP/CPL, indirect pointer modifiers,
absolute/offset destinations and direct MVDM to an AR; four source patterns,
both CPL states, two program pages and three save points. It changes SP
after address formation and again after Read, and replaces fetched
literals, so a reread or recomputed address fails. Another 576 replayed
cases cover older FRAME/PSHM/POPM/STM producers, zero/one/three-word gaps,
a younger FRAME and preserved accumulator patterns. Twenty rejected
control/peripheral-source cases and five rejected control destinations
remain fail-closed. The same fixture fails at the source guard before the
admission; the existing ADD-SP profile also remains green. These are
bounded pipeline-model checks, not measured silicon phase timing.
The complete 288-marker default core suite and the native 5110 save-state
gate also pass with this admission. They protect their existing scopes;
neither is a REL playback or complete native-handset acceptance.

The original REL sequence at `02:a81e`, `7090 0018`
(`MVKD $0018, *AR0+`) now executes. Neighboring instructions copy AR1,
AR6 and AR7, and the adjacent restore sequence uses MVDK to restore SP;
that context-shaped layout does not by itself identify a task or RTOS ABI.
The original `02:6116`, `fb82 510c` (`FCALLD $02:510c`) is the first
instruction of active block `02:6116..6120`.
The block count is initialized to six at `02:610f`; the call's delay words
are `e801 f495`. TI's [SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf),
FCALL[D] page 4-57 and RPTB[D] page 4-148, explicitly restricts far calls
inside block repeats. This restriction does not define a hardware trap or
prove that the supplied program is invalid. TI's separate
[extended-addressing report](rom4_dsp_execution.md#repeat-endpoint-and-page-hazard)
establishes that REA compares only the low PC, including on foreign pages.
The paired atomic/staged hazard fixture passes 384 replays with FCALL and
FCALLD. The bounded delayed-call composition now passes its separate
2,304-case frame/delay/count/replay fixture and the 20 exclusion controls.
This models the explicit instruction/register effects without rescinding
the manual restriction or claiming measured silicon phases or unrestricted
control mixtures. It does not clear BRAF, bypass the loop, synthesize a
callee return or introduce hidden call-owned repeat registers.

`mu4_context_contract.py` pins all 25 caller words and all 55 callee words.
The callee has one forward BGT branch at `02:5117`, with a two-word
`STM #a858,AR3` delay, and a sole FRET at `02:5142`. Both paths converge
at `02:512e`. It has no calls, repeat setups, backward edges, BRC/RSA/REA
writes or BRAF/CPL writes. Its fixed stores target `a858..a859`, `b331`
and `a854`; it pushes AL and pops that word into AR0. Direct offset zero
requires incoming CPL=1. Do not assign the following routine's BRC write
at `02:5170` to this callee: the separate prologue starts at `02:5143`.

The read-only native snapshot matches all 25 loop words in the common
program window, with OVLY enabled, CPL/BRAF set and BRC/RSA/REA equal to
`0006/6116/6120`. Source image and live code agree on the same-page call
and both delay words; initial overlay comparison covers the pinned callee.
This excludes missing loop words or an unclassified callee-owned repeat
setup, not the provisional physical DA150 map or all call/loop hazards.

The unchanged external playback input also executes `02:5fdd`,
`6ff8 0015 0c81`, `STL A,1,*($0015)`. Its destination is C54x AR5
(`0015`), not an unidentified external peripheral. Shifted stores now
admit AR0..AR7 using their ordinary Execute-time write; other low mapped
destinations remain guarded. `MVDK Smem,AL` at `02:6050`, `710a 0008`, also
executes through the tested generic accumulator-destination move path.
The immediate destination is AL, but source mode `0a` denotes AG only
with CPL clear and DP=0; with CPL set it denotes `*(SP+10)`. Do not infer a mapped
source solely from the opcode's low byte.
The run observes
929,701 NAND data reads and 26,211 strobes; unchanged counts do not negate
instruction progress, and neither count is decoder/output acceptance.

`mu4_context_contract.py` also pins the 13 original words at
`02:5fdc..5fe8` and decodes the shifted-store opcode/operation-extension
order independently of the CPU. The preceding instruction writes AR4
(`STLM A,AR4`), not AR5. After the three-word AR5 store come a NOP,
two-word `LD *+AR4(-15826),B` and `STL B,*AR2+`; the first local AR5
consumer is `DLD *+AR5(-15698),B` at `02:5fe4`. Those four intervening
words do not indicate an immediate-use latency violation.

SPRU131G table 7-5 places shifted STL/STH in the store-type group;
table 7-7 gives an AR write two latency cycles before a category-I
consumer such as long-offset indirect DLD, or one before category II.
This is a local static spacing contract, not executed publication evidence.
The early-DAGEN conflict checks include the `6fxx` shifted-store family
alongside ordinary stores, STLM and RMW.

Executable shifted-AR acceptance is split into four isolated profiles:

- `st-shift-ar`: 122,880 replay cases, four extended address forms, all
  eight AR destinations, STL/STH from A/B, four accumulator patterns,
  SST/SXM combinations, shifts -16/-1/0/1/15, self/other pointer aliases,
  two program pages and three save points. Address and operation
  extensions are changed after fetch; address registers and the source
  accumulator are changed between Read and Execute. The test requires
  the retained address but Execute-live source, one write and no data read.
- `st-shift-ar-use`: 30,720 replays of absolute stores followed by legal-gap
  indirect consumers, with gaps 2/3 and the same shift/arithmetic matrix.
  This does not define forbidden-gap behavior or measured silicon phases.
- `st-shift-ar-bank`: 768 replays of short direct stores followed by STM,
  MVDK, POPM or constant ST to the same or a different AR. RAM controls
  publish at clock 7; shared-bank collisions defer the younger write to
  clock 8 without repeating its source read or stack increment.

These isolated shifted-store profiles are separate from the default CPU
suite; passing that suite does not imply their coverage.

MVDK to AL/AH/AG/BL/BH/BG retains its Access-owned source and writes at
the following clock, like the existing retained RAM-destination path.
Guard writes mask to eight bits and preserve the accumulator's other fields.
`mvdk-acc` passes 8,208 replays over eighteen mapped sources plus RAM,
six destinations, direct/indirect/absolute addressing, four word patterns,
two program pages and three save points. Fetched literals and captured
sources are changed to check retention; full accumulator comparisons check
sibling preservation and source/destination aliases. `mvdk-acc-prod` adds
15,552 replays with mapped STL/POPM and direct LD producers, gaps 0/1 and
all six source/destination fields. Adjacent short-form direct LD results
remain a prototype projection, not a guarantee for code violating
SPRU131G table 7-28. MVMD accumulator
destinations and unrelated control/peripheral moves are not admitted.

## REL copy dependency

The copy loop at `02:45b1..45b2` (`DLD *AR1+,A; DST A,*AR0+`)
uses destination `(BK + 2 * stack[SP+8]) & ffff`. Under CPL=1, the
`08` direct operand is a stack slot, not mapped AL. BK comes from
`MVDK *(SP+14),BK` at `02:4547`, not mapped T: three saved registers,
nine locals and the two-word far-return frame place this at caller
stack argument 0. The `02:47d3` caller supplies bases `7000` and `e800`,
opaque context `ac36`, and entry A values `e800` and `f000`.

The load/use dependency at `02:459c..459f` is:

```text
MVKD $0011,*(SP+8)  ; retained AR1 word -> stack slot
LD   *(SP+8),A      ; adjacent same-address consumer
ADD  *(SP+7),A
```

SPRU131G section 7.3.2 requires same-location on-chip reads to consume
a pending write through the internal bus. The staged read-bypass path
includes MVKD's retained Read operand; it neither re-reads live AR1 nor
advances the real destination write. `mvkd-forward` passes 6,912 replays
over direct/postincrement/absolute destinations, LD A/ADD A/LD B consumers,
same/different addresses, gaps 0/1, SXM/OVM, CPL, two pages and three save
points. Changing AR1 after Read and changing fetched literals checks
retention; source reads and real writes occur once. Bank contention and
the physical DA150 memory extent are not established by this CPU fixture.

Without this bypass, the consumer read stale stack word `ac78` despite
MVKD writing the intended index (0,4,8,...); the following stack store
saved the stale value again. This generated `40f0` and overwrote code.
With forwarding, the observed destinations advance through `7000` and
`e800` buffer ranges; the latter 49 setup samples span `e800..ef40`.
A read-only clean-build snapshot matches all 13 original AA99 words at
`40e8..40f4`, including `0c9e 5682 5483 f622 6f95`, in both program and data
spaces. The argument probes were removed after the CPU correction.
This resolves the observed overwrite, not all memory-map fidelity.

`mu4_context_contract.py` pins all 34 entry/dependency/copy words and
tests CPL=1 pointer arithmetic. Initial overlay equality alone is not
evidence that code survives later data writes; retain the distinction
between a bad computed pointer and a physical aliasing defect.

## REL delayed shifted-store contract

The original playback executes instruction `02:41bc`, `6f93 0d9f`,
`STL B,-1,*AR3+`, with the live PC already `02:41bf`.
These two words occupy the delay span of `RPTBD 41eb` at `02:41ba`;
the ordinary body begins at `02:41be`. The decoder admits a complete
two-word shifted STL/STH only when the retained delayed-control owner is
RPTBD. Other control owners, three-word operands, a store spilling beyond
the delay span and unsupported mapped destinations remain guarded.

`rptbd-shift-st` passes 115,200 cases and save/replays across repeat counts
0/1/2/7, direct and indirect addressing, decrement/postincrement, mapped
AR destinations, all four accumulator/store selectors, signed shifts,
SST/SXM, program pages, CPL and IRQ/NMI arrivals. It checks one delay
write and pointer effect, Execute-live source capture, retained fetched
words, BRC+1 body iterations and balanced interrupt frames. Span spill,
three-word addressing, BRC destinations and other control-owner combinations
remain unqualified. Physical pipeline phases remain unvalidated.

## REL memory-sourced repeat contract

`RPT Smem` now admits basic direct operands and short indirect stationary,
postdecrement and postincrement forms (`4700..4797`), in
addition to the existing absolute `47f8` form. DP/CPL are retained at
Decode, the compiler-mode SP base at Access, and the count is sampled
once in Read. The selected pointer changes once for count acquisition,
not once per repeated body. The two extra Class 5A/5B clocks remain a
partial timing placement, not measured silicon phases. See TI SPRU172C
4-146/147 and the existing SPRU131G address-latency contracts.

`rpt-smem` passes 10,368 cases/replays over 36 address/source cases,
counts 0/1/7/255, CPL, compatibility mode, program pages, no interrupt/
IRQ/NMI and three snapshots. Eight additional direct-RAM cases cover
unsigned counts `8000`/`ffff` on both pages and two snapshots, including
65,536 body executions for `ffff`. Tests observe one count read, one
body fetch, exactly RC+1 Execute observations, retained count/body despite
backing-memory mutation, balanced deferred interrupts and unchanged
unrelated state. The earliest replay restores the fixture's queued
external interrupt assertion; later snapshots retain the CPU's accepted
or pending interrupt without supplying a second edge.

TI SPRU131G table 5-4 marks short MOD=3 as write-only, so its former
RPT read fixtures were outside the legal domain. Control/peripheral,
long-offset, nested-repeat and delayed/block-owner combinations remain
outside this qualification; prototype rejection is not a silicon trap contract.
Producer spacing, modifying
self-AR count sources and additional repeated-body families require their
own composition evidence. Encoded direct operand `00` does not by itself
establish an IMR or stack-relative source.

## Repeated accumulator loads

The repeat engine admits `LD src,SHIFT,dst` and `LD src,ASM,dst`
(TI SPRU172C 4-66..68). Each iteration consumes the current source
accumulator and updates only its destination and sticky destination
overflow. Same-accumulator loads therefore compound; cross-accumulator
loads do not. SXM governs right-shift extension and OVM governs
left-shift saturation. The instruction remains one word, with one
Execute publication per steady-state clock. Other register-operation
families remain independently guarded.

`rpt-ld` passes 200,704 cases/replays. Its 32,768 arithmetic cases
cover all four source/destination pairs, all 32 signed shifts, both
shift sources, SXM/OVM, eight source patterns and four repeat counts,
with distributed pages/snapshots. Another 165,888 cases cross short/
long immediate, DP, SP, absolute-RAM and absolute-AR counts with three
representative shifts, flags, two patterns, both pages, IRQ/NMI and
three snapshots. The independent oracle compares both accumulators,
sticky flags and RC at every clock, not just the final result. Tests
also poison the backing opcode/count after capture, require a fresh
one-word continuation and balance the deferred interrupt frame.

The remaining 2,048 cases put immediate-shift loads inside short RPT
inside RPTB, with outer counts 0/2 and inner counts 0/1/7/255. The
focused `rpt-ld-block` profile runs this bank alone. It checks every
arithmetic publication, independent counters and one inner continuation
per outer iteration. RPTB's initial discarded sequential fetch must not
be mistaken for an executed body: the fixture gives that fetch a distinct
`e8ff` word, replaces it before the redirected accepted fetch, and proves
only the actual load executes. This is a declared pipeline-model test,
not a measured physical fetch phase. Block/IRQ composition and memory
counts inside blocks remain outside this bank's claim.

## Accumulator-to-PMST stores

Ordinary direct `STLM A/B,PMST` is admitted. TI SPRU172C 4-175 defines
the direct MMR destination independently of DP/CPL; SPRU131G table 4-3
explicitly includes STLM under saturation-on-store. The core therefore
uses the Execute-live accumulator and the old SST/SXM values before
publishing PMST. Source accumulators and status flags are unchanged.
SPRU131G 7.5.9/table 7-26 place program mapping at Prefetch; example
7-62 explicitly uses STLM to update PMST. Already prefetched words are
not remapped or flushed by this store.

`stlm-pmst` passes 38,912 cases/replays. Its 4,096 ordinary mapping
cases cross eight 40-bit source patterns, A/B, old SST/SXM, CPL, old
OVLY, two program pages, low/high entry and four snapshots. The source
changes after Read to prove Execute-live capture. Distinct old/new-bank
marker instructions prove retained prefetch and subsequent OVLY mapping.
The remaining 34,816 high-entry cases cover all 16 IRQ inputs and NMI,
early/late arrivals, the same source/saturation patterns, pages, old OVLY
and snapshots. A vector fetched on the publication clock retains the
earlier IPTR/OVLY sample; a later vector uses the newly published values.
Every run requires one PMST write, a balanced optional frame and unchanged
XPC, pointers and unrelated state.

Delayed/repeat owners, other destinations and indirect/peripheral addressing
are not qualified by this PMST fixture. This admission does not establish
external-access interlocks or physical MP/MC/DROM
memory decoding; the mapping assertions concern the existing OVLY/IPTR
model, and phase placement remains explicitly partial.

The original AA99 REL image contains `491d f340 0021 f495 f495 891d`
at `02:9fa9..9fae`: an ordinary PMST read/modify/store sequence.
AA22 has a different word (`3763`) at the same numeric store address;
the images must not be merged into one code namespace. Original playback
now passes this sequence without substituting a control value.

## Long-address PSHD contract

`PSHD Smem` (SPRU172C 4-131, class 16B) admits long offset,
long preincrement, long circular preincrement and canonical absolute
forms (`4be0..4bf8`). The complete address word is retained in Decode,
Access resolves the source and allocates the stack word, Read captures
the source once and Execute writes the retained value. The source and
stack address have separate owners. In an isolated entry the declared
model resolves/allocates at clock 5, reads at 6 and writes at 7; physical
bank timing and external-memory contention remain unvalidated.

`pshd-long` passes 99,696 cases/replays. Its 96,000 RAM cases cover all
25 forms, four values, both compatibility/CPL states, BK=0/9, two pages,
early/late IRQ/NMI and three snapshots. Linear offsets are -1/0/4/32767;
nonzero-BK circular offsets are -1/0/4/8, with a valid initial index and
step magnitude at most BK. Absolute cases include the old stack top,
the push destination itself and the end of the 16-bit data space.
Poisoning the fetched literal and sampled RAM cannot retarget or change
the push. Every run requires one source read/write, one pointer update
where applicable and a balanced optional interrupt frame.

The remaining 3,696 cases resolve 26 mapped-register sources through
offset, long preincrement or absolute Smem addressing. Control-register
inputs remain fixed safe state; ordinary register fields use four patterns.
The modifying self-AR case is excluded, not silently tested against its
own updated value. `pshd-long-map` runs this bank alone.

SP, peripheral and modifying self-AR sources, additional modes and
delayed/repeat/block/XC owners are outside this qualification.
Older-producer, adjacent-stack and compound-loop
compositions need their own coverage. Long MOD=13 preincrement is a
read-capable addressing mode, unlike short write-only MOD=3.

## Extended POPD and playback execution boundary

POPD's encoded Smem is a destination. SPRU172C 4-127 specifies one extra
word and cycle for long-offset/absolute addressing. The staged path retains
the extension in Decode, resolves the destination in Access, reads the old
stack top and increments SP in Read, then writes the captured word in Execute.
It advances past both instruction words. Its isolated clocks are 3/5/6/7
for extension/Access/Read/Execute, a partial model rather than silicon timing.
Address-preview AR/ARP effects are rolled back on rejected destinations.

`popd-ram` requires its existing 50 short/composition cases plus 16,128
extended cases/restores over all 25 encodings, four values, CMPT/CPL,
two pages and three snapshots. Circular cases require BK=9, a valid index
and bounded steps; zero-BK circular cases are omitted with raw IDs preserved.
Literal poisoning, later AR changes and stack-memory mutation require one
retained address, one stack read and one destination write, including a
destination aliasing the old stack top. `popd-edges` retains its 31 existing
checks and adds 64 rejection/replay cases for extended mapped/SP/peripheral
destinations, an unadmitted mode and delay/repeat/block/XC owners. Rejection
performs no operand transaction, stack allocation or pointer change.
Extended mapped destinations, IRQ composition and physical timing are not
accepted by these fixtures. Eight neighboring stack/control profiles,
native 5110 save-state and 2,929 tool tests (one skipped) pass.

AA99 contains `8a10 8be1 0004 f4e4` at `02:a26f..a272`; the admitted
long-offset POPD is followed by FRET, not a missing source image. On the
unchanged protected NAND, original playback executes this transfer without
an illegal instruction. The current complete-window execution and codec
observations below remain separate from playback acceptance.

## Playback execution and codec observations

The Execute-entry observer excludes XC-annulled words. The staged core
implements rejection with an internal NOP while preserving its word address;
that slot must not be reported as an original instruction. A per-slot
annulment marker is shifted, reset and saved with the pipeline and only
suppresses the observer callback, not execution clocks or guest state.
Genuine source NOPs, including accepted conditional NOPs, remain observable.
`instr-observer` passes its original 32 cases/restores plus 64 XC1/XC2
accept/reject cases/restores across atomic/staged frontends, pages, OVLY
states and four snapshots. Before the fix, staged case 33 reports an extra
`f495` at the rejected `e966` address; the corrected event sequence matches
the atomic control. Neighboring XC control/signed profiles and native 5110
save-state restoration pass. Older raw instruction-entry totals can include
these synthetic slots and must not be compared as exact source counts;
the source-validated range census excludes them. This remains an entry
observer, not an instruction-retirement assertion.

The detailed baseline below uses the protected NAND
`93548b594a91df0ba87acc316435b12fb6fa35da97b066f0c7141748c5d94bb8`.
An independent 20-second playback of the fresh, current-core recorder
endpoint documented under [Recorder metadata request contract](mu4_recording.md#recorder-metadata-request-contract)
also verifies the title/overlay and 14 replies, with `illegal=0` at
`02:b5e1`. It observes one bootstrap, twelve resident and 24 loader-API
Execute entries; 87,390 completed arithmetic-loop calls and 5,592,960
body entries. The 1,524,209 TX words, 1,691,842 DIN words and 1,363,518
actual DMA source reads are again all zero, with source range `1000..137f`
and the same stream hashes as the baseline. This excludes the stale
recording as the sole cause of silence, not a container, encoder, decoder
or buffer-ownership defect. The diagnostic intentionally fails with
`audio completion is not validated`; no audio acceptance is weakened.

Program-bus hits are not invocation counts. The 20-second run has three
physical bootstrap fetch hits but one Execute entry at logical `00:0e41`.
Physical resident address `00:6d62` has 24 fetch hits; the observer records
12 actual Execute entries at logical `02:6d62` under OVLY. The first resident
entry is at 0.227594058 seconds; subsequent observed entries include
2.366754525, 4.002378107 and 5.638001690 seconds. Bootstrap re-execution is
not established by its three fetch hits. The recurring resident entry is real
and follows the named-loader path described below. These are Execute-entry
observations, not retirement or atomic register snapshots.

AA99's `a2c6` loop loads BRC=`003f`, repeats through `a2e9` and returns at
`a2ea`. Opcode-qualified Execute observations record 87,510 starts,
5,600,640 body entries and 87,510 return entries. The bounds
`64 * returns <= iterations <= 64 * starts` hold with all observed calls returned;
the sampled PC is not evidence of a stuck loop. The source calls this loop
`filter` descriptively, not as a recovered Nokia function name. AA22 has
different instructions at these addresses and must not inherit the attribution.

The audio observation window starts at the verified decoder Execute entry,
0.818115786 seconds, not at codec initialization. It counts 1,524,209 McBSP
TX words and 1,691,842 codec DIN words; both streams contain only zeros.
Their whole-stream FNV hashes differ (`7d8d3d4d` versus `1e4f2315`) because
the stream lengths differ. Codec
power/format/rate/active registers are `0007/0053/0023/0001`, with 230,090,537
clock rises and 845,921 frame rises. Counts and hashes are independent:
neither zero-frame alignment nor a one-to-one PCM relation is established.
The fixture's converted-sample ADC input remains an explicit bench source;
nonzero DIN alone is not decoded music or analog audio acceptance.

Within the same decoder-entry window, actual channel-3 data-space source
reads total 1,363,518, all zero, over addresses
`1000..137f`. This attribution uses `source_read_matches`, not merely an
enabled source-register match. The bounded prefix is `1000`, `1040`,
`1001`, `1041`, then `1002`, `1042`, `1003`, `1043`. Eight bounded register
snapshots confirm count `1/0` alternating, frames `203f..203c`, mode `c541`,
reload count `1`, and element/frame indices `0040/ffc1`. This is the expected
two-element frame sorting, not the former downward-63 walk caused by
re-latching the live count on DE re-enable. That defect's 1,277 nonzero
output words are not usable evidence of decoded media. Complete zero-word
streams also do not prove correct media decoding or stream pairing.
No writer-instruction ownership is
inferred from the data read or the CPU's contemporaneous PC.

The focused DMA pause/restore regression passes, and 2,943 tool tests
(one skipped) pass. The fixture deliberately
terminates with `audio completion is not validated`, so the Make target remains
failed before its later exact-one fetch-count checks. Those legacy fetch checks
are not evidence of three actual boots and were not relaxed into success.
The manual checker validates observation completeness, bounded executed-byte
trails and loop bounds, not playback acceptance:

```sh
python3 tools/noki5510_bootstrap_check.py --playback-observation RUN_DIR/error.log
python3 tools/noki5510_bootstrap_check.py --playback-dma RUN_DIR/error.log
```

## Output-copy observation

The AA99 contract pins 74 words around `02:a31f`. When the double fields
at offsets 2 and 4 are equal, the copy count is the minimum of remaining
source extent (`[7]-[8]`) and destination extent (`[11]-[12]`). AR3 and AR2
are formed from bases `[6]` and `[10]` plus the **old** cursors. Both object
cursors advance before `02:a366: MVDD *AR3+,*AR2+`; consequently a snapshot
in the copy body already contains end cursors. Exhaustion/full flags at
offsets 9/13 are set for the limiting side, both on a tie. The reference
model is restricted to nonnegative signed-word extents and bounded cursors;
2,025 small cursor combinations plus boundary cases test its partition.
The unequal-field helper and an all-alias writer census remain outside scope.

`mu4_playback_frame_observe.lua` is a passive two-second instrument. It
selects real bit-reader data reads and copy-context writes during bounded
windows, captures RAM snapshots, then removes its taps. GENPC is a context
selector, not a retained writer identity. It does not require the debugger
or change guest state. The earlier all-zero bank statistics belonged to a
superseded core/media pair and are not current audio evidence.

The fresh run has 2,401 selected bit-reader reads and 310 sampled copy
boundaries, with no cap or partial-word accesses. The initial `162e` RAM
snapshot contains all 2,977 captured AAC bytes exactly at byte offset 512.
Two objects (`13e0`, `1460`) supply chronological 1,024-word channel blocks
from alternating pairs `7000/7400` and `7800/7c00`, copied in 64-word chunks.
Nine complete blocks per object and a partial tenth are retained; the first
eight pairs are compared with the independently decoded reference, without
shifting, scaling or selecting channels. Every observed first copied word
matches its source snapshot at the old cursor. This is sampled copy/material
evidence, not proof of every MVDD read or decoder-call-to-packet ownership.

The first block pair is zero on both sides. The next has the same
448-sample zero prefix and 576 nonzero samples per channel. The corrected
shifted-store DAGEN ordering removes the large pre-DMA differences; the
remaining small numerical differences are described below. This comparison
uses chronological source blocks, not a fitted serial-stream offset.
The bit-reader taps and RAM snapshots remain inputs to the processing
checker. Retire this material/copy probe only when equivalent direct read
and store captures replace those checkpoints.

Use the canonical recorded NAND, `bootplaystage`, an isolated working
directory and absolute paths as for playback, adding:

```sh
-autoboot_delay 0 -autoboot_script tools/mu4_playback_frame_observe.lua -seconds_to_run 2
python3 tools/mu4_playback_frame_check.py RUN_DIR \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00 \
  --reference-aac captured-eight-packets.aac --reference-pcm host-eight-frames.s16le
```

A separate 59-word witness pins `023d73`'s input setup, the processing call
`023de5 -> 024610`, its nonzero-result exit, and the following 1,024-iteration
two-channel conversion loop through `0248a6` with argument 14. The bounded
runtime proof follows below. With OVLY set, low logical addresses
such as `02:3d73` execute from physical page zero; disassembling raw `02:3d73`
can show unrelated zero backing RAM. Source and retained Execute identity,
not such a raw debugger view, remain authoritative.

## Processing And Conversion

The first eight original `023de5 -> 024610` calls return zero and each
completes a 1,024-sample, two-channel conversion loop. Their net consumption
is 373 bytes followed by seven 372-byte packets: exactly the eight declared
ADTS extents. Each consumed byte matches the independent reference through
the entry reservoir or an actual in-call bit-reader bus read, not merely a
header/payload-prefix comparison. There are 186 such word reads per call.
The last three prefetched bytes lie outside the eight-packet reference;
they remain unconsumed reservoir lookahead and are not validated content.

Byte position is `2*B331 - remaining_bits/8`. The first call advances from
512 to 885 in the initial buffer. Subsequent calls carry three header bytes
in the reservoir and advance from -3 to 369 after compaction. This establishes
packet boundaries without waveform alignment or a guessed file offset.
Conversion destinations alternate `7000` and `7800`, with the second channel
at destination+`0400`. Returned destinations match later copy snapshots,
after the corresponding output loop and before the next processing call
that can reuse the same buffers.

FCALL has already reserved its two stack words at Execute entry. The observer
therefore normalizes caller SP as observed SP+2; the processing A argument
is caller SP+6. Sampling the unadjusted stack word would read the return page,
not converter argument 14. This is a pipeline observation constraint, not a
firmware ABI or scheduling correction.

A 152-word witness pins `0248a6`, its `024e38`/`024d43` shift composition
and the selected arithmetic-right-shift helper. For the observed argument
14, the independent scalar contract is:

```text
rounded = (signed32(raw) + 8192) >> 14
output = clamp(rounded, -32767, 32767)
```

Rounding ties go toward positive infinity. The source explicitly normalizes
to signed 32-bit; zero-extended negative DLD inputs are valid even when the
40-bit accumulator guard is zero. This is not a license to alter general
accumulator signedness or other helper arguments.

All 16,384 observed conversions in the eight successful blocks follow
the independent rounded-shift/clamp model and reach their later pre-copy
source buffers unchanged. The large differences were upstream: shifted
stores could resolve an implicit pointer update after a younger load had
already resolved its address. The converter, copied samples and input
packets were not changed to fit the host reference.

Use the same passive two-second run and independently regenerated reference:

```sh
python3 tools/mu4_processing_check.py RUN_DIR \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00 \
  --reference-aac captured-eight-packets.aac --reference-pcm host-eight-frames.s16le \
  --require-eight-blocks
```

The checker requires call/return order, zero results, original Execute
identity, complete packet-byte coverage, conversion-loop extents, buffer
lifetimes, all sampled conversion values and later RAM correspondence.
The strict option rejects legacy two-block captures; those remain readable
only with their smaller coverage explicitly reported. Tests include drift
in the last conversion and the eighth block's later copy buffer.
All 16,384 converter input payloads also match the final-tail vectors.
The four native word/slot streams and all 858 existing arithmetic snapshots
remain byte-identical to the smaller capture. The optional host-reference
error statistics are diagnostic, with no fitted alignment or assumed codec
tolerance. This proves bounded eight-packet binding and conversion,
not correct decoded audio, all media, analog output or native speech.

## Native Transform Arithmetic

The selected short-block driver takes parameter `0080`: 128 real/imaginary
32-bit components, or 64 complex points in 256 data words. This geometry
comes from its source loop bounds, not an imported library prototype.
The wrapper calls pre-transform `023f05`, bit reversal `023fc5`, transform
driver `023fee`, then post-transform `023f68`.

The passive Execute observer captures every short and long vector in the
first eight packet-bound calls: after bit reversal, combined stages 1-2,
stage 3, each weighted stage and the driver return. Coefficients are
captured separately at `f400..f7ff` and `117a/117b`. The `117b` pointer
names the coefficient's low halfword; starting a pair there would capture
the wrong constant.

`tools/mu4_transform_check.py` pins 645 source words across the wrapper,
bit reversal, controller and three kernel bodies. It verifies snapshot
identity, geometry, hashes, enclosing processing call and stage order.
Independent integer formulas reproduce every component at every captured
stage for all 30 vectors, including source truncation points and return
preservation. The 192 checked kernel-stage vectors comprise 124,928 signed
components. The weighted real cross terms share a shift before high-half
products are added; separately truncating them changes low bits. The source
deliberately omits low*low products, so a full-precision multiply is not the
bit-exact reference.

```sh
python3 tools/mu4_transform_check.py RUN_DIR \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00
```

The large discrepancy was a core address-ordering defect, not a missing
DSP peer or fitted sample conversion. Shifted STL/STH resolved their
Read-owned implicit DAGEN update after a younger load's Access. An adjacent
DLD could therefore use the old pointer while the older store itself was
retargeted by the younger update. The isolated `indirect-dld` profile first
failed at shifted-store variant 96: address `1001` rather than `1002`.
Ordinary-store controls passed. Resolving the older implicit update before
younger Access fixes this while preserving Execute-owned data publication.
All 1,152 composition cases/restores now pass, followed by 23,040 indirect
DLD cases. Explicit mapped-AR writes, delayed repeats and paired arithmetic
retain their separate regression gates and partial timing claims.

The shared core also passes `make verify-5110-menu` after this correction:
the native NSE-1 ROM4 handset reaches its exact Phone book menu oracle through
physical input, without a DSP reset request or illegal instruction. This is
a cross-consumer regression, not validation of MU4 audio or ROM4 speech.

The same change removes the 31/29 mismatched components in the selected
third-stage vectors. The first nonzero PCM block's maximum host-reference
error drops from 43,740 to 7 sample units, RMS from 11,553.09 to 0.8426.
Across the eight packet-bound source-block pairs, maximum error is 13 and
RMS 1.1492 over 16,384 signed samples. These comparisons use neither fitted
alignment nor gain. They do not define a codec-conformance tolerance.

### Audio acceptance prerequisites

Keep the captured-recording comparison distinct from a normative decoder
test. [ISO/IEC 14496-26:2024](https://www.iso.org/standard/86376.html)
describes audio-conformance tests with compressed examples and reference
waveforms in an electronic annex. All eight captured ADTS headers identify
MPEG-2, profile bits 1 (LC), sampling index 4 (44.1 kHz), channel
configuration 2 and one raw-data block per frame. The MPEG-4 suite must not
silently be assigned to this MPEG-2 stream; select the applicable procedure
and establish supported payload tools separately. The present packets and
FFmpeg output are not that official suite.

[ETSI TS 126 406, section 5.1](https://www.etsi.org/deliver/etsi_ts/126400_126499/126406/19.00.00_60/ts_126406v190000p.pdf)
requires at least accuracy K=15 for its AAC-LC RMS/LSB criterion and refers
to ISO conformance procedures. This is an enhanced-aacPlus conformance
specification, not proof that the MU4 implements that target. Neither the
observed maximum of 13 signed sample units nor RMS 1.1492 can be substituted
for its criterion without the specified reference precision, normalization,
evaluation intervals and test sequences.

The [AAC validation contract](aac_validation.md) now pins an acquired official
baseline and draft procedure. Before adding an audio-acceptance threshold,
establish applicability, decoder configuration, delay and measurement point,
and implement the comparison rules independently. Broader original-media
coverage and exact DMA/TX/DIN epoch pairing remain separate requirements;
passing an arithmetic snapshot or this handset regression closes neither.

The passive `--inventory` report observes 30 wrapper entries
across the eight packet-bound processing calls: call 2 has sixteen 64-point
vectors (`7800..7f00` and `e800..ef00`, step `0100`); each other call has two
512-point vectors. The first long vector alternates between `7000` and
`7800`; its companion is `e800`. Entry identity, source opcode, parameter,
vector bounds and enclosing call times are checked. This is execution
coverage; the snapshot checker separately validates all 30 arithmetic paths.
The long driver
uses the same early kernels followed by additional weighted stages at
return sites `02408b`, `0240a6` and `0240be`; its wrapper return is `024946`.

```sh
python3 tools/mu4_transform_check.py RUN_DIR --inventory \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00
```

The current snapshot format uses invocation-numbered files and records the
processing call, parameter, vector geometry and source opcode. It captures
312 snapshots, including per-invocation coefficients and constants. The
checker rejects missing/duplicate stages, altered identities or files,
overlapping invocations and changed return vectors. Synthetic short/long
fixtures include rehashed arithmetic corruption; independent inverse-DFT
impulses sanity-check the model at both sizes without defining a codec
tolerance. The four native word/slot streams and the original eighteen
selected snapshots reproduce the smaller proof byte-for-byte.

Remaining work is understanding the small PCM residual against a floating-point decoder,
coverage beyond these eight calls, and complete digital
transport alignment. Analog output and native speech remain unvalidated.
The earlier per-word arithmetic exclusions were valid for corrupted inputs;
they are superseded by this stage-level contract, not current frontiers.

TI's [SPRA554B double-precision C54x FFT report](https://www.ti.com/lit/an/spra554b/spra554b.pdf)
is an algorithm reference, not an identification of this binary. Its
32-bit data layout and specialized early stages corroborate the recovered
shape. Its scaling-flag descriptions on PDF pages 4 and 5 conflict; the
firmware's own branches remain authoritative. The ignored local PDF is
`roms/reference-docs/npm5/ti_spra554b_c54x_double_precision_fft.pdf`, SHA-256
`0dafa8acb794d328a6835cb718f93d367884adf15bd3e540d9ecd7e7e6fba7a0`.

### Post-Transform Rotation And Permutation

The original `023f68..023fc4` routine rotates each complex pair using two
coefficient halves, then reverses the imaginary components in place. Its
split-word products omit low*low terms. Unlike the weighted FFT kernel,
each cross-product sum is shifted separately before the two products are
combined. A full-precision complex multiply is therefore not bit-exact.

The passive observer captures input, coefficients and output for every one
of the same 30 transforms, using their existing invocation and packet-call
identities. Short coefficients occupy `f200..f2ff`; long coefficients occupy
`f000..f7ff`. These are observed operands, not fitted constants. Every input
is byte-identical to the checked FFT return. Source-derived rotation and
imaginary reversal reproduce all 16,384 output components across sixteen
short and fourteen long vectors, with no mismatches.

```sh
python3 tools/mu4_transform_check.py RUN_DIR --post \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00
```

This mode pins 738 original source words, including the post-transform's
93-word body. It requires complete FFT acceptance, exact input continuity,
all post snapshots and their geometry, hashes and chronology. Tests include
rehashing a wrong output and comparing split products against independent
full-precision bounds; neither supplies a PCM tolerance. Pre-transform,
window/overlap processing, other media, waveform conformance, analog output
and native speech remain outside this mode's acceptance.

### Pre-Transform And Bit Reversal

The original `023f05..023f67` routine consumes alternating components from
the front and back of the input. Its split-word products have the same
omitted low*low terms, but it negates the first product before an arithmetic
half-shift. The other three products are half-shifted individually; only
then are the stored values shifted left three. Moving a half-shift after
the store, or negating an already rounded product, changes low bits.
The routine's final imaginary reversal restores the paired output order.

The passive observer captures all 30 inputs, coefficient tables and outputs,
using a shared pre/post snapshot implementation. The source-derived model
matches all 16,384 pre-transform output components. An independent complex
index bit reversal then matches every component of the captured FFT inputs.
These are separate checks: consistently rehashing a wrong arithmetic output
or changing only the permutation still fails.

```sh
python3 tools/mu4_transform_check.py RUN_DIR --pre \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00
```

This mode pins 837 original source words and requires pre-transform,
bit-reversal, complete FFT and post-transform acceptance together. Scalar
tests independently bound the omitted low*low error against full signed
products, including the negative half-shift, at both supported sizes.
The result validates these 30 arithmetic invocations, not spectral input
correctness, window/overlap processing, all media, waveform conformance,
analog output or native speech.

### Window-Product Helper

The `024a70` call invokes `025bb3..025c19`. Its outgoing count and
coefficient pointer are independent of the preceding FFT parameter. The
helper rotates mirrored left/right sample pairs using forward/backward
coefficient walks, then reverses each half-buffer in place. Each split-word
product is truncated before the products are combined. Captures use the
actual helper count; a long FFT does not imply a long window operation.

`tools/mu4_window_check.py` pins this 103-word body and its 15-word caller
setup in addition to the complete transform-chain witnesses. It checks
input/coefficient/output geometry, hashes, chronological call identity and
the full mirrored-product/permutation model. The helper's input is observed
after scaling and branch-specific operations; it is not claimed identical
to the post-transform output. Dispatch behavior, overlap carry and
final buffer movement remain separate contracts to establish.

All 30 captured helper operations match: eighteen 128-component windows and
twelve 1,024-component windows, totaling 14,592 signed output components.
Two short windows follow long FFTs. The shared snapshot writer avoids duplicating file and hash
logic across these observer boundaries.

```sh
python3 tools/mu4_window_check.py RUN_DIR \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00
```

The combined checker pins 955 source words. Tests include both window sizes,
mixed FFT/window geometry, full-product bounds, metadata corruption and
consistently rehashed wrong arithmetic. This helper acceptance does not
establish correct selection of its inputs, complete overlap-add behavior,
decoded waveform conformance, analog output or native speech.

### Transform Scaling

Caller `024a46` sends the full post-transform vector through `025c1a`,
which applies `025ce2` to each signed 32-bit component. The scalar subtracts
two signed 16-bit arguments: a positive difference repeats arithmetic
right shifts; a negative difference repeats left shifts. The final store
keeps 32 bits. This is distinct from the rounded/clamped PCM converter.

All 30 captured inputs are byte-identical to the checked post-transform
outputs. Independent shift/store arithmetic matches all 16,384 resulting
components. Observed argument pairs are `(10,14)` fourteen times, `(12,14)`
eight times, `(7,14)` six times and `(27,14)` twice, covering both shift
directions.

```sh
python3 tools/mu4_window_check.py RUN_DIR --scale \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00
```

This combined mode pins 1,042 source words, checks each argument's identity
and invocation, exact post-transform input continuity, and complete scale
outputs before window processing. Tests cover zero/positive/negative shifts,
signed arguments, 32-bit wrap, malformed metadata and consistently rehashed
wrong outputs. This mode alone does not establish callback or tail behavior,
other media, decoded waveform conformance, analog output or speech.

### Window Callback Buffer Exchange

The two indirect calls at `024a60` and `024a63` select these source-pinned
entry pairs from the observed mode:

| Mode | First callback | Second callback | Observed invocations |
| --- | --- | --- | ---: |
| 0 | `024af8` | `024955` | 10 |
| 1 | `024b13` | `024955` | 2 |
| 2 | `024b3e` | `02496e` | 16 |
| 3 | `024b59` | `02496e` | 2 |

Both calls, actual callee entries and returns are observed for all 30
invocations. The first callback exchanges signed 32-bit samples with a
16-bit buffer: modes 0/1 use 512 history words, mode 2 uses 64. New history
uses `((signed32 + 0x8000) >> 16) & 0xffff`, in reversed order. Old history
is sign-extended and shifted left 16 into the raw buffer. Mode 3 additionally
copies 64 reversed old history words into raw component indices `896..959`
and uses forward old-history order in the first 512 raw components.

All 16,384 raw components and 8,192 history words match the independent
buffer model, including unchanged regions.

```sh
python3 tools/mu4_overlap_check.py RUN_DIR \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00
```

The combined checker pins 1,347 source words, verifies both dispatch targets
against actual entry/return observations, and checks first-callback buffers.
Tests cover all four modes, signed history, rounding boundaries, untouched
regions, dispatch corruption and consistently rehashed wrong outputs.
This mode alone does not validate second-callback memory effects, complete
writer ownership between calls, later output binding, other media, decoded
waveform conformance, analog output or speech.

### Final Buffer Tails And History Continuity

The mode-specific tail at `024a7e..024af7` runs once per channel object,
sixteen times in the eight checked calls. Modes 0/1 leave these buffers
unchanged. Mode 2 publishes 448 rounded raw values into the first part of
the 16-bit history buffer, moves the old history into the raw prefix and
repositions the remaining raw values through the 128-component saved buffer.
Mode 3 swaps and shifts the saved/raw ranges, then reverses the moved range.
The ordered moves matter because the source and destination ranges overlap.

Ninety-six snapshots cover each tail's complete 1,024-component raw vector,
512-word history and 128-component saved buffer before and after execution.
All 26,624 output values match the source-derived buffer model. Together
with the first-callback exchanges, these tails account for all 15,360 reads
whose history words were previously observed: no differences remain. The
first 1,024 history reads are initial-state observations, not a validated
provisioning contract. This is continuity at captured boundaries, not proof
that no writes occur between them.

```sh
python3 tools/mu4_overlap_check.py RUN_DIR --tail \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00
```

This combined mode pins 1,469 source words. Tests compare the tail formulas
against ordered overlapping copies, inject continuity breaks and reject
consistently rehashed wrong outputs. All 16,384 converter inputs match the
eight paired final-tail blocks exactly at their 32-bit payload boundary.
Valid zero/sign extension is checked separately; comparing signed 40-bit
register values directly would invent differences on zero-extended DLDs.

Second-callback memory effects, complete writer coverage,
other media, spectral input correctness,
decoded waveform conformance, analog output and speech remain unvalidated.

### Coefficient Cache Publication

The second window callback publishes coefficients using two original
finite-difference decoders. `025c35 -> 025ffa` integrates signed word
corrections through three recurrence states; `025c6a -> 026038` integrates
four high-to-low signed nibbles per correction word through four states.
Each descriptor supplies an explicit prefix, initial states and correction
stream. The host model uses those original upload words, not fitted curves.
The checked descriptors stay in a non-saturating 32-bit domain; other
descriptor and overflow domains are not accepted by this model.

| Descriptor | Coefficients | Correction format | Observed loads |
| --- | ---: | --- | ---: |
| `bbe6` | 1,024 | Packed signed nibbles | 2 |
| `c4e8` | 1,024 | Packed signed nibbles | 10 |
| `bc72` | 128 | Signed words | 18 |
| `c572` | 128 | Signed words | 2 |

Callback `024955` writes `bbe6` at `f000` when its first flag is zero,
otherwise `c4e8`. Callback `02496e` writes `c572` at `f100` when either flag
is nonzero and writes `bc72` at `f000` when either flag is zero. Flags are
observed at the callback's actual argument pointer, not selected by the
harness. The other cache values are preserved in the expected image.

Sixty snapshots cover `f000..f7ff` before and after all 30 callbacks.
Every one of the 61,440 resulting cache words matches the source-derived
publication model. Every captured coefficient operand of the following
window helper is byte-identical to its published cache slice. The four
native word/slot streams and all 858 earlier arithmetic snapshots remain
unchanged; the strict eight-block processing/conversion check still passes.

```sh
python3 tools/mu4_coefficient_check.py RUN_DIR \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00
```

Run this alongside the processing checker with `--require-eight-blocks`.
The combined coefficient check pins 1,750 source words. Tests compare both
recurrences with independent binomial integration, exercise the selector
truth table and reject rehashed corruption in preserved cache regions.
Generator scratch effects, complete writer coverage, unseen decoder/media
paths, independent waveform criteria, analog output and native speech
remain outside acceptance. Cache equality is not absence-of-writes evidence.

## Resident reload contract

The common AA55 API at `00:2080` branches to disk loader `00:3538`.
The 20-second run observes 24 actual API entries. Its bounded first 16 trails
alternate descriptor 0 and descriptor 1. The initial AA55 caller is `00:09b6`;
subsequently AA99 `02:959b` passes A=0 for
`MCUSI16`, while AA22 `02:91c7` passes A=1 for `RELSI16`. All 12 resident
trails show loader `00:3545 -> 00:2000`, followed by the loaded entry's
`00:2002 -> 02:6d62`. This is not an arbitrary FRET into startup or a reset
vector re-entry. Retained executed words identify the calling overlay;
current memory at the same numeric address is not a substitute.

AA99 `02:956f..959c` compares lifecycle `[373e]` with 4 and skips this
cleanup when unequal. `7212 3742` at `02:9575` is **MVDM $3742,AR2**:
the shared word is an input control, not a published worker result. The
AEQ precheck tests control 1; long-offset BANZ then tests
`(AR2 - 2) & ffff` without modifying AR2 (SPRU172C BANZ example 3).
With lifecycle 4, exactly control 2 selects state 2 and an additional call
to `03:9005` with caller `[SP+1]=001a`; all other 65,535 controls
select state 5. Both paths join the mount and descriptor-0 reload sequence.
No nonzero error/control value is required for the observed state-5 path.

`mu4_context_contract.py` pins all 46 AA99 words and the two AA55 API words,
enumerating all 65,536 controls. It makes no EOF/error or PCM-content claim.
The passive three-second Lua observation independently records lifecycle 4
at 0.837819684 seconds, a control read of zero at 2.198474888, state 5 at
2.198475021, and state 4 again at 2.589543040. Its terminal PC is `02:5beb`,
`illegal=0`, with 11 lifecycle writes, three control writes and five control
reads. GENPC selects a bounded read-context range; it does not establish
Access/Read ownership. The data-write tap is not an all-alias writer census.
MVDM, long BANZD, TC-branch and absolute-CMPM core controls also pass.

Use the same fresh copied benchmark NAND, seed ROM path and isolated working
directory as `check-mu4-bootstrap-original`, adding
`-autoboot_delay 0 -autoboot_script tools/mu4_playback_lifecycle_observe.lua`
with an absolute script path and `-seconds_to_run 3`. This short observation
does not run the separate 20-second acceptance diagnostic:

```sh
python3 tools/mu4_context_contract.py roms/5510-mu4-reference/InitData_R060.a00
python3 tools/noki5510_bootstrap_check.py --playback-lifecycle RUN_DIR/error.log
```

Next: follow REL frame/format ingress and the last producer feeding `7000`,
with an independently valid media control where the original container
contract permits it. Then pair valid McBSP words and codec frames before comparing PCM
against the stored media. Do not remove the programmed reload merely to keep
resident-entry counts at one.

The current broad CPU regression and remaining operand-domain audit are
summarized under [Operand-domain acceptance](c54x_execution_contracts.md#operand-domain-acceptance).
The long-PSHD acceptance remains its three scoped profiles, with separate
RPT, stack/repeat and PMST controls. These checks do not establish physical
bank timing, all legal/illegal operand domains or decoded audio.

The external request, saved file and peer replies remain unchanged. The
original REL diagnostic fails closed with `audio_acceptance=0`.

```sh
python3 tools/run_c54x_core.py --profile mvkd-sp \
  --mame mame/mame --rompath roms --output-root tmp
python3 tools/run_c54x_core.py --profile rptb-fcalld \
  --mame mame/mame --rompath roms --output-root tmp
python3 tools/run_c54x_core.py --profile mvkd-forward \
  --mame mame/mame --rompath roms --output-root tmp
python3 tools/run_c54x_core.py --profile rpt-smem \
  --mame mame/mame --rompath roms --output-root tmp
python3 tools/run_c54x_core.py --profile rpt-ld \
  --mame mame/mame --rompath roms --output-root tmp
python3 tools/run_c54x_core.py --profile stlm-pmst \
  --mame mame/mame --rompath roms --output-root tmp
python3 tools/run_c54x_core.py --profile pshd-long \
  --mame mame/mame --rompath roms --output-root tmp
```

```sh
make check-mu4-bootstrap-original \
  MU4_BOOTSTRAP_BIOS=bootplaystage \
  MU4_BOOTSTRAP_SOURCE_RUN=/path/to/completed-retained-run \
  MU4_BOOTSTRAP_INPUT_NAND=/path/to/bootrecmeta-run/mu4_recorder_endpoint.nand
```

This diagnostic deliberately returns failure while audio completion is
unvalidated. Even if later CPU coverage passes the present stop, it cannot
silently become a playback gate. It preserves the source NAND and claims
neither REL audio validity, completed decoding, analog output nor handset boot.

## Playback Word Capture

The bench records complete words at the existing actual channel-3 source
read, McBSP TX and codec DIN callbacks, without extra guest memory reads or
feedback. Files are raw little-endian words, not a claim about stereo frame
phase or analog audio. Output is fresh-run only: file handles are not
save-state artifacts, and restored capture integrity is unsupported.
`tools/mu4_playback_capture_check.py RUN_DIR` verifies byte/word counts,
observed word hashes, nonzero counts and the existing strict DMA observation.
New captures also write `mu4_playback_din.slots`: one packed nine-byte record
per DIN word, little-endian unsigned 64-bit emulated nanoseconds followed by
the callback's actual slot byte (0 or 1). This observes the existing callback,
not extra guest reads or a changed serial clock. `--require-framing` requires
its terminal record, count and raw-byte FNV hash; legacy word-only captures
remain explicitly identified as lacking slot evidence. File save/restore is
unsupported for both outputs.

New captures additionally write `mu4_playback_tx.times`, one little-endian
64-bit emulated-nanosecond timestamp per existing completed-TX callback.
The terminal record binds count, byte extent and raw-byte FNV hash.
`--require-tx-times` requires both TX times and validated DIN slot metadata.
Pairing selects the next chronological DIN callback within one BCLK period
(84 ns at this fixture's 11.9952 MHz codec clock); it never chooses by sample
value, searches for a matching word or fits an offset. The checker reports
unpaired TX and unmatched DIN separately. Legacy captures cannot establish
this timestamp-pairing result; file save/restore remains unsupported.

The post-DAGEN-correction twenty-second replay completes with illegal=0
and completed parent dispatch. Its numerical streams are deliberately
re-banked after the evidenced CPU fix; they are not the pre-fix oracle.
All four word/slot files reproduce the shorter proof run as exact prefixes,
and both selected complete transforms reproduce at every stage.
Its complete stream identities are:

| Boundary | Words | SHA256 |
| --- | ---: | --- |
| DMA source | 1,631,772 | `dc9662f5705b6ac7b1f16dadcab9587ef456e9e4de24a3be5abc48c0a03e43a0` |
| McBSP TX | 1,661,065 | `4e6ddc62d886f62883211216425dc3ba7401b37d05ef0e5a19ff4b64ce0c63d8` |
| Codec DIN | 1,691,842 | `cafaec6eefc94f7b4789db241470858b17209f20176ad8fe30fce81e2193b684` |

Chronological first-nonzero anchors, checked over 64 words, give initial
DMA-to-TX offset 92 and TX-to-DIN offset 367. The first 783,108 and 783,200
overlapping words respectively match exactly, including zero words.
Thereafter a single-offset comparison diverges. The codec sidecar rules
out a DIN slot discontinuity; the first disagreement is the underflow
interval described below, not evidence of a corrupted transmitted word.
Later source/TX reset and direct-write epochs still require alignment.
The checker reports the complete mismatch extent, not a best-fit realignment,
and never promotes integrity or overlap to decoded-audio acceptance.

With verified slot metadata, the checker also reports the actual DIN time
and slot for each retained fixed-offset mismatch. In this capture the first
TX-to-DIN difference is source index 783,200 / sink index 783,567 at
9,702,093,026 ns, slot 1 (`ffbc` versus `0000`). This precedes the next
logged system-clock change at 9.869008605 s; that later change cannot explain
the onset.

The passive transport observation brackets DXR writes at `0023` from
9.698 through 9.876 s and records every McBSP0 indexed configuration write
at `0038/0039` from boot. The last DXR write before the gap is `ffbc` at
9.702069134 s. There are no further DXR writes until the observed SPCR2
reset (`0102`) at 9.871026174 s; the next write is `0000` at 9.874720177 s.
All 14,900 DIN words from the first disagreement until that reset match
the modeled underflow pattern: slot 0 receives repeated `ffbc`, while slot 1
samples the held-low serial pin as `0000`. McBSP completes only the repeated
first word, whereas the codec continues sampling both slots. Comparing
these callbacks at one constant word offset therefore ceases to be valid.
This explains the first divergence without changing clocks or samples; it
does not establish later epoch pairing, analog behavior or decoder accuracy.

Two fresh ten-second runs reproduce the twenty-second capture as exact
prefixes in all four files. The full-interval observation records 147
configuration writes and 474 DXR writes, ending at 10.016666667 s with
illegal=0. `tools/mu4_transport_observe.lua` retains this recording-specific,
read-only observation preset; use it as `-autoboot_script` with the same
`mu4nand -bios bootplaystage` media fixture and a ten-second run. It is not
a generic transport-acceptance gate, a source of MMIO writes or a claim
that file-capture save/restore is supported. Next classify the remaining
reset/reload intervals before asserting complete DMA/TX/DIN pairing.

### Timestamp-paired serial callbacks

A fresh twenty-second run with the TX observer preserves the three word
streams and DIN sidecar byte-for-byte. All 1,661,065 completed TX callbacks
pair with the immediately following DIN callback: 525,994 at 41 ns and
1,135,071 at 42 ns, with zero value mismatches and zero unpaired TX words.
The TX timestamp file has 13,288,520 bytes, SHA256
`e59e55b476e7f0d280e814f2782c3a3cd6343ac95aeb099b008be56f658e270f`.
This covers the complete observed serial callback stream, including both
later reloads, rather than only its first fixed-offset prefix.

The remaining 30,777 DIN callbacks have no completed TX word in that clock
window. All have value `0000`: 737 in slot 0 and 30,040 in slot 1. Every
one follows a completed TX word and equals the uniform level implied by
that word's last bit. The checker reports this held-level consistency
separately from actual pin ownership, with no exceptions in this capture.
It does not infer the initial pin state when no TX word precedes a sample.
Their existence is not a failed transmitted-word comparison: the codec also
samples idle pins during underflow/reset. Only the first 14,900-word interval
has the independent DXR/configuration-write coverage above; the other 15,877
have value consistency, not a causal reset/pin-ownership proof.
This result does not establish channel-3 DMA-to-DXR
ownership, correct handling of every overwritten/interrupted transmission,
PCM accuracy, analog output or native speech. The original strict playback
completion check still exits rejecting audio acceptance at 20 s, illegal=0.

```sh
python3 tools/mu4_playback_capture_check.py RUN_DIR \
  --require-framing --require-tx-times
```

Unit tests reject malformed/duplicate metadata, changed hashes and extents,
non-increasing TX times and missing required sidecars, and report pairings
outside the clock bound as unpaired.
A wrong first word followed by a matching word stays a mismatch; an
underflow gap cannot shift subsequent pairings by index. Unmatched-sample
tests distinguish initial unknown pin state, held-high/held-low consistency
and contrary samples without dropping exceptions from the report.

The DIN sidecar contains 1,691,842 records (15,226,578 bytes), SHA256
`311d7ef1e63b60307fdbe9846d4220168b443334add930fab29580e905241bfa`.
It begins at slot 0, yields 845,921 complete pairs, and has zero orphan
words or slot discontinuities. Pair spacing is 1,333/1,334 ns and frame
spacing 22,675/22,676 ns, consistent with the modeled 44.1 kHz clock.
CPU clock changes do not interrupt that cadence. Slot metadata remains
byte-identical to the pre-fix baseline; numerical word hashes change as above.

### DMA source-to-DXR ownership

`mu4_playback_dxr.writes` records every observed full-word DXR bus write
after the AA99 playback selection: little-endian nanoseconds (8 bytes),
channel-3 source-read ordinal (4), word (2), active DMA owner (1), reserved
zero (1). Owner zero means no active DMA destination write, not an inferred
CPU instruction; owners 1 through 6 identify channels 0 through 5. Only
channel 3 carries a source ordinal. Ownership comes from the synchronous
DMA destination-write inspector, not matching sample values or GENPC.
The terminal record binds extent and raw-byte FNV hash. Like the other
capture files, this is fresh-run evidence without save/restore support.

The complete twenty-second observation contains 1,631,772 DXR writes,
all owned by channel 3. Every actual source-read ordinal occurs exactly
once, in order, with its original source value. No other writer appears
within this capture window; this does not assert that CPU code never
writes DXR in other profiles or before selection. A fresh run preserves
all three word streams, DIN slots and TX timestamps byte-for-byte.
The ownership file is 26,108,352 bytes, SHA256
`1da07329b20a74191a13ea61c91655347463e6e5ee29969b2b875e264239e1ad`.

```sh
python3 tools/mu4_playback_dxr_check.py RUN_DIR
```

The checker first requires capture integrity and both serial time sidecars,
then rejects missing, duplicated, skipped or out-of-order source ordinals,
wrong source values, foreign-owner ordinals, reserved bytes and malformed
metadata. Rehashed corruption tests exercise those semantic failures.
This proves the actual source-to-DXR bus transaction, not that the controller
accepted the write, preserved it through overwrite/reset, or generated exactly
one TX callback. Keep `dxr_to_tx_validated` false until buffer loads,
discarded writes and underflow repetition are causally accounted for.

The optional reset-aware sequence check consumes the full passive indexed
McBSP0 configuration observation from `mu4_transport_observe.lua`. It checks
the observation's terminal count, register-index continuity and endpoint,
then merges SPCR2 writes, owned DXR writes and completed-TX times in order.
It does not search by sample value. A clear XRST discards pending words and
clears retained DXR; writes while reset is asserted are ignored. Otherwise
pending writes feed completions in order, and an empty queue predicts the
retained word. More than two pending words stops as an unresolved overwrite
case rather than silently dropping one. Timestamp ties between event kinds
also stop rather than inventing their order.

For the complete capture this model accounts for 1,631,766 fresh-write
completions and 29,206 retained-word repetitions. The first 93 TX words
precede a reset within the capture and remain unattributed: their equality
to zero is not evidence of pre-capture write ownership. Together these
account for all 1,661,065 TX callbacks. Source ordinals `1`, `783109`,
`786383`, `1559251` and `1562525` are pending at resets; all five are zero.
Ordinal `1631772` remains pending at the endpoint. No observed write falls
inside asserted reset, and maximum pending depth is two. Thus all 1,631,772
source writes are accounted for without a fitted offset or omitted zero
samples.

```sh
python3 tools/mu4_playback_dxr_check.py RUN_DIR --check-buffer-sequence
```

This is reset-aware sequence agreement, not direct observation of internal
DXR-to-XSR loads or proof that zero-valued writes cannot have ambiguous
buffer histories. Keep that distinction from the actual bus-ownership and
TX-to-DIN callback proofs above. Tests exercise nonzero reset discards,
ignored writes, repetition, pending endpoints, wrong values, overfull queues,
time ties and incomplete control metadata. A capture with no checked
post-reset TX words fails instead of passing entirely as an unknown prefix.

### Independent Reference Comparison

The first 2,977 captured plaintext bytes form eight host-valid AAC packets,
SHA256 `3f0a2b7eafb22ede5b3e3f1bb54c83f2f959386bedd897ebebf218e6fb816ff4`.
FFmpeg decodes eight AAC frames at 44.1 kHz without errors, producing
8,192 signed stereo sample frames, SHA256
`440e479020b5e558b9aa792adc14af8bb4974f30898cba57f3189ac1434f8993`.
The regenerated host output matches the earlier reference byte-for-byte.

#### Complete-window host reference

`mu4_recorder_transform_check.py --output-projected-aac PATH` can explicitly
extend the single state from a freshly verified recording prefix across
complete 32-byte saved-file windows. It first requires all captured states
to agree and the inverse to reproduce captured plaintext. It never models
a partial final window or claims that later native state was observed.
Output is a new host-only artifact, not an emulation input; existing files
and input evidence cannot be overwritten. Without this option the checker
remains read-only.

For the post-DST recording identified above, the unique origin is 622.
The projection covers 143,040 bytes, with 18 final bytes left unmodeled.
It finds 384 consecutive complete ADTS header/length records totaling
142,849 bytes, SHA256
`812882ea5da3fb9051a04e421922694366e580e75ae98df469936426eef1a873`.
All headers declare MPEG-2 LC, 44.1 kHz stereo and one raw-data block.
Another 191 projected bytes follow that chain and are not accepted as AAC.
Header syntax alone is not payload validity: FFmpeg decodes 383 frames,
but rejects packet 384 at byte 142,477 (nominal extent 372) with an exhausted
data-stream element. Its permissive whole-input exit zero is not a pass.
Partial-final-packet/container handling and later native state remain
unresolved; do not repair the payload or assume either explanation.

The explicit `--project-packet-limit 383` output contains the first
142,477 bytes, SHA256
`0a3091469463f5937df4be728e6edc0af087c21a06a3f8097713522cc4cb2b8e`.
Its report still inventories all 384 header-delimited packets and labels
the selected output prefix separately. FFmpeg with `-xerror` decodes this
isolated prefix with no diagnostics into 392,192 stereo sample frames
(1,568,768 bytes of signed LE16), SHA256
`000e716c4dbe30e78d137706be5621aefb271059d343bf5ab793f1406706dfab`.
Its first eight frames are byte-identical to the earlier 32,768-byte host
reference. These are independent host-decoder results for a declared model
projection, not native output acceptance beyond the captured prefix.

```sh
python3 tools/mu4_recorder_transform_check.py RECORDING_TRACE EXTRACTED_TEST \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00 \
  --output-projected-aac NEW_PREFIX.aac --project-packet-limit 383
ffmpeg -v error -xerror -i NEW_PREFIX.aac -f s16le -acodec pcm_s16le NEW_REFERENCE.s16le
```

Whole-session return observation below extends the chronological comparison;
its termination decision still needs classification. Do not infer a waveform
offset or gain from first nonzero samples: guest post-conversion processing
remains separate from decoder conversion blocks.
It is analysis output, never media supplied back to the emulation.

```sh
ffmpeg -v error -i captured-eight-packets.aac -map 0:a:0 \
  -c:a pcm_s16le -f s16le host-eight-frames.s16le
python3 tools/mu4_playback_capture_check.py RUN_DIR --require-framing \
  --reference-pcm host-eight-frames.s16le --reference-rate 44100
```

The capture checker's optional first-nonzero-frame comparison is diagnostic
only: it does not establish source-packet/output identity and never selects
a fitted lag, scaling or channel permutation. Use the stronger
[processing proof](#processing-and-conversion) to bind the first eight input
packets and locate the differing values before conversion. The large
processing corruption is corrected; the remaining small fixed-point/float
residual needs broader coverage and independent acceptance criteria, not
a guessed serial phase.

#### Native session input coverage

`mu4_packet_reads_observe.lua` captures actual data-bus reads in the original
bit-reader context over 0.8--10 seconds, bounded to `162e..1e2d` and 131,072
records. `mu4_decoder_call` entries additionally expose the existing input
base snapshot; the eight-call detailed arithmetic budget is unchanged.
`mu4_packet_session_check.py` composes the complete session/capture and
eight-call arithmetic checks with the existing reservoir/bus-byte proof.
It starts at reference offset zero, requires each call's net byte consumption
to equal the next declared ADTS packet length, and requires coverage of every
consumed byte. It never searches for a favorable offset or supplies missing
bus bytes from the reference.

All 375 native calls match the first 139,501 projected reference bytes,
SHA256 `e94a84480d87edbb2a00ed57366cf4a1a6c6ec5dde621a17199933964b46c729`.
The closed capture has 73,381 reads, of which 69,750 are within the checked
calls; the other 3,631 are explicitly reported outside-call observations.
Reservoir prefixes and actual reads cover every byte of the checked packets.
All these headers specify one raw block, and every call produces one complete
stereo conversion block. This closes input-packet correspondence for the
375-call prefix, not retained operand or complete output-store ownership.

The passive linked-buffer instrument `mu4_buffer_flow_observe.lua` observes
three initialized descriptors from 0.825 through ten seconds: producer
`1622` -> transform `1616` -> decoder input `160a`. Their upstream/downstream
links agree with the complete source-pinned refresh routine. One fresh run
records 43772 bus writes and 20766 changed values without caps or CPU faults;
identical full-word writes are counted but deduplicated from the event log.
`mu4_buffer_flow_check.py RUN --source-a00 .../InitData_R060.a00` checks the
scope, topology, domains, write order and complete observation terminal.

There are 376 pending-request changes at `15e0` in context `02068e`, versus
375 clears in context `02088b`. The last request is `00ba` at 9.700572343s.
The preceding source-node advances and consumer compaction remain live
through all 375 decoder calls; this is not a generally absent refill peer.
At the subsequent existing Execute-entry gate capture, decoder input is
`162e..1d46` (1816 words, above the `0400` admission threshold), but both
activity fields are zero. The remaining boundary is software end/drain
ordering, not lack of buffered input. Context PCs on data writes remain
correlations, not retained instruction ownership.

Later writes of `ffff` to `12fb/12fc` at about 9.859s have low-PC contexts
`002faf/002fb2`; do not reinterpret those reused addresses as new AA99
control messages. The observation closes at ten seconds, while the original
twenty-second diagnostic still ends with its mandatory audio-completion
failure. This capture does not extend audio acceptance. Use a unique run
directory and confirm child-process termination before reusing evidence
paths; the checker rejects NUL-corrupted logs and incomplete terminals.

The 142,849-byte reference retains 3,348 unconsumed bytes with nine complete
ADTS headers. Header completeness is not payload validity: the strict host
reference has eight further decoded blocks and rejects its final packet.
The tail remains outside native processing acceptance. The existing maximum
sample difference 13 / RMS 1.24083 is unchanged and is not a selected codec
tolerance. No native whole-file, analog-audio or speech claim follows.

Run the isolated `bootplaystage` diagnostic with
`-autoboot_script <absolute-path>/tools/mu4_packet_reads_observe.lua` through
its twenty-second terminal, then:

```sh
python3 tools/mu4_packet_session_check.py RUN_DIR \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00 \
  --reference-aac PROJECTED_REFERENCE.aac --reference-pcm STRICT_HOST_PREFIX.s16le
```

The diagnostic's deliberate audio-completion failure remains mandatory;
packet-prefix validation is separate. Wrong bytes, missing reads, changed
scope, caps, failed calls and partial terminals fail closed. Retain this
bounded instrument until end-of-input policy is understood; do not expand
the arithmetic snapshot budget or inject a completion to consume the tail.

#### Native session return coverage

The compact observation follows at most 512 processing calls in the first
verified AA99 session, independently of the eight-call detailed arithmetic
budget. It logs each entry/return and completed conversion block, and writes
the returned converter low words as interleaved signed LE16. These are
Execute-entry observations before the caller's store, not proof of retired
stores beyond the separately verified eight-call prefix. The terminal record
binds call/return/block counts, byte extent, raw-byte FNV hash, pending state,
cap status and observed contract errors. Capture-file save/restore is unsupported.

The completed twenty-second run records 375 calls, 375 zero-status returns
and 375 complete 1,024-sample stereo blocks, with no bad/pending/capped state
and illegal=0. `mu4_decoded_returns.le16` contains 768,000 signed words,
1,536,000 bytes, SHA256
`1871f924d9c12dca42bcced964dfe1ee0a42165574f5d4e424fbf284fdfb26dc`.
It reproduces the shorter run exactly. The six existing word, slot, time
and ownership captures remain byte-identical, as do all 918 detailed
arithmetic snapshots; the bounded coefficient/transform checks still pass.

Against the first 375 blocks of the 383-packet host reference, 391,341 of
768,000 signed samples differ, maximum absolute difference 13 and RMS
1.2408299054. The consumed-input proof above now binds this packet prefix;
these remain unfitted numerical diagnostics, not a chosen audio tolerance
or complete later output-store binding. No nonzero processing status
occurs before termination, so the rejected host packet 384 does not directly
explain this session ending at 375 calls. The host reference has eight
additional blocks; do not silently truncate that discrepancy or call it a
decoder failure. The mapped input-length cleanup still needs its legitimate
end-of-input lifecycle classified; whole-file completion, output-store binding
beyond the detailed prefix and independent codec criteria remain open.

```sh
python3 tools/mu4_decoder_session_check.py RUN_DIR \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00 \
  --reference-pcm STRICT_HOST_PREFIX.s16le
```

The checker requires finalized capture integrity, the preserved eight-call
arithmetic checks, ordered call/return/block identities and matching PCM
extent/hash. Reference comparison stops at the first failed native call;
later successes cannot collapse its missing position. Return status uses
the caller's low word, retaining upper accumulator bits only as context.
Tests reject partial/pending/capped captures, wrong ordering/times/scope,
changed counts/hash and missing successful blocks. Waveform disagreement
remains an explicit diagnostic, never fitted away or labeled conformance.

#### Dispatch and termination boundary

`decoder_dispatch` in `mu4_context_contract.py` pins 99 additional AA99 words
around the original caller and ABI. Receiver reference `15eb` selects table
`c686`; method index 13 uses the far pointer at `c6a0`, `0002,3d5d`.
Wrapper `029879` supplies index 13 to common dispatch `0298b8`; its selected
direct caller is `0293cd`. Body `023d5d` sets up the already verified
processing call/conversion loop. Its post-conversion call at `023e2e`
delegates through `029f2b` to virtual method 12; no hardware meaning is
assigned to that method here.

Before dispatch, `0293a2` refreshes the input object referenced by `15e9`.
Helper `02973c` returns `object[1] - object[0]`; the caller retains its low
word in AR2. The threshold setup at `02936d` selects AR1=0 for nonzero
`1310`, otherwise `0400`; `0293ac` copies AR1 to AR0. Opcode `f5aa` at
`0293ad` is `CMPR 1,AR2`: [TI SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf),
page 4-34, specifies unsigned AR2<AR0. The TC branch at `0293ae` skips to
`02940b` when that local availability test fails. Independently, the AEQ
branch at `0293b5` skips the same way when `15d9 OR context[0x16]` is zero.
Counter units, complete buffer-refresh effects and runtime branch operands
remain unvalidated; do not translate `0400` into bytes, samples or a physical
FIFO depth without evidence.

The source census covers 49,843 mapped AA99 words and 49,828 adjacent pairs.
It finds one encoded `0002,3d5d` pair (`c6a0`) and one wrapper target among
the recognized direct FCALL/FCALLD/FB forms (`0293cd`). This is quantified
raw-pattern coverage, not code/data classification, all indirect callers or
a whole-ROM absence proof. Tests mutate every pinned word, retain additional
candidate references, and distinguish threshold equality, unsigned high-bit
values, cursor wrap and the separate activity condition.

The bounded Execute observer now covers these caller paths from 9.64 to
9.86 seconds in the first verified AA99 session. A fresh ten-second run
produces 170 events, zero opcode mismatches and no trace cap. Check with
`python3 tools/mu4_decoder_gate_check.py RUN/error.log`. The checker requires
an uncensored window, verified parent terminal, resident-entry bounds and
known executed branch successors; live registers/RAM remain context only.
The `call` field counts underlying processing entries, not virtual-method
attempts. This short run does not replace the finalized twenty-second
capture-integrity or audio checks.

All 18 observed availability branches pass. Activity skips occur seven times
after processing count 373, seven after 374 and twice after 375; the first
two groups each subsequently reach another processing call. Thus inactivity
is normal between blocks, not evidence by itself of a missing event. The
last successful processing return is at 9.700534036 seconds. Two later
evaluations pass availability but skip for inactivity; the last selected
caller event is at 9.700780776. Original loader entry `002080` follows at
9.701774207. No further selected caller events occur through 9.86; the next
resident entry is at 9.868868536.

All 918 detailed snapshots and the complete 375-block return stream remain
byte-identical to the prior capture; all six transport streams match its
prefixes. The next question is the original cleanup/lifecycle decision
(`02956f`, state `373e`, control `3742`) leading to loader re-entry, not
how to force an inactive decoder to run. Neither these observations nor
the pinned cleanup route establish EOF or why only 375 blocks are decoded.

`playback_lifecycle_literals` in the context-contract report enumerates all
mapped literal occurrences of these two fields, retaining unknown patterns.
AA55 covers 10,760 words and one occurrence (the control-zero store pattern
at `0008f2`). AA99 covers 49,843 words and 43 occurrences: 25 immediate-store
patterns, 12 immediate-compare patterns, three A-load patterns and three
unclassified literals. Exactly one AA99 immediate-store pattern writes
state 4 (`020b0e`); the other stores select different states or control
values. This selects a source candidate for runtime observation, not an
all-writer closure: boundaries, indirect stores, aliases and dynamically
constructed addresses are not classified by this raw-word census.

A fresh ten-second run with `mu4_playback_lifecycle_observe.lua` records
state 4 at 0.837819684 seconds (bus-context PC `020b11`), with no subsequent
write to that field until state 5 at 9.700791097. The cleanup-context read
of control zero occurs at 9.700790964, immediately after the last caller
event at 9.700780776 and before loader entry at 9.701774207. The terminal
accounts for all ten lifecycle writes and three control writes, with
`illegal=0`; the bounded gate check also passes unchanged. All 918 snapshots,
six transport streams and decoded returns match the prior ten-second run
byte-for-byte. These are raw bus timings/context PCs, not retained
instruction ownership. State 4 becoming newly set is not the late trigger:
the remaining question is why execution leaves the processing loop and
reaches the already-enabled cleanup at this point.

The older `--playback-lifecycle` checker requires a later state-4 re-entry
as well as cleanup. This ten-second recording ends before that re-entry,
so it does not pass that separate contract; do not weaken the checker or
claim the complete repeating lifecycle from this shorter observation.

The next static boundary is now pinned as `playback_cleanup_ingress` in
the context tool (103 AA99 words). Routine `029422` fills a local frame via
`029264`; the selector at frame word 4 dispatches inputs `11`, `16`, `17`.
Input `17` copies frame word 5 to `aa1a` at `02944b`, writes reply selector
`8017` and takes the ordinary tail. Input `11` enters at `029471`, writes
reply selector `8011`, and has a later compare at `0294fd`: `aa1a == 1`
branches to cleanup `02956f`. The complete interpreter/producer is not yet
modeled. The mapped AA99 image has three literal `aa1a` references, including
an immediate-one store pattern at `027887`; this is not an indirect-writer
closure. Local-frame roles beyond the shown copies remain unassigned.

The passive `mu4_cleanup_control_observe.lua` bus taps now cover a complete
ten-second run. `aa1a` is written zero at 0.007246732 seconds, then one at
0.823387403 (context PC `02788a`). Its only read is one at 9.700790847
(context PC `029503`). The terminal accounts for both writes and the one
read, with no cap and `illegal=0`. Check using
`python3 tools/mu4_cleanup_control_check.py RUN/error.log`; the checker
requires complete full-word writes, chronological matching reads, explicit
passive scope and a non-capped terminal. All 925 output artifacts (918
snapshots, six transport streams and decoded returns) match the previous
ten-second capture exactly; the gate checker still passes.

Thus a late change to `aa1a` is not the trigger on this observed bus path.
The one-valued control was present before decoding began. The input-`11`
length and completion paths are mapped below; the remaining question is
correct end/drain ordering for the selected recording. This is
not proof of EOF, a decoder error, absence of indirect writers, or Execute
ownership at the context PCs. No control word or input selector should be
forced to extend playback.

`mu4_cleanup_ingress_observe.lua` uses bounded debugger breakpoints with
opcode guards. The staged core calls this hook at Execute entry; live
register/frame snapshots remain context only and annulment is unclaimed.
Run with the same fresh NAND fixture plus `-debug -debugger none`, an initial
`g` debugger script, and this observer as the autoboot script. Check with
`python3 tools/mu4_cleanup_ingress_check.py RUN/error.log`. The checker
requires the verified original AA99 image, first-resident-session bounds,
ordered identities/opcodes, a non-capped terminal and explicit branch
successors; it rejects the earlier 512-event prefix as incomplete.

The complete ten-second capture has 1,881 events (budget 4,096), 376 inputs
of `0011`, no selected input-17 store, and one observed cleanup successor.
The first 375 length branches select `0294ca`; the final branch selects
`0294fd`, then `029500 -> 02956f`. The final local frame still has
`f6=f7=00ba`, as do the preceding two transactions. At `0294c3`, live B
changes from `fffffffffa` (-6) on the preceding transaction to `00000000b4`
(180) on the final one. At the join, live AR1 is 6, AR2 is 1 and A is
`0000023140`; no callback site `0294e6` occurs on this final path.

The pinned source derives local full/half doublewords from `371a`, compares
the counter at `a9f6` plus an incoming signed word against the logical half,
and takes `0294ea` on BGEQ. That path computes a low-word remainder into AR1,
sets `aa1c=1`, replaces the counter with the local full value, and joins
the `aa1a` test. These source operations and the observed successors locate
the immediate stop in input-length handling, not a decoder-return failure.
The complete producer contract and whether this represents intended
end-of-input or an emulation defect remain unresolved. The input-word
units are established below; buffered decoder input is not drained by this
selected branch. Do not force a larger
length or suppress cleanup. All 925 output artifacts remain byte-identical
to the earlier ten-second capture, with `illegal=0` and the gate check green.

`playback_final_input` composes 235 original source words to distinguish
the final short-input route from ordinary completion. The BGEQ path reaches
`0294ea`, computes the remaining count in AR1, sets `aa1c=1`, stores the
local full extent into the counter, and joins `0294fb`. With `aa1a==1`,
`029500` branches directly to `02956f`, bypassing both normal reader call
`0294e0` and the ordinary input-12 completion publication `029563..029568`.
Thus the observed last request does not receive that short-read completion.

For active state 4 the cleanup path includes loader call `02959b -> 002080`.
The source after it (`02959d..0295a7`) prepares selector `16`, argument `9`
and a two-word send through `0292a7`. It is not a pre-reload flush request;
its presence after the loader call is not proof of delivery in the original
session. The alternative `aa1a!=1` path restores the counter from `3726`,
clears `aa1c`, calls API `002092`, then calls the reader before its ordinary
completion. That is not an independently established decoder-drain operation
and does not justify changing the control word.

The selected source policy is therefore explicit. Whether its 375-block
stop is correct for the recording's intended audio extent remains separate:
header-shaped buffered bytes alone do not prove that every remaining packet
belongs in the intended output. Resolve recording metadata/encoder extent or
an independent playback contract before treating this policy as a device bug.

The original R061 image does not change this pinned final-input policy.
`mu4_playback_version_check.py` checks both complete source hashes and all
235 mapped policy words: 234 are identical at the same addresses; the sole
difference is the page-3 FCALL at `02958a`, whose target changes from
`039005` to `039019`. The complete 28-word destination body through FRET
is identical after relocation. No other policy-word normalization is allowed.
Mutation tests cover every policy word in both versions and both target bodies.
The images have 5,617 raw destination differences overall, so this is not
whole-image equivalence or proof that their native runtime behavior is equal.
It rules out a direct change to these known short-read/cleanup operations,
not all possible R061 decoder, request-size or lifecycle differences.

```sh
python3 tools/mu4_playback_version_check.py \
  roms/5510-mu4-reference/InitData_R060.a00 \
  roms/5510-mu4-reference/InitData_R061.a00
```

`playback_input_units` pins 116 source words across AA55/AA99.
Descriptor arithmetic is `36b0 + index*002c`; index 2 is `3708`, and its
size pair at offset `0012` is `371a`. The local logical right shift by one
converts an even byte extent to file words. Frame word 7 flows through AR1
and adapter `0292cc` to the count argument of callbacks `0295cd/0295de`,
then APIs `2090/2086`. Their common reader advances the destination by one
16-bit word per iteration and doubles its cursor for the underlying byte
read; `2086` additionally swaps each returned word's byte order. The source
also pins the zero initializer of `a9f6` at `02788a`. This is source argument
flow, not an independent capture of every read/seek or all counter writes.

For an independent file-extent comparison, add
`--input-file tmp/mu4-dst-bypass-recorder/TEST` to the ingress checker.
The extracted file is 143,680 bytes, SHA-256
`4ba59580f8bf50b8d45d7a62d8ddf42ea94dfb7d884ff05b5092e7c13141470d`.
With a zero initial counter, the captured first 375 positive requests total
71,834 words against a limit of 71,840. Six words remain; request 376 asks
for 186, crossing by 180. This source model agrees with all 376 executed
length successors, without fitting a length or changing media. Equality
also selects the boundary. Tests reject odd/zero/oversized extents,
nonpositive/signed-high requests, missing boundaries and requests continuing
after the modeled cleanup. This establishes consistency of the word-count
interpretation, not correctness of decoder draining, the final packet,
waveform conformance or native handset audio completion.

#### Refill Control and Buffered Tail

`playback_refill_control` pins 140 source words. Sender `020643` checks
the high bit of its local reply word, then prioritizes pending `12fb`
(input `16`) over pending `12fc` (input `17`), before ordinary refill
gates. Input `11` is built at `020709`; all three use `02073b -> 029629`.
Receiver `029264` copies 16 words from data `0090..009f` to its local
frame; these mailboxes are RAM in this bench, not modeled external devices.
The input-17 builder forwards the nonzero pending value in AR1. Exhaustive
16-bit tests show this prefix cannot construct a zero-valued input 17;
that is not closure over other producers or pending-word writers.

The alternate `aa1a != 1` path is not an established drain-only mode.
When its AR2 test is nonzero, `02950a` resets the counter from `3726`,
clears `aa1c` and seeks via `2092`, then calls the selected reader. The
shared tail builds input `12` at `029542` and publishes through `029568`.
Do not manufacture a zero control/message to reach this seek/read path
and present the result as organic draining.

`mu4_cleanup_buffer_observe.lua` captures one passive data-RAM snapshot
(`0080..ffff`, 130,816 LE16 bytes) at the first opcode-guarded `02956f`
debugger hook. Its complete ten-second run has `illegal=0` and the same
925 output artifacts as the previous run, byte-for-byte. Check using
`mu4_cleanup_buffer_check.py RUN/error.log RUN/mu4_cleanup_input.le16`.
The checker binds object, cursor, limit and context to the verified first
AA99 session, preserving the observation-only/retirement-unclaimed scope.

Object `160a` has cursor `162e`, limit `1d46`: 1,816 words / 3,632 bytes
remain in the captured contiguous extent. Its high-byte-first view has
eight ADTS-header candidates at offsets 369, 741, 1113, 1485, 1857, 2229,
2601 and 2973; the other byte view has none. Signatures alone do not prove
valid payloads. More decisively, bit-reader `a854` holds 24 valid bits,
reservoir `a858/a859=e0ff/f950`, base `b332=162e`, index `b331=00ba`.
The low 24 reservoir bits supply prefix `ff f9 50`.

With `--reference-aac tmp/mu4-complete-host-reference/projected.aac`, the
checker uses the 375 captured successful processing blocks as a fixed
packet ordinal, not an offset search. Prefix plus high-byte-first buffer
matches all 3,348 projected bytes from packet 375 at offset 139,501 through
the projected end; 287 additional buffer bytes remain uninterpreted. The
reference SHA-256 is
`812882ea5da3fb9051a04e421922694366e580e75ae98df469936426eef1a873`.
Thus this buffer contains bytes matching reference-valid packets 375--382
and the already-rejected final packet 383. This is tail-content equality,
not native decoding of those packets or validation of the malformed final
packet. Prefix searches are ambiguous because the fixture repeats; no
search-selected alignment is accepted. Tests preserve reservoir low-bit
selection and reject a wrong fixed tail, failed ordinal, partial snapshot,
pointer mismatch or unaligned reservoir.

`playback_refill_completion` pins the receiver's 80 source words. Dispatch
`0207b5` selects input `12` at `020862`. A zero pending word `15e0` skips
the buffer update and produces reply `8000`. Otherwise frame word 3 is
copied to `15df`, and `02976d` adds it modulo 16 bits to field 6 of the
object referenced by `15e7`; metadata is updated and `020888` clears the
pending word. The common tail sets the reply high bit and publishes via
`020944 -> 02961e`. The small reference model covers the pending test and
cursor delta only, not all metadata or an EOF/drain protocol.

The existing snapshot has pending `15e0=00ba` and last supplied
`15df=00ba`. File-buffer object `1622` has fields 0/1 both `1e0a` (empty
by the same difference helper), with field 6 at `1d50`; the downstream
buffer still contains the tail described above. Mailbox first words are
`0080=8012`, `0090=8011`. These raw markers do not identify an individual
message or prove its payload/lifetime.

`mu4_refill_completion_observe.lua` captures paired entry/updated debugger
hooks and control-19 contexts. Its complete ten-second run has 752 events
(budget 2,048), 375 active completion pairs and zero inactive entries.
Every cursor delta and pending clear agrees with the source model; supplied
counts total 71,834 words. Control 19 first appears with argument zero and
no pending request, then with argument one and a pending 186-word request
after the final successful completion. This is cleanup-associated control,
not a claim that input 19 means DSP stop, mute or drain. All 925 output
artifacts remain byte-identical and `illegal=0`. Add
`--completion-log COMPLETION_RUN/error.log` to the buffer checker to verify
this separate capture; capped, overlapping, unpaired, wrong-cursor and
out-of-session traces fail rather than become absence evidence.

Thus ordinary completion delivery is functioning on the observed path.
The final request takes the file-length cleanup branch before its completion
publisher, leaving it pending; there is no evidence here for repairing a
missing peripheral interrupt by synthesizing input 12. Next classify the
original controller's cleanup/drain policy and its legitimate producers,
rather than extending decoding with invented completions. The current
one-pass path is explained, but its early cleanup is not yet classified
as an emulation defect or correct handset behavior. Preserve the strict
audio-completion failure and never replay this RAM snapshot.

The `playback_refill_policy_literals` census makes the policy's source
coverage explicit: AA55 has 10,760 mapped words and zero `aa1a/12fc`
literals; AA22 has 51,921 and zero; AA99 has 49,843 and eight. AA99 includes
five immediate-store patterns, one MVDK destination pattern, one compare
and one unclassified literal. Its sole immediate `aa1a` store writes one
at `027887`; its other store is frame-word payload at `02944b`. The known
`12fc` immediate stores write zero or one. The census preserves MVDK and
unknown references rather than silently counting only literal stores.
These are raw mapped-word patterns, not a code/data split or closure over
indirect, bulk-initialization, table-driven or other-image writers.

Together with the two-write/one-read runtime control capture, this classifies
the observed branch as the uploaded controller's default one-pass cleanup
path. It does not establish that this is the complete product playback
lifecycle or justify changing hardware timing to force eight more calls.
The remaining policy question needs the legitimate producer/consumer
contract or an independent handset playback reference, not another guessed
peripheral response.

`playback_pending_control` pins 215 further AA99 words around the controller,
notification and input-16 receiver. Controller frame setup `020769` enables CPL and loads request
frame word 2 into AR1. Its input `17` branch at `0207f5` stores **one** to
`12fc`, independent of the parameter; it is not a zero-valued input-17
producer. Input `16` at `0207fa` instead sets `12fa/15db` and copies frame
word 3 into `12fb`, which the previously mapped sender forwards as input-16
payload. Receiver `029458` replies `8016`, passes frame word 5 through
`0291d3`, then loads immediate `3708` into frame word 0, puts `9` in frame
word 1 and passes the reply frame word-5 pointer in A to `0299cc`.
The complete 50-word copy helper selects forward copying
when source AR4 exceeds destination AR0 and backward copying otherwise,
with a zero-count exit. Destination arrives in A; source and count arrive
in caller frame words 0 and 1. This call copies fixed record `3708..3710`
to reply frame words 5..13, not a parameter-indexed region. The acquired
image's hook at `0291d3` is `NOP; NOP; FRET`; runtime replacement and live argument ownership are not
closed. These source fragments distinguish a reply-copy control from an
established EOF/drain operation. The pending-argument producer census below
maps additional internal routes; their live incoming-command ownership and
the original controller's end-of-input protocol remain to be established
before changing cleanup.
Every pinned word has a mutation test; no message or state is injected.

The verified vector target, not that frame setup, identifies the wrapper
entrance: AA99 vector `022064` saves XPC and branches to `02:0742`, whose
register-save prefix precedes the local frame. Initializer `0278cd` supplies
far callback `02:0981` to setter `0292f0`, which stores it at `aa7e`.
Publisher `0292a7` copies its frame to mailbox `0080`, loads that pointer
and conditionally invokes it. Callback `020981` sets `12f8=1` and executes
`INTR 25`, completing a source-owned software notification chain. The three
literal `aa7e` references in the mapped AA99 image are retained in the report;
computed aliases and runtime replacement are not closed. This is not evidence
that a missing external HPI interrupt explains the final pending request.
Those source fragments do not close live message ownership or the input-16
argument's upstream producer.

`playback_pending16_producers` scans all186290 mapped words: AA55=10760,
AA22=51921, AA99=49843 and AA88=73766. All six exact`12fb` literals are in
AA99, and each is classified:

| Local site | Operation |
| --- | --- |
| `02065e` | Read pending argument into AR1 for the sender. |
| `02072e` / `0278ba` | Clear pending argument. |
| `020800` | Input16 handler copies incoming frame word3. |
| `0208e5` | Input3 handler supplies constant3 if`12fd != 0` and frame word3 is zero. |
| `020915` | Input1 handler supplies constant3 unconditionally. |

The141-word contract includes the controller's input1/2/3/4 dispatch and
both complete local constant-producing branches. Input1 clears`12fc`,
`15e0`, `15e1` and`12fd`, sets`15db=1`, initializes AA4C fields and publishes
a reply. Input3 clears`15da/12f9`; its zero frame-word3 selects`15db=1`,
then the active`12fd` branch substitutes`15de` and sets pending3. Its
frame-word4 equality test separately controls AR7.

Thus pending input16 is not restricted to an externally supplied argument:
the controller has two source-owned constant producers. The known
first-session notification inventory below contains neither input1 nor3,
so it does not exercise those routes. This is a raw mapped-word census plus
pinned local instructions, not a code/data split, computed-alias/bulk-writer
closure or runtime proof that either incoming command is issued at EOF.
The acquired receiver hook remains a no-op; no command, argument, hook or
pending state should be synthesized to turn this source finding into a
playback-completion claim.

The companion`playback_input3_senders` contract extends the source coverage
to285 words. Builder`02905d` writes BA56 as`{03, A, caller-SP[0],
caller-SP[1]}` and sends four words through`0292a7`. Its three direct calls
are`028481`, `0284a4` and`02859a`; the following words contain the target
literal, not another instruction entry.

The AA99 serial dispatcher indexes the table at`023d09` by selector minus
`29`. Selector`30` selects index7/word`023d10=8d03`, reaching`038d03` in
the current program page. This is **selector30, not recorder selector36**.
Parameter03 calls`02846f`, which requires`A825=1`, sets it to2 and supplies
builder arguments`{1,0,1}`; the
controller's frame-word3 is nonzero, so this route does not set pending3.
Parameter04 calls`028498`, whose builder arguments are`{0,0,1}`; with
active`12fd`, this reaches the input03 pending3 producer. The third caller
also supplies`{0,0,1}`, but its upstream owner remains unresolved.

The dispatcher rejects a matching previous class/selector when the selector
is above`28`. A declared external control test must respect this guard
rather than force its previous-command state; the existing status-query
transition supplies the intervening exchange in the explicit control fixture
below. These are source-derived routes, not official command names or an
automatic EOF sequence.

The `bootplaystop` bench profile now qualifies explicit serial control after
native decoder progress. It sends playback `30/02`, then status `49`, waits
for the complete status response, acknowledges it, and sends `30/04` through
the existing serial input. No firmware RAM, CPU PC or internal message is
injected. Request token `02` receives ACK `02`; the DSP status response has
its independent TX token `03`. Equating those tokens incorrectly parks the
peer and must not replace semantic response and framing checks.

The qualified run observes the source-identified route
`038d03 -> 028498 -> 02905d -> 0208ce -> 0208e5 -> 020727`.
The input03 frame contains `{03,0,0,1}`, and the input16 sender sees pending
argument `3`. Original resident AA22 is then reloaded and executes IDLE3
`028922:f5e1`; the staged core previously rejected that instruction. At
20 seconds the CPU is clean, idle at `028923`, with five decoder calls,
29 serial input bytes and six complete native response frames. Sixteen
read-only observations stay below the 1024-event cap. The AA99 `receive16`
and `hook16` sites are **not observed**; the sender alone does not prove
their execution after the overlay transition.

Use `tools/mu4_playback_serial_control_observe.lua` with `bootplaystop` and
the existing finalized recorder NAND/ROM setup, plus `-debug -debugger none
-autoboot_delay 0`. Headless host options `-joystickprovider none
-midiprovider none` avoid unnecessary physical host-device initialization;
they do not alter the emulated serial protocol. The run retains the expected
rejecting endpoint `MU4 playback coverage diagnostic: audio completion is
not validated`. Check it with:

```sh
.venv/bin/python tools/mu4_playback_serial_control_check.py \
  roms/5510-mu4-reference/InitData_R060.a00 RUN_DIR INPUT_NAND --console CONSOLE_LOG
```

The full input NAND SHA256 is
`ca1a96666fb527b41a40796c123bd6a6ec5c486b0d4cbf373d2368d58af75923`.
Under `-nonvram_save`, unchanged on-disk NAND establishes input retention,
not absence of runtime NAND writes or durable persistence. The checker
independently verifies wire bytes/checksums, opposite-direction tokens,
ordered source opcodes, uncapped read-only observations and the clean final
diagnostic. This is an explicit user control, **not automatic EOF draining,
protected-tail recovery, audio-completion acceptance or complete 5510 boot**.

`mu4_playback_serial_restore.lua` adds a separate same-process checkpoint at
1.05 seconds, after the status request but before its response and the next
control. It restores 36 exposed CPU registers and checksums over 65,408
non-MMIO data words and the 16-MiB program backing. The reference and restored
0.15-second windows each contain exactly 14 native TX-register writes: the
complete status response followed by control ACK03, with identical values,
order and timestamps. The restored continuation reaches the same pending3
sender, resident IDLE3 and rejecting terminal diagnostic as the cold control.

The external peer's ACK count belongs to the shared media-command save state,
not only recorder profiles. Binary diagnostic streams require separate host
file handling: before loading, preserve each complete reference as
`.before-load-1`, then rewind its active file and capture counters together.
Nine preserved reference prefixes (3,631,970 bytes) match the replay; all nine
complete restored captures additionally match the independent cold control
byte-for-byte (173,513,990 bytes). This covers TX, codec input, DMA source,
slot/time records, DXR writes/events, decoded-return blocks and the ideal
headphone diagnostic stream, not measured analogue output.

Restored metadata says `save_restore=same_process_rewind`; cold captures retain
`unsupported`. Existing cold-only audio checkers deliberately do not accept the
new scope. Loading into another process, loading after capture closure and
portable capture files remain unsupported; missing, closed or shorter active
captures fail closed. File-position checks are not a process-identity proof.
Files are external
diagnostics, not emulated storage; NVRAM retention under `-nonvram_save` still
does not prove runtime NAND write absence. Reproduce using the restore Lua in
place of the cold observer, then require both independent runs:

```sh
.venv/bin/python tools/mu4_playback_serial_restore_check.py RESTORED_RUN \
  --console RESTORED_CONSOLE --cold-control COLD_RUN --cold-console COLD_CONSOLE \
  --source roms/5510-mu4-reference/InitData_R060.a00 --input-nand INPUT_NAND
```

The checker rejects missing registers/checksums, non-pending checkpoints,
changed replay timing or bytes, altered wire framing, capped observations,
reference-prefix drift, any cold/restored capture difference, incomplete
continuation or unhealthy process exit. This qualifies this native control's
restoration; it does not promote automatic EOF, full audio completion, native
handset speech or complete 5510 support.

The `resident_idle3` source contract pins all 22 words at `028914..028929`
and ten caller words around `03ac0b`. The caller enables IMR bit 2 through
`AR1=0; ORM 4,*AR1`; the routine pushes IMR/IFR, selects `IMR=0004`, writes
`CLKMD=0`, polls CLKMD bit 0 until clear, writes zero again and executes
`028922:f5e1`. Its tail writes the saved IFR word through W1C acknowledgement
semantics, restores IMR and returns. Do not describe POPM IFR as restoring
the old pending-bit set.

All 186,290 mapped words in AA55/AA22/AA99/AA88 are scanned for the adjacent
`f982 8914` far-call pattern; only AA22 `03ac0b` matches. This is not
code/data classification, an indirect-call census or proof of every caller.
The runtime IDLE3 context's return words `0003/ac0d` agree with this caller.
The selected mask excludes this bench's command RX IRQ6; NMI and reset are
separate architectural inputs, not IMR bits. Combined with `UC_DSP_INT` on
the schematic, this identifies a controller interrupt boundary worth testing,
not permission to infer its timing from a serial packet or rename `30/04` as
an official power-off command. A declared logical pin fixture now qualifies
the native wake continuation; the normal MSP430 waveform and ordinary
EOF/draining remain separate unknowns.

### Declared resident INT2 wake fixture

`mu4_resident_int2_observe.lua` asserts the schematic `UC_DSP_INT` input
through the bench input port, without writing firmware state. Its paired
`mu4_resident_int2_control.lua` observes the same boot without an edge.
Both start at 11 seconds in resident `028923`, IMR `0004`, IFR `0c38`.
The control remains there with zero IMR/IFR writes in the observation window.
The pulse is sampled at 11.016666667 and released at 11.066666667 seconds;
this is a declared 50 ms logical assertion, not a measured MCU waveform.

The positive branch executes the original INT2 handler, restores the model
PLL selection, reaches `028925:8a01` / `028926:8a00`, and returns to
`03ac0d`. The data-write tap observes IFR `0c38` written with saved word
`0438`, then IMR restored to `0ac5`. The next Execute-entry observation
has IFR `0808`: newly pending bits survive, including the still-asserted
INT2 level. A later interrupt runs before the caller observation, so that
observation is not an immediate post-POPM pending-bit snapshot.
The worker reaches IDLE1 at `0298ea` cleanly and later enters resident
IDLE3 again. The bounded register observer records 258 writes below its
1024 cap; path observations record five events below their 32 cap.
Execute-entry context is not a guarantee of pre-instruction SP state.

Check both branches, original-image contract, retained whole input NAND and
the unchanged rejecting audio endpoint with:

```sh
python3 tools/mu4_resident_int2_check.py SOURCE_A00 INPUT_NAND CONTROL_RUN PULSE_RUN
```

Each run's console is `RUN-launch.log`. Unchanged `-nonvram_save` input
files do not prove runtime NAND write absence. This paired fixture proves
the declared wake input's native continuation, not automatic EOF, complete
audio, normal MSP430 control or full 5510 graphical boot.

`mu4_resident_int2_restore.lua` qualifies a same-process checkpoint at
10.95 seconds, before a declared INT2 assertion. It compares 36 exposed
CPU registers, a checksum of all 65,408 non-MMIO data words and the complete
16 MiB program backing before and after load. Both legs assert the logical
pin at 11.000000000 and clear it at 11.050000000 seconds; 282 IMR/IFR
writes match in order, value and absolute time within the 0.25-second
window. Both return to the native worker's clean idle. All nine preserved
diagnostic streams match their restored prefixes (93,569,510 bytes total).
This does not claim independent final-file equality with a differently
timed cold pulse, cross-process capture restoration or normal MCU timing.

The command peer's saved history now holds 4,096 words instead of 64:
late checkpoints include more than one frame's historical output. This
is a bounded diagnostic history, not an emulated serial FIFO. Overflow
still fails with explicit TX/decoded counts; no history is silently truncated.

```sh
python3 tools/mu4_resident_int2_restore_check.py SOURCE_A00 INPUT_NAND RESTORE_RUN
```

The checker requires the original image contract, complete retained input,
ordinary native serial-control acceptance and rejecting audio endpoint,
matching checkpoint state, actual board-pin edges, replay publications,
all nine preserved capture prefixes and an uncapped completion marker.

### Resident command interface after wake

`mu4_resident_status_observe.lua` adds a declared serial-peer status query
after the paired INT2 fixture, using the existing selector `49` packet with
request token `04`. The bench input `MCU status request` queues this packet
through the ordinary timed command receiver; it does not write DSP memory,
select a PC or claim a recovered MSP430 command policy. Its one-shot request latch is
saved with the other playback peer state. The matched
`mu4_resident_status_control.lua` sends the same query without an INT2 edge.

Both request the packet at 11.2 seconds and begin ingress at 11.201.
After wake, all eight bytes are consumed (37 total fixture bytes), native
firmware emits ACK `7f 04 55` and a complete XOR-checked status frame:
`1e 05 aa 01 71 01 00 00 02 c2 55`. The request token `04` and native
TX token `02` belong to opposite directions; the latter follows the
resident reload and must not be forced to match the request token.
Without wake, only the first additional byte is offered (30 total), no
ACK04 or response follows, and the DSP remains at resident `028923`
through the 20-second diagnostic. This is a bounded negative control,
not a permanent-stall or all-wake-source claim.

```sh
python3 tools/mu4_resident_status_check.py SOURCE_A00 INPUT_NAND CONTROL_RUN WAKE_RUN
```

The checker composes the existing native serial/wake contracts with exact
query byte inventory, acknowledgement ordering, reply framing/payload and
the sleeping control's lack of a response. Both legs retain the strict
rejecting audio endpoint and unchanged complete input NAND. This proves
the original resident command interface is usable after the declared wake,
not normal MCU policy, automatic EOF, complete music or native phone speech.

`mu4_resident_status_restore.lua` separately checkpoints at 11.205 seconds,
after five of the eight status bytes have entered, but before native ACK04
or response6. It composes the same declared INT2/status inputs with the existing
serial restore observer; no DSP memory or counters are changed. All 36 exposed
registers, 65,408 non-MMIO data words and the 16 MiB program backing match
exactly after load. Both branches emit the same fourteen DXR writes and
timestamps over 150 ms: ACK `7f0455`, then native status frame
`1e05aa017101000002c255`. The firmware eventually returns to resident
`028923`, not permanently to worker `0298ea`.

The nine preserved diagnostic prefixes match (95,886,186 bytes), and all nine
final files match the independent cold status run exactly (173,513,990 bytes).
The complete retained input NAND is unchanged. Capture:
`tmp/mu4-resident-status-restore`; cold control: `tmp/mu4-resident-status`.
Use `mu4_playback_serial_restore_check.py --resident-status` with its existing
source/input/cold-control arguments. The original default pre-control restore
contract remains distinct and unchanged. Both paths deliberately retain the
rejecting audio-completion endpoint; this is native serial/device restoration,
not normal MSP430 policy, automatic EOF or complete music acceptance.

The post-wake caller is not another receive wait. The separately pinned
52-word `resident_wake_tail` contract loads `07d0`, calls `03acdf`, then writes
zero to `b558` at `03ac11`. Helper `03acdf..03acf1` passes stack literal
`000a` to resident `00:2098`, retains its result in AR1, reads `b582` into
AR0, and performs unsigned `CMPR GT,AR1`. Its `f820` is **BC NTC**, not
BC TC: it skips the store when AR1 is not greater than the old value.
Otherwise it publishes AR1 to `b582`. There is no backward branch or
receive operation in this helper: the effect is a countdown lower bound,
not a synchronous delay or a cap on the old value.

The separate AA55 bootstrap supplies the nested call's source: `002098`
branches to `00352c`, whose five words clear SXM, load AL into A, repeat
`SUBC SP(2),A` sixteen times and return. The retained low word is the
unsigned low-16-bit argument divided by the caller's ten. Consequently the
bounded software model is `max(old_b582, low16(argument) / 10)`; the native
call's `07d0` produces `00c8`. All 65,536 argument values are checked against
the model at zero, quotient-adjacent and maximum counter values. These
host arithmetic checks do not replace the executable CPU SUBC/repeat
fixtures or measure physical countdown units.
`SP(2)` assumes the CPL-addressed ABI, present in the native wake context's
`ST1=6113`; this is not a context-free interpretation of the direct operand.

The same contract pins the consumer at `03ac03`: `b582 != 0` branches
to `03ac11`, bypassing the resident deep-idle call. Worker prefix
`03a8c9..03a8d7` decrements a nonzero counter, then tests `bb94 & 0002`;
when set it adds one back (including changing zero to one). This is an
asynchronous idle-inhibition countdown, not a receive-completion latch.
The worker's cadence and ownership of the inhibiting flag remain unknown.

A read-only data-tap window after the declared wake observes `b582` being
set to `00c8`, followed by `b558=0`; ordinary activity subsequently
decrements `b582` to `00c7`. This supports asynchronous bookkeeping, not
another missing MSP430 acknowledgement. The countdown's physical units
and its official lifecycle meaning remain
unqualified. Source ownership comes from the pinned upload,
not data-tap context PCs or earlier same-address overlay observations.

The bounded native playback fixture now observes 402 complete software
notification groups: publication `0292c1`, callback `020981`, vector
`022064`, wrapper `020742`, and request-frame read `020780`. All groups
retain the mailbox value and callback `0002:0981`; flag `12f8` follows
`0,0,1,1,0` across those entries. Request IDs are `12` (375), `04/05`
(11 each), `19` (2), and `14/02/15` (one each). Neither request `16/17`
nor their selected handlers occurs in this first-session observation.
This is fixture/window evidence, not an all-producer absence proof.

Use `tools/mu4_notification_observe.lua` with the isolated `bootplaystage`
NAND/ROM setup, `-debug -debugger none`, and a ten-second stop bound; the
observer starts debugger execution itself. Check the log with
`python3 tools/mu4_notification_check.py RUN/error.log`. It rejects censored
chains, changed scope, opcode/address disagreement, truncated captures,
unverified overlays and out-of-session entries. Program-word guards must
retain the upper page above `8000`; only the lower 32K uses OVLY's shared
backing. The observer logs the actual opcode rather than hiding a mismatch
behind a per-site opcode predicate.

The 2,010-event capture completes at 10.016667 seconds, below its 4,096-event
cap, with `illegal=0` and verified first-session completion. Its 1,536,000-byte
returned-sample file retains SHA256
`1871f924d9c12dca42bcced964dfe1ee0a42165574f5d4e424fbf284fdfb26dc`.
The ten-second observation does not satisfy the separate twenty-second
whole-playback terminal or establish retired operand ownership, annulment,
physical interrupt latency or audio completion. Ordinary software
notification is functioning here; resolving end-of-input policy still needs
the legitimate control lifecycle, not a synthetic HPI edge.
Retain this observer while cleanup policy is unresolved; retire its PC hooks
when a smaller accepted boundary regression can replace them.

The directory's adjacent `ADDM #1,*SP(2)` / `LD *SP(2),A` dependency
requires same-address forwarding. [TI SPRU131G](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
section 7.3.2 specifies an internal-bus bypass for an on-chip read from a
location with a pending write. The staged model now forwards retained
RMW results into supported Read-stage RAM consumers without reading stale
RAM, moving the EB write, or publishing ADDM flags before Execute.
Register/peripheral reads are excluded; unrelated addresses and spaced
consumers retain real data-bus reads. Physical bank contention, other
producer families and measured DA150 bus phases remain unvalidated.
The isolated `tms54test -bios rmw-forward` fixture covers 768 cases with
save/replay, boundary values, SXM/OVM combinations, two program pages,
compiler/noncompiler addressing, same/different addresses and zero/one
gap words. Existing PSHD and descriptor-copy expectations now reflect
forwarded data rather than treating raw first-half DB ordering as proof
that consumers must see old RAM.

The defect's read-only witness is stack address `15de`: cycle 192385 read
zero for the increment, while cycle 192386 read zero for the follower
before writing one. It produced `MMCUSI16`, displaced extension bytes
and malformed lengths, then zero-length loader reads and a 37,019,530-read
retry tail. That tail is superseded, not a current startup result.
Program-Fetch A snapshots alone never proved a consumed descriptor index;
real data reads established descriptor zero before later index-128 retries.
Original `0e41..0e48` selects stack `1200`, adds `03ff`, and aligns it;
the loader later selects `3aea`. Immediate
`MPY #lk,A`, `f066` at `00:2eda`, now has retained literal/T,
arithmetic and producer/restoration coverage. STLM A,T,
`880e` at `00:2ed8`, now has publication, saturation and consumer/replay
coverage. Unsupported Decode can stop older Execute; the runtime PC alone
does not prove retirement of instructions still in the pipeline. Repeated
`ST #lk,*AR1+`, `7691` at `00:08ea`, now has short-memory store
retention, two-clock cadence and interrupt/restoration coverage. BANZD,
`6e87` at `00:0a50`, now has short-indirect delayed-branch coverage.
Short-memory repeated
`SUBC Smem,A`, `1e02` behind `RPT #15` at `00:352e`, is now covered.
The runtime reaches a different program range; that is not proof of the
complete loaded image or startup acceptance.
`ADD *(0016),0,A,B` at `00:319c`, encoded
`6ff8 0016 0d00`, now has staged arithmetic coverage; its memory source
is mapped AR6. The preceding
`LD *AR1(0004),T` at `00:3194`, encoded `30e1 0004`, now has
Read-publication and dependent-multiply coverage.
The preceding `STL A,-1,*AR1(0003)` at `00:313e`, encoded
`6fe1 0003 0c9f`, now has shifted-store coverage.
Unsupported Decode can stop older Execute work, so
later PCs alone are not retirement proof. The
initializer loop's complete runtime effects have not yet been accepted.
The terminal bootstrap-entry failure remains coverage evidence,
not a passing recorder gate.

Reproduce in a new disk-backed run directory with a copy of the retained
`mu4nand_13/nand` seed under `nvram/mu4nand_25/nand`, then run:

```sh
/path/to/mame mu4nand -bios bootrecstage -rompath /path/to/seed/roms \
  -nvram_directory "$PWD/nvram" -video none -sound none -nothrottle \
  -debugger none -log -nonvram_save -seconds_to_run 60
```

Do not reuse the source seed as writable NVRAM. The instruction contract at
`00:30c6` now has short-indirect DST address/modification, two-write publication
and replay conformance across all twelve short modes. The next instruction
at `00:30c7` has long-offset LD source/address and replay conformance.
Short-indirect DLD at `00:30d3` has retained paired-read, pointer-modification,
accumulator-publication and replay coverage, including all short modes and
the extended forms. Long-offset BANZ at `00:30df` has retained offset/target,
resolved-address predicate, pointer-modification and replay coverage.
Immediate SUB at `00:3216` has retained-literal, arithmetic-flag and
replay coverage, including shift-16 and preceding-load composition.
Register SUB at `00:321c` has source/destination, signed-shift, arithmetic-flag
and replay coverage, including ASM forms and preceding-load composition.
Register LD at `00:30f7` has shifted-source, publication and replay coverage,
including ASM forms and carry preservation. Long-word DADD at `00:30fa`
has paired-read, source/destination, arithmetic-flag and replay coverage
for ordinary and C16 modes. LDU at `00:3114` has retained-operand,
unsigned-extension, address-ownership and publication/replay coverage.
Memory MPYA at `00:3520` has retained-source, signed A-high multiplier,
Read-owned T, Execute-owned B/flags and replay coverage. Memory LD
shifted by 16 at `00:3521` has retained-source, SXM extension,
accumulator publication and replay coverage. Unsigned memory multiply
at `00:3523` has retained Smem/T, unsigned-product, FRCT/OVM,
destination/flag and replay coverage. The extended Smem instruction
at `00:3291` has checked extension decoding, retained-address/source,
SXM/shift/status and replay coverage. The shifted store at `00:313e`
has destination ownership, Execute-live source, saturation,
low-word publication and replay coverage. The load into T at `00:3194`
has retained address/source, T publication, dependent-multiply
ordering and replay coverage. The ADD at `00:319c` has mapped
source sampling, shifted operand, separate source/destination accumulator,
arithmetic flags and replay coverage. Single-step SUBC has retained
source/address, conditional-subtract/shift, carry and replay coverage.
Its ALU-difference overflow projection is explicitly unvalidated at the
silicon boundary. Repeated SUBC now has sixteen-step quotient/remainder,
repeat ownership, IRQ ordering and restoration coverage. BANZD at `00:0a50`
has retained target/count, pointer-update ownership, delay-slot
execution and restoration coverage. The repeated store at `00:08ea` has
literal/address retention, count+1 writes, per-iteration pointer
ownership, pending-IRQ ordering and restoration coverage. STLM A,T at
`00:2ed8` has source/saturation ownership, T publication,
dependent-instruction ordering and replay coverage. Zero-gap T consumers
are explicitly outside TI's allowed STLM latency; the model does not claim
hardware forwarding. Immediate MPY at `00:2eda` has retained literal/T,
FRCT/OVM, destination/overflow and replay coverage. STLM B,BRC at
`00:32ec` now has 3,072 replay cases covering source/saturation ownership,
Execute BRC publication and retained LDM reads. The context tool pins
`891a f495 f072 32f4 1386 f678 f3e8 f1a0 8196`: the NOP before
RPTB satisfies SPRU131G table 7-3 category 9, and the block spans
`00:32f0`--`00:32f4`. Zero-gap LDM results are a declared pipeline
projection, not a silicon forwarding claim. Straight-line RPTB now has
6,144 count/data/replay cases, 24 IRQ/NMI replay cases and sixteen
endpoint/exclusion cases. Delayed/nested/control-changing blocks and
exact silicon phase/terminal-counter behavior remain unaccepted;
passing the idle-call fixtures alone is insufficient to enable native
staged startup.

The original five-word byte-swap body now has isolated staged execution
and 1,536 save-state replay cases, plus another 1,536 cases for two
consecutive zero-gap bodies, supplemented by the bounded RPTB matrix;
the exact coverage and reproduction command live in
[the native loader contract](rom4_dsp_loader.md).

The context tool also pins the 12-word AA55 reset sequence at
`00:ff80`--`00:ff8b`: PMST=`ffc8`, AR0=`0060`, `RPT #ff20`,
`MVPD 0060,*AR0+`, PMST=`ffe8`, then `FB 00:0e41`. This is a
65,313-word program-to-data copy, not a NOP delay or constant fill.
TI SPRU131G section 6.7 specifies count+1 executions and implicit increment
of the absolute program source; the encoded AR0 postincrement advances the
destination independently. Both ranges are `0060`--`ff80`, ending with
AR0=`ff81`. The original upload supplies 10,749 words in this range;
54,564 source words are not assigned by the upload inventory, whose absence
does not establish their runtime values. Do not replace the copy with
guessed zero-fill or program/data aliasing.

The next staged contract therefore needs both long-immediate repeat and
program-source MVPD, with retained operand words, count+1 retirements,
per-iteration source/destination progression, exact continuation and
save/replay. Interrupt suppression begins when RPT is decoded and includes
NMI until the repeated instruction completes (SPRU131G 6.7). The repeat's
first and steady-state bus phases remain partial. The bounded staged repeat
cases and pending-IRQ/NMI replay are described in the CPU contract.
Observation-only `bootrecstage` taps verify all 65,313 program reads and
data writes, in order with matching values, before the original serial entry
is reached. `mu4_staged_bootstrap_copy: PASS` deliberately reports
`native_startup_complete=0`; this does not close recorder acceptance.
The new diagnostic is intentionally not a passing save acceptance gate.
The independent INTM-change protection fix passes pending-interrupt load,
multiword-store and return fixtures, but the metadata diagnostic still has
cursor 27, RX/TX 36/47 and no metadata delivery. It does not close this
general timer/write/pipeline boundary.

The `bootrecsave` reproduction reaches original resident
startup and control return, but stops with RX-ready set, zero
tracks and PRD `1`; the metadata packet has not been delivered. This remains
a CPU interrupt-recognition/pipeline-drain question, not evidence of metadata
rejection. The independently verified stopped-PSC/read-only-TCR corrections
did not change serial progress: RX/TX remained 36/47, with three responses,
two ACKs and completion `8009`. Sampled handler PC and TIM depend on the
observation phase; the replay below is the authoritative snapshot. The
atomic `bootrecsave` recorder acceptance checker still fails; passing timer-register conformance
must not promote it.
A blanket return-instruction guard is not established by
[SPRU131G](https://www.ti.com/lit/ug/spru131g/spru131g.pdf): section 6.10.7
explicitly protects SSBX/RSBX INTM and their following instruction, while
section 7.2 examples 7-9/7-10 describe RETE/RETED clearing INTM during execute.
Do not transfer that instruction-specific guarantee to RETE, RETF or FRETE,
or choose a fixed post-return delay to make this firmware advance. The next
CPU experiment must distinguish recognition, already-decoded instruction
drain and memory-mapped timer-write visibility using firmware-free fixtures.

The ISR's return path is also checked independently of its endpoint PC.
The vector saves XPC, the handler reserves a frame word, and its stack store
selects the original `FRETE` stub at `02:4044`. A bounded read-only trace
shows the first RET taking that stub, then restoration of XPC `2`, PC
`98de`, and the pre-interrupt SP `1272`. The following TINT repeats the
same frame; no cumulative stack loss or substituted return target is seen.
The temporary stack probe is removed after harvesting this conclusion.
The firmware-free `probe-c54x-timer-boundary` reproduces starvation with
just indirect PRD/TCR stores and a `PSHM XPC; FRETE` ISR; its long-period
control reaches the IMR mask. This excludes NAND, codec and serial work as
necessary causes of the stall, but is not proof of the exact physical
interrupt latency. See [the CPU contract](rom4_dsp_loader.md) for scope.

On the current build the short-period synthetic case stops at `01:0608`
with SP `1000`, IMR `0008`, IFR `0008` and its completion marker clear.
That PC is the `LD #0,AR1` immediately after the indirect TCR reload,
before the indirect IMR mask at `01:060a`; the ISR restores the stack but
the mask's address setup never completes. The PRD `0100` control reaches
`01:060f`, sets the marker and clears IMR bit 3 while IFR bit 3 remains
latched. Thus the discriminator is execution reaching the mask, not
whether the timer flag can latch while masked. These are observations of
the current core, not physical timing acceptance criteria.

The probe also substitutes a direct `ANDM #fff7,*(0000)` at `01:0608`
for the pointer setup and indirect mask, retaining the completion marker's
address. Both short-period cases leave the marker clear; both long-period
controls set it and mask TINT. In the direct short-period observation the
CPU is at vector `01:004c` with SP `0fff`. This excludes the extra pointer
setup instruction as the sole cause, without establishing a physical
interrupt delay or changing the CPU implementation. The diagnostic now
reports four cases and still declares `fidelity_acceptance=0`.

The CPU's MOD4/MOD7 addressing modes require reverse carry/borrow when
subtracting/adding AR0, not linear arithmetic ([TI SPRU131G](https://www.ti.com/lit/ug/spru131g/spru131g.pdf),
section 5.5.3.5 and table 5-4). Linear `*AR+0B` indexing made the codec
buffer-reordering routine's `DST` at `03:c85d` write to address zero,
clobbering IMR and leaving subsequent serial data unread. Correct reverse
carry restores native receive progress without firmware changes.
`check-c54x-core` covers 2,688 MOD4/MOD7 MAR/load/store cases and the
reference guide's 16-address sequence in both directions. The dedicated
mask/idle probes and stale unread-character assertion are retired.

The current control endpoint is `03:bc4d`, IMR `0ac1`, IFR `0438`,
lifecycle 9, mode 3, continuation 8 and zero tracks. The application
dispatcher `02:84cd` is called at `03:94a2`. The ordinary loop can reach
that call directly; the `fd85` completion handshake at `03:9499..94a1`
is not an unconditional prerequisite. The control run observes 40 application
calls, one successful dequeue and zero entries at the two processing waits.
The unresolved contract is the original sender's command sequencing and
the legitimate updates to the previous-command fields, not absent receive
delivery or a demonstrated processing stall. Recover that contract before
changing packet semantics or claiming recording finalization.
A literal-word scan of all 57,875 extended words in the original AA88
upload finds four `3766/3767` operands, all in this dispatcher: two reads
and its two previous-command updates. This is direct-reference coverage,
not proof that no indirect writer or initialization record exists.
Reproduce it with `make check-mu4-bootstrap-original
MU4_BOOTSTRAP_SOURCE_RUN=tmp/native-gates/mu4-retained
MU4_BOOTSTRAP_BIOS=bootreccontrol`.
Its receive PASS label proves native packet receipt and acknowledgement,
not successful control-handler execution or recording.

Isolated replays after the MMR bus, INTR SP ordering and XC protection
corrections distinguish this duplicate filter from the save fixture's CPU
boundary. `tmp/mu4-recorder-control-xc/error.log` receives
selector `36`/parameter `4` at 11.406 seconds with previous class/selector
`1/36`; native receipt/acknowledgement passes in this atomic diagnostic, while recording completion remains
unclaimed.
With input-derived CLKMD rates and the C5416-compatible reset profile,
`bootrecsave` receives control at 10.086 seconds with previous selector `49`.
The resident image returns at approximately 10.718 seconds and completion
reaches `8009`. The 20-second endpoint is `02:3d79`, ST1 `2900`, IMR `0ac9`,
IFR `0c78`, cursor 27/50 and RX-ready set: 611 application calls, two dequeued
messages, 23 processing-wait entries and 611 processing-return observations.
RX/TX are 36/47, with three responses and two ACKs; TIM/PRD/TCR are
`0000/0001/0000`. It exits 3 because metadata has not been delivered.
These counts and endpoint PC are observations, not acceptance constants.
Correcting the reset/PLL execution rates changes progress and phase but does
not remove the warm timer/receive stall. The source and isolated NAND copy
compare byte-for-byte with NVRAM saving disabled and no firmware-state forcing.
The control fixture's working warm dispatch does not close the save fixture's
timer/interrupt recognition and drain boundary.
Packet-delay tuning is not a substitute for that CPU contract.

Resident command and loader addresses are reused by the recorder overlay;
their resident trace counters must stop when that overlay takes ownership.

Create a fresh firmware-owned source first:

```sh
make check-mu4-retained-original MU4_RETAINED_RUN_DIR=tmp/native-gates/mu4-retained
make check-mu4-bootstrap-original \
  MU4_BOOTSTRAP_SOURCE_RUN=tmp/native-gates/mu4-retained \
  MU4_BOOTSTRAP_RUN_DIR=tmp/native-gates/mu4-recorder \
  MU4_BOOTSTRAP_BIOS=bootrecsave
```

Use new output paths for subsequent runs; extraction does not overwrite them.
Without explicit output paths both gates allocate unique directories under
the disk-backed `NATIVE_SCRATCH_ROOT` (default `tmp/native-gates`).
`MU4_STORAGE_SECONDS` defaults to 1,800 emulated seconds: the one-millisecond
upload cadence plus two complete readbacks at the compatible reset divider
outlast the former 1,200-second reset-fixture limit. This is an outer harness
deadline, not a timer/serial/NAND calibration change.
Fresh original provisioning, reset/reload and fresh-process status service
pass with this clock profile. The resulting NAND is byte-identical to the
previous firmware-created source, without donor provisioning or a runtime
snapshot. The independent complete-startup `bootstatus` gate also passes.
The atomic `bootrecsave` metadata diagnostic fails at the stated receive boundary;
this retained negative control does not contradict the staged `bootrecmeta`
metadata/persistence acceptance above. Use `bootrecord` for the initial
recorder-loader diagnostic.
This runs in an isolated NAND copy, forbids NVRAM saving and verifies that
the source disk file remains unchanged. `bootstatus` supplies the independent
complete-startup lifecycle observation.
Use `MU4_BOOTSTRAP_BIOS=bootrecordram` for the provisional shared-RAM probe;
it passes startup, native response and execution checks. It is not a
passing recording acceptance gate.

Reproduce the command-table extraction with
`tools/noki5510_a00_inventory.py --segment aa22 --extract-program-range
0x240f5 0x24109 --disassembler-little-endian --output <new-path>` on the
original `InitData_R060.a00`. The twenty-word output SHA-256 is
`6f1e6fa4d54f681f90fea6f512a00127a6aa26b6013d3e55b3cfb4cad8d444c0`.
Coverage is this recovered twenty-entry branch only; other selector tables,
indirect state writers and actual recording execution remain separate work.

The pin bench registers its external receive/transmit waveform state,
request/acknowledgement cursors, serial shadows and NAND GPIO latches for
save states. Its growing TX observation vectors use bounded 64-word
snapshot storage with checked counts, rather than registering an empty
vector's fixed initial address and length.

`MU4_STORAGE_BIOS=replay` waits until the independently checked streaming
prefix completes, then checkpoints after three data bits of the first request
byte. Both original-firmware continuations reach the same worker-window
endpoint, consume the 11-byte request/acknowledgement stream, transmit the
same 14 response bytes and drain the firmware queues without retry. The
uncompressed terminal save streams match across all registered
emulation-state items: CPU, RAM, NAND, codec, DMA, McBSP, external peer and
emulation timers. The only exclusions are nine fields of
`timer/lua_engine::resume/`: MAME's Lua post-load callback deliberately resets
that frontend timer, and this fixture runs no Lua script. Restore runs in
a separate control callback so the terminal-check timer retains its saved
start/expiry state. Observer counters and the test-phase sequencer are not
a restored lifecycle oracle. This proves the recovered status transaction's
native continuation, not arbitrary mid-boot checkpoints, native reset,
processing commands, physical board attachment or full MU4 boot.

`check-mu4-bootstrap-original MU4_BOOTSTRAP_BIOS=bootreplay` applies the
same settled mid-byte checkpoint and terminal-state comparison after
the complete original uploaded startup, using retained NAND supplied by
`MU4_BOOTSTRAP_SOURCE_RUN`. Both continuations satisfy the unchanged
status and pin-receive checks, and all 1,040 registered emulation-state
items match, with the same nine Lua frontend-timer exclusions. Original
startup/resident entry counts remain one: restoration resumes the DSP,
not the upload or reset sequence. Read/stream observation counters are
not restored hardware state and therefore accumulate across both legs;
their totals must not be interpreted as one uninterrupted playback.
This extends the verified checkpoint to complete-upload native control,
not music decoding or physical board reset.

`check-mu4-bootstrap-original MU4_BOOTSTRAP_BIOS=bootmeasure` sends the
unchanged measurement request through McBSP2 pins after complete original
startup, using the same retained-medium source. The measurement and tone
observers/verifier are shared with the routine-level `measure` profile,
not a second, weaker acceptance path. Six stereo sample-energy blocks
match independent arithmetic applied to live firmware input reads; the
12-byte response is acknowledged, all 11 received bytes drain, and the
firmware returns to mode zero with empty queues. The fixed 20-second run
also checks 42 stereo tone blocks (5,376 buffer samples) and 12 signed
phase wraps against independent table/phase/scaling calculations. Block
counts are observations of this fixture, not a physical timing contract.
The codec input remains a declared converted-sample fixture. These checks
prove native DSP processing and generated tone buffers after original
startup, not accepted media decoding, analog output or native speech.

The same profile continuously compares every McBSP0 transmitter word with
the independently pin-decoded AIC23 DIN word. A four-word bounded comparison
queue rejects overflow, unpaired codec words, channel-order errors and
sample mismatches; it is registered for save states and cleared on reset.
The complete 20-second observation matches 1,744,936 words, including
5,160 nonzero words, with no pending word at its endpoint. Acceptance
requires coverage beyond the old 1,024-word prefix, nonzero output and at
most one final launched-but-not-yet-sampled word; exact volume is not a
physical clock-rate oracle. This verifies sustained digital transport
during native processing; the source-selection proof below connects tone
samples to that transport. It does not model the DAC/analog output.

Original worker `02:955b..9588` writes DMA source-reload index `32`, selects
`ba79` or `baf9`, calls the generator and flips `bb86`. Each bank contains
64 left samples followed by 64 right samples. A passive data-read observer
uses side-effect-free DMA channel-3 **active source-read** inspection to
distinguish DMA reads from CPU operands and other channels, without changing
the DMA register index. The older `source_matches` inspector identifies an
enabled programmed source only; an address match alone cannot attribute a
bus read to that channel. `source_read_matches` is scoped to the actual
synchronous source read and is false during the destination write and
outside the transfer. Device conformance checks cover ordinary reads while
enabled, wrong channel/space/address, cancellation and a pending-transfer
save/restore. The symmetric `destination_write_matches` inspector is true
only inside the actual destination bus write, after source-read ownership
has ended. Executable data/program-write checks reject wrong channel,
space and address, exclude ordinary fixture writes, require matching
source/destination transaction counts, and cover cancellation and pending
save/restore. Neither inspector changes the DMA index or persists a bus
owner into a save state. I/O-space destination ownership is not independently
exercised by this fixture. Use this explicit write owner when binding
playback source reads to DXR; matching a word or timestamp alone is not
ownership evidence. The complete playback capture now validates this
source-to-DXR leg. Bounded internal load/completion attribution is covered by the
[serial-epoch contract](aac_validation.md#actual-shift-buffer-and-din-epochs);
full-session DXR-to-TX accounting remains separate.
Every uncancelled generated word matches the independent
tone calculation in alternating left/right source order. Bounded queues
then require every observed DMA source word to match McBSP TX, followed by
the existing independent codec-DIN comparison.

The command's original cleanup matters: `03:b056/b058` zero both tone
banks while the last DMA block is in flight, before `03:b05e` writes mode
zero. The observer recognizes only those exact CPU store boundaries and
zero values, updates its expected source image and accounts for samples
cancelled before DMA read. This fixture generates 5,376 words: 5,180 are
independently verified through DMA and 196 are cancelled by original
cleanup. The complete source stream has 1,744,937 reads, 1,744,936 completed
transmitter words and one pending source word. Acceptance requires exact
generated = consumed + cancelled accounting, no premature bank overwrite,
stereo source order and source-to-TX-to-DIN agreement. These quantities
describe this bench, not real-board rates or a requirement that every
generated sample must play before a command stops.

`noki5510_a00_inventory.py --extract-program-range START END --segment aa22`
reconstructs final logical words in record order (last write wins), rejects
holes and verifies the selected wire checksum. Optional
`--disassembler-little-endian` exports GNU tic54x disassembly input, not
wire bytes or inferred physical aliases. Unit tests cover overlap, byte
order, holes, range rejection and checksum corruption.

McBSP error recovery is modeled for the supported externally framed,
single-phase 8/12/16-bit modes. SPRU302B section 2.3.7.4 specifies that TX
underflow lowers XEMPTY and repeats old DXR at subsequent frame syncs;
fresh DXR raises XEMPTY only when transferred to the shifter. A mid-frame
underflow waits for a new frame sync, even after refill. Section 2.3.7.1
defines receive overrun after three unread words: DRR and RBR survive,
incoming RSR data may be lost, and a side-effectful DRR read clears RFULL.
Executable gates check framing, status, ready edges, reset and save/restore
of both error states. Wider, internally clocked and multichannel modes are
not established by these tests. Routine profiles check the first 1,024 DIN
words; the complete-startup measurement profile checks digital transport
continuously as described above. Neither transfer counts nor pin-decoded
output alone prove music decoding.

SPRU302B section 2.3.7.3 additionally defines transmit overwrite: a new DXR
write before DXR-to-XSR handoff replaces the buffered word rather than
stopping the device. The supported external-frame implementation preserves
any word already in XSR and does not invent another readiness edge. Three
pin-driven cases exercise overwrite before the first frame and overwrite
of the next word while XSR is active, checking exact serial bits, completed
words and IRQ counts at 8/12/16 bits. The old abort is reproduced in
`tmp/mcbsp-overwrite-red/output.log`; the full original-storage gate requires
`mu4_mcbsp_overwrite: PASS` alongside its existing underflow/overrun and
save-state gates. The complete gate passes with
`MU4_STORAGE_RUN_DIR=tmp/mcbsp-overwrite-green`; its isolated `error.log`
retains the original storage, native entry, serial/DMA and error-state
results. This extends generic serial error behavior, not native
recording completion or speech.

SPRU302B sections 2.3.6.2/2.3.7.2 define unexpected receive-frame sync:
with RFIG clear, discard the partial word and pending RBR, retain unread DRR,
restart reception at the new frame, and set sticky RSYNCERR (SPCR1 bit 3).
With RFIG set, ignore the early frame and finish the original word. RSYNCERR
is software-writable, clears on receiver reset, and drives the receive IRQ
in RINTM mode 3; DRR reads do not clear it. Six external-pin cases cover both
behaviors at 8/12/16 bits, retained DRR, software set/clear and reset. A separate
save/replay case restores a half-received replacement word with RSYNCERR and
its IRQ held, checking exact payload replay and no duplicate IRQ edge.
The full original-storage gate requires `mu4_mcbsp_receive_sync: PASS` and
`mu4_mcbsp_receive_sync_restore: PASS`; both pass in
`MU4_STORAGE_RUN_DIR=tmp/mcbsp-rsync-restore`. These mid-word tests do not
establish terminal frame packing, which has separate coverage below. This
is generic McBSP recovery evidence, not recorder completion or native speech.

Receive frame packing follows SPRU302B section 2.3.5.3 and figures 2-23/2-24:
the minimum frame period is the number of bits in the frame. With data delay
`D`, an expected FSR can arrive while the previous final word has `B <= D`
bits left. The receiver finishes that tail and retains the next frame with
`D-B` delay clocks remaining, without RSYNCERR or discarding either word.
RFIG ignores premature sync, not this legal overlap. An FSR one clock before
the minimum period instead uses the documented recovery/ignore contract.
The pin fixture covers 36 legal three-word streams (8/12/16 bits, delays
0/1/2, gaps 0/1, both RFIG values) and 18 premature-sync controls, checking
exact words, ready edges and error status. Save/replay retains a pending
next frame while the previous word's last bit is still unreceived.
The original-storage gate requires `mu4_mcbsp_receive_packing: PASS` and
`mu4_mcbsp_receive_packing_restore: PASS`. Cases yield between scheduler
callbacks so CPU interrupt synchronization drains normally; interrupt delivery
is not suppressed. Multiple pending receive frames remain unsupported.
The full gate passes with `MU4_STORAGE_RUN_DIR=tmp/mcbsp-rxpack-yield`,
retaining the original storage, native-entry and DMA checks. This validates
generic serial reception, not native recorder completion or speech.

The TX sync-recovery contract is distinct: SPRU302B section
2.3.7.5 and figure 2-36 show an early FSX with XFIG clear restarting the
current XSR word from its first bit, while a newer word already queued in
DXR remains queued. It sets sticky, software-writable XSYNCERR (SPCR2 bit 3),
cleared by software zero or transmitter reset; XINTM mode 3 selects its IRQ.
XFIG set ignores the pulse. The figure explicitly shows partial B followed
by complete B, then queued C, without an extra XRDY transition at restart.
`tx_frame_w` restarts the preserved XSR directly, not via `load_shift`, which
consumes DXR and would replace B with C. Six pin-driven cases check
partial-B/full-B/C ordering at 8/12/16 bits, frame-ignore as a negative control,
sticky status/software/reset behavior and readiness-edge counts. A save/replay
case retains a half-shifted restarted XSR, queued DXR and held error IRQ,
checking the remaining physical bits and no duplicate IRQ edge. The full
original-storage gate requires `mu4_mcbsp_transmit_sync: PASS` and
`mu4_mcbsp_transmit_sync_restore: PASS`. Terminal frame packing has separate
pin-level coverage below; XINTM modes 1/2 and
internal-clock sync recovery remain unvalidated.
The complete gate passes with
`MU4_STORAGE_RUN_DIR=tmp/mcbsp-txsync-restore-final`, retaining the original
storage, loader/native-entry, DMA and existing serial error-state results.

Transmit frame packing uses the same minimum frame period from SPRU302B
section 2.3.5.3, but not the receiver's delay counter convention. Zero-delay
TX emits its first bit asynchronously at FSX; nonzero delay includes the
first shifting edge. An expected next-frame sync that overlaps the final XSR
tail retains both that tail and the queued DXR word, then starts the next
frame with the unelapsed delay. It does not restart XSR or assert XSYNCERR,
even with XFIG set. The fixture drives FSX before the selected shift edge and
checks the bit count after every clock as well as complete payloads and
ready IRQ edges: 36 cases at 8/12/16 bits, delays 0/1/2, gaps 0/1 and both
XFIG values. A save/replay test retains the final old bit, queued DXR and
pending next frame, checking every remaining output bit and one new ready
edge. The full original-storage gate requires
`mu4_mcbsp_transmit_packing: PASS` and
`mu4_mcbsp_transmit_packing_restore: PASS`. Multiple pending transmit frames,
sync between words of an active multiword frame, and other sub-clock FSX
phases remain unsupported or unvalidated. This is controller evidence, not
native recorder or speech completion.
The old controller's terminal-overlap abort is reproduced in
`tmp/mcbsp-txpack-red.log`; the full original-storage gate, including overlap
save/replay and per-edge assertions, passes with
`MU4_STORAGE_RUN_DIR=tmp/mcbsp-txpack-final`.

Side-effect-free five-second boundary snapshots show words `142c/142d`
and `145c/145d` both remaining `0001,e9ff`; word `145e` changes from zero
to six. Do not label either double-word pair as a byte cursor from this
observation. The last NAND read-address sequence changes from column `00`,
row bytes `02,e9,01` to column `00`, row bytes `98,e9,01` (rows `01e902`
and `01e998` under the validated small-page address grammar). Thus the
bounded startup reaches later physical rows, rather than simply repeating
one row. These two samples do not prove monotonic traversal, the directory
extent or a termination time. In particular, these row samples must not be
attributed to the earlier filename lookup once its return is observed.

At index six, `03:bb7b` performs unsigned `CMPR` with AR2=`0006` and
AR0=`0200`; `03:bb7c` sees TC set and takes the read path. The iterator
limit here is 512 entries, recovered directly from the comparison operands.
Address helper `03:bd13` receives ordinal six. Subsequent
byte-reader entries at `329d` receive A=`0001:e9ff` and stack arguments
`00c0`, `00c1`, …, `00cb` with source descriptor `1422`; the later filename
lookup uses copied descriptor `3aea` with the same address and offsets.
The ordinal-to-offset mapping is six times 32 bytes. Original `03:bbd6`
reads eight name bytes, three extension bytes, then attributes and the later
word/double-word fields of that fixed-size directory entry.

The common reader's cache works in this observed path: desired word index
`0060` is preserved, the count rises from zero to `0060`, and later reads
reuse cached row `0001:e9ff`. At branch opcode `327e`, measured differences
are `005f`, `005e`, `005d`, `005c` as the count increases. The address builder
at `3035` increments the requested row to `0001:ea00`; `3050` is `5700`
(`DLD` into B), and subsequent masks/shifts emit the correct address bytes.
The iterator and this cache are not the current stopping point. Read-tap
register snapshots are pre-execution; probe opcode boundaries, not extension
words, and do not treat a destination register's old value as a return result.

`mu4_native_entry: PASS` establishes the two observed entry reads only;
absence of an illegal opcode is not a complete-startup acceptance criterion.

Three generic CPU contracts are independently exposed by this run:

- `6fe1 0010 0c48` at `3ac3` consumes long Smem displacement before the
  shifted-operation extension, not after it. The old synthetic `6fea`
  encoding is corrected while retaining its result/preupdate/cycle checks.
- `f808` in arithmetic helper `4a2c` is carry-clear conditional branch.
  C/NC, including delayed forms, now have eight status-preserving fixtures.
- `60e1 0002 0001` compares context field +2 with one. Long `CMPM` consumes
  displacement before immediate; four fixtures check match/mismatch and
  preupdate/no-update. The reversed decoder falsely reached trap `4330`
  despite a valid context type. Do not record that trap as a media defect.

The core suite and native ROM4 regression guard these corrections outside
the MU4 routine fixture. Unrelated multi-extension opcodes still require
independent evidence; a synthetic instruction stream is not an encoding
authority.

This exercises generic core contracts that matter beyond MU4: long-offset
`BANZ/BANZD` tests the effective Sind value and consumes displacement before
target (96 cases); `CMPR` compares unsigned ARx against AR0 (192 cases);
and block repeat uses ST1 bit 15 for BRAF without clearing bit 14 CPL.
The core gate asserts active and retired BRAF with CPL set. These rules follow
TI SPRU172C and SPRU131G; memory-copy extension ordering is also checked
against original firmware execution rather than a synthetic test alone.

This is original-routine execution, not a complete DA150 board: the fixture
holds ADD_H enabled, uses a test clock and separate test program/data maps,
and supplies a reset wrapper. It neither seeds a filesystem nor selects an
overlay. U201 is not attached to a handset yet; address-enable ownership,
physical memory mapping and original media placement remain prerequisites.

This routine-level address-map control does not qualify media placement or
overlay selection. The independently built native storage/loader fixtures are
described in [MU4 hardware](mu4_hardware.md); a valid FAT disk alone is not
evidence that an overlay payload was loaded by the original loader.
