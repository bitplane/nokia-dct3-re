# Nokia 2100 NAM-2 bring-up

## Current result

The own-image v5.21 `nam2c3` research configuration now renders **Code
accepted** after physical `12345` and left softkey, preserves its EEPROM
catalogue through parameter erases, and passes checksum/startup release on
a cold restart of the firmware-written NVRAM. This is not usable idle or
service parity: late physical input remains unconsumed. The C3 configuration
is explicitly a supported-part trial, not an identification of the acquired
board; `noki2100` retains its previous flash composition.

The hash-pinned v5.84 MCU and PPM-E streams execute through a product-local NAM-2
profile. The firmware completes CCONT and LCD traffic, performs a 64-exchange DSP
bootstrap, acknowledges DSP service command 4, completes the type-05 external-
service discovery transaction, accepts a class-0x40 application registration,
and completes its physical M2BUS terminal startup exchange. The compact type-74
completion clears the initial `CONTACT SERVICE` frame. A checksum-corrected
fixture that preserves the v5.84 MCU/PPM bytes and attaches only the v5.21
donor's 64 KiB PMM partition does not make that foreign catalogue valid for
v5.84. On a freshly seeded fixture the phone clears the service frame, presents
the security editor and consumes physical `12345` plus Navi, but rejects it
because the identity-derived verifier changes during the same boot.

This is a bounded interactive compatibility result, not a default boot profile.
No 3210, 3310, or 5210 keypad, display, SIM, service, radio, or nonvolatile-state
contract is inherited merely because its values appear compatible.

## Own v5.21 C3 storage checkpoint

`nam2c3 -bios 521sharp` uses the same acquired image and the same two-byte
checksum repair below, but selects the separate Intel C3 model. No donor
PMM, credential edit or firmware-state write is added. The installed family-3
identity reader `135340` enters ID mode and reads four halfwords at
`20010a..200110` through caller `3071bc`: word addresses `85..88`, the C3
**user** protection field, not its factory serial number. The generic model
now reads erased user words unless supplied through an optional 18-byte
`flash:otp` region (lock word, four factory words, four user words, BE).
Unknown factory words remain undumped. The generic C3 model separately
supports one-way user-OTP programming and irreversible user locking; the
handset checkpoint below does not program OTP or invent factory identity.
The earlier unimplemented-ID fallback returned zero and incorrectly changed
the runtime checksum from `a0aa` to `98b2`; no checksum was adjusted for that
fallback.

Fresh run `tmp/nam2-c3-otp-first-20261020` confirms family 3, checksum
`a0aa/a0aa`, release 1, five physical digits, then the reviewed localized
Code-accepted pixels. Unlike the uniform-block run's rejection, the original
EEPROM catalogue survives the firmware's index-7 and index-6 erases. The
executable prefix is unchanged. Restart
`tmp/nam2-c3-cold-20261020` uses a copy of that persisted NVRAM in
`tmp/nam2-c3-cold-storage-20261020`, without re-running the generator;
checksum `a0aa/a0aa` and release 1 reproduce. Both stored identity blocks
also validate by direct sum. The late-input restart does not consume keys:
credential presentation in the first run is not proof of unattended idle,
repeatable late input or network services.

Reproduce the first run with `tools/nam2_c3_security_observe.lua` in debugger
mode (`-debug -debugger none`). Seed the checksum fixture at
`RUN/nvram/nam2c3_1/flash`. Copy the resulting NVRAM to a separate cold-run
directory without regenerating it, then restart with the read-only
`tools/noki2100_v521_startup_observe.lua`. Validate the captures using:

```sh
.venv/bin/python tools/nam2_c3_checkpoint_check.py \
  roms/noki2100/2100f521sharp.fls FIRST_RUN COLD_RUN COLD_FLASH
```

### Security-input save-state replay

`tools/nam2_c3_security_state.lua` saves at 7.75 s before any code contact,
then physically enters `12345`/left softkey at 8..10.5 s. It loads the saved
state and repeats the same contacts. The checkpoint requires exact restored
ARM/banked registers, time and RAM checksum, equal continuation architecture
at 11 s, and the existing reviewed Code-accepted frame in both branches.
The checker additionally validates preserved executable/own PMM contents.
This protects the early editor/storage checkpoint, not late input, usable
idle, native DSP execution or the fitted-part hypothesis.

```sh
.venv/bin/python tools/make_2100_v521_profile.py \
  roms/noki2100/2100f521sharp.fls RUN/nvram/nam2c3_1/flash
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame --run-dir RUN -- \
  nam2c3 -bios 521sharp -rompath ../roms -video none -sound none \
  -nothrottle -seconds_to_run 16 -log -autoboot_delay 0 \
  -autoboot_script ../tools/nam2_c3_security_state.lua
.venv/bin/python tools/nam2_c3_security_state_check.py \
  roms/noki2100/2100f521sharp.fls RUN
```

### Remaining idle/input boundary

`tools/nam2_c3_idle_observe.lua` combines capped read-only breakpoints,
one-second state samples and physical contacts; run it with the same debug
options and a fresh own checksum fixture. Capture
`tmp/nam2-c3-idle-key-20261020` establishes:

- At 10 s, global control is `05`, nesting depth is zero, and early input
  works. At 13..30 s, control is `0a`, nesting remains zero, and the CPU
  continues executing the supervisor loop rather than remaining suspended.
- The seven-predicate chain at `2f8612..2f8648` reaches the all-idle branch
  `2f866a`. The deep-sleep guard `2fac86` returns zero: flag `03` and
  destination/current `7fff/34bb` pass, but the pending timer's adjusted
  interval is `2`, below its signed `>=3` requirement at `2facd8`.
  This measurement covers the first eight guarded iterations, not every
  later timer value.
- That branch calls clock helper `304c2e` with argument zero and loops via
  `2f85fa`; it does not traverse the separate control-`05` restore at
  `2f8658`. Timer/DSP FIQ bits accumulate (`19` at 14 s onward).
- A physical Keypad-1 contact at 18 s changes IRQ pending from `00` to
  `01`. IRQ0 remains pending through 30 s with local mask `a2` (unmasked)
  and global control `0a` (delivery disabled). This is not a lost matrix
  contact or an interrupt-nesting leak.

The idle branch acknowledges IRQ3 (`09 = 08`) and unmasks it (`0b &= f7`)
at `2f8674/2f8682`. Its handler `3075c8` schedules numeric event `79` with
delay `4b` through `2abaf4`, then acknowledges and remasks IRQ3. The runtime
12-byte event descriptor at `101334` is
`ffffffff0000ff0001000079` at 15 s in the physical no-SIM control:
inactive state 1, zero remaining interval, and byte `+6 = ff`. This does not
establish a task-message destination or the physical IRQ3 source. In particular,
`2ab970` is the cancel/remove helper, not an immediate publisher. Recover the
descriptor routing and IRQ3 source before implementing a wake response.
The initializer `2ac7a8` consumes 265 eight-byte ROM records after the 25
task records at `330b3c`. Event `79` therefore comes from `331030`
(`00ff0000ffffffff`); it copies ROM bytes 0/1 to runtime flags/routing
bytes `+7/+6`, initializes state `+8 = 1`, and stores the numeric index at
`+a`. The hash-qualified flash-contract extractor reports this mapping.
Expiry helper `2abcec` sets state 3, then tests runtime flag byte `+7`
bit 0 at `2abcf2..2abcf6`. With this record's zero flags it returns at
`2abd38`, bypassing the task-linkage code and its `+6` routing byte entirely.
Thus the observed descriptor is a non-delivering interval, not a missing
startup message. Predicate `2fd07c` checks its remaining delay through
`2abc00(79)`; the schedule/remask sequence is consistent with an idle holdoff,
The retained MADos archive (commit
`c4b9591e243a74484e27becf343655bb459f99b7`) explicitly names line 3
`IRQ_SLEEP` in `include/hw/int.h`. This independent source label supports
a sleep-related interpretation; it was missed in the earlier header audit.
It does not specify the generating edge, wake latency, or global-enable
readback/restoration behavior. Those hardware semantics, rather than a
task-message producer, remain the boundary to recover.

The next contract to recover is how ordinary idle is expected to service
pending work or restore global delivery when the deep-sleep guard declines.
The hash-qualified extractor's `idle_control_flow` checks both guard exits,
the explicit-enable block and the complete clock helper. A zero guard branches
to `2f85f4` (argument zero); a nonzero guard reaches `2f85f6` with argument
one after accounting. Neither traverses `2f865a`'s control-`05` write. Both
helper arms resolve their own literal base to `20000`, clear clock bit 0,
and converge on `2000d`; only the nonzero arm sets bit 1. The helper contains
no global-enable store. This closes a firmware-restoration interpretation of
that helper, not the physical meaning of the two clock bits.
Do not auto-write control `05`, alter software timer results or change the
credential on this evidence. The accepted-code presentation and persistent
storage are independent of this still-open idle/interrupt boundary.

The last permission transition is now localized, not an unidentified peer
event. A change-only return trace in
`tmp/nam2-c3-gate-transitions-20261020` finds the third predicate,
`2c85a4` (return observed at `2f8626`), changing from zero to one. Its exact
condition is `byte[10ef46] != 0 && query_29a9cc() != 0 && byte[10917b] != 0`.
The query selects `109173 == 1` when `107b8b == 0`, otherwise
`109173 != 0`. The fresh run has `107b8b=2,109173=1` before the transition.
The remaining predicates are nonzero when subsequently reached.

Passive byte-lane-aware watches in
`tmp/nam2-c3-banked-idle-owners-20261020` reproduce the final writers:

| State | Final writer | Observed meaning |
| --- | --- | --- |
| `10ef46: 0 -> 1` at 12.760697154 s | `21d364` | Flag set before receive call `2ac49c`; `21d36e` clears it after that call returns. It is not independently a startup-readiness report. |
| `10917b: 0 -> 1` at 12.771119462 s | `2b7fd6` | SIM-side idle permission. Its routine `2b7f74` invokes SIM clock/control helpers; `2e6e68` clears CTSI clock bit 5 and uses SIMI clock-control register `3a` for ASIC-specific handling. |

Thus the receive/SIM idle contract permits the branch after the accepted-code
transaction. The observation does not show an interrupt nesting leak or an
unproduced registration event, nor does it justify holding the SIM busy to
avoid the branch. Resume at the controller/clock behavior when these genuine
permissions allow ordinary idle; do not suppress their firmware-owned writes.

The physical no-SIM control isolates this from the security transaction and
the SIM clock-stop write. Run
`tools/nam2_c3_no_sim_idle_observe.lua` with the same fresh checksum fixture
and debug options. In `tmp/nam2-c3-no-sim-20261020`, firmware renders the
localized Insert-SIM screen without early code contacts. The permission
predicate already returns one at 10 s; control remains `0a` through 30 s,
the timer advances and the 18 s physical key remains pending. Clock control
stays `2c`, unlike the card-present run's `0c`. Therefore clearing the SIM
clock bit or accepting a security code is not required for the failure.

A separate NSE-8 no-SIM comparison
(`tmp/nse8-no-sim-gate-control-20261020`) retains control `05` at 10..30 s,
but it does **not** validate the disputed idle branch: its first supervisor
predicate `2b03d8` returns zero at `2a930e`, selecting the explicit enable
path. A working sibling's interrupt delivery cannot be used as proof of the
unexercised NAM-2 idle/clock contract.

The extended capture `tmp/nam2-c3-idle-timer-late-20261020` excludes a
stopped Timer 0 or permanently fixed interval of 2: the hardware counter
advances from `068f` at 13 s to `0f22` at 30 s while software timestamp
`10f9f8` remains `0672`. Eight separately capped guard samples after roughly
20 s return an adjusted interval of zero, still declining deep sleep.
IRQ0 from the 18 s physical contact remains pending. Waiting for this short
timer or inventing a faster clock is therefore not an evidenced correction.
The deep-sleep helper `2fad22` reads Timer 1's counter and terminal value,
but its stores at `2fadac/2fadce` target RAM `10f9fa`, not Timer-1 MMIO.
Literal reload `2fada2` selects `2fae44=10f9f4`; it replaces the earlier
MMIO base `2fae40=20000`. Both calls to signed divide `307098` preserve R2.
The bounded runtime observer confirms both store PCs with base `10f9f4`,
target `10f9fa` and low-halfword write masks. Sixteen observed stores match
the software `+50` and modulo-79 remainder sequence, beginning `0032/0064/0015`.
These are fractional software accounting, not a writable hardware destination
or an absent MMIO side effect. They do not justify changing Timer 1's model.

`nam2_v521_flash_contract.py` follows the literal and store register fields,
checks both divide call targets and R2 save/restore, and exports
`software_sleep_accounting_writers`. Its tests distinguish identical STRH
opcodes with RAM versus MMIO base literals. Runtime qualification also retains
the existing own-image/checksum/PMM and physical-wake boundary checks:

```sh
.venv/bin/python tools/nam2_c3_wake_flow_check.py \
  roms/noki2100/2100f521sharp.fls tmp/nam2-c3-sleep-accounting --sleep-accounting
```

The whole-boot clock-write observer `tools/nam2_c3_clock_observe.lua` separates
that late window from earlier idle work. A fresh own-checksum/no-SIM run
(`tmp/nam2-clock-caller-qualified`) observes helper write `304c56` emitting
`2e` first at8.918624385s, then `2c` first at9.750710077s. Through29s the
uncapped totals are4653 and156792 respectively. The static helper at304c2e
uses `(old & fe)` for argument0, versus `(old | 02) & fe` for nonzero;
the stack-saved return002f85fb independently places both command forms in
the supervisor's shared call at2f85f6, rather than its other direct caller
256034. The deep-sleep branch at2f868c therefore cannot be described as
wholly unentered on this boot. These are observed command writes, not proof of clock suspension,
destination writes or successful wakeup. Late input still leaves IRQ0 pending
and control0a. The next discriminating window is the early2e-to-late2c
transition, not another post-10s sample of the same declined guard. No clock
or global-enable behavior is changed. Own executable and both stored identity
checksums pass the existing `check_pmm` qualification.

### First wake/delivery boundary

Run the clock observer with `-verbose` to retain the device's actual
sleep/wake decisions, not only the requested command. Fresh no-SIM capture
`tmp/nam2-clock-guard-final` accepts sleep at8.918624385s. At9.000000000s
the HLE peer enqueues idle type03 and signals FIQ0; MAD2 wakes the ARM with
FIQ009, IRQ000 and control0a. The next stop request is rejected because
that locally unmasked FIQ is still pending. At the first2c write, the queue
head00101964 holds interval3, Timer0 is04ec and its software stamp04eb:
the adjusted interval is2, explaining the later guard decline without
postulating a stopped hardware timer.

The independent physical control `tools/nam2_c3_clock_key_observe.lua` presses
Keypad1 at8.95s, before that DSP tick. Capture `tmp/nam2-clock-key-final`
wakes through IRQ with FIQ008/IRQ001 and control0a at8.950000000s; IRQ0
remains pending at10s and19s. Both captures retain the own executable and
valid identity/checksum storage. This rules out an HLE-packet-specific cause
for the wake-without-service result. It does not establish automatic global
enable, IRQ_SLEEP generation or control-register readback semantics. The
remaining contract is how a real clock wake permits firmware service from
this disabled-delivery lifecycle; neither forcing05 nor suppressing the
physical wake source is an evidenced repair.

The independent archived Blacksphere IRQ3 and control-bit reference is
qualified in [MAD2 fidelity](mad2_fidelity.md#archived-interrupt-reference).
Its 331x labels corroborate the sleep-related source but do not specify the
missing NAM-2 wake/readback transition.

The bounded post-wake flow is now independently captured by
`nam2_c3_wake_flow_observe.lua`. After the 8.95 s physical IRQ wake, 64
positive fetch observations cover 16 repetitions of supervisor PCs
`2f85fa/2f860e/2f866a/2f8688` through approximately 8.952551 s.
Every sampled CPSR has low byte `3f`: Thumb System mode with both ARM IRQ
and FIQ masks clear. MAD2 control is `0a` throughout these observations;
at 19 s IRQ0 is pending with nesting zero. This excludes a CPU CPSR
mask leak within the measured continuation, not every CPU exception issue.
The unobserved mid-routine POP is not used as negative execution evidence.

`nam2_c3_wake_flow_check.py` also requires the C3 family, own checksum/release
and persisted executable/PMM qualification. Its result explicitly denotes a
known wake-without-delivery failure, not usable idle/input. It requires the
physical contact and hardware wake, ordered complete flow census, unmasked
CPU mode and late pending-key observation; changed or incomplete traces fail.
No controller state, RAM or guest timer result is written by the observer.

```sh
.venv/bin/python tools/nam2_c3_wake_flow_check.py \
  roms/noki2100/2100f521sharp.fls tmp/nam2-c3-wake-flow-qualified
```

Executable provenance is independently bounded by the acquired
`NAM-2_MCU_v5.21.EXE` installer (SHA-256
`8d53e0ccced4c510707ee752f2b66129f033fc8762082e7f4b29a80653d2d99a`).
Gzip member `49aa6d` decodes 176 contiguous MCU records covering
`200000..35ffff`; member `2080b8` decodes 72 matching PPM records covering
`360000..3effff`. Together they equal all `0x1f0000` executable-prefix bytes
of the Sharp-labelled full image, including the idle/control routines.
This rules out a difference between those acquired executable sources, not
cryptographic vendor authenticity or PMM provenance. Reproduce with
`tools/nam2_v521_flash_contract.py IMAGE --installer INSTALLER`.

The own exception dispatchers constrain control-register reinterpretation.
Runtime vectors branch to ARM IRQ entry `2fb574` and FIQ entry `2fb6dc`.
IRQ reads `2000c` at `2fb57c`, tests bit 3 and immediately returns if set;
FIQ reads it at `2fb6e0`, tests bit 1 and likewise returns if set. Early
exception-mode reads in `tmp/nam2-c3-vectors-20261020` observe `05` before
dispatch. The critical-section helper `2fab36` saves the actual low control
byte at `10f9f4`, writes `0a` on outer entry and tracks depth at `10f9f6`;
`2fab58` restores the saved low nibble only on final leave. These consumers
do not justify hiding disable bits on reads or enabling interrupts on every
idle-loop iteration. The physical write/read/clock-transition contract still
needs independent evidence; no alternative emulator's read-mask convention
is promoted by this audit.
The passive control-write inventory through 15 s observes only `05` and
`0a` in the no-SIM run. Independent per-domain enable/disable command
semantics therefore cannot by themselves change this observed failure:
both domains are explicitly commanded together, not accidentally lost to
a partial whole-byte write. The existing idle observer retains the value
inventory; it does not establish all possible command or readback semantics.

### Primary NAM-2 sleep and clock reference

The original **NAM-2 System Module DF9**, Issue1 12/02, is retained in
`roms/research/nam2-service-manual/2100/03-nam2-syst.pdf`. The public
[two-part manual archive](https://www.eserviceinfo.com/download.php?fileid=26128)
links [part2](https://www.eserviceinfo.com/download.php?fileid=26129).
Both volumes pass `unrar t` for all twelve PDFs; the ignored collection
retains the test log, source URLs and a complete `SHA256SUMS` manifest.

| Artifact | SHA-256 |
| --- | --- |
| `gsm_NAM-2_.part1.rar` | `580d111824a0133808ce559b170c43db78ce8a502f4f46d1a03a673d42fea041` |
| `gsm_NAM-2_.part2.rar` | `7b47a0a07c492c0cfdbb7168aa5a21d79064a79fe0548ebabb82ecc241a6d9c5` |
| `03-nam2-syst.pdf` | `7ffbd55b8b9ce167b5cf38cfd82fdb5091278a6dc2ce0993fec8595d7e8cbf13` |
| `12-nam2-schem.pdf` | `9cec6c53e658b9e4de29ee8aafc8d60c4b049a0e52cfafe62b37e26eda7a9c7a` |

Visually reviewed pages20..22 establish product-specific facts:

- Page20 describes wake from sleep-counter expiry or external interrupts,
  followed by oscillator settling and clock/regulator restoration. It supplies
  no CTSI register/readback rule or automatic global-enable guarantee.
- Pages20/21 assign PCM clocks to COBBA-GJP: 13MHz divided by13 gives
  1MHz; division by125 gives 8kHz framing. This is not live-media acceptance.
- Pages21/22 name MAD2WD1 and a 45.5MHz DSP, with 13MHz input and
  6.5/13MHz MCU/system clocks. Page22 specifies 16Mbit flash and
  2048Kbit work memory. Capacity alone does not establish upper-window
  address mirroring in the current512KiB RAM aperture.

The PDF metadata retains a NAM-1 subject; rendered headers identify NAM-2.
Its extracted font mapping is corrupt, so numerical claims require page-image
inspection. These references do not resolve disabled delivery after wake,
provide a resident mask or justify enabling interrupts as a workaround.

## Own v5.21 checksum fixture

The complete Sharp-labelled v5.21 image (SHA-1
`b7e30deb4393a76d509f843d25ecf20881bc25bb`) has an independently reproduced
checksum defect; this is separate from the v5.84/foreign-PMM experiment.
Own validator `0x291b0c` reads logical bytes `0000..011f`, optionally replaces
eight identity bytes through `0x3071bc`, sums `0000..011b` at `0x2f13b8`,
then compares the dword at `011c`. In the fresh control the optional callback
at `0x10fad4` is null, identity bytes remain erased, and the actual comparison
at `0x291b3e` is `0000a0aa != 000099a6`. Task 2 records item 18 as `12`;
its result-table sweep clears status bit 6 at `0x253136`.

The independent reference comparison at `0x2530a4` already succeeds:
computed/stored `933d/933d`, secondary `2d10`. Thus a missing or foreign
version reference is not the observed v5.21 defect. Supervisor release helper
`0x2e0b5c` returns zero at `0x2f84aa`, and the third startup predicate
`0x2c85a4` remains false. These are own-ROM addresses, not relocated v5.84
assumptions.

`tools/make_2100_v521_profile.py` accepts only the unchanged acquired full
image and changes exactly offsets `1fc144/1fc145` (`99a6 -> a0aa`). It
preserves all executable, identity, lock, calibration and other PMM bytes;
it does not import another image or alter firmware state. A fresh cold run
with this derived NVRAM confirms checksum equality, retains status bit 6,
returns one from the release helper and executes SIM SELECT/STATUS/READ
APDUs. It renders the localized Security-code prompt instead of
`CONTACT SERVICE`. This does not make erased identity fields authentic or
promote native DSP, registration or usable MMI.

Reproduce with an empty run directory:

```sh
.venv/bin/python tools/make_2100_v521_profile.py \
  roms/noki2100/2100f521sharp.fls RUN/nvram/noki2100_1/flash
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame --run-dir RUN -- \
  noki2100 -bios 521sharp -rompath ../roms -video none -sound none \
  -nothrottle -seconds_to_run 13 -log -verbose -debug -debugger none \
  -autoboot_delay 0 -autoboot_script ../tools/noki2100_v521_startup_observe.lua
```

The observer uses debugger breakpoints and read/write taps only for logging;
it writes no firmware or MMIO state. Fresh raw controls and corrected runs
are retained under ignored `tmp/nam2-v521-*-20261020/`, with the banked
observer reproduced in `tmp/nam2-v521-banked-observer-20261020/`.

The next boundary is physical input delivery. A separate fresh corrected run
with `tools/noki2100_v521_security_input.lua` (26 seconds, without the debugger)
presses `12345`/left softkey at seconds 14 through 19. It does not render
masked digits or change the prompt. Device tracing observes IRQ0 asserted
and retained across those contacts while the IRQ mask is `a2` and control
is `0a`. Masks are active-low, so IRQ0 is locally unmasked; global delivery
is disabled by control `0a`. The PC is parked at the clock-stop helper's
return (`304c58`), with a MAD2 sleep request at 11.741268 seconds.
The ordinary supervisor explicitly selects that stop request: the nonzero
result from `2fac86` at `2f8688` calls `2fad22`, sets `r0=1`, and branches
to the shared call at `2f85f6`. It bypasses the immediately preceding
`r0=0`; looking only at instructions adjacent to the call would misclassify
its argument. Runtime breakpoints observe argument 1, caller `2f85fb`,
control `0a`, FIQ mask `08` and IRQ mask `a2`.

Separating local-source clock wake from globally gated CPU-line delivery in
MAD2 restarts the ARM when an unmasked FIQ becomes pending at second 12.
It does not resolve input: firmware continues executing with global control
`0a`, and physical keys still do not render digits. The next contract is
global interrupt restoration across this ordinary idle continuation, not an
absent keypad edge. Neither automatically writing `05` at wake nor changing
the security verifier is justified by this experiment. No keypad mask or
task event is to be forced.

An early-input control independently proves the physical matrix and editor:
`tools/noki2100_v521_early_security_input.lua` presses the same six contacts
at seconds `8, 8.5, 9, 9.5, 10, 10.5`, before the observed idle transition.
Every IRQ0 assertion is followed by firmware acknowledgement; the screen
renders five masked digits and then localized `Wrong code!` after confirmation.
This is a real credential rejection, unlike the late-input control. It does
not establish why `12345` is rejected, a valid replacement credential,
usable idle or persistent PMM. The default late fixture remains at seconds
14 through 19; the early wrapper changes only physical input timing.

Run the corrected image as above with `-seconds_to_run 20`, omit debugger
options, and use `../tools/noki2100_v521_early_security_input.lua`. Then:

```sh
.venv/bin/python tools/noki2100_v521_security_check.py \
  roms/noki2100/2100f521sharp.fls RUN
```

The checker requires the own source/executable, 992-exchange bootstrap, all
six physical contacts and IRQ service windows, and reviewed 96x65 prompt,
five-digit and rejection pixels. Fresh captures
`tmp/nam2-v521-early-security-20261020` and
`tmp/nam2-v521-flash-20261020` both pass. The latter's passive flash observer
reached its 8192-write cap and cannot support an absence conclusion. Post-run
flash preserves the executable but no longer contains the original PMM
catalogues; new top-partition data is present. Thus the initial checksum must
not be assumed to persist at its original address, and cold-storage validity
is not accepted. Audit the actual erase/program lifecycle before attributing
that result to part geometry or interpreting it as intended compaction.

The uncapped command-filtered bus capture
`tmp/nam2-v521-erase-20261020/error.log` covers 20.016667 seconds and 20,910
top-partition writes. Seven writes contain a `0020` or `00d0` halfword;
the first four are program data at RAM-stub PC `135206`. The command pair
is `0020` at `3fff00` (PC `13524e`, 3.230879385 s), followed by `00d0` at
`3fa000` (PC `135252`, 3.230879846 s). Another `00d0` at the same address
occurs at 4.233167308 s without a second captured `0020` setup. The current
generic flash part erases the entire containing 64 KiB block; its fitted-part
identity and this firmware's expected erase span remain to be established.
An 8 KiB PMM segment spacing or a Sharp filename alone is not evidence for
changing that silicon geometry. No flash behavior is altered by this capture.

The independent ID-mode bus capture
`tmp/nam2-v521-flash-id-caller-20261020/error.log` establishes what the MCU
actually sees, not the fitted part's identity. At 0.033089077 s the RAM stub
writes `0090` to `200000`; reads at PCs `13520a` and `135226` return `00b0`
from the upper halfword, PC `13522a` reads `00d0` from the lower halfword,
and PC `13522e` reads zero at `200004`. An `00ff` write leaves ID mode.
Thus this transaction has no observed manufacturer/device byte-lane mismatch.
The saved LR `2e678f` belongs to a shared wrapper (`2e6774..2e678e`), not
an identified part-selection routine. Neither the ID values nor this wrapper
establish the firmware's selected erase geometry; that selection remains the
next static/runtime boundary to recover. Do not infer an 8 KiB erase unit
from the PMM spacing or change all Sharp `b0/d0` devices on this evidence.

The query stub's saved stack return identifies caller `305b5c`, which copies
the ID reader from `33dffc` to `1351f0` before querying. Selector `305b0c`
scans all ten eight-byte records at `33d748`; each has a family plus three
ID halfwords, with `ffff` as wildcard and the last matching record winning.
Observed `b0/d0/0000` matches the manufacturer-wide family-1 record. The
family-1 branch at `305bd0` copies `0x144` bytes from `33e064` to `1351f0`
and installs program `1351f1` and erase `135239` in slots `10fac8/10facc`.
These reproduce the captured program/erase PCs. This selects an algorithm,
not an erase size: the ten-record table contains no geometry field.
`tools/nam2_v521_flash_contract.py` extracts the hash-qualified table and
tests its selection semantics; the startup observer records the runtime
selection and installed slots. The PMM erase-span contract remains open.
Fresh isolated capture `tmp/nam2-v521-family-20261020/error.log` confirms
family `1`, ID tuple `0000/b0/d0/0000`, program `1351f1`, erase `135239`,
and both extra dispatch slots zero. Its observer makes no firmware writes.

The PMM erase wrapper `2e6998` indexes eight 24-byte descriptors at
`10b27c`, loads the first word as the erase address, and calls the installed
erase slot at `2e69b4`. Fresh capture
`tmp/nam2-v521-pmm-layout-20261020/error.log` observes index 7 with descriptor
addresses `3fc000,3fe000,3f0000,3f2000,3f4000,3f6000,3f8000,3fa000`.
All eight fall inside one current 64 KiB erase block. The wrapper then
reprograms only the selected descriptor's header (`2e69ba..2e69f8`), not
the other seven headers. This is direct evidence of a storage-composition
mismatch worth resolving, stronger than segment spacing inferred from an
image. It still does not identify the physical chip: the `b0/d0` identity is
the emulator's configured part, and its broad algorithm-family match is not
proof that this part is suitable for NAM-2. Recover a supported boot-block
part's identity/geometry before replacing it; do not change the shared
uniform-block device to fit these descriptors.

There is a concrete supported alternative, not yet selected by the machine:
Intel `0089/88c2` selects family 3 via the explicit record at `33d780`.
That branch copies `0x174` bytes from `33e1a8`, with erase entry `135249`;
unlike family 1, its routines issue `60/d0` unlock and `60/01` lock sequences.
Intel's [C3 datasheet, 290645-011](https://www.ersinelektronik.com/class/INNOVAEditor/assets/Datasheets/TE28F160C3.pdf)
identifies `88c2` as 16-Mbit top boot, with eight 8 KiB parameter blocks
and 31 64 KiB main blocks (sections 2.2, 3.3 and appendix F). All blocks
reset locked; a geometry-only substitute or the existing adapter's
acknowledge-only lock handling would not implement that contract.
The reference is retained locally as
`roms/reference-docs/Intel-28FxxxC3-290645-011.pdf`, SHA-256
`f5977db4c0c9c044dd1cc18503d6763d3095c2f645e2d4b74303299fdd229532`.
The overlay now provides a distinct generic `INTEL_28F160C3T` part with
geometry-driven erases and per-block lock/reset/status state, including the
WP# override of lock-down protection. Existing parts do not enable these
semantics. `driver/intelfsh_c3_test.cpp` is a development-only executable
conformance fixture, not a handset provisioning route:

```sh
make build
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame \
  --run-dir tmp/c3-conformance -- c3test -video none -sound none \
  -nothrottle -seconds_to_run 4 -log
```

It checks identity, power-up lock, rejected locked program/erase, NOR writes,
parameter/main erase isolation, lock-down with both WP# levels, reset and
save/load restoration of locks, pin, array and protection-register state.
Protection command `c0` separately programs user words `85..88` one-way;
factory words reject writes, and clearing protection-lock bit 1 cannot be
undone by programming or reset. These semantics come from section 3.4 of
the retained Intel datasheet, not a guessed handset response.

Read-query command `98` implements Appendix C's 56 low-byte values at word
offsets `10..47`, including `QRY`, Intel command set `0003`, x16 width,
2 MiB density, 31 lower 64-KiB blocks, eight top 8-KiB blocks and the
`PRI1.0`/protection descriptors. The fixture checks all bytes and zero upper
bytes, derives capacity from the region descriptors, checks identity/lock
reads, changes read modes, and restores query mode through save/load.
Fresh and cold fixture processes both produce the CFI pass marker, and the
NAM-2 security replay retains accepted pixels and own PMM. The table reports
the part's published capabilities, not implementation completeness:
program/erase suspension and measured operation timing remain unqualified.
This does not resolve the handset's idle wake or establish the fitted part.

Changed OTP state is serialized as an optional 18-byte big-endian tail after
the raw array in this C3 part's NVRAM file. Raw-array-only seeds retain their
existing format; the tail is written only after OTP changes. Partial or
oversized tails and an unlocked factory segment are rejected. OTP state is
included in save states but not cleared by device reset; ordinary block
locks are volatile and reset locked. The standalone fixture tests malformed
tail bounds, then a second fresh MAME process verifies the retained user
word `1200`, irreversible lock `fffc` and neighboring array data `5678`:

```sh
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame \
  --run-dir tmp/c3-cold -- c3test -video none -sound none -nothrottle \
  -nvram_directory ../tmp/c3-conformance/nvram -seconds_to_run 4 -log
```

Fresh and cold validation captures are
`tmp/c3-otp-banked-{first,cold}-20261020.stdout.log`. The original `noki2100`
configuration still uses its previous part; `nam2c3` is the explicit trial
described above, not proof of the acquired board's chip.

The retained MADos archive's `include/hw/ioports.h` independently names
the enable/disable pairs, but `hw/clock.c`'s attempted clock-source switch
is commented out and explicitly marked nonworking. It supplies no validated
idle-transition behavior to substitute for the unresolved own-ROM contract.

## Established contracts

- The normalized stock image occupies `0x200000..0x3effff` and executes from the
  ordinary DCT3 flash mapping in a 16-Mbit device window.
- Public NAM-2 flash maps reserve `0x3f0000..0x3fffff` for EEPROM/PMM. The
  supplied v5.84 MCU+PPM package ends before that partition, so it is not a
  complete product-state image.
- Direct static analysis resolves 652 MAD2 accesses from 326 literal seeds and
  recovers the familiar PUP, keypad GPIO and UIF register regions.
- The 18-entry CCONT descriptor vocabulary is byte-for-byte identical to the
  already decoded later-MAD2 table.
- Runtime establishes the GENSIO route independently: control offset `0x2d`
  receives `0x22`, CCONT transfer uses `0x2c`, receive status is polled at
  `0x6d`, and response data is read at `0x6c`.
- With that route configured, a ten-second run records 283 LCD command writes,
  8,741 LCD data writes and 22 complete display dumps without a soft reset.
- Runtime commands address banks 0 through 8 and ordinary bank transfers contain
  exactly 96 bytes. Board documentation specifies a 96x65 display, while the
  recovered text establishes reversed segment order. The modeled controller RAM
  is therefore 96x72 with a 96x65 viewport and mirrored X scan.
- The separately acquired complete v5.21 NAM-2 image uses a distinct DSP
  bootstrap: 992 alternating zero-valued handoffs on shared cells
  `0x0fe/0x100`, followed by reads of verdict cells `0x000/0x002/0x004`.
  Publishing those verdicts only after the measured count closes its DSP upload
  and yields the same service-control and discovery traffic as v5.84. It still
  converges on `CONTACT SERVICE`, so populated PMM alone does not supply its
  remaining version-local startup contract.
- NAM-2 v5.84 publishes its command-`0x64` application status only after accepting
  the peer registration. The constructor at `0x258ce0` derives its status byte
  from RAM `0x13fdb3` bit 6. Earlier notes called this bit 7 by reading
  `lsrs #7` as a direct index; the following carry branch tests original bit 6.
- The global initializer then evaluates seven predicates at `0x2f8396..0x2f83ca`
  and spins at `0x2f83e6` while any predicate is false. Before the terminal
  model, its first predicate `0x3003c8` returned zero.
- `0x3003c8` composes lower-idle check `0x2fd208` with queue-empty check
  `0x2be6c0`. The queue state is empty. The lower-idle check's RAM predicates are
  also clear, but it requires MAD2 FIQ-mask register offset `0x0a` bit 3 to be
  set before querying object `0x79`. Firmware reduces the mask from `0xff` to
  `0xe0` by 0.208 seconds and never restores bit 3, so this mask predicate is the
  first established failing condition.
- The four bit-3 setter regions belong to the MBUS lifecycle. `0x2f7f90`
  initializes the controller, `0x2f7c24` acknowledges FIQ3 and unmasks it to
  start activity, `0x2f7d2a` handles FIQ3 and remasks it, and `0x307758`
  services per-byte FIQ2 events. Public MADos material identifies FIQ3 as the
  independent 423.1 Hz `FIQ_MBUSTIM` source; modeling that clock makes this
  firmware-owned sequence run without a product-specific kick.
- The resulting transmitter emits the checksum-valid physical MBUS frame
  `1f ff 00 d0 00 01 01 01 31` at 9,600 baud. With no peer it retries the
  same frame. Reusing the DSP-framed D0 acknowledgement/completion semantics,
  including corrected service-node addressing, is rejected and still retries;
  packet-family resemblance is therefore insufficient evidence for a reply.
- Public M2BUS documentation establishes terminal node `0x1d`, type-`0x7f`
  transport acknowledgements, terminal startup `D0/04`, and phone response
  `D0/05`. The retained product-local terminal answers the organic request at
  the byte boundary using the documented 2.5/3 ms idle periods and request-
  derived ACK fields. NAM-2 accepts both transport ACKs and organically emits
  `D0/05`; `make verify-2100-mbus` fixes that complete exchange as an acceptance
  gate. Modeling peer-byte occupancy on status bit 6 prevents the controller
  from starting a frame over the terminal's character. Exact absolute phase is
  not gated: repeated clean runs place the first D0/01 at 0.222849 s, and the
  preempted supervisor does not consume that timestamp as a deadline.
- Closing M2BUS advances the first global-initializer predicate. The second
  predicate also passes; the third routine `0x2c9780` returns false because byte
  `0x10f206` is zero. The NAM-2 creation table at `0x330ef4` identifies
  `0x21d114` as task 18. Its initializer posts readiness report `0x12` at
  `0x21d218`. Two direct event producers at `0x2b7e7c` and `0x2b833a` construct
  status `0x120c` after SIM status word `0x91`, identifying it as the SIM
  proactive-command/SAT task. The coherent run leaves its entire
  state block `0x10f1d8..0x10f207` zero: task 18 remains unstarted rather than
  failing a transaction after initialization.
- Supervisor `0x2f80b6` contains the missing release explicitly. Its second
  batch starts tasks 10, 11, 13, 12, 14, 19, 18, 15 and 16 through scheduler
  entry `0x2ac3b0`. The coherent run takes the preceding failure branch at
  `0x2f8290`: service byte `0x10ec50` is already 1, startup byte `0x10fea1` is
  zero, and mode byte `0x10fd24` is 2, but helper `0x2e18a8` returns zero because
  `0x13fdb3` bit 6 has been cleared. The same shift/carry audit establishes that
  the helper's preceding spin tests original bit 2, not bit 3.
- The first batch resumes task 2 at `0x2f8204..0x2f8206`, then calls helper
  `0x2e18a8` at `0x2f822a`. Scheduler resume preempts the supervisor: it enters
  at 0.184789 s but does not reach the helper result branch until 1.197912 s,
  after task 2 and the service exchange have run. The failure
  branch calls `0x2f1c1c`, which publishes startup tuple `0x11/0x44` and returns;
  it does not register a deferred release. An exhaustive scan of all 28 Thumb
  calls to scheduler entry `0x2ac3b0` finds no other task-18 resume site.
- Task 2's initializer at `0x256720` owns the readiness-byte transition. It sets
  bit 6, reads two logical product-state locations through `0x306e78`, and
  retains bit 6 only when the live computed reference equals `0x0270` and the
  reference or `0x0190` is nonzero. The coherent erased-tail run computes
  `0x4cb2` while both locations read `0xffff`, so firmware deliberately clears
  bit 6 at `0x25689e`.
- The product-state reader exposes a linked catalog, not two physical records.
  The v5.21 donor has five `PMMCAT` segments plus an `EEPROM` segment whose
  logical payload begins at tail offset `0xc026`; logical address `N` is stored
  at `0xc026 + N`. The catalog entry type and family-local index must not be
  confused with the logical addresses consumed by `0x306e78`.
- Task 2 separately validates logical block `0x0000..0x011f`. It sums bytes
  `0x0000..0x011b` and compares the result with the big-endian dword at
  `0x011c`. The unmodified v5.21 donor stores `0x99a6`, while v5.84 computes
  `0xa0aa`; this is why the donor's pending-status slot `0x12` remained live.
  Correcting only that integrity dword preserves the donor-derived reference
  `0x933d` at `0x0270`, keeps nonzero `0x2d10` at `0x0190`, and retains
  readiness bit 6 without changing firmware RAM.
- Timer slot `0x18` belongs to task 2: its runtime descriptor names owner task 2,
  state 2 and delay `0x007d`. Expiry is delivered through task 2's ordinary
  timer/message queue. It is neither event `0x187d`, a service request, nor
  evidence of a supervisor retry.
- Shared selector `0x100e2=1` starts a finite ROM-owned DSP code-block transfer.
  Firmware publishes 133 intermediate replies `0x100e4=2`, then clears the
  selector and publishes final reply 4 on chunk 134. Re-publishing selector 1
  is incorrect because it restarts the transfer at its descriptor head.
- This later transfer is not the missing release timing. With the readable
  donor catalogue, supervisor `0x2f80b6` enters at 0.201823 s and the first
  code-block reply appears at 0.213180 s. A diagnostic 50 us peer cadence
  completes all 134 chunks by 0.228037 s without releasing task 18. The later
  supervisor trace proves scheduler preemption, rather than linear execution,
  gives task 2 time to validate product state before the branch; transfer
  throughput is therefore independently excluded.
- A write-watch fixes the ordering independently of the static decode. Task 2
  starts at 0.353336 s,
  sets readiness bits 6 and 7 at 0.353350..0.353352 s, and clears bit 6 at
  0.358995 s in `0x256a84`. At its eventual branch the stock run has service 1,
  mode 2 and readiness byte `0x81`, so the cleared bit is observed directly.
- The post-map class-`0x00`/command-`0x5f` `EXIT ANYSTATE` stream continues while
  the initializer spins. It is the already-classified periodic external-service
  channel, not proof that the missing task is advancing. A 120-second run still
  has the same blank frame and zero task state, bounding out a short scheduling
  delay.
- NAM-2 performs an organic five-row keypad scan. Its column mask remains `0x3f`
  at the blank frontier, so an injected host key reaches the physical matrix but
  is intentionally ignored by firmware. This is evidence that application/MMI
  initialization has not completed, not a keypad-device failure.
- The bounded BSI experiment used raw `0x0b6` and changed neither bit 6 nor the
  blank frontier. Its conversion of the manual's 0.5 V point assumed a 2.8 V
  ADC reference without evidence: VBB/pull-up supply is not the ADC reference.
  This excludes only that raw fixture, not correct physical BSI provisioning.
  NAM-2's fitted transfer and the applicability of the shared 1.5 V BSI
  reference remain to be established before choosing a replacement input.

The runtime GENSIO observation supersedes the conservative static census's zero
direct SELECT sites. That census excluded dynamic/table-derived addressing and
therefore established bounded absence only.

## Donor-profile boundary

The display, DSP bootstrap, service discovery, application registration,
keypad wiring, MBUS controller, terminal timing, arbitration and startup
exchange are established for v5.84. Matching product state remains the first
unclosed input to ordinary MMI settlement. The bounded research fixture
preserves every v5.84 executable byte and borrows only the v5.21 PMM partition.

The corrected composition establishes these negative facts:

- the v5.84 logical-PMM reader returns erased data for the security-code record
  at logical `0x110..0x112` and the eight-byte verifier record at
  `0x668..0x66f`;
- the credential reader therefore takes its ROM fallback, `12345`, rather than
  reading the donor's visible BCD field;
- verifier initialization at `0x2ccaf0` calls the firmware cipher at `0x2fe780`
  with packed digits `05 12 34 50`;
- the identity preparation path `0x2e1918 -> 0x291528 -> 0x306f36` supplies the
  sixteen-byte string `00115<<00004014\0`, including two non-decimal nibbles;
- initialization derives verifier `d3 34 3e d8` from identity string
  `00115<<00004014\0`; physical submission later derives `d7 36 3b df` from
  `00116=000000243\0`. Both cipher calls use packed `05 12 34 50`, so the
  mismatch is identity/FAID state rather than a different entered code;
- the comparison at `0x2fe992` rejects the derived bytes and paints
  `Wrong code!` organically.

Those observations quantify the missing input without inventing it: v5.84
requires its own PMM catalogue/descriptors, logical records `0x110` and `0x668`,
and coherent identity/FAID material consumed by `0x2e1918`. The held archives
contain none of those v5.84 product bytes. Their values cannot be derived from
the v5.21 donor or from the reject branch; a matching PMM/full-flash capture is
required.

An earlier version of the fixture copied the donor's complete two-megabyte
flash image into MAME's persistent flash NVRAM. Because restored flash NVRAM
overrides the selected BIOS, those experiments silently executed v5.21 code
despite selecting BIOS `584e`. Their 281-byte identity-block census and
alternate-credential results therefore do not establish v5.84 verifier
semantics and are retired. The corrected generator rejects unexpected image
sizes, preserves bytes `0x000000..0x1effff` from v5.84 verbatim and imports only
the donor PMM partition at `0x1f0000..0x1fffff`. Unit tests protect that
composition. The first corrected run accidentally selected a stale
higher-numbered frame from the shared run directory, so it did not independently
prove the result. The gate now removes old snapshots; a fresh isolated run
reproduces both cipher calls and the same `Wrong code!` frame.
`make verify-2100-interactive` reproduces this bounded compatibility failure
through physical keypad input without firmware-state forcing.

The version-mismatched donor remains a diagnostic input rather than a
distributable v5.84 product profile. Inventing replacement state from the
invalid initialization result would be state forcing. The supplied archive
`2100sharp.pmm` is also not v5.84 PMM data:
it consists of 256 nine-byte flasher headers followed by 0x2000-byte payload
chunks, and stripping those headers produces the byte-identical v5.21 Sharp
full image catalogued as `2100f521sharp.fls`. The earlier experiment that loaded
only the MCU+PPM length and reported erased donor locations is discarded.

The emulation frontier is accepted product-state validation before idle MMI.
Promotion still requires a matching v5.84 product-state capture; the
later selector-1 code-block upload is fully bounded and is not a substitute for
that input.

Historical repair archives identify
`eeprom2100.fls` and 64 KiB 2100 PMM attachments, but no retrievable copy has yet
been recovered. The correct next input is a matching virgin or handset capture,
not a driver-side value synthesized to satisfy the comparison.
SIM and radio remain dormant at this boundary; their behavior must not be
promoted until execution reaches their firmware consumers.

The strongest surviving retrieval leads are exact but account-gated. The
GSMHosting thread exposes attachments `2100[1].pmm.txt` and
`2100_edited_bestcool.pmm.txt` (64.1 KiB each). The Elektroda archive lists
`2100fubu511pl.zip` (1.18 MiB), described as a v5.11 full backup. The historical
`ALL_PM_PMM_by_xTroy.zip` inventory names `eeprom2100.fls`, but its surviving
MCRF URL now returns 404. None of these bytes has been recovered, so they remain
retrieval leads rather than evidence inputs.

A collection audit on 2026-09-22 scanned all 39 local raw `.fls`, `.bin`, and
`.pmm` images with the PMMCAT parser. The only 2100 catalogues are the five
segments in `2100f521sharp.fls` and the same five segments inside
`2100sharp.pmm`'s flasher container. The v5.84 archive lists only
`NAM205.840`, `NAM205.84E`, and that v5.21 Sharp container. No untested local
2100 PMM candidate exists, so this audit does not authorize a new boot fixture.
An additional public listing advertises a 2 MB NAM-2 v5.84 service-box file,
but the download is subscriber-gated and its contents are unknown; size and
version label alone do not establish that it contains a matching PMM tail.

## Public evidence availability

Contemporary service references independently identify the NAM-2 partition map
and the need for product PMM data. Repair tooling describes PMM as the virgin
EEPROM input used after MCU+PPM flashing, and one surviving archive inventory
explicitly lists `eeprom2100.fls`. An archived repair discussion names a
`2100FuBu0584` full backup, but its attachment is no longer available. Another
archive records that a donated virgin EEPROM still produced `CONTACT SERVICE`,
consistent with the requirement to rebuild handset-specific FAID/identity data
after grafting. These sources prove that suitable captures existed; they do not
provide bytes that can be used as the v5.84 oracle.

The surviving v5.84 download trail was checked by exact archive name and object
identifier. Its mirrors all resolve to
`proshivki.org__nam_2_v5.84.rar` (Shareflare object
`8272.89d0d3c4a214639b1e8d3eba69`, also mirrored by Letitbit) and consistently
describe a 1.11 MB `mcu+ppm(e)` package. That is the already-held Wintesla
payload, not a two-megabyte full flash. No indexed copy of the missing PMM tail
or the named `2100FuBu0584` backup was found. The Internet Archive CDX endpoint
was also unreachable during the check, so archive recovery remains a possible
external follow-up rather than negative proof that no capture survives.

- [DCT3 flash address table](https://www.nokia-tuning.net/index.php?s=flashadress)
- [Archived NAM-2 EEPROM discussion](https://nokiafree.org/forums/archive/index.php/t-17045.html)
- [Archived PMM collection inventory](https://www.mcrf.ru/forum/archive/index.php/t-22889.html)
- [2100 PMM attachment record](https://forum.gsmhosting.com/vbb/f131/2100-contact-service-234791/)
- [2100 v5.11 full-backup attachment record](https://www.elektroda.pl/rtvforum/topic162542.html)
- [Archived `2100FuBu0584` reference](https://gsmforum.ru/threads/podskazhite-kak-podnyat-ufsom-nokia-2100.6670/)
- [Surviving v5.84 MCU+PPM listing](https://www.shram.kiev.ua/mob/proshnokia.shtml)
- [Subscriber-gated 2 MB v5.84 service-box listing](https://www.telecelula.com.br/new/download_Procurar.asp?cod_categoria=7&cod_fabricante=2&pg=63)
