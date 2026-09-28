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

`make check-c54x-rom4-cold-execute` independently starts the local clean-room
core at the mask-ROM reset vector with only the recovered program image and
complete `0xb000..0xefff` DROM populated. Reset reaches PC `0x0f00`, where the
program word is still zero, and stops at PC `0x0f01`. This is the expected MCU
upload boundary: loader1 is not resident in the mask image and isolated DSP
execution cannot proceed by pre-seeding it.

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
An instruction-PC census of the coherent no-cell run resolves the default
route more tightly. `0x00ac=0` fails both mode comparisons at `0x322e/0x3232`,
so INT0 runs the four-read block at `0x3249..0x325b` eight times. It then
reaches `0x3268`, tests `0x00af` at `0x326b..0x3272`, and branches directly
to `0x32c3` and the common exit at `0x33f3`. The `0x3274..0x32c2` work,
including calls to `0x48d0`, `0x42f6`, and `0x44b6`, does not execute on that
first zero-input frame. These are observed branch PCs, not decoded purposes
for the callees. ROM `0x4414..0x4460` is an initializer that stores `0x00ac`
and `0x00af`; direct call sites include `0x09a4`, `0x0ab8`, `0x4de3`,
`0x4e00`, and `0x9b62/0x9b9f/0x9bb0`. Whether ordinary acquisition reaches
this initializer at all remains unproved. A focused 30-second coherent no-cell
run recorded zero
executions at `0x4414/0x4428/0x4433/0x4460` while servicing 6,497 frame
expiries and 207,232 port-`0x27` reads. Thus the observed baseline loop does
not itself establish that this mode initializer or its result-publication
branches are reached. No firmware or DSP state was forced for this census.
The host-command jump table is distinct: DSP code `0x398d..0x3997` adds the
incoming type to data-ROM base `0xb00f`, reads the function pointer, and
branches through it. Type `0x1a` selects table entry `0xb029 = 0x3d5e`,
consistent with the observed `0x3d70` search-list handler; it does not select
`0x4414`. Direct calls to `0x4414` instead come from `0x09a4`, `0x0ab8`,
`0x4de3/0x4e00`, and `0x9b62/0x9b9f/0x9bb0`. The last group is reached from
the separate `0x3660` control dispatch or ROM function lists. This classifies
`0x4414` as a DSP control-mode operation, not a direct host-packet handler.
A changed-write watch over the same 30-second boot found no writes to
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
setting the flag. In the coherent 30-second no-cell run, a changed-write watch
recorded zero writes to `0x1949`, `0x1953`, or `0x1973`. These are dormant
control-mode paths in that run, not evidence of a missing direct MCU command.
The source of the transition, and whether it belongs to ordinary search or a
later channel mode, remain open. The recovered ROM also has three `PORTR`
sites for port `0x39` (`0x40ff/0x4102/0x41a4`) beside
port-`0x38` status reads. A coherent 12-second run records zero reads on both
`0x38` and `0x39`; those sites are dormant while the `0x27` loop runs. Their
activation and sample encoding must be recovered before a controlled GSM
burst can be meaningfully attached.

Port `0x27` is bidirectional in ROM4: seven static `PORTW` sites at
`0x4248/0x424d/0x4258/0x425d/0x4267/0x426c/0x4271` belong to a separate
transmit routine. The driver now forwards those writes to a replaceable COBBA
transmit callback, as it already did for receive reads; no transmit activity
has been observed in the coherent boot. The port-write sites on
`0x31/0x32` are `0x36f7/0x36fb/0x370c/0x3710/0x4020/0x4025/0xa23a/0xa23e`.
None ran in the 12-second census (`rf_port32_writes=0`), so the payload and
trigger remain unresolved. Calling these ports a synthesizer pair was
premature: Nokia's NSE-1 manual assigns synthesizer control to MAD2's SCU,
while COBBA produces analog TXC and AFC signals. The manual also describes
the COBBA parallel interface as carrying control and receive/transmit samples
over a 12-data-bit bus; that corroborates the boundary, not the sample encoding:
<https://www.eserviceinfo.com/preview_html.php?fileid=26879&previewid=13251>.

The SIM transaction ending near 8.51 seconds is not a stalled initialization
sequence: the firmware has read all ten configured ADN records. At 31.002
seconds it organically issues `A0 F2` STATUS as its periodic card-presence
monitor while the UI also refreshes the LCD. The long quiet interval is a
healthy maintenance cadence and was unrelated to the former masked-INT0
receiver boundary.

## RF ownership and capture target

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
The next static target is the initializer/dispatcher that assigns `0x121f` and
the `0x18xx/0x19xx` block; do not synthesize port-`0x38/0x39` readiness before
that firmware-owned mode transition is identified.
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
40-bit wrap. Fixed-high-word loads and multiplier families remain separate
audit surfaces.
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
CALLD and RETD likewise use two and three cycles respectively, rather than
four each. A cycle-stamped call/return fixture checks both delay pairs and
stack balance against SPRU172C pages 4-27 and 4-139. CALA/CALAD retain their
documented six/four-cycle costs.
The long-immediate ALU decoder charges two cycles, including the separately
decoded XOR form. A cycle-stamped LD fixture checks the additional cycle;
the LD contract is SPRU172C page 4-68. This does not cover all extended-address
or repeated multicycle instruction timing.
This documented timing correction deliberately re-banks the RF startup offset:
the first port-27 read is at frame 23, 0.133172 s, rather than the historical
0.128149 s. At 30 seconds there are 6,497 frame expiries and exactly 207,200
reads (`32 * (6497 - 22)`), with unchanged terminal IMR/IFR and no burst-port
activity. The gate retains an exact cadence assertion, not a tolerance.
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

A focused consumer trace closes the direct type-`0x1a` activation hypothesis.
The resident host-command dispatcher advances the transmit-ring consumer at
C54x PC `0x3909`. The selected handler spans `0x3d70..0x3db6`: it derives
local value `0x0010`, copies the payload into scratch words `0x1200..0x121f`,
updates control
words `0x0284/0x0286/0x0287`, sets bit 3 at `0x06bc`, and increments `0x06e3`
from zero to one before returning. It performs no I/O-port access. Across the
same 30-second receiver gate, none of those identified control words is read
after the handler returns; the existing port-`0x27` cadence continues and the
`0x32/0x38/0x39` counts remain zero. Temporary write/read taps used for this
classification were removed. Thus the observed search-list packet is accepted
and stored, but does not by itself enter the dormant parallel receive path or
establish an RF acquisition. Which lifecycle consumes the stored control state
and which DSP path owns ordinary acquisition remain unresolved. Separately,
the `0x7b0a` mode initializer remains useful for later dedicated-channel work;
injecting a reply or waveform at type-`0x1a` would skip both boundaries.

The post-handler reader census covers `0x1200..0x121f` and the complete
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

`tools/c54x_opcode_coverage.py` compares `[opcov]` records from a verbose
30-second 5110 v5.30 run with the standalone `tms54test` fixture. The current
idle run dispatches 457 distinct opcode words in 91 high-byte groups (set SHA-256
`e5ab0413453f271100996a54cea8f712eebe6381d4f7b4f7bf959f55631efefc`).
The tap is in `execute_run` immediately before `execute_one`, not in the
extension-word fetch helper: these are instructions dispatched by the current
emulated core, not raw program-memory reads or independent silicon evidence.
`first_pc` is only the first observed site; each count aggregates all sites.
The fixture dispatches 274 distinct words: 228 overlap the ROM4 run, 46 occur
only in the fixture, and 229 ROM4 words do not occur in the fixture. These are
*word* counts, not instruction-family counts. Fixture execution alone is not
proof that a particular result is asserted, and this one boot is not a census
of every possible ROM4 path.

The coverage tool now separates fixture execution from explicit result
assertions. Of the 457 ROM4 words, 198 have an `opassert` marker after a
passing exact-word check, 30 execute in the fixture without such a marker,
and 229 are absent from the fixture. The 30-word class includes setup and
control instructions as well as older checks not yet tagged; it is not a
claim that all 30 lack semantic tests. The formerly leading challenge-loop
words `6d8c`, `8084`, `108a`, `1a8b`, `1c84`, and `e598` now have isolated
assertions as well as the aggregate transform-result check.
The 229-word class is a priority list
for new fixtures, ordered by observed execution count, not proof that those
instructions are incorrect. A marker establishes the checked outcome only,
not complete coverage of an instruction's operand or flag variants.
`--group-report` with `--fixture-log` ranks unasserted ROM4 executions by
opcode high byte. This is a workload ranking, not an instruction-family
decoder. In the current 30-second run, `f4` leads because NOP executes over
one million times. The formerly dominant `4a`/`8a` gaps are now covered by
a twelve-register MMR save/restore fixture: it initializes each register,
checks all twelve stack values, overwrites the registers, and checks the
restored values after the reverse pops. This adds exact checks for 24 ROM4
stack words and the ROM4 `STM` initializer words. The next high-volume
unasserted group is `77` (mostly executed-only immediate MMR stores).
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
premature execution of the next word; it does not independently time the
minimum or establish all wake-source distinctions among IDLE 1/2/3.
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
change was needed. The ROM4 trace exercises long-offset `MAR` explicitly,
but does not establish generic long-offset Smem reads or writes. Those
MOD12-14 forms remain a decoder and cycle-cost gap, not a validated feature.
The one-word Smem arithmetic/logical handlers (`ADD`, `ADDC`, `SUB`, `SUBS`,
`AND`, `OR`, `XOR`, and `SUBC`) now charge the extra cycle TI SPRU172C assigns
to absolute addressing. Exact `07f8`, `1af8`, and `08f8` fixtures check an
arithmetic carry input, a logical result, a signed subtraction result, and
their two-cycle costs. The core previously charged one cycle to these
absolute forms. This does not establish long-offset decoding or every flag
and accumulator variant in the family.
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
two-cycle indirect costs. Other shift, flag, and long-offset variants remain
unasserted.
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
