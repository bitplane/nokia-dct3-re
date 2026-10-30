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

## Contract references

- [Execution, staged/atomic comparison and timing maps](rom4_dsp_execution.md).
- [Independent arithmetic and original-code controls](c54x_arithmetic_observations.md).
- [RF/XF capture and vendor-reference methods](rom4_dsp_capture.md).
- [Native bootstrap and assist-negative observation controls](rom4_native_observations.md).
- [Native speech activation and buffer maps](native_speech_activation.md).

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
[the current native boundary contract](native_speech_activation.md#native-speech-activation-and-pcm-buffers).
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
New BRC admission requires an inactive staged block controller; it does not model in-loop
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
measured silicon phase. Only standalone STLM `0021` is admitted by this
contract; compound delay/repeat/XC behavior remains unvalidated.
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
retarget or repeat the transfer. Unsupported CPU/peripheral destinations
remain outside this admission; prototype exclusions are not hardware traps.
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
in [COBBA serial-control ownership](rom4_audio_transport.md#serial-control-ownership-correction).

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
