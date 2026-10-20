# Nokia 5190 NSB-1 bring-up

## Current boundary

The acquired service package supplies independently identified NSB-1 v6.71
MCU and PPM A/B streams. It is not a full phone dump and does not establish
graphical boot or phone services. The separate `noki5190` MCU research
configuration executes the own input, initializes the own EEPROM geometry,
installs its exact ROM4-catalogue program and reaches a supervisor flag loop.
The supervisor flag is cleared organically by FIQ; its sampled loop is not
a blocked lifecycle. In the ordinary silent configuration, the unanswered transition is the uploaded
program's resident-version publication to shared word `10004`, before the
MCU begins sparse-flash handoffs. Separate `nsb1r4t` executes acquired NSE-1
ROM4 as an explicit compatibility candidate: it passes own version selection,
63 block handoffs and native loader/resident continuation. LCD bus/pixel
acceptance reproduces firmware clear/fill/clear self-test, not application
presentation. Fitted NSB-1 mask identity and DSP cadence remain unproved.
No scheduler or ownership flag is injected.

## Qualified executable input

Source: [archived Nokia service package](https://archive.org/download/Nokia_DCT3_firmwares/5190e671.zip).
Archive SHA-256:
`7c7167aba12342fa1ef3810c93bf518b0f8aeb79f67e9637e19d76b34e781128`.
The acquisition manifest is `roms/archive-dct3-packages/manifest.json`.

| Stream | Supplied CPU span | Decoded SHA-1 |
| --- | --- | --- |
| `nsb1___6.710` MCU | `200000..2d2cff` | `f953fd19093f17f36845cf46417f7ddd750ace80` |
| `nsb1___6.71a` PPM A | `2e0000..2fffff` | `0c55c1825cb722872c18e10a714f4be27cae3871` |
| `nsb1___6.71b` PPM B | `2e0000..2fffff` | `8c3aa2858a3bc82cda1644a81d03263983d1c491` |

All three independently contain `V  6.71`, `25-01-01`, `NSB-1`, `(c) NMP.`.
The restore INI names MCU/PPM ROM4 files and erase span `200000..2fffff`;
USA selects A, USA_T9 selects B. A ROM4 filename is not a resident DSP dump
or proof that the recovered NSE-1 mask fits this product.

`tools/nsb1_package_check.py` pins the archive, closes its six-member
inventory, validates the restore map, strictly decodes all three record
streams and checks embedded PPM identities. Normalization writes a 1 MiB
flash view with the **unsupplied** `2d2d00..2dffff` gap erased. This gap is
not acquired data or calibrated storage. No external EEPROM, identity,
security state or internal boot/DSP mask is supplied or synthesized.

```sh
.venv/bin/python tools/nsb1_package_check.py \
  roms/archive-dct3-packages/5190e671.zip \
  --ppm A --output roms/noki5190/5190f671a.fls
```

The normalized A SHA-1 is
`27b8f0cb8640e5b423960b206e9aee065f32d726`. Omitting `--output` is read-only.
The checker rejects source overwrite and never extracts archive paths.

### Executable integrity and controller mode

Own routine `294e70` reads the stored big-endian halfword at `200022` and
at most two six-byte `{24-bit start,24-bit inclusive end}` entries beginning
at `200024`; start `ffffff` terminates the list. The qualified input declares
`200024..2bffff`, followed by a sentinel. Its big-endian halfword sum modulo
65536 is `8016`, exactly the stored value. The normalizer now independently
checks this grammar/sum. This covers that declared range, **not whole-flash
authentication**, and requires no executable-byte repair.

Passive observation of byte `10c4c4` identifies initialization from zero to
2 at store `2a438e`. Own initializer `2a42c6` reads ADC selector 3 via
`2b1730`; after an earlier special-case window, a sample below ROM threshold
`3fa` takes this store. The configured selector-3 input is `280`. Getter
`2a46ac` consequently returns 2 in the startup controller. Static decode and
positive debugger observations show this continuation still executes the
flash checksum and SIM initialization. It is not evidence that mode 2 is
an erroneous boot state, nor justification for adjusting an ADC value to
select a different branch. Physical pack semantics remain unproved.

Diagnostic instruction probes must validate debugger-console errors as well
as the emulation log: accepted breakpoint installation does not validate its
action expressions. This ARM debugger exposes the link register as `r14`,
not `lr`; an invalid action must not become a false "function never ran"
conclusion. The retained normal observer uses passive RAM/MMIO taps instead.

## Own EEPROM initializer

Routine `2ab168` reads ROM byte `200005` through its literal at `2ab1dc`.
The own source byte is `f4`. It constructs the runtime descriptor at
`10c4a4`: byte 0 is `(f4 >> 6) & 1 = 1`, byte 1 is
`1 << (((f4 >> 3) & 7) - 1) = 32`, and word 4 is
`1 << ((f4 & 7) + 9) = 8192`. This establishes the firmware-declared
two-byte word addressing, 32-byte write page and 8 KiB capacity; it does
not independently identify the fitted EEPROM part.

Read helper `2ab0e6` calls address setup `2aaf58`. The latter checks the
address against descriptor word 4, selects slave byte `a0` for nonzero
descriptor byte 0, sends `address >> 8` at `2aafa8`, then sends its low byte
at `2aafb4`, and issues repeated START/read slave `a1` for reads.
Its zero-mode branch instead folds high address bits into the slave byte.
Consequently a one-byte-addressed 24C16 fixture is not faithful to this
initializer, even if its donor contents happen to satisfy a checksum.

Validator `26cc4c` reads `120` bytes, calls byte-sum routine `2a0fbc` over
the first `11c`, and compares the resulting 16-bit sum with the big-endian
32-bit field at `11c`. This confirms one external checksum clue without
supplying the absent calibration/identity contents. Routine `239da4`
independently reads `120..1fd`, sums its bytes modulo 65536, then subtracts
the **two individual bytes** returned by the `154/155` reader `295bcc`.
It compares that value with the big-endian u16 at `1fe`. This excludes the
adjustment bytes from the sum; it is not subtraction of their numeric u16.

## MCU research configuration

The own ARM entry at `200040` initializes stacks using `10fc54` and
switches to Thumb at `2000e8`. The trial maps only the minimum 64 KiB SRAM
window covering these stacks, not the generic driver's larger window;
physical fitted capacity and aliases remain unproved. Flash uses a
capacity-compatible 1 MiB trial part. The external I2C device implements
the own 8 KiB/32-byte-page protocol, with erased initial contents and an
unmeasured 5 ms write cycle. Boot/DSP masks and EEPROM dump remain separate
`NO_DUMP` entries. Shared MCU boot-transfer HLE remains explicit; no DSP
acknowledgement, service or radio peer is enabled. The own-mapped SIMI
controller is present with an empty socket; no synthetic card is inserted.

Own serial routine `2ad4b0` selects control `22` at `20028`, sends at
`2002a`, polls `20029` bit 2 and reads `2002d`. LCD clear `2ae0ac` shares
selection/status at `28/29`, command `2c`, data `2b`, six banks and 84 columns.
EEPROM byte transfer `2ac704` independently drives PUP signal `20020` bit 0
as SDA and bit 2 as SCL, releasing SDA through direction `20024` bit 0.
These port-level contracts justify the existing device composition, not
measured serial timing or fitted controller identification.

Key scanner `2af050` uses five rows, signal `31`, direction `2f`, columns
`30` and mask `33`. Decoder `2af768` selects normal table `2d2984` and
special table `2d29a0` using selector byte `10c466`. The normal table matches
the existing NSE-1 host layout; the active special table is
`3e 3e 3e 3e 0d`, selecting Power on bit 4, not bit 1. These are independently
mapped source facts; physical input/UI delivery has not yet been accepted.

A bounded physical Menu test on the checksum-only native compatibility
configuration holds row 2/column 1 from 3.0 to 3.25 seconds. Passive reads
of `20030..20033` show 11 accesses, all before 0.281 seconds; scanner
`2af050` is entered at startup, but neither that entry nor another keypad
register read follows the held contact through five seconds. Debugger action
expressions were checked for console errors. This does **not** demonstrate
decoded Menu delivery or application rejection: interrupt enable/routing and
scanner scheduling remain to be established for this lifecycle. A lack of
repaint alone cannot close the input path.

The subsequent routing observation identifies a firmware-owned local mask:
routine `2af7a0`, entered with return address `26f3db`, ORs `1f` into
column-mask register `20033` at store `2af7c6` (0.280282 seconds). Samples
before, during and after the Menu hold retain mask `3f`, row signal `e0`
and row direction `1f`. MAD2 pending IRQ remains zero, IRQ mask is `ce`,
global control `05`, and CPSR interrupt-disable bits are clear. Thus this
test is stopped by masked column-change detection before CPU interrupt
delivery, not evidence of a broken MAD2 IRQ route. The firmware continuation
that clears this mask remains unresolved; do not clear it diagnostically and
claim ordinary input support. Register-observation reads in this second run
are distinct from firmware reads and must not be counted as scanner activity.

An aligned Thumb literal-load scan for the exact `20033` address finds four
candidate unmask continuations: stores `26eb08`, `26f33e`, `26f4fc`, and
`26f7a2` AND the register with `e0`, clearing all five column-mask bits.
The fifth matching literal-load site belongs to the masking routine above.
This is coverage of direct PC-relative loads of that address, not a proof
that computed/indirect writers do not exist. None of the four candidate
unmask stores executes in the five-second checksum-control observation.
The startup receive call at `26f3f8` nevertheless returns repeatedly at
`26f3fc`, with observed values `37`, `c8`, `33`, `ca`, and `32`; its
receive loop is not wholly dormant. The remaining question is which
firmware transition reaches a legitimate input-enable continuation, not
whether to synthesize a key IRQ through the existing column mask.

The native checksum-control run extended to 30 seconds narrows the
`26f4aa..26f4be` continuation: byte `10ffb0` reaches `06` at store
`2aa942` around 1.249 seconds, satisfying its low-nibble comparison with 6.
The other byte, `10c3f5`, accumulates `08 -> 0a -> 0e` at `26f420` and
remains `0e` at 10, 20 and 29 seconds, short of the required low nibble `f`.
Positive instruction observations associate those accumulated bits with
logical inputs `17`, `16` and `15`. The bit-0 branch at `26f42e` requires
logical input `14`; it is not reached in the observed interval.

Own wrapper `2aaae8` calls the existing helper `2b1ee0`, then posts with
`r0=1,r1=14` through `268a94` at `2aaaf6`. An aligned classic Thumb BL scan
finds one direct-call candidate, `221f08`, after call `276fa8` and a flag
OR with `40`. This does not close indirect/data-driven callers or establish
the semantic owner of that routine. The next boundary is this report's
organic producer lifecycle, not a missing column IRQ or an arbitrary delay
before pressing Menu. No report or readiness byte has been injected.

The own image contains diagnostic state strings passed through `2afd0a`.
A bounded 30-second instruction trace of that helper records these positive
labels in the `221000..2253ff` region: `CHECK TEST MODE`, `CHECK POWER ON
REASON`, `INIT CHARGING`, `DO INIT CHARGING`, periodic connection/mode checks,
then `BOOT UP CHARGE`, `INITIALIZE`, `WAIT CHARGER VOLTAGE SETTING`, and
`CHARGER DISCONNECTED`. Their source string addresses are respectively
`2240c4`, `2241c4`, `2241ec`, `2241fc`, `223c34`/`224a74`, `224380`,
`2243cc`, `224390`, and `2243b0`. No report-14 wrapper entry is observed;
the candidate blocks `221e88`, `221efc`, and `221f08` are also unobserved.
This ties the current investigation to a positively identified charging
lifecycle, but does not prove which physical input should select a different
path. Generic ADC inputs and power-on/CCONT status semantics remain candidate
causes, not established defects. Some other diagnostic calls point at binary
or empty data rather than plain ASCII; do not assign names by treating every
helper argument as a zero-terminated string.

Getter `2a42a0` reads byte `10ff6d`. Its observed initializer is store
`2a456e`, which writes `0a` at 0.004768 seconds; no subsequent write is
observed through 30 seconds. In routine `2a4522`, the literal at `2a4734`
addresses MAD2 reset register `20001`. Its bit-0-set branch at `2a4528`
selects this `0a` store. Thus this selector is not a direct interpretation
of CCONT's charger-reset latch. The current MAD2 cold-reset value is `01`;
the observed branch is consistent with that input and does not justify
changing CCONT reset causes or clearing the MAD2 power-up indication.
Charging initialization and its report completion still need tracing from
this cold-reset lifecycle.

The subsequent boot-policy selector `2aa98e` takes its cold-reset `0a`
branch at `2aa9d2`. Positive probes observe ADC selector 3 returning `280`,
the scanner returning `ff` at `2aaa1a`, and packed serial read `90ff`
returning `03` at `2aaa24`. The selector then returns 2 at `2aaa48`.
Its shift/carry decisions test status bit 2 with `LSRS #3` and bit 1 with
`LSRS #2`; they do not test bits 3 and 2. Thus the return 2 follows the
observed bit-1-set status, not a nonzero charger ADC measurement. This
disambiguates the boot-policy result from the separately stored ADC-derived
controller mode at `10c4c4`, which also happens to be 2 in this fixture.
Neither equality alone establishes a shared semantic meaning.

Receive wrapper `220384` passes raw event `21` unchanged; the state dispatch
at `221b16..221b1a` directs it to candidate block `221e88`. Two direct
task-`14`, event-`21` publishers are `2232b0` and `2237bc`. Neither publisher
nor the report wrapper is observed in the bounded 30-second trace. The first
64 raw receive observations instead contain `b1`, `4c`, `174`, `175`, `176`
and `179`; that capped list is not exhaustive absence evidence for later
receives. It does establish live periodic activity, rather than a wholly
dormant charging task.

Reproduce the wider direct-post census with `tools/nsb1_package_check.py
roms/archive-dct3-packages/5190e671.zip --post-census`. It reuses the shared
swap16/Thumb argument-recovery machinery after source qualification: 115
direct calls to `268a94`, 84 with both task/event arguments recovered, 31
with incomplete arguments. The two event-`21` candidates above both resolve
to task `14`; no resolved event-`21` call has an unknown destination. This
does not close those 31 calls, other transport APIs, indirect calls, or
table-driven production. The JSON explicitly sets `absence_proved=false`.

Of the 31 incomplete calls, 30 have known destinations other than task `14`.
The remaining call `2a17be` routes an object from `r5`, matching its byte 6
against five eight-byte entries at `2ced48`: destination words are `2`, `1`,
`17`, callback `2b2891`, and `8`. Values below `18` select task delivery;
larger values are called as functions. None directly selects task `14`.
The same optional census additionally reports 28 direct calls to `269200`
(14 with both arguments recovered) and 376 to `268944` (83 with both).
Every destination in these additional calls is recovered; the only task-14
call among them is `2a4f20`, with scalar `30`. No additional recovered
scalar `21` candidate appears. Object pointers are not semantically treated
as event IDs, and indirect/tail-call/other-API coverage remains open.

A single host charger-contact diagnostic connects at 3 seconds and removes
at 6 seconds, using the existing configured connected raw value `3ff`
(physical voltage not validated). It still observes `{phase=06,reports=0e,
column_mask=3f}` at 10 seconds and a white 84x48 raster; neither direct
event-21 publisher nor report-14 wrapper executes through 30 seconds. This
rules out that specific contact sequence as an unblock, not the whole
charging/ADC hypothesis. The source reader `2a3686` explicitly samples ADC
selector 5 and applies calibration before returning a charger measurement;
its calibration/units require independent validation before changing inputs.

The charging state dispatcher at `224836` indexes 30 word targets at
`22484c` using halfword `10b9a2` (structure base `10b96c`, offset `36`).
A passive changed-value watch observes `0000 -> 0018` during initialization,
`0018 -> 001a` at `221aba` around 0.284 seconds, `001a -> 0004` at
`2219f2` around 3.697 seconds, then `0004 -> 0003` at `221aba` around
5.183 seconds. State 3 remains at 29 seconds. Table entries 24, 26, 4 and
3 point respectively to `223d3c`, `223fb6`, `224064`, and `224114`, matching
the positively observed initialization/boot-charge/voltage-wait/disconnected
labels. The state-3 handler tests translated events `49` and `41` before
its ordinary continuation. This proves initialization progresses to a
disconnected waiting state; it does not yet identify the missing report's
ordering dependency or establish that another ADC tuple would be correct.

The reusable observer now records `nsb1_input_readiness` at all five normal
sample times. The checksum-control checker requires the five-second
boundary `{startup_reason=0a, phase=06, reports=0e, column_mask=3f}` and asks
for deliberate review if it changes. This protects the observed incomplete
state, not a requirement that a future faithful phone remain blocked.

### External reference leads, not provisioning evidence

The [independent emulator's current coverage table](https://github.com/djr-747/nokia-dct3-emulator/blob/main/docs/MODELS.md)
reports 5190 standby. Its current product profile still uses an NSE-1 donor
EEPROM and security-settings repairs, so that outcome is not a forcing-free,
own-storage comparison for this configuration. No donor bytes are admitted.

Own lookup `2a75e4` splits the record ID into high-byte group and low-byte
index, selecting eight-byte group entries at `2cc3a0`. Group 7 points to
`2cc0a8`; the reader multiplies the low index by 8 and compares a **32-bit**
record ID before returning the entry. Records are `{BE32 id, BE16 offset,
BE16 length}`. Entries `2cc0b0` and `2cc0b8` identify `0701` at EEPROM
`04a8`, length 8, and `0702` at `04b0`, length `30`. Their IDs start two
bytes before the superficially matching `0701`/`0702` halfwords. Helper
`2a78aa` reads entry offsets `+4/+6`; `2a7974` passes the resolved address,
destination and length to external read `2ab0e6`. Write wrapper `2a792e`
instead uses `2aafe2`. The package checker validates these two own records
and their storage bounds without generating contents. Security-field
semantics and valid factory contents remain unproved. In particular,
`04b0+1f=04cf` is only an address relationship until its reader/validator
is recovered, not permission to clear a purported lock field.

In the checksum-control native run, wrapper `2b245a` actually reads `0702`
to `10fcd6` (return `2b2467`) and `0701` to `10c494` (return `2b2471`).
Consequently EEPROM `04cf` corresponds to RAM `10fcf5`. A passive byte-lane
read watch observes exactly two accesses through five seconds: generic
settings-table validation at `28c040` and EEPROM serialization at `2ab072`.
The final byte is `11`, not erased `ff`. The settings table at `2cfbf4`
contains `{35,11,02,39}`; its RAM base is `10fcc0`, so offset `35` names
this byte. The validator tests table flags and maximum before assigning a
default. These observations establish generic settings ownership, **not** a
security-level interpretation or a lock decision at the current frontier.
The watch excludes its own final snapshot read; no guest state is written.
Do not import the external profile's field name or clear the byte merely
because that profile reaches standby.

Public catalogues also list a [5190 system-module document, `sysa3.pdf`](https://www.eserviceinfo.com/downloadsm/233124/NOKIA_sysa3.html).
Its contents have not been acquired or verified here; the listing alone
does not establish ADC nets or fitted component identities.

Fresh passive runs observe the following without firmware RAM/MMIO writes:

- runtime EEPROM descriptor is mode 1, page `20`, capacity `2000`;
- both own catalogues initialize at `10adc0` and `10ad50`, whose first
  pointers are `2b35a4` and `2b94e4`; family selector is 4;
- all 186 words installed at MCU `11e00` match the own descriptor at
  `2be564`, payload SHA-1 `e1e8f15a305bf32aea98f444074f30828d933fc4`;
- 246 shared-memory writes occur before the observed supervisor park;
- through five seconds sampled PC is `2a4672..2a4678`, SP `10c6dc`, with
  EEPROM geometry/catalogues stable. This is ordinary supervisor idle:
  FIQ entry `2acac4` clears flag `10c540` at `2acaca`, and the supervisor
  re-arms it at `2a4668`. A five-second run observes 39 flag writes, with
  repeated wake/re-arm pairs. Global control is `05` after startup, both
  ARM interrupt-disable bits are clear, and local FIQ mask is `e3`.
  Repeated samples in this loop therefore do not imply an interrupt deadlock;
- persisted flash equals the normalized source. The 8 KiB EEPROM contains
  firmware-created writes, which are not proof of valid calibration or identity.

Use a fresh `RUN` directory:

```sh
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame --run-dir RUN -- \
  noki5190 -rompath ../roms -video none -sound none -nothrottle \
  -seconds_to_run 6 -log -autoboot_delay 0 \
  -autoboot_script ../tools/nsb1_boot_observe.lua
.venv/bin/python tools/nsb1_boot_check.py roms/noki5190/5190f671a.fls RUN
```

The checker pins the source, exact installed program, five ordered geometry/
catalogue observations, stack bounds, supervisor park and unchanged executable
flash. It additionally requires repeated FIQ-owned flag clears and actual
global interrupt-enable observations. Register `0b` is labeled `irqmask`,
not global control (`0c`). It is a boundary regression, not a graphical boot
oracle.

### First missing DSP publication

MCU routine `28f934` initializes shared results to `0000/ffff/ffff/ffff`,
loads its selected program, supplies arguments
`0100/0300/0000/8000/0001/0001/0200` at `100f6..10102` and releases reset
with command `01` at `28f9c8` (0.007852154 s in the measured profile).
Its first result check at `28f9dc` reads shared `10004` (DSP data `0802`),
then retries at `28f9fc`. The own uploaded program's early MVPD reads
resident program cell `ff87` into `0802`; this precedes its block loop and
the later calls to resident helpers `8023` and `802b`.

With the explicitly silent DSP the MCU repeatedly reads `ffff`, times out,
clears success byte `10ad2d`, saves `ffff` at `10ad36` and skips the
sparse-flash transfer branch. Both ownership flags remain 1. The subsequent
supervisor wakeups are independent evidence that the RTOS still runs, not
proof of a completed verifier. Do not describe this as an unacknowledged
first block: no first sparse-flash block has been supplied yet.

The observer suppresses its own snapshot reads from the API-read tap.
Accepted read evidence must come from the firmware PCs above, not the
supervisor PC that happened to be current while Lua inspected memory.

## Native ROM4 compatibility instrument

`nsb1r4t` retains own NSB-1 flash, controller composition and erased 8 KiB
external EEPROM. It replaces the silent backend with acquired NSE-1
program/data images, explicitly named as NSE-1 inputs rather than relabeled
NSB-1 dumps. SHA-1 values are
`a05a1e96a8c36ec5a47e1ea059d15afa54ca5739` (program) and
`024c7f970f4ef754d3e90471de48a167515f930d` (data). The 52 MHz DSP clock is a
declared compatibility cadence here, not an independently measured NSB-1
timing contract. No HLE reply or donor EEPROM is attached.

Own firmware automatically tries two uploaded programs. The first,
descriptor `2b9344`, has 186 words with SHA-1
`73b0cc6e6fc602f3a2f7934eb0c35d7f31c96b3f`; the second, descriptor `2be564`,
has SHA-1 `e1e8f15a305bf32aea98f444074f30828d933fc4`. Passive release-time
readback matches both payloads exactly. Native execution publishes resident
version 4: the MCU rejects the first candidate's requested version 3 and
accepts the second candidate's requested version 4. Both attempts are also
present in the silent control; reset-edge tracing must not suppress the
second release or count a later command `03` as another reset release.

The accepted comparison performs all 63 main-loop ownership handoffs,
then the MCU's final-result poll at `28fa96` observes shared `10002`
change from `ffff` to `0004` through native execution. It subsequently
releases its own 580-word loader (`fd00/ff80/0244/0500/0078/0000`), and
executes native loader/resident code observed at `0d8f` and `8d98`.
By 0.5 seconds success byte `10ad2d` is 1, saved version `10ad36` is 4,
and the handoff count is 63; these remain observed through five seconds.
Source executable flash is unchanged and EEPROM persists at 8 KiB.
The final panel is white after the separately validated LCD self-test. This establishes compatibility of this
bootstrap path, **not fitted-mask identity, valid calibration, graphical
boot, RF operation, audio or phone-service parity**.

```sh
.venv/bin/python tools/nsb1_rom4_compare.py \
  mame/mame roms tmp/nsb1-rom4-comparison --seconds 6
```

The runner rejects existing run directories and qualifies all three input
hashes before creating its private ROM directory. It admits no EEPROM image,
records provenance, requires ordered version mismatch/acceptance, both exact
uploads, 63 handoffs, final-result publication and positive native I/O traces, and checks persisted
executable identity. `--verbose` optionally records the larger native opcode
inventory. Shorter runs record evidence without claiming the six-second
acceptance boundary. The normal `noki5190` silent-MCU gate remains independent
and unchanged in behavior.

### LCD transport and pixel self-test

Passive MCU MMIO capture observes selection `21` at `20028`, commands at
`2002c` and data at `2002b`. The own initialization stream is
`24,40,80,41,80,42,80,43,80,44,80,45,80,20,21,05,14,ce,20,0c`.
Firmware writes six 84-byte zero banks, then six full `ff` banks, then
clears them again. By 0.5/1 seconds totals are 32 commands, 1008 data bytes,
504 nonzero bytes; by 2/5 seconds totals are 68/2016/504.

The named snapshot `snap/nsb1_native_1s.png` is exactly 84x48 black;
`snap/nsb1_native_5s.png` is exactly 84x48 white. These differ from the
ordinary end-of-run auto-snapshot under `snap/nsb1r4t/`. The compatibility
checker now requires both MCU traffic and decoded pixel values, rejecting
an always-white panel or a missing nonzero transfer. No framebuffer or
firmware state is written by the observer.

This validates the exercised command/data transport and full raster under
this configuration, not fitted controller identity, contrast, analogue LCD
timing or application UI readiness. A white final frame alone must not be
used to infer a broken display: the observed firmware deliberately clears
the self-test pattern. The next consumer-side investigation is application
presentation after self-test, with the own EEPROM validity gates kept
separate from hardware transport.

### EEPROM checksum-only diagnostic control

With initially erased storage, own self-test flag byte `10fde5` reaches
`c4`, loses bit `40` at checksum-failure store `23a018`, becomes `80` after
the DSP-related check, then `00` after `29a61c` clears bit 7. This is an
observed failed integrity path, not proof of missing native execution.

An explicitly labeled control recomputes only `11c..11f` and `1fe..1ff`
in the EEPROM created by a qualified own erased-storage run. It preserves
every calibration, identity, security and other content byte; it neither
imports a donor nor makes erased contents into a valid factory profile.
The runner verifies the source run's exact input manifest and unchanged
own executable before generating the six-byte integrity control. It cannot
chain from another checksum control or silently reuse prior NVRAM.

```sh
.venv/bin/python tools/nsb1_rom4_compare.py mame/mame roms \
  tmp/nsb1-checksum-control --seconds 6 \
  --checksum-control-from tmp/nsb1-rom4-comparison
```

This control retains bit `40` (`c0` at 0.5 seconds, `40` at 1/2/5 seconds),
with the same completed native bootstrap. But the LCD traffic and final
white frame remain unchanged. Therefore checksum consistency alone does
**not** establish application presentation or valid provisioning. The default
native acceptance and its strict LCD oracle remain unchanged; the diagnostic
deliberately makes no presentation claim. Its input/output hashes and changed
offsets are retained in the run manifest. Retire this control as a bring-up
instrument once product-matched calibrated storage supplies its replacement;
retain the decoded integrity formulas and this negative result.

### Empty-socket SIMI boundary

Own routine `29cde8` resets SIMI at `20038`, configures `2003a`, writes
RX/TX FIFO controls `2003d/3e`, and writes control `32` at `20039`.
Receiver `29ce5e` drains `20037` while testing count `2003c`; the nearby
initializer manipulates FIQ6 masking. These independently recovered register
contracts justify attaching the existing controller with no inserted card.
No ATR, APDU reply, SIM-ready event or internal firmware flag is supplied.

The erased-storage native control makes no SIMI access in the first five
seconds. The checksum-only control reaches the own initializer at about
0.94 seconds: passive taps observe 18 writes, including `38=ff` at `29cdfc`,
`3a=03` at `29ce26` and the FIFO-control sequence beginning at `29ce3c`.
All five samples report zero SIMI reads, consistent with this bounded
empty-socket path. The diagnostic checker requires positive initializer
sites and the 0/0/18/18/18 write-count sequence, not merely a source setter.
Native bootstrap and LCD self-test oracles remain unchanged.

This is firmware initialization/write-side acceptance, not character RX/TX,
ATR, card security, application SIM handling or proof that SIMI was the
remaining UI prerequisite. ADC selector routing and sensible board inputs
remain separately unvalidated; do not assign VBATT to channel 0 from its
position in a generic tuple.
