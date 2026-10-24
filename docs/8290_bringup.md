# Nokia 8290 / NSB-7 contracts

## Current boundary

The qualified input is own NSB-7 v5.22 MCU/PPM-A. Product profiles have
different acceptance boundaries; research results never promote the normal
machine implicitly.

| Profile | Demonstrated boundary |
| --- | --- |
| `noki8290` | Fail-closed supervisor and exact CONTACT SERVICE frame; DSP/service replies disabled, resident masks undumped. No phone-service acceptance. |
| `nsb7stage` | Executes own verifier/loaders to missing resident DSP routine `2c75`; no fabricated completion. |
| `nsb7hle` | Explicit own-upload-to-runtime HLE, own checksum/retry fixtures and nominal ADC2 hypothesis; physical and laboratory services qualified below. |

| Research-HLE contract | Qualified coverage |
| --- | --- |
| UI and storage | Physical security code, menus, SIM phonebook save/cold retrieval, RTC retention and Net monitor00 clean idle. Physical rail-off/PWRONX restart retains RTC/card and reauthenticates before warm registration and menu recovery. Own EEPROM journal rotation matches the complete runtime shadow. |
| SIM security | PIN1/PIN2 change/rejection, retry persistence, exhaustion/PUK recovery and request Off/On. Permanent PUK1 rejects the card; permanent PUK2 retains ordinary PIN1/menu use. |
| FDN | Explicit extended card; physical PIN2-authorized On/Off, Save/edit/erase/cancel and cold retrieval. Registered listed/unlisted/Off controls distinguish permitted calls from local denial. |
| SIM Toolkit | Phase-2+ nine-byte TERMINAL PROFILE; DISPLAY TEXT, GET INKEY, GET INPUT, SET UP MENU, SELECT ITEM, menu-driven SEND SHORT MESSAGE and SET UP CALL accept/Cancel. Exact physical/protocol/UI acceptance; non-SMS cases preserve the full card, SMS permits only its exact EF_SMSS reference update. |
| PCS laboratory network | Acquisition, BCCH, Location Updating/EF_LOCI, background measurements, addressed paging, physical incoming/outgoing CC/RR signaling and SMS delivery/read/delete/cancel/Reply. |
| Authenticated composition | Physical PIN before fresh/cold registration, calls, SMS and phonebook; a long wrong-PIN control remains unregistered. |
| Restoration | Exact ARM/banked registers, RAM/time, device/protocol state and pixels at idle, call, active SMS and open Toolkit-menu boundaries, with physical continuation. SIP connected and unanswered phases invalidate the old external dialog rather than replay it. |
| External host | Incoming SMS/read and outgoing Reply accept/reject/silence decisions with physical failure recovery; incoming/outgoing local SIP with documented HLE PCM and actual microphone/earpiece waveforms. Four call-phase restoration gates preserve handset state, dispose external transactions and reject replay. |

All provisioning is own-derived or explicitly synthetic external card data;
no donor PMM, identity or firmware-state forcing is admitted. Native resident
DSP execution, native speech, measured complete analog calibration and external
RF remain unproved. HLE SIP/COBBA media and sustained endpoint waveforms are
qualified separately from native execution. Host busy and unanswered-alerting
physical recovery are qualified; arbitrary failure/pending-call cases,
AoC and further SIM Toolkit commands are not established by these gates. The supplied
SET UP EVENT LIST and TIMER MANAGEMENT fixtures are explicitly rejected, not supported.
Detailed contracts
and their reproduction commands below own each coverage claim.

### Toolkit capability rejection controls

On the authenticated registered `nsb7hle` profile, the supplied SET UP EVENT LIST
(user activity/idle events) and TIMER MANAGEMENT (start timer 1 for five seconds)
commands both return terminal-result byte `31`. Each capture completes five
FETCH/TERMINAL RESPONSE exchanges and one physical menu-selection ENVELOPE;
neither emits an event/timer download ENVELOPE or starts a network call/SMS.
Observation continues through 108 seconds, followed by physical End-key recovery.
The complete card and exact menu/idle frames remain unchanged. These results
qualify rejection of these particular requests, not every variant of either command.

Use `tools/nsb7_toolkit_capability.lua` with
`fixtures/nsb7_toolkit_events/nsb7hle.cfg` or
`fixtures/nsb7_toolkit_timer/nsb7hle.cfg`, respectively. Validate isolated captures:

```sh
.venv/bin/python tools/nsb7_toolkit_check.py roms/noki8290/8290f522a.fls RUN_DIR --events-rejected
.venv/bin/python tools/nsb7_toolkit_check.py roms/noki8290/8290f522a.fls RUN_DIR --timer-rejected
```

The [Nokia 8290 user guide](https://www.instructionsmanuals.com/sites/default/files/2019-05/Nokia-8290-en.pdf)
identifies NSB-7/GSM1900; the [NSB-7 service manual](https://www.manualslib.com/manual/2171641/Nokia-Nsb-7-Series.html)
provides independent hardware-reference evidence, not missing provisioning.

## Qualified package contract

`roms/archive-dct3-packages/nsb7_5.22.exe` is a ZIP self-extractor with five
members, not the adjacent-gzip installer format. Its SHA256 is
`5c28267a14e5d3e36ed4dc166987da42e07b04a83ff88499dc38ee4485df0040`.
The own restore INI selects `AMERICA_A`, names both phone streams and gives
erase limits `200000..3cffff`. Both phone headers identify `V  5.22`,
`08-11-01`, `NSB-7`; the PPM identifies pack A.

| Member | CPU start | Bytes | Decoded SHA1 |
| --- | --- | --- | --- |
| `nsb7___5.220` | `200000` | `160000` | `6f2c32bfdb5dce75019f85ac56271fbf05a216cf` |
| `nsb7___5.22a` | `360000` | `70000` | `e09921c52e90888992792eceb57d2241f4eb091c` |

The joined image is `1d0000` bytes, SHA1
`4dacc50f57fe295168a53d3e5855a81acf35ab96`, with no padding or donor tail.

```sh
.venv/bin/python tools/nsb7_package_check.py \
  roms/archive-dct3-packages/nsb7_5.22.exe \
  --output roms/noki8290/8290f522a.fls
```

## MCU baseline and direct-access boundary

The ARM entry at `200040` selects the `20000` MAD2 window. A constant-index
MMIO census finds 243 literal seeds and 747 resolved candidate accesses;
these are linear-scan candidates, not exhaustive control-flow ownership.
The reviewed CCONT read path selects `22` at `305796`, writes `2c` at
`3057b4`, polls `6d` bit 2 at `3057b8`, and reads `6c` at `3057c0`.
Thus the byte-write-ready GENSIO mechanism is selected independently.

The six-second sample is in supervisor `2f82fe..2f8304`, with global
control `05`; shared observations remain `0000,ffff,0001,0001` at
`10000/10004/100fe/10100`. These are observations, not a DSP success verdict.
The 84x48 presentation matches the failure text, but keypad wiring and the
remaining board defaults are provisional, not complete NSB-7 hardware acceptance.

```sh
make build
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame \
  --run-dir tmp/nsb7-boot -- noki8290 -rompath ../roms -video none \
  -sound none -nothrottle -seconds_to_run 7 -log -verbose \
  -autoboot_delay 0 -autoboot_script ../tools/nsb7_boot_observe.lua
.venv/bin/python tools/nsb7_boot_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-boot
```

Use a fresh run directory for the erased-storage baseline. The checker pins
own source bytes, sampled continuation/shared state and exact RGB failure
pixels. It does not test task messages, input, native DSP or phone services.
The own native-upload and transport-HLE evidence below remain separate from
this normal-machine baseline.
A sibling's working HLE or EEPROM is not evidence for those contracts.

## Own identity-6 native upload boundary

`nsb7stage` is a separate research composition, not the normal machine.
It selects identity 6 from the own compiled fragment, a declared unmeasured
13 MHz execution clock and the own reset-release bit-0 path. No resident
mask is supplied, and identity 6 is not a measured fitted-part assertion.
The MCU routine at `2cb2e4..2cb2f2` compares the identity with 6 before
selecting one of two descriptor roots. Both branches exist in this image;
only the identity-6 composition is qualified below.

| Own descriptor (file offset) | Destination / words | Payload SHA1 |
| --- | --- | --- |
| `11ee88` verifier | `0f00` / 223 | `6646da3c5be9c70deda7e0b5b9f257d5d2ace815` |
| `11dca0` program fragment | `ff80` / 104 | `440bf49f1eba4cadb12f7f7581c992b0025807d6` |
| `1183a4` support upload | `fd00` / 638 | `70c931c40b3e7cdebc8d09f2488e3cc1e3c848d9` |
| `11ddcc` second loader | `0a00` / 623 | `89080bea5785eb944586574a3e1463145d8d4460` |

The support upload is not the second loader: the latter's descriptor is
`0a00,1000,026f,0200,03e8,0000`, and the staged device compares every one of
its 623 words against the own source before entry. The extraction tool's
`--product 8290` verifier/fragment/loader modes pin the corresponding source
hash and descriptor; `--loader` denotes the support upload, consistent with
that tool's existing contract.

Native execution releases the 223-word verifier twice, publishes
`0000/0006/0006/0006` at DSP PC `0f64`, executes the 126-word first loader,
verifies the complete 623-word second loader at `0a00`, then stops at absent
resident program address `2c75`. This is not a returned success or completed
runtime handshake. No substitute opcode, return or runtime HLE is enabled.

```sh
.venv/bin/python tools/nsb7_native_boundary_check.py \
  mame/mame roms tmp/nsb7-native-boundary
```

The fresh-directory runner requires the ordered native publications and
controlled missing-code stop. The MCU failure-frame baseline remains separate.
Native continuation itself requires an authentic resident source; the
separate runtime HLE contract below does not remove that boundary.

## Runtime transport and compact self-test

`nsb7hle` explicitly suspends native execution at `2c75` after the own
uploads, then enables shared service transport and request-correlated D0
discovery. It emits `1eff00d000030101e000`, consumes the D0/04 response and
acknowledges it with `1e0200d0000305014100`. Setup requests `13/14/15/16`
precede `70:0d00`.

The own class dispatcher `24b81a..24b844` resolves class `74` and calls
`248b00` at `24b856`. Its command cascade selects `0d` at `248b74`.
Flag `13fde1` bit 2 arms this path; it cancels timer `19`, clears fault
index `0f`, and consumes reply byte `+9` bits 0/1 for fault indices `10/11`.
That contract qualifies the existing compact request-correlated `74:0d00`
HLE response, not a silicon measurement or security-record verdict.

Runtime observation sees flags `84` and faults `ff,ff,ff` at consumer entry,
then flags `80` and faults `00,00,00` at the join `248c28`. Firmware submits
the `0a09` follow-up before that traced join. Neither the transport nor this
consumer result establishes idle: the display still shows CONTACT SERVICE.

The remaining concrete failure is independent of the three cleared faults.
Writer `248948` sets fault index `0c` to `0c` after checksum comparison
`24892c..24893e`: computed `bd42`, stored logical EEPROM word `1e0=ffff`, and
marker word `170=ffff`. Helper `2486dc` sums `c0` bytes from logical offset
`120`, then subtracts both bytes of word `154` through helper `2e3eb4`.
These are offsets in a 32 KiB shadow, not record IDs or physical flash
addresses. The unprovisioned comparison fails equality; the separate nonzero
guard would pass for these values. This is not evidence for another missing
DSP reply.

```sh
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame \
  --run-dir tmp/nsb7-service -- nsb7hle -rompath ../roms -video none \
  -sound none -nothrottle -seconds_to_run 9 -log -verbose \
  -autoboot_delay 0 -autoboot_script ../tools/nsb7_service_observe.lua
.venv/bin/python tools/nsb7_service_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-service
```

The observer is bounded and read-only. The checker requires the native
handoff, discovery, actual response consumption and still-failing own
checksum comparison; it cannot pass as normal-machine boot acceptance.
Do not transplant another product's PMM, force the fault clear, or answer an
unobserved identity/record query to bypass this check.

## Logical EEPROM replay and checksum-only fixture

Reader `30d658` calls `2cdd08`, which bounds `offset+length` by `8000` and
copies from RAM base `10d5c8` (literal `2cde3c`). The persisted own flash
contains an EEPROM journal at file offset `1d0000` (CPU `3d0000`), with
header word `+18=00c8`. A complete initial 32 KiB record at `+20` is followed
by ordinary write records. Replaying the first 64 KiB window, including all
11 initial-run records and their byte checksums, matches every byte of the
runtime shadow. This does not establish the fitted flash part or its erase
geometry; `00c8` is an observed header field, not an imported NSE-5 version.

Long cold security sequences independently exercise journal rotation. After
988 records filled the first sector to relative `ff8a`, firmware retired its
`EEPROM` header and published a fresh complete-shadow journal at file offset
`1e0000` (CPU `3e0000`). A separate cold capture replayed all 32768 shadow
bytes exactly from that second sector; calibration and prefix checksums
remained valid. `nsb7_storage_check.py` therefore checks both observed 64 KiB
slots and requires exactly one live `EEPROM`/`00c8` header. It does not guess
generation order when both headers appear live. Fixture append addresses
follow the selected sector; the retired sector remains unchanged.

The own erased calibration span independently computes `bd42` using
`sum(shadow[120:1e0]) - sum(shadow[154:156])`, modulo 65536. The explicit
research fixture appends only `08e001e0bd42` at file offset `1d80e6`.
The header encodes a two-byte write, valid record checksum and logical
destination `1e0`. All executable bytes and every logical byte except the
two application-checksum bytes remain unchanged. No calibration values,
identity, marker, credential or firmware RAM state are manufactured.

```sh
# Fresh unprovisioned capture; omitting verbose avoids the large CPU trace.
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame \
  --run-dir tmp/nsb7-storage -- nsb7hle -rompath ../roms -video none \
  -sound none -nothrottle -seconds_to_run 7 -log -autoboot_delay 0 \
  -autoboot_script ../tools/nsb7_storage_observe.lua
.venv/bin/python tools/nsb7_storage_check.py \
  tmp/nsb7-storage/nvram/nsb7hle/flash \
  --shadow tmp/nsb7-storage/nsb7_nv_shadow.bin \
  --trace tmp/nsb7-storage/error.log \
  --checksum-fixture tmp/nsb7-checksum/nvram/nsb7hle/flash
```

Boot that generated external input using the same observer and run directory.
The own comparison reads `bd42/bd42/ffff` and takes success store `248942`,
clearing fault index `0c` organically. Independent replay still matches all
32 KiB after the new boot and a separate cold process using only that own
retained flash; the accepted checksum persists without another fixture write.
The calibration-only fixture's screen remains CONTACT SERVICE: checksum
validity is not sufficient to establish idle, nor does it make erased
calibration into authentic factory data. The unprovisioned baseline remains
protected separately. The independently required prefix checksum is defined
below; a guessed DSP completion cannot replace either integrity contract.

## Prefix checksum and complete startup-result table

Initializer `2e95b6..2e95c4` sets 29 bytes at `13fbe0` to `ff`.
The observer and unprovisioned service checker cover all 29, not only the
first 18 around the compact self-test. With only the calibration checksum
corrected, index `12` (hex) remains `12`. Its first writer is `2e8ffc`,
which stores `fd` when `29656e` returns anything other than one; `2488cc`
then converts that failure to the index value `12`.

Reader `29656e` copies logical bytes `0000..011f`, calls `2f1db0` with
length `011c`, and compares its result with the big-endian 32-bit word at
`011c`. The checksum helper accumulates bytes modulo 65536. For the own
erased prefix this produces `00001ae4`, versus stored `ffffffff`.
No identity-field semantics are inferred from that integrity check.

The separate `--prefix-checksum-fixture OUTPUT` option appends
`101b011c00001ae4` to the own journal, retaining every logical byte except
the four checksum bytes. In particular it does not create an IMEI, phone
code, SIM identity or calibration. Use the previously qualified flash and
matching shadow with `tools/nsb7_storage_check.py`; outputs must be fresh.

Firmware then compares `00001ae4/00001ae4`, returns one, and stores zero at
result index `12` without the failure conversion. Both checksum comparisons
and full-shadow replay remain valid after a cold restart with the own
retained flash. Flags at six seconds are `40`; the rendered frame is blank
rather than CONTACT SERVICE, and the frame remains blank at 15 and 30 seconds
with control `05` and flags `40`. Neither that flag nor blank pixels establish
idle or input delivery. This checksum-only negative control lacks the nominal
ADC input qualified below. The own keypad map and its physical acceptance
are separate contracts; the normal erased-storage and native missing-code
boundaries remain unchanged.

## Keypad contract and full-scale-input negative control

Own scanner `308240` scans five rows with no row-pin shift. Its normal
table at `343d7c` and special table at `343d98` match the existing five-row
logical host layout; the special Power contact is column mask `10`.
This table equality supports input reuse, not broader board equivalence.

With both checksum-only repairs, read-only debugger observations confirm
initializer `30835c` executes. MMIO write taps record initialization at
1.129 seconds, a scan at 1.154 seconds, and a later write at `30ada8` at
1.224 seconds setting column mask `6b` to `3f`. Routine `30ad82` explicitly
ORs its low five bits with `1f`; this is firmware-owned masking, not a host
input failure inferred from missing logs. Physical Menu and digit-1 presses
at eight and ten seconds leave the columns masked and produce no observed
scanner IRQ or decode breakpoint. Global control remains `05`.

`tools/nsb7_input_observe.lua` reproduces those physical contacts and
read-only mask/initialization observations with the debugger enabled. It
does not unmask columns or write guest state. This masked-input control is
not the selected nominal-ADC research profile; organic input delivery is
qualified under that profile below.

The saved return address at the mask write is `29eee7`, identifying direct
caller `29eee2` (the other direct BL sites are `29ee14` and `29f0d6`). This
is the branch selected by the startup result `2`; it resets the state
through `301ed0`, then receives readiness reports. Its continuation at
`29efa2` requires the low nibble at `13ff00` to be `6` and that at `1308b1`
to be `0f`. Only after those tests does it call `30acf2` at `29efde`.
Runtime writes establish state `6` at 2.189 seconds and readiness `0e`
at 2.853 seconds. The report decoder maps `14/16/15/17` to bits `1/2/4/8`;
the missing bit therefore belongs to report `14`, not a guessed key event.
This identifies the report-14 dependency of the full-scale-input control,
not a missing report in the qualified nominal-ADC profile.

`tools/nsb7_keypad_conformance.lua`, on a separate fresh `noki8290` run,
tests 20 physical matrix contacts against all five row drives (100 scans),
plus the Power special contact and release. It writes controller MMIO and
is explicitly conformance evidence, not organic input delivery. Those
checks pass; the normal failure-frame oracle and native `2c75` controlled
stop also reproduce after the keypad configuration change.

### Analog report-14 ownership

Own wrapper `301f6c` clears its local diagnostic flag, then posts task
`1` / report `14` through `289a38`. An aligned Thumb BL scan of the entire
qualified image finds one direct caller, `2463b2`. This does not exclude
indirect or table-driven publishers. The call is in the analog routine
containing the firmware strings `VBAT Checks` and `Start limited fast VBAT
read`; no DSP reply is inferred from the report number.

Its 21-entry state table is at `247ab8`; state is the halfword at
`130020` (context `r6+1c`). Runtime writes with the current default board
inputs enter `11` at 1.228 seconds, `4` at 4.691 seconds, then `3` at
5.155 seconds. Repeated state writes are active dispatch, not evidence of
an inactive task. Read-only observation of the firmware's own text routine
`30ae82` independently identifies `CHECK TEST MODE`, `CHECK POWER ON REASON`,
`INIT CHARGING`, then `BOOT UP CHARGE`, `WAIT CHARGER VOLTAGE SETTING`, and
`CHARGER DISCONNECTED`. These strings name software branches, not proof
that the physical charger input was asserted.

State 3 entry `2472e0` distinguishes events `49` and `41`. On `49`, it
tests its countdown and context byte `+0d`, then predicate `305800` before
proceeding. The nominal channel-2 input below allows organic readiness;
report `14`, keypad unmasking and charger causes must not be synthesized.
The full electrical transfer and other raw analog defaults remain
provisional. The NSB-7 service manual independently describes BLB-2 battery
identification and a 100 kohm BSI pull-up, but does not establish the current
emulator tuple as a measured NSB-7 electrical profile.

### Nominal VBATT and physical UI acceptance

`nsb7hle` alone supplies ADC channel 2 = `554`, a declared nominal
3.6 V / 6.5 mV-per-bit CCONT transfer hypothesis. It replaces only the
full-scale channel-2 placeholder; BSI, BTEMP, all other ADC inputs, charger
status, executable bytes and the checksum-only EEPROM contents are unchanged.
This is not measured NSB-7 calibration or a recovered complete battery model.
The normal `noki8290` and native-only `nsb7stage` profiles retain their
previous inputs and acceptance boundaries.

This single input change produces readiness `0f` at approximately 3.391
seconds. Before the scripted physical contacts the keypad mask is `20`.
Menu at eight seconds produces IRQ0, raw matrix position `06`, logical
key `19`, and an `Insert SIM card` frame. Digit-1 at ten seconds produces
IRQ0, raw `07`, logical `01`, and the dial editor displaying `1` with
`Clear`. The early six-second frame is still blank; it must not substitute
for the later physical-input evidence. The analog task still visits charging
branches later, so the result does not validate the whole power lifecycle.

Both physical-contact frames and the ordered readiness/IRQ/scan/decode
evidence reproduce in a new process using the preceding run's own retained
flash. `tools/nsb7_input_check.py IMAGE RUN` requires the explicit runtime
HLE handoff, complete readiness sequence, enabled pre-press masks, decodes
between each physical press/release, and exact pixels for both frames.
It rejects normal/native profiles and makes no SIM, RF or speech claim.
Run `tools/nsb7_input_observe.lua` with `-debug -debugger none`; seed only
the own flash containing the two checksum-only repairs described above.

The status retry's `9004` query resolves index `10` in table `343ee0` to
serial command `70`, CCONT register `0e`, mask `04`. That is a status query,
not an ADC sample, and was left untouched. The own SIMI activation and
card transport are qualified separately below, without an application-level
SIM reply.

## SIMI and physical card boundary

The own constant-index census resolves 54 candidate accesses in the
`36..3f` register window. Reviewed initializer `2eef68` clears IIR `38`,
sets clock register `3a` to `03`, enables CTSI `0d` bit 5, programs RX/TX
FIFO controls `3d/3e`, and writes control `39=32`. Receive handler `2eeee6`
reads/acknowledges IIR `38`; its `40` cause drains RX fill `3c` through data
`37` at `2eee80`. The controller/card are enabled only in `nsb7hle`;
the fail-closed normal and native-only profiles remain unchanged.

With the existing standards-oriented synthetic card, firmware subsequently
writes control `33/b3`, receives ATR `3b1005`, exchanges PPS `ff00ff`, and
issues SELECT, GET RESPONSE, READ BINARY and READ RECORD commands through
the real controller byte path. Own wire observations contain 938 TX bytes,
including MF/DF and ICCID/phase/IMSI reads, all 50 ADN records, and a final
`a0f2000016` presence-monitor request. No APDU or firmware message is
injected. The capture is bounded below its 4096-byte TX and 8192-byte RX
limits. This establishes startup reads, not successful credential checks,
SIM update commands or a complete SIM feature matrix.

The physical Menu fixture submits an empty phone-security editor and
produces `Code error`; digit-1 then produces `Security code: *` with
`OK/Clear`. This is phone-side security presentation, not SIM PIN failure
or proof that a stored phone code is accepted. The own phone-code/retry
record contract is qualified below without a firmware-memory patch or a
sibling's security bytes.

`tools/nsb7_sim_observe.lua` retains the configured physical card, runs the
same two contacts, and records read-only controller traffic.
`tools/nsb7_sim_check.py IMAGE RUN` checks the explicit HLE boundary,
activation/FIQ consumer, complete captured TX framing, startup commands,
ADN record reads, ATR/PPS reception, final STATUS and exact security-editor
pixels. It reproduces in a new process retaining only the preceding run's
own flash and synthetic-card NVRAM. The empty-socket input observer instead
asserts the physical `Remove SIM card` contact at startup and continues
to reproduce its separate `Insert SIM card` / dial-digit acceptance.

## Phone-code and erased retry contract

Own reader `295a28` reads three packed bytes at logical EEPROM offset
`0110`, expands their nibbles, and checks the first five decoded characters.
At `295a72`, invalid digits select ROM string `12345` at `295e08`; return
value zero records that fallback. Runtime observes input `ffffff` and
output `3132333435`, not a provisioned or borrowed phone code.

Initializer `2c8e7a` calls this reader and encodes the result through
`302c5c` into runtime record `130a8c`. On physical `12345`, validator
`302e78` observes input `313233343500` and equal encoded/stored words
`d83b30d8`. Its subsequent retry guard `302e24` nevertheless rejects counts
greater than or equal to five. The initial eight-byte record is read from
logical `09d4`; byte `09d8`, corresponding to runtime `130a90`, is erased
`ff`. This is independently observed at the NV reader and retry consumer.
It is not a failed character decode or mismatched code.

`tools/nsb7_storage_check.py --security-retry-fixture OUTPUT`, with the
matching own runtime `--shadow`, appends short journal record
`04e109d800`: only logical byte `09d8` becomes zero. Zero is the retry-clear
value used by the firmware at `302e64`; it is not a guessed code or lock
mode. The fixture leaves every identity, packed-code and encoded-code byte
unchanged, along with both prior checksum fields. It refuses a non-erased
retry byte, unknown source image, mismatching shadow or occupied append
space. This is explicitly a generated research fixture, not authentic
factory provisioning.

`tools/nsb7_security_observe.lua` physically enters digits 1..5 and Menu at
8..13 seconds. The unchanged erased-retry control shows equal encoded words,
retry `ff`, and `Code error`. The one-byte fixture shows the same encoded
words, retry `00`, and `Code accepted`. Both exact frames are checked by
`tools/nsb7_security_check.py IMAGE RUN` (add `--rejected` for the negative
control), alongside each physical key decode and independently replayed
own code/retry storage. Acceptance repeats in a new process retaining the
preceding run's own flash and card NVRAM.

The post-acceptance frame has `Menu/Names` softkeys and a radio-status
overlay in the main area (source-text contract below). Do not call it
an ordinary idle or registered screen until Net monitor is disabled as
qualified below. Physical menu
activation works independently, as qualified below; no security verdict or
UI state is injected.

## Physical menu acceptance

After the same physical security sequence, Menu at 26 seconds opens the
firmware's `Messages` menu. Down at 29 seconds selects `Call log`; Menu at
32 seconds opens its `Missed calls` item. Text, icons, scrollbar and
softkey labels render coherently on all three screens. Their corresponding
logical decodes are `19/18/19`. This distinguishes the idle radio-status
overlay from a general LCD/controller or keyboard-navigation failure.

`tools/nsb7_menu_observe.lua` composes the security observer and those
physical contacts. `tools/nsb7_menu_check.py IMAGE RUN` requires the
security comparison and retry evidence, each decoded menu contact, and
exact RGB frames for all three views. The same frames reproduce in a new
process retaining the preceding run's own flash and card. It does not
establish call history, messaging services, a usable idle screen or RF.

The radio-status overlay is Net monitor test `01`. Physical selection of
test `00` removes it and persists across a cold restart, as qualified
below. No LCD/font or network-presentation workaround is needed. The
individual setting-bit semantics and erased-input initialization remain
undecoded; the physical menu is the supported settings path.

### Firmware-owned clock setting and RTC retention

`tools/nsb7_clock_input.lua` navigates the physical Settings/Clock/Set the
time controls, enters `13:47`, confirms, and returns through Back. The
editor displays the intended digits and the firmware confirms `Time is
set`. The idle top-right clock then displays `13:47`, while the main-area
radio-status overlay remains. A valid clock therefore does not by itself
disable Net monitor; its physical control is qualified below.

Independent journal replay observes a firmware-written opaque word at
logical `0408` (`ffffffff -> 91055b00`) plus several setting bytes. Do not
infer timestamp units or a complete settings format from that word.
In a new process with the same own flash and card but fresh CCONT state,
the firmware asks for time again. Retaining the preceding run's own
nine-byte CCONT NVRAM instead restores `13:47` without time-entry keys.
The CCONT counter minute/hour bytes agree with the two UI outcomes;
retained controller bytes are not a donor handset configuration.

`tools/nsb7_clock_check.py SETTING RETAINED FLASH_ONLY` requires security
acceptance in each run, physical time-entry decodes and confirmation
pixels, independently replayable own flash, the CCONT minute/hour
contrast, and the fresh-controller time prompt. The retained-run pixel
check deliberately covers only the top-right clock `(52,0)-(78,8)`;
this clock-only gate makes no full-frame claim. The separate Net monitor00
and settled-idle gates qualify clean idle pixels. Clock setting does not
establish calendar or alarm support. No device behavior, RTC seed, NV field or guest RAM was patched
to make the time-setting transaction succeed.

## Persistent SIM phonebook acceptance

The physical Names/Add entry workflow saves contact `A / 12345`. It
produces exactly one complete `a0dc010420` UPDATE RECORD through SIMI and
the shared card's normal APDU receiver, followed by `Saved to SIM card`.
The first 32-byte ADN record is:

```text
41ffffffffffffffffffffffffffffffffff04812143f5ffffffffffffffffff
```

The remaining 49 ADN records stay erased. No handset RAM, card-storage
file or APDU stream is edited to create the entry. A new process retaining
the preceding run's own flash, card and CCONT state physically opens
Names/Find and Details: it displays `A`, then `12345`. That retrieval sends
no UPDATE RECORD and leaves the complete card NVRAM byte-for-byte unchanged.
This proves persistence independently of the warm handset's RAM cache.

`tools/nsb7_phonebook_save_input.lua` and
`tools/nsb7_phonebook_read_input.lua` hold all menu-layout knowledge.
`tools/nsb7_phonebook_check.py SAVE COLD` requires the own security and
physical-key evidence, complete captured APDU framing, one exact save
payload versus no cold write, replayable own handset flash, persisted ADN
bytes, and exact saved/list/details frames. It shares the card-record
contract checks used by the other handsets; the NSB-7 input/UI evidence
remains product-local. Contact editing, deletion, full-card failure and
RF services are not established by this single-record round trip.

## Initial radio boundary (no peer responses enabled)

`tools/nsb7_radio_observe.lua` composes the physical security fixture with
a read-only TX-producer tap. Own writer `2cb08c` loads the producer at
HPI offset `a4`, writes the type/length header and packed payload, wraps
at byte offset `a4` (82 words), and commits the new producer at `2cb0ec`.
That independently supports the observer's ring extent and byte ordering.

The bounded 25-second fixture observes 29 complete packets with no capture
gap or cap hit: service discovery/acknowledgements, compact self-test,
parameter uploads, `4c/4`, seven `51` uploads, then `55/4` with payload
`03050000`. No later TX packet is observed in this fixture; this is not a
proof that no other firmware producer exists. `tools/nsb7_radio_check.py
IMAGE RUN` checks the own source hash, physical security evidence, ordered
type/length inventory and every producer wrap. This is a no-radio-peer
diagnostic baseline, not registration acceptance; retire or replace it
when a qualified peer changes the ordinary packet sequence.

Own constructor `2b6656` allocates eight bytes, marks four payload bytes
and type `55`, calls encoder `2b65f6`, then posts through task 3. Encoder
selector 3 uses ROM table `343781 = 05 03 03`; option zero produces the
observed `03 05` prefix. The remaining two bytes are zero-initialized by
the constructor. Do not assign continuous-scan or cancellation meanings
to the selector/option bytes from this alone.

Own receive consumers `2209d4` and `224058` explicitly test input `8b`
before passing their object pointer to wrapper `2a3704`. It selects
`2889d8` or `288ae6` according to the current request context. The first
parser reads a big-endian channel at object offsets `+6/+7`, signed RSSI
at `+9`, advances records by four bytes and bounds its loop to 40 entries.
Its channel eligibility helper is `2885e4`. The wrapper was located with
a sibling code-pattern hypothesis, then independently decoded here; an
exact 16-byte cross-ROM signature did not match because the literal-load
encoding differs. Sibling addresses or packet labels are not the contract.

### Own RX routing and channel classes

RX broker `2cb1cc` allocates/initializes a message through `2cb156`, then
copies ring payload halfwords to message base `+4` (`2cb208..2cb226`).
Dispatcher `309f14` subtracts `83` before indexing the 13-entry table
`309f60`. Entry eight (`8b`) is `309fb4`, calling `2e5258`. That forwarder
requires byte `130784 == 1`, then posts the unchanged message to task
**13** through `2898e8`. Do not reuse the sibling's task-12 attribution.
The gate is one in read-only samples at 3.5, 6, 14 and 24 seconds.

The unchanged four-byte prefix means parser object `+6/+7` is wire
payload `+2/+3`, and object `+9` is wire payload `+5`. These match the
shared measurement encoder's 40 four-byte channel/RSSI records. This is
static wire-layout agreement; no 8b measurement response has yet been
delivered to this product.

Classifier `2c7f48` tests capability predicates `2de900(1..5)` in order.
They read bits 31..27 of word `12e4ec`; the live word is `98000000` in
all four samples. Its decoded channel ranges under that word are:

| Inputs | Returned class |
| --- | --- |
| `0000` | 0 |
| `0001..007c` (1..124) | 1 |
| `007d..01ff` (125..511) | 0 |
| `0200..0375` (512..885) | 2 |
| `0376..ffff` | 0 |

The bit-28 branch precedes the overlapping bit-27 branch. If bit 27 were
enabled alone, 512..810 would instead return class 3. Thus a laboratory
carrier such as 600 is software-eligible as class 2 under the observed
word; these class numbers and feature flags do **not** establish fitted
RF bands or extra transceiver hardware. The independent product manual
identifies this handset as GSM1900.

The optional `--contract-json PATH` on the radio checker reports the own
RX entry/task/prefix and evaluates the decoded classifier over all 65,536
16-bit inputs. It checks the pinned source bytes and threshold literals;
this is a static decoded-contract check, not original CPU execution on
every input. Unit cases cover precedence and range endpoints.

### Opt-in PCS acquisition acceptance

Radio HLE defaults off in the NSB-7 input configuration. The existing
`Radio peer HLE` setting enables a product-owned **candidate-contention**
contract: the decoded four-byte type-55 forms `03050000/03030000` and
the subsequent eighty-channel type-56 list, and its type-02/20 receiver
configuration are answered, followed by eight BCCH blocks and the same
receiver-configuration grammar for CCCH modes and request-correlated access
frame completion, a request-correlated Immediate Assignment, bit-1
dedicated-channel acknowledgement, one uplink block request and a
SABM-correlated UA. Location Updating Accept, traffic,
neighbour and authentication contracts are not inherited from siblings.
Other products retain their existing contracts unchanged. Retire the
acquisition-coverage limit only after the subsequent product contracts are
qualified; zero/default channel parameters are not evidence for them.

`fixtures/nsb7_measurement/nsb7hle.cfg` enables that peer and the shared
standards-level PCS topology at ARFCN 600/601. With a single receivable
cell, the own broker receives `8b` containing carrier 600, RSSI -60 and an
unused `ffff` second slot. It forwards from task 4 to task 13, whose
`2a3704` consumer reads those same values and classifies 600 as class 2.
Firmware then organically publishes `56/160` beginning `0258`. One
standards-level SCH observation answers this list, using the shared
fourteen-byte type-80/subtype-40 encoding. No firmware queue, state or APDU is
injected to make this progression happen.

`tools/nsb7_radio_measurement_observe.lua` adds own broker/consumer/class
observations. Use `tools/nsb7_radio_check.py IMAGE RUN --contention` with
`tools/nsb7_lapdm_observe.lua` for this composition. It requires all 42
TX publications, the correct task
and payload values, and the complete observed RX inventory: two existing
external-service `8e/10` frames, one measurement `8b/166`, and one SCH
`80/14`, one channel acknowledgement `89/8`, and eight BCCH `80/34`
frames, receiver acknowledgements, one access completion, one assignment
one uplink block request and one contention-resolution UA through the
25-second fixture. The older `--measurement`, `--synchronization`,
`--channel`, `--bcch`, `--ccch`, `--rach`, `--assignment`, `--dedicated`
and `--uplink` checks retain the preceding bounded trace contracts. No LU Accept,
paging block, RSSI or terminal-search
response is accepted. The no-peer/default fixture still
reproduces its original 29-packet inventory.

Own constructor `2b6848` allocates 164 bytes, writes type `56` and payload
length 160, and fills that payload with `ff` before returning to its
callers. The single-cell fixture sends `0258` followed by 79 unused
`ffff` words. The observer captures the object at sender `2b6880`, before
the ring writer, and the checker requires the complete transmitted payload.

The bootstrap identity cell at shared byte offset `004` is reused by the
runtime TX ring. With packet start cursor `22`, candidate word 49 maps to
that cell: `(0x22 + 1 + 49) % 0x52 == 2`. Continuing to answer sentinel
writes there changes an unused `ffff` candidate to identity value `0006`.
The HLE retires the parked bootstrap responder after its first runtime TX
commit; this phase is saved and cleared by device reset. Native ownership
alone is insufficient: firmware asserts the native DSP reset line again
while runtime HLE remains in use. This is a bootstrap/runtime ownership
correction, not a candidate-payload special case.

Decode each subsequent request's callers and populated fields before
extending peer coverage. LU Accept and signaling-channel release are
qualified below; traffic release, neighbours and authentication remain
unestablished on this handset.

### Candidate response routing

The own RX dispatcher has thirteen entries for types `83..8f`, reported
in full by the radio checker's contract JSON. Types `85/8d/8e` enter the
generic path `30a006`, not a dedicated radio forwarder. Type `8a` enters
`309fbc -> 2e4fdc`: it forwards the original object to task 13 only when
`130784 == 1`, otherwise freeing it. This routing does not establish that
`8a` is a successful synchronization response.

Type `80` enters `2e4ef4`, under the same gate, and parses the payload
after the four-byte object prefix. Payload bytes 3..5 form a big-endian
24-bit value; byte 2 is a status selector; byte 0 selects these branches:

| Subtype | Own branch / handler |
| --- | --- |
| `40` | `2e4f8a`: forward to task 13 under the same `130784 == 1` gate |
| `50` | `2e4e96` |
| `60` | `2e4d24` |
| `70` | `2e4b86` |
| `80` | `2e49e6` |
| `a0` | `2e4f52`: retag then forward to task 17 |
| `b0` | `2e4f4c`: status-dependent branch to `2e49e6` |

Other subtypes are freed. Status values other than 1 also call `2e49d0`
before subtype dispatch. The subtype-40 context literal is `130778`, so
its byte `+0c` aliases the outer gate rather than imposing a second
independent readiness requirement.

After the single-cell measurement, root `12fbc8` has `+08 == 0`, while
`+0c/+18/+28` all identify the same carrier-600 record. Its byte `+0d`
is `a2`; the RX gate is already 1. Task 13's candidate wait checks type
`80`/subtype `40` at `223fea..223ffe` and calls `22076c -> 287574`.
The parser looks up the candidate using payload bytes 6..7 as a big-endian
channel. Update helper `28734c` checks payload byte 2: zero calls
`2872c4`, clears the record's failure counter, and changes the low nibble
of byte `+0d` to 4; a nonzero error increments the counter and ordinarily
sets that nibble to 1. The payload's bytes 3..5 supply the 24-bit frame
value, bytes 8..9 supply a signed shift on the relevant call variant,
and byte 10 shifted right by two supplies the decoded BSIC. These offsets
fit the existing shared fourteen-byte SCH payload shape. The qualified
run observes the same payload at broker `2e4ef4`, task-13 lookup `287574`
and update `28734c`. Firmware changes the candidate's flags `a2 -> a4`,
stores decoded BSIC `12`, and publishes type `02`/20 bytes beginning
`041202000000005050000258`. The final eight request bytes are captured
but not yet assigned semantics. Channel completion and the subsequent
BCCH, CCCH, access-frame and assignment lifecycles are independently
qualified below, including Location Updating acceptance and receiver return.

### Receiver configuration acknowledgement

Own constructor `2b7170` allocates 24 bytes and publishes a 20-byte type-02
payload. BCCH caller `2ef69e` sets internal mode `50`, the candidate channel,
and BSIC, caching its first twelve internal bytes through `2ef3ec` before
transmission. The pending cache is `[1306e4]`; the live receiver record is
`130308`. In the single-cell run the cache is
`045000000000120002580000`, while the live channel/mode remain zero.

Type-89 handler `2e5054` first checks `130784`. Its separate queued
application descriptor `[130844]` is null for this request. This is an
explicit handler branch, not a missing prerequisite: `2e50ce` calls
`2ef3ec(NULL)`, which copies the cached channel/BSIC/mode into the live
record and frees the cache, then forwards the original response to task
13. When an application descriptor exists, the handler instead checks
the response's low bit against descriptor byte 2 and requires byte 3 to
be armed; that other lifecycle is not established by this fixture.

One zero-bit `89/8` response to the captured type-02 request is sufficient
to commit carrier `0258`, BSIC `12` and mode `50`; at six seconds the
cache is null and all three live fields agree. The bounded peer then
stops. Clearing report count alone is insufficient because the full
candidate-RA-info phase re-arms its timed response and then enters BCCH;
partial channel-only coverage must leave that phase as well. No terminal
`8f` or sibling BCCH stream is
included in this contract.

Without BCCH delivery, firmware times out and publishes type `03` with
zero payload, then type-55 control `04080000` at about 8.87 seconds. The
bounded `--channel` checker requires this tail and exactly five total RX packets,
so an accidentally inherited BCCH continuation fails this bounded acceptance.
It does not establish network camping, registration, calls or SMS.

### BCCH system information

Subtype-50 forwarder `2e4e96` retags a received block as `07ff` and sends
it to task 17. A nonzero decode error with matching live BSIC/channel is
discarded; the qualified laboratory blocks use zero error. Task-17 parser
`29d42c`, state record `12d790`, copies 24 bytes from object `+0e` (wire
payload `+0a`), checks the RR protocol discriminator and message header,
then dispatches SI1/SI2/SI3/SI4 using message types `19/1a/1b/1c`.

After receiver acknowledgement the partial peer delivers eight existing
standards-level BCCH blocks, with no unsolicited RA-info or RSSI reports.
Broker and task-17 observations show the same block bytes on carrier 600.
The supplied SI3 identifies laboratory cell identity 1, PLMN 001/01 and
LAC 1; firmware's own SI bitmap reaches `0f` at `29d5ae`.
Firmware then publishes a second type-02/20 receiver request beginning
`041202090000001060000258`, selecting mode `60` rather than the previous
BCCH mode `50`. Its acknowledgement is qualified separately below.

`--bcch` requires all eight broker-to-task-17 block pairs, all four SI
types, the own bitmap completion, the organic CCCH request and the full
13-packet RX inventory. A sibling's extra RA-info, measurement or channel
response fails this contract. This proves BCCH acquisition and SI
completion, not camping, Location Updating or registration.

### Common-control receiver setup

The mode-60 wire requests use the same `2b7170` constructor and
`2ef3ec` cache/commit mechanism as the BCCH request. Internal mode `10`
encodes wire selector `60` through `2b71ba..2b71d6`; later modes `16`
and `1a` retain that selector. Each request first caches its own twelve
configuration bytes, with a null application descriptor, then commits
on a zero-bit type-89 response. No firmware flags are supplied by the
peer.

The qualified run observes these successive cache values while the live
receiver still holds the preceding mode:

| Requested mode | Cached bytes | Previous live mode |
| --- | --- | --- |
| `10` | `041000000000120002580000` | `50` |
| `16` | `041600000000120002580000` | `10` |
| `1a` | `041a00000000120002580000` | `16` |

After the third acknowledgement, the cache is empty and the live receiver
holds carrier 600, BSIC `12`, mode `1a`. Firmware also publishes type
`46/8`, an empty `0f/68` neighbour list, control `1f/4 = 00040000`, and
finally `0c/8 = 00000f6600000000`. The latter has the nonzero access
octet `0f`; its completion and assignment are qualified below. The
semantics of the type-46 payload and remaining control bytes are not
inferred solely from this ordering.

`--ccch` requires all 38 TX packets, sixteen RX packets, all three cached
configuration/acknowledgement pairs, and the final mode plus the organic
access request. The partial common-control contract retains the receiver phase for repeated
configuration, but clears scheduled reports and waits so no unqualified
PCH/AGCH or access-completion indication is emitted. Registration and
native speech remain unproved.

### Access-frame completion and timing

Own type-84 forwarder `2e4fa4` sends the original object to task 13 under
`130784 == 1`. Its wait dispatches through `224bbe -> 2243c0 -> 2f5274`.
The parser reads payload byte 0 as the access octet and bytes 1..3 as a
big-endian 24-bit frame. It stores that frame and constructs task-17
message `07f9` containing the same octet plus `(frame/1326)&31`,
`frame%51`, and `frame%26`. No firmware state is supplied by the peer.

Type-0c constructor `2b6e58` places the timing mode in payload byte 1,
access octet in byte 2, slot delay in byte 3, and (for mode 1) a low-16-bit
scheduled base in bytes 4..5. Initial producer `2f5446` emits mode 0 with
a relative delay in this fixture; it also has an unqualified initial-mode-1
branch, which the partial peer rejects without a completed access reference.
Retry producer `2f53a4` emits mode 1, deriving its base
from the previous completed access frame plus the firmware's separation
table. In the non-combined laboratory profile this separation is 55 frames.
The combined-CCCH branch uses converter `2f5ace` over allowed RACH frame
positions and is not qualified by the current profile.

The peer's old immediate completion ignored these fields: a request with
102-frame delay was completed in the next frame, and a retry targeting a
later base was completed before it. NSB-7's partial contract now waits
until the decoded deadline. Relative requests anchor to their receive
frame; retries anchor to the previous reported access frame, preserving
the distinction between sixteen-bit base truncation and GSM hyperframe
wrap. Deadline/reference state is saved and cleared on device reset.
`nokia_radio_timing.h` has executable C++ tests for relative and scheduled
delays, stale bases, base/counter wrap, and the due/not-due boundary.
Other products retain their existing timing policy pending qualification.

The bounded `--rach` fixture has four own access requests and four type-84/8 replies.
The same payload reaches the broker and task-13 parser, and each task-17
reference matches the returned frame/octet. Reports occur zero to two
frames after the deadline because the HLE service polling cadence is
calibrated, not measured silicon timing. The checker rejects premature
reports, mismatched octets/references and any assignment block. With no
assignment, firmware retries, then returns to live receiver mode `16`.
All 45 TX and 23 RX publications are accounted for. This establishes
access-frame reporting; assignment is qualified separately below.

### Immediate Assignment

The common-control subtype-60 path calls `2e49e6`, which retags the block
as `0623` and forwards it to task 14, preserving the 24-byte block at
object `+0e`. The task's receive loop is `2648bc`, observed returning to
`2648da`. Case `264966` passes the message through `264592/2645ec`.
The common-control decoder validates pseudo-length/control bits and
copies the RR body from object `+0f`; it returns decoder result `061a`,
also passed to diagnostic helper `2e6184`. That helper checks a trace
mask through `307484`; observing its argument is not proof of a mailbox
post. This is distinct from task 17's BCCH parser.

The supplied Immediate Assignment uses the shared standards-level encoder
and echoes the completed RACH octet and reduced frame reference exactly.
The later downlink metadata is a different frame: table `343fb4` supplies
non-combined CCCH block endings `9,15,19,25,29,35,39,45,49` modulo 51.
The partial peer waits for the next such ending at least four frames
after the reported access and reports that block-ending frame, even if
the HLE poll is later. The request-reference IE still identifies the
original access frame, not the downlink frame. These are separate inputs
to the existing assignment encoder.

The qualified run completes access octet `0f` at frame 2391 and delivers
the assignment with downlink metadata frame 2395. The same assignment
block reaches the forwarding boundary and task-14 decoder, whose result
is `061a`.
Firmware publishes control `1f/4 = 00090000`, then a type-02/20 dedicated
request `0412000000000040800002584000000000000000`. Its own descriptor
at `[130844]` has tag `0402`, expected bit 1 and armed byte 1; this is
not the earlier null-descriptor common-control lifecycle. The bounded
`--assignment` fixture leaves it unanswered, with the live receiver still
in mode `1a`; the acknowledgement is qualified separately below.

`--assignment` checks all 41 TX and eighteen RX packets, the single
request-correlated access completion, the original assignment reference,
the distinct eligible downlink block frame, the unchanged block at all
three consumer boundaries, and the armed bit-1 dedicated descriptor.
That bounded fixture rejects an extra dedicated acknowledgement. Immediate Assignment
acceptance does not yet establish LAPDm, Location Updating, registration
or a call.

### Dedicated acknowledgement and first uplink

The own `0402` descriptor requires reply bit 1, not the zero-bit replies
used for common control. With one type-89/8 response containing byte 1,
handler `2e5054` takes the queued-descriptor branch. The read-only watch
observes instruction `2e5086` clear that same descriptor's armed byte,
and the live receiver commits mode `40` on carrier 600 with BSIC `12`.
The `--dedicated` contract stops there without an uplink request.

Type-86 decoder `2e5228` accepts payload selector `80` and calls
`2e513e`. Under the firmware's ordinary critical section it checks
dedicated queue pointer `[130434]` and calls constructor/dispatcher
`2b64d0`: a nonempty queue selects case 0, which publishes the queued
type-1b frame; an empty queue selects case 1 and builds a fill frame.
The own jump table is at `2b64e0`, with targets `2b6568` and `2b65b0`
for those cases. Neither a frame nor a queue pointer is supplied by the
emulator.

The qualified block request finds a nonempty queue and firmware publishes
`1b/28` with payload
`0080013f4905087000f000fffe200809101010325476982b2b000000`.
After the two-byte DSP prefix, LAPDm address/control/length are `01/3f/49`:
SABM with an eighteen-byte information field. That field is the handset's
MM Location Updating Request, beginning `05 08`, with own classmark `20`
and synthetic-card mobile identity `0910101032547698`. The final two
LAPDm fill bytes and three DSP trailer bytes are preserved in the trace.
This is generated by firmware after physical boot/SIM initialization,
not constructed by the host or copied from another model's message.

`--uplink` requires all 42 TX and twenty RX packets, the armed-descriptor
clear, committed mode `40`, both block-request decoder boundaries, the
same nonzero queue pointer, and the exact own SABM/Location Updating
Request. That bounded fixture rejects an extra UA/downlink reply. Contention
resolution and Location Updating acceptance are qualified separately below;
this uplink-only fixture does not prove registration.

### Contention-resolution UA

The peer accepts the actual type-1b SABM through the existing LAPDm link
and GSM-session parser, not a synthesized upper-layer request. Its UA
echoes the same eighteen information bytes and is delivered as a dedicated
type-80/34 block. Both forwarding helper `2e49e6` and task-14 decoder
`2645ec` receive the same `01 73 49` LAPDm header plus that information.
The decoder returns result `0614`; the diagnostic hook observes this
value without treating it as a mailbox post.

Firmware handler `2664be` validates contention data. In the selected
context it reads the pending information length at `+12e`, the pending
buffer pointer at `+130`, and compares pending bytes `buffer+4` against
received information at `context+1a` using `30e8e8`. With nonempty pending
information, the matching branch selects state 4 and releases the pending
buffer; mismatch bypasses that transition. The state byte
is `context+163`, not the context's first byte, which is a selector.
The read-only watch observes length 18 and byte-identical sent/received
information while that state is 2, followed by instruction `2665c0`
writing state 4 in task 14. No state or timer is injected.

`--contention` requires that native comparison and state transition,
the exact echoed information at both consumer boundaries, 42 MCU TX
packets and twenty-one RX packets. The peer withholds the subsequently
queued Location Updating Accept. This establishes the contention
exchange and firmware link progression, not completed registration or
native DSP execution.

### Location-update acceptance and release boundary

The `candidate_location_update` qualification level permits the existing
GSM session to send its queued Location Updating Accept after the accepted
contention echo. It also permits the handset's actual type-1b supervisory
acknowledgements in the location-update and channel-release phases. It
does not enable the subsequent receiver-deconfiguration request.

In `tmp/nsb7-lu-accept`, both MCU dedicated-block consumers observe
`030045050200f1100001170809101010325476982b2b2b2b`.
Firmware responds to the following type-86 block request with
`0080032101` followed by fill/trailer bytes: LAPDm RR with N(R)=1.
The peer then sends Channel Release block
`03020d060d002b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b`,
again observed at both MCU consumers. Firmware acknowledges with
`0080034101` plus fill/trailer (N(R)=2). Its own link-state writes
progress from 4 to 3 at `26644a`, then to 0 at `265674`.

Finally, firmware publishes type-02/20 receiver request
`041202000000001a600002580000000f00000000` at 11.724501 seconds.
This is the withheld boundary for `candidate_location_update`. These wire-level
acknowledgements establish reception and link progression, not by
themselves durable MM registration, SIM EF_LOCI persistence, operator
presentation, or native DSP execution.

### Common-control return and persistent location state

The pending descriptor at the return request is tag `0409`, not the
earlier armed `0402`. In handler `2e5054`, the comparison at `2e506e`
uses literal `0409` from `2e52a0` and branches to `2e50ce`: it commits
the cached receiver with `2ef3ec(NULL)` without checking descriptor reply
bit or armed state. `candidate_release` therefore enables the existing
zero-valued type-89 channel confirmation, then withholds paging reports.

In both `tmp/nsb7-release` and `tmp/nsb7-release-cold`, the reply commits
mode `1a`. Firmware subsequently emits type-46/8, two type-02/20 requests
for mode `16`, and type-57/4 `03050000`. The two further ordinary
channel confirmations commit mode `16`; no paging payload is supplied.

Firmware also sends this physical SIM command sequence:
`a0 a4 00 00 02 6f 7e` (select EF_LOCI),
`a0 d6 00 04 05 00 f1 10 00 01` (write laboratory LAI), and
`a0 d6 00 0a 01 00` (location status updated). Saved card bytes
1600..1610 change from `ffffffffffffffffffff01`
to `ffffffff00f1100001ff00`. No host write populates this state.

`tools/nsb7_radio_check.py IMAGE RUN --release` requires both downlink
consumers, firmware acknowledgements, link release, the return-request
sequence, three channel confirmations with no later paging payload,
SIM TX commands and saved EF_LOCI. This qualifies an accepted laboratory
location update and receiver return, not durable paging reachability,
usable operator presentation, calls, SMS or native DSP speech.

### Post-release empty paging reception

`candidate_paging` enables the existing BCCH/PCH cycle only after a
registered session and a subsequent common-control receiver confirmation.
It does not enable an incoming service. This partial profile omits type-83
measurements, so it must re-arm its registered cycle on the final type-80
report; otherwise the shared scheduler stops after one cycle.

In `tmp/nsb7-paging-cycle`, sixteen empty Paging Request Type 1 blocks
`1506210001f0` plus eighteen `2b` fill bytes reach both `2e49e6` and
task-14 `2645ec`. Each produces diagnostic decoder result `061a`.
No new access request follows through 25 seconds; EF_LOCI remains updated.
`--paging` checks these consumers, decoded blocks, receiver-return sequence
and card persistence. This is reception evidence, not an addressed page
or proof that call/SMS delivery works.

The first paging frame is 3453; subsequent frames remain in the same
102-frame subscriber group. The calibrated peer poll cadence sometimes
skips an opportunity (204-frame rather than 102-frame separation).
The checker permits those explicit gaps but rejects a different group;
physical-time paging delivery remains unqualified. The idle screenshot
still contains the radio-status overlay despite working softkey text and
clock. The ordinary lab-registered idle frame is qualified separately
after physical Net monitor disable below.

### Addressed-page identity/dispatch control without background completion

This bounded control withholds background-measurement completion. It protects
identity and RR-dispatch evidence, not the currently selected complete peer.
The validated access continuation is defined in the background-measurement
contract below.

`fixtures/nsb7_page/nsb7hle.cfg` queues one laboratory page through the
existing NETCFG input; it does not enable an incoming call or SMS. At
frame 3555, task 14 receives Paging Request Type 1 block
`31062110080910101032547698` plus eleven `2b` bytes. It publishes
`05eb` to task 17 through `26458c`, with a separate information buffer
containing length `000c` and RR bytes `062110080910101032547698`.

Task 17's receive call returns at `2b2edc`. Its `05ea/05eb` branch at
`2b311c` checks protocol discriminator 6 and calls RR parser `274220`.
The complete subtract cascade selects Paging Request Type 1 handler
`274e4a` for message `21`. Identity helper `274124` receives the LV
identity `08 0910101032547698`; own identity reader `2a15d8` returns
the same length and bytes. Comparison `30e8e8` returns zero at `2741f6`,
and the helper returns match 1 at `274e82`. The RR parser then returns
event `083e` at `2b3130`. Empty paging blocks return zero there.

In the observed run, dispatch-record bytes begin `00fe010005eb000000`
at that return boundary. The meaning of the `fe` byte is not assigned.
There is no new type-0c CHANNEL REQUEST through 25 seconds: the runtime
TX inventory remains 49 packets. Identity matching and `083e` dispatch alone
are insufficient for access; the required background completion is qualified
below. The page identity and RR parser are not missing-peer hypotheses.

`tools/nsb7_page_observe.lua` captures transport posts, pointed-to
information, task-17 receive return, identity comparison and parser
result without guest writes. `--addressed-page` checks the matching
page/identity/result chain and rejects an unexpected access continuation.
The qualified capture is `tmp/nsb7-page-parser`; the earlier
`tmp/nsb7-page-identity` independently reproduces identity matching.
Neither establishes an over-air Paging Response, call or SMS delivery.

The receive wrapper returns `083e` to caller `260c2a` (Thumb return
address `260c2b`). The first comparison at `260c34`, using literal
`083e` at `260ce8`, explicitly selects `260c4c`; it does not fall
through the generic dispatcher. That continuation sets record `12c97c`
byte `+0b` from zero to one, frees its saved pointer at `+28` (observed
`1069bc`), clears the pointer, and branches to `260c16`. Firmware writes
outer state 4 and re-enters receive call `260c26`. Events `083f` and
`0840` share this static continuation; their semantics are not inferred.

`tmp/nsb7-page-dispatch-qualified` proves the wrapper return, selected
branch, flag update and cleared saved pointer, in addition to the IMSI
match. `--addressed-page` requires this continuation.
This bounded negative control does not satisfy the background-measurement
prerequisite and must not be treated as the live paging frontier.

A two-byte-aligned PC-relative literal-reference scan of own-ROM
`25d000..261000` finds six comparisons against `083e`, at `25e666`,
`25f7a2`, `25f9fa`, `260a9a`, `260b2a` and `260c30`, plus one literal
load used to synthesize that event at `260af0`. This bounds the checked
direct-literal surface, not indirect or arithmetic event selection.
The alternative path at `260af4` can call `25f6de` only when API
`200690(1)` returns 1, record `+3` is zero and record `+6` is not 1.
The `260c4c` path does not evaluate those conditions. The alternate branch's
complete lifecycle is not decoded; it is not needed to explain the qualified
background-completion path below.

The serving-RSSI sufficiency experiment is falsified. In
`tmp/nsb7-page-rssi`, the existing shared type-83 payload reaches own
forwarder `2e49b4` and task 13 as message `1802`, with signed serving
samples `c3/c4/c5`. The matching page still returns `083e`, sets the
same flag and releases the same saved record, without a new access
request through 25 seconds. This is not a proof that
all task-13 readiness inputs are irrelevant or that measurement timing
is correct. See `nsb7_serving_rssi_alone_unblocks_addressed_paging` in
the falsification ledger.

### Firmware-owned readiness request

Reader `200690` stores its selector on the stack, passes it through
diagnostic helper `2e61c4`, then reads `12d213` for selector 1 and
`12d219` otherwise. The former is initialized to 1 at `2006f6`
(`12d1ec+27`). No physical hardware meaning is assigned to these bytes.
At the matched-page continuation, selector-1 storage is 1 and RR record
`12c97c` has `+3=0`, `+6=1`. Thus `+6`, not the selector-1 byte or
`+3`, fails the alternative path's static conditions.

The read-only write-watch observes `12c982` (`record+6`) clear during
initialization (`25e2f6`), set at `260a48` around 3.459 and 6.670 seconds,
clear at `26080e` around 10.549 seconds, then set again at `260a48`
around 14.667 seconds. These are firmware writes in task 17.
Entry `260a34` records the triggering event before it is overwritten:
`07d3 -> 03eb` on the first set and `07d4 -> 03ec` on the later sets.
The shared tail stores 1 in `+6` and enters outer state 4.

The post-release `07d4` is not DSP-generated. Task 18 posts `09c8` to
task 16 at 14.663 seconds through send site `22e31e`. Task 16's
translator `201124` subtracts base `09c6`; input `09c8` selects the
literal `07d4` at `2011bc`, then calls constructor/sender `20081c`.
The send to task 17 returns at `200e38`; saved caller `20117d` ties
the runtime post to that translator. The resulting `07d4` reaches
`260a34`, reports `03ec`, and sets `+6` before the addressed page.
These task identities are numeric observations, not imported sibling
task names or a claim that `07d4` means a particular GSM primitive.

`tmp/nsb7-page-mm-request` and `--addressed-page` require this request
chain, the writer and all three readiness values alongside the accepted
IMSI and `083e` continuation. The `09c8` completion boundary is described
below. Forcing `+6` clear would bypass the firmware-owned completion contract.

### Post-release service completion boundary

The timeout observations here belong to the background-disabled control,
not the qualified complete paging composition.

Task 18's generic sender `22e28c` receives `09c8` from call `232052`
(Thumb return `232057`) both before registration and after release.
That branch sends `09c5` first, then `09c8`, and waits through `22e5c4`
for event `1584` at `23205a`. The first wait completes with `1584`
around 10.551 seconds; the post-release wait instead returns `1599`
around 20.289 seconds. No `1584` is injected or substituted for that
timeout.

The successful completion chain is:
`03ec` (task 17 to task 13, `2b299c`) -> task-13 completion `2a3646`
-> `07ee` (task 13 to task 17, `2a35e0`) -> `09f9` (task 17 to task 16)
-> `1584` (task 16 to task 18). Completion dispatcher `2a3646` reads
the pending object through `130854` (also `13084c+8`) and dispatches
tag `03ec` to `2a3696`; that calls constructor `2a354c` with `07ee`.
The neighboring `03eb` case selects `07ef`, not `07ee`.

`tmp/nsb7-page-service-completion` observes two `03ec` submissions,
at 6.669 and 14.666 seconds, but only one `2a3646` completion for tag
`03ec`, at 10.548 seconds. The corresponding `07ee` and `09f9` precede
the first `1584`. After release the receiver-return indication is instead
`09f7 -> 1580`, followed by the new `09c8` request and no second `03ec`
completion through 25 seconds. This identifies a task-13 service
completion boundary; it does not yet identify the missing condition or
prove that another DSP packet is required.

`--addressed-page` additionally requires the two own caller observations,
two service submissions, single completion, and `1584` then `1599`
returns from the same task-18 record in this withheld-completion control.
The background-measurement contract below resolves the missing completion;
the bounded timeout fixture is not the currently selected peer coverage.

### Background measurement completes paging-response access

The outstanding type-57/4 `03050000` is a request, not a stop-only
notification. Own constructor `2b6a1e` allocates and clears eight bytes,
sets type `57` and payload length 4, and uses the same selector encoder
`2b65f6` as the recovered acquisition constructors. NSB-7's existing
type-8b consumer accepts the forty-record channel/RSSI layout already
qualified for initial acquisition. The product now enables the shared
background-measurement contract. Its exact request is admitted through
the partial-coverage guard; other unqualified requests remain withheld.

In `tmp/nsb7-background` and independently seeded
`tmp/nsb7-background-cold`, RX29 is type-8b/166 after TX49 type-57.
The ordinary task-13 lifecycle reconfigures the receiver and completes
pending `03ec` through caller `220de6` at 17.906 seconds. Task 18's
second wait receives `1584` at 17.909 seconds, rather than timeout
`1599`. The queued matched page then causes firmware to emit its own
CHANNEL REQUEST at TX57, accept an assignment, and publish TX61:
`0080013f41062707032058810809101010325476982b2b2b2b000000`.
The LAPDm SABM information is the RR Paging Response beginning `06 27`,
including the handset classmark and the same synthetic-card IMSI. Both
MCU consumers receive UA contention echo
`01734106270703205881080910101032547698` plus five `2b` bytes.
The page-only network then releases the channel; this is not call setup.

`--page-response` requires the ordered type-57/type-8b/completion/access
chain, the exact own Paging Response, both contention-echo consumers,
69 bounded TX packets, no service timeout, and retained laboratory
EF_LOCI. The cycle continues with another background request after the
page-only release. This page-only gate does not itself establish call/SMS
or idle presentation; those independently qualified contracts follow below.
The earlier serving-type-83 sufficiency falsification remains valid:
this correction answers type 57 with type 8b, not periodic serving RSSI.

### Incoming call setup and bounded pre-traffic evidence

`fixtures/nsb7_incoming_call/nsb7hle.cfg` selects the existing laboratory
incoming-call input instead of a page-only transaction. After the own
Paging Response, the network sends Ciphering Mode Command `06 35`,
then an Identity Request and CC SETUP. The partial product previously
discarded type-1b uplinks in `service_uplink_wait`, causing repeated
downlink attempts. It now admits the observed 28-byte SEND_BLOCK with
prefix `00 80` to the existing LAPDm/session parser in that phase.
No firmware event, queue or result is supplied by the driver.

In `tmp/nsb7-call-service` and `tmp/nsb7-call-service-cold`, firmware
acknowledges the command and emits Ciphering Mode Complete `06 32`.
Both MCU consumers receive CC SETUP block
`032445030504046002008134015c0581551532f4` plus four `2b` bytes.
The handset then sends TX67 type-1b information
`8308040460020180150101` (CALL CONFIRMED), followed by TX68
`8341` (ALERTING with the sequence bit set). The peer's RR Assignment
Command leads to TX70 type-02/20:
`041202000271012fc10002580000000400000000`.

Its pending `0402` descriptor requires reply bit zero and is armed.
That `c1` traffic-channel request remains withheld in this checkpoint;
there is no new main-link SABM on selector `b0`. Later empty uplink fill
blocks are not call progress. `--incoming-call` checks the ordered TX
inventory, exact own call-control uplinks, SETUP at both MCU consumers,
pending descriptor and retained EF_LOCI. It rejects a traffic-link
continuation, which requires its own acceptance contract.

This proves incoming-call signaling through ALERTING, not visible
ringing, physical Answer, CONNECT, traffic-channel completion or native
speech. The captured main-area screen remains unsuitable as a call-UI
oracle. The subsequently qualified `c1` acknowledgement and physical
answer/release continuation are described below.

### Physical Answer/End and traffic-link signaling

The current profile admits the own `c1` request to the shared traffic
assignment handler. Type-89 with bit zero clears its armed `0402`
descriptor at `2e5086`. Firmware immediately publishes an empty SABM
on selector `b0`, without waiting for a block request. That observed
28-byte uplink is admitted in the traffic-establishment phase. After
UA, active `b0` uplinks are admitted throughout the shared service-phase
range, including unsolicited first transmissions; limiting them to the
scheduled wait phase discarded those first frames and induced retries.

`tools/nsb7_call_observe.lua` extends the existing physical security-code
fixture with Send at 22 seconds and End at 27 seconds, both released
after 200 ms. It exits at 35 seconds. In `tmp/nsb7-call-release` and
`tmp/nsb7-call-release-cold`, the handset emits:

| TX | Firmware-owned signaling |
| --- | --- |
| 71 | Empty `b0` LAPDm SABM; UA reaches both MCU consumers |
| 72 | RR Assignment Complete information `062900` |
| 75 | CC CONNECT `8307`, after physical Send |
| 78 | CC DISCONNECT `836502e090`, after physical End |
| 80 | CC RELEASE COMPLETE `832a` |
| 83 | Receiver deconfiguration `041202001117001a600002580000001400000001` |
| 86 | Common-control mode-16 configuration |
| 87 | Background measurement request `57/03050000` |

The CONNECT ACKNOWLEDGE block `036009030f` plus nineteen `2b` bytes
reaches both forwarding helper and task-14 consumer on selector `b0`.
The final deconfiguration uses byte `14`, now an own product parameter
for the existing shared release handler. Its descriptor is `0409`,
so the already-qualified cache-commit branch accepts a zero-valued reply.
The receiver returns through modes `1a` and `16`, with EF_LOCI retained.

`--physical-call` checks the earlier SETUP/ALERTING path, traffic
descriptor clear, UA and CONNECT ACK at both MCU consumers, ordered
92-packet TX inventory, physical key timing, CONNECT/clearing uplinks,
release parameter and common-control return. This proves laboratory
incoming-call control and physical Answer/End, not native DSP speech,
audio fidelity, external telephony or a usable call-screen image.
This signaling-only gate does not qualify call-screen pixels or restoration;
the independent authenticated/host and state gates below supply those checks.

### Incoming SMS delivery and cold SIM readback

`fixtures/nsb7_incoming_sms/nsb7hle.cfg` queues one ordinary laboratory
SMS instead of a call. `tools/nsb7_sms_observe.lua` extends passive
protocol observation to 35 seconds, without input after phone-code
acceptance. No emulation change was needed after the call-control work.

In `tmp/nsb7-sms`, the handset establishes SAPI 3, acknowledges both
CP-DATA segments, then emits CP-ACK `8904` (TX70) and RP-ACK
`8901020240` (TX71). After network CP-ACK it acknowledges RR release,
deconfigures the dedicated receiver (TX74), returns to common control
and issues a new background request (TX78). These are MCU uplinks,
not merely laboratory-session state changes.

The physical SIM TX stream contains one `A0 DC 01 04 B0` UPDATE RECORD
with the exact 176-byte record saved in EF_SMS slot 1. Its status is
`03` (unread), followed by laboratory SMSC `06912143658709` and the
shared SMS-DELIVER body for sender digits `5551234`, text `hello`; remaining
bytes are `ff`. The other nine records remain empty. EF_LOCI remains
the accepted laboratory location. No card file is populated by the host.

`tmp/nsb7-sms-retained` starts a separate process with that process's
own saved flash/card/CCONT files and no radio/SMS input configuration.
Firmware requests READ RECORD 1 (`A0 B2 01 04 B0`) and consumes the
entire retained 176-byte record through SIMI RX. The card file remains
byte-identical; no UPDATE RECORD occurs. This proves cold retention and
MCU transport readback, not that the message body is rendered correctly
or marked read by the UI.

```sh
.venv/bin/python tools/nsb7_sms_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-sms tmp/nsb7-sms-retained
```

The checker pins the own ROM, initial location-update exchange, ordered
83-packet delivery inventory, SAPI3/CP/RP acknowledgements, release,
exact wire write and stored record, untouched other slots, and cold
full-record RX with unchanged storage. The user-driven reading continuation
is qualified below; deletion and outgoing Reply are qualified in the following sections.

### Physical SMS reading and readable message screen

`tools/nsb7_sms_read_observe.lua` presses the physical left softkey at
23 and 25 seconds after the same incoming delivery. The first delivered
record is unchanged until that reading sequence. Firmware then sends a
second complete UPDATE RECORD at 25.483 seconds, changing only its
status from unread `03` to read `01`; the text, sender, other nine SMS
records and the rest of the card remain byte-identical.

The captured `tmp/nsb7-sms-read/snap/nsb7_sms_read_25.png` visibly shows
`hello` with `Options` and `Back`. Its 84x48 RGB pixel SHA256 is
`73f302b41033281ec674b38bfb011a95857ddbd0b533b94a5520a821c9b36b4f`.
The checker hashes decoded pixels rather than PNG file metadata. This is
a readable message-screen result, not a resolution of the idle/call
overlay question. `progress_latest_frame.png` is updated
from this actual firmware screenshot.

`tmp/nsb7-sms-read-retained` cold-starts using that process's own saved
files and no queued network SMS. The MCU reads the full retained read
record through SIMI; the entire card stays unchanged, including status
`01`. The extended acceptance command is:

```sh
.venv/bin/python tools/nsb7_sms_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-sms tmp/nsb7-sms-retained \
  --read tmp/nsb7-sms-read --read-cold tmp/nsb7-sms-read-retained
```

It additionally requires exact physical key timing, delivery-before-read
wire writes, a status-only card change after the key presses, the message
pixel oracle and cold full-record readback. No firmware, SIM record or
read flag is set by the observer. Deletion and outgoing Reply are
qualified below.

### Physical SMS erase, cancellation and cold persistence

The readable Options list selects Erase first. Physical Menu at 28 seconds
opens it, and Menu at 30 seconds opens `Erase message?` with OK/Back.
The record remains read (`01`) at this confirmation boundary.
`tools/nsb7_sms_delete_observe.lua` presses Menu/OK at 32 seconds;
`tools/nsb7_sms_delete_cancel.lua` instead presses physical Back at the
same time. Both compose the existing delivery/read fixture, with no
guest or card writes from the observer.

`tmp/nsb7-sms-delete-confirm` shows a third complete SIM UPDATE RECORD
after OK. Only card byte 1636 changes, from read `01` to unused `00`.
The old SMS body remains in the unused record: this is logical deletion,
not secure erasure. `tmp/nsb7-sms-delete-cancel` performs only the earlier
delivery/read writes, and its entire card is byte-identical to the
read-message baseline. Back does not transmit a logical-delete write.

`tmp/nsb7-sms-delete-retained` cold-starts with the confirmed-deletion
process's own saved files, without a new network SMS. The MCU reads the
complete retained record (status `00`, residual old body) through SIMI;
the full card remains unchanged. This qualifies deleted-slot persistence,
not secure data destruction or a corrected empty-inbox/idle pixel oracle.

```sh
.venv/bin/python tools/nsb7_sms_delete_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-sms-read \
  tmp/nsb7-sms-delete-confirm tmp/nsb7-sms-delete-cancel \
  tmp/nsb7-sms-delete-retained
```

Acceptance requires the own ROM and read-screen oracle, exact physical
confirmation/cancellation sequence, status-only card difference,
delivery/read/delete write counts and ordering, the complete wire delete
record, Back's non-write control, and cold full-record readback. Other
SMS slots and all remaining card bytes must be unchanged.

## Physical outgoing SMS Reply

The physical Reply fixture composes the incoming-message/read fixture.
Menu at 28 seconds opens Options, Down selects Reply, Menu at 32/34
selects As message, keypad 2 enters `A`, and Menu at 38/40/42 selects
Send and confirms the sender-derived destination `5551234`. The observer
only drives physical inputs and captures frames; it does not construct
the handset's SMS or edit guest state.

The MCU establishes its SAPI-3 service and emits the reassembled CP-DATA
`390118000100069121436587090d11000781551532f40000a70141`.
The lab network decodes one accepted SMS-SUBMIT with destination `5551234`,
SMSC `1234567890`, GSM7 user length 1 and payload `41`. Network CP-ACK
and RP-ACK are followed by the handset's `3904` CP-ACK, acknowledged
Channel Release and return to common control. The 44.3-second image
reads `Message sent` (84x48 RGB SHA256
`9ac0ef8ceeb981079a93cde45249a30256763d25119e5fcfae1218f05cf5c0b7`).

The 70-second observation also guards a shared peer lifecycle correction:
an outgoing service must retain the consumed latch for an earlier
one-shot incoming fixture. Otherwise that incoming SMS repeats after
the outgoing release. Explicitly queued successors still rearm paging;
`verify-radio-sms-sequential` checks two-message delivery, save/load
replay and physical notification/read/selective-delete paths.

```sh
.venv/bin/python tools/nsb7_outgoing_sms_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-sms-reply-final
```

This qualifies lab-network submission and the firmware confirmation UI,
not delivery through an external mobile network or native DSP execution.

## Physical outgoing call signalling

With the measurement-only PCS fixture (no incoming page configured),
`tools/nsb7_outgoing_call_observe.lua` enters `5551234` on physical keypad
contacts at 20..26 seconds, presses Send at 28 seconds and End at 40.
The handset produces CM SERVICE REQUEST
`05247103205881080910101032547698`, then SETUP
`03450404600201805e0581551532f4150101`, whose Called Party BCD Number
is `5551234`. The existing lab peer supplies CM SERVICE ACCEPT,
CALL PROCEEDING, traffic assignment, ALERTING and CONNECT. Firmware
publishes Assignment Complete and CONNECT ACKNOWLEDGE organically.

Physical End produces DISCONNECT `036502e090`, followed by network
RELEASE, handset RELEASE COMPLETE `032a`, acknowledged RR Channel
Release and the product's traffic-deconfigure parameter `14`. The
receiver returns to common control and requests background measurement
`57/03050000`. No new peer response or driver behavior was required.

```sh
.venv/bin/python tools/nsb7_outgoing_call_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-outgoing-call
```

Acceptance checks the own ROM, exact physical input sequence, decoded
called number and payload lengths, one SETUP/traffic assignment,
ordered connection and release, absence of an incoming page, and a
complete 50-second observation. Both `tmp/nsb7-outgoing-call` and a fresh
process retaining its own flash/card/CCONT storage at
`tmp/nsb7-outgoing-call-cold` pass this check. Ordinary dial/call presentation
without the radio-status overlay is qualified below. L1 speech ticks with
`good=0` do not establish decoded speech;
this result is call-control signalling only, not a native voice or
external telephone call.

## Idle radio-status overlay source

The earlier blanket description of the idle/dial main area as malformed
graphics is too strong. The glyph lookup at `0x28156a`, called from
`0x23ed6c`, receives font `0x341420` and characters from the firmware's
UTF-16 buffer `0x1040e0`. After registration on the measurement-only
fixture, the complete source text is:

```text
 600   0 xxx
 0  0 x xxxx
   0       0
    CCCH
```

The own ROM contains the ASCII status label `CCCH` at `0x2363c0`, beside
`AGCH`, `BCCH`, `SEAR`, `NSPS`, `xxxx`, `CBCH` and `SDCC`; its pointer
appears in the table at `0x330544`. The repeated `x` marks are therefore
supplied by firmware, not created by LCD corruption. The byte-output
routine `0x307308` is called by `0x282666`, which sends framebuffer
`0x129124` masked by plane `0x128f2c` over the same LCD path used by
readable menus and SMS screens.

`tools/nsb7_display_text_observe.lua` records the source string read-only,
while composing the existing physical Messages/Call log/Missed calls
fixture. `tools/nsb7_display_text_check.py IMAGE RUN` requires the own ROM,
complete runtime string and all three existing exact menu control frames.
This proves the origin of the apparent junk, not every aspect of its
layout. The physical menu experiment below confirms it is Net monitor
test `01`. Do not disable it by guest-state writes or claim an LCD/font repair.

### Physical Net monitor disable and ordinary idle

From Messages, physical Up selects the final `Net monitor` menu item.
Opening it displays `Test: 01`. `tools/nsb7_netmonitor_off.lua` enters
`00` with two physical keypad-0 presses and confirms with Menu/OK.
The firmware returns to a clean idle frame containing `DCT3 LAB`,
`13:47`, signal/battery bars and `Menu/Names`. Its 84x48 RGB SHA256 is
`18af845e6ddde36b7e5ba8ecb09c7d6c6c5a67170d2e56e18d99327961e0f147`.
No RAM, flash or LCD state is written by the harness.

Independent replay of the menu-only control and disabled run's own
flash journals differs at exactly one logical byte: `0x04f1: ff -> 3d`.
Both had the same starting own storage. A new process retaining the
disabled run's own flash/SIM/CCONT state reproduces the exact idle frame
after physical security-code entry, without navigating Net monitor.
The bit-level meaning of `3d` is not established; it is a firmware-written
setting, not a new fixture constant to inject.

```sh
.venv/bin/python tools/nsb7_netmonitor_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-netmonitor-menu \
  tmp/nsb7-netmonitor-off-final tmp/nsb7-netmonitor-off-cold \
  --call tmp/nsb7-outgoing-call-clean
```

Acceptance requires the decoded physical contacts, Net monitor menu,
test-01/test-00 and idle pixel oracles, independently validated journals,
the one-byte control difference and cold retained setting/frame.
`tmp/nsb7-outgoing-call-clean` starts with that own retained storage and
passes the outgoing signalling checker unchanged. Its dial screen reads
`5551234`, its call screen reads `Calling...`, and it returns to the same
clean idle after physical End. These are lab-HLE presentation and
signalling results, not native speech or external-network qualification.

### Settled-idle save/load acceptance

`tools/nsb7_state_idle.lua` starts from the own retained test-00 storage,
enters the phone code physically, and saves settled lab idle at 27 seconds.
Pre-save and post-load observations match the complete 37-register
ARM/banked inventory, R15/SP, mapped-RAM checksum and emulated time exactly.
The 27..30-second reference/restored intervals have identical ordered
TX/RX/service/SIM protocol records using the shared replay checker (which
excludes bodyless idle polls and diagnostic print timestamps).

Both reference and restored screenshots equal the full clean-idle RGB
oracle above. A physical Menu contact after the restored interval decodes
`19` and produces the existing exact Messages-menu oracle. The entire
persistent SIM card remains byte-identical to the input storage.

```sh
.venv/bin/python tools/nsb7_state_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-state-idle \
  tmp/nsb7-netmonitor-off-cold
```

This qualifies settled-idle architectural/protocol/pixel restoration and
post-load input for `nsb7hle`, not native DSP restoration. Active-call
and incoming-SMS boundaries are qualified separately below.

### Established-call and active incoming-SMS restoration

The three entry scripts now compose `tools/nsb7_state_roundtrip.lua`
rather than duplicating architecture/replay mechanics.
`nsb7_state_call.lua` saves the physically dialed outgoing call at 33 seconds,
after CONNECT ACKNOWLEDGE and before physical End. Exact ARM/RAM/time
restoration and reference/restored call pixels pass. The quiet signalling
window has no ordinary packet exchange, so call acceptance additionally
compares the model's ordered `radio_l1` frame-clock events over 33..36
seconds. Their `good=0` traffic is timing evidence, not speech acceptance.
After replay, physical End at 40 seconds still drives DISCONNECT,
RELEASE COMPLETE and common-control return, with the exact clean-idle
frame at 50 seconds. The entire SIM card is unchanged.

`nsb7_state_sms.lua` saves at 19.35 seconds: the handset has sent CP-ACK
at about 19.3474, but has not yet sent RP-ACK at about 19.3632. The
checker rejects a save after RP completion. Exact architectural state,
three-second protocol replay and reference/restored notification pixels
pass. Physical Menu at 23/25 seconds then reads `hello`, reproduces its
full text oracle and writes only the read status. The complete resulting
card equals the own input card plus the one delivered/read SMS record;
delivery and read-status UPDATE RECORD wire ordering are checked.

```sh
.venv/bin/python tools/nsb7_state_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-state-call \
  tmp/nsb7-netmonitor-off-cold --call
.venv/bin/python tools/nsb7_state_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-state-sms-active \
  tmp/nsb7-netmonitor-off-cold --sms
```

These results cover an established lab-HLE call and the incoming CP/RP
completion boundary, not every call/SMS phase, outgoing-submit restoration,
host-dialog replay or native DSP/media restoration.

## Physical SIM PIN and cold retry retention

This is distinct from the phone's `Security code`. The explicit card is
created by `tools/make_sim_card_profile.py --pin-enabled`: PIN1 `1234`,
three retries, laboratory identities and otherwise empty user storage.
`fixtures/nsb7_sim_security/nsb7hle.cfg` disables the lab radio peer, so
this experiment cannot be mistaken for authenticated registration.
The own phone flash/CCONT inputs retain the earlier physical test-00
setting; no SIM credential or retry is written through guest RAM.

`tools/nsb7_pin_input.lua` captures `Enter PIN code`, then physically
enters `4321` and Menu at 8..12 seconds. The MCU transmits
`A0 20 00 01 08 34 33 32 31 FF FF FF FF`; the card returns `9804`,
the firmware displays `Code error`, and exactly PIN1 retries change
from 3 to 2. The initial legacy card prefix is preserved byte-for-byte
otherwise; the device's normal 46-byte PLMN/SMSS/AoC serialization
extension is independently checked.

A new process copies the wrong run's complete own card, flash and CCONT.
`tools/nsb7_pin_correct.lua` physically enters `1234`; the corresponding
VERIFY returns `9000`, exactly retries 2 -> 3 change in the entire card,
and the screen advances to the phone `Security code` prompt. Neither
run sends credential-change/enable/disable/unblock commands or a file
UPDATE. This proves PIN retry persistence and recovery; PUK recovery
and PIN-request settings, PIN2 and PIN-enabled registration have separate
acceptance contracts below.

```sh
.venv/bin/python tools/nsb7_pin_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-pin-wrong-final \
  tmp/nsb7-pin-correct-final
```

Acceptance checks the own ROM, decoded physical contacts and exact APDUs,
VERIFY outcomes and retry counts, no unrelated security/file writes,
complete initial/cold/final card contents and exact PIN/error/phone-lock
prompt pixels.

### Physical PIN exhaustion and PUK recovery

Three independent cold wrong-PIN processes start with the declared
PIN-enabled card and retain the preceding process's entire own card.
Their VERIFY results are `9804`, `9804`, `9840`, with PIN1 retries
`3 -> 2 -> 1 -> 0`. The final screen reads `Enter PUK code`.
No retry state is edited to manufacture a blocked card.

`tools/nsb7_puk_wrong.lua` enters wrong PUK `12345679`, replacement PIN
`4321` and its confirmation through physical keys. The own firmware
sends `A0 2C 00 00 10` followed by eight PUK ASCII bytes and
`34 33 32 31 FF FF FF FF`. The response is `9804`; exactly PUK1 retries
change `10 -> 9`, while PIN1 remains blocked and its credential unchanged.

A fresh process retains that failed-PUK card. `tools/nsb7_puk_input.lua`
enters correct PUK `12345678` and the same new PIN/confirmation. The
UNBLOCK response is `9000`; exactly PIN1 changes from `1234` to `4321`,
PIN1 retries reset `0 -> 3`, and PUK1 retries reset `9 -> 10`.
A final cold process uses `tools/nsb7_pin_recovered.lua` to physically
VERIFY `4321`, receiving `9000` and the separate phone Security-code
prompt with the entire persistent card unchanged.

```sh
.venv/bin/python tools/nsb7_puk_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-pin-wrong-final \
  tmp/nsb7-pin-wrong-second tmp/nsb7-pin-blocked tmp/nsb7-puk-wrong \
  tmp/nsb7-puk-recovered tmp/nsb7-pin-recovered-cold
```

Acceptance requires every decoded physical PUK/new/confirm contact,
the exact VERIFY/UNBLOCK APDUs and outcomes, independently valid own
flash journals, all six complete initial/final card pairs, no unrelated
credential/file writes, and exact PUK/phone-lock prompt frames. Permanent
PUK exhaustion, PIN2/PUK2, ordinary PIN changes and request settings are
not implied by this recovery result.

### Physical SIM-card-security Off/On

The own menu route is `4 -> 6 -> 1` (`Settings / Security settings /
SIM card security`).
`tools/nsb7_pin_request.lua` physically enters startup PIN `1234` and
phone code `12345`, navigates this route, enters the current PIN at the
settings prompt, selects Off and confirms. The firmware sends
`A0 26 00 01 08 31 32 33 34 FF FF FF FF`, receives `9000` and displays
`SIM card security / Off`. Exactly card byte 3520 changes `1 -> 0`.

A fresh process retains the complete saved card/phone/CCONT storage.
`tools/nsb7_pin_request_enable.lua` reaches and physically accepts the
phone-code prompt without any startup SIM VERIFY. The same settings route
selects On and sends `A0 28 00 01 08` plus padded `1234`, receiving `9000`.
Exactly byte 3520 changes back `0 -> 1`; the complete card equals the
original enabled card. Another cold process shows the exact `Enter PIN
code` prompt and physically verifies `1234`, leaving persistent card
contents unchanged. All runs use the radio-disabled security fixture.

```sh
.venv/bin/python tools/nsb7_pin_request_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-pin-request-off \
  tmp/nsb7-pin-request-on tmp/nsb7-pin-request-restored
```

Acceptance requires decoded physical navigation/authentication contacts,
the setting APDU after final confirmation, correct security statuses,
complete cold card comparisons, own flash-journal validity, phone-code
acceptance without startup VERIFY while Off, exact Off/On menu frames,
and the restored cold PIN prompt/VERIFY. Ordinary PIN1 change is qualified
below, as are PIN2/PUK2 and PIN-enabled network registration. Those results
are not inferred from the request-setting fixture alone.

### Ordinary PIN1 change and rejected-current-PIN control

The own `4 -> 6 -> 5` Change access codes path opens `Current PIN code`;
successful completion returns to the `Change PIN code` leaf `4-6-5-1`.
`tools/nsb7_pin_change.lua` authenticates PIN1/phone code physically,
enters current PIN `1234`, new PIN `4321` and its confirmation. The MCU
transmits `A0 24 00 01 10`, padded old `1234` and padded new `4321`.
`9000` and the exact `PIN code changed` frame accompany a change to only
the four PIN1 credential bytes; retries, enabled state, PUKs and all
user files remain unchanged.

The independent `tools/nsb7_pin_change_wrong.lua` control starts with the
same own card and enters current PIN `1111`. CHANGE CHV returns `9804`
and displays `Code error`; only PIN1 retries change `3 -> 2`, and PIN1
remains `1234`. A separate cold run using the successful process's entire
own card physically verifies `4321`, returns `9000` and reaches the phone
Security-code prompt with no persistent card changes.

```sh
.venv/bin/python tools/nsb7_pin_change_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-pin-change-wrong \
  tmp/nsb7-pin-change tmp/nsb7-pin-changed-cold
```

Acceptance checks all physical old/new/confirm decodes, initial PIN
authentication, CHANGE CHV after final confirmation, exact success/error
statuses and pixels, own flash journals and complete initial/final cards
for both controls and cold reuse. This ordinary CHANGE CHV result is
independent of the earlier PUK-based replacement-PIN recovery.

### Physical PIN2 change, retry isolation and cold reuse

The own `4 -> 6 -> 5 -> 2` leaf is `Change PIN2 code` and prompts
`Current PIN2 code`. `tools/nsb7_pin2_change.lua` composes the shared
physical Access codes harness with old PIN2 `5678`, new `8765` and its
confirmation. The firmware sends `A0 24 00 02 10` plus the two padded
credentials, receives `9000` and displays `PIN2 code changed`.
Only PIN2's four credential bytes change; PIN1, both PUKs, retries,
enabled state and all files remain unchanged.

The independent `tools/nsb7_pin2_wrong.lua` control uses current PIN2
`1111`: CHANGE CHV returns `9804` and its own exact `Code error` frame.
Only PIN2 retries change `3 -> 2`; PIN1 retries remain 3 and neither
credential changes. `tools/nsb7_pin2_restore.lua` starts a new process
with the successful run's complete saved card, authenticates using
PIN2 `8765` and changes it back to `5678`. The entire final card equals
the original declared enabled card. PIN1 authentication still happens
independently at startup in every run.

```sh
.venv/bin/python tools/nsb7_pin2_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-pin2-wrong \
  tmp/nsb7-pin2-change tmp/nsb7-pin2-cold
```

Acceptance reuses the observed physical change parser with selector 2,
adds explicit PIN2/PUK2 retry accounting, complete initial/final card
comparisons and exact PIN2 success/error frames. The shared PIN1 branch
is separately rerun. Exhaustion/PUK2 recovery and FDN authorization are
qualified by their own contracts below, not by this credential-change gate.
AoC authorization and PIN2-transaction save-state restoration are not covered.

### Physical PIN2 exhaustion and PUK2 recovery

The same physical wrong-PIN2 change workflow runs in three independent
cold processes, retaining each preceding complete card. CHANGE CHV2
returns `9804`, `9804`, `9840`, with only PIN2 retries changing
`3 -> 2 -> 1 -> 0`. PIN1 still verifies at startup in every process.
The final view displays `PIN2 code blocked`.

On that physically exhausted card, `tools/nsb7_puk2_wrong.lua` navigates
the same menu and enters PUK2 `87654322`, new PIN2 `8765` and its
confirmation. The MCU sends `A0 2C 00 02 10` plus PUK2 and padded PIN2.
`9804` changes only PUK2 retries `10 -> 9`; PIN2 stays blocked and its
credential remains `5678`.

A cold process retains that entire failed-PUK2 card.
`tools/nsb7_puk2_recover.lua` physically enters correct PUK2 `87654321`
and confirmed new PIN2 `8765`. UNBLOCK CHV2 returns `9000`, changing
only PIN2 credential bytes and resetting PIN2/PUK2 retries `0/9 -> 3/10`.
The firmware then automatically sends VERIFY CHV2 with `8765`, receives
`9000` and displays `Code accepted`. The checker requires that ordering;
the failed-PUK2 control must not verify the proposed new PIN2.

A final cold `nsb7_pin2_restore.lua` process authenticates with `8765`
and changes PIN2 back to `5678`. Its complete saved card equals the
original declared laboratory card, including unchanged PIN1, PUK1,
enabled state and all files. No retry counter or credential is edited
in guest memory or preseeded to force an exhausted state.

```sh
.venv/bin/python tools/nsb7_puk2_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-pin2-wrong \
  tmp/nsb7-pin2-wrong-second tmp/nsb7-pin2-blocked tmp/nsb7-puk2-wrong \
  tmp/nsb7-puk2-recovered tmp/nsb7-puk2-cold
```

Acceptance covers decoded physical inputs, selectors 1/2, exact
CHANGE/UNBLOCK/automatic-VERIFY commands, both credential families'
retry fields, all six complete cold card pairs, own flash journals and
blocked/error/accepted/changed pixel oracles. Permanent PUK2 exhaustion and
FDN authorization have separate acceptance below. This six-process gate does
not establish AoC authorization or PIN2-transaction save-state replay.

## PIN-authenticated fresh and cold registration

`tools/nsb7_pin_registration.lua` composes physical PIN `1234` at
8..12 seconds, phone code `12345` at 16..21, and the existing read-only
radio observers. The initial card is the declared PIN-enabled profile
with unregistered LOCI; the own phone storage retains physical Net
monitor test00 and the clock setting. The PCS measurement fixture enables
the lab peer, but the harness posts no radio or registration messages.

After successful VERIFY, firmware emits the own Location Updating
Request `05087000f000fffe20080910101032547698`. Exact contention UA,
Location Updating Accept and release bytes reach both MCU consumers;
the same link states `4 -> 3 -> 0` and acknowledgement payloads are
required. PIN-gated startup adds three earlier packets: this run's SABM
is TX45 and acknowledgement/release requests are TX46..48 rather than
the older TX42/TX43..45. The shared checker takes those explicit ordinals
without changing its default or weakening payload/consumer checks.

Firmware writes exactly registered EF_LOCI
`ffffffff00f1100001ff00` through SIMI, returns to common control and
requests background measurement. The complete serialized card differs
from the declared profile only in LOCI. The full 45-second idle image
matches the clean `DCT3 LAB` oracle.

A new process retaining the complete own card/phone/CCONT storage again
requires physical PIN, then emits
`05087200f110000120080910101032547698`, carrying the retained LAI.
The same exact acceptance/release checks pass; the firmware does not
rewrite already-correct LOCI, and the entire card remains unchanged.
Its full idle image matches the same oracle. Registration follows SIM
PIN verification and can precede completion of the separate phone-lock
editor; the two credentials are not interchangeable.

`tools/nsb7_pin_registration_wrong.lua` uses the same fresh PIN-enabled
card and radio fixture but enters wrong `4321` and no further keys.
Through 45 seconds it sends no subscriber Location Updating Request and
receives no acknowledged Location Updating Accept. Only PIN1 retries
change `3 -> 2`; LOCI and all files remain unmodified.

```sh
.venv/bin/python tools/nsb7_pin_registration_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-pin-registration-wrong \
  tmp/nsb7-pin-registration tmp/nsb7-pin-registration-cold
```

Acceptance combines physical PIN and phone-code evidence, exact request
variants and their ordering after VERIFY, the contiguous 57-entry TX inventory,
MCU consumer/release checks, SIMI writes versus cold non-writes,
complete initial/final card comparisons and full clean-idle pixels.
This is authenticated lab-HLE registration, not native DSP/RF or network
authentication/ciphering. PIN-enabled outgoing-call/SMS acceptance is
qualified below; PIN-enabled save-state boundaries remain separate work.

## PIN-authenticated call and SMS composition

`tools/nsb7_pin_outgoing_call.lua` composes physical PIN and phone-code
entry with the existing outgoing-call fixture. Dial inputs are shifted
eight seconds so they start at 28 seconds, after phone-code completion;
Send is at 36, End at 48 and the final frame at 58. This is a physical
test-input schedule, not a change to firmware, timer or peer latency.
The strict cold-registration exchange precedes the service. SETUP for
`5551234`, traffic assignment, CONNECT ACKNOWLEDGE, physical End and
CC/RR release pass unchanged. Exact dial/Calling/clean-idle pixels pass,
and the entire SIM card is unchanged.

`tools/nsb7_pin_sms_reply.lua` uses the same own PIN-enabled registered
card and the incoming-SMS lab fixture. In this composition, physical Menu
at 23/25/27/29 seconds follows Messages/Inbox/sender/read and displays
the existing exact `hello` text oracle. The read status changes `03 -> 01`
over SIMI. The old two-contact notification-reader route is not assumed
to apply to this UI context.

The ordinary Reply sequence then starts at 32 seconds, enters `A` and
confirms Send/destination at 46. The existing exact SMS-SUBMIT to
`5551234`, CP/RP acknowledgements, handset completion and RR release
pass. The frame at 48.3 reads `Message sent`; the 74-second observation
also requires exactly one incoming transaction, with no fixture replay.

Sending additionally writes EF_SMSS (`6f43`), offset 0, length 2,
with `00 ff` after the accepted submission. Card byte 3560 changes
`ff -> 00`, matching the submitted TP-MR `00`. Acceptance checks this
exact APDU/order separately from the earlier delivery/read transaction.
The complete final card contains only the one read `hello` record and
that sender-reference change relative to the input; all other records,
credentials, retries and location data are preserved.

```sh
.venv/bin/python tools/nsb7_pin_services_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-pin-outgoing-call \
  tmp/nsb7-pin-sms-reply-qualified
```

Authentication and radio-startup scopes are kept as raw trace slices:
the last physical phone-code release bounds authentication, and TX58
bounds the first post-registration service. No log bytes or guest state
are rewritten to make the inherited protocol checks pass. This qualifies
PIN-authenticated outgoing call control and incoming/read/reply SMS.
The additional incoming-call and active-state coverage below remains
laboratory HLE signaling, not native speech or external telephony.

### PIN-authenticated incoming call and restoration

`nsb7_pin_incoming_call.lua` composes physical PIN authentication with
the existing incoming-call fixture. Physical Send at 22 seconds decodes
as `0x0e`, publishes CONNECT, and receives CONNECT ACK. Physical End at
27 seconds decodes as `0x0f` and completes DISCONNECT/RELEASE before
returning to the exact clean idle frame. The checker retains the exact
packet bodies and allows only the three authenticated-startup packets
preceding the established incoming-call inventory (95 packets total).
Whole-card persistence is unchanged.

`nsb7_pin_incoming_state.lua` saves the connected incoming call at
23 seconds, compares all 37 ARM/banked registers, RAM checksum and exact
emulated time after restoration, and compares three seconds of reference
and restored radio events and connected-call pixels. Physical End after
replay returns to the same clean idle frame without changing SIM storage.

The descriptor observer follows only the live `0x0402` object and its
ACK consumer at `0x2e5086`: the heap address is reused between registration
and calls, so unrelated writes to a freed slot must not exhaust its trace
budget or be attributed to the descriptor.

```sh
.venv/bin/python tools/nsb7_pin_services_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-pin-outgoing-call \
  tmp/nsb7-pin-sms-reply-qualified --incoming tmp/nsb7-pin-incoming-call-live
.venv/bin/python tools/nsb7_state_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-pin-incoming-state-live \
  tmp/nsb7-pin-registration-cold --incoming-call
```

### PIN-authenticated active SMS restoration

`nsb7_pin_sms_state.lua` composes the same physical PIN/phone-code and cold
registration with a save at 22.03 seconds, between the retained-clock CP-ACK
(22.024441) and RP-ACK (22.040198). Exact ARM/banked registers, RAM checksum,
emulated time, three seconds of protocol/CCONT replay and notification pixels
match after restoration. Physical Menu at 26/28/30/32 seconds reads `hello`;
the complete final card differs only by the delivered/read SMS record.
The `--pin` checker bounds authentication and registration before the save,
not merely somewhere in the combined reference/replayed trace.

```sh
.venv/bin/python tools/nsb7_state_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-ccont-sms-state-v2 \
  tmp/nsb7-pin-registration-cold --sms --pin
```

### PIN-authenticated persistent phonebook

`nsb7_pin_phonebook_save.lua` and `nsb7_pin_phonebook_read.lua` compose
physical PIN/phone-code entry and retained-location laboratory registration
with the established Names/Add and cold Find/Details routes. Each process
proves authentication before the first phonebook contact. Save emits exactly
one complete UPDATE RECORD for `A / 12345`; cold retrieval emits no UPDATE.
Exact Saved, Find and Details frames match the existing phonebook oracles.
The complete saved card equals the own PIN-enabled registered input with
only its first 32-byte ADN record replaced; the cold card is identical.
Credentials, retries, other records and location information are preserved.

The authenticated wrappers select the passive LAPDm/radio observer as the
menu harness's base observer, so registration is measured rather than inferred
from successful UI activity. The ordinary menu harness remains unchanged by
default.

```sh
.venv/bin/python tools/nsb7_phonebook_check.py \
  tmp/nsb7-pin-phonebook-save tmp/nsb7-pin-phonebook-cold \
  --pin-source tmp/nsb7-pin-registration-cold
```

### Permanent PUK1 exhaustion

`nsb7_puk_exhaustion.py` starts from the own organically PIN-exhausted card
with ten PUK retries. Ten independent cold processes physically enter a
wrong PUK and matching replacement PIN; each preserves its predecessor's
flash/card/clock files. Exact input/decode/APDU checks require one UNBLOCK
per process, `9804` while retries remain and `9840` at zero. Whole-card
comparison permits only the PUK1 retry byte to decrement; no replacement
credential or file change is accepted.

An eleventh cold process makes no credential attempt. It retains the exact
exhausted card and renders the reviewed `SIM card rejected` frame (RGB SHA256
`28d2c33c296179f37c24b66a42dd40014d9a8671e257f41338ca066c55bce3e0`).
This is permanent SIM-card rejection, not evidence of radio functionality.
The runner rejects an existing output directory to preserve prior evidence.

```sh
.venv/bin/python tools/nsb7_puk_exhaustion.py \
  tmp/nsb7-pin-blocked tmp/nsb7-puk-exhaustion-fresh
```

### Permanent PUK2 isolation

The same runner's `--pin2` mode starts from the own organically PIN2-blocked
card and physically consumes all ten PUK2 retries across independent cold
processes. Each process authenticates PIN1 and enters the phone code before
Access codes / PIN2 recovery. Exact UNBLOCK CHV2 bodies and contact decoding
are retained; `9804` becomes `9840` on the last attempt. Whole-card checks
permit only the PUK2 retry byte to decrement, preserving PIN1, PIN2, PUK1,
files and PIN-request policy.

An eleventh cold process physically authenticates PIN1, accepts the phone code
and navigates Messages / Call log / Missed calls with the existing exact
frames. It makes no credential mutation and retains the complete exhausted
card. Thus permanent PIN2-service loss does not imply whole-card rejection.
The endurance sequence also exercises the independently shadow-validated
flash-journal rotation described above.

`--check-only` revalidates retained traces/card chains without writing evidence.
`--resume` revalidates existing complete processes and runs only missing ones;
an incomplete or invalid retained process stops rather than being overwritten.

```sh
.venv/bin/python tools/nsb7_puk_exhaustion.py \
  tmp/nsb7-pin2-blocked tmp/nsb7-puk2-exhaustion-fresh --pin2
.venv/bin/python tools/nsb7_puk_exhaustion.py \
  tmp/nsb7-pin2-blocked tmp/nsb7-puk2-exhaustion --pin2 --check-only
```

### Fixed-dialling activation and persistence

The Nokia [8290 user guide](https://www.instructionsmanuals.com/sites/default/files/2019-05/Nokia-8290-en.pdf)
identifies Menu 4-6-4 as Fixed dialing. This is a harness navigation source,
not a hardware or APDU specification. The explicit laboratory FDN extension
is generated by `sim_fdn_profile.py` from the own registered PIN-enabled
card; no handset identity or firmware state is edited. Startup reads the
advertised FDN records organically.

Four isolated processes compose physical PIN1 and phone-code entry with:

| Phase | Input script | Result |
| --- | --- | --- |
| On | `nsb7_fdn_input.lua` | Physical PIN2 `5678` verifies; ADN is invalidated and LOCI/IMSI rehabilitated through SIMI; Fixed dialing On renders. |
| Cold On | `nsb7_fdn_cold.lua` | On persists; startup rehabilitates LOCI/IMSI without another PIN2 attempt. |
| Off | `nsb7_fdn_disable.lua` | Physical PIN2 verifies; LOCI/IMSI/ADN are rehabilitated; Fixed dialing Off renders. |
| Cold Off | `nsb7_fdn_cold.lua` | Off persists without any validity-changing APDU. |

The checker requires the complete physical contact/decode inventory, exact
VERIFY bodies and selected-file invalidation/rehabilitation sequence, own
flash-journal integrity and reviewed On/Off pixels. Each process copies the
predecessor's own flash/card/clock; whole-card comparison permits only the
ADN-validity byte to change during On/Off, with no cold-card change.
The activation fixture's complete two-Select sequence is retained explicitly
in the input script; menu presentation alone is not used as protocol proof.

```sh
.venv/bin/python tools/nsb7_fdn_check.py \
  tmp/nsb7-fdn-on-select tmp/nsb7-fdn-cold-on \
  tmp/nsb7-fdn-off tmp/nsb7-fdn-cold-off
```

This qualifies FDN discovery, PIN2 authorization and durable On/Off, not
protected FDN record CRUD, listed/unlisted dialing or connected calls with
FDN enabled. The radio-disabled fixture keeps those claims separate.

### Protected FDN Save and cold retrieval

With the own cold FDN-On card as input, `nsb7_fdn_write.lua` physically
selects Fixed dialing / Numbers / Add, authenticates PIN2 `5678` and saves
`A / 12345`. Exactly one successful UPDATE RECORD targets selected EF_FDN
`6f3b`, record 1, absolute mode 4, with the complete 32-byte body. Ordinary
ADN is not the update target. The full card differs only in that FDN record;
FDN validity, other records and credentials remain unchanged.

An independent process copies the saved own storage and physically executes
Numbers / Search / empty search / Details through `nsb7_fdn_read.lua`.
Reviewed list and number frames display `A` and `12345`; no PIN2 request or
record update occurs, and the entire card remains identical.

The negative fixture `nsb7_fdn_write_wrong.lua` starts from the same blank
FDN-On source and submits `5675`. Firmware renders Code error; no UPDATE is
sent, and complete-card comparison permits only PIN2 retries `3 -> 2`.
Startup identity rehabilitation is independently checked in all three runs.
The shared APDU parser tracks selected files and complete command bodies,
so an ADN write or a command-like byte sequence inside record data cannot
satisfy the protected FDN write check.

```sh
.venv/bin/python tools/nsb7_fdn_record_check.py \
  tmp/nsb7-fdn-cold-on tmp/nsb7-fdn-write tmp/nsb7-fdn-read \
  tmp/nsb7-fdn-write-wrong
```

Listed-versus-unlisted dial permission remains a separate acceptance target.
These radio-disabled storage runs do not
establish a connected FDN-enabled call.

### FDN erase cancellation and durable deletion

Starting from the saved own card, `nsb7_fdn_cancel.lua` physically reopens
Details, verifies PIN2 and selects Erase. The reviewed `Erase? A` prompt is
dismissed with the real Back contact; the contact list returns and no UPDATE
is sent. The whole card remains byte-identical.

`nsb7_fdn_erase.lua` cold-starts from that unchanged cancellation output and
confirms the same prompt with OK. Exactly one successful EF_FDN record-1
UPDATE writes 32 erased bytes after PIN2 authentication. Firmware renders
`Phone book erased`; the entire card changes only in that record.
`nsb7_fdn_empty.lua` then cold-starts from the erased output and physically
searches, rendering `No phone numbers` without PIN2 verification, an UPDATE
or any persistent card change. FDN remains enabled throughout.

The checker requires all three optional phases together and validates the
actual predecessor-card link, full card, exact input/decode inventory,
selected-file APDUs, own flash integrity and prompt/result pixels.

```sh
.venv/bin/python tools/nsb7_fdn_record_check.py \
  tmp/nsb7-fdn-cold-on tmp/nsb7-fdn-write tmp/nsb7-fdn-read \
  tmp/nsb7-fdn-write-wrong --cancel tmp/nsb7-fdn-cancel \
  --erase tmp/nsb7-fdn-erase --empty tmp/nsb7-fdn-empty
```

### Protected FDN edit and cold edited-number retrieval

`nsb7_fdn_edit.lua` starts from the saved own `A / 12345` FDN-On card,
physically reopens Details and authorizes Options with PIN2. Edit retains
name `A`; Clear removes the final digit and keypad `6` replaces it. One
successful EF_FDN record-1 UPDATE writes the exact `A / 12346` record after
PIN2 verification. The complete card changes only record byte 22 (`f5 -> f6`)
at persistent offset 3600, leaving validity, credentials and all other files
unchanged. Firmware renders the reviewed Saved to SIM card frame.

A separate cold process uses the existing physical Numbers/Search/Details
route, retains the entire edited card and emits no UPDATE or PIN2 request.
The reviewed detail frame displays `12346`, independently of the stored-byte
assertion. Paired `--edit` / `--edit-cold` arguments are required; incomplete
pairs are rejected, and the original Save/read/wrong-PIN2 checks still run.

```sh
.venv/bin/python tools/nsb7_fdn_record_check.py \
  tmp/nsb7-fdn-cold-on tmp/nsb7-fdn-write tmp/nsb7-fdn-read \
  tmp/nsb7-fdn-write-wrong --edit tmp/nsb7-fdn-edit \
  --edit-cold tmp/nsb7-fdn-edit-cold
```

### Registered FDN dial permission

Three measurement-enabled cold compositions use the own edited FDN record
`A / 12346`, with physical PIN1 and phone-code entry before dialing. The
original physical outgoing-call harness now accepts explicit fixture digits;
its default `5551234` and existing gates are unchanged.

| Control | Physical number | Observed result |
| --- | --- | --- |
| FDN On, listed | `12346` | CM Service Request, exact called-party SETUP, one traffic assignment, CONNECT/ACK, physical End and complete CC/RR release. |
| FDN On, unlisted | `12347` | Exact Call not allowed frame; no CM Service Request, SETUP, traffic assignment or CONNECT. |
| FDN Off, same unlisted | `12347` | The ordinary physical PIN2-authorized Off route precedes a new process; CM/CC/RR completes and physical End releases. |

The On controls start with identical source cards. Physical Off changes only
ADN validity; all dial runs retain their entire input card, including the FDN
record and retries. Actual contacts must decode while held. Authentication
and retained-location registration are qualified before the first dial key.
All three final frames match the separately reviewed 13:49 DCT3 LAB idle
oracle: the own retained RTC has advanced through the earlier physical cold
workflows. The original 13:47 oracle is unchanged; neither time nor pixels
are rewritten or masked.

The Off control emits identical SETUP bodies first on SDCCH and then on TCH
after Assignment Complete, with the same observed request ID. Its explicit
`setup_transfer` check requires exactly those two packets, their channel
prefixes, identical bodies/request identity and ordering before CONNECT.
Default outgoing-call acceptance still rejects duplicate SETUP. This records
the observed assignment-crossing sequence, not a generic permission to ignore
duplicates or a hardware timing claim.

```sh
.venv/bin/python tools/nsb7_fdn_dial_check.py \
  tmp/nsb7-fdn-edit-cold tmp/nsb7-fdn-dial-listed \
  tmp/nsb7-fdn-dial-unlisted tmp/nsb7-fdn-dial-disable tmp/nsb7-fdn-dial-off
```

This establishes bounded FDN enforcement and permitted lab-HLE call signaling,
not emergency-number exceptions, wildcard/prefix matching, native speech,
external RF or a SIP-connected call.

## Authenticated host SMS

Two independent compositions qualify the existing external WebSocket backend
on NSB-7 without changing driver/device behavior. Both copy only the own
PIN-enabled registered flash/card/clock, physically authenticate PIN1 and
phone code, and qualify retained-location registration before service input.

| Direction | Configuration / input | Acceptance |
| --- | --- | --- |
| Incoming | `fixtures/nsb7_host/nsb7hle.cfg`, `nsb7_pin_sms_read.lua`, `run_host_incoming_sms_gate.py` | Automatic lab SMS is disabled. After registration on PCS carrier600, the host supplies one `hello` from5551234. Queued/delivered host events and exact paging/CP/RP precede physical Menu23/25/27/29 reading and the existing hello frame. Whole card permits only that delivered/read SMS record. |
| Outgoing | `fixtures/nsb7_host_sms_reply/nsb7hle.cfg`, `nsb7_pin_sms_reply.lua`, `run_host_sms_gate.py --user-data 41 --user-data-length 1` | The incoming source is the ordinary lab hello fixture; physical Reply submits A/5551234 to the host. Wrong request ID is rejected, the correlated accept is consumed once and its duplicate rejected. Handset CP/RP closes; full card permits only read status and the exact EF_SMSS reference update. |

The outgoing final frame is the reviewed composer containing `A`, not a
captured success banner. This is consistent with completed protocol/storage
acceptance but must not be described as a Message sent frame. The standalone
incoming and outgoing runs do not claim a single host-originated-message to
host-reply session, external SMS carrier delivery, SIP or native RF/speech.

Run the existing host runners with the isolated MAME command, `-http` and
matching `-http_port` (qualified local ports18930 incoming,18929 outgoing).
The handset scripts exit at35 and74 emulated seconds respectively. Host
runner success alone is insufficient: require the combined checker too.

```sh
.venv/bin/python tools/nsb7_host_sms_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-ccont-sms-in \
  tmp/nsb7-ccont-sms-out
```

The independent retained-clock repeats `tmp/nsb7-ccont-sms-in` and
`tmp/nsb7-ccont-sms-out` pass this same checker without changing any message
frame hashes, CP/RP acceptance or full-card expectations. They use the own
unedited13:47:45 source. The separate authenticated SMS save/load repeat
above retains its CP-ACK/RP-ACK boundary and now compares CCONT replay too.

### Incoming host SMS ingress restoration

`nsb7_host_incoming_sms_state.lua` uses the same PIN-enabled own-source
storage and host configuration with automatic lab SMS disabled. The external
host admits one`hello` from5551234 after PCS600 registration. A read-only
task14 CP-DATA observation at`2645ec` triggers save within110microseconds,
after PIN/phone authentication but before CP/RP acknowledgement or SIM update.
No message, timer result or firmware state is injected by the observer.

The reference executes for3s, including one exact unread SMS write and
CP-ACK/RP-ACK; load then restores all37 ARM/banked registers, PC/SP, full-RAM
digest and time. The full reference/restored transport, GSM, SIM, RTC and
analog observations compare exactly (apart from already-declared diagnostic
timestamps). Host phases are queued/delivered under epoch1, then queued/delivered
under epoch2, with only one external admission and no host resend. The runner
must remain attached past reference delivery to observe the restored epoch;
it also rejects Lua/validity/fatal console errors independently of exit status.

Physical Menu26/28/30/32 reads the message and produces the exact hello frame.
The final complete card contains only the one delivered/read SMS change.
Whole-card reading checks use the restored branch, after independently proving
that the reference and restored branches each store/acknowledge once and replay
identically; discarded reference execution is not counted as a second persisted
message. Both replay-end frames equal the reviewed Nokia startup frame, not a
message notification. A35.1s endpoint observes all35 retained RTC ticks; it does
not claim the longer call fixture's later analog-control0f transition.

Use`run_host_incoming_sms_gate.py --port 18950 --require-restore` around the
isolated MAME command with matching HTTP port, this script and42s guard.
Capture its stdout/stderr and require the product checker too:

```sh
.venv/bin/python tools/nsb7_host_incoming_sms_state_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-host-sms-ingress-qualified \
  --console tmp/nsb7-host-sms-ingress-qualified-launch.log
```

The ordinary one-shot external delivery/read control passes independently with
`nsb7_pin_sms_read.lua`, without`--require-restore`. Passive screenshot waits
cancelled by load return normally; physical key-action failures remain errors.
This is host-originated signaling/storage restoration, not external carrier
delivery, SIP SMS, native DSP, speech or handset-media continuity.

### Pending outgoing host SMS restoration

`nsb7_host_sms_submit_state.lua` preserves the authenticated physical Reply
while it waits for an external result. The unchanged host-SMS configuration
delivers the source `hello` through the ordinary laboratory path; physical
read/Reply submits one `A` to5551234. Firmware publishes CP-ACK and request1/
epoch1 at47.53s. The host runner's `--require-restore` withholds its decision.

Save48/load after0.5s restores all37 ARM/banked registers, PC/SP, RAM digest
and emulated time exactly. No RP result may precede save. The pending request
republishes unchanged under epoch2; explicit stale-epoch, wrong-ID and
duplicate rejection controls surround exactly one accepted new-epoch decision.
The adapter's stale-SMS trace records the supplied/current epochs without
changing its existing rejection behavior. One original SMS-SUBMIT, ordered
RP-ACK/CP closure/RR release and epoch2 ended are mandatory.

Whole-card acceptance permits only the incoming record/read status and one
exact EF_SMSS update after the original submission. Snapshots independently
pin the saved/restored composer cursor phases,49s `Message sent`,74s composer,
82s menu and88s registered idle. The screen cache can retain the cursor phase
rendered before load; these are reviewed individual frames, not a framebuffer
or external-event replay claim. Physical End78/Menu82/End85 must decode, and
all retained RTC ticks, own flash journal, healthy console and actual `.sta`
file are checked. No card contents or firmware state are forced.

Prepare the same fresh own-source storage/configuration as ordinary outgoing
host SMS. Use `run_host_sms_gate.py --port 18949 --user-data 41
--user-data-length 1 --require-restore`, the isolated MAME command with matching
HTTP port, script `../tools/nsb7_host_sms_submit_state.lua`, and94s guard:

```sh
.venv/bin/python tools/nsb7_host_sms_submit_state_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-host-sms-submit-state-epoch
```

The independent non-restored control uses `tmp/nsb7-host-sms-submit-control`,
ordinary `nsb7_pin_sms_reply.lua` and no `--require-restore`. This qualifies
pending host-decision restoration, not an external SMS carrier, SIP SMS,
every CP/RP timeout phase or native DSP/RF behavior.

### Host SMS rejection and silence

Fresh independent copies of the same PIN-enabled source qualify RP-ERROR and
RP silence, not just the host-accepted Reply. Both use
`fixtures/nsb7_host_sms_reply/nsb7hle.cfg` and the existing host SMS runner
with `--user-data 41 --user-data-length 1`. Wrong-ID and duplicate decisions
reject while exactly one matching decision is accepted.

| Host decision | Physical script / local port | Firmware outcome and recovery |
| --- | --- | --- |
| `rp_error` | `nsb7_pin_host_sms_failure.lua` /18938 | RP-ERROR, handset CP-ACK and release; exact `Message not sent, try again` frame at48.3s. End78/Menu82/End85 restores exact registered idle at88s. |
| `rp_silence` | `nsb7_pin_host_sms_silent.lua` /18940 | No RP-ACK or RP-ERROR; exact `Message not sent` frame at105s, handset timeout/DISC/UA/deconfiguration and host ended at114.58s. End168/Menu172/End175 restores exact registered idle at178s. |

Observation must extend beyond the firmware timeout; an88s run leaves the
silent transaction pending and is not accepted as completion. Use95/185s
launch guards respectively. The physical scripts terminate at88/178s.

Both complete cards equal the incoming/read message projection with the
original EF_SMSS bytes retained. Unlike successful Reply, neither failure
writes EF_SMSS or advances its reference. The checker enforces that
distinction, single submission/completion, physical decoded contacts, exact
failure/menu/final frames, valid own flash journal and every retained RTC
tick. The successful host SMS gate retains its original mandatory reference
update. No driver behavior, state forcing or host-side success substitution
is needed.

```sh
.venv/bin/python tools/nsb7_host_sms_failure_check.py roms/noki8290/8290f522a.fls tmp/nsb7-host-sms-reject
.venv/bin/python tools/nsb7_host_sms_failure_check.py roms/noki8290/8290f522a.fls tmp/nsb7-host-sms-silent-long --rp-silence
```

These are laboratory HLE signaling and UI/storage results, not an external
SMSC, SIP SMS, fitted resident DSP or hardware RF qualification.

## Host-originated USSD notification

The own PIN-enabled registered card and `fixtures/nsb7_host/nsb7hle.cfg`
also qualify one external supplementary-service notification. The physical
PIN/phone-code fixture emits a host readiness marker at24s; the host then
admits `Host notice` once, without a firmware write or fabricated handset
message. PCS600 registration and both MCU location-update consumers remain
required by the existing authentication checker.

The qualified run admits the request at24.010s, receives the network notify
at25.735s, returns result `8b3a05a203020101` at26.284s and completes RR
release/delivery at26.320s. Physical Menu30 opens the full `Host notice` text;
End32 returns to exact13:48 `DCT3 LAB` idle, and Menu34 continues navigation.
The whole3570-byte card remains unchanged and the own flash journal is valid.
Reviewed RGB hashes are pinned by `nsb7_host_ussd_check.py`; an adapter-level
delivered event alone is insufficient.

Starting with a fresh copy of `tmp/nsb7-pin-registration-cold/nvram`, copy
the host cfg to `RUN/cfg/nsb7hle.cfg` and retain the initial card as
`RUN/initial_sim_card.bin`. Do not copy RAM or save states. Then run:

```sh
.venv/bin/python tools/run_host_incoming_ussd_gate.py --port 18950 \
  --text 'Host notice' --ready-marker 'nsb7_ussd: ready' -- \
  .venv/bin/python tools/run_mame_isolated.py --mame-dir mame --run-dir RUN -- \
  nsb7hle -rompath ../roms -video none -sound none -nothrottle \
  -seconds_to_run 42 -log -verbose -debug -debugger none -http \
  -http_port 18950 -autoboot_delay 0 \
  -autoboot_script ../tools/nsb7_host_ussd_notify.lua > RUN-console.log 2>&1
.venv/bin/python tools/nsb7_host_ussd_check.py \
  roms/noki8290/8290f522a.fls RUN --console RUN-console.log
```

Qualified capture: `tmp/nsb7-host-ussd-authenticated`. The console marker is
fixture coordination after physical input, not a device readiness claim.
This is a one-way notification with physical reading/dismissal, not an
interactive USSD reply session, mobile-originated USSD, native DSP/RF or speech.
The shared runner drains and checks the complete console, so a Lua input error
cannot be mistaken for success. Its restore mode validates each epoch's
ordered lifecycle and waits for restored delivery even if the reference
already delivered; this handset notification gate does not exercise restore.

### Physical mobile-originated USSD

`nsb7_host_ussd_request.lua` shares the read-only authentication/radio
observer and physical-input engine with the notification fixture. It presses
`*`, `1`, `2`, `3`, `#` at26..30s and Send32. The own MCU emits REGISTER
`1b7b1c14a11202010102013b300a04010f0405aa986c36027f0100` at33.622s:
operation59, DCS0f and packed `*123#` payload `aa986c3602`.

The host deliberately submits request2 first; it is rejected. The matching
request1 succeeds and returns `Host network`. The MCU receives RELEASE
COMPLETE, acknowledges RR release, and the host session ends34.200s before
physical End38. Exact response frames36/37, registered idle39 and Messages
menu41 pass. The complete SIM card remains unchanged; the own flash journal
and initial PIN/phone authentication are independently checked.

Use the same fresh-card/host-cfg preparation as the notification gate:

```sh
.venv/bin/python tools/run_host_ussd_gate.py --port 18950 -- \
  .venv/bin/python tools/run_mame_isolated.py --mame-dir mame --run-dir RUN -- \
  nsb7hle -rompath ../roms -video none -sound none -nothrottle \
  -seconds_to_run 48 -log -verbose -debug -debugger none -http \
  -http_port 18950 -autoboot_delay 0 \
  -autoboot_script ../tools/nsb7_host_ussd_request.lua > RUN-console.log 2>&1
.venv/bin/python tools/nsb7_host_ussd_check.py \
  roms/noki8290/8290f522a.fls RUN --console RUN-console.log --request
```

Qualified capture: `tmp/nsb7-host-ussd-request-qualified`. This proves one
mobile-originated request and host-decided result, not multi-turn USSD,
arbitrary alphabet coverage, native DSP/RF or speech. Pending restoration
has its separate gate below.
The shared host runner requires a stable positive request ID, at most one
request per epoch, correlated accepted/ended states and a healthy complete
console. Restore fixtures require the next transport epoch and republication
of the same request, not a new external admission.

### Pending USSD host decision across save/load

`nsb7_host_ussd_restore.lua` saves at33.8s after the physical `*123#`
request is published but before any host response. The runner withholds
the epoch1 decision, observes one second of pending reference execution and
loads the actual `nsb7_ussd.sta`. All37 ARM/banked registers, PC/SP, full-RAM
digest and emulated time restore exactly.
The discarded reference has its own exact reviewed `Requesting` frame,
distinct from the restored result.

Request1 republishes once under epoch2. The runner submits the old-epoch
response, a wrong request2 response and then the matching response; the
first two are rejected and only the third succeeds. The qualified run ends
the USSD/RR session34.200s, before physical End38. The existing exact
`Host network`36/37, idle39 and menu41 frames, unchanged complete card and
valid own journal pass. After excluding the discarded reference interval,
all43 retained RTC ticks remain consecutive.

Use the same fresh input preparation and command as the ordinary request
gate, adding `--require-restore` to `run_host_ussd_gate.py` and replacing
the Lua script with `nsb7_host_ussd_restore.lua`. Qualification:

```sh
.venv/bin/python tools/nsb7_host_ussd_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-host-ussd-request-restore \
  --console tmp/nsb7-host-ussd-request-restore-launch.log --request --restore
```

This proves pending-session restoration and epoch-safe host completion.
Reference and restored protocol traffic are intentionally unequal: only the
restored branch receives a host result. No equal-traffic replay, external
dialogue continuity, multi-turn USSD or native DSP/RF/speech is claimed.

### Laboratory continued USSD

`fixtures/nsb7_ussd_continued/nsb7hle.cfg` selects the laboratory continuation
outcome without enabling the host backend. Start from the same own,
PIN-enabled registered cold NVRAM used above; copy no RAM or save states.
Retain the starting card as `RUN/initial_sim_card.bin`, and copy the cfg to
`RUN/cfg/nsb7hle.cfg`.

`nsb7_ussd_continued.lua` physically dials `*123#` and presses Send at 32 s.
The network's FACILITY uses mandatory LV framing (not the tagged Facility
IE of REGISTER), with Invoke 2 and `Enter reply`. Answer at 36 s opens the real
editor; Keypad 2 at 38 s enters numeric `2`; Send at 40 s emits the handset's exact
ReturnResult `1b3a12a210020102300b02013c300604010f040132`.
The session checks transaction/Invoke correlation, returns `Nokia test network`
for the original Invoke 1, and completes RR release at 40.373 s. End at 44 s and
Menu at 46 s recover the existing exact idle/menu frames. The entire 3,570-byte
card is unchanged; source flash/journal and physical authentication are checked.

```sh
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame --run-dir RUN -- \
  nsb7hle -rompath ../roms -video none -sound none -nothrottle \
  -joystickprovider none -midiprovider none -seconds_to_run 54 -log -verbose \
  -debug -debugger none -autoboot_delay 0 \
  -autoboot_script ../tools/nsb7_ussd_continued.lua > RUN-launch.log 2>&1
.venv/bin/python tools/nsb7_ussd_continued_check.py \
  roms/noki8290/8290f522a.fls RUN
```

The checker pins six reviewed frames: prompt, empty editor, numeric reply,
final network text, idle and Menu, plus exact physical contacts and protocol
order. Qualified capture: `tmp/nsb7-ussd-continued-complete`.
This is one continued laboratory dialogue on research HLE, not host-controlled
multi-turn USSD, arbitrary repeated prompts or native RF/DSP/speech.

## Network-originated laboratory USSD request

`fixtures/nsb7_network_ussd_request/nsb7hle.cfg` selects the network request
without a host backend. Use the same own PIN-enabled cold NVRAM preparation as
the continued-dialogue fixture above, but copy this cfg and run
`tools/nsb7_network_ussd_request.lua` with a 46-second limit. The script exits
at 41 seconds. It presses End at 24 seconds to dismiss the startup presentation
before Read at 26, Answer at 28, numeric 2 at 30 and Send at 32.

The 29-byte REGISTER arrives at 21.455 seconds; the actual handset returns
`8b3a12a210020101300b02013c300604010f040132` at 32.247 seconds. Unlike NSE-8,
NSB-7 does not set the FACILITY sequence bit here; both encodings are accepted
by the bounded parser. Transaction 8b/invoke 1 are checked before RELEASE
COMPLETE and RR release at 32.279 seconds. Firmware displays Done. End at 36
and Menu at 38 recover the exact existing idle and Messages-menu frames.

```sh
.venv/bin/python tools/nsb7_network_ussd_request_check.py \
  roms/noki8290/8290f522a.fls RUN
```

The checker requires own source/journal, pre-request authentication, all physical
contacts, ordered protocol exchange, six reviewed frames and the unchanged
complete card. Qualified capture: `tmp/nsb7-network-ussd-request-clean`.
This proves one network-originated laboratory reply, not host-controlled
multi-turn dialogue or native DSP/RF/speech. Without the physical End dismissal,
the same early request remains pending behind the startup presentation and the
planned Read key enters the menu instead; that run is not reply acceptance.

For pending-editor restoration, use the same fixture/cold storage and run
`tools/nsb7_network_ussd_state.lua`, then the checker above with `--state`.
Save at 29.25 seconds follows physical Answer and precedes all reply input.
Both branches physically press 2 at 29.75 and Send at 30.75. The exact reply
occurs at 31.223902 in each branch; all 37 ARM/banked registers, RAM checksum,
time and the three-second radio/SIM/RTC/SS replay agree. Both branches show the
same Done frame; restored End at 36 seconds recovers exact idle at 38 with the
complete card unchanged. Capture: `tmp/nsb7-network-ussd-state`.
This tests a pending laboratory dialogue, not restoration of an external host
session or arbitrary continuation.

The external counterpart uses `fixtures/nsb7_host_ussd_request/nsb7hle.cfg`
and `nsb7_host_network_ussd_request.lua`, with the same own cold storage.
Wrap the isolated MAME invocation with `run_host_incoming_ussd_gate.py
--port 18950 --text 'Enter reply' --request --expected-reply 2
--ready-marker 'nsb7_ussd: ready'`, enable MAME HTTP on that port and use a
50-second limit. The fixture exits at45s. Admission follows authentication
at24.010s. Read30/Answer32/Keypad2 at34/Send36 yield the same typed ReturnResult
at36.532s; RR release completes36.564s. Only afterward, at36.570s, the host
receives identity1/epoch1/DCS15/data32/empty=false and then `answered`.
End40/Menu42 recover exact idle/menu with unchanged whole card.

`nsb7_network_ussd_request_check.py IMAGE RUN --host` checks own source/journal,
authentication, physical contacts, six reviewed frames, ordered admission/
reply/RR/host publication and complete card equality. The Done animation has
its own reviewed phase hash rather than weakening the frame predicate.
Capture: `tmp/nsb7-host-network-ussd`. This is a single real host request/reply;
the standalone bridge CLI notification default is unchanged, and multi-turn
dialogue remains unqualified.

Pending host-editor restoration uses the same host fixture and cold storage,
but runs `nsb7_host_network_ussd_state.lua` with runner `--require-restore` and
checker `--host --state`. Save33.25s precedes physical2 at33.75 and Send34.75
in each branch. Both branches emit the identical handset result at34.997178s;
all37 ARM/banked registers, RAM checksum, time and three-second radio/SIM/RTC/SS
replay agree. The host receives one data32 response after each branch's RR
release at35.03s, first under epoch1 and then epoch2. Queued/answered phases
are independently required per epoch; only one initial request is admitted.
Both Done frames and restored End40/final42 idle match exactly, with complete
card preservation. Capture: `tmp/nsb7-host-network-ussd-state`. This establishes
same-process pending-dialogue restoration, not external network continuity or
arbitrary multi-turn continuation.

Physical editor dismissal is independently checked by
`nsb7_host_network_ussd_empty.lua` with the same host fixture/seed. The runner
uses `--request --empty-reply`; the checker uses `--host --empty-reply`.
After Read30/Answer32, End34 sends the firmware's empty ReturnResult
`8b3a05a203020101` at34.470s, not RELEASE COMPLETE or a typed empty string.
RR release completes34.501s. The host receives DCS0/data empty/empty=true and
then `answered` at34.510s, preserving the wire result rather than inventing
text or relabelling it as a no-result termination. Exact prompt/editor and
four idle/menu frames pass, with unchanged complete card. Capture:
`tmp/nsb7-host-network-ussd-empty`. The generic host `cancelled` phase remains
the distinct RELEASE-COMPLETE-without-result implementation path, not the
behavior qualified by this NSB-7 End-key fixture.

## Host incoming call from settled idle

`nsb7_pin_host_incoming_call.lua` composes physical PIN/phone-code entry with
the radio observer and named Send/End contacts. The host fixture uses
`fixtures/nsb7_host/nsb7hle.cfg`: automatic incoming calls are disabled.
At45s the handset emits a readiness snapshot matching the exact clean idle
oracle; the host runner must start from a fresh, absent readiness file and
wait for that artifact before requesting caller `447700900123`.

The settled boundary is part of acceptance, not just a host synchronization
convenience. A startup logo or an arbitrary existing PNG must not satisfy it,
even if signaling happens to work underneath. Authentication and PCS600
registration must precede readiness; the checker verifies the ready pixels.

The exact ringing frame at51s displays `447700900123`. Physical Send52
decodes0e, emits CONNECT and receives CONNECT ACK before the host reports
connected. The53s frame shows the firmware's active `Call 1` UI. Physical
End60 decodes0f and completes DISCONNECT/RELEASE/RELEASE COMPLETE, then host
ended and common-control/background return. The68s final frame equals clean
idle. Host request1 remains in epoch1 through the exact queued/paging/
alerting/connected/media_closed/ended sequence; `media_closed` is lifecycle
bookkeeping, not evidence of speech. The complete SIM card is unchanged.

Use `run_host_incoming_signaling_gate.py --port 18932 --ready-file
tmp/nsb7-pin-host-incoming-idle/snap/nsb7_call_ready.png` around the isolated
MAME command with `-http -http_port 18932`, this Lua script and a75s guard.
Require the handset checker as well as runner success:

```sh
.venv/bin/python tools/nsb7_host_incoming_call_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-pin-host-incoming-idle
```

This validates external host signaling and physical UI acceptance, not host
media, native speech, an external carrier or a SIP-connected call. Connected
host-call restoration is qualified separately below.

## Physical power-off and authenticated restart

The explicit `nsb7hle` composition now qualifies physical Power45..49,
firmware CCONT rail-off at51.442157231s, a blank55s frame and physical
PWRONX65..67. Wake occurs at65.016666667s with cause02; firmware reads
ready/PWRONX without charger. Every RTC second remains continuous through
the13.57s off interval and the subsequent boot. The checked DSP/radio
endpoints produce no activity while rails are off.

Restart requires fresh physical PIN1234/OK73..77, accepted before the
retained-LAI Location Updating transaction completes at78.239702s.
Phone code12345/OK81..86 is independently accepted. The90s frame is still
the Nokia startup presentation, not settled idle. Physical Menu95/End98
proves restored interaction; the exact101s frame is registered13:49 idle.
The complete SIM card is unchanged, no retained-location file write occurs,
and the own flash journal remains valid. This is the research-HLE product,
not promotion of normal `noki8290`, native speech or active-call power loss.

Use the qualified PIN-enabled registered source and
`fixtures/nsb7_host/nsb7hle.cfg` with `nsb7_power_cycle.lua`, an isolated
MAME launch and110s guard. The script exits at101s. Validate with:

```sh
.venv/bin/python tools/nsb7_power_check.py roms/noki8290/8290f522a.fls tmp/nsb7-power-cycle-clean
```

The ring observer resets only its diagnostic cursor at55s, while rails are
off; its capture index remains monotonic. Retaining the old cursor would
misread reset-cleared words as zero-length packets. No guest memory,
device state or decoded result is supplied by that reset. Boot-time context
clears are also outside the separately delimited new Location Updating
transaction; the shared consumer checker still requires its complete
ordered downlink, acknowledgement and release-state sequence.

## Host-decided outgoing call

`nsb7_pin_host_outgoing_call.lua` uses the same own PIN-enabled registered
storage and `fixtures/nsb7_host/nsb7hle.cfg`. Physical authentication and the
exact45s clean-idle snapshot precede dial digits46..52 (`5551234`), Send54,
End66 and final76s. Exact frames cover ready idle, displayed number, Calling,
the60s active `Call 1` UI and return to idle. The complete SIM card remains
unchanged and the own flash journal stays valid.

The host receives request1/epoch1 for5551234. A wrong-ID busy decision is
rejected; the correlated connect decision queues and consumes once; its
duplicate is rejected. One SETUP and traffic assignment lead to CONNECT/ACK,
host connected, physical End and the complete CC/RR release, followed by
media_closed/ended in the same epoch. Media sequence counters remain zero;
the phase name is not speech evidence.

The shared runner's `--observe-end` option keeps its WebSocket attached until
connected then ended, with one90s wall-clock budget. It submits no remote
termination or media and rejects request/epoch changes. It requires connect
and cannot be combined with restoration, media echo or remote termination.

Use `run_host_call_adapter_gate.py --port 18933 --decision connect
--observe-end` around the isolated MAME command with `-http -http_port 18933`,
this Lua script and an82s guard; require the handset checker too:

```sh
.venv/bin/python tools/nsb7_host_outgoing_call_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-pin-host-outgoing-call
```

This qualifies host-decided outgoing signaling with physical/UI continuity,
not host speech, SIP media, native RF or restoration of an external dialog.

## Own PCM documentation and physical speech control

The acquired NSB-7 service manual, System Module printed page27/PDF page41,
visibly specifies COBBA-generated 512 kHz PCMDClk and 8 kHz PCMSClk:
64 clocks/frame, one active-high sync clock, a 16-bit MSB-first word with
bits15..13 extending bit12, data changing on rising edges and sampled on
falling edges. The MAD pin table identifies the clocks as inputs from
COBBA. UI printed page8/PDF page88 identifies internal MIC2, headset MIC1
and differential internal EAR. These establish framing and routing, not
measured analog gain or working emulated media.

`tools/nsb7_speech_control_check.py` pins the acquired v5.22 image hash,
instructions, selector table and literal pools. Compiler `0x2cb62c` uses
table `0x2cb660`: selector17 updates bit`0x0200` in shadow`0x12df02`,
selector8 compiles the command, and writer`0x2cb93e` stores to`0x100a8`.
The authenticated physical outgoing host-call probe observes Send54s ->
decoded`0x0e` -> `860b`, then End66s -> decoded`0x0f` -> `840a`, through
that own writer. No PCM configuration or firmware-state mutation was added.

```sh
.venv/bin/python tools/nsb7_speech_control_check.py \
  roms/noki8290/8290f522a.fls \
  --log tmp/nsb7-own-speech-control-probe/error.log
```

The manual's System Module parts list (printed64/PDF78) names
MAD2WD1 v18 ROM5. The research composition's compiled identity6 branch is
not a measured fitted-mask identity; the BOM is not grounds to silently
replace it. Resident DSP identity/execution and native speech remain open.
The product configuration now applies those own framing/routing facts and
the command8/bit0200 speech predicate. A headless authenticated physical
host-connected outgoing call (`tmp/nsb7-own-pcm-qualified`) executes500
uplink and493 downlink blocks. `tools/nsb7_pcm_check.py` checks own
Send/End controls, transfers bounded by enable/disable, 512000/8000 clocks,
64-clock frames and exact47-idle-clock accounting. This is HLE PCM transport
with a laboratory voice peer, not SIP waveform or native DSP speech proof.
Microphone input is silent in this headless test.

An independent real-time outgoing SIP call also passes the own authentication,
physical dialing/release, exact UI/card/journal and clocked PCM checks:
`tmp/nsb7-sip-outgoing-media-realtime` reports474 uplink,451 downlink,
474 PCM transmitted and508 received at the bridge. Normal SIP confirmation
and release and contiguous handset downlink acceptance are checked. Use
the existing SIP command without `--signaling-only`, with `-throttle` in
place of `-nothrottle`, and run the final checker with `--media`:

```sh
.venv/bin/python tools/nsb7_sip_check.py roms/noki8290/8290f522a.fls \
  tmp/nsb7-sip-outgoing-media-realtime --media
```

This supersedes the zero-media requirement only for that outgoing fixture;
the earlier signaling-only commands describe the pre-PCM configuration.
The independent incoming real-time SIP run
`tmp/nsb7-sip-incoming-media-realtime` carries396 uplink,376 downlink,
396 transmitted and400 received PCM blocks. Its own45s ready snapshot
precedes INVITE; physical Answer52 and End60 own connection/release.
Exact caller/UI/card/journal and own incoming speech-control/PCM checks pass
through `nsb7_sip_check.py --incoming --media`.
The isolated physical-audio gate additionally verifies sustained440Hz
microphone-to-SIP and660Hz SIP-to-earpiece waveforms using actual MAME
capture/playback streams, neutral codec gains, RMS>=500 and expected-tone
energy>=0.5 for at least two consecutive1s windows. Incoming capture
`tmp/nsb7-sip-incoming-waveform/handset` passes8s/4s; outgoing capture
`tmp/nsb7-sip-outgoing-waveform-clean/handset` passes9s/8s. Both complete
wrappers exit successfully after product/UI/storage and waveform checks.

```sh
make verify-8290-sip-incoming-waveform \
  SIP_WAVEFORM_NVRAM_DIR="$PWD/tmp/nsb7-pin-registration-cold/nvram" \
  RUN_DIR=tmp/nsb7-sip-incoming-waveform-fresh
# Use verify-8290-sip-outgoing-waveform for the independent outgoing leg.
```

The explicit source must come from the own-source provisioning/registration
chain, never donor data. `run_noki8290_sip_media.py` requires a fresh run,
validates source card/journal, records predecessor hashes, preserves the
predecessor and applies exact whole-run acceptance. The shared audio wrapper
restores host audio defaults and unloads temporary routes on success/failure.
Connected outgoing media restoration independently passes
`tmp/nsb7-sip-outgoing-media-restore-v2`. Before save60s, the own control
writer enables speech and at least100 uplink/50 downlink PCM blocks satisfy
exact clock accounting. Save/load retains all37 ARM/banked registers, RAM
and emulated time; the external dialog is deliberately not restored.
The bridge inventories198 uplink/194 downlink/195 transmitted/234 received
PCM blocks, sends BYE for the old epoch and accepts one cause41 clear in
epoch2, with no redial or dialog/media replay. Physical End/Menu recovery,
saved/restored/final pixels, retained RTC, exact card and valid journal pass.

```sh
make verify-8290-sip-outgoing-media-restore \
  SIP_WAVEFORM_NVRAM_DIR="$PWD/tmp/nsb7-pin-registration-cold/nvram" \
  RUN_DIR=tmp/nsb7-sip-outgoing-media-restore-fresh
```

Connected incoming restoration independently passes
`tmp/nsb7-sip-incoming-media-restore`: physical Answer52 precedes own speech
enable and documented PCM, save55/load retains exact architecture/RAM/time,
and the bridge inventories170 uplink/165 downlink/167 transmitted/176 received
blocks before invalidating epoch1. One BYE and one epoch2 cause41 clear occur
without repeated admission, Answer or media replay. Exact caller, saved/
restored/cleanup/final pixels, retained clock, complete card and journal pass.
`verify-8290-sip-incoming-media-restore` runs the independent incoming leg
with the same explicit own-source NVRAM argument.

Both unanswered phases pass on the PCM-enabled composition:
`tmp/nsb7-pcm-outgoing-pending-restore-clean` and
`tmp/nsb7-pcm-incoming-alerting-restore`. All four old-epoch media counters
are zero in each; outgoing CANCEL/487 and incoming rejection clear the
handset before physical End, without CONNECT, Answer or external media.
Both retain exact architecture, snapshots, whole-card/journal and RTC checks.
The outgoing local-codec teardown exception is bounded separately below.
This acceptance intentionally clears the live call after restore, not
seamless external-call resumption or native DSP speech.
Running an audio producer faster than wall-clock SIP is not valid media
acceptance: the unthrottled diagnostic left RTP queued past release and
failed the closed-session check. No device timing was altered to hide it.

## SIP calls and restoration

The current `nsb7hle` composition qualifies physical outgoing and incoming
local SIP200 calls with clocked HLE media, independently documented framing,
and sustained endpoint waveforms. Native DSP speech, external RF and
normal-machine promotion remain unproved. Received remote RTP alone does
not qualify handset audio.

`run_noki8290_sip_media.py` requires a fresh destination and an explicit
own-source NVRAM directory from the authenticated provisioning/registration
chain. It validates the complete laboratory card and own flash journal,
records source hashes, copies storage into the isolated run and verifies
that the predecessor remains unchanged. Host calls are enabled only by the
reviewed fixture; no firmware state or contact contents are injected.
The PJSIP bindings must be available through absolute `PYTHONPATH` entries;
see [the host bridge contract](external_call_bridge.md).

Ordinary outgoing acceptance requires PIN/phone-code entry, registered PCS600
idle45s, physical5551234/Send54, one correlated SIP200 decision and physical
End66. Incoming requires a fresh45s idle snapshot before INVITE5551234,
physical Answer52 and End60. The own caller SETUP traverses both task4 and
task14. Exact ready/dial/caller/connected/final frames, full unchanged card,
valid journal and continuous RTC remain required. Only explicitly reviewed
Calling/ringing animation alternatives qualify; matching an arbitrary crop
or accepting a new hash does not.

### Restoration phases

| Phase | Save / physical cleanup | External transaction |
| --- | --- | --- |
| Outgoing connected | Save60; End66, Menu70, End73, final76 | Confirmed dialog closes with BYE |
| Incoming connected | Save55; End60, Menu62, End65, final68 | Physical Answer precedes confirmation; BYE after restore |
| Outgoing pending | Save60; same outgoing cleanup | SIP180 remains external/provisional; CANCEL/487, no CONNECT/redial |
| Incoming ringing | Save51; same incoming cleanup without Answer | Old INVITE rejected603; no Answer/CONNECT/readmission |

Every phase requires the actual snapshot file, healthy execution console,
all37 ARM/banked registers, PC/SP, complete-RAM digest and emulated time
equal across save/load. Lua waits canceled by load are replaced only with
physical cleanup input. Own protocol checks require one epoch change, one
accepted cause41 termination and complete CC/RR release. Unanswered network
clearing must precede the later physical End, not merely occur afterward.
Saved/restored/cleanup/final display, whole-card persistence, flash validity
and retained RTC checks remain independent of architectural equality.

Connected phases additionally require own physical Send/Answer before
speech-enable writer2cb93e, at least100 uplink/50 downlink blocks with exact
PCM accounting before save, and positive old-epoch uplink/downlink/host-PCM
inventory. Restoring the handset deliberately invalidates the old external
dialog; it does not replay or seamlessly resume a live SIP call.

Unanswered phases require no connected host state, GSM CONNECT, SIP
confirmation or admitted external media, plus exactly one discarded-epoch
inventory with all four media counters zero. The pending outgoing fixture
can execute a brief local HLE PCM burst during post-load radio teardown:
observed ticks60.520/60.540/60.560 have downlink0. Acceptance checks exact
documented clocks and confines any such activity strictly between restore
and radio release. This falsifies the former zero-codec assertion, not the
no-external-speech contract; its physical-silicon fidelity remains unknown.

### Reproduction

Use the current build and absolute PJSIP binding/library paths. For an
ordinary call, invoke `run_noki8290_sip_media.py` with `--outgoing-media`
or `--incoming-media`, explicit `--nvram-source` and `--pjsua`.
The runner selects the own physical Lua fixture and runs the complete
product checker. Physical waveform gates additionally isolate host audio.

Restoration Make targets use the same explicit
`SIP_WAVEFORM_NVRAM_DIR` and fresh `RUN_DIR`:
`verify-8290-sip-outgoing-media-restore`,
`verify-8290-sip-incoming-media-restore`,
`verify-8290-sip-outgoing-pending-restore` and
`verify-8290-sip-incoming-alerting-restore`.
The final checker distinguishes `--media` for connected acceptance from
`--clocked-unanswered` for the current unanswered composition.
Legacy `--signaling-only` retains its stricter no-codec assertion for the
pre-PCM configuration; it is not the current-build acceptance mode.

## Connected host-call restoration

Two isolated PIN-authenticated runs compose the settled-idle host-call
fixtures with the shared exact architecture/replay harness. Observers, save,
End and finish times are explicit fixture inputs; ordinary idle/call/SMS
defaults remain unchanged.

| Direction | Save / replay | Post-load ownership and continuation |
| --- | --- | --- |
| Outgoing | Save60, compare reference/restored60..63 | Request1 republishes connected under epoch2. An old-epoch connect decision is explicitly rejected. Physical End66 clears CC/RR and epoch2 host state; final76 pixels equal idle. |
| Incoming | Save55, compare reference/restored55..58 | Request1 republishes connected under epoch2. An old-epoch termination is explicitly rejected. Physical End60 clears CC/RR and epoch2 host state; final68 pixels equal idle. |

Both require all37 ARM/banked registers, RAM checksum and exact emulated time
to match; ordered packet, SIM and L1-clock observations replay identically.
Reference/restored pixels must equal each other and the independently reviewed
active-call frame, not merely two blank images. The45s ready frame must equal
clean idle. Physical PIN acceptance and registration must precede save;
the whole final card remains identical to its own input. Existing call checks
still require the single original SETUP/assignment and no new host request.

Use `nsb7_pin_host_outgoing_state.lua` with the existing host runner's
`--decision connect --observe-restore` (qualified local port18934), and
`nsb7_pin_host_incoming_state.lua` with the incoming signaling runner's
`--observe-restore` and fresh readiness artifact (port18935). Then require:

```sh
.venv/bin/python tools/nsb7_state_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-pin-host-outgoing-state \
  tmp/nsb7-pin-registration-cold --host-call
.venv/bin/python tools/nsb7_state_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-pin-host-incoming-state \
  tmp/nsb7-pin-registration-cold --host-incoming-call
```

This qualifies epoch-safe restoration of the emulated signaling/session state
with a cooperating WebSocket observer. It is not resurrection of an external
SIP dialog, host audio/PCM restoration, native speech or external RF.

## Host unanswered alerting and physical recovery

An independent cold copy of the qualified PIN-enabled registered source uses
`fixtures/nsb7_host/nsb7hle.cfg` and `nsb7_pin_host_busy.lua` with the external
runner's `--decision no_answer` on localhost port18937. The shared script's
failure-named screenshot is only a capture filename: at60s the firmware shows
`Call 1` with Options/End call, while protocol and host state remain alerting,
not connected. Pixels alone must not promote connection.

The host rejects wrong request2, accepts exactly one outcome2 for request1/epoch1
and rejects its duplicate. One traffic assignment reaches ALERTING without
CONNECT/ACK. Physical End66 precedes handset DISCONNECT, network RELEASE,
handset RELEASE COMPLETE and RR closure. Host phases are exactly alerting then
ended, both with zero media sequences. Physical Menu70/End73 restores ordinary
navigation and exact registered idle at76s. Complete SIM equality, own flash
journal validity, continuous retained RTC and exact ready/alerting/menu/final
frames pass. This qualifies cancellation of an unanswered host-decided call,
not a remote no-answer timeout, speech or native DSP execution.

```sh
.venv/bin/python tools/nsb7_host_no_answer_check.py roms/noki8290/8290f522a.fls RUN_DIR
```

The observed capture is `tmp/nsb7-host-no-answer`; use the isolated launch
recipe above with `--decision no_answer`, the busy/recovery Lua script and
an82s guard. No firmware or device behavior changed.

## Host busy result and physical recovery

`nsb7_pin_host_busy.lua` retains the settled45s start, physical5551234/Send54
and End66, then adds physical Menu70 and End73. The shared host runner uses
`--decision busy` (qualified localhost port18936), not a connect decision or
media echo. Wrong-ID and duplicate decisions are rejected; one matching busy
decision queues/consumes once.

The handset completes CM/SETUP/Call Proceeding, then receives network
DISCONNECT, emits RELEASE and receives RELEASE COMPLETE/RR release. There is
no traffic assignment, Alerting, CONNECT, speech or connected host phase.
The exact60s result frame shows `Call 1` with a crossed-call indicator, not
a Busy banner and not an active call. Physical Menu70 opens the reviewed
Messages screen; End73 returns to the exact76s idle frame. The full SIM card
is unchanged. No firmware state or outcome is substituted.
The current retained-clock repeat is `tmp/nsb7-ccont-busy`: all76 ticks are
continuous from the unedited13:47:45 source, the ready/result frames show
13:48, and final13:49 idle exactly matches the connected control at76s.
The Messages frame is unchanged. The checker requires the boot analog
control write but does not require connected-call acquisition pulses in
this no-traffic negative control.

```sh
.venv/bin/python tools/nsb7_host_busy_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-ccont-busy
```

### CCONT seconds-write boundary exposed by the controls

CCONT address07 has separate read/write roles. The own-ROM command tables
at343ee0 expose write-only selector08 and read-only selector09, both encoding
address07. The analog acquisition caller2aeba6 supplies channel8/value500;
3080ba encodes this as0f, and30818a temporarily clears/restores it around
other ADC reads. The actual GENSIO stream confirms control22/command38/data.
This is not a firmware request to set the RTC seconds counter.

The device now retains that write in a separate, save-state-registered
acquisition-control latch. Its electrical effects remain unknown. RTC reads
continue to expose seconds, and the recovered minute/hour clock-load strobe
remains responsible for setting time. The former writable-seconds model
incorrectly reset retained seconds at boot and stalled the clock during calls;
its busy-versus-connected clock divergence is not a valid hardware oracle.

The independently repeated connected run in `tmp/nsb7-ccont-alias` starts
from the same retained13:47:45 source without editing it. All76 one-second
ticks are continuous despite39 control writes. Reviewed exact pixels show
13:48 ready and13:49 after physical End; authentication, host correlation,
CC/RR release and unchanged complete card pass. The outgoing checker banks
those exact frames without masking the clock and enforces every tick.
Other clock-sensitive NSB-7 workflows still require requalification before
their old exact hashes can be treated as current-device evidence.

Outgoing host save/load is independently requalified in
`tmp/nsb7-ccont-state-v2` with `--observe-restore`: save at60s, exact
CPU/RAM/time restoration, identical three-second radio/SIM/L1 plus CCONT
RTC/acquisition replay, republished connected request under epoch2 and
explicit stale-epoch rejection, then physical End66 and exact13:49 idle.
Its ready/active/final hashes use the same reviewed retained-clock contract
as the ordinary outgoing run; image equality alone is not acceptance.

Incoming host controls are independently repeated in
`tmp/nsb7-ccont-incoming` and `tmp/nsb7-ccont-incoming-state`. The ordinary
run checks every retained-clock tick through68s, exact13:48 ready/caller/
active/final pixels, physical Answer52/End60 and unchanged complete card.
The restore run saves55s, reproduces CPU/RAM/time and three-second
radio/SIM/L1/CCONT replay, republishes connected under epoch2, rejects the
old-epoch termination and physically releases to the same exact idle frame.
These remain signaling/session tests, not native speech or SIP restoration.

The shared two-process RTC gate uses the recovered clock-load strobe, not
a fictitious seconds-write shortcut, to program12:00 and wait a full minute
for the alarm/IRQ. It then proves retained12:01:04 advances to05/06/07 in
a fresh process and exercises legacy/atomic malformed-NVRAM cases.

## Authenticated SIM Toolkit DISPLAY TEXT

`fixtures/nsb7_toolkit/nsb7hle.cfg` selects the existing Phase-2+ external
card with one DISPLAY TEXT command, nominal reviewed hardware inputs and
the PCS laboratory cell. The own PIN-enabled retained-location flash/card/
clock source is unchanged. `nsb7_toolkit_input.lua` reuses the qualified
physical authentication and read-only radio/LAPDm observers; it captures
the prompt at65s and presses the physical left softkey at66s.

Firmware reads EF_PHASE03, sends a nine-byte TERMINAL PROFILE and continues
to registered idle. The card makes its command pending at20.026s, but the
first later STATUS is near62.018s: it returns9116 and the firmware FETCHes
22 bytes. A short observation ending before that poll cannot establish
absence of Toolkit support. The reviewed prompt says `DCT3 SAT`, with OK
and Back softkeys. Physical OK decodes19 and sends terminal response
`810301218002028281030100`, followed by9000; the final68s frame is the
same reviewed13:48 registered idle. A response with result12 is a timeout,
not successful physical dismissal.

`nsb7_toolkit_check.py` requires the full authentication/registration chain,
exactly one profile/FETCH/response exchange, decoded OK before response,
both exact RGB frame hashes, valid own flash journal and byte-identical
complete SIM storage. It does not claim further proactive commands, native
DSP, external RF or speech. No driver/device behavior was changed.

```sh
.venv/bin/python tools/nsb7_toolkit_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-toolkit-final
```

### Interactive inputs

`fixtures/nsb7_toolkit_interactive/nsb7hle.cfg` selects the existing three-
command card. `nsb7_toolkit_interactive.lua` composes the same authentication
and DISPLAY TEXT path, then enters physical5 at69s and OK at70s. This
product requires explicit OK for GET INKEY; the digit alone remains in its
one-character editor. The card receives successful terminal response
`8103022200020282810301000d020435` and offers GET INPUT.

Physical4/2 at72/73s fills the reviewed `Enter 42` editor; OK74 returns
`8103032300020282810301000d03043432`, then9000. The exact76s frame is
registered13:49 idle. Full authentication/retained-location acceptance and
the unchanged complete card pass independently. No driver/device changes
were needed. SET UP MENU is qualified separately below; other commands
are not established by this three-command fixture.

The interactive checker requires one profile and three FETCH/response
exchanges, all six physical actions with decoded contacts before their
corresponding response, and five reviewed exact frames. It rejects missing
GET INKEY confirmation, wrong/missing decoded keys, late decode evidence,
timeout results and wrong returned input bytes.

```sh
.venv/bin/python tools/nsb7_toolkit_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-toolkit-interactive-v2 --interactive
```

### Installed menu and selection envelope

The four-command profile in `fixtures/nsb7_toolkit_menu/nsb7hle.cfg` and
`nsb7_toolkit_menu.lua` preserves the same authenticated input sequence.
Successful GET INPUT returns9128; firmware FETCHes SET UP MENU and answers
`810304250002028281030100`/9000. Physical Menu78, Up80/82 and Select84
open the installed `DCT3 menu`/`Continue` item. This own provisioning also
exposes Net monitor after the Toolkit entry, so one Up from the main menu
reaches Net monitor rather than Toolkit; both navigation steps are explicit.

Physical Select86 emits exactly one ENVELOPE
`d30702020181100101` (item1), answered9000. End88 returns to exact13:49
idle. The full authentication/registration chain, four FETCH/response pairs,
twelve decoded physical actions, eleven exact frames, valid own journal and
unchanged full card pass. No firmware state, menu selection or envelope is
injected. Other proactive command families remain independent targets.

```sh
.venv/bin/python tools/nsb7_toolkit_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-toolkit-menu-v2 --menu
```

### Menu-driven SMS

`fixtures/nsb7_toolkit_sms/nsb7hle.cfg` selects the existing SEND SHORT
MESSAGE card application. `nsb7_toolkit_sms.lua` reuses the complete physical
authentication and interactive/menu sequence; Select86 advertises9124,
then FETCH supplies the card's command. Firmware submits exactly one8-bit
`SAT` message to5551234 through SMSC1234567890, CP39/RP01 and TP-MR00.
The laboratory peer sends CP-ACK/RP-ACK, and firmware returns successful
terminal response `810305130002028281030100`/9000, acknowledges CP and
releases the radio channel. This is decoded laboratory CP/RP/RR delivery,
not an external SMS carrier or native RF claim.

The SIMI path performs one UPDATE BINARY of EF_SMSS offset0/length2. The
complete3570-byte final card equals the own source except TP-MR byte3560
changesff to00; byte3561 and every other byte remain identical. The checker
requires this exact change, not an arbitrary allowance for SMS metadata.
The reviewed87s frame says `SAT SMS`; End108 returns to exact13:49 idle.
All eleven frame checks remain active, with only the selected-command frame
different from the plain menu fixture. No driver/device behavior changed.

```sh
.venv/bin/python tools/nsb7_toolkit_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-toolkit-sms --sms
```

### Call consent and cancellation

`fixtures/nsb7_toolkit_call/nsb7hle.cfg` selects the existing SET UP CALL
card application. Both own-source controls install/select the menu through
the same physical sequence; Select86 returns911c and FETCH supplies the
request for5551234. The reviewed prompt says `SAT call`, with Call/Cancel.

`nsb7_toolkit_call.lua` physically accepts at90s. Decoded consent precedes
SETUP and one outgoing request. CC/RR completes Call Proceeding, one traffic
assignment, Assignment Complete, Alerting, CONNECT and CONNECT ACKNOWLEDGE.
Only then does the card receive terminal response
`810306100002028281030100`/9000. A separate100s snapshot shows connected
`SAT call`; physical End108 precedes DISCONNECT/Release/Release Complete
and RR release, with exact13:49 idle afterward.

`nsb7_toolkit_call_decline.lua` instead presses the physical right softkey
at90s. It returns `810306100002028281030122`/9000 and the reviewed Toolkit
item screen, without any GSM outgoing request, SETUP, traffic assignment or
CONNECT. End108 exits to the same exact idle frame. Both controls enforce
thirteen decoded actions, thirteen exact frames, full authentication/
registration, valid own flash and byte-identical complete SIM storage.
This is laboratory call signaling and consent, not speech, host/SIP calls
or native resident DSP execution. No driver/device behavior changed.

```sh
.venv/bin/python tools/nsb7_toolkit_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-toolkit-call-v2 --call
.venv/bin/python tools/nsb7_toolkit_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-toolkit-call-decline --call-decline
```

### Open-menu restoration

`nsb7_toolkit_menu_state.lua` holds the same four-command card's installed
menu open after physical Select84, then saves at85.25s with no held contact.
A bounded physical-action callback presses Select0.5s after both reference
and restored starts. Each three-second interval contains exactly one
`d30702020181100101` envelope accepted with9000. Full SIM command/body/
status, CCONT RTC/control and radio records are identical, as are all37
exported ARM/banked registers, RAM checksum and emulated time.

Physical End90 after restored replay returns to exact13:49 idle. Twelve
exact frames include the existing authenticated input/menu captures, both
post-selection replay frames and final idle. The full card is unchanged and
the own flash journal remains valid. This validates stateful card/firmware
menu composition, not merely a screenshot match or host-dialog restoration.
The shared architecture checker is also used by the existing call/SMS gates;
the replay callback is absent from ordinary fixtures.

```sh
.venv/bin/python tools/nsb7_toolkit_state_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-toolkit-menu-state
```

### Proactive item selection

`fixtures/nsb7_toolkit_select_item/nsb7hle.cfg` and
`nsb7_toolkit_select_item.lua` compose the same own authenticated menu path
with SELECT ITEM. Selection86 returns9122; FETCH supplies the Alpha/Beta
dialog. Physical Down90 selects Beta and OK92 returns
`810307240002028281030100100102`/9000, including item identifier02.
End98 returns to exact13:49 idle. No GSM call or SMS submission is allowed.

Acceptance requires one profile, five FETCH/response pairs, one envelope,
fourteen decoded physical actions before their responses, thirteen exact
frames, full registration, a valid own journal and unchanged complete card.
Wrong item, wrong Down key and response-before-OK controls are rejected.
The checker uses a single named mode internally; its CLI options remain
mutually exclusive. This is firmware/card UI acceptance, not native DSP/RF.

```sh
.venv/bin/python tools/nsb7_toolkit_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-toolkit-select-item --select-item
```

### Polling commands and retained STATUS traffic

`fixtures/nsb7_toolkit_polling/nsb7hle.cfg` and
`nsb7_toolkit_polling.lua` reuse the physical menu sequence. Selection86
returns910f, FETCH supplies POLL INTERVAL, and firmware returns
`81030a03000202828103010004020105`: success with reported interval five
seconds. The card immediately chains POLLING OFF through910b; response
`81030b040002028281030100`/9000 succeeds. The full card is unchanged.

The extended run observes STATUS at93.004/124.014/155.016/186.018s after
OFF, and keeps running through physical End188/final189s. These roughly
31-second intervals are measured research timing, not a physical-clock
specification or proof of the caller's semantic ownership. They are not the
negotiated five-second cadence and are compatible with separate SIM presence
checks. [GSM 11.14 section 6.4.14](https://www.etsi.org/deliver/etsi_TS/101200_101299/101267/08.02.01_60/ts_101267v080201p.pdf)
leaves presence detection unaffected by POLLING OFF. The older NSE-8 gate's
no-STATUS observation therefore remains product/fixture-specific.

Acceptance covers one profile, six FETCH/response pairs, one envelope,
twelve decoded physical actions, eleven exact frames including13:50 final
idle, full registration, valid own flash and unchanged complete card. It
requires at least four later STATUS samples with the observed cadence and
an untruncated RTC observation, and rejects call/SMS side effects. Because
OFF follows immediately, actual operation of the five-second schedule and
the precise presence-poll owner remain unproved. No device behavior changed.

```sh
.venv/bin/python tools/nsb7_toolkit_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-toolkit-polling-long --polling
```

### REFRESH and renewed application session

`fixtures/nsb7_toolkit_refresh/nsb7hle.cfg` and `nsb7_toolkit_refresh.lua`
use the existing qualifier0 REFRESH card application. Physical selection86
returns910b; FETCH supplies REFRESH. The 8290 sends a second nine-byte
TERMINAL PROFILE near86.182s before successful terminal response
`810309010002028281030100` near91.012s. That outstanding response must
survive re-profiling; it is not a new card request to cancel it.

The card-ready callback initializes DISPLAY TEXT ID21/number1 before marking
the next command pending. REFRESH completion clears the prior ID; without
that initialization the old model logged readiness but advertised9100 and
accepted no subsequent FETCH. The corrected model advertises9116 and the
handset FETCHes22 bytes near124.069s. This is a card-application state fix,
not firmware-state forcing or a change to the handset's poll timing.

The extended fixture captures the renewed prompt129s, then physically
completes DISPLAY TEXT OK130, GET INKEY5/OK134/136 and GET INPUT4/2/OK140/
142/144. A fresh SET UP MENU is acknowledged; End148 returns to exact13:50
idle. The checker requires two profiles, nine FETCH/response pairs, one
envelope, eighteen decoded actions, eighteen exact frames, full initial
registration, valid own journal and unchanged complete card. It rejects
9100/zero-length FETCH, missing re-profile, failed REFRESH and incomplete
renewed input. No GSM call or SMS submission is permitted. The default3210
and existing NSE-8 REFRESH gates remain green; native DSP/RF remain unproved.

```sh
.venv/bin/python tools/nsb7_toolkit_check.py \
  roms/noki8290/8290f522a.fls tmp/nsb7-toolkit-refresh-fixed --refresh
```
