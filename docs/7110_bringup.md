# Nokia 7110 NSE-5 bring-up

## Current boundary

The native-core compatibility boundary is parked pending independent evidence,
not further identity guessing. Public searches for paired NSE-5 PMM/MSID
captures and COBBA serial-control register specifications have not supplied
the missing contract. This is a bounded search result, not proof that the data
does not exist. Gammu's [7110 protocol documentation](https://docs.gammu.org/protocol/n7110.html)
lists identification reads, but no paired PMM or register-5/6 interpretation;
service packages and basic-initialization records do not establish a handset's
original paired identity. Software work may resume on new independent evidence;
no hardware acquisition or collaborator response is a prerequisite for work
on other products.

NSE-5 v5.01 PPM C completes 228 alternating sparse-flash buffer handoffs and
waits at `0x432f96..0x432f9c` for the DSP-owned halfword at MCU `0x10002` to
change from `0xffff`. Graphical boot, firmware-owned button handling, SIM,
phonebook and registration are not validated. `make verify-7110-bootstrap`
protects this boundary without supplying a final result.

A bounded full-core compatibility trial with the recovered NSE-1 ROM4 program
and data images advances beyond this wait and issues LCD traffic using the
stock 7110 flash and product-local PMM. It does not establish the fitted 7110
mask-ROM identity or complete boot. The normal profile remains fail-closed.
The former display-controller prerequisite is now covered by SED1565 rather
than the inherited PCD8544 profile. The normal product selects SED1565
with a 96-by-65 panel window at segment 18; the DSP completion boundary remains
unchanged.

The full-transfer trial first exposes the DSP program-memory extent:
NSE-5 writes executable code above the backend's `0x2800` overlay limit.
A larger-bank trial removes execution of stale mask words. Preserving
COBBA's existing nominal ready input across writes then reaches DSP idle,
but the MCU rejects primitive `0x35` and stops with code 4. The transform and
stored-input provenance are reproduced independently; compatibility of those
inputs with the modeled chip identity remains unresolved. Graphical boot is unproved.
Neither fitted geometry nor physical COBBA status semantics is established. See
[the upload/read evidence](#diagnostic-publication-and-partial-code-block-transfer)
before revisiting queue loss or interpreting the resulting bad return as
a CPU opcode gap.

### Reproducible compatibility instrument

`nse5r4t` is an explicitly labeled research composition, not another supported
handset or an assertion about the fitted 7110 mask. It runs the acquired
NSE-1 program/data mask through the C54x backend against the unchanged NSE-5
flash and product-local PMM. It inherits the 7110 board/display/input wiring;
the normal `noki7110` configuration remains fail-closed.

After `make build`, reproduce the bounded observation with:

```sh
.venv/bin/python tools/nse5_rom4_compat_check.py mame/mame roms run_7110_rom4_repro
```

The runner hashes all four inputs, prepares a private ROM directory, removes
only this fixture's retained NVRAM, and captures six native panel images
through eight seconds. Its Lua observer reads MCU state and snapshots the
screen; it does not write firmware state or synthesize DSP results. The
banked model reproduces 238 mailbox writes and 1710 DSP completion strobes.
Version 4 is copied to `0x167038` by 0.5 seconds; the current peripheral model
produces word 0 equal to zero, not a hardcoded acceptance value. The final MCU
PC is `0x4e9510`, and the final native image is blank. This protects the
compatibility observation window, not UI startup, SIM or fitted-mask identity.
The backend retains its independently documented peripheral/timing models;
executing authentic mask bytes does not validate those models for NSE-5.

Use this instrument to find the first unmet application-start boundary after
the LCD initialization test. Keep its results separate from normal-product
acceptance; do not promote the mask or COBBA assumptions merely because it
advances farther.

The observer now retains its tap subscriptions for the whole run and validates
entry details at the executed verifier and service initializer as positive
controls. Read taps are observation-only; an unretained subscription can be
collected before the observation window ends, so truncated entry counts are
not valid absence evidence. The executed startup calls `0x3bb818` for nineteen
indexed contexts, with 424 context-wrapper entries at `0x4bc214` through eight
seconds. The call at `0x49fd64` to `0x3a2612` completes: all four callees
(`0x46bdd8`, `0x46bc16`, `0x3209e8`, `0x311eb0`) execute and subsequent context
releases occur. It is not a stalled initialization call.

Add `--menu` to the instrument command to press/release the physical row-0,
column-2 Menu switch at approximately 4.2/4.4 seconds. This does not change
firmware RAM or interrupt masks. The current composition initializes the
keypad at `0x474112` once and executes nine scans at `0x474004`, the last around
1.776 seconds. By one second, global IRQ mask `0x8e` leaves IRQ0 unmasked, but
column mask `0x3f` masks every matrix column and remains so through eight
seconds. The later Menu input produces no IRQ0 handler entry at `0x4740f0`,
no key-decoder entry at `0x4cfc5c`, and no repaint. This identifies a firmware-
owned input-disable lifecycle, not evidence that the verified physical matrix
needs another mapping or an injected navigation event.

### Application readiness and input enable

The persistent column-mask write is owned by the keypad software constructor
`0x4cfcf6`, called at `0x39391c` inside application entry `0x39378c`.
At approximately 0.683 seconds it sets byte `0x168990` to 1 and
`0x168994` to 0; its write at `0x4cfd40` masks column bits `0x1e`.
Those two state bytes retain their values through eight seconds. The flag
setter `0x4cfc18` and unmask/post helper `0x4cfbc4` do not execute during
that window. The mask therefore follows a firmware-owned startup branch,
not an unexplained device interrupt.

The application consumes events through `0x393518`/`0x3bbeb8`.
Reports `0x14`, `0x16`, `0x15`, `0x17` respectively set bits 0, 1, 2, 3
of readiness byte `0x16ab85`. The test at `0x3939d0` requires both the
low nibble of `0x17fed8` to equal 6 and readiness to equal `0x0f` before
entering `0x3939e6`. Subsequent branches explicitly clear column mask bits
`0x1e`, restore roller/slide enables, unmask IRQ7 and call the keypad flag
setter. These paths must remain firmware-owned.

The retained-tap compatibility run receives reports `0x17` and `0x16` at
approximately 0.688 seconds, but observes neither `0x14` nor `0x15`.
Readiness remains `0x0a` and application continuation halfword `0x1689e4`
is `0x0012` at the one-, two-, four- and eight-second samples.
There are three readiness-test entries and no completed-readiness entry.
This is an observed missing startup-report boundary, not proof that all
other startup conditions are satisfied or that mode `0x12` itself blocks
message processing.

Report `0x15` has a concrete subsystem checklist: `0x311eb0` accepts
inputs `0x0e..0x19` except `0x17`, sets one of eleven bytes at `0x16a2b4`, and calls
report stub `0x4c8bd0` only when every byte is nonzero. Input `0x64`
instead clears all eleven bytes. The run observes only that reset input,
from `0x3a2624`, and all eleven bytes remain zero at eight seconds.
Report stubs `0x4c8b94` (`0x14`) and `0x4c8bd0` (`0x15`) have zero
observed entries. Their queue-send primitive is `0x3bbe04`, not the
separate queue-send primitive `0x3bc3e8` used by keypad helper events.

The report-`0x14` call at `0x2dfb1c` is executable Thumb code interleaved
with strings and literal data. Disassembly must start at a known instruction
boundary (`0x2dfb10`), not decode the preceding embedded `VBAT Checks`
string as instructions. This call remains a live producer candidate.

### Second-phase task release

The twelve-byte task records begin at `0x25d3fc`. They identify entries
`0x0e..0x16`, `0x18`, `0x19` as the eleven subsystem tasks whose startup
completion calls feed `0x311eb0`. Task `0x17` is separate and has entry
`0x2df5b4`, containing the report-`0x14` producer. The first release phase
includes task `0x17`, but the eleven checklist tasks belong to the second
release phase at `0x49fddc..0x49fe1a`; that phase is not entered in the
compatibility run. Their absence is therefore not eleven independent
missing hardware responses.

The deciding predicate `0x469cc2` waits for bit 2 of byte `0x17fe15` to
clear, then returns 1 only if bit 6 is set. The runtime write-watch sees
initialization `00 -> 08 -> 48 -> c8` at approximately 0.695 seconds,
then `cc` at 0.701 seconds. Event `0xd8` reaches `0x311416` at
approximately 2.253 seconds while bit 2 remains set. Its busy-expiry branch
`0x311430` clears bit 6 (`0xcc -> 0x8c`) and then bit 2 (`-> 0x88`).
The predicate consequently returns zero and startup enters `0x49fe36`,
skipping the second-phase task release. At that decision bootstrap byte
`0x16702c` is 1, mode byte `0x16ab88` is 2, and startup state bytes
`0x16bc78..79` are `02 00`.

The preceding constructor at `0x3aef0c` runs once. It constructs task-3
messages with class/type `0x70`, command `0x13` and four data bytes,
followed by commands `0x14`, `0x15`, `0x16` with twelve, twenty and
twenty-four data bytes. These are allocated packet lengths, not timer IDs.
This ties the release failure to an observed startup transaction expiry;
the message construction alone does not prove DSP-ring delivery or identify
the correct response payload. `--verbose` enables existing device-boundary
traces in the compatibility runner to check that transport next.

### Self-test reply lost at a full firmware queue

The verbose run establishes transport delivery: the DSP consumes all four
stock `0x70` startup packets and publishes replies through the RX ring.
It publishes type `0x74`, payload `0d 00`, at approximately 0.701377 seconds.
The MCU consumes that ring entry and `0x469dbc` wraps it for task 2.
The self-test consumer `0x30e7a6` would cancel the outstanding timer,
clear busy bit 2, and interpret status bits 0/1 of the second payload byte;
zero preserves the release-result bit 6. That handler has no observed entry.

The queue-send failure is measured, not inferred from a silent handler:

- Task 2's numeric/pointer queue has capacity 12, with indices at
  `0x101acc`/`0x101acd`. One slot distinguishes full from empty.
- Eleven preceding type-`0x74` payloads `32 00 01 11` fill the queue
  before task 2 begins receiving at approximately 0.703905 seconds.
- The `0d 00` post at 0.701839 seconds sees producer 11, consumer 0.
  Its next producer would wrap to the consumer, so the queue is full.
- The return at `0x3bbe98`, qualified by the saved caller `0x469e0d`,
  has return-state register `r4 == 0` at 0.701853 seconds. The unqueued
  message still contains primitive `0x0d`, detail `0x00`.

Thus the executing DSP is not silent and a success-shaped reply is not
missing: it is rejected before the firmware consumer can see it. This does
not prove the candidate mask is fully compatible or that accepting this
reply would complete graphical boot. The repeated `0x32` publications may
reflect a peripheral-model defect or mask/product incompatibility; their
meaning and legitimate triggering conditions still need decoding.

The compatibility runner writes `startup_queue.json` with the captured
post geometry, modulo-full calculation and observed failures. Each queue
tap is capped at 64 details; lack of a later detail is not absence evidence.
The next question is which DSP instruction/peripheral condition produces
the repeated `32 00 01 11` reports before task 2 can drain them. Do not
filter these reports, enlarge the firmware queue, change task scheduling,
inject the self-test reply, cancel the expiry, or promote the candidate
DSP mask to manufacture successful startup.

### Diagnostic publication and partial code-block transfer

The passive DSP data-space watch at native word `0x08e4` captures the
publication PC and stack. The early `32 00 01 11` publications use packet
sender `0x37ce` (producer write at `0x3805`); the captured stack includes
`0x385f`, the return from the diagnostic builder called at `0x385d`.
That builder at `0x4aa5` writes packet prefix `0x3200`, length 4 and type
`0x74`. Its caller checks DSP word `0x0871`, the code-block request cell.
This ties the repeated reports to loader activity, not an idle heartbeat.
The meaning of the diagnostic reason remains unvalidated.

The MCU receives IRQ4 for both request `0x12` and request `1`. Handler
`0x433574` reads the latter at `0x4335da`; this is not a lost interrupt
edge. The former completes, clears request offset `0x0e2` and writes
status 4 at offset `0x0e4`. The latter performs a partial transfer:
at branch target `0x4336a6`, approximately 0.523229 seconds, the loader
has `0x028f` words remaining, chunk field `0x044c`, source `0x229224`
and destination `0x10260`. It writes status 2, which the DSP clears at
approximately 0.523258 seconds, while leaving request 1 outstanding.
Thus an uncleared request does not mean the MCU ignored its selector.

Reproduce with `tools/nse5_rom4_compat_check.py --verbose`; the observer
emits `nse5_compat_code_block` and `nse5_compat_dsp_publication` records.
The continuation signal is now identified: DSP routine `0x31f9` clears
status `0x0872`, then pulses data address `0x0029` bit 3 at PCs
`0x3200`/`0x3203`. The same bit is pulsed for the initial selector `0x12`
and selector 1. The passive observer records all three with the outstanding
request and status. TI's [SPRU038A, sections 3.5.3 and 3.5.4](https://www.ti.com/jp/lit/ug/spru038a/spru038a.pdf)
documents this address/bit as BSCR.HINT, a DSP-to-ARM interrupt output.
This is corroboration from related TI silicon, not proof of fitted MAD2
identity or every BSCR bit's semantics.

Replacing the request-word notification with this output completes selectors
1 and 2 and advances to 8 and 3 in the NSE-5 trial. It does **not** establish
boot: execution later reaches opcode `0x0363` in data-like words at `0x06f5`
and produces invalid RX-ring indices. The same change exposes the legitimate
`RCD ANEQ` opcode `0xfe44` at `0x90eb` on NSE-1. That instruction is now
implemented with executable true/false tests against TI SPRU172C's delayed
conditional-return contract. With the legacy relative slot timer, the HINT
trial produces a rapid loop involving port `0x0f` writes of 0 and `0x3a98`.
That does not justify suppressing zero or tuning the clock.

The backend now delivers service IRQ through BSCR.HINT transitions instead
of the request-write approximation. The complete-transfer execution and
counter/compare contracts below have focused acceptance and preserve NSE-1
boot/menu behavior. Do not clear requests, synthesize completions, suppress
diagnostics or alter timer values to obtain boot.

The expanded loader trial exposes two independently testable CPU omissions:
PSHM/POPM use seven-bit MMR addresses, and `BCD ALT` (`0xfa43`) tests signed
40-bit A before its delay slots. Executable fixtures cover high-MMR stack
round trips, balanced SP, true/false branches and cycle counts.

ROM readers `0x44dc` and scheduler writers `0x25f7`/`0x366c` support a
free-running port-`0x0d` counter and absolute port-`0x0f` compare hypothesis.
An experiment removes the rapid zero-compare loop. `BCD BLEQ` (`0xfa4f`)
at `0x0ad6` is implemented with negative/zero/positive signed-B fixtures,
delay-slot condition capture and cycle counts. With these instructions,
NSE-1 passes runtime coherence and its exact physical Menu frame. The RF
acceptance now pins the measured fresh-profile output sequence, rather than
the superseded zero-write boundary (see below). Counter equality ordering,
clock/control behavior and physical CTSI correspondence remain unvalidated
on silicon; the implemented model has controller conformance coverage.

On NSE-5, `--dsp-tail` captures the first entry below program `0x0800`:
`RETE` at `0x3620` returns to `0x2470`, branches to `0x2754`, then `RET`
at `0x2763` with SP `0x0854` pops zero and enters `0x0000`. The later
`0x0363` failure is execution of data, not evidence for a new opcode.
The return-stack trace and upload/read comparison identify an earlier cause:
the uploaded trampoline at `0x2470` branches to `0x2754`, which redirects
caller `0x45c4` to uploaded code at `0x282d`. The backend discards program
writes at or above `0x2800`, so this entry instead executes mask-ROM words.
It repeatedly calls `0x45c2`, shrinking SP from `0x1ec6` into shared HPI
storage. The final zero return is a consequence, not the original defect.

The DSP's `MVDP` at `0x31d6` writes `0x4a08` to `0x282d`; subsequent reads
return `0x2833` under the old geometry. An explicitly experimental `0x3000`
overlay extent makes all ten captured comparisons at `0x282d..0x2836`
match, removes this recursion and completes a nine-second observation run
without an unsupported opcode. The exact fitted RAM extent remains unproven.
TI [SPRU131G, chapter 3](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
documents model-dependent C54x memory maps; the `0x27ff` limit is not a
CPU-family-wide architectural limit.

The mask routine `0x4641..0x464a` waits for COBBA register D bits `0x000c`.
The device already supplies these as calibrated nominal inputs at reset;
NSE-5 reads `0x000c`, writes zero at 0.523968 s, then polls zero forever.
Thus the observed failure is erasure of an existing input by a configuration
write, not absence of a new DSP completion. A provisional input-retention
trial preserves those two bits while leaving other D bits as opaque storage.
This does not establish their physical ownership or ready latency. The
COBBA conformance fixture checks zero writes and preservation of other bits.

With retention plus the experimental loader/overlay changes, the DSP reaches
idle `0x408d`, emits regular completion strobes and encounters no unsupported
opcode during nine seconds. The MCU instead rejects primitive `0x35`:
`0x3af4d0` receives length `0x32`, which skips the length-`0x34` checksum path.
At `0x3af540`, flags byte `0x17fe15` is `0xcc` (bit 6 set). Payload byte
`+0x15` is `0x76`, selecting `0x3af5a2`; byte `+0x21` is `0xd0`, outside
the required `0x78..0x7f`, so `0x3af64e` marks it invalid. This reaches
system-stop code 4 at `0x4e94d6`, called from `0x3af6b0`, and loops at
`0x4e9510`. Both observed initializations return the same payload.

The concrete unresolved question is the compatibility of the stored data
with the modeled COBBA inputs and recovered mask. The staging and transform
audits below establish the observed arithmetic, not correct provisioning.
Identity-like field checks do not justify repairing the reply bytes.

The complete consumer spans `0x3af4d0..0x3af88c`; its first rejection is
not its whole contract. After structural validation it selects a result
from the reply selector and context, compares two short fields through
byte-comparison routine `0x4ee9b4`, and can compare the retained 24-byte
record read from logical storage address `0x20`. Later branches copy the
decoded blocks into context storage or write a transformed retained record
through `0x3af44c`, then publish a result through `0x3ae594`. None of these
later acceptance/storage branches is reached by the captured invalid reply.
This establishes a stored-data lifecycle, not a generic DSP-ready message;
the precise identity/lock semantics still require independent evidence.
The first structural check also accepts some nondecimal nibbles, so it
must not be labeled a strict decimal IMEI validator on that check alone.

Transport observation confirms the DSP publishes the bytes without a later
MCU mutation: the `0x74` packet has 52 payload bytes and returns through
mask caller `0x4bac`. At the first shared payload write, DSP PC `0x37fc`
has source AR1 `0x120c`, destination AR2 `0x088c` and base AR3 `0x1202`.
A bounded source-write tail records the transformation in
`0x7f7a..0x8012`; `0x8012` writes `0x2ad0` to D:`0x120c` before publication.
Consequently the invalid field predates transport and consumer decoding.
The original-input and CPU arithmetic audits below separate these layers
before assigning fault to PMM contents or incompatible mask data.
The input is a MCU-originated type-`0x70` packet containing
`16 18` followed by the 24-byte block beginning `81 84 b1 91`.
At 0.670170 s, MCU `0x432cd8` writes its first word to HPI `0x1004a`
from the packet buffer around `0x104384`; DSP `0x4b85` stages it into
D:`0x120e` at 0.672109 s. Transform entry `0x7f2d` then sees duplicate
blocks at `0x1202` and `0x120e`, and a second entry operates on `0x1208`.
AR1 `0xb0bc` at staging names a mask dispatch table, not an established
cryptographic key. The existing 5110 transform fixture validates one input
and loop path, not this 7110 input. Neither acquired file contains this
24-byte block verbatim because it is MCU-prepared, not a raw PMM slice.
The constructor `0x3aefcc` sets type `0x70`, primitive `0x16` and length
`0x18`. Copy routine `0x4ef178` first reads 24 bytes from RAM `0x157444`,
matching acquired PMM `0x46..0x5d`. Routine `0x3ae364` reads 12 bytes from
storage base `0x157424` through `0x4ebfbc`/`0x3fb854`, matching PMM
`0x26..0x31`. It duplicates them, multiplies adjacent byte pairs into
little-endian 16-bit products, reverses byte order, complements bit-reversed
bytes and XORs that mask into the block at `0x3ae400`.

`tools/nse5_pmm_stage.py` independently reproduces all 24 transmitted bytes
from those two acquired slices; its observed-packet unit fixture protects
the calculation. This is a read-only staging model, not an identity decoder
or provisioning generator. It rules out MCU preparation and transport
corruption for this block, but does not prove the stored block is valid for
the recovered DSP mask or that the DSP transform is correctly executed.
The DSP transform audit below closes the observed operation contract;
repairing these bytes or bypassing consumer validation remains inadmissible.
At transform entry the serial words at D:`0x1f0c..0x1f0d` are
`0x0016,0x0010`, the existing nominal COBBA inputs. Table preparation
produces `d1b4 5ffb 4ff0 2d7b 0f4c e1c3` at D:`0x13dc`; these are the
resident `0xb6df` words with the first two words XORed by those inputs.
This proves what the model used, not what the acquired handset used.

A bounded data-space write watch confirms the acquisition chain in both
initializations. The loader clears D:`0x1f0c..0x1f0d`; the builder briefly
writes `0053/414e`, then writes `0016/0000` and finally `0010` to the second
word at instruction tails `0x4af1`, `0x4af4` and `0x4afd`. The transform
therefore consumes the later COBBA-derived values, not the temporary pair.
Here “serial registers” means registers accessed over the serial-control
interface. Their use in the MSID/retained-data calculations does not alone
prove a unique physical chip serial number, and their earlier designation
as analog measurements does not establish electrical units. Register
semantics and the original paired input values remain separate unknowns.

A fresh CPU conformance run passes. The nine-second compatibility trace
contains 419 exact opcodes: 403 have focused assertions, one (`ec02`) is
executed-only, and 15 are absent from the fixture. Their first observed PCs
are outside the resident transform; this is not proof that all transform
operand/status combinations are correct. The read-only traces retain the
serial words and prepared table for a subsequent intermediate-state audit.
The NSE-1 Python profile helper is an encoder; using it with the reverse
schedule is not an independently verified decoder and must not be used to
diagnose a CPU error from a differing result.

The resident helper `0x7f7b..0x7f8b` is checked separately against word
arithmetic: it rotates the 32-bit pair addressed by AR2 right by 10 and
the pair addressed by AR3 right by 31. The pointers sometimes swap; they
are not assumed to address consecutive regions. A bounded trace of 128
calls, including the rejected `0x35` transaction, matches these operations.
`tools/nse5_transform_trace_check.py TRACE` rejects missing observations,
malformed operands or a mismatched result. This establishes that stage on
the observed operands, not the full codec or PMM/COBBA pairing.
The same check covers 128 calls to mixing helper `0x7fb1..0x7fe7`.
The trace resolves its three address-table-selected input pairs before
execution and reads its selected output pair afterward. A straight-line
integer model of the unextended XORs, 32-bit logical shifts and low-word
stores agrees on every observed call. Shift and XOR semantics are grounded
in TI [SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf), not in
the phone's expected identity values. Round sequencing, final reversal and
compatibility of the acquired provisioning with nominal COBBA inputs remain
distinct from these stage checks.

The completed arithmetic audit checks 128 rotations, 128 mixing calls,
110 nonlinear calls, 18 bit reversals and ten complete eleven-round loops.
For each complete transform, it records six original data words, the six
prepared table words, all twelve schedule words and six returned words.
An independent word-level model reproduces all ten transforms, including
both blocks of the rejected primitive `0x35`. Its prepared table is
`6521 4cda 4d33 3bc6 3342 2c9e`; using the raw table before its linear-mix
and reversal preparation is not an equivalent calculation.

This closes the observed transform arithmetic as a source of the rejection;
it does not prove fitted mask identity, valid provisioning or every CPU path.
The remaining input contract is whether the acquired PMM was prepared for
the nominal COBBA serial inputs used here. Establish the stored identity
record's format/reader and its provenance before deriving or configuring
any handset-specific serial value. No value inferred merely by making the
validator accept is admissible.
The MCU constructor `0x3aef0c` makes the storage mapping explicit:
command `0x14` reads twelve bytes at logical offset `0x14`; `0x15` reads
twelve at `0x00` followed by eight at `0x0c`; `0x16` reads twenty-four at
`0x20`, then applies the staging transform. The acquired record's payload
base is file offset `0x26`. `nse5_pmm_stage.py` reports all three requests,
and each matches the observed DSP TX packet. Twelve-byte length alone does
not identify the first record as an MSID: the live `0x15` path prepares the
different table `1962 ea23 537c 121a 2247 c2ea`, not the MSID helper's table.

The logical reader `0x3fb854..0x3fb898` bounds the request against `0x898`
and copies from fixed cache base `0x157424`; it does not search the flash
journal per request. A bounded write watch and entry observation at
`0x46cb84` capture the startup cache copy twice: source `0x5fa026`,
destination `0x157424`, length `0x898`, caller return `0x3faf87`, at
0.088319 and 0.755891 seconds. The first twelve cached bytes reproduce
the acquired file, with no intervening content substitution before the
requests. This excludes a cache-copy/address error, not an incorrect
upstream record-selection policy or incompatible stored content.

The loader at `0x3faec4` replays a sector-local write journal rather than
choosing one identity record. `tools/nse5_pmm_journal.py` independently decodes
the observed write-only path: 96 records terminate at file offset `0x1340`.
Later writes do not touch the first `0x38` logical bytes used by the three
requests. At the executed completion branch `0x3fafd8`, both full 2200-byte
cache snapshots exactly match independent replay (SHA-256
`7738e93a7cf8fd84e26709f42c05470da5859cc6b3be5f3a38e2af8733af4734`).
The decoder rejects deletion records rather than guessing their semantics;
none occurs in this sector. A later wall-clock snapshot is not equivalent:
live firmware settings writes already change the cache by 0.1 seconds.
This closes journal replay as a cause of the rejected command inputs, not
physical chip identity or validity of the stored provisioning.

```sh
.venv/bin/python tools/nse5_pmm_journal.py \
  'roms/noki7110/7110 virgin eeprom 005fa000.fls' \
  --trace run_7110_journal_complete/error.log
```

Nokia's [NSE-5 repair hints, page 13](https://www.manualslib.com/manual/2806569/Nokia-7110.html?page=13)
require re-establishing IMEI/SIMLOCK data after replacing COBBA or D301.
This independently supports a hardware/provisioning pairing requirement;
it supplies neither this dump's original serial words nor a register map.
The filename "virgin eeprom" is not evidence of compatibility with the
model's nominal serial. A paired COBBA observation, a provenance-backed
stored identity format, or an independently documented factory profile is
needed before changing that input.

### Manufacturer Release And Storage Layout

The acquired [Nokia NSE-5 v5.01 installer](https://archive.org/download/Nokia_DCT3_firmwares/nse5_mcu_5.01.exe)
contains independently decompressible gzip members; no installer execution is
required to inspect them. Its release-note member begins at archive offset
`0x70b9a9` and expands to 29931 bytes. The notes identify DSP software
`P30.4.107`, HP2.5-or-newer hardware, 4 MB flash, 512 KB SRAM and PMM release
14. These describe the release target, not a fitted mask-ROM dump.

Sections 7.2–7.4 distinguish MCU-only `nse5nx05.010` from
`nse5nx05.01_`, which includes Flash Data Initialization. They warn that the
EEPROM mover was removed and that older layouts require an intermediate
release. A successful archive hash therefore does not establish that an
independently acquired PMM tail has the correct lifecycle/layout for v5.01.

The installer contains record-stream members at `0x834a7` (3214792 expanded
bytes) and `0x55da10` (3280400 expanded bytes), plus PPM C at `0x2c9814`
(524864 expanded bytes). `tools/nse5_installer_audit.py` pins the installer
hash, bounds decompression, validates ordered non-overlapping records and
compares only supplied ranges; it does not fill gaps or emit replacement ROMs.
All 392 MCU records and 64 PPM C records exactly match the acquired flash.
The basic image repeats those MCU bytes and adds eight sparse 8192-byte
storage records. Its `0x5fa000` and `0x5fc000` records differ from the acquired
PMM in 4170 and 3 bytes respectively; other initialization ranges fall outside
the two acquired images. The three command-source slices at PMM `0x26..0x5d`
are erased in the basic image. It therefore supplies no paired identity for
the acquired provisioned records. Flash mismatch is excluded for these
members, but PMM validity/migration is not proved. Do not replace the
product-local PMM merely because another member advances boot or interpret
initialized defaults as a paired hardware identity.

```sh
.venv/bin/python tools/nse5_installer_audit.py \
  roms/archive-dct3-packages/nse5_mcu_5.01.exe \
  roms/noki7110/7110f501_ppmc.fls \
  'roms/noki7110/7110 virgin eeprom 005fa000.fls'
```

### Generated MSID Versus Stored Data

The complete primitive-`0x34` reply is distinct from rejected `0x35`:
`0000007400120100340e0082f4af7937041f6f93224c65ab`.
Its algorithm-`0x82` MSID is `82f4af7937041f6f93224c65ab`. The existing
decoder recovers three word groups `be4cf224 / 00160010 / a8a9aa46`,
exactly matching the independently captured DSP encoder input at
0.671833 seconds. The middle group therefore corroborates the nominal
serial representation used by this composition; it does not recover the
original handset's serial or validate the PMM. Both initializations agree.
`nse5_msid_reply_check.py TRACE` checks complete reply framing, prior matching
encoder observation and inverse agreement; six negative/positive unit
fixtures protect missing, malformed, late and disagreeing evidence.

[Gammu's documented 7110 service protocol](https://docs.gammu.org/protocol/n7110.html)
lists an external MSID response under `0xb5` separately from DSP-version and
COBBA information requests under `0xc8`. This supports keeping generated
service identity, stored PMM records and hardware observations distinct.
It supplies no paired MSID/PMM dump for the acquired handset. A generated
MSID from this model cannot be used as evidence for new hardware inputs.

### Loader trial regression boundary

`tools/c54x_rom4_timer_trace_check.py LOG` protects the observed NSE-1
idle cadence under the trial: 16 consecutive slot expiries coincide with
frame wraps and remain one 5000-quarter-symbol period apart. Both the
four-second instrumented trace and fresh 30-second trace pass; six unit
tests reject missing observations, sequence gaps, changed reload, phase
errors and the old one-tick storm. This is a read-only trace check, not
physical clock validation or complete counter conformance. It does not
exercise nonzero compares, equality ordering, reload changes, reset
retention or save-state restoration; the separate fixture below covers
those implemented-model cases. The default overlay remains `0x2800`;
only `nse5r4t` selects the research `0x3000` extent. The source boundary
test now protects both facts instead of requiring a hardcoded comparator.

`make check-c54x-rom4-counter-observe` runs a fresh isolated NSE-1 boot and
the cadence check above. Its read-only Lua observer joins short-interval
port-`0x0d` reads, tolerating one tick of quantization against elapsed
emulated time at the declared 13 MHz/12 rate. It also samples the
non-destructive register 128 times at 100-microsecond intervals after
0.25 seconds: firmware alone provides only five reads in this short boot.
The observed result is 133 reads, 130 checked pairs, 128 advancing pairs
and zero errors. Samples cross counter wraps but do not establish compare
or reset semantics. Long gaps and explicit counter/reload writes break
the comparison chain; no firmware or controller writes are synthesized.

`make check-c54x-rom4-compare` is separately classified as destructive
controller model conformance, never a boot fixture. It stops DSP instruction
execution with debugger state in a disposable process, directly writes
CTSI ports, and reads the saved expiry count. It verifies ahead/behind
absolute compares, equality scheduled next cycle, periodic repetition,
outside-period sentinel cancellation, reload-induced cancellation, and
counter/compare suspension and retention across the MAD2 DSP reset line.
An isolation tap rejects unexpected firmware writes to the tested ports.
The fixture exits without restoring or retaining its modified state.
It also saves a pending nonzero compare, runs a reference interval, loads
the state, and replays that interval. Counter position, expiry count and
emulated timestamp restore exactly; the replay produces the same expiry
count and counter phase within one tick. Pre-save and post-load notifiers
anchor the snapshots to completed state operations, not request time.
These cases pass the trial implementation, but do not establish physical
equality ordering or real-chip reset retention.

The banked backend uses BSCR.HINT-driven service IRQ and absolute
port-`0x0f` compare against the free-running port-`0x0d` counter. NSE-5's
larger overlay geometry remains a research configuration, not identification
of fitted silicon. Fresh acceptance passes DSP coherence, long RF cadence,
the exact 5110 menu, normal 7110 fail-closed bootstrap, 3210 baseline and
frontier, and the 3310/3330/3410 frontiers. All 1211 tool tests pass.

A fresh, isolated 30-second NSE-1 run starts receiver reads on frame 30,
finishes with 6499 frame expiries and 207040 reads, exactly
`32 * (6499 - 30 + 1)`. It emits these three port-`0x31/0x32` pairs:
`2a04/0006` at DSP `0xa240` twice, then `2813/0030` at `0x4028`.
It remains idle at `0x408d`, IFR zero, IMR `0x035f`, with no port-`0x38/0x39`
burst reads. A shorter instrumented run additionally emits `0041/0040`
at `0x3712`; exact command counts are not established as invariant.
The old assertion (zero port-`0x32` writes, first read on frame 29) is
superseded deliberately: the fresh-profile RF gate now requires frame 30
and exactly these three ordered pairs at their observed instruction tails.
It still rejects parallel burst activity, pending INT0, changed interrupt
mask and receiver cadence. This is an emulated-boundary oracle, not RF
electrical validation or network registration evidence.

The bounded, read-only `tools/c54x_rom4_rf_operand_observe.lua` now maps
these writes to resident firmware operands. Both calls enter `0xa22f`
with stacked return `0xa1d5` (the call at `0xa1d3`). The `PORTW` instruction
at `0xa23e` reads through AR5=`0x1921`, whose word is `0x0006`; its port-31
partner comes from the same table stream. Instruction `0x4025` instead
uses absolute Smem `0x0009`, the CPU's accumulator-high MMR: accumulator
`0x00302813` supplies the observed `0x2813/0x0030` pair through MMRs 8/9.
Reading backend data-array words 8/9 would not recover those CPU MMRs.
These are executed firmware table/accumulator outputs, not peer-injected
RF payloads. Their electrical meaning remains unvalidated.

Regenerating the EEPROM profile while retaining the same flash/SIM NVRAM
reproduces the fresh three-write sequence at 2.095882, 2.114346 and
2.129435 seconds. Reusing the persisted EEPROM instead reaches four
writes around 1.515--1.548 seconds, including `0x0041/0x0040` at
`0x3710` through AR3=`0x1923` (word `0x0040`). Regenerating the EEPROM
again removes that fourth write. Thus the fresh-versus-persisted profile
distinction explains this observed variation; it is not evidence that
adding observation taps changes DSP timing. Do not apply the fresh-profile
operand count to preserved-NVRAM runs. Cached instruction fetch increments
PC before its read callback, so these taps match PC=`address+1`, not
PC=`address`.
Do not substitute donor provisioning, alter validation or invent a COBBA
identity. Readiness remains `0x0a`, keypad columns remain masked and the LCD
is blank; DSP progress is not graphical boot or fitted-mask compatibility.

`--dsp-tail` emits bounded upload/helper/stack observations and
`program_upload.json`; comparisons are limited to captured writes and later
reads. The broad fetch tap is removed after the upload window to avoid
dominating runtime. Its records include extension words, not solely decoded
instruction boundaries.
Entry hooks are branch-target observations; an unobserved fallthrough-only
hook is not absence evidence.

## Display contract

The full-core trial emits 141 LCD commands and 2,592 data bytes (three
96-column, nine-page transfers). Initialization includes
`a6 a4 a3 a1 c0 22 81 35 2f e3 40 b0 10 00 af`. This command stream is
incompatible with the inherited PCD8544 decoder; its captured pixels are not
evidence of graphical boot. A 20-second trial ends without a soft reset and
with the C54x frame timer still running, but does not validate UI settlement.

Independent [physical 7110 display measurements](https://serdisplib.sourceforge.net/ser/sed1565.html)
identify an on-glass SED1565 controller and a 96-by-65 panel. Its 132-column
controller RAM exposes the panel from column 18, with nine pages and only one
visible bit in the final page. The firmware transfers start with `b0 11 02`,
selecting page zero and column `0x12`, independently corroborating the offset.
The primary
[Epson SED1565 datasheet, revision 1.2](https://serdisplib.sourceforge.net/ser/doc/sed1565.pdf)
defines the 132-by-65 RAM and serial interface. `patches/mame-sed1565.patch`
extends MAME's existing SED15xx family with a distinct command decoder and
serial interface. Existing SED1560 and PCD8544 behavior is unchanged.

`make verify-sed1565` runs 17 executable controller checks without phone
firmware: serial input, command arguments, segment/common directions, start
line, ninth page, invalid pages, column saturation, read-modify-write, display
modes, chip select and reset. Software reset preserves display RAM, display
enable and segment direction; the reset pin additionally restores those
control defaults. Analog contrast, supply/busy timing and the separate static
indicator output remain unmodeled. Controller conformance is not evidence of
a complete 7110 boot.

The passive Lua mirror uses the SED1565 grammar for this product and emits a
native screen snapshot alongside it for independent pixel comparison.

With the candidate ROM4 composition, all four captures in an eight-second
run matched native output pixel-for-pixel at 96 by 65. The frames contain
initialization patterns and then cleared RAM, not a graphical boot UI. The
run issued 141 commands and 2,592 data bytes, with zero soft resets and no
unsupported-controller commands. This establishes the display transport and
decoder for the observed stream, not application startup or mask-ROM identity.

The final sampled PC `0x49fff8` belongs to an interrupt-driven idle loop,
not the verifier wait. `0x49ffec` stores 1 to byte `0x168f04`; IRQ/FIQ
dispatcher entries `0x4cab50` and `0x4cac14` clear it at `0x4cab56` and
`0x4cac1a`. A read-only runtime write-watch observed all three writers.
`0x49fff6..0x49fffc` polls the flag, then returns to idle predicates when it
clears. A sampled PC in this range cannot alone establish a boot deadlock.
The next runtime question is which application-start/input lifecycle follows
the cleared LCD test, while keeping the mask-ROM compatibility assumption
explicit and separate from physical input claims.

The initial generic configuration published three calibrated values after
64 exchanges. Those values preceded completion of this product's upload and
are not evidence of successful 7110 verification. The product-local profile
now acknowledges ownership only; it does not publish a final verdict.

## Inputs

- `roms/noki7110/7110f501_ppmc.fls`: `0x390000` bytes,
  SHA-1 `53af8324919f455ba8199d2c05f7a921cfb811d5`.
- Product-local PMM `7110 virgin eeprom 005fa000.fls`: `0x6000` bytes,
  SHA-1 `8b4dd782fc9d1306268ba63124ee463ac646912b`, mapped at ARM `0x5fa000`.
  Its filename does not independently establish factory provenance.
- The legacy `dsp_prom`, `dsp_drom` and `dsp_pdrom` placeholders satisfy
  existing ROM declarations only. They are not executable 7110 mask ROMs.

## Loader contract

`0x432eae` reads a verifier descriptor through RAM `0x1670b0`. The descriptor
at ARM `0x22d904` is `0f00 0000 00d2 0700 00b4 0000`: 210 program words,
loaded at DSP P:`0x0f00`. The extracted big-endian program SHA-1 is
`caca7599d9ca1a7dddf2df37f32be4aacd420deb`. It is **not** the 223-word
NSM-3/NPE-3 verifier.

The initial remaining count is `0x0001:c800`. `0x432f2c..0x432f60` sends
227 blocks of 512 halfwords sampled every `0x20` bytes from ARM `0x200040`.
The terminal block contains 510 further samples and two `0xffff` terminators.
Ownership cells are MCU `0x100fe` and `0x10100`, alternating 114 times each.
After completion, `0x432f9e..0x432fa6` copies words 1 and 0 into the
bootstrap state at `0x16702c`, offsets `0x0c` and `0x0a` respectively.
`0x1670b0` is the descriptor-pointer slot, not the result structure.

GENSIO selection `0x25`, CCONT command/data at `0x2c`, read at `0x6c` and
status at `0x6d` operate through the existing controller contract in this
bounded run. No alternate wiring or serial-ready projection was required.

## Staged DSP program

The program sets PMST `0xffa8`, writes 4 through `MVDP` to P:`0xff87`,
reads that location into D:`0x0802`, then processes each input block through
P:`0x8023`. Its final checksum routine calls P:`0x802b`. Both routines occur
at those addresses in the recovered NSE-1 ROM4 program; their instruction
sequences also match local routines at P:`0x0fb7` and `0x0fbf` in this upload.
This corroborates a ROM4-compatible verifier ABI, not complete interchangeability
of the 5110 and 7110 DSP images.

TI's [C54x CPU reference, SPRU131G, table 4-3](https://www.ti.com/lit/pdf/spru131)
places `MP/MC` at PMST bit 6, not bit 7. For `0xffa8`, bit 6 is clear
(on-chip program ROM enabled), bit 5 is set (data RAM overlaid into program
space), and bit 3 is set (`DROM`). Consequently the generic CPU contract does
not require the write to P:`0xff87` to replace an enabled mask-ROM word.
The fixture's read-only upper program mapping is consistent with that CPU
contract. The remaining product-specific uncertainty is the fitted mask-ROM
contents/extent, not the meaning of these PMST bits.

The computed checksum is stored at D:`0x1f0e/0x1f0f`, separately from the
MCU-visible final result. P:`0x0f65` stores a COBBA-derived result to D:`0x0800`;
P:`0x0f67` reads P:`0xff87` into D:`0x0801`, then idles. A checksum alone is
therefore not a legitimate mailbox completion value. PMST-dependent mapping,
the P:`0xff87` write/read behavior and COBBA port-`0x2d` replies must be
established before any result is promoted into the handset model.

## Executed verifier fixture

`make verify-7110-verifier` executes the stock 210-word program with the
product-local 228-block flash stream. P:`0x8000..0xffff` is read-only recovered
NSE-1 ROM4 code, including both CRC routines and the version word at `0xff87`.
The fixture checks the source hash; no ROM4 code is transcribed into sources.
This is an explicit memory-map assumption, not proof of the fitted 7110 die.

All 228 blocks execute in order. With peripheral reads left unsupported the
program stops at COBBA port `0x2d`, after the checksum calculation. With the
existing COBBA model it reaches IDLE at P:`0x0f6b` and publishes:

- D:`0x0800`: `0x0000`, or `0x0016` under the register-F sensitivity fixture;
- D:`0x0801..0x0803`: `0x0004` in both cases;
- D:`0x1f0e/0x1f0f`: checksum `0xa98692ad`, unchanged by the peripheral input.

These are reproducible executable fixture results, not a measured handset
publication. The full handset remains fail-closed: the COBBA register-F
reset/read contract and product-specific PMST mapping are not promoted merely
because the sensitivity fixture finishes.

## Next question

### Immediate MCU consumer

The loader returns at `0x432fb8`. Its direct call at `0x49ff12` is followed
by a test of the caller's saved `r4`, not the loader's mailbox fields:
`0x49ff16..0x49ff1c` optionally calls `0x45c61c`, and `0x49ff20` calls
service initialization at `0x3bc2a8`. Thus the immediate caller does not
validate the copied word-0/word-1 pair as a checksum verdict. This does not
exclude later readers of the bootstrap structure or identify the correct
COBBA reply.

The result-store literal is at `0x4330b0` and contains `0x16702c`.
The descriptor-pointer literal at `0x433160` contains `0x1670b0`.
These are distinct objects. Four halfword-aligned literal occurrences of
`0x16702c` appear in the pinned image (`0x4330b0`, `0x4334e8`, `0x433754`,
`0x49ff30`); scanning every halfword for Thumb-1 PC-relative loads targeting
those occurrences yields 15 candidate loads. Code/data classification and
indirect or derived pointers remain outside that count.

Reproduce the candidate list with
`tools/find_literal_loads.py 0x16702c --rom roms/noki7110/7110f501_ppmc.fls --encoding big`.
The tool defaults to the older swap16 image format; omitting `--encoding big`
is not a valid absence test on this acquired FLS.

An additional literal at `0x45c33c` points directly to `0x167036`, the
word-0 result field. Its reader at `0x45bf8a..0x45bfb0` emits three bytes:
bits 8..11 plus `0x37`, bits 4..7 plus `0x30`, and bits 0..3 plus `0x30`,
then a zero terminator. This is a concrete formatting use of the returned
COBBA word, not a pass/fail comparison. The containing command selector and
the register-F hardware meaning have not yet been established; the formatting
alone does not justify assuming a zero reset value. The subtract-cascade
at `0x45bf50..0x45bf62` selects this reader for request `0x0d` to the
information handler starting at `0x45bf02`. No externally documented name
for that request has been established.

This is a bounded disassembly result from the pinned flash: the linear Thumb
scan found one direct `BL 0x432eae`. It is not an exhaustive indirect-call or
bootstrap-structure reader census. Decode the flash in big-endian Thumb
order without an additional halfword swap; swapping it again produces
plausible-looking but incorrect instructions.

Does primary hardware material or the firmware's consumers establish the
7110's COBBA register-F reset/read result and fitted mask-ROM contents used
above? The generic PMST ROM-enable semantics are established by TI, but a
matching verifier ABI alone does not identify the complete mask ROM. Answer
the product-specific questions before enabling the later service responder.
Display geometry, Navi Roller and slide wiring remain separate product
contracts, not inherited 3310 inputs.

## Primary hardware anchor

Nokia's NSE-5 System Module manual, Issue 1 (07/99), is available as a
[readable mirror preview](https://www.eserviceinfo.com/preview_html.php?fileid=5461&previewid=3023).
The locally acquired HTML is `roms/research/nse5/manuals/ch2sys-preview.html`;
it is a partial text rendition, not the complete PDF or legible schematics.

Page 2-7 identifies three roller interrupt inputs and an independent
slide-position interrupt. Its key inventory includes roller push, two soft
keys, Send/End, digits, star/hash and power. This supports a distinct physical
input profile; it does not establish matrix positions, roller phase ordering,
interrupt numbers or slide polarity. Those must be recovered before wiring
host input to the controller.

The block diagram identifies 512 KiB SRAM and 32-Mbit flash, consistent with
the current mapped capacities. The ASIC pin table identifies `GenDet` as a
slide input; the roller uses separate flex-pool pins. The register mux and
interrupt decode are still unknown. No 3310 Up/Down event or firmware-level
navigation message is an acceptable replacement for these physical inputs.

## UIF+ firmware contract

The stock image provides a three-phase input decoder, not a quadrature
Up/Down key substitute:

| Surface | Recovered contract |
| --- | --- |
| Phase sampler `0x473d14` | reads GPIO `0xf1` bit 1, `0xf2` bit 0 and `0xf3` bit 5 |
| Valid phases | `(1,0,0)` = 1; `(0,1,0)` = 2; `(0,0,1)` = 3 |
| Phase transition | `1 -> 3 -> 2 -> 1` produces `0x17`; reverse produces `0x18`; unchanged phase produces `0x5a` |
| State | previous phase at `0x168a30`; sampled bits at offsets 6..8; current phase at offset 9 |
| Dispatch `0x473f7a..0x473fa8` | `0x17` reaches `0x4cfb28`; `0x18` reaches `0x4cfb62` |
| Key-facing paths | handlers conditionally publish matching `0x17`/`0x18` values through `0x45c724` and `0x45c704` |
| Interrupt handling | caller masks IRQ7, samples GPIO, restores the mask and acknowledges status `0x80` at MAD2 `0x09` |

The slow sampler at `0x473a9c` actively drives/probes each GPIO pair and
collects six observations. Its classifier at `0x473de8` and restoration
routine at `0x473c80` must also be respected: returning a fixed one-hot
pattern at every GPIO read is not a complete electrical model. Invalid
patterns in the fast sampler retain the previous current-phase byte rather
than defining a fourth phase. Physical rotation direction, contact topology,
pulls and transition timing remain unvalidated.

The slow classifier's initialization record at `0x4ffbd0` copies 62 bytes
to `0x168a3c`, supplying eight records with eight-byte stride and six
significant observations per record. `tools/nse5_roller_contract.py` extracts
these from the hash-pinned flash. Observation order is B,C while driving A
low; A,C while driving B low; then A,B while driving C low. Here A/B/C are
respectively `0xf1` bit 1, `0xf2` bit 0 and `0xf3` bit 5.

| Record | Six sampled levels | Classifier effect |
| --- | --- | --- |
| 0 | 111010 | phase 1 |
| 1 | 111111 | retain previous phase |
| 2 | 101101 | phase 2 |
| 3 | 111111 | retain previous phase |
| 4 | 010111 | phase 3 |
| 5 | 111111 | retain previous phase |
| 6 | 000000 | retain previous phase 1/2/3; initial 0 -> 3 |
| 7 | 111111 | shadowed by record 1 |

The classifier selects the first exact match; no match also retains the
previous phase. Rows 0/2/4 override that history. This is a decoded sampling
contract, not a proven passive-contact schematic. In particular, the table
does not justify replacing all GPIO reads with a fixed phase pattern.

A passive closed-pair model reproduces all three distinct slow patterns:
phase 1 closes B-C, phase 2 closes A-C, and phase 3 closes A-B, with released
inputs high. Restoration drives the previous phase's isolated pin low. With
no rotation the fast sample is invalid and retains that phase; either new
closed pair produces the correct one-hot sample for the next phase. All
nine previous/current combinations are checked by
`tools/test_nse5_roller_contract.py`. This supports a contact-network model
without substituting firmware events. The restoration literals establish
`0xb1..0xb3` as the relevant release/drive controls: a set roller bit releases
that pin for sensing, a cleared bit drives its `0x31..0x33` latch. Phase 1
clears B1 bit 1 and latch 31 bit 1, then sets B2 bit 0 and B3 bit 5; phases
2/3 perform the corresponding permutation. The slow sampler follows the
same direction rule for each low-drive probe. The extractor checks all
seven restoration/sampler GPIO literals, preventing confusion between a
direction-bank write and an output-latch write. This does not establish
direction polarity for every UIF pin or the separate `0x70` bank.

The 7110 board now connects that network through UIF's generic input-sample
callback. A three-position wrapping `Navi Roller` host input selects the
mechanical closed pair, not a firmware key/event. Only the identified roller
bits are driven by this callback; other products retain the register-only
input behavior. Down/Up decrement/increment the contact position; these host
bindings do not establish physical clockwise/counterclockwise naming.
`make verify-7110-keypad-controller` now also exercises 18 low-drive probe
observations and all nine restored-drive previous/current combinations on
the compiled device through MMIO and physical input fields. Pull strength,
interrupt edge/mux behavior and firmware navigation remain unvalidated.
No IRQ7 source is synthesized by the position input yet.

The common IRQ7 dispatcher is `0x4caa04`. It takes one F1/F2/F3 snapshot,
compares F1 bit 7 against saved F1 at `0x168a32`, and calls the slide handler
at `0x4741c2` if changed. It then compares F1 bit 1, F2 bit 0 and F3 bit 5
against the saved roller samples at `0x168a32..34`, calling `0x473f4c` if
any differs. Both checks can run during one interrupt: slide is checked
first, not exclusively. The tail acknowledges MAD2 `0x09` bit `0x80`.
No separate source-ID register is read by this dispatcher.

The hash-pinned extractor scans all 1,867,775 halfword positions for classic
Thumb BL encodings and finds exactly one direct-call candidate for each
handler: slide at `0x4caa2c`, roller at `0x4caa60`. Both are independently
decoded in the common dispatcher. This scan does not close indirect calls,
ARM-mode calls or hardware enable-bit semantics. Its four dispatcher pool
literals are checked along with the seven sampling/restoration literals.

Register `0xef` is also load-bearing: initialization enables `0x04` and
`0x89`; the slide handler clears `0x04`, while roller handling clears
`0x89` during service and sets it again afterward. The association of those
three individual roller-enable bits with A/B/C, edge/level polarity and
masked-change retention remains unresolved. Do not infer it from bit order
or fabricate IRQ7 merely because a host position changed.

Separate readers at `0x4741b2` and `0x4741ec` invert GPIO `0xf1` bit 7.
The latter stores the resulting logical state and selects two software
continuations. Nokia identifies a separate slide input, but the physical
open/closed polarity and pin-mux association must still be confirmed; do not
infer them from the inversion alone. The nearby control path at `0x4741c2`
also masks/acknowledges IRQ7. Its coexistence with roller handling requires
an aggregate input interrupt model, not two independently clearing sources.

No post-bootstrap application-input acceptance run is possible at the current
fail-closed frontier. Roller pin sampling has controller conformance coverage;
the slide has no physical input wiring yet, and neither has IRQ7 acceptance.

### Keypad matrix

The conventional keypad scanner at `0x474004` iterates five rows
(`0x4740aa` compares against 5), using row signal `0x28`, direction `0xa8`
and column input `0x2a`. It scans column bits 1..4 and returns `row * 5 +
column`; bit 0 is not an ordinary scanned key. Decoder `0x4cfc5c` selects a
25-byte normal map at `0x28dcd8` or a five-byte special map at `0x28dcf4`,
indexed by byte `0x168995`. The established map is index zero; additional
product map indices have not been validated.

| Row | Column 1 | Column 2 | Column 3 | Column 4 |
| --- | --- | --- | --- | --- |
| 0 | Send (`0x0e`) | left softkey (`0x19`) | right softkey (`0x1a`) | star (`0x0c`) |
| 1 | End (`0x0f`) | 0 (`0x0a`) | hash (`0x0b`) | 6 |
| 2 | 1 | roller push (`0x12`) | 4 | 7 |
| 3 | 2 | 5 | 8 | 9 |
| 4 | 3 | unused | unused | unused |

The separate special map is `5a 0d 5a 5a 5a`: column bit 1 is Power,
requiring mask `0x02`, not the inherited `0x04`. The key-code names shared
with independently mapped products identify digits, star/hash, call keys and
softkeys. The remaining fitted matrix input `0x12` is labeled roller push by
Nokia's physical key inventory; its application action remains unvalidated.
Rotation still requires the separate three-phase UIF+ contract above.

`make verify-7110-keypad-controller` pins the flash hash and both tables,
then tests all 17 fitted matrix inputs across all five row drives (85 scans)
and Power press/release. This is MMIO controller conformance, not a firmware
message injection or proof of post-bootstrap UI key acceptance. The normal
bootstrap gate separately protects the unchanged fail-closed DSP boundary.
