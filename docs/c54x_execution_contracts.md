# C54x execution contracts

## Authority and scope

This is the processor-validation reference, separate from handset bring-up
and loader ownership. The core implementation lives in
`cpu/tms320c54x/`; executable acceptance lives in
the compiled `driver/tms320c54x_test*.cpp` modules and
`tools/run_c54x_core.py`.
The [default-suite checkpoint](#core-regression-checkpoint) identifies the last
fully qualified default run by executable and source hashes. A later focused
fixture run does not silently renew that checkpoint. Other sections retain their explicitly bounded
instruction, composition, original-firmware and negative-control scopes;
their older executable hashes are not competing default-suite baselines.
Passing CPU tests does not establish physical pipeline timing, a fitted
handset DSP mask, RF acquisition or end-to-end speech.

The external observation-only ROM4 opcode inventory contains 628 distinct
words in 112 high-byte groups after 30 million MCU instructions; sorted-set
SHA-256 is `63ae45c872ccea39112898bfde7f5926acd675701675a860d765a66a35815d34`.
`tools/c54x_opcode_coverage.py` retains this reference. It measures reachability,
not correctness or current core coverage; imported interrupt masks can change
reachability without establishing a legitimate hardware reset state. The
loader document owns that mask-provenance contract.

### Harness preservation contract

`fixtures/c54x_conformance_baseline.json` freezes the declared BIOS inventory,
464 isolated runner profiles, seven named scopes, 293 default-suite result
requirements, fixture PASS formats and 4,202 lexical assertion-pattern sites
(a source-pattern count, not an executed assertion count). Exact result
strings retain case/restore counts and scope exclusions. Check it with:

```sh
.venv/bin/python tools/c54x_harness_inventory.py
.venv/bin/python -m unittest tools.test_c54x_harness_inventory
```

The CLI test exercises every isolated profile's machine/BIOS selection and
result checking using simulated process output. The inventory is a structural
preservation check, not executed coverage, an independent arithmetic oracle
or evidence of silicon timing. Native conformance and affected handset gates
remain necessary. Never automatically regenerate the baseline to make a
refactor pass; `--write` is reserved for an explicitly reviewed contract change.
Expected arithmetic/timing results must remain independent of CPU execution
helpers and shared profile-selection metadata.

`tools/c54x_profiles.py` reads literal BIOS ownership from the C++ declarations;
the runner does not maintain a second machine/profile list. Unsupported or
ambiguous declarations fail closed. Exact expected results live separately in
`tools/c54x_test_oracles.py`, without imports from the CPU or profile decoder.
`tools/run_c54x_core.py` owns CLI selection, process execution and result
validation, retaining its established checker/import API. The seven named
scopes share one selection table; the frozen baseline independently checks
their result contracts and all 464 isolated machine/BIOS selections.

The bench is separate from the processor implementation. Its current source
boundaries are:

| Source | Responsibility |
| --- | --- |
| `driver/tms320c54x_test.cpp` | Machine/ROM declarations, backend serial bench and stock-loader verifier compositions |
| `driver/tms320c54x_test_state.h` | Shared bench state, fixture types and method declarations; no CPU execution implementation |
| `driver/tms320c54x_test_state.cpp` | Bench lifecycle, address maps, assertions and device wiring |
| `driver/tms320c54x_test_dispatch.cpp` | Ordered callback routing and the initial-program completion tail |
| `driver/tms320c54x_test_arithmetic.cpp` | Arithmetic, multiplier and logical-operation fixtures and checks |
| `driver/tms320c54x_test_addressing.cpp` | Address generation, memory/register transfers and operand-domain fixtures |
| `driver/tms320c54x_test_pipeline.cpp` | Control flow, repeats, dependency and pipeline-composition fixtures |
| `driver/tms320c54x_test_interrupts.cpp` | Interrupt acceptance, entry, return and mask-composition fixtures |
| `driver/tms320c54x_test_peripherals.cpp` | Bus, clock, serial, DMA and timer fixtures |
| `driver/tms320c54x_test_restore.cpp` | Explicit cross-context snapshot/restore fixtures; each other family retains its own replay checks |
| `driver/tms320c54x_test_disassembler.cpp` | Bounded decoding and operand-domain matrices |
| `driver/tms320c54x_test_rom4.cpp` | Acquired-ROM routine execution and independent reference comparisons |
| `driver/tms320c54x_test_atomic.cpp` | The sequential legacy atomic test program, including same-callback fall-through |

The result dispatcher preserves phase-check order, including paths that update
the phase and fall through within one callback. Family extraction must retain
that ordering and distinguish callback returns from returns inside observers'
lambdas. Shared bench state and snapshot/replay ownership are unchanged; moving
a case does not authorize changing its inputs, expected values or observation
boundaries. The dispatcher delegates consecutive clauses to ordered handlers,
not an unordered subsystem sweep. A handler returning false permits subsequent
clauses to run even if an earlier clause changed the phase. The atomic program
remains one coherent source unit because its inline program construction and
continuations share this ordering contract.
The shared bench declaration and legacy atomic program remain large. Further
splitting needs an actual fixture/state boundary and an ordered-execution
preservation check, not an arbitrary line-count target.

The absolute-alias fixtures share declarative family/count/status profiles in
`driver/tms320c54x_test_profiles.h`; the CPU model does not consume this test
metadata. `test_c54x_alias_profiles.py` executes the host C++ decoder and
compares all10,304 encoded cases/status inputs with their retained domains.
All ten separate MAME rejection matrices also pass with the original markers
and restoration counts. These focused matrices do not extend the separately
qualified default-suite population or change silicon admission claims.

Handset/MU4 state and media acceptance remain in
[5510 bring-up](5510_bringup.md), [AAC validation](aac_validation.md)
and [ROM4 loader](rom4_dsp_loader.md).

CPU admission remains fail-closed. In particular, short Smem MOD=3
preincrement is write-only (SPRU131G table 5-4). The
corrections below cover selected read families, not the entire ISA. The
single-address source/RMW exclusion census below closes the implemented
Smem/Lmem and MMR-source classes, not every positive fixture domain or
multi-operand grammar; passing mechanically decoded cases is not
hardware-legality evidence.
Long-offset MOD=13 preincrement is a distinct read-capable form and must not
be removed by that audit.

The debugger's shifted-memory (`6fxx`) decoder applies the same source/store
distinction: short `98..9f` is valid for STL/STH destinations, not LD/ADD/SUB
sources; address bytes `f9..ff` are reserved. Long-offset MOD=13 remains
admitted. This corrects an older disassembler fixture that treated all 256
address bytes as legal whenever the operation extension was recognized.
The exhaustive matrix distinguishes 108,992 valid and 5,696 invalid operand
tuples, each tested at two program-page positions, separately from reserved
operation extensions. It checks instruction claims and bounded word fetches,
not execution timing or completion of the remaining source/RMW audit.
`tools/run_c54x_core.py --mame mame/mame --rompath mame/roms --profile xorm
--output-root tmp` runs this matrix before its separate 1,024 XORM execution
and restoration cases. Accepted evidence: `tmp/run_c54x_core_45tucxku`.
The [full-core checkpoint](#core-regression-checkpoint) covers the current
source-admission guards; optional profiles retain their separate scopes below.

### Operand-domain acceptance

`source-domain` on `tms54xtra` enumerates all 111 implemented ordinary
Smem/Lmem source/RMW high-byte classes, ten shifted read extensions and four
MMR-source forms. All 30,000 cases and save/replays check short `98..9f` and
unadmitted `f9..ff`, CMPT, two program pages, both CPU frontends and snapshots
before/after rejection. Data reads/writes, program writes and I/O accesses stay
zero; accumulators, T, status, repeat/stack pointers and all ARs are retained.
The independent fixture inventory does not call CPU admission helpers.

The unmodified core fails case 3428: atomic `0ef9` (SUBB) executes an operand
transaction and changes status/repeat state instead of rejecting the alias.
The shared atomic source-operand predicate now covers all implemented
single-address source classes, including the special-load, test and transfer
families omitted by the previous partial masks. Shifted `6fxx` aliases are
rejected before addressing; short `6f98..9f` still uses its operation extension
to distinguish invalid reads from valid destination stores. MMR long forms
retain their separate grammar. The existing 463 isolated contracts, default
requirements and seven named scopes are unchanged; this adds one profile.
Capture: `tmp/fidelity-source-domain-green/run_c54x_core_5tmpwpkh`.

This closes the source/RMW exclusion surface, not positive admission of every
addressing mode, arbitrary repeat/delay compositions, destination-only alias
policy, Xmem/Ymem encodings or physical silicon traps. Existing positive
preincrement stores and long-MOD13 reads remain distinct contracts.

[SPRU131G](https://www.ti.com/lit/ug/spru131g/spru131g.pdf) table 5-4 and
section 5.5.3.1 restrict short MOD=3 (`*+ARx`, Smem byte `98..9f`) to writes.
The rejection banks below check both frontends, CMPT settings, program
pages and two save/load checkpoints. They require no operand transaction,
pointer change, accumulator/T or status effect; stack and transfer banks
also observe stack, program and I/O effects as applicable. These are
fail-closed emulator contracts, not claims about silicon traps.

The shared rejection fixture selects CMPT through ST1 bit 5. Its older
setup instead toggled PMST bit 14, so prior `cmpt=2` output did not establish
compatibility-mode coverage. All twelve profiles in the table below pass
with the corrected setup. This correction does not alter the encoded-source
legality rule or establish CMPT coverage for unrelated fixture helpers.

| Profile | Rejection cases/restores | Selected scope |
| --- | --- | --- |
| `load-mod3-edge` | 9,728 | LD/LDU/LD TS/LDR/shift-16/DLD/variable LD; all ARs and destinations, all 32 variable shifts. |
| `mul-mod3-edge` | 4,352 | 34 classes covering multiply/MAC/square, T/ASM loads, BITT, POLY, immediate MAC, DP loads and RPT; ASM and RC retained. |
| `mpy-mod3-edge` | 256 | Memory/immediate MPY to both destinations. |
| `arith-mod3-edge` | 35,840 | 24 scalar classes and eight variable-shift selectors over all 32 shifts. |
| `bitf-mod3-edge` | 512 | CMPM/BITF with both initial TC values. |
| `subc-mod3-edge` | 512 | Both destinations; standalone and short-repeat SUBC. |
| `long-pair` / `drsub` | 3,584 each | 14 paired-arithmetic classes including C16; rejection adds no positive staged T-variant support. |
| `logic-short` | 2,816 | AND/OR/XOR plus source-side MVDK, MVMD, PORTW, WRITA and MVDP; 576 positive logic cases run separately. |
| `xorm` | 1,024 | ANDM/ORM/XORM/ADDM short-preincrement exclusions; 1,024 legal XORM cases run separately in the same profile. |
| `stack-mod3-edge` | 512 | LDM A/B, PSHM and PSHD; atomic prefix includes `489a` rejection; AR upper bits and SP retained. |
| `pmac-mod3-edge` (`tms54xtra`) | 512 | MACP/MACD A/B; all ARs, CMPT modes, pages, frontends and two checkpoints; no data access or program/I/O write. |
| `delay-mod3-edge` (`tms54xtra`) | 512 | LTD/DELAY source operands; all ARs, CMPT/TC settings, pages, frontends and two checkpoints; no source read, implicit successor write or pointer/T change. |

`mpy-abs-edge` independently checks224 cases/restores for memory/immediate
MPY modes`f9..ff`: both destinations, CMPT settings, program pages, frontends
and two checkpoints. The decoder and staged path do not admit these modes,
but the former atomic helper interpreted every mode`>=f8` as absolute and
read an operand in the first atomic control case. The MPY handler now rejects
these unvalidated aliases before operand access, retaining A/B/T/status and
all ARs. This aligns the emulator's fail-closed admission paths; it does not
establish that silicon traps on these encodings or settle other families'
alias behavior. Canonical`f8` absolute and legal long MOD13 remain distinct.
Capture:`tmp/run_c54x_core_gfni9tle`; the unchanged-core diagnostic is retained
in`tmp/c54x-mpy-alias-negative.log`.

`mac-abs-edge` checks448 cases/restores for the same`f9..ff` admission
boundary: both source and destination accumulators, CMPT settings, program
pages, frontends and two checkpoints. The unchanged atomic MAC path attempted
an operand read at case56 (the first atomic case), despite staged rejection.
MAC now rejects before operand access or T publication, matching MPY and the
staged classifier. All448 cases retain A/B/T/status/ARs with zero data reads
or writes. This is an emulator admission-consistency correction, not a
silicon-trap claim or evidence that a handset uses these aliases.
Negative control:`tmp/mac-abs-edge-before/run_c54x_core_da4c3f2y`;
corrected capture:`tmp/mac-abs-edge-after/run_c54x_core_28eddmjv`.
The unchanged224-case MPY alias control and the complete legal MPY/MAC
profile also pass after this correction; the latter capture is
`tmp/mac-abs-edge-controls/run_c54x_core__82ilqn1`.
The staged NSE-1 Phone book menu oracle also remains exact
(`make verify-5110-menu NSE1_MACHINE=nse1pipe`).

`delay-abs-edge` checks224 cases/restores for LTD/DELAY modes`f9..ff`,
both CMPT settings, program pages, frontends and checkpoints before/after
rejection. The unchanged atomic path performs a data transaction at case56,
while staged execution rejects these aliases. Both handlers now reject before
fetching an absolute address or reading/copying its operand. Canonical`f8`
and legal long MOD13 are unchanged. This is fail-closed emulator admission
consistency, not a silicon-trap claim or evidence of a handset opcode gap.
The512-case short-MOD3 control,15,360 legal DELAY copies and73,728 delayed-control
copies/restores pass separately. Negative capture:
`tmp/c54x-delay-alias-negative/run_c54x_core__7hzrh5d`; corrected capture:
`tmp/c54x-delay-alias-fixed/run_c54x_core_qyy685wz`.
The native staged NSE-1 menu oracle and existing MPY/MAC alias controls also
pass; the tool suite passes5,116 tests.
The current full-default-suite checkpoint also includes this guard; the
isolated rejection/copy matrices retain their own case counts and scopes.

`pmac-abs-edge` separately checks448 cases/restores for MACP/MACD
`f9..ff` aliases, both destinations, CMPT settings, program pages,
frontends and two checkpoints. The unchanged atomic path performs a data
transaction at case56. The corrected handler rejects before reading the
source/coefficient, publishing T or performing MACD's successor copy.
All cases preserve A/B/T/status and every AR with zero data transactions.
The existing512-case short-MOD3 control remains unchanged. These are
fail-closed admission contracts, not silicon-trap claims or positive staged
MACP/MACD support. Negative capture:
`tmp/c54x-pmac-alias-negative/run_c54x_core_dl6032t6`; corrected capture:
`tmp/c54x-pmac-alias-fixed/run_c54x_core_9z1v8zec`.

The existing256-case short-MOD3 MPY rejection control and the complete
`mpy-mem-imm` profile pass separately:98,304 legal MPY cases,262,144 MAC
cases and1,408 MAC exclusions, all with restoration. The legal profile's
capture is`tmp/run_c54x_core_0izklf6w`. These are bounded instruction
regressions; the [core checkpoint](#core-regression-checkpoint) separately
qualifies the complete default suite including the LTD/DELAY and MACP/MACD
absolute-alias guards.
Native staged5110 Phone book menu acceptance and both3210 default/frontier
regressions also pass without rebanking their oracles. This does not expand
handset, RF or speech support.

`rmw-abs-edge` checks896 cases/restores for ANDM/ORM/XORM/ADDM aliases
`f9..ff`, both initial TC values, CMPT settings, program pages, frontends
and two checkpoints. The unchanged core performed an operand transaction
at case56, the first atomic case, while the staged prefix rejected them.
The atomic RMW admission guard now rejects these unadmitted aliases before
address-extension fetching or operand access. Every case retains A/B/T,
ST0/ST1 and all ARs with zero data reads/writes. Canonical absolute `f8`
and legal long MOD13 remain separate supported forms; this is fail-closed
frontend consistency, not a claim that silicon traps or a handset uses an
illegal opcode. Negative evidence is
`tmp/c54x-rmw-alias-negative/run_c54x_core_fvfsp7hq`; corrected evidence is
`tmp/c54x-rmw-alias-fixed/run_c54x_core_xglw8jax`.

The legal atomic XORM, staged absolute XORM, mapped BRC/AR ADDM and active
BRC-loop consumer controls have separate captures under
`tmp/c54x-rmw-alias-controls`. Their scopes do not supersede the full-core
checkpoint or establish untested instruction behavior.

Corrected positive load domains are:

| Profile | Admitted cases/restores | Domain |
| --- | --- | --- |
| `indirect-load` | 6 | Stationary/postdecrement/postincrement, both destinations; two invalid cases omitted. |
| `ldu-shift16` | 54,528 per family | 17 RAM forms plus mapped AR sources; 3,072 invalid cases omitted per family. |
| `ld-ts` | 208,896 | Reindexed 17-form domain; 12,288 invalid cases omitted. |
| `load-variable` | 104,448 | 17 forms and all 32 shifts; 6,144 invalid cases omitted. |
| `indirect-dld` | 23,040 | 15 indirect/long forms; 1,536 invalid cases omitted. |

The LDU/shift-16/variable/DLD banks preserve raw case IDs and skip invalid
forms rather than substituting duplicate legal cases. Shared scalar
arithmetic omits `memory_arithmetic_case` form 5; the four memory multiply/T
banks omit RAM kind 5. Each latter bank retains 52,224 cases: 27,648 RAM
cases over nine forms plus 24,576 mapped-source cases, totaling 208,896
across MPYA/MASA/MACA/MACAR, MPYU, MPY/MPYR and LD T. Unrelated multiply
generators still require independent domain audit.

Memory SQUR uses this same generator and raw-ID exclusion, retaining
52,224 cases/restores rather than admitting the 3,072 short-MOD=3 RAM
cases. `squr-smem` verifies both accumulators, eight patterns, sixteen flag
states, RAM/mapped sources, Read-stage T publication, retained operands and
preservation of a newer T value. Accepted captures are
`tmp/run_c54x_core_pkd3rv1w` (SQUR) and `tmp/run_c54x_core_jyveg6cq`
(512 LTD/DELAY rejection cases/restores). Neither establishes physical
pipeline phases or complete instruction-set conformance.

Other positive domains retain 104,448 scalar ADD/SUB cases, 52,224 each for
ADDS/SUBS, 208,896 shift-16 and 153,984 TS arithmetic cases (6,528 address
plus 147,456 numeric). Variable-shift arithmetic retains 245,760 cases.
CMPM and BITF each retain 52,224 cases over 17 forms, omitting 3,072 invalid
cases. `rpt-subc-st` retains 38,400 SUBC and 38,400 repeated-store cases;
memory/immediate MPY retains 98,304. Paired reads retain 6,144 DSUB,
12,288 DADD and 6,144 DRSUB cases over eight forms, omitting raw form 4.
These totals describe selected fixture domains, not every legal operand.
Circular-input constraints have their own
[contract](#circular-fixture-domains); modulo expectations alone are not
silicon evidence.

LTD and DELAY read `Smem` and copy to `Smem+1`; LTD also loads T
([SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf), pages4-81/4-41).
The implicit successor write does not make the encoded source a write operand.
Their atomic handlers reject short `98..9f` before address modification or
operand access. The unchanged core fails the matrix's first atomic case at
the no-data-transaction assertion. An older default fixture had incorrectly
treated `4d9b` as a valid preincrement read; its positive control now uses
legal long MOD=13, `4deb 0001`, preserving the distinct two-cycle contract.
This is fail-closed source admission, not a silicon-trap claim or new positive
staged LTD/long-DELAY support. MACP/MACD have the same explicit source
guard; MVDK, MVDP, MVMD, PORTW and WRITA are covered by the central
`execute_one` admission check. Destination-side transfers are distinct,
as described below; the implicit write alone is not evidence of a missing
guard. Unenumerated instruction families still require their own audit.

After the guard, `delay-copy` and `delay-copy-ctrl` pass 15,360 and 73,728
legal cases/restores. Native 5110 save/load and the 30-second RF boundary
remain unchanged (20,280 frames, 648,032 reads, 1,537 port-32 writes);
the default 3210 semantic acceptance also passes. The
[core checkpoint](#core-regression-checkpoint) additionally qualifies the
complete default suite with these source guards and corrected fixtures.

Operand role is essential. MVKD/MVDM, PORTR, MVPD and POPD encode a
destination and must not inherit a read-only Smem restriction. In particular,
`7c9a` in operand-fetch case 45 is a valid MVPD destination, not a missed
source rejection. Xmem/Ymem have separate grammars. For `6fxx`, classify the
operation extension before deciding source versus destination; shifted
STL/STH remain unchanged. Rejection support does not imply positive staged
support for ADC/SUBB, paired T variants or otherwise unsupported operations.
MVDP's Smem is a data-memory **source** (SPRU172C p. 4-111), unlike
MVPD and READA destinations. The atomic MVDP handler formerly admitted
short `98..9f`, read data, advanced the AR and could write program memory.
Its early admission guard now rejects that encoding before any operand
effect. The unchanged-core regression fails on a data transaction;
`logic-short` adds 256 cases/restores across all ARs, initial TC, CMPT,
program pages, frontends and two checkpoints. The full bank requires zero
data/program/I/O writes, zero source reads and unchanged pointers/status.
This is fail-closed admission, not a silicon exception specification.
Ordinary long MOD=13 and the destination-side preincrement forms are not
removed. Full core conformance retains all 292 required markers, including
legal postincrement, absolute and long-offset MVDP controls. Native clocked
boot, all four keypad-tone settings, 5110 save/load, RF/processing and the
3210 baseline/frontier regressions pass without rebanking their oracles.
This closes the short-MOD=3 source-guard question for these named copy
families, not the whole ISA or their positive timing/composition domains. The
[pinned TI assembler control](rom4_dsp_loader.md#standalone-vendor-assembler-operand-control)
now warns on all four short-preincrement RMW forms just as on the forbidden
LD source, while preserving ordinary stores and long-offset reads. This
supports the atomic admission guard for ANDM/ORM/XORM/ADDM before operand
effects. The unchanged-core control fails on a data transaction; the guarded
`xorm` profile passes its 1,024 legal cases/restores and a separate 1,024-case
rejection matrix over all ARs, CMPT, initial TC, pages, frontends and two
save points. Rejection preserves pointers/status and performs no data,
program or I/O write. Accepted evidence: `tmp/run_c54x_core_g0gyh6d9`;
the unchanged-core failure is `tmp/run_c54x_core_wpl42iu_`.
The debugger likewise declines these short-preincrement forms without
fetching their immediate; ordinary stores and long-offset forms remain
separate positive controls. This is fail-closed admission, not a silicon
trap specification or an all-ISA closure.

Post-guard legal controls independently pass `long-mvkd` (427,784 shared
cases/restores, including 153,600 RMW), `preinc-store` (6,144), `preinc-st`
(1,536) and `xorm-absolute` (4,096). Their accepted runs are respectively
`tmp/run_c54x_core_am84mku0`, `tmp/run_c54x_core_51wgjuyl`,
`tmp/run_c54x_core_e65578ms` and `tmp/run_c54x_core_9yfpz7bw`.
Native ROM4 external-clock boot and wire/pin restoration also pass in
`tmp/native-pcm-clock-_gqp6iqq`. Original MU4 `bootrecmeta` passes its own
resident/overlay checks and recording/metadata transaction in
`tmp/native-gates/c54x-rmw-mod3-mu4`; its complete endpoint NAND is byte-equal
to the accepted recorder-persistence endpoint (SHA256
`75a523a9c1b6a0f6d98d47812b3082717e0764a79e0956b47b2de4fa7160d6c3`).
This is a native regression result, not repaired recording-tail policy,
whole-file playback, handset boot or native speech acceptance.

MACP/MACD source admission is separately corrected. SPRU172C pages 4-87
and 4-89 define Smem as the value read into the multiply and T; MACD's
implicit copy writes its successor location, not the source operand. The
atomic handler formerly applied short preincrement before reading that
source. An unchanged-core run of `pmac-mod3-edge` fails on a data transaction;
the guard now rejects `98..9f` before address modification or operand access.
This is fail-closed emulation of an unsupported operand form, not a claimed
silicon exception. It does not introduce positive staged MACP/MACD support.

Run `tools/run_c54x_core.py --mame mame/mame --rompath mame/roms
--profile pmac-mod3-edge --output-root tmp`. Its 512 cases and restores cover
four opcode classes across eight ARs, CMPT, page, frontend and save-point
choices. Existing postdecrement/postincrement, absolute and long-offset
MACP/MACD examples remain in the default core regression; long MOD=13 is not
removed. The remaining source/RMW audit is still not an all-ISA closure.
The cold finalized MU4 prefix preserves all seven prior digital captures
byte-for-byte (decoded returns, DMA, DXR writes, TX words/times and DIN
words/slots); its independent ideal-DAC checker also passes. This excludes
an observed prefix regression, not every media stream or completed playback.

The repeated immediate-ST fixture distinguishes first-body postincrement from
later retained iterations. Its qualified postincrement address update is in
Access, two clocks before the first literal write; later repeat updates occur
one clock before their writes. Other addressing forms retain their separately
qualified schedule. The earlier first-update assertion incorrectly applied the
later-iteration timing to postincrement; correcting it changes no store
execution. This is the existing emulator phase contract, not a new physical
timing claim. Conformance failures now print the case, tick, replay, registers
and transaction counters, and the runner rejects any such assertion diagnostic
even if completion markers are also present.
The current full default core gate reproduces all 293 required
markers exactly once, including the legal MACP/MACD examples. The separate
program-multiply rejection and repeated-SUBC/ST profiles and the complete tool
suite also pass. These are executable emulator regressions, not all-ISA or
physical pipeline conformance.

### Standalone staged MVDP

The staged frontend now implements legal standalone data-to-program copies,
separately from the opposite-direction MVPD path. Decode retains the explicit
program destination and addressing extension in their architectural order:
absolute source before `pmad`, but long-offset source after `pmad`. Access
resolves the source once; Read captures its value and mapped program target;
Execute writes the retained word. Destination fields travel with their
instruction, rather than sharing an Access-written global latch. The two
extra hold clocks reproduce the four/five-clock standalone instruction
totals; their exact silicon bus placement remains partial.

`tms54xtra -bios mvdp` covers 336 cases and restores: seven source forms
(direct, SP-relative, postincrement, absolute, offset, preincrement-offset,
circular), eight AR selections, two instruction pages and three snapshots.
Changing the source and AR after their sampling points must not alter or
duplicate the transfer. `mvdp-pair` adds 12 adjacent-transfer cases/restores
across both pages and six checkpoints, requiring two ordered source reads
and two distinct program writes. The unchanged staged core fails the first
legal standalone case before a transfer, so this is an executable admission
regression, not a source-string invariant.

These fixtures do not establish every legal operand, external wait states,
mapped CPU/peripheral sources, self-modifying-code prefetch coherency, or
control-delay/XC/block-repeat composition. The latter compositions
remain explicitly unsupported; no atomic fallback is used. Native handset
speech and the unresolved serial-status input are not promoted by these
instruction fixtures.

### Repeated staged MVDP

Single-instruction repeat uses the same separate data-to-program direction:
the first transfer captures its source in the ordinary staged slot, then
the retained repeat stream reads a fresh data word for each later iteration.
The source's indirect modification occurs once per transfer. The explicit
`pmad` increments with 16-bit wrap in the instruction's logical program page;
OVLY translation applies to each resulting program address. The initial
instruction retains its standalone hold clocks and later iterations retire
one transfer per stream clock. No instruction-extension refetch or atomic
fallback is introduced.

`tms54xtra -bios rpt-mvdp` checks 9,600 cases/restores: five source forms
(DP, SP-relative, unmodified AR, postincrement, postdecrement), all eight
AR selections, counts 0/1/2/7/255, short and long immediate counts, two
instruction pages, ordinary and wrapping program destinations, both OVLY
states, and three snapshots. Read/write taps require exactly count+1
ordered transfers and one-clock accelerated retirement. The preceding core
fails the fixture at the repeated opcode's admission boundary.

This does not establish physical bus phase, memory-sourced counts, all
circular modes, count values outside the tested
domain, or self-modifying-code coherence. Absolute and long-offset Smem
remain forbidden in single-instruction repeat by the independent addressing
guard; standalone support must not weaken that guard. Repeat inside a block
or control-delay/XC composition remains outside this implementation.

`rpt-mvdp-irq` separately validates 288 cases/restores with IRQ and NMI,
counts 0/1/7/255, both immediate-count forms, two pages, three request times
and three snapshots. SPRU172C section 2.6 specifies that repeat Decode
disables all interrupts, including NMI but excluding reset, until completion.
The fixture requires every program write before acknowledgment, the exact
unexecuted continuation in RTN, a real PSHM/RETE ISR, a balanced stack, and
no duplicate transfers after restoration. The existing core satisfies this
contract without another timing adjustment or CPU change. Standalone MVDP
interrupt interaction and reset/HOLD during repeat remain separate questions.

Input pulses enter through MAME's scheduled `set_input_line` path. Raw
`write_stream` checkpoints must occur after the input synchronization has
reached the CPU, not inside the callback that merely enqueues the edge.
The fixture snapshots avoid those enqueue ticks; an undelivered scheduler
event is not evidence that the CPU failed to save a pending interrupt.

### MMR long-form boundary

SPRU131G section 5.6 separately excludes `*ARx(lk)`, `*+ARx(lk)`,
`*+ARx(lk)%` and `*(lk)` from MMR addressing. These are bytes `e0..f8`;
`f9..ff` are reserved. The shared guard rejects these bytes before operand
effects in the nine encoded LDM A/B, PSHM, MVDM, MVMD, STM, STLM A/B and
POPM classes. MVMM has its own compact operand grammar and is not covered
by this byte-oriented guard. Valid short MMR forms keep their page-zero and
AR-upper-bit-clearing semantics. Ordinary Smem long MOD=13 (`e8..ef` plus
offset) remains read-capable and must not be rejected by this MMR rule.

`mmr-long-edge` in the `tms54xtra` namespace covers 4,608 cases/restores:
nine classes, 25 forbidden plus seven reserved bytes, CMPT settings, two
pages, both frontends and two checkpoints. It requires no data/stack access,
AR update/truncation, accumulator/T, SP or status effect. The guard applies
to opcode words, not immediate extensions or annulled words. The old core
fails this fixture with an operand transaction. The corrected behavior is
unsupported-form rejection, not a hardware exception specification.

### Single-repeat transfer boundary

The coherent `bootrecmeta` metadata path includes `710f 0012` at `02:9cb5`:
an ordinary `MVDK TRN,AR2` before a younger repeat header, not a repeated
mapped move. The guard requires the latched repeat body's own address;
genuine mapped repeat bodies retain their exclusion. The firmware-free
`move-rpt-owner` profile qualifies both cases with cleared-pipeline replay.
This qualifies instruction admission, not physical pipeline timing.

SPRU131G sections 5.2.4 and 5.5.3.2 exclude absolute and long-offset
Smem operands from single-instruction repeats. This restricts the repeated
body, not an absolute-memory count operand of the repeat instruction itself.
It also does not prohibit a standalone extended transfer or a transfer in a
block repeat.

The six MVKD/MVDK, MVPD/MVDP and READA/WRITA classes reject operand bytes
`e0..f8` under an active single repeat before operand effects. The
`rpt-xfer-edge` profile checks 9,600 cases/restores: six classes, all 25
offset/absolute bytes, short and long immediate RPT counts 0 and 2, CMPT,
two program pages, both frontends and two checkpoints. It requires no data
or stack access, AR update/truncation, or accumulator/T/SP/status effect.
The old core fails with an operand transaction. This is fail-closed emulator
admission, not a silicon-trap claim. Other repeated operand families and
RPTZ setup/operand combinations remain outside this fixture's closure.

The staged C54x `bootrecstage` frontier is distinct from the atomic MU4
profiles below. It loads all 48,261 resident program and 3,656 compared data
words exactly, emits the original pin-decoded acknowledgement
`007f,0001,0055`, and enters the recorder routine. It also compares the
recorder overlay's 73,762 RAM destination words before execution (four
register words excluded) and enters original recorder startup at `0073f8`.
The paired copy witness records repeated MVDD (`e598`) at `03:8eda`
completing 128 ordered source reads from `0000..007f` into RAM beginning
at `0e35`. The current legacy observer does not renew that instruction-owned
read claim. Firmware then
publishes buffer pointers `1800` and `1880`. The zero source is shared with
the atomic control, not a demonstrated staged-only pointer defect; see
[Recorder low-page copy](#recorder-low-page-copy).

The stack-relative `7102 001a` at `03:e195` now transfers its captured word
to BRC before the later `RPTBD e1d2` at `03:e1aa`. Passive guest-access
taps observe source `5cee` with SP=`5cec`: 124 reads and matching BRC
publications across counts `6`, `0e`, `1e`, `3e`, `7e`. These observations
do not imply an adjacent producer/consumer hazard in the original sequence.
Execute-entry hooks separately observe 124 entries each at the move, RPTBD
and following instruction `03:e1d3`; this is not merely an advanced live PC.
The signed circular load `11df` at `03:dc39` and its following instruction
each have 49 observed Execute entries. Passive reads begin at `aa5a` and
advance by two with AR0=`2`, BK=`0`; no firmware state is injected.
In-block XC executes through `03:da71..03:da93`: Execute-entry hooks
observe `03:da85` (`XC 1,BLT`) and `03:da8f` (`XC 2,TC`) 1,168 times each,
and continuation `03:da94` twice. The alternate `03:da79` signed-XC route
has zero Execute entries in this run; its static presence is not runtime proof.
The delayed far return at `03:d95c` now reaches its original caller's
continuation `03:a1a9`; the former transfer into page `42` and eventual
`fe76` data execution are eliminated. Immediate-store forwarding also corrects
the subsequent interval count, so its DST zero-fill no longer wraps into the
register window. Software-saved repeat replacement/restoration now passes
the isolated interrupt fixture, and the original recorder advances past
`RPTB 03:faa5`. Accumulator SQUR (`f48d`) at `03:dbe6` also executes.
The internal BD (`f273`) at `03:da7e` and extended BANZ (`6ce0`) at
`02:a4cb` also execute. Absolute ADD `00f8 000b` at `02:9e9d` now
occupies the two-word delay span of FCALLD `03:dc44` at `02:9e9b`;
its source is mapped BL, not RAM. Memory SUB `08f8` at `03:d309` now
reads mapped BK (`0019`). Immediate store `76b2` at `03:df3e` now uses
`*AR2+0` addressing. FCALAD B (`f7e7`) at `03:b3a2` also executes.
Signed MAC `29f8` at `03:b305` now reads mapped BK (`0019`), and
two-word MAC `28f8 0015` at `03:cc06` reads mapped AR5 in the FCALLD
delay owned by `fb83 cb1e` at `03:cc04`. DLD `56e3 0010` at `02:a4d3`
also passes in both delay words of BD `f273 a4dc` at `02:a4d1`, inside
the `a4cb..a4dd` block repeat. Absolute immediate ST `76f8 0014 fdae`
at `03:c72c` now initializes AR4 through the early DAGEN-write path before
the following multiply consumes it. The five-second observation is free of
instruction exclusions. The subsequent ASM load at `03:a165` and its
adjacent `LD B,ASM,A` also execute after branch-delay admission.

The twenty-second empty-field control/save fixture executes without an instruction
exclusion and ends at `02:98ea`, ILLEGAL=`0`, IDLE=`1`. It processes the
ten-second control and fifteen-second poll/metadata traffic: input cursor
`50`, receive ring empty, seven responses and no pending acknowledgement.
The former NORM stop was downstream of incomplete merge-loop restoration;
the correction and its independent regression are documented below.

The empty-field metadata response has status=`00`, parameter=`07`,
reason=`03`, success=`0`. The operation finds `REL_001 .REL` and rejects
empty ID3 fields, not a missing file. Startup acceptance now uses a generic
instruction observer and still requires exactly one Execute entry at
`0073f8`, with captured opcode `fd00`. The retired program-read counter
reported two live-PC-qualified reads; a separate Execute breakpoint and the
new observer each report one actual entry. Ordinary reads, prefetch and
advanced live PCs are not instruction identity.

The separate `bootrecmeta` positive input supplies filename `TEST`, artist
`TEST` and title `MAME`. Updater `02:8385` and outer operation `03:8dc3`
return zero, and firmware sends parameter-7 status=`01`, reason=`0c`,
success=`1`. Absolute ADD `00f8 0018` at `03:a1cf` now reads mapped SP
under its independent arithmetic/producer contract. Post-save refresh
completes within the twenty-second window: PC=`02:98ea`, ILLEGAL=`0`,
IDLE=`1`, input cursor `58`, seven responses, no pending acknowledgement
and one reported track. Its internal startup, native metadata and dispatch
assertions pass without weakening the exact-one condition. The separate
staged cold-open fixture below rereads the saved catalogue and title from
NAND. File/audio completeness and playback remain open.

The active `bootplaystage` diagnostic sends an original playback request
through serial pins and loads the matching `RELSI16` image. Its 49,839
compared program/data words match original marker `aa99` before startup;
four low-window words remain excluded. Native execution passes mapped-SP
MVKD, the delayed far call inside block `02:6116..6120`, the shifted
store to AR5 at `02:5fdd` and `MVDK Smem,AL` at `02:6050`.
Pending MVKD writes now forward to same-address consumers, eliminating
the observed stack-index corruption and subsequent code overwrite without
changing the memory map. The two-word shifted store `6f93 0d9f` at
`02:41bc` now executes in the delay span of `RPTBD 41eb`; other delayed
control owners remain independently guarded. Memory-sourced `RPT Smem`
and repeated immediate/ASM-shift accumulator loads have independent
count, arithmetic, interrupt and replay coverage. Original playback now
passes the repeated `LD A,-1,A` at `02:5cf9` and the PMST store at
`02:9fae`, the long-offset PSHD at `02:a13d`, and long-offset
`POPD` (`8be1 0004`) at `02:a270`. At the 20-second observation, the
unaltered original playback run has PC `02:4e3c`, `illegal=0`, one actual
bootstrap Execute entry and 12 resident Execute entries. Its paired arithmetic
loop completes repeatedly. Correcting the DMA frame shadow removes the
out-of-buffer source walk and its misleading nonzero output. The current
boundary is why processing reaches its programmed reload tail with silent buffers and whether PCM
has the expected media provenance, not an
unsupported opcode. [Playback observations](5510_bringup.md#playback-execution-and-codec-observations)
also cover a fresh recording made with corrected DMA and MAC support:
it reaches `02:b5e1` without an illegal instruction but remains silent.
Thus the older recording is not the sole explanation for the zero output.
The two input artifacts and their observations remain distinct.
The separate varying-input recorder gate also passes, but playing its
file takes an earlier/different path: zero tracked arithmetic-loop entries,
twenty resident Execute entries and silent output, without an illegal
instruction. The next discriminating question is where this input's
decoder/read path diverges before the loop, not whether the constant
fixture alone should produce nonzero samples.
The playback observations
separate fetch hits, Execute entries and audio counts. The coverage diagnostic
still intentionally fails without audio acceptance. The generic 16-bit
repeat comparator retains the documented foreign-page hazard; delayed
calls do not acquire a hidden page-private loop controller.
This is a failed coverage diagnostic, not a playback acceptance or an
unavailable-ROM boundary.

The empty-field fixture observes 2,108 NAND program setups, 2,107 confirms and
1,098,240 data writes, but track count is zero and the recorded file is not
verified. Exact resident-memory comparison still passes (48,261 program
and 3,656 data words). These counts do not establish a complete recorder
transaction, audio output or handset boot. The positive fixture instead
observes 2,236 setups, 2,235 confirms and 1,165,824 data writes. Its captured
directory and ID3 text fields remain correct, and the reported track count
advances to one. Neither counters nor the successful protocol reply prove
file/audio completeness, playback or handset boot.

The passive observer records ILLEGAL explicitly: searching only for the
words "unsupported" or "illegal" misses some guard diagnostics. NAND
writes alone do not prove complete recording, and five seconds without
an instruction exclusion does not validate audio or handset boot.

Validated instruction and composition boundaries are collected below;
aggregate coverage is in [Core regression checkpoint](#core-regression-checkpoint).
Original-firmware probes are observation-only. Execute-entry evidence is
distinct from merely moving a Decode stop, and neither establishes complete
recording, validated audio or handset boot.

## Retained recorder execution contracts

Fixture counts below describe focused acceptance. Shared broad-run scope,
tooling totals and native handset restoration are centralized in the
[core regression checkpoint](#core-regression-checkpoint).

### Direct interrupt return to a restored block repeat

The former NORM stop at `03:dad2` sampled T=`4421` from
`LD *AR5-,T` at `03:dacf`, reading `c423`. That word was an earlier
`DST A,*AR5+` output, later reused by an incomplete preparation pass.
The merge repeat `03:da71..03:da93` decrements AR5 by two per iteration;
its setup AR5=`c35b`, BRC=`0247` should leave AR5=`becb`. Instead one
exit left AR5=`c0f1`, BRC=`0113`: exactly 275 iterations and 550 words
short. The following full-length pass consequently consumed stale output.
This was not EXP latency: the wrong T came from incomplete preparation, not
the independent negative-count alias used by ROM4 Pow2. The producer uses CPL/SP-relative parameters, not literal AR3/AR4
reads. Seven seconds was too short to observe the stop; ten-second
debugger, observer-free and source-observer controls agreed on the failing
sample. Do not attribute that window difference to observer perturbation.

The recorder's first premature merge-loop exit follows common-window
`FRETE` at `1f18`, returning into `03:da85`. Firmware has restored BRAF,
RSA=`da71`, REA=`da93` and nonzero BRC, but the staged fetch controller
previously retained the ISR's completed loop. It therefore ran the rest of
one iteration and fell through. Ordinary near returns already reconstructed
that prediction; direct far returns did not. A debugger history rendered
these common-window instructions as zero words under logical page 3;
disassembling their actual page-zero backing identifies the restore and
`FRETE` instructions. Do not infer execution of zero-filled code from that
history rendering.

`repeat-body-isr` reproduces the defect independently: the pre-fix first
case performs one of two copies and exits with BRC=`1`, BRAF set. The
return paths now rebuild fetch prediction from firmware-restored
RSA/REA/BRC/ST1 before fetching the continuation: Execute for an ordinary
far return, Read for a delayed far return. No hidden loop-context stack,
firmware-address hook or shift-domain relaxation is introduced.

The fixture passes 768 cases and save/replays: outer counts 1/2/7/56,
ISR counts 0/1/3/63, IRQ/NMI, two pages, FRETE/FRETED and six snapshots.
It checks ordered source/destination words, exact saved register values,
seven balanced stack accesses in each direction, retained stack samples
after backing-memory mutation, and one resumed continuation. The ISR's
software saves/restores own the context; the test has no intervening near
return to conceal failed far-return reactivation. Physical silicon stage
placement remains unvalidated.

### ASM loads in branch delay words

`--profile asm-delay` passes 30,720 cases and save/replays: BD,
taken/untaken BCD and BD with an older source store; DP, SP, postincrement,
offset and absolute sources; all 32 ASM codes, positive/negative B,
SXM/OVM, two pages and three checkpoints. A one-word ASM load shares the
delay span with `LD B,ASM,A`; a two-word load fills the span and the
selected target performs that consumer before reporting the result.

The unchanged ASM execution path samples one source word in Read and
publishes only ST1's low five bits in Execute. The fixture mutates the
pointer after Access, the source after Read and the extension after Fetch;
it checks the following consumer's signed/logical shift, unchanged B/T,
unrelated status, pointer ownership and one selected target write. No CPU
state is added. Calls, interrupts, repeat composition and physical phase
placement remain outside this fixture's acceptance.

The original `3200` at `03:a165` follows BD at `03:a163` and precedes
`f682` at `03:a166`. Its stopped context has ST1=`6116` (CPL set) and
SP=`63d6`, so `$00` selects stack RAM, not peripheral zero or DP=`0f80`.
The preceding `STL A,$00` supplies that frame word. A literal opcode-zero
interpretation without the captured addressing status would misidentify
the missing contract.

### Constant DAGEN initialization

`--profile st-dagen` passes 3,840 cases plus 80 excluded repeat cases,
all with save/replay. Direct-DP and absolute ST target AR0..AR7, SP or BK;
four values, no older store/an older DAGEN store/an older RAM store,
two extra consumer gaps, two pages and four checkpoints are covered.
Indirect LD, PSHM and circular LD consume the initialized register.

SPRU131G table 7-4 classifies constant ST as an early DAGEN writer, with
the shared-bank deferral described in section 7.5.2. The retained operation
publishes once at its Access clock (5 for direct, 6 for absolute without a
prefix), representing the original opcode's Read with owned extension words.
The direct form behind an older DAGEN store defers one clock; the older RAM
store does not cause that deferral. The existing saved deferral flag owns it,
and Execute does not duplicate the write. These are frontend placements,
not measured silicon phases.

Tests poison both extensions after Fetch and check exact publication clocks,
consumer reads, pointer/stack changes and unrelated register/status preservation.
BK consumers have at least one gap word, increased to two for the bank-deferred
case so consumer Decode follows publication. The closer deferred-BK case
samples the old value in this frontend; its silicon behavior is unvalidated,
not an admitted new latency rule. The BK=1 fixture starts at `5002`, not
`5003`, so its starting index is inside the one-word circular buffer.
Repeated mapped stores reject before any
register publication. Indirect/compiler-relative aliases, CPL=1 stack behavior,
interrupts and control-delay compositions need separate acceptance.

### Signed MAC/MAS with a BK operand

`--profile mac-bk` passes 98,304 cases and save/replays: MAC, MACR, MAS
and MASR, both destinations, direct/absolute BK addressing, 64 T/BK pairs,
16 FRCT/OVM/SMUL/SXM combinations, two pages and three checkpoints.
The existing multiply-accumulate pipeline samples BK once in Read, preserves
T, and publishes the signed accumulation/subtraction with optional rounding,
saturation and sticky overflow. Poisoning BK and T after Read proves that
the operation retains its operands and does not overwrite their later values.
The non-destination accumulator, status and absence of data writes are checked.

The shared fixture's original `mac-smem` population also passes 73,728
cases and save/replays, now with an explicit unchanged-T assertion at Read.
Admission is limited to the signed accumulating `28xx..2fxx` family;
other BK operand families, additional special registers, control-delay
composition and physical silicon phase placement are not established here.
The original recorder executes the BK MAC and reaches the separate
control-delay exclusion in [Current boundary](5510_bringup.md#current-boundary).

### Signed MAC/MAS in control delay words

`--profile mac-delay` passes 147,456 cases and save/replays: all four
signed accumulating operations, BD, taken/untaken BCD, CALLD, cross-page
FCALLD and RETD; short, absolute RAM, absolute AR5 and offset sources;
both destinations, sixteen T/memory pairs, eight FRCT/OVM/SMUL combinations,
two pages and three checkpoints. Expected arithmetic independently rounds
the accumulation/subtraction, not the product. The conditional branch
decides from the original accumulator before the delay operation changes it.

The fixture mutates the address literal after Fetch, the pointer after
Access, and memory/T after Read. It checks one source read, retained T,
unchanged other accumulator, overflow/saturation and stack/page ownership;
the selected target copies the completed result. This reuses the existing
MAC execution path and complete delay-word accounting, with no new CPU state.
`mac-delay-edge` separately checks that a two-word MAC cannot start in the
last one-word slot. Interrupt arrival during these MAC delays, waited
memory and physical phase timing remain unvalidated.

### Double-word loads in control delay words

`--profile dld-delay` passes 147,456 cases and save/replays over BD,
taken/untaken BCD, CALLD, cross-page FCALLD and RETD. Offset, preincrement,
circular and absolute forms cover eight AR selectors, offsets -1/+2 with
even/odd effective addresses, both destinations, four distinct long-word
patterns, SXM/C16, both compatibility states, two pages and three checkpoints.
The first addressed word is the high half and its XOR-1 partner is the low
half; both are sampled once in Read. Sign extension follows SXM, and C16
does not change this load's final bit pattern (SPRU172C section 4-42).

The fixture poisons the extension after Fetch, all ARs after Access and both
source words after Read. It checks the full accumulator result and preserved
other accumulator/status/T, one pointer update, exact call/return frames and
the selected target's observation of the completed value. The existing DLD
execution path and saved pipeline operands are reused; no CPU state is added.
The separate `dld-delay-edge` fixture covers partial-slot rejection.
Interrupt arrival, waited memory, repeat composition and physical phase timing
are not established by these isolated fixtures.

### Software-saved repeat context

The retained diagnostic context has ST1=`e104` (BRAF set), BRC=`003f`,
RSA=`db51`, REA=`db56`, SP=`5cc2`. Its range is `03:db51..03:db56`;
it is not an observed inactive-BRAF case. The saved-context witness below
establishes software restoration, not an automatic hardware loop stack.

The outer `RPTBD` at `03:db4d` repeats `03:db51..03:db56`, with a delayed
conditional call at `03:db53` to `03:dada`. Its return address is `db57`,
one word beyond REA. The observed interrupt enters logical `03:206c`, whose
common-window vector saves XPC and branches to `03:9528`; RTN is `dadc`
and BRC is `0037`. Thus the interrupted instruction is in the called leaf,
not directly in the loop body.

The ISR saves BRC/RSA/REA at `03:954d..03:954f` and restores them at
`03:9568..03:956a`. The subsequent common-window gateway also saves the
repeat context before dispatching application callbacks: `1ef7..1efa` selects
MMRs `0019..001d` and repeats `PSHD *AR7+` five times. Passive Write taps
observe BRC=`0037` at `5cd6`, RSA=`db51` at `5cd5`, REA=`db56` at `5cd4`
and PMST=`202c` at `5cd3`; these words remain intact at the exclusion.
`1eff..1f06` provides the reverse `POPD` sequence, and ST1 is popped at
`1f16` before FRETE at `1f18`. This restores BRC before BRAF, as required
by [TI SPRU131G section 6.10.6](https://www.ti.com/lit/ug/spru131g/spru131g.pdf).
Section 6.8 establishes that there is only one physical set of repeat
registers; software must save outer contexts for nesting.

The callback reaches `03:fa5e` with the outer BRAF still set and later
publishes its own BRC=`003f` before `RPTB 03:faa5`. An outside-body RPTB
now replaces the one loop controller; a near return rebuilds its fetch
prediction from restored architectural BRAF/BRC/RSA/REA, even after an
inner loop has finished. There is no automatic context stack.

`--profile repeat-isr` passes 3,840 cases and save/replays: five outer counts
(`0,1,2,7,56`), four inner counts (`0,1,3,63`), IRQ/NMI, two pages, two
stack alignments, FRETE/FRETED and six checkpoints. Physical input interrupts
an outside-body CCD callee. The ISR saves repeat/status registers, executes
its own loop and restores the saved words; FRETE returns to the leaf, whose
near return targets either the body or REA+1. Ordered copies, balanced
frames, retained stack operands and final register state are checked. The
original firmware passes this setup; this is not a complete recording
or a claim that every original outer-loop iteration has settled.

Companion isolated controls are `repeat-context`, `repeat-pmst`,
`repeat-popd`, `repeat-rearm`, `rptb-ccd`, `rptbd-irq` and `rptb-fcall-edge`.
Their renewal is not implied by a default run; aggregate scope is maintained
in [Core regression checkpoint](#core-regression-checkpoint).

### Accumulator SQUR

`--profile squr` passes 32,768 cases and save/replays: two destinations,
16 A operands, 16 untouched-B patterns, eight FRCT/OVM/SXM combinations,
two pages and four checkpoints. The independent expectation squares signed
A(32-16), including `10000` (-65536) and `0ffff` (+65535) high-part cases
that a signed 16-bit AH implementation gets wrong. Tests check Execute
operand publication, saturation, sticky destination overflow, unchanged T
and ST1, preserved non-destination accumulator and no data-memory access.
The implementation reuses the existing multiply-result contract. Other
producer compositions and physical pipeline phases are not established by
this fixture. Original recorder execution now passes SQUR and reaches the
internal BD at `03:da7e`.
The shared MAX/MIN fixture, memory-MAC and saved-repeat profiles pass as
controls.

### Unconditional delayed branch in a block

`f273` is two-word BD, not RETD. The original instruction at `03:da7e`
targets `03:da8b`, with delay instructions at `03:da80..03:da81`; all
belong to the repeated range `03:da71..03:da93`. Admission now includes
unconditional BD when its target is inside that block and both delay words
end strictly before REA. The existing Access-owned target redirect already
rebuilds the fetch count from Decode-owned BRC; no separate redirect or
saved state was added.

`--profile rptb-bd` passes 512 cases and save/replays across counts
`0,1,2,7`, four source patterns, both accumulator delay consumers, one
two-word or two single-word delay instructions, two pages and four snapshots.
It checks ordered source reads, selected writes, exact iterations and delay
effects. `rptb-br-edges` retains 32 exclusions, with its delayed-control case
now testing a BD delay span at REA rather than an admitted internal BD.
The original recorder passes this branch and stops at extended BANZ
`02:a4cb`. Interrupt composition and measured physical phases remain outside
this fixture's claim.
The conditional, nondelayed and backward block-branch profiles, exclusion
profile, saved-repeat and SQUR profiles pass as controls.

### Long BANZ in a block

The original `6ce0 fffc a4d5` at `02:a4cb` tests the resolved `AR0-4`
address without a data-memory read and selects a buffer path inside the
repeated range `02:a4cb..02:a4dd`. Nondelayed BANZ now has the same
bounded admission as B/BC: the complete instruction must end before REA
and its target must remain inside the block. A taken BANZ also resets the
speculative fetch count from Decode-owned BRC after discarding fall-through
words; AR modification remains owned by the existing address logic.

`--profile rptb-banz` passes 768 cases and save/replays: three long forms
(offset, preincrement, circular), eight initial/offset pairs, outer counts
`0,1,2,7`, two pages and four checkpoints. Selected-arm writes and sequential
source reads occur once per iteration, AR0 updates occur once per branch,
and taken/untaken transitions follow the computed address rather than the
loaded data word. CMPT, interrupts and physical phases remain unvalidated
for this composition. The standalone long BANZD, internal BD/BCD, branch
exclusion, forward/backward branch and saved-repeat profiles pass as controls.
Original execution advances to the separately mapped FCALLD/absolute-ADD
delay sequence above; this is not full recorder acceptance.

### Absolute ADD in a control delay

Absolute ADD `00f8/01f8` uses two words and may occupy a complete
two-word control-delay span. The original `00f8 000b` at `02:9e9d`
adds BL to A before the far-call target `03:dc44` observes A. The existing
Access address capture, Read operand and Execute arithmetic implement the
operation; admission now counts both words and retains the delay-span bound.
Control/peripheral sources outside the admitted register set remain excluded.

`--profile add-delay` passes 18,432 cases and save/replays across BD,
CALLD, FCALLD and RETD; RAM, AR3, AL, AH, BL and BH sources; both
destinations; four operand/initial patterns; SXM/OVM; two pages and three
snapshots. Aliased accumulator fields are included in the independent
arithmetic expectation. Carry, sticky overflow, saturation, preserved T/ST1,
one source read, one address-literal fetch, owned call/return frames and the
target's observed result are checked. RAM is replaced after Read to detect
rereads; mapped accumulator inputs are not overwritten after Read, since
that could also change the Execute-owned destination. Conditional controls,
adjacent producer ordering and measured physical phases are not established
by this fixture. Original execution passes the sequence and reaches the
separate mapped-BK SUB exclusion at `03:d309`.
Absolute ADD/BK controls, both shared delay-load profiles and their exclusion
profiles, and internal BD/BANZ pass as controls.

### BK as an ordinary arithmetic operand

BK (`0019`) is admitted as a side-effect-free 16-bit source for ordinary
ADD/SUB, matching the existing absolute-ADD BK contract. This does not
admit SP, repeat-control or peripheral registers, or other arithmetic families
whose mapped inputs remain excluded. Existing Read capture and arithmetic
helpers own the operand and result; no synthetic register value is supplied.

`--profile arith-bk` passes 1,024 cases and save/replays across ADD/SUB,
both destinations, direct/absolute forms, eight input patterns (including
zero BK), SXM/OVM, two pages and two checkpoints. Changing BK to `2222`
after Read does not alter the result. Source-read count, carry, sticky
overflow, saturation, untouched accumulator and absence of data writes are
checked. `--profile arith-ar` separately reproduces the existing 1,024-case
auxiliary-register control through the same fixture. Absolute-ADD/BK,
delay-ADD, BK move/store and internal BANZ controls also pass. The original
recorder passes its SUB and reaches the immediate-store exclusion above;
native recording and physical pipeline phases remain unproved.

### AR0-indexed immediate stores

Immediate ST admits short `*ARn-0` and `*ARn+0` modes, reusing Read-owned
address capture and AR modification. Its two-word Decode retains the literal
for Execute; accumulator saturation and later accumulator changes do not
select its data. An indexed destination below `0060` is rejected before
pointer modification or a data write. This does not change the established
direct/absolute immediate-store register contracts. Reverse-carry and circular
short immediate-store forms remain outside this admission.

`--profile imm-index` passes 98,304 cases and save/replays, including 96,000
RAM stores and 2,304 low-page exclusions. The matrix covers eight AR selectors,
CMPT and AR0 aliasing, eight signed/wrapping indices, both directions, four
literals, four accumulator patterns, SST/SXM, two pages and three checkpoints.
Pointers change before and after Read, the literal is replaced after capture,
and accumulators change before Execute. Checks require one literal/opcode fetch,
no destination read, one correct write, preserved status except compatibility
ARP and no pointer update on exclusion. Producer compositions and physical
pipeline phases are not established. Original firmware passes this store and
reaches the separate FCALAD B exclusion above.
Indexed accumulator-store/load and MAR, immediate forwarding, store-delay,
mapped-BK and saved-repeat profiles pass as controls.

### Delayed far accumulator call

SPRU172C page 4-55 specifies one-word FCALAD A/B: save PC+3 then
caller XPC, transfer through source bits 22-0, and execute two following
words. Its Class 30B delayed cadence is four cycles, versus six for
nondelayed FCALA. Neither form may be repeated or placed inside RPTB.

The staged model samples the 23-bit target in Read, redirects Prefetch and
discards the already fetched continuation after the delay span. PC+3 publishes
in Read and caller XPC in Execute, without the nondelayed held clock. Two
single-word or one two-word delay instructions retire before IRQ/NMI can
enter. Existing target and per-slot frame fields supply save-state ownership;
no additional CPU saved state or automatic context stack is introduced.

`--profile fcala-delay` passes 648 cases and save/replays: both accumulators,
three caller and three target pages, both delay layouts, no interrupt/IRQ/NMI
and six checkpoints. After target sampling the source accumulator is changed;
delay instructions and callee observe the changed data, but control still
uses the captured target. Checks cover PC+3/XPC publication, a balanced
FRET, a separate interrupt frame, delay effects once, target execution once,
and preserved flags/non-source accumulator. The nominal model has target
Fetch at clock 6 and callee Execute at 10, with frame writes at 5 and 6;
these are declared placements, not measured silicon bus phases. Adjacent
instruction-producer composition and waited memory remain separate contracts.
Original execution passes FCALAD and reaches the mapped-BK multiply exclusion.
Its captured sequence targets `03:b2ae`, with NOP and `LDM AR1,A` in the
caller's delay words. The callee starts with `PSHM AR1` and `FRAME #-7`.
Observed caller SP `6562` therefore becomes `6558` after the two call words,
one saved register and seven local words, matching the new stop. This is
coherent entry evidence, not completion of that callee or the recorder.
SP-modifying delay words are not covered by the isolated fixture.

`tools/mu4_repeat_context_observe.lua` reproduces the read-only witness on
`mu4nand -bios bootrecstage`: add `-debug -debugger none`, the absolute
`-autoboot_script` path, `-autoboot_delay 0`, `-log` and `-nonvram_save` to
the retained-NAND command below. The tool exits after five emulated seconds.
Require the positive outer/vector/copy-entry records and saved-word records;
MMR event PCs describe live context, not necessarily the owning pipeline slot.
This is a manual frontier instrument, not a passing nested-repeat gate. Retire
it when an isolated composition fixture and original resumption proof replace it.

### Immediate-store forwarding

[TI SPRU131G section 7.3.2](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
specifies an internal-bus bypass when an on-chip read conflicts with a pending
write to the same location. The retained RAM-read path now includes immediate
stores, taking the literal already captured by Decode. Forwarding and the real
Execute write share the store-word helper. The real Write is not moved and
register/peripheral addresses remain outside this bypass.

`imm-forward` passes 6,912 cases/restores: direct, postincrement and absolute
stores; LD A, ADD A and LD B consumers; four literals, SXM/OVM combinations,
DP/CPL, two pages, adjacent/NOP-separated and same/different addresses.
Snapshots bracket Read and Write; modifying the already fetched literal
cannot change either the forwarded value or real write. The test checks one
pointer update and write, preserved ST1, consumer results/flags and real reads
only for separated or different-address controls. The expanded fixture fails
on the preceding core; the separate 768-case RMW-forwarding control passes.
IRQ/repeat composition, mapped/peripheral forwarding and measured bus timing
are not established by this matrix.

In original recorder code, `7602 ffff` at `03:d961` stores `ffff` to stack
word `6576`, immediately followed by `ADD *SP(2),A` at `03:d963`. Previously
the ADD read stale zero before the real `ffff` write, leaving A=`31` instead
of `30`. That count caused an extra interval: its delta was `-0400`, then
`fbff` after subtracting one, producing a huge BRC count. The later
`DST B,*AR4+` at `03:d9af` eventually wrapped to the register window;
it was not evidence for relaxing the long-word register-pair guard.
With forwarding, two observed invocations publish A=`30` and finish their
zero-fill loops, reaching `03:d9b2`. Passive Write taps observe contiguous
spans `9e82..a681` and `ae2e..b62d`, each 2,048 zero words under this fixture.
The register-window exclusion remains unchanged. This advances execution,
not recorder-output acceptance.

### Delayed far returns after stack restoration

FRETD/FRETED must consume the stack after an older POPM/POPD's Read-owned
SP update, just as near returns do. The retained delayed return's first
sample remains Access-owned, but is ordered after the older stack operation;
the second sample remains in Read. No new pipeline state or atomic fallback
is introduced. Exact DA150 bus phases remain unmeasured.

The default native fixture covers 72 cases/restores: FRET/FRETE and their
delayed forms, two pages, two single-word or one two-word delay instruction,
and 36 preceding-POPM compositions. It checks SP, restored AR1, INTM, ordered
XPC/PC reads, target Fetch, delay effects and replay around both stack samples.
The expanded fixture fails on the preceding CPU implementation.
The native 5110 save-state gate, original resident-memory comparison and
154 runner unit tests also pass; none establishes complete recorder output.

In original recorder firmware, the `03:d8bb..03:d95e` function saves three
registers and allocates 20 local words, then restores them before FRETD at
`03:d95c`. Its caller is `FCALL 03:d8bb` (`f983 d8bb`) at `03:a1a7`,
whose continuation is `03:a1a9`. Passive stack taps observe that `0003,a1a9` frame
at `657a..657b`. Consuming the preceding saved-register slot as XPC instead
redirected execution to `42:a1a9`; subsequent execution through unmapped
pages eventually stopped on `fe76` at `02:6609`. That word is not evidence
that RCD is the next required instruction: the acquired aa88 record marks
`6604..6613` as data (original word `6609=beef`), outside the aa22/aa88
program spans. Debugger history records logical Execute PCs, but disassembles
current raw program memory; apply the established OVLY projection separately
for common-window instruction bytes.

### Circular Fixture Domains

[SPRU131G section 5.5.3.4](https://www.ti.com/lit/ug/spru131g/spru131g.pdf),
printed page 5-16, requires a base aligned to the smallest `2^N` strictly
larger than BK, an initial index inside the queue, and a step magnitude no
greater than BK. The old paired generator used BK=16 with offset `f495`
(-2923), and with AR0 self-index steps around 4103. Those inputs were
modulo-model stress cases, not documented nonzero-BK hardware-domain cases.

The paired generator now carries its initial pointer and BK explicitly.
Long circular forms use initial `4007`, BK=8192 and aligned base `4000`;
the same opcode-shaped negative offset exercises a legal wrap to `549c`.
Short circular AR0 self-index forms use BK=8192 so the pointer-derived step
fits the buffer; other short circular forms retain BK=16. An independent
modulo oracle first checks the documented domain, stopping on invalid
fixture inputs rather than asserting a silicon trap. Fourteen host-side
guard controls cover invalid size/step/index combinations, signed wrap and
power-of-two mask boundaries; they are not executed DSP instructions.
BK=0 behavior and physical memory-bank/timing details are outside this pass.

The existing `indirect-dld` profile now requires the guard, 3,072 extended
DST cases, 9,216 short-indirect DST cases, 1,152 store/DLD composition cases
and 23,040 DLD cases. Case IDs, instruction-shaped literals, save/restore
and bus-access assertions are retained; no invalid case is replaced by a
duplicate legal tuple. The full paired run passes, as does the neighboring
9,728-case short-MOD3 load exclusion bank. CPU emulation code is unchanged.

```sh
python3 tools/run_c54x_core.py --mame mame/mame --rompath mame/roms \
  --output-root tmp/c54x-paired-circular --profile indirect-dld
```

The shared arithmetic/load generator carries BK through its callers rather
than configuring BK=16 independently of their expected addresses. Short
indexed circular forms start all ARs at `5007`, so AR0 supplies a signed
step of that magnitude even when another AR is selected. Those forms and
long circular forms use BK=32768, aligned base zero and initial index
`5007`: both the AR0-derived step and every signed 16-bit literal fit the
buffer. Unit-step circular forms retain BK=16. The guarded modulo oracle
computes the indexed pointer update and long circular address only after
checking this domain. Raw case IDs, signed literals, case counts and
read/restore assertions are retained; no CPU behavior changes.

This contract applies to the shared ADD/SUB, unsigned and shift-16 arithmetic,
CMPM/BITF, offset/unsigned/shifted/variable/TS loads, memory multiply/T-load
and TS arithmetic fixtures. Their initializers use the fixture's BK;
handwritten register-only aggregates retain their default BK=16. It does
not establish physical memory-bank placement or instruction timing.

Executable acceptance passes with `run_c54x_core.py --profile` for
`memory-alu`, `memory-shift16`, `adds`, `subs`, `cmpm-smem`, `bitf-smem`,
`load-offset`, `ldu-shift16`, `load-variable`, `ld-ts`, `ld-ts-producer`,
`mul-t-sources`, `t-load-control`, `ts-alu`, `ts-alu-producer` and
`shifted-alu`. These retain the existing complete case counts, bus assertions
and save/restore checks; the Python suite passes 3,107 tests with one skip.

Long BANZ/BANZD and repeat-block BANZ also carry BK through their shared
fixture. Circular forms use BK=32768 with initial indices below that size;
linear forms retain their 16-bit wrap inputs. Circular zero/nonzero branch
cases include positive wrap (`7fff + 1`), negative wrap (`0234 - 2923`)
and a full-buffer negative step (`0 - 32768`). The repeated-case oracle
checks the domain on every iteration, not just the initial pointer. No
operand memory access is introduced. Signed offsets, raw case IDs, delayed
instruction layouts and replay checkpoints are retained.

The existing `banzd-long` profile covers ordinary BANZ before BANZD, without
adding a BIOS slot; its runner requires both 2,304-case and 4,608-case
markers. `rptb-banz` separately covers 768 cases at repeat counts 0/1/2/7.
Both profiles pass, as do the neighboring `rptb-bcd` and `rptb-bd` controls.
The runner rejects missing or duplicate ordinary/delayed completion markers.

Immediate-long and accumulator-long store fixtures carry their selected
initial pointer and BK too. Circular forms use `4007`, BK=8192 and base
`4000`, preserving the signed `f495` literal as a legal negative wrap to
`549c`. Noncircular forms retain `1007` and BK=16. Shifted stores, long
MVDK/MVKD and long RMW inherit the same address contract; their initializers
set only the selected pointer to the circular start, preserving unrelated
AR values and alias checks. MVDK's retained-BK assertion uses the fixture's
BK unless the instruction explicitly writes BK.

The compatibility-named `long-mvkd` BIOS now isolates all seven shared
consumers: immediate ST, ordinary and shifted STL/STH, MVDK, its original
instruction-pair control, RMW and MVKD. Its runner requires every marker,
covering 427,784 cases/restores without adding a BIOS slot or changing CPU
emulation. The peripheral-only fixtures retain their separate register and
latency contracts. `long-mvkd`, `st-long-periph`, `rmw-long-periph`,
`preinc-store` and `preinc-st` all pass, including the unrelated short-store
controls. The runner rejects missing or duplicate shared-consumer markers.

These corrections do not constitute an exhaustive ISA or fixture-domain
audit. Other older generators' modulo expectations remain insufficient
evidence of documented input legality.

### Signed circular memory loads

Signed LD admits the four short circular modifiers `c0..df`, using the
existing retained address-generation and load paths. BK is sampled in
Decode; the source address, AR0 index and pointer update belong to Access;
the word is sampled in Read and published to the selected accumulator in
Execute. Low mapped/peripheral sources remain excluded before reading or
updating the source pointer. No address-generation arithmetic was changed.

`ld-circular` passes 393,216 cases/restores: 368,640 RAM cases and 24,576
mapped-source exclusions; four modifiers, eight encoded registers, both
compatibility states and destinations, four source words, eight SXM/OVM/SST
states, two pages and three snapshots. BK covers 0/1/7/16, with 8192 used
for valid RAM-pointer self-indexing. Post-Decode BK changes, post-Access
pointer/index changes and post-Read RAM changes cannot retarget the load or
repeat its effects. Ordinary indexed-load coverage remains a separate gate.

Nonzero buffers obey [SPRU131G section 5.5.3.4](https://www.ti.com/lit/ug/spru131g/spru131g.pdf):
aligned base, initial index inside the queue, step magnitude at most BK.
The independent expectation uses modulo, while the CPU uses iterative
wrapping; values outside that documented domain are not a conformance oracle.
BK=0 retains the independently exercised linear-update contract described in
[ROM4 codec validation](djr_dsp_integration.md). Generic IRQ/NMI, delay/repeat
composition and physical bus timing are not established by this isolated
matrix; the recorder's 49 executed iterations are separately scoped evidence.

### Memory transfers into the block counter

SPRU131G table 7-24 gives MVDK/MVMD-to-BRC zero latency before RPTB[D].
The retained frontend reads the source and publishes BRC at Access, before
younger Decode initializes the repeat. The move does not inherit DAGEN-bank
write deferral. A preceding mapped accumulator write uses the existing
late-sample path rather than a stale mapped read. No new save fields or
atomic-interpreter fallback are introduced.

`move-brc` passes 960 cases/restores: DP, SP, postincrement, absolute,
long-offset, MVMD AR4, MVMD AL and MVDK AL sources; counts 0/1/2/7/127;
adjacent/NOP-separated RPTB/RPTBD; two pages and three snapshots. The
source is read once, BRC writes once without a destination read, and the
following loop copies exactly count-plus-one words. Mutating the source
after capture cannot change the loop count. The mapped-source cases do not
independently establish every preceding accumulator-producer composition.

`move-brc-edge` passes 24 cases/restores for MVDK/MVMD in a repeat body
or RPTBD delay. Rejection precedes source reads, BRC writes and source-pointer
updates, including restoration before/after Decode. The completing two-word
move retains its delay context even when its extension consumes the final
delay word. These are bounded model exclusions, not asserted silicon traps.
In-loop modification (table 7-25), general control-delay composition,
IRQ/NMI composition and physical bus timing remain unvalidated.

### Shifted memory loads in control delays

The original `6f00 0c5f` at `03:d01d` is a two-word memory load with
encoded shift -1 into A. It fills both delay words of `BCD d036,AEQ`
at `03:d01b`. The retained Decode span now admits short shifted-load
forms and validates the load extension before Access; a store/arithmetic
extension does not inherit admission merely by sharing the first word.
The existing source Read, encoded shift and Execute result are reused,
without an atomic-interpreter fallback or additional CPU save fields.

`shift-delay` passes 110,592 cases/restores for BD, taken/untaken BCD,
CALLD, FCALLD and RETD; DP/plain-indirect/postincrement sources; A/B;
all 32 shifts; four source words; four SXM/OVM states; two pages and three
snapshots. Replacing the fetched extension, source memory and address
registers after capture must not change the result or duplicate updates.
The selected continuation observes the completed load and the original
call/return frame, while a conditional owner retains its earlier decision.
`shift-delay-edge` passes 16 cases/restores for spill into the final delay
word, store/arithmetic extensions and a long-source form, with no data
access or pointer/accumulator effect. These are bounded conformance
exclusions, not silicon traps. Mapped sources, SP-specific call/return
addressing, three-word forms, IRQ/NMI composition and physical phase
timing are not established by these fixtures.

### ASM-controlled accumulator stores

`8793` at `03:e0c9` is `STH B,ASM,*AR3+`; `84b5` at `03:e0e7` is
`STL A,ASM,*AR5+0`. The frontend admits short DP/SP/basic-indirect and
AR0-indexed add/subtract forms, with RAM destinations only. The existing
Read-owned address update retains ASM before older Execute publications;
the accumulator and SXM/SST handling remain Execute-owned. Dedicated
ASM loads forward their publication to meet SPRU131G example 7-52's
zero-gap contract. Ordinary ST1 writes do not receive that forwarding
(table 7-22). This is a partial stage model, not measured bus timing.

`asm-store` and `asm-store-ld` each pass 122,880 cases/restores: four
STL/STH A/B selectors, all 32 shifts, eight accumulator patterns, four
SXM/SST states, DP/SP/postincrement/indexed-sub/indexed-add forms, two
pages and three snapshots. The second profile prefixes an immediate ASM
load without a gap. After Read, ASM/address/index mutations must not
change the write, while accumulator mutation must affect it. Independent
shift/saturation expectations check one Execute write and no data reads.
The indexed composition uses AR5 and AR0=3; the broader ordinary indexed
store matrix remains a separate address-generation regression. Final-tree
store-index/bypass/preincrement/delay and immediate-ASM/block-branch
controls pass separately. Long-offset, preincrement, reverse/circular ASM
store forms, mapped destinations, general ST1-producer latency and
interrupt/repeat/delay composition are not established by these matrices.

The Execute write and pending-write bypass share one store-word calculation,
including the retained ASM shift, live Execute accumulator and SXM/SST
handling. `asm-store-bp` passes 24,576 cases/restores over four selectors,
all 32 shifts, four SXM/SST states, both unsigned-load destinations,
adjacent/NOP-separated consumers, same/different addresses, two pages and
three snapshots. Changing ASM after Read cannot change either publication.
The adjacent alias suppresses the backing read while the real store still
writes once. This fixture failed against the earlier unshifted bypass even
though the actual RAM write was correct. Ordinary and Xmem bypass controls
also pass; explicit-shift Smem stores, bank contention and stack ordering
remain outside this ASM composition scope.

### Immediate ASM publication

`ed1e` is `LD #-2,ASM` (SPRU172C 4-70/4-71). All 32 encoded signed
five-bit values update only ST1.ASM in Execute, using the retained opcode
without a data access or atomic-interpreter fallback. SPRU131G example
7-52 requires zero latency to the following ASM consumer; the existing
Read-after-older-Execute ordering supplies that visibility.

`asm-immediate` passes 9,216 cases/restores for LD/ADD/SUB consumers,
all 32 shifts, two accumulator patterns, four SXM/OVM states, zero/one
intervening NOP, two pages and three snapshots. Replacing the opcode
after Fetch must not alter its immediate. Per-clock checks cover ASM,
other status, accumulator results and absence of data transactions.
The memory-sourced `asm-consumer`, `asm-edge` and `asm-mapped` profiles
also pass separately. This does not establish every immediate-ASM
store consumer, interrupt/delay composition or physical phase timing.

### Delayed internal block branches

The original `BCD c85e,AGEQ` at `03:c852` has delay words `5692` and
`578a` at `03:c854..c855`, entirely inside the block `03:c851..c862`.
Both delay words execute regardless of the captured predicate. Admission
is limited to signed accumulator conditions with an internal target and
the complete delay span strictly before the endpoint. A taken redirect
restarts speculative Fetch accounting from the Decode-owned BRC without
consuming an extra iteration; it does not reset the architectural count.

`rptb-bcd` passes 3,072 cases/restores: 12 signed A/B predicates, counts
0/1/2/7, four source patterns, two one-word delay operations or one two-word
operation, two pages and four snapshots. It checks ordered source reads,
one selected-arm write per iteration, both delay words, final pointers,
status and count. Delay arithmetic can change the tested accumulator
after its decision; source RAM changes after Read cannot change the
captured word. Existing nondelayed/negative/backward block-branch,
delayed-call and interrupt-loop profiles pass separately. This fixture
does not establish its own IRQ/NMI composition, endpoint-overlapping delay
spans, outside-block targets or physical phase timing.

Passive original Execute counts are 510 at the branch, 510 at its first
delay address, 8,702 at the second delay address and 510 at the join. The
extra second-address visits are not classified by this observer; these
totals must not be interpreted as a paired per-branch execution trace.

### Reverse-carry MAR

`6dbb` is `MAR *AR3+0B`. SPRU131G 5.5.3.5 defines AR0-indexed
reverse carry/borrow for MOD 7/4; FFT use specifies a power-of-two AR0.
The staged frontend admits both reverse forms through the existing
Access-owned auxiliary update, using the shared addressing helper without
adding a data-memory transaction or an Execute-time update.

`mar-reverse` passes 3,072 cases/restores across add/subtract, eight
auxiliary registers, both compatibility states, eight power-of-two indices,
two initial layouts, two program pages and three snapshots. Expected
addresses use an independent MSB-first bit-serial carry/borrow oracle,
not the core's reverse/add/reverse implementation. Mutation after Access
checks that AR0 and the selected register are not consumed again. AR0
self-aliasing, compatibility ARP publication, accumulators/T and remaining
status are checked; no data reads/writes or extension fetches occur.
Non-power-of-two index meaning, producer latency, interrupt composition
and physical phase timing are unvalidated. The existing 3,072-case
`mar-index` profile also passes on this source.

### Post-loop BRC write ownership

STM BRC uses the existing zero-latency Access publication before a new
RPTB (SPRU131G table 7-24). Its loop membership must come from that
Access instruction, not whichever older instruction occupies Execute.
The original `STM #003f,BRC` at `03:d69d` overlaps the older loop endpoint
`DST A,*AR1+` at `03:d69c`. A guard using only Execute's body tag wrongly
attributes the value `003f` to the DST and rejects an outside-loop reload.
Early Access/Read writes now pass their owner stage to the shared guard;
ordinary Execute writes retain their own stage. No in-loop latency or
dynamic BRC-update semantics are added by this correction.

`repeat-rearm` covers 64 cases/restores: consecutive blocks with first
counts 0/1/3/7, next counts 0/2, two pages and four snapshots. It checks
one BRC publication and independently counted iterations in both blocks.
The negative `rearm-edge` fixture passes eight cases/restores for an actual
Access-owned in-body STM BRC write, excluded before publication. These are
CPU conformance guards, not silicon traps. Physical phase timing and
interrupt composition are not established by this fixture.

### Signed dual-memory MAC

SPRU172C 4-82/4-83 identifies `b0a0` as
`MAC *AR4+,*AR2,A,A`: signed X/Y product plus the source accumulator,
with X also published to T in Read. This Class-7, one-word instruction
reuses paired address capture and retained RAM reads. The accumulator
remains Execute-owned, with FRCT, SMUL product handling, OVM and sticky
destination overflow handled by the existing arithmetic helpers.

`mac-dual` passes 215,040 cases/restores: 18,432 addressing cases and
196,608 arithmetic cases, 512 same-accumulator encodings, three layouts,
two BK modes, 64 input pairs, eight accumulator patterns, 32 flag states,
two pages and three snapshots. Operand/pointer/BK mutation after capture
detects rereads or repeated updates; accumulator mutation before Execute
checks feedback ownership. `mac-dual-edge` rejects 16 low mapped operand
cases before reads or pointer updates. Cross-accumulator MAC, rounded
MACR, MAS/MASR, physical bank timing and interrupt/repeat composition are
not admitted by this contract. Execution of the original MAC is not proof
of valid recording or audio.

### Active repeat context and stack bypass

The original wrapper uses `POPD *AR7` at logical `03:1f02`, with AR7
`001d`, SP `5cd3` and saved/current PMST `202c`; RSA/REA remain
`db51/db56`, outside the restore routine. OVLY maps this low-half code
to physical program `001f02`. A raw read at `031f02` returns zero and
must not be interpreted as missing program content.

`pmst-popd` passes 64 cases/restores for indirect POPD PMST: four values,
two source pages, lower/upper targets and four snapshots. It checks one
retained stack read, one publication and subsequent program mapping.
`repeat-pmst` adds 384 cases/restores using the same/replaced loop-context
matrix below, with an actual PSHM/POPD PMST pair outside the loop body,
ordered stack transactions and independently checked iteration counts.
The active PMST case restores unchanged PMST only. `repeat-popd` adds
384 cases/restores for indirect REA/RSA/BRC restoration outside the loop
body, using the same/replaced-context matrix and checking retained source
words, ordered publications and actual loop iterations. Its PSHM and
POPD sequences are separated; adjacent PSHM-to-POPD bypass is not claimed.
Active PMST mapping changes and in-body control writes remain excluded;
these tests do not establish interrupt composition or physical
bus-phase timing. Existing `pmst-pop`, `pmst-prefetch`, `repeat-context`
and `pop-repeat` profiles also pass on this source. The full default
checkpoint below remains revision-bound, not renewed by these profiles.

Separated POPM REA/RSA/BRC writes outside the active loop body are admitted;
in-body writes remain excluded. A near return into the restored bounds,
or immediately after their endpoint, rebinds the retained loop bounds and
iteration count from RSA/REA/BRC while preserving BRAF. `repeat-context`
passes 384 cases/restores: same and replaced contexts, four counts, two
layouts, two pages, two stack alignments and six snapshots. Ordered copies
and actual iteration counts are checked, not just register readback. Six
NOPs separate BRC restoration from return. Inactive-to-active BRAF changes,
general nested/interrupt contexts and other reentry routes are unvalidated.

Adjacent direct PSHM/POPM on a RAM stack requires same-clock write/read
bypass: the younger Read samples the older Execute publication, retaining
its already allocated stack address. SPRU131G section 7.3.2 documents the
on-chip pending-write bypass. `push-pop-same` passes 2,048 cases/restores
across eight sources, T/AR3/BK/SP destinations, four patterns, two pages,
two stack alignments and four snapshots. Source and stack mutation after
capture detect rereads. This does not establish PSHD/POPD composition,
peripheral stacks or physical bus-phase timing.

In the original context wrapper, a Decode-time stop sees SP before two
older pending pops have read their operands: REA/RSA/BRC are then at
SP+2..4, not SP..2. That observation must not be diagnosed as a corrupt
saved frame without accounting for the older pipeline slots.

### Delayed conditional calls

CCD admits AGEQ/ALT/ANEQ/AEQ/AGT/ALEQ and their B equivalents. SPRU172C
4-29 specifies the two-word delay span, a taken frame containing `PC+4`,
and no stack transaction for an untaken call. The retained Read-stage
predicate is evaluated after older Execute publications and before stack
reservation. Only a taken call publishes its one-word frame in Execute;
the existing delayed-branch flow executes the two delay words on either
path without adding the nondelayed CC model's held clock. No new CPU save
fields are introduced. Physical phase timing is not independently measured.

`conditional-cd` passes 1,920 cases/restores across 12 signed accumulator
conditions, eight 40-bit patterns, two delay layouts (two single-word
instructions or one two-word instruction), two pages and five snapshots.
Delay instructions change the tested accumulator without changing the
retained decision. Fetched target/delay words and sampled return frames
are replaced afterward to detect rereads. Both paths execute delay work
once; only the taken path visits the callee, balances a frame and returns
to the shared continuation. `cond-cd-edge` adds 32 cases/restores for eight
unadmitted condition encodings on two pages, before/after the Decode guard.
They stop before stack reservation, data transactions or status changes;
these are prototype exclusions, not silicon traps.

An in-block CCD may target an outside callee when its complete header and
two delay words fit within the block, including a delay span ending at REA.
The frame remains `PC+4`, even when that equals the address after REA.
On a near return from outside the block, remaining Decode-owned BRC state
rebuilds discarded fetch predictions. Returning after REA wraps to RSA only
while the outer block remains active; the final iteration falls through.
Neither the stack frame nor firmware RAM is patched to select that path.

`rptb-ccd` passes 1,024 cases/restores across counts 0/1/2/7, AEQ/ANEQ/
BEQ/BNEQ, both outcomes, middle/endpoint placement, both delay layouts,
two pages and four snapshots. Every iteration performs one ordered RAM
copy and the expected delay/callee/suffix work, with balanced `PC+4` frames
and final BRC/BRAF/RSA/REA checks. Interrupt composition, nested loop-control
changes in callees, other conditional-call families and physical timing
remain unvalidated. Nondelayed CC/RC, RPTB, RPTBD IRQ/NMI, in-block far
calls, backward branches and inner single repeats retain their focused
acceptance; this does not renew every isolated CPU profile.

### Mapped MVDK sources

MVDK admits sources `0008..0019` (accumulator words, T/TRN, AR0..AR7,
SP and BK). Access resolves and retains the source address and word before
source-pointer modification or destination publication. Source/destination
aliases therefore use the old sampled word. The existing destination timing
is preserved: DAGEN registers publish in Access, RAM and T/TRN in Read.
No new CPU save fields or atomic fallback are introduced.

SPRU172C 4-108 specifies the move and addressing forms; SPRU131G 7.5.10
distinguishes direct accumulator-producer latency from mapped-write/read
no-conflict ordering. A same-clock mapped STL/POPM accumulator publication
is sampled after Execute and can still publish a DAGEN destination in the
move's Access clock. A direct ALU producer is not forwarded by that rule.

`mvdk-mapped` passes 14,256 cases/restores: 18 sources, direct/postincrement/
absolute forms, 11 destinations (eight ARs, T/TRN and RAM), four patterns,
two pages and three snapshots. Guard words use eight bits; the indirect
AR6-source self-alias uses its actual address value. Replacing fetched
literals and changing the source after Access cannot change the retained
transfer. `mvdk-producer` passes 5,184 cases/restores across six accumulator
words, three forms, STL/POPM/direct LD, gaps 0/1, AR3/RAM, four input
patterns, two pages and three snapshots. LD's immediate is eight bits.
The unseparated short-form direct-LD case checks an Access-owned pipeline
projection, not a guaranteed silicon result for violating the latency rule.
STLM-to-accumulator sources, general producer/interrupt/repeat compositions
and measured physical timing remain unvalidated.

Actual read-identity acceptance is separate from that timing projection.
The mapped, accumulator-destination and CLKMD MVDK profiles now require
the captured Address-slot instruction address/opcode inside every source
bus callback. They reject another address and debugger scope, and require
the identity to be unavailable between callbacks, including after restore.
All22,536 cases/restores pass with the existing one-read/one-write counts.
A control that bypasses only the Address-stage observer fails at case0's
first source read; it does not establish a guest arithmetic fault.
Restoring the observer reproduces the same executable SHA256
`6c27ffa46fa576b480b2773e7e8c831e146fac44e3406104e65e8d88782413e3`
and all three positive profiles. No CPU emulation change is retained.
This tests the bounded diagnostic API, not every operand family, physical
silicon phases or a renewed complete default-core checkpoint.

`mvdk-map-edge` passes 48 cases/restores for 12 still-unadmitted status,
repeat-control and peripheral sources on two pages with snapshots before/
after the guard. They stop before data transactions or destination writes;
these are prototype exclusions, not silicon traps. Existing MVMD accumulator,
MVMD ST1, T/TRN move, mapped multiply and RPTBD interrupt controls pass.
Passive Execute hooks verify the original recorder's MVDK and following
instruction, with zero observer writes; current execution counts and the
stop are stated above. Resident-memory and low-page-copy controls remain
unchanged; this is not complete recording or validated audio.

### Delayed block-repeat boundary

A passive program-space read at the stopped original recorder supplies this
instruction sequence; it is not an injected test program:

```text
03:d9c2  891a       STLM B,BRC
03:d9c3  8812       STLM A,AR2
03:d9c4  f272 d9c9  RPTBD 0xd9c9
03:d9c6  6deb 0a40  MAR *+AR3(0x0a40)
03:d9c8  e5b8       repeated body, first word
03:d9c9  e5b9       repeated body, inclusive endpoint
03:d9ca  f6e4       following delayed far return
```

SPRU131G section 6.8 specifies that RPTBD executes either one two-word
instruction or two one-word instructions in its delay span. Thus this
sequence requires RSA `0xd9c8`, REA `0xd9c9`, one MAR update before the
loop, and `BRC+1` body iterations. Treating the delay span as two decoded
instructions, repeating MAR with the body, or starting RSA immediately after
the header would be incorrect. Section 7.5.8.1 separately prohibits BRC
modification in RPTBD delay slots and describes producer latency before
RPTB[D]; the preceding STLM-to-BRC sequence needs that ownership checked,
not a blanket immediate forwarding rule.

The staged implementation shares RPTB's saved loop state and the existing
control-delay machinery. Complete Decode establishes the loop; the Access
redirect preserves both prefetched delay words. No new scalar CPU save fields
are needed. The `rptbd` profile passes 512 cases/restores across counts
0/1/2/7, four delay layouts (two MAR words, STM T, offset LD and the original
two-word MAR), four data patterns, two pages and four snapshots. Replacing
the endpoint and delay words after Fetch, and the delay-load source after
Read, cannot change retained work. Ordered body reads/writes, final memory,
RSA/REA/BRC/BRAF, once-only delay effects and unrelated registers are checked.

`rptbd-edge` passes 28 cases/restores across seven exclusions, two pages and
snapshots before/after the guard: STM/STLM/POPM BRC in the delay span, a
branch, a two-word spill into one remaining delay word, and nested RPTBD
in the body or delay span. They stop before data transactions, pointer
updates or count modification. These are bounded prototype exclusions,
not silicon traps or exhaustive coverage of indirect control-register writes.
The existing RPTB edge fixture now tests nested RPTBD rather than rejecting
all RPTBD instructions.

The separate original-firmware run records one RPTBD Execute entry, one
delay-MAR entry and 1,024 first-body entries with zero observer writes. Its
resident-memory comparison and first 128 ordered low-page reads still pass.
RPTB (6,168 cases/restores plus 16 edge cases), backward in-block branches,
in-block far calls, delayed LD (147,456 cases/restores) and the native 5110
save-state control pass on this implementation. These focused profiles retain
their own scope; current default-suite coverage is recorded in the
[core checkpoint](#core-regression-checkpoint).

`rptbd-irq` passes 768 cases/restores across four counts, all four delay
layouts, two pages, IRQ2/NMI, arrivals during the delay span or at the first
body write, and six snapshots. Interrupt acknowledgement follows complete
delay work; each case balances one retained continuation frame and returns
to ordered copies without repeating delay side effects. The terminal IDLE
is separated from the body by eight NOPs to distinguish loop continuation
from the separately tested IDLE-wake path. Header literals, delay words,
delay-load samples, CPU loop state and host observation counters survive
replay. Other arrival clocks, nested interrupts, instruction-produced BRC
latency matrices and measured physical timing remain unvalidated. No CPU
behavior change was needed for this additional interrupt acceptance.

The [core regression checkpoint](#core-regression-checkpoint) distinguishes
default-suite coverage from isolated-profile coverage. Instruction contracts
below retain their focused acceptance and explicit limits; none establishes
complete native audio or every composition of supported instructions.
The tool suite passes 2,861 tests with one skip; the native 5110
save-state round trip and exact original recorder resident-memory comparison
also pass. The first 128 ordered low-page reads remain unchanged.

Plain LD through `*ARx-0` or `*ARx+0` captures its old source address and
AR0-sized postmodify in Access. Read samples the retained RAM address;
Execute sign-extends or zero-extends according to SXM without rewriting the
pointer or changing carry/overflow. CMPT selects the indirect AR and may
publish ARP as for existing basic indirect consumers. AR0 selecting itself
uses the same pre-update value as address and index.

`ld-ar0-index` passes 98,304 cases/restores: 96,000 RAM cases and 2,304 low
mapped/peripheral exclusions before Read or pointer publication. The matrix
crosses eight AR fields, plus/minus, CMPT, both destinations, eight index
values, four RAM words, SXM/SST/OVM, two pages and three snapshots. SST and
OVM do not alter the word load. Pointer/index replacement after Decode checks
Access ownership; pointer replacement after Access and RAM replacement after
Read cannot affect the retained operand or duplicate updates. The exclusions
are prototype limits, not silicon faults. General producer latency,
interrupt/delay composition, other unadmitted address modifiers and measured
physical phase timing remain unvalidated.

Plain LD Smem,A/B with a long-offset address can occupy a complete two-word
control-delay span. Decode counts both words before admission; existing
Access/Read/Execute ownership retains the literal, address, auxiliary update
and RAM word through the control transfer. This follows SPRU131G 6.3's
one-two-word/two-one-word delay rule; the load is not a PC discontinuity.

`load-delay` passes 147,456 cases/restores across BD, BCD taken/untaken,
CALLD, FCALLD and RETD; offset/preincrement/circular/absolute forms; eight
AR fields, offsets +/-1 (two RAM addresses for absolute), A/B, four RAM
values, SXM/OVM, CMPT, two pages and three snapshots. Absolute ignores the
encoded AR dimension. Replacing literals after Fetch, pointers after Access
and RAM after Read cannot change the retained load or duplicate updates.
The selected target observes the result once; BCD decisions survive delay
loads that change A. Call frames and RETD's separate stack read are checked;
the CALLD/FCALLD targets stop after observation, not a return round trip.
`ld-delay-edge` adds 28 cases/restores: 16 two-word spills into a one-word
remainder and 12 still-excluded offset LDU/TS/shift-16 family cases, before
data reads, writes, pointer updates or result publication. Family exclusions
are prototype limits, not silicon traps. Mapped sources, general interrupt
composition and measured physical phases remain unvalidated.

Near B and non-delayed BC admit backward as well as forward targets inside
the current block, provided the complete branch precedes REA. A taken branch
rebuilds discarded endpoint predictions from Decode-owned BRC; an untaken BC
retains the held word's block metadata. Back edges do not consume outer
iterations. Conditional calls are explicitly excluded from this branch class,
despite sharing the generic conditional-control predicate.

`rptb-back` passes 1,536 cases/restores across outer counts 0/1/2/7, inner
counts 1/2/4/8, conditional GE back edges or LT exits plus unconditional back
edges, both accumulators, two pages, none/IRQ/NMI and four snapshots. Each
outer iteration initializes its inner accumulator counter; the inner loop
copies ordered RAM words until negative. The test checks exactly
`(outer+1)*inner` copies, one outer suffix, final counters/BRAF, continuation,
balanced interrupt frames and restored held words. Interrupts arrive after
the first copy, not at every clock. The 32-case `rptb-br-edges` control now
tests below/above-block targets and a conditional call instead of treating
ordinary backward branches as unsupported. Unconditional near calls remain
covered by `rptb-fcall-edge`. Outside-block branches, delayed back edges and
measured physical phase timing remain unvalidated.

FCALL is admitted from inside a block when the complete call precedes the
endpoint; FCALLD additionally requires both delay words inside the block.
Its target must be outside the caller's logical block. Existing retained
frames and Access-time redirection preserve the caller's Decode-owned loop
state and rebuild discarded endpoint predictions; no separate call/loop state
or save fields are introduced. Code outside the low 16-bit interval does not
acquire its body tags, but a foreign page's matching low PC can consume the
loop. See the shared [repeat-endpoint contract](rom4_dsp_loader.md#repeat-endpoint-and-page-hazard).

`rptb-fcall` passes 768 cases/restores across counts 0/1/2/7, early/near-end
placement, two caller pages, same/cross-page callees, one/two nested far-call
frames, none/IRQ/NMI and four snapshots. Interrupts arrive on the first callee
RAM read, not at every possible clock. The test checks one ordered copy per
outer iteration, balanced frame/interrupt transactions, caller-page return,
prefix/suffix counts, BRC, RSA/REA and final BRAF. The original recorder reaches
its `FCALL` Execute hook once before the next exclusion. `rptb-fcalld` adds
2,304 replay cases with the same counts/pages/depths/interrupts, middle and
endpoint delay placement, and two single-word, one double-word or original
LD/NOP delay layouts. It checks once-only delay effects and paired page/PC
frames returning to call+4. `rptb-fcall-edge` has 20 cases/restores retaining
delayed/non-delayed internal targets, a non-delayed call ending at the
endpoint, near calls and delayed spans spilling outside the block. These
reject before data work or frame allocation and are prototype limits, not
silicon traps. Callee loop-control writes, arbitrary control mixtures and
physical phase timing remain unvalidated.

Acceptance for this composition includes all 288 required checks in the
default isolated CPU suite, 16 focused repeat/control profiles, the native
5110 save-state round trip and 2,891 tool tests (one skipped). The original
REL run is a separate failed coverage diagnostic at the mapped shifted-store
boundary, not an audio acceptance test.

MACSU Xmem,Ymem,A/B retains both RAM words in Read, treating X as unsigned
and Y as signed independently of SXM. X is also published to T in Read, after
older Execute writes; retirement does not overwrite a younger T. Execute
uses the current destination accumulator, FRCT and SMUL product handling,
OVM and sticky destination overflow. Carry and other status bits are preserved.
This follows SPRU172C 4-91 and the shared multiply-product/result helpers.

`macsu-dual` passes 215,040 restores in factored matrices: 18,432 address cases
cover all 512 encodings, three alias/address layouts, linear/circular BK,
two pages and three snapshots; 196,608 arithmetic cases cross 64 value pairs,
both destinations, eight accumulator patterns, 32 status profiles, two pages
and three snapshots. This is not the address/arithmetic full cross-product.
Post-Access pointer replacement and post-Read RAM/T replacement cannot change
retained inputs; replacing the accumulator after Read checks Execute ownership.
Paired reads are ordered and occur twice even when logical addresses alias;
only Xmod updates a shared AR. `macsu-dual-edge` adds 16 low mapped/peripheral
source exclusions before reads, pointer updates or register publication.
These are prototype exclusions, not silicon traps. Physical bank arbitration
and general interrupt/repeat composition remain unvalidated.

Short-immediate RPT is admitted within an active block; tested bodies remain
wholly inside the block. RPTC owns the inner repetition; BRC and endpoint metadata
still belong to the outer block's Decode. Inner completion rebuilds discarded
prefetch predictions from BRC and resumes either after the body or at RSA
when the repeated body ends the outer block. Cloned MVDD pipeline slots do
not decrement the outer count. No new scalar save-state fields are required.
SPRU131G sections 6.7/6.8 describe the separate single/block controls; the
block-count update is Decode-owned. This is not nested RPTB support.

`rptb-single` passes 768 cases/restores over outer and inner counts 0/1/2/7,
MVDD, immediate ST and NOP bodies, middle/end placement, two pages and four
snapshots. Ordered RAM copies/stores, one prefix/suffix per outer iteration,
final BRC/RPTC, BRAF, RSA/REA and continuation are checked. NOP has no memory
effects; it is a control-state case, not an independently measured cycle count.
`rptb-rpt-edge` adds 16 cases/restores for a short header at the endpoint,
long-immediate or memory-count inner RPT, and nested RPTB; all remain prototype
exclusions before data transactions. Those exclusions are not silicon traps.
Boundary-straddling body encodings, combined inner-RPT/block interrupt behavior
and measured physical phases remain unvalidated, independently of the existing
standalone-repeat and RPTB IRQ/NMI controls.

Extended STL/STH A/B stores admit mapped T (`000e`) alongside the existing
AR destinations. The destination and any auxiliary update belong to Read;
the accumulator value and SST conversion belong to Execute, which publishes
one T write without reading the destination. `st-t` passes 49,152 cases and
restores across four stores, absolute/offset/preincrement/circular addressing,
eight AR fields, eight accumulator patterns, SST/SXM, CMPT, two pages and
three snapshots. Literal, pointer and accumulator replacement at their stage
boundaries checks that ownership without duplicated transactions.

SPRU131G section 7.5.6, table 7-13 requires separation after a store to T for
Category I consumers, including short-address TS loads, but excludes long-offset
consumers from that category. `st-t-consumer` passes 15,360 cases/restores with
short and long TS consumers at gaps 0/1, four stores, eight patterns, SST/SXM,
four RAM values, two pages and three snapshots. Short gap 0 checks only the
prototype's old-T projection, not guaranteed silicon behavior; long gap 0 uses
the new T. Its 3,072 overlapping younger LD T cases preserve Read-owned
publication after the older store's Execute, with no retirement rewrite.
`st-t-edge` adds 64 cases/restores retaining the exclusion of four other low
mapped/peripheral destinations before data reads, writes or register results.
These exclusions are prototype limits, not silicon traps. General interrupt/
control-delay composition and measured physical phases remain unvalidated.

ADD/SUB Smem,TS,A/B retains both the RAM word and T in Read before older
Execute publications, then shifts and adds/subtracts in Execute. Only T bits
5..0 select the shift. SPRU172C defines the supported domain as -16..31;
raw codes 32..47 are excluded by the staged prototype, not claimed silicon
traps. Arithmetic uses SXM, 32-bit carry, sticky destination overflow and OVM
saturation through the existing shared helper. `ts-alu` passes 153,984
cases/restores in two factored matrices: 6,528 address cases over 17 read-capable
address forms, eight AR fields, both CMPT states, all four selectors, two pages
and three snapshots; and 147,456 arithmetic cases crossing four memory values,
eight accumulator patterns, 48 supported counts, SXM/OVM, selectors, pages
and snapshots. Address and arithmetic matrices are not their full cross-product.
Post-capture T, RAM, literal and pointer replacement cannot change retained
inputs or duplicate auxiliary updates.

`ts-alu-producer` passes 18,432 cases/restores with STM T, LD T and STLM T,
gaps 0/1, eight supported counts, four memory values, SXM/OVM, A/B and ADD/SUB,
two pages and three snapshots. STLM's forbidden zero-gap sequence checks the
prototype's old-T projection, not a guaranteed hardware outcome. `ts-alu-edge`
adds 256 unsupported-count and 64 direct mapped/peripheral-source exclusions.
The former may perform one retained RAM read before rejection; the latter
reject before Read. Neither publishes an ALU result or status change.
Mapped sources and general interrupt/control-delay composition remain
unvalidated. The neighboring legacy TS-load producer control passes without
promoting its raw-64-code projection to an architectural oracle.

`LD Smem,ASM` retains its RAM word in Read and updates only ST1 bits 4..0 in
Execute, leaving T unchanged. Its shared Smem-special path also serves T loads
and memory multiplies, whose distinct T publication remains in Read. The
`asm-load` fixture passes 73,728 cases/restores over DP, SP, postincrement,
offset, preincrement and absolute addressing, all eight AR fields, all 32 ASM
encodings, four status/payload profiles, both CMPT states, two pages and three
snapshots. Replacing the literal, source word and ARs after their owning stage
cannot cause resampling or duplicate updates. `asm-edge` adds 16 direct mapped/
peripheral exclusions for control, SP and peripheral sources before reads or
register results; these are
prototype exclusions, not silicon traps. Incrementing excluded forms and mapped
ASM-source producer latency are not established by those tests.

The separate `asm-mapped` fixture admits the existing register window
`0008..0017` (AL/AH/AG, BL/BH/BG, T, TRN and AR0..AR7). It passes 49,152
cases/restores across all 16 sources, DP/postincrement/offset/absolute forms,
32 ASM counts, four payload/status profiles, two pages and three snapshots.
The Read-owned source is retained after all source registers and address
pointers are replaced; Execute changes only ASM, with no data writes or
duplicate source/literal reads. Guard-register sources use their eight-bit
width. Pointer registers are selected separately from an AR source, so
source/address alias behavior is not claimed. This fixture does not establish
general producer latency, CMPT, interrupt or control-delay composition.
SPRU131G table 7-28 and example 7-69 explicitly cover mapped accumulator
consumers, including long-address `LD *(AL),ASM`; the original recorder uses
the equivalent AH source.

SPRU131G table 7-3 and example 7-52 require zero consumer latency for this
load. `asm-consumer` passes 9,216 cases/restores with immediately following
LD/ADD/SUB ASM operations, all 32 signed shift encodings, positive/negative
accumulators, SXM/OVM combinations, gaps 0/1, two pages and three snapshots.
Upper bits of the source word do not overwrite other ST1 flags. The isolated
`t-load-control` reuses 52,224 existing RAM/mapped T-load and 768 multiply-pair
cases, with 52,992 restores, to protect Read-owned T publication without a
retirement rewrite. The 73,728-case MAC/MAS and delayed-comparison controls
also pass after the shared-path rename. General ASM-load interrupt/control-delay
composition, mapped sources and measured physical phase timing remain unvalidated.

Short two-word CMPM/BITF is admitted in a complete BD/BCD delay span. The
owner is recovered from retained pipeline slots, not reread from program
memory; calls and returns remain excluded. SPRU131G section 6.3.2 permits one
two-word instruction after a delayed conditional branch and requires that its
effects do not change the condition already tested. `compare-delay` passes
27,648 cases/restores across BD/BCD TC/BCD NTC, CMPM/BITF, DP/SP/postincrement
RAM sources, 16 value pairs, both initial TC states, an optional instruction-
driven BITF prefix, none/IRQ/NMI, two pages and four snapshots. The comparison
may overwrite TC without retargeting the branch. Replaced literals, RAM and
postincrement pointers cannot change retained inputs; IRQ/NMI acknowledgement
follows TC publication. Interrupt return is checked by the selected continuation,
balanced SP and restored status, not an IDLE bit that the interrupt may wake.
`cmp-delay-edge` adds 48 exclusions for a two-word instruction starting in the
last delay word or a three-word comparison, before operand reads or register
publication. Speculative literal prefetch is not claimed absent, and rejection
is not a silicon trap claim. MVDM-delay, standalone CMPM-branch and delayed-store
controls pass. These tests do not establish measured physical phase timing.

AR0-indexed STL/STH now admits low-byte modes `a8..af` and `b0..b7` for
opcode upper bytes `80..83`. The existing store path retains the address and
performs auxiliary modification in Read, then samples the selected accumulator
and writes once in Execute (SPRU131G section 7.1, EAB/EB store phases).
`store-index` passes 98,304 cases/restores: 96,000 RAM cases and 2,304 low-page
exclusions across all four selectors, both directions, eight AR fields, both
CMPT states, eight index values, four accumulator patterns, SST/SXM combinations,
two pages and three snapshots. Changing every AR after Access proves Read-owned
address/index selection; changing the selected accumulator after Read proves
Execute-owned data. A and B differ, and no destination read occurs. Replacing
ARs after Read also detects duplicate pointer updates. Low-page CPU/peripheral
destinations fail closed before update/write; this is not a silicon trap claim.
Instruction-driven producer and delayed-control composition, and physical bus
phase timing, remain unvalidated. Store-bypass and indexed-MAR controls pass.

MAX/MIN (`f486/f586/f487/f587`, SPRU172C 4-99/100) compares signed 40-bit
accumulators in Execute and replaces only the selected destination and carry.
Equality selects B and sets carry; SXM and OVM do not change comparison or
saturate the selected value. `minmax` passes 65,536 cases/restores: all four
selectors, 256 value pairs, eight status combinations, two pages and four
snapshots. Guard-bit patterns distinguish signed 40-bit from signed 32-bit or
unsigned comparison. Replacing both accumulators after Read proves Execute
ownership; no data access occurs. This is not an instruction-driven adjacent
producer or interrupt-composition proof. Signed-XC and EXP/NORM controls pass.

Indexed MAR now admits the AR0 subtraction/addition forms (`6da8..6db7`).
SPRU131G table 5-4 defines ordinary 16-bit auxiliary arithmetic, not reverse
carry or circular indexing; SPRU172C MAR defines CMPT/ARP selection. Both
index and selected register are consumed by the existing Access-owned modifier.
`mar-index` passes 3,072 cases/restores: both directions, eight AR fields,
both compatibility states, eight indices, two starting layouts, two pages and
three snapshots. AR0 may also be the destination. Replacing every AR after
Access proves retirement and restoration do not reapply the update; no data
read, data write or extension fetch occurs. Adjacent AR0/CMPT producer latency
and physical phase timing are not established by this fixture.

Immediate MAC (`f067/f167/f267/f367`, SPRU172C syntax 3) retains its literal
and Read-owned signed T operand through the existing Class-2 path. Execute
adds the signed product to the selected 40-bit source accumulator. FRCT shifts
the product by one; SPRU131G section 4.5.3 requires SMUL pre-accumulation
saturation only when SMUL, FRCT and OVM are all set. Final destination overflow
and saturation use the shared multiplier helper; T and carry are preserved.
`mac-t-imm` passes 172,032 cases/restores: 24,576 isolated and 147,456 with
STLM-to-T producers, all four accumulator selections, 64 operand pairs and
16 flag combinations. Literal replacement and post-Read T mutation cannot
alter the retained operands. Zero-gap STLM sequences check the prototype's
stale-T projection, not a guaranteed silicon result. Adjacent accumulator
producer timing, repeat and interrupt composition remain unvalidated.
The 86,016-case immediate-MPY control also passes after sharing this path.

Dual-memory ADD/SUB (`a000..a3ff`, SPRU172C syntax 6) uses the same retained
address/read machinery as dual MPY, but does not load T. Both operands are
extended according to SXM and shifted left by 16; their sum/difference replaces
the destination independently of its previous value. Execute publishes carry,
sticky destination overflow and OVM saturation through the shared arithmetic
helper. `alu-dual` passes 86,016 cases/restores: 36,864 address cases and 49,152
arithmetic cases across ADD/SUB, all 1,024 encodings, three alias layouts,
linear/circular addressing, 64 value pairs, 32 status combinations, two pages
and three snapshots. These are factored matrices, not a full cross-product.
`alu-dual-edge` adds 32 low-page operand exclusions before reads or pointer
updates. Physical bank timing, repeat composition and general producer latency
remain unvalidated. The full 43,008-case dual-MPY control also passes after
sharing the retained dual-read path.

Dual-memory MPY (`a400..a5ff`) captures both addresses before modifying either
auxiliary register. SPRU131G section 5.5.4 gives Xmod priority when both operands
use the same AR. Two logical reads supply signed 16-bit operands; X publishes T
in Read, while the product publishes in Execute with FRCT and destination
overflow/OVM handling. Execute does not rewrite a newer T value.
`mpy-dual` passes 43,008 cases/restores: an 18,432-case address matrix covers
all 512 encodings, three alias layouts, linear/circular addressing, two pages
and three snapshots; a separate 24,576-case arithmetic matrix covers 64 value
pairs and 32 status combinations. These are factored matrices, not their full
Cartesian product. `mpy-dual-edge` passes 16 exclusions for low-page CPU and
peripheral sources before reads or pointer updates. Physical bank arbitration
and repeat composition remain unvalidated; these exclusions are not silicon
trap claims.
The EXP/NORM, mapped repeated-MVDD and XC controls pass, as does the native
5110 save-state round trip. The original resident-memory comparison still
passes independently of the new Decode boundary.

SPRU131G section 7.5.10 distinguishes direct accumulator consumers (no
pipeline latency) from memory-mapped accumulator reads. The zero-terminated
table walk uses adjacent `LDU *AR2+,A` / `RC AEQ` at `1c47..1c48`.
Its return predicate must consume the older load's Execute publication,
not the accumulator present at Access. The atomic zero-entry control
returns from `1c48` to `1c59` (SP `6f12`, AR2 `8002`, A zero), then
restarts the table walk through `1c54`. A synthetic nonzero terminator
would hide a producer/consumer timing error rather than repair the table.
`ret-load` passes 64 cases/restores: A/B, zero/nonzero results opposed to the old accumulator,
adjacent/one-NOP separation, two pages and four snapshots. Conditional return
now decides after older Execute publications and retains its stack sample.
Sequential prefetch preserves the existing taken/untaken cadence. The 768-case
isolated return fixture, conditional-call and stack/IRQ-return controls pass.
These checks do not establish physical bus-phase timing.
The following `BC ...,NC` (`f808 e6bc`) at `03:e6be` now executes through
the retained branch machinery. `carry-branch` passes 384 cases/restores:
C/NC, BC/BCD, both flag values, status-bit producers with gaps 0/1/2,
two pages, four snapshots and post-evaluation flag mutation. Branch target
and literal fetches remain exactly once. Arithmetic carry producers are not
covered by this fixture. TC, BIO and adjacent-load return controls also pass.
Long-offset BANZD `6eea ffff 949b` at `02:94c5` now shares the existing
retained offset-address and delayed-branch machinery. Offset `-1` and target
`949b` are separate retained extensions; the following `7006 0012` is one
two-word delay instruction, not another branch operand. `banzd-long` passes
4,608 cases/restores covering all eight registers, offset/preincrement/circular
forms, eight offset pairs, both CMPT states, two pages and both delay layouts.
Later literal/AR/BK changes cannot alter the captured decision or duplicate
the auxiliary update. No data-memory transaction is performed by the branch.
Physical stage timing remains unvalidated.

WRITA `7f00` at `02:8931` now executes as a retained data-to-program transfer.
Its wrapper loads A with `032e00`; common-window mapping projects that to
program RAM `2e00`. Source DAGEN belongs to Access, the data word is sampled
once in Read, and the program destination is retained after older Execute
publications. Four held clocks model Class 26's additional latency; their
bus-phase placement is provisional, not measured silicon timing. Later source,
AR, A or PMST mutations cannot redirect or resample the retained write.
The wait counter and mapped program destination are save-state registered.

`write-a` passes 3,584 cases/restores: DP, CPL, short postincrement, absolute,
offset, long preincrement and circular sources; eight registers, four values,
two logical code pages, two mapping states and four snapshots. It checks one
source read and one program write, no destination read, status preservation
and physical program-RAM readback. `write-a-edge` passes 12 cases/restores
rejecting CPU/peripheral sources, RPT, RPTB, control-delay and XC composition
before any data read or program write. Those excluded combinations remain
work to implement, not hardware-invalid forms. Mapped READA, PMST mapping,
MVDD source repeats and load/return controls pass. WRITA's focused acceptance
is separate from the versioned aggregate regression scopes.

Compact Xmem arithmetic (`9000..93ff`) retains its RAM address and performs
one pointer update at Access, samples the word at Read and publishes ADD/SUB
at Execute through the shared arithmetic helper. It preserves the other
accumulator and updates carry/sticky destination overflow with SXM/OVM
semantics. `xmem-alu` passes 393,216 cases/restores over all 1,024 encodings:
16 Xmem modes, shifts 0..15, eight source/accumulator patterns, both
accumulators, eight status combinations, two pages and three snapshots.
Source-memory and AR changes after their sampling boundaries cannot change
the result. BK-zero linear and circular-wrap cases are included; mapped
sources and repeat composition are not established by this isolated fixture.
The original `9008` at `03:838b` executes inside the firmware's block-repeat
loop and advances to the current unsupported form; this is a bounded original
composition observation, not general block-repeat arithmetic acceptance.

Compact shifted Xmem stores (`9800..9bff`) now present the retained write
address on EAB in Read and write EB in Execute, following SPRU131G section
7.1 and SPRU172C Class 10A. Source shifting and SST saturation use the
Execute-visible accumulator; this is distinct from a memory operand sample.
AR modification occurs once with the write address, before younger Access
uses the pointer. CPU low-page register/peripheral destinations remain excluded.

`xmem-store` passes 786,432 cases/restores across all 1,024 STL/STH encodings:
16 Xmem modes, shifts 0..15, eight 40-bit source patterns, both accumulators,
16 status combinations, two pages and three snapshots. The fixture changes
AR after EAB sampling and the source accumulator before Execute, then checks
the actual RAM word. SST, SXM and OVM vary independently; neither the source
accumulator nor status is changed by the store. There are no destination reads.
`xmem-store-bp` adds 288 cases/restores for same-location store-to-load bypass,
with zero/one gap, alias/nonalias addresses, six producers and both load
destinations. A bypass suppresses only the stale backing-memory read, not the
store's real Execute write. Physical bank arbitration remains unvalidated.

The original `9808` at `03:83ad` executes after a long-offset load and advances
to the current boundary. The passive source observer also reports terminal
live PC, illegal and idle state; a quiet short run is not acceptance.
DRSUB (`5800/5900`) uses the same retained paired-read path as DADD/DSUB,
but computes memory minus the selected accumulator. At original `02:8d23`,
`58f8 efb8` consumes the RAM pair `efb8/efb9` after a DLD from `efbe/efbf`.
The two words are sampled once in parity-defined MSW/LSW order, then the
shared long-word arithmetic helper publishes at Execute. C16 mode subtracts
the two lanes independently and does not saturate with OVM; full-width mode
uses SXM, carry, sticky destination overflow and OVM saturation.
`drsub` passes 6,144 cases/restores across eight read-capable address forms, A/B, eight
patterns, C16/SXM/OVM combinations and both pair parities. Later extension,
AR/SP and memory changes cannot alter the retained address or word pair.
`long-pair` independently runs 6,144 DSUB and 12,288 DADD cases
as controls. Both profiles also require the 3,584-case short-MOD=3 rejection
bank described above. CPU low-page pairs remain excluded; physical phase timing and
general producer/interrupt composition are not established by these fixtures.

NORM (`f48f/f58f/f68f/f78f`) retains T in Read before older Execute publishes
and decodes that retained count at Execute. SPRU172C 4-122 defines the
documented shift domain as -16..31. ROM4's Pow2 helper additionally uses raw
TS codes 32..47 as aliases of right shifts -16..-1, followed by conditional
SFTA -16 for larger shifts. Atomic and staged NORM share this decode; see
`docs/cobba_control_boundary.md` for its independent evidence and limits.
The shared shift helper supplies SXM right shifts, sticky destination overflow
and OVM saturation while preserving carry and the unselected accumulator.
`norm` passes 98,304 cases/restores over all selectors, 48 supported shifts,
eight 40-bit patterns, eight status combinations, two pages and four snapshots.
Later T mutation cannot change the captured count. The compatibility profile
name `norm-edge` now covers 16,384 alias cases/restores across all selectors,
eight source patterns, eight status combinations, two pages and two snapshots.

SPRU131G table 7-13 and example 7-40 require one separating word after EXP.
`exp-norm` passes 6,144 cases/restores over all selectors, 16 patterns,
gaps 0/1/2, SXM/OVM, two pages and four snapshots. Gap zero checks the
prototype's stale-T projection, not a guaranteed hardware result for an
invalid-latency sequence. The original ABS/EXP/two-word move/NORM prefix at
`03:de97..de9b` has the required separation and now executes.

Staged XC accepts the twelve single signed accumulator predicates GE/LT/NE/EQ/
GT/LE for A/B with XC1/XC2 protected spans. XC itself is a one-word opcode.
It uses the shared signed
40-bit comparator at Access, preserving the existing skip-word and two-word
interrupt-protection mechanisms. Carry/BIO/overflow and combined conditions
outside the previously supported TC/NTC/UNC forms remain unimplemented.
`xc-signed` passes 2,304 cases/restores: six comparisons, A/B, eight patterns,
three legal slot layouts, two pages and four snapshots. Predicate replacement
before and after Access distinguishes the owned sample from Decode/Execute;
guard-only nonzero and positive-bit-31 values prevent a 32-bit comparison.
The two-word layout includes an opcode-shaped literal and verifies that
rejected words never become a nested instruction or data write.

`xc-controls` passes 212 existing cases/restores covering unconditional IRQ
protection, arithmetic/IRQ composition, TC/NTC and equality sampling. This
guards the common mechanism; signed-predicate/IRQ composition is not separately
established. The original `XC 1,ALT` at `03:dea4` executes without a firmware
branch override. Aggregate test counts and the current execution boundary
are maintained above, not repeated for this instruction family.

The unchanged `aa88` upload contains two pertinent signed in-block forms:
`ff4a` at `03:da79` is `XC 2,BGEQ`, protecting `32f8 0009` (a two-word
ASM load); `fd4b` at `03:da85` is `XC 1,BLT`, protecting `ed11`
(`LD #-15,ASM`). Both spans finish inside `03:da71..03:da93`.
SPRU131G example 7-57 explicitly uses XC within RPTB. The frontend admits
its supported predicates when both interrupt-protected words remain inside
the block. Selected instructions and substituted NOPs preserve their original
block/end tags, including a two-word instruction ending at REA.

`block-xc` passes 5,760 cases/restores over 15 predicates (12 signed A/B
conditions, TC/NTC and UNC), three slot layouts, counts 0/1/2/7, interior/end
placement, two pages, four snapshots and two source rotations. It verifies
ordered source reads, selected writes, absent rejected writes, opcode-shaped
literals, pointer updates and per-iteration BRC/BRAF ownership.
`block-xc-edge` passes 24 cases/restores rejecting spans crossing the block
end before data access or pointer effects. This exclusion is a bounded model
boundary, not a claimed silicon trap.

A younger decoded XC extends interrupt protection: older retirement at the
same PC in a previous iteration must not release that younger span. Release
therefore checks the live Decode/Access/Read owners, using already-saved
pipeline state rather than a new timing counter. PC-only release can admit a
pending IRQ before the complete protected group retires.
`block-xc-irq` passes 384 cases/restores for XC1/XC2, three-/four-/five-word
loops, counts 0/1/3/7, IRQ/NMI, two pages and four snapshots. The source is
asserted by the first protected RAM write; speculative opcode fetches are not
treated as Execute entries. The pipeline's overlapping groups finish before
one vector fetch and stack write. This is a declared pipeline-ownership
contract, not measured interrupt latency for physical NOP gaps. Restoring
IFR/NMI state does not resubmit an external line before scheduler settlement.

`xc-controls`, `xc-signed`, `rptb-br-edges`, `rptbd-edge`, `rptb-bcd`,
`rptb-single` and `repeat-rearm` pass as focused controls. Predicated control
transfers, crossing loop boundaries and silicon phase timing remain outside
this composition scope. Original-firmware execution is separately counted
in [Current boundary](5510_bringup.md#current-boundary), not inferred from static bytes.

Original recorder NAND data writes are observed at `02:891c`. NAND traffic alone
does not establish a valid recording, completed file, PCM or physical audio.
The atomic control is not a physical oracle.
The load at `02:9edc` (`1281`, source `002a`) uses a bench RAM-backed
gap whose physical ownership remains unknown. Output words
`0580,0a09` are not decoded audio. No complete staged boot, service
transaction or physical DA150 mapping is established.


### Scalar memory-logic absolute aliases

Memory AND/OR/XOR must not treat reserved address bytes`f9..ff` as the
canonical absolute form`f8`. The staged classifier already rejects these;
the atomic frontend formerly fetched the extension, read data and changed
the destination accumulator. The isolated `logic-abs-edge` unchanged-core
control fails at case56, its first atomic case, on the no-data-access check.
The early admission guard now rejects before operand effects. This is
fail-closed emulator admission consistency, not a silicon-trap claim.

The672-case/restoration matrix covers all three operations, both destination
accumulators, seven aliases, CMPT settings, program pages, frontends and two
checkpoints. A/B/T, status and every AR are retained with zero data reads or
writes. The legal `logic-absolute` control separately retains1536 cases/restores
with exactly one source read, no data write and retained operands. Long MOD13
and ordinary source forms are unchanged; other instruction families remain
outside this alias audit.

```sh
.venv/bin/python tools/run_c54x_core.py --mame mame/mame --rompath mame/roms \
  --profile logic-abs-edge --output-root tmp/c54x-logic-alias-fixed
.venv/bin/python tools/run_c54x_core.py --mame mame/mame --rompath mame/roms \
  --profile logic-absolute --output-root tmp/c54x-logic-alias-legal
```

The negative capture is `tmp/c54x-logic-alias-negative/run_c54x_core_nqifkx2q`;
accepted controls are `tmp/c54x-logic-alias-fixed/run_c54x_core_7s58o4k8`
and `tmp/c54x-logic-alias-legal/run_c54x_core_piarsysf`.

### Scalar arithmetic absolute aliases

The staged scalar arithmetic classifier excludes address bytes`f9..ff`,
but the atomic frontend formerly read operands and changed status through
those aliases. The unchanged-core `arith-abs-edge` fixture fails at case56,
the first atomic case, on its no-data-transaction assertion. An early
atomic admission guard now rejects the tested families before operand
effects. This is fail-closed emulator consistency, not silicon trap behavior.

The2688-case/restoration matrix covers12 forms (ADD, ADDS, ADD-TS, ADDC,
SUB, SUBS, SUB-TS, SUBC, and the two source/destination selections of each
high-word ADD/SUB family), both accumulator selections, all seven aliases,
CMPT, program pages, both frontends and two checkpoints. A/B/T, status and
all ARs remain unchanged with zero data transactions. Legal staged absolute
ADD separately passes2048 cases/restores; mapped AR arithmetic passes1024
cases/restores covering direct/absolute forms. These controls do not close
every arithmetic opcode, repeat/interrupt composition or physical timing.

```sh
.venv/bin/python tools/run_c54x_core.py --mame mame/mame --rompath mame/roms \
  --profile arith-abs-edge --output-root tmp/c54x-arithmetic-alias-fixed
```

Negative capture:`tmp/c54x-arithmetic-alias-negative/run_c54x_core_160gph3i`;
fixed:`tmp/c54x-arithmetic-alias-fixed/run_c54x_core_rjj2xv2d`;
legal controls:`tmp/c54x-arithmetic-alias-legal-add/run_c54x_core_66hmi0hw`
and`tmp/c54x-arithmetic-alias-legal-ar/run_c54x_core_icucz_yq`.

### Long-memory arithmetic absolute aliases

Atomic long-memory arithmetic also treated unadmitted `f9..ff` operands as
canonical absolute `f8`, unlike the staged frontend. The unchanged-core
`long-abs-edge` control fails at case56, the first atomic case, on a data
transaction; ST0 also changes from1804 to1004. The shared early admission
guard now covers the full `5xxx` memory family before operand effects.
Canonical `f8` remains admitted; this is emulator fail-closed consistency,
not a silicon-trap claim or new staged support for T-dependent arithmetic.

The1,568-case/restoration matrix covers seven families (both DADD input
selections, DSUB, DRSUB, DADST, DSUBT and DSADT), two destinations,
seven aliases, CMPT, two pages, both frontends and two checkpoints.
A/B/T, status and every AR remain unchanged with zero data reads/writes.
The legal long-pair control separately retains18,432 DADD/DSUB
cases/restores and3,584 MOD3 exclusions. Other arithmetic composition
and physical timing retain their independent scopes.

```sh
.venv/bin/python tools/run_c54x_core.py --mame mame/mame --rompath mame/roms \
  --profile long-abs-edge --output-root tmp/c54x-long-alias-fixed
.venv/bin/python tools/run_c54x_core.py --mame mame/mame --rompath mame/roms \
  --profile long-pair --output-root tmp/c54x-long-alias-fixed
```

## Core regression checkpoint

The current default C54x suite includes the completed single-address
source/RMW exclusion census correction and retains the earlier
atomic load, memory-T multiply, MPY/MAC, LTD/DELAY, MACP/MACD, RMW, memory-logic,
scalar-arithmetic and long-memory arithmetic absolute-alias corrections.
All 293 required markers appear exactly once, followed by core conformance
PASS and intentional MAME exit 3; the isolated runner returns 0, with
`interrupted=false`. Its 294 ordered PASS records and 5,690 distinct asserted
opcode words exactly match the modularization baseline. The capture is
`tmp/fidelity-source-domain-full/run_c54x_core_jkrbwol3`:

```sh
.venv/bin/python tools/run_c54x_core.py --mame mame/mame --rompath mame/roms \
  --output-root tmp/c54x-modularization
```

The recorded executable SHA256 is
`350683a309ba9cf2d65977bb6e1783725a09dad3d83cdf992fd0c4c85d3c60c2`.
CPU source SHA256 is
`7191436aa1d97343cf5b75a67b041f03fd884febd29757c2791fe2afca466e16`.
`fixtures/c54x_fidelity_checkpoint.json` banks the CPU and split bench-source
hashes, immutable binary identity and exact result comparison. Inputs were
rechecked unchanged at completion. Later handset/device changes do not renew
this CPU-only checkpoint. The opcode-set SHA256 is
`7fd37083d052b81e65f629b5e2062965e612cab737cd54ee85a0f84d60ed76e4`
(sorted four-digit opcode strings joined by newlines, without a final newline).
Optional
profiles retain their separate acceptance scopes; this run does not prove
the complete ISA, physical pipeline timing, fitted DSP-mask compatibility,
native RF or end-to-end handset speech and calls. Revision-bound checkpoints below preserve their narrower
instruction and original-firmware comparisons, not competing current
default-suite baselines.

The historical `c54x_modularization_checkpoint.json` separately records seven named core scopes, representative
arithmetic, interrupt and restore profiles, and ROM4 CRC/FR decoder controls on
both frontends. The board extraction preserves all 271 compiled machine/device
XML records and passes eight handset gates: default and frontier 3210, v5.01
menu, 3310/3330/3410/5210 frontiers and the native standard-SWWSR 5110 menu.
That 5110 composition also passes its complete handset save-state gate.
Seven native PCM cases cover external frame clocks, wire delivery, pin-clocked
loopback and active save/load. These are preservation checks, not production
clock, native RF or call-media acceptance.

Separate revision-bound controls retain their narrower evidence: the
pre-modularization EFR decoder passed both frontends with 163 frames,
26,080 reference-exact PCM samples and 65,440 ordinary RAM words replayed per
frame; its staged NSE-1 Phone book menu was exact. The scalar-arithmetic
checkpoint passed the reset-bank6150 laboratory clocked-tone gate in
`tmp/nsm1-post-arithmetic-native-tone`, preserving cold-input, storage, SIM,
pixel and waveform checks. These additional runs are not silently renewed by
the modularization checkpoint.

The cold-catalogue checkpoint adds no CPU instruction admissions. Its staged
`bootopenstage` gate passes with the saved recorder endpoint and rejects the
original no-track seed after successful transport. The existing atomic
`bootstatus` control retains its exact 11 RX/14 TX exchange. Tooling acceptance
is 2,879 tests (one skip); earlier CPU instruction scopes below are not
implicitly renewed by these MU4 fixture runs.

The instruction-observation checkpoint passes `instr-observer`: 32 cases
and save/replays covering atomic/staged frontends, two logical pages,
OVLY clear/set and four snapshots. Each observes the same seven ordered
instruction entries, including three real repeat iterations; ordinary
program reads, extension words and annulled fetches produce no extra events.
Overwriting program bytes after fetch does not change the captured opcode.
The staged accelerated-repeat execution path is observed separately from
the normal Execute slot. This optional diagnostic callback is configuration,
not architectural state or a physical pin; it does not assert retirement
or an atomic snapshot of other registers. Tooling acceptance is 2,879 tests
(one skip). Fresh `check-mu4-bootstrap-original` runs pass for both
`bootrecmeta` (native success) and `bootrecstage` (expected empty-field
rejection), with exactly one startup Execute entry each and unchanged source
NAND. Their endpoint hashes reproduce the pre-observer captures exactly.
The older atomic `bootrecordram` gate and native 5110 save-state round trip
also pass with this observation wiring.
Current default CPU population coverage is stated above.

The mapped-SP ADD checkpoint passes `add-sp` (4,096 arithmetic cases/restores,
576 stack-update contexts/restores and twenty control/peripheral exclusions),
plus the unchanged `add-absolute` and `add-bk` controls. The native 5110
save-state round trip and exact original MU4 resident-memory comparison pass.
Tooling acceptance at that checkpoint was 2,878 tests (one skip). These
isolated scopes are distinct from the current full default population above.

The direct-ISR-return checkpoint at `59f4285f` renews the full default population: all
288 required markers exactly once, expected completion exit and terminal
completion check. `repeat-body-isr` passes 768 cases and save/replays.
Controls `repeat-isr`, `repeat-context`, `repeat-pmst`, `repeat-popd`,
`rptbd-irq`, `rptb-fcall`, `rptb-bd`, `rptb-bcd` and `rptb-banz` pass.
Tooling acceptance is 2,877 tests (one skip), and the native 5110
save-state round trip and exact MU4 resident-memory comparison (48,261
program and 3,656 data words) pass. These scopes do not establish complete
recording, physical loop timing or handset speech.

The earlier ASM-delay checkpoint at `930c486b` covers 30,720 cases and save/replays plus sixteen
partial-slot exclusions. Controls `asm-load`, `asm-mapped`, `asm-consumer`,
`asm-edge`, `dld-delay-edge`, `mac-delay-edge` and `st-delay-edge` also pass.
It also renewed exact MU4 resident-memory comparison (48,261 program and
3,656 data words); those additional isolated profiles are revision-bound.

The constant-DAGEN checkpoint at `026b6670` renewed the full default run: all 288 required
markers exactly once, expected completion exit and terminal completion check.
The separate `st-dagen` profile covers 3,840 positive cases, 80 exclusions
and 3,920 save/replays. Controls `imm-forward`, `st-extended-ar`,
`st-ar-consumer`, `st-t` and `stm-brc` also passed. None of these scopes
establishes complete recording or speech.

Earlier isolated renewals remain revision-bound, not implicitly rerun by
the renewed default population:

- `fd9e402c`: `dld-delay` (147,456), `dld-delay-edge` (sixteen),
  `load-delay`, `ld-delay-edge`, `add-delay`, `mac-delay-edge`,
  `st-delay-edge` and `rptb-back`.
- `40996160`: `mac-delay` (147,456), `mac-delay-edge` (sixteen),
  `mpy-delay`, `mpy-delay-edge`, `st-delay-edge`, `add-delay` and `fcala-delay`.
- `d1ea11b3`: `mac-bk` (98,304), `mac-smem` (73,728), `arith-bk`,
  `arith-ar`, `add-bk`, `add-delay`, `fcala-delay`, `mpy-mapped` and `mac-t-imm`.
- `5ad89352`: `fcala-delay` (648), the eight accumulator-call/branch
  profiles and producer controls, `far-call-delay`, `far-atomic`,
  `stack-irq-return` and `repeat-isr`.
- `1fe0df59`: full default run, all 93 named isolated profiles registered at
  that revision, and the seven additional scopes `--bus-registers`, `--bus-io`,
  `--atomic-io`, `--compose`, `--bus-stm`, `--mapped-read-a`, `--dp-absolute`.
  The separate atomic `bootreplay` control also passed bootstrap loading,
  pin-level status service and mid-byte save/replay with selected-state
  equality and unchanged NAND; this is not staged transaction completion.

Current isolated profiles are enumerated by `EXTRA_PROFILES` in
`tools/run_c54x_core.py`; the older 93-profile population is revision-bound.
A renewed default run does not imply a renewed run of every isolated profile
or of the seven additional scopes.
Each fixture retains its own opcode, composition, physical timing and bus
ownership limits. Latest tool counts, native 5110 restoration and original
resident-memory checks are recorded above; the recorder execution bound is in
[Current boundary](5510_bringup.md#current-boundary). SIP lifecycle unit tests require local
loopback socket permission. No aggregate scope establishes native speech.

Default-scope reproduction (serialize with other native runs):

```sh
mkdir -p tmp/native-gates
env TMPDIR=$PWD/tmp/native-gates python3 tools/run_c54x_core.py \
  --mame mame/mame --rompath roms --output-root tmp
```

## Recorder low-page copy

The first zero MVDD source has an explicit firmware path. Startup initializes
the object at `fe76`, including zero field `fe78`. The common-window callback
dispatcher passes that object's address to `03:8e91`; `71e1 0002 0013` at
`03:8e9d` reads `[AR1+2]` into AR3. Selector `05b8` reads zero, so the
comparison with two at `03:8ed2` does not take the TC branch. The firmware
sets AR2=`0e35`, executes `RPT #127` and reaches `e598` at `03:8eda` with
AR3 still zero. Do not infer a missing initializer from this zero alone.

The paired `bootrecstage` and atomic `bootrecsave` probes use identical
retained NAND bytes, the same acquired R060 image and the same pin-command
sequence. With a one-second stop bound:

- the pre-admission staged CPU at `046271df` rejected the first low-page
  MVDD before any source read; the mapped-source implementation now performs
  128 ordered reads and observes the subsequent `1800`, `1880` publications;
- atomic execution also starts that copy at zero, performs 128 ordered
  reads of `0000..007f`, then publishes `1800` followed by `1880` to `fe78`
  through the existing firmware buffer path (write context `02:1c96`);
- both observe selector `05b8` as zero on the first callback.

`tools/mu4_recorder_source_observe.lua` observes field, selector and low-page
activity without debugger support, memory reads of its own or DUT writes.
Attach it with `-autoboot_script /absolute/path/to/tools/mu4_recorder_source_observe.lua
-autoboot_delay 0` to the existing recorder commands, retaining
`-nonvram_save`. Compare the two NVRAM inputs byte-for-byte before the run;
the BIOS-specific NAND tags are `mu4nand_25` and `mu4nand_24`, respectively.
The paired one-second witness recorded `first_copy_reads=128`, `order_errors=0`
and `first_copy_before_nonzero=1`. The legacy observer's arm also qualifies
the selector read with a live-PC range, however, and its subsequent low-page
tap does not identify an instruction owner. Broader staged runs can mix other
MMR reads into that summary; it is not a copy-acceptance gate. Revalidation
needs retained MVDD Read identity, not a tighter guessed live-PC range.
Field-access totals likewise describe the observation window, not cadence
or acceptance constants.

This falsifies a staged-only source-pointer explanation. It does not prove
that a physical DA150 deliberately reads this page before its first buffer
is published: both frontends share board models and provisional mappings.
Do not seed `fe78` or suppress the callback. Mapped-source MVDD now has
49,152 scalar and 400 repeated save/replay cases, including CPU aliases,
read-clear peripheral endpoints and crossing the peripheral-window boundary.
Twelve exclusion cases reject CPU-register destinations, dual-peripheral
transfers and repeated peripheral writes before side effects. These use a
plain fixture RAM map: the older test map's `0060..0061` IRQ probes are not
ordinary low-page RAM. Focused MVDD acceptance and aggregate regression
scopes are recorded separately. Unknown physical register identities,
especially data `002a`, remain separate from transaction conformance.

For execution tracing, startup's instruction address is `0073f8`; the legacy
`pc=0073f9` was an advanced live PC, not the executed address. Debugger
disassembly of common-window PCs on nonzero pages
is not reliable: use the explicit loader projection of program records
(for example `02:1f83` to physical `001f83`), not a merged data/program
dictionary or a missing-word-as-zero fallback.


## NSK-1 zero-word execution boundary

The pre-correction own-input NSK-1 listed-FDN run observes 629 distinct first opcode words;
this is not the NSE-1 cold-boot inventory. `63f8` and `f166` require immediate
multiply disassembly (three and two words respectively). The decoder now
formats these existing execution forms, with the address extension before
the immediate. Focused disassembler/conformance tests pass; this is not a new
multiply-execution or native-radio qualification.

An execution-breakpoint observer across all 128 XPC pages confirms execution
at page-zero `29d3`, not merely a data read or a truncated nonzero-page PC.
Observed PMST is `ffac`; program words `29d3/29d4` and data word `29d3` are
all zero under the old `2800` overlay extent. First captured SP values are `1eab` and `1ea7`, with stack heads
`3798/00fa/3073/3073` and `38ca/3976/3923/30bc`. Zero data reads do not
exclude discarded uploads: `program_w` ignores writes outside the overlay.
Live disassembly identifies `3798` and `38ca` as return addresses immediately
after calls to ROM routine `a51b`. A bounded instruction trace follows
`a539 -> 24cf -> 26c3`, the return-address comparison against `a53b`, and
`26d0 B 29d3`. A program-write observer sees 811 writes in `2800..2b2a`,
including `29d3=7714,29d4=085e,29d5=7194,29d6=1f2b,29d7=1284` at
approximately 0.150281 s. The old mapping discards this own-firmware code.
The NSK-1 research configuration selects a `3000` overlay extent; this is
an upload-derived minimum bank model, not fitted-mask
identification or proof of the full physical RAM size. Other products are
not changed by this observation.
The corrected 65-second run passes `nsk1_native_upload_check.check_upload`,
the existing native boot/storage predicates, physical listed-FDN dial/SIM
checks, unchanged whole-card comparison and the previous exact digit/progress
frame hashes. The zero-entry probe no longer fires. No registration, connected
call, speech or full physical-memory qualification follows from this fix.
The corrected run observes 636 distinct first opcode words, all with decoded
boundaries (including observed extension variants); opcode `0000` is absent.
This is decoding coverage, not independent semantic proof for every instruction.

Reproduction uses `make_c54x_entry_script.py 0x29d3 OUTPUT`, copied as
`native_entry.cmd` into an isolated qualified NSK-1 FDN-enabled cold run, and
`nsk1_native_entry_observe.lua`. Its 65-second run retains the qualified
boot/storage predicates, physical listed-number input, SIM conversation,
unchanged whole card and exact digit/progress frames. Debugger `focus` alone
does not select the command expression CPU: the observer explicitly sets
`visible_cpu`, then verifies installation of all 128 DSP breakpoints.
Queued headless startup scripts or failed breakpoint installation supply no
negative execution evidence. The bounded counter is debugger-host state only.

## Native MAR assertion attribution

Missing exact-word tags in the default fixture do not imply missing semantic
coverage across isolated profiles. `mar-index` already checks 3,072 cases
plus save/restore replays against independently calculated 16-bit AR updates;
`mar-reverse` checks another 3,072 cases/replays using a bit-serial oracle.
The architectural Access update now emits exact-word assertion attribution:
`6da8..6db7` for indexed MAR, `6da0..6da7` and `6db8..6dbf` for reverse carry.
Fresh verbose profile runs require both the existing completion gate and
execution evidence for each attributed word. CPU execution is unchanged.

The corrected NSK-1 trace contains indexed words `6db1/6db2/6db3/6db7`,
with counts 5/69751/69751/2244: 141,751 executions now link directly to the
isolated semantic fixture. This is not a new all-opcode closure, physical
pipeline timing proof or evidence about arbitrary producer compositions.
Use `c54x_opcode_coverage.py --additional-fixture-log` to combine separately
completed profile evidence; never manufacture execution credit by combining
an assertion-only run with a handset's execution trace.

## Native dual-memory subtraction

The corrected NSK-3 long trace executes `a245` 69,751 times. This exact
encoding is `SUB *AR2-,*AR3-,A`: both pointers post-decrement, unlike
the separately tested mixed-direction `a249` form. The isolated
`dual-sub-native` profile checks eight source patterns in each operand,
with SXM and OVM independently clear/set (256 cases, each replayed from
save state). It checks the shifted signed/unsigned difference, guard bits,
32-bit overflow/saturation, no-borrow carry, unchanged B/T/status controls,
unchanged source RAM and both pointer updates. Initial overflow flags are
clear; sticky overflow, other registers/encodings, staged pipeline ownership
and physical cycle timing are outside this fixture's claim.

```sh
.venv/bin/python tools/run_c54x_core.py --mame mame/mame \
  --rompath mame/roms --profile dual-sub-native --verbose \
  --output-root tmp/native-dual-sub
```

The qualified run contains the `a245` assertion and 512 actual executions.
No CPU arithmetic implementation change is needed. The default core runner
separately requires every marker in `run_c54x_core.REQUIRED`; its assertions
alone must not be promoted to exact execution attribution without an opcode
inventory.

The runner owns a separate process group and reaps it on SIGTERM,
KeyboardInterrupt or wait failure; interruption is never a conformance pass.
`exit_status.json` retains the actual child return code, command and
`interrupted` flag, including interrupted runs that reach cleanup. No manifest
or partial PASS prefix establishes completion. An uncatchable launcher kill
still requires external orphan cleanup. Profile runs qualify only their named
matrix, not the default full-core set or native handset speech.

## Paired-memory assertion attribution

`indirect-dld` already validates extended DST (3,072 cases), short-indirect
DST (9,216), indirect DLD (23,040), and store/load composition (1,152),
including saved in-flight execution. The final paired-memory result checks
now attribute their exact opcode instead of only emitting family-level PASS
markers. Source/address retention, bus counts and pointer checks are unchanged;
this does not change CPU execution or expand the tested status/operand domains.

A fresh verbose profile run links 22 observed NSK-3 words (641,822 executions)
to actual execution and assertions in that fixture. Eight of these words were
absent from the default fixture's assertion inventory: `4e81/4e83/4e85/4e86`,
`4ed2`, `5682`, `578d`, and `57d2` (120,365 observed executions). This is
attribution of existing semantic checks, not discovery of eight CPU defects.
Run `tools/run_c54x_core.py` with the same paths as above and
`--profile indirect-dld --verbose`, retaining its complete log before combining
it through the coverage tool's `--additional-fixture-log` option.

## Mapped move and younger repeat ownership

Decode can arm an RPT header while an older MVDK remains in Access. A global
repeat-active flag does not make that older instruction a repeated body.
The mapped-source/destination MVDK exclusions apply only when the body is
latched and the instruction's full program address matches that body.
They still reject genuinely repeated mapped moves before source/pointer or
destination effects. No new mapped-repeat execution semantics are admitted.

`run_c54x_core.py --profile move-rpt-owner` checks 64 cases and cleared-pipeline
save/restores: `MVDK TRN,AR2` before short/long RPT, the same move as the rejected
body, counts 0/3, two values/pages and two checkpoints. The pre-fix core rejects
the ordinary predecessor before any body is latched. The fixture establishes
instruction ownership, not physical pipeline-phase accuracy.

## Mapped source in parallel Store/Load

ST||LD may read X addresses `0000..005f` while Y remains ordinary RAM.
Owned registers use their existing read interfaces; gaps `0002..0005` and
`001f` delegate to the board map. X is retained in Read, whereas the store
uses the Execute-live old accumulator before load replacement. Mapped AR
sources observe the existing Access-stage modifier model. Loading T retains
its Read publication and cannot overwrite a newer T in Execute.

`stld-map` passes 147,456 cases and cleared-pipeline replays: all six A/B/T forms, all 32
source addresses, eight nonaliasing selector pairs, two patterns, SXM/SST/OVM,
ASM=0/-1, two pages and three checkpoints. It checks one read and one RAM
write, exact pointer updates, late accumulator/ASM/T changes and preserved
carry under the existing explicitly unvalidated policy. IMR/IFR reads do not
acknowledge interrupt state. Same-pointer aliasing, other ASM values and RAM
numeric boundaries remain in the separate `stld` matrix. Mapped Y writes and
mapped single-repeat bodies remain excluded. Physical
timing and arbitrary block/interrupt composition are not proved here.

The acquired EFR loop uses AR5 as a postincremented counter while `c8ba`
also performs its X read; atomic and staged traces agree on that deliberate
low-address sequence. Qualified peripheral reads let that loop complete.
Both atomic and staged EFR encoders pass all 163 complete reference frames:
9,291 parameters, 326 VAD/SP flags and 2,771 packed output words, including
all 65,440 ordinary RAM words on every frame's save/replay. The repeatable
gate is `make check-c54x-rom4-efr-encoder-staged`; it verifies the independent
ETSI fixture and retains the atomic comparison requirements. Staged FR
encoder and decoder retain their separate 32-frame controls. These isolated
DTX-disabled algorithm results do not prove handset speech or physical bus
timing; DTX has its own acceptance below. The staged decoder separately
passes 163 clean speech frames and all 26,080 PCM samples, with full-RAM
replays, through `make check-c54x-rom4-efr-decoder-staged`. Its existing
ten-case delayed-repeat preflight remains atomic; decoder reset and every
speech frame run on the staged frontend with an explicit selection marker.

Both staged EFR homing suites also pass their independent 46-frame fixtures
with full-RAM replays: encoder 2,622 parameters, 92 flags and 782 packed
words; decoder 7,360 PCM samples. Their reference includes six homing frames
and two matching twenty-frame speech segments. The decoder additionally checks four
steady encoder-homing outputs. Acceptance targets are
`check-c54x-rom4-efr-homing-encoder-staged` and
`check-c54x-rom4-efr-homing-decoder-staged`.

Staged DTX encoding and decoding pass all 518 frames: 29,526 parameters,
1,036 flags and 8,806 packed words match on encoding; 82,880 PCM samples,
eight SID frames, 135 erased frames and 46 nonzero comfort-noise frames match
on decoding. Both check full 65,440-word RAM replay for every frame.
Acceptance targets: `check-c54x-rom4-efr-dtx-encoder-staged` and
`check-c54x-rom4-efr-dtx-decoder-staged`. Atomic runs remain controls.

The independent invalid/bad-SID conversation also passes all 518 frames on
the staged frontend: 82,880 PCM samples, four invalid codewords, one bad valid
codeword, per-frame invalid-SID control checks and full-RAM replay.
`check-c54x-rom4-efr-invalid-sid-staged` uses the same hashed inputs, reference,
program and cold data as the atomic control. Its dedicated frontend marker
is mandatory; this is decoder-entry coverage, not RF SID classification.

The mapped AH
multiply-accumulate at `fc00` (`6709`, literal `f38b`) is now covered by
the mapped memory/immediate MAC contract below.
Short staged BIOS aliases preserve MAME's sixteen-character limit. Fixture
parity tests require identical hashes in paired atomic/staged regions, and
the runner rejects MAME driver-validity errors even if a completion marker
is printed afterward.

Both EFR frontend BIOSes must load the same hashed PCM and reference-vector
files; program/data ROM coverage alone does not establish codec-fixture
coverage. A fixture-declaration regression enforces this pairing. With these
inputs loaded, verbose first-frame-entry snapshots agree on all 65,440
ordinary RAM words after reset and PCM publication. Younger Access/Read
publications make raw pre-Execute register equality an invalid pipeline
equivalence test. Verbose tracing retains the independent vector
acceptance requirement and does not alter the ordinary gate.

MVDD's retained Read sample bypasses a pending same-location RAM write to
an ordinary following load. The acquired example is CALLD `d392`, final
delay-word MVDD `d395` (`e5a1`, `ed8a` to `006e`), then Xmem load `d40d`.
Without bypass the load retains previous `006e=7ff8` despite MVDD having
sampled `7fd3`; the resulting low-word store to `0071` is `e000` instead of
`4c00`. The earlier read of `006e` belongs to the first delay-word DELAY,
not the callee: PC context alone is not an operand-ownership annotation.

`mvdd-follow` passes 1,536 cases and cleared-pipeline replays: adjacent and
both CALLD delay slots, Smem/Xmem loads, all four source modifiers, four
boundary values, zero/one gap, two program pages and four snapshots. It
checks retained source after poisoning, new-value consumption, exactly one
source read and copy write, balanced return and preserved sibling state.
Disabling only this bypass makes the fixture fail on stale RAM. Physical
pipeline phases and arbitrary peripheral/interrupt compositions are not
claimed. Repeated MVDD deliberately remains outside this bypass: its
overlapping RAM matrix qualifies Read-before-older-Write ordering separately.

Ordinary mapped ADD/SUB must likewise retain the older AR operand before a
younger Access update. In the acquired case ADD `b9b4` (`0013`) precedes
parallel store/load `b9b5` (`c8d8`): consuming the incremented AR3 wrongly
stores `0fb8=1` instead of zero. `addsub-ar-age` passes 18,432 cases and
cleared-pipeline replays: ADD/SUB, DP/indirect/absolute/offset forms, all
eight ARs, both accumulators, four boundary words, MAR increment/decrement
and MVMM writers, both SXM states, two pages and three snapshots. A zero
initial destination isolates operand age and extension from unrelated ALU
producer hazards. Absolute ADD's separate decode path is included.
Carry/TS/shift-16 arithmetic, RMW, logic and dual-read age compositions
retain their separate contracts; this does not admit them implicitly.

At `0020` the board endpoint is BSP receive data, whose ordinary read clears
receive-ready and overrun; debugger reads must not acknowledge either.
The independent `shift-bsp` control passes 40,960 cases and save/load replays.
The parallel `stld-bsp` fixture passes 6,144 cases and cleared-pipeline replays:
six forms, eight distinct X/Y selector pairs, two signed-boundary patterns,
SXM/SST/OVM, two pages and four checkpoints spanning Access, its extra wait,
Read and Execute. It checks one acknowledged source read, one RAM store,
retained ASM, Execute-live accumulator data, Read-owned T publication and a
second frame left pending. The staged wait classifier now includes this
parallel source. Destination MMIO, mapped single-repeat bodies and simultaneous
peripheral arbitration remain separate exclusions. The extra clock is a
documented instruction cost, not a measured silicon pipeline phase; the
parallel carry-preservation policy remains unvalidated.

`stld-periph` passes 393,216 cases and cleared-pipeline replays, extending
the same six-form/eight-selector/two-pattern/mode/page/checkpoint matrix
across all 64 addresses `0020..005f`. A volatile read fixture checks one
sample at Read with the extra peripheral clock, one RAM store at Execute,
and no resampling after the endpoint changes. This is bus-delivery and
instruction-ordering evidence, not an endpoint-identity claim: the fixture
supplies the read value even at CPU-owned timer/clock addresses. Actual
register semantics retain their separate gates. On the handset board,
`0021` reads BSP transmit data without acknowledging it, `0022` reads BSP
control, `0024..26` are CPU-owned timer registers, and unowned gaps delegate
to the board map. The BSP receive side effect is qualified separately above.

## Short T loads in ordinary delayed controls

Short RAM `LD Smem,T` may occupy either delay word of BD, BCD, CALLD,
FCALLD or RETD, in addition to the separate RPTBD/BANZD contracts. Access
owns the address and pointer modification; Read publishes the sampled word
to T. Execute must not overwrite a newer T, and the selected continuation
must consume the resulting T through the ordinary multiply path.

`ctrl-t-load` passes 55,296 cases and cleared-pipeline replays: six control
outcomes, both slots, direct/plain/postincrement/postdecrement sources,
DP/CPL, four patterns and flag combinations, both multiply destinations,
two pages and three checkpoints. Three cohorts retain the loaded T or
replace it at Read with its complement or `5aa5`. Source/opcode/target
poisoning, one source read, target products, pointer effects and owner-only
stack transactions are checked. CPL addresses account for each control's
SP change; BCD's second delay position uses the existing prototype control
timing helper, not a newly measured silicon phase. The mapped/peripheral,
extended-address and arbitrary interrupt/producer compositions are not
qualified by this RAM fixture.

## Mapped memory/immediate MAC

Memory/immediate MAC admits mapped accumulator fields `0008..000d`,
T/TRN and AR0..AR7. Read retains the mapped word and publishes it to T;
Execute combines that retained word and literal with the live selected
accumulator. Source and destination accumulator selectors remain independent.
SMUL clamps the fractional product before accumulation only in its existing
fractional-overflow mode; result saturation and sticky OVA/OVB remain
destination-owned. No data-memory write is synthesized.

`mac-map-imm` passes 786,432 cases and cleared-pipeline replays: all sixteen
mapped addresses, DP/plain-indirect/absolute/offset forms, four source-word
patterns (guard fields retain their eight-bit width), eight literals, all
FRCT/OVM/SXM/SMUL combinations, four accumulator selectors, two pages and
three snapshots. A/B change after Read to distinguish retained memory data
from the Execute-live addend, including same-accumulator source aliases.
T, TRN, pointers and literals are poisoned after their ownership boundaries.
The fixture checks exact product/accumulation/saturation/status results,
one source read, no bus write and no duplicate extension fetch. Restoring
the old MAC mapped-source guard makes it fail.

The existing `mpy-mem-imm` RAM/MPY controls retain their matrices. Their
negative MAC fixture now excludes the remaining eighty low/control/peripheral
addresses and eight write-only MOD=3 forms: 1,408 cases/replays across all
selectors, two pages and two snapshots. Producer, control, repeat and
interrupt compositions are not newly qualified, nor are physical bus phases.

## Parallel Store/Multiply

The staged frontend admits `ccxx..cfxx`: either accumulator as store
source and either accumulator as multiply destination. X/Y capture and
modification share the parallel-store path. Read retains X, T and ASM;
Execute stores the old live accumulator before replacing the selected
destination with signed X times T. FRCT doubles the product, OVM applies
to the result, and SMUL does not apply the MAC-family product clamp.
The atomic model's carry-preservation policy remains silicon-unvalidated.

`st-par-mpy` passes 270,336 cases and cleared-pipeline replays: 73,728
address cases and 196,608 numeric cases, all 1,024 encodings, linear and
circular addressing, pointer/memory/accumulator aliases, all 32 ASM modes,
signed word patterns, FRCT/OVM/SMUL/SXM/SST and sticky-status combinations,
two pages and three checkpoints. T and source memory change after Read;
the accumulator changes before Execute and ASM changes after capture.
`st-mpy-edge` retains 64 replayed mapped-X/Y rejections before reads,
writes or pointer effects. Mapped/peripheral operands, arbitrary producer/
interrupt/repeat composition and physical bus phases are not qualified.

## Memory SQUR in delayed controls

Short RAM SQUR Smem,A/B (`26xx/27xx`) may occupy ordinary control and
RPTBD delay words. Read retains Smem and publishes it to T; Execute replaces
the selected accumulator with the signed square. FRCT doubles the product,
OVM clamps the result with sticky overflow, and SMUL does not clamp the
product. This differs from SQURA/SQURS, which accumulate into the old value.

`squr-delay` passes 27,648 cases and cleared-pipeline replays across BD,
both BCD outcomes, CALLD, FCALLD and RETD; both destinations; DP/plain/
postincrement RAM; eight patterns; FRCT/OVM/SMUL/SXM; two pages and three
checkpoints. `rptbd-squr` passes 92,160 cases and cleared-pipeline replays:
first/second/both-same/both-split layouts plus SQUR followed by MPYA,
with the same addressing/pattern/mode dimensions, outer counts 0/1/7 and
four checkpoints. The mixed case consumes the correct signed 17-bit A
high word at MPYA Execute. Per-clock results/status, one read per source,
pointer/opcode retention, Read-owned T followed by a newer T, and completed
results observed by every outer-body copy are checked. Extended/mapped
sources, arbitrary interrupts and physical bank phases remain unqualified.

## Square accumulation in ordinary delayed controls

Short RAM SQURA/SQURS forms may occupy ordinary delayed branch/call/return
spans as well as the independently qualified RPTBD delays. Read retains
the signed memory operand and publishes it to T; Execute adds or subtracts
its square from the selected live accumulator. FRCT doubles the square,
OVM applies to the result, and SMUL does not clamp the product.

`square-delay` passes 55,296 cases and save/load replays: BD, both BCD
outcomes, CALLD, FCALLD and RETD; direct, indirect and postincrement RAM;
both operations and destinations; eight patterns; FRCT/OVM/SMUL/SXM;
two pages and three checkpoints. It checks one retained read, pointer
updates, T publication followed by a newer T value, per-clock accumulator
publication, result/status/stack preservation and target observation.
BCD fixes A to satisfy its branch condition, so those cases do not vary
A independently. Extended/mapped sources and arbitrary interrupt composition
remain unqualified. The existing RPTBD square matrix and mapped-repeat
exclusions remain separate controls; physical silicon phases are not proved.

## Rounded memory multiply in delayed controls

Complete MPYR Smem forms may occupy ordinary delayed branch/call/return
spans. The signed memory and T operands are retained in Read; Execute rounds
the product by adding `8000` and clearing its low word, then applies the
existing result-overflow/OVM rules. FRCT doubles the product before rounding.
MPYR does not use the MAC-family SMUL product clamp and does not publish T.

`mpyr-delay` passes 36,864 cases and save/load replays over BD, both BCD
outcomes, CALLD, FCALLD and RETD; postincrement, absolute RAM/AR7 and offset;
both destinations; sixteen signed operand pairs including positive/negative
rounding ties; FRCT/OVM/SMUL; two pages and three checkpoints. Memory, T,
pointer and literal poisoning reject resampling, while the target observes
the completed result and stack effects remain exact. `mpyr-delay-edge`
retains sixteen straddle rejections without data-side effects. Other
addressing, interrupt composition and physical pipeline phases remain
unqualified; ordinary MPY/MAC fixtures are separate controls.

## Low-address dual ADD/SUB operands

Dual ADD/SUB admit IMR/IFR (`0000/0001`) as Read-owned, non-acknowledging
sources, and delegate `0002..0005` to the existing board map. No read constant
or peripheral identity is introduced for those unowned words. ST0/ST1 and
other low control/peripheral sources remain guarded, as do low sources for
multiply/MAC/SQDST.

`alu-map-irq` passes 18,432 cases and cleared-pipeline replays over IMR/IFR,
X/Y/both placements, eight selector pairs, ADD/SUB, both accumulators,
SXM/OVM, BK=0/32, two patterns, two pages and three checkpoints. Reads leave
IMR/IFR intact. Post-Read poisoning changes both registers to a disjoint
mask/pending pair, avoiding an unrelated IDLE wake, while Execute must use
the retained operands. This does not qualify concurrent interrupt arrival.
`alu-low-bus` passes 36,864 corresponding cases over all four unowned words,
with mutable board-map values and post-Read poisoning. Address modifiers and
alias priority retain the existing dual-operand model; physical bus phases
and producer/control hazards remain unvalidated.

Both acquired-ROM frontends deliberately set AR5=0 via `8815` at `d43b`
after clearing A at `d436`. `a0b8` at `d45b` uses it as a postincremented
operand address/counter. The staged stops at values 0 and then 2 therefore
identified missing operand domains, not an unexplained zero pointer. The
frontends already differ in other data values here; this observation is not
state or EFR-result parity.

## Square arithmetic in RPTBD delay words

Short SQURA/SQURS RAM forms may occupy either or both RPTBD delay words.
Each source address and pointer modifier is owned by Access, memory/T by
Read, and the selected accumulator with sticky overflow by Execute. The
second square sees the first square's result when they share a destination.
FRCT/OVM apply; square products do not use the MAC-family SMUL product clamp.
Mapped/peripheral sources, extended addressing, other control owners and
interrupt composition remain outside this qualification.

`rptbd-square` passes 147,456 cases and cleared-pipeline replays: SQURA/SQURS,
A/B destinations, first-only/second-only/both-same/both-split layouts,
DP/indirect/postincrement, eight patterns, FRCT/OVM/SMUL/SXM, counts 0/1/7,
two pages and four checkpoints. Per-clock accumulator/flag checks cover
sequential saturation. Retained opcode/source/address checks and a newer T
value reject resampling or T republication. Both accumulator results are
stored by every count+1 body iteration, while the delay work occurs once.
This qualifies the declared staged ownership model, not physical bus phases.

## BRC memory moves in ordinary delayed controls

Complete two-word MVMD/MVDK-to-BRC forms may occupy ordinary delayed
branch/call/return spans. Their existing source sample and zero-latency BRC
publication remain owned by Access. RPTBD's delay span and active block-body
BRC writes remain excluded; allowing an ordinary control delay does not
license modification of a block owner's count.

`move-brc-delay` passes 1,800 cases and save/load replays: BD, taken/untaken
BCD, CALLD, FCALLD and RETD; DP/postincrement and mapped AR/AL source forms;
counts 0/1/2/7/127; subsequent RPTB/RPTBD; two pages and three checkpoints.
Source poisoning cannot change the retained count. The resulting loops copy
exactly count+1 words, and call/return frames remain separately owned.
SP-relative sources, extended addressing, interrupt composition and silicon
pipeline timing are not qualified by this matrix. Existing ordinary-source
and block-body/RPTBD-delay exclusion fixtures remain separate controls.

## XC protection across a selected near return

TI [SPRU172C, XC description](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)
states that XC and the two following instruction words are uninterruptible,
including for XC 1. In the staged model, a selected plain RET in the first
following word redirects the remaining protection endpoint to its return
target. Leaving that endpoint at the abandoned sequential address would
retain protection indefinitely. A later ordinary status write must not be
misclassified as an XC-controlled instruction.

`xc-ret` passes 192 cases and cleared-pipeline replays: XC 1 BEQ/BNEQ,
selected/annulled returns, one-/two-word resumed instructions, two pages,
four checkpoints and none/IRQ/NMI. Interrupts are introduced only on selected
return paths, at fixture clock 7. The resumed write precedes interrupt
stack/vector effects; the original stack target is sampled once and poisoned
after Read. No-interrupt runs also execute a later ordinary STM ST1, beyond
the protected word's retirement. Adjacent-status admission, other return or
control-transfer forms and silicon interrupt timing remain unqualified.
This validates the declared resumed-word protection model, not an independent
measurement of the physical return pipeline.

Save/load fixtures must not queue synchronized input changes immediately
before `read_stream`: restoring scheduler timers does not restore the pending
`device_input` event queue. Saved IFR/NMI/entry state must not be followed by
a duplicate assertion of the original physical event.

## Staged successor memory copy

Short `DELAY Smem` (`4dxx`, low byte below `98`) retains its source address
and applies its pointer modifier in Access, samples one RAM word in Read,
and writes that retained word to source+1 in Execute. It does not modify
A/B, T/TRN or arithmetic flags. Compiler-mode DP/SP and compatibility ARP
selection use the existing address-age model. Mapped operands and a source
at `ffff` fail before reads, writes or pointer updates. LTD, longer address
forms, repeat/block/XC composition and physical bus arbitration remain
outside this qualification.

`delay-copy` passes 15,360 cases and replays over DP/SP, indirect,
postdecrement and postincrement, all eight AR selectors, eight patterns,
SXM/OVM, compatibility, two pages and three checkpoints. Post-Access pointer
poisoning and post-Read source poisoning cannot redirect or change the copy.
`delay-copy-ctrl` passes 73,728 cases and replays over BD, both BCD outcomes,
CALLD, FCALLD and RETD. Its four address forms exclude SP-relative data;
the control owner retains its independent stack transactions. This is the
declared staged operand-age model, not a physical pipeline timing claim.

## Accumulator-source multiply in control-delay words

Short MPYA/MASA/MACA/MACAR may run in BD/BCD/CALLD/FCALLD/RETD delay
words. They use the ordinary signed 17-bit A-high and signed Smem arithmetic;
memory publishes to T in Read and B publishes in Execute. The return/call
frame remains independently owned. Extended address forms remain guarded.

`maca-delay` passes 55,296 cases and save/load replays: six control paths,
four operations, DP/indirect/postincrement, eight input patterns,
FRCT/OVM/SMUL/SXM, two pages and three checkpoints. The two BCD paths seed
A=0/1 to select their outcomes; they do not independently vary A-high.
Checks cover A preservation, B publication, one memory sample, pointer update,
T poisoning after Read, balanced frames and target observation. This does
not qualify interrupt composition, accumulator-producer hazards or physical
pipeline phases.

## Shifted arithmetic in control-delay words

Two-word shifted Smem ADD/SUB may occupy the complete delay span of BD,
BCD, CALLD, FCALLD or RETD. Access owns the address and pointer update;
Read retains one memory operand; Execute consumes the selected accumulator
and publishes the result, carry and sticky overflow. Source and destination
accumulators are independently selected. The existing SXM/OVM rules apply.

`shift-alu-delay` passes 442,368 cases and save/load replays: six control
paths including both BCD outcomes, DP/indirect/postincrement addressing,
ADD/SUB, four source/destination pairs, all 32 signed shifts, four memory
values, SXM/OVM, two pages and three checkpoints. Extension and memory
poisoning check retained ownership; the target observes the result and
stack/pointer counts remain exact. `shift-delay-edge` retains 16 negative
cases for spill, store extension, write-only source and long source forms.
Interrupt composition and physical pipeline phases remain unvalidated.

## Overflow-only conditional branches

Staged BC/BCD admit AOV, ANOV, BOV and BNOV. Their predicate tests the
selected accumulator's sticky overflow flag and consumes that flag whether
or not the branch is taken (SPRU131G table 4-1). The other overflow flag,
carry, TC and DP are preserved. Delayed execution retains the decision;
subsequent flag writes must not cause another test or clear.
SPRU172C's BOV example retains the flag, contrary to the CPU guide's explicit
rule. This model follows the latter; these fixtures are not an independent
silicon resolution of that documentation conflict.

`ov-branch-pipe` passes 4,608 cases plus cleared-pipeline replay: both branch
forms, four predicates, all initial overflow pairs, seeded/SSBX/RSBX producers,
three gaps, two pages and four checkpoints. Post-Evaluate flag mutation and
delay-word updates cannot change the selected path. Literal and target-fetch
counters reject duplicate consumption, and no data writes or stack changes
occur. Arithmetic-producer hazards, interrupt composition and physical
pipeline phases remain unqualified. The existing atomic overflow, TC/carry,
signed-branch/call and delayed-arithmetic controls remain separate gates.

## Mapped dual-memory arithmetic operands

Dual ADD/SUB admit mapped sources `0008..0017` on either or both operand
lanes. Access retains both addresses and applies the existing X-priority
dual modifier; Read samples the resulting mapped-register values once.
Execute uses those retained words with the existing signedness, saturation,
carry and sticky-overflow arithmetic. A source that is also an updated AR
therefore observes that update in this model. Multiply/MAC and SQDST mapped
operands remain separately guarded.

`alu-map` passes 147,456 cases plus cleared-pipeline replay: sixteen mapped
addresses, X/Y/both lane placements, eight selector pairs (including aliases),
ADD/SUB, both accumulators, SXM/OVM, BK=0/32, two value patterns, two pages and
three checkpoints. The BK=32 fixtures use valid circular pointer geometry and
include wrap; BK=0 retains the declared linear behavior. Post-Read poisoning
of mapped or RAM sources cannot change the result or cause another pointer
update. Carry/overflow, unselected accumulator lanes, T/TRN and all ARs are
checked independently. `alu-map-edge` passes 3,552 cases plus replay over
74 unqualified low/control/peripheral addresses, with no reads, writes
or address updates. The prior arithmetic negative fixture now uses unowned
ST0 rather than newly admitted T; multiply/square negatives are unchanged.

This qualifies the declared operand-age/register/arithmetic model, not
physical bus arbitration, producer/control hazards or interrupt composition.
The acquired EFR encoder uses this contract for `a20a` mapped-AR3 subtraction.
Its complete vector acceptance is established by the separate codec gate,
not by this instruction matrix alone.

## READA in unconditional BD delay words

Short RAM-destination READA may occupy either BD delay word. Its existing
four-clock program-bus hold retains the program source after older ALU
publication, then retains the sampled word for the real RAM write. The
acquired decoder case is BD `ca2e`, ADD `ca30`, READA `ca31` (`7e40`).

`bd-read-a` passes 512 cases and cleared-pipeline replays: both delay slots,
DP/plain/postincrement/SP destinations, four boundary words, two caller
pages, two program-source pages and four snapshots spanning the held
transfer. The second-slot cases use an ordinary first-slot ADD to produce
the program address. Changing A after capture, changing AR3 after Read and
poisoning the program word after its sample cannot retarget or duplicate
the transfer. The destination update occurs once at Read; program sampling
and the RAM write retain their existing held-clock placement. Disabling
the BD admission fails the fixture. Physical timing, mapped destinations,
absolute/extended forms, interrupts and other control owners are not
qualified by this matrix and retain guards.

## Short repeat in accumulator-conditional BCD delay words

A short immediate RPT in the first BCD delay word owns its following
one-word body on either branch outcome. Its continuation is selected from
the branch's retained Read predicate: the encoded target when taken, the
word after the full delay span otherwise. Repeat Decode must not sample
the predicate early. BCD's empty prefetch slot also places the count header
in Read when a one-word body reaches Decode; that exact delay relationship
admits the Read-owned count, not arbitrary older repeat headers.

`bcd-rpt` passes 69,120 atomic/staged cases and cleared-pipeline replays:
twelve signed A/B predicates, zero/+1/-1/positive-guard patterns, STL/MVDD/NOP
bodies, counts 0/1/8/39/255, independent path counts 0/2/7, both directions,
two pages and four checkpoints. Staged A/B changes after predicate Read
reverse the condition without changing the selected path. Target/body
poisoning, execution observations, ordered copies, exact buffers and
preserved counters/status check that the delay repeat drains before a fresh
target or fallthrough repeat. `bcd-rpt-edge` adds twenty replayed rejections
of late headers, long/memory counts and spilling bodies without data,
pointer or counter effects. TC/carry/overflow/BIO predicates, conditional
calls, active outer blocks, arbitrary interrupts and silicon phases remain
unqualified for this repeat composition.

## Short repeat in CALLD delay words

Short immediate RPT in near CALLD's first delay word owns the following
one-word body before the retained call target runs. CALLD writes its normal
continuation frame once; repeating the delay body must not repeat that push.
The shared saved redirect mechanism resumes the target after the delay
repeat drains, and a target-side repeat has an independent count.

`calld-rpt` passes 5,760 cases and cleared-pipeline replays: atomic/staged,
STL/MVDD/NOP bodies, counts 0/1/8/39/255, target counts 0/2/7, forward/backward
calls, four patterns, two pages and four checkpoints. The target returns
with RET to the original caller; one frame write and one return read restore
SP. Target-literal poisoning, staged body-opcode poisoning, ordered accesses,
execution observations and buffer guards check continuation ownership.
Far calls, late-header placement, active outer blocks, arbitrary body/interrupt
composition and physical pipeline phases remain unqualified. The shared
exclusion fixture retains CALLD with RPT in the last delay word.

## Short repeat in RETD delay words

A short immediate RPT in RETD's first delay word retains the return target
already sampled by the return's stack Read. Its body drains before target
execution. Repeating the body must neither pop another return word nor
re-sample the stack location after it has changed. Existing return-prefetch
ownership is retained; the repeat uses the same saved continuation mechanism
as BD, not a new stack or firmware-specific target.

`retd-rpt` passes 5,760 cases and cleared-pipeline replays across atomic/staged
frontends, STL/MVDD/NOP bodies, counts 0/1/8/39/255, independent target counts
0/2/7, both return directions, four patterns, two pages and four checkpoints.
The original return word is read once, then poisoned; the target starts its
own fresh repeat only after delay writes finish. Tests check balanced SP,
ordered accesses, pointers, unrelated loop state, status and retained staged
body opcodes. The shared delay exclusion test retains a negative RETD case
with RPT in the second delay word. Near CALLD has its separate qualification
above; other unqualified owners remain guarded. Interrupt and physical
pipeline timing are not qualified here.

## Short repeat in RPTBD delay words

The staged model admits a short immediate RPT in the first RPTBD delay word,
followed by an already-qualified one-word repeat body. Its count-plus-one
copies complete once before the outer block runs. This does not install a
branch redirect: RPTBD retains its block controller rather than BD's target.
Discarding speculative outer-body prefetch rebuilds its remaining-iteration
prediction from BRC, including a single-iteration outer block. Otherwise a
correct store count can conceal an incorrectly retained BRAF.

`rptbd-rpt` passes 5,760 cases plus cleared-pipeline replay: atomic/staged,
STL/MVDD/NOP delay bodies, inner counts 0/1/8/39/255, outer counts 0/2/7,
one/two-word outer blocks, four patterns, two pages and four checkpoints.
Ordered read/write observations require the delay body to drain before outer
publication; counters, pointer updates, stack and final status are checked.
`rptbd-rpt-edge` passes twenty cases plus replay, rejecting late placement,
long/memory count forms and spilling store/copy bodies before data access or
short-repeat pointer/counter updates. RPTBD's preceding setup is retained.

TI SPRU131G sections 6.7/6.8 describe the two repeat controllers and RPTBD's
two delay words. The acquired EFR reset supplies the concrete `f272 c9c9
ec05 8392` composition. These bounded algorithm and restoration tests do not
establish silicon pipeline timing or interrupt composition. RETD has the
separate qualification above, as do near CALLD and accumulator-conditional
BCD. Other delayed owners remain guarded.

## Mapped memory logic sources

Basic and absolute Smem AND/OR/XOR admit owned mapped sources `0008..0017`.
Read retains the raw sixteen-bit word; Execute combines its zero extension
with the live destination accumulator. SXM/OVM do not change the logical
operand or result, and no status bits are written. Existing peripheral-logic
contracts are unchanged. Long-offset mapped sources and the remaining low
register/control addresses stay guarded.

`logic-map` passes 110,592 cases plus replay: AND/OR/XOR, sixteen mapped
sources, DP/absolute/plain-indirect forms, both accumulators, eight flag
states, four patterns, early/late accumulators, two pages and three snapshots.
The independent bitwise reference checks high accumulator bits, zero
extension, source/destination lane aliases and preservation of Execute-live
status. Mapped sources change after Read without retargeting or resampling.
`logic-map-edge` passes 1,152 cases plus replay over sixteen unqualified low
addresses, with no reads, writes or pointer changes. Both profiles clear the
live pipeline before load. Producer forwarding, wider control compositions
and physical pipeline timing remain outside this qualification.

The isolated ROM4 FR encoder and decoder now pass their complete bounded
32-frame comparisons and full-RAM replay on both frontends. The acceptance
scope and commands live in the [COBBA control boundary](cobba_control_boundary.md);
this does not establish a handset speech stream or native RF lifecycle.

## Mapped shift-16 ADD/SUB sources

Shift-16 Smem ADD/SUB admits the sixteen owned mapped sources `0008..0017`:
A/B lanes, T/TRN and AR0..7. The mapped word is retained in Read while the
independently selected input accumulator and carry state remain Execute-owned.
The existing arithmetic preserves partial carry: ADD only sets C on a carry,
and SUB only clears it on a borrow. Overflow remains sticky on the selected
destination, with OVM clamping. Memory sampling and arithmetic do not republish
the mapped source after it changes.

`shift16-map` passes 147,456 cases plus replay: eight ADD/SUB/input/output
forms, sixteen mapped sources, DP/absolute/plain-indirect addressing, eight
carry/SXM/OVM flag states, four input patterns, early/late input accumulators,
two pages and three checkpoints. An independent integer oracle checks the
shifted result and flags. The fixture changes the mapped source after Read,
including aliases with either accumulator, then checks the retained word
against the later live accumulator and carry. Pipeline state is cleared before
restoring each snapshot.

`shift16-map-edge` passes 7,680 cases plus replay over eighty unqualified
control/peripheral addresses, with no reads, writes or pointer updates. SP/BK
and other control registers remain outside this extension. These tests do not
establish arbitrary producer forwarding, repeat/interrupt compositions or
physical pipeline timing; in particular they do not inherit the separate
LDM/LD AR read-age exception.

## Conditional register and accumulator stores

The staged frontend admits STRCD, SRCCD and SACCD A/B (`9cxx..9fxx`, valid
signed accumulator conditions). Every instruction captures one Xmem address,
updates its pointer regardless of the predicate, reads RAM or a mapped
auxiliary register and writes once.
A false predicate writes back the retained Read sample, not the later live
memory value (SPRU131G 6.6.3; SPRU172C 4-152). SACCD obeys ASM/SXM but does
not use SST saturation.

The partial staged model captures address/DAGEN in Access and memory/ASM in
Read, then samples the predicate and source in Execute. SRCCD's Execute-owned
BRC is explicitly documented in SPRU131G 7-73; this qualification does not
establish silicon timing for every other predicate/source or producer pairing.
The shared store-word calculation is also used for an adjacent RAM bypass,
without replacing the actual write transaction.

`cond-store` passes 442,368 cases plus replay: four source forms, twelve A/B
predicates, six zero/sign/guard patterns, sixteen Xmem encodings, four ASM
shifts, four flag combinations, two pages and three snapshots. OVM/SST are
toggled together; the two BK values are coupled to source patterns. These are
not independent full cross-products. Late pointer and ASM changes check
retention; changing A/B/T/BRC after Read checks the declared Execute sampling
policy. Poisoning RAM after Read checks the false write-back path. Source
registers and status are preserved. `cond-store-edge` passes 1,024 cases plus
replay, rejecting accumulator, BRC and peripheral destinations before pointer
or bus effects.

`cond-store-ar` passes 221,184 cases and cleared-pipeline replays: four
forms, twelve predicates, six patterns, sixteen Xmem encodings, all eight
mapped AR destinations, two pages and three checkpoints. BK is zero;
ASM=0/-1 and SXM/OVM/SST are coupled to patterns rather than a full cross
product. The target may alias the addressing pointer: its Read sample then
contains the Access-stage modification, and its Execute write replaces that
register once. A false predicate writes the retained sample, not a newer
poisoned AR value. AR0 destinations also test signed AR0-based address steps.
Source/predicate values change before Execute, and non-target registers and
status remain unchanged except for the declared pointer update.

The acquired EFR block deliberately sets AR5 to `0016` at `d1a6` and uses
conditional store `9d3a` at `d1ad` to target mapped AR6. Atomic and staged
traces agree on that initialization, not on all preceding data values.
Control/peripheral register writes, mapped circular and arbitrary short-repeat/
interrupt/control compositions and physical bank contention remain unqualified.

## AR0-indexed MVDK sources

MVDK admits short `*ARx-0` / `*ARx+0` source modes (`71a8..71b7`).
The old source address is sampled before its linear AR0-indexed update and
before a potentially aliasing mapped destination is published. CMPT/ARP
selection and destination-specific publication use the established move
path. BK does not impose circular wrapping on these modes.

`mvdk-index` passes 294,336 cases plus replay, including 8,064 excluded source
geometries. It reuses the circular-move matrix with independently recomputed
linear pointer results: sixteen unique encodings, twenty-one destination
kinds, eighteen mapped sources, representative signed indices, two pages and
two/three checkpoints. Each accepted case samples one raw word, fetches one
destination literal and publishes once; accumulator lanes, T/TRN, DAGEN,
BRC and RAM destinations retain their separate ownership. The 19,712
`mvdk-index-edge` cases plus replay preserve control/peripheral exclusions.
Both profiles clear the live pipeline before restoring their snapshots.

Short RPT has a separate `mvdk-index-rpt` qualification: 8,928 cases plus
replay, all sixteen encodings, counts 0/1/5/15, six separated/overlapping RAM
placements, CMPT/CPL combinations, two pages and three checkpoints. Source
samples and pointer updates are fresh per copy; EAR advances from the one
retained destination literal. The reference preserves the existing staged
ordering in which a copy's Access sample precedes the previous copy's Read
publication. This is not independent silicon validation of overlapping-bus
ordering. 288 self-indexed source geometries that enter mapped space are
excluded from positive acceptance. `mvdk-irpt-edge` checks 480 first-access
mapped source/destination exclusions with zero transactions and replay after
clearing live pipeline state. Interrupt and wider control compositions remain
unqualified; no firmware-specific address participates in the implementation.

## Parallel store with ADD/SUB

The staged frontend admits `ST src,Ymem || ADD/SUB Xmem,dst` (`c000..c7ff`).
The two pointers resolve in Access, with X modification winning on aliases.
Read retains X and the store's ASM; Execute samples both accumulators, stores
the old selected source, then replaces the destination. ADD uses the opposite
accumulator plus the shifted memory operand. SUB reverses that order:
`(Xmem << 16) - opposite_accumulator` (SPRU172C 4-177/185). Fractional
accumulator bits must survive the subtraction; subtracting only high words
is not equivalent.

`st-par-alu` passes 540,672 cases plus replay: eight operation/source/destination
forms, 147,456 addressing cases and 393,216 numeric cases, two pages and three
snapshots. Independent integer expectations check bit-32 carry, sticky
destination overflow, OVM, shifted/saturated stores and preserved sibling
state. The fixture changes accumulators after Read and changes ASM/pointers
after their capture, poisons the sampled memory word, and checks one read and
one ordered store. Aliased source/destination stores must use the old value.
Clearing the live pipeline before each load verifies restored retained state.

`st-par-edge` passes 128 cases plus replay, excluding mapped X or Y operands
before either pointer changes or any data access. Short RPT remains guarded;
these tests do not qualify repeated arithmetic, arbitrary inter-copy alias
forwarding, physical bank contention or wider interrupt/control compositions.
The stage ownership is a partial model, not measured silicon timing.

## Circular ASM-shifted accumulator stores

ASM-shifted STL/STH (`84xx..87xx`) admit all four short circular modes
`c0..df`. They use the existing circular address generator with Decode-owned
BK, Read-owned pointer/AR0 and ASM, and Execute-live accumulator data. The
destination is retained after one pointer update. Register/peripheral
destinations remain rejected before circular or compatibility-ARP changes.

`asm-circular` passes 393,216 cases plus replay: low/high A/B selectors,
four circular modes, all eight encoded ARs, both CMPT states, four BK inputs,
four index cases, shifts -16/-1/0/15, SXM/SAT combinations, two pages and three
snapshots. Source patterns are coupled to the AR selection; this is not a
complete arithmetic-pattern cross-product. Independent modulo and shifted
store expectations check the one RAM publication and no destination read.
Live BK changes after Decode, pointers/ASM change after Read, and accumulator
data changes immediately before Execute. The saved pipeline must restore the
retained values after the live pipeline is cleared.

BK=0 retains the pre-existing linear behavior required by the atomic ROM4
control; the physical-silicon discrepancy remains unresolved. The fixtures
respect the documented nonzero-BK index bounds, including a large block for
self-indexed AR0. Wider producer, control and repeat compositions are not
qualified here. The stage ownership is a partial model, not measured bus
timing.

## XC 1 selecting the block-repeat endpoint

Conditional execution and interrupt protection have different extents:
XC 1 selects one following word, but XC and both following instruction words
are uninterruptible (SPRU172C 4-199). An XC 1 in the penultimate block word
therefore has an in-block conditional slot while its protection crosses the
repeat back-edge. The staged frontend retains the next iteration's first word
as the protection endpoint when BRC is nonzero, or the ordinary fall-through
word on the final iteration. It does not conditionally annul that extra word.
The existing saved XC endpoint field owns this continuation; no firmware PC
or new synthetic event participates.

`block-xc` passes 8,704 cases plus replay: seventeen predicates (including
OV/NOV), four layouts including the penultimate XC 1, counts 0/1/2/7, interior
and endpoint positions, two pages, four checkpoints and two source rotations.
Ordered reads, selected writes, per-iteration conditions, pointer effects,
BRC/BRAF and final fall-through remain checked. The eighteen `block-xc-edge`
cases plus replay retain guards when the *conditionally selected* span crosses
the endpoint: XC 1 at the endpoint, or XC 2 at/just before it. These are
model-scope exclusions, not silicon traps.

`block-xc-end-irq` passes 64 cases plus replay: a two-word XC 1/store loop,
counts 0/1/3/7, pending IRQ/NMI, two pages and four snapshots. Every repeated
store and the final protected continuation store must precede the one vector
fetch and stack write. The live pipeline is cleared before restoration so
protection cannot survive accidentally from the completed run. The existing
384-case short-loop IRQ/NMI control also passes. These establish the bounded
staged protection contract, not measured physical pipeline phases.

## Repeated parallel store/ADD/SUB

Short RPT admits the RAM parallel ADD/SUB families using fresh address and
Read stages for each copy. Execute stores the old live source accumulator,
then computes the selected result using the opposite accumulator and the
retained X sample. SUB remains memory-minus-opposite-accumulator. Carry
uses the 32-bit boundary; overflow is sticky and OVM clamps the result.
Store-side ASM/SXM/SST behavior is shared with the isolated instruction.

`st-alu-rpt` passes 30,720 cases and cleared-pipeline replays: eight forms,
fixed/incrementing/decrementing/aliased/AR0-strided layouts, four shifts,
SXM/SST/OVM combinations, counts 0/1/7/255, two pages and three checkpoints.
An independent integer reference checks per-clock A/B/carry/overflow
feedback, unchanged T, every old-source store, source/opcode retention and
exact pointer updates. The strided layout uses AR0=3 with linear BK=0;
arbitrary circular strides are not qualified here. `st-alu-rpt-edge` adds
128 replayed mapped-X/Y rejections without data or pointer effects.
Interrupt/control composition, inter-copy same-address forwarding and
physical bank phases remain unqualified.

## Repeated parallel store/load

Short RPT can retain the one-word `ST src,Ymem || LD Xmem,dst/T` body and
issue fresh Access/Read/Execute work for every copy. X and Y addresses are
captured before either pointer update; X modification wins when ARs alias.
Each copy samples X in Read, then stores the Execute-live old accumulator
before replacing an aliased destination accumulator. T loads publish in Read
and are not repeated at Execute. Store shift retains the existing Read-owned
ASM contract. Carry remains the single-instruction model's unvalidated
preserve policy, not a new silicon claim (SPRU172C 4-178/179).

`stld-repeat` passes 9,216 cases plus replay: all six A/B/T forms; fixed,
native `b9` postincrement, decrement/increment and same-AR X-priority layouts;
four shifts; SXM/SAT combinations; counts 0/1/7/255; two pages; and three
checkpoints across pending reads, stores and drain. A separate integer oracle
checks every accumulator/T transition and each RAM publication. Source and
opcode poisoning check retention; fixed-source samples change between copies.
The live pipeline is cleared before loading each snapshot.

`stld-rpt-edge` reuses the single-instruction mapped-leg fixtures under short
RPT: 96 cases plus replay reject X or Y mapped operands before data reads,
writes or pointer changes. The guard is a supported-model boundary, not a
silicon trap. Memory-bank contention, circular repeated addressing and
same-address inter-copy forwarding are not established by this qualification.

## Direct STM to absolute low RAM

`7760..777f` stores the retained immediate to the encoded absolute address
`0060..007f`, independent of DP, CPL, CMPT or SP. Access retains the address;
Execute writes once without a destination read or DAGEN early publication.
The pending literal participates in the existing RAM forwarding path, while
the actual write is preserved. These RAM forms share retained-literal
handling with plain indirect STM, not the early register-write path.

`stm-low-ram` passes 65,536 cases and cleared-pipeline replays: all 32
addresses, eight literals including opcode values, four DP settings,
four CPL/CMPT combinations, two pages and four checkpoints. An adjacent
LDM consumes the forwarded word without a physical read; with one NOP gap
it reads committed RAM once. Literal, pointer, DP/CPL and SP poisoning
checks retention and absolute addressing; exact consumer results, one
write, neighboring RAM, unrelated registers and continuation PC are checked.
This does not qualify arbitrary repeat/control/interrupt composition or
physical bank arbitration. The EFR `7767` at `b955` is this absolute-RAM
form, not a DP-relative write or an identified peripheral.

## Plain indirect STM to page-zero RAM

`STM #lk,*ARx` (`7780..7787`) resolves a page-zero address in Access,
independent of DP. CMPT with selector zero uses ARP; other selectors name
their own AR. The selected pointer's upper nine bits are cleared, as in
SPRU172C 4-176. The literal and destination are retained for one ordinary
RAM write at Execute, with no destination read. This is a partial staged
publication model, not measured silicon timing.

For example, the ROM4 encoder's `7782 b3a6` at `879c` uses AR2=`1268`
(ST0=`0024`, ST1=`0340`), writes `b3a6` to RAM `0068`, and leaves AR2=`0068`.
Atomic and staged entry registers agree there. Treating this as a write to
`1268` loses the MMR-addressing contract.

`stm-ind` passes 1,536 cases plus replay: eight selectors, both CMPT modes,
four literals, four RAM destinations spanning `0060..007f`, two program
pages, and checkpoints before/after address resolution and publication.
Literal poisoning and a subsequent pointer change verify retained operands;
neighboring RAM, unrelated registers and flags remain unchanged. The live
pipeline is cleared before loading each saved state.

`stm-ind-edge` passes the same case count/replay over destinations `0000`,
`0010`, `0028` and `005f`: these unqualified register/peripheral targets are
rejected before pointer changes or data transactions. This is a model-scope
guard, not a silicon trap. Modifying indirect modes, mapped-register timing
and control/repeat compositions are not qualified by these tests. These
profiles use plain RAM instead of the generic test map's synthetic endpoints
at `0060..0061`.

## Short repeat in BD's delay span

Short RPT in BD's first delay word owns the following one-word body before
control reaches the branch target. Starting a staged repeat discards target
prefetch, so the CPU retains the decoded target separately and resumes it only
after all repeated delay copies drain. Both continuation fields are reset and
saved. This is instruction-derived continuation state, not a firmware hook.

`bd-rpt` passes 5,760 cases plus replay on atomic and staged frontends: STL,
MVDD and NOP bodies; counts 0/1/8/39/255; independent target repeat counts
0/2/7; forward/backward targets; four data/status patterns; two pages; and four
checkpoints before, during or after the pending continuation. The selected
frontend's live pipeline is cleared before each load, so retained targets and
repeat metadata must actually be restored rather than survive from the future
run. It checks exact
buffer contents/guards, ordered transactions, pointers, preserved registers,
execution-owned body/header counts and final PC. The target literal is poisoned
after capture; body-opcode poisoning is staged-only because the atomic frontend
refetches repeat opcodes. Equal architectural outcomes are not a clock/bus
equivalence claim. The existing acquired-ROM atomic delayed-repeat fixture
provides separate BD/RETD corroboration.

`bd-rpt-edge` passes 32 rejection/replay cases: late header, BANZD,
late CALLD/RETD headers, long/memory counts and spilling store/copy bodies, over two pages and
two checkpoints. No rejected-repeat transaction or address-register/counter
change occurs. The enclosing CALLD's Decode stack reservation and RETD's one
return-word Read/SP increment legitimately precede rejection and are retained,
not undone. These are bounded model exclusions, not silicon traps. Other
control owners, active blocks, interrupt composition and physical bus phases
are not newly qualified.

## Repeated dual-memory MAC

MAC/MACR/MAS/MASR (`bxxx`) can use the existing per-copy pipeline issue path.
Each copy retains two RAM reads, publishes X to T in Read, and reads its
feedback accumulator in Execute. MPY/MACSU and parallel-store repeat families
are not admitted by this change.

`mac-rpt` passes 98,304 cases plus replay: four operations, all four source/
destination accumulator pairings, four operand/accumulator patterns, sixteen
FRCT/OVM/SMUL/SXM combinations, counts 0/1/7/255, two pages and three snapshots.
The four address layouts cover fixed pointers, the ROM's exact `b09a`
post-increment form, decrement/increment, and conflicting modifiers on an
aliased AR. Both aliased reads retain the original address and X's update wins
(SPRU131G 5.5.4). This is representative repeat addressing, not a fresh census
of all 4,096 non-repeated encodings.

The independent arithmetic expectation checks every accumulator/ST0 clock,
rounding, product clamp and destination saturation. Source words change after
each paired read, the opcode is poisoned after capture, and a newer T follows
the final Read. The gate requires exact paired reads, one opcode fetch, no CPU
data writes, exact final pointers/status and one fresh continuation. Initial
carry/overflow states vary with the four patterns, not independently.
`mac-rpt-edge` adds 512 rejection/replay cases across operations, accumulator
pairings, either low operand, four low addresses, two pages and two snapshots;
no reads, writes, pointer or T changes precede rejection. These exclusions are
bounded model guards, not silicon traps. Memory-count headers, nested blocks,
interrupt composition, physical RAM banks and bus phases are not newly
qualified by these gates.

## Repeated RAM square-accumulate

Short RAM-source SQURA/SQURS use the existing per-iteration pipeline issue
path. Every copy owns Access/Read/Execute: the memory sample publishes T in
Read and the retained sample is squared against the live Execute accumulator
(SPRU172C 4-163/4-164). FRCT scales the square, OVM controls saturation,
SMUL does not clamp it, and carry is unchanged.

`sqacc-repeat` passes 49,152 cases plus replay: add/subtract, A/B destinations,
DP/plain/post-increment RAM sources, eight input/accumulator patterns, sixteen
FRCT/SXM/OVM/SMUL combinations, counts 0/1/7/255, two program pages and four
snapshots. An independent expectation checks every accumulator/status clock.
Fixed-address inputs alternate after each read; post-increment inputs vary
by address and consumed words are poisoned. The body opcode is overwritten
after capture, and a newer T is installed after the final Read. It checks
one opcode fetch, exact reads, no CPU data writes, and final continuation.
Initial carry/overflow states vary with the eight patterns, not as independent
full-cross-product axes. Extended or mapped/peripheral repeats, interrupt and
nested-block composition are not newly qualified; physical bus phases remain
unvalidated. Non-repeated square arithmetic retains its separate matrix.
`sqacc-rpt-edge` adds 64 rejection/replay cases across all four operations,
mapped T/AR5 and two low peripheral/hole addresses, two pages and snapshots
before/after rejection. It checks zero source reads, CPU writes or changes to
accumulators, ST0, T and the source pointer. This is a bounded model
exclusion, not a claimed silicon trap.

## BANZD delay-owned T loads

One-word `LD Smem,T` can occupy either delay position of BANZD, using the
existing Read-owned T publication. `banzd-t-load` passes 9,216 cases plus
replay: counters 0/1/7, four source patterns, DP/SP and plain/post-increment/
post-decrement addressing, both delay positions, four SXM/OVM combinations,
A/B multiply consumers, two program pages and three snapshots.

Each selected target or fallthrough multiplies the loaded T by an independent
RAM operand. The fixture checks exactly one delay read, one multiply/store,
one source-pointer modification, the original-condition AR7 decrement,
unchanged BRC/status/stack and final PC. Source data, the load opcode and
the branch-target literal are poisoned after capture. Forward/backward target
layouts are paired with A/B destinations, not independently crossed.
The original RPTBD delay-load matrix retains its separate result. Other
control owners, extended loads, mapped/peripheral delay sources and interrupt
composition are not newly qualified; physical bus phases remain unvalidated.

## Memory-counted single repeat inside a block

RPT with a memory count can occur inside an active block when its complete
header and following body start remain inside the block endpoint. It retains
RPTC independently of BRC and uses the existing block-resume prediction after
the final repeated body. Endpoint-overlapping headers remain guarded;
long-immediate and nested-block setup exclusions are unchanged.

`rptb-memory` passes 3,840 cases plus replay, extending the existing short-RPT
fixture across DP, SP, plain indirect, post-increment and absolute sources.
It covers outer/base-inner counts 0/1/2/7, MVDD/ST/NOP bodies at the middle
or end, two program pages and four checkpoints. Post-increment sources
alternate base and base+1 counts across outer iterations; consumed words
are overwritten with `ffff`. It checks one fresh count read and pointer
update per outer iteration, exact ordered body transactions, independent
outer completion, and one final fallthrough. IRQ/NMI composition and physical
bus phases are not newly qualified. SPRU172C's RPTB description explicitly
permits single-instruction repeat loops inside block repeats.

## ADDM publication to T

Direct-DP and absolute `ADDM` can write mapped T (`000e`); unrelated low
registers retain their guards. `addm-t` passes 6,144 cases plus save/load
replay using the existing independent mapped-ADDM arithmetic matrix:
four source words, four immediates, sixteen SXM/OVM/carry/OVA combinations,
both CMPT modes, two program pages and three checkpoints. It checks one
retained Read, one Execute publication, unchanged A/B and address registers,
literal poisoning and a changed live T between Read and Execute.
Direct forms read/write at clocks 6/7, absolute forms at 7/8.

SPRU172C 4-9 specifies a non-repeatable instruction affecting C/OVA under
SXM/OVM. SPRU131G 7.5.6 classifies it as a store-type T writer: short
Category-I shift/bit-test consumers require a separating instruction;
other consumers have zero latency. This gate establishes publication, not
those consumer compositions, IRQ/delay interaction or physical bus phases.

## Repeated arithmetic accumulator shifts

Single-repeat SFTA reuses the ordinary instruction's arithmetic on each
Execute iteration, with a live source accumulator and retained opcode.
Unlike shifted LD, it updates carry for nonzero shifts and does not raise
new overflow for a zero shift (SPRU172C 4-155). The existing left-carry
formula/zero-shift policy remains explicit: the manual's left-shift example
and formula disagree, so these tests do not establish silicon behavior.

`rpt-sfta` passes 16,384 cases plus save/load replay: all four source/destination
selections, shifts -16 through 15, eight signed/guard-bit patterns, both SXM
and OVM values, and short repeat counts 0, 1, 7 and 255. The independent
expectation checks every arithmetic publication, carry and sticky overflow,
one accepted body fetch despite opcode poisoning, and one fresh continuation.
Both initial carry values and program pages are represented, but not a full
cross-product of those axes. Memory count sources, IRQ/NMI and nested-block
composition are not newly qualified for SFTA.

## Memory SQUR

`SQUR Smem,A/B` (`26xx/27xx`) squares the signed sixteen-bit memory word,
publishes that original word into T at Read, and writes the retained square
and overflow state at Execute (SPRU172C 4-161). FRCT doubles the product;
OVM controls positive overflow saturation. It does not consume an unrelated
old T or overwrite a newer T at retirement. The accumulator-A form remains
a distinct seventeen-bit operation that leaves T unchanged.

`squr-smem` passes 52,224 cases plus replay through the existing memory
multiply operand matrix: RAM and CPU-mapped sources, both destinations,
eight patterns and sixteen mode combinations. It checks one retained read,
address/literal ownership, later T poisoning, unselected accumulator and carry
state and no data writes. Program/memory are changed after capture.
Peripheral electrical timing, single-repeat and delay-slot SQUR composition
are not newly qualified. The original register-SQUR and multiply/T-source
gates retain their independent results.
The bounded memory multiply/bit disassembler matrix also covers both SQUR
destinations (2,304 forms over two program pages), including indirect and
extended-address syntax.

## Delayed forward block exits

BD and the twelve signed-accumulator BCD predicates can exit forward beyond
an active block when both delay words remain strictly inside its bounds.
The existing delayed-control engine retains the predicate before the delay
words and executes those words once on both outcomes. This admission does
not automatically clear architectural BRAF or consume the remaining BRC;
software can clear BRAF outside the block and revisit the old endpoint after
the documented spacing. Backward exits, endpoint-overlapping delay spans,
other predicate families and interrupt composition are not newly qualified.

`rptb-exit-delay` passes 9,984 cases plus replay: BD/twelve A/B predicates,
counts 0/1/2/7, first/middle/last/never exits, two endpoint layouts, two delay
load/store orders, two program pages and six checkpoints. A delay load
reverses the selected condition, so a re-evaluated rather than retained
decision fails. Ordered reads/writes, single delay effects, BRC/BRAF at exit,
explicit software clear and a spaced endpoint revisit are checked. Original
non-delayed forward exits and internal delayed branches retain their gates.

## RPTBD delay-owned T load

One-word `LD Smem,T` can occupy either RPTBD delay position while retaining
the ordinary Read-owned T publication. The admission is limited to direct
and short-indirect encodings below MOD 3; other control owners and extended
T-load delay forms are not newly admitted. The first body multiply must see
the loaded T, rather than an old value or a re-read of changed memory.

`rptbd-t-load` passes 9,216 cases plus replay: counts 0/1/7, four input
patterns, direct/plain/postincrement/postdecrement addressing, DP/SP modes,
both delay positions, four SXM/OVM combinations, A/B multiply destinations,
two program pages and three checkpoints. It verifies one delay read at
prototype clock 7 or 8, count+1 body products, single delay DAGEN update,
buffer sentinels and unselected state. Source memory and the fetched opcode
are poisoned after capture. Interrupt composition and physical stage timing
are not established.

## Repeated fixed-accumulator transfer

RPTZ's sequence-opening clear precedes the first MVMD read of the same
accumulator (SPRU172C 4-150). In the staged prototype, the matching AL/BL
source sample is retained after the older RPTZ Execute, not before it in
that same clock. Opposite-accumulator reads and ordinary direct-ALU producer
latency are unchanged. No result arithmetic or opcode timing is replaced
with a firmware-specific answer.

`mvmd-rptz` passes 1,536 cases plus replay: AL/BL sources, A/B clears,
counts 0/1/39/255, four seeds and SXM/OVM combinations, two pages and three
checkpoints. Matching clears yield zero on the first sample; opposite
sources retain their value. A deliberate mid-repeat source change must
affect only later copies. The unmodified core fails the retained-source
state check. The native FR decoder independently passes all 32 vector
frames and their full ordinary-RAM replay after this correction; this is
isolated algorithm acceptance, not handset speech.

`MVMD AL/BL, dmad` (`7308/730b`) can repeat into ordinary RAM. Each iteration
samples the same mapped source, retains that word, and writes the next EAR
destination. The original literal is fetched once, not incremented in program
memory. This uses the existing pipelined move stages (Access sample, Read
publication), not measured physical bus phases; other MMR sources and mapped
destinations retain their guards (SPRU172C 4-115).

`mvmd-repeat` passes 1,536 cases plus replay: counts 0/1/39/255, four source
patterns, four SXM/OVM combinations, short/long RPT, two pages and three
checkpoints. A deliberate source change halfway through the read sequence
must affect only later copies; destination and source guards, untouched ARs,
stack, T, other accumulator and status are checked. Post-fetch literal
poisoning and save/load must not cause re-fetch or duplicated accesses.

## Mapped-load AR source age

LDM and zero-shift LD/LDU from mapped AR0..AR7 retain their Read operand
before a younger instruction's same-clock Access update of that AR. Source
address selection, destination Execute and extension rules are unchanged:
LDM/LDU zero-extend, while LD sign-extends only under SXM (SPRU172C 4-73
for LDM; the retained LD/LDU contracts below apply to ordinary loads).
This is an age-order correction within the prototype clock, not a new
physical timing measurement. Other MMR sources retain their existing rules.

The ROM4 timer-descriptor producer uses the five-word block at
`a1af..a1b3`: LDM AR5,A; MAR *AR5+; STL B,*AR2+; READA *AR2+; LD #1,B.
The old ordering let MAR change the source of the older LDM, shifting the
program-table read by one word. Its first target became `0884` instead of
`0834`; the last target became `0299` instead of `0974`. Ordinary queue
consumption consequently lost a later `0000/0050` control. Retaining the
older AR sample restores both the target sequence and the shared RF-control
prefix, without firmware writes or transport changes. The initializer and
loop words are pinned by `rf_timer_table_generation` in the codec contract.

`ldm-ar-age` passes 2,304 cases plus replay: eight ARs, A/B destinations,
four values including sign/wrap boundaries, younger MAR increment/decrement
and MVMM writes, both SXM states, two pages and three checkpoints around
Read/Execute. It checks one Read at clock 5, retained zero-extended Execute
at 6, the younger update, other accumulator and status preservation, and no
duplicate bus accesses after restore. The unmodified core fails the Read
sample. General interrupt/stack/control-MMR compositions are not newly
qualified.

`mapped-load-age` adds 18,432 cases plus replay for LD/LDU through direct DP,
plain indirect, absolute and ordinary offset addressing. It covers the same
eight AR sources, A/B destinations, four values, three younger writers,
SXM states, pages and snapshots as the LDM matrix. One-word loads retain
Read/Execute at clocks 5/6; two-word forms retain 6/7. The unmodified ordinary
load path fails the old-register sample despite LDM already being corrected.

`shift16-load-age` adds 9,216 cases and cleared-pipeline replays for
`LD Smem,16,A/B`, covering the same four addressing forms, eight ARs,
four boundary values, younger MAR increment/decrement and MVMM, both SXM
states, two pages and three snapshots. It retains the older Read before
younger Access and applies sign extension/shift only to that retained word.
Disabling the shifted-load retention fails the source-read assertion.
The acquired example is `b99e` (`4413`) followed by parallel `e4d8`:
the load must retain AR3=2 before the younger instruction increments it,
otherwise the next parameter-array copy starts with 3. TS-based mapped
loads remain excluded; other ALU/dual-read families are not admitted by
this qualification.

A literal adjacency scan of the 65,535 acquired ROM4 program words found no
direct mapped LD/LDU followed immediately by MAR on the same AR. This is not
a control-flow or indirect-source census and does not establish that the
hazard is unreachable. The ordinary-load correction is ISA conformance, not
an independently observed additional handset blocker.

Apart from the separately qualified ordinary ADD/SUB matrix above, read-age
qualification is not inferred for RMW, other arithmetic/logic, multiply,
port-transfer or dual-read families. They retain their separate operand,
forwarding and publication rules; their same-clock AR-age compositions need
dedicated tests before a broader read-order change.

## ADDM BRC and interactive staged execution

Absolute ADDM admits BRC outside active block, single-repeat and delay
compositions. Read captures the source once and Execute publishes the
retained arithmetic result. This uses ADDM's existing SXM/OVM, carry and
overflow behavior (SPRU172C 4-9). Other low controls remain guarded.

`addm-brc` passes 3,072 cases plus save-state replay: four initial values,
four immediates, sixteen SXM/OVM/C/OVA combinations, two CMPT states,
two pages and three snapshots. The immediate and BRC are changed after
capture. Source Read at clock 7 and publication at 8 occur once; ARs and
accumulators remain unchanged. These are prototype stage assertions, not
measured physical phases.

SPRU131G 7.5.8.1/table 7-24 gives ADDM one intervening instruction before
RPTB[D], unlike the separately listed zero-latency STM/MVDK writers.
`addm-brc-loop` passes 320 cases plus replay: derived counts 0/1/3/7,
increments +1/-1, NOP/CALL separators, RPTB/RPTBD, two pages and five
snapshots. It checks one arithmetic publication, exactly count+1 body
iterations, final BRC/BRAF and balanced call frames. Zero-gap and active-loop
counter edits are not newly qualified. Existing mapped-AR ADDM/consumer,
TC-block, polling-interrupt, RCD and delayed-block exclusion profiles pass.

The native `0a40..0a5f` upload still binds uniquely to MCU offset `958e8`,
but its ADDM no longer stops execution. A ten-second clean cold checkpoint
has ILLEGAL clear, 22 host-consumer advances, 262 RF-control writes and 2,878
transfer expiries. Continuing CPU-issued control writes distinguish this from
the earlier frozen-counter polling failure; they do not establish valid RF
reception or registration. These counts replace the rejected reset-lifecycle
baseline, not an independently measured silicon timing contract.

`make verify-5110-menu NSE1_MACHINE=nse1pipe` performs a fresh own-model
EEPROM boot, presses/releases the physical Menu key at eight seconds and
requires the existing Phone book frame SHA-256
`d82cc6891fcf4efb0bd11ded583508f40826f58aa69463708a46897b76fdffb5`.
The staged wrapper also requires terminal ILLEGAL clear and no unsupported
form or reset. `make verify-5110-save-state NSE1_MACHINE=nse1pipe`
saves the running handset at seven seconds. It compares save-time
PC/ST0/ST1/SP, I/O `21`, BSP control `22` and emulated time exactly, then
requires an identical nonempty 160 ms stream of DSP I/O writes and timestamps
on the reference and restored branches. Trace overflow fails instead of
accepting a truncated comparison. It then opens the menu through physical
input. Its structural check also requires a clear DSP ILLEGAL flag after load. Default invocations
of both gates still test the atomic profile. This validates the running
idle/radio-service composition and subsequent menu input, not saves inside
native calls, SMS transactions or every application.
Both profiles use the native DSP backend, not `nokia_dsp_hle`. Both now pass
menu, late-input, exact save/replay and short/long physical power gates.
All enable reset tracing and explicitly fail on resets or DSP faults;
terminal ILLEGAL alone is insufficient. Native calls, registration and speech
remain unaccepted.

`NSE1_MACHINE` selects the frontend for menu, restoration, late-input and
power-lifecycle gates; the former `NSE1_MENU_MACHINE` remains a compatibility
alias.

Short postincrement immediate ST (`7690..7697`) now retains its old address
and updates AR in Access, presents the retained write address in Read, and
publishes its immediate in Execute. SPRU131G figure 7-1 separates those
events; SPRU172C 4-116 places MVMM's source read in Decode. Delaying the
store's AR update until Read made an adjacent `MVMM AR3,AR4` copy a pointer
one word short. Identical challenge buffers then produced a first bus write
at `1201` instead of `1202`, a wrong challenge response and MCU reason-4
reset. The corrected clean run continues with validator `r6=1` at 0.349379 s,
without that reset. This is a CPU addressing correction, not new provisioning.

`st-mvmm` passes 1,536 cases plus replay: one/two immediate stores, all eight
AR sources and destinations (including aliases), zero/one NOP gaps, two
pages and three snapshots around Decode/Access. It checks each original
store address/value and Execute clock, the one-time retained MVMM read/write,
and a following indirect load. The unmodified core fails at the source read.

An adjacent MVMM also consumes the pending short STL/STH postincrement
through an internal DAGEN bypass. The accumulator store retains its existing
Read-address/Execute-write placement; unrelated mapped loads and explicit
AR stores retain their separate latency contracts. The acquired case is
STL at `d444` followed by MVMM `d445` (`e734`): the copied AR3 is `1104`,
not `1103`. Reading the stale cell made a later copy start one word early.

`st-acc-mvmm` passes 24,576 cases and cleared-pipeline replays: STL/STH
from A/B, four boundary words, one/two stores, every AR source/destination
pair including aliases, zero/one NOP gap, two pages and three snapshots.
It checks the original store addresses and Execute write clocks, one
retained MVMM publication and the following indirect load. The adjacent
case bypasses the stale cell; a gapped case performs the ordinary mapped
read. Disabling only this bypass fails the matrix. Other modifiers,
mapped destinations, interrupts and physical pipeline timing are outside
this qualification.
These are prototype phase assertions, not measured silicon clocks; other
immediate-store forms and general interrupt/repeat composition are not newly
qualified.

The read-only `tools/c54x_rom4_challenge_observe.lua` records buffer writes
and context snapshots; fetch snapshots are not operand-capture proof. Use
`-autoboot_delay 0 -autoboot_script <absolute script path> -seconds_to_run 1`
on each frontend with separate freshly provisioned NVRAM directories.
It modifies no memory, retains tap handles, and reports write-trace truncation.

## Conditional delayed returns

The staged frontend admits the twelve signed A/B predicates and TC/NTC for
RCD outside active block bodies and other delay spans. The predicate is
retained at Read after older Execute publications. A taken return reads and
pops the stack once; an untaken return neither reads nor changes it. Both
paths execute the two retained delay words, with false conditions resuming
at PC+3. Delay-word values cannot change the captured decision. Carry,
BIO and overflow predicates are not admitted by this qualification.
This follows SPRU172C 4-133/4-134, reusing the retained RETD delay-word path.

`rcd` passes 4,480 cases plus save-state replay: fourteen predicates, eight
patterns including signed 32-/40-bit boundaries, two delay layouts, two
stack alignments, two pages and five snapshots. One layout changes the
tested accumulator or TC after the decision; the other contains a two-word
STM with an opcode-shaped literal. Stack memory is changed after its one
read to detect resampling. Ordered continuation stores observe the delay
results, and the fixture checks selected target, final registers/status,
balanced conditional pop, no untaken stack read and no repeated delay fetch.
The unchanged core rejects `fe42` at `0600` in the fixture. Existing
conditional/TC returns, BITT/RETD, load-return, interrupted STLM, polling-loop
and delayed TC-block profiles pass. General interrupt/block compositions
and physical bus phases remain unvalidated.

Native execution proceeds through `90eb` and another RF-control burst,
then reaches `6bf8 001a 0001` in uploaded program RAM at `0a4c`.
The 32-word live window `0a40..0a5f` binds uniquely to MCU offset `958e8`;
the following helper begins with RPTB at `0a52`. The canonical observer
captures this window at stop and the checker verifies its MCU binding.
Mapped ADDM to the block-repeat counter is now qualified above. The source
binding protects against mistaking corrupt bytes or another image's code
for a missing instruction.

## Delayed TC branches inside a block

TC/NTC delayed branches use the existing within-block target and complete
delay-span checks. Their predicate is retained at Read as for standalone
TC branches; a taken internal branch resets speculative fetch iteration
state without changing Decode-owned BRC. Calls and out-of-block delayed
targets are not newly admitted by this qualification.

`rptb-tc-delay` passes 512 cases plus save-state replays: TC/NTC,
counts 0/1/2/7, four source patterns, two delay layouts, two pages and
four snapshots. BITT produces TC from ordered, post-incremented RAM reads;
each source is changed after Read. One delay layout overwrites TC with
SSBX/RSBX and advances a pointer, while the other performs a two-word
accumulator addition. Selected-arm writes, retained branch decisions,
target-literal fetches, pointer updates, final status and exact loop counts
are checked. The old core rejects `fa20` at `0605` in this fixture.
Accumulator-condition, unconditional and BANZ block profiles, block-repeat
exclusions/interrupts, and the polling-loop interrupt profile remain green.
General TC-branch/block interrupt composition remains unvalidated.

The native three-second capture after this qualification has ILLEGAL clear and continues through
28 consumer advances. A longer cold run finds the next unsupported staged
form at `90eb`, `fe44` (`RCD ANEQ`), now separately qualified above.
The intermediate five-second capture is not the current acceptance endpoint.

## Delayed-branch polling interrupts

The staged CPU must not wait for every delay instruction to reach Execute
before recognizing a pending interrupt. In a short polling loop, the only
clock with the old control fully retired can coincide with the next branch's
partial Decode, starving interrupt recognition indefinitely. A synthetic
polling loop reproduces this with an enabled pending IRQ and no handler entry.

The qualified exception covers a retired delayed near branch with a complete
two-word absolute LD in Access, after discarded fall-through prefetch has
cleared. The decoded load drains ahead of INTR, following SPRU131G 6.10.7
and 7.2. Prefetch/Fetch instances at the same PC are not decoded control
owners. The retained control opcode is saved with the rest of the pipeline;
calls, returns and flag-writing delay instructions keep their existing guards.
This is a bounded composition, not a general interrupt-latency claim.

`poll-interrupt` passes 384 cases and save-state replays: IRQ/NMI, 32
arrival phases, two pages and three snapshots before assertion, after the
pin event settles and during entry. The handler stores the foreground
accumulator to prove that the delay load completed, updates the polled RAM
once, then returns. The test requires the branch target as the stacked
continuation, one acknowledgement, balanced SP and eventual loop exit.
Delayed-comparison, delayed far-call, delayed block-repeat, interrupted STLM,
status-mask, BITT/RETD and block/XC interrupt profiles also pass unchanged.
A broader guard relaxation failed comparison-publication and far-call-frame
tests and is not part of this contract.

In the native run, this releases the `4717` polling loop without changing
RAM, firmware inputs or peripheral timing. Execution reaches the delayed
TC branch inside block `473c..4777`, qualified above. This is not native
handset boot or RF acquisition.

## CMPM mapped-register source qualification

CMPM now admits T, TRN and AR0..AR7 (`000e..0017`) in addition to
the qualified accumulator slices. These are CPU-owned mapped registers,
not board RAM. The operand is captured at Read; TC is published at
Execute, and the source is not written back. Immediate and absolute-address
extensions are retained rather than fetched again after capture.

`cmpm-map` passes 69,120 cases and save-state replays across direct,
indirect and absolute addressing, both CPL settings, two pages, three
snapshots, eight source values, six literals, initial TC clear/set and
both TC/NTC branches. Sources and instruction extensions are changed
after capture to detect re-reads. The indirect pointer is distinct from
the source register. `cmpm-map-edge` passes 3,840 exclusion cases and
replays for controls `0000..0007` and `0018..005f`; those guards are an
implementation boundary, not a claim that silicon traps on those addresses.
The original accumulator, direct-memory and TC-call profiles also pass.

Fresh three- and thirty-second native captures after this change reach
PC `471c` and `471a`, respectively, with ILLEGAL clear. Both retain twelve
RF-control writes, 32 transfer expiries, 357 completion strobes, 74 mailbox
writes, 329 slot expiries, 10,400 RF reads and twenty-one consumer advances.
Frame expiries advance from 573 to 6,423. The terminal T=`5555`, BK=`0048`,
ST0/ST1=`081f/6103` and AR0..AR3=`126d/06b8/067a/0008` agree; A differs
between `1a00` and zero. Artifacts are
`tmp/cmpm-map-native{,-long}/error.log`.
The surrounding mask-ROM loop reads `06b7`, subtracts `*AR1` (`06b8`)
and branches to `4717` while equal. The delayed branch's two-word LD
reloads `06b7`. A passive read/write probe observes both words initialized
to `1a00` at PC contexts `4442/4443`, first at 1.853057865 s and again
at 1.993330865 s. Between those initializations, the frame-ISR path's
store at PC context `3363` advances `06b7` through `1a04/1a08/1a0c/1a10`
on successive frame periods. After the second initialization both words
remain `1a00` through three seconds, with no further writes. The loop is
actively reading equal words, not CPU IDLE or an unsupported-opcode halt.
Artifact: `tmp/cmpm-wait-native/error.log`, produced by the read-only
`tmp/cmpm_wait_probe.lua`.

The pre-correction terminal IFR=`0225` and IMR=`035f` include enabled pending frame
source 0; ST1=`6103` has INTM clear. Thus the next question is delivery
through the short delayed-branch loop, not a fabricated peripheral reply.
The delayed-control guard was responsible, as qualified above. This is an
important counterexample: timer advancement alone is not evidence of ongoing
firmware service or a healthy wait. These pre-correction captures are not
the current acceptance endpoint.
It does not establish upload completion, RF acquisition or handset boot.

## STLM suspension of an interrupted loop

Outside-body STLM to ST1 is admitted without delay/RPT/XC composition.
An Execute-owned BRAF clear retires the private loop predictor only when
no younger loop-body words remain in flight. Architectural BRC/RSA/REA
are preserved for software save/restore. In-body writes and overlapping
control compositions remain guarded; the six-word in-body BRAF latency
is not newly qualified.

`repeat-stlm` passes 3,840 cases and save-state replays: outer counts
0/1/2/7/56, inner counts 0/1/3/63, IRQ/NMI, two pages, two stack alignments,
FRETE/FRETED, six snapshots and callee returns inside the loop or at REA+1.
The program saves real interrupt state, computes the BRAF-clearing word
with LDM/AND/OR/STLM, runs its own loop, restores REA/RSA/BRC before ST1,
then returns through the interrupted callee. Ordered reads/writes,
one interrupt delivery, actual iteration counts and balanced frames are
checked. The unchanged core rejects this composition at `090a`.
The fixture separates STLM from the following STM because BRC's Read-owned
publication can precede the older status store's Execute in the same clock.
Primary constraints remain SPRU131G 6.10.6 and 7.5.7.1; physical phases and
arbitrary overlapping loop-controller writes remain unvalidated.

A read-only live-vector/stack probe identifies this prologue as the frame
ISR: vector `ffc0` contains delayed branch `f273 3204`, with ST0/ST1 saves
in its delay slots. The transfer interrupt uses a different vector (`ffc8`
to `3598`); RF activity alone does not identify the interrupt source.
At the pre-admission stop, SP=`1eaf` holds BK/REA/RSA/BRC=`0052/4dee/4deb/0003`.
Saved ST1/ST0 at `1ec1..1ec2` are `c903/081f`; the next stack words are
ISR return `408d`, callee return `4def`, and outer return `30d8`.
The interrupted callee was entered by CALLD at `4deb`, whose two-word
delay instruction ends at the loop endpoint `4dee`.
Thus the qualified composition is interrupt entry during an outside-body
callee, software suspension of the saved loop, and eventual restoration
through that callee's return. Merely admitting STLM is not sufficient.
The handler's `AND #2000,A; OR #4900,A` computes `4900` from saved `c903`,
clearing BRAF. The value now retires organically in the native run.

The same probe logs every changed write at consumer index `0853`, rather
than the canonical observer's first eight. All twenty-one observed writes
are nonzero, including ring wraps; the second initialization resets the
index before advance eight. This confirms the aggregate for this capture,
not twenty-one packets in one session. Probe evidence is local artifact
`tmp/st1-context-native/error.log`; neither probe changes firmware state.
The live checker retains the 0.5-second register/upload checkpoint and
the later service evidence; physical menu access is verified separately.

## MVDM accumulator slices

The staged Class-19A path admits MVDM destinations AL/AH/AG/BL/BH/BG.
The absolute source is retained once and the existing mapped-register write
merges only that slice; AG/BG retain eight bits, and SXM/OVM do not extend
or saturate a register copy (SPRU172C 4-110). The two-word slot publishes
at the existing Class-19A clock; this is not a measured bus-phase claim.

`mvdm-acc` passes 110,592 cases and save-state replays across six slices,
RAM/AR/BK sources, eight words, eight accumulator patterns, sixteen
SXM/OVM/CPL/FRCT combinations, two pages and three snapshots. The source
literal and data change after capture; an immediately following logical
consumer must preserve the merged accumulator. All neighboring slices,
status and unrelated accumulator remain checked. `mvdm-acc-edge` adds
2,064 replayed exclusions covering all 86 currently excluded mapped
sources in `0000..005f`, two pages and two snapshots, without data reads,
writes or accumulator/status changes. MVDM-to-accumulator repeats remain
guarded; arbitrary interrupt/repeat compositions are not newly qualified.
The unchanged core rejects the first accumulator case before source access.

## Direct Page-Zero Stack

PSHM/POPM use seven-bit direct MMR addresses independent of DP and CPL
(SPRU172C 4-132 and the existing Class-17A pop contract). The staged path
admits `0060..007f` through the existing data map, not synthesized values.
On the current NSE-1 board map these addresses use the data backing store;
this does not identify the physical function of every MAD2 register there.
Peripheral-page `0020..005f` and indirect encodings remain excluded.

`stack-page0` passes 73,728 cases and save-state replays: all 32 addresses,
eight words, four stack placements, two CPL values, two pages and three
snapshots. Push, pop, zero/one/three-gap pairs and consecutive pushes check
retained volatile sources, exactly-once writes and SP updates, unchanged
status/accumulators and source/stack overlap. The fixture's `0060/0061`
provider uses saved storage in this profile, rather than the separate
interrupt-trigger provider used by older profiles. `stack-page0-edge`
adds 1,536 replayed exclusions for 64 peripheral and 128 indirect encodings,
both instructions, two pages and two snapshots, with no data access or
stack allocation.

SPRU131G 7.3.2 requires same-location on-chip reads to consume a pending
write rather than stale memory. The consecutive-push overlap case fails
without forwarding (case 61952, second source `0061`, stale `5aa5` instead
of pending `0000`). The bounded implementation preserves the older stack
write and samples the younger source once after that same-clock publication,
as for the existing adjacent PSHM/POPM contract. This does not qualify all
memory arbitration or measured bus phases; indirect and arbitrary interrupt/
repeat compositions remain outside this gate.

## Parallel Store/Load

The staged frontend admits all six ST||LD forms: either accumulator as
store source, with A/B or T as load destination (1,536 encodings).
X/Y addresses and updates are Access-owned, using original pointers and
Xmod priority when the auxiliary registers alias. The X sample and ASM
are retained in Read. The store samples the old accumulator at Execute,
before replacing an aliased accumulator destination. Loading T publishes
in Read after older Execute writes; it is not republished at Execute.
The [mapped-source qualification](#mapped-source-in-parallel-storeload)
extends X reads below `0060`. Mapped/peripheral Y remains
excluded before pointer changes; single-repeat mapped sources are separately
excluded.

`stld` passes 405,504 cases and save-state replays: 110,592 address cases
and 294,912 numeric cases, including aliased pointers/memory/accumulators,
circular and linear modes, eight words, eight accumulator patterns,
all 32 ASM values across the address cases, eight numeric boundary shifts,
SXM/SST/OVM/carry combinations, two pages and three snapshots. Source memory,
ASM, accumulators, pointers and T change after their respective capture
points. `stld-edge` adds 96 replayed no-read/no-write/no-pointer-change
mapped-Y exclusions. The repeated-source exclusion retains
owned mapped X cases as well.
The existing parallel-MAC store-shift path and ordinary-RAM forwarding
are shared; no DSP reply or firmware state is supplied by this qualification.

Primary data semantics are TI SPRU172C 4-178/4-179; PMST.SST saturation is
specified by SPRU131G table 4-3, rather than assuming that OVM controls
store saturation. A remaining status ambiguity is explicit: SPRU172C lists
a C effect without an equation, while SPRU131G table 4-1's carry summary
does not name this form. The atomic model's carry-preservation policy is
retained and regression-tested, **not** promoted to a silicon contract.
Gate output requires `carry_policy=preserve_unvalidated`; native acceptance
reports `parallel_store_carry_claim=false`. Physical bus phases and arbitrary
interrupt/repeat compositions remain outside the standalone fixture scope.

## ADDC ownership

ADDC retains unsigned Smem in Read and samples its accumulator and carry
in Execute. Sign extension is suppressed regardless of SXM; OVM and
sticky OVA/OVB follow the arithmetic result (TI SPRU172C page 4-8).
Carry is defined at bit 32, not at the 40-bit guard boundary (SPRU131G
table 4-1). An input word of `ffff` plus carry must remain a 17-bit
increment, not wrap to zero before addition.

`addc` passes 110,592 cases and replays: 73,728 numeric cases over six RAM
addressing forms and 36,864 mapped cases over sixteen sources `0008..0017`.
Both accumulators, two carry inputs, SXM/OVM combinations, eight words and
eight accumulator patterns, direct/absolute/indirect mapped addressing,
two pages and three snapshots are covered. Source words and auxiliary
registers change after Read; half the cases also change the accumulator
and carry before Execute. `addc-chain` adds 4,096 replays of adjacent
ADDC instructions in AA/BA/AB/BB order, checking the preceding ALU's carry
and accumulator updates without a gap. `addc-edge` adds 1,920 no-read,
no-write exclusions for control/unowned mapped sources. Interrupt,
repeat/control composition and physical phase timing remain unqualified.
The native `07f8 000e` (ADDC T,B) at `4914` executes; no arithmetic result
is supplied by the observer.

## Unsigned Mapped Arithmetic

ADDS (`02xx/03xx`) adds unsigned Smem; SUBS (`0axx/0bxx`) subtracts
unsigned Smem. In particular, `0af8 000b` is SUBS from absolute BL, not
ADDS. Both suppress sign extension regardless of SXM and ignore input C;
output carry/no-borrow is taken at bit 32, with sticky OVA/OVB and OVM
saturation (SPRU172C 4-10/4-194; SPRU131G table 4-1).

The staged path admits mapped sources `0008..0017`: six accumulator
slices, T/TRN and eight auxiliary registers. Sources are actual mapped
reads retained in Read; the accumulator and arithmetic flags are
Execute-owned. SP/BK, control and peripheral sources remain excluded.
`unsigned-map` passes 221,184 cases and save-state replays, including
147,456 RAM and 73,728 mapped cases across both operations, both
accumulators, three mapped-address forms, six RAM forms, eight words,
eight accumulator patterns, SXM/OVM and initial-C combinations, late
accumulator/status changes, two pages and three snapshots. Source memory,
mapped words and pointers change after Read, including self-aliasing
accumulator sources. `unsigned-edge` adds 3,840 replayed exclusions for
80 addresses and DP/absolute/indirect forms without data access, writes
or pointer changes. The unchanged core rejects mapped ADDS AL (`0208`).
The fixture shares address/sample ownership with ADDC but uses operation-specific
add/subtract expectations. Native `0af8 000b` at `403e` executes
without an observer-supplied source/result; the RF/host counters do not
increase at this new endpoint. This does not qualify arbitrary
interrupt/repeat composition or measured bus phases.

## Circular MVDK qualification

Short circular sources `71c0..71df` retain one raw source word, update the
source pointer using the old BK/index, then publish the destination. Native
`71d2` uses a +1 circular step, not AR0. The `mvdk-circular` profile passes
294,336 cases and save-state replays, including 8,064 excluded source cases,
all 32 encodings, 21 destination kinds, 18 owned mapped sources and native
BK=82. `mvdk-circ-edge` passes 19,712 exclusion/replay cases with no data
reads or writes. Unsupported literal destinations reject in Decode;
unsupported resolved sources reject in Access. Physical phase timing and
interrupt/repeat composition remain unvalidated.

Successful, nonexcluded cases emit exact-word assertion attribution for
`71c0..71df`; excluded cases do not earn execution credit. The completion gate
requires `asserted_encodings=32` after checking that every encoding reached its
transfer assertion. This associates the existing source-sample, pointer-update,
destination-publication and replay tests with exact native words such as `71c2`.
It does not expand the operand matrix, alter CPU execution, or establish RF
sample encoding or acquisition. Reproduce with `tools/run_c54x_core.py
--profile mvdk-circular --verbose` and retain both its successful completion and
the run's `error.log` before using the assertion inventory in a coverage report.

The resident sequence `ec05 7192 13d0` at `7f8d..7f8f` is qualified:
`RPT #5; MVDK *AR2+,13d0`. TI SPRU172C page 4-108 specifies six
copies, incrementing the destination EAR as well as the source's explicit
postincrement. It becomes single-cycle after the repeat pipeline starts.
The `mvdk-repeat` gate passes 105,696 cases and save-state replays across
184 direct/short/circular source encodings, four counts, six disjoint/overlap
placements, both CMPT/CPL states, two program pages and three snapshots.
Its address oracle skips 288 self-indexed geometries that enter mapped
memory, rather than claiming them as RAM cases. `mvdk-rpt-edge` independently
passes 480 mapped-source/destination exclusion and replay cases. Mapped
repeat accesses, physical memory-bank timing and interrupt composition
remain unqualified. The overlap tests pin the existing staged order:
Access(n) samples before Read(n-1) publishes in the same clock; this is not
a silicon timing measurement.

The native sequence sets BRC=5 at `7ff3`, establishes the block with
`f072 7ff8` at `7ff5`, then calls `7ffa` with `f074 7ffa` at `7ff7`.
The call spans the entire two-word block. Its call/return and repeat-end
bookkeeping is qualified by the near-call profiles below.

## Near calls within block repeats

CALL/CALLD to an outside target is admitted only when the complete call and
any delay words fit inside the block. RET/RETD restore the caller's repeat
prediction without changing the ordinary one-word return frame: CALL pushes
PC+2, CALLD PC+4 (TI SPRU172C page 4-27). The existing 16-bit repeat-end
comparator, including its cross-page hazard, is unchanged.

`rptb-call` passes 3,456 cases and save-state replays: BRC 0/1/5/15,
middle/endpoint/whole-block calls, pages 0/2/127, one/two call depths,
IRQ/NMI controls, CALL/CALLD, RET/RETD and four save points. Ordered data
copies, frame balance, exact ordinary return addresses, delay effects,
final BRC/BRAF and a once-only post-loop sentinel are checked. The final
parking IDLE follows interrupt-drain space: a pending IRQ legitimately
wakes an adjacent IDLE (SPRU131G section 6.11.1), so that cannot itself
prove a failed return. Physical phase timing is not a silicon measurement.
`rptb-call-edge` passes 72 exclusion/replay cases for internal targets,
spilled delay words and a spilled target literal, with no data reads,
writes or stack allocation. The existing far-call exclusion profile now
uses an internal near-call target rather than rejecting all near calls.

## Memory-sourced repeat counts

`RPT Smem` captures RC once in Read, rather than interpreting its address
literal as a count. That retained value supplies the pipelined body's
remaining issues. The admitted mapped sources are `0008..0019`: A/B lanes,
T, TRN, AR0..AR7, SP and BK. Control/peripheral reads remain excluded.
Every new body replaces the previous opcode before Read can seed an issue
count, including non-pipelined bodies such as NOP. Otherwise a following
NOP repeat can issue stale copies and update their auxiliary registers.
The count-owner address comparison wraps the low 16-bit PC within XPC.

`rpt-mem-copy` passes 27,648 cases and replays: 24 addressing/source forms,
18 owned mapped sources, counts 0/1/7/31, MVDD/MVKD/MVDK bodies, two flag
states, two pages, IRQ/NMI controls, four snapshots and two raw-word
patterns. Each source is mutated after Read; body fetches and count reads
remain once-only, copies total RC+1, and interrupts follow the final copy.
`rpt-mem-wrap` adds 192 replays with an absolute header at `fffe..ffff`
and the body at `0000`. `rpt-mem-chain` adds 384 copy→memory-count-NOP
sequences and replays, checking four NOP retirements and no stale pointer
updates. The chained fixture fails without replacing non-pipelined body
ownership. Class-5A/5B's existing two held clocks remain partial stage
placement, not a measured physical timing claim.

The native `47f8 0008; e59c` sequence at `37f9..37fb` executes with the
firmware's own AL count. No count or packet is supplied by the observer.

## Staged dual-memory MAC family

MAC, MACR, MAS and MASR use the complete `Bxxx` encoding family, with
independent source/destination accumulators. TI SPRU172C pages 4-82 and
4-94 specify signed X/Y multiplication, Read-phase publication of X into T,
and optional rounding by adding `0x8000` and clearing the low sixteen bits.
The accumulator is sampled at Execute; SMUL product saturation precedes
addition/subtraction, and rounding precedes destination overflow handling.

The `mac-family` profile passes 1,720,320 cases and save-state replays,
covering all 4,096 encodings, aliased/distinct pointers, circular and legacy
BK=0 addressing, both program pages, all four source/destination pairs,
eight accumulator seeds, 64 numerical operand pairs and status combinations.
Its expected rounding uses signed floor division rather than the CPU's bit
mask implementation. Address/operand mutation and snapshots around Access,
Read and Execute verify retained operands and Execute-owned feedback.
`mac-family-edge` adds 128 exclusion/replay cases with no mapped reads or
register publication. Physical memory-bank timing and interrupt/repeat
composition remain unvalidated; this is not native handset acceptance.

## Staged parallel store/MAC

The next staged family is `Dxxx`: `ST src,Ymem || MAC[R]/MAS[R] Xmem,dst`.
SPRU172C pages 4-180/4-182 and TI's
[SPRU179B, pages 4-184/4-186](https://speech.di.uoa.gr/dsp/manuals/E18.pdf)
agree on separate store-source and arithmetic-destination selectors,
signed `T * Xmem`, optional rounding after accumulation, and storage of
the old source accumulator when source equals destination. SPRU131G
5.5.4 requires X to be read before an aliased Y write and gives X's
modifier priority when both operands select the same auxiliary register.
PMST.SST applies to the shifted store value (SPRU131G 4.1.2).

`d6e1` is specifically `ST B,*AR3 || MACR *AR4+0%,A`. Pipeline observations
are not retirement records: rejection in Decode can freeze older pending
slots. Admission alone does not prove retirement; isolated instruction
fixtures qualify results separately from bounded native execution.

The atomic implementation is not a complete status oracle: both TI
instruction manuals list C as affected for parallel stores, whereas the
current helper preserves it for ST/MAC. Their instruction descriptions do
not define that carry update. Resolve this discrepancy before claiming
full status fidelity; neither an atomic comparison nor a successful boot
can establish the missing rule. The staged prototype retains the atomic
model's carry-preservation hypothesis explicitly, not as a validated rule.
It captures the two addresses before X-priority pointer modification,
retains X and T in Read, retains ASM separately, and samples both
accumulators at Execute before writing the old source and replacing dst.
The store shares the ordinary pending-store forwarding mechanism, whose
adjacent-consumer composition is not qualified by the isolated fixture.
Adjacent ASM/SXM/FRCT/OVM writers, physical memory-bank timing and
interrupt/single-repeat composition remain unvalidated.

The `stmac` profile passes 1,720,320 cases and save-state replays over all
4,096 encodings, all source/destination pairs, 32 ASM shifts, distinct and
aliased addresses, both program pages, circular/legacy BK=0 modes, numerical
operand/accumulator patterns and status combinations. These are distributed
cohorts, not the Cartesian product of every dimension. Read owns one X
access and retained T; Execute owns one Y write and accumulator publication.
Mutating X, T, ASM, source pointers and feedback after their respective
sampling points verifies retention. Expected stores independently apply
SXM and SST to the shifted old source, including source/destination aliases.
`stmac-edge` passes 128 mapped-operand exclusions/replays with no bus writes,
reads or pointer updates. The acceptance marker explicitly leaves C
unvalidated rather than promoting the carry-preservation hypothesis.

## Staged square distance

SQDST uses all 256 `E2xx` encodings. TI SPRU172C page 4-160 specifies
that B accumulates the square of old A's signed bits 32..16, before A is
replaced by the X/Y difference shifted left sixteen bits. The implementation
retains both memory words in Read and samples old A/B at Execute. T is
neither an input nor a publication. The generic SMUL product clamp remains
the existing arithmetic model, not new evidence about the fitted MAD2 revision.

`sqdst` passes 9,216 addressing cases plus 393,216 numerical cases and
402,432 save-state replays. The cohorts cover all encodings, three pointer
layouts, circular/legacy BK=0 modes, both program pages, 64 operand pairs,
eight signed-17-bit A patterns, eight B patterns, 32 status combinations
and three snapshots. Independent expected results check subtraction carry,
separate sticky A/B overflow, OVM clamping, FRCT, and the existing SMUL model.
Post-Read accumulator mutation distinguishes Execute-owned feedback from
retained X/Y words; it also detects squaring A after replacing it. Sixteen
mapped-operand exclusion/replay cases reject before reads or publications.
Physical memory-bank timing and interrupt/single-repeat composition remain
unvalidated.

## Staged accumulator saturation

SAT A/B is admitted as a register-only Execute operation. TI SPRU172C,
page 4-154, specifies signed 32-bit saturation independently of OVM and
replacement of the selected accumulator's overflow flag. The `sat` core
profile verifies 3,072 cases and save-state replays: 16 signed 40-bit
patterns around both clamp boundaries, both accumulators, all initial
overflow-bit combinations, SXM/OVM combinations, two program pages and
three snapshots around adjacent Execute publications. A second adjacent
SAT clears the overflow from a preceding clamp. The other accumulator,
unrelated status bits and modes remain unchanged. This qualifies the
declared staged schedule, not measured silicon pipeline phases;
single-instruction repeat remains rejected by the existing repeat guard.

## Staged circular accumulator stores

Short STL/STH modes 8 through 11 use the existing circular address generator:
the old auxiliary-register address is retained at Read, its pointer is updated
once using the retained BK, and accumulator data is sampled at Execute.
TI SPRU131G section 5.5.3.4 defines the nonzero-block alignment, initial-index
and step-size constraints. The `store-circular` fixture reuses the independently
calculated circular-load address cases and the indexed-store transaction checks.
Its 786,432 cases and replays cover all four store selectors, eight auxiliary
registers, compatibility selection,
block sizes, boundary indices, signed steps, saturation modes, program pages
and snapshots before/after address and data publication. Low-page destinations
are rejected before pointer or bus effects; destinations are never read.

BK=0 retains the pre-existing linear-update behavior as an explicit legacy
exception. The manual describes suppressed circular modification instead;
this store extension does not resolve that fitted-silicon discrepancy or claim
measured physical pipeline timing. Single-instruction repeat is still excluded.

## Short mapped-AR ADDM

DP and plain indirect `ADDM #lk,Smem` may target AR0..AR7, in addition to
the separately qualified absolute mapped-AR path. The short forms retain
the source at Read and publish one arithmetic result at Execute; a newer
write to the destination cannot replace the retained operand. Compatibility
mode's ordinary ARP update remains visible for indirect addressing.

`addm-ar-short` passes 49,152 cases and cleared-pipeline replays: two forms,
eight registers, four values, four immediates, sixteen SXM/OVM/carry/overflow
configurations, both compatibility modes, two pages and three checkpoints.
It checks Read at clock 6, Write at clock 7, retained literal/source values,
one read/write and unchanged accumulators and unrelated ARs. The indirect
fixtures use a separate addressing pointer. Short modifying forms and other
mapped/peripheral destinations are not newly admitted; physical pipeline
timing and additional compositions are not inferred from this matrix.

The active-speech erasure bench organically reaches `6b84 0001` at `c38c`,
with AR4=`0010` selecting AR0 in both frontends. Its staged continuation
completes all 163 frames with the same PCM hash as atomic execution. The
recovery-policy reference differences remain characterized, not accepted as
conformance; the strict staged erasure comparison still rejects them.
`efr-loss-pipe` and `efr-erasure-pipe` load the same hashed inputs as their
atomic controls. Observation requires its own staged marker, full frame/RAM
replay and a valid driver; a completed characterization cannot satisfy the
strict conformance checker.

## BD long accumulator multiply

`bd-maca-long` qualifies stationary long-offset and absolute
MPYA/MASA/MACA/MACAR to B within BD's two delay words: 98,304 cases and
cleared-pipeline replays cover eight addressing ARs, three address forms,
four operations, eight numeric patterns, sixteen arithmetic-mode combinations,
two pages and four checkpoints. Smem and T are Read-owned; accumulator inputs
remain Execute-owned. `bd-maca-edge` rejects sixteen final-delay-word spill
cases and their replays. Other long addressing modes and control owners are
not newly admitted.

## Pending stack allocation and MVMM

A directly preceding PSHM's pending stack decrement participates in MVMM's
Decode view of SP when the destination is AR0..AR7. Actual stack allocation
remains at Read and the stack write remains at Execute; forwarding must not
allocate twice. `push-mvmm-sp` checks 2,304 cases and cleared-pipeline replays:
one/two pushes, AR2/ST0/ST1/SP source families, three gaps, eight destinations,
normal/wrapping stacks, two pages and three checkpoints, with exact bus events.
This does not qualify SP-to-SP composition or other push families.

The original EFR decoder wrapper reads its return address using MVMM SP,AR2
after PSHM AR2 and PSHM ST0. A stale SP selects a different wrapper despite
identical entry registers and 65,440 ordinary RAM words. Frame completion alone
therefore does not establish correct decoder execution.

## Absolute XORM

Staged `6af8` retains its absolute address, immediate and Read sample, publishes
one XOR result at Execute and leaves status unchanged. The same XOR result
feeds pending-write forwarding. `xorm-absolute` checks 4,096 cases and restores:
four addresses, 64 value/status combinations, compatibility modes, two pages
and four checkpoints. An adjacent load must consume the XOR result without a
second bus read; later literal, pointer and RAM changes cannot replace retained
operands. Other XORM addressing forms remain unqualified. These are emulator
phase contracts, not measured silicon timing.

## MVDM to T and TRN

Staged `720e/720f` use the existing Class-19A absolute-source transfer:
retain one address literal, read the source once and publish the encoded
mapped register at clock 6. This follows the MVDM operation in SPRU172C
4-110; the clock placement remains an emulator pipeline contract, not a
measured Lead bus phase. Source ownership and repeat exclusions are unchanged.

`mvdm-t` checks 12,288 cases and cleared-pipeline replays: T/TRN destinations,
RAM/AR4/AR1 sources, eight values, sixteen status combinations, zero/one-gap
mapped LDM consumers, two pages and four checkpoints. Literal changes after
Decode and source changes after Read cannot replace the retained transfer.
The consumer reads the new unsigned word; status, the opposite T/TRN register
and B remain unchanged. No additional mapped/peripheral sources, repeat or
interrupt compositions are qualified by this fixture.

## Staged LMS

`LMS Xmem,Ymem` (`e1xx`, TI SPRU172C page 4-80) retains both RAM samples
at Read and uses the live A/B accumulators at Execute. A adds SXM-extended
X shifted sixteen bits plus `8000`; B accumulates the signed X/Y product,
with FRCT and PMST.SMUL product saturation. A owns carry, and overflow flags
remain sticky and independently destination-owned. Neither calculation
publishes T. Address capture and X-priority alias modification use the
existing dual-operand path; low-page sources reject before either read or
pointer update.

`lms` passes 9,216 addressing cases plus 393,216 numerical cases, each
with cleared-pipeline replay. The fixture covers all 256 encodings, three
pointer layouts, linear/circular updates, 64 sample pairs, eight old-A and
eight old-B patterns, 32 arithmetic flag configurations and three snapshots.
It changes A/B after Read to distinguish Execute-owned feedback, poisons
source words and pointers after capture, and checks exact paired reads,
carry/overflow, no writes and unchanged newer T. `lms-edge` passes sixteen
rejection cases and replays without data or register effects. Physical bank
conflicts, measured silicon phases and repeat/interrupt compositions are
not established by these fixtures.

## Staged repeated T-based MAC

Short RAM `MAC/MACR/MAS/MASR Smem,A/B` bodies (`28xx..2fxx`, short
address byte below `98`) use the existing pipelined repeat issue/retirement
path. Each iteration retains its memory and T operands at Read, uses the
live destination accumulator at Execute, and does not publish T. Rounding,
fractional multiplication, PMST.SMUL product saturation, accumulator
saturation and sticky destination overflow retain ordinary-instruction semantics.

`tmac-repeat` passes 98,304 cases and cleared-pipeline replays: both
destinations, four operations, DP/plain/postincrement RAM forms, eight
operand/accumulator patterns, sixteen flag configurations, counts 0/1/7/255,
two program pages and four snapshots. It checks every accumulator publication,
one source read per iteration, a single retained opcode, source/opcode poisoning,
and preservation of a newer T value after the last Read. `tmac-rpt-edge`
passes 64 cases and replays rejecting mapped T/AR5, peripheral and low-page
edge sources before any data transaction or arithmetic/pointer/T change.
This is bounded instruction qualification, not measured silicon timing or
blanket acceptance of mapped, extended, interrupt or block-repeat compositions.

## Staged TC conditional calls

Nondelayed CC with TC/NTC uses the existing retained call transaction.
SPRU172C page 4-29 assigns condition byte `30` to TC and `20` to NTC:
`f930` is TC and `f920` is NTC. `tc-call` verifies 64 cases
and replays across both predicates, both initial TC values, two program
pages and four snapshots. After the decision, TC and accumulator values
are changed and the target literal is overwritten: only the retained
decision may allocate a frame, the target must stay retained and RET
must balance the single taken-call write. Untaken calls touch neither
SP nor stack memory. This fixture does not establish measured silicon
pipeline phases.

Delayed CCD TC/NTC (`fb30`/`fb20`) uses the same retained Read decision and
the existing delayed-call PC+4 frame. `tc-delay-call` passes 1,280 cases
and cleared-pipeline replays: both predicates and initial TC values, eight
unrelated accumulator patterns in either accumulator, two delay layouts,
two pages and five snapshots across decision/frame/delay/return state.
An organic SSBX/RSBX in either delay slot flips TC after evaluation; it
cannot change whether the call owns a frame or reaches its retained target.
Target/delay words and the return frame are poisoned after their samples.
The matrix checks both delay words once on both paths, no untaken stack
transaction, and one balanced taken call. Disabling the TC admission fails
the fixture. The acquired encoder uses `fb30` at `cd7f` during its first
DTX SID interval. Block-repeat compositions remain guarded and interrupt
compositions remain unqualified;
the former edge controls now use unadmitted compound bytes `24`/`34`
instead of the qualified `20`/`30`.

## Staged accumulator-lane comparisons

CMPM admits accumulator lanes `0008..000d` through the existing memory-read
path. SPRU172C page 4-33 defines a 16-bit equality test that replaces TC;
the staged implementation captures the operand at Read and publishes TC at
Execute. `cmpm-acc` verifies 6,912 cases and replays across all six lanes,
direct/indirect/absolute forms, DP/SP modes, four values, two literals, both
TC branch senses, both initial TC values, two program pages and three
snapshots. Accumulators are poisoned before Read and changed after it;
the literal is also overwritten after capture. Only one operand read occurs,
with no data write, and the branch consumes the retained comparison result.
Other mapped registers and peripherals remain outside this qualification;
the fixture does not claim measured physical pipeline phases.

## Repeated data-memory moves

Short MVKD destinations use real repeated pipeline slots, not the atomic
interpreter or a bulk memory copy. SPRU172C page 4-113 and SPRU131G section
6.7/table 6-17 specify count+1 transfers, an incrementing absolute source
address and single-cycle issue after setup. The fetched source literal is
retained; each injected slot owns its incremented 16-bit source, DAGEN
destination, Read value and Execute write. Resume skips both words of MVKD,
unlike the pre-existing one-word pipelined repeat bodies.

`rpt-mvkd` verifies 9,216 cases and replays across DP/SP and four short
indirect modes, eight auxiliary registers, counts 0/1/2/7, both immediate
count forms, distinct/equal/overlapping layouts, two program pages and four
snapshots. It checks consecutive writes, source progression, pointer updates,
one opcode/literal fetch and retention after source/literal poisoning.
Same-address reads inherit the existing pending-write bypass documented by
SPRU131G 7.3.2: forwarded iterations do not issue a second RAM read.
This differs from assuming that physical Read-before-Write ordering must
return the old memory word. The source and issue state survive restoration.

Mapped sources/destinations and long-offset/absolute Smem repeat forms
remain excluded. `rpt-mvkd-edge` checks 32 cases and replays of those exclusions
before bus or pointer effects. Interrupt composition and physical pipeline phases remain
unqualified for this new repeated form.

## Immediate accumulator-field publication

STM to AL/AH/AG/BL/BH/BG retains its literal through Decode and writes
the selected field at Execute, not on DAGEN's early-write path. The other
fields are preserved from the live accumulator at publication; AG/BG retain
only eight bits. The mapped-producer ordering rule from SPRU131G section
7.5.10 also covers this writer, so an adjacent MVDK samples after its mapped
write rather than treating it as a direct-ALU producer with setup latency.

`stm-acc` verifies 3,456 cases and replays across six fields, six literals,
four accumulator patterns, SXM/OVM combinations, two program pages and
three snapshots. It poisons the fetched literal, changes sibling fields
before Execute, checks every clock's accumulator state and follows the
write with a mapped MVDK read and RAM publication. Both writes and the
single read must occur once at their declared stages; flags and modes
are unchanged. The schedule remains a staged-core model, not a measured
silicon-phase claim.

## Long-address T/TRN stores

ST T/TRN admits absolute, signed-offset, premodified and circular long
Smem forms using the shared extended-store address path. SPRU172C page
4-167 specifies the source and additional address word; the modeled
destination is retained at Read while T/TRN remains Execute-live.
`st-t-long` verifies 6,144 cases and replays across both sources, four
values, all eight auxiliary registers, compatibility selection, RAM and
peripheral endpoints, two program pages and three snapshots. It poisons
the literal and changes auxiliary registers after address capture, and
changes T/TRN immediately before Execute. No destination read occurs;
peripheral writes retain the existing one-extra-clock model.

`st-t-long-edge` verifies 6,144 cases and replays that reject mapped
destinations before data effects, restoring any previewed long-address
pointer/ARP modification. `st-t-branch` verifies 32 cases and replays of
ordinary stores after both outcomes of nondelayed TC/NTC branches.
A draining conditional-branch control counter is not evidence that the
following instruction occupies a delay span. Actual delay spans, repeats,
XC and block-body compositions remain unqualified and explicitly excluded
for this new form. The schedule is not a measured silicon-phase claim.

## Unowned low-address bus reads

The CPU's internal map does not own data words `0002..0005`; accesses
delegate to the board data map. Shifted LD now preserves that dispatch
instead of rejecting every low-page address as an unsupported CPU register.
No register, return constant or peripheral response has been added.
`shift-unowned` verifies 122,880 cases and replays: all four addresses,
all 32 shifts, DP/SP/absolute/post-increment/offset forms, SXM/OVM states,
both accumulators, four volatile word patterns, two program pages and three
snapshots. It checks a single bus read, retained operands/literals/pointers
and Execute publication without the peripheral-region extra clock. This
qualifies bus delegation in the current model, not the physical identity,
electrical behavior or fitted-silicon timing of reserved register-map gaps.

The native `a5e8` chain loads data word `00f1`, adds word `00f2`, and
publishes AR3 through STLM `8813` at `a5ec`. Passive existing-access taps
show boot clearing both fields, then firmware storing `00f2=3` while
`00f1` remains zero. Both the staged and ordinary atomic profiles take this
initial path and make six early reads of board word `0003`, returning zero
under the existing board model. The atomic run later publishes `00f1=b736`
and `b71a`. Thus the zero base is not a staged-only AR3 derivation error;
it is not independent proof of real reserved-gap hardware behavior either.
The two profiles use different SWWSR policies, so this comparison is not
a timing oracle. Logged `pc_context` values are observation-time PCs, not
asserted instruction-retirement addresses.

The staged checker retains the ordered field writes/reads, six early gap reads
and their counts as regression evidence. Its result explicitly leaves
`physical_low_gap_identity_claim` false. The Lua taps issue no extra reads
of these fields or endpoints and never replace values.

## Circular PORTR and block composition

PORTR admits short circular modes 8/9/10/11 through the shared address
generator (SPRU131G 5.5.3.4). Access retains the old destination and updates
its selected pointer once; Read samples the retained port and Execute
publishes the sampled word. No data-destination read occurs. Unsupported
mapped destinations stop before pointer or I/O effects.

`portr-circular` verifies 98,304 cases and replays across all four modes,
eight registers, compatibility selection, block sizes and index boundaries,
four samples, two full-width port literals, mode flags, program pages and
three snapshots. Literal, pointer and volatile I/O values change after their
ownership stages, so re-sampling and duplicate side effects are observable.
BK=0 retains the declared legacy linear behavior, not a new silicon claim.

`portr-block` verifies 1,728 cases and replays of circular post-increment
inside RPTBD: counts 2/7/17, BK 1/7/16, every auxiliary register,
compatibility modes, two initial ring positions, two program pages and
three snapshots. A volatile test provider supplies distinct words; the
fixture checks two-word issue cadence, retained sample publication,
ring wrap, final BRC and unchanged unrelated registers. It does not supply
GSM packets or assert measured physical bus timing.

The current native run logs the first sixteen RF reads per initialization
pass, at modeled frames 30 and 28. Observation-time PCs
`324e/3254/325a/3260` repeat; these are not retirement addresses. Its final
provider counter is 10,336 reads, not an uninterrupted-run total: the logged
sample index restarts at one between passes. RF acquisition and handset
boot remain unclaimed.

## PORTR consumer forwarding

SPRU131G 7.3.2's same-location pending-write bypass applies to an ordinary
RAM destination populated by PORTR, not only to accumulator stores and memory
copies. The staged Read/Execute model retains the I/O word and forwards it
to an adjacent same-address consumer while preserving the real EB write.
It must not reread the port, forward to a different address, or extend this
rule to mapped/peripheral destinations without separate qualification.

The ROM's request dispatcher makes this load-use contract observable:
`35b9` reads the in-service mask into RAM, `35bb/35bc` loads and complements
it, and `35bd/35bf` reads and ANDs the incoming request. A stale RAM read can
cancel the request against its own mask and silently skip host commands.
Consequently, a live RF loop without an illegal instruction is not sufficient
evidence of functional boot or host delivery.

The `portr-bypass` fixture failed on stale RAM (`a55a` instead of an I/O
sample of zero) without forwarding. It passes 6,656 cases and replays:
6,144 LD/AND/OR/XOR consumer cases covering both accumulators, SXM states,
eight samples, zero/one/two gaps, aliased/non-aliased addresses, two program
pages and four snapshots; plus 512 versions of the native request-mask
sequence. The volatile endpoint is sampled once and the real memory write
still occurs once per PORTR. This qualifies the logical on-chip RAM contract,
not measured bank contention or peripheral forwarding.

Duration-matched ten-second cold controls separate frontend and SWWSR selection:

| Machine | Frontend | SWWSR | Host consumer advances | RF-control writes | Outcome |
|---|---|---|---|---|---|
| `noki5110` | atomic | legacy | 22 | 262 | Challenge accepted; ILLEGAL clear. |
| `nse1astd` | atomic | standard | 22 | 262 | Challenge accepted; ILLEGAL clear. |
| `nse1pipe` | staged | standard | 22 | 262 | Challenge accepted; separate physical Menu/lifecycle gates pass. |
| `nse1plg` | staged | legacy | 0 | 0 | Existing `7728` guard at `0f05`; not a hardware result. |

All three live controls end at 10.016666667 s from fresh own-model EEPROMs.
Their first 32 RF-control pairs match after ignoring instruction-PC context
and timestamps; so do the first eight logged host-consumer transitions. This
is bounded prefix agreement, not whole-session parity. Atomic/legacy,
atomic/standard and staged/standard respectively have 4,242/4,239/4,234 frame
expiries, 134,816/134,720/134,560 RF reads, and 2,889/2,885/2,878 transfer
expiries. Their endpoint mode cells differ: atomic has `mode_aa=0,mode_ac=4`,
staged has `mode_aa=1,mode_ac=0xc`. Neither those differences nor equal prefix
values establish which bus dialect or clock timing is physical. Wider
state/traffic agreement remains open. The clean trace includes first-header reads (`0a05` at
contexts `3871/387f`). The passive tap counts twenty-two nonzero consumer
advances but logs only the first eight. The eighth changes `0033` to `002f`;
consumer advances are not independently counted packets.
`make check-c54x-rom4-staged` generates fresh own-model EEPROM/NVRAM and
checks the reset-free ten-second capture against the reviewed structured
fixture `fixtures/c54x_rom4_staged_checkpoint.json`. The fixture is a
regression fingerprint, not a physical timing specification. The checker
requires all 390 records in order, unchanged non-time fields, ordered bounded
timestamps, the same half-second ownership epochs, MCU validator `r6=1`, no
reset/fault, and unique binding of all four observed uploads to their MCU
source offsets. It does not pin absolute sub-microsecond timestamps. The
machine identity, clear ILLEGAL flags and accepted MCU challenge are enforced
independently of the fingerprint, so re-banking data cannot promote a fault.
The four handset gates independently test physical input and restoration.
The first RF-control writes include two six-write bursts, `2813/0030` at
`4026`, then two more six-write bursts. Their transfer targets begin
`0834/0884/08d4/0924/0974`. Later controls include ascending
`2727..272d` writes at `4026` in the logged prefix. The observer
logs only the first 32 controls and first 256 transfer expiries; summaries
count all 262 controls and 2,878 expiries in this ten-second capture.
CPU ILLEGAL must remain clear. In stopped runs, device timer edges alone
are not further execution. RF-control
and transfer-expiry parity remain open; these are calibrated bench-device
events, not RF acquisition.

The four-second atomic coherent gate requires two full six-write descriptor
bursts followed by `2813/0030` at PC context `4028` (thirteen controls total),
plus the firmware queue producer. The five-write partial fingerprint is
rejected, not retained as another accepted profile. Its minimum-frame bound
is 800; the separate thirty-second scan check retains its exact longer-run
counts and reload/selector requirements.

