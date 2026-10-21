# Nokia 8290 / NSB-7 inputs

## Current boundary

The acquired NSB-7 v5.22 package now has a qualified, contiguous MCU/PPM-A
image. `noki8290` is a fail-closed MCU research configuration: it reaches
the supervisor and renders the firmware's complete `CONTACT SERVICE`
failure frame. Idle, organic keypad/SIM, registration, calls and SMS are
not established for that fail-closed machine. The separate `nsb7hle`
research composition, with own checksum-only journal repairs and a declared
nominal VBATT input, reaches `Insert SIM card` and physically enters digit
`1` into the dial editor with an empty socket. With the synthetic card
inserted, SIMI startup file reads and the phone Security code editor
reproduce across cold runs. A separate one-byte erased-retry fixture permits
physical acceptance of the own-ROM default phone code and menu navigation.
A physical SIM phonebook save and cold Details retrieval are qualified.
The opt-in PCS peer qualifies band measurements, candidate synchronization
receiver-configuration acknowledgement and complete BCCH system-information
parsing, CCCH setup, timed random-access completion and an accepted
Immediate Assignment, dedicated-channel setup and an emitted Location
Updating Request inside LAPDm SABM, followed by accepted contention-resolution
UA and firmware link state `2 -> 4`. Location Updating Accept and Channel
Release are acknowledged by firmware; EF_LOCI is updated over SIMI and
retained, and the receiver returns to common control. Repeated empty paging
blocks reach its RR decoder. An addressed page now produces the handset's
own Paging Response after background measurement completion. Stable
registered idle presentation, calls and SMS remain unestablished.
In the normal machine, DSP exchange/service replies are disabled; own internal
ROM regions are explicitly undumped rather than attached sibling files.
No PMM, resident DSP mask, hardware profile or provisioning is supplied by
this normalization. The [Nokia 8290 user guide](https://www.instructionsmanuals.com/sites/default/files/2019-05/Nokia-8290-en.pdf)
identifies the product as NSB-7 and GSM1900; the [NSB-7 service manual](https://www.manualslib.com/manual/2171641/Nokia-Nsb-7-Series.html)
is the independent hardware-reference starting point.

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
protected separately. Next trace the surviving startup result and its owner,
not another guessed DSP completion.

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
idle or input delivery. The remaining task is to recover the own keypad
contract and test application activity physically, alongside the surviving
startup continuation. The normal erased-storage baseline and native
missing-resident-code boundary are unchanged.

## Keypad startup boundary

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
does not unmask columns or write guest state. The next question is which
startup dependency releases that keypad state. Interactive boot remains
unproven.

The saved return address at the mask write is `29eee7`, identifying direct
caller `29eee2` (the other direct BL sites are `29ee14` and `29f0d6`). This
is the branch selected by the startup result `2`; it resets the state
through `301ed0`, then receives readiness reports. Its continuation at
`29efa2` requires the low nibble at `13ff00` to be `6` and that at `1308b1`
to be `0f`. Only after those tests does it call `30acf2` at `29efde`.
Runtime writes establish state `6` at 2.189 seconds and readiness `0e`
at 2.853 seconds. The report decoder maps `14/16/15/17` to bits `1/2/4/8`;
the missing bit therefore belongs to report `14`, not a guessed key event.
This identifies the next producer question without claiming that the
readiness gate is the sole remaining UI dependency.

`tools/nsb7_keypad_conformance.lua`, on a separate fresh `noki8290` run,
tests 20 physical matrix contacts against all five row drives (100 scans),
plus the Power special contact and release. It writes controller MMIO and
is explicitly conformance evidence, not organic input delivery. Those
checks pass; the normal failure-frame oracle and native `2c75` controlled
stop also reproduce after the keypad configuration change.

### Missing analog readiness report

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
proceeding. Decode that predicate and the preceding power/ADC branch before
changing board inputs; do not synthesize report `14`, unmask the keypad,
or assert a charger cause to escape the branch. Raw analog defaults remain
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

The post-acceptance frame has `Menu/Names` softkeys but malformed main-area
content. Do not call it a usable idle or registered screen. Physical menu
activation works independently, as qualified below; no security verdict or
UI state is injected.

## Physical menu acceptance

After the same physical security sequence, Menu at 26 seconds opens the
firmware's `Messages` menu. Down at 29 seconds selects `Call log`; Menu at
32 seconds opens its `Missed calls` item. Text, icons, scrollbar and
softkey labels render coherently on all three screens. Their corresponding
logical decodes are `19/18/19`. This distinguishes the malformed idle
content from a general LCD/controller or keyboard-navigation failure.

`tools/nsb7_menu_observe.lua` composes the security observer and those
physical contacts. `tools/nsb7_menu_check.py IMAGE RUN` requires the
security comparison and retry evidence, each decoded menu contact, and
exact RGB frames for all three views. The same frames reproduce in a new
process retaining the preceding run's own flash and card. It does not
establish call history, messaging services, a usable idle screen or RF.

The remaining display/application question is why the idle main area
contains malformed content while menu resources render normally. Inspect
firmware-owned settings/resource selection (including erased NV inputs)
before modifying the LCD device or synthesizing network presentation.

### Firmware-owned clock setting and RTC retention

`tools/nsb7_clock_input.lua` navigates the physical Settings/Clock/Set the
time controls, enters `13:47`, confirms, and returns through Back. The
editor displays the intended digits and the firmware confirms `Time is
set`. The idle top-right clock then displays `13:47`, while malformed
main-area content remains. A valid clock therefore does not by itself
resolve that separate display/resource issue.

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
the rest of the malformed idle screen is not promoted to an oracle.
This qualifies own RTC/settings behavior, not full idle, calendar or alarm
support. No device behavior, RTC seed, NV field or guest RAM was patched
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
still has malformed main-area content despite working softkey text and
clock, so there is no usable registered-idle graphics claim.

### Addressed page: own identity accepted, RR continuation unresolved

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
TX inventory remains 49 packets. The unresolved question is how the
firmware's RR lifecycle advances after `083e`, not whether the page carries
the correct subscriber identity or reaches the RR parser.

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
match. The observer retains only these focused taps; the temporary
time-window instruction-path trace and unconfirmed snapshot-restore
hook were removed. `--addressed-page` now requires this continuation.
The next question is which lifecycle exit from this state should initiate
access, or whether a prerequisite to entering the ordinary paging-ready
state remains incomplete. No missing hardware response is yet identified.

A two-byte-aligned PC-relative literal-reference scan of own-ROM
`25d000..261000` finds six comparisons against `083e`, at `25e666`,
`25f7a2`, `25f9fa`, `260a9a`, `260b2a` and `260c30`, plus one literal
load used to synthesize that event at `260af0`. This bounds the checked
direct-literal surface, not indirect or arithmetic event selection.
The alternative path at `260af4` can call `25f6de` only when API
`200690(1)` returns 1, record `+3` is zero and record `+6` is not 1.
The current `260c4c` path does not evaluate those conditions. Their
ownership and the lifecycle selecting that alternative remain open.

The serving-RSSI sufficiency experiment is falsified. In
`tmp/nsb7-page-rssi`, the existing shared type-83 payload reaches own
forwarder `2e49b4` and task 13 as message `1802`, with signed serving
samples `c3/c4/c5`. The matching page still returns `083e`, sets the
same flag and releases the same saved record, without a new access
request through 25 seconds. The trial scheduling change was removed;
the observer retains the bounded routing tap. This is not a proof that
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
below. Forcing `+6` clear would bypass that unanswered contract.

### Post-release service completion boundary

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
returns from the same task-18 record. The next investigation is what
prevents task 13 completing the post-release `03ec`, compared with the
earlier successful lifecycle. That boundary is resolved by the background
measurement contract below; the bounded timeout fixture remains a negative
control, not the currently selected peer coverage.

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
page-only release. Subsequent service-completion stability, incoming
call/SMS and usable idle graphics remain separate acceptance work.
The earlier serving-type-83 sufficiency falsification remains valid:
this correction answers type 57 with type 8b, not periodic serving RSSI.
